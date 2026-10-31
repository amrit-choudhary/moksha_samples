/**
 * Rally game: a two-player paddle ball match run by Samples_Server (server/rally_server.h). Each frame the client
 * sends where its paddle should be and draws the newest match state the server sent; no prediction or smoothing,
 * except that the own paddle is drawn where the player put it. Drag (or hold the mouse) or A/D/Left/Right to move.
 *
 * Headless (Sample_Rally_Headless) it plays as a bot that chases the ball; the server gives its seat to a human
 * when the match is full, so it can run all the time as the partner for a single phone.
 */

#pragma once

#include <cstdint>

#include "moksha/client/game/game.h"
#include "moksha/client/net/game_client.h"
#include "rally/scene/scene_rally.h"
#include "rally/scene/scene_ui_rally.h"
#include "shared/net/samples_protocol.h"

namespace Samples {

class GameRally : public ME::Game, public ME::IGameClientListener {
   public:
    virtual void Init(ME::Engine& inEngine) override;
    virtual void Update(double deltaTime) override;
    virtual void FixedUpdate(double fixedDeltaTime) override;
    virtual void End() override;

    // RALLY_STATE from the server.
    void OnGameMessage(uint8_t senderClientID, uint8_t verb, ME::ByteReader& reader) override;

    virtual const char* GetDisplayName() const override;

   private:
    // This client's seat in the newest state, or -1.
    int FindOwnSeat() const;

    // The paddle position the player asks for, in view coordinates. The one place that reads input.
    float ReadPaddleX(float deltaTime) const;

    // The bot's paddle: chases the ball at a limited speed, so it can miss.
    float BotPaddleX(float deltaTime) const;

    // Places the sprites and sets the UI from the newest state.
    void ShowState();
    void SetMessage(const char* message, const char* hint);

    static constexpr double CONNECT_RETRY_SECONDS = 1.0;
    static constexpr float KEY_SPEED = 90.0f;  // paddle speed with the keys, world units per second
    static constexpr float BOT_SPEED = 55.0f;

    SceneRally* rallyScene = nullptr;
    SceneUIRally* rallyUI = nullptr;

    NetProtocol::RallyState state;
    bool bHaveState = false;
    bool bBot = false;
    float paddleX = 0.0f;  // view coordinates
    double connectRetryTimer = 0.0;

    // What the UI shows, so labels change only when it does.
    int shownSeat = -2;
    const char* shownMessage = nullptr;
    uint8_t shownScores[2] = {0xFF, 0xFF};
};

}  // namespace Samples
