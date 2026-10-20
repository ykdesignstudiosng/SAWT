/*
  SAWT - FormantEngine.cpp
*/
#include "FormantEngine.h"

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
}

void FormantEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    scratch.assign ((size_t) maxBlockSize, 0.0f);
    for (auto& v : voices)
    {
        v = Voice{};
        v.ps.prepare (sampleRate, maxBlockSize);
    }
    prepared = true;
    reset();
}

void FormantEngine::reset()
{
    for (auto& v : voices)
    {
        const int note = v.note;
        v = Voice{};
        v.note = note;
        v.ps.reset();
    }
}

int FormantEngine::activeVoices() const noexcept
{
    int c = 0;
    for (const auto& v : voices) if (v.active) ++c;
    return c;
}

FormantEngine::Voice* FormantEngine::oldestVoice()
{
    Voice* best = nullptr;
    for (auto& v : voices)
        if (! v.active) { best = &v; break; }
    if (best == nullptr)
        for (auto& v : voices)
            if (best == nullptr || v.seed < best->seed) best = &v;
    return best;
}

void FormantEngine::noteOn (int note, float vel)
{
    auto* v = oldestVoice();
    if (v == nullptr) return;
    const bool wasActive = v->active;
    const int oldNote = v->note;
    const auto savedPs = std::move (v->ps);
    *v = Voice{};
    v->ps = std::move (savedPs);
    if (! wasActive || oldNote != note) v->ps.reset();
    v->active = true;
    v->note = note;
    v->vel = vel;
    v->envStage = 1;
    v->env = 0.0f;
    v->stageTime = 0.0;
    v->seed = voiceCounter++;
    v->readPos = 0.0;
}

void FormantEngine::noteOff (int note)
{
    for (auto& v : voices)
        if (v.active && v.note == note && v.envStage != 4)
        { v.envStage = 4; v.stageTime = 0.0; }
}

void FormantEngine::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi,
                            const MelodicParams& p, float bendSemi)
{
    if (sample == nullptr || ! sample->isValid()) return;
    const int n = out.getNumSamples();
    auto* outL = out.getWritePointer (0);
    auto* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;
    const double dt = 1.0 / sr;
    const double sampleLen = (double) sample->numSamples();

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOn (true) && ! m.isNoteOff (true)) continue;
        if (m.getChannel() == drumMidiChannel) continue;
        if (m.isNoteOn (true)) noteOn (m.getNoteNumber(), m.getVelocity() / 127.0f);
        else noteOff (m.getNoteNumber());
    }

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

        // target pitch ratio (formants stay untouched by PSOLA)
        v.ratio = noteToRatio ((float) v.note - 60.0f + p.tune) * noteToRatio (bendSemi);
        v.ps.setRatio (v.ratio);

        // read the vocal sample at unity speed into the scratch buffer
        for (int i = 0; i < n; ++i)
        {
            scratch[(size_t) i] = readMono (*sample, v.readPos);
            v.readPos += 1.0;
            if (v.readPos >= sampleLen - 2.0) v.readPos -= sampleLen - 4.0;
        }

        // pitch-shift through PSOLA into a temp buffer, then env + place
        static thread_local std::vector<float> tmp;
        tmp.resize ((size_t) n);
        v.ps.process (scratch.data(), n, tmp.data());

        const float e = v.env * v.vel * 0.9f;
        for (int i = 0; i < n; ++i)
        {
            outL[i] += tmp[(size_t) i] * e;
            if (outR != nullptr) outR[i] += tmp[(size_t) i] * e;
        }
    }
}

} // namespace sawt
