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
void RackEngine::Delay::resize(std::size_t size) { data.assign(size, 0.0f); position = 0; }
void RackEngine::Delay::clear() noexcept { std::fill(data.begin(), data.end(), 0.0f); position = 0; }
float RackEngine::Delay::read(float delay) const noexcept {
    if (data.empty()) return 0.0f;
    const auto index = static_cast<double>(position) - static_cast<double>(std::clamp(delay, 1.0f, static_cast<float>(data.size() - 1)));
    const auto base = std::floor(index);
    // Wrap the integer index: adding the buffer length to a tiny negative float can round past its end.
    const auto lower = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(base) + static_cast<std::ptrdiff_t>(data.size())) % data.size();
    const auto fraction = static_cast<float>(index - base);
    return data[lower] + fraction * (data[(lower + 1) % data.size()] - data[lower]);
}
void RackEngine::Delay::write(float sample) noexcept {
    if (data.empty()) return;
    data[position] = sample;
    position = (position + 1) % data.size();
}
void RackEngine::setSettings(RackSettings settings) noexcept {
    target = settings;
    target.algorithm = validAlgorithm(settings.algorithm);
    target.fatness = unit(settings.fatness); target.color = unit(settings.color);
    target.space = unit(settings.space); target.size = unit(settings.size);
    target.rise = unit(settings.rise); target.riseMix = unit(settings.riseMix);
    target.width = unit(settings.width); target.spread = unit(settings.spread);
    target.outputDb = std::isfinite(settings.outputDb) ? std::clamp(settings.outputDb, -24.0f, 12.0f) : 0.0f;
    core.setSettings({target.fatness, target.color, 0.0f, !target.sausageOn, target.algorithm});
}
void RackEngine::prepare(double sampleRate) {
    rate = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 48000.0;
    smoothing = static_cast<float>(std::exp(-1.0 / (0.01 * rate)));
    core.prepare(rate);
    widthPole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 250.0 / rate));
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
void RackEngine::reset() noexcept {
    core.reset();
    for (auto& delay : combs) delay.clear();
    for (auto& channel : diffusers) for (auto& delay : channel) delay.clear();
    for (auto& delay : pitch) delay.clear();
    for (auto& delay : widen) delay.clear();
    damp = {}; highpass = {}; previous = {}; dry = {};
    phase = 0.0f; dryPosition = 0;
    grainPhases = {0.0f, 0.25f, 0.5f, 0.75f};
    widthAllpass = {}; widthLow = widthPhase = 0.0f;
    smooth = target;
    if (!target.spaceOn) smooth.space = 0.0f;
    if (!target.riseOn) smooth.riseMix = 0.0f;
    if (!target.widthOn) smooth.width = 0.0f;
    active = target.bypass ? 0.0f : 1.0f;
    spaceCleared = riseCleared = widthCleared = true;
}
void RackEngine::process(float* left, float* right, int samples) noexcept {
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
            const auto mode = target.algorithm;
            std::array<float, 8> echoes {}, feedbackSignal {};
            const auto damping = mode == Algorithm::Velvet ? 0.6f : mode == Algorithm::Prism ? 0.2f : 0.35f;
            for (std::size_t tap = 0; tap < combs.size(); ++tap) {
                const auto length = static_cast<float>(combs[tap].data.size() - 1);
                echoes[tap] = combs[tap].read(mode == Algorithm::Punch ? std::floor(length * 0.4f) : length);
                damp[tap] = echoes[tap] * (1.0f - damping) + damp[tap] * damping;
                feedbackSignal[tap] = damp[tap];
                wet[tap / 4] += echoes[tap] * 0.6f;
            }
            if (mode == Algorithm::Velvet) {
                float sum = 0.0f;
                for (const auto value : damp) sum += value;
                for (auto& value : feedbackSignal) value -= 0.25f * sum;
            } else if (mode == Algorithm::Prism) {
                for (std::size_t stride = 1; stride < feedbackSignal.size(); stride *= 2)
                    for (std::size_t base = 0; base < feedbackSignal.size(); base += stride * 2)
                        for (std::size_t j = 0; j < stride; ++j) {
                            const auto a = feedbackSignal[base + j], b = feedbackSignal[base + j + stride];
                            feedbackSignal[base + j] = a + b;
                            feedbackSignal[base + j + stride] = a - b;
                        }
                for (auto& value : feedbackSignal) value *= 0.3535533905932738f;
            }
            const auto feedback = mode == Algorithm::Punch ? 0.25f + smooth.size * 0.5f : 0.45f + smooth.size * 0.49f;
            for (std::size_t tap = 0; tap < combs.size(); ++tap)
                combs[tap].write(mid * 0.25f + feedbackSignal[tap] * feedback);
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
            const auto mode = target.algorithm;
            const auto windowSeconds = mode == Algorithm::Velvet ? 0.065 : mode == Algorithm::Punch ? 0.02 : 0.05;
            const auto window = static_cast<float>(rate * windowSeconds);
            const auto ratio = std::pow(2.0f, smooth.rise);
            phase += (ratio - 1.0f) / window;
            phase -= std::floor(phase);
            const float inputs[] {l, r};
            std::array<float, 2> shifted {};
            const auto heads = mode == Algorithm::Velvet || mode == Algorithm::Prism ? 4 : 2;
            if (heads == 4)
                for (std::size_t head = 0; head < grainPhases.size(); ++head) {
                    grainPhases[head] += (ratio - 1.0f) / (window * (1.0f + 0.06f * static_cast<float>(head)));
                    grainPhases[head] -= std::floor(grainPhases[head]);
                }
            const auto hpPole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * (20.0 + 600.0 * smooth.rise) / rate));
            for (std::size_t ch = 0; ch < 2; ++ch) {
                if (mode == Algorithm::Classic) {
                    const auto second = std::fmod(phase + 0.5f, 1.0f);
                    const auto weight = 0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<float> * phase);
                    shifted[ch] = weight * pitch[ch].read(2.0f + (1.0f - phase) * window)
                                  + (1.0f - weight) * pitch[ch].read(2.0f + (1.0f - second) * window);
                } else {
                    float totalWeight = 0.0f;
                    for (int head = 0; head < heads; ++head) {
                        const auto grainPhase = heads == 4 ? grainPhases[static_cast<std::size_t>(head)] : phase + static_cast<float>(head) * 0.5f;
                        const auto grainWindow = heads == 4 ? window * (1.0f + 0.06f * static_cast<float>(head)) : window;
                        const auto position = std::fmod(grainPhase
                                                       + (mode == Algorithm::Prism ? static_cast<float>(ch) * 0.125f : 0.0f), 1.0f);
                        const auto weight = mode == Algorithm::Punch ? 1.0f - std::abs(2.0f * position - 1.0f)
                            : (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * position)) / static_cast<float>(heads);
                        shifted[ch] += weight * pitch[ch].read(2.0f + (1.0f - position) * grainWindow);
                        totalWeight += weight;
                    }
                    shifted[ch] /= std::max(totalWeight, 1.0e-6f);
                }
            }
            const auto feedback = mode == Algorithm::Velvet ? 0.16f : mode == Algorithm::Punch ? 0.1f : 0.25f;
            for (std::size_t ch = 0; ch < 2; ++ch) {
                pitch[ch].write(inputs[ch] + shifted[mode == Algorithm::Prism ? 1 - ch : ch] * smooth.rise * feedback);
                highpass[ch] = shifted[ch] - previous[ch] + hpPole * highpass[ch];
                previous[ch] = shifted[ch];
                (ch == 0 ? l : r) += smooth.riseMix * (highpass[ch] - inputs[ch]);
            }
        } else if (!riseCleared) {
            for (auto& delay : pitch) delay.clear();
            highpass = {}; previous = {}; phase = 0.0f; riseCleared = true;
            grainPhases = {0.0f, 0.25f, 0.5f, 0.75f};
        }

        if (smooth.width > 0.0f && right) {
            widthCleared = false;
            const auto mid = (l + r) * 0.5f;
            const auto side = (l - r) * 0.5f;
            float decorrelated;
            if (target.algorithm == Algorithm::Velvet) {
                constexpr float coefficients[2][4] {{0.2f, 0.45f, -0.25f, 0.7f}, {-0.45f, 0.1f, 0.6f, -0.65f}};
                float outputs[] {mid, mid};
                for (std::size_t ch = 0; ch < 2; ++ch)
                    for (std::size_t stage = 0; stage < 4; ++stage) {
                        const auto coefficient = coefficients[ch][stage] * (0.3f + smooth.spread * 0.6f);
                        const auto output = widthAllpass[ch][stage] - coefficient * outputs[ch];
                        widthAllpass[ch][stage] = outputs[ch] + coefficient * output;
                        outputs[ch] = output;
                    }
                decorrelated = outputs[0] - outputs[1];
            } else {
                float modulation = 0.0f;
                if (target.algorithm == Algorithm::Prism) {
                    widthPhase += static_cast<float>(0.23 / rate);
                    widthPhase -= std::floor(widthPhase);
                    modulation = std::sin(2.0f * std::numbers::pi_v<float> * widthPhase) * 0.002f;
                }
                const auto first = widen[0].read(static_cast<float>(rate) * (0.003f + smooth.spread * 0.011f + modulation));
                const auto second = widen[1].read(static_cast<float>(rate) * (0.009f + smooth.spread * 0.019f - modulation));
                widen[0].write(mid); widen[1].write(mid);
                decorrelated = first - second;
            }
            auto added = side + decorrelated * 0.65f;
            if (target.algorithm == Algorithm::Punch) {
                widthLow = added + widthPole * (widthLow - added);
                added -= widthLow;
            }
            const auto widened = target.algorithm == Algorithm::Classic
                ? side * (1.0f + smooth.width) + decorrelated * smooth.width * 0.65f : side + added * smooth.width;
            l = mid + widened; r = mid - widened;
        } else if (!widthCleared) {
            for (auto& delay : widen) delay.clear();
            widthAllpass = {}; widthLow = widthPhase = 0.0f;
            widthCleared = true;
        }
        const auto gain = std::pow(10.0f, smooth.outputDb / 20.0f);
        left[i] = active == 0.0f ? delayed[0] : delayed[0] + active * (l * gain - delayed[0]);
        if (right) right[i] = active == 0.0f ? delayed[1] : delayed[1] + active * (r * gain - delayed[1]);
    }
}

void Rack::setSettings(RackSettings settings) noexcept {
    target = settings;
    target.algorithm = validAlgorithm(settings.algorithm);
    for (std::size_t i = 0; i < engines.size(); ++i) {
        settings.algorithm = algorithms[i];
        engines[i].setSettings(settings);
    }
}
void Rack::prepare(double sampleRate) {
    const auto rate = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 48000.0;
    fadeSamples = static_cast<int>(rate * 0.02);
    for (auto& engine : engines) { engine.setSettings(target); engine.prepare(rate); }
    reset();
}
void Rack::reset() noexcept {
    current = 0; transition = 0;
    algorithms.fill(target.algorithm);
    for (auto& engine : engines) { engine.setSettings(target); engine.reset(); }
}
void Rack::process(float* left, float* right, int samples) noexcept {
    if (left == nullptr || samples <= 0) return;
    if (transition == 0 && algorithms[current] == target.algorithm) {
        engines[current].process(left, right, samples);
        return;
    }
    for (int i = 0; i < samples; ++i) {
        const auto next = 1 - current;
        if (transition == 0 && algorithms[current] != target.algorithm) {
            algorithms[next] = target.algorithm;
            engines[next].setSettings(target);
            engines[next].reset();
            transition = Core::latency + fadeSamples;
        }
        auto l = left[i], r = right ? right[i] : 0.0f;
        engines[current].process(left + i, right ? right + i : nullptr, 1);
        if (transition > 0) {
            engines[next].process(&l, right ? &r : nullptr, 1);
            if (transition <= fadeSamples) {
                const auto mix = static_cast<float>(fadeSamples - transition + 1) / static_cast<float>(fadeSamples);
                left[i] += mix * (l - left[i]);
                if (right) right[i] += mix * (r - right[i]);
            }
            if (--transition == 0) current = next;
        }
    }
}

}
