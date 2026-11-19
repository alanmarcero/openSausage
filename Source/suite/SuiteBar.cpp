#include "SuiteBar.h"

namespace {
void styleSearch(juce::TextEditor& editor) {
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff152026));
    editor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe6f0f5));
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff283842));
    editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff48f0c8));
    editor.setJustification(juce::Justification::centredLeft);
    editor.setIndents(6, 2);
    editor.setFont(juce::FontOptions(13.0f));
}
}
SuiteBar::SuiteBar(SausageProcessor& owner) : processor(owner) {
    const auto add = [this](juce::TextButton& button, const juce::String& id, const juce::String& tip) {
        button.setComponentID(id);
        button.setName(button.getButtonText());
        button.setTooltip(tip);
        addAndMakeVisible(button);
    };
    add(undo, "suiteUndo", "Undo the last control change. Cmd/Ctrl+Z.");
    add(redo, "suiteRedo", "Redo the control change. Shift+Cmd/Ctrl+Z.");
    add(slotA, "suiteA", "Hear slot A.");
    add(slotB, "suiteB", "Hear slot B.");
    add(copyAToB, "suiteCopyAToB", "Copy A into B.");
    add(copyBToA, "suiteCopyBToA", "Copy B into A.");
    add(favorite, "suiteFavorite", "Star or unstar the selected preset. Stars stay at the top of the list.");
    add(save, "suiteSave", "Save the current sound. Uses the search text as the name.");
    add(renameButton, "suiteRename", "Rename the selected saved preset to the search text. Factory recipes stay read-only.");
    add(deleteButton, "suiteDelete", "Delete the selected saved preset. Factory recipes stay read-only.");
    undo.onClick = [this] { processor.history.undo(); refresh(); };
    redo.onClick = [this] { processor.history.redo(); refresh(); };
    slotA.onClick = [this] { if (processor.compare.isB()) processor.compare.toggle(); refresh(); };
    slotB.onClick = [this] { if (!processor.compare.isB()) processor.compare.toggle(); refresh(); };
    copyAToB.onClick = [this] { processor.compare.copyAToB(); refresh(); };
    copyBToA.onClick = [this] { processor.compare.copyBToA(); refresh(); };
    favorite.onClick = [this] { toggleFavorite(); };
    save.onClick = [this] { savePreset(); };
    renameButton.onClick = [this] { renamePreset(); };
    deleteButton.onClick = [this] { deletePresetFile(); };
    search.setComponentID("presetSearch");
    search.setName("Preset search");
    search.setTooltip("Filter factory and saved presets. Save uses this text as the name. Rename uses it as the new name.");
    search.setTextToShowWhenEmpty("Search presets", juce::Colour(0xff869ca8));
    search.setMultiLine(false);
    search.setReturnKeyStartsNewLine(false);
    styleSearch(search);
    search.onTextChange = [this] { rebuild(); };
    search.onReturnKey = [this] { savePreset(); };
    addAndMakeVisible(search);
    browser.setComponentID("presetBrowser");
    browser.setName("Preset browser");
    browser.setTooltip("Factory recipes and presets saved in Documents/Alan Marcero/openSausage/Presets.");
    browser.setTextWhenNothingSelected("Presets");
    browser.onChange = [this] { loadSelected(); };
    addAndMakeVisible(browser);
    rebuild();
}
void SuiteBar::paint(juce::Graphics& graphics) {
    graphics.setColour(juce::Colour(0xff0c1418));
    graphics.fillRect(getLocalBounds());
}
void SuiteBar::resized() {
    auto row = getLocalBounds().reduced(8, 3);
    const auto take = [&row](int width) {
        const auto area = row.removeFromLeft(width);
        row.removeFromLeft(4);
        return area;
    };
    undo.setBounds(take(58));
    redo.setBounds(take(58));
    row.removeFromLeft(6);
    slotA.setBounds(take(32));
    slotB.setBounds(take(32));
    copyAToB.setBounds(take(52));
    copyBToA.setBounds(take(52));
    row.removeFromLeft(8);
    search.setBounds(take(168));
    deleteButton.setBounds(row.removeFromRight(70));
    row.removeFromRight(4);
    renameButton.setBounds(row.removeFromRight(76));
    row.removeFromRight(4);
    save.setBounds(row.removeFromRight(58));
    row.removeFromRight(4);
    favorite.setBounds(row.removeFromRight(34));
    row.removeFromRight(6);
    browser.setBounds(row);
}
const PresetLibrary::Entry* SuiteBar::selected() const {
    const auto id = browser.getSelectedId();
    if (id <= 0 || id > static_cast<int>(shown.size())) return nullptr;
    return &shown[static_cast<std::size_t>(id - 1)];
}
void SuiteBar::rebuild() {
    const auto keep = selected() != nullptr ? selected()->name : juce::String();
    rebuilding = true;
    browser.clear(juce::dontSendNotification);
    shown = processor.presets.entries(search.getText());
    int previousGroup = -1;
    for (int index = 0; index < static_cast<int>(shown.size()); ++index) {
        const auto& entry = shown[static_cast<std::size_t>(index)];
        const int group = entry.favorite ? 0 : entry.factory ? 1 : 2;
        if (previousGroup != -1 && group != previousGroup) browser.addSeparator();
        previousGroup = group;
        auto label = entry.name;
        if (entry.favorite) label = "* " + label;
        browser.addItem(label, index + 1);
    }
    for (int index = 0; index < static_cast<int>(shown.size()); ++index)
        if (shown[static_cast<std::size_t>(index)].name == keep) {
            browser.setSelectedId(index + 1, juce::dontSendNotification);
            break;
        }
    rebuilding = false;
    refresh();
}
void SuiteBar::loadSelected() {
    if (rebuilding) return;
    const auto* entry = selected();
    if (entry == nullptr) return;
    if (entry->factory) processor.setCurrentProgram(entry->factoryIndex);
    else if (const auto tree = processor.presets.loadUser(entry->name)) processor.loadParameterTree(*tree);
    refresh();
}
void SuiteBar::savePreset() {
    auto name = search.getText().trim();
    if (name.isEmpty()) {
        name = "Preset";
        for (int number = 2; processor.presets.loadUser(name) || processor.presets.isFactoryName(name); ++number)
            name = "Preset " + juce::String(number);
    }
    juce::String error;
    if (!processor.presets.save(name, processor.copyParameterTree(), error)) {
        save.setTooltip(error);
        return;
    }
    save.setTooltip("Save the current sound. Uses the search text as the name.");
    rebuild();
    for (int index = 0; index < static_cast<int>(shown.size()); ++index)
        if (shown[static_cast<std::size_t>(index)].name.equalsIgnoreCase(PresetLibrary::sanitise(name))) {
            browser.setSelectedId(index + 1, juce::dontSendNotification);
            break;
        }
    refresh();
}
void SuiteBar::renamePreset() {
    const auto* entry = selected();
    if (entry == nullptr || entry->factory) return;
    juce::String error;
    if (!processor.presets.rename(entry->name, search.getText(), error)) {
        renameButton.setTooltip(error);
        return;
    }
    renameButton.setTooltip("Rename the selected saved preset to the search text. Factory recipes stay read-only.");
    search.clear();
    rebuild();
}
void SuiteBar::deletePresetFile() {
    const auto* entry = selected();
    if (entry == nullptr || entry->factory) return;
    juce::String error;
    if (!processor.presets.remove(entry->name, error)) {
        deleteButton.setTooltip(error);
        return;
    }
    deleteButton.setTooltip("Delete the selected saved preset. Factory recipes stay read-only.");
    rebuild();
}
void SuiteBar::toggleFavorite() {
    const auto* entry = selected();
    if (entry == nullptr) return;
    processor.presets.setFavorite(entry->name, !entry->favorite);
    rebuild();
}
void SuiteBar::refresh() {
    undo.setEnabled(processor.history.canUndo());
    redo.setEnabled(processor.history.canRedo());
    const bool hearingB = processor.compare.isB();
    slotA.setToggleState(!hearingB, juce::dontSendNotification);
    slotB.setToggleState(hearingB, juce::dontSendNotification);
    const auto* entry = selected();
    const bool user = entry != nullptr && !entry->factory;
    renameButton.setEnabled(user);
    deleteButton.setEnabled(user);
    favorite.setEnabled(entry != nullptr);
    favorite.setToggleState(entry != nullptr && entry->favorite, juce::dontSendNotification);
}
