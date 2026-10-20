/*
  SAWT - GranularEngine.cpp
*/
#include "GranularEngine.h"

namespace sawt
{
namespace
{
    inline float noteToRatio (float note) { return std::pow (2.0f, note / 12.0f); }

    inline float readMono (const SampleData& s, double pos)
    {
        const int n = s.numSamples();
        if (n < 2) return 0.0f;
        pos = juce::jlimit (0.0, (double) n - 1.001, pos);
        const int i0 = (int) pos;
        const float fr = (float) (pos - (double) i0);
        const float* M = s.mono.getNumSamples() > 0 ? s.mono.getReadPointer (0)
                                                    : s.buffer.getReadPointer (0);
        return M[i0] + (M[i0 + 1] - M[i0]) * fr;
    }

    inline float hann (float t)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * t);
    }
}

void GranularEngine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    reset();
}

void GranularEngine::reset()
{
    for (auto& v : voices) v = Voice{};
    for (auto& g : grains) g = Grain{};
}

int GranularEngine::activeVoices() const noexcept
{
    int c = 0;
    for (const auto& v : voices) if (v.active) ++c;
    return c;
}

GranularEngine::Voice* GranularEngine::oldestVoice()
{
    Voice* best = nullptr;
    for (auto& v : voices)
        if (! v.active) { best = &v; break; }
    if (best == nullptr)
        for (auto& v : voices)
            if (best == nullptr || v.seed < best->seed) best = &v;
    return best;
}

void GranularEngine::noteOn (int note, float vel, const MelodicParams& p)
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
    v->spawnAcc = 0.0;
    if (sample != nullptr && sample->isValid())
        v->scanPos = juce::jlimit (0.0, (double) sample->numSamples() - 2.0,
                                   (double) p.gPos * (double) sample->numSamples());
    v->ratio = noteToRatio ((float) note - 60.0f + p.tune);
}

void GranularEngine::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.note == note && v.envStage != 4)
        { v.envStage = 4; v.stageTime = 0.0; }
}

void GranularEngine::spawnGrain (Voice& v, const MelodicParams& p, double sampleLen)
{
    Grain* g = nullptr;
    for (auto& cand : grains)
        if (! cand.active) { g = &cand; break; }
    if (g == nullptr)
    {
        for (auto& cand : grains)
            if (g == nullptr || cand.age > g->age) g = &cand;   // steal oldest
    }
    if (g == nullptr) return;

    *g = Grain();
    g->active = true;
    g->len = juce::jlimit (0.01f, 0.4f, p.gSize);
    g->age = 0.0;
    g->speed = v.ratio * noteToRatio (((rng.nextFloat() * 2.0f - 1.0f) * p.detune / 100.0f) * 0.5f);

    double pos = 0.0;
    switch (p.gMode)
    {
        case GrainMode::Scan:
            pos = v.scanPos + (rng.nextFloat() - 0.5f) * p.gJitter * 0.25 * sampleLen;
            break;
        case GrainMode::Freeze:
            pos = v.basePos + (rng.nextFloat() - 0.5f) * p.gJitter * 0.08 * sampleLen;
            break;
        case GrainMode::Random:
            pos = rng.nextDouble() * sampleLen;
            break;
    }
    g->readPos = juce::jlimit (0.0, juce::jmax (0.0, sampleLen - 2.0), pos);

    const float t = rng.nextFloat() * 2.0f - 1.0f;
    const float ang = (t * p.spread + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    g->panL = std::cos (ang);
    g->panR = std::sin (ang);
    g->amp = 0.5f + 0.5f * rng.nextFloat();
}

void GranularEngine::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi,
                             const MelodicParams& p, float bendSemi)
{
    if (sample == nullptr || ! sample->isValid()) return;
    const double sampleLen = (double) sample->numSamples();
    const int n = out.getNumSamples();
    auto* outL = out.getWritePointer (0);
    auto* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const double dt = 1.0 / sr;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOn (true) && ! m.isNoteOff (true)) continue;
        if (m.getChannel() == drumMidiChannel) continue;
        if (m.isNoteOn (true)) noteOn (m.getNoteNumber(), m.getVelocity() / 127.0f, p);
        else noteOff (m.getNoteNumber());
    }

    // ---- voice envelopes + scanning + grain spawning ----
    for (auto& v : voices)
    {
        if (! v.active) continue;
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

        v.ratio = noteToRatio ((float) v.note - 60.0f + p.tune) * noteToRatio (bendSemi);

        // scan position drifts through the sample
        if (p.gMode == GrainMode::Scan)
        {
            const double rate = (0.02 + 0.45 * p.gDrift) * sampleLen; // fraction per second
            v.scanPos += rate * dt;
            if (v.scanPos >= sampleLen - 2.0) v.scanPos -= sampleLen - 4.0;
        }
        else
        {
            v.basePos = (double) p.gPos * sampleLen;
        }

        // spawn grains
        v.spawnAcc += dt * (double) p.gRate;
        while (v.spawnAcc >= 1.0)
        {
            spawnGrain (v, p, sampleLen);
            v.spawnAcc -= 1.0;
        }
    }

    // ---- render grains (mono source, stereo out) ----
    const float bendRatio = noteToRatio (bendSemi);
    juce::ignoreUnused (bendRatio);
    for (auto& g : grains)
    {
        if (! g.active) continue;
        g.age += dt;
        if (g.age >= (double) g.len) { g.active = false; continue; }

        const float w = hann ((float) (g.age / (double) g.len));
        const float e = w * g.amp;
        for (int i = 0; i < n; ++i)
        {
            const float s = readMono (*sample, g.readPos) * e;
            outL[i] += s * g.panL;
            if (outR != nullptr) outR[i] += s * g.panR;
            g.readPos += (double) g.speed;
            if (g.readPos >= sampleLen - 2.0) g.readPos -= sampleLen - 4.0;
            if (g.readPos < 0.0) g.readPos += juce::jmax (1.0, sampleLen - 4.0);
        }
    }

    // gentle soft-clip for dense grain sums
    for (int i = 0; i < n; ++i)
    {
        outL[i] = std::tanh (outL[i] * 1.2f) * 0.9f;
        if (outR != nullptr) outR[i] = std::tanh (outR[i] * 1.2f) * 0.9f;
    }
}

} // namespace sawt
