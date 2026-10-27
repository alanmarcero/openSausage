#pragma once
#include "Algorithm.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace sausage {
class Waveshaper {
public:
    float process(float input, Algorithm algorithm) noexcept {
        if (algorithm == Algorithm::Classic) return std::tanh(input);
        const auto x = static_cast<double>(input);
        double integral = 0.0;
        switch (algorithm) {
            case Algorithm::Velvet:
                integral = (x * std::atan(x) - 0.5 * std::log1p(x * x)) * 2.0 / std::numbers::pi;
                break;
            case Algorithm::Punch:
                integral = std::abs(x) <= 1.0 ? 0.5 * x * x : std::abs(x) - 0.5;
                break;
            case Algorithm::Prism: integral = 1.0 - std::cos(x); break;
            case Algorithm::Classic: return std::tanh(input);
        }
        const auto delta = x - previous;
        double output = 0.0;
        if (std::abs(delta) > 1.0e-5 * std::max({1.0, std::abs(x), std::abs(previous)})) {
            output = (integral - previousIntegral) / delta;
        } else {
            const auto midpoint = (x + previous) * 0.5;
            switch (algorithm) {
                case Algorithm::Velvet: output = std::atan(midpoint) * 2.0 / std::numbers::pi; break;
                case Algorithm::Punch: output = std::clamp(midpoint, -1.0, 1.0); break;
                case Algorithm::Prism: output = std::sin(midpoint); break;
                case Algorithm::Classic: output = std::tanh(midpoint); break;
            }
        }
        previous = x;
        previousIntegral = integral;
        return static_cast<float>(output);
    }
private:
    double previous = 0.0, previousIntegral = 0.0;
};
}
