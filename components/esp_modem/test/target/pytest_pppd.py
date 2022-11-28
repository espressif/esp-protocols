# SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
from __future__ import print_function, unicode_literals

import subprocess
import time
from threading import Event, Thread

import netifaces


def run_server(server_stop, port, server_ip, client_ip):
    print('Starting PPP server on port: {}'.format(port))
    try:
        arg_list = [
            'sudo', 'pppd', port, '115200',
            '{}:{}'.format(server_ip, client_ip), 'modem', 'local', 'noauth',
            'debug', 'nocrtscts', 'nodetach', '+ipv6'
        ]
        p = subprocess.Popen(arg_list, stdout=subprocess.PIPE, bufsize=1)
        while not server_stop.is_set():
            if p.poll() is not None:
                if p.poll() == 16:
                    print('[PPPD:] Terminated: hang-up received')
                    break
                else:
                    raise ValueError(
                        'ENV_TEST_FAILURE: PPP terminated unexpectedly with {}'
                        .format(p.poll()))
            line = p.stdout.readline()
            if line:
                print('[PPPD:]{}'.format(line.rstrip()))
            time.sleep(0.1)
    except Exception as e:
        print(e)
        raise ValueError('ENV_TEST_FAILURE: Error running PPP server')
    finally:
        p.terminate()
    print('PPP server stopped')


def test_examples_protocol_pppos_connect(dut):
    """
    steps:
      1. starts PPP server
      2. get DUT as PPP client to connect to the server
      3. run unit test FW
    """

    # Look for test case symbolic names
    try:
        server_ip = dut.app.sdkconfig.get('TEST_APP_PPP_SERVER_IP')
        client_ip = dut.app.sdkconfig.get('TEST_APP_PPP_CLIENT_IP')
    except Exception:
        print(
            'ENV_TEST_FAILURE: Some mandatory configuration not found in sdkconfig'
        )
        raise

    # the PPP test env uses two ttyUSB's: one for ESP32 board, another one for ppp server
    # use the other port for PPP server than the DUT/ESP
    port = '/dev/ttyUSB0' if dut.serial.port == '/dev/ttyUSB1' else '/dev/ttyUSB1'
    # Start the PPP server
    server_stop = Event()
    t = Thread(target=run_server,
               args=(server_stop, port, server_ip, client_ip))
    t.start()
    try:
        ppp_server_timeout = time.time() + 30
        while 'ppp0' not in netifaces.interfaces():
            print(
                "PPP server haven't yet setup its netif, list of active netifs:{}"
                .format(netifaces.interfaces()))
            time.sleep(0.5)
            if time.time() > ppp_server_timeout:
                raise ValueError(
                    'ENV_TEST_FAILURE: PPP server failed to setup ppp0 interface within timeout'
                )

        dut.expect('All tests passed')
    finally:
        server_stop.set()
        t.join()
