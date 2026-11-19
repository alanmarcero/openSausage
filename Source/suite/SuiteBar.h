#pragma once
#include "PluginProcessor.h"

class SuiteBar final : public juce::Component {
public:
    explicit SuiteBar(SausageProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    void rebuild();
    void loadSelected();
    void savePreset();
    void renamePreset();
    void deletePresetFile();
    void toggleFavorite();
    const PresetLibrary::Entry* selected() const;
    SausageProcessor& processor;
    juce::TextButton undo {"Undo"}, redo {"Redo"}, slotA {"A"}, slotB {"B"}, copyAToB {"A>B"}, copyBToA {"B>A"};
    juce::TextButton favorite {"*"}, save {"Save"}, renameButton {"Rename"}, deleteButton {"Delete"};
    juce::TextEditor search;
    juce::ComboBox browser;
    std::vector<PresetLibrary::Entry> shown;
    bool rebuilding = false;
};
