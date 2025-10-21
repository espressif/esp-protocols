# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import json
import random
import re
import string
import sys

from websocket_server import WebsocketServer, get_my_ip


def test_examples_protocol_websocket(dut):
    """
    steps:
      1. obtain IP address
      2. connect to uri specified in the config
      3. send and receive data
    """

    # Test for echo functionality:
    # Sends a series of simple "hello" messages to the WebSocket server and verifies that each one is echoed back correctly.
    # This tests the basic responsiveness and correctness of the WebSocket connection.
    def test_echo(dut):
        dut.expect('WEBSOCKET_EVENT_CONNECTED')
        for i in range(0, 5):
            dut.expect(re.compile(b'Received=hello (\\d)'))
        print('All echos received')
        sys.stdout.flush()

    # Test for clean closure of the WebSocket connection:
    # Ensures that the WebSocket can correctly receive a close frame and terminate the connection without issues.
    def test_close(dut):
        code = dut.expect(
            re.compile(
                b'websocket: Received closed message with code=(\\d*)'))[0]
        print('Received close frame with code {}'.format(code))

    # Test for JSON message handling:
    # Sends a JSON formatted string and verifies that the received message matches the expected JSON structure.
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
            print('\n Sent message and received message are equal \n')
            sys.stdout.flush()
        else:
            raise ValueError(
                'DUT received string do not match sent string, \nexpected: {}\nwith length {}\
                                 \nreceived: {}\nwith length {}'.format(
                    data[0], len(data[0]), match, len(match)))

    # Test for receiving long messages:
    # This sends a message with a specified length (2000 characters) to ensure the WebSocket can handle large data payloads. Repeated 3 times for reliability.
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
                print('\n Sent message and received message are equal \n')
                sys.stdout.flush()
            else:
                raise ValueError(
                    'DUT received string do not match sent string, \nexpected: {}\nwith length {}\
                                \nreceived: {}\nwith length {}'.format(
                        send_msg, len(send_msg), recv_msg, len(recv_msg)))

    # Test for receiving the first fragment of a large message:
    # Verifies the WebSocket's ability to correctly process the initial segment of a fragmented message.
    def test_recv_fragmented_msg1(dut):
        dut.expect('websocket: Total payload length=2000, data_len=1024, current payload offset=0')

    # Test for receiving the second fragment of a large message:
    # Confirms that the WebSocket can correctly handle and process the subsequent segment of a fragmented message.
    def test_recv_fragmented_msg2(dut):
        dut.expect('websocket: Total payload length=2000, data_len=976, current payload offset=1024')

    # Test for receiving fragmented text messages:
    # Checks if the WebSocket can accurately reconstruct a message sent in several smaller parts.
    def test_fragmented_txt_msg(dut):
        dut.expect('Received=' + 32 * 'a' + 32 * 'b')
        print('\nFragmented data received\n')

    # Extract the hexdump portion of the log line
    def parse_hexdump(line):
        match = re.search(r'\(.*\) Received binary data: ([0-9A-Fa-f ]+)', line)
        if match:
            hexdump = match.group(1).strip().replace(' ', '')
            # Convert the hexdump string to a bytearray
            return bytearray.fromhex(hexdump)
        return bytearray()

    # Capture the binary log output from the DUT
    def test_fragmented_binary_msg(dut):
        match = dut.expect(r'\(.*\) Received binary data: .*')
        if match:
            line = match.group(0).strip()
            if isinstance(line, bytes):
                line = line.decode('utf-8')

            # Parse the hexdump from the log line
            received_data = parse_hexdump(line)

            # Create the expected bytearray with the specified pattern
            expected_data = bytearray([0, 0, 0, 0, 0, 1, 1, 1, 1, 1])

            # Validate the received data
            assert received_data == expected_data, f'Received data does not match expected data. Received: {received_data}, Expected: {expected_data}'
            print('\nFragmented data received\n')
        else:
            assert False, 'Log line with binary data not found'

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
        with WebsocketServer(server_port, use_tls, client_verify) as ws:
            if use_tls is True:
                uri = 'wss://{}:{}'.format(get_my_ip(), server_port)
            else:
                uri = 'ws://{}:{}'.format(get_my_ip(), server_port)
            print('DUT connecting to {}'.format(uri))
            dut.expect("Please enter WebSocket endpoint URI", timeout=30)
            dut.write(uri)
            test_echo(dut)
            test_recv_long_msg(dut, ws, 2000, 3)
            test_json(dut, ws)
            test_fragmented_txt_msg(dut)
            test_fragmented_binary_msg(dut)
            test_recv_fragmented_msg1(dut)
            test_recv_fragmented_msg2(dut)
            test_close(dut)
    else:
        print('DUT connecting to {}'.format(uri))
        test_echo(dut)
