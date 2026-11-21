#pragma once
#include "ParameterUndo.h"

// Two full parameter slots. The audible slot is the live parameter state.
// The other slot is stored here and recalled with the session.
// Each slot has its own undo history; switching and copying are not undo steps.
class ABCompare {
public:
    // The loaded preset's name, and whether it changed since that load or save.
    struct Label {
        juce::String name;
        bool edited = false;
    };
    ABCompare(juce::AudioProcessorValueTreeState&, ParameterUndo&);
    const Label& label() const noexcept { return labels[static_cast<std::size_t>(active)]; }
    void setLabel(Label);
    // Sets the label of the audible slot as part of the current undo step.
    void relabel(Label);
    bool isB() const noexcept { return active == 1; }
    void toggle();
    void copyAToB();
    void copyBToA();
    void write(juce::ValueTree& state) const;
    void install(int slot, juce::ValueTree savedInactive, Label live);
private:
    class LabelAction;
    juce::ValueTree capture() const;
    juce::ValueTree current(int slot) const;
    void applyTree(const juce::ValueTree&);
    void copyRaw(bool aToB);
    juce::AudioProcessorValueTreeState& parameters;
    ParameterUndo& history;
    juce::ValueTree inactive;
    std::array<Label, 2> labels;
    int active = 0;
};
