/*
  SAWT - WaveformView.cpp
*/
#include "WaveformView.h"
#include "Theme.h"

namespace sawt
{
using namespace theme;

void WaveformView::setSample (const SamplePtr& s)
{
    sample = s;
    lastWidth = 0;
    rebuildPeaks();
    repaint();
}

void WaveformView::setSliceMarkers (const std::vector<std::pair<float, float>>& ns)
{
    slices = ns;
    repaint();
}

void WaveformView::rebuildPeaks()
{
    peaks.clear();
    if (sample == nullptr || ! sample->isValid()) return;

    const int w = juce::jmax (getWidth(), 400);
    const int n = sample->numSamples();
    const auto& src = sample->mono.getNumSamples() > 0 ? sample->mono : sample->buffer;
    const float* x = src.getReadPointer (0);

    peaks.resize ((size_t) w, { 0.0f, 0.0f });
    const double step = (double) n / (double) w;
    for (int px = 0; px < w; ++px)
    {
        const int i0 = (int) ((double) px * step);
        const int i1 = (int) juce::jmin ((double) n, ((double) px + 1.0) * step);
        float mn = 0.0f, mx = 0.0f;
        for (int i = i0; i < i1; ++i)
        {
            mn = juce::jmin (mn, x[i]);
            mx = juce::jmax (mx, x[i]);
        }
        peaks[(size_t) px] = { mn, mx };
    }
    lastWidth = w;
}

void WaveformView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (c (panel).withAlpha (0.6f));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (c (line).withAlpha (0.5f));
    g.drawRoundedRectangle (b, 5.0f, 1.0f);

    // centre line
    g.setColour (c (line).withAlpha (0.35f));
    g.drawHorizontalLine ((int) b.getCentreY(), b.getX(), b.getRight());

    if (getWidth() != lastWidth && sample != nullptr && sample->isValid())
        rebuildPeaks();

    if (peaks.empty() || sample == nullptr)
    {
        g.setColour (c (inkDim).withAlpha (0.5f));
        g.setFont (font (11.0f));
        g.drawText ("no sample loaded", b, juce::Justification::centred);
        return;
    }

    const int w = (int) peaks.size();
    const float midY = b.getCentreY();
    const float amp = b.getHeight() * 0.42f;

    // waveform
    juce::Path wave;
    for (int px = 0; px < juce::jmin (w, (int) b.getWidth()); ++px)
    {
        const auto& p = peaks[(size_t) px];
        const float xx = b.getX() + (float) px;
        wave.addRectangle (juce::Rectangle<float> (xx, midY - p.max * amp,
                                                   1.0f, juce::jmax (1.0f, (p.max - p.min) * amp)));
    }
    g.setColour (c (ink).withAlpha (0.85f));
    g.fillPath (wave);

    // slice markers
    g.setColour (c (emerald).withAlpha (0.9f));
    for (const auto& s : slices)
    {
        const float sx = b.getX() + s.first * b.getWidth();
        const float ex = b.getX() + s.second * b.getWidth();
        g.drawLine (sx, b.getY(), sx, b.getBottom(), 1.2f);
        g.drawLine (ex, b.getY(), ex, b.getBottom(), 0.6f);
    }

    // playhead
    g.setColour (c (gold));
    const float phx = b.getX() + playhead * b.getWidth();
    g.drawLine (phx, b.getY(), phx, b.getBottom(), 1.4f);
}

} // namespace sawt
