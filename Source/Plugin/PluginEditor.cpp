#include "PluginEditor.h"
#include "BinaryData.h"

namespace {
const juce::Colour ink(0xffeee7d9), cream(0xffeee7d9), panel(0xf21d2528);
juce::Image trimAlpha(const juce::Image& image) {
    int left = image.getWidth(), top = image.getHeight(), right = 0, bottom = 0;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            if (image.getPixelAt(x, y).getAlpha() > 32) {
                left = std::min(left, x); top = std::min(top, y);
                right = std::max(right, x); bottom = std::max(bottom, y);
            }
    return image.getClippedImage({left, top, right - left + 1, bottom - top + 1});
}
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
        const auto color = segment > 20 ? juce::Colour(0xffef7353) : juce::Colour(0xff83d4c3);
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
    text(g, mode == 0 ? subtitle : descriptions[mode][kind], {20, 57, 226, 22}, 12.0f, juce::Colour(0xffa3afaf));
    g.setColour(juce::Colour(0xff3b4649)); g.drawHorizontalLine(88, 20.0f, 246.0f);
    text(g, primaryLabel.toUpperCase(), {20, 101, 226, 20}, 12.0f, ink, juce::Font::bold, juce::Justification::centred);
    text(g, secondaryLabel.toUpperCase(), {132, 323, 114, 24}, 12.0f, ink, juce::Font::bold);
    g.setFont(juce::FontOptions(12.0f)); g.setColour(juce::Colour(0xffa3afaf));
    g.drawFittedText(hint, {132, 350, 112, 30}, juce::Justification::topLeft, 2);
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
    bypass.setBounds(154, 20, 94, 30);
    primary.setBounds(44, 126, 178, 178);
    secondary.setBounds(20, 314, 100, 96);
}
SausageEditor::SausageEditor(SausageProcessor& owner)
    : AudioProcessorEditor(owner), ownerProcessor(owner),
      enamel(juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize)),
      sausagePanel(owner.parameters, "Sausage", "SATURATION + COMPRESSION", "fatness", "Fatness", "color", "Color", "sausageBypass", juce::Colour(0xffdaab7b), "Warm weight to full-on crunch."),
      spacePanel(owner.parameters, "Space", "STEREO REVERB", "space", "Mix", "size", "Size", "spaceBypass", juce::Colour(0xff87c8b8), "Small rooms. Big atmospheres."),
      risePanel(owner.parameters, "Rise", "PITCH + BUILDUP", "rise", "Rise", "riseMix", "Mix", "riseBypass", juce::Colour(0xffb1a4cf), "Lift the pitch. Build the tension."),
      widthPanel(owner.parameters, "Width", "STEREO WIDENER", "width", "Width", "spread", "Spread", "widthBypass", juce::Colour(0xff89becf), "Give your sound some elbow room."),
      outputAttachment(owner.parameters, "output", output), bypassAttachment(owner.parameters, "bypass", bypass) {
    setLookAndFeel(&look);
    const auto sheet = juce::ImageCache::getFromMemory(BinaryData::sausageexpressions_png, BinaryData::sausageexpressions_pngSize);
    for (int frame = 0; frame < 3; ++frame)
        faces[static_cast<std::size_t>(frame)] = trimAlpha(sheet.getClippedImage({frame * sheet.getWidth() / 3, 0, sheet.getWidth() / 3, sheet.getHeight()}));
    for (auto* panel : {&sausagePanel, &spacePanel, &risePanel, &widthPanel}) addAndMakeVisible(panel);
    dial(output, "output", juce::Colour(0xffeac35d));
    output.setTextValueSuffix(" dB"); output.setTooltip("Final output trim, after all four effects. Master bypass ignores trim.");
    output.setColour(juce::Slider::textBoxTextColourId, cream);
    bypass.setComponentID("bypass"); bypass.setName("Master bypass"); bypass.setClickingTogglesState(true);
    bypass.setTooltip("Bypass the complete chain, preserving the reported 16-sample latency.");
    recipes.setComponentID("recipes"); recipes.setTextWhenNothingSelected("CHOOSE A RECIPE");
    recipes.addItemList({"01 / Clean slate", "02 / Gentle heat", "03 / Wide drums", "04 / In orbit", "05 / Lift off"}, 1);
    recipes.onChange = [this] {
        const auto selected = recipes.getSelectedId();
        if (selected > 0) { ownerProcessor.applyRecipe(selected - 1); recipes.setSelectedId(0, juce::dontSendNotification); }
    };
    algorithms.setComponentID("algorithm"); algorithms.setName("Global algorithm");
    for (int i = 0; i < 4; ++i) algorithms.addItem(sausage::algorithmNames[i], i + 1);
    algorithms.setTooltip("One matched algorithm set for all four effects. Classic: original. Velvet: soft and spacious. Punch: tight and aggressive. Prism: bright and animated. Keeps your knob settings.");
    algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(owner.parameters, "algorithm", algorithms);
    addAndMakeVisible(output); addAndMakeVisible(bypass); addAndMakeVisible(recipes); addAndMakeVisible(algorithms);
    setResizable(true, true);
    setResizeLimits(870, 540, 1740, 1080);
    getConstrainer()->setFixedAspectRatio(1160.0 / 720.0);
    setSize(1160, 720);
    startTimerHz(25);
}
SausageEditor::~SausageEditor() { stopTimer(); setLookAndFeel(nullptr); }
void SausageEditor::timerCallback() {
    animationPhase += 0.13f;
    for (auto* panel : {&sausagePanel, &spacePanel, &risePanel, &widthPanel}) panel->advanceAnimation();
    repaint();
}
void SausageEditor::paint(juce::Graphics& g) {
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / 1160.0f));
    g.fillAll(juce::Colour(0xff101719));
    g.drawImage(enamel, juce::Rectangle<float>(0, 0, 1160, 720), juce::RectanglePlacement::stretchToFit);
    g.setColour(juce::Colour(0xff101719).withAlpha(0.24f)); g.fillRect(0, 0, 1160, 609);
    const bool sausageBypassed = ownerProcessor.parameters.getRawParameterValue("sausageBypass")->load() > 0.5f
                             || ownerProcessor.parameters.getRawParameterValue("bypass")->load() > 0.5f;
    const auto fat = sausageBypassed ? 0.0f : ownerProcessor.parameters.getRawParameterValue("fatness")->load() / 100.0f;
    const auto frame = fat < 0.4f ? 0 : fat < 0.75f ? 1 : 2;
    const auto bodyWidth = 94.0f + fat * 26.0f;
    const auto bounce = std::sin(animationPhase) * fat * 3.0f;
    g.setOpacity(1.0f);
    g.drawImage(faces[static_cast<std::size_t>(frame)], juce::Rectangle<float>(99.0f - bodyWidth * 0.5f, 9.0f + bounce, bodyWidth, 145.0f + fat * 4.0f), juce::RectanglePlacement::stretchToFit);
    text(g, "openSausage", {186, 23, 650, 66}, 60.0f, ink, juce::Font::bold);
    text(g, "Saturation / Space / Pitch / Stereo", {190, 94, 520, 25}, 14.0f, ink, juce::Font::bold);
    text(g, "Saturate. Make space. Lift it up. Spread it out.", {190, 121, 520, 20}, 13.0f, juce::Colour(0xffa9b6b7));
    text(g, "GLOBAL ALGORITHM", {734, 81, 180, 20}, 11.0f, ink, juce::Font::bold);
    text(g, "RECIPE", {930, 81, 196, 20}, 11.0f, ink, juce::Font::bold);
    text(g, "ALAN MARCERO", {930, 31, 196, 24}, 15.0f, ink, juce::Font::bold, juce::Justification::centredRight);
    text(g, "AUDIO TOOLS / No. 04", {930, 57, 196, 20}, 11.0f, juce::Colour(0xffa9b6b7), juce::Font::plain, juce::Justification::centredRight);
    g.setColour(juce::Colour(0xf00c1418)); g.fillRect(0, 609, 1160, 111);
    g.setColour(juce::Colour(0xffdaab7b).withAlpha(0.4f)); g.drawHorizontalLine(609, 24, 1136);
    meter(g, "INPUT", ownerProcessor.inputPeak.load(std::memory_order_relaxed), 30);
    meter(g, "OUTPUT", ownerProcessor.outputPeak.load(std::memory_order_relaxed), 224);
    text(g, "OUTPUT TRIM", {425, 638, 120, 22}, 12.0f, cream, juce::Font::bold);
    text(g, "COMPRESSION", {687, 623, 174, 22}, 11.0f, cream, juce::Font::bold);
    text(g, juce::String(ownerProcessor.gainReduction.load(std::memory_order_relaxed), 1) + " dB", {687, 646, 174, 28}, 22.0f, juce::Colour(0xffe9c65d), juce::Font::bold);
    g.setColour(juce::Colour(0xff3b474a)); g.drawHorizontalLine(687, 24, 1136);
    text(g, "SIGNAL FLOW   SAUSAGE  >  SPACE  >  RISE  >  WIDTH  >  OUTPUT", {30, 693, 760, 20}, 10.0f, juce::Colour(0xffa0adb0));
    text(g, "4x SATURATION  /  v1.2.0", {880, 693, 246, 20}, 10.0f, juce::Colour(0xffa0adb0), juce::Font::plain, juce::Justification::centredRight);
    for (const auto point : {juce::Point<float>(10, 10), juce::Point<float>(1150, 10), juce::Point<float>(10, 600), juce::Point<float>(1150, 600)}) {
        g.setColour(ink.withAlpha(0.45f)); g.fillEllipse(point.x - 3, point.y - 3, 6, 6);
        g.setColour(juce::Colour(0xffd8b15a)); g.drawLine(point.x - 2, point.y, point.x + 2, point.y, 1);
    }
}
void SausageEditor::resized() {
    sausagePanel.setBounds(24, 172, 266, 415); spacePanel.setBounds(306, 172, 266, 415);
    risePanel.setBounds(588, 172, 266, 415); widthPanel.setBounds(870, 172, 266, 415);
    output.setBounds(548, 611, 108, 74); bypass.setBounds(934, 633, 192, 36); recipes.setBounds(930, 104, 196, 34);
    algorithms.setBounds(734, 104, 180, 34);
    const auto transform = juce::AffineTransform::scale(static_cast<float>(getWidth()) / 1160.0f);
    for (juce::Component* child : std::initializer_list<juce::Component*> {&sausagePanel, &spacePanel, &risePanel, &widthPanel, &output, &bypass, &recipes, &algorithms})
        child->setTransform(transform);
}
