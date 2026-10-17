/**
 * Persists a single global high score in an embedded SQLite database.
 */

#pragma once

#include <cstdint>

#include "sqlite3.h"

namespace Samples {

class ScoreDB {
   public:
    /**
     * Opens (creating if needed) the database file and loads the stored high score.
     * Returns false on failure; the other calls are then no-ops.
     */
    bool Open(const char* path);
    void Close();

    /**
     * Stores the score if it beats the current high score.
     * Returns true if it did.
     */
    bool SubmitScore(uint32_t score);

    uint32_t GetHighScore() const;

   private:
    sqlite3* db = nullptr;
    uint32_t highScore = 0;
};

}  // namespace Samples
