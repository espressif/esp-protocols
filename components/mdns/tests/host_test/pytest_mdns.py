# SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import logging
import subprocess
import sys
import time
from pathlib import Path

import pexpect
import pytest
from bonjour_order_responder import DEFAULTS
from dnsfixture import DnsPythonWrapper

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)
ipv6_enabled = False


def _console_line(label: str, value: str) -> str:
    """Build a line as printed by mdns_print_results (avoids flake8 E203 on f'LABEL : {x}')."""
    return label + value


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

    def wait_for_output(self, expected_data, timeout=10):
        logger.info(f'Waiting for: {expected_data}')
        self.process.expect(expected_data, timeout=timeout)
        output = (self.process.before or '') + (self.process.after or '')
        logger.info(f'Captured output while waiting: {output}')
        return output

    def wait_for_browse_result(self, timeout=10):
        """Wait until browse notifier printed the full result (A is last required field)."""
        return self.wait_for_output(_console_line('A   : ', DEFAULTS['ipv4']), timeout=timeout)

    def wait_for_browse_goodbye(self, timeout=10):
        """Wait for Bonjour-style PTR TTL=0 removal notification (interface line then PTR)."""
        self.wait_for_output('TTL: 0', timeout=timeout)
        return self.wait_for_output(_console_line('PTR : ', DEFAULTS['instance']), timeout=timeout)

    def terminate(self):
        self.send_input('exit')
        self.get_output('Exit')
        self.process.wait()
        self.process.close()
        assert self.process.exitstatus == 0


def _bonjour_responder_cmd(srv_first: bool = False, goodbye_after: int = -1) -> list[str]:
    cmd = [
        sys.executable,
        str(Path(__file__).with_name('bonjour_order_responder.py')),
        '--interface',
        'eth0',
        '--service',
        DEFAULTS['service'],
        '--proto',
        DEFAULTS['proto'],
    ]
    if srv_first:
        cmd.append('--srv-first')
    if goodbye_after >= 0:
        cmd.extend(['--goodbye-after', str(goodbye_after)])
    return cmd


def _run_bonjour_responder(*, srv_first: bool = False, goodbye_after: int = -1, label: str):
    proc = subprocess.Popen(
        _bonjour_responder_cmd(srv_first=srv_first, goodbye_after=goodbye_after),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    time.sleep(0.5)
    # poll() stays None while the responder is still running after startup.
    assert proc.poll() is None, f'{label} failed to start (need eth0 and port 5353?)'
    return proc


def _stop_bonjour_responder(proc):
    proc.terminate()
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=3)


@pytest.fixture
def bonjour_responder():
    proc = _run_bonjour_responder(label='Bonjour-order responder')
    yield proc
    _stop_bonjour_responder(proc)


@pytest.fixture
def bonjour_responder_srv_first():
    proc = _run_bonjour_responder(srv_first=True, label='SRV-first responder')
    yield proc
    _stop_bonjour_responder(proc)


@pytest.fixture
def bonjour_responder_goodbye():
    proc = _run_bonjour_responder(goodbye_after=1, label='Goodbye responder')
    yield proc
    _stop_bonjour_responder(proc)


@pytest.fixture(scope='module')
def mdns_console():
    app = MdnsConsole('./build_linux_console/mdns_host.elf')
    yield app
    app.terminate()


@pytest.fixture(scope='module')
def dig_app():
    return DnsPythonWrapper()


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


def test_ptr_additional_records_for_service(dig_app):
    # Query PTR for the service type and ensure SRV/TXT are in Additional (RFC 6763 §12.1)
    resp = dig_app.run_query('_http._tcp.local', query_type='PTR')
    # Answer section should have at least one PTR to the instance
    answers = dig_app.parse_answer_section(resp, 'PTR')
    assert any('test_service._http._tcp.local' in a for a in answers)
    # Additional section should include SRV and TXT for the same instance
    dig_app.check_additional(resp, 'SRV', 'test_service._http._tcp.local', expected=True)
    dig_app.check_additional(resp, 'TXT', 'test_service._http._tcp.local', expected=True)


def test_instance_any_answer_records(dig_app):
    """Query ANY for the service instance and ensure SRV/TXT are in Answer (Q/A path)."""
    resp = dig_app.run_query('test_service._http._tcp.local', query_type='ANY')

    # Answer section should contain SRV and TXT records for the instance
    srv_answers = dig_app.parse_section(resp, 'answer', 'SRV')
    txt_answers = dig_app.parse_section(resp, 'answer', 'TXT')
    assert any('test_service._http._tcp.local' in a for a in srv_answers)
    assert any('test_service._http._tcp.local' in a for a in txt_answers)

    # We should not see a PTR for the generic service name in the Answer section
    ptr_answers = dig_app.parse_section(resp, 'answer', 'PTR')
    assert not any('_http._tcp.local' in a for a in ptr_answers)


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


def test_service_discovery_dns_sd(dig_app):
    """Query _services._dns-sd._udp.local PTR to discover registered service types (RFC 6763 §9)."""
    resp = dig_app.run_query('_services._dns-sd._udp.local', query_type='PTR')
    answers = dig_app.parse_answer_section(resp, 'PTR')
    assert any('_test._tcp.local' in a for a in answers), \
        f'Expected _test._tcp.local in DNS-SD response, got: {answers}'
    assert any('_test2._tcp.local' in a for a in answers), \
        f'Expected _test2._tcp.local in DNS-SD response, got: {answers}'


def test_service_discovery_query(mdns_console):
    """Test querier-side: query _services._dns-sd._udp via mdns_query_ptr (exercises multi-label splitting)."""
    mdns_console.send_input('mdns_query_ptr _services._dns-sd _udp -t 2000 -m 10')
    mdns_console.get_output('Query PTR: _services._dns-sd._udp.local')
    mdns_console.get_output('mdns>')


def test_remove_delegated_service(mdns_console, dig_app):
    mdns_console.send_input('mdns_service_remove _test2 _tcp -h delegated')
    mdns_console.send_input('mdns_service_lookup _test2 _tcp -d')
    mdns_console.get_output('No results found!')
    dig_app.check_record('_test2._tcp.local', query_type='PTR', expected=False)
    # add the delegated service again, would be used in the TXT test
    mdns_console.send_input('mdns_service_add _test2 _tcp 80 -i extern -h delegated')
    mdns_console.get_output('MDNS: Service Instance: extern')


def check_txt_for_service(instance, service, proto, mdns_console, dig_app, host=None, with_inst=False):
    for_host_arg = f'-h {host}' if host is not None else ''
    for_inst_arg = f'-i {instance}' if with_inst else ''
    mdns_console.send_input(f'mdns_service_txt_set {service} {proto} {for_host_arg} {for_inst_arg} key1 value1')
    dig_app.check_record(f'{instance}.{service}.{proto}.local', query_type='SRV', expected=True)
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=True, expect='key1=value1')
    mdns_console.send_input(f'mdns_service_txt_set {service} {proto} {for_host_arg} {for_inst_arg} key2 value2')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=True, expect='key2=value2')
    mdns_console.send_input(f'mdns_service_txt_remove {service} {proto} {for_host_arg} {for_inst_arg} key2')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=False, expect='key2=value2')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=True, expect='key1=value1')
    mdns_console.send_input(f'mdns_service_txt_replace {service} {proto} {for_host_arg}  {for_inst_arg} key3=value3 key4=value4')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=False, expect='key1=value1')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=True, expect='key3=value3')
    dig_app.check_record(f'{service}.{proto}.local', query_type='TXT', expected=True, expect='key4=value4')


def test_update_txt(mdns_console, dig_app):
    check_txt_for_service('local', '_test', '_tcp', mdns_console=mdns_console, dig_app=dig_app)
    check_txt_for_service('local', '_test', '_tcp', mdns_console=mdns_console, dig_app=dig_app, with_inst=True)


def test_update_delegated_txt(mdns_console, dig_app):
    check_txt_for_service('extern', '_test2', '_tcp', mdns_console=mdns_console, dig_app=dig_app, host='delegated')
    check_txt_for_service('extern', '_test2', '_tcp', mdns_console=mdns_console, dig_app=dig_app, host='delegated', with_inst=True)


def test_service_port_set(mdns_console, dig_app):
    dig_app.check_record('local._test._tcp.local', query_type='SRV', expected=True, expect='80')
    mdns_console.send_input('mdns_service_port_set _test _tcp 81')
    dig_app.check_record('local._test._tcp.local', query_type='SRV', expected=True, expect='81')
    mdns_console.send_input('mdns_service_port_set _test2 _tcp -h delegated 82')
    dig_app.check_record('extern._test2._tcp.local', query_type='SRV', expected=True, expect='82')
    mdns_console.send_input('mdns_service_port_set _test2 _tcp -h delegated -i extern 83')
    dig_app.check_record('extern._test2._tcp.local', query_type='SRV', expected=True, expect='83')
    mdns_console.send_input('mdns_service_port_set _test2 _tcp -h delegated -i invalid_inst 84')
    mdns_console.get_output('ESP_ERR_NOT_FOUND')
    dig_app.check_record('extern._test2._tcp.local', query_type='SRV', expected=True, expect='83')


def test_service_subtype(mdns_console, dig_app):
    dig_app.check_record('local._test._tcp.local', query_type='SRV', expected=True)
    mdns_console.send_input('mdns_service_subtype _test _tcp _subtest -i local')
    dig_app.check_record('_subtest._sub._test._tcp.local', query_type='PTR', expected=True)
    mdns_console.send_input('mdns_service_subtype _test2 _tcp _subtest2 -i extern -h delegated')
    dig_app.check_record('_subtest2._sub._test2._tcp.local', query_type='PTR', expected=True)


def test_service_set_instance(mdns_console, dig_app):
    dig_app.check_record('local._test._tcp.local', query_type='SRV', expected=True)
    mdns_console.send_input('mdns_service_instance_set _test _tcp local2')
    dig_app.check_record('local2._test._tcp.local', query_type='SRV', expected=True)
    mdns_console.send_input('mdns_service_instance_set _test2 _tcp extern2 -h delegated')
    mdns_console.send_input('mdns_service_lookup _test2 _tcp -d')
    mdns_console.get_output('PTR : extern2')
    dig_app.check_record('extern2._test2._tcp.local', query_type='SRV', expected=True)
    mdns_console.send_input('mdns_service_instance_set _test2 _tcp extern3 -h delegated -i extern')
    mdns_console.get_output('ESP_ERR_NOT_FOUND')


def test_service_remove_all(mdns_console, dig_app):
    mdns_console.send_input('mdns_service_remove_all')
    mdns_console.send_input('mdns_service_lookup _test2 _tcp -d')
    mdns_console.get_output('No results found!')
    mdns_console.send_input('mdns_service_lookup _test _tcp')
    mdns_console.get_output('No results found!')
    dig_app.check_record('_test._tcp.local', query_type='PTR', expected=False)


def _browse_v301test(mdns_console, *, reset_browse: bool = True):
    service = DEFAULTS['service']
    proto = DEFAULTS['proto']
    if reset_browse:
        mdns_console.send_input(f'mdns_browse_del {service} {proto}')
        mdns_console.get_output('mdns>')
    mdns_console.send_input(f'mdns_browse {service} {proto}')
    return mdns_console.wait_for_browse_result(timeout=10)


def test_browse_bonjour_additional_order_includes_ip(mdns_console, bonjour_responder):
    """Bonjour order (A/AAAA before SRV) must populate browse addresses on first response."""
    output = _browse_v301test(mdns_console)
    assert _console_line('PTR : ', DEFAULTS['instance']) in output
    assert _console_line('A   : ', DEFAULTS['ipv4']) in output
    mdns_console.send_input(f"mdns_browse_del {DEFAULTS['service']} {DEFAULTS['proto']}")
    mdns_console.get_output('mdns>')


def test_browse_srv_first_includes_ip(mdns_console, bonjour_responder_srv_first):
    """Control case: SRV before A/AAAA should populate browse addresses."""
    output = _browse_v301test(mdns_console)
    assert _console_line('PTR : ', DEFAULTS['instance']) in output
    assert _console_line('A   : ', DEFAULTS['ipv4']) in output
    mdns_console.send_input(f"mdns_browse_del {DEFAULTS['service']} {DEFAULTS['proto']}")
    mdns_console.get_output('mdns>')


def test_browse_ptr_goodbye_notifies_removal(mdns_console, bonjour_responder_goodbye):
    """Bonjour service removal via standalone PTR TTL=0 must notify browse consumers."""
    service = DEFAULTS['service']
    proto = DEFAULTS['proto']
    mdns_console.send_input(f'mdns_browse_del {service} {proto}')
    mdns_console.get_output('mdns>')
    mdns_console.send_input(f'mdns_browse {service} {proto}')
    mdns_console.wait_for_browse_result(timeout=10)
    mdns_console.send_input(f'mdns_browse {service} {proto}')
    output = mdns_console.wait_for_browse_goodbye(timeout=10)
    assert _console_line('PTR : ', DEFAULTS['instance']) in output
    mdns_console.send_input(f'mdns_browse_del {service} {proto}')
    mdns_console.get_output('mdns>')


if __name__ == '__main__':
    pytest.main(['-s', 'test_mdns.py'])
