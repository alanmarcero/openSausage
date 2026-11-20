#pragma once
#include "Algorithm.h"
#include "Waveshaper.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>

namespace sausage {
enum class SaturationFocus { Neutral, KeepLow, KeepHigh };

struct Settings {
    float fatness = 0.0f;
    float color = 0.0f;
    float outputDb = 0.0f;
    bool bypass = false;
    Algorithm algorithm = Algorithm::Classic;
    float saturationMix = 1.0f;
    float dynamics = 1.0f;
    SaturationFocus focus = SaturationFocus::Neutral;
};

class Core {
public:
    static constexpr int kStage1Pairs = 40;
    static constexpr int kStage2Pairs = 8;
    static constexpr int latency = (2 * kStage1Pairs - 1) + kStage2Pairs;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setSettings(Settings settings) noexcept;
    void process(float* left, float* right, int samples) noexcept;
    float reductionDb() const noexcept { return reduction; }

private:
    template <int Pairs, std::size_t Ring>
    struct HalfbandStage {
        static constexpr std::size_t kMask = Ring - 1;
        std::array<float, Ring> upBuf {}, downEven {}, downOdd {};
        std::size_t upPos = 0, downPos = 0;

        std::array<float, 2> upsample(float input, const std::array<float, Pairs>& taps) noexcept {
            upBuf[upPos] = input;
            float evenSum = 0.0f;
            std::ranges::for_each(std::views::iota(0, Pairs), [&](int k) {
                const float a = upBuf[(upPos - static_cast<std::size_t>(k)) & kMask];
                const float b = upBuf[(upPos - static_cast<std::size_t>(2 * Pairs - 1 - k)) & kMask];
                evenSum += taps[static_cast<std::size_t>(k)] * (a + b);
            });
            const float oddOut = upBuf[(upPos - static_cast<std::size_t>(Pairs - 1)) & kMask];
            upPos = (upPos + 1) & kMask;
            return {2.0f * evenSum, oddOut};
        }

        float downsample(float evenIn, float oddIn, const std::array<float, Pairs>& taps) noexcept {
            downEven[downPos] = evenIn;
            downOdd[downPos] = oddIn;
            float acc = 0.5f * downOdd[(downPos - static_cast<std::size_t>(Pairs)) & kMask];
            std::ranges::for_each(std::views::iota(0, Pairs), [&](int k) {
                const float a = downEven[(downPos - static_cast<std::size_t>(k)) & kMask];
                const float b = downEven[(downPos - static_cast<std::size_t>(2 * Pairs - 1 - k)) & kMask];
                acc += taps[static_cast<std::size_t>(k)] * (a + b);
            });
            downPos = (downPos + 1) & kMask;
            return acc;
        }
    };

    struct SvfCoeffs {
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    };

    struct Svf2 {
        float ic1 = 0.0f, ic2 = 0.0f;
        std::array<float, 2> tick(float input, const SvfCoeffs& c) noexcept {
            const float v3 = input - ic2;
            const float v1 = c.a1 * ic1 + c.a2 * v3;
            const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return {v1, v2};
        }
    };

    struct Channel {
        HalfbandStage<kStage1Pairs, 128> stage1 {};
        HalfbandStage<kStage2Pairs, 32> stage2 {};
        float stage2Align = 0.0f;
        std::array<Waveshaper, 3> shapers {};
        std::array<float, latency> dry {};
        Svf2 keepLowA {}, keepLowB {}, keepHighA {}, keepHighB {};
        Svf2 scHp {};
        float scShelfLow = 0.0f;
        float low = 0.0f, keepHighPostLp = 0.0f, dcInput = 0.0f, dcOutput = 0.0f;
    };

    std::array<Channel, 2> channels {};
    std::array<float, kStage1Pairs> stage1Taps {};
    std::array<float, kStage2Pairs> stage2Taps {};
    SvfCoeffs keepLowCoeffs {}, keepHighCoeffs {}, scHpCoeffs {};
    Settings target {}, current {};
    double rate = 48000.0;
    float smoothing = 0.0f, attack = 0.0f, fastRelease = 0.0f, slowRelease = 0.0f;
    float rmsPole = 0.0f, peakAttack = 0.0f, peakRelease = 0.0f, scShelfPole = 0.0f;
    float tonePole = 0.0f, keepHighPostPole = 0.0f, dcPole = 0.0f;
    float rmsSq = 0.0f, peakEnv = 0.0f, envelope = 0.0f, reduction = 0.0f;
    float focusPosition = 0.0f;
    float active = 1.0f;
    int dryPosition = 0;
};
}
