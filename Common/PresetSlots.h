// Pedal-style internal preset slots, stored with the plugin state.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace pedalui
{
class PresetSlots
{
public:
    PresetSlots (juce::AudioProcessorValueTreeState& s, int numSlots, juce::StringArray excludedIds)
        : apvts (s), excluded (std::move (excludedIds))
    {
        slots.resize ((size_t) numSlots);
    }

    int size() const { return (int) slots.size(); }
    bool isFilled (int i) const { return juce::isPositiveAndBelow (i, size()) && slots[(size_t) i].isValid(); }
    int getCurrent() const { return current; }

    void save (int i)
    {
        if (! juce::isPositiveAndBelow (i, size())) return;
        juce::ValueTree t ("Slot");
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! excluded.contains (rp->getParameterID()))
                    t.setProperty (rp->getParameterID(), rp->getValue(), nullptr);
        slots[(size_t) i] = t;
        current = i;
    }

    bool recall (int i)
    {
        if (! isFilled (i)) return false;
        const auto& t = slots[(size_t) i];
        for (int k = 0; k < t.getNumProperties(); ++k)
        {
            const auto id = t.getPropertyName (k).toString();
            if (auto* p = apvts.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost ((float) t.getProperty (id));
                p->endChangeGesture();
            }
        }
        current = i;
        return true;
    }

    juce::ValueTree toTree() const
    {
        juce::ValueTree t ("PresetSlots");
        t.setProperty ("current", current, nullptr);
        for (int i = 0; i < size(); ++i)
        {
            juce::ValueTree s ("Entry"); s.setProperty ("index", i, nullptr);
            if (slots[(size_t) i].isValid()) s.addChild (slots[(size_t) i].createCopy(), -1, nullptr);
            t.addChild (s, -1, nullptr);
        }
        return t;
    }

    void fromTree (const juce::ValueTree& t)
    {
        if (! t.hasType ("PresetSlots")) return;
        current = t.getProperty ("current", -1);
        for (auto e : t)
        {
            const int i = e.getProperty ("index", -1);
            if (juce::isPositiveAndBelow (i, size()))
                slots[(size_t) i] = e.getNumChildren() > 0 ? e.getChild (0).createCopy() : juce::ValueTree();
        }
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::StringArray excluded;
    std::vector<juce::ValueTree> slots;
    int current = -1;
};
} // namespace pedalui
