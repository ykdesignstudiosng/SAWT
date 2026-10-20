/*
  SAWT - SpectrumView.cpp
*/
#include "SpectrumView.h"
#include "../PluginProcessor.h"
#include "Theme.h"

namespace sawt
{
using namespace theme;

SpectrumView::SpectrumView (SAWTAudioProcessor& p) : processor (p)
{
    startTimerHz (30);
    fifoData.fill (0.0f);
}

void SpectrumView::timerCallback()
{
    // drain everything available (keep up, avoid fifo staleness)
    int start1, size1, start2, size2;
    processor.specFifoCtrl.prepareToRead (4096, start1, size1, start2, size2);
    const int got = size1 + size2;

    if (got > 0)
    {
        const float* src = processor.specFifo.getReadPointer (0);
        for (int i = 0; i < size1; ++i)
        {
            fifoData[(size_t) fifoStart] = src[start1 + i];
            fifoStart = (fifoStart + 1) % 4096;
        }
        for (int i = 0; i < size2; ++i)
        {
            fifoData[(size_t) fifoStart] = src[start2 + i];
            fifoStart = (fifoStart + 1) % 4096;
        }
        fifoValid = juce::jmin (2048, fifoValid + got);
        processor.specFifoCtrl.finishedRead (got);
        computeBands();
    }

    repaint();
}

void SpectrumView::computeBands()
{
    // window the most recent 2048 samples
    for (int i = 0; i < 2048; ++i)
    {
        const int idx = (fifoStart - 2048 + i + 2 * 4096) % 4096;
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / 2047.0f);
        windowed[(size_t) i] = fifoData[(size_t) idx] * w;
    }

    // forward FFT in-place (real signal, packed into two halves)
    std::array<float, 4096> scratch {};
    std::copy (windowed.begin(), windowed.end(), scratch.begin());
    fft.performRealOnlyForwardTransform (scratch.data(), true);

    for (int i = 0; i < 1024; ++i)
        magnitudes[(size_t) i] = std::sqrt (scratch[(size_t) (2 * i)] * scratch[(size_t) (2 * i)]
                                          + scratch[(size_t) (2 * i + 1)] * scratch[(size_t) (2 * i + 1)]);

    // log-spaced band mapping 40 Hz .. 18 kHz
    const double sr = processor.getSampleRate();
    const double fMin = 40.0, fMax = juce::jmin (18000.0, sr * 0.5 - 200.0);
    for (int b = 0; b < numSpectrumBands; ++b)
    {
        const double f0 = fMin * std::pow (fMax / fMin, (double) b / (double) numSpectrumBands);
        const double f1 = fMin * std::pow (fMax / fMin, (double) (b + 1) / (double) numSpectrumBands);
        const int bin0 = juce::jlimit (0, 1023, (int) (f0 / sr * 2048.0));
        const int bin1 = juce::jlimit (bin0 + 1, 1024, (int) (f1 / sr * 2048.0));
        float sum = 0.0f;
        for (int k = bin0; k < bin1; ++k) sum += magnitudes[(size_t) k];
        const float v = juce::jlimit (0.0f, 1.0f,
            std::log10 (1.0f + 40.0f * sum / (float) (bin1 - bin0)) * 0.42f);

        // smooth down fast, decay slow
        bands[(size_t) b] = juce::jmax (v, bands[(size_t) b] * 0.72f);
        peaks[(size_t) b] = juce::jmax (bands[(size_t) b], peaks[(size_t) b] * 0.94f);
    }
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (c (bg));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (c (line).withAlpha (0.5f));
    g.drawRoundedRectangle (b, 5.0f, 1.0f);

    const float bw = b.getWidth() / (float) numSpectrumBands;
    for (int i = 0; i < numSpectrumBands; ++i)
    {
        const float v = juce::jlimit (0.0f, 1.0f, bands[(size_t) i]);
        const float pk = juce::jlimit (0.0f, 1.0f, peaks[(size_t) i]);
        const float x = b.getX() + (float) i * bw + 1.0f;
        const float w = bw - 2.0f;

        if (v > 0.002f)
        {
            const float h = v * (b.getHeight() - 6.0f);
            g.setColour (c (gold).withAlpha (0.25f + 0.55f * v));
            g.fillRoundedRectangle (x, b.getBottom() - 3.0f - h, w, h, 2.0f);
            g.setColour (c (gold).withAlpha (0.9f));
            g.fillRect (x, b.getBottom() - 3.0f - h, w, 1.5f);
        }
        if (pk > 0.01f)
        {
            const float py = b.getBottom() - 3.0f - pk * (b.getHeight() - 6.0f);
            g.setColour (c (emerald).withAlpha (0.85f));
            g.fillRect (x, py, w, 1.2f);
        }
    }
}

} // namespace sawt
