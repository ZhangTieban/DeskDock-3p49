import tempfile
import unittest
from pathlib import Path
from patch_audio_library import patch_library, OLD_SCAN, OLD_CARRY, MARKER

class AudioPatchTests(unittest.TestCase):
    def test_pinned_idempotent_and_backup(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);(folder/'src').mkdir()
            (folder/'library.properties').write_text('version=2.4.1\n')
            source=folder/'src/AudioGeneratorMP3.cpp'
            original=OLD_SCAN+'\n'+OLD_CARRY
            source.write_text(original)
            patch_library(folder)
            patched=source.read_bytes();patch_library(folder)
            self.assertEqual(patched,source.read_bytes())
            self.assertEqual(original,source.with_suffix('.cpp.deskdock-original').read_text())
            self.assertIn(MARKER,source.read_text())
            self.assertTrue((folder/'src/DeskDockMp3Sync.h').exists())
    def test_unknown_version_untouched(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);(folder/'library.properties').write_text('version=3.0.0\n')
            with self.assertRaises(RuntimeError): patch_library(folder)
            self.assertFalse((folder/'src').exists())
    def test_unexpected_source_untouched(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);(folder/'src').mkdir()
            (folder/'library.properties').write_text('version=2.4.1\n')
            source=folder/'src/AudioGeneratorMP3.cpp';source.write_text('unexpected')
            with self.assertRaises(RuntimeError): patch_library(folder)
            self.assertEqual(source.read_text(),'unexpected')
