| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# ESP-MQTT SSL Sample application (mutual authentication)

This example demonstrates the usage of mqtt command component.
It connects to the broker test.mosquitto.org using ssl transport with client certificate and as a demonstration subscribes/unsubscribes and send a message on certain topic.
(Please note that the public broker is maintained by the community so may not be always available, for details please visit http://test.mosquitto.org)

It uses ESP-MQTT library which implements mqtt client to connect to mqtt broker.

## How to use example

### Hardware Required

This example can be executed on any ESP32 board, the only required interface is WiFi and connection to internet.

### Configure the project

* Open the project configuration menu (`idf.py menuconfig`)
* Configure Wi-Fi or Ethernet under "Example Connection Configuration" menu. See "Establishing Wi-Fi or Ethernet Connection" section in [examples/protocols/README.md](../../README.md) for more details.

* Generate your client keys and certificate

Navigate to the main directory

```
cd main
```

Generate a client key and a CSR. When you are generating the CSR, do not use the default values. At a minimum, the CSR must include the Country, Organisation and Common Name fields.

```
openssl genrsa -out client.key
openssl req -out client.csr -key client.key -new
```

Paste the generated CSR in the [Mosquitto test certificate signer](https://test.mosquitto.org/ssl/index.php), click Submit and copy the downloaded `client.crt` in the `main` directory.

Please note, that the supplied files `client.crt` and `client.key` in the `main` directory are only placeholders for your client certificate and key (i.e. the example "as is" would compile but would not connect to the broker)

The server certificate `mosquitto.org.crt` can be downloaded in pem format from [mosquitto.org.crt](https://test.mosquitto.org/ssl/mosquitto.org.crt).

Note: Incase your certificate and keys file name differs, please update the root `CMakeLists.txt` file and main/`ssl_mutual_auth.c` accordingly.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.


### Command Usage:
```
esp> help
help  [<string>]
  Print the summary of all registered commands if no arguments are given,
  otherwise print summary of given command.
      <string>  Name of command

mqtt  [-CD] [-h <host>] [-u <username>] [-P <password>]
  mqtt command
  -C, --connect  Connect to a broker
  -h, --host=<host>  Specify the host uri to connect to
  -u, --username=<username>  Provide a username to be used for authenticating with the broker
  -P, --password=<password>  Provide a password to be used for authenticating with the broker
    , --cert  Define the PEM encoded certificate for this client, if required by the server
    , --key  Define the PEM encoded private key for this client, if required by the server
    , --cafile  Define the PEM encoded CA certificates that are trusted
  -D, --disconnect  Disconnect from the broker

mqtt_pub  [-t <topic>] [-m <message>]
  mqtt publish command
  -t, --topic=<topic>  Topic to Subscribe/Publish
  -m, --message=<message>  Message to Publish

mqtt_sub  [-U] [-t <topic>]
  mqtt subscribe command
  -t, --topic=<topic>  Topic to Subscribe/Publish
  -U, --unsubscribe  Unsubscribe from a topic
```

#### Connect/Disconnect:
```
esp> mqtt -h mqtts://test.mosquitto.org:8884 -C --cert --key --cafile
I (668129) console_mqtt: broker: mqtts://test.mosquitto.org:8884
I (668129) console_mqtt: MQTT_EVENT_BEFORE_CONNECT
esp> I (671679) console_mqtt: MQTT_EVENT_CONNECTED
esp>
esp> mqtt -D
I (1189949) console_mqtt: mqtt client disconnected
```

#### Subscribe/Unsubscribe:
```
esp> mqtt_sub -t test0
I (897289) console_mqtt: Subscribe successful, msg_id=57425, topic=test0
esp> I (897799) console_mqtt: MQTT_EVENT_SUBSCRIBED, msg_id=57425
esp>
esp> mqtt_sub -U -t test0
I (902009) console_mqtt: Unsubscribe successful, msg_id=27663, topic=test0
esp> I (902509) console_mqtt: MQTT_EVENT_UNSUBSCRIBED, msg_id=27663
```

#### Publish:
```
esp> mqtt_pub -t test0 -m "Hello, Testing 123"
I (999469) console_mqtt: Publish successful, msg_id=55776, topic=test0, data=Hello, Testing 123
I (1000009) console_mqtt: MQTT_EVENT_PUBLISHED, msg_id=55776
esp>
```

#### Receiving data event:
```
esp> I (999999) console_mqtt: MQTT_EVENT_DATA
I (999999) console_mqtt: TOPIC=test0

I (999999) console_mqtt: DATA=Hello, Testing 123
```
