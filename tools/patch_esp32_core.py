"""Apply the radio DNS/SNTP core-lock fix to pinned Arduino ESP32 3.3.7."""
from pathlib import Path
import argparse
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
MARKER = '// DeskDock DNS cache core-lock fix v1'
OLD = '    dns_clear_cache();'
NEW = '''#if defined(CONFIG_LWIP_TCPIP_CORE_LOCKING) && CONFIG_LWIP_TCPIP_CORE_LOCKING
    deskClearDnsWithCoreLock(
      []() { return sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER); },
      []() { LOCK_TCPIP_CORE(); },
      []() { UNLOCK_TCPIP_CORE(); },
      []() { dns_clear_cache(); });
#else
    dns_clear_cache();
#endif'''

def patch_core(folder: Path):
    if not re.search(r'^version=3\.3\.7\s*$', (folder/'platform.txt').read_text(encoding='utf-8'), re.M):
        raise RuntimeError('Expected Arduino ESP32 3.3.7; review before changing core versions')
    source = folder/'libraries/Network/src/NetworkManager.cpp'
    text = source.read_text(encoding='utf-8')
    if MARKER in text:
        if NEW not in text:
            raise RuntimeError('Incomplete DeskDock DNS patch')
    else:
        if text.count(OLD) != 1:
            raise RuntimeError('Unexpected NetworkManager source; refusing replacement')
        backup = source.with_suffix('.cpp.deskdock-original')
        if not backup.exists(): shutil.copy2(source, backup)
        text = MARKER+'\n#include "lwip/tcpip.h"\n#include "DeskDockDnsCoreLock.h"\n'+text.replace(OLD, NEW)
        source.write_text(text, encoding='utf-8')
    shutil.copyfile(ROOT/'dns_core_lock.h', source.parent/'DeskDockDnsCoreLock.h')
    print('Verified DeskDock DNS core-lock patch: '+str(source))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--core-dir', required=True, type=Path)
    patch_core(parser.parse_args().core_dir)
