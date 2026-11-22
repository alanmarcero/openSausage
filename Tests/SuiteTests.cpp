#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
float raw(const SausageProcessor& processor, const char* id) { return processor.parameters.getRawParameterValue(id)->load(); }
void setDenormalised(SausageProcessor& processor, const char* id, float value) {
    auto* parameter = processor.parameters.getParameter(id);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    parameter->endChangeGesture();
}
// Records the areas a component asks to repaint. Without a window, repaints are otherwise lost.
struct RepaintSpy final : juce::CachedComponentImage {
    explicit RepaintSpy(juce::Component& target) : owner(target) {}
    void paint(juce::Graphics& g) override { owner.paintEntireComponent(g, false); }
    bool invalidateAll() override { dirty = owner.getLocalBounds(); return true; }
    bool invalidate(const juce::Rectangle<int>& area) override { dirty = dirty.getUnion(area); return true; }
    void releaseResources() override {}
    juce::Component& owner;
    juce::Rectangle<int> dirty;
};
float storedValue(const PresetLibrary& library, const juce::String& preset, const char* id) {
    const auto tree = library.loadUser(preset);
    if (!tree) return -1.0f;
    return tree->getChildWithProperty("id", id).getProperty("value");
}
// True when two visible children of parent overlap.
bool overlapping(const juce::Component& parent) {
    const auto& children = parent.getChildren();
    return std::ranges::any_of(children, [&children](const juce::Component* child) {
        return child->isVisible() && std::ranges::any_of(children, [child](const juce::Component* other) {
            return other != child && other->isVisible() && other->getBounds().intersects(child->getBounds());
        });
    });
}
std::vector<juce::Component*> descendants(juce::Component& root) {
    std::vector<juce::Component*> all {&root};
    std::ranges::for_each(root.getChildren(), [&all](juce::Component* child) {
        const auto below = descendants(*child);
        all.insert(all.end(), below.begin(), below.end());
    });
    return all;
}
juce::Component* find(juce::Component& root, const juce::String& id) {
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren()) if (auto* match = find(*child, id)) return match;
    return nullptr;
}
int crossings(SausageProcessor& processor, double ppq, int numerator, int denominator) {
    struct Head final : juce::AudioPlayHead {
        double ppq = 0;
        int numerator = 4, denominator = 4;
        juce::Optional<PositionInfo> getPosition() const override {
            PositionInfo info;
            info.setPpqPosition(ppq);
            info.setBpm(120.0);
            info.setIsPlaying(false);
            juce::AudioPlayHead::TimeSignature signature;
            signature.numerator = numerator;
            signature.denominator = denominator;
            info.setTimeSignature(signature);
            return info;
        }
    } head;
    head.ppq = ppq;
    head.numerator = numerator;
    head.denominator = denominator;
    processor.setPlayHead(&head);
    processor.prepareToPlay(48000, 256);
    int count = 0;
    float previous = 0.0f;
    constexpr int total = 48000;
    for (int start = 0; start < total; start += 256) {
        juce::AudioBuffer<float> audio(2, 256);
        juce::MidiBuffer midi;
        for (int sample = 0; sample < 256; ++sample) {
            const auto value = 0.2f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * static_cast<float>(start + sample) / 48000.0f);
            audio.setSample(0, sample, value);
            audio.setSample(1, sample, value);
        }
        processor.processBlock(audio, midi);
        if (start >= total / 2) {
            for (int sample = 0; sample < 256; ++sample) {
                const auto value = audio.getSample(0, sample);
                if (previous <= 0.0f && value > 0.0f) ++count;
                previous = value;
            }
        }
    }
    processor.setPlayHead(nullptr);
    return count;
}
}

int runOpenSausageSuiteTests() {
    const auto& recipes = SausageProcessor::factoryRecipes();
    require(recipes.size() == 48, "Expected 48 factory recipes");
    SausageProcessor processor;
    require(processor.getNumPrograms() == 48, "Host program count is not the recipe count");
    for (int index = 0; index < processor.getNumPrograms(); ++index)
        require(processor.getProgramName(index) == recipes[static_cast<std::size_t>(index)].name, "Host program order differs from the editor recipes");
    require(processor.getProgramName(-1).isEmpty() && processor.getProgramName(48).isEmpty(), "Out-of-range program name was not empty");
    processor.changeProgramName(4, "renamed");
    require(processor.getProgramName(4) == "Uplifting / Lift Off", "Factory program name was edited");
    setDenormalised(processor, "riseLength", 3.0f);
    processor.setCurrentProgram(4);
    require(processor.getCurrentProgram() == 4, "Current program did not follow the host");
    require(std::abs(raw(processor, "rise") - 85.0f) < 0.05f, "Program did not apply Lift Off");
    require(std::abs(raw(processor, "riseLength") - 3.0f) < 0.05f, "Program changed Rise Length");
    setDenormalised(processor, "fatness", 12.0f);
    juce::MemoryBlock session;
    processor.getStateInformation(session);
    SausageProcessor restored;
    restored.parameters.getParameter("fatness")->setValueNotifyingHost(1.0f);
    restored.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    require(restored.getCurrentProgram() == 4, "Session lost the host program");
    require(std::abs(raw(restored, "fatness") - 12.0f) < 0.05f, "Session reapplied the factory recipe");
    require(std::abs(raw(restored, "rise") - 85.0f) < 0.05f, "Session lost Rise");
    require(std::abs(raw(restored, "riseLength") - 3.0f) < 0.05f, "Session lost Rise Length");
    require(!restored.history.canUndo(), "Loading a session left an undo step");

    auto legacy = processor.parameters.copyState();
    legacy.removeProperty("program", nullptr);
    legacy.removeProperty("ab", nullptr);
    for (int index = legacy.getNumChildren(); --index >= 0;)
        if (legacy.getChild(index).getProperty("id").toString() == "riseLength") legacy.removeChild(index, nullptr);
    juce::MemoryBlock legacyData;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), legacyData);
    processor.setCurrentProgram(2);
    processor.setStateInformation(legacyData.getData(), static_cast<int>(legacyData.getSize()));
    require(std::abs(raw(processor, "riseLength")) < 0.05f, "Old session did not default Rise Length to Free");
    require(processor.getCurrentProgram() == 0, "Old session did not default the program");
    require(processor.compare.label().name == "Uplifting / Clean Slate" && processor.compare.label().edited,
        "Old session claimed an unedited preset");

    SausageProcessor edits;
    auto* fatness = edits.parameters.getParameter("fatness");
    fatness->beginChangeGesture();
    fatness->setValueNotifyingHost(0.2f);
    fatness->setValueNotifyingHost(0.45f);
    fatness->setValueNotifyingHost(0.6f);
    fatness->endChangeGesture();
    require(std::abs(raw(edits, "fatness") - 60.0f) < 0.05f, "Gesture did not reach the parameter");
    edits.applyRecipe(4);
    require(std::abs(raw(edits, "fatness") - 38.0f) < 0.05f && std::abs(raw(edits, "rise") - 85.0f) < 0.05f, "Recipe did not apply");
    require(edits.history.canUndo(), "Recipe load was not undoable");
    edits.history.undo();
    require(std::abs(raw(edits, "fatness") - 60.0f) < 0.05f && std::abs(raw(edits, "rise")) < 0.05f, "Undo did not restore the whole recipe as one step");
    edits.history.undo();
    require(std::abs(raw(edits, "fatness")) < 0.05f, "A drag became more than one undo step");
    require(!edits.history.canUndo(), "Undo history was longer than the two gestures");
    edits.history.redo();
    require(std::abs(raw(edits, "fatness") - 60.0f) < 0.05f, "Redo did not restore the drag");
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(edits.createEditor());
        require(editor->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)), "Cmd/Ctrl+Z was not handled");
        require(std::abs(raw(edits, "fatness")) < 0.05f, "Cmd/Ctrl+Z did not undo");
        require(editor->keyPressed(juce::KeyPress('Z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 'Z')), "Shift+Cmd/Ctrl+Z was not handled");
        require(std::abs(raw(edits, "fatness") - 60.0f) < 0.05f, "Shift+Cmd/Ctrl+Z did not redo");
        auto* undoButton = dynamic_cast<juce::TextButton*>(find(*editor, "suiteUndo"));
        require(undoButton != nullptr && undoButton->isEnabled() && static_cast<bool>(undoButton->onClick), "Undo button missing or disabled");
        undoButton->onClick();
        require(std::abs(raw(edits, "fatness")) < 0.05f, "Undo button did not undo");
    }

    SausageProcessor slots;
    setDenormalised(slots, "fatness", 25.0f);
    slots.compare.copyAToB();
    setDenormalised(slots, "fatness", 70.0f);
    slots.compare.toggle();
    require(slots.compare.isB() && std::abs(raw(slots, "fatness") - 25.0f) < 0.05f, "B did not keep the copied sound");
    require(!slots.history.canUndo(), "B used the undo history of A");
    setDenormalised(slots, "fatness", 40.0f);
    slots.history.undo();
    require(std::abs(raw(slots, "fatness") - 25.0f) < 0.05f && !slots.history.canUndo(), "Undo in B reached a step taken in A");
    slots.compare.toggle();
    require(!slots.compare.isB() && std::abs(raw(slots, "fatness") - 70.0f) < 0.05f, "A did not keep its edited sound");
    slots.history.undo();
    require(std::abs(raw(slots, "fatness") - 25.0f) < 0.05f, "Undo in A did not use the history of A");
    slots.history.redo();
    slots.compare.copyBToA();
    require(std::abs(raw(slots, "fatness") - 25.0f) < 0.05f && !slots.history.canUndo(), "Copy B to A did not replace A and clear its history");
    setDenormalised(slots, "fatness", 70.0f);
    slots.compare.toggle();
    juce::MemoryBlock slotsState;
    slots.getStateInformation(slotsState);
    SausageProcessor slotsRestored;
    slotsRestored.setStateInformation(slotsState.getData(), static_cast<int>(slotsState.getSize()));
    require(slotsRestored.compare.isB() && std::abs(raw(slotsRestored, "fatness") - 25.0f) < 0.05f, "Session lost slot B");
    slotsRestored.compare.toggle();
    require(!slotsRestored.compare.isB() && std::abs(raw(slotsRestored, "fatness") - 70.0f) < 0.05f, "Session lost slot A");

    auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openSausage-suite-tests");
    folder.deleteRecursively();
    std::vector<juce::String> names;
    for (const auto& recipe : recipes) names.emplace_back(recipe.name);
    PresetLibrary library(folder, names);
    require(PresetLibrary::userDirectory("openSausage") == juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("Alan Marcero").getChildFile("openSausage").getChildFile("Presets"),
        "User presets are not under Documents/Alan Marcero/openSausage/Presets");
    juce::String error;
    auto tree = slotsRestored.copyParameterTree();
    require(!library.save("Uplifting / Clean Slate", tree, error), "Factory preset was overwritten");
    require(!library.remove("Uplifting / Lift Off", error), "Factory preset was deleted");
    if (!library.save("My Glue", tree, error)) throw std::runtime_error(error.toStdString());
    require(library.entries("").size() == 49, "Browser did not list factory presets plus the saved preset");
    auto filtered = library.entries("my glue");
    require(filtered.size() == 1 && !filtered[0].factory && filtered[0].name == "My Glue", "Search missed the saved preset");
    require(library.entries("MY GLUE").size() == 1, "Search is case-sensitive");
    require(library.setFavorite("Uplifting / Clean Slate", true), "Factory preset could not be starred");
    require(library.entries("")[0].name == "Uplifting / Clean Slate" && library.entries("")[0].favorite, "Starred preset was not listed first");
    if (!library.rename("My Glue", "My Glue 2", error)) throw std::runtime_error(error.toStdString());
    require(!library.loadUser("My Glue").has_value(), "Renamed preset left the old file");
    auto loaded = library.loadUser("My Glue 2");
    require(loaded.has_value(), "Renamed preset did not load");
    SausageProcessor presetTarget;
    setDenormalised(presetTarget, "fatness", 3.0f);
    setDenormalised(presetTarget, "color", 9.0f);
    presetTarget.loadParameterTree(*loaded, "My Glue 2");
    require(presetTarget.compare.label().name == "My Glue 2" && !presetTarget.compare.label().edited, "Loading a preset did not show its name");
    require(std::abs(raw(presetTarget, "fatness") - raw(slotsRestored, "fatness")) < 0.05f, "Loaded preset did not restore Fatness");
    presetTarget.history.undo();
    require(std::abs(raw(presetTarget, "fatness") - 3.0f) < 0.05f && std::abs(raw(presetTarget, "color") - 9.0f) < 0.05f,
        "Preset load was not one undo step");
    if (!library.remove("My Glue 2", error)) throw std::runtime_error(error.toStdString());
    require(library.entries("my glue").empty(), "Deleted preset is still listed");
    folder.deleteRecursively();

    auto barFolder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openSausage-suite-bar");
    barFolder.deleteRecursively();
    SausageProcessor bar(barFolder);
    setDenormalised(bar, "fatness", 77.0f);
    if (!bar.presets.save("My Bass", bar.copyParameterTree(), error)) throw std::runtime_error(error.toStdString());
    setDenormalised(bar, "fatness", 10.0f);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(bar.createEditor());
        auto* search = dynamic_cast<juce::TextEditor*>(find(*editor, "presetSearch"));
        auto* presetName = dynamic_cast<juce::TextEditor*>(find(*editor, "presetName"));
        auto* saveButton = dynamic_cast<juce::TextButton*>(find(*editor, "suiteSave"));
        auto* deleteButton = dynamic_cast<juce::TextButton*>(find(*editor, "suiteDelete"));
        auto* status = dynamic_cast<juce::Label*>(find(*editor, "presetStatus"));
        require(search && presetName && saveButton && deleteButton && status, "Preset search, name, Save, Delete, or status missing");
        search->setText("My Bass", false);
        search->onTextChange();
        search->onReturnKey();
        require(std::abs(raw(bar, "fatness") - 77.0f) < 0.05f, "Return in the search did not load the first match");
        require(std::abs(storedValue(bar.presets, "My Bass", "fatness") - 77.0f) < 0.05f, "Return in the search overwrote the preset");
        search->setText("acid", false);
        search->onTextChange();
        search->onReturnKey();
        require(!barFolder.getChildFile("acid.preset").exists(), "Return in the search created a preset");
        require(std::abs(raw(bar, "fatness") - 64.0f) < 0.05f, "Return did not load Rolling / Acid Top Grit");
        auto* browser = dynamic_cast<juce::ComboBox*>(find(*editor, "presetBrowser"));
        auto* previous = dynamic_cast<juce::TextButton*>(find(*editor, "suitePrevious"));
        auto* next = dynamic_cast<juce::TextButton*>(find(*editor, "suiteNext"));
        auto* suiteBar = dynamic_cast<SuiteBar*>(saveButton->getParentComponent());
        require(browser && previous && next && suiteBar, "Preset browser, Previous, or Next missing");
        require(bar.compare.label().name == "Rolling / Acid Top Grit" && !bar.compare.label().edited, "Loading did not show the preset name");
        next->onClick();
        require(bar.compare.label().name == "Tech / Peak Acid Bite", "Next did not step through the browser list");
        next->onClick();
        require(bar.compare.label().name == "Rolling / Acid Top Grit", "Next did not wrap to the first preset");
        previous->onClick();
        require(bar.compare.label().name == "Tech / Peak Acid Bite", "Previous did not wrap to the last preset");
        bar.history.undo();
        require(bar.compare.label().name == "Rolling / Acid Top Grit", "Undo did not restore the previous preset name");
        setDenormalised(bar, "color", 20.0f);
        suiteBar->refresh();
        require(bar.compare.label().edited && browser->getText() == "Rolling / Acid Top Grit *", "An edit did not mark the preset name");
        bar.history.undo();
        require(!bar.compare.label().edited, "Undoing the only edit kept the edited mark");
        presetName->setText("My Bass", false);
        saveButton->onClick();
        require(std::abs(storedValue(bar.presets, "My Bass", "fatness") - 77.0f) < 0.05f && saveButton->getButtonText() == "Replace?",
            "Save replaced a preset without asking");
        saveButton->onClick();
        require(std::abs(storedValue(bar.presets, "My Bass", "fatness") - 64.0f) < 0.05f, "The second Save click did not replace the preset");
        require(status->getText() == "Saved My Bass.", "Save did not report in the status line");
        require(bar.compare.label().name == "My Bass" && !bar.compare.label().edited, "Save did not show the saved name");
        deleteButton->onClick();
        require(barFolder.getChildFile("My Bass.preset").existsAsFile() && deleteButton->getButtonText() == "Delete?",
            "Delete removed a preset without asking");
        deleteButton->onClick();
        require(!barFolder.getChildFile("My Bass.preset").existsAsFile(), "The second Delete click did not remove the preset");
    }
    barFolder.deleteRecursively();

    auto confirmFolder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openSausage-suite-confirm");
    confirmFolder.deleteRecursively();
    SausageProcessor confirm(confirmFolder);
    if (!confirm.presets.save("Preset X", confirm.copyParameterTree(), error)) throw std::runtime_error(error.toStdString());
    if (!confirm.presets.save("Preset Y", confirm.copyParameterTree(), error)) throw std::runtime_error(error.toStdString());
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(confirm.createEditor());
        auto* slotA = dynamic_cast<juce::TextButton*>(find(*editor, "suiteA"));
        auto* slotB = dynamic_cast<juce::TextButton*>(find(*editor, "suiteB"));
        auto* search = dynamic_cast<juce::TextEditor*>(find(*editor, "presetSearch"));
        auto* presetName = dynamic_cast<juce::TextEditor*>(find(*editor, "presetName"));
        auto* saveButton = dynamic_cast<juce::TextButton*>(find(*editor, "suiteSave"));
        auto* deleteButton = dynamic_cast<juce::TextButton*>(find(*editor, "suiteDelete"));
        auto* nextButton = dynamic_cast<juce::TextButton*>(find(*editor, "suiteNext"));
        require(slotA && slotB && search && presetName && saveButton && deleteButton && nextButton, "Suite confirm controls missing");
        search->setText("Preset X", false);
        search->onTextChange();
        search->onReturnKey();
        slotB->onClick();
        search->setText("Preset Y", false);
        search->onTextChange();
        search->onReturnKey();
        slotA->onClick();
        search->setText({}, false);
        search->onTextChange();
        // Arm Delete? on Preset X in A, then switch to B (Preset Y) within the confirm window.
        deleteButton->onClick();
        require(deleteButton->getButtonText() == "Delete?", "First Delete click did not arm Delete?");
        slotB->onClick();
        require(deleteButton->getButtonText() == "Delete", "A/B switch did not disarm Delete?");
        deleteButton->onClick();
        require(confirmFolder.getChildFile("Preset X.preset").existsAsFile() && confirmFolder.getChildFile("Preset Y.preset").existsAsFile(),
            "Switching A/B after arming Delete? deleted a preset");
        // Arm Replace? on Preset Y in B, then switch to A.
        presetName->setText("Preset Y", false);
        saveButton->onClick();
        require(saveButton->getButtonText() == "Replace?", "Save did not arm Replace?");
        slotA->onClick();
        require(saveButton->getButtonText() == "Save", "A/B switch did not disarm Replace?");
        // Arm Delete? on Preset X, then change selection via Next.
        deleteButton->onClick();
        require(deleteButton->getButtonText() == "Delete?", "Delete did not re-arm on Preset X");
        nextButton->onClick();
        require(deleteButton->getButtonText() == "Delete", "Selection change did not disarm Delete?");
        require(confirmFolder.getChildFile("Preset X.preset").existsAsFile() && confirmFolder.getChildFile("Preset Y.preset").existsAsFile(),
            "Selection change deleted a preset");
    }
    confirmFolder.deleteRecursively();

    SausageProcessor formats;
    std::vector<juce::AudioProcessorParameter*> floats;
    std::ranges::copy_if(formats.getParameters(), std::back_inserter(floats),
        [](juce::AudioProcessorParameter* parameter) { return dynamic_cast<juce::AudioParameterFloat*>(parameter) != nullptr; });
    require(floats.size() == 13, "Expected 13 float parameters");
    constexpr std::array<float, 6> points {0.0f, 0.1f, 0.25f, 0.5f, 0.77f, 1.0f};
    const auto unstable = std::ranges::find_if(floats, [&points](juce::AudioProcessorParameter* parameter) {
        return !std::ranges::all_of(points, [parameter](float value) {
            const auto text = parameter->getText(value, 0);
            return parameter->getText(parameter->getValueForText(text), 0) == text;
        });
    });
    if (unstable != floats.end()) throw std::runtime_error(("Typed text does not round-trip: " + (*unstable)->getName(64)).toStdString());
    auto* fatnessText = formats.parameters.getParameter("fatness");
    require(fatnessText->getText(fatnessText->convertTo0to1(55.0f), 0) == "55%", "Percent is not shown as whole percent");
    require(juce::approximatelyEqual(fatnessText->getValueForText("50"), fatnessText->getValueForText("50%")), "50 and 50% differ");
    auto* trimText = formats.parameters.getParameter("output");
    require(trimText->getText(trimText->convertTo0to1(3.0f), 0) == "+3.0 dB" && trimText->getText(trimText->convertTo0to1(-0.04f), 0) == "0.0 dB",
        "Wet trim is not shown as signed dB");
    auto* preDelayText = formats.parameters.getParameter("preDelay");
    require(preDelayText->getText(preDelayText->convertTo0to1(4.5f), 0) == "4.5 ms" && preDelayText->getText(preDelayText->convertTo0to1(45.0f), 0) == "45 ms",
        "Pre-delay is not shown in ms");
    require(std::abs(preDelayText->convertFrom0to1(preDelayText->getValueForText("0.05 s")) - 50.0f) < 0.05f, "Pre-delay did not convert s to ms");
    auto* riseText = formats.parameters.getParameter("rise");
    require(riseText->getText(1.0f, 0) == "+12.0 st", "Rise is not shown in semitones");
    require(riseText->getText(riseText->getValueForText("7 st"), 0) == "+7.0 st", "Rise does not accept semitones");

    SausageProcessor labels;
    labels.applyRecipe(4);
    setDenormalised(labels, "fatness", 12.0f);
    labels.compare.toggle();
    require(labels.compare.label().name == "Uplifting / Clean Slate" && !labels.compare.label().edited, "B showed the preset name of A");
    labels.compare.toggle();
    require(labels.compare.label().name == "Uplifting / Lift Off" && labels.compare.label().edited, "A lost its preset name or edited mark");
    juce::MemoryBlock labelState;
    labels.getStateInformation(labelState);
    SausageProcessor labelsRestored;
    labelsRestored.setStateInformation(labelState.getData(), static_cast<int>(labelState.getSize()));
    require(labelsRestored.compare.label().name == "Uplifting / Lift Off" && labelsRestored.compare.label().edited, "Session lost the preset name");
    labelsRestored.compare.toggle();
    require(labelsRestored.compare.label().name == "Uplifting / Clean Slate", "Session lost the preset name of B");

    SausageProcessor sized;
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(sized.createEditor());
        const auto designWidth = editor->getWidth(), designHeight = editor->getHeight();
        const auto* limits = editor->getConstrainer();
        require(limits->getMinimumWidth() == designWidth * 3 / 4 && limits->getMaximumWidth() == designWidth * 3 / 2
                    && juce::approximatelyEqual(limits->getFixedAspectRatio(), static_cast<double>(designWidth) / designHeight),
            "Editor does not resize from 75% to 150% at a fixed aspect ratio");
        editor->setSize(designWidth * 3 / 4, designHeight * 3 / 4);
        auto* algorithms = find(*editor, "algorithm");
        const auto scaled = editor->getLocalArea(algorithms, algorithms->getLocalBounds());
        require(std::abs(scaled.getWidth() - algorithms->getWidth() * 3 / 4) <= 1 && std::abs(scaled.getHeight() - algorithms->getHeight() * 3 / 4) <= 1,
            "Editor content did not scale uniformly");
        editor->setSize(designWidth * 6 / 5, designHeight * 6 / 5);
        juce::MemoryBlock sizedState;
        sized.getStateInformation(sizedState);
        SausageProcessor reopened;
        reopened.setStateInformation(sizedState.getData(), static_cast<int>(sizedState.getSize()));
        std::unique_ptr<juce::AudioProcessorEditor> reopenedEditor(reopened.createEditor());
        require(reopenedEditor->getWidth() == designWidth * 6 / 5 && reopenedEditor->getHeight() == designHeight * 6 / 5,
            "Editor did not reopen at its saved size");
    }

    SausageProcessor layout;
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(layout.createEditor());
        auto* content = find(*editor, "algorithm")->getParentComponent();
        auto* suiteBar = find(*editor, "suiteUndo")->getParentComponent();
        const std::array<juce::Component*, 7> containers {content, suiteBar, find(*editor, "presetBrowser")->getParentComponent(),
            find(*editor, "fatness")->getParentComponent(), find(*editor, "space")->getParentComponent(),
            find(*editor, "rise")->getParentComponent(), find(*editor, "width")->getParentComponent()};
        require(std::ranges::none_of(containers, [](juce::Component* container) { return overlapping(*container); }), "Editor controls overlap");
        const auto all = descendants(*editor);
        require(std::ranges::none_of(all, [](juce::Component* component) {
            const auto* button = dynamic_cast<juce::TextButton*>(component);
            return button != nullptr && button->isVisible() && button->getHeight() < 36;
        }), "A button is shorter than 36 px");
        require(std::ranges::none_of(all, [](juce::Component* component) {
            const auto* knob = dynamic_cast<juce::Slider*>(component);
            return knob != nullptr && knob->isRotary() && std::min(knob->getWidth(), knob->getHeight() - knob->getTextBoxHeight()) - 26 < 46;
        }), "A knob face is smaller than 46 px");
        const std::array<juce::Component*, 3> icons {find(*editor, "suitePrevious"), find(*editor, "suiteNext"), find(*editor, "suiteFavorite")};
        require(std::ranges::all_of(icons, [](juce::Component* icon) { return icon->getWidth() >= 40 && icon->getHeight() >= 40; }),
            "An icon button is smaller than 40x40");
        require(std::ranges::none_of(all, [](juce::Component* component) {
            const bool control = dynamic_cast<juce::Slider*>(component) != nullptr
                              || dynamic_cast<juce::ComboBox*>(component) != nullptr
                              || dynamic_cast<juce::Button*>(component) != nullptr;
            return control && component->getTitle().isEmpty();
        }), "A slider, combo box, or button is missing a title");
        const auto* corner = *std::ranges::find_if(editor->getChildren(), [](juce::Component* child) {
            return dynamic_cast<juce::ResizableCornerComponent*>(child) != nullptr;
        });
        const auto clearOfCorner = [&] {
            return std::ranges::none_of(suiteBar->getChildren(), [&](juce::Component* control) {
                return editor->getLocalArea(control, control->getLocalBounds()).intersects(corner->getBounds());
            });
        };
        editor->setSize(editor->getWidth() * 3 / 4, editor->getHeight() * 3 / 4);
        require(clearOfCorner(), "The resize corner covers a suite bar control at 75%");
        editor->setSize(editor->getWidth() * 2, editor->getHeight() * 2);
        require(clearOfCorner(), "The resize corner covers a suite bar control at 150%");
    }

    SausageProcessor peaks;
    peaks.prepareToPlay(48000, 64);
    juce::AudioBuffer<float> loud(2, 64), quiet(2, 64);
    loud.clear();
    quiet.clear();
    loud.setSample(0, 10, 0.9f);
    quiet.setSample(0, 10, 0.1f);
    juce::MidiBuffer noMidi;
    peaks.processBlock(loud, noMidi);
    peaks.processBlock(quiet, noMidi);
    require(std::abs(peaks.inputPeak.exchange(0.0f) - 0.9f) < 1.0e-6f, "Input meter missed a peak between reads");
    require(juce::approximatelyEqual(peaks.inputPeak.load(), 0.0f), "Reading the input meter did not reset it");
    LevelMeter meter("INPUT");
    meter.push(0.5f, 0.0);
    meter.push(0.1f, 500.0);
    require(juce::approximatelyEqual(meter.heldPeak(), 0.5f), "Peak hold dropped before 1 s");
    meter.push(0.1f, 1100.0);
    require(juce::approximatelyEqual(meter.heldPeak(), 0.1f), "Peak hold lasted past 1 s");
    meter.push(1.2f, 1200.0);
    meter.push(0.1f, 5000.0);
    require(meter.isClipped(), "Clip light did not latch above 0 dBFS");
    const auto now = juce::Time::getCurrentTime();
    meter.mouseDown(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), {}, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     &meter, &meter, now, {}, now, 1, false));
    require(!meter.isClipped(), "A click did not clear the clip light");

    SausageProcessor effect;
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(effect.createEditor());
        auto* spacePanel = dynamic_cast<ModulePanel*>(find(*editor, "space")->getParentComponent());
        auto* size = dynamic_cast<juce::Slider*>(find(*editor, "size"));
        spacePanel->dimInactive();
        require(size->getAlpha() < 0.5f && size->isEnabled() && size->getTooltip().endsWith("Raise Mix to hear this."),
            "Size did not show that it has no effect at Mix 0%");
        setDenormalised(effect, "space", 40.0f);
        spacePanel->dimInactive();
        require(juce::approximatelyEqual(size->getAlpha(), 1.0f) && !size->getTooltip().contains("Raise"), "Size stayed dimmed after Mix went up");
        setDenormalised(effect, "bypass", 1.0f);
        spacePanel->dimInactive();
        require(size->getAlpha() < 0.5f && size->getTooltip().endsWith("Turn off Master bypass to use this."), "Master bypass did not dim Size");
    }

    SausageProcessor visuals;
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(visuals.createEditor());
        auto* panel = dynamic_cast<ModulePanel*>(find(*editor, "fatness")->getParentComponent());
        require(panel != nullptr, "Sausage panel missing");
        auto* spy = new RepaintSpy(*panel);
        panel->setCachedComponentImage(spy);
        panel->setBeatPhase(0.25f);
        spy->dirty = {};
        panel->setBeatPhase(0.25f);
        require(spy->dirty.isEmpty(), "Panel repainted although nothing changed");
        setDenormalised(visuals, "algorithm", 1.0f);
        spy->dirty = {};
        panel->setBeatPhase(0.25f);
        require(spy->dirty.contains(juce::Rectangle<int>(20, 57, 210, 22)), "Subtitle did not repaint after the algorithm changed");
        setDenormalised(visuals, "fatness", 50.0f);
        panel->setBeatPhase(0.25f);
        setDenormalised(visuals, "sausageBypass", 1.0f);
        spy->dirty = {};
        panel->setBeatPhase(0.25f);
        require(spy->dirty.contains(juce::Rectangle<int>(238, 62, 10, 10)), "LED did not repaint after the module was bypassed");
        setDenormalised(visuals, "sausageBypass", 0.0f);
        panel->setBeatPhase(0.25f);
        spy->dirty = {};
        panel->setBeatPhase(0.5f);
        require(spy->dirty.contains(juce::Rectangle<int>(132, 384, 104, 24)), "Beat display did not repaint when the beat moved");
        panel->setCachedComponentImage(nullptr);
    }

    SausageProcessor pitch;
    setDenormalised(pitch, "rise", 100.0f);
    setDenormalised(pitch, "riseMix", 100.0f);
    setDenormalised(pitch, "riseLength", 0.0f);
    const auto steady = crossings(pitch, 0.0, 4, 4);
    setDenormalised(pitch, "riseLength", 3.0f);
    const auto start = crossings(pitch, 0.0, 4, 4);
    const auto end = crossings(pitch, 15.9, 4, 4);
    setDenormalised(pitch, "riseLength", 1.0f);
    const auto threeFour = crossings(pitch, 3.9, 3, 4);
    const auto fourFour = crossings(pitch, 3.9, 4, 4);
    require(start > 80 && end > start * 3 / 2 && steady > start * 3 / 2, "Host tempo position did not change the rise");
    require(fourFour > threeFour + 30, "Rise length ignored the host time signature");

    {
        SausageProcessor lockProc;
        std::unique_ptr<juce::AudioProcessorEditor> lockEditor(lockProc.createEditor());
        std::vector<juce::String> paramIds;
        for (auto* parameter : lockProc.getParameters()) {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
            require(ranged != nullptr, "Non-ranged parameter in getParameters()");
            paramIds.push_back(ranged->paramID);
            require(find(*lockEditor, ranged->paramID) != nullptr, "Parameter control missing from editor");
        }
        require(paramIds.size() == 22, "Unexpected parameter count");

        for (const auto& id : paramIds) {
            require(!lockProc.isParamLocked(id), "Parameter started locked");
            lockProc.setParamLocked(id, true);
            require(lockProc.isParamLocked(id), "Parameter failed to lock");
            lockProc.setParamLocked(id, false);
            require(!lockProc.isParamLocked(id), "Parameter failed to unlock");
        }

        for (const auto& id : paramIds) lockProc.setParamLocked(id, true);
        require(lockProc.hasAnyLockedParam(), "hasAnyLockedParam() was false");
        juce::MemoryBlock lockState;
        lockProc.getStateInformation(lockState);

        SausageProcessor restoredLocks;
        restoredLocks.setStateInformation(lockState.getData(), static_cast<int>(lockState.getSize()));
        for (const auto& id : paramIds)
            require(restoredLocks.isParamLocked(id), "Parameter lock did not survive getStateInformation/setStateInformation");

        restoredLocks.unlockAllParams();
        require(!restoredLocks.hasAnyLockedParam(), "unlockAllParams() did not clear all locks");
        juce::MemoryBlock unlockedState;
        restoredLocks.getStateInformation(unlockedState);
        SausageProcessor restoredUnlocked;
        restoredUnlocked.setParamLocked("fatness", true);
        restoredUnlocked.setStateInformation(unlockedState.getData(), static_cast<int>(unlockedState.getSize()));
        require(!restoredUnlocked.hasAnyLockedParam(), "Unlocked state did not clear prior locks");

        SausageProcessor cleanSource;
        const auto cleanTree = cleanSource.copyParameterTree();

        SausageProcessor presetProc;
        setDenormalised(presetProc, "fatness", 77.0f);
        setDenormalised(presetProc, "color", 44.0f);
        setDenormalised(presetProc, "satFocus", 2.0f);
        setDenormalised(presetProc, "softClip", 1.0f);
        setDenormalised(presetProc, "riseLength", 4.0f);
        presetProc.setParamLocked("fatness", true);
        presetProc.setParamLocked("satFocus", true);
        presetProc.setParamLocked("softClip", true);
        presetProc.setParamLocked("riseLength", true);

        presetProc.setCurrentProgram(0);
        require(std::abs(raw(presetProc, "fatness") - 77.0f) < 0.05f, "setCurrentProgram overwrote locked fatness");
        require(std::abs(raw(presetProc, "satFocus") - 2.0f) < 0.05f, "setCurrentProgram overwrote locked satFocus");
        require(std::abs(raw(presetProc, "softClip") - 1.0f) < 0.05f, "setCurrentProgram overwrote locked softClip");
        require(std::abs(raw(presetProc, "color") - 0.0f) < 0.05f, "setCurrentProgram did not update unlocked color");

        setDenormalised(presetProc, "color", 55.0f);
        presetProc.loadParameterTree(cleanTree, "Clean Slate");
        require(std::abs(raw(presetProc, "fatness") - 77.0f) < 0.05f, "loadParameterTree overwrote locked fatness");
        require(std::abs(raw(presetProc, "riseLength") - 4.0f) < 0.05f, "loadParameterTree overwrote locked riseLength");
        require(std::abs(raw(presetProc, "color") - 0.0f) < 0.05f, "loadParameterTree did not update unlocked color");
    }
    return 0;
}
