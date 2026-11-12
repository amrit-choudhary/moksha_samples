/**
 * Particle Life scene: one glowing sprite per particle in instanced set 0, blended additively, so where particles
 * crowd the colours add up toward white. The set is sized for ParticleLifeSim::MAX_PARTICLES once; the draw count
 * limits the upload and draw to the live particles. Additive sprites don't write depth, so they all share one z.
 */

#pragma once

#include <cstdint>

#include "moksha/client/scene/scene.h"

namespace Samples {

class ParticleLifeSim;

class SceneParticleLife : public ME::Scene {
   public:
    virtual void CreateResources() override;
    virtual void BuildCamera() override;
    virtual void BuildInstancedSpriteTransforms() override;
    virtual void BuildInstancedSpriteRenderers() override;

    virtual const char* GetDisplayName() const override;

    // Copies every live particle into its sprite: position, size, and a colour from its type, brighter when fast.
    void Sync(const ParticleLifeSim& sim);

    // Half the view's height in world units; the world fills the view.
    static constexpr float VIEW_HALF_HEIGHT = 100.0f;
};

}  // namespace Samples
