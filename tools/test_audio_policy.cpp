#include "../mp3_sync_scan.h"
#include "../audio_leveler.h"
#include "../ota_identity_scan.h"
#include <vector>
constexpr bool boundaryChecks() {
  unsigned char full[1536] = {}; full[1535]=0xff;
  if (deskMp3SyncOffset(full,1536)!=-1 || !deskMp3CarrySyncByte(full,1536)) return false;
  unsigned char split[]={0xff,0xfb};
  return deskMp3SyncOffset(split,2)==0 && deskMp3SyncOffset(split,1)==-1 &&
         deskMp3SyncOffset(split,0)==-1 && !deskMp3CarrySyncByte(split,0);
}
constexpr bool gainEquivalent(int start,int end) {
  for(int peak=start;peak<=end;++peak) {
    for(int gain: {1024,2048,4096,6144,8192}) {
      const int old=peak ? std::min(int64_t(gain),int64_t(30000)*4096/peak) : gain;
      if(audioLimitedGain(peak,gain)!=old) return false;
    }
  }
  return true;
}
static_assert(boundaryChecks(),"MP3 boundary and split sync regression");
static_assert(gainEquivalent(0,8192),"Gain equivalence 1");
static_assert(gainEquivalent(8193,16384),"Gain equivalence 2");
static_assert(gainEquivalent(16385,24576),"Gain equivalence 3");
static_assert(gainEquivalent(24577,32768),"Gain equivalence 4");
constexpr bool gainEdges() {
  for(int peak: {0,1,14999,15000,15001,29999,30000,30001,32767,32768}) {
    for(int gain=1024;gain<=8192;++gain) {
      const int old=peak ? std::min(int64_t(gain),int64_t(30000)*4096/peak) : gain;
      if(audioLimitedGain(peak,gain)!=old) return false;
    }
  }
  return true;
}
static_assert(gainEdges(),"Every gain at boundary peaks");

int main() {
  constexpr char marker[] = DESKDOCK_ROLLBACK_ID_PREFIX "0.2.05|";
  constexpr char other[] = DESKDOCK_ROLLBACK_ID_PREFIX "0.2.04|";
  for (size_t position=4060; position<=4100; ++position) {
    std::vector<uint8_t> image(8192,0);
    memcpy(image.data()+position,marker,sizeof(marker));
    char version[24] = {}; bool found=false;
    auto read=[&](uint32_t offset,uint8_t *block,size_t count) {
      memcpy(block,image.data()+offset,count); return true;
    };
    if (!deskReadImageIdentity(image.size(),version,found,read,[](){}) ||
        !found || strcmp(version,"0.2.05")) return 1;
    memcpy(image.data()+6000,other,sizeof(other));
    if (deskReadImageIdentity(image.size(),version,found,read,[](){})) return 2;
    std::fill(image.begin(),image.end(),0);
    if (!deskReadImageIdentity(image.size(),version,found,read,[](){}) || found) return 3;
    if (deskReadImageIdentity(image.size(),version,found,
        [](uint32_t,uint8_t *,size_t){return false;},[](){})) return 4;
  }
  return 0;
}
