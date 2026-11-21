#pragma once
#include "ParameterUndo.h"

// Two full parameter slots. The audible slot is the live parameter state.
// The other slot is stored here and recalled with the session.
// Each slot has its own undo history; switching and copying are not undo steps.
class ABCompare {
public:
    ABCompare(juce::AudioProcessorValueTreeState&, ParameterUndo&);
    bool isB() const noexcept { return active == 1; }
    void toggle();
    void copyAToB();
    void copyBToA();
    void write(juce::ValueTree& state) const;
    void install(int slot, juce::ValueTree savedInactive);
private:
    juce::ValueTree capture() const;
    juce::ValueTree current(int slot) const;
    void applyTree(const juce::ValueTree&);
    void copyRaw(bool aToB);
    juce::AudioProcessorValueTreeState& parameters;
    ParameterUndo& history;
    juce::ValueTree inactive;
    int active = 0;
};
