/*
  SAWT - Slicer.cpp - onset detection + rough percussion classification
*/
#include "Slicer.h"

#include <cmath>
#include <algorithm>

namespace sawt
{

namespace
{
    struct Onset { int start; float energy; float zcr; int bursts; };

    // one-pole high-pass (removes low hum so consonant attacks pop out)
    inline float hp (float x, float& y1, float& x1)
    {
        const float R = 0.995f;
        float y = R * (y1 + x - x1);
        x1 = x; y1 = y;
        return y;
    }

    float zeroCrossRateW (const float* x, int n)
    {
        if (n <= 1) return 0.0f;
        int z = 0;
        for (int i = 1; i < n; ++i)
            if ((x[i] >= 0.0f) != (x[i - 1] >= 0.0f)) ++z;
        return (float) z / (float) n;
    }

    PadKind classify (float durSec, float zcr, int bursts, float energy)
    {
        const bool shortHit = durSec < 0.14f;
        if (bursts >= 3 && durSec < 0.35f)          return PadKind::Clap;
        if (shortHit && zcr > 0.28f)                return (durSec < 0.09f ? PadKind::HatC : PadKind::HatO);
        if (shortHit && zcr > 0.15f)                return PadKind::Rim;
        if (zcr < 0.08f && durSec >= 0.10f && energy > 0.02f) return PadKind::Kick;
        if (durSec < 0.40f && zcr >= 0.08f && zcr <= 0.25f)   return PadKind::Snare;
        if (durSec >= 0.40f)                        return PadKind::Kick; // long low bloom
        return PadKind::Perc1;
    }
}

juce::String TransientSlicer::kindName (PadKind k)
{
    switch (k)
    {
        case PadKind::Kick:  return "KICK";
        case PadKind::Snare: return "SNARE";
        case PadKind::HatC:  return "HAT C";
        case PadKind::HatO:  return "HAT O";
        case PadKind::Perc1: return "PERC 1";
        case PadKind::Perc2: return "PERC 2";
        case PadKind::Clap:  return "CLAP";
        case PadKind::Rim:   return "RIM";
        default:             return "PERC";
    }
}

std::vector<DrumSlice> TransientSlicer::slice (const SampleData& sample, int maxSlices)
{
    std::vector<DrumSlice> out;
    const auto& mono = sample.mono.getNumSamples() > 0 ? sample.mono : sample.buffer;
    const int n = mono.getNumSamples();
    const int sr = (int) sample.sampleRate;
    if (n < 2048) return out;
    const float* x = mono.getReadPointer (0);

    // envelope follower on the high-passed signal
    std::vector<float> env ((size_t) n, 0.0f);
    float y1 = 0.0f, x1 = 0.0f, e = 0.0f;
    const float aAtk = std::exp (-1.0f / (0.0008f * (float) sr));
    const float aRel = std::exp (-1.0f / (0.0600f * (float) sr));
    for (int i = 0; i < n; ++i)
    {
        const float v = std::fabs (hp (x[i], y1, x1));
        e = v > e ? aAtk * e + (1.0f - aAtk) * v : aRel * e + (1.0f - aRel) * v;
        env[(size_t) i] = e;
    }

    // adaptive threshold from a 1-second sliding mean
    const int win = juce::jmin (n, (int) (1.0 * sr));
    const int minGap = (int) (0.09 * sr);
    std::vector<Onset> onsets;
    int lastOnset = -minGap * 10;

    double runningSum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        runningSum += env[(size_t) i];
        if (i >= win) runningSum -= env[(size_t) i - win];
        const float mean = (float) (runningSum / (double) juce::jmin (i + 1, win));
        const float thr = mean * 4.0f + 0.004f;

        const float now = env[(size_t) i];
        const float prev = i > 0 ? env[(size_t) i - 1] : 0.0f;
        if (now > thr && prev <= thr && i - lastOnset >= minGap && i < n - (int) (0.05 * sr))
        {
            lastOnset = i;
            Onset o;
            o.start = juce::jmax (0, i - (int) (0.004 * sr));
            o.energy = now;
            o.bursts = 1;
            // count secondary bursts within 80 ms (clap signature)
            int extra = 0;
            const int burstWin = (int) (0.08 * sr);
            for (int j = i + (int) (0.012 * sr); j < juce::jmin (n, i + burstWin); ++j)
            {
                const float v = env[(size_t) j];
                const float pv = env[(size_t) j - 1];
                if (v > thr * 0.8f && pv <= thr * 0.8f) { ++extra; }
            }
            o.bursts += extra;
            onsets.push_back (o);
        }
    }
    if (onsets.empty()) return out;

    // measure each slice
    const int maxLen = (int) (0.6 * sr);
    for (auto& o : onsets)
    {
        int end = juce::jmin (n, o.start + maxLen);
        for (int i = o.start + (int) (0.02 * sr); i < end; ++i)
            if (env[(size_t) i] < o.energy * 0.18f) { end = i; break; }
        const int len = juce::jmax ((int) (0.05 * sr), end - o.start);
        const float zcrW = zeroCrossRateW (x + o.start, juce::jmin (len, n - o.start));
        DrumSlice s;
        s.start = o.start;
        s.length = juce::jmin (len, n - o.start);
        s.energy = o.energy;
        s.kind = classify (s.length / (float) sr, zcrW, o.bursts, o.energy);
        out.push_back (s);
    }

    // energy-rank, keep best N
    std::stable_sort (out.begin(), out.end(),
        [] (const DrumSlice& a, const DrumSlice& b) { return a.energy > b.energy; });
    if ((int) out.size() > maxSlices) out.resize ((size_t) maxSlices);
    return out;
}
} // namespace sawt
