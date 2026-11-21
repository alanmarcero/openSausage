#include "PluginEditor.h"
#include "BinaryData.h"
#include <ranges>

namespace {
const juce::Colour ink(0xffe6f0f5), cream(0xffe6f0f5), panel(0xf2152026);
constexpr int designWidth = 1160, designHeight = 760;
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
    slider.setNumDecimalPlacesToDisplay(1);
    slider.setColour(juce::Slider::textBoxTextColourId, ink);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setDoubleClickReturnValue(true, id == "size" || id == "spread" ? 50.0 : 0.0);
    slider.setWantsKeyboardFocus(true);
}
}
LevelMeter::LevelMeter(juce::String name) : label(std::move(name)) {}
void LevelMeter::push(float peak, double nowMs) {
    const auto before = std::tuple(level, held, clipped);
    level = peak;
    if (peak >= held || nowMs - heldAt >= 1000.0) {
        held = peak;
        heldAt = nowMs;
    }
    clipped = clipped || peak > 1.0f;
    if (std::tuple(level, held, clipped) != before) repaint();
}
void LevelMeter::paint(juce::Graphics& g) {
    const auto heldDb = juce::Decibels::gainToDecibels(held, -60.0f);
    text(g, label, {0, 0, 90, 18}, 12.0f, cream, juce::Font::bold);
    auto readout = juce::String("-inf");
    if (heldDb > -60.0f) readout = (heldDb > 0.0f ? "+" : "") + juce::String(heldDb, 1);
    text(g, readout + " dBFS", {60, 0, 100, 18}, 12.0f, cream, juce::Font::plain, juce::Justification::centredRight);
    // 22 segments span -60 to 0 dBFS. The segment of the held peak stays lit.
    const auto topSegment = [](float gain) {
        return std::min(21, static_cast<int>(std::ceil((juce::Decibels::gainToDecibels(gain, -60.0f) + 60.0f) * 22.0f / 60.0f)) - 1);
    };
    const int levelTop = topSegment(level), heldTop = topSegment(held);
    std::ranges::for_each(std::views::iota(0, 22), [&](int segment) {
        const bool lit = segment <= levelTop || segment == heldTop;
        const auto litColour = segment >= 20 ? 0xffffbe3b : 0xff48f0c8;
        g.setColour(juce::Colour(lit ? litColour : 0xff313f42));
        g.fillRoundedRectangle(static_cast<float>(segment * 7), 26.0f, 5.0f, 16.0f, 1.0f);
    });
    g.setColour(juce::Colour(clipped ? 0xffff4d4d : 0xff313f42));
    g.fillRoundedRectangle(154.0f, 26.0f, 5.0f, 16.0f, 1.0f);
}
void LevelMeter::mouseDown(const juce::MouseEvent&) {
    clipped = false;
    repaint();
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
        {"SOFT SATURATION", "DARK HALL", "SMOOTH RISE", "DIFFUSE WIDTH"},
        {"HARD-CLIP SATURATION", "TIGHT ROOM", "CLEAN RISE", "BASS-SAFE WIDTH"},
        {"WAVEFOLD SATURATION", "BRIGHT HALL", "FEEDBACK RISE", "MOVING WIDTH"}
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
    // Each caption is centred above its control.
    const auto caption = [&g](const juce::String& label, const juce::Component& control) {
        text(g, label, control.getBounds().withY(control.getY() - 20).withHeight(18), 12.0f, ink, juce::Font::bold, juce::Justification::centred);
    };
    caption(primaryLabel.toUpperCase(), primary);
    caption(secondaryLabel.toUpperCase(), secondary);
    if (kind == 0) {
        caption("SATURATION MIX", saturationMix);
        caption("DYNAMICS", dynamics);
        caption("SATURATION FOCUS", focus);
    }
    if (kind == 1) caption("PRE-DELAY", preDelay);
    if (kind == 2) caption("LENGTH", riseLength);
    g.setFont(juce::FontOptions(12.0f)); g.setColour(juce::Colour(0xffa3afaf));
    if (kind == 0) g.drawFittedText(hint, {130, 288, 122, 44}, juce::Justification::topLeft, 3);
    if (kind == 3) g.drawFittedText(hint, {132, 350, 112, 30}, juce::Justification::topLeft, 2);
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
void ModulePanel::setBeatPhase(float position) {
    const bool bypassed = moduleBypass->load() > 0.5f || masterBypass->load() > 0.5f;
    const auto value = bypassed ? 0.0f : amount->load();
    const std::array<float, 2> header {algorithm->load(), value > 0.0f ? 1.0f : 0.0f};
    const std::array<float, 3> display {value, detail->load(), position * value};
    if (header != shownHeader) repaint(20, 57, 232, 22);
    if (display != shownDisplay) repaint(130, 378, 122, 34);
    shownHeader = header;
    shownDisplay = display;
    phase = position;
}
void ModulePanel::resized() {
    bypass.setBounds(154, 20, 94, 36);
    if (kind == 0) {
        primary.setBounds(4, 118, 126, 166);
        secondary.setBounds(10, 314, 108, 96);
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
    addAndMakeVisible(content);
    content.setBounds(0, 0, designWidth, designHeight);
    for (auto* p : {&sausagePanel, &spacePanel, &risePanel, &widthPanel}) content.addAndMakeVisible(p);
    dial(output, "output", juce::Colour(0xffffbe3b));
    output.setTooltip("Wet trim: level of the processed signal before Soft Clip and Rack mix. It has no effect at Rack mix 0%. Master bypass skips it.");
    output.setColour(juce::Slider::textBoxTextColourId, cream);
    inputMeter.setComponentID("inputMeter");
    outputMeter.setComponentID("outputMeter");
    inputMeter.setTooltip("Input peak in dBFS, held for 1 s. The red light latches above 0 dBFS; click to clear it.");
    outputMeter.setTooltip("Output peak in dBFS, held for 1 s. The red light latches above 0 dBFS; click to clear it.");
    dial(masterMix, "masterMix", juce::Colour(0xff48f0c8));
    masterMix.setDoubleClickReturnValue(true, 100.0);
    masterMix.setTooltip("Rack mix: blend of the dry input and the processed rack. 0% is dry, 100% is fully processed.");
    masterMix.setColour(juce::Slider::textBoxTextColourId, cream);
    softClip.setComponentID("softClip"); softClip.setName("Soft Clip"); softClip.setClickingTogglesState(true);
    softClip.setTooltip("Anti-aliased post-rack soft clipper ceiling so aggressive builds never exceed 0 dBFS.");
    bypass.setComponentID("bypass"); bypass.setName("Master bypass"); bypass.setClickingTogglesState(true);
    bypass.setTooltip("Bypass the complete chain. Latency stays at " + juce::String(owner.getLatencySamples()) + " samples.");
    algorithms.setComponentID("algorithm"); algorithms.setName("Global algorithm");
    for (int i = 0; i < 4; ++i) algorithms.addItem(sausage::algorithmNames[i], i + 1);
    algorithms.setTooltip("One algorithm set for all four effects. Classic is the original set. Velvet uses atan and a hall. Punch uses clipping and a short room. Prism uses wavefolding and a brighter hall. Knob settings stay put.");
    algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.parameters, "algorithm", algorithms);
    for (auto* child : std::initializer_list<juce::Component*> {&output, &masterMix, &inputMeter, &outputMeter, &softClip, &bypass, &algorithms, &suite, &suite.presetRow})
        content.addAndMakeVisible(child);
    setWantsKeyboardFocus(true);
    // Read the saved width first: the resize limits below call resized(), which stores the width.
    const int savedWidth = owner.editorWidth.load();
    setResizable(true, true);
    setResizeLimits(designWidth * 3 / 4, designHeight * 3 / 4, designWidth * 3 / 2, designHeight * 3 / 2);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(designWidth) / designHeight);
    const int width = juce::jlimit(designWidth * 3 / 4, designWidth * 3 / 2, savedWidth > 0 ? savedWidth : designWidth);
    setSize(width, width * designHeight / designWidth);
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
    suite.refresh();
    for (auto* p : {&sausagePanel, &spacePanel, &risePanel, &widthPanel}) p->setBeatPhase(animationPhase());
    content.repaint(20, 4, 175, 160);
    const auto now = juce::Time::getMillisecondCounterHiRes();
    inputMeter.push(ownerProcessor.inputPeak.exchange(0.0f), now);
    outputMeter.push(ownerProcessor.outputPeak.exchange(0.0f), now);
    content.repaint(738, 646, 126, 28);
}
void SausageEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff0d1317)); }
void SausageEditor::paintContent(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff0d1317));
    g.drawImage(enamel, juce::Rectangle<float>(0, 0, designWidth, designHeight), juce::RectanglePlacement::stretchToFit);
    g.setColour(juce::Colour(0xff0d1317).withAlpha(0.95f)); g.fillRect(0, 0, designWidth, designHeight);
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
    text(g, "Preset", {706, 81, 412, 20}, 14.0f, ink, juce::Font::bold);
    text(g, "ALAN MARCERO", {930, 31, 196, 24}, 15.0f, ink, juce::Font::bold, juce::Justification::centredRight);
    g.setColour(juce::Colour(0xf00c1418)); g.fillRect(0, 596, designWidth, designHeight - 596);
    g.setColour(juce::Colour(0xff48f0c8).withAlpha(0.4f)); g.drawHorizontalLine(596, 24, 1136);
    text(g, "Wet trim", output.getBounds().withY(output.getY() - 20).withHeight(18), 13.0f, cream, juce::Font::bold, juce::Justification::centred);
    text(g, "Rack mix", masterMix.getBounds().withY(masterMix.getY() - 20).withHeight(18), 13.0f, cream, juce::Font::bold, juce::Justification::centred);
    text(g, "Compression", {738, 623, 126, 22}, 13.0f, cream, juce::Font::bold);
    text(g, juce::String(ownerProcessor.gainReduction.load(std::memory_order_relaxed), 1) + " dB", {738, 646, 126, 28}, 21.0f, juce::Colour(0xffffbe3b), juce::Font::bold);
    g.setColour(juce::Colour(0xff3b474a)); g.drawHorizontalLine(715, 24, 1136);
    for (const auto point : {juce::Point<float>(10, 10), juce::Point<float>(1150, 10), juce::Point<float>(10, 600), juce::Point<float>(1150, 600)}) {
        g.setColour(ink.withAlpha(0.45f)); g.fillEllipse(point.x - 3, point.y - 3, 6, 6);
        g.setColour(juce::Colour(0xffd8b15a)); g.drawLine(point.x - 2, point.y, point.x + 2, point.y, 1);
    }
}
void SausageEditor::resized() {
    content.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designWidth));
    ownerProcessor.editorWidth = getWidth();
    sausagePanel.setBounds(24, 172, 266, 415); spacePanel.setBounds(306, 172, 266, 415);
    risePanel.setBounds(588, 172, 266, 415); widthPanel.setBounds(870, 172, 266, 415);
    output.setBounds(400, 618, 100, 96); masterMix.setBounds(520, 618, 100, 96);
    softClip.setBounds(864, 633, 112, 36); bypass.setBounds(986, 633, 146, 36);
    inputMeter.setBounds(30, 628, 160, 44); outputMeter.setBounds(214, 628, 160, 44);
    algorithms.setBounds(532, 104, 158, 36);
    suite.setBounds(0, 716, 1160, 44);
    suite.presetRow.setBounds(706, 102, 426, 66);
}
