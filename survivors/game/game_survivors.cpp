#include "survivors/game/game_survivors.h"

#include <cstdio>

#include "moksha/client/app/app_config.h"
#include "moksha/client/app/engine.h"
#include "moksha/client/audio/audio_system.h"
#include "moksha/client/debug/debug_system.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/misc/delegate.h"
#include "moksha/shared/physics/physics_system.h"

void Samples::GameSurvivors::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    survivorsScene = new SceneSurvivors();
    scene = survivorsScene;
    scene->Init();
    scene->spriteCamera->aspectRatio =
        static_cast<float>(engine->window->width) / static_cast<float>(engine->window->height);

    uiScene = new ME::SceneUI();
    uiScene->Init();

    enemies = new ME::Pool<Enemy>(SceneSurvivors::MAX_ENEMIES);
    bullets = new ME::Pool<Bullet>(SceneSurvivors::MAX_BULLETS);

    physicsScene = new ME::PhysicsScene();
    physicsScene->Init(scene->staticColliders, scene->dynamicColliders);
    engine->physics->SetCollisionListener(this);
    engine->physics->SetScene(physicsScene);
}

void Samples::GameSurvivors::Start() {
    Game::Start();

    waveTimer = engine->timers->SetInterval(ME::Delegate::Make<GameSurvivors, &GameSurvivors::OnWaveTimer>(this),
                                            WAVE_INTERVAL_SECONDS);
    fireTimer = engine->timers->SetInterval(ME::Delegate::Make<GameSurvivors, &GameSurvivors::OnFireTimer>(this),
                                            FIRE_INTERVAL_SECONDS);
}

void Samples::GameSurvivors::Update(double deltaTime) {
    Game::Update(deltaTime);

    if (engine->input->GetKeyPressed(ME::Input::KeyCode::Escape)) {
        engine->Quit();
    }
    if (engine->input->GetKeyPressed(ME::Input::KeyCode::G)) {
        bGodMode = !bGodMode;
    }
    if (engine->input->GetKeyPressed(ME::Input::KeyCode::B)) {
        const uint8_t type = static_cast<uint8_t>(waveRandom.NextRange(0, ENEMY_TYPE_COUNT - 1));
        SpawnWave(type, BURST_COUNT, waveRandom.NextRange(0, 1'000'000'000));
    }

    const float dt = static_cast<float>(deltaTime);
    MovePlayer(dt);
    MoveEnemies(dt);
    MoveBullets(dt);

    if (invulnerableTime > 0.0f) {
        invulnerableTime -= dt;
    }
    if (bPlayerTouched) {
        bPlayerTouched = false;
        if (invulnerableTime <= 0.0f && !bGodMode) {
            --playerHealth;
            invulnerableTime = PLAYER_INVULNERABLE_SECONDS;
            if (playerHealth == 0) {
                ResetRound();
            }
        }
    }

    // Pool slots only move here, so the physics steps between two Updates see stable slot indices.
    ReleaseDead();
    survivorsScene->SyncPlayer();
    survivorsScene->SyncEnemies(*enemies);
    survivorsScene->SyncBullets(*bullets);

    PrintStats();
}

void Samples::GameSurvivors::End() {
    engine->timers->Clear(waveTimer);
    engine->timers->Clear(fireTimer);

    delete physicsScene;
    physicsScene = nullptr;
    delete enemies;
    enemies = nullptr;
    delete bullets;
    bullets = nullptr;
    delete scene;
    scene = nullptr;
    survivorsScene = nullptr;
    delete uiScene;
    uiScene = nullptr;

    Game::End();
}

void Samples::GameSurvivors::CollisionCallback(ME::ColliderAABB* a, ME::ColliderAABB* b,
                                               ME::CollisionResultAABB* result) {
    delete result;

    // The physics system passes the lower layer first: Player before Enemy, Enemy before Projectile.
    const ME::ColliderAABB* colliders = scene->dynamicColliders.data;
    const size_t indexA = static_cast<size_t>(a - colliders);
    const size_t indexB = static_cast<size_t>(b - colliders);

    if (indexA == SceneSurvivors::PLAYER_COLLIDER) {
        bPlayerTouched = true;
        return;
    }

    Enemy& enemy = (*enemies)[indexA - SceneSurvivors::FIRST_ENEMY_COLLIDER];
    Bullet& bullet = (*bullets)[indexB - SceneSurvivors::FIRST_BULLET_COLLIDER];
    if (enemy.bDead || bullet.bDead) {
        return;
    }

    bullet.bDead = true;
    b->isEnabled = false;
    enemy.health -= 1.0f;
    if (enemy.health <= 0.0f) {
        enemy.bDead = true;
        a->isEnabled = false;
        ++kills;
    }
    engine->audio->PlayAudio(SurvivorsSfx::HIT, false);
}

const char* Samples::GameSurvivors::GetDisplayName() const {
    return "Survivors";
}

void Samples::GameSurvivors::SpawnWave(uint8_t enemyType, uint16_t count, uint32_t seed) {
    if (enemyType >= ENEMY_TYPE_COUNT) {
        return;
    }

    // Spawn just inside the walls, spread along a random edge each.
    ME::Random spawnRandom{seed};
    const float halfWidth = survivorsScene->GetFloorHalfWidth() - SceneSurvivors::TILE_SIZE * 0.5f;
    const float halfHeight = survivorsScene->GetFloorHalfHeight() - SceneSurvivors::TILE_SIZE * 0.5f;

    for (uint16_t i = 0; i < count; ++i) {
        Enemy* enemy = enemies->Acquire();
        if (enemy == nullptr) {
            return;  // pool full
        }

        const float along = static_cast<float>(spawnRandom.NextDouble() * 2.0 - 1.0);
        ME::Vec2 position{};
        switch (spawnRandom.NextRange(0, 3)) {
            case 0:
                position = ME::Vec2{along * halfWidth, halfHeight};
                break;
            case 1:
                position = ME::Vec2{along * halfWidth, -halfHeight};
                break;
            case 2:
                position = ME::Vec2{-halfWidth, along * halfHeight};
                break;
            default:
                position = ME::Vec2{halfWidth, along * halfHeight};
                break;
        }

        *enemy = Enemy{};
        enemy->position = position;
        enemy->type = enemyType;
        enemy->health = ENEMY_TYPES[enemyType].health;
    }
}

void Samples::GameSurvivors::OnWaveTimer() {
    ++wave;
    if (wave > bestWave) {
        bestWave = wave;
    }

    const uint32_t count = WAVE_BASE_COUNT + wave * ENEMIES_PER_WAVE_STEP;
    const uint8_t type = static_cast<uint8_t>(waveRandom.NextRange(0, ENEMY_TYPE_COUNT - 1));
    SpawnWave(type, static_cast<uint16_t>(count < 0xFFFF ? count : 0xFFFF), waveRandom.NextRange(0, 1'000'000'000));
}

void Samples::GameSurvivors::OnFireTimer() {
    const ME::Vec3 playerPos = survivorsScene->GetPlayer().GetPosition();

    // Aim at the nearest living enemy.
    const Enemy* target = nullptr;
    float bestDistanceSq = 0.0f;
    for (size_t i = 0; i < enemies->GetActiveCount(); ++i) {
        const Enemy& enemy = (*enemies)[i];
        if (enemy.bDead) {
            continue;
        }
        const float dx = enemy.position.x - playerPos.x;
        const float dy = enemy.position.y - playerPos.y;
        const float distanceSq = dx * dx + dy * dy;
        if (target == nullptr || distanceSq < bestDistanceSq) {
            target = &enemy;
            bestDistanceSq = distanceSq;
        }
    }
    if (target == nullptr) {
        return;
    }

    Bullet* bullet = bullets->Acquire();
    if (bullet == nullptr) {
        return;
    }

    ME::Vec2 direction{target->position.x - playerPos.x, target->position.y - playerPos.y};
    direction.NormaliseSafe();
    *bullet = Bullet{};
    bullet->position = ME::Vec2{playerPos.x, playerPos.y};
    bullet->velocity = direction * BULLET_SPEED;
    engine->audio->PlayAudio(SurvivorsSfx::SHOOT, false);
}

void Samples::GameSurvivors::MovePlayer(float deltaTime) {
    const ME::Input::InputManager* input = engine->input;
    const float step = PLAYER_SPEED * deltaTime;
    ME::Vec3 pos = survivorsScene->GetPlayer().GetPosition();

    if (input->GetKeyDown(ME::Input::KeyCode::W)) {
        pos.y += step;
    }
    if (input->GetKeyDown(ME::Input::KeyCode::S)) {
        pos.y -= step;
    }
    if (input->GetKeyDown(ME::Input::KeyCode::A)) {
        pos.x -= step;
    }
    if (input->GetKeyDown(ME::Input::KeyCode::D)) {
        pos.x += step;
    }

    // The walls are drawn, not collided: keep the player on the floor.
    const float halfWidth = survivorsScene->GetFloorHalfWidth() - SceneSurvivors::TILE_SIZE * 0.5f;
    const float halfHeight = survivorsScene->GetFloorHalfHeight() - SceneSurvivors::TILE_SIZE * 0.5f;
    pos.x = pos.x < -halfWidth ? -halfWidth : (pos.x > halfWidth ? halfWidth : pos.x);
    pos.y = pos.y < -halfHeight ? -halfHeight : (pos.y > halfHeight ? halfHeight : pos.y);
    survivorsScene->GetPlayer().SetPosition(pos);
}

void Samples::GameSurvivors::MoveEnemies(float deltaTime) {
    const ME::Vec3 playerPos = survivorsScene->GetPlayer().GetPosition();
    for (size_t i = 0; i < enemies->GetActiveCount(); ++i) {
        Enemy& enemy = (*enemies)[i];
        ME::Vec2 toPlayer{playerPos.x - enemy.position.x, playerPos.y - enemy.position.y};
        toPlayer.NormaliseSafe();
        enemy.position += toPlayer * (ENEMY_TYPES[enemy.type].speed * deltaTime);
    }
}

void Samples::GameSurvivors::MoveBullets(float deltaTime) {
    const float halfWidth = survivorsScene->GetFloorHalfWidth();
    const float halfHeight = survivorsScene->GetFloorHalfHeight();
    for (size_t i = 0; i < bullets->GetActiveCount(); ++i) {
        Bullet& bullet = (*bullets)[i];
        bullet.position += bullet.velocity * deltaTime;
        if (bullet.position.x < -halfWidth || bullet.position.x > halfWidth || bullet.position.y < -halfHeight ||
            bullet.position.y > halfHeight) {
            bullet.bDead = true;
        }
    }
}

void Samples::GameSurvivors::ReleaseDead() {
    // Back to front: Release moves the last active item into the freed slot.
    for (size_t i = enemies->GetActiveCount(); i-- > 0;) {
        if ((*enemies)[i].bDead) {
            enemies->Release(&(*enemies)[i]);
        }
    }
    for (size_t i = bullets->GetActiveCount(); i-- > 0;) {
        if ((*bullets)[i].bDead) {
            bullets->Release(&(*bullets)[i]);
        }
    }
}

void Samples::GameSurvivors::ResetRound() {
    ME::Log("Survivors: died on wave ", wave, " with ", kills, " kills.");

    for (size_t i = 0; i < enemies->GetActiveCount(); ++i) {
        (*enemies)[i].bDead = true;
    }
    for (size_t i = 0; i < bullets->GetActiveCount(); ++i) {
        (*bullets)[i].bDead = true;
    }

    wave = 0;
    kills = 0;
    playerHealth = PLAYER_MAX_HEALTH;
    invulnerableTime = PLAYER_INVULNERABLE_SECONDS;
    survivorsScene->GetPlayer().SetPosition(0.0f, 0.0f, 0.0f);
}

void Samples::GameSurvivors::PrintStats() const {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Enemies: %zu / %zu", enemies->GetActiveCount(), SceneSurvivors::MAX_ENEMIES);
    ME::DebugSystem::ScreenPrintSlot(3, buffer);
    snprintf(buffer, sizeof(buffer), "Bullets: %zu", bullets->GetActiveCount());
    ME::DebugSystem::ScreenPrintSlot(4, buffer);
    snprintf(buffer, sizeof(buffer), "Wave: %u (best %u)", wave, bestWave);
    ME::DebugSystem::ScreenPrintSlot(5, buffer);
    snprintf(buffer, sizeof(buffer), "Health: %u  Kills: %u%s", playerHealth, kills, bGodMode ? "  [GOD]" : "");
    ME::DebugSystem::ScreenPrintSlot(6, buffer);
}
