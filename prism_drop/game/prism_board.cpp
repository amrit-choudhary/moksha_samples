#include "prism_drop/game/prism_board.h"

#include <cmath>

namespace {

using Samples::BoardCell;
using Samples::PIECE_CELLS;

// Spawn shapes, x right and y down inside the box. Rotations are derived from these.
constexpr BoardCell SHAPES[Samples::PIECE_TYPE_COUNT][PIECE_CELLS] = {
    {{0, 1}, {1, 1}, {2, 1}, {3, 1}},  // I
    {{0, 0}, {1, 0}, {0, 1}, {1, 1}},  // O
    {{1, 0}, {0, 1}, {1, 1}, {2, 1}},  // T
    {{1, 0}, {2, 0}, {0, 1}, {1, 1}},  // S
    {{0, 0}, {1, 0}, {1, 1}, {2, 1}},  // Z
    {{0, 0}, {0, 1}, {1, 1}, {2, 1}},  // J
    {{2, 0}, {0, 1}, {1, 1}, {2, 1}},  // L
};

constexpr int BOX_SIZES[Samples::PIECE_TYPE_COUNT] = {4, 2, 3, 3, 3, 3, 3};

// Kick tests per start rotation and direction ([from][0] clockwise, [from][1] counter-clockwise), x right and y up.
constexpr int KICK_TESTS = 5;
constexpr BoardCell KICKS_JLSTZ[4][2][KICK_TESTS] = {
    {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}, {{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}},
    {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}, {{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}},
    {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}, {{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}},
    {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}, {{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}},
};
constexpr BoardCell KICKS_I[4][2][KICK_TESTS] = {
    {{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}, {{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}},
    {{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}, {{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}},
    {{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}, {{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}},
    {{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}, {{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}},
};

// Points per lines cleared (0-4), for plain clears and T-spins, times the level.
constexpr uint32_t CLEAR_POINTS[5] = {0, 100, 300, 500, 800};
constexpr uint32_t TSPIN_POINTS[5] = {400, 800, 1200, 1600, 1600};
constexpr uint32_t PERFECT_CLEAR_POINTS = 2000;

}  // namespace

void Samples::PrismBoard::Reset(uint32_t seed) {
    for (auto& row : cells) {
        for (uint8_t& cell : row) {
            cell = 0;
        }
    }
    bagRandom = ME::Random(seed);
    queueCount = 0;
    RefillQueue();
    bHasPiece = false;
    bHasHeld = false;
    bCanHold = true;
    bLastMoveRotation = false;
    score = 0;
    level = 1;
    lines = 0;
    combo = 0;
    bLastClearHard = false;
}

bool Samples::PrismBoard::Spawn() {
    const PieceType type = queue[0];
    for (size_t i = 1; i < queueCount; ++i) {
        queue[i - 1] = queue[i];
    }
    --queueCount;
    RefillQueue();
    bCanHold = true;
    SpawnType(type);
    return Fits(piece);
}

void Samples::PrismBoard::SpawnType(PieceType type) {
    piece.type = type;
    piece.rotation = 0;
    piece.x = type == PieceType::O ? 4 : 3;
    piece.y = VISIBLE_HEIGHT + 1;
    bHasPiece = true;
    bLastMoveRotation = false;
    // Drop one row straight away when there is room, so the piece shows at once.
    ActivePiece lower = piece;
    --lower.y;
    if (Fits(piece) && Fits(lower)) {
        piece = lower;
    }
}

bool Samples::PrismBoard::Move(int dx) {
    if (!bHasPiece) {
        return false;
    }
    ActivePiece moved = piece;
    moved.x += dx;
    if (!Fits(moved)) {
        return false;
    }
    piece = moved;
    bLastMoveRotation = false;
    return true;
}

bool Samples::PrismBoard::Rotate(int direction) {
    if (!bHasPiece || piece.type == PieceType::O) {
        return false;
    }
    const uint8_t from = piece.rotation;
    const uint8_t to = static_cast<uint8_t>((from + (direction > 0 ? 1 : 3)) % 4);
    const auto& kicks = piece.type == PieceType::I ? KICKS_I : KICKS_JLSTZ;
    const int column = direction > 0 ? 0 : 1;
    for (int test = 0; test < KICK_TESTS; ++test) {
        ActivePiece rotated = piece;
        rotated.rotation = to;
        rotated.x += kicks[from][column][test].x;
        rotated.y += kicks[from][column][test].y;
        if (Fits(rotated)) {
            piece = rotated;
            bLastMoveRotation = true;
            return true;
        }
    }
    return false;
}

bool Samples::PrismBoard::StepDown() {
    if (!bHasPiece) {
        return false;
    }
    ActivePiece lower = piece;
    --lower.y;
    if (!Fits(lower)) {
        return false;
    }
    piece = lower;
    bLastMoveRotation = false;
    return true;
}

int Samples::PrismBoard::HardDrop() {
    int rows = 0;
    if (!bHasPiece) {
        return 0;
    }
    ActivePiece lower = piece;
    --lower.y;
    while (Fits(lower)) {
        piece = lower;
        --lower.y;
        ++rows;
    }
    if (rows > 0) {
        bLastMoveRotation = false;
    }
    return rows;
}

Samples::LockResult Samples::PrismBoard::Lock() {
    LockResult result;
    if (!bHasPiece) {
        return result;
    }
    result.bTSpin = IsTSpin();

    BoardCell placed[PIECE_CELLS];
    GetCells(piece, placed);
    bool bAnyVisible = false;
    for (const BoardCell& cell : placed) {
        if (cell.y >= 0 && cell.y < BOARD_HEIGHT && cell.x >= 0 && cell.x < BOARD_WIDTH) {
            cells[cell.y][cell.x] = static_cast<uint8_t>(static_cast<uint8_t>(piece.type) + 1);
        }
        bAnyVisible = bAnyVisible || cell.y < VISIBLE_HEIGHT;
    }
    bHasPiece = false;
    result.bLockOut = !bAnyVisible;

    for (int y = 0; y < BOARD_HEIGHT && result.lines < 4; ++y) {
        if (IsRowFull(y)) {
            result.rows[result.lines++] = y;
        }
    }

    // Scoring, at the level before this clear.
    const uint32_t scoredLevel = level;
    if (result.lines == 0) {
        combo = 0;
        if (result.bTSpin) {
            result.points = TSPIN_POINTS[0] * scoredLevel;
        }
    } else {
        const bool bHard = result.lines == 4 || result.bTSpin;
        result.bBackToBack = bHard && bLastClearHard;
        bLastClearHard = bHard;
        uint32_t points = (result.bTSpin ? TSPIN_POINTS[result.lines] : CLEAR_POINTS[result.lines]) * scoredLevel;
        if (result.bBackToBack) {
            points += points / 2;
        }
        result.combo = combo;
        points += 50 * combo * scoredLevel;
        ++combo;

        // A perfect clear: every row is either full or empty.
        bool bPerfect = true;
        for (int y = 0; y < BOARD_HEIGHT && bPerfect; ++y) {
            if (IsRowFull(y)) {
                continue;
            }
            for (int x = 0; x < BOARD_WIDTH; ++x) {
                if (cells[y][x] != 0) {
                    bPerfect = false;
                    break;
                }
            }
        }
        result.bPerfectClear = bPerfect;
        if (bPerfect) {
            points += PERFECT_CLEAR_POINTS * scoredLevel;
        }
        result.points = points;

        lines += result.lines;
        const uint32_t newLevel = lines / LINES_PER_LEVEL + 1;
        const uint32_t capped = newLevel < MAX_LEVEL ? newLevel : MAX_LEVEL;
        result.bLevelUp = capped > level;
        level = capped;
    }
    score += result.points;
    return result;
}

void Samples::PrismBoard::ClearFullRows(const LockResult& result) {
    // Top row first, so lower indices stay valid while rows above shift down.
    for (int i = static_cast<int>(result.lines) - 1; i >= 0; --i) {
        for (int y = result.rows[i]; y < BOARD_HEIGHT - 1; ++y) {
            for (int x = 0; x < BOARD_WIDTH; ++x) {
                cells[y][x] = cells[y + 1][x];
            }
        }
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            cells[BOARD_HEIGHT - 1][x] = 0;
        }
    }
}

bool Samples::PrismBoard::Hold() {
    if (!bHasPiece || !bCanHold) {
        return false;
    }
    const PieceType current = piece.type;
    if (bHasHeld) {
        SpawnType(held);
    } else {
        Spawn();
    }
    held = current;
    bHasHeld = true;
    bCanHold = false;
    return true;
}

bool Samples::PrismBoard::IsOnFloor() const {
    if (!bHasPiece) {
        return false;
    }
    ActivePiece lower = piece;
    --lower.y;
    return !Fits(lower);
}

int Samples::PrismBoard::GhostY() const {
    ActivePiece lower = piece;
    while (true) {
        --lower.y;
        if (!Fits(lower)) {
            return lower.y + 1;
        }
    }
}

void Samples::PrismBoard::GetCells(const ActivePiece& candidate, BoardCell out[PIECE_CELLS]) const {
    GetShape(candidate.type, candidate.rotation, out);
    for (size_t i = 0; i < PIECE_CELLS; ++i) {
        out[i].x = candidate.x + out[i].x;
        out[i].y = candidate.y - out[i].y;
    }
}

void Samples::PrismBoard::GetShape(PieceType type, uint8_t rotation, BoardCell out[PIECE_CELLS]) {
    const size_t index = static_cast<size_t>(type);
    const int size = BOX_SIZES[index];
    for (size_t i = 0; i < PIECE_CELLS; ++i) {
        BoardCell cell = SHAPES[index][i];
        // Clockwise quarter turns inside the box, y down: (x, y) -> (size - 1 - y, x).
        for (uint8_t turn = 0; turn < rotation % 4; ++turn) {
            const int x = cell.x;
            cell.x = size - 1 - cell.y;
            cell.y = x;
        }
        out[i] = cell;
    }
}

int Samples::PrismBoard::BoxSize(PieceType type) {
    return BOX_SIZES[static_cast<size_t>(type)];
}

uint8_t Samples::PrismBoard::GetCell(int x, int y) const {
    if (x < 0 || x >= BOARD_WIDTH || y < 0 || y >= BOARD_HEIGHT) {
        return 0;
    }
    return cells[y][x];
}

bool Samples::PrismBoard::IsRowFull(int y) const {
    if (y < 0 || y >= BOARD_HEIGHT) {
        return false;
    }
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        if (cells[y][x] == 0) {
            return false;
        }
    }
    return true;
}

int Samples::PrismBoard::StackHeight() const {
    for (int y = BOARD_HEIGHT - 1; y >= 0; --y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            if (cells[y][x] != 0) {
                return y;
            }
        }
    }
    return -1;
}

Samples::PieceType Samples::PrismBoard::PeekNext(size_t index) const {
    return index < queueCount ? queue[index] : PieceType::I;
}

void Samples::PrismBoard::AddDropPoints(int rows, bool bHard) {
    if (rows > 0) {
        score += static_cast<uint64_t>(rows) * (bHard ? 2u : 1u);
    }
}

double Samples::PrismBoard::GravityInterval() const {
    const double steps = static_cast<double>(level - 1);
    const double seconds = std::pow(0.8 - steps * 0.007, steps);
    return seconds > 1.0 / 1200.0 ? seconds : 1.0 / 1200.0;
}

bool Samples::PrismBoard::Fits(const ActivePiece& candidate) const {
    BoardCell placed[PIECE_CELLS];
    GetCells(candidate, placed);
    for (const BoardCell& cell : placed) {
        if (cell.x < 0 || cell.x >= BOARD_WIDTH || cell.y < 0 || cell.y >= BOARD_HEIGHT) {
            return false;
        }
        if (cells[cell.y][cell.x] != 0) {
            return false;
        }
    }
    return true;
}

void Samples::PrismBoard::RefillQueue() {
    // A shuffled bag of all seven types whenever fewer than a bag's worth are queued.
    while (queueCount + PIECE_TYPE_COUNT <= QUEUE_SIZE) {
        PieceType bag[PIECE_TYPE_COUNT];
        for (size_t i = 0; i < PIECE_TYPE_COUNT; ++i) {
            bag[i] = static_cast<PieceType>(i);
        }
        for (size_t i = PIECE_TYPE_COUNT - 1; i > 0; --i) {
            const size_t j = bagRandom.NextRange(0, static_cast<uint32_t>(i));
            const PieceType swap = bag[i];
            bag[i] = bag[j];
            bag[j] = swap;
        }
        for (size_t i = 0; i < PIECE_TYPE_COUNT; ++i) {
            queue[queueCount++] = bag[i];
        }
    }
}

bool Samples::PrismBoard::IsTSpin() const {
    if (piece.type != PieceType::T || !bLastMoveRotation) {
        return false;
    }
    // The T's centre is the middle of its 3 x 3 box; walls and the floor count as filled corners.
    const int cx = piece.x + 1;
    const int cy = piece.y - 1;
    const int corners[4][2] = {{cx - 1, cy - 1}, {cx + 1, cy - 1}, {cx - 1, cy + 1}, {cx + 1, cy + 1}};
    int filled = 0;
    for (const auto& corner : corners) {
        const int x = corner[0];
        const int y = corner[1];
        if (x < 0 || x >= BOARD_WIDTH || y < 0) {
            ++filled;
        } else if (y < BOARD_HEIGHT && cells[y][x] != 0) {
            ++filled;
        }
    }
    return filled >= 3;
}
