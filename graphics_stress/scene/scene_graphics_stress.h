/**
 * Graphics Stress scene: a spiral galaxy baked once at startup. Instanced set 0 holds the clouds (large, soft, the
 * overdraw load), set 1 the stars (small, the instance load). Sets are sized once, so unused instances have zero scale
 * and draw no pixels. Plain sprites are giant stars, one draw call each. The camera's roll costs no per-sprite work;
 * moving stars go through the renderer's dirty-instance update every frame.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/scene/scene.h"

namespace Samples {

class SceneGraphicsStress : public ME::Scene {
   public:
    virtual void CreateResources() override;
    virtual void BuildCamera() override;
    virtual void BuildSpriteTransforms() override;
    virtual void BuildSpriteRenderers() override;
    virtual void BuildInstancedSpriteTransforms() override;
    virtual void BuildInstancedSpriteRenderers() override;

    virtual const char* GetDisplayName() const override;

    // Show the first count instances of a set, marking only the ones that change. count is clamped to the maximum.
    void SetStarCount(uint32_t count);
    void SetCloudCount(uint32_t count);

    uint32_t GetStarCount() const {
        return starCount;
    }

    uint32_t GetCloudCount() const {
        return cloudCount;
    }

    // Multiplies every cloud's size. Fill load grows with its square; a change rewrites each visible cloud once.
    void SetCloudScale(float scale);

    // Orbits the first count shown stars by deltaTime and marks them dirty, as a moving game object would.
    void MoveStars(uint32_t count, float deltaTime);

    // Orbits the plain sprites, which the renderer re-reads every frame.
    void MoveGiants(float deltaTime);

    // Turns the view around the galaxy's centre; angle is in radians.
    void SetRoll(float angle);

    static constexpr uint32_t MAX_STARS = 100'000;
    static constexpr uint32_t MAX_CLOUDS = 5'000;
    static constexpr uint32_t GIANT_COUNT = 1'000;  // the engine's limit, MaxSpriteRendererCount

   private:
    uint32_t starCount = MAX_STARS;  // as built; Init leaves every instance visible until the game sets a count
    uint32_t cloudCount = MAX_CLOUDS;
    float cloudScale = 1.0f;

    float starSizes[MAX_STARS];
    float cloudSizes[MAX_CLOUDS];
    float starOrbitRates[MAX_STARS];  // radians per second
    float giantOrbitRates[GIANT_COUNT];
};

}  // namespace Samples
