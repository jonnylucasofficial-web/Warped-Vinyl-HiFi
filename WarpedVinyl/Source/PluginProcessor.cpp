#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
struct Factory { const char* name; float tone, lag, mix, rpm, depth, warp; int rise, fall; };
const Factory factory[] = {
    { "Warped 33",              0.55f, 0.15f, 1.00f, 0.30f, 0.35f, 0.70f, 0, 0 },
    { "Classic Chorus",         0.65f, 0.25f, 0.50f, 0.35f, 0.45f, 0.50f, 0, 0 },
    { "Seasick Vibrato",        0.50f, 0.10f, 1.00f, 0.45f, 0.75f, 0.50f, 1, 1 },
    { "Lush Lag Ensemble",      0.60f, 0.85f, 0.55f, 0.25f, 0.40f, 0.50f, 0, 0 },
    { "Fast Rotary-ish",        0.70f, 0.05f, 0.70f, 0.78f, 0.30f, 0.50f, 0, 0 },
    { "Wobbly Tape Wow",        0.45f, 0.30f, 1.00f, 0.15f, 0.55f, 0.20f, 0, 2 },
    { "Square-ish Pitch Steps", 0.60f, 0.10f, 1.00f, 0.40f, 0.40f, 0.50f, 2, 2 },
};
}

WarpedVinylProcessor::WarpedVinylProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WarpedVinylHiFi", createLayout()),
      slots (apvts, 4, { wvids::on, wvids::exp, wvids::expConnected })
{
    const juce::String knobIds[] = { wvids::lag, wvids::mix, wvids::rpm, wvids::depth, wvids::warp };
    for (int i = 0; i < 5; ++i) effective[(size_t) i] = val (knobIds[i]);
    startTimerHz (20);
}

APVTS::ParameterLayout WarpedVinylProcessor::createLayout()
{
    APVTS::ParameterLayout l;
    auto knob = [&] (const juce::String& id, const juce::String& name, float def,
                     std::function<juce::String (float, int)> text = nullptr)
    {
        if (! text) text = [] (float v, int) { return juce::String (v * 10.0f, 1); };
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                   juce::NormalisableRange<float> (0.0f, 1.0f), def,
                   juce::AudioParameterFloatAttributes().withStringFromValueFunction (text)));
    };
    knob (wvids::tone, "Tone", 0.6f);
    knob (wvids::lag, "Lag", 0.1f, [] (float v, int) { return juce::String (wv::Engine::lagToMs (v), 1) + " ms"; });
    knob (wvids::mix, "Mix", 0.5f, [] (float v, int) { return juce::String (juce::roundToInt (v * 100)) + "% wet"; });
    knob (wvids::rpm, "RPM", 0.4f, [] (float v, int) {
        const float hz = wv::rpmKnobToHz (v);
        return juce::String (juce::roundToInt (hz * 60.0f)) + " rpm (" + juce::String (hz, 2) + " Hz)"; });
    knob (wvids::depth, "Depth", 0.5f);
    knob (wvids::warp, "Warp", 0.5f);
    knob (wvids::rampSpeed, "Ramp Speed", 0.5f, [] (float v, int) {
        return juce::String (0.5f * std::pow (60.0f, 1.0f - v), 1) + " s"; });

    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { wvids::tapDiv, 1 }, "Tap Division",
                                                         juce::StringArray { "1 (3)", "2 (6)", "4 (8)" }, 2));
    const juce::StringArray shapes { "Smooth", "Linear", "Expo" };
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { wvids::rise, 1 }, "Rise Shape", shapes, 1));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { wvids::fall, 1 }, "Fall Shape", shapes, 1));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { wvids::on, 1 }, "Effect On", true));
    knob (wvids::exp, "Expression", 0.0f);
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { wvids::expConnected, 1 }, "Expression Connected", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { wvids::hostSync, 1 }, "Sync Tap to Host Tempo", false));

    const char* leftNames[]  = { "Assign: Lag", "Assign: Mix", "Assign: RPM", "Assign: Depth", "Assign: Warp",
                                 "Bounce", "Sweep (B/T)", "Polarity" };
    const char* rightNames[] = { "Lag Rise", "Mix Rise", "RPM Rise", "Depth Rise", "Warp Rise",
                                 "Tap Div 3-6-8", "Tap Control Ramp" };
    for (int i = 0; i < wvids::leftDips.size(); ++i)
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { wvids::leftDips[i], 1 }, leftNames[i], false));
    for (int i = 0; i < wvids::rightDips.size(); ++i)
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { wvids::rightDips[i], 1 }, rightNames[i], i < 5));
    return l;
}

bool WarpedVinylProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void WarpedVinylProcessor::prepareToPlay (double sampleRate, int)
{
    fs = sampleRate;
    engine.prepare ((float) sampleRate, juce::jmax (1, getTotalNumOutputChannels()));
}

float WarpedVinylProcessor::tapDivision() const
{
    static const float a[] = { 1, 2, 4 }, b[] = { 3, 6, 8 };
    const int i = juce::jlimit (0, 2, (int) val (wvids::tapDiv));
    return dip ("dipTapDiv") ? b[i] : a[i];
}

void WarpedVinylProcessor::setParamFromMidi (const juce::String& id, float normalised)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}

void WarpedVinylProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isProgramChange()) { pendingProgram = m.getProgramChangeNumber(); continue; }
        if (! m.isController()) continue;
        const int cc = m.getControllerNumber();
        const float v = (float) m.getControllerValue() / 127.0f;
        const bool b = m.getControllerValue() >= 64;
        auto three = [&] (const juce::String& id)
        {
            const int raw = m.getControllerValue();
            const int idx = raw <= 3 ? juce::jlimit (0, 2, raw - 1) : juce::jlimit (0, 2, raw * 3 / 128);
            setParamFromMidi (id, (float) idx / 2.0f);
        };
        switch (cc)
        {
            case 14: setParamFromMidi (wvids::tone, v); break;
            case 15: setParamFromMidi (wvids::lag, v); break;
            case 16: setParamFromMidi (wvids::mix, v); break;
            case 17: setParamFromMidi (wvids::rpm, v); break;
            case 18: setParamFromMidi (wvids::depth, v); break;
            case 19: setParamFromMidi (wvids::warp, v); break;
            case 20: setParamFromMidi (wvids::rampSpeed, v); break;
            case 21: three (wvids::tapDiv); break;
            case 22: three (wvids::rise); break;
            case 23: three (wvids::fall); break;
            case 93: tapRequests.fetch_add (1); break;            // any value = tap
            case 100: setParamFromMidi (wvids::exp, v); break;
            case 102: setParamFromMidi (wvids::on, b ? 1.0f : 0.0f); break;
            default:
                if (cc >= 61 && cc <= 68) setParamFromMidi (wvids::leftDips[cc - 61], b ? 1.0f : 0.0f);
                else if (cc >= 71 && cc <= 77) setParamFromMidi (wvids::rightDips[cc - 71], b ? 1.0f : 0.0f);
                break;
        }
    }
}

void WarpedVinylProcessor::timerCallback()
{
    const int pc = pendingProgram.exchange (-1);
    if (pc >= 0 && pc < slots.size()) slots.recall (pc);
}

void WarpedVinylProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    handleMidi (midi);
    midi.clear();

    const int numSamples = buffer.getNumSamples();
    const float blockSeconds = (float) (numSamples / fs);
    clockSeconds += numSamples / fs;

    const bool tapCtrlRamp = dip ("dipTapCtrl");
    const float div = tapDivision();
    const float rpmKnob = val (wvids::rpm), rampKnob = val (wvids::rampSpeed);

    // --- tap tempo ---------------------------------------------------------------
    auto applyTapPeriod = [&] (double period)
    {
        const float cycle = (float) (4.0 * period / div);            // seconds per LFO cycle
        if (tapCtrlRamp) { tapRampSeconds = 2.0f * cycle; rampFromTap = true; rampKnobAtTap = rampKnob; }   // ramp divisions 2x slower
        else             { tapRpmHz = 1.0f / cycle;       rpmFromTap  = true; rpmKnobAtTap  = rpmKnob;  }
    };
    for (int n = tapRequests.exchange (0); n > 0; --n)
    {
        if (tap.tap (clockSeconds)) applyTapPeriod (tap.period);
        if (! tapCtrlRamp && n == 1) engine.resetPhase();             // tapping re-syncs the wave
    }
    if (val (wvids::hostSync) > 0.5f)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto bpm = pos->getBpm()) applyTapPeriod (60.0 / *bpm);

    // turning the knob takes control back from tap
    if (rpmFromTap && std::abs (rpmKnob - rpmKnobAtTap) > 0.02f) rpmFromTap = false;
    if (rampFromTap && std::abs (rampKnob - rampKnobAtTap) > 0.02f) rampFromTap = false;
    tapOverridesRpm = rpmFromTap; tapOverridesRamp = rampFromTap;

    // --- ramping / expression -------------------------------------------------------
    for (int i = 0; i < 5; ++i)
    {
        modulator.cfg.assigned[(size_t) i] = dip (wvids::leftDips[i]);
        modulator.cfg.rise[(size_t) i] = dip (wvids::rightDips[i]);
    }
    modulator.cfg.bounce = dip ("dipBounce");
    modulator.cfg.sweepTop = dip ("dipSweep");
    modulator.cfg.polarity = dip ("dipPolarity");
    modulator.cfg.expConnected = val (wvids::expConnected) > 0.5f;
    modulator.cfg.rampEnabled = true;

    const bool on = val (wvids::on) > 0.5f;
    const bool holding = holdRequested.load();
    if ((on && ! wasOn) || (holding && ! wasHolding) || (! on && ! holding)) modulator.resetRamp();   // ramp restarts on engage / hold
    modulator.holdActive = holding;
    wasOn = on; wasHolding = holding;

    const float rampSeconds = rampFromTap ? tapRampSeconds : 0.5f * std::pow (60.0f, 1.0f - rampKnob);
    if (on || holding) modulator.advance (blockSeconds, rampSeconds);
    rampPosition = modulator.rampPos;

    const std::array<float, 5> knobs { val (wvids::lag), val (wvids::mix), rpmKnob, val (wvids::depth), val (wvids::warp) };
    const auto k = modulator.apply (knobs, val (wvids::exp));
    for (int i = 0; i < 5; ++i) effective[(size_t) i] = k[(size_t) i];

    const bool rpmModulated = k[2] != rpmKnob;
    wv::Params p;
    p.tone = val (wvids::tone);
    p.lag = k[0]; p.mix = k[1]; p.depth = k[3]; p.warp = k[4];
    p.rateHz = (rpmFromTap && ! rpmModulated) ? tapRpmHz : wv::rpmKnobToHz (k[2]);
    p.rise = (wv::Shape) (int) val (wvids::rise);
    p.fall = (wv::Shape) (int) val (wvids::fall);
    p.on = on;
    currentRateHz = p.rateHz;
    engine.setParams (p);

    engine.process (buffer.getArrayOfWritePointers(), juce::jmin (2, buffer.getNumChannels()), numSamples);
    lfoPhase = (float) engine.getPhase();
}

int WarpedVinylProcessor::getNumPrograms() { return (int) std::size (factory); }
const juce::String WarpedVinylProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, getNumPrograms()) ? factory[i].name : "";
}
void WarpedVinylProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    const auto& f = factory[i];
    auto set = [&] (const juce::String& id, float v) { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set (wvids::tone, f.tone); set (wvids::lag, f.lag); set (wvids::mix, f.mix); set (wvids::rpm, f.rpm);
    set (wvids::depth, f.depth); set (wvids::warp, f.warp);
    set (wvids::rise, (float) f.rise); set (wvids::fall, (float) f.fall);
}

void WarpedVinylProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("WarpedVinylState");
    root.addChild (apvts.copyState(), -1, nullptr);
    root.addChild (slots.toTree(), -1, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, dest);
}

void WarpedVinylProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto root = juce::ValueTree::fromXml (*xml);
        auto params = root.getChildWithName (apvts.state.getType());
        if (params.isValid()) apvts.replaceState (params);
        slots.fromTree (root.getChildWithName ("PresetSlots"));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WarpedVinylProcessor(); }
