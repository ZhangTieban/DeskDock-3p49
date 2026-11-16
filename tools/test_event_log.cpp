#include "../event_log_policy.h"
static_assert(deskBackgroundHttpReady(false,false,0,90000,40000),"idle requests need no audio buffer");
static_assert(!deskBackgroundHttpReady(true,false,131071,90000,40000),"preserve the complete audio cushion");
static_assert(deskBackgroundHttpReady(true,false,131072,24576,16384),"sufficient audio and TLS budget");
static_assert(!deskBackgroundHttpReady(true,true,262144,90000,40000),"new station commands have priority");
static_assert(!deskBackgroundHttpReady(true,false,262144,24575,40000),"low total memory defers requests");
static_assert(!deskBackgroundHttpReady(true,false,262144,90000,16383),"fragmented memory defers requests");
static_assert(deskBackgroundHttpReady(true,false,65536,90000,40000,65536),"direct live radio must not wait forever for a larger-than-startup cushion");
static_assert(!deskBackgroundHttpReady(true,false,65535,90000,40000,65536),"direct radio still preserves its minimum cushion");
constexpr bool escaping() {
  char out[64]={};
  const size_t n=deskLogEscape(out,sizeof(out),"a\n\"\\");
  const char expected[]="a\\u000a\\\"\\\\";
  if(n!=sizeof(expected)-1)return false;
  for(size_t i=0;i<sizeof(expected);++i)if(out[i]!=expected[i])return false;
  char shortOut[6]={};
  return deskLogEscape(shortOut,sizeof(shortOut),"\n")==0 && shortOut[0]==0;
}
static_assert(escaping(),"JSON lines escape controls without partial escape sequences");
constexpr bool utf8() {
  char out[32]={};const char text[]="日本語";
  if(deskLogEscape(out,sizeof(out),text)!=sizeof(text)-1)return false;
  for(size_t i=0;i<sizeof(text);++i)if(out[i]!=text[i])return false;
  return true;
}
static_assert(utf8(),"UTF-8 Japanese text survives JSON encoding");
constexpr bool incompleteUtf8() {
  char incomplete[]={'a',(char)0xe6,(char)0x97,0};deskLogTrimUtf8(incomplete);
  char complete[]="日本語";deskLogTrimUtf8(complete);
  return incomplete[0]=='a' && incomplete[1]==0 && sizeof(complete)==10 && complete[8]!=0;
}
static_assert(incompleteUtf8(),"bounded messages never end with an incomplete UTF-8 character");
int main(){return 0;}
