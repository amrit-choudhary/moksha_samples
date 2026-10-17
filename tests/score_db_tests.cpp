/**
 * Tests for ScoreDB. Uses SQLite's ":memory:" database except where persistence itself is tested.
 */

#include <cstdio>

#include "server/db/score_db.h"
#include "moksha/test_framework/test_framework.h"

TEST(ScoreDB, NewDatabaseStartsAtZero) {
    Samples::ScoreDB db;
    ASSERT(db.Open(":memory:"));

    EXPECT(db.GetHighScore() == 0);
    db.Close();
}

TEST(ScoreDB, HigherScoreIsStored) {
    Samples::ScoreDB db;
    ASSERT(db.Open(":memory:"));

    EXPECT(db.SubmitScore(10));
    EXPECT(db.GetHighScore() == 10);
    db.Close();
}

TEST(ScoreDB, LowerOrEqualScoreIsRejected) {
    Samples::ScoreDB db;
    ASSERT(db.Open(":memory:"));
    db.SubmitScore(10);

    EXPECT(!db.SubmitScore(5));
    EXPECT(!db.SubmitScore(10));
    EXPECT(db.GetHighScore() == 10);
    db.Close();
}

TEST(ScoreDB, SubmitWithoutOpenIsNoOp) {
    Samples::ScoreDB db;

    EXPECT(!db.SubmitScore(10));
    EXPECT(db.GetHighScore() == 0);
}

TEST(ScoreDB, ScorePersistsAcrossReopen) {
    const char* path = "score_db_test.db";
    std::remove(path);

    Samples::ScoreDB writer;
    ASSERT(writer.Open(path));
    writer.SubmitScore(42);
    writer.Close();

    Samples::ScoreDB reader;
    ASSERT(reader.Open(path));
    EXPECT(reader.GetHighScore() == 42);
    reader.Close();

    std::remove(path);
}
