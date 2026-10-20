/*
  SAWT - FormantShifter.h
  Shifts formants without changing pitch:
      resampler(speed = f)  ->  PSOLA(ratio = 1/f)

  Stage 1 resampling moves BOTH pitch and formants by f.
  Stage 2 (TD-PSOLA) brings the pitch back, leaving formants shifted by f.
*/
#pragma once

#include "PSOLA.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>

namespace sawt
{

class FormantShifter
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setShiftSemitones (float st) noexcept { shiftTarget = juce::jlimit (-12.0f, 12.0f, st); }
    int   latency() const noexcept { return resamplerLatency + ps.latency(); }

    void process (const float* in, int n, float* out);

private:
    double sr = 48000.0;
    int maxBlock = 512;
    int resamplerLatency = 0;

    std::vector<float> ring;
    static constexpr int ringMask = (1 << 14) - 1;
    long long writeAbs = 0;
    double readAbs = 0.0;

    float shift = 0.0f, shiftTarget = 0.0f;
    PSOLA ps;
    std::vector<float> tmp;
};

} // namespace sawt
