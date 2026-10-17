/**
 * Messages the samples add on top of the engine's protocol: the net test's scores and quiz. Their verbs are
 * game verbs (from GameProtocol::FIRST_GAME_VERB), sent with GameClient::SendGameMessage / GameServer::Send.
 */
#pragma once

#include <cstdint>

#include "moksha/shared/net/game_protocol.h"

namespace ME {
class ByteReader;
class ByteWriter;
}  // namespace ME

namespace Samples::NetProtocol {

enum class Verb : uint8_t {
    SCORE_SEND = ME::Net::GameProtocol::FIRST_GAME_VERB,  // u32 score, client to server.
    SCORE_RECV,                                           // u32 score, relayed to the other clients.
    HIGHSCORE_RECV,                                       // u32 global high score, server to client.
    QUIZ_QUESTION,                                        // QuizQuestion, see WriteQuizQuestion.
};

/**
 * An arithmetic question "lhs op rhs" with two answer options, exactly one of them correct.
 * Which option is correct is not sent; the receiver can compute it from lhs, op and rhs.
 */
class QuizQuestion {
   public:
    uint32_t id = 0;  // increases with each question, so answers can be matched to their question
    uint8_t lhs = 0;
    uint8_t rhs = 0;
    char op = '+';  // '+' or '-'
    uint8_t options[2] = {0, 0};
};

/**
 * Wire layout: u32 id | u8 lhs | u8 op | u8 rhs | u8 option0 | u8 option1.
 * ReadQuizQuestion fails, leaving out unchanged, if the datagram is short or op is not '+' or '-'.
 */
bool WriteQuizQuestion(ME::ByteWriter& writer, const QuizQuestion& question);
bool ReadQuizQuestion(ME::ByteReader& reader, QuizQuestion& out);

}  // namespace Samples::NetProtocol
