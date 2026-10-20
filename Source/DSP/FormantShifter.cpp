/*
  SAWT - FormantShifter.cpp
*/
#include "FormantShifter.h"

namespace sawt
{

void FormantShifter::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = maxBlockSize;
    ring.assign ((size_t) ringMask + 1, 0.0f);
    tmp.assign ((size_t) maxBlockSize, 0.0f);
    ps.prepare (sampleRate, maxBlockSize);
    resamplerLatency = 4 * maxBlockSize;
    reset();
}

void FormantShifter::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writeAbs = 0;
    readAbs = 0.0;
    shift = shiftTarget;
    ps.reset();
    ps.setRatio (1.0f);
}

void FormantShifter::process (const float* in, int n, float* out)
{
    shift += (shiftTarget - shift) * 0.3f;
    // semitones -> resampling speed via 2^(st/12)
    const float speed = std::pow (2.0f, shift / 12.0f);

    // push input
    for (int i = 0; i < n; ++i)
    {
        ring[(size_t) (writeAbs & ringMask)] = in[i];
        ++writeAbs;
    }

    // near-zero shift: straight copy (no chain, no colouration)
    if (std::abs (shift) < 0.02f)
    {
        // keep the read pointer tracking so nothing drifts while inactive
        readAbs = (double) (writeAbs - resamplerLatency);
        std::copy (in, in + n, out);
        return;
    }

    // resampler stage: emit n samples reading at `speed`
    // gentle distance control keeps the read inside the ring
    const double targetDist = (double) resamplerLatency;
    double dist = (double) writeAbs - readAbs;
    readAbs += (double) n * (double) speed * (1.0 + 0.02 * (dist - targetDist) / (double) n);
    dist = (double) writeAbs - readAbs;
    if (dist > 2.0 * targetDist) readAbs = (double) writeAbs - 2.0 * targetDist;
    if (dist < 0.5 * targetDist) readAbs = (double) writeAbs - 0.5 * targetDist;

    for (int i = 0; i < n; ++i)
    {
        const double rp = readAbs + (double) i * (double) speed * (1.0 + 0.02 * (dist - targetDist) / (double) n);
        const long long i0 = (long long) std::floor (rp);
        const float fr = (float) (rp - (double) i0);
        const float s0 = ring[(size_t) (i0 & ringMask)];
        const float s1 = ring[(size_t) ((i0 + 1) & ringMask)];
        const float s2 = ring[(size_t) ((i0 + 2) & ringMask)];
        const float s3 = ring[(size_t) ((i0 + 3) & ringMask)];
        const float a1 = 0.5f * (s1 - s0);
        const float a2 = s0 - 2.5f * s1 + 2.0f * s2 - 0.5f * s3;
        const float a3 = 0.5f * (s3 - s0) + 1.5f * (s1 - s2);
        tmp[(size_t) i] = s1 + fr * (a1 + fr * (a2 + fr * a3));
    }

    // PSOLA stage: bring pitch back by 1/speed, formants stay shifted
    ps.setRatio (1.0f / speed);
    ps.process (tmp.data(), n, out);
}

} // namespace sawt
