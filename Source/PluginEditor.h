#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

//==============================================================================
/** Red-and-black look for the rotary knobs. */
class RadioReadyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RadioReadyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

//==============================================================================
/** Small stereo in/out level meters + gain-reduction bar. */
class MeterPanel final : public juce::Component
{
public:
    explicit MeterPanel (RadioReadyProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;

private:
    RadioReadyProcessor& processor;
    float displayIn[2] { 0.0f, 0.0f };
    float displayOut[2] { 0.0f, 0.0f };
    float displayGr = 0.0f;
};

//==============================================================================
class RadioReadyEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit RadioReadyEditor (RadioReadyProcessor&);
    ~RadioReadyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    RadioReadyProcessor& processor;
    RadioReadyLookAndFeel lookAndFeel;

    juce::Slider cleanSlider, punchSlider, airSlider, loudSlider;
    juce::Slider inputSlider, outputSlider;
    juce::Label cleanLabel, punchLabel, airLabel, loudLabel;
    juce::Label inputLabel, outputLabel;
    juce::ToggleButton bypassButton { "BYPASS" };

    // Listen (Auto) mode controls
    juce::ToggleButton listenButton { "LISTEN - AUTO" };
    juce::Label listenIndicator;
    juce::Label readoutLabel;
    juce::String lastReadoutText;

    MeterPanel meterPanel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> cleanAttachment, punchAttachment,
        airAttachment, loudAttachment, inputAttachment, outputAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> listenAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RadioReadyEditor)
};
