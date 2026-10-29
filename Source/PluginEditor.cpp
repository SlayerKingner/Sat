#include "PluginEditor.h"

using namespace juce;

static const Colour kPanel   (0xff1e2022);
static const Colour kPanel2  (0xff2a2d30);
static const Colour kAccent  (0xffff7a3d);
static const Colour kText    (0xffd9d9d9);

//==============================================================================
EduLookAndFeel::EduLookAndFeel()
{
    setColour (Slider::rotarySliderFillColourId, kAccent);
    setColour (TextButton::buttonColourId, kPanel2);
    setColour (TextButton::buttonOnColourId, kAccent);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff111111));
    setColour (TooltipWindow::textColourId, Colours::white);
}

void EduLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float d = jmin (b.getWidth(), b.getHeight());
    auto c = b.getCentre();
    const float r = d * 0.5f;
    const float ang = startAngle + pos * (endAngle - startAngle);
    const bool enabled = s.isEnabled();

    // จุดสเกลรอบปุ่ม
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        Point<float> p (c.x + std::sin (a) * (r + 0.0f), c.y - std::cos (a) * (r + 0.0f));
        g.setColour (Colours::white.withAlpha (0.55f));
        g.fillEllipse (p.x - 1.6f, p.y - 1.6f, 3.2f, 3.2f);
    }

    const float kr = r * 0.78f;
    g.setGradientFill (ColourGradient (Colour (0xff4a4d52), c.x - kr, c.y - kr, Colour (0xff0e0f10), c.x + kr, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (Colours::black);
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.5f);
    g.setColour (Colour (0xff1a1b1d));
    g.fillEllipse (c.x - kr * 0.68f, c.y - kr * 0.68f, kr * 1.36f, kr * 1.36f);

    // เข็มชี้
    g.setColour (enabled ? Colours::white : Colours::grey);
    Path p;
    p.addRoundedRectangle (-1.8f, -kr * 0.95f, 3.6f, kr * 0.42f, 1.5f);
    g.fillPath (p, AffineTransform::rotation (ang).translated (c.x, c.y));
}

void EduLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const bool isPunish = b.getComponentID() == "punish";

    if (isPunish)
    {
        auto face = r.reduced (2.0f);
        g.setColour (on ? kAccent : Colour (0xff4b4f54));
        g.fillRoundedRectangle (face, 6.0f);
        g.setColour (Colours::black);
        g.drawRoundedRectangle (face, 6.0f, 2.0f);
        g.setColour (on ? Colours::white : kText);
        g.setFont (Font (FontOptions (15.0f, Font::bold)));
        g.drawText ("ON", face, Justification::centred);
    }
    else
    {
        // สวิตช์เล็ก: วงกลมสว่างเมื่อเปิด + ข้อความ
        auto led = r.removeFromLeft (r.getHeight()).reduced (4.0f);
        g.setColour (on ? kAccent : Colour (0xff3a3d40));
        g.fillEllipse (led);
        g.setColour (Colours::black);
        g.drawEllipse (led, 1.5f);
        g.setColour (kText);
        g.setFont (Font (FontOptions (11.5f, Font::bold)));
        g.drawText (b.getButtonText(), r.reduced (3.0f, 0.0f), Justification::centredLeft);
    }
}

void EduLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? kAccent : Colour (0xff50545a));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r, 5.0f, 2.0f);
    g.setColour (on ? Colours::white : kText);
    g.setFont (Font (FontOptions (17.0f, Font::bold)));
    g.drawText (b.getButtonText(), r, Justification::centred);
}

//==============================================================================
void VUMeter::tick()
{
    shown += (target - shown) * 0.25f;   // ให้เข็มเคลื่อนไหวนุ่ม ๆ
    repaint();
}

void VUMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0b0b0b));
    g.fillRoundedRectangle (r, 12.0f);
    auto face = r.reduced (7.0f);
    g.setGradientFill (ColourGradient (Colour (0xfffbf3d6), face.getCentreX(), face.getY(),
                                       Colour (0xffd9cb9a), face.getCentreX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 8.0f);

    const Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.55f);
    const float R = face.getHeight() * 1.42f;
    auto angleFor = [] (float db) { return jmap (jlimit (-20.0f, 3.0f, db), -20.0f, 3.0f, -0.62f, 0.62f); };

    // โซนแดง 0..+3
    Path red;
    red.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleFor (0.0f), angleFor (3.0f), true);
    g.setColour (Colour (0xffc62828));
    g.strokePath (red, PathStrokeType (4.0f));
    Path black;
    black.addCentredArc (pivot.x, pivot.y, R, R, 0.0f, angleFor (-20.0f), angleFor (0.0f), true);
    g.setColour (Colours::black);
    g.strokePath (black, PathStrokeType (1.5f));

    g.setFont (Font (FontOptions (11.0f)));
    for (int db : { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 })
    {
        const float a = angleFor ((float) db);
        const Point<float> p1 (pivot.x + std::sin (a) * R, pivot.y - std::cos (a) * R);
        const Point<float> p2 (pivot.x + std::sin (a) * (R - 7.0f), pivot.y - std::cos (a) * (R - 7.0f));
        g.setColour (db >= 0 ? Colour (0xffc62828) : Colours::black);
        g.drawLine ({ p1, p2 }, 1.4f);
        const Point<float> tp (pivot.x + std::sin (a) * (R - 16.0f), pivot.y - std::cos (a) * (R - 16.0f));
        g.drawText (String (db), Rectangle<float> (24, 14).withCentre (tp), Justification::centred);
    }

    g.setColour (Colour (0xff444444));
    g.setFont (Font (FontOptions (14.0f)));
    g.drawText ("ATTITUDE", face.withTop (face.getCentreY() + 2.0f), Justification::centredTop);

    // เข็ม
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (face.toNearestInt());
    const float a = angleFor (shown);
    g.setColour (Colours::black);
    g.drawLine (pivot.x, pivot.y, pivot.x + std::sin (a) * (R - 3.0f), pivot.y - std::cos (a) * (R - 3.0f), 2.0f);
    g.setColour (Colour (0xff222222));
    g.fillRect (face.withTop (face.getBottom() - 12.0f));
}

//==============================================================================
static void setupKnob (Component& parent, Slider& s, EduLookAndFeel& laf, const String& tip, double def)
{
    s.setSliderStyle (Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    s.setRotaryParameters (degreesToRadians (225.0f), degreesToRadians (495.0f), true);
    s.setPopupDisplayEnabled (true, true, &parent);
    s.setLookAndFeel (&laf);
    s.setTooltip (tip);
    s.setDoubleClickReturnValue (true, def);
    parent.addAndMakeVisible (s);
}

static const char* kStyleInfo[5] = {
    "A  -  Ampex 350 tape preamp (tube): ultra-smooth saturation, slightly rolled-off highs.",
    "E  -  EMI TG channel: beefy low end with a smooth, airy top-end sheen.",
    "N  -  Neve 1057 (germanium): weighty but solid lows, focused mids, asymmetric clipping.",
    "T  -  Culture Vulture triode: warm and punchy, lots of EVEN harmonics (good for drums).",
    "P  -  Culture Vulture pentode: bright and edgy, mostly ODD harmonics."
};

EduSaturatorEditor::EduSaturatorEditor (EduSaturatorProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (860, 430);

    setupKnob (*this, drive,   laf, "Drive: input gain into the saturation circuit (0 - 10)", 3.0);
    setupKnob (*this, lowCut,  laf, "Low Cut: removes lows BEFORE saturation (20 Hz - 1 kHz)", 20.0);
    setupKnob (*this, tone,    laf, "Tone: tilt EQ BEFORE saturation (Dark <-> Bright)", 0.0);
    setupKnob (*this, highCut, laf, "High Cut: low-pass AFTER saturation (1 kHz - 20 kHz)", 20000.0);
    setupKnob (*this, output,  laf, "Output level (-24 dB - 0 dB)", 0.0);
    setupKnob (*this, mix,     laf, "Mix: dry <-> wet (parallel processing)", 1.0);
    drive.setSliderStyle (Slider::RotaryVerticalDrag);

    aDrive = std::make_unique<SA> (proc.apvts, ParamID::drive,   drive);
    aLow   = std::make_unique<SA> (proc.apvts, ParamID::lowCut,  lowCut);
    aTone  = std::make_unique<SA> (proc.apvts, ParamID::tone,    tone);
    aHigh  = std::make_unique<SA> (proc.apvts, ParamID::highCut, highCut);
    aOut   = std::make_unique<SA> (proc.apvts, ParamID::output,  output);
    aMix   = std::make_unique<SA> (proc.apvts, ParamID::mix,     mix);

    // ให้ popup แสดงหน่วย/รูปแบบตัวเลขอ่านง่าย
    lowCut.textFromValueFunction  = [] (double v) { return String (roundToInt (v)) + " Hz"; };
    highCut.textFromValueFunction = [] (double v) { return v >= 1000 ? String (v / 1000.0, 1) + " kHz" : String (roundToInt (v)) + " Hz"; };
    output.textFromValueFunction  = [] (double v) { return String (v, 1) + " dB"; };
    mix.textFromValueFunction     = [] (double v) { return String (roundToInt (v * 100)) + " % wet"; };
    tone.textFromValueFunction    = [] (double v) { return v < -0.01 ? "Dark " + String (roundToInt (-v * 100)) + "%"
                                                    : v > 0.01 ? "Bright " + String (roundToInt (v * 100)) + "%" : String ("Flat"); };
    drive.textFromValueFunction   = [] (double v) { return String (v, 1); };

    for (auto* t : { &punish, &thump, &steep, &autoGain })
    {
        t->setLookAndFeel (&laf);
        addAndMakeVisible (*t);
    }
    punish.setComponentID ("punish");
    thump.setButtonText ("THUMP");
    steep.setButtonText ("STEEP");
    autoGain.setButtonText ("AUTO");
    punish.setTooltip ("Punish: +20 dB extra gain into the saturation (all-out distortion)");
    thump.setTooltip ("Thump: +4.5 dB boost at the Low Cut frequency (like tape 'head bump')");
    steep.setTooltip ("Steep: High Cut slope 6 dB/oct (off) or 30 dB/oct (on)");
    autoGain.setTooltip ("Auto: Output follows Drive in the opposite direction (auto-gain)");
    aPunish = std::make_unique<BA> (proc.apvts, ParamID::punish,   punish);
    aThump  = std::make_unique<BA> (proc.apvts, ParamID::thump,    thump);
    aSteep  = std::make_unique<BA> (proc.apvts, ParamID::steep,    steep);
    aAuto   = std::make_unique<BA> (proc.apvts, ParamID::autoGain, autoGain);

    const char* names[] = { "A", "E", "N", "T", "P" };
    for (int i = 0; i < 5; ++i)
    {
        styleBtn[i].setButtonText (names[i]);
        styleBtn[i].setClickingTogglesState (false);
        styleBtn[i].setLookAndFeel (&laf);
        addAndMakeVisible (styleBtn[i]);
        styleBtn[i].onClick = [this, i]
        {
            if (aStyle) { aStyle->beginGesture(); aStyle->setValueAsPartOfGesture ((float) i); aStyle->endGesture(); }
        };
    }
    aStyle = std::make_unique<ParameterAttachment> (*proc.apvts.getParameter (ParamID::style),
                [this] (float v) { setStyleButtons ((int) std::lround (v)); });
    aStyle->sendInitialUpdate();

    addAndMakeVisible (vu);
    startTimerHz (30);
}

EduSaturatorEditor::~EduSaturatorEditor()
{
    for (auto* s : { &drive, &lowCut, &tone, &highCut, &output, &mix }) s->setLookAndFeel (nullptr);
    for (auto* t : { &punish, &thump, &steep, &autoGain }) t->setLookAndFeel (nullptr);
    for (auto& b : styleBtn) b.setLookAndFeel (nullptr);
}

void EduSaturatorEditor::setStyleButtons (int index)
{
    index = jlimit (0, 4, index);
    for (int i = 0; i < 5; ++i)
        styleBtn[i].setToggleState (i == index, dontSendNotification);
    styleText = kStyleInfo[index];
    repaint();
}

void EduSaturatorEditor::timerCallback()
{
    vu.setValue (proc.attitudeDb.load());
    vu.tick();

    // เมื่อเปิด Auto ปุ่ม Output จะขยับตามที่ Auto-Gain คำนวณ
    const bool autoOn = autoGain.getToggleState();
    output.setEnabled (! autoOn);
    const double v = autoOn ? (double) proc.autoOutputDb.load()
                            : (double) *proc.apvts.getRawParameterValue (ParamID::output);
    if (std::abs (output.getValue() - v) > 0.01)
        output.setValue (v, dontSendNotification);
}

//==============================================================================
void EduSaturatorEditor::paint (Graphics& g)
{
    g.fillAll (kPanel);

    // กรอบไม้ด้านข้าง (วาดด้วยสี ไม่ใช้รูปภาพ)
    g.setColour (Colour (0xff6b3f22));
    g.fillRect (0, 0, 16, getHeight());
    g.fillRect (getWidth() - 16, 0, 16, getHeight());

    g.setColour (kText);
    g.setFont (Font (FontOptions (24.0f, Font::bold)));
    g.drawText ("EDU SATURATOR", getLocalBounds().withTrimmedLeft (560).withTrimmedRight (40).withHeight (95).withY (60),
                Justification::centredLeft);
    g.setFont (Font (FontOptions (12.0f)));
    g.setColour (kText.withAlpha (0.6f));
    g.drawText ("analog saturation study model", getLocalBounds().withTrimmedLeft (562).withY (92).withHeight (20),
                Justification::centredLeft);

    auto label = [&] (const String& t, Rectangle<int> r, Justification j = Justification::centred)
    {
        g.setColour (kText);
        g.setFont (Font (FontOptions (13.0f, Font::bold)));
        g.drawText (t, r, j);
    };
    label ("DRIVE",    drive.getBounds().withY (drive.getY() - 20).withHeight (18));
    label ("LOW CUT",  lowCut.getBounds().withY (lowCut.getY() - 18).withHeight (16));
    label ("TONE",     tone.getBounds().withY (tone.getY() - 18).withHeight (16));
    label ("HIGH CUT", highCut.getBounds().withY (highCut.getY() - 18).withHeight (16));
    label ("MIX",      mix.getBounds().withY (mix.getY() - 18).withHeight (16));
    label ("OUTPUT",   output.getBounds().withY (output.getY() - 18).withHeight (16));
    g.setFont (Font (FontOptions (10.0f)));
    g.setColour (kText.withAlpha (0.7f));
    auto small = [&] (const String& t, Rectangle<int> r, Justification j) { g.drawText (t, r, j); };
    small ("20 Hz", lowCut.getBounds().withY (lowCut.getBottom() - 12).withHeight (12), Justification::bottomLeft);
    small ("1 kHz", lowCut.getBounds().withY (lowCut.getBottom() - 12).withHeight (12), Justification::bottomRight);
    small ("DARK",   tone.getBounds().withY (tone.getBottom() - 12).withHeight (12), Justification::bottomLeft);
    small ("BRIGHT", tone.getBounds().withY (tone.getBottom() - 12).withHeight (12), Justification::bottomRight);
    small ("1 kHz",  highCut.getBounds().withY (highCut.getBottom() - 12).withHeight (12), Justification::bottomLeft);
    small ("20 kHz", highCut.getBounds().withY (highCut.getBottom() - 12).withHeight (12), Justification::bottomRight);
    small ("DRY", mix.getBounds().withY (mix.getBottom() - 12).withHeight (12), Justification::bottomLeft);
    small ("WET", mix.getBounds().withY (mix.getBottom() - 12).withHeight (12), Justification::bottomRight);
    small ("-24 dB", output.getBounds().withY (output.getBottom() - 12).withHeight (12), Justification::bottomLeft);
    small ("0 dB",   output.getBounds().withY (output.getBottom() - 12).withHeight (12), Justification::bottomRight);

    // แถบ Style ด้านล่าง
    auto bar = Rectangle<int> (16, getHeight() - 92, getWidth() - 32, 92);
    g.setColour (kPanel2);
    g.fillRect (bar);
    label ("STYLE", Rectangle<int> (getWidth() - 160, bar.getY() + 8, 120, 24), Justification::centredLeft);
    g.setColour (kAccent);
    g.setFont (Font (FontOptions (13.0f)));
    g.drawText (styleText, bar.withTrimmedTop (56).reduced (20, 0), Justification::centredLeft);
}

void EduSaturatorEditor::resized()
{
    // แถวบน
    punish.setBounds (150, 28, 64, 38);
    vu.setBounds (300, 20, 250, 170);
    drive.setBounds (24, 78, 190, 190);

    // แถวกลาง: knob ย่อย
    const int y = 218, s = 108;
    thump.setBounds (232, y + 45, 80, 22);
    lowCut.setBounds (300, y, s, s);
    tone.setBounds (418, y, s, s);
    highCut.setBounds (536, y, s, s);
    steep.setBounds (650, y + 45, 80, 22);
    mix.setBounds (740, y - 10, s, s);
    output.setBounds (640, 112, 110, 110);
    autoGain.setBounds (755, 160, 80, 22);

    // ปุ่ม Style
    for (int i = 0; i < 5; ++i)
        styleBtn[i].setBounds (40 + i * 80, getHeight() - 84, 64, 40);
}
