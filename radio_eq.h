#pragma once

#include <math.h>
#include <stdint.h>

// Three RBJ biquads in the decoded PCM path. Coefficients are rebuilt when
// the stream sample rate or a user gain changes; sample processing allocates nothing.
class RadioEq {
 public:
  void configure(int sampleRate, int bassDb, int midDb, int trebleDb) {
    enabled_ = bassDb || midDb || trebleDb;
    if (!enabled_) return;
    const int boosts = (bassDb > 0 ? bassDb : 0) +
                       (midDb > 0 ? midDb : 0) +
                       (trebleDb > 0 ? trebleDb : 0);
    headroom_ = powf(10.0f, -boosts / 20.0f);
    low_.configure(Biquad::LowShelf, 120.0f, sampleRate, bassDb);
    mid_.configure(Biquad::Peak, 1000.0f, sampleRate, midDb);
    high_.configure(Biquad::HighShelf, 6000.0f, sampleRate, trebleDb);
  }

  void process(int16_t sample[2]) {
    if (!enabled_) return;
    for (int channel = 0; channel < 2; ++channel) {
      float value = sample[channel] * headroom_;
      value = low_.process(value, channel);
      value = mid_.process(value, channel);
      value = high_.process(value, channel);
      if (value > 32767.0f) value = 32767.0f;
      if (value < -32768.0f) value = -32768.0f;
      sample[channel] = static_cast<int16_t>(value >= 0.0f ? value + 0.5f :
                                            value - 0.5f);
    }
  }

 private:
  struct Biquad {
    enum Type { LowShelf, Peak, HighShelf };
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1[2] = {}, z2[2] = {};

    void configure(Type type, float frequency, int rate, int gainDb) {
      z1[0] = z1[1] = z2[0] = z2[1] = 0.0f;
      if (!gainDb || rate <= 0) {
        b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f;
        return;
      }
      const float nyquistSafe = rate * 0.45f;
      if (frequency > nyquistSafe) frequency = nyquistSafe;
      const float omega = 6.28318530718f * frequency / rate;
      const float sine = sinf(omega), cosine = cosf(omega);
      const float A = powf(10.0f, gainDb / 40.0f);
      const float alpha = type == Peak ? sine / 1.6f : sine * 0.70710678118f;
      const float beta = 2.0f * sqrtf(A) * alpha;
      float bb0, bb1, bb2, aa0, aa1, aa2;
      if (type == Peak) {
        bb0 = 1.0f + alpha * A;
        bb1 = -2.0f * cosine;
        bb2 = 1.0f - alpha * A;
        aa0 = 1.0f + alpha / A;
        aa1 = bb1;
        aa2 = 1.0f - alpha / A;
      } else if (type == LowShelf) {
        bb0 = A * ((A + 1.0f) - (A - 1.0f) * cosine + beta);
        bb1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosine);
        bb2 = A * ((A + 1.0f) - (A - 1.0f) * cosine - beta);
        aa0 = (A + 1.0f) + (A - 1.0f) * cosine + beta;
        aa1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosine);
        aa2 = (A + 1.0f) + (A - 1.0f) * cosine - beta;
      } else {
        bb0 = A * ((A + 1.0f) + (A - 1.0f) * cosine + beta);
        bb1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosine);
        bb2 = A * ((A + 1.0f) + (A - 1.0f) * cosine - beta);
        aa0 = (A + 1.0f) - (A - 1.0f) * cosine + beta;
        aa1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosine);
        aa2 = (A + 1.0f) - (A - 1.0f) * cosine - beta;
      }
      b0 = bb0 / aa0; b1 = bb1 / aa0; b2 = bb2 / aa0;
      a1 = aa1 / aa0; a2 = aa2 / aa0;
    }

    float process(float input, int channel) {
      const float output = b0 * input + z1[channel];
      z1[channel] = b1 * input - a1 * output + z2[channel];
      z2[channel] = b2 * input - a2 * output;
      return output;
    }
  };

  bool enabled_ = false;
  float headroom_ = 1.0f;
  Biquad low_, mid_, high_;
};
