#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// Sora Kick - процессор для бочки (индустриал / хард-техно / rawstyle).
//
// Цепочка:
//   Transient shaper (Attack / Sustain) -> Length gate
//   + Sub Layer (синус/треугольник/пила/квадрат с питч-дропом, стартует от атаки)
//   + Rumble (параллельный перегруженный низ)
//   -> Sub EQ -> Drive (4x oversampling) -> DC block
//   -> Crush -> Screech EQ -> Click EQ -> Low/High cut -> Mix/Output
//==============================================================================
class SoraKickAudioProcessor : public juce::AudioProcessor,
                               private juce::AudioProcessorValueTreeState::Listener,
                               private juce::AsyncUpdater
{
public:
    SoraKickAudioProcessor();
    ~SoraKickAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ClannadKI"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::StringArray getCharacterNames();
    static juce::StringArray getDriveTypeNames();
    static juce::StringArray getWaveNames();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    void processChunk (juce::AudioBuffer<float>&);
    void updateFilters();

    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;

    Filter subEQ, clickEQ, screechEQ, lowCutF, highCutF, dcBlock;

    juce::dsp::Oversampling<float> oversampling { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 256 };
    juce::AudioBuffer<float> dryBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSm, outSm;

    // параметры (атомики из APVTS)
    std::atomic<float> *pCharacter = nullptr, *pDriveType = nullptr, *pLayerWave = nullptr,
                       *pDrive = nullptr, *pAttack = nullptr, *pSustain = nullptr,
                       *pSub = nullptr, *pClick = nullptr, *pBits = nullptr, *pRate = nullptr,
                       *pLowCut = nullptr, *pHighCut = nullptr, *pLayer = nullptr,
                       *pLayerFreq = nullptr, *pLayerDecay = nullptr, *pLayerDrop = nullptr,
                       *pDropTime = nullptr, *pTrigger = nullptr, *pRumble = nullptr,
                       *pRumbleFreq = nullptr, *pLength = nullptr, *pScreech = nullptr,
                       *pScreechFreq = nullptr, *pMix = nullptr, *pOutput = nullptr;

    // пресеты ("Character")
    std::atomic<int> pendingCharacter { 1 };
    int appliedCharacter = 1;

    // состояние DSP
    double sr = 44100.0;
    int maxBlock = 512;
    int latency = 0;

    float lastSub = -9999.f, lastClick = -9999.f, lastLow = -9999.f, lastHigh = -9999.f,
          lastScreech = -9999.f, lastScreechFreq = -9999.f;

    float envFast = 0.f, envSlow = 0.f, tGain = 1.f;

    bool armed = true;
    bool gateSeen = false;
    juce::int64 sinceTrigger = 0;
    float gateGain = 1.f;

    double layerPhase = 0.0;
    float layerAmp = 0.f, pitchEnv = 0.f;

    float rLp1[2] { 0.f, 0.f }, rLp2[2] { 0.f, 0.f };

    float held[2] { 0.f, 0.f };
    int holdCount[2] { 0, 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoraKickAudioProcessor)
};
