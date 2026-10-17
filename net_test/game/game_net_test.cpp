#include "game_net_test.h"

#include <cstdio>

#include "moksha/client/app/engine.h"
#include "moksha/client/debug/debug_system.h"
#include "moksha/client/ui/button.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/misc/delegate.h"
#include "moksha/shared/serialization/byte_reader.h"
#include "moksha/shared/serialization/byte_writer.h"

using Samples::NetProtocol::Verb;

Samples::GameNetTest::GameNetTest(double inDurationSeconds) : Game(), durationSeconds(inDurationSeconds) {}

Samples::GameNetTest::~GameNetTest() {}

void Samples::GameNetTest::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    // Plain, un-subclassed Scene: no Build*() overrides, so no meshes/sprites/lights - renders
    // as a black background.
    scene = new ME::Scene();
    scene->Init();

    sceneUINetTest = new SceneUINetTest();
    sceneUINetTest->Init();
    uiScene = sceneUINetTest;

    ME::GameClient* gameClient = engine->gameClient;
    ME::Delegate::Bind<GameNetTest, &GameNetTest::OnScoreButtonClicked>(sceneUINetTest->GetScoreButton()->onClick,
                                                                        this);
    ME::Delegate::Bind<GameNetTest, &GameNetTest::OnConnected>(gameClient->onConnected, this);
    ME::Delegate::Bind<GameNetTest, &GameNetTest::OnPong>(gameClient->onPong, this);
    gameClient->SetListener(this);
    ME::Delegate::Bind<GameNetTest, &GameNetTest::OnAnswer0Clicked>(sceneUINetTest->GetAnswerButton(0)->onClick, this);
    ME::Delegate::Bind<GameNetTest, &GameNetTest::OnAnswer1Clicked>(sceneUINetTest->GetAnswerButton(1)->onClick, this);

    gameClient->SendConnectRequest();

    isBot = engine->IsHeadless();
    if (isBot) {
        botScoreTimer = engine->timers->SetInterval(
            ME::Delegate::Make<GameNetTest, &GameNetTest::OnBotScoreTimer>(this), kBotScoreIntervalSeconds);
        botChatTimer = engine->timers->SetInterval(ME::Delegate::Make<GameNetTest, &GameNetTest::OnBotChatTimer>(this),
                                                   kBotChatIntervalSeconds);
    }
    if (durationSeconds > 0.0) {
        durationTimer = engine->timers->SetTimeout(ME::Delegate::Make<GameNetTest, &GameNetTest::OnDurationTimer>(this),
                                                   durationSeconds);
    }

    ME::Log("Net Test Game Start!", isBot ? " (bot)" : "");
}

void Samples::GameNetTest::Start() {
    Game::Start();
}

void Samples::GameNetTest::Update(double deltaTime) {
    Game::Update(deltaTime);

    pingTimer += deltaTime;
    if (pingTimer >= kPingIntervalSeconds) {
        pingTimer -= kPingIntervalSeconds;
        engine->gameClient->SendPing();
        ++pingsSent;
        pingSentAt = engine->time->GetTimeSinceStartup();
        RefreshPingLabel();
    }
}

void Samples::GameNetTest::OnGameMessage(uint8_t /*senderClientID*/, uint8_t verb, ME::ByteReader& reader) {
    switch (static_cast<Verb>(verb)) {
        case Verb::SCORE_RECV: {
            uint32_t value = 0;
            if (reader.ReadU32(value)) {
                OnScoreReceived(value);
            }
            break;
        }
        case Verb::HIGHSCORE_RECV: {
            uint32_t value = 0;
            if (reader.ReadU32(value)) {
                OnHighScoreReceived(value);
            }
            break;
        }
        case Verb::QUIZ_QUESTION: {
            NetProtocol::QuizQuestion question;
            if (NetProtocol::ReadQuizQuestion(reader, question)) {
                OnQuizQuestion(question);
            }
            break;
        }
        default:
            break;
    }
}

void Samples::GameNetTest::OnConnected() {
    char buf[32];
    snprintf(buf, sizeof(buf), "Connected as Client %d", engine->gameClient->GetClientID());
    sceneUINetTest->GetStatusLabel()->SetText(buf);
    if (isBot) {
        ME::Log(buf);
    }
}

void Samples::GameNetTest::OnPong() {
    if (pingSentAt >= 0.0) {
        const double rtt = engine->time->GetTimeSinceStartup() - pingSentAt;
        rttMin = (rttCount == 0 || rtt < rttMin) ? rtt : rttMin;
        rttMax = rtt > rttMax ? rtt : rttMax;
        rttSum += rtt;
        ++rttCount;
        pingSentAt = -1.0;
    }
    ++pongsReceived;
    RefreshPingLabel();
}

void Samples::GameNetTest::OnScoreReceived(uint32_t opponentScore) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Opponent Score: %u", opponentScore);
    sceneUINetTest->GetOpponentScoreLabel()->SetText(buf);
}

void Samples::GameNetTest::OnHighScoreReceived(uint32_t newHighScore) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Global High: %u", newHighScore);
    sceneUINetTest->GetHighScoreLabel()->SetText(buf);
}

void Samples::GameNetTest::OnScoreButtonClicked() {
    ++score;

    char buf[32];
    snprintf(buf, sizeof(buf), "Your Score: %u", score);
    sceneUINetTest->GetYourScoreLabel()->SetText(buf);

    SendScore(score);
}

void Samples::GameNetTest::SendScore(uint32_t value) {
    uint8_t payload[4];
    ME::ByteWriter writer(payload, sizeof(payload));
    writer.WriteU32(value);
    engine->gameClient->SendGameMessage(static_cast<uint8_t>(Verb::SCORE_SEND), payload, writer.GetSize());
}

void Samples::GameNetTest::OnQuizQuestion(const NetProtocol::QuizQuestion& question) {
    lastQuizQuestion = question;

    snprintf(questionText, sizeof(questionText), "Q%u: %u %c %u = ?", question.id, static_cast<unsigned>(question.lhs),
             question.op, static_cast<unsigned>(question.rhs));
    sceneUINetTest->GetQuizQuestionLabel()->SetText(questionText);

    char buf[8];
    for (int i = 0; i < SceneUINetTest::ANSWER_COUNT; ++i) {
        snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(question.options[i]));
        sceneUINetTest->GetAnswerLabel(i)->SetText(buf);
    }
    SetAnswerButtonsVisible(true);

    // The bot answers at once, with the correct option.
    if (isBot) {
        const int correct = question.op == '+' ? question.lhs + question.rhs : question.lhs - question.rhs;
        PickAnswer(question.options[0] == correct ? 0 : 1);
        ++quizzesAnswered;
        return;
    }

    // A question arriving early replaces the open one, so its timeout must not close the new one.
    engine->timers->Clear(answerTimer);
    answerTimer = engine->timers->SetTimeout(ME::Delegate::Make<GameNetTest, &GameNetTest::OnAnswerTimeout>(this),
                                             kAnswerWindowSeconds);
}

void Samples::GameNetTest::OnAnswerTimeout() {
    SetAnswerButtonsVisible(false);

    char buf[64];
    snprintf(buf, sizeof(buf), "%s  Time up!", questionText);
    sceneUINetTest->GetQuizQuestionLabel()->SetText(buf);
    ME::Log("Quiz: time up");
}

void Samples::GameNetTest::OnAnswer0Clicked() {
    PickAnswer(0);
}

void Samples::GameNetTest::OnAnswer1Clicked() {
    PickAnswer(1);
}

void Samples::GameNetTest::PickAnswer(int index) {
    engine->timers->Clear(answerTimer);
    SetAnswerButtonsVisible(false);

    const unsigned picked = lastQuizQuestion.options[index];
    char buf[64];
    snprintf(buf, sizeof(buf), "%s  Picked %u", questionText, picked);
    sceneUINetTest->GetQuizQuestionLabel()->SetText(buf);
    ME::Log("Quiz: picked ", picked);
}

void Samples::GameNetTest::SetAnswerButtonsVisible(bool visible) {
    for (int i = 0; i < SceneUINetTest::ANSWER_COUNT; ++i) {
        sceneUINetTest->GetAnswerButton(i)->SetVisible(visible);
    }
}

void Samples::GameNetTest::RefreshPingLabel() {
    char buf[48];
    snprintf(buf, sizeof(buf), "Pings: %u / Pongs: %u", pingsSent, pongsReceived);
    sceneUINetTest->GetPingLabel()->SetText(buf);
}

void Samples::GameNetTest::OnBotScoreTimer() {
    OnScoreButtonClicked();
}

void Samples::GameNetTest::OnBotChatTimer() {
    char message[32];
    snprintf(message, sizeof(message), "hello from bot %d", engine->gameClient->GetClientID());
    engine->gameClient->SendChat(message);
    ++chatsSent;
}

void Samples::GameNetTest::OnDurationTimer() {
    LogBotSummary();
    engine->Quit();
}

void Samples::GameNetTest::LogBotSummary() {
    const double rttAvg = rttCount > 0 ? rttSum / rttCount : 0.0;
    char buf[224];
    snprintf(buf, sizeof(buf),
             "BOT SUMMARY client=%d connected=%d pings=%u pongs=%u rtt_ms(min/avg/max)=%.1f/%.1f/%.1f scores=%u "
             "chats=%u quizzes=%u",
             engine->gameClient->GetClientID(), engine->gameClient->IsConnected() ? 1 : 0, pingsSent, pongsReceived,
             rttMin * 1000.0, rttAvg * 1000.0, rttMax * 1000.0, score, chatsSent, quizzesAnswered);
    ME::Log(buf);
}

void Samples::GameNetTest::End() {
    // Before the UI goes away: the timeout callback writes to its widgets, and the client must stop calling us.
    engine->timers->Clear(answerTimer);
    engine->timers->Clear(botScoreTimer);
    engine->timers->Clear(botChatTimer);
    engine->timers->Clear(durationTimer);
    engine->gameClient->SetListener(nullptr);

    // sceneUINetTest and uiScene alias the same object - deleting uiScene destroys the widgets
    // SceneUINetTest owns (see SceneUINetTest::~SceneUINetTest()).
    delete uiScene;
    delete scene;

    Game::End();
    ME::Log("Net Test Game End!");
}
