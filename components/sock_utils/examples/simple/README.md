# Simple example

This example demonstrates using socket helpers on ESP_PLATFORM

The functionality of this example is meant to be very generic, so the application is compilable on linux without any IDF dependencies:
```bash
gcc main/app.c -lpthread
```

## Example output on ESP_PLATFORM

```
I (8073) esp_netif_handlers: example_netif_sta ip: 192.168.0.32, mask: 255.255.255.0, gw: 192.168.0.1
I (8073) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.0.32
I (8073) example_common: Connected to example_netif_sta
I (8083) example_common: - IPv4 address: 192.168.0.32,
I (8093) example_common: - IPv6 address: fe80:0000:0000:0000:XXXX, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (8103) sock_utils_example: Received signal: IP4
I (8103) sock_utils_example: IPv4 address of interface "sta": 192.168.0.32
I (8113) main_task: Returned from app_main()
```

## Example output on linux platform

```
I[sock_utils_example]: Received signal: IP4
I[sock_utils_example]: IPv4 address of interface "lo": 127.0.0.1
I[sock_utils_example]: IPv4 address of interface "en0": 192.168.0.28
I[sock_utils_example]: IPv4 address of interface "docker0": 172.17.0.1
```
