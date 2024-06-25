# mqtt command

## Overview

This example demonstrates the usage of mqtt command component.
To set up the example with Wi-Fi, Ethernet, or both, go to the project configuration menu (idf.py menuconfig) and choose "Example Connection Configuration." Then pick either Wi-Fi, Ethernet, or both under "Connect using.

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
esp> mqtt -h mqtt://192.168.50.185 -C
I (1678559) console_mqtt: broker: mqtt://192.168.50.185
I (1678559) console_mqtt: MQTT_EVENT_BEFORE_CONNECT
esp> I (1678849) console_mqtt: MQTT_EVENT_CONNECTED
esp>
esp> mqtt -D
I (1691939) console_mqtt: mqtt client disconnected
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

#### Receiving Data Event:
```
esp> I (999999) console_mqtt: MQTT_EVENT_DATA
I (999999) console_mqtt: TOPIC=test0

I (999999) console_mqtt: DATA=Hello, Testing 123
```
