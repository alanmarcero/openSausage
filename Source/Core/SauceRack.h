#pragma once
#include "SausageCore.h"
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
};

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
        void write(float sample) noexcept;
    };
    Core core;
    RackSettings target {}, smooth {};
    std::array<Delay, 8> combs;
    std::array<float, 8> damp {};
    std::array<std::array<Delay, 4>, 2> diffusers;
    std::array<Delay, 2> pitch, widen;
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
