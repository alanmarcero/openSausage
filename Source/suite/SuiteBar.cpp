#include "SuiteBar.h"

namespace {
constexpr juce::uint32 confirmWindowMs = 3000;
void styleSearch(juce::TextEditor& editor) {
    editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff152026));
    editor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe6f0f5));
    editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff283842));
    editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff48f0c8));
    editor.setJustification(juce::Justification::centredLeft);
    editor.setIndents(6, 2);
    editor.setFont(juce::FontOptions(14.0f));
}
}
SuiteBar::SuiteBar(SausageProcessor& owner) : processor(owner) {
    const auto add = [](juce::Component& parent, juce::TextButton& button, const juce::String& id,
                        const juce::String& tip, const juce::String& title = {}) {
        const auto spoken = title.isNotEmpty() ? title : button.getButtonText();
        button.setComponentID(id);
        button.setName(spoken);
        button.setTitle(spoken);
        button.setTooltip(tip);
        parent.addAndMakeVisible(button);
    };
    add(*this, undo, "suiteUndo", "Undo the last control change. Cmd/Ctrl+Z.");
    add(*this, redo, "suiteRedo", "Redo the control change. Shift+Cmd/Ctrl+Z.");
    add(*this, slotA, "suiteA", "Hear slot A.", "Slot A");
    add(*this, slotB, "suiteB", "Hear slot B.", "Slot B");
    add(*this, copyAToB, "suiteCopyAToB", "Copy A into B.");
    add(*this, copyBToA, "suiteCopyBToA", "Copy B into A.");
    add(presetRow, presetRow.previous, "suitePrevious", "Previous preset");
    add(presetRow, presetRow.next, "suiteNext", "Next preset");
    add(presetRow, presetRow.favorite, "suiteFavorite", "Star or unstar the current preset. Stars stay at the top of the list.");
    add(*this, save, "suiteSave", "Save the current sound under the preset name. Asks before replacing a saved preset.");
    add(*this, renameButton, "suiteRename", "Rename the current saved preset to the preset name. Factory recipes stay read-only.");
    add(*this, deleteButton, "suiteDelete", "Delete the current saved preset. Asks first. Factory recipes stay read-only.");
    undo.onClick = [this] { processor.history.undo(); refresh(); };
    redo.onClick = [this] { processor.history.redo(); refresh(); };
    slotA.onClick = [this] { if (processor.compare.isB()) processor.compare.toggle(); refresh(); };
    slotB.onClick = [this] { if (!processor.compare.isB()) processor.compare.toggle(); refresh(); };
    copyAToB.onClick = [this] { processor.compare.copyAToB(); refresh(); };
    copyBToA.onClick = [this] { processor.compare.copyBToA(); refresh(); };
    presetRow.previous.onClick = [this] { step(-1); };
    presetRow.next.onClick = [this] { step(1); };
    presetRow.favorite.onClick = [this] { toggleFavorite(); };
    save.onClick = [this] { savePreset(); };
    renameButton.onClick = [this] { renamePreset(); };
    deleteButton.onClick = [this] { deletePresetFile(); };
    search.setComponentID("presetSearch");
    search.setName("Preset search");
    search.setTooltip("Filter factory and saved presets. Return loads the first match.");
    search.setTextToShowWhenEmpty("Search presets", juce::Colour(0xff869ca8));
    search.setMultiLine(false);
    search.setReturnKeyStartsNewLine(false);
    styleSearch(search);
    search.onTextChange = [this] {
        rebuild();
        if (search.isEmpty()) { report({}); return; }
        if (shown.empty()) { report("No presets match.", true); return; }
        report("Return loads " + shown.front().name + ".");
    };
    search.onReturnKey = [this] { if (!shown.empty()) load(shown.front()); };
    addAndMakeVisible(search);
    name.setComponentID("presetName");
    name.setName("Preset name");
    name.setTooltip("Name for Save and Rename. Return saves.");
    name.setTextToShowWhenEmpty("Preset name", juce::Colour(0xff869ca8));
    name.setMultiLine(false);
    name.setReturnKeyStartsNewLine(false);
    styleSearch(name);
    name.onTextChange = [this] { disarm(); };
    name.onReturnKey = [this] { savePreset(); };
    addAndMakeVisible(name);
    presetRow.status.setComponentID("presetStatus");
    presetRow.status.setFont(juce::FontOptions(13.0f));
    presetRow.status.setJustificationType(juce::Justification::centredLeft);
    presetRow.addAndMakeVisible(presetRow.status);
    auto& browser = presetRow.browser;
    browser.setComponentID("presetBrowser");
    browser.setName("Preset browser");
    browser.setTitle("Preset browser");
    browser.setTooltip("Factory recipes and presets saved in Documents/Alan Marcero/openSausage/Presets. An asterisk marks unsaved edits.");
    browser.onChange = [this] {
        if (const auto* entry = selected(); entry != nullptr && !rebuilding) load(*entry);
    };
    presetRow.addAndMakeVisible(browser);
    rebuild();
}
void SuiteBar::paint(juce::Graphics& graphics) {
    graphics.setColour(juce::Colour(0xff0c1418));
    graphics.fillRect(getLocalBounds());
}
void SuiteBar::resized() {
    // The right end stays clear of the editor's resize corner.
    auto row = getLocalBounds().reduced(8, 4).withTrimmedRight(32);
    const auto take = [&row](int width) {
        const auto area = row.removeFromLeft(width);
        row.removeFromLeft(4);
        return area;
    };
    undo.setBounds(take(64));
    redo.setBounds(take(64));
    row.removeFromLeft(8);
    slotA.setBounds(take(40));
    slotB.setBounds(take(40));
    copyAToB.setBounds(take(108));
    copyBToA.setBounds(take(108));
    row.removeFromLeft(8);
    search.setBounds(take(180));
    row.removeFromLeft(8);
    deleteButton.setBounds(row.removeFromRight(76));
    row.removeFromRight(4);
    renameButton.setBounds(row.removeFromRight(84));
    row.removeFromRight(4);
    save.setBounds(row.removeFromRight(84));
    row.removeFromRight(4);
    name.setBounds(row);
}
const PresetLibrary::Entry* SuiteBar::selected() const {
    const auto id = presetRow.browser.getSelectedId();
    if (id <= 0 || id > static_cast<int>(shown.size())) return nullptr;
    return &shown[static_cast<std::size_t>(id - 1)];
}
void SuiteBar::rebuild() {
    auto& browser = presetRow.browser;
    rebuilding = true;
    browser.clear(juce::dontSendNotification);
    shown = processor.presets.entries(search.getText());
    const auto& label = processor.compare.label();
    shownLabel = label.edited ? label.name + " *" : label.name;
    browser.setTextWhenNothingSelected(shownLabel);
    // Without a search, factory recipes sit in one submenu per genre.
    juce::PopupMenu genre;
    juce::String genreName;
    const auto closeGenre = [&] {
        if (genre.getNumItems() > 0) browser.getRootMenu()->addSubMenu(genreName, genre);
        genre = {};
    };
    int previousGroup = -1;
    for (int index = 0; index < static_cast<int>(shown.size()); ++index) {
        const auto& entry = shown[static_cast<std::size_t>(index)];
        const int group = entry.favorite ? 0 : entry.factory ? 1 : 2;
        if (previousGroup != -1 && group != previousGroup) {
            closeGenre();
            browser.addSeparator();
        }
        previousGroup = group;
        juce::PopupMenu::Item item(entry.name);
        item.setID(index + 1);
        if (entry.name.equalsIgnoreCase(label.name)) item.text = shownLabel;
        if (entry.favorite) {
            juce::Path shape;
            shape.addStar({8.0f, 8.0f}, 5, 3.0f, 7.5f);
            auto star = std::make_unique<juce::DrawablePath>();
            star->setPath(shape);
            star->setFill(juce::Colour(0xff48f0c8));
            item.setImage(std::move(star));
        }
        if (group != 1 || search.getText().isNotEmpty()) {
            browser.getRootMenu()->addItem(std::move(item));
            continue;
        }
        const auto entryGenre = entry.name.upToFirstOccurrenceOf(" / ", false, false);
        if (entryGenre != genreName) closeGenre();
        genreName = entryGenre;
        genre.addItem(std::move(item));
    }
    closeGenre();
    const auto current = std::ranges::find_if(shown, [&label](const PresetLibrary::Entry& entry) { return entry.name.equalsIgnoreCase(label.name); });
    if (current != shown.end()) browser.setSelectedId(static_cast<int>(std::distance(shown.begin(), current)) + 1, juce::dontSendNotification);
    rebuilding = false;
    refresh();
}
void SuiteBar::load(const PresetLibrary::Entry& entry) {
    disarm();
    report({});
    if (entry.factory) processor.setCurrentProgram(entry.factoryIndex);
    const auto tree = processor.presets.loadUser(entry.name);
    if (tree) processor.loadParameterTree(*tree, entry.name);
    if (!entry.factory && !tree) report("Could not load " + entry.name + ".", true);
    name.setText(entry.factory ? juce::String() : entry.name, false);
    rebuild();
}
void SuiteBar::step(int direction) {
    if (shown.empty()) return;
    const auto count = static_cast<int>(shown.size());
    // From an unlisted preset, Next starts at the first entry and Previous at the last.
    const int from = std::max(presetRow.browser.getSelectedId() - 1, direction > 0 ? -1 : 0);
    load(shown[static_cast<std::size_t>((from + direction + count) % count)]);
}
void SuiteBar::savePreset() {
    auto presetName = name.getText().trim();
    if (presetName.isEmpty()) {
        presetName = "Preset";
        for (int number = 2; processor.presets.loadUser(presetName) || processor.presets.isFactoryName(presetName); ++number)
            presetName = "Preset " + juce::String(number);
    }
    if (processor.presets.loadUser(presetName) && armed != &save) {
        disarm();
        armed = &save;
        armedAt = juce::Time::getMillisecondCounter();
        save.setButtonText("Replace?");
        report(presetName + " exists. Click Replace? to overwrite it.");
        return;
    }
    disarm();
    juce::String error;
    if (!processor.presets.save(presetName, processor.copyParameterTree(), error)) {
        report(error, true);
        return;
    }
    const auto saved = PresetLibrary::sanitise(presetName);
    processor.compare.setLabel({saved, false});
    report("Saved " + saved + ".");
    search.setText({}, false);
    rebuild();
}
void SuiteBar::renamePreset() {
    const auto* entry = selected();
    if (entry == nullptr || entry->factory) return;
    const auto from = entry->name;
    juce::String error;
    if (!processor.presets.rename(from, name.getText(), error)) {
        report(error, true);
        return;
    }
    const auto renamed = PresetLibrary::sanitise(name.getText());
    processor.compare.setLabel({renamed, processor.compare.label().edited});
    report("Renamed " + from + " to " + renamed + ".");
    search.setText({}, false);
    rebuild();
}
void SuiteBar::deletePresetFile() {
    const auto* entry = selected();
    if (entry == nullptr || entry->factory) return;
    const auto presetName = entry->name;
    if (armed != &deleteButton) {
        disarm();
        armed = &deleteButton;
        armedAt = juce::Time::getMillisecondCounter();
        deleteButton.setButtonText("Delete?");
        report("Click Delete? again to remove " + presetName + ".");
        return;
    }
    disarm();
    juce::String error;
    if (!processor.presets.remove(presetName, error)) {
        report(error, true);
        return;
    }
    // The sound is no longer saved anywhere.
    processor.compare.setLabel({presetName, true});
    report("Deleted " + presetName + ".");
    rebuild();
}
void SuiteBar::toggleFavorite() {
    const auto* entry = selected();
    if (entry == nullptr) return;
    processor.presets.setFavorite(entry->name, !entry->favorite);
    rebuild();
}
void SuiteBar::disarm() {
    armed = nullptr;
    save.setButtonText("Save");
    deleteButton.setButtonText("Delete");
}
void SuiteBar::report(const juce::String& message, bool isError) {
    presetRow.status.setText(message, juce::dontSendNotification);
    presetRow.status.setColour(juce::Label::textColourId, juce::Colour(isError ? 0xffffbe3b : 0xffa3afaf));
}
void SuiteBar::PresetRow::resized() {
    auto row = getLocalBounds();
    status.setBounds(row.removeFromBottom(20));
    auto controls = row.removeFromTop(40);
    previous.setBounds(controls.removeFromLeft(40));
    favorite.setBounds(controls.removeFromRight(40));
    controls.removeFromRight(6);
    next.setBounds(controls.removeFromRight(40));
    browser.setBounds(controls.reduced(6, 2));
}
void SuiteBar::refresh() {
    if (armed != nullptr && juce::Time::getMillisecondCounter() - armedAt > confirmWindowMs) disarm();
    const auto& label = processor.compare.label();
    if ((label.edited ? label.name + " *" : label.name) != shownLabel) rebuild();
    undo.setEnabled(processor.history.canUndo());
    redo.setEnabled(processor.history.canRedo());
    const bool hearingB = processor.compare.isB();
    slotA.setToggleState(!hearingB, juce::dontSendNotification);
    slotB.setToggleState(hearingB, juce::dontSendNotification);
    const auto* entry = selected();
    const bool user = entry != nullptr && !entry->factory;
    renameButton.setEnabled(user);
    deleteButton.setEnabled(user);
    presetRow.favorite.setEnabled(entry != nullptr);
    presetRow.favorite.setToggleState(entry != nullptr && entry->favorite, juce::dontSendNotification);
    presetRow.previous.setEnabled(!shown.empty());
    presetRow.next.setEnabled(!shown.empty());
}
