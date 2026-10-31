/**
 * Messages the samples add on top of the engine's protocol: the net test's scores and quiz, the survivors
 * sample's spawn orders and status reports, and the rally sample's paddle input and match state. Their verbs are
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

    // Survivors; verbs from SURVIVORS_JOIN on belong to the survivors server.
    SURVIVORS_JOIN,    // empty, client to server: a round starts; the server restarts this client's waves.
    SURVIVORS_STATUS,  // SurvivorsStatus, client to server, about once a second.
    SURVIVORS_WAVE,    // SurvivorsWave, server to client: what to spawn next.

    // Rally; verbs from RALLY_INPUT on belong to the rally server.
    RALLY_INPUT,  // RallyInput, client to server, every frame; it is also the client's heartbeat.
    RALLY_STATE,  // RallyState, server to client, every server tick.
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

// A survivors client's load: how many enemies it simulates and how long its frames take.
class SurvivorsStatus {
   public:
    uint32_t enemiesAlive = 0;
    uint32_t kills = 0;
    uint16_t frameTimeMicros = 0;  // average over the last report interval, saturated at 65535
};

// Spawn order: count enemies of enemyType, placed from seed (the same seed gives the same positions).
class SurvivorsWave {
   public:
    uint32_t wave = 0;
    uint8_t enemyType = 0;
    uint16_t count = 0;
    uint32_t seed = 0;
};

// Wire layouts: u32 enemiesAlive | u32 kills | u16 frameTimeMicros, and u32 wave | u8 enemyType | u16 count |
// u32 seed. Read* fail, leaving out unchanged, if the datagram is short.
bool WriteSurvivorsStatus(ME::ByteWriter& writer, const SurvivorsStatus& status);
bool ReadSurvivorsStatus(ME::ByteReader& reader, SurvivorsStatus& out);
bool WriteSurvivorsWave(ME::ByteWriter& writer, const SurvivorsWave& wave);
bool ReadSurvivorsWave(ME::ByteReader& reader, SurvivorsWave& out);

// Where a rally player wants its paddle, in its own view (x grows to the player's right). Bots give up their seat to
// a human when the match is full.
class RallyInput {
   public:
    float paddleX = 0.0f;
    bool bBot = false;
};

// The rally match, in court coordinates (seat 0 at the bottom). Each seat holds a clientID or Rally::NO_CLIENT.
class RallyState {
   public:
    uint8_t phase = 0;  // a Rally::Phase (shared/rally/rally_rules.h)
    uint8_t seatClients[2] = {0xFF, 0xFF};
    uint8_t scores[2] = {0, 0};
    float ballX = 0.0f;
    float ballY = 0.0f;
    float paddleX[2] = {0.0f, 0.0f};
};

// Wire layouts: f32 paddleX | u8 bot, and u8 phase | u8 seat0 | u8 seat1 | u8 score0 | u8 score1 | f32 ballX |
// f32 ballY | f32 paddle0 | f32 paddle1 (floats as their 32 bits). Read* fail, leaving out unchanged, if short.
bool WriteRallyInput(ME::ByteWriter& writer, const RallyInput& input);
bool ReadRallyInput(ME::ByteReader& reader, RallyInput& out);
bool WriteRallyState(ME::ByteWriter& writer, const RallyState& state);
bool ReadRallyState(ME::ByteReader& reader, RallyState& out);

}  // namespace Samples::NetProtocol
