# Websocket Sample application

This example shows how to set up and communicate over a websocket.

## Table of Contents

- [Hardware Required](#hardware-required)
- [Configure the project](#configure-the-project)
- [Pre-configured SDK Configurations](#pre-configured-sdk-configurations)
- [Server Certificate Verification](#server-certificate-verification)
- [Generating Self-signed Certificates](#generating-a-self-signed-certificates-with-openssl)
- [Build and Flash](#build-and-flash)
- [Testing with pytest](#testing-with-pytest)
- [Example Output](#example-output)
- [WebSocket Test Server](#websocket-test-server)
- [Python Flask Echo Server](#alternative-python-flask-echo-server)

## Quick Start

1. **Install dependencies:**
   ```bash
   pip install -r esp-protocols/ci/requirements.txt
   ```

2. **Configure and build:**
   ```bash
   idf.py menuconfig  # Configure WiFi/Ethernet and WebSocket URI
   idf.py build
   ```

3. **Flash and monitor:**
   ```bash
   idf.py -p PORT flash monitor
   ```

4. **Run tests:**
   ```bash
   pytest .
   ```

## How to Use Example

### Hardware Required

This example can be executed on any ESP32 board, the only required interface is WiFi and connection to internet or a local server.

### Configure the project

* Open the project configuration menu (`idf.py menuconfig`)
* Configure Wi-Fi or Ethernet under "Example Connection Configuration" menu.
* Configure the websocket endpoint URI under "Example Configuration", if "WEBSOCKET_URI_FROM_STDIN" is selected then the example application will connect to the URI it reads from stdin (used for testing)
* To test a WebSocket client example over TLS, please enable one of the following configurations: `CONFIG_WS_OVER_TLS_MUTUAL_AUTH` or `CONFIG_WS_OVER_TLS_SERVER_AUTH`. See the sections below for more details.

### Pre-configured SDK Configurations

This example includes several pre-configured `sdkconfig.ci.*` files for different testing scenarios:

* **sdkconfig.ci** - Default configuration with WebSocket over Ethernet (IP101 PHY, ESP32, IPv6) and hardcoded URI.
* **sdkconfig.ci.plain_tcp** - WebSocket over plain TCP (no TLS, URI from stdin) using Ethernet (IP101 PHY, ESP32, IPv6).
* **sdkconfig.ci.mutual_auth** - WebSocket with mutual TLS authentication (client/server certificate verification, skips CN check) and URI from stdin.
* **sdkconfig.ci.dynamic_buffer** - WebSocket with dynamic buffer allocation, Ethernet (IP101 PHY, ESP32, IPv6), and hardcoded URI.

Example:
```
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.ci.plain_tcp" build
```

### Server Certificate Verification

* Mutual Authentication: When `CONFIG_WS_OVER_TLS_MUTUAL_AUTH=y` is enabled, it's essential to provide valid certificates for both the server and client.
  This ensures a secure two-way verification process.
* Server-Only Authentication: To perform verification of the server's certificate only (without requiring a client certificate), set `CONFIG_WS_OVER_TLS_SERVER_AUTH=y`.
  This method skips client certificate verification.
* Example below demonstrates how to generate a new self signed certificates for the server and client using the OpenSSL command line tool

Please note: This example represents an extremely simplified approach to generating self-signed certificates/keys with a single common CA, devoid of CN checks, lacking password protection, and featuring hardcoded key sizes and types. It is intended solely for testing purposes.
In the outlined steps, we are omitting the configuration of the CN (Common Name) field due to the context of a testing environment. However, it's important to recognize that the CN field is a critical element of SSL/TLS certificates, significantly influencing the security and efficacy of HTTPS communications. This field facilitates the verification of a website's identity, enhancing trust and security in web interactions. In practical deployments beyond testing scenarios, ensuring the CN field is accurately set is paramount for maintaining the integrity and reliability of secure communications

### Generating a self signed Certificates with OpenSSL manually
* The example below outlines the process for creating new certificates for both the server and client using OpenSSL, a widely-used command line tool for implementing TLS protocol:

```
Generate the CA's Private Key;
openssl genrsa -out ca_key.pem 2048

Create the CA's Certificate
openssl req -new -x509 -days 3650 -key ca_key.pem -out ca_cert.pem

Generate the Server's Private Key
openssl genrsa -out server_key.pem 2048

Generate a Certificate Signing Request (CSR) for the Server
openssl req -new -key server_key.pem -out server_csr.pem

Sign the Server's CSR with the CA's Certificate
openssl x509 -req -days 3650 -in server_csr.pem -CA ca_cert.pem -CAkey ca_key.pem -CAcreateserial -out server_cert.pem

Generate the Client's Private Key
openssl genrsa -out client_key.pem 2048

Generate a Certificate Signing Request (CSR) for the Client
openssl req -new -key client_key.pem -out client_csr.pem

Sign the Client's CSR with the CA's Certificate
openssl x509 -req -days 3650 -in client_csr.pem -CA ca_cert.pem -CAkey ca_key.pem -CAcreateserial -out client_cert.pem

```

Expiry time and metadata fields can be adjusted in the invocation.

Please see the openssl man pages (man openssl) for more details.

It is **strongly recommended** to not reuse the example certificate in your application;
it is included only for demonstration.

### Certificate Generation Options

#### Option 1: Manual OpenSSL Commands
Follow the step-by-step process in the section above to understand certificate generation.

#### Option 2: Automated Script
**Note:** Test certificates are already available in the example. If you want to regenerate them or create new ones, use the provided `generate_certs.sh` script:

```bash
# Auto-detect local IP address (recommended for network testing)
./generate_certs.sh

# Specify custom hostname or IP address
./generate_certs.sh 192.168.1.100

# Use localhost (for local-only testing)
./generate_certs.sh localhost
```

This script automatically generates all required certificates in the correct directories and cleans up temporary files.

**Important:** The server certificate's Common Name (CN) must match the hostname or IP address that ESP32 clients use to connect. If not specified, the script attempts to auto-detect your local IP address. Certificate verification will fail if there's a mismatch between the CN and the actual connection address.

**CN Mismatch Handling:**
If you encounter certificate verification failures due to CN mismatch, you have two options:

1. **Recommended (Secure):** Regenerate certificates with the correct CN:
   ```bash
   ./generate_certs.sh <actual_hostname_or_ip>
   ```

2. **Testing Only (Less Secure):** Skip CN verification by enabling `CONFIG_WS_OVER_TLS_SKIP_COMMON_NAME_CHECK=y` in `idf.py menuconfig`.

   ⚠️ **WARNING:** This option disables an important security check and should **NEVER** be used in production environments. It makes your application vulnerable to man-in-the-middle attacks.

#### Option 3: Online Certificate Generators
- **mkcert**: `install mkcert` then `mkcert -install` and `mkcert localhost`
- **Let's Encrypt**: For production certificates (free, automated renewal)
- **Online generators**: Search for "self-signed certificate generator" online

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

## Testing with pytest

### Install Dependencies

Before running the pytest tests, you need to install the required Python packages:

```
pip install -r esp-protocols/ci/requirements.txt
```

### Run pytest

After installing the dependencies, you can run the pytest tests:

Run all tests in current directory:
```
pytest .
```

Run specific test file:
```
pytest pytest_websocket.py
```

To specify the target device or serial port, use:
```
pytest --target esp32 --port /dev/ttyUSB0
```

## Example Output

```
I (482) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (2492) example_connect: Ethernet Link Up
I (4472) tcpip_adapter: eth ip: 192.168.2.137, mask: 255.255.255.0, gw: 192.168.2.2
I (4472) example_connect: Connected to Ethernet
I (4472) example_connect: IPv4 address: 192.168.2.137
I (4472) example_connect: IPv6 address: fe80:0000:0000:0000:bedd:c2ff:fed4:a92b
I (4482) WEBSOCKET: Connecting to wss://echo.websocket.org...
I (5012) WEBSOCKET: WEBSOCKET_EVENT_CONNECTED
I (5492) WEBSOCKET: Sending hello 0000
I (6052) WEBSOCKET: WEBSOCKET_EVENT_DATA
W (6052) WEBSOCKET: Received=hello 0000

I (6492) WEBSOCKET: Sending hello 0001
I (7052) WEBSOCKET: WEBSOCKET_EVENT_DATA
W (7052) WEBSOCKET: Received=hello 0001

I (7492) WEBSOCKET: Sending hello 0002
I (8082) WEBSOCKET: WEBSOCKET_EVENT_DATA
W (8082) WEBSOCKET: Received=hello 0002

I (8492) WEBSOCKET: Sending hello 0003
I (9152) WEBSOCKET: WEBSOCKET_EVENT_DATA
W (9162) WEBSOCKET: Received=hello 0003

```


## WebSocket Test Server

### Standalone Test Server

This example includes a standalone WebSocket test server (`websocket_server.py`) that can be used for testing your ESP32 WebSocket client:

#### Quick Start

**Plain WebSocket (No TLS):**
```bash
# Plain WebSocket server (no encryption)
python websocket_server.py

# Custom port
python websocket_server.py --port 9000
```

**Server-Only Authentication:**
```bash
# TLS WebSocket server (ESP32 verifies server)
python websocket_server.py --tls

# Custom port with TLS
python websocket_server.py --port 9000 --tls
```

**Mutual Authentication:**
```bash
# TLS with client certificate verification (both verify each other)
python websocket_server.py --tls --client-verify

# Custom port with mutual authentication
python websocket_server.py --port 9000 --tls --client-verify
```

#### Server Features
- **Echo functionality** - Automatically echoes back received messages
- **TLS support** - Secure WebSocket (WSS) connections
- **Client certificate verification** - Mutual authentication support
- **Binary and text messages** - Handles both data types
- **Auto IP detection** - Shows connection URL with your local IP

#### Verification Modes

**Plain WebSocket (No TLS):**
- No certificate verification on either side
- Use for local testing or trusted networks
- Configuration: `CONFIG_WS_OVER_TLS_MUTUAL_AUTH=n` and `CONFIG_WS_OVER_TLS_SERVER_AUTH=n`

**Server-Only Authentication (`--tls` without `--client-verify`):**
- ESP32 verifies the server's certificate
- Server does NOT verify the ESP32's certificate
- Use when you trust the client but want to verify the server
- Configuration: `CONFIG_WS_OVER_TLS_SERVER_AUTH=y`

**Mutual Authentication (`--tls --client-verify`):**
- ESP32 verifies the server's certificate
- Server verifies the ESP32's certificate
- Use when both parties need to authenticate each other
- Configuration: `CONFIG_WS_OVER_TLS_MUTUAL_AUTH=y`

#### Usage Examples

**Plain WebSocket (No TLS or Client Verification):**
```bash
# Basic server (port 8080)
python websocket_server.py

# Custom port
python websocket_server.py --port 9000
```

**Server-Only Authentication (ESP32 verifies server, server doesn't verify ESP32):**
```bash
# TLS server without client verification
python websocket_server.py --tls

# Custom port with TLS
python websocket_server.py --port 9000 --tls
```

**Mutual Authentication (Both ESP32 and server verify each other's certificates):**
```bash
# TLS server with client certificate verification
python websocket_server.py --tls --client-verify

# Custom port with mutual authentication
python websocket_server.py --port 9000 --tls --client-verify
```

The server will display the connection URL (e.g., `wss://192.168.1.100:8080`) that you can use in your ESP32 configuration.

### Alternative: Python Flask Echo Server

By default, the `wss://echo.websocket.org` endpoint is used. You can also setup a Python Flask websocket echo server locally and try the `ws://<your-ip>:5000` endpoint. To do this, install Flask-sock Python package

```
pip install flask-sock
```

and start a Flask websocket echo server locally by executing the following Python code:

```python
from flask import Flask
from flask_sock import Sock

app = Flask(__name__)
sock = Sock(app)


@sock.route('/')
def echo(ws):
    while True:
        data = ws.receive()
        ws.send(data)


if __name__ == '__main__':
    # To run your Flask + WebSocket server in production you can use Gunicorn:
    # gunicorn -b 0.0.0.0:5000 --workers 4 --threads 100 module:app
    app.run(host="0.0.0.0", debug=True)
```

## Troubleshooting

### Common Issues

**Connection failed:**
- Verify WiFi/Ethernet configuration in `idf.py menuconfig`
- Check if the WebSocket server is running and accessible
- Ensure the URI is correct (use `wss://` for TLS, `ws://` for plain TCP)

**TLS certificate errors:**
- **Certificate verification failed:** The most common cause is CN mismatch. Ensure the server certificate's Common Name matches the hostname/IP you're connecting to:
  - Check your connection URI (e.g., if connecting to `wss://192.168.1.100:8080`, the certificate CN must be `192.168.1.100`)
  - Regenerate certificates with the correct CN: `./generate_certs.sh <your_hostname_or_ip>`
  - For testing only, you can bypass CN check with `CONFIG_WS_OVER_TLS_SKIP_COMMON_NAME_CHECK=y` (NOT recommended for production)
- Verify certificate files are properly formatted and accessible
- Ensure the CA certificate used to sign the server certificate is loaded on the ESP32

**Build errors:**
- Clean build: `idf.py fullclean`
- Check ESP-IDF version compatibility
- Verify all dependencies are installed

**Test failures:**
- Ensure the device is connected and accessible via the specified port
- Check that the target device matches the configuration (`--target esp32`)
- Verify pytest dependencies are installed correctly

### Getting Help

- Check the [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/)
- Review the [WebSocket client component documentation](../../README.md)
- Report issues on the [ESP Protocols repository](https://github.com/espressif/esp-protocols)
