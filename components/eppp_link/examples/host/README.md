
# Client side demo of ESP-PPP-Link

This is a basic demo of using esp-mqtt library, but connects to the internet using a PPPoS client. To run this example, you would need a PPP server that provides connectivity to the MQTT broker used in this example (by default a public broker accessible on the internet).

If configured, this example could also run a ping session and an iperf console.


The PPP server could be a Linux computer with `pppd` service or an ESP32 acting like a connection gateway with PPPoS server (see the "slave" project).
