#include "rally/game/game_rally.h"

#include "moksha/client/app/engine.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/serialization/byte_reader.h"
#include "moksha/shared/serialization/byte_writer.h"
#include "shared/rally/rally_rules.h"

using Samples::NetProtocol::Verb;
using Samples::Rally::Phase;

void Samples::GameRally::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    rallyScene = new SceneRally();
    scene = rallyScene;
    scene->Init();
    rallyScene->FitCamera(static_cast<float>(engine->window->width) / static_cast<float>(engine->window->height));

    rallyUI = new SceneUIRally();
    uiScene = rallyUI;
    uiScene->Init();

    bBot = engine->IsHeadless();
    engine->gameClient->SetListener(this);
    engine->gameClient->SendConnectRequest();
    ME::Log("Rally: started", bBot ? " as a bot" : "");
}

void Samples::GameRally::Update(double deltaTime) {
    Game::Update(deltaTime);

    if (engine->input->GetKeyPressed(ME::Input::KeyCode::Escape)) {
        engine->Quit();
    }

    // A CONNECT can be lost (UDP, or iOS dropping it while it asks for local network access), so keep asking until
    // the server answers.
    if (!engine->gameClient->IsConnected()) {
        connectRetryTimer += deltaTime;
        if (connectRetryTimer >= CONNECT_RETRY_SECONDS) {
            connectRetryTimer -= CONNECT_RETRY_SECONDS;
            engine->gameClient->SendConnectRequest();
        }
    }

    const float dt = static_cast<float>(deltaTime);
    const float x = bBot ? BotPaddleX(dt) : ReadPaddleX(dt);
    paddleX = x > Rally::PADDLE_MAX_X ? Rally::PADDLE_MAX_X : (x < -Rally::PADDLE_MAX_X ? -Rally::PADDLE_MAX_X : x);

    ShowState();
}

void Samples::GameRally::FixedUpdate(double fixedDeltaTime) {
    Game::FixedUpdate(fixedDeltaTime);

    // Sent every step, seated or not: it asks for a seat and tells the server this client is still here.
    if (!engine->gameClient->IsConnected()) {
        return;
    }
    NetProtocol::RallyInput input;
    input.paddleX = paddleX;
    input.bBot = bBot;

    uint8_t payload[8];
    ME::ByteWriter writer(payload, sizeof(payload));
    NetProtocol::WriteRallyInput(writer, input);
    engine->gameClient->SendGameMessage(static_cast<uint8_t>(Verb::RALLY_INPUT), payload, writer.GetSize());
}

void Samples::GameRally::End() {
    engine->gameClient->SetListener(nullptr);

    delete scene;
    scene = nullptr;
    rallyScene = nullptr;
    delete uiScene;
    uiScene = nullptr;
    rallyUI = nullptr;

    Game::End();
}

void Samples::GameRally::OnGameMessage(uint8_t /*senderClientID*/, uint8_t verb, ME::ByteReader& reader) {
    if (static_cast<Verb>(verb) == Verb::RALLY_STATE && NetProtocol::ReadRallyState(reader, state)) {
        bHaveState = true;
    }
}

const char* Samples::GameRally::GetDisplayName() const {
    return "Rally";
}

int Samples::GameRally::FindOwnSeat() const {
    if (!bHaveState) {
        return -1;
    }
    const uint8_t clientID = engine->gameClient->GetClientID();
    for (int i = 0; i < 2; ++i) {
        if (state.seatClients[i] == clientID) {
            return i;
        }
    }
    return -1;
}

float Samples::GameRally::ReadPaddleX(float deltaTime) const {
    const ME::Input::InputManager* input = engine->input;
    if (input->GetMouseButtonDown(ME::Input::MouseButton::Left)) {
        return rallyScene->ScreenToWorldX(input->GetMousePosNorm().x);
    }

    float x = paddleX;
    if (input->GetKeyDown(ME::Input::KeyCode::LeftArrow) || input->GetKeyDown(ME::Input::KeyCode::A)) {
        x -= KEY_SPEED * deltaTime;
    }
    if (input->GetKeyDown(ME::Input::KeyCode::RightArrow) || input->GetKeyDown(ME::Input::KeyCode::D)) {
        x += KEY_SPEED * deltaTime;
    }
    return x;
}

float Samples::GameRally::BotPaddleX(float deltaTime) const {
    const float flip = FindOwnSeat() == 1 ? -1.0f : 1.0f;
    const float toBall = state.ballX * flip - paddleX;
    const float step = BOT_SPEED * deltaTime;
    return paddleX + (toBall > step ? step : (toBall < -step ? -step : toBall));
}

void Samples::GameRally::ShowState() {
    // Seat 1 sees the court turned around, so both players defend the bottom of their screen.
    const int seat = FindOwnSeat();
    const int otherSeat = seat == 1 ? 0 : 1;
    const float flip = seat == 1 ? -1.0f : 1.0f;
    const Phase phase = static_cast<Phase>(state.phase);

    if (seat != shownSeat && bHaveState) {
        if (seat >= 0) {
            ME::Log("Rally: playing in seat ", seat, " as client ", static_cast<unsigned>(state.seatClients[seat]));
        } else {
            ME::Log("Rally: no seat, the match is full");
        }
        shownSeat = seat;
    }

    const bool bBallVisible = bHaveState && (phase == Phase::Serve || phase == Phase::Play);
    rallyScene->Place(paddleX, state.paddleX[otherSeat] * flip, state.ballX * flip, state.ballY * flip, bBallVisible);

    const uint8_t ownScore = state.scores[seat == 1 ? 1 : 0];
    const uint8_t otherScore = state.scores[otherSeat];
    if (ownScore != shownScores[0] || otherScore != shownScores[1]) {
        rallyUI->SetScore(ownScore, otherScore);
        shownScores[0] = ownScore;
        shownScores[1] = otherScore;
    }

    if (!engine->gameClient->IsConnected()) {
        SetMessage("Connecting...", "");
    } else if (!bHaveState) {
        SetMessage("Joining...", "");
    } else if (seat < 0) {
        SetMessage("Match full", "Two players are in");
    } else if (phase == Phase::Waiting) {
        SetMessage("Waiting for a partner", "Drag to move your paddle");
    } else if (phase == Phase::Won) {
        SetMessage(ownScore > otherScore ? "You win!" : "You lose", "New match soon");
    } else {
        SetMessage("", "");
    }
}

void Samples::GameRally::SetMessage(const char* message, const char* hint) {
    // Messages are string literals, so the pointer tells whether it changed.
    if (message == shownMessage) {
        return;
    }
    shownMessage = message;
    rallyUI->SetMessage(message);
    rallyUI->SetHint(hint);
    if (message[0] != '\0') {
        ME::Log("Rally: ", message);
    }
}
