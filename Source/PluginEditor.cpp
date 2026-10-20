/*
  SAWT - PluginEditor.cpp - the Voice-Only Synthesizer UI
*/
#include "PluginEditor.h"

using namespace sawt;
using namespace theme;

//==============================================================================
SAWTAudioProcessorEditor::SAWTAudioProcessorEditor (SAWTAudioProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      inGainKnob (p.valueTree(), pid::inGain, "IN GAIN", 54.0f),
      masterKnob (p.valueTree(), pid::master, "MASTER", 54.0f),
      monitorToggle (p.valueTree(), pid::inMonitor, "MONITOR"),
      snapToggle (p.valueTree(), pid::inSnap, "SNAP"),
      melodicDrop ("DROP A VOCAL SAMPLE - A NAME, A HUM, A BREATH"),
      engineAttach (p.valueTree(), pid::engine, engineBox),
      gModeAttach (p.valueTree(), pid::gMode, gModeBox),
      xyPad (p.valueTree(), pid::xyX, pid::xyY),
      drumDrop ("DROP VOCAL FOR DRUMS / BEATBOX"),
      drumSourceAttach (p.valueTree(), pid::dSource, drumSourceBox),
      drumPanel (p, p.valueTree()),
      spectrum (p),
      fxOnToggle (p.valueTree(), pid::fxOn, "FX")
{
    setLookAndFeel (&lnf);

    // ---- top bar ----
    addAndMakeVisible (logo);
    addAndMakeVisible (prevPreset);
    addAndMakeVisible (nextPreset);
    addAndMakeVisible (presetBox);
    addAndMakeVisible (savePreset);
    prevPreset.onClick = [this] { auto n = presetBox.getSelectedItemIndex() - 1; if (n >= 0) { presetBox.setSelectedItemIndex (n, juce::sendNotificationSync); loadSelectedPreset(); } };
    nextPreset.onClick = [this] { auto n = presetBox.getSelectedItemIndex() + 1; if (n < presetBox.getNumItems()) { presetBox.setSelectedItemIndex (n, juce::sendNotificationSync); loadSelectedPreset(); } };
    savePreset.onClick = [this] { savePresetDialog(); };
    presetBox.onChange = [this] { loadSelectedPreset(); };
    refreshPresetList();

    addAndMakeVisible (inGainKnob);
    addAndMakeVisible (masterKnob);
    addAndMakeVisible (monitorToggle);
    addAndMakeVisible (snapToggle);
    addAndMakeVisible (captureButton);
    captureButton.setClickingTogglesState (false);
    captureButton.setColour (juce::TextButton::buttonColourId, c (panel));
    captureButton.onClick = [this]
    {
        if (processor.isCapturing()) processor.stopCapture();
        else processor.startCapture();
    };

    // ---- melodic ----
    addAndMakeVisible (melodicDrop);
    melodicDrop.onFileChosen = [this] (const juce::File& f)
    { processor.loadSampleFromFile (f, false); };

    melodicTitle.setText ("VOICE ENGINE", juce::dontSendNotification);
    melodicTitle.setFont (font (12.0f));
    melodicTitle.setColour (juce::Label::textColourId, c (gold));
    melodicTitle.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (melodicTitle);

    addAndMakeVisible (melodicWave);

    engineBox.addItemList ({ "SAMPLER", "GRANULAR", "FORMANT", "XY MORPH" }, 1);
    addAndMakeVisible (engineBox);
    gModeBox.addItemList ({ "SCAN", "FREEZE", "RANDOM" }, 1);
    addAndMakeVisible (gModeBox);
    if (auto* e = processor.valueTree().getRawParameterValue (pid::engine))
        engineBox.setSelectedItemIndex ((int) e->load(), juce::dontSendNotification);
    if (auto* gm = processor.valueTree().getRawParameterValue (pid::gMode))
        gModeBox.setSelectedItemIndex ((int) gm->load(), juce::dontSendNotification);
    addAndMakeVisible (xyPad);

    addKnob (melodicKnobs, pid::tune,  "TUNE");
    addKnob (melodicKnobs, pid::fine,  "FINE");
    addKnob (melodicKnobs, pid::glide, "GLIDE");
    addKnob (melodicKnobs, pid::formant, "FORMANT");
    addKnob (melodicKnobs, pid::att,   "ATTACK");
    addKnob (melodicKnobs, pid::dec,   "DECAY");
    addKnob (melodicKnobs, pid::sus,   "SUSTAIN");
    addKnob (melodicKnobs, pid::rel,   "RELEASE");
    addKnob (melodicKnobs, pid::cutoff,"CUTOFF");
    addKnob (melodicKnobs, pid::reso,  "RESO");
    addKnob (melodicKnobs, pid::envAmt,"ENV>CUT");
    addKnob (melodicKnobs, pid::unison,"UNISON");
    addKnob (melodicKnobs, pid::detune,"DETUNE");
    addKnob (melodicKnobs, pid::spread,"SPREAD");
    addKnob (melodicKnobs, pid::level, "LEVEL");
    addKnob (granKnobs, pid::gSize,   "G SIZE");
    addKnob (granKnobs, pid::gRate,   "G RATE");
    addKnob (granKnobs, pid::gPos,    "G POS");
    addKnob (granKnobs, pid::gDrift,  "DRIFT");
    addKnob (granKnobs, pid::gJitter, "JITTER");

    // ---- drums ----
    addAndMakeVisible (drumDrop);
    drumDrop.onFileChosen = [this] (const juce::File& f)
    { processor.loadSampleFromFile (f, true); };

    drumTitle.setText ("VOCAL DRUMS", juce::dontSendNotification);
    drumTitle.setFont (font (12.0f));
    drumTitle.setColour (juce::Label::textColourId, c (gold));
    drumTitle.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (drumTitle);

    drumSourceBox.addItemList ({ "SLICE KIT", "VOCAL 808s" }, 1);
    addAndMakeVisible (drumSourceBox);
    if (auto* dsrc = processor.valueTree().getRawParameterValue (pid::dSource))
        drumSourceBox.setSelectedItemIndex ((int) dsrc->load(), juce::dontSendNotification);
    addAndMakeVisible (sliceButton);
    sliceButton.onClick = [this] { processor.autoSliceDrums(); };
    addKnob (drumKnobs, pid::dPunch, "PUNCH", 50.0f);
    addKnob (drumKnobs, pid::dDrive, "DRIVE", 50.0f);
    addKnob (drumKnobs, pid::dTone,  "TONE",  50.0f);
    addKnob (drumKnobs, pid::dLevel, "LEVEL", 50.0f);
    addAndMakeVisible (drumPanel);

    // ---- bottom ----
    addAndMakeVisible (spectrum);
    addAndMakeVisible (fxOnToggle);
    addKnob (fxKnobs, pid::eLow,    "LOW",   44.0f, 8.5f);
    addKnob (fxKnobs, pid::eMid,    "MID",   44.0f, 8.5f);
    addKnob (fxKnobs, pid::eHigh,   "HIGH",  44.0f, 8.5f);
    addKnob (fxKnobs, pid::cThresh, "THRS",  44.0f, 8.5f);
    addKnob (fxKnobs, pid::cRatio,  "RATIO", 44.0f, 8.5f);
    addKnob (fxKnobs, pid::sDrive,  "DRIVE", 44.0f, 8.5f);
    addKnob (fxKnobs, pid::chRate,  "RATE",  44.0f, 8.5f);
    addKnob (fxKnobs, pid::chDepth, "DEPTH", 44.0f, 8.5f);
    addKnob (fxKnobs, pid::chMix,   "MIX",   44.0f, 8.5f);
    addKnob (fxKnobs, pid::dlTime,  "TIME",  44.0f, 8.5f);
    addKnob (fxKnobs, pid::dlFb,    "FB",    44.0f, 8.5f);
    addKnob (fxKnobs, pid::dlMix,   "MIX",   44.0f, 8.5f);
    addKnob (fxKnobs, pid::rvSize,  "SIZE",  44.0f, 8.5f);
    addKnob (fxKnobs, pid::rvDamp,  "DAMP",  44.0f, 8.5f);
    addKnob (fxKnobs, pid::rvMix,   "MIX",   44.0f, 8.5f);
    addKnob (fxKnobs, pid::lfBits,  "BITS",  44.0f, 8.5f);
    addKnob (fxKnobs, pid::lfWow,   "WOW",   44.0f, 8.5f);
    addKnob (fxKnobs, pid::lfBreath,"BREATH",44.0f, 8.5f);
    addKnob (fxKnobs, pid::width,   "WIDTH", 44.0f, 8.5f);

    setResizable (true, true);
    setResizeLimits (1000, 660, 1900, 1250);
    setSize (designW, designH);
    startTimerHz (12);
}

SAWTAudioProcessorEditor::~SAWTAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void SAWTAudioProcessorEditor::timerCallback()
{
    melodicWave.setPlayheadNorm (processor.melodicPlayPos.load (std::memory_order_relaxed));
    melodicWave.repaint();

    // keep drop zones & waveforms in sync with loaded samples
    if (auto ms = processor.getMelodicSample())
    {
        if (ms != lastMelodic) { lastMelodic = ms; melodicWave.setSample (ms); }
        melodicDrop.setFileName (ms->name);
    }
    if (auto ds = processor.getDrumSample())
    {
        if (ds != lastDrum)
        {
            lastDrum = ds;
            // reflect fresh slice markers whenever the drum sample changes
            processor.autoSliceDrums();
        }
        drumDrop.setFileName (ds->name);
    }

    static int lastSliceCount = -1;
    const int sc = processor.drumEngine().sliceCount.load (std::memory_order_relaxed);
    if (sc != lastSliceCount)
    {
        lastSliceCount = sc;
        // drum waveform is refreshed via drumDrop / wave views below
    }

    captureButton.setButtonText (processor.isCapturing() ? "STOP  [REC]" : "CAPTURE MIC");
    captureButton.setColour (juce::TextButton::buttonColourId,
                             processor.isCapturing() ? c (red) : c (panel));
    xyPad.syncFromParams();
}

//==============================================================================
void SAWTAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    auto factories = PresetManager::factoryNames();
    for (int i = 0; i < factories.size(); ++i)
        presetBox.addItem (factories[i], 1 + i);              // ids 1..4

    auto users = processor.presets().listUserPresets();
    if (users.size() > 0) presetBox.addSeparator();
    for (int i = 0; i < users.size(); ++i)
        presetBox.addItem (users[i], 100 + i);                // ids 100+

    presetBox.setSelectedItemIndex (0, juce::dontSendNotification);
}

void SAWTAudioProcessorEditor::loadSelectedPreset()
{
    const int id = presetBox.getSelectedId();
    if (id >= 1 && id <= 4)
    {
        processor.presets().loadFactory (id - 1);
        processor.presets().currentPresetName = presetBox.getText();
    }
    else if (id >= 100)
    {
        processor.presets().loadUserPreset (presetBox.getText());
    }
    xyPad.syncFromParams();
}

void SAWTAudioProcessorEditor::savePresetDialog()
{
    auto* aw = new juce::AlertWindow ("Save Preset",
                                      "Name for this voice preset:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", processor.presets().currentPresetName);
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, aw] (int result)
        {
            if (result == 1)
            {
                if (auto* te = aw->getTextEditor ("name"))
                {
                    const auto name = te->getText();
                    if (processor.presets().saveUserPreset (name))
                        processor.presets().currentPresetName = name;
                    refreshPresetList();
                }
            }
            delete aw;
        }), false);
}

void SAWTAudioProcessorEditor::addKnob (std::vector<std::unique_ptr<sawt::Knob>>& v,
                                        const char* id, const char* name,
                                        float size, float titleSize)
{
    v.push_back (std::make_unique<sawt::Knob> (processor.valueTree(), id, name, size, titleSize));
    addAndMakeVisible (*v.back());
}

//==============================================================================
void SAWTAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (c (bg));

    // faint watermark star
    theme::fillStar (g, juce::Rectangle<float> (-160.0f, 180.0f, 640.0f, 640.0f),
                     c (gold).withAlpha (0.025f));
}

void SAWTAudioProcessorEditor::resized()
{
    const float sx = (float) getWidth() / (float) designW;
    const float sy = (float) getHeight() / (float) designH;
    auto R = [sx, sy] (float x, float y, float w, float h)
    { return juce::Rectangle<int> ((int) (x * sx), (int) (y * sy),
                                   juce::jmax (1, (int) (w * sx)), juce::jmax (1, (int) (h * sy))); };

    // ---------------- top bar ----------------
    logo.setBounds (R (10, 4, 262, 58));
    prevPreset.setBounds (R (284, 18, 26, 28));
    presetBox.setBounds (R (314, 18, 218, 28));
    nextPreset.setBounds (R (536, 18, 26, 28));
    savePreset.setBounds (R (568, 18, 62, 28));

    inGainKnob.setBounds (R (642, 6, 58, 56));
    monitorToggle.setBounds (R (704, 16, 86, 26));
    snapToggle.setBounds (R (792, 16, 70, 26));
    captureButton.setBounds (R (866, 16, 118, 28));
    masterKnob.setBounds (R (994, 6, 58, 56));

    // ---------------- melodic panel ----------------
    const auto mel = R (10, 68, 576, 492);
    auto b = mel;
    {
        // section title
        b.reduce (8, 4);
        melodicTitle.setBounds (b.removeFromTop (16));

        auto dz = b.removeFromTop (72);
        melodicDrop.setBounds (dz);
        b.reduce (0, 2);
        melodicWave.setBounds (b.removeFromTop (54));

        // engine + XY row
        b.reduce (0, 3);
        auto row = b.removeFromTop (118);
        xyPad.setBounds (row.removeFromRight (118));
        engineBox.setBounds (row.removeFromTop (26).removeFromLeft (190));
        auto knobRow = row.removeFromTop (66).translated (0, 24);
        const int kw = 66;
        melodicKnobs[2]->setBounds (knobRow.removeFromLeft (kw));   // glide
        melodicKnobs[3]->setBounds (knobRow.removeFromLeft (kw));   // formant

        // macro rows
        b.reduce (0, 3);
        auto r1 = b.removeFromTop (84);
        const int ids1[6] = { 0, 1, 4, 5, 6, 7 };   // glide+formant live in engine row
        const int w1 = r1.getWidth() / 6;
        for (int i = 0; i < 6; ++i)
            melodicKnobs[(size_t) ids1[i]]->setBounds (r1.getX() + i * w1, r1.getY(), w1, r1.getHeight());

        auto r2 = b.removeFromTop (84);
        const int n2 = 7;
        const int w2 = r2.getWidth() / n2;
        for (int i = 0; i < n2; ++i)
            melodicKnobs[(size_t) (8 + i)]->setBounds (r2.getX() + i * w2, r2.getY(), w2, r2.getHeight());

        // granular row
        auto r3 = b.removeFromTop (84);
        gModeBox.setBounds (r3.removeFromRight (96).withSizeKeepingCentre (96, 26));
        const int n3 = 5;
        const int w3 = r3.getWidth() / n3;
        for (int i = 0; i < n3; ++i)
            granKnobs[(size_t) i]->setBounds (r3.getX() + i * w3, r3.getY(), w3, r3.getHeight());
    }

    // ---------------- drum panel ----------------
    const auto dr = R (594, 68, 556, 492);
    b = dr;
    {
        b.reduce (8, 4);
        drumTitle.setBounds (b.removeFromTop (16));

        auto head = b.removeFromTop (66);
        drumSourceBox.setBounds (head.removeFromLeft (128).withHeight (26).withY (head.getY() + 2));
        sliceButton.setBounds (head.removeFromLeft (110).withHeight (26).withY (head.getY() + 2));
        const int dk = (int) head.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
            drumKnobs[(size_t) i]->setBounds (head.getX() + i * dk, head.getY() - 8, dk, 74);

        drumDrop.setBounds (b.removeFromTop (58));
        b.reduce (0, 2);
        drumPanel.setBounds (b.removeFromTop (300));
    }

    // ---------------- spectrum + FX ----------------
    spectrum.setBounds (R (10, 566, 1140, 42));

    auto fxr = R (10, 614, 1140, 118);
    fxOnToggle.setBounds (fxr.removeFromLeft (46).removeFromTop (24));
    auto fxArea = fxr.reduced (2, 2);
    const int nMods = 8;
    const int mw = fxArea.getWidth() / nMods;
    // order: EQ(3) COMP(2) DRIVE(1) CHORUS(3) DELAY(3) VERB(3) LOFI(3) WIDTH(1)
    const int counts[nMods] = { 3, 2, 1, 3, 3, 3, 3, 1 };
    int idx = 0;
    for (int m = 0; m < nMods; ++m)
    {
        auto cell = fxArea.removeFromLeft (mw);
        const int k = counts[m];
        const int kw = cell.getWidth() / juce::jmax (1, k);
        for (int i = 0; i < k; ++i)
        {
            if (idx < (int) fxKnobs.size())
                fxKnobs[(size_t) idx]->setBounds (cell.getX() + i * kw, cell.getY(), kw, cell.getHeight());
            ++idx;
        }
    }
}
