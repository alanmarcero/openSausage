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
    const auto sz = static_cast<std::ptrdiff_t>(data.size());
    const auto index = static_cast<double>(position) - static_cast<double>(std::clamp(delay, 1.0f, static_cast<float>(sz - 1)));
    const auto base = static_cast<std::ptrdiff_t>(std::floor(index));
    const auto fraction = static_cast<float>(index - std::floor(index));

    auto at = [this, sz](std::ptrdiff_t idx) noexcept -> float {
        return data[static_cast<std::size_t>((idx % sz + sz) % sz)];
    };

    const float ym1 = at(base - 1);
    const float y0  = at(base);
    const float y1  = at(base + 1);
    const float y2  = at(base + 2);

    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);

    return ((c3 * fraction + c2) * fraction + c1) * fraction + c0;
}
float RackEngine::Delay::readFixed(std::size_t delaySamples) const noexcept {
    if (data.empty()) return 0.0f;
    const auto sz = data.size();
    const auto d = std::min(delaySamples, sz - 1);
    return data[position >= d ? position - d : position + sz - d];
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
    target.saturationMix = unit(settings.saturationMix); target.dynamics = unit(settings.dynamics);
    target.focus = settings.focus >= SaturationFocus::Neutral && settings.focus <= SaturationFocus::KeepHigh
        ? settings.focus : SaturationFocus::Neutral;
    target.outputDb = std::isfinite(settings.outputDb) ? std::clamp(settings.outputDb, -24.0f, 12.0f) : 0.0f;
    target.preDelayMs = std::isfinite(settings.preDelayMs) ? std::clamp(settings.preDelayMs, 0.0f, 100.0f) : 0.0f;
    target.masterMix = unit(settings.masterMix);
    target.softClip = settings.softClip;
    target.riseBars = settings.riseBars > 0 ? settings.riseBars : 0;
    target.risePpq = std::isfinite(settings.risePpq) ? settings.risePpq : 0.0;
    target.riseTempo = std::isfinite(settings.riseTempo) && settings.riseTempo > 0.0 ? settings.riseTempo : 120.0;
    target.riseQuartersPerBar = std::isfinite(settings.riseQuartersPerBar) && settings.riseQuartersPerBar > 0.0
        ? settings.riseQuartersPerBar : 4.0;
    target.riseRunning = settings.riseRunning;
    core.setSettings({target.fatness, target.color, 0.0f, !target.sausageOn, target.algorithm,
                      target.saturationMix, target.dynamics, target.focus});
}
void RackEngine::prepare(double sampleRate) {
    rate = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 48000.0;
    smoothing = static_cast<float>(std::exp(-1.0 / (0.01 * rate)));
    core.prepare(rate);
    const auto svf = [this](double cutoff, float& a1, float& a2, float& a3) {
        const auto g = static_cast<float>(std::tan(std::numbers::pi * std::min(cutoff, 0.42 * rate) / rate));
        a1 = 1.0f / (1.0f + g * (g + std::numbers::sqrt2_v<float>));
        a2 = g * a1;
        a3 = g * g * a1;
    };
    widthPole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 250.0 / rate));
    svf(180.0, widthHpA1, widthHpA2, widthHpA3);
    svf(150.0, riseFbHpA1, riseFbHpA2, riseFbHpA3);
    svf(9000.0, riseFbLpA1, riseFbLpA2, riseFbLpA3);
    constexpr double times[] {0.0297, 0.0371, 0.0411, 0.0437, 0.0307, 0.0383, 0.0427, 0.0451};
    for (std::size_t i = 0; i < combs.size(); ++i) combs[i].resize(static_cast<std::size_t>(rate * times[i]) + 1);
    // Freeverb allpass tunings in samples at 44.1 kHz. The right channel is 23 samples longer, for stereo spread.
    constexpr double diffuserSamples[] {556.0, 441.0, 341.0, 225.0};
    for (std::size_t ch = 0; ch < diffusers.size(); ++ch)
        for (std::size_t i = 0; i < diffusers[ch].size(); ++i)
            diffusers[ch][i].resize(static_cast<std::size_t>((diffuserSamples[i] + 23.0 * static_cast<double>(ch)) * rate / 44100.0) + 1);
    for (auto& delay : pitch) delay.resize(static_cast<std::size_t>(rate * 0.15) + 4);
    for (auto& delay : widen) delay.resize(static_cast<std::size_t>(rate * 0.04) + 4);
    for (auto& delay : spacePreDelay) delay.resize(static_cast<std::size_t>(rate * 0.11) + 4);
    constexpr double widthMs[2][4] {{2.9, 4.7, 7.1, 10.3}, {3.7, 5.9, 8.3, 11.9}};
    for (std::size_t ch = 0; ch < widthDiffusers.size(); ++ch)
        for (std::size_t i = 0; i < widthDiffusers[ch].size(); ++i)
            widthDiffusers[ch][i].resize(static_cast<std::size_t>(rate * widthMs[ch][i] * 0.0016) + 4);
    reset();
}
void RackEngine::reset() noexcept {
    core.reset();
    for (auto& delay : combs) delay.clear();
    for (auto& channel : diffusers) for (auto& delay : channel) delay.clear();
    for (auto& delay : pitch) delay.clear();
    for (auto& delay : widen) delay.clear();
    for (auto& delay : spacePreDelay) delay.clear();
    for (auto& channel : widthDiffusers) for (auto& delay : channel) delay.clear();
    fdnLp = {}; fdnLow = {}; dry = {};
    for (std::size_t i = 0; i < fdnPhase.size(); ++i) fdnPhase[i] = static_cast<float>(i) * 0.137f;
    dryPosition = 0;
    riseReadAge = {static_cast<float>(rate * 0.04), static_cast<float>(rate * 0.04)};
    riseFadeAge = riseReadAge;
    riseFadeRemaining = {}; riseFadeTotal = {}; risePeriod = {};
    riseFbHpIc1 = {}; riseFbHpIc2 = {}; riseFbLpIc1 = {}; riseFbLpIc2 = {};
    clipPrevX = {}; clipPrevIntegral = {};
    widthHpIc1 = widthHpIc2 = 0.0f; widthLow = widthPhase = 0.0f;
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
        smooth.preDelayMs = follow(smooth.preDelayMs, target.preDelayMs, smoothing);
        smooth.masterMix = follow(smooth.masterMix, target.masterMix, smoothing);
        active = follow(active, target.bypass ? 0.0f : 1.0f, smoothing);

        if (smooth.space > 0.0f) {
            spaceCleared = false;
            float wet[] {0.0f, 0.0f};
            const float stereoIn[] {l, r};
            for (std::size_t ch = 0; ch < 2; ++ch) spacePreDelay[ch].write(stereoIn[ch]);
            const auto preDelaySamples = static_cast<float>(rate) * smooth.preDelayMs * 0.001f;
            float fdnIn[] {l, r};
            if (preDelaySamples >= 1.0f)
                for (std::size_t ch = 0; ch < 2; ++ch) fdnIn[ch] = spacePreDelay[ch].read(preDelaySamples);
            const auto mode = target.algorithm;
            std::array<float, 8> echoes {}, feedbackSignal {};
            const auto damping = mode == Algorithm::Velvet ? 0.6f : mode == Algorithm::Prism ? 0.2f : 0.35f;
            const auto lfDamp = mode == Algorithm::Velvet ? 0.28f : mode == Algorithm::Prism ? 0.12f : 0.2f;
            const auto lfPole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 220.0 / rate));
            for (std::size_t tap = 0; tap < combs.size(); ++tap) {
                const auto len = static_cast<float>(combs[tap].data.size() - 1);
                fdnPhase[tap] += static_cast<float>((0.4 + 0.11 * static_cast<double>(tap)) / rate);
                fdnPhase[tap] -= std::floor(fdnPhase[tap]);
                const auto modulation = std::sin(2.0f * std::numbers::pi_v<float> * fdnPhase[tap]) * static_cast<float>(rate * 0.0012);
                const auto baseDelay = mode == Algorithm::Punch ? len * 0.4f : len;
                echoes[tap] = combs[tap].read(std::clamp(baseDelay + modulation, 2.0f, len));
                fdnLp[tap] = echoes[tap] * (1.0f - damping) + fdnLp[tap] * damping;
                fdnLow[tap] = fdnLp[tap] + lfPole * (fdnLow[tap] - fdnLp[tap]);
                feedbackSignal[tap] = fdnLp[tap] - lfDamp * fdnLow[tap];
                wet[tap / 4] += echoes[tap] * 0.6f;
            }
            // Orthogonal 8x8 Hadamard mixing spreads every tap across the network each sample.
            for (std::size_t stride = 1; stride < feedbackSignal.size(); stride *= 2)
                for (std::size_t base = 0; base < feedbackSignal.size(); base += stride * 2)
                    for (std::size_t j = 0; j < stride; ++j) {
                        const auto a = feedbackSignal[base + j], b = feedbackSignal[base + j + stride];
                        feedbackSignal[base + j] = a + b;
                        feedbackSignal[base + j + stride] = a - b;
                    }
            for (auto& value : feedbackSignal) value *= 0.3535533905932738f;
            const auto feedback = mode == Algorithm::Punch ? 0.25f + smooth.size * 0.5f : 0.45f + smooth.size * 0.49f;
            for (std::size_t tap = 0; tap < combs.size(); ++tap)
                combs[tap].write(fdnIn[tap / 4] * 0.25f + feedbackSignal[tap] * feedback);
            // Series Schroeder allpasses, (z^-N - g) / (1 - g z^-N), turn the sparse comb echoes into a dense tail.
            // Freeverb's version outputs delayed - input, which is not allpass and boosts some frequencies up to 4x.
            for (std::size_t ch = 0; ch < diffusers.size(); ++ch) {
                for (auto& allpass : diffusers[ch]) {
                    const auto delayed = allpass.readFixed(allpass.data.size() - 1);
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
            for (auto& delay : spacePreDelay) delay.clear();
            fdnLp = {}; fdnLow = {}; spaceCleared = true;
        }

        if (smooth.riseMix > 0.0f) {
            riseCleared = false;
            const auto mode = target.algorithm;
            const auto amount = syncedRise(smooth.rise, target.riseBars,
                riseSamplePpq(target.risePpq, i, target.riseTempo, rate, target.riseRunning), target.riseQuartersPerBar);
            const auto ratio = std::clamp(std::pow(2.0f, amount), 1.0f, 2.0f);
            const float inputs[] {l, r};
            std::array<float, 2> shifted {};
            const int fadeLength = static_cast<int>(rate * (mode == Algorithm::Velvet ? 0.012 : mode == Algorithm::Punch ? 0.003 : mode == Algorithm::Prism ? 0.008 : 0.006));
            const auto triggerAge = static_cast<float>(rate * 0.016);
            for (std::size_t ch = 0; ch < 2; ++ch) {
                riseReadAge[ch] -= ratio - 1.0f;
                if (riseFadeRemaining[ch] > 0) riseFadeAge[ch] -= ratio - 1.0f;
                if (riseFadeRemaining[ch] <= 0 && riseReadAge[ch] < triggerAge && ratio > 1.0f) {
                    // Pitch-synchronous jump: find the lag whose waveform best matches the current read segment.
                    const int minLag = std::max(16, static_cast<int>(rate / 1200.0));
                    const int maxLag = std::min(static_cast<int>(rate / 40.0), static_cast<int>(rate * 0.03));
                    constexpr int segment = 48;
                    int bestLag = std::min(maxLag, std::max(minLag, static_cast<int>(rate * 0.005)));
                    double bestCorrelation = -2.0;
                    for (int lag = minLag; lag <= maxLag; lag += 8) {
                        double xy = 0.0, xx = 0.0, yy = 0.0;
                        for (int k = 0; k < segment; ++k) {
                            const auto a = static_cast<double>(pitch[ch].read(riseReadAge[ch] + static_cast<float>(k)));
                            const auto b = static_cast<double>(pitch[ch].read(riseReadAge[ch] + static_cast<float>(lag + k)));
                            xy += a * b; xx += a * a; yy += b * b;
                        }
                        const auto correlation = xy / std::sqrt(xx * yy + 1.0e-12);
                        if (correlation > bestCorrelation) { bestCorrelation = correlation; bestLag = lag; }
                    }
                    for (int lag = std::max(minLag, bestLag - 7); lag <= std::min(maxLag, bestLag + 7); ++lag) {
                        double xy = 0.0, xx = 0.0, yy = 0.0;
                        for (int k = 0; k < segment; ++k) {
                            const auto a = static_cast<double>(pitch[ch].read(riseReadAge[ch] + static_cast<float>(k)));
                            const auto b = static_cast<double>(pitch[ch].read(riseReadAge[ch] + static_cast<float>(lag + k)));
                            xy += a * b; xx += a * a; yy += b * b;
                        }
                        const auto correlation = xy / std::sqrt(xx * yy + 1.0e-12);
                        if (correlation > bestCorrelation) { bestCorrelation = correlation; bestLag = lag; }
                    }
                    risePeriod[ch] = bestLag;
                    riseFadeAge[ch] = riseReadAge[ch] + static_cast<float>(bestLag);
                    riseFadeTotal[ch] = std::clamp(bestLag / 2, 32, std::max(32, fadeLength));
                    riseFadeRemaining[ch] = riseFadeTotal[ch];
                }
                float out = pitch[ch].read(std::max(2.0f, riseReadAge[ch]));
                if (riseFadeRemaining[ch] > 0) {
                    const auto total = std::max(32, riseFadeTotal[ch]);
                    const auto t = 1.0f - static_cast<float>(riseFadeRemaining[ch]) / static_cast<float>(total);
                    const auto fadeIn = 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * std::clamp(t, 0.0f, 1.0f));
                    out = (1.0f - fadeIn) * out + fadeIn * pitch[ch].read(std::max(2.0f, riseFadeAge[ch]));
                    if (--riseFadeRemaining[ch] == 0) riseReadAge[ch] = riseFadeAge[ch];
                }
                shifted[ch] = out;
            }
            const auto feedback = mode == Algorithm::Velvet ? 0.16f : mode == Algorithm::Punch ? 0.1f : 0.25f;
            for (std::size_t ch = 0; ch < 2; ++ch) {
                float feedbackSource = shifted[ch];
                if (mode == Algorithm::Prism) {
                    // TPT SVF band-limit (150 Hz high-pass into 9 kHz low-pass) plus tanh clamp keeps the cross loop stable.
                    const auto raw = shifted[1 - ch];
                    const auto v3h = raw - riseFbHpIc2[ch];
                    const auto v1h = riseFbHpA1 * riseFbHpIc1[ch] + riseFbHpA2 * v3h;
                    const auto v2h = riseFbHpIc2[ch] + riseFbHpA2 * riseFbHpIc1[ch] + riseFbHpA3 * v3h;
                    riseFbHpIc1[ch] = 2.0f * v1h - riseFbHpIc1[ch];
                    riseFbHpIc2[ch] = 2.0f * v2h - riseFbHpIc2[ch];
                    const auto highPassed = raw - std::numbers::sqrt2_v<float> * v1h - v2h;
                    const auto v3l = highPassed - riseFbLpIc2[ch];
                    const auto v1l = riseFbLpA1 * riseFbLpIc1[ch] + riseFbLpA2 * v3l;
                    const auto v2l = riseFbLpIc2[ch] + riseFbLpA2 * riseFbLpIc1[ch] + riseFbLpA3 * v3l;
                    riseFbLpIc1[ch] = 2.0f * v1l - riseFbLpIc1[ch];
                    riseFbLpIc2[ch] = 2.0f * v2l - riseFbLpIc2[ch];
                    feedbackSource = std::tanh(std::clamp(v2l, -4.0f, 4.0f));
                }
                pitch[ch].write(inputs[ch] + feedbackSource * amount * feedback);
                (ch == 0 ? l : r) += smooth.riseMix * (shifted[ch] - inputs[ch]);
            }
        } else if (!riseCleared) {
            for (auto& delay : pitch) delay.clear();
            riseFadeRemaining = {}; riseFadeTotal = {}; risePeriod = {};
            riseReadAge = {static_cast<float>(rate * 0.04), static_cast<float>(rate * 0.04)};
            riseFadeAge = riseReadAge;
            riseFbHpIc1 = {}; riseFbHpIc2 = {}; riseFbLpIc1 = {}; riseFbLpIc2 = {};
            riseCleared = true;
        }

        if (smooth.width > 0.0f && right) {
            widthCleared = false;
            const auto mid = (l + r) * 0.5f;
            const auto side = (l - r) * 0.5f;
            float decorrelated;
            if (target.algorithm == Algorithm::Velvet) {
                float outputs[] {mid, mid};
                for (std::size_t ch = 0; ch < 2; ++ch)
                    for (auto& allpass : widthDiffusers[ch]) {
                        const auto delaySamples = static_cast<float>(allpass.data.size() - 1) * (0.7f + 0.5f * smooth.spread);
                        const auto delayed = allpass.read(delaySamples);
                        const auto fed = outputs[ch] + 0.5f * delayed;
                        allpass.write(fed);
                        outputs[ch] = delayed - 0.5f * fed;
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
            // 180 Hz LR2 (TPT SVF) on the decorrelated injection only keeps sub-bass mono-compatible.
            const auto v3 = decorrelated - widthHpIc2;
            const auto v1 = widthHpA1 * widthHpIc1 + widthHpA2 * v3;
            const auto v2 = widthHpIc2 + widthHpA2 * widthHpIc1 + widthHpA3 * v3;
            widthHpIc1 = 2.0f * v1 - widthHpIc1;
            widthHpIc2 = 2.0f * v2 - widthHpIc2;
            const auto decorrelatedSafe = decorrelated - std::numbers::sqrt2_v<float> * v1 - v2;
            auto added = side + decorrelatedSafe * 0.65f;
            if (target.algorithm == Algorithm::Punch) {
                widthLow = added + widthPole * (widthLow - added);
                added -= widthLow;
            }
            const auto widened = target.algorithm == Algorithm::Classic
                ? side * (1.0f + smooth.width) + decorrelatedSafe * smooth.width * 0.65f : side + added * smooth.width;
            l = mid + widened; r = mid - widened;
        } else if (!widthCleared) {
            for (auto& delay : widen) delay.clear();
            for (auto& channel : widthDiffusers) for (auto& delay : channel) delay.clear();
            widthHpIc1 = widthHpIc2 = 0.0f; widthLow = widthPhase = 0.0f;
            widthCleared = true;
        }
        const auto gain = std::pow(10.0f, smooth.outputDb / 20.0f);
        float wetOutL = l * gain;
        float wetOutR = r * gain;
        // ADAA-1 tanh runs every sample so its state stays warm when Soft Clip toggles on.
        const auto clippedL = static_cast<float>(Waveshaper::tanhAdaa1(wetOutL, clipPrevX[0], clipPrevIntegral[0]));
        const auto clippedR = static_cast<float>(Waveshaper::tanhAdaa1(wetOutR, clipPrevX[1], clipPrevIntegral[1]));
        if (target.softClip) {
            wetOutL = clippedL;
            wetOutR = clippedR;
        }
        const float mixedL = delayed[0] + smooth.masterMix * (wetOutL - delayed[0]);
        const float mixedR = delayed[1] + smooth.masterMix * (wetOutR - delayed[1]);
        left[i] = active == 0.0f ? delayed[0] : delayed[0] + active * (mixedL - delayed[0]);
        if (right) right[i] = active == 0.0f ? delayed[1] : delayed[1] + active * (mixedR - delayed[1]);
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
    warmupSamples = static_cast<int>(rate * 0.05);
    for (auto& engine : engines) { engine.setSettings(target); engine.prepare(rate); }
    reset();
}
void Rack::reset() noexcept {
    current = 0; transition = 0; warmup = 0;
    algorithms.fill(target.algorithm);
    for (auto& engine : engines) { engine.setSettings(target); engine.reset(); }
}
void Rack::process(float* left, float* right, int samples) noexcept {
    if (left == nullptr || samples <= 0) return;
    if (warmup == 0 && transition == 0 && algorithms[current] == target.algorithm) {
        engines[current].process(left, right, samples);
        return;
    }
    for (int i = 0; i < samples; ++i) {
        const auto next = 1 - current;
        if (target.algorithm == algorithms[current]) {
            warmup = 0; transition = 0;
        } else if (warmup == 0 && transition == 0) {
            algorithms[next] = target.algorithm;
            engines[next].setSettings(target);
            engines[next].reset();
            warmup = warmupSamples;
        } else if (target.algorithm != algorithms[next]) {
            algorithms[next] = target.algorithm;
            engines[next].setSettings(target);
            engines[next].reset();
            warmup = warmupSamples; transition = 0;
        }
        auto l = left[i], r = right ? right[i] : 0.0f;
        engines[current].process(left + i, right ? right + i : nullptr, 1);
        if (warmup > 0) {
            engines[next].process(&l, right ? &r : nullptr, 1);
            if (--warmup == 0) transition = fadeSamples;
        } else if (transition > 0) {
            engines[next].process(&l, right ? &r : nullptr, 1);
            const auto progress = static_cast<float>(fadeSamples - transition + 1) / static_cast<float>(fadeSamples);
            const auto angle = progress * 0.5f * std::numbers::pi_v<float>;
            const auto sameRight = !right || r == right[i];
            if (l != left[i] || !sameRight) {
                left[i] = std::cos(angle) * left[i] + std::sin(angle) * l;
                if (right) right[i] = std::cos(angle) * right[i] + std::sin(angle) * r;
            }
            if (--transition == 0) current = next;
        }
    }
}

}
