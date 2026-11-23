#include "SausageLookAndFeel.h"

namespace {
// Draws a disabled control at 40% opacity.
struct DimWhenDisabled {
    DimWhenDisabled(juce::Graphics& g, const juce::Component& control) : graphics(control.isEnabled() ? nullptr : &g) {
        if (graphics != nullptr) graphics->beginTransparencyLayer(0.4f);
    }
    ~DimWhenDisabled() {
        if (graphics != nullptr) graphics->endTransparencyLayer();
    }
    juce::Graphics* graphics;
};
void drawFocusRing(juce::Graphics& g, const juce::Component& control) {
    if (!control.hasKeyboardFocus(false)) return;
    g.setColour(juce::Colour(0xff48f0c8));
    g.drawRoundedRectangle(control.getLocalBounds().toFloat().reduced(1.0f), 5.0f, 1.5f);
}
}

SausageLookAndFeel::SausageLookAndFeel() {
    setColourScheme(getDarkColourScheme());
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff152026));
    setColour(juce::TooltipWindow::textColourId, juce::Colour(0xffe6f0f5));
    setColour(juce::TooltipWindow::outlineColourId, juce::Colour(0xff48f0c8));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff305350));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe6f0f5));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff152026));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff48f0c8));
    setColour(juce::Slider::trackColourId, juce::Colour(0xff48f0c8));
    setColour(juce::Slider::backgroundColourId, juce::Colour(0xff283842));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff152026));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffe6f0f5));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff283842));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff48f0c8));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff152026));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe6f0f5));
}
void SausageLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position, float start, float end, juce::Slider& slider) {
    const DimWhenDisabled dim(g, slider);
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(13.0f);
    const auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre(diameter, diameter);
    const auto center = bounds.getCentre();
    const auto radius = diameter * 0.5f;
    const auto angle = start + position * (end - start);
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    // A range that spans zero, such as a gain, lights the ticks from 0 to the value.
    const bool spansZero = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto origin = spansZero ? static_cast<float>(slider.valueToProportionOfLength(0.0)) : 0.0f;
    const auto lowest = static_cast<int>(std::ceil(std::min(origin, position) * 20.0f));
    const auto highest = static_cast<int>(std::max(origin, position) * 20.0f);
    for (int tick = 0; tick <= 20; ++tick) {
        const auto a = start + static_cast<float>(tick) / 20.0f * (end - start);
        g.setColour(tick >= lowest && tick <= highest ? accent : juce::Colour(0xff536266));
        const juce::Point<float> p {std::sin(a), -std::cos(a)};
        g.drawLine({center + p * (radius + 6.0f), center + p * (radius + 10.0f)}, 1.7f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillEllipse(bounds.translated(0, 5));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff6b7371), bounds.getTopLeft(), juce::Colour(0xff121b1e), bounds.getBottomRight(), false));
    g.fillEllipse(bounds);
    g.setColour(juce::Colour(0xff89928b));
    g.drawEllipse(bounds, 1.5f);
    auto face = bounds.reduced(diameter * 0.12f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff39474b), face.getTopLeft(), juce::Colour(0xff172126), face.getBottomRight(), false));
    g.fillEllipse(face);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawEllipse(face, 1.0f);
    const juce::Point<float> direction {std::sin(angle), -std::cos(angle)};
    g.setColour(accent);
    g.drawLine({center + direction * (radius * 0.38f), center + direction * (radius * 0.77f)}, 4.0f);
    g.fillEllipse(center.x - 3.0f, center.y - 3.0f, 6.0f, 6.0f);
    drawFocusRing(g, slider);
}
void SausageLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position, float minimum, float maximum,
                                          juce::Slider::SliderStyle style, juce::Slider& slider) {
    const DimWhenDisabled dim(g, slider);
    if (style != juce::Slider::LinearHorizontal) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, position, minimum, maximum, style, slider);
        drawFocusRing(g, slider);
        return;
    }
    const auto trackHeight = 6.0f;
    const auto trackY = static_cast<float>(y) + (static_cast<float>(height) - trackHeight) * 0.5f;
    const juce::Rectangle<float> track(static_cast<float>(x) + 4.0f, trackY, static_cast<float>(width) - 8.0f, trackHeight);
    g.setColour(juce::Colour(0xff0e161a));
    g.fillRoundedRectangle(track, 3.0f);
    g.setColour(juce::Colour(0xff354852));
    g.drawRoundedRectangle(track, 3.0f, 1.0f);
    const auto clampedPos = juce::jlimit(track.getX(), track.getRight(), position);
    const auto accent = slider.findColour(juce::Slider::trackColourId);
    if (clampedPos > track.getX()) {
        g.setColour(accent.withAlpha(0.85f));
        g.fillRoundedRectangle(track.withRight(clampedPos), 3.0f);
    }
    constexpr float thumbRadius = 6.5f;
    const auto thumbY = track.getCentreY();
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillEllipse(clampedPos - thumbRadius, thumbY - thumbRadius + 1.5f, thumbRadius * 2.0f, thumbRadius * 2.0f);
    g.setColour(accent);
    g.fillEllipse(clampedPos - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
    g.setColour(juce::Colour(0xff152026));
    g.fillEllipse(clampedPos - 2.2f, thumbY - 2.2f, 4.4f, 4.4f);
    drawFocusRing(g, slider);
}
void SausageLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down, int buttonX, int buttonY, int buttonWidth, int buttonHeight,
                                      juce::ComboBox& box) {
    const DimWhenDisabled dim(g, box);
    LookAndFeel_V4::drawComboBox(g, width, height, down, buttonX, buttonY, buttonWidth, buttonHeight, box);
    drawFocusRing(g, box);
}
void SausageLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) {
    over = over || button.hasKeyboardFocus(false);
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    if (button.getComponentID().startsWith("suite")) {
        const bool on = button.getToggleState();
        g.setColour(juce::Colour(!button.isEnabled() ? 0xff141c20 : on ? 0xff243936 : over || down ? 0xff2a3b40 : 0xff182429));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(on && button.isEnabled() ? juce::Colour(0xff48f0c8) : juce::Colour(0xff283842));
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
        return;
    }
    const DimWhenDisabled dim(g, button);
    if (button.getComponentID() == "softClip") {
        const auto enabled = button.getToggleState();
        g.setColour(juce::Colour(over || down ? 0xff2a3b40 : 0xff182429));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(enabled ? juce::Colour(0xff48f0c8) : juce::Colour(0xff354852));
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
        g.setColour(enabled ? juce::Colour(0xff48f0c8) : juce::Colour(0xff7a8e96));
        g.fillEllipse(10.0f, static_cast<float>(button.getHeight()) / 2.0f - 3.0f, 6.0f, 6.0f);
        return;
    }
    const auto bypassed = button.getToggleState();
    g.setColour(bypassed ? juce::Colour(0xff30373a) : juce::Colour(0xff182429));
    if (over || down) g.setColour(bypassed ? juce::Colour(0xff43494b) : juce::Colour(0xff2a3b40));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(juce::Colour(0xff283842));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    g.setColour(bypassed ? juce::Colour(0xffffbe3b) : juce::Colour(0xff48f0c8));
    g.fillEllipse(10.0f, static_cast<float>(button.getHeight()) / 2.0f - 3.0f, 6.0f, 6.0f);
}
void SausageLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    const auto id = button.getComponentID();
    if (id.startsWith("suite")) {
        g.setColour(button.isEnabled() ? (button.getToggleState() ? juce::Colour(0xff48f0c8) : juce::Colour(0xffe6f0f5)) : juce::Colour(0xff536266));
        const auto centre = button.getLocalBounds().toFloat().getCentre();
        juce::Path icon;
        if (id == "suiteFavorite") {
            icon.addStar(centre, 5, 5.0f, 11.0f);
            if (button.getToggleState()) g.fillPath(icon);
            g.strokePath(icon, juce::PathStrokeType(1.6f));
            return;
        }
        if (id == "suitePrevious" || id == "suiteNext") {
            const float pointing = id == "suiteNext" ? 1.0f : -1.0f;
            icon.addTriangle(centre.x - 5.0f * pointing, centre.y - 7.0f, centre.x - 5.0f * pointing, centre.y + 7.0f, centre.x + 6.0f * pointing, centre.y);
            g.fillPath(icon);
            return;
        }
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
        return;
    }
    const DimWhenDisabled dim(g, button);
    if (button.getComponentID() == "softClip") {
        g.setColour(button.getToggleState() ? juce::Colour(0xff48f0c8) : juce::Colour(0xffc2d2da));
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText("Soft clip", button.getLocalBounds().withTrimmedLeft(20), juce::Justification::centred);
        return;
    }
    g.setColour(button.getToggleState() ? juce::Colour(0xff869ca8) : juce::Colour(0xffe6f0f5));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    const auto bypassed = button.getToggleState();
    const auto moduleLabel = bypassed ? "Bypassed" : "Active";
    const auto masterLabel = bypassed ? "Master bypassed" : "Master active";
    const auto label = button.getComponentID() == "bypass" ? masterLabel : moduleLabel;
    g.drawText(label, button.getLocalBounds().withTrimmedLeft(20), juce::Justification::centred);
}
