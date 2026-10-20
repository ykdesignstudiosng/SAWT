/*
  SAWT - PluginProcessor.h
  The Voice-Only Synthesizer.

  Signal chain (audio thread):
    input ──┬─ monitor/snap ─────────────────────────┐
            │                                        ▼
    MIDI ─> melodic engines ─> formant shifter ─> mix ─> FX rack ─> master ─> out
    MIDI ─> drum engine ────────────────────────┘

  Every sample rendered originates from the user's vocal material.
*/
#pragma once

#include "Params.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "Core/PresetManager.h"
#include "DSP/SamplerEngine.h"
#include "DSP/GranularEngine.h"
#include "DSP/FormantEngine.h"
#include "DSP/FormantShifter.h"
#include "DSP/DrumEngine.h"
#include "DSP/EffectsRack.h"
#include "DSP/PSOLA.h"
#include "DSP/PitchDetector.h"

#include <atomic>
#include <mutex>

class SAWTAudioProcessor : public juce::AudioProcessor,
                           private juce::AsyncUpdater
{
public:
    SAWTAudioProcessor();
    ~SAWTAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SAWT"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Editor-facing API (message thread)
    juce::AudioProcessorValueTreeState& valueTree() { return apvts; }
    sawt::PresetManager& presets() { return presetManager; }

    sawt::SamplePtr getMelodicSample() const;
    sawt::SamplePtr getDrumSample() const;
    bool loadSampleFromFile (const juce::File& f, bool drum);
    void autoSliceDrums();                       // re-run transient slicer on drum sample
    void triggerPad (int pad, float vel);
    void clearMelodicSample();
    sawt::DrumEngine& drumEngine() { return drums; }    void clearDrumSample();

    // live capture (mic -> melodic slot)
    bool isCapturing() const noexcept { return capturing.load(); }
    void startCapture();
    void stopCapture();

    // spectrum bands for the UI (0..1, log-spaced)
    std::array<std::atomic<float>, sawt::numSpectrumBands> spectrumBands {};

    // playback position for the waveform cursor
    std::atomic<float> melodicPlayPos { 0.0f };

    juce::String lastLoadError;

    // spectrum fifo (editor drains this on its UI timer)
    juce::AudioBuffer<float> specFifo;
    juce::AbstractFifo specFifoCtrl { 4096 };

private:
    void loadSampleInternal (sawt::SamplePtr&& s, bool drum);
    void rebuildSlicesFromCurrentDrumSample();
    void analyseMelodicForDrums();               // wavetable + breath extraction
    void pushSpectrum (const float* interleavedL, const float* maybeR, int n);
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState apvts;
    sawt::ParamStore store;
    sawt::PresetManager presetManager;

    // engines
    sawt::SamplerEngine sampler;
    sawt::GranularEngine granular;
    sawt::FormantEngine formantEngine;
    sawt::FormantShifter formantShiftL, formantShiftR;
    sawt::DrumEngine drums;
    sawt::EffectsRack fx;
    sawt::PSOLA snapPSOLA;
    sawt::PitchDetector snapDetector;

    // samples (message-thread writers, audio-thread readers via spinlock)
    juce::SpinLock sampleLock;
    sawt::SamplePtr melodicSample, drumSample;

    // analysis results for the drum synth
    sawt::VoiceMaterial material;

    // scratch
    juce::AudioBuffer<float> melodicBuf, drumBuf, monoIn;
    juce::AudioBuffer<float> xyBufA, xyBufB;
    std::vector<float> snapScratch, snapOut;
    double sr = 48000.0;
    int maxBlock = 512;

    // live snap + capture
    std::atomic<bool> capturing { false };
    std::atomic<bool> captureStopRequested { false };
    juce::AudioBuffer<float> captureBuffer;      // audio-thread owned while capturing
    int capturePos = 0;
    bool capturePrimed = false;
    float lastHeldFreq = 220.0f;
    int heldNoteCount = 0;

    juce::AudioFormatManager formatManager;

    std::atomic<float> reportedLatency { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SAWTAudioProcessor)
};
