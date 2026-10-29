// ทดสอบ DSP แบบ headless: ไม่มีค่า NaN, ฮาร์โมนิกของแต่ละ Style, Mix/Highcut/Auto ทำงานตามคาด
#include "../Source/PluginProcessor.h"
#include <cstdio>
using namespace juce;

static void setP (EduSaturatorProcessor& p, const char* id, float real)
{ auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (real)); }

static std::vector<float> run (EduSaturatorProcessor& p, float freq, float amp, int blocks = 200)
{
    const int bs = 512; std::vector<float> out; double ph = 0; MidiBuffer m;
    for (int b = 0; b < blocks; ++b)
    {
        AudioBuffer<float> buf (2, bs);
        for (int i = 0; i < bs; ++i) { float s = amp * (float) std::sin (ph); ph += 2 * MathConstants<double>::pi * freq / 48000.0; buf.setSample (0, i, s); buf.setSample (1, i, s); }
        p.processBlock (buf, m);
        if (b >= blocks - 40) for (int i = 0; i < bs; ++i) out.push_back (buf.getSample (0, i));
    }
    return out;
}
static float harm (const std::vector<float>& x, float f, int k)   // ระดับฮาร์โมนิกที่ k (DFT)
{
    double re = 0, im = 0; const double w = 2 * MathConstants<double>::pi * f * k / 48000.0;
    // x เริ่มที่ block ที่ 160 -> เฟสไม่สำคัญเพราะใช้ขนาด
    for (size_t i = 0; i < x.size(); ++i) { re += x[i] * std::cos (w * i); im += x[i] * std::sin (w * i); }
    return (float) (2 * std::sqrt (re * re + im * im) / x.size());
}
static float rms (const std::vector<float>& x) { double s = 0; for (auto v : x) s += v * v; return (float) std::sqrt (s / x.size()); }

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* msg) { std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", msg); if (! ok) ++fails; };
    const char* names[] = { "A", "E", "N", "T", "P" };
    for (int st = 0; st < 5; ++st)
    {
        EduSaturatorProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        setP (p, ParamID::style, (float) st); setP (p, ParamID::drive, 6.0f);
        auto y = run (p, 1000.0f, 0.2f);
        bool finite = true; for (auto v : y) if (! std::isfinite (v) || std::abs (v) > 10) finite = false;
        float h1 = harm (y, 1000, 1), h2 = harm (y, 1000, 2), h3 = harm (y, 1000, 3);
        std::printf ("Style %s: H1=%.3f H2=%.4f H3=%.4f  (H2/H3=%.2f)\n", names[st], h1, h2, h3, h2 / h3);
        check (finite, "output finite");
        check (h3 > 0.001f, "adds harmonics when driven");
        if (st == 3) check (h2 > h3, "T (triode) is even-harmonic dominant");
        if (st == 4) check (h3 > h2 * 2.0f, "P (pentode) is odd-harmonic dominant");
    }
    { // Drive 0 + Mix: สัญญาณเบาเกือบโปร่งใส
        EduSaturatorProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        setP (p, ParamID::drive, 0.0f); auto y = run (p, 1000.0f, 0.05f);
        check (std::abs (rms (y) / (0.05f / 1.4142f) - 1.0f) < 0.25f, "drive 0: low-level signal ~unity gain");
    }
    { // Punish เพิ่ม distortion
        EduSaturatorProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        setP (p, ParamID::drive, 3.0f); auto a = run (p, 1000.0f, 0.1f); float h3a = harm (a, 1000, 3) / harm (a, 1000, 1);
        setP (p, ParamID::punish, 1.0f); auto b = run (p, 1000.0f, 0.1f); float h3b = harm (b, 1000, 3) / harm (b, 1000, 1);
        check (h3b > h3a * 2, "Punish increases distortion");
        check (p.attitudeDb.load() > 0.0f, "Attitude meter moves into red with Punish");
    }
    { // Auto-gain ลด output เมื่อ drive เพิ่ม
        EduSaturatorProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        setP (p, ParamID::drive, 8.0f); setP (p, ParamID::output, 0.0f);
        auto a = run (p, 1000.0f, 0.1f);
        setP (p, ParamID::autoGain, 1.0f);
        auto b = run (p, 1000.0f, 0.1f);
        check (rms (b) < rms (a) * 0.7f, "Auto-gain lowers output at high drive");
    }
    { // High Cut + Steep: ตัดแหลมจริง (ฮาร์โมนิกที่ 5 ของ 3 kHz = 15 kHz)
        EduSaturatorProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        setP (p, ParamID::drive, 8.0f);
        auto a = run (p, 3000.0f, 0.2f); float hiA = harm (a, 3000, 5);
        setP (p, ParamID::highCut, 4000.0f); setP (p, ParamID::steep, 1.0f);
        auto b = run (p, 3000.0f, 0.2f); float hiB = harm (b, 3000, 5);
        check (hiB < hiA * 0.1f, "High Cut (steep) removes fizz above cutoff");
    }
    { // Mix 0 = dry ล้วน (ผ่าน latency ของ oversampling)
        EduSaturatorProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        setP (p, ParamID::drive, 10.0f); setP (p, ParamID::mix, 0.0f);
        auto y = run (p, 1000.0f, 0.3f);
        check (std::abs (harm (y, 1000, 3)) < 0.002f && std::abs (harm (y, 1000, 1) - 0.3f) < 0.01f, "Mix=0 passes dry signal untouched");
    }
    return fails;
}
