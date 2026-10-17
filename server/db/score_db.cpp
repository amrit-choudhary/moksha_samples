#include "score_db.h"

#include <iostream>

bool Samples::ScoreDB::Open(const char* path) {
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        std::cout << "ScoreDB: open failed: " << sqlite3_errmsg(db) << '\n';
        Close();
        return false;
    }

    // Single row table: id is pinned to 1.
    const char* initSql =
        "CREATE TABLE IF NOT EXISTS highscore (id INTEGER PRIMARY KEY CHECK (id = 1), score INTEGER NOT NULL);"
        "INSERT OR IGNORE INTO highscore (id, score) VALUES (1, 0);";
    if (sqlite3_exec(db, initSql, nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cout << "ScoreDB: init failed: " << sqlite3_errmsg(db) << '\n';
        Close();
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT score FROM highscore WHERE id = 1;", -1, &stmt, nullptr) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW) {
        highScore = static_cast<uint32_t>(sqlite3_column_int64(stmt, 0));
    }
    sqlite3_finalize(stmt);

    std::cout << "ScoreDB: opened, high score " << highScore << '\n';
    return true;
}

void Samples::ScoreDB::Close() {
    sqlite3_close(db);
    db = nullptr;
}

bool Samples::ScoreDB::SubmitScore(uint32_t score) {
    if (db == nullptr || score <= highScore) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "UPDATE highscore SET score = ? WHERE id = 1;", -1, &stmt, nullptr) != SQLITE_OK) {
        std::cout << "ScoreDB: prepare failed: " << sqlite3_errmsg(db) << '\n';
        return false;
    }
    sqlite3_bind_int64(stmt, 1, score);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    if (!ok) {
        std::cout << "ScoreDB: write failed: " << sqlite3_errmsg(db) << '\n';
        return false;
    }

    highScore = score;
    return true;
}

uint32_t Samples::ScoreDB::GetHighScore() const {
    return highScore;
}
