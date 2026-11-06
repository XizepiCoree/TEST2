#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Пресеты "Character". Выбор персонажа выставляет все параметры ниже,
// после чего их можно подкрутить вручную.
namespace
{
    struct Preset
    {
        const char* name;
        float drive;       // %
        int   type;        // 0 Soft, 1 Hard, 2 Fold, 3 Asym
        float attack;      // %
        float sustain;     // %
        float sub;         // dB
        float click;       // dB
        float bits;
        float rate;
        float lowCut;      // Hz
        float highCut;     // Hz
        float layer;       // %
        float layerFreq;   // Hz
        float layerDecay;  // ms
        int   wave;        // 0 Sine, 1 Tri, 2 Saw, 3 Square
        float drop;        // octaves
        float dropTime;    // ms
        float rumble;      // %
        float rumbleFreq;  // Hz
        float length;      // ms (1000 = выкл.)
        float screech;     // dB
        float screechFreq; // Hz
    };

    const Preset presets[] =
    {
        // name            drv typ atk  sus  sub clk bit rt  lc   hc     lyr frq dec  wv drop dt  rmb rfq  len  scr  sfq
        { "Sky Punch",      25, 0,  40, -20,  3,  3, 16, 1,  30, 14000,  35, 52, 200, 0, 1.6f, 25,   0, 150, 1000, 0, 1500 },
        { "Hard Stomp",     80, 1,  70, -60,  2,  7, 16, 1,  35, 16000,  60, 55, 140, 2, 2.5f, 18,  35, 180,  260, 4, 1800 },
        { "Rawstyle",       70, 2,  50,  40,  3,  5, 16, 1,  30, 14000,  70, 48, 380, 1, 3.0f, 30,  70, 150, 1000, 8, 1400 },
        { "Rumble Wall",    60, 3,  20,  55,  6,  2, 16, 1,  25,  9000,  80, 42, 600, 0, 1.2f, 40,  90, 120, 1000, 0, 1000 },
        { "Pitch Bender",   55, 1,  60, -30,  2,  6, 14, 1,  35, 16000,  85, 50, 220, 3, 4.0f, 45,  20, 200, 1000, 3, 2500 },
        { "Crushed 96",     35, 0,  30,  10,  4,  3,  8, 6,  30,  5000,  40, 48, 260, 0, 1.6f, 25,  10, 150, 1000, 0, 1500 },
        { "Rusty Cannon",   55, 3,  40,   0,  2,  5, 10, 3,  35,  6500,  25, 58, 150, 2, 1.8f, 25,  20, 200,  500, 2, 1500 },
        { "Short Click",    90, 1,  90, -90,  0, 10, 16, 1,  60, 18000,  20, 70,  60, 3, 3.0f, 12,   0, 150,  120, 6, 3000 },
    };

    constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));

    inline float follow (float env, float in, float attackCoef, float releaseCoef)
    {
        const float c = in > env ? attackCoef : releaseCoef;
        return in + c * (env - in);
    }

    inline float shape (float x, int type)
    {
        switch (type)
        {
            case 0:  return std::tanh (x);                                            // Soft
            case 1:  return juce::jlimit (-1.0f, 1.0f, x);                            // Hard clip
            case 2:  return (2.0f / juce::MathConstants<float>::pi)                   // Fold
                          * std::asin (std::sin (x * juce::MathConstants<float>::halfPi));
            default: return std::tanh (x + 0.35f) - std::tanh (0.35f);                // Asym
        }
    }

    inline float polyBlep (float t, float dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    inline float wrap01 (float x) { return x - std::floor (x); }

    // форма волны сабового слоя; p = фаза 0..1, dt = приращение фазы
    inline float layerWave (int wave, float p, float dt)
    {
        switch (wave)
        {
            case 0:  return std::sin (juce::MathConstants<float>::twoPi * p);                 // Sine
            case 1:  return 2.0f * std::abs (2.0f * wrap01 (p + 0.25f) - 1.0f) - 1.0f;       // Triangle
            case 2:
            {
                const float q = wrap01 (p + 0.5f);                                            // Saw (старт с нуля)
                return (2.0f * q - 1.0f) - polyBlep (q, dt);
            }
            default:                                                                          // Square
                return (p < 0.5f ? 1.0f : -1.0f) + polyBlep (p, dt) - polyBlep (wrap01 (p + 0.5f), dt);
        }
    }

    constexpr float waveScale[4] = { 1.0f, 1.0f, 0.6f, 0.5f };
}

//==============================================================================
juce::StringArray SoraKickAudioProcessor::getCharacterNames()
{
    juce::StringArray names { "Custom" };
    for (auto& p : presets)
        names.add (p.name);
    return names;
}

juce::StringArray SoraKickAudioProcessor::getDriveTypeNames()
{
    return { "Soft", "Hard", "Fold", "Asym" };
}

juce::StringArray SoraKickAudioProcessor::getWaveNames()
{
    return { "Sine", "Triangle", "Saw", "Square" };
}

juce::AudioProcessorValueTreeState::ParameterLayout SoraKickAudioProcessor::createLayout()
{
    using Range = juce::NormalisableRange<float>;
    const auto& d = presets[0];

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addF = [&p] (const char* id, const char* name, Range range, float def, const char* unit)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def,
            juce::AudioParameterFloatAttributes().withLabel (unit)));
    };

    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "character", 1 }, "Character", getCharacterNames(), 1));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "driveType", 1 }, "Drive Type", getDriveTypeNames(), d.type));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "layerWave", 1 }, "Layer Wave", getWaveNames(), d.wave));

    addF ("attack",      "Attack",       Range (-100.f, 100.f, 1.f),          d.attack,      " %");
    addF ("sustain",     "Sustain",      Range (-100.f, 100.f, 1.f),          d.sustain,     " %");
    addF ("length",      "Length",       Range (50.f, 1000.f, 1.f, 0.5f),     d.length,      " ms");
    addF ("sub",         "Sub",          Range (-12.f, 12.f, 0.1f),           d.sub,         " dB");
    addF ("click",       "Click",        Range (0.f, 12.f, 0.1f),             d.click,       " dB");
    addF ("drive",       "Drive",        Range (0.f, 100.f, 0.1f),            d.drive,       " %");
    addF ("bits",        "Bits",         Range (4.f, 16.f, 1.f),              d.bits,        " bit");
    addF ("rate",        "Rate",         Range (1.f, 32.f, 1.f),              d.rate,        " x");
    addF ("screech",     "Screech",      Range (0.f, 18.f, 0.1f),             d.screech,     " dB");
    addF ("screechFreq", "Screech Freq", Range (600.f, 4000.f, 1.f, 0.5f),    d.screechFreq, " Hz");
    addF ("layer",       "Layer",        Range (0.f, 100.f, 0.1f),            d.layer,       " %");
    addF ("layerFreq",   "Layer Freq",   Range (30.f, 90.f, 0.1f),            d.layerFreq,   " Hz");
    addF ("layerDecay",  "Layer Decay",  Range (60.f, 800.f, 1.f),            d.layerDecay,  " ms");
    addF ("layerDrop",   "Pitch Drop",   Range (0.f, 5.f, 0.01f),             d.drop,        " oct");
    addF ("dropTime",    "Drop Time",    Range (5.f, 150.f, 1.f),             d.dropTime,    " ms");
    addF ("trigger",     "Trigger",      Range (-50.f, -6.f, 0.1f),           -24.f,         " dB");
    addF ("rumble",      "Rumble",       Range (0.f, 100.f, 0.1f),            d.rumble,      " %");
    addF ("rumbleFreq",  "Rumble Freq",  Range (60.f, 400.f, 1.f, 0.6f),      d.rumbleFreq,  " Hz");
    addF ("lowCut",      "Low Cut",      Range (20.f, 300.f, 1.f, 0.4f),      d.lowCut,      " Hz");
    addF ("highCut",     "High Cut",     Range (1000.f, 20000.f, 1.f, 0.35f), d.highCut,     " Hz");
    addF ("mix",         "Mix",          Range (0.f, 100.f, 0.1f),            100.f,         " %");
    addF ("output",      "Output",       Range (-24.f, 12.f, 0.1f),           0.f,           " dB");

    return { p.begin(), p.end() };
}

//==============================================================================
SoraKickAudioProcessor::SoraKickAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pCharacter   = apvts.getRawParameterValue ("character");
    pDriveType   = apvts.getRawParameterValue ("driveType");
    pLayerWave   = apvts.getRawParameterValue ("layerWave");
    pDrive       = apvts.getRawParameterValue ("drive");
    pAttack      = apvts.getRawParameterValue ("attack");
    pSustain     = apvts.getRawParameterValue ("sustain");
    pSub         = apvts.getRawParameterValue ("sub");
    pClick       = apvts.getRawParameterValue ("click");
    pBits        = apvts.getRawParameterValue ("bits");
    pRate        = apvts.getRawParameterValue ("rate");
    pLowCut      = apvts.getRawParameterValue ("lowCut");
    pHighCut     = apvts.getRawParameterValue ("highCut");
    pLayer       = apvts.getRawParameterValue ("layer");
    pLayerFreq   = apvts.getRawParameterValue ("layerFreq");
    pLayerDecay  = apvts.getRawParameterValue ("layerDecay");
    pLayerDrop   = apvts.getRawParameterValue ("layerDrop");
    pDropTime    = apvts.getRawParameterValue ("dropTime");
    pTrigger     = apvts.getRawParameterValue ("trigger");
    pRumble      = apvts.getRawParameterValue ("rumble");
    pRumbleFreq  = apvts.getRawParameterValue ("rumbleFreq");
    pLength      = apvts.getRawParameterValue ("length");
    pScreech     = apvts.getRawParameterValue ("screech");
    pScreechFreq = apvts.getRawParameterValue ("screechFreq");
    pMix         = apvts.getRawParameterValue ("mix");
    pOutput      = apvts.getRawParameterValue ("output");

    apvts.addParameterListener ("character", this);
}

SoraKickAudioProcessor::~SoraKickAudioProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener ("character", this);
}

//==============================================================================
void SoraKickAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "character")
    {
        pendingCharacter = (int) std::round (newValue);
        triggerAsyncUpdate();
    }
}

void SoraKickAudioProcessor::handleAsyncUpdate()
{
    const int idx = pendingCharacter.load();
    if (idx == appliedCharacter)
        return;

    appliedCharacter = idx;
    if (idx <= 0 || idx > numPresets)   // 0 = Custom: ничего не трогаем
        return;

    const auto& p = presets[idx - 1];

    auto set = [this] (const char* id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    };

    set ("driveType",   (float) p.type);
    set ("layerWave",   (float) p.wave);
    set ("drive",       p.drive);
    set ("attack",      p.attack);
    set ("sustain",     p.sustain);
    set ("length",      p.length);
    set ("sub",         p.sub);
    set ("click",       p.click);
    set ("bits",        p.bits);
    set ("rate",        p.rate);
    set ("screech",     p.screech);
    set ("screechFreq", p.screechFreq);
    set ("lowCut",      p.lowCut);
    set ("highCut",     p.highCut);
    set ("layer",       p.layer);
    set ("layerFreq",   p.layerFreq);
    set ("layerDecay",  p.layerDecay);
    set ("layerDrop",   p.drop);
    set ("dropTime",    p.dropTime);
    set ("rumble",      p.rumble);
    set ("rumbleFreq",  p.rumbleFreq);
}

//==============================================================================
bool SoraKickAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SoraKickAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 };

    subEQ.prepare (spec);
    clickEQ.prepare (spec);
    screechEQ.prepare (spec);
    lowCutF.prepare (spec);
    highCutF.prepare (spec);
    dcBlock.prepare (spec);

    *dcBlock.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 12.0f);

    oversampling.initProcessing ((size_t) maxBlock);
    oversampling.reset();
    latency = (int) std::round (oversampling.getLatencyInSamples());
    setLatencySamples (latency);

    dryDelay.setMaximumDelayInSamples (juce::jmax (latency + 8, 64));
    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();
    dryBuffer.setSize (2, maxBlock);

    mixSm.reset (sr, 0.02);
    outSm.reset (sr, 0.02);
    mixSm.setCurrentAndTargetValue (pMix->load() * 0.01f);
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    lastSub = lastClick = lastLow = lastHigh = lastScreech = lastScreechFreq = -9999.f;
    updateFilters();

    envFast = envSlow = 0.f;
    tGain = 1.f;
    armed = true;
    gateSeen = false;
    sinceTrigger = 0;
    gateGain = 1.f;
    layerPhase = 0.0;
    layerAmp = pitchEnv = 0.f;
    rLp1[0] = rLp1[1] = rLp2[0] = rLp2[1] = 0.f;
    held[0] = held[1] = 0.f;
    holdCount[0] = holdCount[1] = 0;
}

void SoraKickAudioProcessor::updateFilters()
{
    using C = juce::dsp::IIR::Coefficients<float>;

    const float subDb       = pSub->load();
    const float clickDb     = pClick->load();
    const float screechDb   = pScreech->load();
    const float screechFreq = pScreechFreq->load();
    const float lc          = pLowCut->load();
    const float hc          = juce::jmin (pHighCut->load(), (float) (0.45 * sr));

    if (subDb != lastSub)
    {
        *subEQ.state = *C::makePeakFilter (sr, 60.0f, 0.8f, juce::Decibels::decibelsToGain (subDb));
        lastSub = subDb;
    }
    if (clickDb != lastClick)
    {
        *clickEQ.state = *C::makePeakFilter (sr, 4000.0f, 0.8f, juce::Decibels::decibelsToGain (clickDb));
        lastClick = clickDb;
    }
    if (screechDb != lastScreech || screechFreq != lastScreechFreq)
    {
        *screechEQ.state = *C::makePeakFilter (sr, screechFreq, 3.0f, juce::Decibels::decibelsToGain (screechDb));
        lastScreech = screechDb;
        lastScreechFreq = screechFreq;
    }
    if (lc != lastLow)
    {
        *lowCutF.state = *C::makeHighPass (sr, lc);
        lastLow = lc;
    }
    if (hc != lastHigh)
    {
        *highCutF.state = *C::makeLowPass (sr, hc);
        lastHigh = hc;
    }
}

//==============================================================================
void SoraKickAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int total = buffer.getNumSamples();
    int pos = 0;

    // на случай, если хост даст блок больше заявленного
    while (pos < total)
    {
        const int len = juce::jmin (maxBlock, total - pos);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), pos, len);
        processChunk (chunk);
        pos += len;
    }
}

void SoraKickAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer)
{
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0)
        return;

    // ---- параметры
    const float attack      = pAttack->load() * 0.01f;
    const float sustain     = pSustain->load() * 0.01f;
    const float driveAmt    = pDrive->load() * 0.01f;
    const int   driveType   = (int) pDriveType->load();
    const int   wave        = juce::jlimit (0, 3, (int) pLayerWave->load());
    const float bits        = pBits->load();
    const int   rateN       = juce::jmax (1, (int) std::round (pRate->load()));
    const float layerLevel  = pLayer->load() * 0.01f * 0.9f * waveScale[wave];
    const float layerFreq   = pLayerFreq->load();
    const float drop        = pLayerDrop->load();
    const float trigLin     = juce::Decibels::decibelsToGain (pTrigger->load());
    const float rumbleLevel = pRumble->load() * 0.01f * 0.8f;
    const float lengthMs    = pLength->load();
    const bool  gateOn      = lengthMs < 999.f;

    mixSm.setTargetValue (pMix->load() * 0.01f);
    outSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    updateFilters();

    const float fs = (float) sr;
    const juce::int64 lengthSamples = (juce::int64) (lengthMs * 0.001f * fs);

    const float fastA      = std::exp (-1.0f / (0.0005f * fs));
    const float fastR      = std::exp (-1.0f / (0.025f  * fs));
    const float slowA      = std::exp (-1.0f / (0.030f  * fs));
    const float slowR      = std::exp (-1.0f / (0.120f  * fs));
    const float gainSm     = std::exp (-1.0f / (0.002f  * fs));
    const float gateCoef   = std::exp (-1.0f / (0.004f  * fs));
    const float ampDecay   = std::exp (-6.9078f / (pLayerDecay->load() * 0.001f * fs));
    const float pitchDecay = std::exp (-1.0f / (pDropTime->load() * 0.001f * fs));
    const float rumbleA    = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * pRumbleFreq->load() / fs);

    float* data[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    // ---- 1) сухой сигнал с компенсацией задержки + transient shaper + gate + sub layer + rumble
    for (int i = 0; i < n; ++i)
    {
        float peak = 0.f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = data[ch][i];
            dryDelay.pushSample (ch, x);
            dryBuffer.setSample (ch, i, dryDelay.popSample (ch));
            peak = juce::jmax (peak, std::abs (x));
        }

        envFast = follow (envFast, peak, fastA, fastR);
        envSlow = follow (envSlow, peak, slowA, slowR);

        // transient shaper
        const float eps = 1.0e-4f;
        const float tr = juce::jlimit (0.f, 1.f, (envFast - envSlow) / (envFast + eps));
        const float su = juce::jlimit (0.f, 1.f, (envSlow - envFast) / (envSlow + eps));
        const float targetGain = juce::Decibels::decibelsToGain (attack * 12.f * tr + sustain * 12.f * su);
        tGain = targetGain + gainSm * (tGain - targetGain);

        // триггер (для сабового слоя и Length gate)
        if (armed && envFast > trigLin)
        {
            armed = false;
            gateSeen = true;
            sinceTrigger = 0;
            layerPhase = 0.0;
            layerAmp = 1.f;
            pitchEnv = 1.f;
        }
        else if (! armed && envFast < trigLin * 0.3f)
        {
            armed = true;
        }

        if (sinceTrigger < 1000000000) ++sinceTrigger;

        const float gateTarget = (gateOn && gateSeen && sinceTrigger > lengthSamples) ? 0.f : 1.f;
        gateGain = gateTarget + gateCoef * (gateGain - gateTarget);

        // сабовый слой с питч-дропом
        float layerSample = 0.f;
        if (layerAmp > 0.0005f)
        {
            const float f  = layerFreq * std::exp2 (drop * pitchEnv);
            const float dt = f / fs;

            layerPhase += (double) dt;
            if (layerPhase >= 1.0)
                layerPhase -= 1.0;

            layerSample = layerWave (wave, (float) layerPhase, dt) * layerAmp * layerLevel;
            layerAmp *= ampDecay;
            pitchEnv *= pitchDecay;
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            float v = (data[ch][i] * tGain + layerSample) * gateGain;

            // rumble: параллельный сильно перегруженный низ
            if (rumbleLevel > 0.0005f)
            {
                const float r = std::tanh (v * 12.f);
                rLp1[ch] += rumbleA * (r - rLp1[ch]);
                rLp2[ch] += rumbleA * (rLp1[ch] - rLp2[ch]);
                v += rLp2[ch] * rumbleLevel;
            }

            data[ch][i] = v;
        }
    }

    juce::dsp::AudioBlock<float> block (buffer);
    auto sub = block.getSubsetChannelBlock (0, (size_t) numCh);
    juce::dsp::ProcessContextReplacing<float> ctx (sub);

    // ---- 2) Sub EQ
    subEQ.process (ctx);

    // ---- 3) Drive (4x oversampling)
    auto up = oversampling.processSamplesUp (sub);
    if (driveAmt > 0.001f)
    {
        const float pre  = juce::Decibels::decibelsToGain (driveAmt * 36.f);
        const float comp = std::pow (pre, -0.25f);

        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* d = up.getChannelPointer (ch);
            for (size_t s = 0; s < up.getNumSamples(); ++s)
                d[s] = shape (d[s] * pre, driveType) * comp;
        }
    }
    oversampling.processSamplesDown (sub);

    dcBlock.process (ctx);

    // ---- 4) Crush (bit depth + sample rate reduction)
    if (bits < 15.99f || rateN > 1)
    {
        const float steps = std::pow (2.f, bits - 1.f);
        for (int ch = 0; ch < numCh; ++ch)
        {
            for (int i = 0; i < n; ++i)
            {
                if (--holdCount[ch] <= 0)
                {
                    held[ch] = bits < 15.99f ? std::round (data[ch][i] * steps) / steps : data[ch][i];
                    holdCount[ch] = rateN;
                }
                data[ch][i] = held[ch];
            }
        }
    }

    // ---- 5) Screech + Click EQ + Low/High cut
    screechEQ.process (ctx);
    clickEQ.process (ctx);
    lowCutF.process (ctx);
    highCutF.process (ctx);

    // ---- 6) Mix + Output
    for (int i = 0; i < n; ++i)
    {
        const float m = mixSm.getNextValue();
        const float o = outSm.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            data[ch][i] = (dryBuffer.getSample (ch, i) * (1.f - m) + data[ch][i] * m) * o;
    }
}

//==============================================================================
juce::AudioProcessorEditor* SoraKickAudioProcessor::createEditor()
{
    return new SoraKickAudioProcessorEditor (*this);
}

void SoraKickAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SoraKickAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

            // загруженный проект не должен перезаписываться пресетом
            appliedCharacter = (int) std::round (pCharacter->load());
            pendingCharacter = appliedCharacter;
            cancelPendingUpdate();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoraKickAudioProcessor();
}
