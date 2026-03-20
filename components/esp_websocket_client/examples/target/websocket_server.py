# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import socket
import ssl
from threading import Event, Thread

from SimpleWebSocketServer import (SimpleSSLWebSocketServer,
                                   SimpleWebSocketServer, WebSocket)


def get_my_ip():
    """Get the local IP address of this machine."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # doesn't even have to be reachable
        s.connect(('8.8.8.8', 1))
        IP = s.getsockname()[0]
    except Exception:
        IP = '127.0.0.1'
    finally:
        s.close()
    return IP


class WebsocketTestEcho(WebSocket):
    """WebSocket handler that echoes back received messages."""

    def handleMessage(self):
        if isinstance(self.data, bytes):
            print(f'\n Server received binary data: {self.data.hex()}\n')
            self.sendMessage(self.data, binary=True)
        else:
            print(f'\n Server received: {self.data}\n')
            self.sendMessage(self.data)

    def handleConnected(self):
        print('Connection from: {}'.format(self.address))

    def handleClose(self):
        print('{} closed the connection'.format(self.address))


class WebsocketServer:
    """WebSocket server for testing purposes."""

    def __init__(self, port, use_tls=False, client_verify=False):
        self.port = port
        self.use_tls = use_tls
        self.client_verify = client_verify
        self.exit_event = Event()
        self.thread = None
        self.server = None

    def send_data(self, data):
        """Send data to all connected clients."""
        if self.server and hasattr(self.server, 'connections'):
            for nr, conn in self.server.connections.items():
                conn.sendMessage(data)

    def run(self):
        """Run the WebSocket server."""
        if self.use_tls:
            ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ssl_context.load_cert_chain(
                certfile='main/certs/server/server_cert.pem',
                keyfile='main/certs/server/server_key.pem'
            )
            if self.client_verify:
                ssl_context.load_verify_locations(cafile='main/certs/ca_cert.pem')
                ssl_context.verify_mode = ssl.CERT_REQUIRED
            ssl_context.check_hostname = False
            self.server = SimpleSSLWebSocketServer('', self.port, WebsocketTestEcho, ssl_context=ssl_context)
        else:
            self.server = SimpleWebSocketServer('', self.port, WebsocketTestEcho)

        print(f"WebSocket server starting on port {self.port} (TLS: {self.use_tls}, Client verify: {self.client_verify})")

        while not self.exit_event.is_set():
            self.server.serveonce()

    def start(self):
        """Start the server in a separate thread."""
        self.thread = Thread(target=self.run)
        self.thread.start()

    def stop(self):
        """Stop the server."""
        self.exit_event.set()
        if self.thread:
            self.thread.join(10)
            if self.thread.is_alive():
                print('Thread cannot be joined', 'orange')

    def __enter__(self):
        """Context manager entry."""
        self.start()
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        """Context manager exit."""
        self.stop()


def create_websocket_server(port, use_tls=False, client_verify=False):
    """Factory function to create a WebSocket server."""
    return WebsocketServer(port, use_tls, client_verify)


def run_forever(port=8080, use_tls=False, client_verify=False):
    """Run WebSocket server forever (for standalone use)."""
    print(f"Starting WebSocket server on port {port}")
    print(f"TLS enabled: {use_tls}")
    print(f"Client verification: {client_verify}")
    print(f"Server IP: {get_my_ip()}")
    print(f"Connect with-->{'wss' if use_tls else 'ws'}://{get_my_ip()}:{port}")
    print("Press Ctrl+C to stop the server")

    server = WebsocketServer(port, use_tls, client_verify)

    try:
        server.start()
        # Wait for the server thread to complete or be interrupted
        server.thread.join()
    except KeyboardInterrupt:
        print("\nServer stopped by user")
    except Exception as e:
        print(f"Server error: {e}")
    finally:
        server.stop()


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="WebSocket Test Server")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument("--tls", action="store_true", help="Enable TLS/WSS")
    parser.add_argument("--client-verify", action="store_true", help="Require client certificate verification")

    args = parser.parse_args()

    # Usage examples:
    # python3 websocket_server.py                           # Plain WebSocket on port 8080
    # python3 websocket_server.py --tls                   # TLS WebSocket on port 8080
    # python3 websocket_server.py --tls --client-verify   # TLS with client cert verification
    # python3 websocket_server.py --port 9000 --tls      # Custom port with TLS

    run_forever(port=args.port, use_tls=args.tls, client_verify=args.client_verify)
