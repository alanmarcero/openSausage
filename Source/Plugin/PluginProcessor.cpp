#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace {
constexpr const char* ids[] {"fatness", "color", "space", "size", "rise", "riseMix", "width", "spread", "output", "sausageBypass", "spaceBypass", "riseBypass", "widthBypass", "bypass", "algorithm", "satMix", "satDynamics", "satFocus", "preDelay", "masterMix", "softClip", "riseLength"};
juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    const char* names[] {"Fatness", "Color", "Space Mix", "Space Size", "Rise Amount", "Rise Mix", "Width Amount", "Width Spread"};
    for (int i = 0; i < 8; ++i)
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(ids[i], 1), names[i], juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), (i == 3 || i == 7) ? 50.0f : 0.0f));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("output", 1), "Wet Trim", juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));
    const char* switches[] {"Sausage Bypass", "Space Bypass", "Rise Bypass", "Width Bypass", "Master Bypass"};
    for (int i = 0; i < 5; ++i) result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(ids[i + 9], 1), switches[i], false));
    juce::StringArray algorithms;
    for (const auto* name : sausage::algorithmNames) algorithms.add(name);
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("algorithm", 2), "Global Algorithm", algorithms, 0));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("satMix", 1), "Saturation Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("satDynamics", 1), "Saturation Dynamics", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("satFocus", 1), "Saturation Focus", juce::StringArray {"Neutral", "Keep Low", "Keep High"}, 0));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("preDelay", 2), "Space Pre-Delay", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel("ms")));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("masterMix", 2), "Master Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("softClip", 2), "Soft Clip", false));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("riseLength", 1), "Rise Length",
        juce::StringArray {"Free", "1 Bar", "2 Bars", "4 Bars", "8 Bars", "16 Bars"}, 0));
    return result;
}
std::vector<juce::String> recipeNames() {
    std::vector<juce::String> names;
    for (const auto& recipe : SausageProcessor::factoryRecipes()) names.emplace_back(recipe.name);
    return names;
}
}
SausageProcessor::SausageProcessor(juce::File presetFolder)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "openSausageState", layout()),
      history(*this), compare(parameters, history),
      presets(std::move(presetFolder), recipeNames()) {
    static_assert(std::size(ids) == parameterCount);
    for (std::size_t i = 0; i < values.size(); ++i) values[i] = parameters.getRawParameterValue(ids[i]);
    setLatencySamples(sausage::Core::latency);
}
sausage::RackSettings SausageProcessor::settings(bool hostBypassed) const noexcept {
    auto value = [this](std::size_t i) { return values[i]->load(std::memory_order_relaxed); };
    auto config = sausage::RackSettings {value(0) / 100.0f, value(1) / 100.0f, value(2) / 100.0f, value(3) / 100.0f,
            value(4) / 100.0f, value(5) / 100.0f, value(6) / 100.0f, value(7) / 100.0f, value(8),
            value(9) < 0.5f, value(10) < 0.5f, value(11) < 0.5f, value(12) < 0.5f, hostBypassed || value(13) >= 0.5f,
            static_cast<sausage::Algorithm>(static_cast<int>(value(14))), value(15) / 100.0f, value(16) / 100.0f,
            static_cast<sausage::SaturationFocus>(static_cast<int>(value(17))),
            value(18), value(19) / 100.0f, value(20) >= 0.5f};
    constexpr int bars[] {0, 1, 2, 4, 8, 16};
    const float rawLength = value(21);
    const int rounded = std::isfinite(rawLength) ? static_cast<int>(std::lround(rawLength)) : 0;
    config.riseBars = bars[static_cast<std::size_t>(std::clamp(rounded, 0, 5))];
    return config;
}
void SausageProcessor::prepareToPlay(double sampleRate, int) { clockRate = sampleRate; rack.setSettings(settings()); rack.prepare(sampleRate); }
void SausageProcessor::releaseResources() { rack.reset(); }
bool SausageProcessor::isBusesLayoutSupported(const BusesLayout& buses) const {
    const auto input = buses.getMainInputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo()) && input == buses.getMainOutputChannelSet();
}
SausageProcessor::MusicalClock SausageProcessor::readClock(int samples) {
    MusicalClock clock;
    clock.ppq = visualQuarter;
    if (auto* host = getPlayHead(); host != nullptr && wrapperType != wrapperType_Standalone) {
        if (auto position = host->getPosition()) {
            clock.running = position->getIsPlaying();
            if (const auto bpm = position->getBpm(); bpm && std::isfinite(*bpm) && *bpm > 0) clock.tempo = *bpm;
            if (const auto signature = position->getTimeSignature(); signature && signature->numerator > 0 && signature->denominator > 0)
                clock.quartersPerBar = signature->numerator * (4.0 / signature->denominator);
            if (const auto ppq = position->getPpqPosition(); ppq && std::isfinite(*ppq)) clock.ppq = *ppq;
            else if (const auto seconds = position->getTimeInSeconds(); seconds && std::isfinite(*seconds))
                clock.ppq = *seconds * clock.tempo / 60.0;
        }
    }
    const double advance = clock.running && clockRate > 0 ? samples * clock.tempo / (60.0 * clockRate) : 0.0;
    visualQuarter = clock.ppq + advance;
    beatPhase.store(static_cast<float>(visualQuarter - std::floor(visualQuarter)), std::memory_order_relaxed);
    return clock;
}

void SausageProcessor::process(juce::AudioBuffer<float>& buffer, bool hostBypassed) {
    juce::ScopedNoDenormals noDenormals;
    const auto clock = readClock(buffer.getNumSamples());
    for (int channel = getTotalNumInputChannels(); channel < buffer.getNumChannels(); ++channel) buffer.clear(channel, 0, buffer.getNumSamples());
    if (buffer.getNumSamples() == 0 || getTotalNumInputChannels() == 0) return;
    float input = 0.0f;
    for (int channel = 0; channel < getTotalNumInputChannels(); ++channel) input = juce::jmax(input, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    auto config = settings(hostBypassed);
    config.risePpq = clock.ppq;
    config.riseTempo = clock.tempo;
    config.riseQuartersPerBar = clock.quartersPerBar;
    config.riseRunning = clock.running;
    rack.setSettings(config);
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
int SausageProcessor::getNumPrograms() { return static_cast<int>(factoryRecipes().size()); }
void SausageProcessor::setCurrentProgram(int index) {
    if (index < 0 || index >= getNumPrograms()) return;
    applyRecipe(index);
    updateHostDisplay();
}
const juce::String SausageProcessor::getProgramName(int index) {
    const auto& recipes = factoryRecipes();
    if (index < 0 || index >= static_cast<int>(recipes.size())) return {};
    return recipes[static_cast<std::size_t>(index)].name;
}
void SausageProcessor::getStateInformation(juce::MemoryBlock& destination) {
    auto state = parameters.copyState();
    state.setProperty("program", currentProgram, nullptr);
    compare.write(state);
    if (const auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}
void SausageProcessor::setStateInformation(const void* data, int bytes) {
    if (const auto xml = getXmlFromBinary(data, bytes))
        if (xml->hasTagName(parameters.state.getType())) {
            const ParameterUndo::Suspend guard(history);
            auto state = juce::ValueTree::fromXml(*xml);
            const int program = static_cast<int>(state.getProperty("program", 0));
            const int slot = static_cast<int>(state.getProperty("ab", 0));
            auto inactive = state.getChildWithName("inactiveSlot").createCopy();
            if (auto child = state.getChildWithName("inactiveSlot"); child.isValid()) state.removeChild(child, nullptr);
            if (!state.getChildWithProperty("id", "algorithm").isValid()) {
                auto algorithm = parameters.copyState().getChildWithProperty("id", "algorithm").createCopy();
                algorithm.setProperty("value", 0.0f, nullptr);
                state.appendChild(algorithm, nullptr);
            }
            for (const auto& [id, defaultValue] : std::array<std::pair<const char*, float>, 7>{{
                     {"satMix", 100.0f}, {"satDynamics", 100.0f}, {"satFocus", 0.0f},
                     {"preDelay", 0.0f}, {"masterMix", 100.0f}, {"softClip", 0.0f},
                     {"riseLength", 0.0f}}}) {
                if (!state.getChildWithProperty("id", id).isValid()) {
                    auto parameter = parameters.copyState().getChildWithProperty("id", id).createCopy();
                    parameter.setProperty("value", defaultValue, nullptr);
                    state.appendChild(parameter, nullptr);
                }
            }
            parameters.replaceState(state);
            currentProgram = program >= 0 && program < getNumPrograms() ? program : 0;
            compare.install(slot, inactive);
            history.clear();
        }
}
void SausageProcessor::loadParameterTree(const juce::ValueTree& tree) {
    const bool grouped = history.beginGroup("Preset");
    for (int index = 0; index < tree.getNumChildren(); ++index) {
        const auto child = tree.getChild(index);
        auto* parameter = parameters.getParameter(child.getProperty("id").toString());
        if (parameter == nullptr) continue;
        const float denormalised = child.getProperty("value");
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(denormalised));
        parameter->endChangeGesture();
    }
    if (grouped) history.endGroup();
}
juce::ValueTree SausageProcessor::copyParameterTree() { return parameters.copyState(); }
const std::vector<SausageRecipe>& SausageProcessor::factoryRecipes() {
    static const std::vector<SausageRecipe> recipes{
        // Uplifting (12) — indices 0, 3, 4 preserve EditorCapture test invariants
        {"Uplifting / Clean Slate",         0,  0,  0, 50,  0,  0,  0, 50,  0},
        {"Uplifting / Trance Supersaw",    45, 60, 40, 85, 45, 35, 90, 85, -7, 100, 100, 1, 28, 100, 1},
        {"Uplifting / Euphoria Vox",       30, 52, 46, 82, 18, 15, 78, 74, -5, 100, 100, 2, 34, 100, 0},
        {"Uplifting / In Orbit",           16, 38, 48, 78, 24, 16, 62, 70, -5},
        {"Uplifting / Lift Off",           38, 60, 35, 82, 85, 65, 75, 80, -8},
        {"Uplifting / Big Room Lead",      65, 45, 28, 65, 55, 40, 75, 70, -6, 100,  88, 1, 18, 100, 1},
        {"Uplifting / Vocal Riser",        25, 50, 60, 90, 95, 80, 70, 75, -5, 100, 100, 2, 35, 100, 0},
        {"Uplifting / Pure 138 Air",       22, 68, 34, 72,  0,  0, 84, 82, -3,  86,  92, 2, 24, 100, 0},
        {"Uplifting / Cathedral Arp",      34, 56, 54, 88, 20, 12, 80, 78, -5,  90, 100, 0, 42,  94, 0},
        {"Uplifting / Skyline Drop",       58, 48, 26, 60, 72, 58, 86, 76, -7,  88,  72, 1, 16, 100, 1},
        {"Uplifting / Afterglow Pad",      20, 44, 58, 92, 12, 10, 88, 86, -4, 100, 100, 2, 50,  86, 0},
        {"Uplifting / Mainstage Rise",     42, 62, 42, 80, 92, 72, 78, 80, -7, 100,  84, 0, 26, 100, 1},

        // Rolling (6)
        {"Rolling / Tech Bass Tight",      50, 15,  0, 30, 10,  0, 15, 25, -5, 100,  82, 1,  0, 100, 1},
        {"Rolling / Acid Top Grit",        64, 58, 14, 28, 18, 12, 44, 48, -6, 100,  76, 2,  0, 100, 1},
        {"Rolling / Wide Shuffle Hats",    26, 72, 20, 34,  0,  0, 86, 84, -3,  80, 100, 2, 12, 100, 0},
        {"Rolling / Kick Bus Punch",       46, 22,  0, 25,  0,  0, 10, 30, -5,  64,  58, 1,  0, 100, 1},
        {"Rolling / Groove Arp Forward",   48, 46, 24, 48, 22, 14, 58, 56, -5, 100,  85, 0, 14, 100, 0},
        {"Rolling / Clap Bus Glue",        38, 34, 28, 42,  0,  0, 62, 58, -4,  76,  70, 0, 18,  92, 0},

        // Progressive (6)
        {"Progressive / Gentle Heat",      32, 18,  0, 35,  0,  0, 12, 35, -4},
        {"Progressive / Deep Warmth",      35, 20, 25, 60, 15, 10, 40, 45, -4,  85, 100, 1, 22,  92, 0},
        {"Progressive / Velvet Chords",    28, 30, 44, 74,  0,  0, 68, 66, -4,  88, 100, 0, 32,  84, 0},
        {"Progressive / Melodic Synth",    42, 44, 36, 68, 24, 18, 72, 70, -5, 100,  94, 0, 24, 100, 0},
        {"Progressive / Wide Perc Air",    18, 64, 32, 52,  0,  0, 82, 80, -3,  75, 100, 2, 18,  90, 0},
        {"Progressive / Afterhours Pad",   24, 36, 52, 86, 16, 12, 78, 82, -4,  90, 100, 0, 46,  80, 0},

        // Psy (6)
        {"Psy / Squelch Driver",           76, 78, 16, 36, 34, 24, 54, 62, -7, 100,  82, 2,  0, 100, 1},
        {"Psy / FM Lead Stereo",           54, 66, 28, 54, 40, 32, 88, 84, -6, 100,  90, 0, 16, 100, 0},
        {"Psy / Space Zapper",             48, 70, 38, 66, 68, 54, 80, 78, -6, 100, 100, 0, 20, 100, 1},
        {"Psy / 140 Mid Focus",            62, 52, 12, 28, 18, 14, 48, 52, -6,  92,  74, 2,  0, 100, 1},
        {"Psy / Alien Tension Riser",      40, 74, 44, 76, 94, 78, 84, 80, -7, 100, 100, 0, 28, 100, 1},
        {"Psy / Tight Bass Glue",          56, 18,  0, 24,  0,  0,  8, 26, -5,  88,  60, 1,  0, 100, 1},

        // Tech (6)
        {"Tech / Future Rave Punch",       72, 65, 22, 50, 70, 55, 80, 75, -7, 100,  78, 1, 14, 100, 1},
        {"Tech / Hardstyle Distortion",    95, 85, 10, 25, 40, 30, 35, 40, -9, 100,  66, 0,  0, 100, 1},
        {"Tech / Electro Dirty Synth",     78, 80, 12, 35, 50, 45, 65, 60, -7, 100,  72, 0,  0, 100, 1},
        {"Tech / Festival Drop Slam",      80, 55, 30, 75, 80, 65, 85, 80, -8, 100,  64, 1, 20, 100, 1},
        {"Tech / Industrial Drum Slam",    84, 48, 18, 32,  0,  0, 36, 44, -8,  78,  48, 1,  0, 100, 1},
        {"Tech / Peak Acid Bite",          70, 72, 14, 30, 26, 18, 52, 50, -7,  94,  76, 2,  0, 100, 1},

        // Bass & Breaks (6)
        {"Bass & Breaks / Wobble Heat",    85, 70, 15, 40, 60, 50, 60, 65, -8, 100,  75, 1,  0, 100, 1},
        {"Bass & Breaks / DnB Break",      58, 40, 18, 35, 25, 20, 45, 50, -6,  74,  62, 1,  0, 100, 1},
        {"Bass & Breaks / Sub Crusher",    60, 10,  0, 20,  0,  0,  5, 20, -5, 100,  80, 1,  0, 100, 1},
        {"Bass & Breaks / Neuro Stack",    88, 76, 12, 32, 36, 28, 72, 68, -8,  92,  60, 1,  0, 100, 1},
        {"Bass & Breaks / Reece Wide Bus", 66, 54, 22, 46, 14, 10, 78, 74, -6,  86,  70, 1, 10, 100, 1},
        {"Bass & Breaks / Jungle Drums",   64, 46, 20, 34,  0,  0, 54, 52, -6,  68,  54, 0,  8,  94, 1},

        // Classic (6)
        {"Classic / Wide Drums",           55, 28, 12, 30,  0,  0, 68, 60, -6},
        {"Classic / Warm Console Bus",     36, 24,  8, 28,  0,  0, 22, 42, -3,  72,  85, 0,  0,  76, 0},
        {"Classic / Parallel Fattener",    68, 38, 14, 34,  0,  0, 38, 46, -5,  44,  78, 0,  0, 100, 0},
        {"Classic / Vintage Chamber",      24, 32, 42, 62,  0,  0, 52, 58, -4, 100, 100, 0, 25,  88, 0},
        {"Classic / French Touch Pump",    52, 44, 18, 36,  0,  0, 48, 52, -5,  86,  52, 1,  0, 100, 0},
        {"Classic / Analog Polish",        28, 36, 20, 45,  0,  0, 44, 54, -3,  78,  92, 0, 12,  90, 0}
    };
    return recipes;
}
void SausageProcessor::applyRecipe(int index) {
    const auto& recipes = factoryRecipes();
    if (index < 0 || index >= static_cast<int>(recipes.size())) return;
    currentProgram = index;
    const bool grouped = history.beginGroup("Recipe");
    const auto& r = recipes[static_cast<std::size_t>(index)];
    const std::array<std::pair<std::size_t, float>, 20> targetValues{{
        {0, r.fatness}, {1, r.color}, {2, r.space}, {3, r.size}, {4, r.rise},
        {5, r.riseMix}, {6, r.width}, {7, r.spread}, {8, r.output},
        {9, 0.0f}, {10, 0.0f}, {11, 0.0f}, {12, 0.0f}, {13, 0.0f},
        {15, r.satMix}, {16, r.satDynamics}, {17, r.satFocus},
        {18, r.preDelay}, {19, r.masterMix}, {20, r.softClip}
    }};
    std::ranges::for_each(targetValues, [this](const auto& entry) {
        auto* parameter = parameters.getParameter(ids[entry.first]);
        if (parameter == nullptr) return;
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(entry.second));
        parameter->endChangeGesture();
    });
    if (grouped) history.endGroup();
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SausageProcessor(); }
