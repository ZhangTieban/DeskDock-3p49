#pragma once
constexpr int kWeatherSlots = 6;
constexpr int deskWeatherCount(int count) {
  return count < 1 ? 1 : count > kWeatherSlots ? kWeatherSlots : count;
}
