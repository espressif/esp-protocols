# Websocket LWS client example

This example will shows how to set up and communicate over a websocket.

## How to Use Example

### Hardware Required

This example can be executed on any ESP32 board, the only required interface is WiFi and connection to internet or a local server.

### Configure the project

* Open the project configuration menu (`idf.py menuconfig`)
* Configure Wi-Fi or Ethernet under "Example Connection Configuration" menu.
* Configure the websocket endpoint URI under "Example Configuration"

### Server Certificate Verification

* Mutual Authentication: When `CONFIG_WS_OVER_TLS_MUTUAL_AUTH=y` is enabled, it's essential to provide valid certificates for both the server and client.
  This ensures a secure two-way verification process.
* Server-Only Authentication: To perform verification of the server's certificate only (without requiring a client certificate), set `CONFIG_WS_OVER_TLS_SERVER_AUTH=y`.
  This method skips client certificate verification.
* Example below demonstrates how to generate a new self signed certificates for the server and client using the OpenSSL command line tool

Please note: This example represents an extremely simplified approach to generating self-signed certificates/keys with a single common CA, devoid of CN checks, lacking password protection, and featuring hardcoded key sizes and types. It is intended solely for testing purposes.
In the outlined steps, we are omitting the configuration of the CN (Common Name) field due to the context of a testing environment. However, it's important to recognize that the CN field is a critical element of SSL/TLS certificates, significantly influencing the security and efficacy of HTTPS communications. This field facilitates the verification of a website's identity, enhancing trust and security in web interactions. In practical deployments beyond testing scenarios, ensuring the CN field is accurately set is paramount for maintaining the integrity and reliability of secure communications

It is **strongly recommended** to not reuse the example certificate in your application;
it is included only for demonstration.


### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output

```
I (18208) lws-client: LWS minimal ws client echo

219868: lws_create_context: LWS: 4.3.99-v4.3.0-424-ga74362ff, MbedTLS-3.6.2 NET CLI SRV H1 H2 WS SS-JSON-POL ConMon IPv6-absent
219576:  mem: platform fd map:    20 bytes
217880: __lws_lc_tag:  ++ [wsi|0|pipe] (1)
216516: __lws_lc_tag:  ++ [vh|0|default||-1] (1)
I (18248) lws-client: connect_cb: connecting

210112: __lws_lc_tag:  ++ [wsicli|0|WS/h1/default/echo.websocket.events] (1)
204800: [wsicli|0|WS/h1/default/echo.websocket.events]: lws_client_connect_3_connect: trying 13.248.241.119
180776: lws_ssl_client_bio_create: allowing selfsigned
I (19998) wifi:<ba-add>idx:0 (ifx:0, b4:89:01:63:9d:08), tid:0, ssn:321, winSize:64
I (20768) lws-client: WEBSOCKET_EVENT_CONNECTED
I (20768) lws-client: Sending hello 0000
I (20778) lws-client: WEBSOCKET_EVENT_DATA
W (20778) lws-client: Received=echo.websocket.events sponsored by Lob.com


I (20968) lws-client: WEBSOCKET_EVENT_DATA
W (20968) lws-client: Received=hello 0000


I (22978) lws-client: Sending hello 0001
I (23118) lws-client: WEBSOCKET_EVENT_DATA
W (23118) lws-client: Received=hello 0001


I (23778) lws-client: Sending hello 0002
I (23938) lws-client: WEBSOCKET_EVENT_DATA
W (23938) lws-client: Received=hello 0002


I (25948) lws-client: Sending hello 0003
I (26088) lws-client: WEBSOCKET_EVENT_DATA
W (26088) lws-client: Received=hello 0003


I (26948) lws-client: Sending hello 0004
I (27118) lws-client: WEBSOCKET_EVENT_DATA
W (27118) lws-client: Received=hello 0004
```
