#pragma once
#include "ABCompare.h"
#include "PresetLibrary.h"
#include "SauceRack.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

struct SausageRecipe {
    const char* name;
    float fatness, color, space, size, rise, riseMix, width, spread, output;
    float satMix = 100.0f, satDynamics = 100.0f, satFocus = 0.0f, preDelay = 0.0f, masterMix = 100.0f, softClip = 0.0f;
};

class SausageProcessor final : public juce::AudioProcessor {
public:
    explicit SausageProcessor(juce::File presetFolder = PresetLibrary::userDirectory("openSausage"));
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "openSausage"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    void applyRecipe(int index);
    void loadParameterTree(const juce::ValueTree&);
    juce::ValueTree copyParameterTree();
    static const std::vector<SausageRecipe>& factoryRecipes();
    juce::AudioProcessorValueTreeState parameters;
    ParameterUndo history;
    ABCompare compare;
    PresetLibrary presets;
    std::atomic<float> beatPhase {-1.0f};
    std::atomic<float> inputPeak {0.0f}, outputPeak {0.0f}, gainReduction {0.0f};
private:
    struct MusicalClock { double ppq = 0, tempo = 120, quartersPerBar = 4; bool running = true; };
    sausage::Rack rack;
    double visualQuarter = 0.0, clockRate = 44100.0;
    int currentProgram = 0;
    MusicalClock readClock(int samples);
    static constexpr int parameterCount = 22;
    std::array<std::atomic<float>*, parameterCount> values {};
    sausage::RackSettings settings(bool hostBypassed = false) const noexcept;
    void process(juce::AudioBuffer<float>&, bool hostBypassed);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SausageProcessor)
};
