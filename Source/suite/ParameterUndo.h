#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>

// Gesture-grouped undo for AudioProcessor parameters. One drag, click, or
// grouped recipe load becomes one UndoManager transaction. Host automation
// on the audio thread is not recorded.
class ParameterUndo : private juce::AudioProcessorParameter::Listener {
public:
    explicit ParameterUndo(juce::AudioProcessor&);
    ~ParameterUndo() override;
    ParameterUndo(const ParameterUndo&) = delete;
    ParameterUndo& operator=(const ParameterUndo&) = delete;

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    void clear();
    // Each A/B slot keeps its own history.
    void selectHistory(int slot);
    void clearHistory(int slot);
    // Returns false when called off the message thread; nothing is recorded then.
    bool beginGroup(const juce::String& name);
    void endGroup();
    void perform(juce::UndoableAction* action);

    class Suspend {
    public:
        explicit Suspend(ParameterUndo& target) noexcept : undo(target) { undo.suspended.fetch_add(1); }
        ~Suspend() { undo.suspended.fetch_sub(1); }
        Suspend(const Suspend&) = delete;
        Suspend& operator=(const Suspend&) = delete;
    private:
        ParameterUndo& undo;
    };

private:
    struct Change { int index; float from; float to; };
    class Action;

    void parameterValueChanged(int index, float newValue) override;
    void parameterGestureChanged(int index, bool starting) override;
    bool messageThread() const noexcept;
    bool valid(int index) const noexcept;
    void commit(std::vector<Change> changes, bool startTransaction);
    void apply(const std::vector<Change>& changes, bool useTo);
    static std::vector<Change> coalesce(std::vector<Change> changes);

    juce::AudioProcessor& processor;
    std::array<juce::UndoManager, 2> histories;
    juce::UndoManager* manager = &histories[0];
    std::vector<float> last;
    std::vector<float> gestureFrom;
    std::vector<bool> gesture;
    std::vector<Change> pending, grouped;
    int openGestures = 0, groupDepth = 0;
    bool applying = false;
    std::atomic<int> suspended {0};
};
