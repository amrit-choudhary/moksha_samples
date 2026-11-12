/**
 * Wind tunnel scene. The engine has no runtime textures yet, so the fluid is drawn as instanced set 0: one white quad
 * per grid cell, coloured each frame by the view mode (smoke tinted by pressure, smoke, pressure, or speed), with solid
 * walls in grey and the obstacle in green with a dark rim.
 * Instanced set 1 holds the streamlines: short additive segments traced through the velocity field from a sparse seed
 * grid. Both sets are sized once for the largest grid; the draw counts limit what is uploaded and drawn.
 */

#pragma once

#include <cstdint>

#include "moksha/client/scene/scene.h"
#include "moksha/shared/misc/engine_limits.h"

namespace Samples {

class WindTunnelSim;

class SceneWindTunnel : public ME::Scene {
   public:
    enum class View : uint8_t { PressureSmoke, Smoke, Pressure, Speed, Count };

    virtual void CreateResources() override;
    virtual void BuildCamera() override;
    virtual void BuildInstancedSpriteTransforms() override;
    virtual void BuildInstancedSpriteRenderers() override;

    virtual const char* GetDisplayName() const override;

    // Places one cell sprite per grid cell, centred in the view; call after the grid changes size.
    void Layout(const WindTunnelSim& sim);

    // Colours every cell, and traces the streamlines when they are on.
    void Sync(const WindTunnelSim& sim, View view, bool bStreamlines);

    // From a point in view units (origin at the view's centre) to the simulation's domain units.
    float ToSimX(const WindTunnelSim& sim, float viewX) const;
    float ToSimY(float viewY) const;

    // Half the view's height in world units: the domain is 1 high and fills the view.
    static constexpr float VIEW_HALF_HEIGHT = 0.5f;

    // One instance per cell, so the grid is capped by the engine's instanced set size (about 236 rows at 16:9).
    static constexpr uint32_t MAX_CELLS = static_cast<uint32_t>(ME::Constants::MaxInstancedSpriteTransformCount);
    static constexpr uint32_t MAX_SEGMENTS = 40'000;

   private:
    void TraceStreamlines(const WindTunnelSim& sim);

    // Colour ranges, eased toward each frame's so the colours don't flicker.
    float pressureMid = 0.0f;
    float pressureDeviation = 1.0f;
    float speedHigh = 1.0f;
    bool bRangeValid = false;
};

}  // namespace Samples
