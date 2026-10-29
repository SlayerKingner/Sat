#pragma once
#include "PluginProcessor.h"

//==============================================================================
// ลุคของปุ่มหมุน/ปุ่มกด (วาดเองทั้งหมด ไม่ใช้ภาพจากผลิตภัณฑ์ใด)
class EduLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EduLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};

//==============================================================================
class VUMeter : public juce::Component
{
public:
    void setValue (float db) { target = db; }
    void tick();
    void paint (juce::Graphics&) override;
private:
    float target = -20.0f, shown = -20.0f;
};

//==============================================================================
class EduSaturatorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EduSaturatorEditor (EduSaturatorProcessor&);
    ~EduSaturatorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setStyleButtons (int index);

    EduSaturatorProcessor& proc;
    EduLookAndFeel laf;

    juce::Slider drive, lowCut, tone, highCut, output, mix;
    juce::ToggleButton punish, thump, steep, autoGain;
    juce::TextButton styleBtn[5];
    VUMeter vu;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> aDrive, aLow, aTone, aHigh, aOut, aMix;
    std::unique_ptr<BA> aPunish, aThump, aSteep, aAuto;
    std::unique_ptr<juce::ParameterAttachment> aStyle;

    juce::String styleText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EduSaturatorEditor)
};
