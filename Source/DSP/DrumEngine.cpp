/*
  SAWT - DrumEngine.cpp
*/
#include "DrumEngine.h"

namespace sawt
{
namespace
{
    inline float noteToRatio (float note) { return std::pow (2.0f, note / 12.0f); }
}

void DrumEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    drumBus.setSize (2, maxBlockSize);
    synth.prepare (sampleRate, maxBlockSize);
    for (auto& f : padFlash) f.store (0.0f, std::memory_order_relaxed);
    reset();
}

void DrumEngine::reset()
{
    for (auto& v : padVoices) v = PadVoice{};
    synth.reset();
}

void DrumEngine::setSlices (const SampleData& sample, const std::vector<DrumSlice>& cuts)
{
    drumSampleLen = juce::jmax (1, sample.numSamples());
    for (int i = 0; i < 8; ++i)
    {
        slices[(size_t) i].setSize (1, 0);
        cutRanges[(size_t) i] = { 0, 0 };
        sliceCount.store (0, std::memory_order_relaxed);
    }
    int used = 0;
    for (const auto& c : cuts)
    {
        if (used >= 8) break;
        const int len = juce::jlimit (16, sample.numSamples() - c.start, c.length);
        if (len < 16) continue;
        slices[(size_t) used].setSize (1, len, false, false, true);
        slices[(size_t) used].copyFrom (0, 0, sample.buffer, 0, c.start, len);
        cutRanges[(size_t) used] = { c.start, c.start + len };
        // gentle edge fade to avoid clicks
        auto* d = slices[(size_t) used].getWritePointer (0);
        const int fade = juce::jmin (64, len / 4);
        for (int i = 0; i < fade; ++i)
        {
            const float w = (float) i / (float) fade;
            d[i] *= w;
            d[len - 1 - i] *= w;
        }
        ++used;
    }
    sliceCount.store (used, std::memory_order_relaxed);
}

std::vector<std::pair<float, float>> DrumEngine::sliceNorms() const
{
    std::vector<std::pair<float, float>> out;
    const int sc = sliceCount.load (std::memory_order_relaxed);
    const double len = (double) juce::jmax (1, drumSampleLen);
    for (int i = 0; i < sc; ++i)
        out.emplace_back ((double) cutRanges[(size_t) i].first / len,
                          (double) cutRanges[(size_t) i].second / len);
    return out;
}

void DrumEngine::triggerPad (int pad, float velocity) noexcept
{
    if (pad < 0 || pad > 7) return;
    pendingPads.push_back ({ pad, velocity });
    lastTriggered.store (pad, std::memory_order_relaxed);
    padFlash[(size_t) pad].store (1.0f, std::memory_order_relaxed);
}

void DrumEngine::chokeHats (int pad)
{
    // closed (2) and open (3) hats are mutually exclusive
    if (pad == 2 || pad == 3)
        for (auto& v : padVoices)
            if (v.active && (v.pad == 2 || v.pad == 3) && v.pad != pad)
                v.active = false;
}

void DrumEngine::startSlice (int pad, float vel, const DrumParams& p)
{
    chokeHats (pad);

    // prefer a synth voice when this pad has no slice
    if (sliceCount.load (std::memory_order_relaxed) <= pad ||
        slices[(size_t) pad].getNumSamples() < 32)
    {
        synth.trigger (pad, vel);
        return;
    }

    PadVoice* free = nullptr;
    for (auto& v : padVoices) if (! v.active) { free = &v; break; }
    if (free == nullptr)
        for (auto& v : padVoices)
            if (free == nullptr || v.t > free->t) free = &v;

    *free = PadVoice();
    free->active = true;
    free->pad = pad;
    free->vel = juce::jlimit (0.0f, 1.5f, vel);
    free->pos = 0.0;
    free->t = 0.0;
    free->decay = juce::jlimit (0.05f, 2.0f, p.pad[pad].decay);
}

void DrumEngine::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi,
                         const DrumParams& p)
{
    const int n = out.getNumSamples();
    auto* outL = out.getWritePointer (0);
    auto* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const double dt = 1.0 / sr;

    // collect pad triggers: MIDI ch10 notes 36..43 + UI clicks
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOn (true)) continue;
        if (m.getChannel() != drumMidiChannel) continue;
        const int pad = m.getNoteNumber() - drumMidiBase;
        if (pad >= 0 && pad <= 7)
            pendingPads.push_back ({ pad, m.getVelocity() / 127.0f });
    }

    // dispatch queued pads to the right source
    const bool useSynth = p.source == DrumSource::VocalSynth;
    for (const auto& q : pendingPads)
    {
        if (useSynth)
        {
            chokeHats (q.first);
            synth.trigger (q.first, q.second);
        }
        else
        {
            startSlice (q.first, q.second, p);
        }
    }
    pendingPads.clear();

    // slice voices -> drum bus
    drumBus.clear();
    for (auto& v : padVoices)
    {
        if (! v.active) continue;
        const auto& buf = slices[(size_t) v.pad];
        const int len = buf.getNumSamples();
        if (len < 16) { v.active = false; continue; }

        const float speed = noteToRatio (p.pad[v.pad].tune);
        const float decayK = 1.0f / juce::jmax (0.05f, v.decay);
        auto* s = buf.getReadPointer (0);

        for (int i = 0; i < n; ++i)
        {
            const int i0 = (int) v.pos;
            if (i0 >= len - 1) { v.active = false; break; }
            const float fr = (float) (v.pos - (double) i0);
            const float x = s[i0] + (s[i0 + 1] - s[i0]) * fr;
            const float env = std::exp (-v.t * decayK);
            const float outv = x * env * v.vel;
            outL[i] += outv;
            if (outR != nullptr) outR[i] += outv;
            v.pos += (double) speed;
            v.t += dt;
        }
    }

    // synth voices (also handles the no-slice fallback voices queued above)
    synth.render (out, p);

    // apply global drum level
    const float lvl = p.level;
    for (int i = 0; i < n; ++i)
    {
        outL[i] *= lvl;
        if (outR != nullptr) outR[i] *= lvl;
    }

    // pad flash decay for UI
    for (int i = 0; i < 8; ++i)
    {
        const float f = padFlash[(size_t) i].load (std::memory_order_relaxed);
        if (f > 0.001f) padFlash[(size_t) i].store (f * 0.85f, std::memory_order_relaxed);
    }
}

} // namespace sawt
