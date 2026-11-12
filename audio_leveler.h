#pragma once
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <algorithm>

// Shared radio/MP3 loudness compensation, with bounded gain and peak limiting.
// Feed the meter only after the I2S driver accepts the sample (retries are common).
class AudioLeveler {
 public:
  void transform(const int16_t input[2], int16_t output[2]) const {
    const int peak = std::max(abs(int(input[0])), abs(int(input[1])));
    const int gain = peak ? std::min(gain_, int((int64_t(30000) * 4096) / peak)) : gain_;
    output[0] = int32_t(input[0]) * gain / 4096;
    output[1] = int32_t(input[1]) * gain / 4096;
  }
  void accepted(const int16_t input[2]) {
    energy_ += int64_t(input[0])*input[0] + int64_t(input[1])*input[1];
    if (++samples_ != 2048) return;
    const float rms = sqrtf(float(energy_) / 4096.0f);
    // Do not raise the noise floor during pauses or silence.
    if (rms >= 128.0f) {
      const int desired = std::max(1024, std::min(8192, int(4200.0f * 4096.0f / rms)));
      gain_ += (desired - gain_) / (desired < gain_ ? 2 : 16);
    }
    energy_ = 0; samples_ = 0;
  }
 private:
  int gain_ = 4096;
  uint64_t energy_ = 0;
  unsigned samples_ = 0;
};
