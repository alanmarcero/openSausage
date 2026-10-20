#include "SausageCore.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

namespace {
constexpr int count = 24000;
std::vector<float> sine(float frequency = 440.0f, float level = 0.25f, int size = count) {
    std::vector<float> data(static_cast<std::size_t>(size));
    for (int i = 0; i < size; ++i)
        data[static_cast<std::size_t>(i)] = level * std::sin(2.0f * std::numbers::pi_v<float> * frequency * static_cast<float>(i) / 48000.0f);
    return data;
}
float rms(const std::vector<float>& data) {
    double sum = 0.0;
    for (std::size_t i = data.size() / 2; i < data.size(); ++i) sum += data[i] * data[i];
    return static_cast<float>(std::sqrt(sum / static_cast<double>(data.size() - data.size() / 2)));
}
std::vector<float> render(sausage::Settings settings, float frequency = 440.0f) {
    sausage::Core core;
    core.setSettings(settings);
    core.prepare(48000.0);
    auto data = sine(frequency);
    core.process(data.data(), nullptr, static_cast<int>(data.size()));
    return data;
}
}

TEST_CASE("Neutral audio is unchanged apart from the reported latency") {
    const auto input = sine();
    const auto output = render({});
    for (int i = 0; i < sausage::Core::latency; ++i) REQUIRE(output[static_cast<std::size_t>(i)] == 0.0f);
    for (std::size_t i = sausage::Core::latency; i < input.size(); ++i)
        REQUIRE(output[i] == input[i - sausage::Core::latency]);
}

TEST_CASE("Bypass ignores processing and output trim but retains latency") {
    REQUIRE(render({1.0f, 1.0f, 12.0f, true}) == render({}));
}

TEST_CASE("Output trim applies the requested decibel gain") {
    REQUIRE(rms(render({0.0f, 0.0f, -6.0206f})) == Catch::Approx(0.0883883f).margin(0.0002f));
}

TEST_CASE("Fatness increases density and creates harmonics") {
    const auto clean = render({});
    const auto driven = render({0.8f, 0.0f, 0.0f});
    REQUIRE(rms(driven) > rms(clean) * 1.5f);
    double real = 0.0, imaginary = 0.0;
    for (std::size_t i = 12000; i < driven.size(); ++i) {
        const auto phase = 2.0 * std::numbers::pi * 1320.0 * static_cast<double>(i) / 48000.0;
        real += driven[i] * std::cos(phase);
        imaginary += driven[i] * std::sin(phase);
    }
    REQUIRE(std::hypot(real, imaginary) / 12000.0 > 0.01);
}

TEST_CASE("Color emphasizes highs more than bass") {
    const auto lowBoost = rms(render({0.0f, 1.0f}, 100.0f)) / rms(render({}, 100.0f));
    const auto highBoost = rms(render({0.0f, 1.0f}, 6000.0f)) / rms(render({}, 6000.0f));
    REQUIRE(highBoost > lowBoost * 1.5f);
}

TEST_CASE("Silence remains silent even at maximum settings") {
    sausage::Core core;
    core.setSettings({1.0f, 1.0f, 12.0f});
    core.prepare(96000.0);
    std::vector<float> data(8192, 0.0f);
    core.process(data.data(), nullptr, static_cast<int>(data.size()));
    for (float value : data) REQUIRE(value == 0.0f);
}

TEST_CASE("Stereo processing does not crossfeed into a silent channel") {
    sausage::Core core;
    core.setSettings({0.9f, 0.5f});
    core.prepare(48000.0);
    auto left = sine();
    std::vector<float> right(left.size(), 0.0f);
    core.process(left.data(), right.data(), static_cast<int>(left.size()));
    REQUIRE(rms(left) > 0.2f);
    REQUIRE(rms(right) == 0.0f);
}

TEST_CASE("Stereo envelope links the compressor gain") {
    sausage::Core core;
    core.setSettings({0.4f, 0.0f});
    core.prepare(48000.0);
    auto left = sine(100.0f, 4.0f);
    auto right = sine(440.0f, 0.05f);
    const auto monoInput = right;
    core.process(left.data(), right.data(), static_cast<int>(right.size()));
    sausage::Core mono;
    mono.setSettings({0.4f, 0.0f});
    mono.prepare(48000.0);
    auto solo = monoInput;
    mono.process(solo.data(), nullptr, static_cast<int>(solo.size()));
    REQUIRE(rms(right) < rms(solo) * 0.8f);
}

TEST_CASE("Processing is independent of host block size") {
    const auto whole = render({0.72f, 0.38f, -4.0f});
    auto split = sine();
    sausage::Core core;
    core.setSettings({0.72f, 0.38f, -4.0f});
    core.prepare(48000.0);
    for (std::size_t offset = 0; offset < split.size(); offset += 37) {
        const auto length = std::min<std::size_t>(37, split.size() - offset);
        core.process(split.data() + offset, nullptr, static_cast<int>(length));
    }
    REQUIRE(split == whole);
}

TEST_CASE("Reset clears delayed audio and filter history") {
    sausage::Core core;
    core.setSettings({0.7f, 0.8f});
    core.prepare(48000.0);
    auto data = sine();
    core.process(data.data(), nullptr, static_cast<int>(data.size()));
    core.reset();
    std::fill(data.begin(), data.end(), 0.0f);
    core.process(data.data(), nullptr, static_cast<int>(data.size()));
    REQUIRE(rms(data) == 0.0f);
    REQUIRE(data.front() == 0.0f);
}

TEST_CASE("Nonfinite samples cannot poison the filter state") {
    sausage::Core core;
    core.setSettings({1.0f, 1.0f, 12.0f});
    core.prepare(48000.0);
    auto data = sine();
    data[20] = std::numeric_limits<float>::quiet_NaN();
    data[40] = std::numeric_limits<float>::infinity();
    core.process(data.data(), nullptr, static_cast<int>(data.size()));
    for (float value : data) REQUIRE(std::isfinite(value));
    REQUIRE(rms(data) > 0.1f);
}

TEST_CASE("Extreme settings remain finite across supported sample rates") {
    for (double rate : {8000.0, 22050.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        sausage::Core core;
        core.setSettings({10.0f, -5.0f, 100.0f});
        core.prepare(rate);
        auto data = sine(1000.0f, 100.0f, 4096);
        core.process(data.data(), nullptr, static_cast<int>(data.size()));
        for (float value : data) { REQUIRE(std::isfinite(value)); REQUIRE(std::abs(value) < 8.0f); }
    }
}

TEST_CASE("Output automation ramps instead of stepping") {
    sausage::Core core;
    core.prepare(48000.0);
    std::vector<float> data(4096, 0.25f);
    core.process(data.data(), nullptr, static_cast<int>(data.size()));
    core.setSettings({0.0f, 0.0f, -24.0f});
    std::fill(data.begin(), data.end(), 0.25f);
    core.process(data.data(), nullptr, static_cast<int>(data.size()));
    REQUIRE(data.front() > 0.24f);
    REQUIRE(data.back() < 0.017f);
    for (std::size_t i = 1; i < data.size(); ++i) REQUIRE(std::abs(data[i] - data[i - 1]) < 0.002f);
}

TEST_CASE("Empty blocks and disconnected input are harmless") {
    sausage::Core core;
    core.prepare(48000.0);
    core.process(nullptr, nullptr, 0);
    core.process(nullptr, nullptr, 64);
    float sample = 0.5f;
    core.process(&sample, nullptr, 0);
    REQUIRE(sample == 0.5f);
}
