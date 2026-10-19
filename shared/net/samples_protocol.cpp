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

bool Samples::NetProtocol::WriteSurvivorsStatus(ME::ByteWriter& writer, const SurvivorsStatus& status) {
    return writer.WriteU32(status.enemiesAlive) && writer.WriteU32(status.kills) &&
           writer.WriteU16(status.frameTimeMicros);
}

bool Samples::NetProtocol::ReadSurvivorsStatus(ME::ByteReader& reader, SurvivorsStatus& out) {
    SurvivorsStatus status;
    if (!reader.ReadU32(status.enemiesAlive) || !reader.ReadU32(status.kills) ||
        !reader.ReadU16(status.frameTimeMicros)) {
        return false;
    }
    out = status;
    return true;
}

bool Samples::NetProtocol::WriteSurvivorsWave(ME::ByteWriter& writer, const SurvivorsWave& wave) {
    return writer.WriteU32(wave.wave) && writer.WriteU8(wave.enemyType) && writer.WriteU16(wave.count) &&
           writer.WriteU32(wave.seed);
}

bool Samples::NetProtocol::ReadSurvivorsWave(ME::ByteReader& reader, SurvivorsWave& out) {
    SurvivorsWave wave;
    if (!reader.ReadU32(wave.wave) || !reader.ReadU8(wave.enemyType) || !reader.ReadU16(wave.count) ||
        !reader.ReadU32(wave.seed)) {
        return false;
    }
    out = wave;
    return true;
}
