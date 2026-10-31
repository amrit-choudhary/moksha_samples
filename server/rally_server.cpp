#include "rally_server.h"

#include "moksha/logging/logging.h"
#include "moksha/server/server_app.h"
#include "moksha/shared/misc/delegate.h"
#include "moksha/shared/net/game_protocol.h"
#include "moksha/shared/serialization/byte_reader.h"
#include "moksha/shared/serialization/byte_writer.h"

namespace GameProtocol = ME::Net::GameProtocol;
using Samples::NetProtocol::Verb;
using Samples::Rally::Phase;

void Samples::RallyServer::OnStart(ME::ServerApp& app) {
    gameServer = &app.GetGameServer();
    timers = &app.GetTimers();
    tickTimer = timers->SetInterval(ME::Delegate::Make<RallyServer, &RallyServer::OnTick>(this), TICK_SECONDS);
}

void Samples::RallyServer::OnEnd() {
    if (timers != nullptr) {
        timers->Clear(tickTimer);
    }
    gameServer = nullptr;
    timers = nullptr;
}

void Samples::RallyServer::OnGameMessage(uint8_t clientID, uint8_t verb, ME::ByteReader& reader) {
    if (static_cast<Verb>(verb) != Verb::RALLY_INPUT) {
        ME::LogWarning("Rally: dropped unknown verb ", static_cast<unsigned>(verb), " from client ",
                       static_cast<unsigned>(clientID));
        return;
    }
    NetProtocol::RallyInput input;
    if (!NetProtocol::ReadRallyInput(reader, input)) {
        return;
    }

    lastHeard[clientID] = now;
    int seat = FindSeat(clientID);
    if (seat < 0) {
        if (!TrySeat(clientID, input.bBot)) {
            return;
        }
        seat = FindSeat(clientID);
    }

    // Input is in the player's own view; seat 1 looks at the court upside down.
    float x = input.paddleX;
    x = x > Rally::PADDLE_MAX_X ? Rally::PADDLE_MAX_X : (x < -Rally::PADDLE_MAX_X ? -Rally::PADDLE_MAX_X : x);
    seats[seat].paddleX = seat == 0 ? x : -x;
}

void Samples::RallyServer::OnTick() {
    now += TICK_SECONDS;
    phaseTime += TICK_SECONDS;

    DropSilentClients();

    const bool bFull = seats[0].clientID != Rally::NO_CLIENT && seats[1].clientID != Rally::NO_CLIENT;
    if (!bFull) {
        phase = Phase::Waiting;
        ballX = 0.0f;
        ballY = 0.0f;
    } else {
        switch (phase) {
            case Phase::Waiting:
                StartMatch();
                break;
            case Phase::Serve:
                if (phaseTime >= SERVE_DELAY_SECONDS) {
                    // Serves alternate between angling left and right.
                    const bool bLeft = (scores[0] + scores[1]) % 2 == 1;
                    ballVelX = (bLeft ? -0.3f : 0.3f) * SERVE_SPEED;
                    ballVelY = serveToward == 0 ? -SERVE_SPEED : SERVE_SPEED;
                    phase = Phase::Play;
                    phaseTime = 0.0;
                }
                break;
            case Phase::Play:
                MoveBall(static_cast<float>(TICK_SECONDS));
                break;
            case Phase::Won:
                if (phaseTime >= WON_DELAY_SECONDS) {
                    StartMatch();
                }
                break;
        }
    }

    SendState();
}

bool Samples::RallyServer::TrySeat(uint8_t clientID, bool bBot) {
    int seat = -1;
    for (int i = 0; i < 2 && seat < 0; ++i) {
        if (seats[i].clientID == Rally::NO_CLIENT) {
            seat = i;
        }
    }
    for (int i = 0; i < 2 && seat < 0 && !bBot; ++i) {
        if (seats[i].bBot) {
            ME::Log("Rally: client ", static_cast<unsigned>(clientID), " takes seat ", i, " from bot client ",
                    static_cast<unsigned>(seats[i].clientID));
            seat = i;
        }
    }
    if (seat < 0) {
        return false;
    }

    seats[seat].clientID = clientID;
    seats[seat].bBot = bBot;
    seats[seat].paddleX = 0.0f;
    ME::Log("Rally: client ", static_cast<unsigned>(clientID), bBot ? " (bot)" : "", " sits in seat ", seat);

    // A new player means a new match.
    phase = Phase::Waiting;
    return true;
}

int Samples::RallyServer::FindSeat(uint8_t clientID) const {
    for (int i = 0; i < 2; ++i) {
        if (seats[i].clientID == clientID) {
            return i;
        }
    }
    return -1;
}

bool Samples::RallyServer::IsConnected(uint8_t clientID) const {
    return lastHeard[clientID] > 0.0 && now - lastHeard[clientID] <= CLIENT_TIMEOUT_SECONDS;
}

void Samples::RallyServer::DropSilentClients() {
    for (int i = 0; i < 2; ++i) {
        if (seats[i].clientID != Rally::NO_CLIENT && !IsConnected(seats[i].clientID)) {
            ME::Log("Rally: client ", static_cast<unsigned>(seats[i].clientID), " went silent, seat ", i, " is free");
            seats[i] = Seat();
        }
    }
}

void Samples::RallyServer::StartMatch() {
    scores[0] = 0;
    scores[1] = 0;
    ME::Log("Rally: match starts, client ", static_cast<unsigned>(seats[0].clientID), " vs client ",
            static_cast<unsigned>(seats[1].clientID));
    StartServe(0);
}

void Samples::RallyServer::StartServe(int towardSeat) {
    phase = Phase::Serve;
    phaseTime = 0.0;
    serveToward = towardSeat;
    ballX = 0.0f;
    ballY = 0.0f;
    ballVelX = 0.0f;
    ballVelY = 0.0f;
}

void Samples::RallyServer::MoveBall(float deltaTime) {
    const float previousY = ballY;
    ballX += ballVelX * deltaTime;
    ballY += ballVelY * deltaTime;

    // Side walls reflect the ball.
    const float wallX = Rally::COURT_HALF_WIDTH - Rally::BALL_HALF_SIZE;
    if (ballX > wallX || ballX < -wallX) {
        const float wall = ballX > 0.0f ? wallX : -wallX;
        ballX = 2.0f * wall - ballX;
        ballVelX = -ballVelX;
    }

    // A paddle returns the ball if the ball crossed its face this step; seat 0 defends y < 0, seat 1 y > 0.
    for (int seat = 0; seat < 2; ++seat) {
        const float side = seat == 0 ? -1.0f : 1.0f;
        const float faceY = side * (Rally::PADDLE_Y - Rally::PADDLE_HALF_HEIGHT - Rally::BALL_HALF_SIZE);
        const bool bCrossed = (previousY - faceY) * side <= 0.0f && (ballY - faceY) * side > 0.0f;
        const float offset = (ballX - seats[seat].paddleX) / (Rally::PADDLE_HALF_WIDTH + Rally::BALL_HALF_SIZE);
        if (!bCrossed || offset < -1.0f || offset > 1.0f) {
            continue;
        }

        // Hitting near an edge sends the ball out at a steeper angle.
        float speed = (ballVelY > 0.0f ? ballVelY : -ballVelY) * SPEED_UP;
        speed = speed < MAX_SPEED ? speed : MAX_SPEED;
        ballVelY = -side * speed;
        ballVelX = offset * MAX_SIDE_SPEED * speed;
        ballY = faceY;
    }

    if (ballY < -Rally::COURT_HALF_HEIGHT) {
        ScorePoint(1);
    } else if (ballY > Rally::COURT_HALF_HEIGHT) {
        ScorePoint(0);
    }
}

void Samples::RallyServer::ScorePoint(int seat) {
    ++scores[seat];
    ME::Log("Rally: seat ", seat, " scores, ", static_cast<unsigned>(scores[0]), " - ",
            static_cast<unsigned>(scores[1]));
    if (scores[seat] >= Rally::WIN_SCORE) {
        phase = Phase::Won;
        phaseTime = 0.0;
        ballX = 0.0f;
        ballY = 0.0f;
        return;
    }
    StartServe(1 - seat);
}

void Samples::RallyServer::SendState() {
    NetProtocol::RallyState state;
    state.phase = static_cast<uint8_t>(phase);
    for (int i = 0; i < 2; ++i) {
        state.seatClients[i] = seats[i].clientID;
        state.scores[i] = scores[i];
        state.paddleX[i] = seats[i].paddleX;
    }
    state.ballX = ballX;
    state.ballY = ballY;

    // Each client gets its own clientID in the header, like every server reply.
    for (size_t i = 0; i < gameServer->GetClientCount(); ++i) {
        const uint8_t clientID = static_cast<uint8_t>(i);
        if (!IsConnected(clientID)) {
            continue;
        }
        uint8_t buffer[GameProtocol::MAX_DATAGRAM_SIZE];
        ME::ByteWriter writer(buffer, sizeof(buffer));
        GameProtocol::WriteHeader(writer, static_cast<uint8_t>(Verb::RALLY_STATE), clientID);
        NetProtocol::WriteRallyState(writer, state);
        gameServer->Send(buffer, writer.GetSize(), clientID);
    }
}
