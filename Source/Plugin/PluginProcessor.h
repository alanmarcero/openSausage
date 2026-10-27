#pragma once
#include "SauceRack.h"
#include <juce_audio_processors/juce_audio_processors.h>

class SausageProcessor final : public juce::AudioProcessor {
public:
    SausageProcessor();
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
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    void applyRecipe(int index);
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputPeak {0.0f}, outputPeak {0.0f}, gainReduction {0.0f};
private:
    sausage::Rack rack;
    std::array<std::atomic<float>*, 15> values {};
    sausage::RackSettings settings(bool hostBypassed = false) const noexcept;
    void process(juce::AudioBuffer<float>&, bool hostBypassed);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SausageProcessor)
};
