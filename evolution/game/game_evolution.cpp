#include "evolution/game/game_evolution.h"

#include "moksha/client/app/engine.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/physics/physics_system.h"

using namespace ME;

Samples::GameEvolution::GameEvolution() : Game() {}

Samples::GameEvolution::~GameEvolution() {}

void Samples::GameEvolution::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    evoScene = new Samples::SceneEvolution();
    scene = evoScene;
    scene->Init();

    evoUIScene = new Samples::SceneUIEvolution();
    uiScene = evoUIScene;
    uiScene->Init();

    evoPhysicsScene = new Samples::PhysicsSceneEvolution();
    evoPhysicsScene->Init(scene->staticColliders, scene->dynamicColliders);
    engine->physics->SetCollisionListener(this);
    engine->physics->SetScene(evoPhysicsScene);

    ME::Log("Evolution Game Start!");
}

void Samples::GameEvolution::Start() {
    Game::Start();
}

void Samples::GameEvolution::Update(double deltaTime) {
    Game::Update(deltaTime);

    evoScene->UpdateCreatures(static_cast<float>(deltaTime));

    const float speed = cameraSpeed * static_cast<float>(deltaTime);
    ME::Vec3 movementVector = ME::Vec3{0.0f, 0.0f, 0.0f};

    const ME::Input::InputManager* input = engine->input;
    if (input->GetKeyDown(ME::Input::KeyCode::W)) {
        movementVector.y += speed;
    }
    if (input->GetKeyDown(ME::Input::KeyCode::S)) {
        movementVector.y -= speed;
    }
    if (input->GetKeyDown(ME::Input::KeyCode::A)) {
        movementVector.x -= speed;
    }
    if (input->GetKeyDown(ME::Input::KeyCode::D)) {
        movementVector.x += speed;
    }

    evoScene->spriteCamera->position += movementVector;
    evoScene->spriteCamera->viewPosition += movementVector;
}

void Samples::GameEvolution::End() {
    delete evoPhysicsScene;
    evoPhysicsScene = nullptr;
    delete evoScene;
    evoScene = nullptr;
    delete evoUIScene;
    evoUIScene = nullptr;

    Game::End();
}

const char* Samples::GameEvolution::GetDisplayName() const {
    return "Evolution Game";
}
