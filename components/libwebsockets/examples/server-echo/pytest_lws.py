# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import json
import random
import re
import ssl
import string
import time

import websocket


def get_esp32_ip(dut):
    """Retrieve ESP32 IP address from ESP-IDF logs."""
    ip_regex = re.compile(r'IPv4 address:\s*(\d+\.\d+\.\d+\.\d+)')
    timeout = time.time() + 60

    while time.time() < timeout:
        try:
            match = dut.expect(ip_regex, timeout=5)
            if match:
                ip_address = match.group(1).decode()
                print(f'ESP32 IP found: {ip_address}')
                return ip_address
        except Exception:
            pass

    print('Error: ESP32 IP not found in logs.')
    raise RuntimeError('ESP32 IP not found.')


def wait_for_websocket_server(dut):
    """Waits for the ESP32 WebSocket server to be ready."""
    server_ready_regex = re.compile(r'LWS minimal ws server echo')
    timeout = time.time() + 20

    while time.time() < timeout:
        try:
            dut.expect(server_ready_regex, timeout=5)
            print('WebSocket server is up!')
            return
        except Exception:
            pass

    print('Error: WebSocket server did not start.')
    raise RuntimeError('WebSocket server failed to start.')


def connect_websocket(uri, use_tls):
    """Attempts to connect to the WebSocket server with retries, handling TLS if enabled."""
    for attempt in range(5):
        try:
            print(f'Attempting to connect to {uri} (Try {attempt + 1}/5)')

            if use_tls:
                ssl_context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH)
                ssl_context.load_cert_chain(certfile='main/certs/client_cert.pem',
                                            keyfile='main/certs/client_key.pem')

                try:
                    ssl_context.load_verify_locations(cafile='main/certs/ca_cert.pem')
                    ssl_context.verify_mode = ssl.CERT_REQUIRED
                    ssl_context.check_hostname = False
                except Exception:
                    print('Warning: CA certificate not found. Skipping mutual authentication.')
                    ssl_context.verify_mode = ssl.CERT_NONE

                ws = websocket.create_connection(uri, sslopt={'context': ssl_context}, timeout=10)

            else:
                ws = websocket.create_connection(uri, timeout=10)

            print('WebSocket connected successfully!')
            return ws
        except Exception as e:
            print(f'Connection failed: {e}')
            time.sleep(3)

    print('Failed to connect to WebSocket.')
    raise RuntimeError('WebSocket connection failed.')


def send_fragmented_message(ws, message, is_binary=False, fragment_size=1024):
    """Sends a message in fragments."""
    opcode = websocket.ABNF.OPCODE_BINARY if is_binary else websocket.ABNF.OPCODE_TEXT
    total_length = len(message)

    for i in range(0, total_length, fragment_size):
        fragment = message[i: i + fragment_size]
        ws.send(fragment, opcode=opcode)


def test_examples_protocol_websocket(dut):
    """Tests WebSocket communication."""

    esp32_ip = get_esp32_ip(dut)
    wait_for_websocket_server(dut)

    # Retrieve WebSocket configuration from SDKCONFIG
    try:
        port = int(dut.app.sdkconfig['WEBSOCKET_PORT'])        # Gets WebSocket port
        use_tls = dut.app.sdkconfig.get('WS_OVER_TLS', False)  # Gets TLS setting
    except KeyError:
        print('Error: WEBSOCKET_PORT or WS_OVER_TLS not found in sdkconfig.')
        raise

    protocol = 'wss' if use_tls else 'ws'
    uri = f'{protocol}://{esp32_ip}:{port}'

    print(f'\nWebSocket Configuration:\n  - TLS: {use_tls}\n  - Port: {port}\n  - URI: {uri}\n')

    ws = connect_websocket(uri, use_tls)

    def test_echo():
        """Sends and verifies an echo message."""
        ws.send('Hello ESP32!')
        response = ws.recv()
        assert response == 'Hello ESP32!', 'Echo response mismatch!'
        print('Echo test passed!')

    def test_send_receive_long_msg(msg_len=1024):
        """Sends and verifies a long text message."""
        send_msg = ''.join(random.choices(string.ascii_letters + string.digits, k=msg_len))
        print(f'Sending long message ({msg_len} bytes)...')
        ws.send(send_msg)
        response = ws.recv()
        assert response == send_msg, 'Long message mismatch!'
        print('Long message test passed!')

    def test_send_receive_binary():
        """Sends and verifies binary data."""
        expected_binary_data = bytearray([0, 0, 0, 0, 0, 1, 1, 1, 1, 1])
        print('Sending binary data...')
        ws.send(expected_binary_data, opcode=websocket.ABNF.OPCODE_BINARY)
        received_data = ws.recv()
        assert received_data == expected_binary_data, 'Binary data mismatch!'
        print('Binary data test passed!')

    def test_json():
        """Sends and verifies a JSON message."""
        json_data = {'id': '1', 'name': 'test_user'}
        json_string = json.dumps(json_data)
        print('Sending JSON message...')
        ws.send(json_string)
        response = ws.recv()
        received_json = json.loads(response)
        assert received_json == json_data, 'JSON data mismatch!'
        print('JSON test passed!')

    def test_recv_fragmented_msg1():
        """Verifies reception of the first text fragment."""
        print('Waiting for first fragment log...')
        dut.expect(re.compile(r'I \(\d+\) LWS_SERVER: .* fragment received from .* \(1024 bytes\)'), timeout=20)
        print('Fragmented message part 1 received correctly.')

    def test_recv_fragmented_msg2():
        """Verifies reception of the second text fragment."""
        print('Waiting for second fragment log...')
        dut.expect(re.compile(r'I \(\d+\) LWS_SERVER: .* fragment received from .* \(.* bytes\)'), timeout=20)
        print('Fragmented message part 2 received correctly.')

    def test_fragmented_txt_msg():
        """Tests sending and receiving a fragmented text message."""
        part1 = ''.join(random.choices(string.ascii_letters + string.digits, k=1024))
        part2 = ''.join(random.choices(string.ascii_letters + string.digits, k=976))
        message = part1 + part2

        send_fragmented_message(ws, message, is_binary=False, fragment_size=1024)

        print('Waiting for Complete text message log...')

        escaped_message_start = re.escape(message[:30])
        escaped_message_end = re.escape(message[-30:])

        dut.expect(
            re.compile(
                rb"I \(\d+\) LWS_SERVER: Complete text message:.*?" + escaped_message_start.encode() + rb".*?" + escaped_message_end.encode(),
                re.DOTALL
            ),
            timeout=20
        )

        print('Fragmented text message received correctly.')

    def test_fragmented_binary_msg():
        """Tests sending and receiving a fragmented binary message."""
        expected_data = bytearray([0, 0, 0, 0, 0, 1, 1, 1, 1, 1] * 5)
        send_fragmented_message(ws, expected_data, is_binary=True, fragment_size=10)

        print('Waiting for Complete binary message log...')
        dut.expect(re.compile(r'I \(\d+\) LWS_SERVER: Complete binary message \(hex\): '), timeout=10)
        print('Fragmented binary message received correctly.')

    def test_close():
        """Closes WebSocket connection and verifies closure."""
        print('Closing WebSocket...')
        ws.close()

        try:
            close_regex = re.compile(rb"websocket: Received closed message with code=(\d+)")
            disconnect_regex = re.compile(rb"LWS_SERVER: Client disconnected")

            match = dut.expect([close_regex, disconnect_regex], timeout=5)

            if match == 0:
                close_code = match.group(1).decode()
                print(f'WebSocket closed successfully with code: {close_code}')
            else:
                print('WebSocket closed successfully (client disconnect detected).')

        except Exception:
            print('WebSocket close message not found.')
            raise

    test_echo()
    test_send_receive_long_msg()
    test_send_receive_binary()
    test_json()
    test_recv_fragmented_msg1()
    test_recv_fragmented_msg2()
    test_fragmented_txt_msg()
    test_fragmented_binary_msg()
    test_close()

    ws.close()
