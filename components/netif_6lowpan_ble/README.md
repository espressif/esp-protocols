# LoWPAN6 BLE Netif

This component provides a custom ESP-NETIF layer for
[RFC7668](https://datatracker.ietf.org/doc/html/rfc7668), allowing devices to
transport IPv6 over Bluetooth Low Energy links.

> :warning: This is an experimental library, developed for a pretty narrow
> use-case. In particular, we just needed to connect to a peripheral using
> LoWPAN6 for local communications so have not really tested this component as
> a LoWPAN6 client. This component does not implement any routing capabilities:
> this won't allow you to route packets from your personal area network to the
> public internet, for example.

## Usage

This component only supports the NimBLE stack. It also requires support for
L2CAP COC connections. This means your project's `sdkconfig` needs at least:
* NimBLE as the Bluetooth stack (`CONFIG_BT_NIMBLE_ENABLED=y`)
* `CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM` set to a non-zero value

There is some boilerplate required to set up this netif layer.
```c
// Initialize the lowpan6_ble module
ESP_ERROR_CHECK(lowpan6_ble_init());

// Configure a new esp_netif to use the lowpan6_ble netstack
esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_LOWPAN6_BLE();
esp_netif_config_t cfg = {
    .base   = &base_cfg,
    .driver = NULL,
    .stack  = netstack_default_lowpan6_ble,
};
esp_netif_t* lowpan6_ble_netif = esp_netif_new(&cfg);

// Create a lowpan6_ble driver instance. Note that each driver instance only
// supports a single LoWPAN6 BLE channel.
s_l6ble_handle = lowpan6_ble_create();
if (s_l6ble_handle != NULL)
{
    // Finally, attach the driver instance to the netif you created above.
    ESP_ERROR_CHECK(esp_netif_attach(lowpan6_ble_netif, s_l6ble_handle));
}
```

All that is left to bring this netif up is to connect to a BLE peer that
supports LoWPAN6 BLE. You can discover potential peers during, e.g., GAP discovery:

```c
// Note: no error handling included for brevity's sake
static int on_gap_event(struct ble_gap_event* event, void* arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_DISC:
        // Verify that the discovered peer supports LoWPAN6 BLE
        if (lowpan6_ble_connectable(&event->disc))
        {
            // Cancel the current discovery process so we can connect
            ble_gap_disc_cancel();

            lowpan6_ble_connect(
                s_l6ble_handle,       // handle to the driver we created above
                &event->disc.addr,    // peer's BLE address
                BLE_CONNECT_TIMEOUT,  // GAP connect timeout in ms
                on_lowpan6_ble_event, // user-defined callback
                NULL                  // user-defined callback argument
            );
        }
    }
}
```

Once that netif is up, you should be able to communicate to the BLE peer with
generic network interfaces. For example, a UDP socket listening for messages
from any address using LwIP's BSD sockets API:

```c
struct sockaddr_in6 server_addr;

server_addr.sin6_family = AF_INET6;
server_addr.sin6_addr   = in6addr_any;
server_addr.sin6_port   = htons(PORT);

int s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
bind(s, (struct sockaddr*)&server_addr, sizeof(server_addr));
int rc = recvfrom(s, buf, len, 0, client_addr, addr_len);
```

> :warning: If using LwIP's Raw API, be careful to separate message receipt
> from further communications. Raw API callbacks are serviced by the NimBLE
> event handler, which means that trying to send data in a callback can and
> will deadlock as the event handler waits for a BLE unstalled event that will
> never arrive.
