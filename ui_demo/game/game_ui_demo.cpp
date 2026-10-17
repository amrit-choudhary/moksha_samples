#include "game_ui_demo.h"

#include <cstdio>

#include "moksha/client/app/engine.h"
#include "moksha/client/debug/debug_system.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/client/ui/button.h"
#include "moksha/logging/logging.h"

Samples::GameUIDemo::GameUIDemo() : Game() {}

Samples::GameUIDemo::~GameUIDemo() {}

void Samples::GameUIDemo::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    // Plain, un-subclassed Scene: no Build*() overrides, so no meshes/sprites/lights — renders
    // as a black background.
    scene = new ME::Scene();
    scene->Init();

    // SceneUIDemo::Init() builds the entire widget tree via its BuildUIElements() override —
    // see scene_ui_demo.cpp. uiScene (base Game's SceneUI*) and sceneUIDemo alias the same
    // object; sceneUIDemo just keeps the concrete type for GetHealthLabel()/GetScoreLabel().
    sceneUIDemo = new SceneUIDemo();
    sceneUIDemo->Init();
    uiScene = sceneUIDemo;

    ME::Delegate::Bind<GameUIDemo, &GameUIDemo::OnScoreButtonClick>(sceneUIDemo->GetScoreButton()->onClick, this);

    ME::Log("UI Demo Game Start!");
}

void Samples::GameUIDemo::Start() {
    Game::Start();
}

void Samples::GameUIDemo::Update(double deltaTime) {
    Game::Update(deltaTime);

    const ME::Input::InputManager* input = engine->input;
    bool healthChanged = false;
    bool scoreChanged = false;

    if (input->GetKeyPressed(ME::Input::KeyCode::D)) {
        ++health;
        healthChanged = true;
    }
    if (input->GetKeyPressed(ME::Input::KeyCode::A)) {
        --health;
        healthChanged = true;
    }
    if (input->GetKeyPressed(ME::Input::KeyCode::W)) {
        ++score;
        scoreChanged = true;
    }
    if (input->GetKeyPressed(ME::Input::KeyCode::S)) {
        --score;
        scoreChanged = true;
    }

    if (healthChanged) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Health: %d", health);
        sceneUIDemo->GetHealthLabel()->SetText(buf);
        ME::DebugSystem::ScreenPrintSlot(1, buf);
    }
    if (scoreChanged) {
        UpdateScoreLabel();
    }

    // Continuous WASD movement for the center image (held, not edge-triggered, unlike the
    // health/score keys above) — UI space is Y-down, so W/up decreases y and S/down increases it.
    constexpr float imageMoveSpeed = 200.0f;
    float moveDelta = static_cast<float>(imageMoveSpeed * deltaTime);
    if (input->GetKeyDown(ME::Input::KeyCode::D)) {
        imageOffset.x += moveDelta;
    }
    if (input->GetKeyDown(ME::Input::KeyCode::A)) {
        imageOffset.x -= moveDelta;
    }
    if (input->GetKeyDown(ME::Input::KeyCode::S)) {
        imageOffset.y += moveDelta;
    }
    if (input->GetKeyDown(ME::Input::KeyCode::W)) {
        imageOffset.y -= moveDelta;
    }
    sceneUIDemo->GetCenterImage()->SetOffset(imageOffset);
}

void Samples::GameUIDemo::UpdateScoreLabel() {
    char buf[32];
    snprintf(buf, sizeof(buf), "Score: %d", score);
    sceneUIDemo->GetScoreLabel()->SetText(buf);
    ME::DebugSystem::ScreenPrint(buf, 2.0f);
}

void Samples::GameUIDemo::OnScoreButtonClick() {
    score += 50;
    UpdateScoreLabel();
}

void Samples::GameUIDemo::End() {
    // sceneUIDemo and uiScene alias the same object — deleting uiScene destroys the widgets
    // SceneUIDemo owns (see SceneUIDemo::~SceneUIDemo()).
    delete uiScene;
    delete scene;

    Game::End();
    ME::Log("UI Demo Game End!");
}
