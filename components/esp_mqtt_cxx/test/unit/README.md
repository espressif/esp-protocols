# Test basic mqtt_cxx wrapper operations

## Warning: Linux target not supported, this test works only on target

## Example output
```
I (588) main_task: Started on CPU0
I (598) main_task: Calling app_main()
Randomness seeded to: 374196253
I (608) mqtt_client_cpp: MQTT_EVENT_BEFORE_CONNECT
E (618) esp-tls: [sock=54] delayed connect error: Connection reset by peer
E (618) transport_base: Failed to open a new connection: 32772
E (618) mqtt_client: Error transport connect
I (618) mqtt_client_cpp: MQTT_EVENT_ERROR
E (628) mqtt_client_cpp: Last error reported from esp-tls: 0x8004
E (628) mqtt_client_cpp: Last error captured as transport's socket errno: 0x68
I (638) mqtt_client_cpp: Last errno string (Connection reset by peer)
I (648) mqtt_client_cpp: MQTT_EVENT_DISCONNECTED
===============================================================================
All tests passed (6 assertions in 1 test case)

Test passed!
I (5658) main_task: Returned from app_main()
```
