#include "PluginEditor.h"
#include "BinaryData.h"
#include <ranges>

namespace {
const juce::Colour ink(0xffe6f0f5), cream(0xffe6f0f5), panel(0xf2152026);
constexpr int designWidth = 1160, designHeight = 760;
const juce::String masterBypassHint {"Turn off Master bypass to use this."};
void text(juce::Graphics& g, const juce::String& value, juce::Rectangle<int> box, float height,
          juce::Colour color = ink, int style = juce::Font::plain, juce::Justification align = juce::Justification::centredLeft) {
    g.setColour(color);
    g.setFont(juce::FontOptions(height, style));
    g.drawText(value, box, align);
}
// Draws a control at 40% opacity while its setting has no effect, and adds what turns it on to its tooltip.
// off names a bypass that silences the control, and wins over hint.
void showEffect(juce::Component& control, const juce::String& off, bool hasNoEffect, const juce::String& hint) {
    auto reason = hasNoEffect ? hint : juce::String();
    if (off.isNotEmpty()) reason = off;
    auto& tips = dynamic_cast<juce::SettableTooltipClient&>(control);
    auto& properties = control.getProperties();
    if (!properties.contains("tooltip")) properties.set("tooltip", tips.getTooltip());
    auto tooltip = properties["tooltip"].toString();
    if (reason.isNotEmpty()) tooltip << " " << reason;
    const float nextAlpha = reason.isEmpty() ? 1.0f : 0.4f;
    if (control.getAlpha() != nextAlpha) {
        control.setAlpha(nextAlpha);
        if (auto* parent = control.getParentComponent())
            parent->repaint(control.getBounds().withY(control.getY() - 20).withHeight(18));
    }
    if (tips.getTooltip() != tooltip) tips.setTooltip(tooltip);
    for (auto* child : control.getChildren())
        if (auto* childTips = dynamic_cast<juce::SettableTooltipClient*>(child))
            if (childTips->getTooltip() != tooltip)
                childTips->setTooltip(tooltip);
}
void setSliderAccent(juce::Slider& slider, juce::Colour accent, bool bypassed) {
    const auto colour = bypassed ? juce::Colour(0xff6b7e84) : accent;
    slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
    slider.setColour(juce::Slider::thumbColourId, colour);
    slider.setColour(juce::Slider::trackColourId, colour);
}
void dial(juce::Slider& slider, const juce::String& id, juce::Colour accent) {
    slider.setComponentID(id);
    slider.setName(id);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 24);
    setSliderAccent(slider, accent, false);
    slider.setNumDecimalPlacesToDisplay(1);
    slider.setColour(juce::Slider::textBoxTextColourId, ink);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setDoubleClickReturnValue(true, id == "size" || id == "spread" ? 50.0 : 0.0);
    slider.setWantsKeyboardFocus(true);
}
void configureHorizontalSlider(juce::Slider& slider, const juce::String& id, juce::Colour accent, int textBoxWidth, int decimals) {
    dial(slider, id, accent);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, textBoxWidth, 24);
    slider.setNumDecimalPlacesToDisplay(decimals);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0f181d));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff2f424c));
}
void drawPadlock(juce::Graphics& g, juce::Rectangle<float> box) {
    constexpr float w = 10.0f, h = 12.0f;
    const float x = box.getX() + (box.getWidth() - w) * 0.5f;
    const float y = box.getY() + (box.getHeight() - h) * 0.5f;
    g.setColour(juce::Colour(0xffffbe3b));
    juce::Path shackle;
    shackle.addArc(x + 2.0f, y + 0.5f, 6.0f, 6.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    shackle.startNewSubPath(x + 2.0f, y + 3.5f);
    shackle.lineTo(x + 2.0f, y + 5.5f);
    shackle.startNewSubPath(x + 8.0f, y + 3.5f);
    shackle.lineTo(x + 8.0f, y + 5.5f);
    g.strokePath(shackle, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillRoundedRectangle(x + 0.5f, y + 5.0f, 9.0f, 6.5f, 1.5f);
    g.setColour(juce::Colour(0xff152026));
    g.fillEllipse(x + 4.0f, y + 7.2f, 2.0f, 2.2f);
}
void paintLockedIndicators(juce::Graphics& g, const juce::Component& container, const SausageProcessor& processor) {
    for (const auto* child : container.getChildren()) {
        if (!child->isVisible()) continue;
        const auto id = child->getComponentID();
        if (id.isEmpty() || !processor.isParamLocked(id)) continue;
        const auto bounds = child->getBounds().toFloat();
        const auto* slider = dynamic_cast<const juce::Slider*>(child);
        const bool isLinear = slider != nullptr && slider->getSliderStyle() == juce::Slider::LinearHorizontal;
        const bool isCombo = dynamic_cast<const juce::ComboBox*>(child) != nullptr;
        const bool isButton = dynamic_cast<const juce::Button*>(child) != nullptr;
        juce::Rectangle<float> badge(bounds.getRight() - 14.0f, bounds.getY() + 2.0f, 12.0f, 13.0f);
        if (isCombo) badge = juce::Rectangle<float>(bounds.getRight() - 32.0f, bounds.getY() + 2.0f, 12.0f, 13.0f);
        else if (isLinear) badge = juce::Rectangle<float>(bounds.getRight() - 14.0f, bounds.getY() + 2.0f, 12.0f, 13.0f);
        else if (isButton) {
            badge = juce::Rectangle<float>(bounds.getRight() - 13.0f, bounds.getY() + 1.0f, 11.0f, 12.0f);
            g.setColour(juce::Colour(0xe6152026));
            g.fillRoundedRectangle(badge.expanded(2.0f, 1.0f), 4.0f);
        }
        drawPadlock(g, badge);
    }
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
void LevelMeter::setBypassed(bool shouldBeBypassed) {
    if (bypassed == shouldBeBypassed) return;
    bypassed = shouldBeBypassed;
    repaint();
}
void LevelMeter::paint(juce::Graphics& g) {
    const auto heldDb = juce::Decibels::gainToDecibels(held, -60.0f);
    const auto headerColour = bypassed ? juce::Colour(0xff94a5ac) : cream;
    text(g, label, {0, 0, 72, 18}, 13.0f, headerColour, juce::Font::bold);
    auto readout = juce::String("-inf");
    if (heldDb > -60.0f) readout = (heldDb > 0.0f ? "+" : "") + juce::String(heldDb, 1);
    const auto readoutColour = bypassed ? juce::Colour(0xff94a5ac)
                                        : (heldDb > -60.0f ? cream : juce::Colour(0xffbccbd2));
    text(g, readout + " dBFS", {64, 0, 98, 18}, 13.5f, readoutColour, juce::Font::plain, juce::Justification::centredRight);
    // 22 segments span -60 to 0 dBFS. The segment of the held peak stays lit.
    const auto topSegment = [](float gain) {
        return std::min(21, static_cast<int>(std::ceil((juce::Decibels::gainToDecibels(gain, -60.0f) + 60.0f) * 22.0f / 60.0f)) - 1);
    };
    const int levelTop = topSegment(level), heldTop = topSegment(held);
    std::ranges::for_each(std::views::iota(0, 22), [&](int segment) {
        const bool lit = segment <= levelTop || segment == heldTop;
        const juce::uint32 litColour = bypassed
            ? (segment >= 20 ? 0xff9b8b62 : 0xff657579)
            : (segment >= 20 ? 0xffffbe3b : 0xff48f0c8);
        g.setColour(juce::Colour(lit ? litColour : 0xff313f42));
        g.fillRoundedRectangle(static_cast<float>(segment * 7), 22.0f, 5.0f, 16.0f, 1.0f);
    });
    const juce::uint32 clipColour = bypassed ? (clipped ? 0xffa85252 : 0xff313f42)
                                             : (clipped ? 0xffff4d4d : 0xff313f42);
    g.setColour(juce::Colour(clipColour));
    g.fillRoundedRectangle(154.0f, 22.0f, 5.0f, 16.0f, 1.0f);
    if (bypassed) {
        const juce::Rectangle<float> badge(41.0f, 21.0f, 78.0f, 18.0f);
        g.setColour(juce::Colour(0xf50d1519));
        g.fillRoundedRectangle(badge, 4.0f);
        g.setColour(juce::Colour(0xff8f7a4a));
        g.drawRoundedRectangle(badge, 4.0f, 1.0f);
        text(g, "BYPASSED", badge.toNearestInt(), 10.5f, juce::Colour(0xffe8c266), juce::Font::bold, juce::Justification::centred);
    }
    constexpr std::array<std::pair<int, const char*>, 3> ticks {{{92, "-24"}, {123, "-12"}, {151, "0"}}};
    for (const auto& [tickX, tickLabel] : ticks) {
        g.setColour(juce::Colour(0xff738790));
        g.drawVerticalLine(tickX, 40.0f, 43.5f);
        text(g, tickLabel, {tickX - 14, 44, 28, 15}, 11.5f, juce::Colour(0xffb4c7d0), juce::Font::plain, juce::Justification::centred);
    }
}
void LevelMeter::mouseDown(const juce::MouseEvent&) {
    clipped = false;
    repaint();
}
ModulePanel::ModulePanel(SausageProcessor& proc, const juce::String& name, const juce::String& sub,
                         const juce::String& primaryId, const juce::String& primaryName, const juce::String& secondaryId,
                         const juce::String& secondaryName, const juce::String& bypassId, juce::Colour color, const juce::String& description)
    : processor(proc), title(name), subtitle(sub), primaryLabel(primaryName), secondaryLabel(secondaryName), hint(description), accent(color),
      amount(proc.parameters.getRawParameterValue(primaryId)), detail(proc.parameters.getRawParameterValue(secondaryId)),
      satMix(proc.parameters.getRawParameterValue("satMix")),
      moduleBypass(proc.parameters.getRawParameterValue(bypassId)), masterBypass(proc.parameters.getRawParameterValue("bypass")), algorithm(proc.parameters.getRawParameterValue("algorithm")),
      primaryAttachment(proc.parameters, primaryId, primary), secondaryAttachment(proc.parameters, secondaryId, secondary), bypassAttachment(proc.parameters, bypassId, bypass) {
    auto& state = processor.parameters;
    kind = primaryId == "space" ? 1 : primaryId == "rise" ? 2 : primaryId == "width" ? 3 : 0;
    dial(primary, primaryId, color); dial(secondary, secondaryId, color);
    primary.setTitle(primaryName);
    secondary.setTitle(secondaryName);
    primary.setTooltip(primaryName + ": " + description + " Drag to change; double-click to reset; click the value to type.");
    secondary.setTooltip(secondaryName + ": fine-tune this effect. Double-click to reset.");
    bypass.setComponentID(bypassId);
    bypass.setName(name + " bypass");
    bypass.setTitle(name + " bypass");
    bypass.setClickingTogglesState(true);
    bypass.setTooltip("Fully bypass " + name + ", including its wet signal and tails. Other modules keep running.");
    addAndMakeVisible(primary); addAndMakeVisible(secondary); addAndMakeVisible(bypass);
    if (kind == 0) {
        configureHorizontalSlider(saturationMix, "satMix", color, 46, 0);
        configureHorizontalSlider(dynamics, "satDynamics", color, 46, 0);
        saturationMix.setDoubleClickReturnValue(true, 100.0);
        dynamics.setDoubleClickReturnValue(true, 100.0);
        saturationMix.setTitle("Saturation Mix");
        dynamics.setTitle("Dynamics");
        saturationMix.setTooltip("Blend the saturated signal with the clean signal. 0% is clean; 100% keeps the full existing Fatness/Color blend.");
        dynamics.setTooltip("Amount of the Fatness envelope compression. 0% bypasses compression; 100% uses the full compressor response.");
        focus.setComponentID("satFocus");
        focus.setTitle("Saturation Focus");
        focus.addItemList({"Neutral", "Keep Low", "Keep High"}, 1);
        focus.setTooltip("Neutral saturates the full signal. Keep Low leaves bass mostly clean and saturates highs; Keep High saturates bass and keeps highs cleaner.");
        saturationMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "satMix", saturationMix);
        dynamicsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "satDynamics", dynamics);
        focusAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, "satFocus", focus);
        addAndMakeVisible(saturationMix); addAndMakeVisible(dynamics); addAndMakeVisible(focus);
    } else if (kind == 1) {
        configureHorizontalSlider(preDelay, "preDelay", color, 48, 0);
        preDelay.setDoubleClickReturnValue(true, 0.0);
        preDelay.setTitle("Pre-Delay");
        preDelay.setTooltip("Space pre-delay (0 to 100 ms) so transients punch through before the reverb tail.");
        preDelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, "preDelay", preDelay);
        addAndMakeVisible(preDelay);
    } else if (kind == 2) {
        riseLength.setComponentID("riseLength");
        riseLength.setName("Rise length");
        riseLength.setTitle("Rise length");
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
    const bool isBypassed = moduleBypass->load() > 0.5f || masterBypass->load() > 0.5f;
    g.setColour(isBypassed ? juce::Colour(0xff3b4649) : accent.withAlpha(0.4f)); g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    g.setColour(cream.withAlpha(0.06f)); g.drawHorizontalLine(2, 10.0f, bounds.getRight() - 10.0f);
    g.setColour(isBypassed ? juce::Colour(0xff546164) : accent); g.fillRoundedRectangle(16.0f, 16.0f, 4.0f, 34.0f, 2.0f);
    text(g, title, {28, 16, 120, 36}, 29.0f, ink, juce::Font::bold);
    constexpr const char* descriptions[4][4] {
        {"SATURATION + COMPRESSION", "STEREO REVERB", "PITCH + BUILDUP", "STEREO WIDENER"},
        {"SOFT SATURATION", "DARK HALL", "SMOOTH RISE", "DIFFUSE WIDTH"},
        {"HARD-CLIP SATURATION", "TIGHT ROOM", "CLEAN RISE", "BASS-SAFE WIDTH"},
        {"WAVEFOLD SATURATION", "BRIGHT HALL", "FEEDBACK RISE", "MOVING WIDTH"}
    };
    const auto mode = juce::jlimit(0, 3, static_cast<int>(algorithm->load()));
    text(g, mode == 0 ? subtitle : descriptions[mode][kind], {20, 57, 210, 22}, 12.5f, juce::Colour(0xffc2d2d6));
    const bool isModuleActive = isActive();
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
        text(g, label, control.getBounds().withY(control.getY() - 20).withHeight(19), 13.0f, juce::Colour(0xfff0f7fa).withAlpha(control.getAlpha()), juce::Font::bold, juce::Justification::centred);
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
    g.setFont(juce::FontOptions(13.0f)); g.setColour(juce::Colour(0xffc2d0d4));
    if (kind == 0) g.drawFittedText(hint, {128, 290, 126, 52}, juce::Justification::topLeft, 3, 0.95f);
    if (kind == 3) g.drawFittedText(hint, {128, 332, 126, 44}, juce::Justification::topLeft, 3, 0.95f);
    const auto value = isModuleActive ? (kind == 0 ? std::max(amount->load(), detail->load()) / 100.0f * (satMix->load() / 100.0f) : amount->load() / 100.0f) : 0.0f;
    const auto secondaryValue = detail->load() / 100.0f;
    const auto motion = phase * value;
    const juce::Rectangle<float> iconTray(126.0f, 383.0f, 128.0f, 28.0f);
    g.setColour(juce::Colour(0x550b1114));
    g.fillRoundedRectangle(iconTray, 5.0f);
    g.setColour(juce::Colour(0x22e6f0f5));
    g.drawRoundedRectangle(iconTray, 5.0f, 1.0f);
    g.setColour(isModuleActive ? accent.darker(0.15f) : juce::Colour(0xff546164));
    const float iconMidY = iconTray.getCentreY();
    if (kind == 0) {
        juce::Path waveform;
        for (int x = 0; x <= 104; ++x) {
            const auto wave = std::tanh(std::sin(static_cast<float>(x) * 0.14f + motion) * (1.0f + value * 8.0f));
            const auto y = iconMidY - wave * (3.0f + value * 6.0f);
            if (x == 0) waveform.startNewSubPath(138.0f, y); else waveform.lineTo(138.0f + static_cast<float>(x), y);
        }
        g.strokePath(waveform, juce::PathStrokeType(1.7f));
    } else if (kind == 1) {
        for (int ring = 0; ring < 4; ++ring) {
            const auto radius = 4.0f + std::fmod(static_cast<float>(ring) * 10.0f + motion * 6.0f, 40.0f) * (0.45f + value * 0.55f);
            g.drawEllipse(iconTray.getCentreX() - radius, iconMidY - radius * 0.28f, radius * 2.0f, radius * 0.56f, 1.5f);
        }
    } else if (kind == 2) {
        for (int arrow = 0; arrow < 4; ++arrow) {
            const auto x = 144.0f + static_cast<float>(arrow) * 30.0f;
            const auto y = (iconMidY + 5.0f) - std::fmod(static_cast<float>(arrow) * 3.5f + motion * 7.0f, 14.0f);
            juce::Path chevron; chevron.startNewSubPath(x - 6.5f, y + 4.5f); chevron.lineTo(x, y - 2.5f); chevron.lineTo(x + 6.5f, y + 4.5f);
            g.strokePath(chevron, juce::PathStrokeType(1.9f));
        }
    } else {
        const auto distance = 4.0f + value * 28.0f;
        const auto wave = std::sin(motion) * value * 2.5f;
        const auto ellipseWidth = 18.0f + secondaryValue * 10.0f;
        g.drawEllipse(iconTray.getCentreX() - distance - ellipseWidth * 0.5f, iconMidY - 10.0f + wave, ellipseWidth, 20.0f, 1.6f);
        g.drawEllipse(iconTray.getCentreX() + distance - ellipseWidth * 0.5f, iconMidY - 10.0f - wave, ellipseWidth, 20.0f, 1.6f);
        g.drawLine(136.0f, iconMidY, 244.0f, iconMidY, 0.9f);
    }
}
void ModulePanel::paintOverChildren(juce::Graphics& g) {
    paintLockedIndicators(g, *this, processor);
}
bool ModulePanel::isActive() const noexcept {
    if (moduleBypass->load() > 0.5f || masterBypass->load() > 0.5f) return false;
    if (kind == 0) return (amount->load() > 0.0f || detail->load() > 0.0f) && satMix->load() > 0.0f;
    if (kind == 2) return amount->load() > 0.0f && detail->load() > 0.0f;
    return amount->load() > 0.0f;
}
void ModulePanel::setBeatPhase(float position) {
    const bool active = isActive();
    const auto value = active ? (kind == 0 ? std::max(amount->load(), detail->load()) * (satMix->load() / 100.0f) : amount->load()) : 0.0f;
    const std::array<float, 2> header {algorithm->load(), active ? 1.0f : 0.0f};
    const std::array<float, 3> display {value, detail->load(), position * value};
    if (header != shownHeader) repaint(20, 57, 232, 22);
    if (display != shownDisplay) repaint(126, 382, 128, 30);
    shownHeader = header;
    shownDisplay = display;
    phase = position;
}
void ModulePanel::dimInactive() {
    juce::String off;
    if (moduleBypass->load() > 0.5f) off = "Turn on " + title + " to use this.";
    if (masterBypass->load() > 0.5f) off = masterBypassHint;
    const bool bypassed = off.isNotEmpty();
    setSliderAccent(primary, accent, bypassed);
    setSliderAccent(secondary, accent, bypassed);
    const bool amountIsZero = amount->load() <= 0.0f, detailIsZero = detail->load() <= 0.0f;
    const auto raise = [](const juce::String& knob) { return "Raise " + knob + " to hear this."; };
    showEffect(primary, off, kind == 2 && detailIsZero, raise(secondaryLabel));
    showEffect(secondary, off, (kind == 1 || kind == 3) && amountIsZero, raise(primaryLabel));
    showEffect(bypass, masterBypass->load() > 0.5f ? masterBypassHint : juce::String(), false, {});
    if (kind == 0) {
        setSliderAccent(saturationMix, accent, bypassed);
        setSliderAccent(dynamics, accent, bypassed);
        showEffect(saturationMix, off, amountIsZero && detailIsZero, "Raise Fatness or Color to hear this.");
        showEffect(dynamics, off, amountIsZero, raise(primaryLabel));
        showEffect(focus, off, amountIsZero, raise(primaryLabel));
    }
    if (kind == 1) {
        setSliderAccent(preDelay, accent, bypassed);
        showEffect(preDelay, off, amountIsZero, raise(primaryLabel));
    }
    if (kind == 2) showEffect(riseLength, off, detailIsZero, raise(secondaryLabel));
}
void ModulePanel::resized() {
    bypass.setBounds(154, 20, 94, 36);
    if (kind == 0) {
        primary.setBounds(4, 112, 124, 160);
        secondary.setBounds(8, 302, 114, 108);
        saturationMix.setBounds(128, 122, 128, 36);
        dynamics.setBounds(128, 182, 128, 36);
        focus.setBounds(128, 244, 128, 36);
    } else {
        primary.setBounds(44, 112, 178, 166);
        secondary.setBounds(10, 302, 114, 108);
        if (kind == 1) preDelay.setBounds(128, 344, 128, 36);
        if (kind == 2) riseLength.setBounds(128, 344, 128, 36);
    }
}
SausageEditor::SausageEditor(SausageProcessor& owner)
    : AudioProcessorEditor(owner), ownerProcessor(owner),
      enamel(juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize)),
      sausagePanel(owner, "Sausage", "SATURATION + COMPRESSION", "fatness", "Fatness", "color", "Color", "sausageBypass", juce::Colour(0xff48f0c8), "4x saturation and compression."),
      spacePanel(owner, "Space", "STEREO REVERB", "space", "Mix", "size", "Size", "spaceBypass", juce::Colour(0xff48f0c8), "Reverb mix. Size sets decay."),
      risePanel(owner, "Rise", "PITCH + BUILDUP", "rise", "Rise", "riseMix", "Mix", "riseBypass", juce::Colour(0xff48f0c8), "Pitch shift, up to an octave."),
      widthPanel(owner, "Width", "STEREO WIDENER", "width", "Width", "spread", "Spread", "widthBypass", juce::Colour(0xff48f0c8), "Stereo width and spacing."),
      outputAttachment(owner.parameters, "output", output), masterMixAttachment(owner.parameters, "masterMix", masterMix),
      bypassAttachment(owner.parameters, "bypass", bypass), softClipAttachment(owner.parameters, "softClip", softClip),
      suite(owner) {
    setLookAndFeel(&look);
    grill = juce::Drawable::createFromImageData(BinaryData::grill_svg, BinaryData::grill_svgSize);
    addAndMakeVisible(content);
    content.setBounds(0, 0, designWidth, designHeight);
    for (auto* p : {&sausagePanel, &spacePanel, &risePanel, &widthPanel}) content.addAndMakeVisible(p);
    dial(output, "output", juce::Colour(0xffffbe3b));
    output.setTitle("Wet trim");
    output.setTooltip("Wet trim: level of the processed signal before Soft Clip and Rack mix. It has no effect at Rack mix 0%. Master bypass skips it.");
    output.setColour(juce::Slider::textBoxTextColourId, cream);
    inputMeter.setComponentID("inputMeter");
    outputMeter.setComponentID("outputMeter");
    inputMeter.setTooltip("Input peak in dBFS, held for 1 s. The red light latches above 0 dBFS; click to clear it.");
    outputMeter.setTooltip("Output peak in dBFS, held for 1 s. The red light latches above 0 dBFS; click to clear it.");
    dial(masterMix, "masterMix", juce::Colour(0xff48f0c8));
    masterMix.setTitle("Rack mix");
    masterMix.setDoubleClickReturnValue(true, 100.0);
    masterMix.setTooltip("Rack mix: blend of the dry input and the processed rack. 0% is dry, 100% is fully processed.");
    masterMix.setColour(juce::Slider::textBoxTextColourId, cream);
    softClip.setComponentID("softClip"); softClip.setName("Soft clip"); softClip.setTitle("Soft clip"); softClip.setClickingTogglesState(true);
    softClip.setTooltip("Anti-aliased post-rack soft clipper ceiling so aggressive builds never exceed 0 dBFS.");
    bypass.setComponentID("bypass"); bypass.setName("Master bypass"); bypass.setTitle("Master bypass"); bypass.setClickingTogglesState(true);
    bypass.setTooltip("Bypass the complete chain. Latency stays at " + juce::String(owner.getLatencySamples()) + " samples.");
    algorithms.setComponentID("algorithm"); algorithms.setName("Global algorithm"); algorithms.setTitle("Algorithm");
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
    content.addMouseListener(&lockListener, true);
    lastLockMask = lockMask();
    startTimerHz(25);
}
SausageEditor::~SausageEditor() {
    stopTimer();
    content.removeMouseListener(&lockListener);
    setLookAndFeel(nullptr);
}
float SausageEditor::animationPhase() const noexcept {
    const float phase = ownerProcessor.beatPhase.load(std::memory_order_relaxed);
    const double idle = std::fmod(juce::Time::getMillisecondCounterHiRes(), 500.0) / 500.0;
    return (phase >= 0.0f ? phase : static_cast<float>(idle)) * juce::MathConstants<float>::twoPi;
}
bool SausageEditor::keyPressed(const juce::KeyPress& key) {
    if (!key.getModifiers().isCommandDown()) return false;
    const auto text = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    const auto code = key.getKeyCode();
    const bool isZ = text == 'z' || code == 'z' || code == 'Z';
    const bool isY = text == 'y' || code == 'y' || code == 'Y';
    if (!isZ && !isY) return false;
    if (isY || key.getModifiers().isShiftDown()) ownerProcessor.history.redo();
    else ownerProcessor.history.undo();
    suite.refresh();
    return true;
}
bool SausageEditor::isCompressionDimmed() const noexcept {
    return ownerProcessor.parameters.getRawParameterValue("bypass")->load() > 0.5f
        || ownerProcessor.parameters.getRawParameterValue("sausageBypass")->load() > 0.5f
        || ownerProcessor.parameters.getRawParameterValue("fatness")->load() <= 0.0f
        || ownerProcessor.parameters.getRawParameterValue("satDynamics")->load() <= 0.0f
        || ownerProcessor.parameters.getRawParameterValue("masterMix")->load() <= 0.0f;
}
bool SausageEditor::isRackBypassed() const noexcept {
    if (ownerProcessor.parameters.getRawParameterValue("bypass")->load() > 0.5f) return true;
    return ownerProcessor.parameters.getRawParameterValue("sausageBypass")->load() > 0.5f
        && ownerProcessor.parameters.getRawParameterValue("spaceBypass")->load() > 0.5f
        && ownerProcessor.parameters.getRawParameterValue("riseBypass")->load() > 0.5f
        && ownerProcessor.parameters.getRawParameterValue("widthBypass")->load() > 0.5f;
}
void SausageEditor::updateDimming() {
    for (auto* p : {&sausagePanel, &spacePanel, &risePanel, &widthPanel})
        p->dimInactive();
    const bool rackBypassed = isRackBypassed();
    inputMeter.setBypassed(rackBypassed);
    outputMeter.setBypassed(rackBypassed);
    const auto off = ownerProcessor.parameters.getRawParameterValue("bypass")->load() > 0.5f ? masterBypassHint : juce::String();
    const bool masterBypassed = off.isNotEmpty();
    setSliderAccent(output, juce::Colour(0xffffbe3b), masterBypassed);
    setSliderAccent(masterMix, juce::Colour(0xff48f0c8), masterBypassed);
    const bool dry = ownerProcessor.parameters.getRawParameterValue("masterMix")->load() <= 0.0f;
    showEffect(output, off, dry, "Raise Rack mix to hear this.");
    showEffect(softClip, off, dry, "Raise Rack mix to hear this.");
    showEffect(masterMix, off, false, {});
    showEffect(algorithms, off, false, {});
}
void SausageEditor::timerCallback() {
    suite.refresh();
    const float phase = animationPhase();
    for (auto* p : {&sausagePanel, &spacePanel, &risePanel, &widthPanel})
        p->setBeatPhase(phase);
    updateDimming();
    const bool sausageBypassed = ownerProcessor.parameters.getRawParameterValue("sausageBypass")->load() > 0.5f
                             || ownerProcessor.parameters.getRawParameterValue("bypass")->load() > 0.5f;
    const float fat = sausageBypassed ? 0.0f : ownerProcessor.parameters.getRawParameterValue("fatness")->load() / 100.0f;
    if (const std::pair<float, float> grillState {phase, fat}; grillState != lastGrillState) {
        lastGrillState = grillState;
        content.repaint(20, 4, 175, 160);
    }
    const auto now = juce::Time::getMillisecondCounterHiRes();
    inputMeter.push(ownerProcessor.inputPeak.exchange(0.0f), now);
    outputMeter.push(ownerProcessor.outputPeak.exchange(0.0f), now);
    const int gainReductionTenths = static_cast<int>(std::lround(ownerProcessor.gainReduction.load(std::memory_order_relaxed) * 10.0f));
    const bool compressionDimmed = isCompressionDimmed();
    if (const std::pair<int, bool> compressionState {gainReductionTenths, compressionDimmed}; compressionState != lastCompressionState) {
        lastCompressionState = compressionState;
        content.repaint(738, 623, 126, 51);
    }
    if (const auto mask = lockMask(); mask != lastLockMask) {
        lastLockMask = mask;
        content.repaint();
    }
}
void SausageEditor::paintContentOverChildren(juce::Graphics& g) {
    paintLockedIndicators(g, content, ownerProcessor);
}
uint32_t SausageEditor::lockMask() const noexcept {
    uint32_t mask = 0;
    uint32_t bit = 1;
    for (auto* parameter : ownerProcessor.getParameters()) {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
            if (ownerProcessor.isParamLocked(ranged->paramID)) mask |= bit;
            bit <<= 1;
        }
    }
    return mask;
}
void SausageEditor::ParamLockListener::mouseDown(const juce::MouseEvent& event) {
    if (!event.mods.isPopupMenu()) return;
    juce::Component* target = event.eventComponent;
    juce::RangedAudioParameter* param = nullptr;
    while (target != nullptr && target != &editor.content) {
        if (target->getComponentID().isNotEmpty()) {
            if (auto* found = editor.ownerProcessor.parameters.getParameter(target->getComponentID())) {
                param = found;
                break;
            }
        }
        target = target->getParentComponent();
    }
    if (param == nullptr || target == nullptr) return;
    const auto paramId = target->getComponentID();
    const bool locked = editor.ownerProcessor.isParamLocked(paramId);
    const auto name = param->getName(64).isNotEmpty() ? param->getName(64) : target->getTitle();
    juce::PopupMenu menu;
    menu.addItem(1, (locked ? "Unlock " : "Lock ") + name);
    if (editor.ownerProcessor.hasAnyLockedParam()) {
        menu.addSeparator();
        menu.addItem(2, "Unlock all parameters");
    }
    juce::Component::SafePointer<SausageEditor> safeEditor(&editor);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(target),
                       [safeEditor, paramId, locked](int result) {
        if (safeEditor == nullptr || result == 0) return;
        if (result == 1) safeEditor->ownerProcessor.setParamLocked(paramId, !locked);
        else if (result == 2) safeEditor->ownerProcessor.unlockAllParams();
        safeEditor->lastLockMask = safeEditor->lockMask();
        safeEditor->content.repaint();
    });
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
    text(g, "v" JucePlugin_VersionString, {190, 121, 150, 20}, 14.0f, juce::Colour(0xffc6d6de));
    text(g, "Algorithm", {532, 79, 145, 21}, 15.0f, juce::Colour(0xfff0f6fa), juce::Font::bold);
    text(g, "Preset", {706, 79, 412, 21}, 15.0f, juce::Colour(0xfff0f6fa), juce::Font::bold);
    text(g, "ALAN MARCERO", {930, 31, 196, 24}, 15.0f, ink, juce::Font::bold, juce::Justification::centredRight);
    g.setColour(juce::Colour(0xf00c1418)); g.fillRect(0, 592, designWidth, designHeight - 592);
    g.setColour(juce::Colour(0xff48f0c8).withAlpha(0.4f)); g.drawHorizontalLine(592, 24, 1136);
    const float gr = ownerProcessor.gainReduction.load(std::memory_order_relaxed);
    const bool compressionDimmed = isCompressionDimmed();
    const bool grActive = !compressionDimmed && gr > 0.05f;
    const auto grColour = (grActive ? juce::Colour(0xffffbe3b) : juce::Colour(0xff7e929c)).withAlpha(compressionDimmed ? 0.4f : 1.0f);
    text(g, "WET TRIM", output.getBounds().withY(output.getY() - 18).withHeight(18), 13.0f, juce::Colour(0xfff0f7fa).withAlpha(output.getAlpha()), juce::Font::bold, juce::Justification::centred);
    text(g, "RACK MIX", masterMix.getBounds().withY(masterMix.getY() - 18).withHeight(18), 13.0f, juce::Colour(0xfff0f7fa).withAlpha(masterMix.getAlpha()), juce::Font::bold, juce::Justification::centred);
    text(g, "COMPRESSION", {738, 623, 126, 22}, 13.0f, juce::Colour(0xfff0f7fa).withAlpha(compressionDimmed ? 0.4f : 1.0f), juce::Font::bold);
    text(g, juce::String(gr, 1) + " dB", {738, 646, 126, 28}, 21.0f, grColour, juce::Font::bold);
    g.setColour(juce::Colour(0xff3b474a)); g.drawHorizontalLine(711, 24, 1136);
    for (const auto point : {juce::Point<float>(10, 10), juce::Point<float>(1150, 10), juce::Point<float>(10, 598), juce::Point<float>(1150, 598)}) {
        g.setColour(ink.withAlpha(0.45f)); g.fillEllipse(point.x - 3, point.y - 3, 6, 6);
        g.setColour(juce::Colour(0xffd8b15a)); g.drawLine(point.x - 2, point.y, point.x + 2, point.y, 1);
    }
}
void SausageEditor::resized() {
    content.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designWidth));
    ownerProcessor.editorWidth = getWidth();
    sausagePanel.setBounds(24, 172, 266, 415); spacePanel.setBounds(306, 172, 266, 415);
    risePanel.setBounds(588, 172, 266, 415); widthPanel.setBounds(870, 172, 266, 415);
    output.setBounds(394, 611, 114, 100); masterMix.setBounds(528, 611, 114, 100);
    softClip.setBounds(864, 633, 112, 36); bypass.setBounds(986, 633, 146, 36);
    inputMeter.setBounds(30, 620, 162, 62); outputMeter.setBounds(214, 620, 162, 62);
    algorithms.setBounds(532, 104, 158, 36);
    suite.setBounds(0, 712, 1160, 48);
    suite.presetRow.setBounds(706, 102, 426, 66);
}
