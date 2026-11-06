# net_connect

`net_connect` provides a unified way to bring up and manage **Wi-Fi STA**, **Ethernet**, **OpenThread**, and **PPP (over UART or USB CDC)** network interfaces on ESP-IDF.

You can use this component in two ways depending on your application's needs:

1. **All-in-one helper (`net_connect()` / `net_disconnect()`):**
   Best for protocol examples and simple apps. Starts all Kconfig-enabled interfaces **in parallel** and waits (up to `CONFIG_NET_CONNECT_TIMEOUT_MS`) for the **required** interfaces to become ready.
2. **Non-blocking handle API (`net_iface.h`):**
   Best for production and multi-interface apps. Lets you create, start, connect, disconnect, stop, and monitor each interface independently without blocking your main task on slow hardware initialization (such as a 4-second Ethernet PHY autonegotiation timeout when the cable is unplugged).

---

## Interface Lifecycle & Readiness States

Every interface handle (`net_iface_handle_t`) moves through a clear set of states that you can query with `net_iface_get_state(h)` or observe via `NET_IFACE_EVENT` callbacks:

| State | Meaning |
|---|---|
| `NET_IFACE_STATE_INIT` | Handle created in memory and configuration stored. No hardware or driver has been started yet. |
| `NET_IFACE_STATE_STARTING` | `net_iface_start()` was called. A temporary background task is initializing the driver and creating the `esp_netif`. |
| `NET_IFACE_STATE_STARTED` | Driver and `esp_netif` are ready, but connection has not been requested yet (for example, `auto_connect_on_start = false`, or Wi-Fi has an empty SSID waiting for runtime provisioning). |
| `NET_IFACE_STATE_CONNECTING` | Link negotiation is in progress (Wi-Fi scanning/associating, Ethernet waiting for link, PPP negotiating, or OpenThread attaching). |
| `NET_IFACE_STATE_CONNECTED` | Physical / Layer-2 link is up; waiting for IP address assignment. |
| `NET_IFACE_STATE_READY` | Required network configuration is complete (see table below). |
| `NET_IFACE_STATE_STOPPING` | `net_iface_stop()` is shutting down the driver and background tasks. |
| `NET_IFACE_STATE_FAILED` | Driver initialization failed or connection retries exceeded `max_retry`. Call `net_iface_get_last_error(h)` for the `esp_err_t` cause. |

### What `NET_IFACE_STATE_READY` means for each interface

- **Wi-Fi STA, Ethernet, and PPP:**
  - `CONNECTED` means the physical or L2 link is up.
  - `READY` means the interface has obtained its **IPv4 address** (when `CONFIG_NET_CONNECT_IPV4=y`) **and** its **preferred IPv6 address** (when `CONFIG_NET_CONNECT_IPV6=y` or `ip.enable_ipv6 = true`).
- **OpenThread:**
  - `READY` means the device has attached to the Thread mesh (`OPENTHREAD_EVENT_ATTACHED`).
- **Note:** `NET_IFACE_STATE_READY` indicates that local IP or mesh configuration is complete. It does **not** guarantee end-to-end internet reachability.

---

## Usage Pattern 1: Quick All-in-One (`net_connect()`)

Include `"net_connect.h"` and call `net_connect()`:

```c
#include "net_connect.h"

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Starts all Kconfig-enabled interfaces concurrently and waits for required ones */
    ESP_ERROR_CHECK(net_connect());

    /* Retrieve a specific esp_netif by its description if needed */
    esp_netif_t *netif = net_get_netif();

    /* ... run application ... */

    ESP_ERROR_CHECK(net_disconnect());
}
```

### Required vs. Optional Interfaces in Kconfig
- Each enabled interface has a `CONFIG_NET_CONNECT_<IFACE>_REQUIRED` toggle in `menuconfig` (`Net Connect Configuration`).
- When both Ethernet and Wi-Fi are enabled, Ethernet defaults to **required** and Wi-Fi defaults to **optional**.
- `net_connect()` waits up to `CONFIG_NET_CONNECT_TIMEOUT_MS` (default `30000` ms; `0` = wait forever) for all **required** interfaces to reach `NET_IFACE_STATE_READY`. Optional interfaces start in the background without failing `net_connect()` if they are unprovisioned or slow to connect.
- **When all interfaces are disabled in Kconfig:** `net_connect()` returns `ESP_OK` immediately without starting any hardware. This allows a single application codebase to build and run unchanged in offline test modes or on Linux host targets where network hardware is not used.

---

## Usage Pattern 2: Non-Blocking Handle API (`net_iface.h`)

### A. Waiting with `net_iface_wait()`, `net_iface_wait_all()`, or `net_iface_wait_any()`

```c
#include "net_iface.h"

net_iface_handle_t eth = NULL;
net_iface_handle_t wifi = NULL;

/* Create handles using Kconfig defaults (pass a params struct to override) */
ESP_ERROR_CHECK(net_eth_create(NULL, &eth));
ESP_ERROR_CHECK(net_wifi_sta_create(NULL, &wifi));

/* Both calls schedule startup in background tasks and return within milliseconds */
ESP_ERROR_CHECK(net_iface_start(eth));
ESP_ERROR_CHECK(net_iface_start(wifi));

/* Wait up to 15 seconds for whichever interface becomes READY first */
net_iface_handle_t ready_iface = NULL;
net_iface_handle_t both[2] = { eth, wifi };
esp_err_t err = net_iface_wait_any(both, 2, 15000, &ready_iface);
if (err == ESP_OK) {
    /* At least one interface is READY! If the other one timed out, it keeps
     * trying to connect in the background unless you call net_iface_stop(). */
}
```

### B. Event-Driven Notifications (`NET_IFACE_EVENT`)

Instead of blocking a task with `net_iface_wait*()`, you can register an event handler on the default ESP event loop:

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
> - **Register before starting, or check current state:** `net_iface_register_event_handler()` does not replay past events. If you register a handler *after* calling `net_iface_start(h)`, call `net_iface_get_state(h)` right after registration to see if the interface already reached `NET_IFACE_STATE_READY` or `NET_IFACE_STATE_FAILED`.
> - **Callback restrictions:** Event callbacks run on the default ESP event loop task. Do **not** call `net_iface_wait*()` or `net_iface_stop()` inside a `NET_IFACE_EVENT` callback (they return `ESP_ERR_INVALID_STATE` to prevent event-loop deadlocks).

---

## Usage Pattern 3: Home-Hub Recipe (Required Ethernet + Optional Wi-Fi + Dynamic Route Priority)

A common gateway/hub scenario boots with **Ethernet** as the primary wired link and **Wi-Fi STA** unprovisioned at boot (`ssid = ""`), then provisions Wi-Fi later at runtime and adjusts the default route priority:

```c
#include "net_iface.h"
#include "esp_netif.h"

static void hub_event_handler(void *arg, esp_event_base_t base,
                              int32_t event_id, void *event_data)
{
    const net_iface_event_data_t *data = (const net_iface_event_data_t *)event_data;

    if (event_id == NET_IFACE_EVENT_READY && data->type == NET_IFACE_TYPE_ETH) {
        /* Example: dynamically boost Ethernet above Wi-Fi (128) when Ethernet is READY */
        esp_netif_set_route_prio(data->netif, 200);
        esp_netif_set_default_netif(NULL); /* Re-evaluate default route by route_prio */
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

    /* 2. Optional, unprovisioned Wi-Fi (empty SSID -> stays in STARTED until credentials are set) */
    net_wifi_sta_params_t wifi_params = {
        .ssid = "",
        .password = "",
        .max_retry = -1,
        .route_prio = NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO,
        .required = false,
        .auto_connect_on_start = false,
    };
    net_iface_handle_t wifi = NULL;
    ESP_ERROR_CHECK(net_wifi_sta_create(&wifi_params, &wifi));

    /* Start both interfaces concurrently */
    ESP_ERROR_CHECK(net_iface_start(eth));
    ESP_ERROR_CHECK(net_iface_start(wifi));

    /* Wait only for required Ethernet */
    ESP_ERROR_CHECK(net_iface_wait(eth, 15000));

    /* Later, when the user provisions Wi-Fi credentials (e.g. over BLE, console, or HTTP): */
    ESP_ERROR_CHECK(net_wifi_sta_set_credentials(wifi, "MyHomeSSID", "MyPassword"));
    ESP_ERROR_CHECK(net_iface_connect(wifi));
}
```

### Default Route Priority Values
By default, `esp_netif` routes outbound traffic through the active (`READY`) interface with the highest `route_prio`:
- **Wi-Fi STA:** `NET_CONNECT_WIFI_DEFAULT_ROUTE_PRIO` (`128`)
- **Ethernet:** `NET_CONNECT_ETH_DEFAULT_ROUTE_PRIO` (`64`)
- **PPP:** `NET_CONNECT_PPP_DEFAULT_ROUTE_PRIO` (`32`)
- **OpenThread:** `NET_CONNECT_THREAD_DEFAULT_ROUTE_PRIO` (`16`)

---

## Interface-Specific Configuration Notes

### Wi-Fi

Choose the Wi-Fi connection method in `menuconfig` and configure:
* Wi-Fi SSID and password (or leave SSID empty `""` when using `net_wifi_sta_create()` to provision later via `net_wifi_sta_set_credentials()`)
* Maximum connection retries (`CONFIG_NET_CONNECT_WIFI_CONN_MAX_RETRY`)
* Wi-Fi scan method, RSSI threshold, and authentication mode threshold

**Using Wi-Fi on chips without an integrated radio (e.g., ESP32-P4, ESP32-H2):**
`CONFIG_NET_CONNECT_WIFI` is automatically available when either native Wi-Fi (`SOC_WIFI_SUPPORTED`) or remote Wi-Fi (`CONFIG_ESP_WIFI_REMOTE_ENABLED` / `CONFIG_ESP_HOST_WIFI_ENABLED` via [`esp_wifi_remote`](https://components.espressif.com/components/espressif/esp_wifi_remote)) is enabled. On non-native Wi-Fi targets, add `esp_wifi_remote` (and `esp_hosted`) to your project dependencies so `net_connect` can drive Wi-Fi over SDIO/SPI/UART using the standard `esp_wifi` API.

### Ethernet

Choose Ethernet connection if your board supports it. Common settings include the Espressif Ethernet Kit (internal EMAC), SPI Ethernet modules (when internal EMAC is unavailable), or OpenCores Ethernet MAC (for QEMU). Configure the PHY/SPI pins under `menuconfig -> Ethernet Configuration` (provided by `espressif/ethernet_init`).

### Thread

Choose Thread connection if your board supports an IEEE 802.15.4 native radio (such as ESP32-C6 or ESP32-H2) or works with an [OpenThread RCP](../../openthread/ot_rcp/README.md). Configure the Thread network dataset in `menuconfig -> Components -> OpenThread -> Thread Core Features -> Thread Operational Dataset`.

If the Thread end-device joins a Thread network with a Thread Border Router that has NAT64 enabled, the end-device can access the internet with standard DNS APIs after enabling:
* `Components -> OpenThread -> Thread Core Features -> Enable DNS64 client`
* `Components -> LWIP -> Hooks -> DNS external resolve Hook -> Custom implementation`

### PPP

Point-to-Point Protocol creates a serial IP tunnel (over UART or USB CDC) to a PPP peer such as a Linux host running `pppd`. For full cellular modem command/control support, prefer [`esp_modem`](https://components.espressif.com/component/espressif/esp_modem).

#### Board-Specific PPP Configuration

**For boards with USB OTG support** (ESP32-S2, ESP32-S3, ESP32-P4, ESP32-H4):
- Use USB CDC mode (`CONFIG_NET_CONNECT_PPP_DEVICE_USB=y`).
- **Required:** Add `esp_tinyusb` to your project's `idf_component.yml`:
  ```yaml
  dependencies:
    espressif/esp_tinyusb: ^2
  ```
  Or run: `idf.py add-dependency espressif/esp_tinyusb^2`
- The device appears as `/dev/ttyACMx` on Linux.

**For boards without USB OTG support** (ESP32, ESP32-C3, ESP32-C6, ESP32-C61):
- Use UART mode (`CONFIG_NET_CONNECT_PPP_DEVICE_UART=y`).
- **Required:** Do **not** include `esp_tinyusb` in your project's `idf_component.yml`.
- Connect via a USB-to-Serial adapter (`/dev/ttyUSBx` on Linux) and configure TX/RX pins (default TX=4, RX=5) and baudrate (default 115200).

For detailed PPP server setup and examples, see the [PPP Connection Example README](examples/ppp_connect_example/README.md).

---

## Resource & Task Footprint

- **Per-interface startup task:** Calling `net_iface_start()` spawns a short-lived FreeRTOS task (`CONFIG_NET_CONNECT_STARTUP_TASK_STACK_SIZE`, default `4096` bytes) that runs blocking driver initialization (`esp_eth_driver_install`, `esp_eth_start`, `esp_wifi_init`, `tinyusb_driver_install`, etc.) and deletes itself immediately once startup completes. The unit test `net_iface_startup_task_stack_high_water_mark` requires at least 512 bytes of that stack to stay unused.
- **Persistent tasks:** Wi-Fi and Ethernet use zero persistent tasks in `net_connect` once started. PPP over UART runs one RX task (`CONFIG_NET_CONNECT_PPP_TASK_STACK_SIZE`, default `4096` bytes) and OpenThread runs one mainloop task (`CONFIG_NET_CONNECT_THREAD_TASK_STACK_SIZE`, default `8192` bytes) while active; both exit cleanly on `net_iface_stop()`.
