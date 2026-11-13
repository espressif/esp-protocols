# Websocket LWS server example

This example will shows how to set up and communicate over a websocket.

## How to Use Example

### Hardware Required

This example can be executed on any ESP32 board, the only required interface is WiFi and connection to internet or a local server.

### Configure the project

* Open the project configuration menu (`idf.py menuconfig`)
* Configure Wi-Fi or Ethernet under "Example Connection Configuration" menu.

### Server Certificate Verification


### Generating a self signed Certificates with OpenSSL


### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example Output


## Python Flask echo server

By default, the `ws://echo.websocket.events` endpoint is used. You can setup a Python websocket echo server locally and try the `ws://<your-ip>:5000` endpoint. To do this, install Flask-sock Python package

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
