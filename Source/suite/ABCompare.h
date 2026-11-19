#pragma once
#include "ParameterUndo.h"

// Two full parameter slots. The audible slot is the live parameter state.
// The other slot is stored here and recalled with the session.
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
    friend class ABToggleAction;
    friend class ABCopyAction;
    juce::ValueTree capture() const;
    juce::ValueTree current(int slot) const;
    void applyTree(const juce::ValueTree&);
    void swap();
    void copyRaw(bool aToB);
    void restore(juce::ValueTree live, juce::ValueTree savedInactive, int slot);
    juce::AudioProcessorValueTreeState& parameters;
    ParameterUndo& history;
    juce::ValueTree inactive;
    int active = 0;
};
