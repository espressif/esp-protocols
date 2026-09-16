#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
"""Controlled mDNS peer for host querier tests (A / SRV only).

Answers only the configured hostname A and instance SRV questions. Ignores all
other traffic so system mDNS probes cannot create an answer storm.
"""

from __future__ import annotations

import argparse
import logging
import socket
import struct
import sys

import dns.flags
import dns.message
import dns.name
import dns.rdataclass
import dns.rdatatype
import dns.rrset

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')
logger = logging.getLogger(__name__)

MDNS_ADDR = '224.0.0.251'
MDNS_PORT = 5353

DEFAULTS = {
    'hostname': 'tinytester',
    'ipv4': '127.0.0.1',
    'instance': 'ESP32',
    'service': '_http',
    'proto': '_tcp',
    'port': 100,
}


def _fqdn(*labels: str) -> str:
    return '.'.join(labels) + '.local.'


def create_socket(interface: str | None) -> socket.socket:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except (AttributeError, OSError):
        pass

    if interface:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, interface.encode() + b'\0')

    sock.bind(('', MDNS_PORT))
    membership = struct.pack('=4s4s', socket.inet_aton(MDNS_ADDR), socket.inet_aton('0.0.0.0'))
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, membership)
    return sock


def build_a_response(query: dns.message.Message, hostname: str, ipv4: str) -> dns.message.Message:
    host_name = _fqdn(hostname)
    # Exact QR|AA (0x8400). dnspython make_response also sets RD, which is fine for
    # answer parsing but keep mDNS-shaped packets consistent with bonjour fixtures.
    response = dns.message.Message(id=query.id)
    response.flags = dns.flags.QR | dns.flags.AA
    response.answer.append(
        dns.rrset.from_text(host_name, 120, dns.rdataclass.IN, dns.rdatatype.A, ipv4)
    )
    return response


def build_srv_response(
    query: dns.message.Message,
    *,
    instance: str,
    service: str,
    proto: str,
    hostname: str,
    port: int,
    ipv4: str,
) -> dns.message.Message:
    instance_name = _fqdn(instance, service, proto)
    host_name = _fqdn(hostname)
    response = dns.message.Message(id=query.id)
    response.flags = dns.flags.QR | dns.flags.AA
    response.answer.append(
        dns.rrset.from_text(
            instance_name,
            120,
            dns.rdataclass.IN,
            dns.rdatatype.SRV,
            f'0 0 {port} {host_name}',
        )
    )
    response.additional.append(
        dns.rrset.from_text(host_name, 120, dns.rdataclass.IN, dns.rdatatype.A, ipv4)
    )
    return response


def match_question(query: dns.message.Message, args: argparse.Namespace):
    host_name = dns.name.from_text(_fqdn(args.hostname))
    instance_name = dns.name.from_text(_fqdn(args.instance, args.service, args.proto))
    for question in query.question:
        if question.rdtype == dns.rdatatype.A and question.name == host_name:
            return 'A'
        if question.rdtype == dns.rdatatype.SRV and question.name == instance_name:
            return 'SRV'
    return None


def send_response(sock: socket.socket, payload: bytes, source: tuple[str, int]) -> None:
    # mDNS queries usually come from port 5353 → multicast reply; one-shot dig uses ephemeral ports.
    destination = (MDNS_ADDR, MDNS_PORT) if source[1] == MDNS_PORT else source
    sock.sendto(payload, destination)
    logger.info('Sent %d byte response to %s (from %s:%d)', len(payload), destination, *source)


def serve(args: argparse.Namespace) -> None:
    sock = create_socket(args.interface)
    logger.info(
        'Querier responder on %s:%d (%s): A %s.local -> %s; SRV %s.%s.%s.local -> %s.local:%d',
        MDNS_ADDR,
        MDNS_PORT,
        args.interface or 'all interfaces',
        args.hostname,
        args.ipv4,
        args.instance,
        args.service,
        args.proto,
        args.hostname,
        args.port,
    )

    while True:
        try:
            data, source = sock.recvfrom(9000)
        except KeyboardInterrupt:
            logger.info('Stopping querier responder')
            break

        try:
            query = dns.message.from_wire(data)
        except Exception as exc:  # noqa: BLE001
            logger.debug('Ignoring non-DNS payload from %s:%d (%s)', source[0], source[1], exc)
            continue

        if query.flags & dns.flags.QR:
            continue

        kind = match_question(query, args)
        if kind == 'A':
            response = build_a_response(query, args.hostname, args.ipv4)
        elif kind == 'SRV':
            response = build_srv_response(
                query,
                instance=args.instance,
                service=args.service,
                proto=args.proto,
                hostname=args.hostname,
                port=args.port,
                ipv4=args.ipv4,
            )
        else:
            continue

        send_response(sock, response.to_wire(), source)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--interface', default='eth0')
    parser.add_argument('--hostname', default=DEFAULTS['hostname'])
    parser.add_argument('--ipv4', default=DEFAULTS['ipv4'])
    parser.add_argument('--instance', default=DEFAULTS['instance'])
    parser.add_argument('--service', default=DEFAULTS['service'])
    parser.add_argument('--proto', default=DEFAULTS['proto'])
    parser.add_argument('--port', type=int, default=DEFAULTS['port'])
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    serve(args)
    return 0


if __name__ == '__main__':
    sys.exit(main())
