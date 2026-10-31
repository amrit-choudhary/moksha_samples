/**
 * Server side of the rally sample: two seats, the ball and the score. A client is connected while its RALLY_INPUT
 * keeps arriving; one silent for CLIENT_TIMEOUT_SECONDS loses its seat. Free seats go to whoever sends input, and a
 * human takes a bot's seat when the match is full. The match state goes to every connected rally client each tick.
 */
#pragma once

#include <cstdint>

#include "moksha/server/game_server.h"
#include "moksha/shared/misc/engine_limits.h"
#include "moksha/shared/time/timer_manager.h"
#include "shared/net/samples_protocol.h"
#include "shared/rally/rally_rules.h"

namespace Samples {

class RallyServer : public ME::IGameServerListener {
   public:
    // Starts the tick timer.
    void OnStart(ME::ServerApp& app) override;

    // Stops the tick timer.
    void OnEnd() override;

    // RALLY_INPUT: refreshes the sender's heartbeat, seats it if it can sit, and moves its paddle.
    void OnGameMessage(uint8_t clientID, uint8_t verb, ME::ByteReader& reader) override;

   private:
    class Seat {
       public:
        uint8_t clientID = Rally::NO_CLIENT;
        bool bBot = false;
        float paddleX = 0.0f;  // court coordinates
    };

    // Interval timer callback, every fixed step: drops silent clients, runs the match and sends its state.
    void OnTick();

    // Puts clientID in a free seat, or in a bot's seat if it is human. False if every seat is taken.
    bool TrySeat(uint8_t clientID, bool bBot);
    int FindSeat(uint8_t clientID) const;
    bool IsConnected(uint8_t clientID) const;

    void DropSilentClients();
    void StartMatch();
    void StartServe(int towardSeat);
    void MoveBall(float deltaTime);
    void ScorePoint(int seat);
    void SendState();

    static constexpr double TICK_SECONDS = 1.0 / 60.0;
    static constexpr double CLIENT_TIMEOUT_SECONDS = 2.0;
    static constexpr double SERVE_DELAY_SECONDS = 1.0;
    static constexpr double WON_DELAY_SECONDS = 3.0;
    static constexpr float SERVE_SPEED = 80.0f;  // court units per second
    static constexpr float MAX_SPEED = 180.0f;
    static constexpr float SPEED_UP = 1.06f;       // per paddle hit
    static constexpr float MAX_SIDE_SPEED = 1.2f;  // sideways speed per unit forward speed, at a paddle's edge

    ME::GameServer* gameServer = nullptr;
    ME::Time::TimerManager* timers = nullptr;
    ME::Time::TimerHandle tickTimer;

    double now = 0.0;
    double lastHeard[ME::Constants::MaxServerClients] = {};  // per clientID; 0 = never heard
    Seat seats[2];

    Rally::Phase phase = Rally::Phase::Waiting;
    double phaseTime = 0.0;  // seconds since the phase began
    int serveToward = 0;     // the seat the next serve heads to
    uint8_t scores[2] = {0, 0};
    float ballX = 0.0f;
    float ballY = 0.0f;
    float ballVelX = 0.0f;
    float ballVelY = 0.0f;
};

}  // namespace Samples
