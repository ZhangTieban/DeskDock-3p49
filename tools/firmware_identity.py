"""Validate the pinned ESP32-S3 app format and DeskDock release identity."""
import hashlib
import re
import struct

PREFIX = b'DeskDock-3p49|rollback1|'

def validate_firmware(payload: bytes, version: str) -> None:
    if not 0x10000 <= len(payload) <= 0x400000:
        raise ValueError('Expected a 64 KiB to 4 MiB app image')
    if payload[0] != 0xE9 or not 1 <= payload[1] <= 16:
        raise ValueError('Invalid ESP image header')
    if struct.unpack_from('<H', payload, 12)[0] != 9 or payload[23] != 1:
        raise ValueError('Expected ESP32-S3 image with appended SHA-256')
    offset, checksum = 24, 0xEF
    for _ in range(payload[1]):
        if offset + 8 > len(payload)-32:
            raise ValueError('Truncated segment header')
        size = struct.unpack_from('<I', payload, offset+4)[0]
        offset += 8
        if offset+size > len(payload)-32:
            raise ValueError('Truncated segment data')
        for byte in payload[offset:offset+size]:
            checksum ^= byte
        offset += size
    checksum_offset = (offset // 16) * 16 + 15
    if checksum_offset + 33 != len(payload) or payload[checksum_offset] != checksum:
        raise ValueError('Invalid image length/checksum')
    if hashlib.sha256(payload[:-32]).digest() != payload[-32:]:
        raise ValueError('Invalid appended image SHA-256')
    versions = set(re.findall(re.escape(PREFIX)+rb'(\d{1,5}\.\d{1,5}\.\d{1,5})\|\x00', payload))
    if versions != {version.encode('ascii')} or any(int(p)>65535 for p in version.split('.')):
        raise ValueError('DeskDock board/version identity does not match release')
