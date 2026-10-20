/*
  SAWT - SamplerEngine.cpp
*/
#include "SamplerEngine.h"

namespace sawt
{
namespace
{
    inline float noteToRatio (float note) { return std::pow (2.0f, note / 12.0f); }

    // 2x linear-interpolated stereo read (mono sample duplicates to both)
    inline void readStereo (const SampleData& s, double pos, float& l, float& r)
    {
        const int n = s.numSamples();
        if (n < 2) { l = r = 0.0f; return; }
        const int i0 = (int) pos;
        const float fr = (float) (pos - (double) i0);
        const int i1 = (i0 + 1) % n;
        const int ia = i0 % n;
        if (s.numChannels() >= 2)
        {
            const float* L = s.buffer.getReadPointer (0);
            const float* R = s.buffer.getReadPointer (1);
            l = L[ia] + (L[i1] - L[ia]) * fr;
            r = R[ia] + (R[i1] - R[ia]) * fr;
        }
        else
        {
            const float* M = s.buffer.getReadPointer (0);
            l = r = M[ia] + (M[i1] - M[ia]) * fr;
        }
    }
}

void SamplerEngine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    reset();
}

void SamplerEngine::reset()
{
    for (auto& v : voices) v = Voice{};
}

int SamplerEngine::activeVoices() const noexcept
{
    int c = 0;
    for (const auto& v : voices) if (v.active) ++c;
    return c;
}

SamplerEngine::Voice* SamplerEngine::oldestVoice()
{
    // prefer a free voice; otherwise steal the earliest-allocated one
    Voice* best = nullptr;
    for (auto& v : voices)
        if (! v.active) { best = &v; break; }
    if (best == nullptr)
    {
        for (auto& v : voices)
            if (best == nullptr || v.seed < best->seed) best = &v;
    }
    return best;
}

void SamplerEngine::noteOn (int note, float vel, const MelodicParams& p)
{
    const int u = juce::jlimit (1, 5, p.unison);
    for (int ui = 0; ui < u; ++ui)
    {
        auto* v = oldestVoice();
        if (v == nullptr) return;
        *v = Voice();
        v->active = true;
        v->note = note;
        v->vel = vel;
        v->envStage = 1;
        v->env = 0.0f;
        v->stageTime = 0.0;
        v->seed = voiceCounter++;

        // detune offsets spread symmetrically
        if (u == 1) v->detuneOffset = 0.0f;
        else
        {
            const float spreadSemis = p.detune / 100.0f;
            const float t = (float) ui / (float) (u - 1) * 2.0f - 1.0f; // -1..1
            v->detuneOffset = t * spreadSemis * 0.5f;
        }

        // pan spread across unison
        const float t = u == 1 ? 0.0f : ((float) ui / (float) (u - 1) * 2.0f - 1.0f);
        v->panL = std::cos ((t * p.spread * 0.7f + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        v->panR = std::sin ((t * p.spread * 0.7f + 1.0f) * juce::MathConstants<float>::pi * 0.25f);

        // glide: start from the last played ratio
        const float target = noteToRatio ((float) note - 60.0f + p.tune + v->detuneOffset);
        v->speedTarget = target;
        v->speed = p.glide > 0.001f ? lastRatio : target;
    }
}

void SamplerEngine::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.note == note && v.envStage != 4)
        { v.envStage = 4; v.stageTime = 0.0; }
}

void SamplerEngine::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi,
                            const MelodicParams& p, float bendSemi)
{
    if (sample == nullptr || ! sample->isValid()) return;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOn (true) && ! m.isNoteOff (true)) continue;
        if (m.getChannel() == drumMidiChannel) continue;   // ch10 is the drum kit
        if (m.isNoteOn (true)) noteOn (m.getNoteNumber(), m.getVelocity() / 127.0f, p);
        else noteOff (m.getNoteNumber());
    }

    const int n = out.getNumSamples();
    auto* outL = out.getWritePointer (0);
    auto* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const double dt = 1.0 / sr;
    const float bendRatio = noteToRatio (bendSemi);
    const float glideCoef = p.glide > 0.001f ? 1.0f - std::exp (-dt / (0.12f * p.glide)) : 1.0f;
    float lastPosNorm = 0.0f;

    for (auto& v : voices)
    {
        if (! v.active) continue;

        // ---- envelope ----
        switch (v.envStage)
        {
            case 1: v.env += dt / juce::jmax (0.001f, p.att);
                    if (v.env >= 1.0f) { v.env = 1.0f; v.envStage = 2; v.stageTime = 0.0; } break;
            case 2: v.env = 1.0f - (1.0f - p.sus) * (float) (v.stageTime / juce::jmax (0.01f, p.dec));
                    if (v.stageTime >= p.dec) { v.env = p.sus; v.envStage = 3; } break;
            case 3: v.env = p.sus; break;
            case 4: v.env -= dt / juce::jmax (0.01f, p.rel) * juce::jmax (v.env, 0.2f);
                    if (v.env <= 0.001f) { v.active = false; v.env = 0.0f; } break;
            default: break;
        }
        v.stageTime += dt;
        if (! v.active) continue;

        // ---- pitch & glide ----
        const float target = v.speedTarget * bendRatio;
        v.speed += (target - v.speed) * glideCoef;
        lastRatio = v.speed;

        // ---- filter coefficients (Cytomic SVF, env-modulated) ----
        const float fc = juce::jlimit (30.0f, (float) sr * 0.45f,
                                       p.cutoff * std::pow (2.0f, p.envAmt * 4.0f * v.env * v.vel));
        v.gTarget = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
        v.g += (v.gTarget - v.g) * 0.25f;
        v.k = juce::jlimit (0.2f, 3.5f, 2.0f - p.reso * 0.9f);

        // ---- render ----
        const int len = sample->numSamples();
        const float a1 = 1.0f / (1.0f + v.g * (v.g + v.k));
        const float a2 = v.g * a1;
        const float b1 = v.g, b2 = v.g * v.g;
        float vl = 0.0f, vr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            readStereo (*sample, v.pos, vl, vr);
            v.pos += (double) v.speed;
            if (v.pos >= (double) len) v.pos -= (double) len;
            if (v.pos < 0.0) v.pos += (double) len;

            const float v3L = vl - v.ic2L;
            const float s1L = a1 * v.ic1L + a2 * v3L;
            const float s2L = v.ic2L + b1 * v.ic1L + b2 * v3L;
            v.ic1L = 2.0f * s1L - v.ic1L;
            v.ic2L = 2.0f * s2L - v.ic2L;

            const float v3R = vr - v.ic2R;
            const float s1R = a1 * v.ic1R + a2 * v3R;
            const float s2R = v.ic2R + b1 * v.ic1R + b2 * v3R;
            v.ic1R = 2.0f * s1R - v.ic1R;
            v.ic2R = 2.0f * s2R - v.ic2R;

            const float e = v.env * v.vel;
            outL[i] += s2L * e * v.panL;
            if (outR != nullptr) outR[i] += s2R * e * v.panR;
        }
        lastPosNorm = (float) (v.pos / (double) juce::jmax (1, len));
    }

    playPosNorm.store (lastPosNorm, std::memory_order_relaxed);
}

} // namespace sawt
