# MQTT broker example

## Overview

This example runs a broker on TLS or TCP transport, specified host and port.

### How to use this example

Choose the preferred connection in the project configuration menu, build and run the example.
After it connects via the specified network interface, it starts the mosquitto broker, which by default listens on all IPv4 interfaces ("0.0.0.0").

If you enabled also the mqtt client, this example will connect to the local broker, subscribe and publish to a topic.

You can connect to the ESP32 mosquitto broker using some other client using the ESP32 IPv4 address and the port specified in the project configuration menu.

> [!IMPORTANT]
> The certificates and keys provided in this example are intended for testing purposes only. They are self-signed, single-use, and configured with a common name of "127.0.0.1". Do not reuse these credentials in any production or real-world applications, as they are not secure for such environments.

For more information on setting up TLS configuration (including certificates and keys), please refer to the ESP-TLS documentation:
```bash
idf.py docs -sp api-reference/protocols/esp_tls.html
```

Configuring the TLS option for the broker is quite similar to setting it up for an HTTPS server, as both involve server-side security configuration. Refer to the HTTPS server documentation for details:
```bash
idf.py docs -sp api-reference/protocols/esp_https_server.html
```

### Test version

This example is also used for testing on loopback interface only, disabling any actual connection, just using the local mqtt client to the loopback interface.

### Example output

```console
I (477) main_task: Calling app_main()
0: mosquitto version 0.1.0 starting
0: Using default config.
0: Opening ipv4 listen socket on port 1883.
0: mosquitto version 0.1.0 running
I (1507) test: Other event id:7
1: New connection from 127.0.0.1:56424 on port 1883.
1: New client connected from 127.0.0.1:56424 as ESP32_d58788 (p2, c1, k120).
I (1528) mqtt_broker: MQTT_EVENT_CONNECTED
I (1528) mqtt_broker: sent subscribe successful, msg_id=54859
I (1538) mqtt_broker: MQTT_EVENT_SUBSCRIBED, msg_id=54859
I (1538) mqtt_broker: sent publish successful, msg_id=0
I (1548) mqtt_broker: MQTT_EVENT_DATA
I (1548) mqtt_broker: TOPIC=/topic/qos0
I (1548) mqtt_broker: DATA=data
```
