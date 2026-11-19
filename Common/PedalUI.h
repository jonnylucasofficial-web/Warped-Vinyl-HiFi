// Shared pedal-style GUI widgets bound to APVTS parameters.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace pedalui
{
struct Palette
{
    juce::Colour body, bodyEdge, ink, knob, knobCap, pointer, accent, panel, panelInk;
};

//==============================================================================
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit PedalLookAndFeel (Palette p) : pal (p)
    {
        setColour (juce::Label::textColourId, pal.ink);
        setColour (juce::TextButton::buttonColourId, pal.panel);
        setColour (juce::TextButton::textColourOffId, pal.panelInk);
        setColour (juce::TextButton::buttonOnColourId, pal.accent);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Slider::textBoxTextColourId, pal.ink);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, pal.panel);
        setColour (juce::ComboBox::textColourId, pal.panelInk);
        setColour (juce::PopupMenu::backgroundColourId, pal.panel);
        setColour (juce::PopupMenu::textColourId, pal.panelInk);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        auto r = b.withSizeKeepingCentre (d, d);
        const float angle = start + pos * (end - start);

        // tick ring
        g.setColour (pal.ink.withAlpha (0.55f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = start + (float) i / 10.0f * (end - start);
            auto p1 = r.getCentre().getPointOnCircumference (d * 0.5f, a);
            auto p2 = r.getCentre().getPointOnCircumference (d * 0.43f, a);
            g.drawLine ({ p1, p2 }, 1.2f);
        }
        auto k = r.reduced (d * 0.12f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (k.translated (0, 2.5f));
        juce::ColourGradient grad (pal.knob.brighter (0.25f), k.getX(), k.getY(),
                                   pal.knob.darker (0.4f), k.getRight(), k.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (k);
        g.setColour (pal.knobCap);
        g.fillEllipse (k.reduced (k.getWidth() * 0.18f));
        g.setColour (pal.pointer);
        auto c = k.getCentre();
        g.drawLine ({ c.getPointOnCircumference (k.getWidth() * 0.12f, angle),
                      c.getPointOnCircumference (k.getWidth() * 0.47f, angle) }, juce::jmax (2.0f, d * 0.05f));
        if (s.isMouseOverOrDragging())
        {
            g.setColour (pal.accent.withAlpha (0.5f));
            g.drawEllipse (k.expanded (1.5f), 1.5f);
        }
    }

    Palette pal;
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& text, bool small = false)
        : isSmall (small)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (small ? 11.0f : 14.0f, juce::Font::bold));
        addAndMakeVisible (slider); addAndMakeVisible (label);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    }
    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromBottom (isSmall ? 14 : 18));
        slider.setBounds (b);
    }
    void setTooltip (const juce::String& t) { slider.setTooltip (t); label.setTooltip (t); }
    juce::Slider slider; juce::Label label;
private:
    bool isSmall;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

//==============================================================================
// Three-position mini toggle bound to a 3-choice parameter (index 0 = up).
class Toggle3 : public juce::Component, public juce::SettableTooltipClient
{
public:
    Toggle3 (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::String title,
             juce::StringArray positionNames, Palette p)
        : names (positionNames), heading (std::move (title)), pal (p)
    {
        param = s.getParameter (id);
        att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); }, nullptr);
        att->sendInitialUpdate();
        setTooltip (heading);
    }
    int index() const { return juce::roundToInt (param->convertFrom0to1 (param->getValue())); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (pal.ink);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (heading, b.removeFromTop (14), juce::Justification::centred);
        auto sw = b.removeFromLeft (b.getWidth() * 0.38f).withSizeKeepingCentre (16, 46);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (sw, 4);
        const int i = index();
        auto bat = sw.reduced (3).withHeight (14).withY (sw.getY() + 3 + (float) i * (sw.getHeight() - 20) / 2.0f);
        g.setColour (juce::Colours::silver); g.fillRoundedRectangle (bat, 3);
        g.setFont (juce::FontOptions (10.5f));
        for (int k = 0; k < 3; ++k)
        {
            g.setColour (k == i ? pal.accent.brighter (0.2f) : pal.ink.withAlpha (0.75f));
            g.drawText (names[k], juce::Rectangle<float> (b.getX() + 2, sw.getY() - 2 + (float) k * (sw.getHeight() - 12) / 2.0f,
                                                          b.getWidth(), 14), juce::Justification::centredLeft);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const float y = (float) e.y - 14.0f, h = (float) getHeight() - 14.0f;
        const int k = juce::jlimit (0, 2, (int) (y / (h / 3.0f)));
        att->setValueAsCompleteGesture ((float) k);
    }
private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::StringArray names; juce::String heading; Palette pal;
};

//==============================================================================
class DipSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    DipSwitch (juce::AudioProcessorValueTreeState& s, const juce::String& id, juce::String text,
               juce::String offText, juce::String onText, juce::String tip, Palette p)
        : label (std::move (text)), off (std::move (offText)), on (std::move (onText)), pal (p)
    {
        param = s.getParameter (id);
        att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); }, nullptr);
        att->sendInitialUpdate();
        setTooltip (tip);
    }
    bool isOn() const { return param->getValue() > 0.5f; }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        auto top = b.removeFromTop (12);
        g.setColour (pal.panelInk.withAlpha (0.6f)); g.setFont (juce::FontOptions (9.0f));
        g.drawText (on, top, juce::Justification::centred);
        auto lab = b.removeFromBottom (24);
        auto bot = b.removeFromBottom (11);
        g.drawText (off, bot, juce::Justification::centred);
        auto sw = b.withSizeKeepingCentre (14, juce::jmin (b.getHeight(), 30.0f));
        g.setColour (juce::Colour (0xff1d3f8f)); g.fillRoundedRectangle (sw, 2);
        auto knob = sw.reduced (2.5f).withHeight (sw.getHeight() * 0.42f);
        if (! isOn()) knob = knob.withY (sw.getBottom() - 2.5f - knob.getHeight());
        g.setColour (juce::Colours::white); g.fillRoundedRectangle (knob, 1.5f);
        g.setColour (pal.panelInk); g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawFittedText (label, lab.toNearestInt(), juce::Justification::centredTop, 2);
    }
    void mouseDown (const juce::MouseEvent&) override { att->setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f); }
private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::String label, off, on; Palette pal;
};

//==============================================================================
class Led : public juce::Component
{
public:
    void setColourAndLevel (juce::Colour c, float lvl) { if (c != col || lvl != level) { col = c; level = lvl; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (12, 12);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (r.expanded (2));
        g.setColour (col.withMultipliedBrightness (0.25f + 0.75f * level)); g.fillEllipse (r);
        if (level > 0.05f)
        {
            juce::ColourGradient glow (col.withAlpha (0.55f * level), r.getCentreX(), r.getCentreY(),
                                       col.withAlpha (0.0f), r.getCentreX() + 16, r.getCentreY(), true);
            g.setGradientFill (glow); g.fillEllipse (r.expanded (10));
        }
    }
private:
    juce::Colour col { juce::Colours::red }; float level = 0;
};

//==============================================================================
// Stomp footswitch. Latching by default; momentary when isMomentary() is true.
// Long-press (hold) callbacks are provided for hold functions.
class Footswitch : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    std::function<void (bool)> onState;         // latched/momentary state output
    std::function<void()> onPress;              // fires on every press
    std::function<void (bool)> onHold;          // true when hold starts, false on release after hold
    std::function<bool()> isMomentary = [] { return false; };
    std::function<bool()> getState = [] { return false; };
    juce::String caption;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        auto cap = b.removeFromBottom (16);
        auto r = b.withSizeKeepingCentre (juce::jmin (b.getWidth(), b.getHeight()) - 4, juce::jmin (b.getWidth(), b.getHeight()) - 4);
        juce::ColourGradient grad (juce::Colour (0xffe8e8e8), r.getX(), r.getY(), juce::Colour (0xff6d6d6d), r.getRight(), r.getBottom(), false);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (r.translated (0, 3));
        g.setGradientFill (grad); g.fillEllipse (r);
        g.setColour (juce::Colour (0xff9a9a9a)); g.fillEllipse (r.reduced (r.getWidth() * 0.22f).translated (0, pressed ? 1.5f : 0));
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.drawEllipse (r.reduced (r.getWidth() * 0.22f), 1);
        g.setColour (findColour (juce::Label::textColourId)); g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (caption, cap, juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override
    {
        pressed = true; held = false; repaint();
        if (onPress) onPress();
        if (isMomentary()) { if (onState) onState (true); }
        else if (onState) onState (! getState());
        startTimer (600);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        stopTimer();
        pressed = false; repaint();
        if (isMomentary() && onState) onState (false);
        if (held && onHold) onHold (false);
        held = false;
    }
private:
    void timerCallback() override { stopTimer(); held = true; if (onHold) onHold (true); }
    bool pressed = false, held = false;
};

//==============================================================================
inline void drawScrew (juce::Graphics& g, juce::Point<float> c)
{
    g.setColour (juce::Colour (0xffbdbdbd)); g.fillEllipse (juce::Rectangle<float> (9, 9).withCentre (c));
    g.setColour (juce::Colour (0xff6a6a6a)); g.drawLine (c.x - 3, c.y, c.x + 3, c.y, 1.4f);
}
} // namespace pedalui
