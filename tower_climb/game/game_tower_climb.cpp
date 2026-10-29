#include "tower_climb/game/game_tower_climb.h"

#include <cmath>

#include "moksha/client/app/engine.h"
#include "moksha/client/audio/audio_system.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/file_io/vfs.h"
#include "moksha/shared/math/math.h"
#include "moksha/shared/physics/physics_system.h"
#include "moksha/shared/serialization/byte_reader.h"
#include "moksha/shared/serialization/byte_writer.h"

void Samples::GameTowerClimb::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    towerScene = new SceneTowerClimb();
    scene = towerScene;
    scene->Init();
    scene->spriteCamera->aspectRatio =
        static_cast<float>(engine->window->width) / static_cast<float>(engine->window->height);

    towerUI = new SceneUITowerClimb();
    uiScene = towerUI;
    uiScene->Init();

    physicsScene = new ME::PhysicsScene();
    physicsScene->Init(scene->staticColliders, scene->dynamicColliders);
    engine->physics->SetCollisionListener(this);
    engine->physics->SetScene(physicsScene);
}

void Samples::GameTowerClimb::Start() {
    Game::Start();

    LoadBest();
    towerUI->ShowReady(bestMeters);
}

void Samples::GameTowerClimb::Update(double deltaTime) {
    Game::Update(deltaTime);

    if (engine->input->GetKeyPressed(ME::Input::KeyCode::Escape)) {
        engine->Quit();
    }

    const Side tap = ReadTap();
    switch (state) {
        case State::Ready:
            if (tap != Side::None) {
                StartClimb();
                LeapTo(tap);
            }
            break;
        case State::Playing:
            if (tap != Side::None) {
                LeapTo(tap);
            }
            break;
        case State::Dead:
            deadTime += deltaTime;
            if (tap != Side::None && deadTime >= RETRY_DELAY_SECONDS) {
                StartClimb();
            }
            break;
    }
}

void Samples::GameTowerClimb::FixedUpdate(double fixedDeltaTime) {
    Game::FixedUpdate(fixedDeltaTime);

    if (state != State::Playing) {
        return;
    }
    if (bHit) {
        Die();
        return;
    }

    const float dt = static_cast<float>(fixedDeltaTime);
    const uint32_t metersBefore = Meters();
    const float scroll = ClimbSpeed() * dt;
    climbed += scroll;
    towerScene->ScrollShaft(scroll);

    untilNextSpawn -= scroll;
    if (untilNextSpawn <= 0.0f) {
        SpawnHazard();
        untilNextSpawn += SpawnGap();
    }

    MoveClimber(dt);
    MoveHazards(scroll, dt);

    // Physics runs after this step and reports overlaps against these positions.
    towerScene->SyncClimber();
    towerScene->SyncHazards(hazards, SceneTowerClimb::MAX_HAZARDS);

    if (Meters() != metersBefore) {
        towerUI->SetHeight(Meters());
    }
}

void Samples::GameTowerClimb::End() {
    delete physicsScene;
    physicsScene = nullptr;
    delete scene;
    scene = nullptr;
    towerScene = nullptr;
    delete uiScene;
    uiScene = nullptr;
    towerUI = nullptr;

    Game::End();
}

void Samples::GameTowerClimb::CollisionCallback(ME::ColliderAABB*, ME::ColliderAABB*,
                                                ME::CollisionResultAABB* result) {
    delete result;
    bHit = true;
}

const char* Samples::GameTowerClimb::GetDisplayName() const {
    return "Tower Climb";
}

Samples::GameTowerClimb::Side Samples::GameTowerClimb::ReadTap() const {
    const ME::Input::InputManager* input = engine->input;
    if (input->GetMouseButtonPressed(ME::Input::MouseButton::Left)) {
        return input->GetMousePosNorm().x < 0.5f ? Side::Left : Side::Right;
    }
    if (input->GetKeyPressed(ME::Input::KeyCode::LeftArrow) || input->GetKeyPressed(ME::Input::KeyCode::A)) {
        return Side::Left;
    }
    if (input->GetKeyPressed(ME::Input::KeyCode::RightArrow) || input->GetKeyPressed(ME::Input::KeyCode::D)) {
        return Side::Right;
    }
    return Side::None;
}

void Samples::GameTowerClimb::StartClimb() {
    for (Hazard& hazard : hazards) {
        hazard.bActive = false;
    }
    towerScene->SyncHazards(hazards, SceneTowerClimb::MAX_HAZARDS);

    wall = Side::Left;
    leapTarget = Side::None;
    leapTime = 0.0f;
    climbTime = 0.0f;
    climbed = 0.0;
    untilNextSpawn = START_SPAWN_GAP;
    bHit = false;
    towerScene->GetClimber().SetPosition(-SceneTowerClimb::CLING_X, SceneTowerClimb::CLIMBER_Y, 0.0f);
    towerScene->GetClimberRenderer().ToggleFlipHorizontal(true);
    towerScene->SyncClimber();

    state = State::Playing;
    towerUI->ShowPlaying();
    towerUI->SetHeight(0);
}

void Samples::GameTowerClimb::Die() {
    state = State::Dead;
    deadTime = 0.0;
    bHit = false;
    engine->audio->PlayAudio(TowerSfx::HIT, false);

    const uint32_t meters = Meters();
    const bool bNewBest = meters > bestMeters;
    if (bNewBest) {
        bestMeters = meters;
        SaveBest();
    }
    towerUI->ShowGameOver(meters, bestMeters, bNewBest);
    ME::Log("Tower Climb: fell at ", meters, " m (best ", bestMeters, " m).");
}

void Samples::GameTowerClimb::LeapTo(Side side) {
    // Taps on the clinging wall, or during a leap, do nothing.
    if (side == wall || leapTarget != Side::None) {
        return;
    }
    leapTarget = side;
    leapTime = 0.0f;
    towerScene->GetClimberRenderer().ToggleFlipHorizontal(side == Side::Left);
    engine->audio->PlayAudio(TowerSfx::JUMP, false);
}

void Samples::GameTowerClimb::MoveClimber(float deltaTime) {
    const float leftX = -SceneTowerClimb::CLING_X;
    const float rightX = SceneTowerClimb::CLING_X;
    float x = wall == Side::Left ? leftX : rightX;
    float y = SceneTowerClimb::CLIMBER_Y;

    if (leapTarget != Side::None) {
        leapTime += deltaTime;
        const float t = leapTime < LEAP_SECONDS ? leapTime / LEAP_SECONDS : 1.0f;
        const float toX = leapTarget == Side::Left ? leftX : rightX;
        x += (toX - x) * t;
        y += std::sin(t * static_cast<float>(ME::PI)) * LEAP_ARC;
        if (t >= 1.0f) {
            wall = leapTarget;
            leapTarget = Side::None;
        }
    } else {
        // A small bob reads as climbing, since the art has one frame per character.
        climbTime += deltaTime;
        y += std::sin(climbTime * 14.0f) * 0.75f;
    }

    towerScene->GetClimber().SetPosition(x, y, 0.0f);
}

void Samples::GameTowerClimb::MoveHazards(float scroll, float deltaTime) {
    const float bottom = -SceneTowerClimb::VIEW_HALF_HEIGHT - SceneTowerClimb::TILE_SIZE;
    for (Hazard& hazard : hazards) {
        if (!hazard.bActive) {
            continue;
        }
        hazard.y -= scroll + HAZARD_TYPES[hazard.type].fallSpeed * deltaTime;
        if (hazard.y < bottom) {
            hazard.bActive = false;
        }
    }
}

void Samples::GameTowerClimb::SpawnHazard() {
    Hazard* slot = nullptr;
    for (Hazard& hazard : hazards) {
        if (!hazard.bActive) {
            slot = &hazard;
            break;
        }
    }
    if (slot == nullptr) {
        return;  // pool full
    }

    uint8_t type = static_cast<uint8_t>(hazardRandom.NextRange(0, HAZARD_DROPPER - 1));
    if (Meters() >= DROPPER_FROM_METERS && hazardRandom.NextRange(0, 99) < DROPPER_CHANCE) {
        type = HAZARD_DROPPER;
    }

    slot->type = type;
    slot->x = hazardRandom.NextRange(0, 1) == 0 ? -SceneTowerClimb::CLING_X : SceneTowerClimb::CLING_X;
    slot->y = SceneTowerClimb::VIEW_HALF_HEIGHT + SceneTowerClimb::TILE_SIZE;
    slot->bActive = true;
}

float Samples::GameTowerClimb::ClimbSpeed() const {
    const float speed = START_SPEED + static_cast<float>(Meters()) * SPEED_PER_METER;
    return speed < MAX_SPEED ? speed : MAX_SPEED;
}

float Samples::GameTowerClimb::SpawnGap() const {
    const float gap = START_SPAWN_GAP - static_cast<float>(Meters()) * SPAWN_GAP_PER_METER;
    return gap > MIN_SPAWN_GAP ? gap : MIN_SPAWN_GAP;
}

uint32_t Samples::GameTowerClimb::Meters() const {
    return static_cast<uint32_t>(climbed / SceneTowerClimb::TILE_SIZE);
}

void Samples::GameTowerClimb::LoadBest() {
    uint8_t buffer[16];
    size_t size = 0;
    if (ME::Vfs::ReadBytes(ME::FileRoot::UserData, SAVE_PATH, buffer, sizeof(buffer), size)) {
        ME::ByteReader reader(buffer, size);
        reader.ReadU32(bestMeters);
    }
}

void Samples::GameTowerClimb::SaveBest() const {
    uint8_t buffer[16];
    ME::ByteWriter writer(buffer, sizeof(buffer));
    writer.WriteU32(bestMeters);
    if (!ME::Vfs::WriteBytes(ME::FileRoot::UserData, SAVE_PATH, buffer, writer.GetSize())) {
        ME::LogWarning("Tower Climb: couldn't save the best height.");
    }
}
