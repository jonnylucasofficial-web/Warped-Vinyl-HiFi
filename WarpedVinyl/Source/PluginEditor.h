#pragma once
#include "PluginProcessor.h"
#include "../../Common/PedalUI.h"

class WarpedVinylEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WarpedVinylEditor (WarpedVinylProcessor&);
    ~WarpedVinylEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void presetClicked (int slot);

    WarpedVinylProcessor& proc;
    pedalui::PedalLookAndFeel lnf;
    pedalui::Knob tone, lag, mix, rpm, depth, warp, ramp, expKnob;
    pedalui::Toggle3 tapDiv, riseShape, fallShape;
    pedalui::Footswitch bypassFs, tapFs;
    pedalui::Led bypassLed, tapLed;
    juce::OwnedArray<pedalui::DipSwitch> dips;
    juce::ToggleButton expConnected { "Expression / CV connected" }, hostSync { "Sync tap to host tempo" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> expAtt, syncAtt;
    juce::OwnedArray<juce::TextButton> presetButtons;
    juce::TextButton saveBtn { "SAVE" };
    juce::Label status;
    juce::TooltipWindow tooltips { this, 400 };
    juce::Rectangle<int> face, back, wavePanel;
    float spin = 0.0f;
};
