#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour background { 0xff0b0b0d };
    const juce::Colour panel { 0xff17171b };
    const juce::Colour panelEdge { 0xff2c2c33 };
    const juce::Colour red { 0xffe01e26 };
    const juce::Colour darkRed { 0xff8f1218 };
    const juce::Colour textMain { 0xfff2f2f2 };
    const juce::Colour textDim { 0xff9a9aa3 };
}

//==============================================================================
RadioReadyLookAndFeel::RadioReadyLookAndFeel()
{
    setColour (juce::Slider::rotarySliderFillColourId, Colours::red);
    setColour (juce::Slider::rotarySliderOutlineColourId, Colours::panelEdge);
    setColour (juce::Slider::thumbColourId, Colours::red);
    setColour (juce::Slider::trackColourId, Colours::red);
    setColour (juce::Slider::backgroundColourId, Colours::darkRed);
    setColour (juce::ToggleButton::tickColourId, Colours::red);
    setColour (juce::ToggleButton::textColourId, Colours::textMain);
    setColour (juce::Label::textColourId, Colours::textMain);
}

void RadioReadyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                              int width, int height,
                                              float sliderPos, float startAngle,
                                              float endAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y,
                                          (float) width, (float) height)
                      .reduced (6.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();
    auto angle = startAngle + sliderPos * (endAngle - startAngle);

    // Background arc track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         startAngle, endAngle, true);
    g.setColour (Colours::panelEdge);
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc (red)
    if (sliderPos > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                startAngle, angle, true);
        g.setColour (Colours::red);
        g.strokePath (valueArc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // Knob body
    auto knobRadius = radius - 8.0f;
    juce::Rectangle<float> knobBounds (centre.x - knobRadius, centre.y - knobRadius,
                                       knobRadius * 2.0f, knobRadius * 2.0f);
    g.setColour (juce::Colour (0xff1d1d22));
    g.fillEllipse (knobBounds);
    g.setColour (Colours::panelEdge);
    g.drawEllipse (knobBounds, 1.5f);

    // Pointer line
    juce::Path pointer;
    const float pointerLength = knobRadius * 0.75f;
    const float pointerThickness = 3.0f;
    pointer.addRectangle (-pointerThickness * 0.5f, -knobRadius,
                          pointerThickness, pointerLength);
    pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                .translated (centre.x, centre.y));
    g.setColour (Colours::textMain);
    g.fillPath (pointer);
}

//==============================================================================
void MeterPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (Colours::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (Colours::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    // Pull fresh values and apply visual fall-off.
    for (int ch = 0; ch < 2; ++ch)
    {
        displayIn[ch] = juce::jmax (processor.getInputLevel (ch), displayIn[ch] * 0.82f);
        displayOut[ch] = juce::jmax (processor.getOutputLevel (ch), displayOut[ch] * 0.82f);
    }
    displayGr = juce::jmax (processor.getGainReductionDb(), displayGr * 0.88f);

    auto drawBar = [&] (float x, float width, float level, const juce::String& label)
    {
        const float top = 34.0f;
        const float bottom = bounds.getHeight() - 26.0f;
        const float height = bottom - top;

        g.setColour (juce::Colour (0xff232329));
        g.fillRoundedRectangle (x, top, width, height, 3.0f);

        // Level mapped from -48..0 dBFS.
        const float db = juce::Decibels::gainToDecibels (juce::jmax (level, 1.0e-5f), -48.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        const float fillHeight = frac * height;

        if (fillHeight > 1.0f)
        {
            g.setColour (db > -3.0f ? Colours::red : juce::Colour (0xffc9c9cf));
            g.fillRoundedRectangle (x, bottom - fillHeight, width, fillHeight, 3.0f);
        }

        g.setColour (Colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (label, (int) x - 6, (int) bottom + 4, (int) width + 12, 16,
                    juce::Justification::centred);
    };

    const float barWidth = 16.0f;
    const float startX = bounds.getCentreX() - barWidth * 2.5f - 12.0f;
    drawBar (startX, barWidth, displayIn[0], "IN L");
    drawBar (startX + barWidth + 6.0f, barWidth, displayIn[1], "IN R");
    drawBar (startX + (barWidth + 6.0f) * 2.0f, barWidth, displayOut[0], "OUT L");
    drawBar (startX + (barWidth + 6.0f) * 3.0f, barWidth, displayOut[1], "OUT R");

    g.setColour (Colours::textMain);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText ("LEVELS", bounds.withHeight (26.0f), juce::Justification::centred);

    // Gain reduction readout under the bars.
    g.setColour (Colours::red);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("GR -" + juce::String (displayGr, 1) + " dB",
                bounds.withTop (bounds.getHeight() - 52.0f).withHeight (20.0f),
                juce::Justification::centred);
}

//==============================================================================
RadioReadyEditor::RadioReadyEditor (RadioReadyProcessor& p)
    : AudioProcessorEditor (&p), processor (p), meterPanel (p)
{
    setLookAndFeel (&lookAndFeel);

    auto setupKnob = [this] (juce::Slider& slider, juce::Label& label,
                             const juce::String& name)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
        slider.setColour (juce::Slider::textBoxTextColourId, Colours::textMain);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, Colours::textMain);
        addAndMakeVisible (label);
    };

    setupKnob (cleanSlider, cleanLabel, "CLEAN");
    setupKnob (punchSlider, punchLabel, "PUNCH");
    setupKnob (airSlider, airLabel, "AIR");
    setupKnob (loudSlider, loudLabel, "LOUD");

    auto setupTrim = [this] (juce::Slider& slider, juce::Label& label,
                             const juce::String& name)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        slider.setColour (juce::Slider::textBoxTextColourId, Colours::textMain);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (slider);

        label.setText (name, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, Colours::textDim);
        addAndMakeVisible (label);
    };

    setupTrim (inputSlider, inputLabel, "INPUT dB");
    setupTrim (outputSlider, outputLabel, "OUTPUT dB");

    bypassButton.setColour (juce::ToggleButton::textColourId, Colours::textMain);
    addAndMakeVisible (bypassButton);

    // Listen (Auto) mode: big toggle, status indicator, live readouts.
    listenButton.setColour (juce::ToggleButton::textColourId, Colours::textMain);
    listenButton.setColour (juce::ToggleButton::tickColourId, Colours::red);
    addAndMakeVisible (listenButton);

    listenIndicator.setText ("AUTO OFF", juce::dontSendNotification);
    listenIndicator.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    listenIndicator.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (listenIndicator);

    readoutLabel.setText ("Play your mix - Listen hears level and balance here.",
                          juce::dontSendNotification);
    readoutLabel.setFont (juce::Font (juce::FontOptions (11.5f)));
    readoutLabel.setColour (juce::Label::textColourId, Colours::textDim);
    addAndMakeVisible (readoutLabel);

    addAndMakeVisible (meterPanel);

    auto& state = processor.getValueTreeState();
    cleanAttachment = std::make_unique<SliderAttachment> (state, "clean", cleanSlider);
    punchAttachment = std::make_unique<SliderAttachment> (state, "punch", punchSlider);
    airAttachment = std::make_unique<SliderAttachment> (state, "air", airSlider);
    loudAttachment = std::make_unique<SliderAttachment> (state, "loud", loudSlider);
    inputAttachment = std::make_unique<SliderAttachment> (state, "inputGain", inputSlider);
    outputAttachment = std::make_unique<SliderAttachment> (state, "outputGain", outputSlider);
    bypassAttachment = std::make_unique<ButtonAttachment> (state, "bypass", bypassButton);
    listenAttachment = std::make_unique<ButtonAttachment> (state, "listen", listenButton);

    setSize (700, 478);
    startTimerHz (30);
}

RadioReadyEditor::~RadioReadyEditor()
{
    setLookAndFeel (nullptr);
}

void RadioReadyEditor::timerCallback()
{
    meterPanel.repaint();

    const bool listening = processor.isListening();
    const juce::String indicatorText = listening ? "LISTENING" : "AUTO OFF";
    if (listenIndicator.getText() != indicatorText)
    {
        listenIndicator.setText (indicatorText, juce::dontSendNotification);
        listenIndicator.setColour (juce::Label::textColourId,
                                   listening ? Colours::red : Colours::textDim);
    }

    auto signedStr = [] (float v, int decimals)
    {
        return (v >= 0.0f ? "+" : "") + juce::String (v, decimals);
    };

    juce::String text;
    text << "HEARD " << juce::String (processor.getDetectedLoudnessDb(), 1) << " dB RMS"
         << "  |  MUD " << signedStr (processor.getMudRelativeDb(), 1)
         << "  PRES " << signedStr (processor.getPresenceRelativeDb(), 1)
         << "  AIR " << signedStr (processor.getAirRelativeDb(), 1)
         << "  |  AUTO: CLEAN " << signedStr (processor.getAutoCleanPoints(), 0)
         << "  AIR " << signedStr (processor.getAutoAirPoints(), 0)
         << "  LOUD " << signedStr (processor.getAutoLoudPoints(), 0);

    if (text != lastReadoutText)
    {
        lastReadoutText = text;
        readoutLabel.setText (text, juce::dontSendNotification);
    }
}

void RadioReadyEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    // Red top strip
    g.setColour (Colours::red);
    g.fillRect (0, 0, getWidth(), 4);

    // Title
    g.setColour (Colours::textMain);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText ("Gutta Radio Ready", 24, 16, 500, 36, juce::Justification::left);

    g.setColour (Colours::red);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("DGUTTA912  •  RADIO-READY MASTER BUS  •  CEILING -1.0 dB",
                24, 52, 560, 20, juce::Justification::left);

    // Knob panel
    auto knobPanel = juce::Rectangle<int> (16, 84, 540, 226).toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (knobPanel, 10.0f);
    g.setColour (Colours::panelEdge);
    g.drawRoundedRectangle (knobPanel.reduced (0.5f), 10.0f, 1.0f);

    // Listen panel (red edge so the auto mode stands out)
    auto listenPanel = juce::Rectangle<int> (16, 318, 540, 84).toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (listenPanel, 10.0f);
    g.setColour (Colours::darkRed);
    g.drawRoundedRectangle (listenPanel.reduced (0.5f), 10.0f, 1.5f);
}

void RadioReadyEditor::resized()
{
    const int knobSize = 118;
    const int knobY = 108;
    const int labelHeight = 20;
    const int firstX = 34;
    const int spacing = 132;

    juce::Slider* knobs[4] = { &cleanSlider, &punchSlider, &airSlider, &loudSlider };
    juce::Label* labels[4] = { &cleanLabel, &punchLabel, &airLabel, &loudLabel };

    for (int i = 0; i < 4; ++i)
    {
        const int x = firstX + i * spacing;
        labels[i]->setBounds (x, knobY, knobSize, labelHeight);
        knobs[i]->setBounds (x, knobY + labelHeight, knobSize, knobSize + 22);
    }

    // Listen row: toggle + indicator, then the live readout line.
    listenButton.setBounds (28, 326, 210, 36);
    listenIndicator.setBounds (248, 326, 180, 36);
    readoutLabel.setBounds (28, 368, 516, 20);

    // Trim sliders + bypass, bottom left
    inputLabel.setBounds (24, 416, 76, 20);
    inputSlider.setBounds (104, 416, 300, 20);
    outputLabel.setBounds (24, 444, 76, 20);
    outputSlider.setBounds (104, 444, 300, 20);
    bypassButton.setBounds (424, 424, 110, 32);

    // Meters, right column
    meterPanel.setBounds (572, 84, 112, 380);
}
