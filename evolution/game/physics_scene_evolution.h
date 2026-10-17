/**
 * Physics scene for the Evolution game.
 */

#pragma once

#include "moksha/shared/physics/collider_aabb.h"
#include "moksha/shared/physics/physics_scene.h"

namespace Samples {

class PhysicsSceneEvolution : public ME::PhysicsScene {
   public:
    PhysicsSceneEvolution();
    virtual ~PhysicsSceneEvolution() override;

    // Initializes the physics scene with collider objects specific to Evolution.
    virtual void Init() override;

    // Initializes the physics scene with the given static/dynamic collider arrays specific to Evolution.
    virtual void Init(ME::Span<ME::ColliderAABB> inStaticColliders,
                      ME::Span<ME::ColliderAABB> inDynamicColliders) override;
};

}  // namespace Samples
