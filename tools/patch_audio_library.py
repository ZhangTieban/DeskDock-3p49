"""Apply the same narrowly scoped ESP8266Audio 2.4.1 fix locally and in CI."""
import argparse
from pathlib import Path
import re
import shutil

ROOT=Path(__file__).resolve().parents[1]
MARKER='// DeskDock MP3 sync boundary fix v1'
OLD_SCAN='''        for (int i = 0; i < lastBuffLen; i++) {
            if ((buff[i] == 0xff) && ((buff[i + 1] & 0xe0) == 0xe0)) {
                // We have a header!
                if (i) {
                    memmove(buff, buff + i, lastBuffLen - i);
                    lastBuffLen -= i;
                }
                foundHeader = true;
                break;
            }
        }'''
NEW_SCAN='''        const int syncOffset = deskMp3SyncOffset(buff, lastBuffLen);
        if (syncOffset >= 0) {
            if (syncOffset) {
                memmove(buff, buff + syncOffset, lastBuffLen - syncOffset);
                lastBuffLen -= syncOffset;
            }
            foundHeader = true;
        }'''
OLD_CARRY='''        if (!foundHeader) {
            unused = 0;
        } else {'''
NEW_CARRY='''        if (!foundHeader) {
            // Keep a split sync prefix, but do not spin on a trailing FF at EOF.
            if (len == 0) return MAD_FLOW_STOP;
            unused = deskMp3CarrySyncByte(buff, lastBuffLen) ? 1 : 0;
            if (unused) buff[0] = 0xff;
        } else {'''

def patch_library(folder:Path):
    properties=(folder/'library.properties').read_text(encoding='utf-8')
    if not re.search(r'^version=2\.4\.1\s*$',properties,re.M):
        raise RuntimeError('Expected ESP8266Audio 2.4.1; review the patch before changing dependency versions')
    source=folder/'src/AudioGeneratorMP3.cpp'
    text=source.read_text(encoding='utf-8')
    if MARKER in text:
        if NEW_SCAN not in text or NEW_CARRY not in text:
            raise RuntimeError('Existing DeskDock patch is incomplete')
    else:
        if text.count(OLD_SCAN)!=1 or text.count(OLD_CARRY)!=1:
            raise RuntimeError('Unexpected MP3 decoder source; refusing an unverified replacement')
        backup=source.with_suffix('.cpp.deskdock-original')
        if not backup.exists(): shutil.copy2(source,backup)
        text=MARKER+'\n#include "DeskDockMp3Sync.h"\n'+text.replace(OLD_SCAN,NEW_SCAN).replace(OLD_CARRY,NEW_CARRY)
        source.write_text(text,encoding='utf-8')
    shutil.copyfile(ROOT/'mp3_sync_scan.h',folder/'src/DeskDockMp3Sync.h')
    print('Verified DeskDock MP3 boundary/carry/EOF patch: '+str(source))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--library-dir',required=True,type=Path)
    patch_library(parser.parse_args().library_dir)
