#include "systems_check/scene/scene_systems_check.h"

#include "moksha/shared/physics/physics_layer.h"

void Samples::SceneSystemsCheck::Init() {
    ME::Scene::Init();
    BuildColliders();
}

void Samples::SceneSystemsCheck::CreateResources() {
    ME::Scene::CreateResources();

    sfxPaths[SystemsCheckSfx::BEEP] = "audio/beep.wav";
    sfxPaths.count = 1;
}

void Samples::SceneSystemsCheck::BuildColliders() {
    using ME::PhysicsLayer;

    const float moverX = MOVER_START_X;
    dynamicColliders[MOVER_COLLIDER] = ME::ColliderAABB(MOVER_COLLIDER, true, false, PhysicsLayer::Player,
                                                        PhysicsLayer::Enemy, moverX - 0.5f, -0.5f, moverX + 0.5f, 0.5f);
    dynamicColliders[TARGET_COLLIDER] =
        ME::ColliderAABB(TARGET_COLLIDER, true, false, PhysicsLayer::Enemy, PhysicsLayer::Player, TARGET_X - 0.5f,
                         -0.5f, TARGET_X + 0.5f, 0.5f);
    // Overlaps the target from the start, but its mask accepts no layer, so it must never be reported.
    dynamicColliders[DECOY_COLLIDER] =
        ME::ColliderAABB(DECOY_COLLIDER, true, false, PhysicsLayer::Layer0, PhysicsLayer::None, TARGET_X - 0.5f, -0.5f,
                         TARGET_X + 0.5f, 0.5f);
    dynamicColliders.count = COLLIDER_COUNT;
}

void Samples::SceneSystemsCheck::MoveMover(float deltaX) {
    ME::ColliderAABB& mover = dynamicColliders[MOVER_COLLIDER];
    mover.minX += deltaX;
    mover.maxX += deltaX;
}
