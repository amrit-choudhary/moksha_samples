/**
 * Server side of the net test sample: relays scores, keeps the global high score in a ScoreDB and sends a quiz
 * question to every client on a fixed interval. The engine's GameServer handles connects, pings, chat and data.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/server/game_server.h"
#include "moksha/shared/random/random_engine.h"
#include "moksha/shared/time/timer_manager.h"
#include "server/db/score_db.h"
#include "shared/net/samples_protocol.h"

namespace Samples {

class NetTestServer : public ME::IGameServerListener {
   public:
    // Opens the score database next to the executable and starts the quiz timer.
    void OnStart(ME::ServerApp& app) override;

    // Stops the quiz timer and closes the score database.
    void OnEnd() override;

    // Sends the new client the current high score.
    void OnClientConnected(uint8_t clientID) override;

    void OnGameMessage(uint8_t clientID, uint8_t verb, ME::ByteReader& reader) override;

   private:
    void HandleScore(ME::ByteReader& reader, uint8_t clientID);
    void SendHighScore(uint8_t clientID);
    void BroadcastHighScore();

    // Interval timer callback: makes a new question and sends it to every connected client.
    void OnQuizTimer();

    static constexpr double QUIZ_INTERVAL_SECONDS = 5.0;

    ME::GameServer* gameServer = nullptr;
    ME::Time::TimerManager* timers = nullptr;
    ScoreDB scoreDB;

    ME::Time::TimerHandle quizTimer;
    ME::Random quizRandom{"quiz", true};
    NetProtocol::QuizQuestion currentQuestion;
};

}  // namespace Samples
