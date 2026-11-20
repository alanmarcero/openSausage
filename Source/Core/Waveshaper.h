#pragma once
#include "Algorithm.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace sausage {
class Waveshaper {
public:
    static double tanhAdaa1(double x, double& prevX, double& prevIntegral) noexcept {
        const double absX = std::abs(x);
        double integral = absX - std::numbers::ln2;
        if (absX < 20.0) integral += std::log1p(std::exp(-2.0 * absX));
        const double delta = x - prevX;
        const double scale = std::max({1.0, absX, std::abs(prevX)});
        double output = std::tanh(0.5 * (x + prevX));
        if (std::abs(delta) > 1.0e-5 * scale) output = (integral - prevIntegral) / delta;
        prevX = x;
        prevIntegral = integral;
        return output;
    }

    float process(float input, Algorithm algorithm) noexcept {
        const double x = static_cast<double>(input);
        if (algorithm == Algorithm::Punch || algorithm == Algorithm::Prism) {
            const bool isPunch = algorithm == Algorithm::Punch;
            const auto f1At = [isPunch](double v) noexcept {
                if (!isPunch) return -std::cos(v);
                const double av = std::abs(v);
                return av <= 1.0 ? 0.5 * v * v : av - 0.5;
            };
            const auto f2At = [isPunch](double v) noexcept {
                if (!isPunch) return -std::sin(v);
                const double av = std::abs(v);
                if (av <= 1.0) return v * v * v / 6.0;
                const double sign = v < 0.0 ? -1.0 : 1.0;
                return sign * (0.5 * v * v - 0.5 * av + 1.0 / 6.0);
            };
            const double f1Center = f1At(previous);
            const double f2Center = f2At(previous);
            double sum = 0.0;
            const std::array<double, 2> endpoints {x, previous2};
            std::ranges::for_each(endpoints, [&](double end) {
                const double diff = end - previous;
                const double scale = std::max({1.0, std::abs(end), std::abs(previous)});
                const double tol = isPunch ? 1.0e-4 * scale : 1.0e-4;
                if (std::abs(diff) > tol) {
                    sum += (f2At(end) - f2Center - diff * f1Center) / (diff * diff);
                    return;
                }
                const double third = (2.0 * previous + end) / 3.0;
                double raw = std::sin(third);
                if (isPunch) raw = std::clamp(third, -1.0, 1.0);
                sum += 0.5 * raw;
            });
            previous2 = previous;
            previous = x;
            previousOut = sum;
            return static_cast<float>(sum);
        }

        // Asymmetric rectification models tube grid-conduction bias; output lag adds core hysteresis.
        const double rectified = std::max(0.0, x) - 0.35 * std::max(0.0, -x);
        const double biasRate = rectified > gridBias ? 0.025 : 0.0006;
        gridBias += biasRate * (rectified - gridBias);
        hystLag += 0.18 * (previousOut - hystLag);
        const double biasDepth = algorithm == Algorithm::Classic ? 0.11 : 0.14;
        const double hystDepth = algorithm == Algorithm::Classic ? 0.06 : 0.035;
        const double u = x - biasDepth * std::tanh(0.6 * gridBias) + hystDepth * (previousOut - hystLag);

        if (algorithm == Algorithm::Classic) {
            previousOut = tanhAdaa1(u, previous, previousIntegral);
            return static_cast<float>(previousOut);
        }

        const double integral = (u * std::atan(u) - 0.5 * std::log1p(u * u)) * 2.0 / std::numbers::pi;
        const double delta = u - previous;
        const double scale = std::max({1.0, std::abs(u), std::abs(previous)});
        double output = std::atan(0.5 * (u + previous)) * 2.0 / std::numbers::pi;
        if (std::abs(delta) > 1.0e-5 * scale) output = (integral - previousIntegral) / delta;
        previous = u;
        previousIntegral = integral;
        previousOut = output;
        return static_cast<float>(output);
    }

private:
    double previous = 0.0, previous2 = 0.0, previousIntegral = 0.0;
    double gridBias = 0.0, hystLag = 0.0, previousOut = 0.0;
};
}
