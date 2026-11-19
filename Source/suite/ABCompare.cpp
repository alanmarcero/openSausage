#include "ABCompare.h"

class ABToggleAction : public juce::UndoableAction {
public:
    explicit ABToggleAction(ABCompare& owner) : ab(owner) {}
    bool perform() override { ab.swap(); return true; }
    bool undo() override { ab.swap(); return true; }
    int getSizeInUnits() override { return 1; }
private:
    ABCompare& ab;
};

class ABCopyAction : public juce::UndoableAction {
public:
    ABCopyAction(ABCompare& owner, bool copyAToB) : ab(owner), aToB(copyAToB) {}
    bool perform() override {
        previousLive = ab.capture();
        previousInactive = ab.inactive.createCopy();
        previousActive = ab.active;
        ab.copyRaw(aToB);
        return true;
    }
    bool undo() override {
        ab.restore(previousLive, previousInactive, previousActive);
        return true;
    }
    int getSizeInUnits() override { return 8; }
private:
    ABCompare& ab;
    bool aToB = true;
    juce::ValueTree previousLive, previousInactive;
    int previousActive = 0;
};

ABCompare::ABCompare(juce::AudioProcessorValueTreeState& state, ParameterUndo& undo)
    : parameters(state), history(undo), inactive(capture()) {}

juce::ValueTree ABCompare::capture() const {
    juce::ValueTree slot("inactiveSlot");
    for (auto* parameter : parameters.processor.getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
            juce::ValueTree child("PARAM");
            child.setProperty("id", ranged->paramID, nullptr);
            child.setProperty("value", ranged->convertFrom0to1(ranged->getValue()), nullptr);
            slot.appendChild(child, nullptr);
        }
    }
    return slot;
}
juce::ValueTree ABCompare::current(int slot) const {
    return slot == active ? capture() : inactive.createCopy();
}
void ABCompare::applyTree(const juce::ValueTree& slot) {
    const ParameterUndo::Suspend guard(history);
    for (int index = 0; index < slot.getNumChildren(); ++index) {
        const auto child = slot.getChild(index);
        if (auto* parameter = parameters.getParameter(child.getProperty("id").toString())) {
            const float denormalised = child.getProperty("value");
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(denormalised));
            parameter->endChangeGesture();
        }
    }
}
void ABCompare::swap() {
    const ParameterUndo::Suspend guard(history);
    auto live = capture();
    applyTree(inactive);
    inactive = live;
    active = active == 0 ? 1 : 0;
}
void ABCompare::copyRaw(bool aToB) {
    const ParameterUndo::Suspend guard(history);
    const auto source = current(aToB ? 0 : 1);
    const int destination = aToB ? 1 : 0;
    if (destination == active) applyTree(source);
    else inactive = source.createCopy();
}
void ABCompare::restore(juce::ValueTree live, juce::ValueTree savedInactive, int slot) {
    const ParameterUndo::Suspend guard(history);
    applyTree(live);
    inactive = std::move(savedInactive);
    active = slot == 1 ? 1 : 0;
}
void ABCompare::toggle() {
    if (!history.beginGroup("A/B")) { swap(); return; }
    history.perform(new ABToggleAction(*this));
    history.endGroup();
}
void ABCompare::copyAToB() {
    if (!history.beginGroup("Copy A to B")) { copyRaw(true); return; }
    history.perform(new ABCopyAction(*this, true));
    history.endGroup();
}
void ABCompare::copyBToA() {
    if (!history.beginGroup("Copy B to A")) { copyRaw(false); return; }
    history.perform(new ABCopyAction(*this, false));
    history.endGroup();
}
void ABCompare::write(juce::ValueTree& state) const {
    state.setProperty("ab", active, nullptr);
    auto slot = inactive.createCopy();
    if (!slot.hasType("inactiveSlot")) {
        juce::ValueTree wrapped("inactiveSlot");
        wrapped.copyPropertiesAndChildrenFrom(inactive, nullptr);
        slot = wrapped;
    }
    if (auto existing = state.getChildWithName("inactiveSlot"); existing.isValid())
        state.removeChild(existing, nullptr);
    state.appendChild(slot, nullptr);
}
void ABCompare::install(int slot, juce::ValueTree savedInactive) {
    active = slot == 1 ? 1 : 0;
    inactive = savedInactive.isValid() && savedInactive.getNumChildren() > 0 ? std::move(savedInactive) : capture();
}
