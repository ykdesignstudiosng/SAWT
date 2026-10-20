/*
  SAWT - PitchDetector.h
  Lightweight real-time F0 detector for monophonic vocals.
  Operates on a 4x-decimated ring buffer with normalised autocorrelation
  and parabolic refinement. Cheap enough for several voices per block.
*/
#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <cmath>

namespace sawt
{

class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ds = sampleRate / decim;
        const int winSamples = (int) (0.045 * ds);          // ~45 ms analysis window
        window = juce::jlimit (256, 1024, winSamples);
        buf.assign ((size_t) window * 2, 0.0f);
        wPos = 0; filled = 0;
        minLag = (int) (ds / 600.0);                        // 600 Hz ceiling
        maxLag = (int) (ds / 75.0);                         // 75 Hz floor
        lastPeriod = (float) (ds / 180.0) * decim;          // ~180 Hz default
    }

    // Push `n` input samples; returns true when a fresh estimate is ready.
    // `periodOut` is in FULL-rate samples.
    bool push (const float* x, int n, float& periodOut, float& clarityOut)
    {
        for (int i = 0; i < n; ++i)
        {
            // simple 4-sample box decimator
            decAcc += x[i];
            if (++decCount >= decim)
            {
                decCount = 0;
                buf[(size_t) wPos] = decAcc / (float) decim;
                decAcc = 0.0f;
                wPos = (wPos + 1) % (int) buf.size();
                ++filled;
                if (filled >= window && sinceDetect >= window / 2)
                {
                    sinceDetect = 0;
                    if (detect (periodOut, clarityOut)) { lastPeriod = periodOut; return true; }
                    periodOut = lastPeriod;
                    clarityOut = 0.0f;
                    return false;
                }
                ++sinceDetect;
            }
        }
        return false;
    }

    float lastPeriodSamples() const { return lastPeriod; }

private:
    bool detect (float& period, float& clarity)
    {
        const int N = (int) buf.size();
        // linearised read of the most recent `window` decimated samples
        std::vector<float> w ((size_t) window);
        for (int i = 0; i < window; ++i)
            w[(size_t) i] = buf[(size_t) ((wPos - window + i + 2 * N) % N)];

        double e0 = 0.0;
        for (int i = 0; i < window; ++i) e0 += (double) w[(size_t) i] * w[(size_t) i];
        if (e0 < 1e-6) { clarity = 0.0f; return false; }

        double best = 0.0; int bestLag = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double num = 0.0, e1 = 0.0, e2 = 0.0;
            for (int i = 0; i + lag < window; ++i)
            {
                const double a = w[(size_t) i], b = w[(size_t) i + lag];
                num += a * b; e1 += a * a; e2 += b * b;
            }
            const double r = num / (std::sqrt (e1 * e2) + 1e-12);
            if (r > best) { best = r; bestLag = lag; }
        }
        if (bestLag == 0 || best < 0.5) { clarity = (float) best; return false; }

        // parabolic refinement around the peak
        auto acfAt = [&] (int lag) -> double
        {
            double num = 0.0, e1 = 0.0, e2 = 0.0;
            for (int i = 0; i + lag < window; ++i)
            {
                const double a = w[(size_t) i], b = w[(size_t) i + lag];
                num += a * b; e1 += a * a; e2 += b * b;
            }
            return num / (std::sqrt (e1 * e2) + 1e-12);
        };
        double rM = acfAt (bestLag - 1), r0 = acfAt (bestLag), rP = acfAt (bestLag + 1);
        const double denom = rM - 2.0 * r0 + rP;
        double frac = 0.0;
        if (std::abs (denom) > 1e-9) frac = juce::jlimit (-0.5, 0.5, 0.5 * (rM - rP) / denom);

        period = (float) ((double) (bestLag + frac) * decim);
        clarity = (float) r0;
        return true;
    }

    static constexpr int decim = 4;
    double sr = 48000.0, ds = 12000.0;
    int window = 512, minLag = 20, maxLag = 160;
    std::vector<float> buf;
    int wPos = 0, filled = 0, sinceDetect = 0;
    float decAcc = 0.0f; int decCount = 0;
    float lastPeriod = 256.0f;
};

} // namespace sawt
