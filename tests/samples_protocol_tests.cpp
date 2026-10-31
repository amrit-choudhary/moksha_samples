/**
 * Tests for the samples' messages (quiz question and rally encoding).
 */

#include <cstdint>

#include "moksha/shared/serialization/byte_reader.h"
#include "moksha/shared/serialization/byte_writer.h"
#include "moksha/test_framework/test_framework.h"
#include "shared/net/samples_protocol.h"

namespace NP = Samples::NetProtocol;

TEST(SamplesProtocol, QuizQuestionRoundTrip) {
    NP::QuizQuestion question;
    question.id = 42;
    question.lhs = 9;
    question.rhs = 4;
    question.op = '-';
    question.options[0] = 6;
    question.options[1] = 5;

    uint8_t buffer[64] = {};
    ME::ByteWriter writer(buffer, sizeof(buffer));
    ASSERT(NP::WriteQuizQuestion(writer, question));
    EXPECT(writer.GetSize() == 9);

    ME::ByteReader reader(buffer, writer.GetSize());
    NP::QuizQuestion out;
    ASSERT(NP::ReadQuizQuestion(reader, out));
    EXPECT(out.id == 42);
    EXPECT(out.lhs == 9);
    EXPECT(out.rhs == 4);
    EXPECT(out.op == '-');
    EXPECT(out.options[0] == 6);
    EXPECT(out.options[1] == 5);
}

TEST(SamplesProtocol, QuizQuestionWithUnknownOpIsRejected) {
    NP::QuizQuestion question;
    question.id = 7;
    question.op = '*';

    uint8_t buffer[64] = {};
    ME::ByteWriter writer(buffer, sizeof(buffer));
    ASSERT(NP::WriteQuizQuestion(writer, question));

    ME::ByteReader reader(buffer, writer.GetSize());
    NP::QuizQuestion out;
    EXPECT(!NP::ReadQuizQuestion(reader, out));
    EXPECT(out.id == 0);
}

TEST(SamplesProtocol, TruncatedQuizQuestionIsRejected) {
    const uint8_t data[5] = {1, 0, 0, 0, 2};
    ME::ByteReader reader(data, sizeof(data));
    NP::QuizQuestion out;

    EXPECT(!NP::ReadQuizQuestion(reader, out));
}

TEST(SamplesProtocol, RallyInputRoundTrip) {
    NP::RallyInput input;
    input.paddleX = -12.5f;
    input.bBot = true;

    uint8_t buffer[64] = {};
    ME::ByteWriter writer(buffer, sizeof(buffer));
    ASSERT(NP::WriteRallyInput(writer, input));
    EXPECT(writer.GetSize() == 5);

    ME::ByteReader reader(buffer, writer.GetSize());
    NP::RallyInput out;
    ASSERT(NP::ReadRallyInput(reader, out));
    EXPECT(out.paddleX == -12.5f);
    EXPECT(out.bBot);
}

TEST(SamplesProtocol, RallyStateRoundTrip) {
    NP::RallyState state;
    state.phase = 2;
    state.seatClients[0] = 3;
    state.seatClients[1] = 0xFF;
    state.scores[0] = 4;
    state.scores[1] = 6;
    state.ballX = 1.25f;
    state.ballY = -70.5f;
    state.paddleX[0] = 30.0f;
    state.paddleX[1] = -8.75f;

    uint8_t buffer[64] = {};
    ME::ByteWriter writer(buffer, sizeof(buffer));
    ASSERT(NP::WriteRallyState(writer, state));
    EXPECT(writer.GetSize() == 21);

    ME::ByteReader reader(buffer, writer.GetSize());
    NP::RallyState out;
    ASSERT(NP::ReadRallyState(reader, out));
    EXPECT(out.phase == 2);
    EXPECT(out.seatClients[0] == 3);
    EXPECT(out.seatClients[1] == 0xFF);
    EXPECT(out.scores[0] == 4);
    EXPECT(out.scores[1] == 6);
    EXPECT(out.ballX == 1.25f);
    EXPECT(out.ballY == -70.5f);
    EXPECT(out.paddleX[0] == 30.0f);
    EXPECT(out.paddleX[1] == -8.75f);
}

TEST(SamplesProtocol, TruncatedRallyStateIsRejected) {
    const uint8_t data[9] = {1, 0, 1, 0, 0, 0, 0, 0, 0};
    ME::ByteReader reader(data, sizeof(data));
    NP::RallyState out;

    EXPECT(!NP::ReadRallyState(reader, out));
    EXPECT(out.seatClients[0] == 0xFF);
}
