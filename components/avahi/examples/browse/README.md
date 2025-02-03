# Initial avahi-browse on ESP32

Running a simplified avahi-server with service browser checking on `_http._tcp` service.

## on your PC

```
$ avahi-publish-service my-test _http._tcp 80
Established under name 'my-test'
Got SIGINT, quitting.
```

## on ESP32

```
I (3466) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3516) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (4556) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:96e6:86ff:fe63:0a0c, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (6056) esp_netif_handlers: example_netif_sta ip: 192.168.0.35, mask: 255.255.255.0, gw: 192.168.0.1
I (6056) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.0.35
I (6056) example_common: Connected to example_netif_sta
I (6066) example_common: - IPv4 address: 192.168.0.35,
I (6066) example_common: - IPv6 address: fe80:0000:0000:0000:xxxx:xxxx:xxxx:xxxx, type: ESP_IP6_ADDR_IS_LINK_LOCAL
Interface monitor initialized with a single WIFI_STA_DEF interface
Joining mDNS multicast group on interface WIFI_STA_DEF.IPv4 with address 192.168.0.35.
New relevant interface WIFI_STA_DEF.IPv4 for mDNS.
I (12746) AVAHI: SB1: (1.IPv4) <my-test> as <_http._tcp> in <local> [NEW] cached=0
I (15936) AVAHI: SB1: (1.IPv4) <my-test> as <_http._tcp> in <local> [REMOVE] cached=0
```
