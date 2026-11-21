#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
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
    slots.compare.toggle();
    require(!slots.compare.isB() && std::abs(raw(slots, "fatness") - 70.0f) < 0.05f, "A did not keep its edited sound");
    slots.compare.copyBToA();
    require(std::abs(raw(slots, "fatness") - 25.0f) < 0.05f, "Copy B to A did not replace A");
    slots.history.undo();
    require(std::abs(raw(slots, "fatness") - 70.0f) < 0.05f, "Undo did not restore A after the copy");
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
    presetTarget.loadParameterTree(*loaded);
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
        presetName->setText("My Bass", false);
        saveButton->onClick();
        require(std::abs(storedValue(bar.presets, "My Bass", "fatness") - 77.0f) < 0.05f && saveButton->getButtonText() == "Replace?",
            "Save replaced a preset without asking");
        saveButton->onClick();
        require(std::abs(storedValue(bar.presets, "My Bass", "fatness") - 64.0f) < 0.05f, "The second Save click did not replace the preset");
        require(status->getText() == "Saved My Bass.", "Save did not report in the status line");
        deleteButton->onClick();
        require(barFolder.getChildFile("My Bass.preset").existsAsFile() && deleteButton->getButtonText() == "Delete?",
            "Delete removed a preset without asking");
        deleteButton->onClick();
        require(!barFolder.getChildFile("My Bass.preset").existsAsFile(), "The second Delete click did not remove the preset");
    }
    barFolder.deleteRecursively();

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
    return 0;
}
