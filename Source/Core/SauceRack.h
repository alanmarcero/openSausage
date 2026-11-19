#pragma once
#include "SausageCore.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace sausage {
struct RackSettings {
    float fatness = 0.0f, color = 0.0f;
    float space = 0.0f, size = 0.5f;
    float rise = 0.0f, riseMix = 0.0f;
    float width = 0.0f, spread = 0.5f;
    float outputDb = 0.0f;
    bool sausageOn = true, spaceOn = true, riseOn = true, widthOn = true, bypass = false;
    Algorithm algorithm = Algorithm::Classic;
    float saturationMix = 1.0f, dynamics = 1.0f;
    SaturationFocus focus = SaturationFocus::Neutral;
    float preDelayMs = 0.0f, masterMix = 1.0f;
    bool softClip = false;
    // 0 keeps the Rise knob as a steady pitch. A positive count sweeps from
    // unison up to Rise across that many bars, starting from risePpq.
    int riseBars = 0;
    double risePpq = 0, riseTempo = 120, riseQuartersPerBar = 4;
    bool riseRunning = false;
};

inline double riseSamplePpq(double startPpq, int sampleIndex, double tempo, double sampleRate, bool running) noexcept {
    if (!std::isfinite(startPpq)) return 0.0;
    if (!running || sampleIndex <= 0 || !(sampleRate > 0.0) || !(tempo > 0.0) || !std::isfinite(tempo)) return startPpq;
    return startPpq + static_cast<double>(sampleIndex) * tempo / (60.0 * sampleRate);
}
inline float syncedRise(float rise, int bars, double ppq, double quartersPerBar) noexcept {
    const float amount = std::isfinite(rise) ? std::clamp(rise, 0.0f, 1.0f) : 0.0f;
    if (bars <= 0 || !(quartersPerBar > 0.0) || !std::isfinite(ppq) || !std::isfinite(quartersPerBar)) return amount;
    const double cycle = static_cast<double>(bars) * quartersPerBar;
    if (!(cycle > 0.0)) return amount;
    double wrapped = std::fmod(ppq, cycle);
    if (wrapped < 0.0) wrapped += cycle;
    return amount * static_cast<float>(wrapped / cycle);
}

class RackEngine {
public:
    void setSettings(RackSettings settings) noexcept;
    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float* left, float* right, int samples) noexcept;
    float reductionDb() const noexcept { return core.reductionDb(); }
private:
    struct Delay {
        std::vector<float> data;
        std::size_t position = 0;
        void resize(std::size_t size);
        void clear() noexcept;
        float read(float delay) const noexcept;
        float readFixed(std::size_t delaySamples) const noexcept;
        void write(float sample) noexcept;
    };
    Core core;
    RackSettings target {}, smooth {};
    std::array<Delay, 8> combs;
    std::array<float, 8> damp {};
    std::array<std::array<Delay, 4>, 2> diffusers;
    std::array<Delay, 2> pitch, widen;
    Delay spacePreDelay;
    std::array<std::array<float, Core::latency>, 2> dry {};
    double rate = 48000.0;
    float smoothing = 0.0f, phase = 0.0f, active = 1.0f;
    std::array<float, 2> highpass {}, previous {};
    std::array<float, 4> grainPhases {0.0f, 0.25f, 0.5f, 0.75f};
    std::array<std::array<float, 4>, 2> widthAllpass {};
    float widthLow = 0.0f, widthPhase = 0.0f, widthPole = 0.0f;
    int dryPosition = 0;
    bool spaceCleared = true, riseCleared = true, widthCleared = true;
};

class Rack {
public:
    void setSettings(RackSettings settings) noexcept;
    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float* left, float* right, int samples) noexcept;
    float reductionDb() const noexcept { return engines[current].reductionDb(); }
private:
    std::array<RackEngine, 2> engines;
    std::array<Algorithm, 2> algorithms {};
    RackSettings target {};
    std::size_t current = 0;
    int transition = 0, fadeSamples = 960;
};
}
