#include "samples_protocol.h"

#include <cstring>

#include "moksha/shared/serialization/byte_reader.h"
#include "moksha/shared/serialization/byte_writer.h"

// Floats travel as their 32 bits, in native byte order like the integers.
static bool WriteF32(ME::ByteWriter& writer, float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return writer.WriteU32(bits);
}

static bool ReadF32(ME::ByteReader& reader, float& out) {
    uint32_t bits = 0;
    if (!reader.ReadU32(bits)) {
        return false;
    }
    std::memcpy(&out, &bits, sizeof(out));
    return true;
}

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

bool Samples::NetProtocol::WriteRallyInput(ME::ByteWriter& writer, const RallyInput& input) {
    return WriteF32(writer, input.paddleX) && writer.WriteU8(input.bBot ? 1 : 0);
}

bool Samples::NetProtocol::ReadRallyInput(ME::ByteReader& reader, RallyInput& out) {
    RallyInput input;
    uint8_t bot = 0;
    if (!ReadF32(reader, input.paddleX) || !reader.ReadU8(bot)) {
        return false;
    }
    input.bBot = bot != 0;
    out = input;
    return true;
}

bool Samples::NetProtocol::WriteRallyState(ME::ByteWriter& writer, const RallyState& state) {
    return writer.WriteU8(state.phase) && writer.WriteU8(state.seatClients[0]) &&
           writer.WriteU8(state.seatClients[1]) && writer.WriteU8(state.scores[0]) && writer.WriteU8(state.scores[1]) &&
           WriteF32(writer, state.ballX) && WriteF32(writer, state.ballY) && WriteF32(writer, state.paddleX[0]) &&
           WriteF32(writer, state.paddleX[1]);
}

bool Samples::NetProtocol::ReadRallyState(ME::ByteReader& reader, RallyState& out) {
    RallyState state;
    if (!reader.ReadU8(state.phase) || !reader.ReadU8(state.seatClients[0]) || !reader.ReadU8(state.seatClients[1]) ||
        !reader.ReadU8(state.scores[0]) || !reader.ReadU8(state.scores[1]) || !ReadF32(reader, state.ballX) ||
        !ReadF32(reader, state.ballY) || !ReadF32(reader, state.paddleX[0]) || !ReadF32(reader, state.paddleX[1])) {
        return false;
    }
    out = state;
    return true;
}
