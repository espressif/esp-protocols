# UDP echo client

This is a quick-and-dirty example to demonstrate the bare minimum required to
communicate between two ESP-32 devices over LoWPAN6 BLE.

These examples were developed and tested using ESP-IDF v5.1.2.

These code samples are based on:
* client
    * [bleprph](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/bleprph), demonstrating how to do GAP advertisement using NimBLE on ESP-IDF
    * [udp_client](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets/udp_client), demonstrating how to write a UDP client using esp-lwip's sockets API
* server
    * [blecent](https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/nimble/blecent), demonstrating how to do GAP discovery using NimBLE on ESP-IDF
    * [udp_server](https://github.com/espressif/esp-idf/tree/master/examples/protocols/sockets/udp_server), demonstrating how to write a UDP server using esp-lwip's sockets API
* both: the boilerplate required to hook up the lowpan6_ble driver provided by this component

You'll need to run both of these examples at the same time. For example, if you
had two ESP-32 devices connected via a USB-to-serial adapter:

```console
cd client
idf.py build flash monitor -p /dev/ttyUSB0
```

```console
cd server
idf.py build flash monitor -p /dev/ttyUSB1
```

Replace these with whatever invocations are appropriate for your setup.

The server will:
* start a UDP echo server on port 1234
* scan for devices supporting the Internet Protocol Support Service (IPSS, GATT
  service UUID 0x1820)
* establish a LoWPAN6 BLE connection to the first such device it finds

The client will:
* initiate GAP advertisement with support for IPSS
* after being connected to, start sending messages to the server at port 1234


## Example output

Example client logs:
```console
I (550) main_task: Started on CPU0
I (560) main_task: Calling app_main()
I (600) BTDM_INIT: BT controller compile version [ec4ac65]
I (610) BTDM_INIT: Bluetooth MAC: 55:44:33:22:11:00
I (610) phy_init: phy_version 4780,16b31a7,Sep 22 2023,20:42:16
I (930) main: BLE host task started
I (930) lowpan6_ble: (lowpan6_ble_create) creating lowpan6_ble driver
I (930) main: netif not up, waiting...
I (960) NimBLE: Failed to restore IRKs from store; status=8

I (970) NimBLE: GAP procedure initiated: advertise;
I (970) NimBLE: disc_mode=2
I (970) NimBLE:  adv_channel_map=0 own_addr_type=0 adv_filter_policy=0 adv_itvl_min=0 adv_itvl_max=0
I (980) NimBLE:

I (1240) main: connection established; status=0
I (1940) main: sending to FE80::0011:22FF:FE33:4455
I (2020) main: Received 22 bytes: `echo: hello it's me!!!`
I (4020) main: sending to FE80::0011:22FF:FE33:4455
I (4120) main: Received 22 bytes: `echo: hello it's me!!!`
I (6120) main: sending to FE80::0011:22FF:FE33:4455
I (6220) main: Received 22 bytes: `echo: hello it's me!!!`

... and so on, forever ...
```

Example server logs:
```console
I (552) main_task: Started on CPU0
I (562) main_task: Calling app_main()
I (602) BTDM_INIT: BT controller compile version [ec4ac65]
I (602) BTDM_INIT: Bluetooth MAC: 00:11:22:33:44:55
I (602) phy_init: phy_version 4780,16b31a7,Sep 22 2023,20:42:16
I (912) main: BLE host task started
I (912) lowpan6_ble: (lowpan6_ble_create) creating lowpan6_ble driver
I (922) main: listening on port 1234
I (922) main: waiting to receive...
I (952) NimBLE: Failed to restore IRKs from store; status=8

I (952) NimBLE: GAP procedure initiated: discovery;
I (952) NimBLE: own_addr_type=0 filter_policy=0 passive=1 limited=0 filter_duplicates=1
I (962) NimBLE: duration=forever
I (962) NimBLE:

I (2852) NimBLE: GAP procedure initiated: connect;
I (2852) NimBLE: peer_addr_type=0 peer_addr=
I (2852) NimBLE: 55:44:33:22:11:00
I (2852) NimBLE:  scan_itvl=16 scan_window=16 itvl_min=24 itvl_max=40 latency=0 supervision_timeout=256 min_ce_len=0 max_ce_len=0 own_addr_type=0
I (2872) NimBLE:

I (3102) main: BLE GAP connection established
I (3822) main: Received 16 bytes from addr=FE80::5544:33FF:FE22:1100 port=52324: hello it's me!!!
I (3832) main: waiting to receive...
I (5922) main: Received 16 bytes from addr=FE80::5544:33FF:FE22:1100 port=52324: hello it's me!!!
I (5932) main: waiting to receive...
I (8022) main: Received 16 bytes from addr=FE80::5544:33FF:FE22:1100 port=52324: hello it's me!!!
I (8032) main: waiting to receive...

... and so on, forever ...
```
