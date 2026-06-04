#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
"""Mock Bonjour-style mDNS responder for browse host tests.

Default response order mimics Bonjour browse answers:
  Answer: PTR
  Additional: A, AAAA, SRV, TXT

Use ``--goodbye-after N`` to answer the first N browse queries normally and then
reply with a standalone PTR TTL=0 (Bonjour service removal).
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
    'service': '_v301test',
    'proto': '_tcp',
    'instance': 'v301instance',
    'hostname': 'v301host',
    'port': 8080,
    'ipv4': '192.168.1.100',
    'ipv6': 'fe80::1234:5678:9abc:def0',
    'txt': 'vdv301=test',
}


def _fqdn(*labels: str) -> str:
    return '.'.join(labels) + '.local.'


def build_goodbye_response(
    query: dns.message.Message,
    *,
    service: str,
    proto: str,
    instance: str,
) -> dns.message.Message:
    service_name = _fqdn(service, proto)
    instance_name = _fqdn(instance, service, proto)

    response = dns.message.make_response(query)
    response.flags |= dns.flags.AA
    response.answer.append(
        dns.rrset.from_text(service_name, 0, dns.rdataclass.IN, dns.rdatatype.PTR, instance_name)
    )
    return response


def build_ptr_response(
    query: dns.message.Message,
    *,
    service: str,
    proto: str,
    instance: str,
    hostname: str,
    port: int,
    ipv4: str,
    ipv6: str,
    txt: str,
    srv_first: bool,
) -> dns.message.Message:
    service_name = _fqdn(service, proto)
    instance_name = _fqdn(instance, service, proto)
    host_name = _fqdn(hostname)

    response = dns.message.make_response(query)
    response.flags |= dns.flags.AA

    response.answer.append(
        dns.rrset.from_text(service_name, 120, dns.rdataclass.IN, dns.rdatatype.PTR, instance_name)
    )

    a_rr = dns.rrset.from_text(host_name, 120, dns.rdataclass.IN, dns.rdatatype.A, ipv4)
    aaaa_rr = dns.rrset.from_text(host_name, 120, dns.rdataclass.IN, dns.rdatatype.AAAA, ipv6)
    srv_rr = dns.rrset.from_text(
        instance_name,
        120,
        dns.rdataclass.IN,
        dns.rdatatype.SRV,
        f'0 0 {port} {host_name}',
    )
    txt_rr = dns.rrset.from_text(instance_name, 120, dns.rdataclass.IN, dns.rdatatype.TXT, txt)

    if srv_first:
        response.additional = [srv_rr, txt_rr, a_rr, aaaa_rr]
    else:
        response.additional = [a_rr, aaaa_rr, srv_rr, txt_rr]

    return response


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

    membership = struct.pack(
        '=4s4s',
        socket.inet_aton(MDNS_ADDR),
        socket.inet_aton('0.0.0.0'),
    )
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, membership)
    return sock


def query_matches(query: dns.message.Message, service: str, proto: str) -> bool:
    target = dns.name.from_text(_fqdn(service, proto))
    for question in query.question:
        if question.rdtype != dns.rdatatype.PTR:
            continue
        if question.name == target:
            return True
    return False


def send_mdns_response(sock: socket.socket, payload: bytes, source: tuple[str, int]) -> None:
    if source[1] == MDNS_PORT:
        destination = (MDNS_ADDR, MDNS_PORT)
    else:
        destination = source
    sock.sendto(payload, destination)
    logger.info('Sent %d byte response to %s (query source %s:%d)', len(payload), destination, *source)


def serve(args: argparse.Namespace) -> None:
    sock = create_socket(args.interface)
    order = 'SRV, TXT, A, AAAA' if args.srv_first else 'A, AAAA, SRV, TXT'
    logger.info(
        'Listening on %s:%d (%s) for PTR %s.%s.local; additional order: %s; goodbye-after=%s',
        MDNS_ADDR,
        MDNS_PORT,
        args.interface or 'all interfaces',
        args.service,
        args.proto,
        order,
        args.goodbye_after,
    )

    response_count = 0
    while True:
        try:
            data, source = sock.recvfrom(9000)
        except KeyboardInterrupt:
            logger.info('Stopping responder')
            break

        try:
            query = dns.message.from_wire(data)
        except Exception as exc:  # noqa: BLE001 - ignore malformed packets
            logger.debug('Ignoring non-DNS payload from %s:%d (%s)', source[0], source[1], exc)
            continue

        if query.flags & dns.flags.QR:
            continue
        if not query_matches(query, args.service, args.proto):
            continue

        if args.goodbye_after >= 0 and response_count >= args.goodbye_after:
            response = build_goodbye_response(
                query,
                service=args.service,
                proto=args.proto,
                instance=args.instance,
            )
            logger.info('Answered PTR goodbye (TTL=0) from %s:%d', source[0], source[1])
        else:
            response = build_ptr_response(
                query,
                service=args.service,
                proto=args.proto,
                instance=args.instance,
                hostname=args.hostname,
                port=args.port,
                ipv4=args.ipv4,
                ipv6=args.ipv6,
                txt=args.txt,
                srv_first=args.srv_first,
            )
            logger.info('Answered PTR browse from %s:%d', source[0], source[1])

        send_mdns_response(sock, response.to_wire(), source)
        response_count += 1


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--interface', default='eth0', help='Network interface for mDNS (default: eth0)')
    parser.add_argument('--service', default=DEFAULTS['service'], help='Service type without domain')
    parser.add_argument('--proto', default=DEFAULTS['proto'], help='Protocol (_tcp/_udp)')
    parser.add_argument('--instance', default=DEFAULTS['instance'], help='Service instance name')
    parser.add_argument('--hostname', default=DEFAULTS['hostname'], help='SRV target host label')
    parser.add_argument('--port', type=int, default=DEFAULTS['port'], help='SRV port')
    parser.add_argument('--ipv4', default=DEFAULTS['ipv4'], help='A record address')
    parser.add_argument('--ipv6', default=DEFAULTS['ipv6'], help='AAAA record address')
    parser.add_argument('--txt', default=DEFAULTS['txt'], help='TXT record payload')
    parser.add_argument(
        '--srv-first',
        action='store_true',
        help='Place SRV/TXT before A/AAAA (control case where browse gets addresses)',
    )
    parser.add_argument(
        '--goodbye-after',
        type=int,
        default=-1,
        help='After N normal browse responses, reply with PTR TTL=0 only (-1 disables)',
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        serve(args)
    except PermissionError:
        logger.error('Binding to port %d requires root or CAP_NET_BIND_SERVICE', MDNS_PORT)
        return 1
    except OSError as exc:
        logger.error('Failed to start responder: %s', exc)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
