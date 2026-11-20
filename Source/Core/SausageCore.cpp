#include "SausageCore.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>

namespace sausage {
namespace {
constexpr float kSqrt2 = std::numbers::sqrt2_v<float>;

float finite(float value, float fallback = 0.0f) noexcept { return std::isfinite(value) ? value : fallback; }
float pole(double seconds, double rate) noexcept { return static_cast<float>(std::exp(-1.0 / (seconds * rate))); }
float follow(float value, float goal, float coefficient) noexcept {
    const auto result = goal + coefficient * (value - goal);
    return std::abs(result - goal) < 1.0e-6f ? goal : result;
}

double besselI0(double x) noexcept {
    double sum = 1.0, term = 1.0;
    const double halfSq = 0.25 * x * x;
    std::ranges::for_each(std::views::iota(1, 26), [&](int k) {
        term *= halfSq / (static_cast<double>(k) * static_cast<double>(k));
        sum += term;
    });
    return sum;
}
}

void Core::setSettings(Settings settings) noexcept {
    target = {std::clamp(finite(settings.fatness), 0.0f, 1.0f),
              std::clamp(finite(settings.color), 0.0f, 1.0f),
              std::clamp(finite(settings.outputDb), -24.0f, 12.0f), settings.bypass, validAlgorithm(settings.algorithm),
              std::clamp(finite(settings.saturationMix, 1.0f), 0.0f, 1.0f),
              std::clamp(finite(settings.dynamics, 1.0f), 0.0f, 1.0f),
              settings.focus >= SaturationFocus::Neutral && settings.focus <= SaturationFocus::KeepHigh
                  ? settings.focus : SaturationFocus::Neutral};
}

void Core::prepare(double sampleRate) noexcept {
    rate = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 48000.0;
    const double osRate = rate * 4.0;
    smoothing = pole(0.01, rate);
    focusPosition = static_cast<float>(target.focus);

    attack = pole(0.006, rate);
    fastRelease = pole(0.035, rate);
    slowRelease = pole(0.18, rate);
    rmsPole = pole(0.015, rate);
    peakAttack = pole(0.001, rate);
    peakRelease = pole(0.05, rate);
    scShelfPole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * std::min(1500.0, 0.4 * rate) / rate));

    tonePole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 1200.0 / osRate));
    keepHighPostPole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * std::min(3500.0, 0.42 * osRate) / osRate));
    dcPole = static_cast<float>(std::exp(-2.0 * std::numbers::pi * 8.0 / osRate));

    const auto makeButterworth = [](double cutoff, double sr) noexcept -> SvfCoeffs {
        const float g = static_cast<float>(std::tan(std::numbers::pi * std::min(cutoff, 0.42 * sr) / sr));
        const float a1 = 1.0f / (1.0f + g * (g + kSqrt2));
        return {a1, g * a1, g * g * a1};
    };
    keepLowCoeffs = makeButterworth(250.0, osRate);
    keepHighCoeffs = makeButterworth(2500.0, osRate);
    scHpCoeffs = makeButterworth(90.0, rate);

    // Build normalized Kaiser-windowed halfband outer tap pairs so DC gain is 1 and Nyquist gain is 0.
    constexpr double kBeta = 10.0;
    const double invI0 = 1.0 / besselI0(kBeta);
    double sum1 = 0.0;
    std::ranges::for_each(std::views::iota(0, kStage1Pairs), [&](int k) {
        constexpr double center = static_cast<double>(2 * kStage1Pairs - 1);
        const double d = static_cast<double>(2 * k) - center;
        const double sinc = std::sin(0.5 * std::numbers::pi * d) / (std::numbers::pi * d);
        const double norm = d / center;
        const double win = besselI0(kBeta * std::sqrt(std::max(0.0, 1.0 - norm * norm))) * invI0;
        stage1Taps[static_cast<std::size_t>(k)] = static_cast<float>(sinc * win);
        sum1 += stage1Taps[static_cast<std::size_t>(k)];
    });
    const float scale1 = static_cast<float>(0.25 / sum1);
    std::ranges::for_each(stage1Taps, [scale1](float& tap) { tap *= scale1; });

    double sum2 = 0.0;
    std::ranges::for_each(std::views::iota(0, kStage2Pairs), [&](int k) {
        constexpr double center = static_cast<double>(2 * kStage2Pairs - 1);
        const double d = static_cast<double>(2 * k) - center;
        const double sinc = std::sin(0.5 * std::numbers::pi * d) / (std::numbers::pi * d);
        const double norm = d / center;
        const double win = besselI0(kBeta * std::sqrt(std::max(0.0, 1.0 - norm * norm))) * invI0;
        stage2Taps[static_cast<std::size_t>(k)] = static_cast<float>(sinc * win);
        sum2 += stage2Taps[static_cast<std::size_t>(k)];
    });
    const float scale2 = static_cast<float>(0.25 / sum2);
    std::ranges::for_each(stage2Taps, [scale2](float& tap) { tap *= scale2; });

    reset();
}

void Core::reset() noexcept {
    channels = {};
    rmsSq = peakEnv = envelope = reduction = 0.0f;
    dryPosition = 0;
    current = target;
    focusPosition = static_cast<float>(target.focus);
    active = target.bypass ? 0.0f : 1.0f;
}

void Core::process(float* left, float* right, int samples) noexcept {
    if (left == nullptr || samples <= 0) return;
    const int numChannels = right != nullptr ? 2 : 1;
    float lastCompression = 1.0f;

    std::ranges::for_each(std::views::iota(0, samples), [&](int i) {
        current.fatness = follow(current.fatness, target.fatness, smoothing);
        current.color = follow(current.color, target.color, smoothing);
        current.outputDb = follow(current.outputDb, target.outputDb, smoothing);
        current.saturationMix = follow(current.saturationMix, target.saturationMix, smoothing);
        current.dynamics = follow(current.dynamics, target.dynamics, smoothing);
        focusPosition = follow(focusPosition, static_cast<float>(target.focus), smoothing);
        active = follow(active, target.bypass ? 0.0f : 1.0f, smoothing);

        const float in0 = std::clamp(finite(left[i]), -32.0f, 32.0f);
        float in1 = 0.0f;
        if (right != nullptr) in1 = std::clamp(finite(right[i]), -32.0f, 32.0f);
        const std::array<float, 2> input {in0, in1};

        if (active == 0.0f && target.bypass) {
            std::ranges::for_each(std::views::iota(0, numChannels), [&](int ch) {
                auto& state = channels[static_cast<std::size_t>(ch)];
                const float dry = state.dry[static_cast<std::size_t>(dryPosition)];
                state.dry[static_cast<std::size_t>(dryPosition)] = input[static_cast<std::size_t>(ch)];
                (ch == 0 ? left : right)[i] = dry;
            });
            dryPosition = (dryPosition + 1) % latency;
            return;
        }

        // Weight sidechain with 90 Hz Butterworth HPF and 1.5 kHz K-shelf so sub-bass does not pump highs.
        std::array<float, 2> weighted {};
        std::ranges::for_each(std::views::iota(0, numChannels), [&](int ch) {
            auto& state = channels[static_cast<std::size_t>(ch)];
            const float x = input[static_cast<std::size_t>(ch)];
            const auto svf = state.scHp.tick(x, scHpCoeffs);
            const float hp90 = x - kSqrt2 * svf[0] - svf[1];
            state.scShelfLow = hp90 + scShelfPole * (state.scShelfLow - hp90);
            weighted[static_cast<std::size_t>(ch)] = hp90 + 0.45f * (hp90 - state.scShelfLow);
        });

        const float sq0 = weighted[0] * weighted[0];
        const float sq1 = weighted[1] * weighted[1];
        const float stereoPower = std::max(0.5f * (sq0 + sq1), 0.75f * std::max(sq0, sq1));
        const float linkedPower = right != nullptr ? stereoPower : sq0;
        rmsSq = follow(rmsSq, linkedPower, rmsPole);
        const float rmsLevel = std::sqrt(2.0f * std::max(0.0f, rmsSq));
        const float instPeak = std::max(std::abs(weighted[0]), std::abs(weighted[1]));
        peakEnv = follow(peakEnv, instPeak, instPeak > peakEnv ? peakAttack : peakRelease);
        const float crest = peakEnv / std::max(rmsLevel, 1.0e-5f);
        const float crestBlend = std::clamp((crest - 1.2f) / 3.5f, 0.0f, 1.0f);
        const float autoRelease = slowRelease + crestBlend * (fastRelease - slowRelease);
        envelope = follow(envelope, rmsLevel, rmsLevel > envelope ? attack : autoRelease);

        float threshold = 1.0f;
        if (current.fatness > 0.0f) threshold = std::pow(10.0f, -14.0f * current.fatness / 20.0f);
        float requestedCompression = 1.0f;
        if (current.fatness > 0.0f && envelope > threshold) {
            requestedCompression = std::pow(threshold / envelope, 1.0f - 1.0f / (1.0f + 7.0f * current.fatness));
        }
        const float compression = 1.0f + current.dynamics * (requestedCompression - 1.0f);
        lastCompression = compression;

        const float drive = 1.0f + 24.0f * current.fatness;
        const float legacyBlend = std::min(1.0f, current.fatness * 5.0f + current.color * 3.0f);
        const float blend = legacyBlend * current.saturationMix;
        const float neutralWeight = std::max(0.0f, 1.0f - focusPosition);
        const float keepLowWeight = focusPosition < 1.0f ? focusPosition : 2.0f - focusPosition;
        const float keepHighWeight = std::max(0.0f, focusPosition - 1.0f);
        float gain = 1.0f;
        if (current.outputDb != 0.0f) gain = std::pow(10.0f, current.outputDb / 20.0f);
        const float fullGain = compression * drive;

        std::ranges::for_each(std::views::iota(0, numChannels), [&](int ch) {
            auto& state = channels[static_cast<std::size_t>(ch)];
            const float dry = state.dry[static_cast<std::size_t>(dryPosition)];
            const float x = input[static_cast<std::size_t>(ch)];
            state.dry[static_cast<std::size_t>(dryPosition)] = x;

            const auto midUp = state.stage1.upsample(x, stage1Taps);
            const auto up01 = state.stage2.upsample(midUp[0], stage2Taps);
            const auto up23 = state.stage2.upsample(midUp[1], stage2Taps);
            std::array<float, 4> os {up01[0], up01[1], up23[0], up23[1]};

            std::ranges::for_each(os, [&](float& up) {
                state.low = up + tonePole * (state.low - up);
                const float highTone = up - state.low;
                const float colored = up + current.color * 2.5f * highTone;

                // Two-SVF TPT Linkwitz-Riley 4th-order split: LP4 + HP4 equals the 2nd-order allpass ap2.
                const auto klA = state.keepLowA.tick(colored, keepLowCoeffs);
                const float klAp2 = colored - 2.0f * kSqrt2 * klA[0];
                const float klLp4 = state.keepLowB.tick(klA[1], keepLowCoeffs)[1];
                const float klHp4 = klAp2 - klLp4;

                const auto khA = state.keepHighA.tick(colored, keepHighCoeffs);
                const float khAp2 = colored - 2.0f * kSqrt2 * khA[0];
                const float khLp4 = state.keepHighB.tick(khA[1], keepHighCoeffs)[1];
                const float khHp4 = khAp2 - khLp4;

                float shaped = 0.0f;
                if (neutralWeight > 0.0f) {
                    shaped += neutralWeight * state.shapers[0].process(colored * fullGain, target.algorithm);
                }
                if (keepLowWeight > 0.0f) {
                    shaped += keepLowWeight * (klLp4 + state.shapers[1].process(klHp4 * fullGain, target.algorithm));
                }
                if (keepHighWeight > 0.0f) {
                    const float lowShaped = state.shapers[2].process(khLp4 * fullGain, target.algorithm);
                    const float distResidual = lowShaped - khLp4;
                    state.keepHighPostLp = distResidual + keepHighPostPole * (state.keepHighPostLp - distResidual);
                    shaped += keepHighWeight * (khHp4 + khLp4 + state.keepHighPostLp);
                }

                const float saturated = colored + current.fatness * (shaped - colored);
                const float dc = saturated - state.dcInput + dcPole * state.dcOutput;
                state.dcInput = saturated;
                state.dcOutput = dc;
                up = dc;
            });

            const float down0 = state.stage2.downsample(os[0], os[1], stage2Taps);
            const float down1 = state.stage2.downsample(os[2], os[3], stage2Taps);
            const float wet = state.stage1.downsample(state.stage2Align, down0, stage1Taps);
            state.stage2Align = down1;

            const float processed = (dry + blend * (wet - dry)) * gain;
            (ch == 0 ? left : right)[i] = active == 0.0f ? dry : dry + active * (processed - dry);
        });
        dryPosition = (dryPosition + 1) % latency;
    });

    float nextReduction = -20.0f * std::log10(std::max(lastCompression, 1.0e-8f));
    if (active == 0.0f && target.bypass) nextReduction = 0.0f;
    reduction = nextReduction;
}
}
