#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
"""
Send a raw mDNS UDP payload (from AFL crash.bin) to the mDNS multicast group.

Usage:
  python tests/test_afl_fuzz_host/send_mdns.py crash.bin \
         [--group 224.0.0.251] [--port 5353] \
         [--src 192.168.1.10] [--bind-iface eth0] \
         [--count 1] [--interval 0.2]

Notes:
- The input file should contain the UDP payload (DNS message) only, as produced by the fuzzer.
- On Linux, specifying --bind-iface helps ensure the packet uses a specific interface.
"""

import argparse
import os
import socket
import sys
import time


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Send raw mDNS UDP payload to 224.0.0.251:5353")
    p.add_argument("bin", help="Path to UDP payload (e.g., crash.bin)")
    p.add_argument("--group", default="224.0.0.251", help="Multicast group (IPv4)")
    p.add_argument("--port", type=int, default=5353, help="Destination UDP port")
    p.add_argument("--src", help="Source IPv4 address to bind and use for multicast (sets IP_MULTICAST_IF)")
    p.add_argument("--bind-iface", help="Interface name to bind (Linux SO_BINDTODEVICE)")
    p.add_argument("--count", type=int, default=1, help="Number of times to send the payload")
    p.add_argument("--interval", type=float, default=0.2, help="Seconds between sends")
    return p.parse_args()


def main() -> int:
    args = parse_args()

    if not os.path.isfile(args.bin):
        print(f"File not found: {args.bin}", file=sys.stderr)
        return 2

    payload = open(args.bin, "rb").read()
    if not payload:
        print("Empty payload", file=sys.stderr)
        return 3

    dst = (args.group, args.port)

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # Reuse addr and set TTL/hop limit to 255 as commonly used by mDNS
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_TTL, 255)
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
        # Avoid receiving our own multicast
        s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 0)

        if args.src:
            # Bind to source and set multicast interface
            s.bind((args.src, 0))
            s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(args.src))

        if args.bind_iface and hasattr(socket, 'SO_BINDTODEVICE'):
            try:
                s.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, args.bind_iface.encode())
            except OSError as e:
                print(f"Warning: SO_BINDTODEVICE failed: {e}", file=sys.stderr)

        for i in range(args.count):
            s.sendto(payload, dst)
            if i + 1 < args.count:
                time.sleep(args.interval)

        print(f"Sent {args.count} packet(s) to {dst[0]}:{dst[1]} ({len(payload)} bytes)")
        return 0
    finally:
        s.close()


if __name__ == "__main__":
    raise SystemExit(main())
