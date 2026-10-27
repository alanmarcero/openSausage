#include "SausageCore.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace sausage {
namespace {
float finite(float value, float fallback = 0.0f) noexcept { return std::isfinite(value) ? value : fallback; }
float pole(double seconds, double rate) noexcept { return static_cast<float>(std::exp(-1.0 / (seconds * rate))); }
float follow(float value, float goal, float coefficient) noexcept {
    const auto result = goal + coefficient * (value - goal);
    return std::abs(result - goal) < 1.0e-6f ? goal : result;
}
}

float Core::Fir::tick(float input, const std::array<float, 65>& taps) noexcept {
    history[static_cast<std::size_t>(position)] = input;
    float result = 0.0f;
    int index = position;
    for (std::size_t tap = 0; tap < taps.size(); ++tap) {
        result += taps[tap] * history[static_cast<std::size_t>(index)];
        if (--index < 0) index = 64;
    }
    if (++position == 65) position = 0;
    return result;
}

void Core::setSettings(Settings settings) noexcept {
    target = {std::clamp(finite(settings.fatness), 0.0f, 1.0f),
              std::clamp(finite(settings.color), 0.0f, 1.0f),
              std::clamp(finite(settings.outputDb), -24.0f, 12.0f), settings.bypass, validAlgorithm(settings.algorithm)};
}

void Core::prepare(double sampleRate) noexcept {
    rate = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 48000.0;
    smoothing = pole(0.01, rate);
    attack = pole(0.003, rate);
    release = pole(0.08, rate);
    tonePole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 1200.0 / (rate * 4.0)));
    dcPole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 8.0 / (rate * 4.0)));
    double sum = 0.0;
    for (std::size_t i = 0; i < coefficients.size(); ++i) {
        const auto x = static_cast<double>(i) - 32.0;
        const auto sinc = x == 0.0 ? 0.225 : std::sin(2.0 * std::numbers::pi * 0.1125 * x) / (std::numbers::pi * x);
        const auto window = 0.42 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / 64.0)
                          + 0.08 * std::cos(4.0 * std::numbers::pi * static_cast<double>(i) / 64.0);
        coefficients[i] = static_cast<float>(sinc * window);
        sum += coefficients[i];
    }
    for (auto& coefficient : coefficients) coefficient /= static_cast<float>(sum);
    reset();
}

void Core::reset() noexcept {
    channels = {};
    envelope = reduction = 0.0f;
    dryPosition = 0;
    current = target;
    active = target.bypass ? 0.0f : 1.0f;
}

void Core::process(float* left, float* right, int samples) noexcept {
    if (left == nullptr || samples <= 0) return;
    for (int i = 0; i < samples; ++i) {
        current.fatness = follow(current.fatness, target.fatness, smoothing);
        current.color = follow(current.color, target.color, smoothing);
        current.outputDb = follow(current.outputDb, target.outputDb, smoothing);
        active = follow(active, target.bypass ? 0.0f : 1.0f, smoothing);
        const float input[] {std::clamp(finite(left[i]), -32.0f, 32.0f), right ? std::clamp(finite(right[i]), -32.0f, 32.0f) : 0.0f};
        const auto peak = std::max(std::abs(input[0]), std::abs(input[1]));
        envelope = follow(envelope, peak, peak > envelope ? attack : release);
        const auto threshold = std::pow(10.0f, -14.0f * current.fatness / 20.0f);
        const auto compression = envelope > threshold ? std::pow(threshold / envelope, 1.0f - 1.0f / (1.0f + 7.0f * current.fatness)) : 1.0f;
        reduction = -20.0f * std::log10(std::max(compression, 1.0e-8f));
        const auto drive = 1.0f + 24.0f * current.fatness;
        const auto blend = std::min(1.0f, current.fatness * 5.0f + current.color * 3.0f);
        const auto gain = std::pow(10.0f, current.outputDb / 20.0f);
        for (int ch = 0; ch < (right ? 2 : 1); ++ch) {
            auto& state = channels[static_cast<std::size_t>(ch)];
            const auto dry = state.dry[static_cast<std::size_t>(dryPosition)];
            state.dry[static_cast<std::size_t>(dryPosition)] = input[ch];
            float wet = 0.0f;
            for (int sub = 0; sub < 4; ++sub) {
                const auto up = state.up.tick(sub == 0 ? input[ch] * 4.0f : 0.0f, coefficients);
                state.low = up + tonePole * (state.low - up);
                const auto colored = up + current.color * 2.5f * (up - state.low);
                const auto shaped = state.shaper.process(colored * compression * drive, target.algorithm);
                const auto saturated = colored + current.fatness * (shaped - colored);
                const auto dc = saturated - state.dcInput + dcPole * state.dcOutput;
                state.dcInput = saturated;
                state.dcOutput = dc;
                const auto down = state.down.tick(dc, coefficients);
                if (sub == 0) wet = down;
            }
            const auto processed = (dry + blend * (wet - dry)) * gain;
            (ch == 0 ? left : right)[i] = active == 0.0f ? dry : dry + active * (processed - dry);
        }
        dryPosition = (dryPosition + 1) % latency;
    }
}
}
