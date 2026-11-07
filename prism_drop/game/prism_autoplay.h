/**
 * Prism Drop autoplay: picks where the falling piece should land by trying every rotation and column and scoring the
 * resulting well (lines cleared, stack height, holes, bumpiness). Drives the title screen's attract mode.
 */

#pragma once

#include <cstdint>

#include "prism_drop/game/prism_board.h"

namespace Samples {

class Placement {
   public:
    uint8_t rotation = 0;
    int x = 0;
    bool bFound = false;
};

// The best landing for the board's falling piece. bFound is false when it has no piece or nowhere fits.
Placement FindBestPlacement(const PrismBoard& board);

}  // namespace Samples
