#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstring>

struct Mp3Duration { uint32_t ms=0; bool estimated=false; };
struct Mp3Header { uint32_t rate=0, bitrate=0, bytes=0, samples=0; bool mono=false, mpeg1=false, crc=false; };
inline uint32_t mp3Big32(const uint8_t *p) {
  return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];
}
inline Mp3Header mp3Header(const uint8_t *p) {
  const uint32_t h=mp3Big32(p); Mp3Header f;
  const unsigned version=(h>>19)&3,layer=(h>>17)&3,bit=(h>>12)&15,sr=(h>>10)&3;
  if((h&0xffe00000)!=0xffe00000||version==1||layer!=1||!bit||bit==15||sr==3)return f;
  static constexpr uint16_t rates[]={44100,48000,32000};
  static constexpr uint16_t high[]={0,32,40,48,56,64,80,96,112,128,160,192,224,256,320};
  static constexpr uint16_t low[]={0,8,16,24,32,40,48,56,64,80,96,112,128,144,160};
  f.mpeg1=version==3; f.rate=rates[sr]/(f.mpeg1?1:version==2?2:4);
  f.bitrate=(f.mpeg1?high[bit]:low[bit])*1000U; f.samples=f.mpeg1?1152:576;
  f.bytes=(f.mpeg1?144:72)*f.bitrate/f.rate+((h>>9)&1);
  f.mono=((h>>6)&3)==3; f.crc=((h>>16)&1)==0; return f;
}
// Probe at most 64 KiB after ID3v2. Never scan an entire song before playback.
inline Mp3Duration mp3ProbeDuration(const uint8_t *data,size_t size,uint32_t audioBytes) {
  for(size_t at=0;at+4<size;at++) {
    const auto f=mp3Header(data+at);
    if(!f.rate||at+f.bytes+4>size)continue;
    const auto next=mp3Header(data+at+f.bytes);
    if(next.rate!=f.rate)continue; // Reject false sync words in cover art/tags.
    const unsigned side=f.mpeg1?(f.mono?17:32):(f.mono?9:17);
    for(unsigned extra=0;extra<=2;extra+=2) {
      const size_t x=at+4+side+extra;
      if(x+12<=at+f.bytes&&(!memcmp(data+x,"Xing",4)||!memcmp(data+x,"Info",4))&&
         (mp3Big32(data+x+4)&1)) {
        const uint32_t frames=mp3Big32(data+x+8);
        const uint64_t ms=uint64_t(frames)*f.samples*1000/f.rate;
        if(frames&&ms<=UINT32_MAX)return {(uint32_t)ms,false};
      }
    }
    const size_t v=at+36;
    if(v+18<=at+f.bytes&&!memcmp(data+v,"VBRI",4)) {
      const uint32_t frames=mp3Big32(data+v+14);
      const uint64_t ms=uint64_t(frames)*f.samples*1000/f.rate;
      if(frames&&ms<=UINT32_MAX)return {(uint32_t)ms,false};
    }
    unsigned frames=0;size_t p=at;bool cbr=true;
    while(p+4<=size) {
      const auto h=mp3Header(data+p);if(!h.rate||p+h.bytes>size)break;
      if(h.rate!=f.rate||h.bitrate!=f.bitrate){cbr=false;break;}
      ++frames;p+=h.bytes;
    }
    if(cbr&&frames>=8&&audioBytes>at) {
      const uint64_t ms=uint64_t(audioBytes-at)*8000/f.bitrate;
      if(ms<=UINT32_MAX)return {(uint32_t)ms,true};
    }
    return {}; // Headerless VBR: remaining time is unknown, not a byte percentage.
  }
  return {};
}

// Count only accepted PCM frames, excluding pause/underrun waits and EOF silence.
class Mp3SampleClock {
 public:
  void rate(uint32_t value) {
    if(value==hz_)return;
    if(hz_)baseUs_+=uint64_t(frames_)*1000000/hz_;
    frames_=0;hz_=value;
  }
  void accepted(){if(hz_)++frames_;}
  uint32_t ms()const{return (baseUs_+(hz_?uint64_t(frames_)*1000000/hz_:0))/1000;}
 private:
  uint64_t baseUs_=0,frames_=0;
  uint32_t hz_=0;
};
