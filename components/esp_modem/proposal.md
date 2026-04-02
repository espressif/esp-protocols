# Proposal: Expose Terminal Creation API and Add DTE Transmit Hooks

**References:** IDFGH-17314 — *Add before-transmit callback hook on Netif for host-sleep / UART power-save wake*

## Background

Modems with UART power-save modes (Quectel QSCLK, u-blox UPSV, SIMCom CSCLK) clock-gate
their UART when the host asserts DTR HIGH. The host must toggle a GPIO around every UART
write to wake the modem before transmitting and re-arm sleep afterwards.

Application code can wrap its own AT commands with GPIO toggling, but *internal* traffic —
lwIP keepalives, MQTT PINGREQs, PPP LCP echo-replies — flows through
`Netif::esp_modem_dte_transmit()` → `DTE::write()` → `Terminal::write()` with no user-accessible
hook. These bytes hit a sleeping UART and are silently lost, causing MQTT disconnects and
PPP link drops.

Two gaps in the current design prevent users from solving this cleanly:

## Problem 1 — Private UART Terminal API

`create_uart_terminal()` is declared in `private_include/uart_terminal.hpp`. C++ users who want
to build a custom DTE subclass (the natural extension point — `DTE::write()` is virtual) must
hack their CMakeLists.txt to reach the private include path:

```cmake
# Workaround: reach into esp_modem private headers
idf_component_get_property(modem_dir esp_modem COMPONENT_DIR)
# then add "${modem_dir}/private_include" to INCLUDE_DIRS
```

This is fragile and undocumented.

## Problem 2 — No C API for Transmit-Time GPIO Gating

Users of the plain-C API (`esp_modem_new()` / `esp_modem_new_dev()`) have no mechanism at all
to hook into the transmit path. They cannot subclass DTE, and there is no callback API to run
code around writes. This makes UART power-save integration impossible without switching to C++.

## Proposed Solution

### Part 1 — Make `create_uart_terminal()` Public

Move the declaration from `private_include/uart_terminal.hpp` to the public C++ header
`include/cxx_include/esp_modem_api.hpp`, alongside the already-public `create_uart_dte()`:

```cpp
// include/cxx_include/esp_modem_api.hpp  (addition)
namespace esp_modem {

/**
 * @brief Create a UART terminal (without wrapping it in a DTE)
 *
 * Useful for building custom DTE subclasses that need a standard UART terminal.
 */
std::unique_ptr<Terminal> create_uart_terminal(const dte_config *config);

} // namespace esp_modem
```

This lets C++ users create a custom DTE cleanly, with no private-header workarounds:

```cpp
auto term = esp_modem::create_uart_terminal(&dte_config);
auto dte  = std::make_shared<MyDTE>(&dte_config, std::move(term), dtr_pin);
```

### Part 2 — Add Transmit Hooks to DTE (C++)

Add optional before/after callbacks to `DTE` that fire around every physical write:

```cpp
// include/cxx_include/esp_modem_dte.hpp  (additions to class DTE)
public:
    using transmit_hook_t = std::function<void()>;

    /**
     * @brief Register hooks that fire around every DTE write operation
     *
     * @param before_tx  Called just before bytes are sent to the terminal (may be nullptr)
     * @param after_tx   Called just after bytes are sent to the terminal (may be nullptr)
     */
    void set_transmit_hooks(transmit_hook_t before_tx, transmit_hook_t after_tx);

private:
    transmit_hook_t before_transmit_;
    transmit_hook_t after_transmit_;
```

The hooks fire in every DTE write path.  Currently there are four distinct paths
that write to the underlying terminal(s):

| DTE method | Caller | Terminal used |
|---|---|---|
| `write(uint8_t*, size_t)` | `Netif` (PPP frames) | `secondary_term` |
| `write(DTE_Command)` | User AT command helpers | `primary_term` |
| `send(uint8_t*, size_t, int)` | User code (generic) | selected term |
| `command(...)` (internal write) | AT command engine | `primary_term` |

Note: `command()` writes directly to `primary_term->write()`, bypassing `DTE::write()`.
The hooks must be added inside `command()` as well — wrapping only the `primary_term->write()`
call, **not** the entire command-wait cycle, so DTR stays LOW only while bytes are in flight.

A private helper keeps the pattern DRY:

```cpp
// In esp_modem_dte.cpp
template<typename Fn>
auto DTE::guarded_write(Fn&& fn) -> decltype(fn()) {
    if (before_transmit_) before_transmit_();
    auto ret = fn();
    if (after_transmit_) after_transmit_();
    return ret;
}
```

### Part 3 — Expose Transmit Hooks via C API

```c
// include/esp_modem_c_api_types.h  (additions)

/**
 * @brief Transmit hook callback type
 * @param user_ctx  Opaque context pointer passed at registration time
 */
typedef void (*esp_modem_transmit_hook_t)(void *user_ctx);

/**
 * @brief Register hooks that fire around every DTE write operation
 *
 * Allows toggling a GPIO (DTR, sleep pin, etc.) before/after UART writes,
 * covering both application-initiated AT commands and internal PPP traffic.
 *
 * @param dce        Modem DCE handle
 * @param before_tx  Called just before bytes are sent (NULL = no hook)
 * @param after_tx   Called just after bytes are sent (NULL = no hook)
 * @param user_ctx   Opaque pointer forwarded to both hooks
 * @return ESP_OK on success
 */
esp_err_t esp_modem_set_transmit_hooks(
    esp_modem_dce_t *dce,
    esp_modem_transmit_hook_t before_tx,
    esp_modem_transmit_hook_t after_tx,
    void *user_ctx
);
```

Implementation in `esp_modem_c_api.cpp` wraps the C callback + context into
`std::function<void()>` and calls `dte->set_transmit_hooks(...)`.

## Thread Safety Note (CMUX)

In CMUX mode, `write()` (PPP data) and `command()` (AT commands) may be called concurrently
from different threads. Both will invoke the transmit hooks. Users implementing DTR gating
under CMUX should use an atomic reference count to keep the modem awake while *any* write is
in progress:

```c
typedef struct {
    gpio_num_t   dtr_pin;
    _Atomic int  tx_active;
    uint32_t     wake_delay_ms;
} sleep_ctx_t;

static void before_tx(void *ctx) {
    sleep_ctx_t *c = (sleep_ctx_t *)ctx;
    if (atomic_fetch_add(&c->tx_active, 1) == 0) {
        gpio_set_level(c->dtr_pin, 0);                // wake modem
        vTaskDelay(pdMS_TO_TICKS(c->wake_delay_ms));   // let it wake up
    }
}

static void after_tx(void *ctx) {
    sleep_ctx_t *c = (sleep_ctx_t *)ctx;
    if (atomic_fetch_sub(&c->tx_active, 1) == 1) {
        gpio_set_level(c->dtr_pin, 1);                // allow modem to sleep
    }
}
```

In the non-CMUX case (single-terminal mode), all writes are serialized by the DTE internal
lock and no special handling is needed.

## Typical Usage (C API, end-to-end)

```c
static sleep_ctx_t sleep_ctx = {
    .dtr_pin = GPIO_NUM_4,
    .tx_active = 0,
    .wake_delay_ms = 20,
};

void app_main(void) {
    gpio_config_t io = {
        .pin_bit_mask = BIT64(sleep_ctx.dtr_pin),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(sleep_ctx.dtr_pin, 1);  // start in sleep-allowed state

    esp_modem_dce_t *dce = esp_modem_new_dev(ESP_MODEM_DCE_BG96, &dte_cfg, &dce_cfg, netif);
    esp_modem_set_transmit_hooks(dce, before_tx, after_tx, &sleep_ctx);
    // ... enable modem sleep mode via AT+QSCLK=1 ...
    esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
    // All subsequent PPP traffic and AT commands will automatically
    // toggle DTR via the hooks.
}
```

## Files Changed

| File | Change |
|---|---|
| `include/cxx_include/esp_modem_api.hpp` | Add `create_uart_terminal()` declaration |
| `include/cxx_include/esp_modem_dte.hpp` | Add `set_transmit_hooks()`, hook members, `guarded_write()` |
| `src/esp_modem_dte.cpp` | Implement `set_transmit_hooks()`, wrap all write paths |
| `include/esp_modem_c_api_types.h` | Add `esp_modem_transmit_hook_t`, `esp_modem_set_transmit_hooks()` |
| `src/esp_modem_c_api.cpp` | Implement C API wrapper for hooks |
| `private_include/uart_terminal.hpp` | Remove `create_uart_terminal()` decl (moved to public header) |

## Backward Compatibility

- **No** changes to `esp_modem_dte_config_t` or any existing config struct / default macro
- **No** changes to existing API signatures
- All additions are purely additive
- When hooks are not registered (the default), behavior is identical to current code;
  overhead is a single null-function check per write (~1 branch)

## Alternatives Considered

### A — `before_transmit` callback on Netif (original issue proposal)

Only covers PPP/networking traffic, not AT commands. Requires exposing `Netif` from DCE
(`get_netif()` accessor), leaking an implementation detail. The DTE-level approach is more
general and covers all write paths.

### B — Built-in sleep GPIO in `esp_modem_dte_config`

Adding a `sleep_io_num` field with automatic toggling. Too opinionated — different modems
need different wake sequences (variable delays, pulse widths, active-low vs active-high,
multi-pin sequences). A callback approach is strictly more flexible.

### C — Expose full `UartTerminal` class

Making the `UartTerminal` class definition public so users can subclass the terminal layer.
Overkill: users need to wrap *DTE-level* writes (to cover both terminals and CMUX), not
terminal-level writes. Exposing `create_uart_terminal()` is sufficient.

### D — Protected virtual `before_write()` / `after_write()` in DTE

Only helps C++ users. Cannot be wrapped by the C API. Callbacks serve both audiences.
C++ users who prefer the subclass approach already have `DTE::write()` as virtual — they
just need `create_uart_terminal()` to be public (Part 1).

## Future Consideration

Some users may benefit from access to the underlying `uart_port_t` handle (e.g., to call
`uart_set_dtr()`, change baud rate at runtime, or reconfigure pins). This is a separate
concern and could be addressed later via a `get_uart_port()` accessor on the terminal or
a new C API function.
