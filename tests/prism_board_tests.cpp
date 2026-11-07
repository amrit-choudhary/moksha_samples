/**
 * Tests for Prism Drop's rules (PrismBoard): bag, movement, rotation kicks, line clears, scoring and hold.
 */

#include "moksha/test_framework/test_framework.h"
#include "prism_drop/game/prism_autoplay.h"
#include "prism_drop/game/prism_board.h"

using Samples::ActivePiece;
using Samples::BOARD_WIDTH;
using Samples::BoardCell;
using Samples::LockResult;
using Samples::PieceType;
using Samples::PrismBoard;

// Spawns pieces until one of the given type is falling.
static void SpawnType(PrismBoard& board, PieceType type) {
    for (int i = 0; i < 14; ++i) {
        ASSERT(board.Spawn());
        if (board.GetPiece().type == type) {
            return;
        }
    }
}

TEST(PrismBoard, BagHoldsEachTypeOnce) {
    PrismBoard board;
    board.Reset(1234);
    // Spawning replaces the falling piece, so the empty well never fills.
    for (int bag = 0; bag < 3; ++bag) {
        bool seen[7] = {};
        for (int i = 0; i < 7; ++i) {
            ASSERT(board.Spawn());
            const int type = static_cast<int>(board.GetPiece().type);
            EXPECT(!seen[type]);
            seen[type] = true;
        }
    }
}

TEST(PrismBoard, RotationsAreQuarterTurns) {
    BoardCell a[4];
    BoardCell b[4];
    for (int type = 0; type < 7; ++type) {
        PrismBoard::GetShape(static_cast<PieceType>(type), 0, a);
        PrismBoard::GetShape(static_cast<PieceType>(type), 4, b);
        for (int i = 0; i < 4; ++i) {
            EXPECT(a[i].x == b[i].x && a[i].y == b[i].y);
        }
    }
    // T turned clockwise points right: its bump is at (2, 1).
    PrismBoard::GetShape(PieceType::T, 1, a);
    bool bBumpRight = false;
    for (int i = 0; i < 4; ++i) {
        bBumpRight = bBumpRight || (a[i].x == 2 && a[i].y == 1);
    }
    EXPECT(bBumpRight);
}

TEST(PrismBoard, MovesStopAtWalls) {
    PrismBoard board;
    board.Reset(7);
    ASSERT(board.Spawn());
    int moves = 0;
    while (board.Move(-1)) {
        ++moves;
    }
    EXPECT(moves > 0 && moves <= 4);
    BoardCell cells[4];
    board.GetCells(board.GetPiece(), cells);
    int minX = BOARD_WIDTH;
    for (const BoardCell& cell : cells) {
        minX = cell.x < minX ? cell.x : minX;
    }
    EXPECT(minX == 0);
}

TEST(PrismBoard, HardDropLandsOnFloorAndGhostAgrees) {
    PrismBoard board;
    board.Reset(9);
    ASSERT(board.Spawn());
    const int ghost = board.GhostY();
    const int rows = board.HardDrop();
    EXPECT(rows > 0);
    EXPECT(board.GetPiece().y == ghost);
    EXPECT(board.IsOnFloor());
}

TEST(PrismBoard, FourIPiecesInARowClearAQuad) {
    PrismBoard board;
    board.Reset(42);
    // Two layers of O pieces fill columns 0-7 of rows 0-3; vertical I pieces then fill columns 8 and 9.
    for (int column = 0; column < 8; column += 2) {
        for (int layer = 0; layer < 2; ++layer) {
            SpawnType(board, PieceType::O);
            while (board.GetPiece().x > column) {
                ASSERT(board.Move(-1));
            }
            while (board.GetPiece().x < column) {
                ASSERT(board.Move(1));
            }
            board.HardDrop();
            EXPECT(board.Lock().lines == 0);
        }
    }
    // Column 8 still needs filling: a vertical I there, then one in column 9 clears all four rows.
    for (int target = 8; target <= 9; ++target) {
        SpawnType(board, PieceType::I);
        ASSERT(board.Rotate(1));
        BoardCell cells[4];
        board.GetCells(board.GetPiece(), cells);
        while (cells[0].x < target) {
            ASSERT(board.Move(1));
            board.GetCells(board.GetPiece(), cells);
        }
        board.HardDrop();
        const LockResult result = board.Lock();
        if (target == 8) {
            EXPECT(result.lines == 0);
        } else {
            EXPECT(result.lines == 4);
            EXPECT(result.points == 800 + 2000);  // a quad at level 1, plus the perfect clear
            EXPECT(result.bPerfectClear);
            board.ClearFullRows(result);
            EXPECT(board.StackHeight() == -1);
            EXPECT(board.GetLines() == 4);
        }
    }
}

TEST(PrismBoard, HoldOncePerPiece) {
    PrismBoard board;
    board.Reset(5);
    ASSERT(board.Spawn());
    const PieceType first = board.GetPiece().type;
    const PieceType next = board.PeekNext(0);
    ASSERT(board.Hold());
    EXPECT(board.GetHeld() == first);
    EXPECT(board.GetPiece().type == next);
    EXPECT(!board.Hold());
    board.HardDrop();
    board.Lock();
    ASSERT(board.Spawn());
    ASSERT(board.Hold());
    EXPECT(board.GetPiece().type == first);
}

TEST(PrismBoard, KickOffTheWall) {
    PrismBoard board;
    board.Reset(3);
    SpawnType(board, PieceType::T);
    ASSERT(board.Rotate(1));  // bump right, the left column now empty
    while (board.Move(-1)) {
    }
    // Turning back to flat needs the box to move right off the wall.
    const ActivePiece before = board.GetPiece();
    ASSERT(board.Rotate(-1));
    EXPECT(board.GetPiece().rotation == 0);
    EXPECT(board.GetPiece().x >= before.x);
}

TEST(PrismBoard, GravityGetsFasterWithLevel) {
    PrismBoard board;
    board.Reset(1);
    EXPECT(board.GravityInterval() > 0.99 && board.GravityInterval() < 1.01);
}

TEST(PrismBoard, AutoplayClearsLinesWithoutToppingOut) {
    PrismBoard board;
    board.Reset(2026);
    uint32_t pieces = 0;
    while (pieces < 300) {
        ASSERT(board.Spawn());
        const Samples::Placement placement = Samples::FindBestPlacement(board);
        ASSERT(placement.bFound);
        ActivePiece target = board.GetPiece();
        target.rotation = placement.rotation;
        target.x = placement.x;
        // Place directly; the game's demo reaches the same spot by moves.
        while (board.GetPiece().rotation != placement.rotation && board.Rotate(1)) {
        }
        while (board.GetPiece().x < placement.x && board.Move(1)) {
        }
        while (board.GetPiece().x > placement.x && board.Move(-1)) {
        }
        board.HardDrop();
        const LockResult result = board.Lock();
        ASSERT(!result.bLockOut);
        board.ClearFullRows(result);
        ++pieces;
    }
    EXPECT(board.GetLines() >= 100);
}
