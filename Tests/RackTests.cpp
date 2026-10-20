#include "SauceRack.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace {
std::vector<float> signal(int size = 48000) {
    std::vector<float> result(static_cast<std::size_t>(size));
    for (int i = 0; i < size; ++i) result[static_cast<std::size_t>(i)] = 0.2f * std::sin(2.0f * std::numbers::pi_v<float> * 440.0f * static_cast<float>(i) / 48000.0f);
    return result;
}
double energy(const std::vector<float>& data, std::size_t start = 0) {
    double result = 0.0;
    for (std::size_t i = start; i < data.size(); ++i) result += data[i] * data[i];
    return result;
}
}

TEST_CASE("Each module has a full independent bypass") {
    for (int module = 0; module < 4; ++module) {
        sausage::RackSettings settings;
        if (module == 0) { settings.fatness = 1.0f; settings.color = 1.0f; settings.sausageOn = false; }
        if (module == 1) { settings.space = 1.0f; settings.spaceOn = false; }
        if (module == 2) { settings.rise = 1.0f; settings.riseMix = 1.0f; settings.riseOn = false; }
        if (module == 3) { settings.width = 1.0f; settings.widthOn = false; }
        sausage::Rack rack;
        rack.setSettings(settings);
        rack.prepare(48000.0);
        auto left = signal(), right = left;
        const auto input = left;
        rack.process(left.data(), right.data(), static_cast<int>(left.size()));
        for (std::size_t i = sausage::Core::latency; i < left.size(); ++i) {
            REQUIRE(left[i] == input[i - sausage::Core::latency]);
            REQUIRE(right[i] == left[i]);
        }
    }
}

TEST_CASE("Space creates a stereo reverb tail") {
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.space = 1.0f;
    rack.setSettings(settings);
    rack.prepare(48000.0);
    std::vector<float> left(48000), right(48000);
    left[0] = right[0] = 1.0f;
    rack.process(left.data(), right.data(), 48000);
    REQUIRE(energy(left, 2000) > 0.001);
    REQUIRE(left != right);
}

TEST_CASE("Space diffuses an impulse into a dense tail") {
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.space = 1.0f;
    rack.setSettings(settings);
    rack.prepare(48000.0);
    std::vector<float> left(48000), right(48000);
    left[0] = right[0] = 1.0f;
    rack.process(left.data(), right.data(), 48000);

    // Undiffused combs leave energy on about 1% of samples from 50 to 150 ms. Diffusion fills about half.
    const auto window = std::vector<float>(left.begin() + 2400, left.begin() + 7200);
    float loudest = 0.0f;
    for (const auto sample : window) loudest = std::max(loudest, std::abs(sample));
    const auto audible = std::count_if(window.begin(), window.end(), [&](float sample) { return std::abs(sample) > loudest * 0.01f; });
    REQUIRE(audible > static_cast<long>(window.size()) / 4);
}

TEST_CASE("Space bypass removes an existing reverb tail completely") {
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.space = 1.0f;
    rack.setSettings(settings);
    rack.prepare(48000.0);
    auto left = signal(), right = left;
    rack.process(left.data(), right.data(), 48000);
    settings.spaceOn = false;
    rack.setSettings(settings);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    rack.process(left.data(), right.data(), 48000);
    REQUIRE(energy(left, 12000) == 0.0);
    REQUIRE(energy(right, 12000) == 0.0);
}

TEST_CASE("Width creates side information from mono without changing its mono sum") {
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.width = 0.9f;
    rack.setSettings(settings);
    rack.prepare(48000.0);
    auto left = signal(), right = left;
    const auto input = left;
    rack.process(left.data(), right.data(), 48000);
    double sideEnergy = 0.0;
    for (std::size_t i = 10000; i < left.size(); ++i) {
        sideEnergy += std::pow(left[i] - right[i], 2);
        REQUIRE(std::abs((left[i] + right[i]) * 0.5f - input[i - sausage::Core::latency]) < 1.0e-6f);
    }
    REQUIRE(sideEnergy > 1.0);
}

TEST_CASE("Rise shifts the wet tone up an octave at maximum amount") {
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.rise = settings.riseMix = 1.0f;
    rack.setSettings(settings);
    rack.prepare(48000.0);
    auto left = signal(), right = left;
    rack.process(left.data(), right.data(), 48000);
    int crossings = 0;
    for (std::size_t i = 24001; i < left.size(); ++i)
        if (left[i - 1] <= 0.0f && left[i] > 0.0f) ++crossings;
    REQUIRE(crossings > 390);
    REQUIRE(crossings < 490);
    REQUIRE(energy(left, 24000) > 1.0);
}

TEST_CASE("Master bypass passes delayed dry audio with every effect cranked") {
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.fatness = settings.color = settings.space = settings.rise = settings.riseMix = settings.width = 1.0f;
    settings.outputDb = 12.0f;
    settings.bypass = true;
    rack.setSettings(settings);
    rack.prepare(48000.0);
    auto left = signal(), right = left;
    const auto input = left;
    rack.process(left.data(), right.data(), 48000);
    for (std::size_t i = sausage::Core::latency; i < left.size(); ++i) REQUIRE(left[i] == input[i - sausage::Core::latency]);
}

TEST_CASE("All four effects can run together without nonfinite output") {
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.fatness = settings.color = settings.space = settings.rise = settings.riseMix = settings.width = 1.0f;
    rack.setSettings(settings);
    rack.prepare(48000.0);
    auto left = signal(), right = left;
    const auto input = left;
    rack.process(left.data(), right.data(), 48000);
    REQUIRE(left != input);
    for (float value : left) { REQUIRE(std::isfinite(value)); REQUIRE(std::abs(value) < 8.0f); }
}

TEST_CASE("Turning off each active module clears its delayed sound") {
    for (int module = 0; module < 4; ++module) {
        sausage::Rack rack;
        sausage::RackSettings settings;
        if (module == 0) settings.fatness = settings.color = 1.0f;
        if (module == 1) settings.space = 1.0f;
        if (module == 2) settings.rise = settings.riseMix = 1.0f;
        if (module == 3) settings.width = 1.0f;
        rack.setSettings(settings);
        rack.prepare(48000.0);
        auto left = signal(), right = left;
        rack.process(left.data(), right.data(), 48000);
        REQUIRE(energy(left, 12000) > 1.0);
        if (module == 0) settings.sausageOn = false;
        if (module == 1) settings.spaceOn = false;
        if (module == 2) settings.riseOn = false;
        if (module == 3) settings.widthOn = false;
        rack.setSettings(settings);
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        rack.process(left.data(), right.data(), 48000);
        REQUIRE(energy(left, 12000) == 0.0);
        REQUIRE(energy(right, 12000) == 0.0);
    }
}
