# net_connect

`net_connect` is an ESP-IDF component for setting up and managing **Wi-Fi STA**, **Ethernet**, **OpenThread**, and **PPP (USB CDC / UART)** network interfaces through a single API.

## What It Does

- **Supports multiple transports:** Wi-Fi STA (built-in or remote via `esp_wifi_remote`), Ethernet, OpenThread, and PPP (USB CDC or UART).
- **Single-call connection (**`net_connect()`**):** Starts all interfaces enabled in `menuconfig` in parallel and waits until the device has a working link.
- **Non-blocking handle API (**`net_iface.h`**):** Starts and controls each interface in the background without blocking your main task on slow hardware setup (such as a ~4-second timeout when an Ethernet cable is unplugged).
- **Multi-interface support:** Run more than one interface at the same time, wait for the first or all interfaces to come up, set Wi-Fi credentials at runtime, and adjust route priorities.
- **Waiting or events:** Block with a timeout (`net_iface_wait`*), or handle state changes asynchronously with `NET_IFACE_EVENT`.
- **IPv4 and IPv6:** Configurable readiness checks for IPv4, IPv6, or both.

### Choosing an API

| API | Use When |
|---|---|
| `net_connect()` / `net_disconnect()` | You want the simplest setup for examples, prototypes, or single-purpose apps. |
| Handle API (`net_iface.h`) | You need non-blocking startup, multiple interfaces, runtime Wi-Fi provisioning, or event-driven control. |

---

## Quick Start (`net_connect()`)

Enable the interface(s) you need in `idf.py menuconfig` under **Net Connect Configuration**, then include `"net_connect.h"` and call `net_connect()`:

```c
#include "net_connect.h"

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Start all enabled interfaces in parallel and wait until ready */
    ESP_ERROR_CHECK(net_connect());

    /* Returns the preferred active netif (order: Ethernet -> Wi-Fi -> Thread -> PPP).
     * To get a specific interface instead, pass its description to net_get_netif_from_desc():
     * "net_connect_netif_eth", "net_connect_netif_sta",
     * "net_connect_netif_thread", or "net_connect_netif_ppp".
     */
    esp_netif_t *netif = net_get_netif();

    /* ... application code ... */

    ESP_ERROR_CHECK(net_disconnect());
}
```

For timeout settings and required vs. optional interface rules, see [Detailed](#detailed-net_connect-behavior) `net_connect()` [Behavior](#detailed-net_connect-behavior).

---

## Handle API (`net_iface.h`)

Use `net_iface.h` when you want to manage each interface separately.

### 1. Start in the Background and Wait

```c
#include "net_iface.h"

net_iface_handle_t eth = NULL;
net_iface_handle_t wifi = NULL;

/* Create handles using menuconfig defaults (or pass a params struct to override) */
ESP_ERROR_CHECK(net_eth_create(NULL, &eth));
ESP_ERROR_CHECK(net_wifi_sta_create(NULL, &wifi));

/* Both calls start driver setup in background tasks and return immediately */
ESP_ERROR_CHECK(net_iface_start(eth));
ESP_ERROR_CHECK(net_iface_start(wifi));

/* Wait up to 15 seconds for whichever interface becomes READY first */
net_iface_handle_t ready_iface = NULL;
net_iface_handle_t both[2] = { eth, wifi };
esp_err_t err = net_iface_wait_any(both, 2, 15000, &ready_iface);
if (err == ESP_OK) {
    /* ready_iface is READY.
     * The other interface keeps running in the background until it is ready,
     * fails, or you call net_iface_stop(). */
}
```

**Notes on waiting:**

- `net_iface_wait_any()` and `net_iface_wait_all()` wait for `NET_IFACE_STATE_READY`.
- `net_iface_wait(h, timeout_ms)` waits for `READY` when `auto_connect_on_start` is `true` or after `net_iface_connect(h)`. If auto-connect is `false` and only `net_iface_start(h)` was called, it waits for `STARTED`.
- Timing out does not stop the interface; the connection attempt continues in the background.

### 2. Event Notifications (`NET_IFACE_EVENT`)

Instead of calling `net_iface_wait*()`, you can register an event handler on the default ESP event loop:

```c
static void on_net_iface_event(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    const net_iface_event_data_t *data = (const net_iface_event_data_t *)event_data;

    switch ((net_iface_event_id_t)event_id) {
    case NET_IFACE_EVENT_READY:
        ESP_LOGI("app", "Interface type %d is READY (netif=%p)", data->type, data->netif);
        break;
    case NET_IFACE_EVENT_LOST_IP:
        ESP_LOGW("app", "Interface type %d lost IP", data->type);
        break;
    case NET_IFACE_EVENT_FAILED:
        ESP_LOGE("app", "Interface type %d FAILED: %s",
                 data->type, esp_err_to_name(data->err));
        break;
    default:
        break;
    }
}
```

> [!Important]
>
> - **Register before starting:** `net_iface_register_event_handler()` does not replay past events. If you register after calling `net_iface_start(h)`, call `net_iface_get_state(h)` right afterward to check if the interface is already `READY` or `FAILED`.
> - **Do not block in callbacks:** Event callbacks run on the default ESP event loop task. Do not call `net_iface_wait*()` or `net_iface_stop()` inside a callback; they return `ESP_ERR_INVALID_STATE` to avoid deadlocks.

### 3. Example: Ethernet Primary + Runtime Wi-Fi Provisioning

In a gateway or hub device, it is common to boot with **Ethernet** connecting right away and **Wi-Fi** unconfigured (`ssid = ""`), then connect Wi-Fi later once the user supplies credentials and raise Ethernet's route priority whenever it is up:

```c
#include "net_iface.h"
#include "esp_netif.h"

static void hub_event_handler(void *arg, esp_event_base_t base,
                              int32_t event_id, void *event_data)
{
    const net_iface_event_data_t *data = (const net_iface_event_data_t *)event_data;

    if (event_id == NET_IFACE_EVENT_READY && data->type == NET_IFACE_TYPE_ETH) {
        /* Set Ethernet priority (200) above Wi-Fi (128) when Ethernet is READY */
        esp_netif_set_route_prio(data->netif, 200);
        esp_netif_set_default_netif(NULL); /* Re-evaluate default route */
    }
}

void start_home_hub(void)
{
    ESP_ERROR_CHECK(net_iface_register_event_handler(hub_event_handler, NULL));

    /* 1. Required Ethernet (default route_prio = 64) */
    net_eth_params_t eth_params = {
        .route_prio = NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO,
        .required = true,
        .auto_connect_on_start = true,
    };
    net_iface_handle_t eth = NULL;
    ESP_ERROR_CHECK(net_eth_create(&eth_params, &eth));

    /* 2. Optional, unprovisioned Wi-Fi (stays in STARTED until credentials are set) */
    net_wifi_sta_params_t wifi_params = {
        .ssid = "",
        .password = "",
        .max_retry = -1, /* < 0 uses CONFIG_NET_CONNECT_WIFI_CONN_MAX_RETRY (default 6) */
        .route_prio = NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO,
        .required = false,
        .auto_connect_on_start = false,
    };
    net_iface_handle_t wifi = NULL;
    ESP_ERROR_CHECK(net_wifi_sta_create(&wifi_params, &wifi));

    /* Start both interfaces in parallel, then wait only for Ethernet */
    ESP_ERROR_CHECK(net_iface_start(eth));
    ESP_ERROR_CHECK(net_iface_start(wifi));
    ESP_ERROR_CHECK(net_iface_wait(eth, 15000));

    /* Later, once Wi-Fi credentials are received (e.g., over BLE, console, or HTTP): */
    ESP_ERROR_CHECK(net_wifi_sta_set_credentials(wifi, "MyHomeSSID", "MyPassword"));
    ESP_ERROR_CHECK(net_iface_connect(wifi));
}
```

#### Default Route Priorities

When multiple interfaces are `READY`, `esp_netif` routes outbound traffic through the interface with the highest `route_prio`:

- **Wi-Fi STA:** `NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO` (`128`)
- **Ethernet:** `NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO` (`64`)
- **PPP:** `NET_CONNECT_PPP_DEFAULT_ROUTE_PRIO` (`32`)
- **OpenThread:** `NET_CONNECT_THREAD_DEFAULT_ROUTE_PRIO` (`16`)

---

## Configuring Each Interface

### Wi-Fi

Enable `CONFIG_NET_CONNECT_WIFI` in `menuconfig` to configure:

- **SSID and password:** Or leave SSID empty (`""`) with `net_wifi_sta_create()` and set credentials later with `net_wifi_sta_set_credentials()`.
- **Retries and scan options:** Maximum retries (`CONFIG_NET_CONNECT_WIFI_CONN_MAX_RETRY`), scan method, RSSI threshold, and authentication mode threshold.

**Chips without built-in Wi-Fi (e.g., ESP32-P4, ESP32-H2):**

- `CONFIG_NET_CONNECT_WIFI` is enabled by default only on chips with native Wi-Fi (`SOC_WIFI_SUPPORTED`).
- On other targets, add [`esp_wifi_remote`](https://components.espressif.com/components/espressif/esp_wifi_remote) (and `esp_hosted`) to your project and enable remote Wi-Fi (`CONFIG_ESP_WIFI_REMOTE_ENABLED` / `CONFIG_ESP_HOST_WIFI_ENABLED`). `CONFIG_NET_CONNECT_WIFI` will then appear in `menuconfig` so you can enable Wi-Fi over SDIO, SPI, or UART.

### Ethernet

Enable Ethernet in `menuconfig` for boards with an internal EMAC (e.g., Espressif Ethernet Kit), a SPI Ethernet module, or OpenCores Ethernet MAC (for QEMU):

- **Hardware and pins:** Configure PHY or SPI pins under the top-level **Ethernet Configuration** menu (provided by `espressif/ethernet_init`).
- **Port selection:** `net_connect` uses the first Ethernet port returned by `ethernet_init_all()`.
- **Cable handling:** `esp_eth_start()` runs in the background task during connection, so an unplugged cable (~4 s PHY autonegotiation timeout) does not block your main task. Unplugging the cable later posts `NET_IFACE_EVENT_LOST_IP` and returns the handle to `CONNECTING` while keeping the driver running.

### OpenThread

- `CONFIG_NET_CONNECT_THREAD` is available when OpenThread (`OPENTHREAD_ENABLED`) is enabled, and defaults to on for chips with a built-in IEEE 802.15.4 radio (such as ESP32-C6 and ESP32-H2). It also works with an [OpenThread RCP](https://github.com/espressif/esp-idf/tree/master/examples/openthread/ot_rcp) build.
- **Mesh dataset:** Configure under `Component config -> OpenThread -> Thread Core Features -> Thread Operational Dataset`.
- **NAT64 / DNS64:** If your Thread Border Router has NAT64 enabled, turn on `Component config -> OpenThread -> Thread Core Features -> Enable DNS64 Client` (this also enables the required lwIP custom DNS resolve hook).

### PPP (Point-to-Point Protocol)

PPP creates a serial IP tunnel over USB CDC or UART to a peer such as a Linux host running `pppd`. For cellular modems that need AT command control, use [`esp_modem`](https://components.espressif.com/component/espressif/esp_modem) instead.

- **USB CDC (**`CONFIG_NET_CONNECT_PPP_DEVICE_USB`**)** — Default on chips with USB OTG (`SOC_USB_OTG_SUPPORTED`: ESP32-S2, ESP32-S3, ESP32-S31, ESP32-P4, ESP32-H4).
  - Add `esp_tinyusb` to your project's `idf_component.yml` (USB mode enables `TINYUSB_CDC_ENABLED`):
    ```yaml
    dependencies:
      espressif/esp_tinyusb: "^2.1.0"
    ```

    Or run: `idf.py add-dependency "espressif/esp_tinyusb^2.1.0"`
  - Appears as `/dev/ttyACMx` on Linux and does not create a UART receive task.
- **UART (**`CONFIG_NET_CONNECT_PPP_DEVICE_UART`**)** — Default on chips without USB OTG (such as ESP32, ESP32-C3, ESP32-C6, ESP32-H2).
  - Do not add `esp_tinyusb` for a UART-only build.
  - Connect via a USB-to-Serial adapter (`/dev/ttyUSBx` on Linux) and configure TX/RX pins (default TX=4, RX=5) and baud rate (default 115200).

For host setup instructions, see the [PPP Connection Example README](examples/ppp_connect_example/README.md).

---

## Reference Details

### Interface Lifecycle & States

Each handle (`net_iface_handle_t`) moves through the states below. You can read the current state with `net_iface_get_state(h)` or monitor transitions with `NET_IFACE_EVENT`:

| State | Meaning |
|---|---|
| `NET_IFACE_STATE_INIT` | Handle created and configuration stored. No driver has started yet. |
| `NET_IFACE_STATE_STARTING` | `net_iface_start()` was called. A temporary background task is initializing the driver and creating `esp_netif`. |
| `NET_IFACE_STATE_STARTED` | Driver and `esp_netif` are ready, but connection has not been requested yet (e.g., `auto_connect_on_start = false` or Wi-Fi SSID is empty). |
| `NET_IFACE_STATE_CONNECTING` | Link setup is in progress (Wi-Fi scanning/associating, Ethernet waiting for link, PPP negotiating, or OpenThread attaching). |
| `NET_IFACE_STATE_CONNECTED` | Physical / Layer-2 link is up; waiting for IP address assignment. |
| `NET_IFACE_STATE_READY` | Network configuration is complete (see details below). |
| `NET_IFACE_STATE_STOPPING` | `net_iface_stop()` is shutting down the driver and background tasks. Returns to `NET_IFACE_STATE_INIT` when finished. |
| `NET_IFACE_STATE_FAILED` | Startup failed or retries exceeded `max_retry`. Call `net_iface_get_last_error(h)` for the `esp_err_t` cause. `net_iface_start()` can be called again from this state. |

#### When is an interface `READY`?

- **Wi-Fi STA, Ethernet, and PPP:**
  - `CONNECTED` means the physical or Layer-2 link is up.
  - `READY` means every IP family enabled for the build is present:
    - **IPv4:** Required when `CONFIG_NET_CONNECT_IPV4` is enabled (enabled by default, except when OpenThread is enabled).
    - **IPv6:** Required when `CONFIG_NET_CONNECT_IPV6=y` and the handle has `ip.enable_ipv6 = true` (set automatically by the create helpers when `CONFIG_NET_CONNECT_IPV6` is enabled). It waits for an address matching `Preferred IPv6 Type`, which defaults to link-local (`CONFIG_NET_CONNECT_IPV6_PREF_LOCAL_LINK`). Setting `ip.enable_ipv6 = true` in code has no effect if `CONFIG_NET_CONNECT_IPV6` is disabled in `menuconfig`.
- **OpenThread:**
  - `READY` means the device has attached to the Thread mesh (`OPENTHREAD_EVENT_ATTACHED`). It does not wait for an IPv6 address event, even though enabling Thread selects `CONFIG_NET_CONNECT_IPV6`.
- **Note:** `READY` only means local IP or mesh configuration is complete. It does not guarantee internet reachability (for example, a link-local IPv6 address satisfies the default IPv6 check).

### Detailed `net_connect()` Behavior

In `menuconfig` (**Net Connect Configuration**), each enabled interface has a `CONFIG_NET_CONNECT_<IFACE>_REQUIRED` option. When both Ethernet and Wi-Fi are enabled, Ethernet defaults to **required** and Wi-Fi defaults to **optional**.

`net_connect()` starts all enabled interfaces in parallel and waits up to `CONFIG_NET_CONNECT_TIMEOUT_MS` (default `30000` ms; `0` = wait forever):

- **One or more interfaces are required:** Waits until every required interface reaches `READY`. Optional interfaces that are slow, unprovisioned, or fail do not cause `net_connect()` to fail and stay running. If any required interface fails or times out, `net_connect()` stops all interfaces it started and returns `ESP_FAIL`. A required interface with `auto_connect_on_start = false` (such as Wi-Fi with an empty SSID) fails immediately without waiting for the timeout.
- **No interfaces are required:** Waits until any auto-connecting interface reaches `READY`. If none succeed in time, `net_connect()` stops them and returns `ESP_FAIL`. If nothing is set to auto-connect, it returns `ESP_OK` immediately.
- **Wi-Fi credentials from stdin (**`CONFIG_NET_CONNECT_WIFI_SSID_PWD_FROM_STDIN`**):** Starts Ethernet first, then waits for `ssid password` on stdin before starting Wi-Fi, Thread, and PPP.
- **All interfaces disabled in** `menuconfig`**:** On device targets, `net_connect()` and `net_disconnect()` return `ESP_OK` without touching hardware, and `net_get_netif()` is not defined.
- **Linux target:** Only a `net_connect()` stub (returning `ESP_OK`) is compiled. `net_disconnect()` and `net_iface.h` are not available on Linux.

### Resource & Task Usage

- **Temporary startup task (all interfaces):** `net_iface_start()` creates a short-lived FreeRTOS task (`CONFIG_NET_CONNECT_STARTUP_TASK_STACK_SIZE`, default `4096` bytes, priority 5) that initializes the driver, starts connecting if `auto_connect_on_start` is set, records its stack high-water mark, and deletes itself. Wi-Fi and Ethernet do not keep any `net_connect` task running afterward (the underlying drivers still run their own tasks).
- **Long-lived tasks (PPP over UART and OpenThread only):**
  - **PPP over UART:** Keeps one `ppp connect` receive task (`CONFIG_NET_CONNECT_PPP_TASK_STACK_SIZE`, default `4096` bytes) until `net_iface_stop()`.
  - **OpenThread:** Keeps one `ot_br_main` task (`CONFIG_NET_CONNECT_THREAD_TASK_STACK_SIZE`, default `8192` bytes) until `net_iface_stop()`. If the OpenThread CLI is enabled, `esp_openthread_cli_create_task()` also starts a `4096`-byte `ot_cli` task that runs until reset (`net_iface_stop()` does not delete it).
