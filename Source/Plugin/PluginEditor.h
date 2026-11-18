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
    void setBeatPhase(float position) noexcept { phase = position; }
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
    juce::Slider primary, secondary, saturationMix, dynamics, preDelay;
    juce::ComboBox focus;
    juce::TextButton bypass;
    juce::AudioProcessorValueTreeState::SliderAttachment primaryAttachment, secondaryAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturationMixAttachment, dynamicsAttachment, preDelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> focusAttachment;
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
    std::unique_ptr<juce::Drawable> grill;
    juce::Image enamel;
    float animationPhase() const noexcept;
    ModulePanel sausagePanel, spacePanel, risePanel, widthPanel;
    juce::Slider output, masterMix;
    juce::TextButton bypass, softClip{"Soft Clip"};
    juce::ComboBox recipes, algorithms;
    juce::TextButton prevRecipe{"Previous"}, nextRecipe{"Next"};
    int currentRecipe = -1;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algorithmAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment outputAttachment, masterMixAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment, softClipAttachment;
    juce::TooltipWindow tooltip {this, 500};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SausageEditor)
};
