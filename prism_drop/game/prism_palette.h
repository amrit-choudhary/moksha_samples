/**
 * Prism Drop colours: the piece colours (the same as the generated block art), the per-level backdrop tints and
 * colour helpers.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/rendering/shared/color.h"

namespace Samples::PrismPalette {

constexpr size_t PIECE_COLOR_COUNT = 7;

// I, O, T, S, Z, J, L; matches PIECE_COLORS in tools/gen_prism_drop_assets.py.
constexpr ME::Color PIECES[PIECE_COLOR_COUNT] = {
    {0.24f, 0.90f, 1.00f, 1.0f}, {1.00f, 0.85f, 0.24f, 1.0f}, {0.72f, 0.42f, 1.00f, 1.0f}, {0.36f, 0.95f, 0.48f, 1.0f},
    {1.00f, 0.31f, 0.42f, 1.0f}, {0.31f, 0.48f, 1.00f, 1.0f}, {1.00f, 0.60f, 0.24f, 1.0f},
};

// Backdrop tints, one per level, cycling.
constexpr size_t LEVEL_TINT_COUNT = 6;
constexpr ME::Color LEVEL_TINTS[LEVEL_TINT_COUNT] = {
    {1.00f, 1.00f, 1.00f, 1.0f}, {0.70f, 0.90f, 1.00f, 1.0f}, {1.00f, 0.78f, 0.92f, 1.0f},
    {0.75f, 1.00f, 0.85f, 1.0f}, {1.00f, 0.88f, 0.68f, 1.0f}, {0.85f, 0.75f, 1.00f, 1.0f},
};

constexpr ME::Color DANGER{1.0f, 0.38f, 0.45f, 1.0f};
constexpr ME::Color GOLD{1.0f, 0.86f, 0.35f, 1.0f};
constexpr ME::Color CYAN{0.45f, 0.95f, 1.0f, 1.0f};
constexpr ME::Color PINK{1.0f, 0.45f, 0.85f, 1.0f};

inline ME::Color Piece(size_t index) {
    return PIECES[index % PIECE_COLOR_COUNT];
}

inline ME::Color Mix(const ME::Color& a, const ME::Color& b, float t) {
    return ME::Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

// Towards white by t, keeping alpha.
inline ME::Color Lighten(const ME::Color& color, float t) {
    return ME::Color(color.r + (1.0f - color.r) * t, color.g + (1.0f - color.g) * t, color.b + (1.0f - color.b) * t,
                     color.a);
}

inline ME::Color WithAlpha(const ME::Color& color, float alpha) {
    return ME::Color(color.r, color.g, color.b, alpha);
}

}  // namespace Samples::PrismPalette
