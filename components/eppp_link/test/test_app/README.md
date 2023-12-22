
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
