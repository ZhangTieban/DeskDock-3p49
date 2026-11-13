from pathlib import Path
import tempfile
import unittest
from patch_esp32_core import patch_core, OLD, NEW

class DnsPatchTests(unittest.TestCase):
    def fixture(self, folder, version='3.3.7', body=OLD):
        (folder/'platform.txt').write_text('version='+version+'\n')
        source=folder/'libraries/Network/src/NetworkManager.cpp'
        source.parent.mkdir(parents=True)
        source.write_text(body)
        return source

    def test_idempotence_backup_and_bounded_scope(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp)
            original='void host() {\n'+OLD+'\n  blocking_connect();\n}\n'
            source=self.fixture(folder,body=original)
            patch_core(folder);first=source.read_bytes();patch_core(folder)
            self.assertEqual(first,source.read_bytes())
            self.assertEqual(source.with_suffix('.cpp.deskdock-original').read_text(),original)
            self.assertIn(NEW+'\n  blocking_connect();',source.read_text())
            self.assertTrue((source.parent/'DeskDockDnsCoreLock.h').is_file())

    def test_unexpected_version_untouched(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);source=self.fixture(folder,version='3.4.0')
            with self.assertRaises(RuntimeError): patch_core(folder)
            self.assertEqual(source.read_text(),OLD)

    def test_unexpected_source_untouched(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);source=self.fixture(folder,body='modified source')
            with self.assertRaises(RuntimeError): patch_core(folder)
            self.assertEqual(source.read_text(),'modified source')
