#include "../mp3_timing.h"
#include <cassert>
#include <vector>
#include <cstdio>
static void big(std::vector<uint8_t>&v,size_t at,uint32_t n){for(int i=3;i>=0;i--){v[at+i]=n;n>>=8;}}
int main(){
  Mp3SampleClock clock;clock.rate(44100);
  for(int i=0;i<44100;i++)clock.accepted();assert(clock.ms()==1000);
  assert(clock.ms()==1000); // Paused/empty output has no accepted frames.
  clock.rate(48000);for(int i=0;i<24000;i++)clock.accepted();assert(clock.ms()==1500);
  std::vector<uint8_t> data(417*20);
  for(size_t p=0;p<data.size();p+=417){data[p]=0xff;data[p+1]=0xfb;data[p+2]=0x90;}
  auto d=mp3ProbeDuration(data.data(),data.size(),16000*180);assert(d.ms==180000&&d.estimated);
  memcpy(data.data()+36,"Xing",4);big(data,40,1);big(data,44,6891);
  d=mp3ProbeDuration(data.data(),data.size(),999999);assert(d.ms==uint64_t(6891)*1152*1000/44100&&!d.estimated);
  memcpy(data.data()+36,"Info",4);d=mp3ProbeDuration(data.data(),data.size(),999999);assert(d.ms&&!d.estimated);
  memcpy(data.data()+36,"VBRI",4);big(data,50,6891);
  d=mp3ProbeDuration(data.data(),data.size(),999999);assert(d.ms&&!d.estimated);
  memset(data.data()+36,0,18);data[417+2]=0xa0; // Unindexed VBR must remain unknown.
  assert(!mp3ProbeDuration(data.data(),data.size(),999999).ms);
  assert(!mp3ProbeDuration(data.data(),3,999999).ms);
  std::vector<uint8_t> garbage(8000,0xff);assert(!mp3ProbeDuration(garbage.data(),garbage.size(),8000).ms);
  puts("PASS: accepted PCM timing, pause/rate changes, CBR estimate, Xing/Info/VBRI, unknown VBR and invalid headers");
}
