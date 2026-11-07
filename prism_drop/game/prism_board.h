/**
 * Prism Drop rules: a 10 x 22 well (rows 20-21 hidden above the top), the seven tetrominoes with rotation kicks, a
 * seven-piece bag, hold, ghost and scoring. Pure game state with no timing, input or drawing, so it can be tested
 * alone; the game decides when gravity, lock delay and line clears happen. Rows count up from 0 at the bottom.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/shared/random/random_engine.h"

namespace Samples {

enum class PieceType : uint8_t {
    I = 0,
    O = 1,
    T = 2,
    S = 3,
    Z = 4,
    J = 5,
    L = 6,
};

constexpr size_t PIECE_TYPE_COUNT = 7;
constexpr int BOARD_WIDTH = 10;
constexpr int BOARD_HEIGHT = 22;  // including the hidden rows
constexpr int VISIBLE_HEIGHT = 20;
constexpr size_t PIECE_CELLS = 4;
constexpr size_t QUEUE_SIZE = 14;  // two bags: always at least 7 known pieces ahead

class BoardCell {
   public:
    int x = 0;
    int y = 0;
};

// The falling piece: its type, rotation (0 spawn, 1 right, 2 reverse, 3 left) and the column and row of its bounding
// box's top-left cell.
class ActivePiece {
   public:
    PieceType type = PieceType::I;
    uint8_t rotation = 0;
    int x = 0;
    int y = 0;
};

// What a lock did, for scoring, sounds and effects.
class LockResult {
   public:
    uint8_t lines = 0;         // full rows, still on the board until ClearFullRows
    bool bTSpin = false;       // a T locked by a rotation with three of its corners filled
    bool bBackToBack = false;  // a quad or T-spin clear following another
    bool bPerfectClear = false;
    bool bLevelUp = false;
    bool bLockOut = false;  // locked entirely in the hidden rows: the game is over
    uint32_t combo = 0;     // consecutive clearing locks before this one
    uint32_t points = 0;
    int rows[4] = {};  // the full rows, bottom first
};

class PrismBoard {
   public:
    // Empties the well, resets the score and fills the queue from a new bag sequence.
    void Reset(uint32_t seed);

    // Takes the next piece from the queue. false when it overlaps the stack (block out): the game is over.
    bool Spawn();

    // Each returns whether the piece moved.
    bool Move(int dx);
    bool Rotate(int direction);  // +1 clockwise, -1 counter-clockwise; tries the kicks in order
    bool StepDown();

    // Drops to the floor and returns the rows fallen. The piece is not locked yet.
    int HardDrop();

    // Writes the piece into the well, finds full rows and scores the lock. The piece is gone until the next Spawn.
    LockResult Lock();

    // Removes the rows found by the last Lock and moves the rest down.
    void ClearFullRows(const LockResult& result);

    // Swaps the piece with the held one (or the next piece when none is held). Once per piece; false when used.
    bool Hold();

    bool IsOnFloor() const;
    // Row of the piece's box when dropped to the floor.
    int GhostY() const;

    // The four cells of piece at its position, in board coordinates.
    void GetCells(const ActivePiece& piece, BoardCell out[PIECE_CELLS]) const;

    // Cell offsets of type at rotation inside its box, x right and y down from the top-left.
    static void GetShape(PieceType type, uint8_t rotation, BoardCell out[PIECE_CELLS]);
    static int BoxSize(PieceType type);

    // 0 when empty, else the piece type + 1.
    uint8_t GetCell(int x, int y) const;
    bool IsRowFull(int y) const;
    // Highest row holding a cell, -1 when empty.
    int StackHeight() const;

    const ActivePiece& GetPiece() const {
        return piece;
    }
    bool HasPiece() const {
        return bHasPiece;
    }
    PieceType PeekNext(size_t index) const;
    bool HasHeld() const {
        return bHasHeld;
    }
    PieceType GetHeld() const {
        return held;
    }
    bool CanHold() const {
        return bCanHold;
    }

    uint64_t GetScore() const {
        return score;
    }
    uint32_t GetLevel() const {
        return level;
    }
    uint32_t GetLines() const {
        return lines;
    }

    // Points for soft and hard drops, per row.
    void AddDropPoints(int rows, bool bHard);

    // Seconds per row of gravity at the current level (the guideline curve, floored at 20 rows per frame).
    double GravityInterval() const;

    // Whether candidate lies inside the well without overlapping the stack.
    bool Fits(const ActivePiece& candidate) const;

    static constexpr uint32_t LINES_PER_LEVEL = 10;
    static constexpr uint32_t MAX_LEVEL = 20;

   private:
    void RefillQueue();
    void SpawnType(PieceType type);
    bool IsTSpin() const;

    uint8_t cells[BOARD_HEIGHT][BOARD_WIDTH] = {};
    ActivePiece piece;
    bool bHasPiece = false;
    bool bLastMoveRotation = false;

    PieceType queue[QUEUE_SIZE] = {};
    size_t queueCount = 0;
    PieceType held = PieceType::I;
    bool bHasHeld = false;
    bool bCanHold = true;

    uint64_t score = 0;
    uint32_t level = 1;
    uint32_t lines = 0;
    uint32_t combo = 0;           // clearing locks in a row
    bool bLastClearHard = false;  // last clear was a quad or T-spin, for back-to-back

    ME::Random bagRandom{1u};
};

}  // namespace Samples
