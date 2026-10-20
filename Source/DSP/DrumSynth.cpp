/*
  SAWT - DrumSynth.cpp - the vocal-only drum machine
*/
#include "DrumSynth.h"

namespace sawt
{
namespace
{
    // The wavetable holds TWO periods of the singer's waveform, so reading the
    // full 1024-table in T seconds yields frequency 2/T.
    // -> phase increment per sample = (tableSize / 2) * f / sr
    inline double tablePhaseInc (double f, double sr, double tableSize)
    {
        return (tableSize * 0.5) * f / sr;
    }

    inline float readTable (const juce::AudioBuffer<float>& t, double phase)
    {
        const int n = t.getNumSamples();
        if (n < 2) return 0.0f;
        double p = phase - std::floor (phase / (double) n) * (double) n;
        const int i0 = (int) p;
        const int i1 = (i0 + 1) % n;
        const float fr = (float) (p - (double) i0);
        const float* x = t.getReadPointer (0);
        return x[i0] + (x[i1] - x[i0]) * fr;
    }

    inline float readNoise (const juce::AudioBuffer<float>& t, double& pos)
    {
        const int n = t.getNumSamples();
        if (n < 4) return 0.0f;
        if (pos >= (double) n) pos -= (double) n;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % n;
        const float fr = (float) (pos - (double) i0);
        const float* x = t.getReadPointer (0);
        const float v = x[i0] + (x[i1] - x[i0]) * fr;
        pos += 1.0;
        return v;
    }
}

void DrumSynth::prepare (double sampleRate, int)
{
    sr = sampleRate;
    reset();
}

void DrumSynth::reset()
{
    for (auto& v : voices) v = Voice{};
    pendingTypes.clear();
}

DrumSynth::Voice* DrumSynth::freeVoice()
{
    for (auto& v : voices) if (! v.active) return &v;
    for (auto& v : voices)
        if (v.t > 0.5) { v.active = false; return &v; }   // steal long-running
    voices[0].active = false;
    return &voices[0];
}

void DrumSynth::trigger (int pad, float velocity)
{
    pendingTypes.push_back ({ pad, velocity });
}

void DrumSynth::startVoice (int type, float vel, const DrumParams& p)
{
    auto* v = freeVoice();
    if (v == nullptr) return;
    const int pad = juce::jlimit (0, 7, type);
    const float tuneMul = std::pow (2.0f, p.pad[pad].tune / 12.0f);
    const float padDecay = juce::jlimit (0.05f, 2.0f, p.pad[pad].decay) / 0.5f;

    *v = Voice();
    v->active = true;
    v->type = pad;
    v->vel = juce::jlimit (0.0f, 1.5f, vel);
    v->t = 0.0;
    v->phase = 0.0;
    v->noisePos = rng.nextDouble() * 1000.0;
    v->seed = voiceCounter++;

    const float tone = juce::jlimit (0.0f, 1.0f, p.tone);
    const bool haveTable = material.hasCycle && material.wavetable.getNumSamples() > 64;
    const bool haveNoise = material.hasBreath && material.breath.getNumSamples() > 64;

    switch (pad)
    {
        case Kick:
            v->baseHz = (42.0f + 18.0f * tone) * tuneMul;
            v->decay = (0.35f + 0.30f * (1.0f - tone)) * padDecay;
            if (! haveTable && haveNoise) v->lp.setLowPass (sr, 90.0 * tuneMul, 0.7);
            break;
        case Snare:
            v->baseHz = (175.0f + 90.0f * tone) * tuneMul;
            v->decay = (0.16f + 0.10f * tone) * padDecay;
            v->bp1.setBandPass (sr, 1400.0 + 2400.0 * tone, 0.9);
            v->hp.setHighPass (sr, 400.0, 0.7);
            break;
        case HatC: case HatO:
            v->decay = ((pad == HatC ? 0.055f : 0.35f)) * padDecay;
            v->bp1.setBandPass (sr, 7000.0 + 3000.0 * tone, 0.6);
            v->bp2.setBandPass (sr, 10500.0, 0.8);
            v->hp.setHighPass (sr, 5500.0, 0.7);
            break;
        case Perc1: case Perc2:
            v->baseHz = (280.0f + 420.0f * tone + (pad == Perc2 ? 140.0f : 0.0f)) * tuneMul;
            v->decay = (0.10f + 0.16f * (1.0f - tone)) * padDecay;
            if (! haveTable && haveNoise) v->bp1.setBandPass (sr, 600.0 * tuneMul, 1.0);
            break;
        case Clap:
            v->decay = 0.16f * padDecay;
            v->bp1.setBandPass (sr, 1000.0 + 500.0 * tone, 1.1);
            v->bp2.setBandPass (sr, 1600.0, 1.0);
            break;
        case Rim:
            v->baseHz = 380.0f * tuneMul;
            v->decay = 0.05f * padDecay;
            v->bp1.setBandPass (sr, 2200.0, 1.5);
            break;
        default: break;
    }

    juce::ignoreUnused (haveTable, haveNoise);
}

void DrumSynth::render (juce::AudioBuffer<float>& out, const DrumParams& p)
{
    const int n = out.getNumSamples();
    auto* outL = out.getWritePointer (0);
    auto* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const double dt = 1.0 / sr;

    const bool haveTable = material.hasCycle && material.wavetable.getNumSamples() > 64;
    const bool haveNoise = material.hasBreath && material.breath.getNumSamples() > 64;
    const double tableN = (double) juce::jmax (64, material.wavetable.getNumSamples());

    // start queued triggers
    for (const auto& q : pendingTypes)
        startVoice (q.first, q.second, p);
    pendingTypes.clear();

    for (auto& v : voices)
    {
        if (! v.active) continue;
        const float drive = 1.0f + p.drive * 6.0f;
        const float punch = 1.0f + p.punch * 1.5f;

        for (int i = 0; i < n; ++i)
        {
            const float t = (float) v.t;
            float s = 0.0f;

            switch (v.type)
            {
                case Kick:
                {
                    // pitch drop from baseHz*5.5 down to baseHz, body = singer's cycle
                    const float fEnd = v.baseHz;
                    const float fStart = fEnd * 5.5f;
                    const float f = fEnd + (fStart - fEnd) * std::exp (-t / 0.035f);
                    v.phase += tablePhaseInc ((double) f, sr, tableN);
                    const float body = haveTable
                        ? readTable (material.wavetable, v.phase)
                        : (haveNoise ? v.lp.process (readNoise (material.breath, v.noisePos)) : 0.0f);
                    const float amp = std::exp (-t / (v.decay * 0.5f));
                    s = std::tanh (body * drive) * amp;
                    break;
                }
                case Snare:
                {
                    v.phase += tablePhaseInc ((double) v.baseHz, sr, tableN);
                    const float body = haveTable ? readTable (material.wavetable, v.phase) : 0.0f;
                    float noise = haveNoise ? readNoise (material.breath, v.noisePos) : 0.0f;
                    noise = v.bp1.process (noise) + 0.4f * v.hp.process (noise);
                    const float bodyAmp = std::exp (-t / 0.06f);
                    const float noiseAmp = std::exp (-t / (v.decay * 0.7f));
                    s = (body * bodyAmp + noise * noiseAmp * 1.4f) * 0.8f;
                    s = std::tanh (s * drive);
                    break;
                }
                case HatC: case HatO:
                {
                    float noise = haveNoise ? readNoise (material.breath, v.noisePos) : 0.0f;
                    noise = v.bp1.process (noise) * 0.7f + v.bp2.process (noise) * 0.5f;
                    noise = v.hp.process (noise);
                    const float amp = std::exp (-t / (v.decay * 0.6f));
                    s = noise * amp * 1.3f;
                    break;
                }
                case Perc1: case Perc2:
                {
                    const float f = juce::jmax (60.0f, v.baseHz * std::exp (-t / 0.08f));
                    v.phase += tablePhaseInc ((double) f, sr, tableN);
                    const float body = haveTable
                        ? readTable (material.wavetable, v.phase)
                        : (haveNoise ? v.bp1.process (readNoise (material.breath, v.noisePos)) : 0.0f);
                    const float amp = std::exp (-t / (v.decay * 0.6f));
                    s = std::tanh (body * drive * 1.2f) * amp;
                    break;
                }
                case Clap:
                {
                    float noise = haveNoise ? readNoise (material.breath, v.noisePos) : 0.0f;
                    noise = v.bp1.process (noise) * 0.8f + v.bp2.process (noise) * 0.5f;
                    float burst = 0.0f;
                    const float offs[3] = { 0.0f, 0.012f, 0.024f };
                    for (int b = 0; b < 3; ++b)
                    {
                        const float tb = t - offs[(size_t) b];
                        if (tb > 0.0f) burst += std::exp (-tb / 0.008f);
                    }
                    const float tail = t > 0.024f ? std::exp (- (t - 0.024f) / 0.10f) : 0.0f;
                    s = noise * (0.8f * burst + 0.6f * tail);
                    break;
                }
                case Rim:
                {
                    float noise = haveNoise ? readNoise (material.breath, v.noisePos) : 0.0f;
                    noise = v.bp1.process (noise);
                    v.phase += tablePhaseInc ((double) v.baseHz, sr, tableN);
                    const float body = haveTable ? readTable (material.wavetable, v.phase) : 0.0f;
                    const float amp = std::exp (-t / 0.02f);
                    s = (noise * 1.2f + body * 0.8f) * amp;
                    break;
                }
                default: break;
            }

            // punch: extra transient gain in the first 4 ms (gain only - still the voice)
            const float transient = t < 0.004f ? punch : 1.0f;
            s *= transient;

            outL[i] += s * v.vel;
            if (outR != nullptr) outR[i] += s * v.vel;
            v.t += dt;
            if (v.t > v.decay * 6.0 + 0.4) { v.active = false; break; }
        }
    }
}

} // namespace sawt
