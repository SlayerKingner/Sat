#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace ParamID
{
    inline constexpr const char* style   = "style";    // A E N T P
    inline constexpr const char* drive   = "drive";    // 0..10
    inline constexpr const char* punish  = "punish";   // +20 dB
    inline constexpr const char* thump   = "thump";
    inline constexpr const char* lowCut  = "lowcut";   // 20 Hz..1 kHz
    inline constexpr const char* tone    = "tone";     // dark..bright
    inline constexpr const char* highCut = "highcut";  // 1 kHz..20 kHz
    inline constexpr const char* steep   = "steep";
    inline constexpr const char* output  = "output";   // -24..0 dB
    inline constexpr const char* autoGain= "auto";
    inline constexpr const char* mix     = "mix";      // dry..wet
}

class EduSaturatorProcessor : public juce::AudioProcessor
{
public:
    EduSaturatorProcessor();
    ~EduSaturatorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Edu Saturator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // อ่านโดย GUI
    std::atomic<float> attitudeDb   { -30.0f }; // ค่าที่เข็ม VU แสดง
    std::atomic<float> autoOutputDb { 0.0f };   // ค่า Output ที่ Auto-Gain คำนวณให้

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    static constexpr int maxCh = 2;

    // ---- Pre-saturation (ที่ sample rate ปกติ) ----
    std::array<Filter, maxCh> lowCutF, thumpF, toneLowF, toneHighF, styleEqPreF1, styleEqPreF2;
    // ---- Post-saturation ----
    std::array<Filter, maxCh> hcOnePole, hcBiq1, hcBiq2;   // High Cut (6 dB/oct หรือ 30 dB/oct)
    std::array<Filter, maxCh> styleEqPostF;

    juce::dsp::Oversampling<float> oversampler { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };

    juce::AudioBuffer<float> dryBuffer, wetBuffer;

    // DC blocker (หลัง saturation ที่ oversampled rate)
    std::array<float, maxCh> dcX1 { 0, 0 }, dcY1 { 0, 0 };
    float dcR = 0.999f;

    double sr = 44100.0;
    float prevDriveGain = 1.0f, prevOutGain = 1.0f, prevMix = 1.0f;
    float meterMs = 0.0f;

    // cache ค่าพารามิเตอร์ เพื่ออัปเดต coefficients เฉพาะเมื่อเปลี่ยน (ไม่ต้อง allocate ทุก block)
    float cLow = -1, cTone = -99, cHigh = -1; int cStyle = -1; bool cThump = false, cSteep = false, cInit = false;

    void updateFilters (float lowHz, bool thump, float tone, float highHz, bool steep, int style);
    float saturate (float x, int style) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EduSaturatorProcessor)
};
