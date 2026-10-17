/**
 * Tests for the samples' net test messages (quiz question encoding).
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
