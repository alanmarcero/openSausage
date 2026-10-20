#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
class SausageLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    SausageLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
};
