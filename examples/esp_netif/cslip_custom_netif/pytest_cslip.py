# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
from __future__ import print_function, unicode_literals

import subprocess
import time
from threading import Event, Thread

import netifaces


def is_esp32(port):
    """
    Check if the given port is connected to an ESP32 using esptool.
    """
    try:
        result = subprocess.run(
            ['esptool.py', '--port', port, 'chip_id'],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True, text=True
        )
        return 'ESP32' in result.stdout
    except subprocess.CalledProcessError:
        return False


def run_server(server_stop, port):
    print('Launching CSLIP(netif=slip) on port: {}'.format(port))
    try:
        arg_list = ['sudo', 'slattach', '-v', '-L', '-s', '115200', '-p', 'slip', port]
        p = subprocess.Popen(arg_list, stdout=subprocess.PIPE, bufsize=1)
        while not server_stop.is_set():
            if p.poll() is not None:
                if p.poll() == 16:
                    print('[SLIP:] Terminated: hang-up received')
                    break
                else:
                    raise ValueError(
                        'ENV_TEST_FAILURE: SLIP terminated unexpectedly with {}'
                        .format(p.poll()))
            time.sleep(0.1)
    except Exception as e:
        print(e)
        raise ValueError('ENV_TEST_FAILURE: Error running SLIP netif')
    finally:
        p.terminate()
    print('SLIP netif stopped')


def test_examples_protocol_cslip(dut):
    # Find a host serial port not used by DUT and not another ESP32
    server_port = None
    for i in ['/dev/ttyUSB0', '/dev/ttyUSB1', '/dev/ttyUSB2']:
        if i == dut.serial.port:
            print(f'DUT port: {i}')
        elif is_esp32(i):
            print(f'Some other ESP32: {i}')
        else:
            print(f'Port for SLIP: {i}')
            server_port = i
    if server_port is None:
        print('ENV_TEST_FAILURE: Cannot locate SLIP port')
        raise

    server_stop = Event()
    t = Thread(target=run_server, args=(server_stop, server_port))
    t.start()
    try:
        # Wait for sl0 to appear
        ppp_server_timeout = time.time() + 30
        while 'sl0' not in netifaces.interfaces():
            print(
                "SLIP netif hasn't appear yet, active netifs:{}"
                .format(netifaces.interfaces()))
            time.sleep(0.5)
            if time.time() > ppp_server_timeout:
                raise ValueError(
                    'ENV_TEST_FAILURE: SLIP netif failed to setup sl0 interface within timeout'
                )
        # Configure host side addresses
        cmd = ['sudo', 'ifconfig', 'sl0', '10.0.0.1', 'dstaddr', '10.0.0.2']
        try:
            subprocess.run(cmd, check=True)
            print('SLIP interface configured successfully.')
        except subprocess.CalledProcessError as e:
            print(f'Failed to configure SLIP interface: {e}')
        # Ping the DUT IP
        cmd = ['ping', '10.0.0.2', '-c', '5', '-W', '10']
        try:
            subprocess.run(cmd, check=True)
            print('Pinging CSLIP interface successfully.')
        except subprocess.CalledProcessError as e:
            print(f'Failed to ping CSLIP interface: {e}')
            raise
    finally:
        server_stop.set()
        t.join()
