#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "WVDsp.h"
#include "../../Common/Modulation.h"
#include "../../Common/PresetSlots.h"

namespace wvids
{
    inline const juce::String tone = "tone", lag = "lag", mix = "mix", rpm = "rpm", depth = "depth", warp = "warp",
                              tapDiv = "tapDiv", rise = "riseShape", fall = "fallShape",
                              on = "on", rampSpeed = "rampSpeed", exp = "expression", expConnected = "expConnected",
                              hostSync = "hostSync";
    // Left bank: ramp / expression assignment + behaviour
    inline const juce::StringArray leftDips { "dipLag", "dipMix", "dipRpm", "dipDepth", "dipWarp",
                                              "dipBounce", "dipSweep", "dipPolarity" };
    // Right bank: per-parameter direction (rise/fall) + tap options
    inline const juce::StringArray rightDips { "dirLag", "dirMix", "dirRpm", "dirDepth", "dirWarp",
                                               "dipTapDiv", "dipTapCtrl" };
}

class WarpedVinylProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    WarpedVinylProcessor();
    ~WarpedVinylProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // GUI → audio thread events
    void requestTap()              { tapRequests.fetch_add (1); }
    void setHold (bool h)          { holdRequested = h; }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    pedalui::PresetSlots slots;

    // State shown in the GUI
    std::atomic<float> lfoPhase { 0.0f }, rampPosition { 0.0f }, currentRateHz { 1.0f };
    std::atomic<bool> tapOverridesRpm { false }, tapOverridesRamp { false };
    std::array<std::atomic<float>, 5> effective {};

private:
    bool dip (const juce::String& id) const { return apvts.getRawParameterValue (id)->load() > 0.5f; }
    float val (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void handleMidi (const juce::MidiBuffer&);
    void setParamFromMidi (const juce::String& id, float normalised);
    void timerCallback() override;
    float tapDivision() const;

    wv::Engine engine;
    cbdsp::KnobModulator<5> modulator;
    cbdsp::TapTempo tap;
    double fs = 48000.0, clockSeconds = 0.0;
    std::atomic<int> tapRequests { 0 }, pendingProgram { -1 };
    std::atomic<bool> holdRequested { false };
    bool wasOn = false, wasHolding = false;
    float tapRpmHz = 1.0f, tapRampSeconds = 4.0f, rpmKnobAtTap = 0.0f, rampKnobAtTap = 0.0f;
    bool rpmFromTap = false, rampFromTap = false;
    int currentProgram = 0;
};
