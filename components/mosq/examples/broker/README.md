# MQTT broker example

## Overview

This example runs a TCP broker on a specified host and port.

### How to use this example

Choose the preferred connection in the project configuration menu, build and run the example.
After it connects vias the specified network interface, it start the mosquitto broker, which by default listens on the connected interface.

If you enabled also the mqtt client, this example will connect the the localbroker and subscribe and publish to a topic.

You can connect to the ESP32 mosquitto broker using some other client using the ESP32 IPv4 address and the port specified in the project configuration menu.

### Test version

This example is also used for testing on loopback interface, disabling the connection and running the broker listening to `"127.0.0.1"` and connecting the client to the that interface.

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
I (1527) test: MQTT_EVENT_CONNECTED
I (1527) test: sent publish successful, msg_id=42466
I (1527) test: sent subscribe successful, msg_id=38406
I (1537) test: sent subscribe successful, msg_id=63973
I (1537) test: sent unsubscribe successful, msg_id=58859
I (1557) test: MQTT_EVENT_PUBLISHED, msg_id=42466
I (1557) test: MQTT_EVENT_SUBSCRIBED, msg_id=38406
I (1567) test: sent publish successful, msg_id=0
I (1577) test: MQTT_EVENT_SUBSCRIBED, msg_id=63973
I (1577) test: sent publish successful, msg_id=0
I (1587) test: MQTT_EVENT_UNSUBSCRIBED, msg_id=58859
I (1757) test: MQTT_EVENT_DATA
TOPIC=/topic/qos0
DATA=data
I (1757) test: MQTT_EVENT_DATA
TOPIC=/topic/qos0
DATA=data
```
