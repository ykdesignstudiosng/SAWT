/*
  SAWT - PSOLA.h
  Real-time time-domain pitch shifter (TD-PSOLA style).

  How it keeps the vocal human:
  -----------------------------
  Grains of exactly TWO pitch-periods of the ORIGINAL voice are overlap-added
  at a hop of (period / ratio). The output pitch becomes  ratio * f0, while the
  spectral envelope (the formants - what makes it sound like a human voice)
  stays untouched, because the grain content is never resampled.

  The reader and writer advance at the same average rate, so there is no time
  drift, and a window-energy ring normalises the OLA sum for any hop.
*/
#pragma once

#include "PitchDetector.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>

namespace sawt
{

class PSOLA
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setRatio (float r) noexcept        { ratioTarget = juce::jlimit (0.25f, 4.0f, r); }
    float getRatio() const noexcept         { return ratioTarget; }
    int   latency() const noexcept          { return latencySamples; }
    float currentF0() const noexcept        { return period > 1.0f ? (float) (sr / (double) period) : 0.0f; }

    // in-place block conversion: nIn == nOut
    void process (const float* in, int n, float* out);

private:
    void runDetectionIfNeeded();
    void emitGrain (double readCenter, double writeCenter, float grainPeriod);

    double sr = 48000.0;
    int    maxBlock = 512;
    int    latencySamples = 0;

    // input ring
    std::vector<float> inRing;
    static constexpr int ringMask = (1 << 14) - 1;          // 16384 samples
    long long writeAbs = 0;                                  // absolute input samples written

    // pitch state
    PitchDetector detector;
    float period = 256.0f;        // full-rate samples
    float periodTarget = 256.0f;
    float clarity = 0.0f;

    // scheduler
    double nextSyn = 0.0;         // absolute output time of next grain center
    double nextRead = 0.0;        // absolute input time of next grain center
    long long outAbs = 0;         // absolute output samples produced

    // OLA accumulation (output side ring)
    std::vector<float> ola, olaW;

    float ratio = 1.0f, ratioTarget = 1.0f;
    int sinceSmooth = 0;
};

} // namespace sawt
