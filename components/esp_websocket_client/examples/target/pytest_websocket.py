# SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import json
import random
import re
import socket
import ssl
import string
import sys
from threading import Event, Thread

from SimpleWebSocketServer import (SimpleSSLWebSocketServer,
                                   SimpleWebSocketServer, WebSocket)


def get_my_ip():
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

    def handleMessage(self):
        self.sendMessage(self.data)
        print('\n Server sent: {}\n'.format(self.data))

    def handleConnected(self):
        print('Connection from: {}'.format(self.address))

    def handleClose(self):
        print('{} closed the connection'.format(self.address))


# Simple Websocket server for testing purposes
class Websocket(object):

    def send_data(self, data):
        for nr, conn in self.server.connections.items():
            conn.sendMessage(data)

    def run(self):
        if self.use_tls is True:
            ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ssl_context.load_cert_chain(certfile='main/certs/server_cert.pem', keyfile='main/certs/server_key.pem')
            if self.client_verify is True:
                ssl_context.load_verify_locations(cafile='main/certs/ca_cert.pem')
                ssl_context.verify = ssl.CERT_REQUIRED
            ssl_context.check_hostname = False
            self.server = SimpleSSLWebSocketServer('', self.port, WebsocketTestEcho, ssl_context=ssl_context)
        else:
            self.server = SimpleWebSocketServer('', self.port, WebsocketTestEcho)
        while not self.exit_event.is_set():
            self.server.serveonce()

    def __init__(self, port, use_tls, verify):
        self.port = port
        self.use_tls = use_tls
        self.client_verify = verify
        self.exit_event = Event()
        self.thread = Thread(target=self.run)
        self.thread.start()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.exit_event.set()
        self.thread.join(10)
        if self.thread.is_alive():
            print('Thread cannot be joined', 'orange')


def test_examples_protocol_websocket(dut):
    """
    steps:
      1. obtain IP address
      2. connect to uri specified in the config
      3. send and receive data
    """

    def test_echo(dut):
        dut.expect('WEBSOCKET_EVENT_CONNECTED')
        for i in range(0, 5):
            dut.expect(re.compile(b'Received=hello (\\d)'))
        print('All echos received')
        sys.stdout.flush()

    def test_close(dut):
        code = dut.expect(
            re.compile(
                b'websocket: Received closed message with code=(\\d*)'))[0]
        print('Received close frame with code {}'.format(code))

    def test_json(dut, websocket):
        json_string = """
            [
               {
                  "id":"1",
                  "name":"user1"
               },
               {
                  "id":"2",
                  "name":"user2"
               }
            ]
        """
        websocket.send_data(json_string)
        data = json.loads(json_string)

        match = dut.expect(
            re.compile(b'Json=({[a-zA-Z0-9]*).*}')).group(0).decode()[5:]
        if match == str(data[0]):
            print('Sent message and received message are equal \n')
            sys.stdout.flush()
        else:
            raise ValueError(
                'DUT received string do not match sent string, \nexpected: {}\nwith length {}\
                                 \nreceived: {}\nwith length {}'.format(
                    data[0], len(data[0]), match, len(match)))

    def test_recv_long_msg(dut, websocket, msg_len, repeats):

        send_msg = ''.join(
            random.choice(string.ascii_uppercase + string.ascii_lowercase +
                          string.digits) for _ in range(msg_len))

        for _ in range(repeats):
            websocket.send_data(send_msg)

            recv_msg = ''
            while len(recv_msg) < msg_len:
                match = dut.expect(re.compile(
                    b'Received=([a-zA-Z0-9]*).*\n')).group(1).decode()
                recv_msg += match

            if recv_msg == send_msg:
                print('Sent message and received message are equal \n')
                sys.stdout.flush()
            else:
                raise ValueError(
                    'DUT received string do not match sent string, \nexpected: {}\nwith length {}\
                                \nreceived: {}\nwith length {}'.format(
                        send_msg, len(send_msg), recv_msg, len(recv_msg)))

    def test_fragmented_msg(dut):
        dut.expect('Received=' + 32 * 'a' + 32 * 'b')
        print('Fragmented data received')

    # Starting of the test
    try:
        if dut.app.sdkconfig.get('WEBSOCKET_URI_FROM_STDIN') is True:
            uri_from_stdin = True
        else:
            uri = dut.app.sdkconfig['WEBSOCKET_URI']
            uri_from_stdin = False

        if dut.app.sdkconfig.get('WS_OVER_TLS_MUTUAL_AUTH') is True:
            use_tls = True
            client_verify = True
        else:
            use_tls = False
            client_verify = False

    except Exception:
        print('ENV_TEST_FAILURE: Cannot find uri settings in sdkconfig')
        raise

    if uri_from_stdin:
        server_port = 8080
        with Websocket(server_port, use_tls, client_verify) as ws:
            if use_tls is True:
                uri = 'wss://{}:{}'.format(get_my_ip(), server_port)
            else:
                uri = 'ws://{}:{}'.format(get_my_ip(), server_port)
            print('DUT connecting to {}'.format(uri))
            dut.expect('Please enter uri of websocket endpoint', timeout=30)
            dut.write(uri)
            test_echo(dut)
            # Message length should exceed DUT's buffer size to test fragmentation, default is 1024 byte
            test_recv_long_msg(dut, ws, 2000, 3)
            test_json(dut, ws)
            test_fragmented_msg(dut)
            test_close(dut)
    else:
        print('DUT connecting to {}'.format(uri))
        test_echo(dut)
