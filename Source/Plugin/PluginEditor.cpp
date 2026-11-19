#include "PluginEditor.h"
#include "BinaryData.h"

namespace {
const juce::Colour ink(0xffe6f0f5), cream(0xffe6f0f5), panel(0xf2152026);
void text(juce::Graphics& g, const juce::String& value, juce::Rectangle<int> box, float height,
          juce::Colour color = ink, int style = juce::Font::plain, juce::Justification align = juce::Justification::centredLeft) {
    g.setColour(color);
    g.setFont(juce::FontOptions(height, style));
    g.drawText(value, box, align);
}
void dial(juce::Slider& slider, const juce::String& id, juce::Colour accent) {
    slider.setComponentID(id);
    slider.setName(id);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 24);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setTextValueSuffix(" %");
    slider.setNumDecimalPlacesToDisplay(1);
    slider.setColour(juce::Slider::textBoxTextColourId, ink);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setDoubleClickReturnValue(true, id == "size" || id == "spread" ? 50.0 : 0.0);
    slider.setWantsKeyboardFocus(true);
}
void meter(juce::Graphics& g, const juce::String& label, float peak, int x) {
    const auto db = juce::Decibels::gainToDecibels(peak, -60.0f);
    text(g, label, {x, 623, 90, 20}, 11.0f, cream, juce::Font::bold);
    text(g, db <= -60.0f ? "-inf dB" : juce::String(db, 1) + " dB", {x + 76, 623, 84, 20}, 11.0f, cream, juce::Font::plain, juce::Justification::centredRight);
    for (int segment = 0; segment < 23; ++segment) {
        const bool lit = db > -60.0f + static_cast<float>(segment) * 60.0f / 23.0f;
        const auto color = segment > 20 ? juce::Colour(0xffffbe3b) : juce::Colour(0xff48f0c8);
        g.setColour(lit ? color : juce::Colour(0xff313f42));
        g.fillRoundedRectangle(static_cast<float>(x + segment * 7), 651.0f, 5.0f, 15.0f, 1.0f);
    }
}
}
ModulePanel::ModulePanel(juce::AudioProcessorValueTreeState& state, const juce::String& name, const juce::String& sub,
                         const juce::String& primaryId, const juce::String& primaryName, const juce::String& secondaryId,
                         const juce::String& secondaryName, const juce::String& bypassId, juce::Colour color, const juce::String& description)
    : title(name), subtitle(sub), primaryLabel(primaryName), secondaryLabel(secondaryName), hint(description), accent(color),
      amount(state.getRawParameterValue(primaryId)), detail(state.getRawParameterValue(secondaryId)),
      moduleBypass(state.getRawParameterValue(bypassId)), masterBypass(state.getRawParameterValue("bypass")), algorithm(state.getRawParameterValue("algorithm")),
      primaryAttachment(state, primaryId, primary), secondaryAttachment(state, secondaryId, secondary), bypassAttachment(state, bypassId, bypass) {
    kind = primaryId == "space" ? 1 : primaryId == "rise" ? 2 : primaryId == "width" ? 3 : 0;
    dial(primary, primaryId, color); dial(secondary, secondaryId, color);
    primary.setTooltip(primaryName + ": " + description + " Drag to change; double-click to reset; click the value to type.");
    secondary.setTooltip(secondaryName + ": fine-tune this effect. Double-click to reset.");
    bypass.setComponentID(bypassId);
    bypass.setName(name + " bypass");
    bypass.setClickingTogglesState(true);
    bypass.setTooltip("Fully bypass " + name + ", including its wet signal and tails. Other modules keep running.");
    addAndMakeVisible(primary); addAndMakeVisible(secondary); addAndMakeVisible(bypass);
    if (kind == 0) {
        dial(saturationMix, "satMix", color);
        dial(dynamics, "satDynamics", color);
        for (auto* slider : {&saturationMix, &dynamics}) {
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 24);
            slider->setNumDecimalPlacesToDisplay(0);
        }
        saturationMix.setDoubleClickReturnValue(true, 100.0);
        dynamics.setDoubleClickReturnValue(true, 100.0);
        saturationMix.setTooltip("Blend the saturated signal with the clean signal. 0% is clean; 100% keeps the full existing Fatness/Color blend.");
        dynamics.setTooltip("Amount of the Fatness envelope compression. 0% bypasses compression; 100% uses the full compressor response.");
        focus.setComponentID("satFocus");
        focus.addItemList({"Neutral", "Keep Low", "Keep High"}, 1);
        focus.setTooltip("Neutral saturates the full signal. Keep Low leaves bass mostly clean and saturates highs; Keep High saturates bass and keeps highs cleaner.");
        saturationMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "satMix", saturationMix);
        dynamicsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "satDynamics", dynamics);
        focusAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "satFocus", focus);
        addAndMakeVisible(saturationMix); addAndMakeVisible(dynamics); addAndMakeVisible(focus);
    } else if (kind == 1) {
        dial(preDelay, "preDelay", color);
        preDelay.setSliderStyle(juce::Slider::LinearHorizontal);
        preDelay.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 24);
        preDelay.setTextValueSuffix(" ms");
        preDelay.setNumDecimalPlacesToDisplay(0);
        preDelay.setDoubleClickReturnValue(true, 0.0);
        preDelay.setTooltip("Space pre-delay (0 to 100 ms) so transients punch through before the reverb tail.");
        preDelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "preDelay", preDelay);
        addAndMakeVisible(preDelay);
    } else if (kind == 2) {
        riseLength.setComponentID("riseLength");
        riseLength.setName("Rise length");
        riseLength.addItemList({"Free", "1 Bar", "2 Bars", "4 Bars", "8 Bars", "16 Bars"}, 1);
        riseLength.setTooltip("Free holds the Rise pitch. 1, 2, 4, 8, or 16 bars sweeps from unison up to Rise, locked to the host tempo.");
        riseLengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "riseLength", riseLength);
        addAndMakeVisible(riseLength);
    }
}
void ModulePanel::paint(juce::Graphics& g) {
    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().translated(0, 3), 8.0f);
    const auto bounds = getLocalBounds().toFloat().reduced(1);
    g.setGradientFill(juce::ColourGradient(panel.brighter(0.08f), bounds.getTopLeft(), panel.darker(0.16f), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(accent.withAlpha(0.4f)); g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    g.setColour(cream.withAlpha(0.06f)); g.drawHorizontalLine(2, 10.0f, bounds.getRight() - 10.0f);
    g.setColour(accent); g.fillRoundedRectangle(16.0f, 16.0f, 4.0f, 34.0f, 2.0f);
    text(g, title, {28, 16, 120, 36}, 29.0f, ink, juce::Font::bold);
    constexpr const char* descriptions[4][4] {
        {"SATURATION + COMPRESSION", "STEREO REVERB", "PITCH + BUILDUP", "STEREO WIDENER"},
        {"ATAN / ANTIALIASED", "HOUSEHOLDER HALL", "FOUR-GRAIN RISE", "ALLPASS WIDTH"},
        {"CLIP / ANTIALIASED", "TIGHT ROOM", "SHORT-GRAIN RISE", "BASS-FOCUSED WIDTH"},
        {"SINE FOLD / ANTIALIASED", "HADAMARD HALL", "CROSS-FEEDBACK RISE", "MODULATED WIDTH"}
    };
    const auto mode = juce::jlimit(0, 3, static_cast<int>(algorithm->load()));
    text(g, mode == 0 ? subtitle : descriptions[mode][kind], {20, 57, 210, 22}, 12.0f, juce::Colour(0xffa3afaf));
    const bool isModuleActive = (moduleBypass->load() <= 0.5f) && (masterBypass->load() <= 0.5f) && (amount->load() > 0.0f);
    const auto ledColor = isModuleActive ? accent : juce::Colour(0xff3b4649);
    g.setColour(ledColor);
    g.fillEllipse(240.0f, 64.0f, 6.0f, 6.0f);
    if (isModuleActive) {
        g.setColour(accent.withAlpha(0.4f));
        g.drawEllipse(238.0f, 62.0f, 10.0f, 10.0f, 1.2f);
    }
    g.setColour(juce::Colour(0xff3b4649)); g.drawHorizontalLine(88, 20.0f, 246.0f);
    if (kind != 0)
        text(g, primaryLabel.toUpperCase(), {20, 101, 226, 20}, 12.0f, ink, juce::Font::bold, juce::Justification::centred);
    if (kind == 0) {
        text(g, primaryLabel.toUpperCase(), {8, 102, 120, 18}, 11.0f, ink, juce::Font::bold, juce::Justification::centred);
        text(g, "SATURATION MIX", {130, 102, 122, 18}, 10.0f, ink, juce::Font::bold, juce::Justification::centred);
        text(g, "DYNAMICS", {130, 162, 122, 18}, 10.0f, ink, juce::Font::bold, juce::Justification::centred);
        text(g, secondaryLabel.toUpperCase(), {8, 294, 116, 18}, 10.0f, ink, juce::Font::bold, juce::Justification::centred);
        text(g, "SATURATION FOCUS", {130, 222, 122, 18}, 9.0f, ink, juce::Font::bold, juce::Justification::centred);
        g.setFont(juce::FontOptions(10.5f)); g.setColour(juce::Colour(0xffa3afaf));
        g.drawFittedText(hint, {130, 288, 122, 44}, juce::Justification::topLeft, 3);
    } else if (kind == 1) {
        text(g, secondaryLabel.toUpperCase(), {132, 304, 114, 18}, 11.0f, ink, juce::Font::bold);
        text(g, "PRE-DELAY", {132, 326, 114, 16}, 10.0f, juce::Colour(0xffa3afaf), juce::Font::bold);
    } else if (kind == 2) {
        text(g, secondaryLabel.toUpperCase(), {132, 304, 114, 18}, 11.0f, ink, juce::Font::bold);
        text(g, "LENGTH", {132, 326, 114, 16}, 10.0f, juce::Colour(0xffa3afaf), juce::Font::bold);
    } else {
        text(g, secondaryLabel.toUpperCase(), {132, 323, 114, 24}, 12.0f, ink, juce::Font::bold);
        g.setFont(juce::FontOptions(12.0f)); g.setColour(juce::Colour(0xffa3afaf));
        g.drawFittedText(hint, {132, 350, 112, 30}, juce::Justification::topLeft, 2);
    }
    const auto value = moduleBypass->load() > 0.5f || masterBypass->load() > 0.5f ? 0.0f : amount->load() / 100.0f;
    const auto secondaryValue = detail->load() / 100.0f;
    const auto motion = phase * value;
    g.setColour(value > 0.0f ? accent.darker(0.2f) : juce::Colour(0xff546164));
    if (kind == 0) {
        juce::Path waveform;
        for (int x = 0; x <= 104; ++x) {
            const auto wave = std::tanh(std::sin(static_cast<float>(x) * 0.14f + motion) * (1.0f + value * 8.0f));
            const auto y = 396.0f - wave * (3.0f + value * 6.0f);
            if (x == 0) waveform.startNewSubPath(132.0f, y); else waveform.lineTo(132.0f + static_cast<float>(x), y);
        }
        g.strokePath(waveform, juce::PathStrokeType(1.7f));
    } else if (kind == 1) {
        for (int ring = 0; ring < 4; ++ring) {
            const auto radius = 3.0f + std::fmod(static_cast<float>(ring) * 8.0f + motion * 5.0f, 32.0f) * (0.4f + value * 0.6f);
            g.drawEllipse(184.0f - radius, 396.0f - radius * 0.32f, radius * 2.0f, radius * 0.64f, 1.3f);
        }
    } else if (kind == 2) {
        for (int arrow = 0; arrow < 4; ++arrow) {
            const auto x = 142.0f + static_cast<float>(arrow) * 26.0f;
            const auto y = 402.0f - std::fmod(static_cast<float>(arrow) * 4.0f + motion * 8.0f, 18.0f);
            juce::Path chevron; chevron.startNewSubPath(x - 5, y + 4); chevron.lineTo(x, y - 2); chevron.lineTo(x + 5, y + 4);
            g.strokePath(chevron, juce::PathStrokeType(1.8f));
        }
    } else {
        const auto distance = 4.0f + value * 28.0f;
        const auto wave = std::sin(motion) * value * 3.0f;
        g.drawEllipse(178.0f - distance, 387.0f + wave, 16.0f + secondaryValue * 8.0f, 18.0f, 1.6f);
        g.drawEllipse(178.0f + distance, 387.0f - wave, 16.0f + secondaryValue * 8.0f, 18.0f, 1.6f);
        g.drawLine(142.0f, 396.0f, 232.0f, 396.0f, 0.7f);
    }
}
void ModulePanel::resized() {
    bypass.setBounds(154, 20, 94, 36);
    if (kind == 0) {
        primary.setBounds(4, 118, 126, 166);
        secondary.setBounds(10, 314, 108, 94);
        saturationMix.setBounds(130, 122, 122, 36);
        dynamics.setBounds(130, 182, 122, 36);
        focus.setBounds(130, 244, 122, 36);
    } else {
        primary.setBounds(44, 126, 178, 178);
        secondary.setBounds(20, 314, 100, 96);
        if (kind == 1) preDelay.setBounds(130, 344, 122, 36);
        if (kind == 2) riseLength.setBounds(130, 344, 122, 36);
    }
}
SausageEditor::SausageEditor(SausageProcessor& owner)
    : AudioProcessorEditor(owner), ownerProcessor(owner),
      enamel(juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize)),
      sausagePanel(owner.parameters, "Sausage", "SATURATION + COMPRESSION", "fatness", "Fatness", "color", "Color", "sausageBypass", juce::Colour(0xff48f0c8), "4x saturation and compression."),
      spacePanel(owner.parameters, "Space", "STEREO REVERB", "space", "Mix", "size", "Size", "spaceBypass", juce::Colour(0xff48f0c8), "Reverb mix. Size sets decay."),
      risePanel(owner.parameters, "Rise", "PITCH + BUILDUP", "rise", "Rise", "riseMix", "Mix", "riseBypass", juce::Colour(0xff48f0c8), "Pitch shift, up to an octave."),
      widthPanel(owner.parameters, "Width", "STEREO WIDENER", "width", "Width", "spread", "Spread", "widthBypass", juce::Colour(0xff48f0c8), "Stereo width and spacing."),
      outputAttachment(owner.parameters, "output", output), masterMixAttachment(owner.parameters, "masterMix", masterMix),
      bypassAttachment(owner.parameters, "bypass", bypass), softClipAttachment(owner.parameters, "softClip", softClip),
      suite(owner) {
    setLookAndFeel(&look);
    grill = juce::Drawable::createFromImageData(BinaryData::grill_svg, BinaryData::grill_svgSize);
    for (auto* p : {&sausagePanel, &spacePanel, &risePanel, &widthPanel}) addAndMakeVisible(p);
    dial(output, "output", juce::Colour(0xffffbe3b));
    output.setTextValueSuffix(" dB"); output.setTooltip("Final output trim, after all four effects. Master bypass ignores trim.");
    output.setColour(juce::Slider::textBoxTextColourId, cream);
    dial(masterMix, "masterMix", juce::Colour(0xff48f0c8));
    masterMix.setDoubleClickReturnValue(true, 100.0);
    masterMix.setTooltip("Global dry/wet parallel rack blend (0% dry to 100% wet).");
    masterMix.setColour(juce::Slider::textBoxTextColourId, cream);
    softClip.setComponentID("softClip"); softClip.setName("Soft Clip"); softClip.setClickingTogglesState(true);
    softClip.setTooltip("Anti-aliased post-rack soft clipper ceiling so aggressive builds never exceed 0 dBFS.");
    bypass.setComponentID("bypass"); bypass.setName("Master bypass"); bypass.setClickingTogglesState(true);
    bypass.setTooltip("Bypass the complete chain, preserving the reported 16-sample latency.");
    recipes.setComponentID("recipes"); recipes.setTextWhenNothingSelected("CHOOSE A RECIPE");
    recipes.setTooltip("Choose a dance-style factory recipe for the four effect sections");
    std::vector<juce::String> groups;
    std::vector<juce::PopupMenu> menus;
    std::ranges::for_each(SausageProcessor::factoryRecipes(), [&, id = 1](const SausageRecipe& recipe) mutable {
        const juce::String full(recipe.name);
        const auto group = full.upToFirstOccurrenceOf(" / ", false, false);
        const auto it = std::ranges::find(groups, group);
        const auto idx = static_cast<std::size_t>(std::distance(groups.begin(), it));
        if (it == groups.end()) {
            groups.push_back(group);
            menus.emplace_back();
        }
        menus[idx].addItem(id++, full);
    });
    auto* root = recipes.getRootMenu();
    std::ranges::for_each(groups, [&, i = std::size_t{0}](const juce::String& group) mutable {
        root->addSubMenu(group, menus[i++]);
    });
    recipes.onChange = [this] {
        const auto selected = recipes.getSelectedId();
        if (selected > 0) {
            currentRecipe = selected - 1;
            ownerProcessor.setCurrentProgram(currentRecipe);
        }
    };
    prevRecipe.setComponentID("prevRecipe");
    prevRecipe.setTooltip("Previous recipe");
    prevRecipe.onClick = [this] {
        const int num = recipes.getNumItems();
        if (num <= 0) return;
        currentRecipe = (currentRecipe <= 0 ? num : currentRecipe) - 1;
        ownerProcessor.setCurrentProgram(currentRecipe);
        recipes.setSelectedId(currentRecipe + 1, juce::dontSendNotification);
    };
    nextRecipe.setComponentID("nextRecipe");
    nextRecipe.setTooltip("Next recipe");
    nextRecipe.onClick = [this] {
        const int num = recipes.getNumItems();
        if (num <= 0) return;
        currentRecipe = (currentRecipe + 1) % num;
        ownerProcessor.setCurrentProgram(currentRecipe);
        recipes.setSelectedId(currentRecipe + 1, juce::dontSendNotification);
    };
    algorithms.setComponentID("algorithm"); algorithms.setName("Global algorithm");
    for (int i = 0; i < 4; ++i) algorithms.addItem(sausage::algorithmNames[i], i + 1);
    algorithms.setTooltip("One algorithm set for all four effects. Classic is the original set. Velvet uses atan and a hall. Punch uses clipping and a short room. Prism uses wavefolding and a brighter hall. Knob settings stay put.");
    algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.parameters, "algorithm", algorithms);
    addAndMakeVisible(output); addAndMakeVisible(masterMix); addAndMakeVisible(softClip); addAndMakeVisible(bypass); addAndMakeVisible(recipes); addAndMakeVisible(algorithms);
    addAndMakeVisible(prevRecipe); addAndMakeVisible(nextRecipe); addAndMakeVisible(suite);
    setWantsKeyboardFocus(true);
    setResizable(true, true);
    setResizeLimits(1160, 720, 1740, 1080);
    getConstrainer()->setFixedAspectRatio(1160.0 / 720.0);
    setSize(1289, 800);
    startTimerHz(25);
}
SausageEditor::~SausageEditor() { stopTimer(); setLookAndFeel(nullptr); }
float SausageEditor::animationPhase() const noexcept {
    const float phase = ownerProcessor.beatPhase.load(std::memory_order_relaxed);
    const double idle = std::fmod(juce::Time::getMillisecondCounterHiRes(), 500.0) / 500.0;
    return (phase >= 0.0f ? phase : static_cast<float>(idle)) * juce::MathConstants<float>::twoPi;
}
bool SausageEditor::keyPressed(const juce::KeyPress& key) {
    const auto text = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    const auto code = key.getKeyCode();
    const bool isZ = text == 'z' || code == 'z' || code == 'Z';
    if (!isZ || !key.getModifiers().isCommandDown()) return false;
    if (key.getModifiers().isShiftDown()) ownerProcessor.history.redo();
    else ownerProcessor.history.undo();
    suite.refresh();
    return true;
}
void SausageEditor::timerCallback() {
    const int program = ownerProcessor.getCurrentProgram();
    if (program != currentRecipe && program >= 0) {
        currentRecipe = program;
        recipes.setSelectedId(program + 1, juce::dontSendNotification);
    }
    suite.refresh();
    for (auto* p : {&sausagePanel, &spacePanel, &risePanel, &widthPanel}) p->setBeatPhase(animationPhase());
    const float scale = static_cast<float>(getWidth()) / 1160.0f;
    repaint(juce::Rectangle<float>(20.0f * scale, 4.0f * scale, 175.0f * scale, 160.0f * scale).toNearestInt());
    repaint(juce::Rectangle<float>(24.0f * scale, 615.0f * scale, 845.0f * scale, 65.0f * scale).toNearestInt());
}
void SausageEditor::paint(juce::Graphics& g) {
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / 1160.0f));
    g.fillAll(juce::Colour(0xff0d1317));
    g.drawImage(enamel, juce::Rectangle<float>(0, 0, 1160, 720), juce::RectanglePlacement::stretchToFit);
    g.setColour(juce::Colour(0xff0d1317).withAlpha(0.95f)); g.fillRect(0, 0, 1160, 720);
    const bool sausageBypassed = ownerProcessor.parameters.getRawParameterValue("sausageBypass")->load() > 0.5f
                             || ownerProcessor.parameters.getRawParameterValue("bypass")->load() > 0.5f;
    const auto fat = sausageBypassed ? 0.0f : ownerProcessor.parameters.getRawParameterValue("fatness")->load() / 100.0f;
    if (grill) {
        juce::Graphics::ScopedSaveState save(g);
        const float pulse = std::cos(animationPhase());
        const float width = 135.0f + 15.0f*fat + 3.0f*pulse;
        g.addTransform(juce::AffineTransform::rotation(.042f*pulse, 102.0f, 86.0f));
        grill->drawWithin(g, {101.0f-width*.5f, 8.0f - 3.0f*pulse, width, 145.0f},
                          juce::RectanglePlacement::centred, 1.0f);
    }
    text(g, "openSausage", {186, 30, 480, 56}, 46.0f, ink, juce::Font::bold);
    text(g, "Saturation / Space / Pitch / Stereo", {190, 94, 330, 25}, 14.0f, ink, juce::Font::bold);
    text(g, "v" JucePlugin_VersionString, {190, 122, 150, 18}, 12.0f, juce::Colour(0xff869ca8));
    text(g, "Algorithm", {532, 81, 145, 20}, 14.0f, ink, juce::Font::bold);
    text(g, "Recipe", {706, 81, 412, 20}, 14.0f, ink, juce::Font::bold);
    text(g, "ALAN MARCERO", {930, 31, 196, 24}, 15.0f, ink, juce::Font::bold, juce::Justification::centredRight);
    g.setColour(juce::Colour(0xf00c1418)); g.fillRect(0, 609, 1160, 111);
    g.setColour(juce::Colour(0xff48f0c8).withAlpha(0.4f)); g.drawHorizontalLine(609, 24, 1136);
    meter(g, "INPUT", ownerProcessor.inputPeak.load(std::memory_order_relaxed), 30);
    meter(g, "OUTPUT", ownerProcessor.outputPeak.load(std::memory_order_relaxed), 214);
    text(g, "Output", {394, 638, 66, 22}, 13.0f, cream, juce::Font::bold);
    text(g, "Rack mix", {562, 638, 74, 22}, 13.0f, cream, juce::Font::bold);
    text(g, "Compression", {738, 623, 126, 22}, 13.0f, cream, juce::Font::bold);
    text(g, juce::String(ownerProcessor.gainReduction.load(std::memory_order_relaxed), 1) + " dB", {738, 646, 126, 28}, 21.0f, juce::Colour(0xffffbe3b), juce::Font::bold);
    g.setColour(juce::Colour(0xff3b474a)); g.drawHorizontalLine(687, 24, 1136);
    for (const auto point : {juce::Point<float>(10, 10), juce::Point<float>(1150, 10), juce::Point<float>(10, 600), juce::Point<float>(1150, 600)}) {
        g.setColour(ink.withAlpha(0.45f)); g.fillEllipse(point.x - 3, point.y - 3, 6, 6);
        g.setColour(juce::Colour(0xffd8b15a)); g.drawLine(point.x - 2, point.y, point.x + 2, point.y, 1);
    }
}
void SausageEditor::resized() {
    sausagePanel.setBounds(24, 172, 266, 415); spacePanel.setBounds(306, 172, 266, 415);
    risePanel.setBounds(588, 172, 266, 415); widthPanel.setBounds(870, 172, 266, 415);
    output.setBounds(456, 611, 96, 74); masterMix.setBounds(632, 611, 96, 74);
    softClip.setBounds(864, 633, 112, 36); bypass.setBounds(986, 633, 146, 36);
    prevRecipe.setBounds(706, 104, 74, 36);
    recipes.setBounds(786, 104, 236, 36);
    nextRecipe.setBounds(1028, 104, 90, 36);
    algorithms.setBounds(532, 104, 158, 36);
    suite.setBounds(0, 688, 1160, 32);
    const auto transform = juce::AffineTransform::scale(static_cast<float>(getWidth()) / 1160.0f);
    std::ranges::for_each(std::initializer_list<juce::Component*> {&sausagePanel, &spacePanel, &risePanel, &widthPanel, &output, &masterMix, &softClip, &bypass, &recipes, &algorithms, &prevRecipe, &nextRecipe, &suite},
                          [transform](juce::Component* child) { child->setTransform(transform); });
}
