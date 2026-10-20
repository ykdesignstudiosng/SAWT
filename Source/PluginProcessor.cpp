/*
  SAWT - PluginProcessor.cpp
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace sawt;

//==============================================================================
SAWTAudioProcessor::SAWTAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SAWT", ParamStore::createLayout()),
      store (apvts),
      presetManager (apvts)
{
    formatManager.registerBasicFormats();
    formatManager.registerFormat (new juce::FlacAudioFormat(), false);
    formatManager.registerFormat (new juce::OggVorbisAudioFormat(), false);
#if ! JUCE_ANDROID
    formatManager.registerFormat (new juce::MP3AudioFormat(), false);
#endif
    store.cachePointers();
    for (auto& b : spectrumBands) b.store (0.0f);
}

//==============================================================================
bool SAWTAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo()
        || in == juce::AudioChannelSet::mono()
        || in.isDisabled();
}

void SAWTAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = samplesPerBlock;

    sampler.prepare (sampleRate, samplesPerBlock);
    granular.prepare (sampleRate, samplesPerBlock);
    formantEngine.prepare (sampleRate, samplesPerBlock);
    formantShiftL.prepare (sampleRate, samplesPerBlock);
    formantShiftR.prepare (sampleRate, samplesPerBlock);
    drums.prepare (sampleRate, samplesPerBlock);
    fx.prepare (sampleRate, samplesPerBlock);
    snapPSOLA.prepare (sampleRate, samplesPerBlock);
    snapDetector.prepare (sampleRate);

    xyBufA.setSize (2, samplesPerBlock);
    xyBufB.setSize (2, samplesPerBlock);
    melodicBuf.setSize (2, samplesPerBlock);
    drumBuf.setSize (2, samplesPerBlock);
    monoIn.setSize (1, samplesPerBlock);
    snapScratch.assign ((size_t) samplesPerBlock, 0.0f);
    snapOut.assign ((size_t) samplesPerBlock, 0.0f);
    specFifo.setSize (2, 4096);
}

//==============================================================================
SamplePtr SAWTAudioProcessor::getMelodicSample() const
{
    const juce::SpinLock::ScopedLockType sl (sampleLock);
    return melodicSample;
}

SamplePtr SAWTAudioProcessor::getDrumSample() const
{
    const juce::SpinLock::ScopedLockType sl (sampleLock);
    return drumSample;
}

void SAWTAudioProcessor::loadSampleInternal (SamplePtr&& s, bool drum)
{
    {
        const juce::SpinLock::ScopedLockType sl (sampleLock);
        if (drum) drumSample = std::move (s);
        else      melodicSample = std::move (s);
    }
}

bool SAWTAudioProcessor::loadSampleFromFile (const juce::File& f, bool drum)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (f));
    if (reader == nullptr)
    {
        lastLoadError = "Could not decode: " + f.getFileName();
        return false;
    }

    auto data = std::make_shared<SampleData>();
    const int len = (int) juce::jmin ((juce::int64) 60 * 48000 * 2, reader->lengthInSamples);
    data->buffer.setSize ((int) reader->numChannels, juce::jmax (1, len));
    reader->read (data->buffer.getArrayOfWritePointers(), (int) reader->numChannels,
                  0, juce::jmax (1, len));
    data->sampleRate = reader->sampleRate;
    data->name = f.getFileNameWithoutExtension();
    data->filePath = f.getFullPathName();

    // mono view
    data->mono.setSize (1, data->buffer.getNumSamples());
    if (data->buffer.getNumChannels() >= 2)
    {
        data->mono.copyFrom (0, 0, data->buffer, 0, 0, data->buffer.getNumSamples());
        data->mono.addFrom (0, 0, data->buffer, 1, 0, data->buffer.getNumSamples(), 0.5f);
        data->mono.applyGain (0, data->buffer.getNumSamples(), 0.6666f);
    }
    else
    {
        data->mono.copyFrom (0, 0, data->buffer, 0, 0, data->buffer.getNumSamples());
    }

    loadSampleInternal (std::move (data), drum);

    if (drum) rebuildSlicesFromCurrentDrumSample();
    else      analyseMelodicForDrums();

    lastLoadError = {};
    return true;
}

void SAWTAudioProcessor::rebuildSlicesFromCurrentDrumSample()
{
    auto s = getDrumSample();
    if (s == nullptr || ! s->isValid()) return;
    TransientSlicer slicer;
    auto cuts = slicer.slice (*s, 8);
    drums.setSlices (*s, cuts);
}

void SAWTAudioProcessor::autoSliceDrums()
{
    rebuildSlicesFromCurrentDrumSample();
}

void SAWTAudioProcessor::analyseMelodicForDrums()
{
    auto s = getMelodicSample();
    if (s == nullptr || ! s->isValid()) return;
    const float tone = store.drums().tone;
    VoiceAnalyzer analyzer;
    material = analyzer.analyze (*s, tone);
    drums.setMaterial (material);

    auto breathCopy = std::make_shared<juce::AudioBuffer<float>>();
    breathCopy->setSize (1, material.breath.getNumSamples(), false, false, true);
    breathCopy->copyFrom (0, 0, material.breath, 0, 0, material.breath.getNumSamples());
    fx.setBreathLayer (std::move (breathCopy));
}

void SAWTAudioProcessor::triggerPad (int pad, float vel) { drums.triggerPad (pad, vel); }

void SAWTAudioProcessor::clearMelodicSample()
{
    loadSampleInternal (nullptr, false);
    material = VoiceMaterial{};
    drums.setMaterial (material);
    fx.setBreathLayer (nullptr);
}

void SAWTAudioProcessor::clearDrumSample()
{
    loadSampleInternal (nullptr, true);
    drums.setSlices (SampleData{}, {});
}

//==============================================================================
// Live capture (mic -> melodic slot)
void SAWTAudioProcessor::startCapture()
{
    captureBuffer.setSize (2, (int) (30.0 * sr));
    captureBuffer.clear();
    capturePos = 0;
    capturePrimed = true;
    capturing.store (true);
}

void SAWTAudioProcessor::stopCapture()
{
    captureStopRequested.store (true);
}

void SAWTAudioProcessor::handleAsyncUpdate()
{
    // runs on the message thread after a capture finishes
    auto data = std::make_shared<SampleData>();
    data->buffer.setSize (2, juce::jmax (1, capturePos));
    for (int ch = 0; ch < 2; ++ch)
        data->buffer.copyFrom (ch, 0, captureBuffer, ch, 0, juce::jmax (1, capturePos));
    data->sampleRate = sr;
    data->name = "Captured Voice";
    data->filePath = "(captured)";

    data->mono.setSize (1, data->buffer.getNumSamples());
    data->mono.copyFrom (0, 0, data->buffer, 0, 0, data->buffer.getNumSamples());
    data->mono.addFrom (0, 0, data->buffer, 1, 0, data->buffer.getNumSamples(), 0.5f);
    data->mono.applyGain (0, data->buffer.getNumSamples(), 0.6666f);

    loadSampleInternal (std::move (data), false);
    analyseMelodicForDrums();
}

//==============================================================================
void SAWTAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0) return;

    const auto mp = store.melodic();
    const auto dp = store.drums();
    const auto lp = store.live();
    const auto xp = store.fx();
    const float masterGain = store.master();

    // ---- MIDI bookkeeping (bend + last held note for snap) ----------------
    float bendSemi = 0.0f;
    int bendCount = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isPitchWheel())
        {
            bendSemi += ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f;
            ++bendCount;
        }
        else if (m.getChannel() != drumMidiChannel)
        {
            if (m.isNoteOn (true))  ++heldNoteCount;
            if (m.isNoteOff (true)) heldNoteCount = juce::jmax (0, heldNoteCount - 1);
            if (m.isNoteOn (true))
                lastHeldFreq = 440.0f * std::pow (2.0f, (float) (m.getNoteNumber() - 69) / 12.0f);
        }
    }
    if (bendCount > 0) bendSemi /= (float) bendCount;

    // ---- render melodic ----------------------------------------------------
    melodicBuf.clear();
    const bool formantActive = mp.engine == EngineType::Formant;
    const bool xyActive = mp.engine == EngineType::XY;

    switch (mp.engine)
    {
        case EngineType::Sampler:
            sampler.render (melodicBuf, midi, mp, bendSemi);
            melodicPlayPos.store (sampler.playPosNorm.load(), std::memory_order_relaxed);
            break;
        case EngineType::Granular:
            granular.render (melodicBuf, midi, mp, bendSemi);
            break;
        case EngineType::Formant:
            formantEngine.render (melodicBuf, midi, mp, bendSemi);
            break;
        case EngineType::XY:
        {
            const float wG = juce::jlimit (0.0f, 1.0f, mp.xyX);
            xyBufA.clear(); xyBufB.clear();
            sampler.render (xyBufA, midi, mp, bendSemi);
            granular.render (xyBufB, midi, mp, bendSemi);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = melodicBuf.getWritePointer (ch);
                const auto* a = xyBufA.getReadPointer (ch);
                const auto* b = xyBufB.getReadPointer (ch);
                for (int i = 0; i < n; ++i)
                    d[i] = a[i] * (1.0f - wG) + b[i] * wG;
            }
            melodicPlayPos.store (sampler.playPosNorm.load(), std::memory_order_relaxed);
            break;
        }
    }

    // XY Y-dimension: drives grain drift + filter opening for timbre morph
    if (xyActive)
    {
        const float y = juce::jlimit (0.0f, 1.0f, mp.xyY);
        auto* dL = melodicBuf.getWritePointer (0);
        auto* dR = melodicBuf.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            dL[i] *= 0.7f + 0.6f * y;
            dR[i] *= 0.7f + 0.6f * y;
        }
    }

    // ---- global formant shift on the melodic bus ---------------------------
    int latency = 0;
    if (std::abs (mp.formant) > 0.02f)
    {
        formantShiftL.setShiftSemitones (mp.formant);
        formantShiftR.setShiftSemitones (mp.formant);
        formantShiftL.process (melodicBuf.getReadPointer (0), n, melodicBuf.getWritePointer (0));
        if (melodicBuf.getNumChannels() > 1)
            formantShiftR.process (melodicBuf.getReadPointer (1), n, melodicBuf.getWritePointer (1));
        latency = formantShiftL.latency();
    }
    else if (formantActive)
    {
        latency = formantEngine.latency();
    }

    // ---- render drums -------------------------------------------------------
    drumBuf.clear();
    drums.render (drumBuf, midi, dp);

    // ---- live input: monitor / snap-to-note ---------------------------------
    const int numIn = juce::jmin (2, getMainBusNumInputChannels());
    if (numIn > 0 && (lp.monitor || capturing))
    {
        const float ig = lp.inGain;
        for (int ch = 0; ch < numIn; ++ch)
            for (int i = 0; i < n; ++i)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * ig);

        if (capturing)
        {
            for (int ch = 0; ch < 2; ++ch)
                captureBuffer.copyFrom (ch, capturePos, buffer,
                                        juce::jmin (ch, buffer.getNumChannels() - 1), 0, n);
            capturePos += n;
            if (capturePos + n >= captureBuffer.getNumSamples() || captureStopRequested.load())
            {
                capturing.store (false);
                captureStopRequested.store (false);
                triggerAsyncUpdate();
            }
        }

        if (lp.snap && heldNoteCount > 0 && ! capturing)
        {
            // detect input F0, then PSOLA the input onto the held note (mono path)
            float period = 0.0f, clarity = 0.0f;
            const bool detected = snapDetector.push (buffer.getReadPointer (0), n, period, clarity);
            if (detected && clarity > 0.4f)
            {
                const float f0 = (float) sr / period;
                const float ratio = juce::jlimit (0.25f, 4.0f, lastHeldFreq / juce::jmax (40.0f, f0));
                snapPSOLA.setRatio (ratio);
            }
            snapPSOLA.process (buffer.getReadPointer (0), n, snapOut.data());
            for (int i = 0; i < n; ++i)
            {
                buffer.setSample (0, i, snapOut[i]);
                if (numIn > 1) buffer.setSample (1, i, snapOut[i]);
            }
            latency = juce::jmax (latency, snapPSOLA.latency());
        }
    }

    // ---- mix -----------------------------------------------------------------
    const float mLevel = mp.level;
    const float dLevel = dp.level;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* out = buffer.getWritePointer (ch);
        const auto* mel = melodicBuf.getReadPointer (ch);
        const auto* dr = drumBuf.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
            out[i] += mel[i] * mLevel + dr[i] * dLevel;
    }

    // ---- FX rack + master -----------------------------------------------------
    fx.update (xp, sr);
    fx.process (buffer);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* out = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            out[i] = std::tanh (out[i] * masterGain);
    }

    // ---- spectrum feed (mono mixdown into fifo) --------------------------------
    {
        int start1, size1, start2, size2;
        specFifoCtrl.prepareToWrite (n, start1, size1, start2, size2);
        if (size1 > 0) specFifo.copyFrom (0, start1, buffer, 0, 0, size1);
        if (size2 > 0) specFifo.copyFrom (0, start2, buffer, 0, size1, size2);
        specFifoCtrl.finishedWrite (size1 + size2);
    }

    // ---- report latency ---------------------------------------------------------
    setLatencySamples (latency);
    reportedLatency.store ((float) latency, std::memory_order_relaxed);
}

//==============================================================================
void SAWTAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = apvts.copyState();
    if (auto ms = getMelodicSample()) tree.setProperty ("melodicPath", ms->filePath, nullptr);
    if (auto ds = getDrumSample())    tree.setProperty ("drumPath",    ds->filePath, nullptr);
    std::unique_ptr<juce::XmlElement> xml (tree.createXml());
    copyXmlToBinary (*xml, destData);
}

void SAWTAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return;
    apvts.replaceState (tree);

    const auto melPath = tree.getProperty ("melodicPath").toString();
    const auto drumPath = tree.getProperty ("drumPath").toString();
    if (melPath.isNotEmpty() && juce::File (melPath).existsAsFile())
        loadSampleFromFile (melPath, false);
    if (drumPath.isNotEmpty() && juce::File (drumPath).existsAsFile())
        loadSampleFromFile (drumPath, true);
}

//==============================================================================
juce::AudioProcessorEditor* SAWTAudioProcessor::createEditor()
{
    return new SAWTAudioProcessorEditor (*this);
}

//==============================================================================
// NOTE: pushSpectrum is kept for future multi-band staging; the fifo above is
// consumed by the editor which performs its own FFT (non-realtime thread).
void SAWTAudioProcessor::pushSpectrum (const float*, const float*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SAWTAudioProcessor();
}
