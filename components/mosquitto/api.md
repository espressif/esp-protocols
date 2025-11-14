# API Reference

## Header files

- [mosq_broker.h](#file-mosq_brokerh)

## File mosq_broker.h





## Structures and Types

| Type | Name |
| ---: | :--- |
| struct | [**mosq\_broker\_config**](#struct-mosq_broker_config) <br>_Mosquitto configuration structure._ |
| typedef int(\* | [**mosq\_connect\_cb\_t**](#typedef-mosq_connect_cb_t)  <br> |
| typedef void(\* | [**mosq\_message\_cb\_t**](#typedef-mosq_message_cb_t)  <br> |

## Functions

| Type | Name |
| ---: | :--- |
|  int | [**mosq\_broker\_run**](#function-mosq_broker_run) (struct [**mosq\_broker\_config**](#struct-mosq_broker_config) \*config) <br>_Start mosquitto broker._ |
|  void | [**mosq\_broker\_stop**](#function-mosq_broker_stop) (void) <br>_Stops running broker._ |


## Structures and Types Documentation

### struct `mosq_broker_config`

_Mosquitto configuration structure._

ESP port of mosquittto supports only the options in this configuration structure.

Variables:

-  mosq\_connect\_cb\_t handle_connect_cb  <br>On connect callback. If configured, user function is called whenever a client attempts to connect. The callback receives client\_id, username, password, and password length. Return 0 to accept the connection, non-zero to reject it.

-  void(\* handle_message_cb  <br>On message callback. If configured, user function is called whenever mosquitto processes a message.

-  const char \* host  <br>Address on which the broker is listening for connections

-  int port  <br>Port number of the broker to listen to

-  esp\_tls\_cfg\_server\_t \* tls_cfg  <br>ESP-TLS configuration (if TLS transport used) Please refer to the ESP-TLS official documentation for more details on configuring the TLS options. You can open the respective docs with this idf.py command: `idf.py docs -sp api-reference/protocols/esp_tls.html`

### typedef `mosq_connect_cb_t`

```c
typedef int(* mosq_connect_cb_t) (const char *client_id, const char *username, const char *password, int password_len);
```

### typedef `mosq_message_cb_t`

```c
typedef void(* mosq_message_cb_t) (char *client, char *topic, char *data, int len, int qos, int retain);
```


## Functions Documentation

### function `mosq_broker_run`

_Start mosquitto broker._
```c
int mosq_broker_run (
    struct mosq_broker_config *config
)
```


This API runs the broker in the calling thread and blocks until the mosquitto exits.



**Parameters:**


* `config` Mosquitto configuration structure


**Returns:**

int Exit code (0 on success)
### function `mosq_broker_stop`

_Stops running broker._
```c
void mosq_broker_stop (
    void
)
```


**Note:**

After calling this API, function mosq\_broker\_run() unblocks and returns.
