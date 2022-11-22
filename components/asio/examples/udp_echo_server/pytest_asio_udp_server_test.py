# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import re
import socket


def test_examples_protocol_asio_udp_server(dut):
    """
    steps: |
      1. join AP
      2. Start server
      3. Test connects to server and sends a test message
      4. Test evaluates received test message from server
      5. Test evaluates received test message on server stdout
    """
    test_msg = b'echo message from client to server'
    data = dut.expect(
        re.compile(
            b' IPv4 address: ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)'),  # noqa: W605
        timeout=30).group(1).decode()
    dut.expect('ASIO engine is up and running', timeout=1)
    cli = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    cli.settimeout(30)
    cli.connect((data, 2222))
    cli.send(test_msg)
    data = cli.recv(1024)
    # 4. check the message received back from the server
    if (data == test_msg):
        print('PASS: Received correct message')
        pass
    else:
        print('Failure!')
        raise ValueError(
            'Wrong data received from asio udp server: {} (expected:{})'.
            format(data, test_msg))
    # 5. check the client message appears also on server terminal
    dut.expect(test_msg.decode())
