#include "../dns_core_lock.h"
#include "../settings_policy.h"
static_assert(deskWeatherCount(0)==1 && deskWeatherCount(-100)==1);
static_assert(deskWeatherCount(1)==1 && deskWeatherCount(3)==3 && deskWeatherCount(6)==6);
static_assert(deskWeatherCount(7)==6 && deskWeatherCount(100)==6);
static_assert(deskBatteryMode(-1,-1)==0,"fresh firmware detects voltage instead of hiding it");
static_assert(deskBatteryMode(-1,2)==0,"legacy hidden default must not suppress an attached battery after OTA");
static_assert(deskBatteryMode(-1,0)==0&&deskBatteryMode(-1,1)==1&&deskBatteryMode(-1,3)==0);
static_assert(deskBatteryMode(2,0)==2&&deskBatteryMode(2,1)==2,"explicit new hide choice survives reboot/OTA");
static_assert(deskBatteryMode(0,2)==0&&deskBatteryMode(1,2)==1);
static_assert(deskBatteryVisible(deskBatteryMode(-1,2),4149),"measured 4.149V must appear despite the old hidden default");
static_assert(!deskBatteryVisible(0,0)&&!deskBatteryVisible(0,2899)&&deskBatteryVisible(0,2900));
static_assert(deskBatteryVisible(1,0)&&!deskBatteryVisible(2,4149));
constexpr bool dnsLockTest(bool alreadyHeld) {
  bool held=alreadyHeld, callbackSafe=false, nestedSafe=false;
  int locks=0, unlocks=0;
  auto query=[&]() { return held; };
  auto lock=[&]() { ++locks; held=true; };
  auto unlock=[&]() { ++unlocks; held=false; };
  deskClearDnsWithCoreLock(query,lock,unlock,[&]() {
    callbackSafe=held;
    deskClearDnsWithCoreLock(query,lock,unlock,[&]() { nestedSafe=held; });
  });
  return callbackSafe && nestedSafe && held==alreadyHeld &&
         locks==(alreadyHeld?0:1) && unlocks==locks;
}
static_assert(dnsLockTest(false), "DNS callback requires core lock");
static_assert(dnsLockTest(true), "Caller-owned core lock must remain held");
int main() { return dnsLockTest(false) && dnsLockTest(true) ? 0 : 1; }
