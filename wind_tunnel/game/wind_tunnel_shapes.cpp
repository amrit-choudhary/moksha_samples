#include "wind_tunnel/game/wind_tunnel_shapes.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

using Point = Samples::WindTunnelSim::Point;
using Polygon = std::vector<Point>;

constexpr float PI = 3.14159265358979f;
constexpr int CIRCLE_SIDES = 48;
constexpr int NACA_SAMPLES = 60;   // per surface
constexpr float QUARTER_CHORD = 0.25f;

Polygon Ring(float cx, float cy, float radius, int sides) {
    Polygon ring;
    ring.reserve(static_cast<size_t>(sides));
    for (int k = 0; k < sides; ++k) {
        const float a = 2.0f * PI * static_cast<float>(k) / static_cast<float>(sides);
        ring.push_back({cx + radius * std::cos(a), cy + radius * std::sin(a)});
    }
    return ring;
}

// Scales a drawing made on a unit length to `length` and centres it on its bounding box.
Samples::WindTunnelSim::Outline Fit(Samples::WindTunnelSim::Outline shape, float length) {
    float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
    for (const Polygon& polygon : shape) {
        for (const Point& point : polygon) {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }
    }
    const float scale = length / std::max(maxX - minX, 1e-6f);
    const float cx = 0.5f * (minX + maxX);
    const float cy = 0.5f * (minY + maxY);
    for (Polygon& polygon : shape) {
        for (Point& point : polygon) {
            point = {(point.x - cx) * scale, (point.y - cy) * scale};
        }
    }
    return shape;
}

// Mean camber line and its slope at x (0 to 1 along the chord).
struct Camber {
    // 4 digit.
    float m = 0.0f;
    float p = 0.0f;
    // 5 digit.
    bool bFiveDigit = false;
    float r = 0.0f;
    float k1 = 0.0f;

    void At(float x, float& yc, float& slope) const {
        if (bFiveDigit) {
            if (x < r) {
                yc = k1 / 6.0f * (x * x * x - 3.0f * r * x * x + r * r * (3.0f - r) * x);
                slope = k1 / 6.0f * (3.0f * x * x - 6.0f * r * x + r * r * (3.0f - r));
            } else {
                yc = k1 / 6.0f * r * r * r * (1.0f - x);
                slope = -k1 / 6.0f * r * r * r;
            }
            return;
        }
        if (m == 0.0f || p == 0.0f) {
            yc = 0.0f;
            slope = 0.0f;
        } else if (x < p) {
            yc = m / (p * p) * (2.0f * p * x - x * x);
            slope = 2.0f * m / (p * p) * (p - x);
        } else {
            const float q = (1.0f - p) * (1.0f - p);
            yc = m / q * ((1.0f - 2.0f * p) + 2.0f * p * x - x * x);
            slope = 2.0f * m / q * (p - x);
        }
    }
};

}  // namespace

Samples::WindTunnelSim::Outline Samples::WindTunnelShapes::Circle(float radius) {
    return {Ring(0.0f, 0.0f, radius, CIRCLE_SIDES)};
}

Samples::WindTunnelSim::Outline Samples::WindTunnelShapes::Square(float halfSide) {
    return {{{-halfSide, -halfSide}, {halfSide, -halfSide}, {halfSide, halfSide}, {-halfSide, halfSide}}};
}

Samples::WindTunnelSim::Outline Samples::WindTunnelShapes::Plate(float halfLength, float halfThickness) {
    return {{{-halfThickness, -halfLength},
             {halfThickness, -halfLength},
             {halfThickness, halfLength},
             {-halfThickness, halfLength}}};
}

bool Samples::WindTunnelShapes::Naca(const char* code, float chord, Outline& out) {
    const size_t length = code != nullptr ? std::strlen(code) : 0;
    if (length != 4 && length != 5) {
        return false;
    }
    int digits[5] = {};
    for (size_t k = 0; k < length; ++k) {
        if (code[k] < '0' || code[k] > '9') {
            return false;
        }
        digits[k] = code[k] - '0';
    }

    Camber camber;
    float thickness = 0.0f;
    if (length == 4) {
        camber.m = static_cast<float>(digits[0]) / 100.0f;
        camber.p = static_cast<float>(digits[1]) / 10.0f;
        thickness = static_cast<float>(digits[2] * 10 + digits[3]) / 100.0f;
    } else {
        // Standard (not reflexed) series: camber peaks at 5, 10, 15, 20 or 25% of the chord. Values for a design lift
        // of 0.3, scaled to the code's.
        static constexpr float R[] = {0.0580f, 0.1260f, 0.2025f, 0.2900f, 0.3910f};
        static constexpr float K1[] = {361.400f, 51.640f, 15.957f, 6.643f, 3.230f};
        if (digits[2] != 0 || digits[1] < 1 || digits[1] > 5) {
            return false;
        }
        const float designLift = 0.15f * static_cast<float>(digits[0]);
        camber.bFiveDigit = true;
        camber.r = R[digits[1] - 1];
        camber.k1 = K1[digits[1] - 1] * designLift / 0.3f;
        thickness = static_cast<float>(digits[3] * 10 + digits[4]) / 100.0f;
    }
    if (thickness <= 0.0f) {
        return false;
    }

    // Upper surface from the trailing edge to the nose, then the lower back: one simple polygon.
    Polygon foil;
    foil.reserve(2 * NACA_SAMPLES + 1);
    auto surface = [&](int k, float side) {
        const float beta = PI * static_cast<float>(k) / static_cast<float>(NACA_SAMPLES);
        const float x = 0.5f * (1.0f - std::cos(beta));
        // The -0.1036 term closes the trailing edge.
        const float yt = 5.0f * thickness *
                         (0.2969f * std::sqrt(x) - 0.1260f * x - 0.3516f * x * x + 0.2843f * x * x * x -
                          0.1036f * x * x * x * x);
        float yc = 0.0f;
        float slope = 0.0f;
        camber.At(x, yc, slope);
        const float theta = std::atan(slope);
        const float px = x - side * yt * std::sin(theta);
        const float py = yc + side * yt * std::cos(theta);
        foil.push_back({(px - QUARTER_CHORD) * chord, py * chord});
    };
    for (int k = NACA_SAMPLES; k >= 0; --k) {
        surface(k, 1.0f);
    }
    for (int k = 1; k < NACA_SAMPLES; ++k) {
        surface(k, -1.0f);
    }
    out = {foil};
    return true;
}

Samples::WindTunnelSim::Outline Samples::WindTunnelShapes::Aircraft(float length) {
    // A generic airliner from the side, nose left, drawn on a unit length.
    Outline shape = {
        // Fuselage.
        {{0.00f, 0.000f},
         {0.02f, 0.030f},
         {0.06f, 0.055f},
         {0.10f, 0.072f},
         {0.13f, 0.085f},
         {0.18f, 0.090f},
         {0.72f, 0.090f},
         {0.84f, 0.078f},
         {0.97f, 0.062f},
         {1.00f, 0.052f},
         {1.00f, 0.025f},
         {0.90f, 0.000f},
         {0.78f, -0.030f},
         {0.70f, -0.050f},
         {0.20f, -0.055f},
         {0.08f, -0.045f},
         {0.03f, -0.025f}},
        // Fin.
        {{0.80f, 0.080f}, {0.90f, 0.250f}, {0.97f, 0.250f}, {0.985f, 0.060f}},
        // Engine on its pylon, under the wing.
        {{0.40f, -0.045f}, {0.43f, -0.045f}, {0.43f, -0.070f}, {0.40f, -0.070f}},
        {{0.33f, -0.065f}, {0.47f, -0.065f}, {0.48f, -0.090f}, {0.46f, -0.115f}, {0.35f, -0.115f}, {0.33f, -0.095f}},
    };
    return Fit(shape, length);
}

Samples::WindTunnelSim::Outline Samples::WindTunnelShapes::Car(float length) {
    // A generic saloon from the side, nose left, drawn on a unit length.
    Outline shape = {
        {{0.00f, 0.12f},
         {0.01f, 0.22f},
         {0.05f, 0.27f},
         {0.30f, 0.31f},
         {0.36f, 0.33f},
         {0.45f, 0.43f},
         {0.52f, 0.45f},
         {0.70f, 0.45f},
         {0.78f, 0.42f},
         {0.88f, 0.33f},
         {0.97f, 0.31f},
         {1.00f, 0.27f},
         {1.00f, 0.13f},
         {0.96f, 0.10f},
         {0.04f, 0.10f}},
        Ring(0.20f, 0.10f, 0.085f, 24),
        Ring(0.80f, 0.10f, 0.085f, 24),
    };
    return Fit(shape, length);
}
