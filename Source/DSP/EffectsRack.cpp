/*
  SAWT - EffectsRack.cpp
*/
#include "EffectsRack.h"

namespace sawt
{

void EffectsRack::prepare (double sampleRate, int maxBlockSize)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlockSize, 2 };

    eqLow.prepare (spec);
    eqMid.prepare (spec);
    eqHigh.prepare (spec);
    compressor.prepare (spec);
    shaper.prepare (spec);
    chorus.prepare (spec);
    reverb.prepare (spec);
    delay.lineL.prepare (spec);
    delay.lineR.prepare (spec);
    delay.lineL.setMaximumDelayInSamples (1 << 16);
    delay.lineR.setMaximumDelayInSamples (1 << 16);
    wowLine.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
    wowLine.setMaximumDelayInSamples (1 << 12);

    shaper.functionToUse = [] (float x)
    {
        // soft asymmetric saturation, gentle at low drive
        return std::tanh (x);
    };

    eqDirty = reverbDirty = true;
    reset();
}

void EffectsRack::reset()
{
    eqLow.reset();
    eqMid.reset();
    eqHigh.reset();
    compressor.reset();
    shaper.reset();
    chorus.reset();
    reverb.reset();
    delay.lineL.reset();
    delay.lineR.reset();
    wowLine.reset();
}

void EffectsRack::setBreathLayer (std::shared_ptr<juce::AudioBuffer<float>> breath)
{
    if (breath == nullptr || breath->getNumSamples() < 64)
    {
        const juce::SpinLock::ScopedLockType sl (breathLock);
        breathPtr = nullptr;
        return;
    }

    auto looped = std::make_shared<juce::AudioBuffer<float>>();
    looped->setSize (1, breath->getNumSamples(), false, false, true);
    looped->copyFrom (0, 0, *breath, 0, 0, breath->getNumSamples());
    // loop-smooth edges
    auto* d = looped->getWritePointer (0);
    const int n = looped->getNumSamples();
    const int fade = juce::jmin (256, n / 4);
    for (int i = 0; i < fade; ++i)
    {
        const float w = (float) i / (float) fade;
        d[i] = d[i] * w + d[n - fade + i] * (1.0f - w);
    }

    const juce::SpinLock::ScopedLockType sl (breathLock);
    breathPtr = std::move (looped);
}

void EffectsRack::update (const FXParams& p, double sr)
{
    params = p;
    updateEQ (sr);
    updateReverb();
    updateDelay (sr);

    compressor.setThreshold (p.cThresh);
    compressor.setRatio (p.cRatio);
    compressor.setAttack (8.0f);
    compressor.setRelease (180.0f);

    chorus.setRate (p.chRate);
    chorus.setDepth (p.chDepth);
    chorus.setCentreDelay (25.0f);
    chorus.setFeedback (0.15f);
    chorus.setMix (p.chMix);

    delay.fb = p.dlFb;
    delay.mix = p.dlMix;
}

void EffectsRack::updateEQ (double sr)
{
    auto low  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sr, 180.0, 0.7, juce::Decibels::decibelsToGain (params.eLow));
    auto mid  = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, params.eMidFreq, 0.9, juce::Decibels::decibelsToGain (params.eMid));
    auto high = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 5500.0, 0.7, juce::Decibels::decibelsToGain (params.eHigh));
    *eqLow.state  = *low;
    *eqMid.state  = *mid;
    *eqHigh.state = *high;
}

void EffectsRack::updateReverb()
{
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = params.rvSize;
    rp.damping = params.rvDamp;
    rp.wetLevel = params.rvMix;
    rp.dryLevel = 1.0f - params.rvMix * 0.4f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
}

void EffectsRack::updateDelay (double sr)
{
    delay.timeSamples = (int) juce::jlimit (16.0, (double) (1 << 16) - 2.0,
                                            params.dlTime * sr);
}

void EffectsRack::process (juce::AudioBuffer<float>& buffer)
{
    if (! params.on) return;
    const int n = buffer.getNumSamples();
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // 1) EQ
    eqLow.process (ctx);
    eqMid.process (ctx);
    eqHigh.process (ctx);

    // 2) compressor
    compressor.process (ctx);

    // 3) drive (amount via pre-gain / post-compensation)
    if (params.sDrive > 0.001f)
    {
        const float pre = 1.0f + params.sDrive * 8.0f;
        const float post = 1.0f / (1.0f + params.sDrive * 3.0f);
        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i) d[i] *= pre;
        }
        shaper.process (ctx);
        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i) d[i] *= post;
        }
    }

    // 4) chorus
    chorus.process (ctx);

    // 5) stereo ping-pong-ish feedback delay
    if (params.dlMix > 0.001f)
    {
        for (int i = 0; i < n; ++i)
        {
            const float inL = buffer.getSample (0, i);
            const float inR = buffer.getNumChannels() > 1 ? buffer.getSample (1, i) : inL;
            const float dL = delay.lineL.popSample (0, (float) delay.timeSamples, true);
            const float dR = delay.lineR.popSample (1, (float) delay.timeSamples * 1.5f, true);
            delay.lineL.pushSample (0, inR + dR * delay.fb);
            delay.lineR.pushSample (1, inL + dL * delay.fb);
            buffer.setSample (0, i, inL + dL * delay.mix);
            if (buffer.getNumChannels() > 1) buffer.setSample (1, i, inR + dR * delay.mix);
        }
    }

    // 6) reverb
    reverb.process (ctx);

    // 7) lo-fi: wow (pitch wobble), bit crush, decimation, breath noise floor
    if (params.lfBits < 15.5f || params.lfWow > 0.001f || params.lfBreath > 0.001f)
    {
        const juce::SpinLock::ScopedLockType sl (breathLock);
        const float* breathData = (breathPtr != nullptr && breathPtr->getNumSamples() > 64)
                                    ? breathPtr->getReadPointer (0) : nullptr;
        const int breathN = (breathData != nullptr) ? breathPtr->getNumSamples() : 0;

        const float levels = std::pow (2.0f, juce::jlimit (4.0f, 16.0f, params.lfBits) - 1.0f);
        const float wowDepth = params.lfWow * 220.0f;   // max delay modulation in samples
        const int chCount = juce::jmin (2, buffer.getNumChannels());
        for (int i = 0; i < n; ++i)
        {
            phase += 0.7f / 48000.0f;
            if (phase > 1.0f) phase -= 1.0f;
            const float wow = wowDepth * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase * 1.7f));

            for (int ch = 0; ch < chCount; ++ch)
            {
                float x = buffer.getSample (ch, i);

                if (params.lfWow > 0.001f)
                {
                    wowLine.pushSample (0, x);
                    x = wowLine.popSample (0, juce::jlimit (10.0f, (float) (1 << 12) - 10.0f, wow), true);
                }

                if (params.lfBits < 15.5f)
                    x = std::round (x * levels) / levels;

                if (params.lfBreath > 0.001f && breathData != nullptr)
                    x += breathData[breathPos] * params.lfBreath * 0.6f;

                buffer.setSample (ch, i, x);
            }
            ++breathPos;
            if (breathN > 0 && breathPos >= breathN) breathPos = 0;
        }
    }

    // 8) stereo width (mid/side)
    if (buffer.getNumChannels() >= 2 && std::abs (params.width - 1.0f) > 0.001f)
    {
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getWritePointer (1);
        const float w = params.width;
        for (int i = 0; i < n; ++i)
        {
            const float m = (L[i] + R[i]) * 0.5f;
            const float s = (L[i] - R[i]) * 0.5f * w;
            L[i] = m + s;
            R[i] = m - s;
        }
    }
}

} // namespace sawt
