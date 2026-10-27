#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
constexpr const char* ids[] {"fatness", "color", "space", "size", "rise", "riseMix", "width", "spread", "output", "sausageBypass", "spaceBypass", "riseBypass", "widthBypass", "bypass", "algorithm"};
juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    const char* names[] {"Fatness", "Color", "Space Mix", "Space Size", "Rise Amount", "Rise Mix", "Width Amount", "Width Spread"};
    for (int i = 0; i < 8; ++i)
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ids[i], 1), names[i], juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), (i == 3 || i == 7) ? 50.0f : 0.0f));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("output", 1), "Output", juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));
    const char* switches[] {"Sausage Bypass", "Space Bypass", "Rise Bypass", "Width Bypass", "Master Bypass"};
    for (int i = 0; i < 5; ++i) result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ids[i + 9], 1), switches[i], false));
    juce::StringArray algorithms;
    for (const auto* name : sausage::algorithmNames) algorithms.add(name);
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("algorithm", 2), "Global Algorithm", algorithms, 0));
    return result;
}
}
SausageProcessor::SausageProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "openSausageState", layout()) {
    for (std::size_t i = 0; i < values.size(); ++i) values[i] = parameters.getRawParameterValue(ids[i]);
    setLatencySamples(sausage::Core::latency);
}
sausage::RackSettings SausageProcessor::settings(bool hostBypassed) const noexcept {
    auto value = [this](std::size_t i) { return values[i]->load(std::memory_order_relaxed); };
    return {value(0) / 100.0f, value(1) / 100.0f, value(2) / 100.0f, value(3) / 100.0f,
            value(4) / 100.0f, value(5) / 100.0f, value(6) / 100.0f, value(7) / 100.0f, value(8),
            value(9) < 0.5f, value(10) < 0.5f, value(11) < 0.5f, value(12) < 0.5f, hostBypassed || value(13) >= 0.5f, static_cast<sausage::Algorithm>(static_cast<int>(value(14)))};
}
void SausageProcessor::prepareToPlay(double sampleRate, int) { rack.setSettings(settings()); rack.prepare(sampleRate); }
void SausageProcessor::releaseResources() { rack.reset(); }
bool SausageProcessor::isBusesLayoutSupported(const BusesLayout& buses) const {
    const auto input = buses.getMainInputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo()) && input == buses.getMainOutputChannelSet();
}
void SausageProcessor::process(juce::AudioBuffer<float>& buffer, bool hostBypassed) {
    juce::ScopedNoDenormals noDenormals;
    for (int channel = getTotalNumInputChannels(); channel < buffer.getNumChannels(); ++channel) buffer.clear(channel, 0, buffer.getNumSamples());
    if (buffer.getNumSamples() == 0 || getTotalNumInputChannels() == 0) return;
    float input = 0.0f;
    for (int channel = 0; channel < getTotalNumInputChannels(); ++channel) input = juce::jmax(input, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    rack.setSettings(settings(hostBypassed));
    rack.process(buffer.getWritePointer(0), getTotalNumInputChannels() > 1 ? buffer.getWritePointer(1) : nullptr, buffer.getNumSamples());
    float output = 0.0f;
    for (int channel = 0; channel < getTotalNumInputChannels(); ++channel) output = juce::jmax(output, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    inputPeak.store(input, std::memory_order_relaxed);
    outputPeak.store(output, std::memory_order_relaxed);
    gainReduction.store(hostBypassed || values[13]->load() > 0.5f || values[9]->load() > 0.5f ? 0.0f : std::max(0.0f, rack.reductionDb()), std::memory_order_relaxed);
}
void SausageProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, false); }
void SausageProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, true); }
juce::AudioProcessorParameter* SausageProcessor::getBypassParameter() const { return parameters.getParameter("bypass"); }
juce::AudioProcessorEditor* SausageProcessor::createEditor() { return new SausageEditor(*this); }
void SausageProcessor::getStateInformation(juce::MemoryBlock& destination) {
    if (const auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, destination);
}
void SausageProcessor::setStateInformation(const void* data, int bytes) {
    if (const auto xml = getXmlFromBinary(data, bytes))
        if (xml->hasTagName(parameters.state.getType())) {
            auto state = juce::ValueTree::fromXml(*xml);
            if (!state.getChildWithProperty("id", "algorithm").isValid()) {
                auto algorithm = parameters.copyState().getChildWithProperty("id", "algorithm").createCopy();
                algorithm.setProperty("value", 0.0f, nullptr);
                state.appendChild(algorithm, nullptr);
            }
            parameters.replaceState(state);
        }
}
void SausageProcessor::applyRecipe(int index) {
    constexpr float recipes[][9] {
        {0, 0, 0, 50, 0, 0, 0, 50, 0},
        {32, 18, 0, 35, 0, 0, 12, 35, -4},
        {55, 28, 12, 30, 0, 0, 68, 60, -6},
        {16, 38, 48, 78, 24, 16, 62, 70, -5},
        {38, 60, 35, 82, 85, 65, 75, 80, -8}
    };
    if (index < 0 || index >= 5) return;
    for (std::size_t i = 0; i < 14; ++i) {
        auto* parameter = parameters.getParameter(ids[i]);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(i < 9 ? recipes[index][i] : 0.0f));
        parameter->endChangeGesture();
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SausageProcessor(); }
