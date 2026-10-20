/*
  SAWT - Theme.h - brand palette + shared drawing helpers
  Dark charcoal + ink gold + emerald. Eight-point star as the identity mark.
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace sawt::theme
{
    using juce::uint32;

    constexpr uint32 bg       = 0xff0d0e11;   // near-black
    constexpr uint32 panel    = 0xff16181d;   // raised panel
    constexpr uint32 panelHi  = 0xff1f2229;   // hover
    constexpr uint32 line     = 0xff2a2e36;   // hairlines
    constexpr uint32 ink      = 0xffede7d9;   // warm ivory text
    constexpr uint32 inkDim   = 0xff8a8f9a;   // secondary text
    constexpr uint32 gold     = 0xffd4a937;   // primary accent
    constexpr uint32 goldDim  = 0xff7a6423;
    constexpr uint32 emerald  = 0xff2fa36b;   // secondary accent
    constexpr uint32 red      = 0xffc25b4e;   // capture / warning
    constexpr uint32 shadow   = 0xdd000000;

    inline juce::Colour c (uint32 v) { return juce::Colour (v); }

    // eight-point star mark: two overlapping squares (draw with drawStar)
    inline void drawStar (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col,
                          float lineW)
    {
        juce::Path a, b;
        a.addRectangle (r);
        auto r2 = juce::Rectangle<float> (r.getWidth(), r.getHeight()).withCentre (r.getCentre());
        b.addRectangle (r2);
        b.applyTransform (juce::AffineTransform::rotation (
            juce::MathConstants<float>::pi * 0.25f, r.getCentreX(), r.getCentreY()));
        g.setColour (col);
        juce::PathStrokeType st (lineW, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);
        g.strokePath (a, st);
        g.strokePath (b, st);
    }

    // filled star watermark (two overlapping translucent squares)
    inline void fillStar (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col)
    {
        juce::Path a, b;
        a.addRectangle (r);
        auto r2 = juce::Rectangle<float> (r.getWidth(), r.getHeight()).withCentre (r.getCentre());
        b.addRectangle (r2);
        b.applyTransform (juce::AffineTransform::rotation (
            juce::MathConstants<float>::pi * 0.25f, r.getCentreX(), r.getCentreY()));
        g.setColour (col);
        g.fillPath (a);
        g.fillPath (b);
    }

    inline juce::Font font (float h) { return juce::Font (juce::FontOptions ().withHeight (h)); }

    inline void panelBackground (juce::Graphics& g, juce::Component& comp, juce::Colour fill)
    {
        auto b = comp.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (fill);
        g.fillRoundedRectangle (b, 6.0f);
        g.setColour (c (line).withAlpha (0.6f));
        g.drawRoundedRectangle (b, 6.0f, 1.0f);
    }
}
