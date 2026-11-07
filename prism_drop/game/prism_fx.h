/**
 * Prism Drop particles: a fixed pool over the scene's effect slots, plus ambient motes drifting behind the well.
 * Each particle moves, spins, resizes and fades over its life; emitters build the game's effects from them. A full
 * pool reuses its oldest slot, so emitters never fail.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/rendering/shared/color.h"
#include "moksha/shared/random/random_engine.h"
#include "prism_drop/scene/scene_prism_drop.h"

namespace Samples {

class Particle {
   public:
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float gravity = 0.0f;  // units per second squared, y up
    float drag = 0.0f;     // velocity decay rate per second
    float angle = 0.0f;
    float spin = 0.0f;
    float width0 = 0.0f;  // size at birth and at death
    float height0 = 0.0f;
    float width1 = 0.0f;
    float height1 = 0.0f;
    float life = 0.0f;
    float maxLife = 1.0f;
    float fadeIn = 0.0f;  // fraction of the life spent fading in
    ME::Color color = ME::Color::White();
    uint16_t tile = 0;
    bool bActive = false;
};

class AmbientMote {
   public:
    float x = 0.0f;
    float y = 0.0f;
    float speed = 0.0f;
    float size = 0.0f;
    float phase = 0.0f;
    float twinkleRate = 0.0f;
    ME::Color color = ME::Color::White();
    uint16_t tile = 0;
};

class PrismFx {
   public:
    void Init(ScenePrismDrop* inScene);
    void Update(float deltaTime);

    // Ends every effect particle at once; the motes keep drifting.
    void Clear();

    // Multiplies the motes' rise speed (faster at higher levels).
    void SetAmbientSpeed(float multiplier) {
        ambientSpeed = multiplier;
    }

    // A freed slot with every field reset to the defaults; filled in by the caller.
    Particle& Emit();

    // A cell bursting apart: shards in its colour, sparks and a flash. power scales speed and count.
    void Shatter(float x, float y, const ME::Color& color, float power);
    // An expanding ring.
    void Ring(float x, float y, const ME::Color& color, float fromSize, float toSize, float seconds);
    // A vertical light trail between two heights, fading out.
    void Streak(float x, float yBottom, float yTop, float width, const ME::Color& color);
    // Puffs and specks thrown sideways from a landing.
    void Dust(float x, float y, float width, const ME::Color& color, int count);
    // Twinkles scattered over a rectangle.
    void Twinkles(float x0, float y0, float x1, float y1, const ME::Color& color, int count);
    // Confetti of every piece colour fired up from a point.
    void Fountain(float x, float y, float angle, int count);
    // One slow shard falling through the view, for the title screen.
    void TitleRain(float viewHalfWidth);

    float RandomRange(float low, float high);

   private:
    void UpdateAmbient(float deltaTime);
    void ResetMote(AmbientMote& mote, bool bAnywhere);

    ScenePrismDrop* scene = nullptr;
    Particle particles[ScenePrismDrop::EFFECT_SLOTS];
    AmbientMote motes[ScenePrismDrop::AMBIENT_SLOTS];
    size_t cursor = 0;
    float ambientSpeed = 1.0f;
    float time = 0.0f;
    ME::Random random{"prismFx", true};
};

}  // namespace Samples
