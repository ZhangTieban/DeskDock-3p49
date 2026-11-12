import hashlib
import struct
import unittest
from firmware_identity import validate_firmware, PREFIX

def image(version='0.2.05', chip=9, extra=b'', prefix=PREFIX):
    header=bytearray(24); header[0:2]=bytes([0xE9,1]);header[23]=1
    struct.pack_into('<H',header,12,chip)
    data=(prefix+version.encode()+b'|\0'+extra).ljust(65536,b'\0')
    payload=header+struct.pack('<II',0x3c000020,len(data))+data
    checksum=0xEF
    for b in data: checksum^=b
    payload+=b'\0'*((15-len(payload)%16)%16)+bytes([checksum])
    return bytes(payload)+hashlib.sha256(payload).digest()

class IdentityTests(unittest.TestCase):
    def test_valid(self): validate_firmware(image(),'0.2.05')
    def test_old_version(self):
        with self.assertRaises(ValueError): validate_firmware(image('0.2.04'),'0.2.05')
    def test_wrong_chip(self):
        with self.assertRaises(ValueError): validate_firmware(image(chip=0),'0.2.05')
    def test_wrong_board(self):
        with self.assertRaises(ValueError): validate_firmware(image(prefix=b'OtherBoard|rollback1|'),'0.2.05')
    def test_ambiguous(self):
        with self.assertRaises(ValueError): validate_firmware(image(extra=PREFIX+b'0.2.04|\0'),'0.2.05')
    def test_corruption(self):
        data=bytearray(image());data[100]^=1
        with self.assertRaises(ValueError): validate_firmware(bytes(data),'0.2.05')
    def test_truncated(self):
        with self.assertRaises(ValueError): validate_firmware(image()[:-1],'0.2.05')
    def test_segment_overflow(self):
        data=bytearray(image());struct.pack_into('<I',data,28,0xffffffff)
        with self.assertRaises(ValueError): validate_firmware(bytes(data),'0.2.05')
    def test_bad_digest(self):
        data=bytearray(image());data[-1]^=1
        with self.assertRaisesRegex(ValueError,'SHA-256'): validate_firmware(bytes(data),'0.2.05')
    def test_bad_checksum_with_valid_digest(self):
        data=bytearray(image());data[-33]^=1
        data[-32:]=hashlib.sha256(data[:-32]).digest()
        with self.assertRaisesRegex(ValueError,'checksum'): validate_firmware(bytes(data),'0.2.05')
    def test_version_overflow(self):
        with self.assertRaises(ValueError): validate_firmware(image('65536.2.05'),'65536.2.05')

if __name__=='__main__': unittest.main()
