#pragma once
#include "Algorithm.h"
#include "Waveshaper.h"
#include <array>
#include <cstddef>

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
    static constexpr int latency = 16;
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void setSettings(Settings settings) noexcept;
    void process(float* left, float* right, int samples) noexcept;
    float reductionDb() const noexcept { return reduction; }
private:
    struct Fir {
        std::array<float, 65> history {};
        int position = 0;
        void push(float input) noexcept;
        float tick(float input, const std::array<float, 65>& taps) noexcept;
        float tickUpsampleZeroStuffed(float input, int sub, const std::array<float, 65>& taps) noexcept;
    };
    struct Channel {
        Fir up, down;
        std::array<Waveshaper, 3> shapers;
        std::array<float, latency> dry {};
        float low = 0.0f, dcInput = 0.0f, dcOutput = 0.0f;
    };
    std::array<Channel, 2> channels {};
    std::array<float, 65> coefficients {};
    Settings target {}, current {};
    double rate = 48000.0;
    float smoothing = 0.0f, attack = 0.0f, release = 0.0f;
    float tonePole = 0.0f, dcPole = 0.0f, envelope = 0.0f, reduction = 0.0f;
    float focusPosition = 0.0f;
    float active = 1.0f;
    int dryPosition = 0;
};
}
