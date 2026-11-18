"""Small RFC7252 datagram codec; no blockwise, observe or DTLS."""
from __future__ import annotations

from dataclasses import dataclass
import secrets
import socket


@dataclass
class Message:
    kind: int
    code: int
    mid: int
    token: bytes = b''
    options: tuple = ()
    payload: bytes = b''

    @property
    def path(self):
        return '/' + '/'.join(v.decode('ascii') for n, v in self.options if n == 11)


def _extended(value):
    if value < 13: return value, b''
    if value < 269: return 13, bytes((value - 13,))
    if value < 65805: return 14, (value - 269).to_bytes(2, 'big')
    raise ValueError('option too long')


def encode(message: Message):
    if len(message.token) > 8: raise ValueError('token too long')
    result = bytearray((0x40 | message.kind << 4 | len(message.token), message.code))
    result.extend(message.mid.to_bytes(2, 'big')); result.extend(message.token)
    previous = 0
    for number, value in sorted(message.options, key=lambda p: p[0]):
        delta, dx = _extended(number - previous); length, lx = _extended(len(value))
        result.append(delta << 4 | length); result.extend(dx); result.extend(lx); result.extend(value)
        previous = number
    if message.payload: result.extend(b'\xff' + message.payload)
    return bytes(result)


def decode(data: bytes):
    if len(data) < 4 or len(data) > 2048 or data[0] >> 6 != 1: raise ValueError('header')
    tkl = data[0] & 15
    if tkl > 8 or len(data) < 4 + tkl: raise ValueError('token')
    offset, number, options = 4 + tkl, 0, []
    def field(nibble):
        nonlocal offset
        if nibble < 13: return nibble
        count = {13: 1, 14: 2}.get(nibble)
        if count is None or offset + count > len(data): raise ValueError('option')
        value = int.from_bytes(data[offset:offset + count], 'big') + (13 if count == 1 else 269)
        offset += count
        return value
    payload = b''
    while offset < len(data):
        head = data[offset]; offset += 1
        if head == 255:
            if offset == len(data): raise ValueError('empty payload marker')
            payload = data[offset:]; break
        delta = field(head >> 4); length = field(head & 15)
        number += delta
        if offset + length > len(data): raise ValueError('truncated option')
        options.append((number, data[offset:offset + length])); offset += length
    return Message((data[0] >> 4) & 3, data[1], int.from_bytes(data[2:4], 'big'), data[4:4+tkl], tuple(options), payload)


def request(host: str, path: str, method=1, payload=b'', timeout=1.0):
    token, mid = secrets.token_bytes(4), secrets.randbelow(65536)
    message = Message(0, method, mid, token, tuple((11, p.encode()) for p in path.strip('/').split('/')), payload)
    # No automatic retry of actuator PUT; timeout is an unknown outcome.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout); sock.connect((host, 5683)); sock.send(encode(message))
        reply = decode(sock.recv(2048))
    if reply.token != token or reply.kind != 2 or reply.mid != mid: raise ValueError('unmatched CoAP response')
    return {'code': f'{reply.code >> 5}.{reply.code & 31:02}', 'payload': reply.payload.decode('utf8', errors='replace')}
