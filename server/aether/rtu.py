"""Bounded Modbus RTU slave codec, ID 1, FC03 and FC06 only."""
from __future__ import annotations


def crc16(data: bytes) -> int:
    crc = 0xffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xa001 if crc & 1 else 0)
    return crc


def frame(body: bytes) -> bytes:
    return body + crc16(body).to_bytes(2, 'little')


def response(request: bytes, registers, restart) -> bytes | None:
    if len(request) < 4 or request[0] != 1 or crc16(request[:-2]) != int.from_bytes(request[-2:], 'little'):
        return None
    function = request[1]
    def exception(code):
        return frame(bytes((1, function | 0x80, code)))
    if len(request) != 8:
        return exception(3)
    address, value = int.from_bytes(request[2:4], 'big'), int.from_bytes(request[4:6], 'big')
    if function == 3:
        if not 1 <= value <= 5:
            return exception(3)
        if address + value > 5:
            return exception(2)
        words = registers()
        if words is None or len(words) != 5 or any(type(w) is not int or not 0 <= w <= 65535 for w in words):
            return exception(4)
        return frame(bytes((1, 3, value * 2)) + b''.join(int(w).to_bytes(2, 'big') for w in words[address:address + value]))
    if function == 6:
        if address != 5:
            return exception(2)
        if value != 1:
            return exception(3)
        return request if restart('MODBUS') else exception(6)
    return exception(1)
