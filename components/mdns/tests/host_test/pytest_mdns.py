# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import logging
import subprocess

import pexpect
import pytest

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)
ipv6_enabled = False


class MdnsConsole:
    def __init__(self, command):
        self.process = pexpect.spawn(command, encoding='utf-8')
        self.process.logfile = open('mdns_interaction.log', 'w')  # Log all interactions
        self.process.expect('mdns> ', timeout=10)

    def send_input(self, input_data):
        logger.info(f'Sending to stdin: {input_data}')
        self.process.sendline(input_data)

    def get_output(self, expected_data):
        logger.info(f'Expecting: {expected_data}')
        self.process.expect(expected_data, timeout=10)
        output = self.process.before.strip()
        logger.info(f'Received from stdout: {output}')
        return output

    def terminate(self):
        self.send_input('exit')
        self.get_output('Exit')
        self.process.wait()
        self.process.close()
        assert self.process.exitstatus == 0


class DigWrapper:
    def __init__(self, server='224.0.0.251', port=5353):
        self.server = server
        self.port = port

    def parse_answer_section(self, output, query_type):
        answer_section = False
        answers = []
        for line in output.splitlines():
            if line.startswith(';; ANSWER SECTION:'):
                answer_section = True
                continue
            if answer_section:
                if line.startswith(';;'):
                    break
                if query_type in line:
                    answers.append(line)
        return answers

    def run_query(self, name, query_type='PTR'):
        command = ['dig', '+timeout=3', '+tries=3', query_type, '-p', str(self.port), f'@{self.server}', name]
        logger.info(f"Running dig command: {' '.join(command)}")
        try:
            result = subprocess.run(command, capture_output=True, text=True, timeout=15)
            logger.info(f'dig return code: {result.returncode}')
            if result.returncode != 0:
                logger.info(f'dig command output: {result.stdout}')
            return result.stdout
        except subprocess.TimeoutExpired as e:
            logger.info(f'dig command timed out: {e}')
            return ''
        except subprocess.CalledProcessError as e:
            logger.error(f'dig command failed: {e}')
            logger.info(f'Partial output before timeout: {e.stdout}')
            return e.stdout or ''

    def check_record(self, name, query_type, expected=True, expect=None):
        output = self.run_query(name, query_type=query_type)
        answers = self.parse_answer_section(output, query_type)
        logger.info(f'dig answers: {answers}')
        if expect is None:
            expect = name
        if expected:
            assert any(expect in answer for answer in answers), f"Expected record '{expect}' not found in answer section"
        else:
            assert 'timeout' in output
            assert not any(expect in answer for answer in answers), f"Unexpected record '{expect}' found in answer section"


@pytest.fixture(scope='module')
def mdns_console():
    app = MdnsConsole('./build_linux_console/mdns_host.elf')
    yield app
    app.terminate()


@pytest.fixture(scope='module')
def dig_app():
    return DigWrapper()


def test_mdns_init(mdns_console, dig_app):
    mdns_console.send_input('mdns_init -h hostname')
    mdns_console.get_output('MDNS: Hostname: hostname')
    dig_app.check_record('hostname.local', query_type='A', expected=True)
    if ipv6_enabled:
        dig_app.check_record('hostname.local', query_type='AAAA', expected=True)


def test_add_service(mdns_console, dig_app):
    mdns_console.send_input('mdns_service_add _http _tcp 80 -i test_service')
    mdns_console.get_output('MDNS: Service Instance: test_service')
    mdns_console.send_input('mdns_service_lookup _http _tcp')
    mdns_console.get_output('PTR : test_service')
    dig_app.check_record('_http._tcp.local', query_type='PTR', expected=True)


def test_remove_service(mdns_console, dig_app):
    mdns_console.send_input('mdns_service_remove _http _tcp')
    mdns_console.send_input('mdns_service_lookup _http _tcp')
    mdns_console.get_output('No results found!')
    dig_app.check_record('_http._tcp.local', query_type='PTR', expected=False)


def test_delegate_host(mdns_console, dig_app):
    mdns_console.send_input('mdns_delegate_host delegated 1.2.3.4')
    dig_app.check_record('delegated.local', query_type='A', expected=True)


def test_undelegate_host(mdns_console, dig_app):
    mdns_console.send_input('mdns_undelegate_host delegated')
    dig_app.check_record('delegated.local', query_type='A', expected=False)


def test_add_delegated_service(mdns_console, dig_app):
    mdns_console.send_input('mdns_delegate_host delegated 1.2.3.4')
    dig_app.check_record('delegated.local', query_type='A', expected=True)
    mdns_console.send_input('mdns_service_add _test _tcp 80 -i local')
    mdns_console.get_output('MDNS: Service Instance: local')
    mdns_console.send_input('mdns_service_add _test2 _tcp 80 -i extern -h delegated')
    mdns_console.get_output('MDNS: Service Instance: extern')
    mdns_console.send_input('mdns_service_lookup _test _tcp')
    mdns_console.get_output('PTR : local')
    mdns_console.send_input('mdns_service_lookup _test2 _tcp -d')
    mdns_console.get_output('PTR : extern')
    dig_app.check_record('_test2._tcp.local', query_type='PTR', expected=True)
    dig_app.check_record('extern._test2._tcp.local', query_type='SRV', expected=True)


def test_remove_delegated_service(mdns_console, dig_app):
    mdns_console.send_input('mdns_service_remove _test2 _tcp -h delegated')
    mdns_console.send_input('mdns_service_lookup _test2 _tcp -d')
    mdns_console.get_output('No results found!')
    dig_app.check_record('_test2._tcp.local', query_type='PTR', expected=False)
    # add the delegated service again, would be used in the TXT test
    mdns_console.send_input('mdns_service_add _test2 _tcp 80 -i extern -h delegated')
    mdns_console.get_output('MDNS: Service Instance: extern')


def check_txt_for_service(instance, service, proto, mdns_console, dig_app, host=None):
    for_host_arg = f'-h {host}' if host is not None else ''
    mdns_console.send_input(f'mdns_service_txt_set {service} {proto} {for_host_arg} key1 value1')
    dig_app.check_record(f'{instance}.{service}.{proto}.local', query_type='SRV', expected=True)
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=True, expect='key1=value1')
    mdns_console.send_input(f'mdns_service_txt_set {service} {proto} {for_host_arg} key2 value2')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=True, expect='key2=value2')
    mdns_console.send_input(f'mdns_service_txt_remove {service} {proto} {for_host_arg} key2')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=False, expect='key2=value2')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=True, expect='key1=value1')
    mdns_console.send_input(f'mdns_service_txt_replace {service} {proto} {for_host_arg} key3=value3 key4=value4')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=False, expect='key1=value1')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=True, expect='key3=value3')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=True, expect='key4=value4')


def test_update_txt(mdns_console, dig_app):
    check_txt_for_service('local', '_test', '_tcp', mdns_console=mdns_console, dig_app=dig_app)


def test_update_delegated_txt(mdns_console, dig_app):
    check_txt_for_service('extern', '_test2', '_tcp', mdns_console=mdns_console, dig_app=dig_app, host='delegated')


if __name__ == '__main__':
    pytest.main(['-s', 'test_mdns.py'])
