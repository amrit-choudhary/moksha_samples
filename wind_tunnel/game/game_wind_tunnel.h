/**
 * Wind tunnel game: an interactive 2D fluid demo after Ten Minute Physics #17. Air blows in from the left with smoke
 * streaks; drag the obstacle (circle, square, plate, NACA airfoil, aircraft or car) with the mouse or a finger and watch
 * the wake follow. Up/Down (or Q/E) turn it; N steps through NACA codes. Lift and drag coefficients are shown, smoothed.
 * The view shows pressure under the smoke, smoke alone, pressure, or speed; streamlines can be drawn on top. The simulation runs fixed steps of 1/60 s (at most two a frame). No physics, audio or network.
 * Headless, it drags the obstacle along a fixed path for BENCH_STEPS steps at each grid size, logging the time per step
 * and a checksum, then quits.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/game/game.h"
#include "wind_tunnel/game/wind_tunnel_sim.h"
#include "wind_tunnel/scene/scene_ui_wind_tunnel.h"
#include "wind_tunnel/scene/scene_wind_tunnel.h"

namespace Samples {

class GameWindTunnel : public ME::Game {
   public:
    virtual void Init(ME::Engine& inEngine) override;
    virtual void Update(double deltaTime) override;
    virtual void End() override;

    virtual const char* GetDisplayName() const override;

   private:
    using View = SceneWindTunnel::View;
    enum class Shape : uint8_t { Circle, Square, Plate, Airfoil, Aircraft, Car, Count };

    // Button callbacks (and their keys).
    void OnShape();
    void OnFoil();
    void OnView();
    void OnLines();
    void OnSmoke();
    void OnSlower();
    void OnFaster();

    void ReadKeys(float deltaTime);
    void ReadPointer(float deltaTime);
    void SelectShape(Shape newShape);
    void ShowForces();
    void ApplyRows();
    void ShowInfo();

    // The mouse in the simulation's domain units.
    void PointerToSim(float& outX, float& outY) const;

    // One headless step along the scripted path; logs and quits when done.
    void Benchmark();

    static constexpr float STEP = 1.0f / 60.0f;
    static constexpr uint32_t MAX_STEPS_PER_FRAME = 2;
    static constexpr float TURN_SPEED = 1.2f;   // radians per second, Q and E
    static constexpr float FORCE_EASE = 0.03f;  // per step, toward the step's lift and drag
    static constexpr double INFO_PERIOD = 0.25;
    static constexpr uint32_t BENCH_STEPS = 300;

    SceneWindTunnel* tunnelScene = nullptr;
    SceneUIWindTunnel* tunnelUI = nullptr;
    WindTunnelSim sim;

    float aspect = 16.0f / 9.0f;
    size_t detailIndex = 2;  // into DETAIL_ROWS
    size_t speedIndex = 3;
    size_t swirlIndex = 1;
    View view = View::PressureSmoke;
    bool bStreamlines = false;

    Shape shape = Shape::Airfoil;
    size_t nacaIndex = 0;
    bool bDragging = false;
    float obstacleAngle = 0.0f;
    float lift = 0.0f;
    float drag = 0.0f;

    bool bBenchmark = false;
    uint32_t benchStep = 0;

    double stepDebt = 0.0;
    double infoTime = 0.0;
    double simTimeSum = 0.0;
    uint32_t simSteps = 0;
    double shownSimTime = 0.0;
    int32_t shownFps = -1;
};

}  // namespace Samples
