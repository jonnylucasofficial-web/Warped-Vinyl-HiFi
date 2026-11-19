#include "PluginEditor.h"

namespace
{
pedalui::Palette wvPalette()
{
    return { juce::Colour (0xff2f5d62),   // body: deep teal
             juce::Colour (0xff1c3a3d),
             juce::Colour (0xfff2e6c9),   // ink: cream
             juce::Colour (0xfff2e6c9),   // knob: cream
             juce::Colour (0xffe1d3b2),
             juce::Colour (0xff1e1e1e),   // pointer
             juce::Colour (0xffe0a03a),   // accent: amber
             juce::Colour (0xff1f2326),
             juce::Colour (0xffdedede) };
}
}

WarpedVinylEditor::WarpedVinylEditor (WarpedVinylProcessor& p)
    : AudioProcessorEditor (p), proc (p), lnf (wvPalette()),
      tone (p.apvts, wvids::tone, "TONE"), lag (p.apvts, wvids::lag, "LAG"), mix (p.apvts, wvids::mix, "MIX"),
      rpm (p.apvts, wvids::rpm, "RPM"), depth (p.apvts, wvids::depth, "DEPTH"), warp (p.apvts, wvids::warp, "WARP"),
      ramp (p.apvts, wvids::rampSpeed, "RAMP", true), expKnob (p.apvts, wvids::exp, "EXPRESSION", true),
      tapDiv (p.apvts, wvids::tapDiv, "TAP DIV", { "1", "2", "4" }, wvPalette()),
      riseShape (p.apvts, wvids::rise, "RISE", { "SMOOTH", "LINEAR", "EXPO" }, wvPalette()),
      fallShape (p.apvts, wvids::fall, "FALL", { "SMOOTH", "LINEAR", "EXPO" }, wvPalette())
{
    setLookAndFeel (&lnf);
    for (auto* c : std::initializer_list<juce::Component*> { &tone, &lag, &mix, &rpm, &depth, &warp, &ramp, &expKnob,
                                                              &tapDiv, &riseShape, &fallShape, &bypassFs, &tapFs,
                                                              &bypassLed, &tapLed, &expConnected, &hostSync, &saveBtn, &status })
        addAndMakeVisible (c);

    tone.setTooltip ("TONE: brightness of the modulated signal");
    lag.setTooltip ("LAG: delay between wet and dry (~5–20 ms). Higher = lusher, thicker chorus");
    mix.setTooltip ("MIX: dry/wet. Full CW = pure vibrato, around noon = chorus");
    rpm.setTooltip ("RPM: modulation rate (tap tempo can override; turn the knob to take back control)");
    depth.setTooltip ("DEPTH: how far the pitch bends");
    warp.setTooltip ("WARP: moves the peak of the wave. Noon = symmetrical; CCW = fast rise/slow fall; CW = slow rise/fast fall");
    ramp.setTooltip ("RAMP: ramp speed (0.5–30 s per sweep). Tap sets it instead when TAP CTRL dip = R");
    tapDiv.setTooltip ("Tap divisions: 1/2/4 cycles per bar (3/6/8 with the TAP DIV dip)");
    riseShape.setTooltip ("Curve of the rising half of the wave");
    fallShape.setTooltip ("Curve of the falling half of the wave");

    bypassFs.caption = "BYPASS";
    bypassFs.getState = [this] { return proc.apvts.getRawParameterValue (wvids::on)->load() > 0.5f; };
    bypassFs.onState = [this] (bool on)
    {
        auto* prm = proc.apvts.getParameter (wvids::on);
        prm->beginChangeGesture(); prm->setValueNotifyingHost (on ? 1.0f : 0.0f); prm->endChangeGesture();
    };
    tapFs.caption = "TAP  (hold = ramp)";
    tapFs.onPress = [this] { proc.requestTap(); };
    tapFs.onHold = [this] (bool h) { proc.setHold (h); };
    tapFs.setTooltip ("Tap to set tempo. Hold for a momentary spell of ramping");

    const char* lnames[] = { "LAG", "MIX", "RPM", "DEPTH", "WARP", "BOUNCE", "SWEEP", "POLARITY" };
    const char* ltips[] = { "Ramp/expression controls LAG", "Ramp/expression controls MIX", "Ramp/expression controls RPM",
                            "Ramp/expression controls DEPTH", "Ramp/expression controls WARP",
                            "BOUNCE: ramp back and forth continuously (off = ramp and hold)",
                            "SWEEP: B = between the bottom and the knob; T = between the knob and the top",
                            "POLARITY: reverses expression pedal travel" };
    const char* rnames[] = { "LAG", "MIX", "RPM", "DEPTH", "WARP", "TAP DIV", "TAP CTRL" };
    const char* rtips[] = { "Direction: LAG rises (up) or falls (down)", "Direction: MIX rises or falls",
                            "Direction: RPM rises or falls", "Direction: DEPTH rises or falls", "Direction: WARP rises or falls",
                            "TAP DIV: divisions 1-2-4 (down) or 3-6-8 (up)",
                            "TAP CTRL: tap sets the RPM (P, down) or the ramp speed (R, up). Ramp divisions are 2x slower" };
    for (int i = 0; i < wvids::leftDips.size(); ++i)
        dips.add (new pedalui::DipSwitch (p.apvts, wvids::leftDips[i], lnames[i], "", i == 6 ? "T" : "ON", ltips[i], wvPalette()));
    for (int i = 0; i < wvids::rightDips.size(); ++i)
        dips.add (new pedalui::DipSwitch (p.apvts, wvids::rightDips[i], rnames[i],
                                          i < 5 ? "FALL" : (i == 5 ? "124" : "P"),
                                          i < 5 ? "RISE" : (i == 5 ? "368" : "R"), rtips[i], wvPalette()));
    for (auto* d : dips) addAndMakeVisible (d);

    expAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, wvids::expConnected, expConnected);
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, wvids::hostSync, hostSync);
    for (auto* b : { &expConnected, &hostSync })
    {
        b->setColour (juce::ToggleButton::textColourId, juce::Colour (0xffdedede));
        b->setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffdedede));
    }

    for (int i = 0; i < 4; ++i)
    {
        auto* b = presetButtons.add (new juce::TextButton (juce::String (i + 1)));
        b->onClick = [this, i] { presetClicked (i); };
        b->setTooltip ("Recall preset " + juce::String (i + 1) + " (MIDI Program Change " + juce::String (i) + ")");
        addAndMakeVisible (b);
    }
    saveBtn.setClickingTogglesState (true);
    saveBtn.setTooltip ("Arm SAVE, then click a preset number to store the current settings");
    status.setColour (juce::Label::textColourId, juce::Colour (0xffdedede));
    status.setFont (juce::FontOptions (12.0f));

    setSize (760, 790);
    startTimerHz (40);
}

WarpedVinylEditor::~WarpedVinylEditor() { setLookAndFeel (nullptr); }

void WarpedVinylEditor::presetClicked (int slot)
{
    if (saveBtn.getToggleState()) { proc.slots.save (slot); saveBtn.setToggleState (false, juce::dontSendNotification); }
    else proc.slots.recall (slot);
}

void WarpedVinylEditor::timerCallback()
{
    const bool on = proc.apvts.getRawParameterValue (wvids::on)->load() > 0.5f;
    bypassLed.setColourAndLevel (juce::Colour (0xffff4030), on ? 1.0f : 0.0f);
    const float ph = proc.lfoPhase.load();
    tapLed.setColourAndLevel (proc.tapOverridesRpm.load() ? juce::Colour (0xff40ff60) : juce::Colour (0xffe0a03a),
                              ph < 0.5f ? 1.0f : 0.15f);

    const bool div368 = proc.apvts.getRawParameterValue ("dipTapDiv")->load() > 0.5f;
    static juce::String last;
    juce::String s = juce::String (proc.currentRateHz.load(), 2) + " Hz  |  ramp " +
                     juce::String (juce::roundToInt (proc.rampPosition.load() * 100)) + "%"
                     + (proc.tapOverridesRpm.load() ? "  |  RPM set by TAP" : "")
                     + (proc.tapOverridesRamp.load() ? "  |  RAMP set by TAP" : "")
                     + (div368 ? "  |  div 3/6/8" : "  |  div 1/2/4");
    if (s != last) { status.setText (s, juce::dontSendNotification); last = s; }

    for (int i = 0; i < 4; ++i)
        presetButtons[i]->setToggleState (proc.slots.getCurrent() == i, juce::dontSendNotification);

    if (on) spin += proc.currentRateHz.load() * juce::MathConstants<float>::twoPi / 40.0f;
    repaint (wavePanel);
}

void WarpedVinylEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141618));
    auto f = face.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (f.translated (0, 4), 14);
    juce::ColourGradient body (lnf.pal.body.brighter (0.15f), f.getX(), f.getY(), lnf.pal.body.darker (0.25f), f.getRight(), f.getBottom(), false);
    g.setGradientFill (body); g.fillRoundedRectangle (f, 14);
    g.setColour (lnf.pal.bodyEdge); g.drawRoundedRectangle (f.reduced (1), 14, 2);
    for (auto pt : { f.getTopLeft() + juce::Point<float> (14, 14), f.getTopRight() + juce::Point<float> (-14, 14),
                     f.getBottomLeft() + juce::Point<float> (14, -14), f.getBottomRight() + juce::Point<float> (-14, -14) })
        pedalui::drawScrew (g, pt);

    // Spinning record + live LFO waveform
    auto w = wavePanel.toFloat();
    auto disc = w.removeFromLeft (w.getHeight()).reduced (4);
    g.setColour (juce::Colour (0xff111111)); g.fillEllipse (disc);
    g.setColour (juce::Colour (0xff2a2a2a));
    for (int i = 1; i < 6; ++i) g.drawEllipse (disc.reduced ((float) i * disc.getWidth() * 0.07f), 0.8f);
    g.setColour (lnf.pal.accent); g.fillEllipse (disc.reduced (disc.getWidth() * 0.36f));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    auto c = disc.getCentre();
    g.drawLine ({ c.getPointOnCircumference (disc.getWidth() * 0.18f, spin), c.getPointOnCircumference (disc.getWidth() * 0.46f, spin) }, 2.0f);

    auto wv = w.reduced (10, 6);
    g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (wv, 6);
    wv::Lfo l;
    l.warp = proc.effective[4].load();
    l.rise = (wv::Shape) (int) proc.apvts.getRawParameterValue (wvids::rise)->load();
    l.fall = (wv::Shape) (int) proc.apvts.getRawParameterValue (wvids::fall)->load();
    const float dep = 0.15f + 0.85f * proc.effective[3].load();
    juce::Path path;
    for (int i = 0; i <= 200; ++i)
    {
        l.phase = std::fmod ((double) i / 100.0, 1.0);
        const float x = wv.getX() + 6 + (wv.getWidth() - 12) * (float) i / 200.0f;
        const float y = wv.getBottom() - 6 - (wv.getHeight() - 12) * (0.5f + dep * (l.value() - 0.5f));
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }
    g.setColour (lnf.pal.ink); g.strokePath (path, juce::PathStrokeType (2.0f));
    l.phase = proc.lfoPhase.load();
    const float px = wv.getX() + 6 + (wv.getWidth() - 12) * (float) l.phase * 0.5f;
    const float py = wv.getBottom() - 6 - (wv.getHeight() - 12) * (0.5f + dep * (l.value() - 0.5f));
    g.setColour (lnf.pal.accent); g.fillEllipse (px - 4, py - 4, 8, 8);

    g.setColour (lnf.pal.ink);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("WARPED VINYL", face.withY (face.getY() + 374).withHeight (36), juce::Justification::centred);
    g.setFont (juce::FontOptions (13.0f, juce::Font::italic));
    g.drawText ("HiFi  /  analog vibrato + chorus", face.withY (face.getY() + 408).withHeight (18), juce::Justification::centred);

    auto b = back.toFloat();
    g.setColour (lnf.pal.panel); g.fillRoundedRectangle (b, 8);
    g.setColour (lnf.pal.panelInk.withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    auto hdr = back.reduced (12, 6).removeFromTop (14);
    g.drawText ("RAMP / EXPRESSION ASSIGN", hdr.removeFromLeft (hdr.getWidth() * 8 / 15), juce::Justification::centred);
    g.drawText ("DIRECTION  /  TAP", hdr, juce::Justification::centred);
}

void WarpedVinylEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    back = r.removeFromBottom (214);
    r.removeFromBottom (10);
    face = r;

    auto f = face.reduced (40, 22);
    auto row1 = f.removeFromTop (100), row2 = f.removeFromTop (100);
    const int kw = row1.getWidth() / 3;
    tone.setBounds (row1.removeFromLeft (kw).reduced (16, 0)); lag.setBounds (row1.removeFromLeft (kw).reduced (16, 0)); mix.setBounds (row1.reduced (16, 0));
    rpm.setBounds (row2.removeFromLeft (kw).reduced (16, 0)); depth.setBounds (row2.removeFromLeft (kw).reduced (16, 0)); warp.setBounds (row2.reduced (16, 0));

    auto togRow = f.removeFromTop (74).reduced (0, 4);
    const int tw = togRow.getWidth() / 4;
    tapDiv.setBounds (togRow.removeFromLeft (tw).reduced (8, 0));
    riseShape.setBounds (togRow.removeFromLeft (tw).reduced (8, 0));
    fallShape.setBounds (togRow.removeFromLeft (tw).reduced (8, 0));
    ramp.setBounds (togRow.withSizeKeepingCentre (64, 70));

    wavePanel = juce::Rectangle<int> (300, 66).withCentre ({ face.getCentreX(), face.getY() + 336 });

    auto fsRow = face.reduced (40, 18).removeFromBottom (110);
    auto fl = fsRow.removeFromLeft (110), fr = fsRow.removeFromRight (110);
    bypassLed.setBounds (fl.getCentreX() - 15, fl.getY() - 26, 30, 24);
    tapLed.setBounds (fr.getCentreX() - 15, fr.getY() - 26, 30, 24);
    bypassFs.setBounds (fl.withSizeKeepingCentre (100, 104));
    tapFs.setBounds (fr.withSizeKeepingCentre (110, 104));

    auto bk = back.reduced (12, 8);
    bk.removeFromTop (16);
    auto dipRow = bk.removeFromTop (92);
    const int n = dips.size(), dw = dipRow.getWidth() / n;
    for (int i = 0; i < n; ++i) dips[i]->setBounds (dipRow.getX() + i * dw, dipRow.getY(), dw, dipRow.getHeight());

    auto ctl = bk.removeFromTop (60);
    expKnob.setBounds (ctl.removeFromLeft (70));
    auto toggles = ctl.removeFromLeft (220);
    expConnected.setBounds (toggles.removeFromTop (30));
    hostSync.setBounds (toggles);
    auto pr = ctl.removeFromRight (300).withSizeKeepingCentre (300, 30);
    for (auto* b : presetButtons) b->setBounds (pr.removeFromLeft (52).reduced (3, 0));
    saveBtn.setBounds (pr.reduced (3, 0));
    status.setBounds (bk.withHeight (22));
}

juce::AudioProcessorEditor* WarpedVinylProcessor::createEditor() { return new WarpedVinylEditor (*this); }
