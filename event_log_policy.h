#pragma once
#include <stddef.h>
#include <stdint.h>
// HLS shares the HTTP gate and needs a larger cushion. Direct ICY streams keep
// receiving on their independent socket, including while background HTTP runs.
constexpr bool deskBackgroundHttpReady(bool playing, bool connecting, uint32_t buffer,
                                       uint32_t heap, uint32_t largest, uint32_t cushion=131072) {
  return !connecting && (!playing || buffer >= cushion) && heap >= 24576 && largest >= 16384;
}
// JSON strings preserve UTF-8, escaping control characters and quotes. Always terminate.
constexpr char deskLogHex[]="0123456789abcdef";
constexpr void deskLogTrimUtf8(char *text) {
  size_t length=0;while(text[length])++length;
  if(!length)return;
  size_t lead=length-1;
  while(lead && ((unsigned char)text[lead]&0xc0)==0x80)--lead;
  const unsigned char c=(unsigned char)text[lead];
  const size_t bytes=c>=0xf0 ? 4 : c>=0xe0 ? 3 : c>=0xc0 ? 2 : 1;
  if(length-lead<bytes)text[lead]=0;
}
// Return bytes consumed, keeping each part independently valid UTF-8.
constexpr size_t deskLogTextPart(char *out,size_t capacity,const char *text) {
  if(!capacity)return 0;
  size_t n=0;while(text&&text[n]&&n+1<capacity){out[n]=text[n];++n;}
  out[n]=0;deskLogTrimUtf8(out);
  n=0;while(out[n])++n;return n;
}
constexpr bool deskMp3BufferLow(bool wasLow,bool playing,uint32_t buffered,uint32_t remaining) {
  return playing&&remaining>4096&&buffered<(wasLow?16384U:2048U);
}
constexpr size_t deskLogEscape(char *out, size_t capacity, const char *text) {
  size_t n=0;
  if (!capacity) return 0;
  for (const char *p=text ? text : ""; *p; ++p) {
    const unsigned char c=(unsigned char)*p;
    const size_t needed=c<32 ? 6 : (c=='"' || c=='\\') ? 2 : 1;
    if (n+needed>=capacity) break;
    if (c<32) {
      out[n++]='\\';out[n++]='u';out[n++]='0';out[n++]='0';
      out[n++]=deskLogHex[c>>4];out[n++]=deskLogHex[c&15];
    } else {
      if (c=='"' || c=='\\') out[n++]='\\';
      out[n++]=(char)c;
    }
  }
  out[n]=0;
  return n;
}
