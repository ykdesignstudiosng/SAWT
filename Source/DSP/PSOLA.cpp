/*
  SAWT - PSOLA.cpp
*/
#include "PSOLA.h"

namespace sawt
{
namespace
{
    inline float hann (float phase)  // phase 0..1
    {
        return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase);
    }
}

void PSOLA::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = maxBlockSize;
    inRing.assign ((size_t) ringMask + 1, 0.0f);
    ola.assign ((size_t) ringMask + 1, 0.0f);
    olaW.assign ((size_t) ringMask + 1, 0.0f);
    detector.prepare (sampleRate);
    reset();
    // 3 x maximum period (75 Hz floor) guarantees grains are fully buffered
    latencySamples = (int) (3.0 * sampleRate / 75.0);
    latencySamples = ((latencySamples + maxBlock - 1) / maxBlock) * maxBlock;
}

void PSOLA::reset()
{
    std::fill (inRing.begin(), inRing.end(), 0.0f);
    std::fill (ola.begin(), ola.end(), 0.0f);
    std::fill (olaW.begin(), olaW.end(), 0.0f);
    writeAbs = 0;
    outAbs = 0;
    nextSyn = (double) latencySamples;
    nextRead = 0.0;
    period = periodTarget = (float) (sr / 180.0);
    ratio = ratioTarget;
}

void PSOLA::process (const float* in, int n, float* out)
{
    // 1) push input into the ring
    for (int i = 0; i < n; ++i)
    {
        inRing[(size_t) (writeAbs & ringMask)] = in[i];
        ++writeAbs;
    }

    // 2) pitch detection on the newly written audio
    float p = 0.0f, c = 0.0f;
    if (detector.push (in, n, p, c))
    {
        if (c > 0.5f) periodTarget = juce::jlimit (sr / 600.0, sr / 70.0, (double) p);
    }

    // 3) smooth ratio & period (per block)
    ratio      += (ratioTarget - ratio) * 0.35f;
    period     += (periodTarget - period) * 0.35f;

    // 4) schedule grains for the output window [outAbs, outAbs + n)
    const double blockEnd = (double) (outAbs + n);
    int guard = 0;
    while (nextSyn < blockEnd && guard++ < 4096)
    {
        emitGrain (nextRead, nextSyn, period);
        const double hop = (double) period / (double) ratio;
        nextSyn  += hop;
        nextRead += hop;
    }

    // 5) read normalised OLA output for this block
    for (int i = 0; i < n; ++i)
    {
        const size_t idx = (size_t) ((outAbs + (long long) i) & ringMask);
        const float w = olaW[idx];
        out[i] = w > 0.15f ? ola[idx] / w : ola[idx] * juce::jmin (1.0f, w * 4.0f);
        ola[idx] = 0.0f;
        olaW[idx] = 0.0f;
    }
    outAbs += n;
}

void PSOLA::emitGrain (double readCenter, double writeCenter, float grainPeriod)
{
    const int G = (int) (grainPeriod * 2.0f);
    if (G < 8) return;

    // reject grains whose tail has not been captured yet
    const double grainEnd = readCenter + (double) G * 0.5;
    if (grainEnd > (double) writeAbs) return;

    const long long wBase = (long long) std::floor (writeCenter);
    const float frac = (float) (writeCenter - (double) wBase);
    const long long rBase = (long long) std::floor (readCenter);
    const float rFrac = (float) (readCenter - (double) rBase);

    for (int j = 0; j < G; ++j)
    {
        const float win = hann ((float) j / (float) (G - 1));
        // cubic-interpolated read from input ring
        const double rp = (double) (rBase - G / 2 + j) + (double) rFrac;
        const long long i0 = (long long) std::floor (rp);
        const float fr = (float) (rp - (double) i0);
        const float s0 = inRing[(size_t) (i0 & ringMask)];
        const float s1 = inRing[(size_t) ((i0 + 1) & ringMask)];
        const float s2 = inRing[(size_t) ((i0 + 2) & ringMask)];
        const float s3 = inRing[(size_t) ((i0 + 3) & ringMask)];
        const float a1 = 0.5f * (s1 - s0);
        const float a2 = s0 - 2.5f * s1 + 2.0f * s2 - 0.5f * s3;
        const float a3 = 0.5f * (s3 - s0) + 1.5f * (s1 - s2);
        const float s = s1 + fr * (a1 + fr * (a2 + fr * a3));

        const long long wo = wBase - G / 2 + j + (long long) std::floor (frac);
        const size_t oi = (size_t) (wo & ringMask);
        ola[oi]  += s * win;
        olaW[oi] += win;
    }
}

} // namespace sawt
