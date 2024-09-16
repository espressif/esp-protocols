# API Reference

## Header files

- [mosq_broker.h](#file-mosq_brokerh)

## File mosq_broker.h





## Structures and Types

| Type | Name |
| ---: | :--- |
| struct | [**mosq\_broker\_config**](#struct-mosq_broker_config) <br>_Mosquitto configuration structure._ |

## Functions

| Type | Name |
| ---: | :--- |
|  int | [**mosq\_broker\_start**](#function-mosq_broker_start) (struct [**mosq\_broker\_config**](#struct-mosq_broker_config) \*config) <br>_Start mosquitto broker._ |


## Structures and Types Documentation

### struct `mosq_broker_config`

_Mosquitto configuration structure._

ESP port of mosquittto supports only the options in this configuration structure.

Variables:

-  char \* host  <br>Address on which the broker is listening for connections

-  int port  <br>Port number of the broker to listen to


## Functions Documentation

### function `mosq_broker_start`

_Start mosquitto broker._
```c
int mosq_broker_start (
    struct mosq_broker_config *config
)
```


This API runs the broker in the calling thread and blocks until the mosquitto exits.



**Parameters:**


* `config` Mosquitto configuration structure


**Returns:**

int Exit code (0 on success)
