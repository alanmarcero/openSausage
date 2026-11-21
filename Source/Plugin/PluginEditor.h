#pragma once
#include "PluginProcessor.h"
#include "SausageLookAndFeel.h"
#include "SuiteBar.h"

class ModulePanel final : public juce::Component {
public:
    ModulePanel(juce::AudioProcessorValueTreeState&, const juce::String& title, const juce::String& subtitle,
                const juce::String& primaryId, const juce::String& primaryLabel, const juce::String& secondaryId,
                const juce::String& secondaryLabel, const juce::String& bypassId, juce::Colour accent, const juce::String& hint);
    void paint(juce::Graphics&) override;
    void resized() override;
    // Repaints the subtitle, LED, and beat display when what they show changed.
    void setBeatPhase(float position);
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
    std::array<float, 2> shownHeader {-1.0f, -1.0f};
    std::array<float, 3> shownDisplay {-1.0f, -1.0f, -1.0f};
    juce::Slider primary, secondary, saturationMix, dynamics, preDelay;
    juce::ComboBox focus, riseLength;
    juce::TextButton bypass;
    juce::AudioProcessorValueTreeState::SliderAttachment primaryAttachment, secondaryAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturationMixAttachment, dynamicsAttachment, preDelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> focusAttachment, riseLengthAttachment;
};

class SausageEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit SausageEditor(SausageProcessor&);
    ~SausageEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    // Holds every control at the design size. The editor scales it uniformly.
    struct Content final : juce::Component {
        explicit Content(SausageEditor& owner) : editor(owner) {}
        void paint(juce::Graphics& g) override { editor.paintContent(g); }
        SausageEditor& editor;
    };
    void timerCallback() override;
    void paintContent(juce::Graphics&);
    SausageProcessor& ownerProcessor;
    Content content {*this};
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
    SuiteBar suite;
    juce::TooltipWindow tooltip {this, 500};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SausageEditor)
};
