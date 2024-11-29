| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# ESP-MQTT SSL Authentication Console

This example demonstrates the use of the MQTT command-line component to connect to both secured and unsecured MQTT brokers. It provides multiple modes of connection, including:

* Unsecured transport: Connect to a broker without encryption.
* SSL/TLS transport: Securely connect using SSL/TLS with options for:
  * Validating the broker using a provided CA certificate.
  * Validating the broker using the internal certificate bundle.
  * Performing SSL mutual authentication using client and broker certificates.

Additionally, the example allows subscribing to topics, unsubscribing from topics, and publishing messages to a specified topic through commands. Connections to the broker at test.mosquitto.org are used to demonstrate these features.
(Please note that the public broker is maintained by the community so may not be always available, for details please visit http://test.mosquitto.org)

It uses ESP-MQTT library which implements mqtt client to connect to mqtt broker.

## How to use example

### Hardware Required

This example can be executed on any ESP32 board, the only required interface is WiFi and connection to internet.

### Configure the project

* Open the project configuration menu (`idf.py menuconfig`)
* Configure Wi-Fi or Ethernet under "Example Connection Configuration" menu. See "Establishing Wi-Fi or Ethernet Connection" section in [examples/protocols/README.md](../../README.md) for more details.

* Generate your client keys and certificate (specific to testing with Mosquitto broker)

Note: The following steps are for testing with the Mosquitto broker. If you're using a different broker, you may need to adapt the steps to meet your broker's certificate and key requirements.

#### Steps for SSL Mutual authentication:
Navigate to the certs directory

```
cd certs
```

Generate a client key and a CSR. When you are generating the CSR, do not use the default values. At a minimum, the CSR must include the Country, Organisation and Common Name fields.

```
openssl genrsa -out client.key
openssl req -out client.csr -key client.key -new
```

Paste the generated CSR in the [Mosquitto test certificate signer](https://test.mosquitto.org/ssl/index.php), click Submit and copy the downloaded `client.crt` in the `main` directory.

Please note, that the supplied files `client.crt` and `client.key` in the `main` directory are only placeholders for your client certificate and key (i.e. the example "as is" would compile but would not connect to the broker)

The broker certificate `mosquitto.org.pem` can be downloaded in pem format from [mosquitto.org.crt](https://test.mosquitto.org/ssl/mosquitto.org.crt). Convert it to `mosquitto.org.pem` simply by renaming it.

Note: If your certificate and key file names differ, update the root `CMakeLists.txt` file and main/`mqtt_ssl_auth_console.c` accordingly.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

Warning: This example might need a bigger app partition size if you're compiling it for debug. To ensere this issue doesn't happen "optimize for size is enabled in menuconfig.

### Command Usage:
```
esp> help
help  [<string>]
  Print the summary of all registered commands if no arguments are given,
  otherwise print summary of given command.
      <string>  Name of command

mqtt  [-CsD] [-h <host>] [-u <username>] [-P <password>] [--cert] [--key] [--cafile]
  mqtt command
  -C, --connect  Connect to a broker (flag, no argument)
  -h, --host=<host>  Specify the host uri to connect to
  -s, --status  Displays the status of the mqtt client (flag, no argument)
  -u, --username=<username>  Provide a username to be used for authenticating with the broker
  -P, --password=<password>  Provide a password to be used for authenticating with the broker
      --cert  Define the PEM encoded certificate for this client, if required by the broker (flag, no argument)
      --key  Define the PEM encoded private key for this client, if required by the broker (flag, no argument)
      --cafile  Define the PEM encoded CA certificates that are trusted (flag, no argument)
      --use-internal-bundle  Use the internal certificate bundle for TLS (flag, no argument)
  -D, --disconnect  Disconnect from the broker (flag, no argument)

mqtt_pub  [-t <topic>] [-m <message>]
  mqtt publish command
  -t, --topic=<topic>  Topic to Subscribe/Publish
  -m, --message=<message>  Message to Publish

mqtt_sub  [-U] [-t <topic>]
  mqtt subscribe command
  -t, --topic=<topic>  Topic to Subscribe/Publish
  -U, --unsubscribe  Unsubscribe from a topic
```

### Connection:

#### Connect without Validating the Broker:
This option connects to the broker without validating its certificate. It is not secure.
```
mqtt -h mqtts://test.mosquitto.org -C
```
or
```
mqtt -h mqtts://mqtt.eclipseprojects.io -C
```

#### Validate the Broker using the Internal Certificate Bundle:
This option uses the ESP-IDF's built-in certificate bundle to verify the broker's identity.
```
mqtt -h mqtts://mqtt.eclipseprojects.io -C --use-internal-bundle
```
or
```
mqtt -h mqtts://test.mosquitto.org -C --use-internal-bundle
```

#### Validate the Broker using a Provided CA Certificate:
This option requires you to provide the broker's CA certificate for validation.
```
mqtt -h mqtts://test.mosquitto.org -C --cafile
```


#### SSL Mutual Authentication(encrypted, client certificate required):
This option performs client authentication in addition to broker validation. It requires the client certificate, private key, and broker CA certificate.
```
mqtt -h mqtts://test.mosquitto.org:8884 -C --cert --key --cafile
```
or
```
mqtt -h mqtts://test.mosquitto.org:8884 -C --cert --key --use-internal-bundle
```

Note: In this example, the broker's certificate is included in the certificate bundle (refer to sdkconfig.default).

### Disconnect:
```
esp> mqtt -D
I (1189949) console_mqtt: mqtt client disconnected
```

### Subscribe/Unsubscribe:
```
esp> mqtt_sub -t test0
I (897289) console_mqtt: Subscribe successful, msg_id=57425, topic=test0
esp> I (897799) console_mqtt: MQTT_EVENT_SUBSCRIBED, msg_id=57425
esp>
esp> mqtt_sub -U -t test0
I (902009) console_mqtt: Unsubscribe successful, msg_id=27663, topic=test0
esp> I (902509) console_mqtt: MQTT_EVENT_UNSUBSCRIBED, msg_id=27663
```

### Publish:
```
esp> mqtt_pub -t test0 -m "Hello, Testing 123"
I (999469) console_mqtt: Publish successful, msg_id=55776, topic=test0, data=Hello, Testing 123
I (1000009) console_mqtt: MQTT_EVENT_PUBLISHED, msg_id=55776
esp>
```

### Receiving data event:
```
esp> I (999999) console_mqtt: MQTT_EVENT_DATA
I (999999) console_mqtt: TOPIC=test0

I (999999) console_mqtt: DATA=Hello, Testing 123
```
