#include "SausageLookAndFeel.h"

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
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff152026));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffe6f0f5));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff283842));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff48f0c8));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff152026));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xffe6f0f5));
}
void SausageLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position, float start, float end, juce::Slider& slider) {
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(13.0f);
    const auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    bounds = bounds.withSizeKeepingCentre(diameter, diameter);
    const auto center = bounds.getCentre();
    const auto radius = diameter * 0.5f;
    const auto angle = start + position * (end - start);
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    for (int tick = 0; tick <= 20; ++tick) {
        const auto a = start + static_cast<float>(tick) / 20.0f * (end - start);
        g.setColour(tick <= static_cast<int>(position * 20.0f) ? accent : juce::Colour(0xff536266));
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
}
void SausageLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) {
    over = over || button.hasKeyboardFocus(false);
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    if (button.getComponentID() == "prevRecipe" || button.getComponentID() == "nextRecipe") {
        g.setColour(juce::Colour(over || down ? 0xff2a3b40 : 0xff182429));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(juce::Colour(0xff283842));
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
        return;
    }
    if (button.getComponentID() == "softClip") {
        const auto enabled = button.getToggleState();
        g.setColour(juce::Colour(over || down ? 0xff2a3b40 : 0xff182429));
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(enabled ? juce::Colour(0xff48f0c8) : juce::Colour(0xff283842));
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
        g.setColour(enabled ? juce::Colour(0xff48f0c8) : juce::Colour(0xff536266));
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
    if (button.getComponentID() == "prevRecipe" || button.getComponentID() == "nextRecipe") {
        g.setColour(juce::Colour(0xffe6f0f5));
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
        return;
    }
    if (button.getComponentID() == "softClip") {
        g.setColour(button.getToggleState() ? juce::Colour(0xff48f0c8) : juce::Colour(0xff869ca8));
        g.setFont(juce::FontOptions(13.5f, juce::Font::bold));
        g.drawText("Soft Clip", button.getLocalBounds().withTrimmedLeft(20), juce::Justification::centred);
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
