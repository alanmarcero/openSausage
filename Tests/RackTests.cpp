#include "SauceRack.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <limits>
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <ranges>
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
        settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
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
    settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
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
    settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
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
    settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
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
    settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
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
    settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
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
    settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
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
    settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
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
        settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
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

TEST_CASE("Global algorithms change every module at the same knob settings") {
    const auto module = GENERATE(0, 1, 2, 3);
    std::array<std::vector<float>, 4> renders;
    for (int mode = 0; mode < 4; ++mode) {
        sausage::RackSettings settings;
        settings.algorithm = static_cast<sausage::Algorithm>(mode);
        if (module == 0) settings.fatness = 0.75f;
        if (module == 1) settings.space = 0.8f;
        if (module == 2) { settings.rise = 0.8f; settings.riseMix = 0.8f; }
        if (module == 3) settings.width = 0.8f;
        sausage::Rack rack;
        rack.setSettings(settings); rack.prepare(48000.0);
        auto left = signal(12000), right = left;
        rack.process(left.data(), right.data(), static_cast<int>(left.size()));
        renders[static_cast<std::size_t>(mode)] = left;
    }
    for (std::size_t a = 0; a < renders.size(); ++a)
        for (std::size_t b = a + 1; b < renders.size(); ++b) {
            double difference = 0.0;
            for (std::size_t i = 4000; i < renders[a].size(); ++i)
                difference += std::pow(renders[a][i] - renders[b][i], 2);
            INFO("module " << module << ", algorithms " << a << " and " << b);
            REQUIRE(difference > 0.01);
        }
}

TEST_CASE("Algorithm automation is block independent and preserves neutral or bypassed audio") {
    const auto bypassed = GENERATE(false, true);
    const auto wet = GENERATE(false, true);
    const auto input = signal(24000);
    std::array<std::vector<float>, 2> renders;
    for (int run = 0; run < 2; ++run) {
        sausage::Rack rack;
        sausage::RackSettings settings;
        if (wet) settings.fatness = settings.space = settings.rise = settings.riseMix = settings.width = 0.75f;
        settings.bypass = bypassed;
        rack.setSettings(settings); rack.prepare(48000.0);
        auto left = input, right = input;
        const int block = run == 0 ? 6000 : 37;
        for (int start = 0; start < 24000; start += 6000) {
            settings.algorithm = static_cast<sausage::Algorithm>(start / 6000);
            rack.setSettings(settings);
            for (int offset = start; offset < start + 6000; offset += block) {
                rack.setSettings(settings);
                rack.process(left.data() + offset, right.data() + offset, std::min(block, start + 6000 - offset));
            }
        }
        renders[static_cast<std::size_t>(run)] = left;
        if (bypassed || !wet)
            for (std::size_t i = sausage::Core::latency; i < left.size(); ++i)
                REQUIRE(left[i] == input[i - sausage::Core::latency]);
    }
    REQUIRE(renders[0] == renders[1]);
}

TEST_CASE("Every algorithm resets to silence and tolerates extreme input and sample rates") {
    sausage::RackSettings settings;
    settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
    settings.fatness = settings.color = settings.space = settings.size = settings.rise = settings.riseMix = settings.width = settings.spread = 1.0f;
    for (const auto rate : {8000.0, 44100.0, 48000.0, 96000.0, 192000.0, 384000.0}) {
        sausage::Rack rack;
        rack.setSettings(settings); rack.prepare(rate);
        auto left = signal(8192), right = left;
        left[20] = std::numeric_limits<float>::quiet_NaN();
        right[40] = std::numeric_limits<float>::infinity();
        left[60] = 100.0f;
        rack.process(left.data(), right.data(), static_cast<int>(left.size()));
        for (std::size_t i = 0; i < left.size(); ++i) {
            REQUIRE(std::isfinite(left[i])); REQUIRE(std::isfinite(right[i]));
            REQUIRE(std::abs(left[i]) < 8.0f); REQUIRE(std::abs(right[i]) < 8.0f);
        }
        rack.reset();
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        rack.process(left.data(), right.data(), static_cast<int>(left.size()));
        REQUIRE(energy(left) == 0.0); REQUIRE(energy(right) == 0.0);
    }
}

TEST_CASE("Rapid algorithm changes settle on the latest choice in mono and stereo") {
    const auto stereo = GENERATE(false, true);
    sausage::Rack rack, reference;
    sausage::RackSettings settings;
    settings.fatness = 0.75f;
    rack.setSettings(settings); rack.prepare(48000.0);
    settings.algorithm = sausage::Algorithm::Prism;
    reference.setSettings(settings); reference.prepare(48000.0);
    auto left = signal(), right = left, expected = left, expectedRight = left;
    constexpr sausage::Algorithm requests[] {sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Classic, sausage::Algorithm::Prism};
    for (std::size_t i = 0; i < 4; ++i) {
        settings.algorithm = requests[i];
        rack.setSettings(settings);
        rack.process(left.data() + i * 37, stereo ? right.data() + i * 37 : nullptr, 37);
    }
    rack.process(left.data() + 148, stereo ? right.data() + 148 : nullptr, 48000 - 148);
    reference.process(expected.data(), stereo ? expectedRight.data() : nullptr, 48000);
    for (std::size_t i = 40000; i < left.size(); ++i) {
        REQUIRE(std::abs(left[i] - expected[i]) < 1.0e-6f);
        if (stereo) REQUIRE(std::abs(right[i] - expectedRight[i]) < 1.0e-6f);
    }
}

TEST_CASE("Rise length math wraps to the bar and free mode ignores the clock") {
    REQUIRE(sausage::syncedRise(0.4f, 0, 99.0, 4.0) == 0.4f);
    REQUIRE(sausage::syncedRise(0.4f, 4, std::numeric_limits<double>::quiet_NaN(), 4.0) == 0.4f);
    REQUIRE(sausage::syncedRise(1.0f, 4, 8.0, 4.0) == 0.5f);
    REQUIRE(std::abs(sausage::syncedRise(1.0f, 4, -2.0, 4.0) - 0.875f) < 1.0e-6f);
    REQUIRE(sausage::syncedRise(1.0f, 1, 3.0, 3.0) == 0.0f);
    REQUIRE(std::abs(sausage::syncedRise(1.0f, 1, 2.999, 3.0) - 0.999666f) < 1.0e-4f);
    REQUIRE(std::abs(sausage::riseSamplePpq(1.0, 48000, 120.0, 48000.0, true) - 3.0) < 1.0e-9);
    REQUIRE(sausage::riseSamplePpq(1.0, 10, 120.0, 48000.0, false) == 1.0);
}

int riseCrossings(sausage::RackSettings settings, double ppq) {
    settings.rise = settings.riseMix = 1.0f;
    settings.risePpq = ppq;
    settings.riseRunning = false;
    sausage::Rack rack;
    rack.setSettings(settings);
    rack.prepare(48000.0);
    auto left = signal(), right = left;
    rack.process(left.data(), right.data(), static_cast<int>(left.size()));
    int crossings = 0;
    for (std::size_t i = 24001; i < left.size(); ++i)
        if (left[i - 1] <= 0.0f && left[i] > 0.0f) ++crossings;
    return crossings;
}

TEST_CASE("Synced rise starts near the input pitch and finishes near an octave") {
    sausage::RackSettings settings;
    settings.algorithm = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
    settings.riseBars = 4;
    settings.riseQuartersPerBar = 4;
    const auto start = riseCrossings(settings, 0.0);
    const auto end = riseCrossings(settings, 15.99);
    const auto steady = riseCrossings(sausage::RackSettings {}, 15.99);
    REQUIRE(end > start * 3 / 2);
    REQUIRE(std::abs(steady - end) < std::max(40, end / 5));
}

TEST_CASE("Free rise mode ignores tempo and playhead position") {
    sausage::RackSettings timed;
    timed.algorithm = sausage::Algorithm::Classic;
    timed.rise = 0.65f;
    timed.riseMix = 0.8f;
    timed.riseBars = 0;
    timed.risePpq = 12.5;
    timed.riseTempo = 174.0;
    timed.riseRunning = true;
    timed.riseQuartersPerBar = 3.0;
    auto plain = timed;
    plain.risePpq = 0.0;
    plain.riseTempo = 120.0;
    plain.riseRunning = false;
    plain.riseQuartersPerBar = 4.0;
    sausage::Rack timedRack, plainRack;
    timedRack.setSettings(timed);
    plainRack.setSettings(plain);
    timedRack.prepare(48000.0);
    plainRack.prepare(48000.0);
    auto left = signal(), right = left, otherLeft = left, otherRight = left;
    timedRack.process(left.data(), right.data(), static_cast<int>(left.size()));
    plainRack.process(otherLeft.data(), otherRight.data(), static_cast<int>(otherLeft.size()));
    REQUIRE(left == otherLeft);
    REQUIRE(right == otherRight);
}

TEST_CASE("Space FDN preserves true stereo input localization and reverbs pure side signals") {
    sausage::Rack antiPhaseRack;
    sausage::RackSettings settings;
    settings.space = 1.0f;
    settings.size = 0.75f;
    antiPhaseRack.setSettings(settings);
    antiPhaseRack.prepare(48000.0);
    std::vector<float> sideL(24000, 0.0f), sideR(24000, 0.0f);
    sideL[0] = 1.0f;
    sideR[0] = -1.0f;
    antiPhaseRack.process(sideL.data(), sideR.data(), 24000);
    REQUIRE(energy(sideL, 1500) > 0.001);
    REQUIRE(energy(sideR, 1500) > 0.001);

    sausage::Rack hardLeftRack, hardRightRack;
    hardLeftRack.setSettings(settings);
    hardRightRack.setSettings(settings);
    hardLeftRack.prepare(48000.0);
    hardRightRack.prepare(48000.0);
    std::vector<float> panLL(12000, 0.0f), panLR(12000, 0.0f);
    std::vector<float> panRL(12000, 0.0f), panRR(12000, 0.0f);
    panLL[0] = 1.0f;
    panRR[0] = 1.0f;
    hardLeftRack.process(panLL.data(), panLR.data(), 12000);
    hardRightRack.process(panRL.data(), panRR.data(), 12000);
    REQUIRE(panLL != panRL);
}

TEST_CASE("WSOLA Rise maintains steady tonal envelope without granular comb warble and clamps Prism feedback") {
    const auto mode = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.algorithm = mode;
    settings.sausageOn = false;
    settings.spaceOn = false;
    settings.widthOn = false;
    settings.rise = 1.0f;
    settings.riseMix = 1.0f;
    rack.setSettings(settings);
    rack.prepare(48000.0);
    auto left = signal(48000), right = left;
    rack.process(left.data(), right.data(), 48000);

    constexpr int win = 480;
    float minRms = 1.0e9f, maxRms = 0.0f;
    std::ranges::for_each(std::views::iota(40, 95), [&](int block) {
        double sumSq = 0.0;
        const int base = block * win;
        std::ranges::for_each(std::views::iota(base, base + win), [&](int idx) {
            const float s = left[static_cast<std::size_t>(idx)];
            sumSq += static_cast<double>(s * s);
        });
        const float blockRms = static_cast<float>(std::sqrt(sumSq / static_cast<double>(win)));
        minRms = std::min(minRms, blockRms);
        maxRms = std::max(maxRms, blockRms);
    });
    REQUIRE(maxRms > 0.05f);
    REQUIRE(minRms > 0.72f * maxRms);
}

TEST_CASE("Prism Rise filters and soft-clamps cross-channel pitch feedback") {
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.algorithm = sausage::Algorithm::Prism;
    settings.sausageOn = false;
    settings.spaceOn = false;
    settings.widthOn = false;
    settings.rise = 1.0f;
    settings.riseMix = 1.0f;
    rack.setSettings(settings);
    rack.prepare(48000.0);

    std::vector<float> left(12000, 0.0f), right(12000, 0.0f);
    std::ranges::for_each(std::views::iota(0, 32), [&](int i) {
        left[static_cast<std::size_t>(i)] = 20.0f;
        right[static_cast<std::size_t>(i)] = -20.0f;
    });
    rack.process(left.data(), right.data(), 12000);

    float tailPeak = 0.0f;
    std::ranges::for_each(std::views::iota(4000, 12000), [&](int i) {
        tailPeak = std::max(tailPeak, std::abs(left[static_cast<std::size_t>(i)]));
    });
    REQUIRE(tailPeak < 0.5f);
}

TEST_CASE("Width protects sub-bass mono compatibility in all modes and disperses midrange in Velvet") {
    const auto mode = GENERATE(sausage::Algorithm::Classic, sausage::Algorithm::Velvet, sausage::Algorithm::Punch, sausage::Algorithm::Prism);
    const auto sideRatioAt = [&](float frequency) {
        sausage::Rack rack;
        sausage::RackSettings settings;
        settings.algorithm = mode;
        settings.sausageOn = false;
        settings.spaceOn = false;
        settings.riseOn = false;
        settings.width = 1.0f;
        settings.spread = 0.75f;
        rack.setSettings(settings);
        rack.prepare(48000.0);
        constexpr int total = 24000;
        std::vector<float> left(total), right(total);
        std::ranges::for_each(std::views::iota(0, total), [&](int i) {
            const float s = 0.25f * std::sin(2.0f * std::numbers::pi_v<float> * frequency * static_cast<float>(i) / 48000.0f);
            left[static_cast<std::size_t>(i)] = s;
            right[static_cast<std::size_t>(i)] = s;
        });
        rack.process(left.data(), right.data(), total);
        double midSq = 0.0, sideSq = 0.0;
        std::ranges::for_each(std::views::iota(12000, total), [&](int i) {
            const float l = left[static_cast<std::size_t>(i)];
            const float r = right[static_cast<std::size_t>(i)];
            const float mid = 0.5f * (l + r);
            const float side = 0.5f * (l - r);
            midSq += static_cast<double>(mid * mid);
            sideSq += static_cast<double>(side * side);
        });
        return std::sqrt(sideSq / std::max(midSq, 1.0e-12));
    };

    REQUIRE(sideRatioAt(50.0f) < 0.12);
    REQUIRE(sideRatioAt(400.0f) > 0.22);
}

TEST_CASE("Rack soft clipper uses antiderivative anti-aliasing at host sample rate") {
    constexpr int size = 4096, fundamental = 997, alias = size - 3 * fundamental;
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.sausageOn = false;
    settings.spaceOn = false;
    settings.riseOn = false;
    settings.widthOn = false;
    settings.outputDb = 12.0f;
    settings.softClip = true;
    rack.setSettings(settings);
    rack.prepare(48000.0);

    constexpr int total = size * 2 + sausage::Core::latency;
    std::vector<float> left(total), right(total);
    const float driveGain = std::pow(10.0f, 12.0f / 20.0f);
    std::ranges::for_each(std::views::iota(0, total), [&](int i) {
        const float s = 0.75f * std::sin(2.0f * std::numbers::pi_v<float> * static_cast<float>(fundamental * i) / static_cast<float>(size));
        left[static_cast<std::size_t>(i)] = s;
        right[static_cast<std::size_t>(i)] = s;
    });
    const auto dryInput = left;
    rack.process(left.data(), right.data(), total);

    std::complex<double> rackAlias {}, naiveAlias {};
    std::ranges::for_each(std::views::iota(size, size * 2), [&](int k) {
        const double basisAngle = -2.0 * std::numbers::pi * static_cast<double>(alias * k) / static_cast<double>(size);
        const auto basis = std::polar(1.0, basisAngle);
        const float outSample = left[static_cast<std::size_t>(k + sausage::Core::latency)];
        const double rawClipped = std::tanh(static_cast<double>(dryInput[static_cast<std::size_t>(k)] * driveGain));
        rackAlias += basis * static_cast<double>(outSample);
        naiveAlias += basis * rawClipped;
    });
    REQUIRE(std::abs(naiveAlias) > 1.0);
    REQUIRE(std::abs(rackAlias) < 0.35 * std::abs(naiveAlias));
}

TEST_CASE("Algorithm transition pre-warms next engine and crossfades equal-power without wet dropout") {
    sausage::Rack rack;
    sausage::RackSettings settings;
    settings.algorithm = sausage::Algorithm::Classic;
    settings.sausageOn = false;
    settings.space = 1.0f;
    settings.size = 0.85f;
    rack.setSettings(settings);
    rack.prepare(48000.0);

    auto left = signal(36000), right = left;
    rack.process(left.data(), right.data(), 24000);
    settings.algorithm = sausage::Algorithm::Velvet;
    rack.setSettings(settings);
    rack.process(left.data() + 24000, right.data() + 24000, 12000);

    double preSq = 0.0;
    std::ranges::for_each(std::views::iota(19200, 24000), [&](int idx) {
        const float v = left[static_cast<std::size_t>(idx)];
        preSq += static_cast<double>(v * v);
    });
    const float preSwitchRms = static_cast<float>(std::sqrt(preSq / 4800.0));

    constexpr int win = 480;
    float minTransitionRms = 1.0e9f;
    std::ranges::for_each(std::views::iota(50, 74), [&](int block) {
        double sumSq = 0.0;
        const int base = block * win;
        std::ranges::for_each(std::views::iota(base, base + win), [&](int idx) {
            const float v = left[static_cast<std::size_t>(idx)];
            sumSq += static_cast<double>(v * v);
        });
        minTransitionRms = std::min(minTransitionRms, static_cast<float>(std::sqrt(sumSq / static_cast<double>(win))));
    });
    REQUIRE(preSwitchRms > 0.01f);
    REQUIRE(minTransitionRms > 0.55f * preSwitchRms);
}
