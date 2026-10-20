#include "SauceRack.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace sausage {
namespace {
float unit(float value) noexcept { return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f; }
float follow(float current, float target, float pole) noexcept {
    const auto value = target + pole * (current - target);
    return std::abs(value - target) < 1.0e-6f ? target : value;
}
float clean(float value) noexcept { return std::isfinite(value) ? std::clamp(value, -32.0f, 32.0f) : 0.0f; }
}
void Rack::Delay::resize(std::size_t size) { data.assign(size, 0.0f); position = 0; }
void Rack::Delay::clear() noexcept { std::fill(data.begin(), data.end(), 0.0f); position = 0; }
float Rack::Delay::read(float delay) const noexcept {
    if (data.empty()) return 0.0f;
    const auto index = static_cast<double>(position) - static_cast<double>(std::clamp(delay, 1.0f, static_cast<float>(data.size() - 1)));
    const auto base = std::floor(index);
    // Wrap the integer index: adding the buffer length to a tiny negative float can round past its end.
    const auto lower = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(base) + static_cast<std::ptrdiff_t>(data.size())) % data.size();
    const auto fraction = static_cast<float>(index - base);
    return data[lower] + fraction * (data[(lower + 1) % data.size()] - data[lower]);
}
void Rack::Delay::write(float sample) noexcept {
    if (data.empty()) return;
    data[position] = sample;
    position = (position + 1) % data.size();
}
void Rack::setSettings(RackSettings settings) noexcept {
    target = settings;
    target.fatness = unit(settings.fatness); target.color = unit(settings.color);
    target.space = unit(settings.space); target.size = unit(settings.size);
    target.rise = unit(settings.rise); target.riseMix = unit(settings.riseMix);
    target.width = unit(settings.width); target.spread = unit(settings.spread);
    target.outputDb = std::isfinite(settings.outputDb) ? std::clamp(settings.outputDb, -24.0f, 12.0f) : 0.0f;
    core.setSettings({target.fatness, target.color, 0.0f, !target.sausageOn});
}
void Rack::prepare(double sampleRate) {
    rate = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 48000.0;
    smoothing = static_cast<float>(std::exp(-1.0 / (0.01 * rate)));
    core.prepare(rate);
    constexpr double times[] {0.0297, 0.0371, 0.0411, 0.0437, 0.0307, 0.0383, 0.0427, 0.0451};
    for (std::size_t i = 0; i < combs.size(); ++i) combs[i].resize(static_cast<std::size_t>(rate * times[i]) + 1);
    // Freeverb allpass tunings in samples at 44.1 kHz. The right channel is 23 samples longer, for stereo spread.
    constexpr double diffuserSamples[] {556.0, 441.0, 341.0, 225.0};
    for (std::size_t ch = 0; ch < diffusers.size(); ++ch)
        for (std::size_t i = 0; i < diffusers[ch].size(); ++i)
            diffusers[ch][i].resize(static_cast<std::size_t>((diffuserSamples[i] + 23.0 * static_cast<double>(ch)) * rate / 44100.0) + 1);
    for (auto& delay : pitch) delay.resize(static_cast<std::size_t>(rate * 0.08) + 4);
    for (auto& delay : widen) delay.resize(static_cast<std::size_t>(rate * 0.04) + 4);
    reset();
}
void Rack::reset() noexcept {
    core.reset();
    for (auto& delay : combs) delay.clear();
    for (auto& channel : diffusers) for (auto& delay : channel) delay.clear();
    for (auto& delay : pitch) delay.clear();
    for (auto& delay : widen) delay.clear();
    damp = {}; highpass = {}; previous = {}; dry = {};
    phase = 0.0f; dryPosition = 0;
    smooth = target;
    if (!target.spaceOn) smooth.space = 0.0f;
    if (!target.riseOn) smooth.riseMix = 0.0f;
    if (!target.widthOn) smooth.width = 0.0f;
    active = target.bypass ? 0.0f : 1.0f;
    spaceCleared = riseCleared = widthCleared = true;
}
void Rack::process(float* left, float* right, int samples) noexcept {
    if (left == nullptr || samples <= 0) return;
    for (int i = 0; i < samples; ++i) {
        float l = clean(left[i]), r = right ? clean(right[i]) : l;
        const float original[] {l, r};
        const float delayed[] {dry[0][static_cast<std::size_t>(dryPosition)], dry[1][static_cast<std::size_t>(dryPosition)]};
        for (std::size_t ch = 0; ch < 2; ++ch) dry[ch][static_cast<std::size_t>(dryPosition)] = original[ch];
        dryPosition = (dryPosition + 1) % Core::latency;
        core.process(&l, right ? &r : nullptr, 1);
        if (!right) r = l;
        smooth.space = follow(smooth.space, target.spaceOn ? target.space : 0.0f, smoothing);
        smooth.size = follow(smooth.size, target.size, smoothing);
        smooth.riseMix = follow(smooth.riseMix, target.riseOn ? target.riseMix : 0.0f, smoothing);
        smooth.rise = follow(smooth.rise, target.rise, smoothing);
        smooth.width = follow(smooth.width, target.widthOn ? target.width : 0.0f, smoothing);
        smooth.spread = follow(smooth.spread, target.spread, smoothing);
        smooth.outputDb = follow(smooth.outputDb, target.outputDb, smoothing);
        active = follow(active, target.bypass ? 0.0f : 1.0f, smoothing);

        if (smooth.space > 0.0f) {
            spaceCleared = false;
            float wet[] {0.0f, 0.0f};
            const auto mid = (l + r) * 0.5f;
            const auto feedback = 0.45f + smooth.size * 0.49f;
            for (std::size_t tap = 0; tap < combs.size(); ++tap) {
                const auto echo = combs[tap].read(static_cast<float>(combs[tap].data.size() - 1));
                damp[tap] = echo * 0.65f + damp[tap] * 0.35f;
                combs[tap].write(mid * 0.25f + damp[tap] * feedback);
                wet[tap / 4] += echo * 0.6f;
            }
            // Series Schroeder allpasses, (z^-N - g) / (1 - g z^-N), turn the sparse comb echoes into a dense tail.
            // Freeverb's version outputs delayed - input, which is not allpass and boosts some frequencies up to 4x.
            for (std::size_t ch = 0; ch < diffusers.size(); ++ch) {
                for (auto& allpass : diffusers[ch]) {
                    const auto delayed = allpass.read(static_cast<float>(allpass.data.size() - 1));
                    const auto fed = wet[ch] + 0.5f * delayed;
                    allpass.write(fed);
                    wet[ch] = delayed - 0.5f * fed;
                }
            }
            l += smooth.space * (wet[0] - l);
            r += smooth.space * (wet[1] - r);
        } else if (!spaceCleared) {
            for (auto& delay : combs) delay.clear();
            for (auto& channel : diffusers) for (auto& delay : channel) delay.clear();
            damp = {}; spaceCleared = true;
        }

        if (smooth.riseMix > 0.0f) {
            riseCleared = false;
            const auto window = static_cast<float>(rate * 0.05);
            const auto ratio = std::pow(2.0f, smooth.rise);
            phase += (ratio - 1.0f) / window;
            phase -= std::floor(phase);
            const auto second = std::fmod(phase + 0.5f, 1.0f);
            const auto weight = 0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<float> * phase);
            const float inputs[] {l, r};
            const auto hpPole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * (20.0 + 600.0 * smooth.rise) / rate));
            for (std::size_t ch = 0; ch < 2; ++ch) {
                const auto shifted = weight * pitch[ch].read(2.0f + (1.0f - phase) * window)
                                   + (1.0f - weight) * pitch[ch].read(2.0f + (1.0f - second) * window);
                pitch[ch].write(inputs[ch] + shifted * smooth.rise * 0.25f);
                highpass[ch] = shifted - previous[ch] + hpPole * highpass[ch];
                previous[ch] = shifted;
                (ch == 0 ? l : r) += smooth.riseMix * (highpass[ch] - inputs[ch]);
            }
        } else if (!riseCleared) {
            for (auto& delay : pitch) delay.clear();
            highpass = {}; previous = {}; phase = 0.0f; riseCleared = true;
        }

        if (smooth.width > 0.0f && right) {
            widthCleared = false;
            const auto mid = (l + r) * 0.5f;
            const auto side = (l - r) * 0.5f;
            const auto first = widen[0].read(static_cast<float>(rate) * (0.003f + smooth.spread * 0.011f));
            const auto second = widen[1].read(static_cast<float>(rate) * (0.009f + smooth.spread * 0.019f));
            widen[0].write(mid); widen[1].write(mid);
            const auto widened = side * (1.0f + smooth.width) + (first - second) * smooth.width * 0.65f;
            l = mid + widened; r = mid - widened;
        } else if (!widthCleared) {
            for (auto& delay : widen) delay.clear();
            widthCleared = true;
        }
        const auto gain = std::pow(10.0f, smooth.outputDb / 20.0f);
        left[i] = active == 0.0f ? delayed[0] : delayed[0] + active * (l * gain - delayed[0]);
        if (right) right[i] = active == 0.0f ? delayed[1] : delayed[1] + active * (r * gain - delayed[1]);
    }
}
}
