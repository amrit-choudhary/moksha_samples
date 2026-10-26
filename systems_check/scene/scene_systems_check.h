/**
 * Systems check scene: nothing to draw, only data for the non-graphics systems. Three dynamic colliders for the
 * physics check (a mover, a target in its path, and a decoy overlapping the target whose layers never collide)
 * and one sound effect.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/scene/scene.h"

namespace Samples {

// Sound ids: indices into sfxPaths.
namespace SystemsCheckSfx {
constexpr uint8_t BEEP = 0;
}  // namespace SystemsCheckSfx

class SceneSystemsCheck : public ME::Scene {
   public:
    virtual void Init() override;
    virtual void CreateResources() override;

    // Moves the mover collider along X; it starts at MOVER_START_X.
    void MoveMover(float deltaX);

    // Dynamic collider slots.
    static constexpr size_t MOVER_COLLIDER = 0;
    static constexpr size_t TARGET_COLLIDER = 1;
    static constexpr size_t DECOY_COLLIDER = 2;
    static constexpr size_t COLLIDER_COUNT = 3;

    // Unit boxes on the X axis: the mover touches the target after travelling 8 units.
    static constexpr float MOVER_START_X = -5.0f;
    static constexpr float TARGET_X = 4.0f;

   private:
    void BuildColliders();
};

}  // namespace Samples
