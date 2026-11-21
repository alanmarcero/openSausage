#include "ParameterUndo.h"
#include <algorithm>

class ParameterUndo::Action : public juce::UndoableAction {
public:
    Action(ParameterUndo& ownerIn, std::vector<Change> changesIn)
        : owner(ownerIn), changes(std::move(changesIn)) {}
    bool perform() override { owner.apply(changes, true); return true; }
    bool undo() override { owner.apply(changes, false); return true; }
    int getSizeInUnits() override { return std::max(1, static_cast<int>(changes.size())); }
private:
    ParameterUndo& owner;
    std::vector<Change> changes;
};

ParameterUndo::ParameterUndo(juce::AudioProcessor& audioProcessor) : processor(audioProcessor) {
    const auto count = processor.getParameters().size();
    last.assign(static_cast<std::size_t>(count), 0.0f);
    gestureFrom.assign(static_cast<std::size_t>(count), 0.0f);
    gesture.assign(static_cast<std::size_t>(count), false);
    for (int index = 0; index < count; ++index) {
        auto* parameter = processor.getParameters()[index];
        last[static_cast<std::size_t>(index)] = parameter->getValue();
        parameter->addListener(this);
    }
}
ParameterUndo::~ParameterUndo() {
    for (auto* parameter : processor.getParameters()) parameter->removeListener(this);
}
void ParameterUndo::undo() { if (messageThread()) manager->undo(); }
void ParameterUndo::redo() { if (messageThread()) manager->redo(); }
bool ParameterUndo::canUndo() const { return manager->canUndo(); }
bool ParameterUndo::canRedo() const { return manager->canRedo(); }
void ParameterUndo::clear() {
    if (messageThread()) std::ranges::for_each(histories, [](juce::UndoManager& history) { history.clearUndoHistory(); });
}
void ParameterUndo::selectHistory(int slot) { manager = &histories[slot == 1 ? 1 : 0]; }
void ParameterUndo::clearHistory(int slot) { if (messageThread()) histories[slot == 1 ? 1 : 0].clearUndoHistory(); }
bool ParameterUndo::beginGroup(const juce::String& name) {
    if (!messageThread()) return false;
    if (groupDepth++ == 0) {
        manager->beginNewTransaction(name);
        grouped.clear();
    }
    return true;
}
void ParameterUndo::endGroup() {
    if (!messageThread() || groupDepth <= 0) return;
    if (--groupDepth > 0) return;
    commit(std::move(grouped), false);
    grouped.clear();
}
void ParameterUndo::perform(juce::UndoableAction* action) {
    if (!messageThread()) { delete action; return; }
    manager->perform(action);
}
bool ParameterUndo::messageThread() const noexcept {
    return juce::MessageManager::existsAndIsCurrentThread();
}
bool ParameterUndo::valid(int index) const noexcept {
    return index >= 0 && index < processor.getParameters().size();
}
std::vector<ParameterUndo::Change> ParameterUndo::coalesce(std::vector<Change> changes) {
    std::vector<Change> result;
    for (const auto& change : changes) {
        if (!std::isfinite(change.from) || !std::isfinite(change.to)) continue;
        const auto it = std::ranges::find(result, change.index, &Change::index);
        if (it == result.end()) result.push_back(change);
        else it->to = change.to;
    }
    std::erase_if(result, [](const Change& change) { return juce::approximatelyEqual(change.from, change.to); });
    return result;
}
void ParameterUndo::commit(std::vector<Change> changes, bool startTransaction) {
    changes = coalesce(std::move(changes));
    if (changes.empty()) return;
    if (startTransaction) manager->beginNewTransaction();
    manager->perform(new Action(*this, std::move(changes)));
}
void ParameterUndo::apply(const std::vector<Change>& changes, bool useTo) {
    const juce::ScopedValueSetter<bool> guard(applying, true);
    for (const auto& change : changes) {
        if (!valid(change.index)) continue;
        auto* parameter = processor.getParameters()[change.index];
        const float value = useTo ? change.to : change.from;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(value);
        parameter->endChangeGesture();
        last[static_cast<std::size_t>(change.index)] = parameter->getValue();
    }
}
void ParameterUndo::parameterValueChanged(int index, float newValue) {
    if (!valid(index) || !messageThread() || applying) return;
    if (gesture[static_cast<std::size_t>(index)]) return;
    const float previous = last[static_cast<std::size_t>(index)];
    last[static_cast<std::size_t>(index)] = newValue;
    if (suspended.load() > 0 || juce::approximatelyEqual(previous, newValue)) return;
    Change change {index, previous, newValue};
    if (groupDepth > 0) grouped.push_back(change);
    else if (openGestures > 0) pending.push_back(change);
    else commit({change}, true);
}
void ParameterUndo::parameterGestureChanged(int index, bool starting) {
    if (!valid(index) || !messageThread() || applying || suspended.load() > 0) return;
    const auto slot = static_cast<std::size_t>(index);
    if (starting) {
        if (gesture[slot]) return;
        gesture[slot] = true;
        gestureFrom[slot] = processor.getParameters()[index]->getValue();
        if (groupDepth == 0 && openGestures == 0) manager->beginNewTransaction();
        ++openGestures;
        return;
    }
    if (!gesture[slot]) return;
    gesture[slot] = false;
    const float to = processor.getParameters()[index]->getValue();
    last[slot] = to;
    if (!juce::approximatelyEqual(gestureFrom[slot], to)) {
        Change change {index, gestureFrom[slot], to};
        if (groupDepth > 0) grouped.push_back(change);
        else pending.push_back(change);
    }
    if (openGestures > 0) --openGestures;
    if (groupDepth == 0 && openGestures == 0) {
        commit(std::move(pending), false);
        pending.clear();
    }
}
