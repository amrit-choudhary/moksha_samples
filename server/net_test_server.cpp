#include "net_test_server.h"

#include <string>

#include "moksha/logging/logging.h"
#include "moksha/server/server_app.h"
#include "moksha/shared/misc/delegate.h"
#include "moksha/shared/misc/utils.h"
#include "moksha/shared/net/game_protocol.h"
#include "moksha/shared/serialization/byte_reader.h"
#include "moksha/shared/serialization/byte_writer.h"

namespace GameProtocol = ME::Net::GameProtocol;
using Samples::NetProtocol::Verb;

void Samples::NetTestServer::OnStart(ME::ServerApp& app) {
    gameServer = &app.GetGameServer();
    timers = &app.GetTimers();

    scoreDB.Open((ME::Utils::GetExecutableDirPath() + "/samples.db").c_str());

    quizTimer = timers->SetInterval(ME::Delegate::Make<NetTestServer, &NetTestServer::OnQuizTimer>(this),
                                    QUIZ_INTERVAL_SECONDS);
}

void Samples::NetTestServer::OnEnd() {
    if (timers != nullptr) {
        timers->Clear(quizTimer);
    }
    scoreDB.Close();
    gameServer = nullptr;
    timers = nullptr;
}

void Samples::NetTestServer::OnClientConnected(uint8_t clientID) {
    SendHighScore(clientID);
}

void Samples::NetTestServer::OnGameMessage(uint8_t clientID, uint8_t verb, ME::ByteReader& reader) {
    switch (static_cast<Verb>(verb)) {
        case Verb::SCORE_SEND:
            HandleScore(reader, clientID);
            break;
        default:
            ME::LogWarning("Dropped unknown game verb ", static_cast<unsigned>(verb), " from client ",
                           static_cast<unsigned>(clientID));
            break;
    }
}

void Samples::NetTestServer::HandleScore(ME::ByteReader& reader, uint8_t clientID) {
    uint32_t score = 0;
    if (!reader.ReadU32(score)) {
        ME::LogWarning("Dropped short score from client ", static_cast<unsigned>(clientID));
        return;
    }

    uint8_t buffer[GameProtocol::MAX_DATAGRAM_SIZE];
    ME::ByteWriter writer(buffer, sizeof(buffer));
    GameProtocol::WriteHeader(writer, static_cast<uint8_t>(Verb::SCORE_RECV), clientID);
    writer.WriteU32(score);
    gameServer->SendToOthers(buffer, writer.GetSize(), clientID);

    // A new global high score goes to every client, including the sender.
    if (scoreDB.SubmitScore(score)) {
        BroadcastHighScore();
    }
}

void Samples::NetTestServer::SendHighScore(uint8_t clientID) {
    uint8_t buffer[GameProtocol::MAX_DATAGRAM_SIZE];
    ME::ByteWriter writer(buffer, sizeof(buffer));
    GameProtocol::WriteHeader(writer, static_cast<uint8_t>(Verb::HIGHSCORE_RECV), clientID);
    writer.WriteU32(scoreDB.GetHighScore());
    gameServer->Send(buffer, writer.GetSize(), clientID);
}

void Samples::NetTestServer::BroadcastHighScore() {
    for (size_t i = 0; i < gameServer->GetClientCount(); ++i) {
        SendHighScore(static_cast<uint8_t>(i));
    }
}

void Samples::NetTestServer::OnQuizTimer() {
    NetProtocol::QuizQuestion question;
    question.id = currentQuestion.id + 1;
    question.lhs = static_cast<uint8_t>(quizRandom.NextRange(1, 9));
    question.rhs = static_cast<uint8_t>(quizRandom.NextRange(1, 9));
    question.op = quizRandom.NextRange(0, 1) == 0 ? '+' : '-';

    // Subtraction keeps the larger number first, so answers are never negative.
    if (question.op == '-' && question.lhs < question.rhs) {
        const uint8_t temp = question.lhs;
        question.lhs = question.rhs;
        question.rhs = temp;
    }

    const int correct = question.op == '+' ? question.lhs + question.rhs : question.lhs - question.rhs;
    const int offset = static_cast<int>(quizRandom.NextRange(1, 3));
    const int wrong = (correct >= offset && quizRandom.NextRange(0, 1) == 0) ? correct - offset : correct + offset;

    const uint32_t correctSlot = quizRandom.NextRange(0, 1);
    question.options[correctSlot] = static_cast<uint8_t>(correct);
    question.options[1 - correctSlot] = static_cast<uint8_t>(wrong);

    currentQuestion = question;
    ME::LogInfo("Quiz question ", question.id, ": ", static_cast<unsigned>(question.lhs), " ", question.op, " ",
                static_cast<unsigned>(question.rhs), " (options ", static_cast<unsigned>(question.options[0]), ", ",
                static_cast<unsigned>(question.options[1]), ")");

    // Each client gets its own clientID in the header, like every server reply.
    for (size_t i = 0; i < gameServer->GetClientCount(); ++i) {
        const uint8_t clientID = static_cast<uint8_t>(i);
        uint8_t buffer[GameProtocol::MAX_DATAGRAM_SIZE];
        ME::ByteWriter writer(buffer, sizeof(buffer));
        GameProtocol::WriteHeader(writer, static_cast<uint8_t>(Verb::QUIZ_QUESTION), clientID);
        NetProtocol::WriteQuizQuestion(writer, currentQuestion);
        gameServer->Send(buffer, writer.GetSize(), clientID);
    }
}
