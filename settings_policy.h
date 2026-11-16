#pragma once
constexpr int kWeatherSlots = 6;
constexpr int deskWeatherCount(int count) {
  return count < 1 ? 1 : count > kWeatherSlots ? kWeatherSlots : count;
}
// v2 defaulted to "absent" without a UI control. Do not carry that hidden
// default into v3. Explicit choices made with the new control survive OTA.
constexpr int deskBatteryMode(int savedV3,int legacyV2) {
  return savedV3>=0&&savedV3<=2?savedV3:legacyV2==1?1:0;
}
constexpr bool deskBatteryVisible(int mode,int millivolts) {
  return mode==1||(mode==0&&millivolts>=2900);
}
