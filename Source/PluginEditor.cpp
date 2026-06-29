include "PluginEditor.h"

namespace
{
const juce::Colour kBackground { 0xff21162d };
const juce::Colour kPanel { 0xff352140 };
const juce::Colour kPanelDark { 0xff2b1b37 };
const juce::Colour kPink { 0xffff7bbd };
const juce::Colour kCyan { 0xff79e8f2 };
const juce::Colour kCream { 0xfffff2da };
const juce::Colour kMuted { 0xffb79fc1 };

void drawText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
               float fontSize, juce::Colour colour, juce::Justification justification = juce::Justification::centred)
{
    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions { fontSize, juce::Font::bold }));
    g.drawFittedText (text, area, justification, 1);
}
} // namespace

CrystalVoiceAudioProcessorEditor::CrystalVoiceAudioProcessorEditor (CrystalVoiceAudioProcessor& p)
    : AudioProcessorEditor (&p), meter (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setResizable (true, true);
    setResizeLimits (760, 520, 1100, 760);

    factoryPreset.addItemList ({ "Crystal Clear", "Idol Pop", "Soft Ballad", "Pop Correction" }, 1);
    factoryPreset.setSelectedItemIndex (0, juce::dontSendNotification);
    configureCombo (factoryPreset);
    factoryPreset.onChange = [this]
    {
        processor.applyFactoryPreset (factoryPreset.getSelectedItemIndex());
    };

    key.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    scale.addItemList ({ "Major", "Minor" }, 1);
    harmony.addItemList ({ "Off", "3rd above", "3rd below", "Both thirds", "Double" }, 1);
    configureCombo (key);
    configureCombo (scale);
    configureCombo (harmony);

    for (auto* slider : { &tune, &harmonyMix, &deEss, &space, &input, &speed, &air, &compression, &delay, &output })
        configureKnob (*slider);

    const std::array<juce::Component*, 18> components
    {
        &factoryPreset, &key, &scale, &harmony,
        &tune, &harmonyMix, &deEss, &space, &input, &speed, &air,
        &compression, &delay, &output, &details, &savePreset, &loadPreset, &meter
    };

    for (auto* component : components)
        addAndMakeVisible (*component);

    details.setToggleState (false, juce::dontSendNotification);
    details.onClick = [this] { updateDetailVisibility(); };
    configureButton (savePreset);
    configureButton (loadPreset);
    savePreset.onClick = [this] { savePresetToFile(); };
    loadPreset.onClick = [this] { loadPresetFromFile(); };

    auto& state = processor.parameters;
    tuneAttachment       = std::make_unique<SliderAttachment> (state, "tuneAmount", tune);
    harmonyMixAttachment = std::make_unique<SliderAttachment> (state, "harmonyMix", harmonyMix);
    deEssAttachment      = std::make_unique<SliderAttachment> (state, "deEss", deEss);
    spaceAttachment      = std::make_unique<SliderAttachment> (state, "reverb", space);
    inputAttachment      = std::make_unique<SliderAttachment> (state, "inputGain", input);
    speedAttachment      = std::make_unique<SliderAttachment> (state, "retuneMs", speed);
    airAttachment        = std::make_unique<SliderAttachment> (state, "air", air);
    compressionAttachment = std::make_unique<SliderAttachment> (state, "compress", compression);
    delayAttachment      = std::make_unique<SliderAttachment> (state, "delay", delay);
    outputAttachment     = std::make_unique<SliderAttachment> (state, "outputGain", output);
    keyAttachment        = std::make_unique<ComboAttachment> (state, "key", key);
    scaleAttachment      = std::make_unique<ComboAttachment> (state, "scale", scale);
    harmonyAttachment    = std::make_unique<ComboAttachment> (state, "harmonyMode", harmony);

    updateDetailVisibility();
    setSize (880, 570);
    startTimerHz (30);
}

void CrystalVoiceAudioProcessorEditor::configureKnob (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 20);
    slider.setColour (juce::Slider::textBoxTextColourId, kCream);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

void CrystalVoiceAudioProcessorEditor::configureCombo (juce::ComboBox& combo)
{
    combo.setColour (juce::ComboBox::backgroundColourId, kPanelDark);
    combo.setColour (juce::ComboBox::textColourId, kCream);
    combo.setColour (juce::ComboBox::outlineColourId, kCyan.withAlpha (0.65f));
    combo.setColour (juce::ComboBox::arrowColourId, kPink);
    combo.setColour (juce::ComboBox::buttonColourId, kPanelDark);
}

void CrystalVoiceAudioProcessorEditor::configureButton (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId, kPanelDark);
    button.setColour (juce::TextButton::buttonOnColourId, kPink);
    button.setColour (juce::TextButton::textColourOffId, kCream);
    button.setColour (juce::TextButton::textColourOnId, kBackground);
}

void CrystalVoiceAudioProcessorEditor::RetroLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                                             float sliderPosProportional, float rotaryStartAngle,
                                                                             float rotaryEndAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                  static_cast<float> (width), static_cast<float> (height)).reduced (9.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (kPanelDark);
    g.fillEllipse (bounds);
    g.setColour (kCyan.withAlpha (0.75f));
    g.drawEllipse (bounds, 2.0f);

    const auto arcRadius = radius * 0.73f;
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (kPink);
    g.strokePath (arc, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto pointerLength = radius * 0.48f;
    const auto pointerX = centre.x + std::cos (angle - juce::MathConstants<float>::halfPi) * pointerLength;
    const auto pointerY = centre.y + std::sin (angle - juce::MathConstants<float>::halfPi) * pointerLength;
    g.setColour (kCream);
    g.drawLine (centre.x, centre.y, pointerX, pointerY, 3.0f);
    g.fillEllipse (centre.x - 4.0f, centre.y - 4.0f, 8.0f, 8.0f);
}

void CrystalVoiceAudioProcessorEditor::RetroLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool, bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (button.getToggleState() ? kPink : kPanelDark);
    g.fillRoundedRectangle (bounds, 7.0f);
    g.setColour (button.getToggleState() ? kBackground : kCyan.withAlpha (0.8f));
    g.drawRoundedRectangle (bounds, 7.0f, 1.5f);
    drawText (g, button.getButtonText(), button.getLocalBounds(), 12.0f,
              button.getToggleState() ? kBackground : kCream);
}

void CrystalVoiceAudioProcessorEditor::Meter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (kPanelDark);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (kCyan.withAlpha (0.7f));
    g.drawRoundedRectangle (bounds, 12.0f, 1.5f);

    const float inputNorm = juce::jlimit (0.0f, 1.0f, (processor.getInputMeterDb() + 60.0f) / 60.0f);
    const float outputNorm = juce::jlimit (0.0f, 1.0f, (processor.getOutputMeterDb() + 60.0f) / 60.0f);
    const auto meterArea = bounds.reduced (18.0f, 42.0f);
    const float barWidth = (meterArea.getWidth() - 18.0f) * 0.5f;

    const auto drawBar = [&g, &meterArea] (float x, float level, juce::Colour colour)
    {
        juce::Rectangle<float> slot (x, meterArea.getY(), (meterArea.getWidth() - 18.0f) * 0.5f, meterArea.getHeight());
        g.setColour (kBackground);
        g.fillRoundedRectangle (slot, 6.0f);
        auto fill = slot.withTop (slot.getBottom() - slot.getHeight() * level);
        g.setColour (colour);
        g.fillRoundedRectangle (fill, 6.0f);
    };

    drawBar (meterArea.getX(), inputNorm, kCyan);
    drawBar (meterArea.getX() + barWidth + 18.0f, outputNorm, kPink);

    drawText (g, "IN", juce::Rectangle<int> (getWidth() / 2 - 74, 12, 48, 18), 12.0f, kMuted);
    drawText (g, "OUT", juce::Rectangle<int> (getWidth() / 2 + 26, 12, 58, 18), 12.0f, kMuted);
    drawText (g, juce::String (std::round (processor.getCorrectionCents())) + "¢", getLocalBounds().removeFromBottom (24), 12.0f, kCream);
}

void CrystalVoiceAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    auto header = getLocalBounds().removeFromTop (78).toFloat();
    g.setColour (kPanel);
    g.fillRect (header);
    g.setColour (kPink);
    g.fillRect (header.removeFromBottom (4.0f));

    drawText (g, "CRYSTAL VOICE", { 24, 12, 330, 30 }, 24.0f, kCream, juce::Justification::centredLeft);
    drawText (g, "LIVE VOCAL CHAIN  /  PROTOTYPE", { 26, 43, 350, 18 }, 10.5f, kCyan, juce::Justification::centredLeft);

    const auto width = getWidth();
    drawText (g, "PRESET", { width - 485, 16, 80, 18 }, 11.0f, kMuted);

    g.setColour (kPanel.withAlpha (0.72f));
    g.fillRoundedRectangle (20.0f, 92.0f, static_cast<float> (getWidth() - 40), 98.0f, 14.0f);
    g.setColour (kCyan.withAlpha (0.45f));
    g.drawRoundedRectangle (20.0f, 92.0f, static_cast<float> (getWidth() - 40), 98.0f, 14.0f, 1.2f);

    drawText (g, "KEY", { 36, 106, 56, 18 }, 10.5f, kMuted);
    drawText (g, "SCALE", { 154, 106, 68, 18 }, 10.5f, kMuted);
    drawText (g, "HARMONY", { 304, 106, 92, 18 }, 10.5f, kMuted);
    drawText (g, "LOW-LATENCY LIVE MODE", { getWidth() - 257, 112, 215, 18 }, 11.0f, kCyan);
    drawText (g, "Pitch correction + diatonic harmonies", { getWidth() - 308, 140, 316, 18 }, 10.5f, kMuted);

    const auto knobTop = 206;
    const std::array<juce::String, 4> labels { "TUNE", "HARMONY", "DE-ESS", "SPACE" };
    const std::array<int, 4> xPositions { 40, 190, 340, 490 };
    for (size_t i = 0; i < labels.size(); ++i)
        drawText (g, labels[i], { xPositions[i], knobTop - 20, 120, 18 }, 12.0f, kCream);

    drawText (g, "Signal", { getWidth() - 177, 212, 120, 18 }, 12.0f, kCream);
    drawText (g, "Use the four controls above for everyday singing.", { 32, 360, getWidth() - 64, 18 }, 11.0f, kMuted);

    if (details.getToggleState())
    {
        const auto advancedTop = getHeight() - 150;
        g.setColour (kPanel.withAlpha (0.78f));
        g.fillRoundedRectangle (20.0f, static_cast<float> (advancedTop - 12), static_cast<float> (getWidth() - 40), 132.0f, 14.0f);
        g.setColour (kCyan.withAlpha (0.45f));
        g.drawRoundedRectangle (20.0f, static_cast<float> (advancedTop - 12), static_cast<float> (getWidth() - 40), 132.0f, 14.0f, 1.2f);

        const std::array<juce::String, 6> labels { "INPUT", "SPEED", "AIR", "COMP", "DELAY", "OUTPUT" };
        const int usableWidth = getWidth() - 48;
        for (int i = 0; i < 6; ++i)
            drawText (g, labels[static_cast<size_t> (i)], { 28 + i * usableWidth / 6, advancedTop - 5, usableWidth / 6, 16 }, 10.5f, kMuted);
    }
}

void CrystalVoiceAudioProcessorEditor::resized()
{
    const int width = getWidth();
    factoryPreset.setBounds (width - 394, 28, 186, 30);
    savePreset.setBounds (width - 197, 28, 82, 30);
    loadPreset.setBounds (width - 108, 28, 82, 30);

    key.setBounds (36, 128, 100, 34);
    scale.setBounds (154, 128, 124, 34);
    harmony.setBounds (304, 128, 156, 34);
    details.setBounds (width - 230, 150, 112, 26);

    tune.setBounds (40, 220, 120, 126);
    harmonyMix.setBounds (190, 220, 120, 126);
    deEss.setBounds (340, 220, 120, 126);
    space.setBounds (490, 220, 120, 126);
    meter.setBounds (width - 184, 232, 142, 136);

    const int advancedTop = getHeight() - 136;
    const int usableWidth = width - 48;
    const int knobWidth = usableWidth / 6;
    const std::array<juce::Slider*, 6> advanced { &input, &speed, &air, &compression, &delay, &output };
    for (int i = 0; i < 6; ++i)
        advanced[static_cast<size_t> (i)]->setBounds (28 + i * knobWidth, advancedTop + 10, knobWidth, 104);
}

void CrystalVoiceAudioProcessorEditor::timerCallback()
{
    meter.repaint();
}

void CrystalVoiceAudioProcessorEditor::updateDetailVisibility()
{
    const bool visible = details.getToggleState();
    const std::array<juce::Component*, 6> advancedControls
    {
        &input, &speed, &air, &compression, &delay, &output
    };

    for (auto* control : advancedControls)
        control->setVisible (visible);
    repaint();
}

void CrystalVoiceAudioProcessorEditor::savePresetToFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Save Crystal Voice preset",
                                                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                        "*.cvpreset");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
    {
        const auto selected = chooser.getResult();
        if (selected == juce::File {})
            return;

        auto file = selected.withFileExtension ("cvpreset");
        juce::MemoryBlock state;
        processor.getStateInformation (state);
        file.replaceWithData (state.getData(), state.getSize());
    });
}

void CrystalVoiceAudioProcessorEditor::loadPresetFromFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load Crystal Voice preset",
                                                        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                        "*.cvpreset");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        juce::MemoryBlock data;
        if (file.loadFileAsData (data))
            processor.setStateInformation (data.getData(), static_cast<int> (data.getSize()));
    });
}
