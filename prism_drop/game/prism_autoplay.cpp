#include "prism_drop/game/prism_autoplay.h"

namespace {

// Weights for a well-known four-feature evaluation of a well; higher is better.
constexpr float HEIGHT_WEIGHT = -0.51f;
constexpr float LINES_WEIGHT = 0.76f;
constexpr float HOLES_WEIGHT = -0.36f;
constexpr float BUMPINESS_WEIGHT = -0.18f;

float Evaluate(const uint8_t (&grid)[Samples::BOARD_HEIGHT][Samples::BOARD_WIDTH]) {
    using Samples::BOARD_HEIGHT;
    using Samples::BOARD_WIDTH;

    int lines = 0;
    bool full[BOARD_HEIGHT] = {};
    for (int y = 0; y < BOARD_HEIGHT; ++y) {
        full[y] = true;
        for (int x = 0; x < BOARD_WIDTH && full[y]; ++x) {
            full[y] = grid[y][x] != 0;
        }
        lines += full[y] ? 1 : 0;
    }

    // Column heights and holes, ignoring the rows that would clear.
    int heights[BOARD_WIDTH] = {};
    int holes = 0;
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        int height = 0;
        int row = 0;
        bool bCovered = false;
        for (int y = BOARD_HEIGHT - 1; y >= 0; --y) {
            if (full[y]) {
                continue;
            }
            if (grid[y][x] != 0) {
                bCovered = true;
            } else if (bCovered) {
                ++holes;
            }
        }
        for (int y = 0; y < BOARD_HEIGHT; ++y) {
            if (full[y]) {
                continue;
            }
            ++row;
            if (grid[y][x] != 0) {
                height = row;
            }
        }
        heights[x] = height;
    }
    int aggregate = 0;
    int bumpiness = 0;
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        aggregate += heights[x];
        if (x > 0) {
            const int step = heights[x] - heights[x - 1];
            bumpiness += step < 0 ? -step : step;
        }
    }
    return HEIGHT_WEIGHT * static_cast<float>(aggregate) + LINES_WEIGHT * static_cast<float>(lines) +
           HOLES_WEIGHT * static_cast<float>(holes) + BUMPINESS_WEIGHT * static_cast<float>(bumpiness);
}

}  // namespace

Samples::Placement Samples::FindBestPlacement(const PrismBoard& board) {
    Placement best;
    if (!board.HasPiece()) {
        return best;
    }
    const ActivePiece current = board.GetPiece();
    float bestScore = 0.0f;
    const uint8_t rotations = current.type == PieceType::O ? 1 : 4;
    for (uint8_t rotation = 0; rotation < rotations; ++rotation) {
        for (int x = -2; x < BOARD_WIDTH; ++x) {
            ActivePiece candidate = current;
            candidate.rotation = rotation;
            candidate.x = x;
            if (!board.Fits(candidate)) {
                continue;
            }
            ActivePiece lower = candidate;
            --lower.y;
            while (board.Fits(lower)) {
                candidate = lower;
                --lower.y;
            }

            uint8_t grid[BOARD_HEIGHT][BOARD_WIDTH];
            for (int y = 0; y < BOARD_HEIGHT; ++y) {
                for (int column = 0; column < BOARD_WIDTH; ++column) {
                    grid[y][column] = board.GetCell(column, y);
                }
            }
            BoardCell cells[PIECE_CELLS];
            board.GetCells(candidate, cells);
            for (const BoardCell& cell : cells) {
                grid[cell.y][cell.x] = 1;
            }
            const float score = Evaluate(grid);
            if (!best.bFound || score > bestScore) {
                best.bFound = true;
                best.rotation = rotation;
                best.x = x;
                bestScore = score;
            }
        }
    }
    return best;
}
