#include "SausageLookAndFeel.h"

SausageLookAndFeel::SausageLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff8efd7));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff242622));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff242622));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xfff8efd7));
    setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff242622));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff8efd7));
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
        g.setColour(tick <= static_cast<int>(position * 20.0f) ? accent : juce::Colour(0xffc6bea9));
        const juce::Point<float> p {std::sin(a), -std::cos(a)};
        g.drawLine({center + p * (radius + 6.0f), center + p * (radius + 10.0f)}, 1.7f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillEllipse(bounds.translated(0, 5));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff55564d), bounds.getTopLeft(), juce::Colour(0xff151713), bounds.getBottomRight(), false));
    g.fillEllipse(bounds);
    g.setColour(juce::Colour(0xff171913));
    g.drawEllipse(bounds, 1.5f);
    auto face = bounds.reduced(diameter * 0.12f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff33362f), face.getTopLeft(), juce::Colour(0xff1c1e19), face.getBottomRight(), false));
    g.fillEllipse(face);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawEllipse(face, 1.0f);
    const juce::Point<float> direction {std::sin(angle), -std::cos(angle)};
    g.setColour(accent);
    g.drawLine({center + direction * (radius * 0.38f), center + direction * (radius * 0.77f)}, 4.0f);
    g.fillEllipse(center.x - 3.0f, center.y - 3.0f, 6.0f, 6.0f);
}
void SausageLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) {
    const auto bypassed = button.getToggleState();
    g.setColour(bypassed ? juce::Colour(0xffd8cfba) : juce::Colour(0xff252921));
    if (over || down) g.setColour(bypassed ? juce::Colour(0xffcbbfa5) : juce::Colour(0xff3c4332));
    g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 5.0f);
    g.setColour(bypassed ? juce::Colour(0xff9b4f36) : juce::Colour(0xffcbe27a));
    g.fillEllipse(10.0f, static_cast<float>(button.getHeight()) / 2.0f - 3.0f, 6.0f, 6.0f);
}
void SausageLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    g.setColour(button.getToggleState() ? juce::Colour(0xff615b4d) : juce::Colour(0xfff8efd7));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    const auto label = button.getComponentID() == "bypass" ? (button.getToggleState() ? "MASTER BYPASSED" : "MASTER ACTIVE") : (button.getToggleState() ? "BYPASSED" : "ACTIVE");
    g.drawText(label, button.getLocalBounds().withTrimmedLeft(20), juce::Justification::centred);
}
