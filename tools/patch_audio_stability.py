"""Pinned ESP8266Audio 2.4.1 AAC memory and recoverable I2S errors."""
from pathlib import Path
import argparse
import re
import shutil

ROOT=Path(__file__).resolve().parents[1]
MARKER='// DeskDock audio stability fix v1'
RULES={
 'libhelix-aac/buffers.c':[
  ('(AACDecInfo *)malloc(sizeof(AACDecInfo))','(AACDecInfo *)deskAacAllocate(sizeof(AACDecInfo))'),
  ('malloc(sizeof(PSInfoBase))','deskAacAllocate(sizeof(PSInfoBase))')],
 'libhelix-aac/sbr.c':[
  ('(PSInfoSBR *)malloc(sizeof(PSInfoSBR))','(PSInfoSBR *)deskAacAllocate(sizeof(PSInfoSBR))')],
 'AudioOutputI2S.cpp':[
  ('    hertz = hz;\n    if (i2sOn)', '    const int previousRate = hertz;\n    hertz = hz;\n    if (i2sOn)'),
  ('''        i2s_channel_disable(_tx_handle);
        i2s_channel_reconfig_std_clock(_tx_handle, &clk_cfg);
        i2s_channel_enable(_tx_handle);''', '''        if (i2s_channel_disable(_tx_handle) != ESP_OK ||
            i2s_channel_reconfig_std_clock(_tx_handle, &clk_cfg) != ESP_OK ||
            i2s_channel_enable(_tx_handle) != ESP_OK) {
            stop();
            hertz = previousRate;
            return false;
        }'''),
  ('    assert(ESP_OK == i2s_new_channel(&chan_cfg, &_tx_handle, nullptr));', '''    if (i2sOn) return true;
    _tx_handle = nullptr;
    if (i2s_new_channel(&chan_cfg, &_tx_handle, nullptr) != ESP_OK) return false;'''),
  ('    assert(ESP_OK == i2s_channel_init_std_mode(_tx_handle, &std_cfg));', '''    if (i2s_channel_init_std_mode(_tx_handle, &std_cfg) != ESP_OK) {
        i2s_del_channel(_tx_handle);
        _tx_handle = nullptr;
        return false;
    }'''),
  ('    i2sOn = (ESP_OK == i2s_channel_enable(_tx_handle));', '''    i2sOn = (ESP_OK == i2s_channel_enable(_tx_handle));
    if (!i2sOn) {
        i2s_del_channel(_tx_handle);
        _tx_handle = nullptr;
        return false;
    }'''),
  ('    SetRate(hertz ? hertz : 44100); // Default', '''    if (!SetRate(hertz ? hertz : 44100)) {
        stop();
        return false;
    }''')]
}

def patch_stability(folder:Path):
    if not re.search(r'^version=2\.4\.1\s*$',(folder/'library.properties').read_text(),re.M):
        raise RuntimeError('Expected ESP8266Audio 2.4.1')
    planned=[]
    for name,rules in RULES.items():
        path=folder/'src'/name
        text=path.read_text(encoding='utf-8')
        if MARKER in text:
            if any(new not in text for _,new in rules):
                raise RuntimeError('Incomplete stability patch: '+name)
        else:
            if any(text.count(old)!=1 for old,_ in rules):
                raise RuntimeError('Unexpected pinned source: '+name)
            for old,new in rules:text=text.replace(old,new)
            text=MARKER+'\n'+('#include "DeskDockAacMemory.h"\n' if name.endswith('.c') else '')+text
        planned.append((path,text))
    # Validate every file before writing any of them.
    for path,text in planned:
        backup=path.with_suffix(path.suffix+'.deskdock-original')
        if not backup.exists():shutil.copy2(path,backup)
        if path.read_text(encoding='utf-8')!=text:path.write_text(text,encoding='utf-8')
    shutil.copyfile(ROOT/'aac_memory.h',folder/'src/libhelix-aac/DeskDockAacMemory.h')
    print('Verified AAC PSRAM + recoverable I2S patch: '+str(folder))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--library-dir',type=Path,required=True)
    patch_stability(parser.parse_args().library_dir)
