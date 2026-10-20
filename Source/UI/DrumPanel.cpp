/*
  SAWT - DrumPanel.cpp
*/
#include "DrumPanel.h"
#include "../PluginProcessor.h"
#include "LookAndFeel.h"
#include "Theme.h"

namespace sawt
{
using namespace theme;

//==============================================================================
PadButton::PadButton (int index, SAWTAudioProcessor& p) : pad (index), processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PadButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.5f);

    const auto flashV = processor.drumEngine().padFlash[(size_t) pad].load (std::memory_order_relaxed);
    flash = juce::jmax (flash * 0.88f, flashV);

    const auto base = selected ? c (goldDim) : c (panel);
    g.setColour (base);
    g.fillRoundedRectangle (b, 6.0f);
    if (flash > 0.005f)
    {
        g.setColour (c (gold).withAlpha (0.55f * flash));
        g.fillRoundedRectangle (b, 6.0f);
    }
    g.setColour (selected ? c (gold) : c (line));
    g.drawRoundedRectangle (b, 6.0f, selected ? 1.6f : 1.0f);

    g.setColour (c (ink));
    g.setFont (font (13.0f));
    g.drawText (padNames[pad], b.reduced (4.0f), juce::Justification::centredTop, 1);

    g.setColour (c (inkDim));
    g.setFont (font (9.0f));
    g.drawText (juce::String ("MIDI ") + juce::String (drumMidiBase + pad),
                b.reduced (4.0f).removeFromBottom (12.0f), juce::Justification::centred, 1);
}

void PadButton::mouseDown (const juce::MouseEvent&)
{
    processor.triggerPad (pad, 1.0f);
    if (onSelected) onSelected (pad);
}

//==============================================================================
DrumPanel::DrumPanel (SAWTAudioProcessor& proc, juce::AudioProcessorValueTreeState& s)
    : processor (proc), state (s)
{
    for (int i = 0; i < 8; ++i)
    {
        pads[(size_t) i] = std::make_unique<PadButton> (i, proc);
        pads[(size_t) i]->onSelected = [this] (int p) { selectPad (p); };
        addAndMakeVisible (*pads[(size_t) i]);
    }
    pads[0]->setSelected (true);

    padTitle.setText ("PAD 1 - KICK", juce::dontSendNotification);
    padTitle.setFont (font (11.5f));
    padTitle.setColour (juce::Label::textColourId, c (ink));
    addAndMakeVisible (padTitle);

    hint.setText ("LOAD A VOCAL IN THE DRUM SLOT, THEN PRESS [AUTO SLICE] - PADS ALSO PLAY ON MIDI CH 10",
                  juce::dontSendNotification);
    hint.setFont (font (9.0f));
    hint.setColour (juce::Label::textColourId, c (inkDim));
    addAndMakeVisible (hint);

    startTimerHz (30);
    selectPad (0);
}

void DrumPanel::selectPad (int i)
{
    selectedPad = i;
    for (int k = 0; k < 8; ++k)
        pads[(size_t) k]->setSelected (k == i);
    padTitle.setText (juce::String ("PAD ") + juce::String (i + 1) + " - " + padNames[i],
                      juce::dontSendNotification);

    // rebuild the per-pad macro knobs for the new selection
    const juce::String idx = juce::String (i);
    padKnobs[(size_t) 0] = std::make_unique<Knob> (state, juce::String (pid::dPadTune) + idx, "TUNE");
    padKnobs[(size_t) 1] = std::make_unique<Knob> (state, juce::String (pid::dPadDec)  + idx, "DECAY");
    padKnobs[(size_t) 2] = std::make_unique<Knob> (state, juce::String (pid::dPadLvl)  + idx, "LEVEL");
    for (auto& k : padKnobs) addAndMakeVisible (*k);
    resized();
    repaint();
}

void DrumPanel::timerCallback()
{
    const int sc = processor.drumEngine().sliceCount.load (std::memory_order_relaxed);
    if (sc != lastSliceCount) { lastSliceCount = sc; repaint(); }
    for (auto& p : pads) p->repaint();
}

void DrumPanel::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void DrumPanel::resized()
{
    auto b = getLocalBounds();
    hint.setBounds (b.removeFromBottom (14).reduced (6, 0));

    // pads 4 x 2
    auto grid = b.removeFromTop (juce::jmin (210, b.getHeight()));
    const int padW = grid.getWidth() / 4;
    const int padH = grid.getHeight() / 2;
    for (int i = 0; i < 8; ++i)
        pads[(size_t) i]->setBounds (grid.getX() + (i % 4) * padW,
                                     grid.getY() + (i / 4) * padH,
                                     padW, padH);

    // selected pad strip
    auto strip = b.removeFromTop (juce::jmin (96, b.getHeight()));
    padTitle.setBounds (strip.removeFromTop (18).reduced (6, 0));
    const int kw = juce::jmin (90, strip.getWidth() / 3);
    for (int k = 0; k < 3; ++k)
    {
        const int x = strip.getX() + strip.getWidth() / 2 - (3 * kw) / 2 + k * kw;
        if (padKnobs[(size_t) k] != nullptr)
            padKnobs[(size_t) k]->setBounds (x, strip.getY(), kw, strip.getHeight());
    }
}

} // namespace sawt
