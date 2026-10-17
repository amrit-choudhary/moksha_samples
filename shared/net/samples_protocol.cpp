#include "samples_protocol.h"

#include "moksha/shared/serialization/byte_reader.h"
#include "moksha/shared/serialization/byte_writer.h"

bool Samples::NetProtocol::WriteQuizQuestion(ME::ByteWriter& writer, const QuizQuestion& question) {
    return writer.WriteU32(question.id) && writer.WriteU8(question.lhs) &&
           writer.WriteU8(static_cast<uint8_t>(question.op)) && writer.WriteU8(question.rhs) &&
           writer.WriteU8(question.options[0]) && writer.WriteU8(question.options[1]);
}

bool Samples::NetProtocol::ReadQuizQuestion(ME::ByteReader& reader, QuizQuestion& out) {
    QuizQuestion question;
    uint8_t op = 0;
    if (!reader.ReadU32(question.id) || !reader.ReadU8(question.lhs) || !reader.ReadU8(op) ||
        !reader.ReadU8(question.rhs) || !reader.ReadU8(question.options[0]) || !reader.ReadU8(question.options[1])) {
        return false;
    }
    if (op != '+' && op != '-') {
        return false;
    }
    question.op = static_cast<char>(op);
    out = question;
    return true;
}
