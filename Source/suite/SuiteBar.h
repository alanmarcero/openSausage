#pragma once
#include "PluginProcessor.h"

class SuiteBar final : public juce::Component {
public:
    // The editor shows this row in its header.
    class PresetRow final : public juce::Component {
    public:
        void resized() override;
        juce::TextButton previous {"Previous preset"}, next {"Next preset"}, favorite {"Favorite"};
        juce::ComboBox browser;
        juce::Label status;
    };
    explicit SuiteBar(SausageProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
    PresetRow presetRow;
private:
    void rebuild();
    void load(const PresetLibrary::Entry&);
    void step(int direction);
    void savePreset();
    void renamePreset();
    void deletePresetFile();
    void toggleFavorite();
    void disarm();
    void report(const juce::String& message, bool isError = false);
    const PresetLibrary::Entry* selected() const;
    SausageProcessor& processor;
    juce::TextButton undo {"Undo"}, redo {"Redo"}, slotA {"A"}, slotB {"B"}, copyAToB {"Copy A to B"}, copyBToA {"Copy B to A"};
    juce::TextButton save {"Save"}, renameButton {"Rename"}, deleteButton {"Delete"};
    juce::TextEditor search, name;
    std::vector<PresetLibrary::Entry> shown;
    juce::String shownLabel;
    juce::Button* armed = nullptr;
    juce::uint32 armedAt = 0;
    bool rebuilding = false;
};
