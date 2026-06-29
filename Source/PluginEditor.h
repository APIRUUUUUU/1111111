#pragma once

#include "PluginProcessor.h"

class CrystalVoiceAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit CrystalVoiceAudioProcessorEditor (CrystalVoiceAudioProcessor&);
    ~CrystalVoiceAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class RetroLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    } lookAndFeel;

    class Meter final : public juce::Component
    {
    public:
        explicit Meter (CrystalVoiceAudioProcessor& processorToUse) : processor (processorToUse) {}
        void paint (juce::Graphics&) override;

    private:
        CrystalVoiceAudioProcessor& processor;
    } meter;

    void timerCallback() override;
    void configureKnob (juce::Slider& slider);
    void configureCombo (juce::ComboBox& combo);
    void configureButton (juce::TextButton& button);
    void savePresetToFile();
    void loadPresetFromFile();
    void updateDetailVisibility();

    CrystalVoiceAudioProcessor& processor;

    juce::ComboBox factoryPreset;
    juce::ComboBox key;
    juce::ComboBox scale;
    juce::ComboBox harmony;

    juce::Slider tune;
    juce::Slider harmonyMix;
    juce::Slider deEss;
    juce::Slider space;

    juce::Slider input;
    juce::Slider speed;
    juce::Slider air;
    juce::Slider compression;
    juce::Slider delay;
    juce::Slider output;

    juce::ToggleButton details { "DETAIL" };
    juce::TextButton savePreset { "SAVE PRESET" };
    juce::TextButton loadPreset { "LOAD PRESET" };

    std::unique_ptr<juce::FileChooser> fileChooser;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> tuneAttachment;
    std::unique_ptr<SliderAttachment> harmonyMixAttachment;
    std::unique_ptr<SliderAttachment> deEssAttachment;
    std::unique_ptr<SliderAttachment> spaceAttachment;
    std::unique_ptr<SliderAttachment> inputAttachment;
    std::unique_ptr<SliderAttachment> speedAttachment;
    std::unique_ptr<SliderAttachment> airAttachment;
    std::unique_ptr<SliderAttachment> compressionAttachment;
    std::unique_ptr<SliderAttachment> delayAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<ComboAttachment> keyAttachment;
    std::unique_ptr<ComboAttachment> scaleAttachment;
    std::unique_ptr<ComboAttachment> harmonyAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrystalVoiceAudioProcessorEditor)
};
