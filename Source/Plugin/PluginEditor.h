#pragma once
#include "PluginProcessor.h"
#include "SausageLookAndFeel.h"

class ModulePanel final : public juce::Component {
public:
    ModulePanel(juce::AudioProcessorValueTreeState&, const juce::String& title, const juce::String& subtitle,
                const juce::String& primaryId, const juce::String& primaryLabel, const juce::String& secondaryId,
                const juce::String& secondaryLabel, const juce::String& bypassId, juce::Colour accent, const juce::String& hint);
    void paint(juce::Graphics&) override;
    void resized() override;
    void advanceAnimation() noexcept { phase += 0.09f; }
private:
    juce::String title, subtitle, primaryLabel, secondaryLabel, hint;
    juce::Colour accent;
    std::atomic<float>* amount;
    std::atomic<float>* detail;
    std::atomic<float>* moduleBypass;
    std::atomic<float>* masterBypass;
    std::atomic<float>* algorithm;
    int kind = 0;
    float phase = 0.0f;
    juce::Slider primary, secondary;
    juce::TextButton bypass;
    juce::AudioProcessorValueTreeState::SliderAttachment primaryAttachment, secondaryAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;
};

class SausageEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit SausageEditor(SausageProcessor&);
    ~SausageEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    SausageProcessor& ownerProcessor;
    SausageLookAndFeel look;
    std::array<juce::Image, 3> faces;
    juce::Image enamel;
    float animationPhase = 0.0f;
    ModulePanel sausagePanel, spacePanel, risePanel, widthPanel;
    juce::Slider output;
    juce::TextButton bypass;
    juce::ComboBox recipes, algorithms;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algorithmAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment outputAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;
    juce::TooltipWindow tooltip {this, 500};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SausageEditor)
};
