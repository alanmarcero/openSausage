#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
juce::Component* find(juce::Component& root, const juce::String& id) {
    if (root.getComponentID() == id) return &root;
    for (auto* child : root.getChildren()) if (auto* match = find(*child, id)) return match;
    return nullptr;
}
void save(juce::Component& editor, const juce::File& directory, const juce::String& name) {
    auto stream = directory.getChildFile(name).createOutputStream();
    require(stream != nullptr, "Cannot create screenshot output");
    stream->setPosition(0); stream->truncate();
    juce::PNGImageFormat png;
    require(png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.5f), *stream), "Screenshot failed");
}
}
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        const juce::File output(argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile("docs/screenshots"));
        require(output.createDirectory().wasOk(), "Cannot create screenshot directory");
        SausageProcessor processor;
        processor.prepareToPlay(48000, 256);
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        require(editor != nullptr, "Editor missing");
        save(*editor, output, "openSausage-clean.png");
        auto* algorithms = dynamic_cast<juce::ComboBox*>(find(*editor, "algorithm"));
        require(algorithms != nullptr && algorithms->getNumItems() == 4, "Four global algorithms missing");
        auto* algorithmParameter = processor.parameters.getParameter("algorithm");
        require(algorithmParameter != nullptr, "Algorithm host parameter missing");
        std::array<std::vector<float>, 4> algorithmAudio;
        for (int mode = 0; mode < 4; ++mode) {
            algorithms->setSelectedId(mode + 1, juce::sendNotificationSync);
            require(static_cast<int>(processor.parameters.getRawParameterValue("algorithm")->load()) == mode, "Algorithm selector not connected");
            processor.applyRecipe(3);
            require(algorithms->getSelectedId() == mode + 1, "Recipe overwrote global algorithm");
            juce::MemoryBlock saved;
            processor.getStateInformation(saved);
            SausageProcessor recalled;
            recalled.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
            require(static_cast<int>(recalled.parameters.getRawParameterValue("algorithm")->load()) == mode, "Algorithm recall failed");
            std::unique_ptr<juce::AudioProcessorEditor> recalledEditor(recalled.createEditor());
            auto* recalledSelector = dynamic_cast<juce::ComboBox*>(find(*recalledEditor, "algorithm"));
            require(recalledSelector && recalledSelector->getSelectedId() == mode + 1, "Reopened editor lost algorithm");
            processor.prepareToPlay(48000, 256); recalled.prepareToPlay(48000, 256);
            for (int block = 0; block < 64; ++block) {
                juce::AudioBuffer<float> audio(2, 256), recalledAudio(2, 256);
                juce::MidiBuffer events;
                for (int i = 0; i < 256; ++i) {
                    const auto sample = 0.2f * std::sin(static_cast<float>(block * 256 + i) * 0.0576f);
                    for (int ch = 0; ch < 2; ++ch) { audio.setSample(ch, i, sample); recalledAudio.setSample(ch, i, sample); }
                }
                processor.processBlock(audio, events); recalled.processBlock(recalledAudio, events);
                for (int i = 0; i < 256; ++i) {
                    require(std::abs(audio.getSample(0, i) - recalledAudio.getSample(0, i)) < 1.0e-6f, "Recalled algorithm sounds different");
                    algorithmAudio[static_cast<std::size_t>(mode)].push_back(audio.getSample(0, i));
                }
            }
            save(*editor, output, "openSausage-" + juce::String(sausage::algorithmNames[mode]).toLowerCase() + ".png");
        }
        for (std::size_t a = 0; a < algorithmAudio.size(); ++a)
            for (std::size_t b = a + 1; b < algorithmAudio.size(); ++b) {
                double difference = 0.0;
                for (std::size_t i = 4096; i < algorithmAudio[a].size(); ++i)
                    difference += std::pow(algorithmAudio[a][i] - algorithmAudio[b][i], 2);
                require(difference > 0.01, "Global selector did not change audio processing");
            }
        auto legacy = processor.parameters.copyState();
        for (int i = legacy.getNumChildren(); --i >= 0;)
            if (legacy.getChild(i).getProperty("id").toString() == "algorithm") legacy.removeChild(i, nullptr);
        juce::MemoryBlock legacyData;
        juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), legacyData);
        processor.setStateInformation(legacyData.getData(), static_cast<int>(legacyData.getSize()));
        require(processor.parameters.getRawParameterValue("algorithm")->load() == 0.0f, "Legacy session did not select Classic");
        require(algorithms->getSelectedId() == 1, "Legacy session did not update selector");
        algorithmParameter->setValueNotifyingHost(1.0f);
        require(algorithms->getSelectedId() == 4, "Host automation did not update selector");
        processor.applyRecipe(0);
        const auto calm = editor->createComponentSnapshot({0, 0, 180, 165});
        auto* knob = dynamic_cast<juce::Slider*>(find(*editor, "fatness"));
        require(knob != nullptr, "Fatness control missing");
        knob->setValue(63.0, juce::sendNotificationSync);
        require(std::abs(processor.parameters.getRawParameterValue("fatness")->load() - 63.0f) < 0.01f, "Knob not connected to DSP");
        processor.parameters.getParameter("fatness")->setValueNotifyingHost(1.0f);
        const auto aggressive = editor->createComponentSnapshot({0, 0, 180, 165});
        bool faceChanged = false;
        for (int y = 0; y < calm.getHeight(); ++y)
            for (int x = 0; x < calm.getWidth(); ++x)
                faceChanged = faceChanged || calm.getPixelAt(x, y) != aggressive.getPixelAt(x, y);
        require(faceChanged, "Host automation did not change the sausage graphics");
        save(*editor, output, "openSausage-rage.png");
        for (const auto* id : {"sausageBypass", "spaceBypass", "riseBypass", "widthBypass", "bypass"}) {
            auto* button = dynamic_cast<juce::TextButton*>(find(*editor, id));
            require(button != nullptr, "Bypass switch missing");
            button->setToggleState(true, juce::sendNotificationSync);
            require(processor.parameters.getRawParameterValue(id)->load() == 1.0f, "Bypass not connected to host parameter");
            button->setToggleState(false, juce::sendNotificationSync);
        }
        auto* recipes = dynamic_cast<juce::ComboBox*>(find(*editor, "recipes"));
        require(recipes != nullptr, "Recipe selector missing");
        recipes->setSelectedId(5, juce::sendNotificationSync);
        require(processor.parameters.getRawParameterValue("rise")->load() == 85.0f, "Recipe did not reach processor");
        for (const auto* id : {"spaceBypass", "widthBypass"})
            dynamic_cast<juce::TextButton*>(find(*editor, id))->setToggleState(true, juce::sendNotificationSync);
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        SausageProcessor restored;
        restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        restored.prepareToPlay(48000, 256);
        for (auto* parameter : processor.getParameters()) {
            auto* named = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter);
            require(named != nullptr, "Parameter ID missing");
            require(std::abs(parameter->getValue() - restored.parameters.getParameter(named->paramID)->getValue()) < 1.0e-6f, "Session recall failed");
        }
        for (const auto* id : {"spaceBypass", "widthBypass"})
            dynamic_cast<juce::TextButton*>(find(*editor, id))->setToggleState(false, juce::sendNotificationSync);
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;
        for (int block = 0; block < 200; ++block) {
            for (int i = 0; i < 256; ++i) {
                const auto sample = 0.2f * std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f * static_cast<float>(block * 256 + i) / 48000.0f);
                buffer.setSample(0, i, sample); buffer.setSample(1, i, sample);
            }
            processor.processBlock(buffer, midi);
        }
        save(*editor, output, "openSausage-lift-off.png");
        for (const auto* id : {"sausageBypass", "spaceBypass", "riseBypass", "widthBypass"})
            dynamic_cast<juce::TextButton*>(find(*editor, id))->setToggleState(true, juce::sendNotificationSync);
        save(*editor, output, "openSausage-bypassed.png");
        editor->setSize(870, 540);
        for (auto* child : editor->getChildren())
            if (dynamic_cast<juce::ResizableCornerComponent*>(child))
                require(std::abs(child->getBoundsInParent().getRight() - editor->getWidth()) <= 1,
                        "Resize handle moved away from the editor corner");
        save(*editor, output, "openSausage-compact.png");
        editor->setSize(1740, 1080);
        for (auto* child : editor->getChildren())
            if (dynamic_cast<juce::ResizableCornerComponent*>(child))
                require(std::abs(child->getBoundsInParent().getRight() - editor->getWidth()) <= 1,
                        "Resize handle moved outside the enlarged editor");
        std::cout << "PASS: four global algorithms, recipe preservation, legacy/current session recall, host automation, native controls, five bypass attachments, audio processing, parameter-reactive mascot, resize, and screenshots.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    return 0;
}
