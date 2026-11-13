import tempfile
import unittest
from pathlib import Path
from patch_audio_stability import patch_stability, RULES, MARKER

class StabilityPatchTests(unittest.TestCase):
    def fixture(self, folder):
        (folder/'library.properties').write_text('version=2.4.1\n')
        originals={}
        for name,rules in RULES.items():
            path=folder/'src'/name;path.parent.mkdir(parents=True,exist_ok=True)
            originals[path]='\n'.join(old for old,_ in rules)+'\n'
            path.write_text(originals[path])
        return originals

    def test_idempotent_and_backups(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);originals=self.fixture(folder)
            patch_stability(folder)
            patched={p:p.read_bytes() for p in originals}
            patch_stability(folder)
            for path,original in originals.items():
                self.assertEqual(patched[path],path.read_bytes())
                self.assertEqual(original,path.with_suffix(path.suffix+'.deskdock-original').read_text())
                self.assertIn(MARKER,path.read_text())
            self.assertEqual((folder/'src/libhelix-aac/DeskDockAacMemory.h').read_bytes(),
                             (Path(__file__).resolve().parents[1]/'aac_memory.h').read_bytes())

    def test_preflight_does_not_partially_modify_library(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);originals=self.fixture(folder)
            last=folder/'src/AudioOutputI2S.cpp';last.write_text('unexpected')
            with self.assertRaises(RuntimeError):patch_stability(folder)
            for path,original in originals.items():
                self.assertEqual('unexpected' if path==last else original,path.read_text())
                self.assertFalse(path.with_suffix(path.suffix+'.deskdock-original').exists())

    def test_version_and_incomplete_patch_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);originals=self.fixture(folder)
            (folder/'library.properties').write_text('version=2.5.0\n')
            with self.assertRaises(RuntimeError):patch_stability(folder)
            (folder/'library.properties').write_text('version=2.4.1\n')
            first=next(iter(originals));first.write_text(MARKER+'\n'+originals[first])
            with self.assertRaises(RuntimeError):patch_stability(folder)
