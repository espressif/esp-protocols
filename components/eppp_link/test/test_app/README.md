
# Test application running both server and client on the same device

Need to connect client's Tx to server's Rx and vice versa:
GPIO25 - GPIO4
GPIO26 - GPIO5

We wait for the connection and then we start pinging the client's address on server's netif.

## Example of output:

```
I (393) eppp_test_app: [APP] Startup..
I (393) eppp_test_app: [APP] Free memory: 296332 bytes
I (393) eppp_test_app: [APP] IDF version: v5.3-dev-1154-gf14d9e7431-dirty
I (423) uart: ESP_INTR_FLAG_IRAM flag not set while CONFIG_UART_ISR_IN_IRAM is enabled, flag updated
I (423) uart: queue free spaces: 16
I (433) eppp_link: Waiting for IP address
I (433) uart: ESP_INTR_FLAG_IRAM flag not set while CONFIG_UART_ISR_IN_IRAM is enabled, flag updated
I (443) uart: queue free spaces: 16
I (443) eppp_link: Waiting for IP address
I (6473) esp-netif_lwip-ppp: Connected
I (6513) eppp_link: Got IPv4 event: Interface "pppos_client" address: 192.168.11.2
I (6523) esp-netif_lwip-ppp: Connected
I (6513) eppp_link: Connected!
I (6523) eppp_link: Got IPv4 event: Interface "pppos_server" address: 192.168.11.1
I (6553) main_task: Returned from app_main()
64bytes from 192.168.11.2 icmp_seq=1 ttl=255 time=18 ms
64bytes from 192.168.11.2 icmp_seq=2 ttl=255 time=19 ms
64bytes from 192.168.11.2 icmp_seq=3 ttl=255 time=19 ms
64bytes from 192.168.11.2 icmp_seq=4 ttl=255 time=20 ms
64bytes from 192.168.11.2 icmp_seq=5 ttl=255 time=19 ms
64bytes from 192.168.11.2 icmp_seq=6 ttl=255 time=19 ms
64bytes from 192.168.11.2 icmp_seq=7 ttl=255 time=19 ms
From 192.168.11.2 icmp_seq=8 timeout        // <-- Disconnected Tx-Rx wires
From 192.168.11.2 icmp_seq=9 timeout
```
## Test cases

This test app exercises these methods of setting up server-client connection:
* simple blocking API (eppp_listen() <--> eppp_connect()): Uses network events internally and waits for connection
* simplified non-blocking API (eppp_open(EPPP_SERVER, ...) <--> eppp_open(EPPP_SERVER, ...) ): Uses events internally, optionally waits for connecting
* manual API (eppp_init(), eppp_netif_start(), eppp_perform()): User to manually drive Rx task
    - Note that the ping test for this test case takes longer, since we call perform for both server and client from one task, for example:

```
TEST(eppp_test, open_close_taskless)I (28562) uart: ESP_INTR_FLAG_IRAM flag not set while CONFIG_UART_ISR_IN_IRAM is enabled, flag updated
I (28572) uart: ESP_INTR_FLAG_IRAM flag not set while CONFIG_UART_ISR_IN_IRAM is enabled, flag updated
Note: esp_netif_init() has been called. Until next reset, TCP/IP task will periodicially allocate memory and consume CPU time.
I (28602) uart: ESP_INTR_FLAG_IRAM flag not set while CONFIG_UART_ISR_IN_IRAM is enabled, flag updated
I (28612) uart: queue free spaces: 16
I (28612) uart: ESP_INTR_FLAG_IRAM flag not set while CONFIG_UART_ISR_IN_IRAM is enabled, flag updated
I (28622) uart: queue free spaces: 16
I (28642) esp-netif_lwip-ppp: Connected
I (28642) esp-netif_lwip-ppp: Connected
I (28642) test: Got IPv4 event: Interface "pppos_server(EPPP0)" address: 192.168.11.1
I (28642) esp-netif_lwip-ppp: Connected
I (28652) test: Got IPv4 event: Interface "pppos_client(EPPP1)" address: 192.168.11.2
I (28662) esp-netif_lwip-ppp: Connected
64bytes from 192.168.11.2 icmp_seq=1 ttl=255 time=93 ms
64bytes from 192.168.11.2 icmp_seq=2 ttl=255 time=98 ms
64bytes from 192.168.11.2 icmp_seq=3 ttl=255 time=99 ms
64bytes from 192.168.11.2 icmp_seq=4 ttl=255 time=99 ms
64bytes from 192.168.11.2 icmp_seq=5 ttl=255 time=99 ms
5 packets transmitted, 5 received, time 488ms
I (29162) esp-netif_lwip-ppp: User interrupt
I (29162) test: Disconnected interface "pppos_client(EPPP1)"
I (29172) esp-netif_lwip-ppp: User interrupt
I (29172) test: Disconnected interface "pppos_server(EPPP0)"
MALLOC_CAP_8BIT usage: Free memory delta: 0 Leak threshold: -64
MALLOC_CAP_32BIT usage: Free memory delta: 0 Leak threshold: -64
 PASS
```
