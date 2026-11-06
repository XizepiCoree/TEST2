#pragma once

#include "PluginProcessor.h"

// Шрифт интерфейса в духе диалоговых окон визуальных новелл (Tahoma есть в Windows)
inline juce::Font vnFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions ("Tahoma", height, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
class SoraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SoraLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

//==============================================================================
class SoraKickAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit SoraKickAudioProcessorEditor (SoraKickAudioProcessor&);
    ~SoraKickAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void buildScenes();
    void addKnob (const char* paramID, const char* title);

    SoraKickAudioProcessor& proc;
    SoraLookAndFeel laf;

    // фоны (по одному на каждый пункт Character) и плавная смена между ними
    std::vector<juce::Image> scenes;
    juce::Image sprite;
    int shownScene = 0, prevScene = 0;
    float fade = 1.f;
    int blinkTicks = 0;

    juce::ComboBox characterBox, driveTypeBox, layerWaveBox;
    std::vector<std::unique_ptr<Knob>> knobs;

    std::unique_ptr<APVTS::ComboBoxAttachment> characterAtt, driveTypeAtt, layerWaveAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoraKickAudioProcessorEditor)
};
