/*
  SAWT - DrumSynth.h
  Vocal-shaped drum synthesis: 808-style kicks, snares, hats, claps, rims and
  percussive hits built EXCLUSIVELY from the uploaded voice.

    * tonal body  = one pitch-period of the singer's waveform (wavetable)
    * noise layer = an unvoiced/breath segment of the same voice

  If the voice contains no usable cycle (pure whisper), the tonal parts
  degrade to low-passed breath noise - still 100% human-derived.
*/
#pragma once

#include "../Params.h"
#include "../Core/Analyzers.h"
#include <array>
#include <vector>
#include <cmath>

namespace sawt
{

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void reset() { x1 = x2 = y1 = y2 = 0; }

    void setBandPass (double sr, double f0, double Q)
    {
        const double w0 = 2.0 * 3.14159265358979 * f0 / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * Q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (alpha / a0); b1 = 0.0f; b2 = (float) (-alpha / a0);
        a1 = (float) (-2.0 * cw / a0); a2 = (float) ((1.0 - alpha) / a0);
    }

    void setHighPass (double sr, double f0, double Q)
    {
        const double w0 = 2.0 * 3.14159265358979 * f0 / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * Q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (((1.0 + cw) / 2.0) / a0);
        b1 = (float) (-(1.0 + cw) / a0);
        b2 = (float) (((1.0 + cw) / 2.0) / a0);
        a1 = (float) (-2.0 * cw / a0); a2 = (float) ((1.0 - alpha) / a0);
    }

    void setLowPass (double sr, double f0, double Q)
    {
        const double w0 = 2.0 * 3.14159265358979 * f0 / sr;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * Q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (((1.0 - cw) / 2.0) / a0);
        b1 = (float) ((1.0 - cw) / a0);
        b2 = (float) (((1.0 - cw) / 2.0) / a0);
        a1 = (float) (-2.0 * cw / a0); a2 = (float) ((1.0 - alpha) / a0);
    }

    inline float process (float x)
    {
        const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

class DrumSynth
{
public:
    enum Type : int { Kick = 0, Snare, HatC, HatO, Perc1, Perc2, Clap, Rim };

    void prepare (double sampleRate, int maxBlockSize);
    void setMaterial (const VoiceMaterial& m) noexcept { material = m; }

    void trigger (int pad, float velocity);
    void render (juce::AudioBuffer<float>& out, const DrumParams& p);
    void reset();

private:
    static constexpr int maxVoices = 12;
    std::vector<std::pair<int, float>> pendingTypes;   // queued (pad, velocity)

    struct Voice
    {
        bool active = false;
        int  type = 0;
        float vel = 1.0f;
        double t = 0.0;             // seconds since trigger
        double phase = 0.0;
        double noisePos = 0.0;
        Biquad bp1, bp2, hp, lp;
        float decay = 0.3f;
        float baseHz = 200.0f;
        long long seed = 0;
    };

    void startVoice (int type, float vel, const DrumParams& p);
    Voice* freeVoice();

    std::array<Voice, maxVoices> voices;
    VoiceMaterial material;
    double sr = 48000.0;
    long long voiceCounter = 0;
    juce::Random rng;
};

} // namespace sawt
