/*
  SAWT - Params.cpp - APVTS layout + snapshot extraction
*/
#include "Params.h"

namespace sawt
{

using namespace juce;

//==============================================================================
static std::unique_ptr<AudioParameterFloat> pFloat (const String& id, const String& name,
                                                    float mn, float mx, float def,
                                                    float skew = 1.0f, const String& unit = {},
                                                    bool logarithmic = false)
{
    NormalisableRange<float> r (mn, mx);
    if (logarithmic) r.setSkewForCentre (skew > 1.0f ? skew : 1000.0f);
    else if (skew != 1.0f) r.skew = skew;
    return std::make_unique<AudioParameterFloat> (id, name, r, def, unit);
}

static std::unique_ptr<AudioParameterBool> pBool (const String& id, const String& name, bool def)
{
    return std::make_unique<AudioParameterBool> (id, name, def);
}

static std::unique_ptr<AudioParameterChoice> pChoice (const String& id, const String& name,
                                                      const StringArray& choices, int def)
{
    return std::make_unique<AudioParameterChoice> (id, name, choices, def);
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout ParamStore::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout L;
    StringArray engines { "Sampler", "Granular", "Formant", "XY Morph" };
    StringArray gModes  { "Scan", "Freeze", "Random" };
    StringArray dSrcs   { "Slice Kit", "Vocal 808s" };

    L.add (pFloat (pid::master, "Master", 0.0f, 2.0f, 0.8f));

    // Melodic
    L.add (pChoice (pid::engine, "Engine", engines, 0));
    L.add (pFloat (pid::tune,   "Tune",   -24.0f, 24.0f, 0.0f, 1.0f, "st"));
    L.add (pFloat (pid::fine,   "Fine",   -100.0f, 100.0f, 0.0f, 1.0f, "ct"));
    L.add (pFloat (pid::glide,  "Glide",  0.0f, 1.0f, 0.05f));
    L.add (pFloat (pid::formant,"Formant", -12.0f, 12.0f, 0.0f, 1.0f, "st"));
    L.add (pFloat (pid::att,    "Attack",  0.001f, 4.0f, 0.01f, 1.0f, "s", true));
    L.add (pFloat (pid::dec,    "Decay",   0.01f, 4.0f, 0.30f, 1.0f, "s", true));
    L.add (pFloat (pid::sus,    "Sustain", 0.0f, 1.0f, 0.70f));
    L.add (pFloat (pid::rel,    "Release", 0.01f, 8.0f, 0.40f, 1.0f, "s", true));
    L.add (pFloat (pid::cutoff, "Cutoff",  20.0f, 20000.0f, 18000.0f, 1000.0f, "Hz", true));
    L.add (pFloat (pid::reso,   "Reso",    0.3f, 10.0f, 0.7f));
    L.add (pFloat (pid::envAmt, "Env->Cut", -1.0f, 1.0f, 0.0f));
    L.add (pFloat (pid::unison, "Unison",  1.0f, 5.0f, 1.0f, 1.0f, "v"));
    L.add (pFloat (pid::detune, "Detune",  0.0f, 50.0f, 12.0f, 1.0f, "ct"));
    L.add (pFloat (pid::spread, "Spread",  0.0f, 1.0f, 0.7f));
    L.add (pFloat (pid::level,  "Level",   0.0f, 2.0f, 0.9f));
    L.add (pFloat (pid::xyX,    "Morph X", 0.0f, 1.0f, 0.35f));
    L.add (pFloat (pid::xyY,    "Morph Y", 0.0f, 1.0f, 0.50f));
    L.add (pFloat (pid::gSize,  "Grain Size", 0.010f, 0.400f, 0.080f, 1.0f, "s", true));
    L.add (pFloat (pid::gRate,  "Grain Rate", 4.0f, 80.0f, 18.0f, 1.0f, "/s"));
    L.add (pFloat (pid::gPos,   "Position", 0.0f, 1.0f, 0.25f));
    L.add (pFloat (pid::gDrift, "Drift",   0.0f, 1.0f, 0.30f));
    L.add (pFloat (pid::gJitter,"Jitter",  0.0f, 1.0f, 0.20f));
    L.add (pChoice (pid::gMode, "Grain Mode", gModes, 0));

    // Drums
    L.add (pChoice (pid::dSource, "Drum Source", dSrcs, 0));
    L.add (pFloat (pid::dPunch, "Punch", 0.0f, 1.0f, 0.35f));
    L.add (pFloat (pid::dDrive, "Drive", 0.0f, 1.0f, 0.20f));
    L.add (pFloat (pid::dTone,  "Tone",  0.0f, 1.0f, 0.50f));
    L.add (pFloat (pid::dLevel, "Drum Level", 0.0f, 2.0f, 0.85f));
    for (int i = 0; i < 8; ++i)
    {
        L.add (pFloat (String (pid::dPadTune) + String (i), "Pad " + String (i + 1) + " Tune", -24.0f, 24.0f, 0.0f, 1.0f, "st"));
        L.add (pFloat (String (pid::dPadDec)  + String (i), "Pad " + String (i + 1) + " Decay", 0.05f, 2.0f, 0.5f, 1.0f, "s", true));
        L.add (pFloat (String (pid::dPadLvl)  + String (i), "Pad " + String (i + 1) + " Level", 0.0f, 1.5f, 0.8f));
    }

    // Live input
    L.add (pFloat (pid::inGain, "Input Gain", 0.0f, 2.0f, 1.0f));
    L.add (pBool  (pid::inMonitor, "Input Monitor", false));
    L.add (pBool  (pid::inSnap, "Snap To Note", false));

    // FX rack
    L.add (pBool  (pid::fxOn, "FX On", true));
    L.add (pFloat (pid::eLow,  "EQ Low",  -12.0f, 12.0f, 0.0f, 1.0f, "dB"));
    L.add (pFloat (pid::eMid,  "EQ Mid",  -12.0f, 12.0f, 0.0f, 1.0f, "dB"));
    L.add (pFloat (pid::eHigh, "EQ High", -12.0f, 12.0f, 0.0f, 1.0f, "dB"));
    L.add (pFloat (pid::eMidFreq, "EQ Freq", 200.0f, 5000.0f, 1200.0f, 1200.0f, "Hz", true));
    L.add (pFloat (pid::cThresh, "Comp Thresh", -48.0f, 0.0f, -18.0f, 1.0f, "dB"));
    L.add (pFloat (pid::cRatio,  "Comp Ratio",  1.0f, 20.0f, 3.0f, 1.0f, ":1"));
    L.add (pFloat (pid::sDrive, "Drive", 0.0f, 1.0f, 0.15f));
    L.add (pFloat (pid::chRate,  "Chorus Rate",  0.05f, 8.0f, 1.2f));
    L.add (pFloat (pid::chDepth, "Chorus Depth", 0.0f, 1.0f, 0.35f));
    L.add (pFloat (pid::chMix,   "Chorus Mix",   0.0f, 1.0f, 0.25f));
    L.add (pFloat (pid::dlTime, "Delay Time", 0.01f, 1.0f, 0.28f));
    L.add (pFloat (pid::dlFb,   "Delay FB",   0.0f, 0.9f, 0.30f));
    L.add (pFloat (pid::dlMix,  "Delay Mix",  0.0f, 1.0f, 0.15f));
    L.add (pFloat (pid::rvSize, "Verb Size", 0.0f, 1.0f, 0.5f));
    L.add (pFloat (pid::rvDamp, "Verb Damp", 0.0f, 1.0f, 0.5f));
    L.add (pFloat (pid::rvMix,  "Verb Mix",  0.0f, 1.0f, 0.2f));
    L.add (pFloat (pid::lfBits, "LoFi Bits", 4.0f, 16.0f, 16.0f, 1.0f, "bit"));
    L.add (pFloat (pid::lfWow,  "Wow",  0.0f, 1.0f, 0.1f));
    L.add (pFloat (pid::lfBreath, "Breath", 0.0f, 1.0f, 0.05f));
    L.add (pFloat (pid::width,  "Width", 0.0f, 2.0f, 1.0f));

    return L;
}

//==============================================================================
void ParamStore::cachePointers()
{
    cache.clear();
    for (auto* p : state.processor.getParameters())
        if (auto* vp = dynamic_cast<juce::RangedAudioParameter*> (p))
            cache[vp->getParameterID()] = state.getRawParameterValue (vp->getParameterID());
}

std::atomic<float>* findCached (const std::map<String, std::atomic<float>*>& c, const char* id)
{
    auto it = c.find (id);
    return it == c.end() ? nullptr : it->second;
}

static float g (const std::map<String, std::atomic<float>*>& c, const char* id, float def = 0.0f)
{
    if (auto* a = findCached (c, id))
        return a->load (std::memory_order_relaxed);
    return def;
}

MelodicParams ParamStore::melodic() const
{
    MelodicParams p;
    p.engine  = (EngineType) (int) g (cache, pid::engine, 0.0f);
    p.tune    = g (cache, pid::tune);
    p.fine    = g (cache, pid::fine);
    p.glide   = g (cache, pid::glide, 0.05f);
    p.formant = g (cache, pid::formant);
    p.att     = g (cache, pid::att, 0.01f);
    p.dec     = g (cache, pid::dec, 0.3f);
    p.sus     = g (cache, pid::sus, 0.7f);
    p.rel     = g (cache, pid::rel, 0.4f);
    p.cutoff  = g (cache, pid::cutoff, 18000.0f);
    p.reso    = g (cache, pid::reso, 0.7f);
    p.envAmt  = g (cache, pid::envAmt);
    p.unison  = (int) juce::jlimit (1.0f, 5.0f, g (cache, pid::unison, 1.0f));
    p.detune  = g (cache, pid::detune, 12.0f);
    p.spread  = g (cache, pid::spread, 0.7f);
    p.level   = g (cache, pid::level, 0.9f);
    p.xyX     = g (cache, pid::xyX, 0.35f);
    p.xyY     = g (cache, pid::xyY, 0.5f);
    p.gSize   = g (cache, pid::gSize, 0.08f);
    p.gRate   = g (cache, pid::gRate, 18.0f);
    p.gPos    = g (cache, pid::gPos, 0.25f);
    p.gDrift  = g (cache, pid::gDrift, 0.3f);
    p.gJitter = g (cache, pid::gJitter, 0.2f);
    p.gMode   = (GrainMode) (int) g (cache, pid::gMode, 0.0f);
    return p;
}

DrumParams ParamStore::drums() const
{
    DrumParams p;
    p.source = (DrumSource) (int) g (cache, pid::dSource, 0.0f);
    p.punch  = g (cache, pid::dPunch, 0.35f);
    p.drive  = g (cache, pid::dDrive, 0.2f);
    p.tone   = g (cache, pid::dTone, 0.5f);
    p.level  = g (cache, pid::dLevel, 0.85f);
    for (int i = 0; i < 8; ++i)
    {
        auto idT = String (pid::dPadTune) + String (i);
        auto idD = String (pid::dPadDec)  + String (i);
        auto idL = String (pid::dPadLvl)  + String (i);
        p.pad[i].tune  = g (cache, idT.toRawUTF8(), 0.0f);
        p.pad[i].decay = g (cache, idD.toRawUTF8(), 0.5f);
        p.pad[i].level = g (cache, idL.toRawUTF8(), 0.8f);
    }
    return p;
}

LiveParams ParamStore::live() const
{
    LiveParams p;
    p.inGain  = g (cache, pid::inGain, 1.0f);
    p.monitor = g (cache, pid::inMonitor) > 0.5f;
    p.snap    = g (cache, pid::inSnap) > 0.5f;
    return p;
}

FXParams ParamStore::fx() const
{
    FXParams p;
    p.on       = g (cache, pid::fxOn) > 0.5f;
    p.eLow     = g (cache, pid::eLow);
    p.eMid     = g (cache, pid::eMid);
    p.eHigh    = g (cache, pid::eHigh);
    p.eMidFreq = g (cache, pid::eMidFreq, 1200.0f);
    p.cThresh  = g (cache, pid::cThresh, -18.0f);
    p.cRatio   = g (cache, pid::cRatio, 3.0f);
    p.sDrive   = g (cache, pid::sDrive, 0.15f);
    p.chRate   = g (cache, pid::chRate, 1.2f);
    p.chDepth  = g (cache, pid::chDepth, 0.35f);
    p.chMix    = g (cache, pid::chMix, 0.25f);
    p.dlTime   = g (cache, pid::dlTime, 0.28f);
    p.dlFb     = g (cache, pid::dlFb, 0.3f);
    p.dlMix    = g (cache, pid::dlMix, 0.15f);
    p.rvSize   = g (cache, pid::rvSize, 0.5f);
    p.rvDamp   = g (cache, pid::rvDamp, 0.5f);
    p.rvMix    = g (cache, pid::rvMix, 0.2f);
    p.lfBits   = g (cache, pid::lfBits, 16.0f);
    p.lfWow    = g (cache, pid::lfWow, 0.1f);
    p.lfBreath = g (cache, pid::lfBreath, 0.05f);
    p.width    = g (cache, pid::width, 1.0f);
    return p;
}

float ParamStore::master() const { return g (cache, pid::master, 0.8f); }

} // namespace sawt
