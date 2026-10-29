#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout EduSaturatorProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::style, 1 }, "Style",
                 StringArray { "A", "E", "N", "T", "P" }, 1));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::drive, 1 }, "Drive",
                 NormalisableRange<float> (0.0f, 10.0f, 0.01f), 3.0f));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::punish, 1 }, "Punish", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::thump, 1 },  "Thump",  false));

    NormalisableRange<float> lowR (20.0f, 1000.0f, 0.0f);
    lowR.setSkewForCentre (std::sqrt (20.0f * 1000.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::lowCut, 1 }, "Low Cut", lowR, 20.0f,
                 AudioParameterFloatAttributes().withLabel ("Hz")));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::tone, 1 }, "Tone",
                 NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    NormalisableRange<float> highR (1000.0f, 20000.0f, 0.0f);
    highR.setSkewForCentre (std::sqrt (1000.0f * 20000.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::highCut, 1 }, "High Cut", highR, 20000.0f,
                 AudioParameterFloatAttributes().withLabel ("Hz")));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::steep, 1 }, "Steep", false));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output",
                 NormalisableRange<float> (-24.0f, 0.0f, 0.01f), 0.0f,
                 AudioParameterFloatAttributes().withLabel ("dB")));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::autoGain, 1 }, "Auto", false));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::mix, 1 }, "Mix",
                 NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    return { p.begin(), p.end() };
}

EduSaturatorProcessor::EduSaturatorProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool EduSaturatorProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

//==============================================================================
void EduSaturatorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int maxBlock = jmax (samplesPerBlock, 512);

    oversampler.initProcessing ((size_t) maxBlock);
    oversampler.reset();

    const int latency = (int) std::lround (oversampler.getLatencyInSamples());
    setLatencySamples (latency);

    dsp::ProcessSpec spec { sampleRate, (uint32) maxBlock, (uint32) maxCh };
    dryDelay.prepare (spec);
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();

    dryBuffer.setSize (maxCh, maxBlock);
    wetBuffer.setSize (maxCh, maxBlock);

    // DC blocker ~ 10 Hz ที่ sample rate x4
    const double osRate = sampleRate * 4.0;
    dcR = (float) (1.0 - (MathConstants<double>::twoPi * 10.0 / osRate));
    dcX1.fill (0.0f); dcY1.fill (0.0f);

    // กำหนดชนิด (อันดับ) ของฟิลเตอร์ให้ตรงตั้งแต่แรก จะได้ไม่ allocate ใน audio thread
    for (int ch = 0; ch < maxCh; ++ch)
    {
        hcOnePole[(size_t) ch].coefficients = Coeffs::makeFirstOrderLowPass (sr, 20000.0f);
        for (auto* f : { &lowCutF[(size_t) ch], &thumpF[(size_t) ch], &toneLowF[(size_t) ch], &toneHighF[(size_t) ch],
                         &styleEqPreF1[(size_t) ch], &styleEqPreF2[(size_t) ch], &hcBiq1[(size_t) ch],
                         &hcBiq2[(size_t) ch], &styleEqPostF[(size_t) ch] })
            f->coefficients = Coeffs::makePeakFilter (sr, 1000.0f, 1.0f, 1.0f);
    }
    cInit = false;
    updateFilters (20.0f, false, 0.0f, 20000.0f, false, 1);

    auto primeAndReset = [] (std::array<Filter, maxCh>& a)
    { for (auto& f : a) { f.processSample (0.0f); f.reset(); } };
    for (auto* a : { &lowCutF, &thumpF, &toneLowF, &toneHighF, &styleEqPreF1, &styleEqPreF2,
                     &hcOnePole, &hcBiq1, &hcBiq2, &styleEqPostF })
        primeAndReset (*a);

    prevDriveGain = prevOutGain = prevMix = -1.0f; // -1 = ต้องเริ่มค่าใหม่ในบล็อกแรก
    meterMs = 0.0f;
}

//==============================================================================
void EduSaturatorProcessor::updateFilters (float lowHz, bool thump, float tone, float highHz, bool steep, int style)
{
    auto setBoth = [] (std::array<Filter, maxCh>& a, Coeffs::Ptr c)
    { a[0].coefficients = c; a[1].coefficients = c; };

    const float nyq = (float) (0.45 * sr);
    const float flat = 1.0f;

    if (! cInit || lowHz != cLow || thump != cThump)
    {
        // Low Cut: high-pass 12 dB/oct ก่อนเข้า saturation
        setBoth (lowCutF, Coeffs::makeHighPass (sr, lowHz, 0.7071f));
        // Thump: ยกเบส "ที่ความถี่ Low Cut" ~ +4 dB (คล้าย head bump ของเทป)
        setBoth (thumpF, Coeffs::makePeakFilter (sr, jmax (lowHz, 30.0f), 1.0f, thump ? Decibels::decibelsToGain (4.5f) : flat));
        cLow = lowHz; cThump = thump;
    }

    if (! cInit || tone != cTone)
    {
        // Tone: tilt EQ นุ่ม ๆ  Dark = เบสขึ้น/แหลมลด,  Bright = เบสลด/แหลมขึ้น (±6 dB)
        const float g = Decibels::decibelsToGain (6.0f * tone);
        setBoth (toneLowF,  Coeffs::makeLowShelf  (sr, 700.0f, 0.5f, 1.0f / g));
        setBoth (toneHighF, Coeffs::makeHighShelf (sr, 700.0f, 0.5f, g));
        cTone = tone;
    }

    if (! cInit || highHz != cHigh || steep != cSteep)
    {
        const float f = jlimit (1000.0f, nyq, highHz);
        if (! steep)
        {
            setBoth (hcOnePole, Coeffs::makeFirstOrderLowPass (sr, f));            // 6 dB/oct
        }
        else
        {
            setBoth (hcOnePole, Coeffs::makeFirstOrderLowPass (sr, f));            // Butterworth อันดับ 5
            setBoth (hcBiq1,    Coeffs::makeLowPass (sr, f, 0.6180f));             // = 30 dB/oct
            setBoth (hcBiq2,    Coeffs::makeLowPass (sr, f, 1.6180f));
        }
        cHigh = highHz; cSteep = steep;
    }

    if (! cInit || style != cStyle)
    {
        // สีของแต่ละ Style (นอกเหนือจากเส้นโค้ง saturation) – ดูคำอธิบายใน README
        Coeffs::Ptr pre1 = Coeffs::makePeakFilter (sr, 1000.0f, 1.0f, 1.0f);
        Coeffs::Ptr pre2 = Coeffs::makePeakFilter (sr, 1000.0f, 1.0f, 1.0f);
        Coeffs::Ptr post = Coeffs::makePeakFilter (sr, 1000.0f, 1.0f, 1.0f);
        switch (style)
        {
            case 0: // A : เทปพรีแอมป์ หลอด – "ultra smooth" ตัดแหลมนิด ๆ
                post = Coeffs::makeHighShelf (sr, 8000.0f, 0.7f, Decibels::decibelsToGain (-2.5f));
                break;
            case 1: // E : เบสอ้วน + ปลายแหลมโปร่ง
                pre1 = Coeffs::makeLowShelf  (sr, 120.0f, 0.7f, Decibels::decibelsToGain (3.0f));
                post = Coeffs::makeHighShelf (sr, 9000.0f, 0.7f, Decibels::decibelsToGain (2.5f));
                break;
            case 2: // N : เบสหนักแต่แน่น, มิดโฟกัสไม่แคบ
                pre1 = Coeffs::makeLowShelf  (sr, 100.0f, 0.7f, Decibels::decibelsToGain (2.0f));
                pre2 = Coeffs::makePeakFilter (sr, 900.0f, 0.8f, Decibels::decibelsToGain (3.0f));
                break;
            case 3: // T : อุ่น มีพันช์
                pre1 = Coeffs::makeLowShelf  (sr, 200.0f, 0.7f, Decibels::decibelsToGain (1.5f));
                break;
            case 4: // P : เน้นฮาร์โมนิกคี่ คม ๆ
                pre2 = Coeffs::makePeakFilter (sr, 2500.0f, 0.8f, Decibels::decibelsToGain (2.0f));
                break;
            default: break;
        }
        setBoth (styleEqPreF1, pre1);
        setBoth (styleEqPreF2, pre2);
        setBoth (styleEqPostF, post);
        cStyle = style;
    }

    cInit = true;
}

//==============================================================================
// เส้นโค้ง saturation ของแต่ละ Style (ทุกเส้นถูกทำให้ความชันที่ศูนย์ = 1 เพื่อให้สัญญาณเบายังคงระดับเดิม)
float EduSaturatorProcessor::saturate (float x, int style) const noexcept
{
    switch (style)
    {
        case 0: // A – นุ่มเรียบมาก: tanh สมมาตร + bias เล็กน้อย
        {
            constexpr float b = 0.06f;
            const float t = std::tanh (b);
            return (std::tanh (x + b) - t) / (1.0f - t * t);
        }
        case 1: // E – soft knee อสมมาตรเล็กน้อย (ทรานส์ฟอร์เมอร์/คอนโซล)
            return x >= 0.0f ? x / (1.0f + 0.8f * x)
                             : x / (1.0f - 1.2f * x);
        case 2: // N – เจอร์เมเนียม: ครึ่งบวก clip เร็ว, ครึ่งลบกว้างกว่า
            return x >= 0.0f ? std::tanh (x)
                             : -std::tanh (-0.6f * x) / 0.6f;
        case 3: // T – ไตรโอด: bias สูง → ฮาร์โมนิกคู่มาก
        {
            constexpr float b = 0.35f;
            const float t = std::tanh (b);
            return (std::tanh (x + b) - t) / (1.0f - t * t);
        }
        case 4: // P – เพนโทด: สมมาตร knee คม → ฮาร์โมนิกคี่
        default:
        {
            const float a = std::abs (x);
            return x / std::cbrt (1.0f + a * a * a);
        }
    }
}

//==============================================================================
void EduSaturatorProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numCh = jmin (getTotalNumInputChannels(), (int) maxCh);
    const int total = buffer.getNumSamples();
    for (int ch = numCh; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, total);
    if (numCh == 0) return;

    // ---- อ่านพารามิเตอร์ ----
    auto pv = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const int   style   = (int) pv (ParamID::style);
    const float drive   = pv (ParamID::drive);
    const bool  punish  = pv (ParamID::punish)   > 0.5f;
    const bool  thump   = pv (ParamID::thump)    > 0.5f;
    const float lowHz   = pv (ParamID::lowCut);
    const float tone    = pv (ParamID::tone);
    const float highHz  = pv (ParamID::highCut);
    const bool  steep   = pv (ParamID::steep)    > 0.5f;
    const float outDbP  = pv (ParamID::output);
    const bool  autoOn  = pv (ParamID::autoGain) > 0.5f;
    const float mixV    = pv (ParamID::mix);

    updateFilters (lowHz, thump, tone, highHz, steep, style);

    // Drive 0..10 -> 0..30 dB, Punish = +20 dB
    const float totalDriveDb = drive * 3.0f + (punish ? 20.0f : 0.0f);
    const float driveGain    = Decibels::decibelsToGain (totalDriveDb);

    // Auto-Gain: ลด Output ตาม Drive (Output จะขยับสวนทางกับ Drive)
    const float autoDb = -jlimit (0.0f, 24.0f, totalDriveDb * 0.5f);
    autoOutputDb.store (autoDb);
    const float outGain = Decibels::decibelsToGain (autoOn ? autoDb : outDbP);

    if (prevDriveGain < 0.0f) { prevDriveGain = driveGain; prevOutGain = outGain; prevMix = mixV; }

    const int maxBlock = wetBuffer.getNumSamples();
    const bool oneChan = (numCh == 1);

    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = jmin (maxBlock, total - start);
        const float t0 = (float) start / (float) total, t1 = (float) (start + n) / (float) total;
        auto lerp = [] (float a, float b, float t) { return a + (b - a) * t; };

        // ---- เก็บ dry (หน่วงให้ตรงกับ latency ของ oversampling) ----
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* in = buffer.getReadPointer (ch, start);
            float* d = dryBuffer.getWritePointer (ch);
            float* w = wetBuffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                dryDelay.pushSample (ch, in[i]);
                d[i] = dryDelay.popSample (ch);
                float x = in[i];

                // ---- PRE-SATURATION: Low Cut -> Thump -> Tone -> สีของ Style ----
                x = lowCutF[(size_t) ch].processSample (x);
                x = thumpF[(size_t) ch].processSample (x);
                x = toneLowF[(size_t) ch].processSample (x);
                x = toneHighF[(size_t) ch].processSample (x);
                x = styleEqPreF1[(size_t) ch].processSample (x);
                x = styleEqPreF2[(size_t) ch].processSample (x);
                w[i] = x;
            }
        }
        (void) oneChan;

        // ---- Drive + Punish (ramp กันเสียงคลิก) ----
        const float g0 = lerp (prevDriveGain, driveGain, t0), g1 = lerp (prevDriveGain, driveGain, t1);
        for (int ch = 0; ch < numCh; ++ch)
            wetBuffer.applyGainRamp (ch, 0, n, g0, g1);

        // ---- Attitude meter: RMS หลัง Drive, ballistics ~300 ms (เหมือน VU) ----
        {
            float ms = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float r = wetBuffer.getRMSLevel (ch, 0, n);
                ms += r * r;
            }
            ms /= (float) numCh;
            const float coef = std::exp (-(float) n / (0.3f * (float) sr));
            meterMs = coef * meterMs + (1.0f - coef) * ms;
            const float rms = std::sqrt (meterMs);
            // 0 VU อ้างอิงที่ RMS 0.25 (≈ -12 dBFS) หลัง Drive
            attitudeDb.store (jlimit (-30.0f, 6.0f, Decibels::gainToDecibels (rms / 0.25f, -60.0f)));
        }

        // ---- SATURATION ที่ 4x oversampling (ลด aliasing) ----
        {
            dsp::AudioBlock<float> block (wetBuffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
            auto up = oversampler.processSamplesUp (block);
            for (size_t ch = 0; ch < (size_t) numCh; ++ch)
            {
                float* d = up.getChannelPointer (ch);
                float x1 = dcX1[ch], y1 = dcY1[ch];
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    const float y = saturate (d[i], style);
                    const float o = y - x1 + dcR * y1;   // DC blocker (ฮาร์โมนิกคู่สร้าง DC)
                    x1 = y; y1 = o;
                    d[i] = o;
                }
                dcX1[ch] = x1; dcY1[ch] = y1;
            }
            oversampler.processSamplesDown (block);
        }

        // ---- POST-SATURATION: High Cut (หลัง saturation) -> สีของ Style -> Output ----
        const float o0 = lerp (prevOutGain, outGain, t0), o1 = lerp (prevOutGain, outGain, t1);
        const float m0 = lerp (prevMix, mixV, t0),         m1 = lerp (prevMix, mixV, t1);
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* w = wetBuffer.getWritePointer (ch);
            const float* d = dryBuffer.getReadPointer (ch);
            float* out = buffer.getWritePointer (ch, start);
            for (int i = 0; i < n; ++i)
            {
                float x = hcOnePole[(size_t) ch].processSample (w[i]);
                if (steep)
                {
                    x = hcBiq1[(size_t) ch].processSample (x);
                    x = hcBiq2[(size_t) ch].processSample (x);
                }
                x = styleEqPostF[(size_t) ch].processSample (x);

                const float f = (float) i / (float) n;
                x *= o0 + (o1 - o0) * f;
                const float m = m0 + (m1 - m0) * f;
                out[i] = d[i] * (1.0f - m) + x * m;
            }
        }
    }

    prevDriveGain = driveGain; prevOutGain = outGain; prevMix = mixV;
}

//==============================================================================
AudioProcessorEditor* EduSaturatorProcessor::createEditor() { return new EduSaturatorEditor (*this); }

void EduSaturatorProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void EduSaturatorProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EduSaturatorProcessor(); }
