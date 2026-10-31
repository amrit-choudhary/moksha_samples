#pragma once

/**
 * Multiplayer networking test/demo game: exercises CONNECT/CONNECTED, PING/PONG, and a
 * SCORE_SEND/SCORE_RECV synced score between exactly two clients, and a timed QUIZ_QUESTION round
 * (answer buttons close after kAnswerWindowSeconds), with all state visible in the UI (see
 * SceneUINetTest). Not shipped gameplay - a manual test harness for the network layer, same role
 * as GameUIDemo for the UI subsystem. The score and quiz messages are this sample's own
 * (shared/net/samples_protocol.h); it receives them as the GameClient's listener.
 *
 * Headless (Sample_NetTest_Headless) it runs as a bot for load tests: it clicks Score, chats and answers
 * every quiz by itself, measures ping round trips, and after durationSeconds logs a summary and quits.
 */

#include <cstdint>

#include "moksha/client/game/game.h"
#include "moksha/client/net/game_client.h"
#include "moksha/shared/time/timer_manager.h"
#include "net_test/scene/scene_ui_net_test.h"
#include "shared/net/samples_protocol.h"

namespace Samples {

class GameNetTest : public ME::Game, public ME::IGameClientListener {
   public:
    // durationSeconds > 0 quits after that long (logging the bot summary first); 0 runs until closed.
    explicit GameNetTest(double inDurationSeconds = 0.0);
    ~GameNetTest();

    virtual void Init(ME::Engine& inEngine) override;
    virtual void Start() override;
    virtual void Update(double deltaTime) override;
    virtual void End() override;

    // Score, high score and quiz messages from the server.
    void OnGameMessage(uint8_t senderClientID, uint8_t verb, ME::ByteReader& reader) override;

    void OnConnected();
    void OnPong();
    void OnScoreButtonClicked();
    void OnAnswerTimeout();
    void OnAnswer0Clicked();
    void OnAnswer1Clicked();

    // Bot timers (headless only) and the --duration timer.
    void OnBotScoreTimer();
    void OnBotChatTimer();
    void OnDurationTimer();

   private:
    void OnScoreReceived(uint32_t opponentScore);
    void OnHighScoreReceived(uint32_t newHighScore);
    void OnQuizQuestion(const NetProtocol::QuizQuestion& question);

    void SendScore(uint32_t value);
    void RefreshPingLabel();
    void PickAnswer(int index);
    void SetAnswerButtonsVisible(bool visible);

    // Same object as the base Game::uiScene, kept here with its concrete type so handlers can
    // reach SceneUINetTest's labels without a cast.
    SceneUINetTest* sceneUINetTest = nullptr;

    uint32_t score = 0;
    uint32_t pingsSent = 0;
    uint32_t pongsReceived = 0;
    double pingTimer = 0.0;

    static constexpr double kPingIntervalSeconds = 2.0;

    double connectRetryTimer = 0.0;
    static constexpr double kConnectRetrySeconds = 1.0;

    // The question currently shown, so a picked option can be reported.
    NetProtocol::QuizQuestion lastQuizQuestion;

    // Open while the answer buttons are shown; cleared early when an answer is picked.
    ME::Time::TimerHandle answerTimer;
    char questionText[48] = {};

    static constexpr double kAnswerWindowSeconds = 3.0;

    void LogBotSummary();

    // Bot mode: true when headless.
    bool isBot = false;
    double durationSeconds = 0.0;
    ME::Time::TimerHandle durationTimer;
    ME::Time::TimerHandle botScoreTimer;
    ME::Time::TimerHandle botChatTimer;

    static constexpr double kBotScoreIntervalSeconds = 1.0;
    static constexpr double kBotChatIntervalSeconds = 10.0;

    // Ping round trips: one ping is in flight at a time (sent every kPingIntervalSeconds).
    double pingSentAt = -1.0;
    double rttMin = 0.0;
    double rttMax = 0.0;
    double rttSum = 0.0;
    uint32_t rttCount = 0;
    uint32_t chatsSent = 0;
    uint32_t quizzesAnswered = 0;
};

}  // namespace Samples
