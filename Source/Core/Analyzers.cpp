/*
  SAWT - Analyzers.cpp
*/
#include "Analyzers.h"

#include <cmath>
#include <vector>

namespace sawt
{

namespace
{
    // Normalised autocorrelation peak -> period in samples (or 0 if unvoiced)
    int detectPeriod (const float* x, int n, int sr, double& clarityOut)
    {
        const int minLag = sr / 600;   // 600 Hz
        const int maxLag = sr / 70;    // 70 Hz
        if (maxLag * 2 >= n) return 0;

        double energy = 0.0;
        for (int i = 0; i < n; ++i) energy += (double) x[i] * x[i];
        if (energy < 1e-7) return 0;

        double bestR = 0.0; int bestLag = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double num = 0.0, e1 = 0.0, e2 = 0.0;
            for (int i = 0; i + lag < n; ++i)
            {
                num += (double) x[i] * x[i + lag];
                e1  += (double) x[i] * x[i];
                e2  += (double) x[i + lag] * x[i + lag];
            }
            double r = num / (std::sqrt (e1 * e2) + 1e-12);
            if (r > bestR) { bestR = r; bestLag = lag; }
        }
        clarityOut = bestR;
        return bestR > 0.6 ? bestLag : 0;
    }

    float zeroCrossRate (const float* x, int n)
    {
        int z = 0;
        for (int i = 1; i < n; ++i)
            if ((x[i] >= 0.0f) != (x[i - 1] >= 0.0f)) ++z;
        return (float) z / (float) n;
    }

    float rms (const float* x, int n)
    {
        double s = 0.0;
        for (int i = 0; i < n; ++i) s += (double) x[i] * x[i];
        return (float) std::sqrt (s / (double) juce::jmax (1, n));
    }
}

VoiceMaterial VoiceAnalyzer::analyze (const SampleData& sample, float tone)
{
    VoiceMaterial mat;
    const auto& mono = sample.mono.getNumSamples() > 0 ? sample.mono : sample.buffer;
    const int n = mono.getNumSamples();
    const int sr = (int) sample.sampleRate;
    if (n < 4096 || mono.getNumChannels() < 1) return mat;
    const float* x = mono.getReadPointer (0);

    // ---- scan frames ----------------------------------------------------
    const int frame = 1024, hop = 256;
    struct FrameInfo { int start; float rms; float zcr; int period; double clarity; };
    std::vector<FrameInfo> frames;
    frames.reserve (n / hop);
    for (int s = 0; s + frame * 2 <= n; s += hop)
    {
        FrameInfo f;
        f.start = s;
        f.rms = rms (x + s, frame);
        f.zcr = zeroCrossRate (x + s, frame);
        double c = 0.0;
        f.period = detectPeriod (x + s, frame * 2, sr, c);
        f.clarity = c;
        frames.push_back (f);
    }
    if (frames.empty()) return mat;

    // ---- pick a voiced frame for the wavetable --------------------------
    // Score frames: periodicity mandatory; tone bias steers F0 (low zcr = dark)
    int best = -1; double bestScore = -1e9;
    for (size_t i = 0; i < frames.size(); ++i)
    {
        const auto& f = frames[i];
        if (f.period == 0) continue;
        if (f.rms < 0.02f) continue;
        double dark = juce::jlimit (0.0, 1.0, (double) f.zcr * 12.0); // 0 dark .. 1 bright
        double bias = juce::jlimit (0.0, 1.0, (double) tone);
        double score = f.clarity * (1.0 - std::abs (dark - bias) * 0.8) + std::log (1.0 + (double) f.rms * 4.0);
        if (score > bestScore) { bestScore = score; best = (int) i; }
    }

    if (best >= 0)
    {
        const auto& f = frames[(size_t) best];
        mat.wavetable.setSize (1, tableSize);
        float* t = mat.wavetable.getWritePointer (0);
        const int period = f.period;
        const int take = period * 2;                      // 2 periods -> smooth loop
        std::vector<float> raw ((size_t) juce::jmin (take, n - f.start));
        for (int i = 0; i < (int) raw.size(); ++i)
            raw[(size_t) i] = x[f.start + i];

        // resample the 2-period chunk into a fixed 1024 table
        const double step = (double) (raw.size() - 1) / (double) tableSize;
        for (int i = 0; i < tableSize; ++i)
        {
            const double p = (double) i * step;
            const int i0 = (int) p;
            const float fr = (float) (p - (double) i0);
            const float a = raw[(size_t) juce::jmin (i0, (int) raw.size() - 1)];
            const float b = raw[(size_t) juce::jmin (i0 + 1, (int) raw.size() - 1)];
            t[i] = a + (b - a) * fr;
        }
        // short crossfade loop seam (last 64 -> first 64)
        const int cf = 64;
        for (int i = 0; i < cf; ++i)
        {
            const float w = (float) i / (float) cf;
            t[tableSize - cf + i] = t[tableSize - cf + i] * (1.0f - w * 0.5f) + t[i] * (w * 0.5f);
        }
        mat.fundamentalHz = (float) sr / (float) period;
        mat.hasCycle = true;
    }

    // ---- pick a breath / unvoiced segment --------------------------------
    int bBest = -1; double bScore = -1e9;
    for (size_t i = 0; i < frames.size(); ++i)
    {
        const auto& f = frames[i];
        if (f.rms < 0.001f || f.rms > 0.3f) continue;
        double score = (double) f.zcr * 2.0 - (double) f.clarity * 3.0 + std::log (1.0 + (double) f.rms * 50.0);
        if (score > bScore) { bScore = score; bBest = (int) i; }
    }
    if (bBest >= 0)
    {
        int s = frames[(size_t) bBest].start;
        s = juce::jlimit (0, juce::jmax (0, n - breathSize), s);
        mat.breath.setSize (1, breathSize);
        float* d = mat.breath.getWritePointer (0);
        for (int i = 0; i < breathSize; ++i)
        {
            const float w = (float) std::sin (juce::MathConstants<double>::pi * (double) i / (double) breathSize);
            d[i] = x[s + i] * w;
        }
        mat.hasBreath = true;
    }

    return mat;
}

} // namespace sawt
