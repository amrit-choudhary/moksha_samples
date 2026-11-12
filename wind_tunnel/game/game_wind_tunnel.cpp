#include "wind_tunnel/game/game_wind_tunnel.h"

#include <chrono>
#include <cmath>
#include <cstdio>

#include "moksha/client/app/engine.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/time/time_manager.h"
#include "wind_tunnel/game/wind_tunnel_shapes.h"

namespace {

using InputManager = ME::Input::InputManager;
using KeyCode = ME::Input::KeyCode;

// H steps through these grid heights. The top one is cut to what fits the scene's cells at the window's aspect.
constexpr uint32_t DETAIL_ROWS[] = {112, 180, 240};
constexpr size_t DETAIL_COUNT = sizeof(DETAIL_ROWS) / sizeof(DETAIL_ROWS[0]);

// The most rows whose grid (rows high, aspect * rows wide) fits in maxCells.
uint32_t RowsThatFit(uint32_t rows, float aspect, uint32_t maxCells) {
    while (rows > Samples::WindTunnelSim::MIN_ROWS &&
           static_cast<uint32_t>(std::lround(aspect * static_cast<float>(rows))) * rows > maxCells) {
        --rows;
    }
    return rows;
}

constexpr float SPEEDS[] = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
constexpr size_t SPEED_COUNT = sizeof(SPEEDS) / sizeof(SPEEDS[0]);

// Vorticity confinement: off is Ten Minute Physics' own look.
constexpr float SWIRLS[] = {0.0f, 0.6f, 1.5f};
constexpr const char* SWIRL_NAMES[] = {"off", "low", "high"};
constexpr size_t SWIRL_COUNT = sizeof(SWIRLS) / sizeof(SWIRLS[0]);

constexpr const char* SHAPE_NAMES[] = {"circle", "square", "plate", "NACA", "aircraft", "car"};

// N steps through these; any 4-digit or standard 5-digit code works.
constexpr const char* NACA_CODES[] = {"2412", "0012", "4412", "6412", "0006", "23012", "0024"};
constexpr size_t NACA_COUNT = sizeof(NACA_CODES) / sizeof(NACA_CODES[0]);

// Sizes in domain heights, and the length each one's coefficients are measured against.
constexpr float CIRCLE_RADIUS = 0.12f;
constexpr float SQUARE_HALF = 0.09f;
constexpr float PLATE_HALF_LENGTH = 0.15f;
constexpr float PLATE_HALF_THICKNESS = 0.016f;
constexpr float CHORD = 0.5f;
constexpr float AIRCRAFT_LENGTH = 0.75f;
constexpr float CAR_LENGTH = 0.6f;

constexpr float DEGREES = 57.29578f;
constexpr float DEGREE = 1.0f / DEGREES;
constexpr const char* VIEW_NAMES[] = {"pressure + smoke", "smoke", "pressure", "speed"};

// Where the obstacle starts; the airfoil starts 8 degrees nose up (a negative, clockwise turn).
constexpr float START_X = 0.45f;
constexpr float AIRFOIL_ANGLE = -8.0f * DEGREE;

}  // namespace

void Samples::GameWindTunnel::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    tunnelScene = new SceneWindTunnel();
    scene = tunnelScene;
    scene->Init();
    const float width = static_cast<float>(engine->window->width);
    const float height = static_cast<float>(engine->window->height);
    aspect = height > 0.0f ? width / height : aspect;
    scene->spriteCamera->aspectRatio = aspect;

    tunnelUI = new SceneUIWindTunnel();
    uiScene = tunnelUI;
    uiScene->Init();

    bBenchmark = engine->IsHeadless();
    if (bBenchmark) {
        detailIndex = 0;  // then every level up
        swirlIndex = 0;
        ME::Log("Wind Tunnel benchmark: ", BENCH_STEPS, " steps of 1/60 s per grid, the obstacle on a fixed path");
    }
    obstacleAngle = AIRFOIL_ANGLE;
    ApplyRows();

    using UI = SceneUIWindTunnel;
    ME::Delegate::Bind<GameWindTunnel, &GameWindTunnel::OnShape>(tunnelUI->GetButton(UI::ButtonId::Shape)->onClick,
                                                                 this);
    ME::Delegate::Bind<GameWindTunnel, &GameWindTunnel::OnFoil>(tunnelUI->GetButton(UI::ButtonId::Foil)->onClick, this);
    ME::Delegate::Bind<GameWindTunnel, &GameWindTunnel::OnView>(tunnelUI->GetButton(UI::ButtonId::View)->onClick, this);
    ME::Delegate::Bind<GameWindTunnel, &GameWindTunnel::OnLines>(tunnelUI->GetButton(UI::ButtonId::Lines)->onClick,
                                                                 this);
    ME::Delegate::Bind<GameWindTunnel, &GameWindTunnel::OnSmoke>(tunnelUI->GetButton(UI::ButtonId::Smoke)->onClick,
                                                                 this);
    ME::Delegate::Bind<GameWindTunnel, &GameWindTunnel::OnSlower>(tunnelUI->GetButton(UI::ButtonId::Slower)->onClick,
                                                                  this);
    ME::Delegate::Bind<GameWindTunnel, &GameWindTunnel::OnFaster>(tunnelUI->GetButton(UI::ButtonId::Faster)->onClick,
                                                                  this);
}

void Samples::GameWindTunnel::Update(double deltaTime) {
    Game::Update(deltaTime);

    if (bBenchmark) {
        Benchmark();
        return;
    }

    const float dt = static_cast<float>(deltaTime);
    ReadKeys(dt);
    ReadPointer(dt);

    // Fixed steps, so the flow looks the same at any frame rate; a slow machine runs slow rather than piling up.
    stepDebt += deltaTime;
    uint32_t steps = 0;
    const auto start = std::chrono::steady_clock::now();
    while (stepDebt >= STEP && steps < MAX_STEPS_PER_FRAME) {
        sim.Step(STEP);
        lift += (sim.GetLift() - lift) * FORCE_EASE;
        drag += (sim.GetDrag() - drag) * FORCE_EASE;
        stepDebt -= STEP;
        ++steps;
    }
    if (steps == MAX_STEPS_PER_FRAME) {
        stepDebt = 0.0;
    }
    const auto stop = std::chrono::steady_clock::now();
    if (steps > 0) {
        simTimeSum += std::chrono::duration<double, std::milli>(stop - start).count();
        simSteps += steps;
        // Only after a step: on a 120 Hz display every other frame has none, and recolouring every cell costs.
        tunnelScene->Sync(sim, view, bStreamlines);
    }

    infoTime += deltaTime;
    if (infoTime >= INFO_PERIOD) {
        shownSimTime = simSteps > 0 ? simTimeSum / simSteps : 0.0;
        ShowInfo();
        ShowForces();
        infoTime = 0.0;
        simTimeSum = 0.0;
        simSteps = 0;
    }

    const double fps = engine->time->GetCurrentFPS();
    const int32_t roundedFps = static_cast<int32_t>(std::lround(fps));
    if (roundedFps != shownFps) {
        shownFps = roundedFps;
        tunnelUI->SetFps(fps);
    }
}

void Samples::GameWindTunnel::End() {
    delete scene;
    scene = nullptr;
    tunnelScene = nullptr;
    delete uiScene;
    uiScene = nullptr;
    tunnelUI = nullptr;

    Game::End();
}

const char* Samples::GameWindTunnel::GetDisplayName() const {
    return "Wind Tunnel";
}

void Samples::GameWindTunnel::OnShape() {
    const uint8_t next = (static_cast<uint8_t>(shape) + 1) % static_cast<uint8_t>(Shape::Count);
    SelectShape(static_cast<Shape>(next));
}

void Samples::GameWindTunnel::OnFoil() {
    // From another shape, back to the airfoil it had; on the airfoil, the next code.
    if (shape == Shape::Airfoil) {
        nacaIndex = (nacaIndex + 1) % NACA_COUNT;
    }
    SelectShape(Shape::Airfoil);
}

void Samples::GameWindTunnel::OnView() {
    view = static_cast<View>((static_cast<uint8_t>(view) + 1) % static_cast<uint8_t>(View::Count));
    ShowInfo();
}

void Samples::GameWindTunnel::OnLines() {
    bStreamlines = !bStreamlines;
    ShowInfo();
}

void Samples::GameWindTunnel::OnSmoke() {
    using Source = WindTunnelSim::SmokeSource;
    const uint8_t next = (static_cast<uint8_t>(sim.GetSmokeSource()) + 1) % static_cast<uint8_t>(Source::Count);
    sim.SetSmokeSource(static_cast<Source>(next));
    ShowInfo();
}

void Samples::GameWindTunnel::OnSlower() {
    if (speedIndex > 0) {
        --speedIndex;
        sim.SetInflowSpeed(SPEEDS[speedIndex]);
        ShowInfo();
    }
}

void Samples::GameWindTunnel::OnFaster() {
    if (speedIndex + 1 < SPEED_COUNT) {
        ++speedIndex;
        sim.SetInflowSpeed(SPEEDS[speedIndex]);
        ShowInfo();
    }
}

void Samples::GameWindTunnel::ReadKeys(float deltaTime) {
    ME::Input::InputManager& input = *engine->input;
    if (input.GetKeyPressed(KeyCode::Escape)) {
        engine->Quit();
    }
    const KeyCode shapeKeys[] = {KeyCode::Alpha1, KeyCode::Alpha2, KeyCode::Alpha3,
                                 KeyCode::Alpha4, KeyCode::Alpha5, KeyCode::Alpha6};
    static_assert(sizeof(shapeKeys) / sizeof(shapeKeys[0]) == static_cast<size_t>(Shape::Count));
    for (uint8_t k = 0; k < static_cast<uint8_t>(Shape::Count); ++k) {
        if (input.GetKeyPressed(shapeKeys[k])) {
            SelectShape(static_cast<Shape>(k));
        }
    }
    if (input.GetKeyPressed(KeyCode::N)) {
        OnFoil();
    }
    if (input.GetKeyPressed(KeyCode::C)) {
        OnView();
    }
    if (input.GetKeyPressed(KeyCode::S)) {
        OnLines();
    }
    if (input.GetKeyPressed(KeyCode::T)) {
        OnSmoke();
    }
    if (input.GetKeyPressed(KeyCode::V)) {
        swirlIndex = (swirlIndex + 1) % SWIRL_COUNT;
        sim.SetVorticity(SWIRLS[swirlIndex]);
        ShowInfo();
    }
    if (input.GetKeyPressed(KeyCode::H)) {
        detailIndex = (detailIndex + 1) % DETAIL_COUNT;
        ApplyRows();
    }
    if (input.GetKeyPressed(KeyCode::R)) {
        sim.ResetFlow();
    }
    if (input.GetKeyPressed(KeyCode::Minus) || input.GetKeyPressed(KeyCode::Subtract)) {
        OnSlower();
    }
    if (input.GetKeyPressed(KeyCode::Equal) || input.GetKeyPressed(KeyCode::Add)) {
        OnFaster();
    }

    // Q turns counter-clockwise, E clockwise.
    float turn = 0.0f;
    if (input.GetKeyDown(KeyCode::Q)) {
        turn += 1.0f;
    }
    if (input.GetKeyDown(KeyCode::E)) {
        turn -= 1.0f;
    }
    obstacleAngle += turn * TURN_SPEED * deltaTime;

    // Up and Down: a degree of angle of attack (nose up is a clockwise turn).
    if (input.GetKeyPressed(KeyCode::UpArrow)) {
        obstacleAngle = (std::round(obstacleAngle * DEGREES) - 1.0f) * DEGREE;
    }
    if (input.GetKeyPressed(KeyCode::DownArrow)) {
        obstacleAngle = (std::round(obstacleAngle * DEGREES) + 1.0f) * DEGREE;
    }
}

void Samples::GameWindTunnel::ReadPointer(float deltaTime) {
    float x = 0.0f;
    float y = 0.0f;
    PointerToSim(x, y);

    const ME::Vec2i mouse = InputManager::GetMousePos();
    if (InputManager::GetMouseButtonPressed(ME::Input::MouseButton::Left) &&
        !tunnelUI->IsOverButton(static_cast<float>(mouse.x), static_cast<float>(mouse.y))) {
        // The obstacle jumps to the pointer, standing still, and follows from there.
        bDragging = true;
        sim.SetObstacle(x, y, obstacleAngle, 0.0f, 0.0f);
        return;
    }
    if (bDragging && !InputManager::GetMouseButtonDown(ME::Input::MouseButton::Left)) {
        bDragging = false;
    }

    if (bDragging && deltaTime > 0.0f) {
        // Its cells carry its speed, which is what stirs the wake.
        const float vx = (x - sim.GetObstacleX()) / deltaTime;
        const float vy = (y - sim.GetObstacleY()) / deltaTime;
        sim.SetObstacle(x, y, obstacleAngle, vx, vy);
    } else {
        sim.SetObstacle(sim.GetObstacleX(), sim.GetObstacleY(), obstacleAngle, 0.0f, 0.0f);
    }
}

void Samples::GameWindTunnel::SelectShape(Shape newShape) {
    namespace Shapes = WindTunnelShapes;
    if (newShape != shape) {
        obstacleAngle = newShape == Shape::Airfoil ? AIRFOIL_ANGLE : 0.0f;
    }
    shape = newShape;
    switch (shape) {
        case Shape::Circle:
            sim.SetOutline(Shapes::Circle(CIRCLE_RADIUS), 2.0f * CIRCLE_RADIUS);
            break;
        case Shape::Square:
            sim.SetOutline(Shapes::Square(SQUARE_HALF), 2.0f * SQUARE_HALF);
            break;
        case Shape::Plate:
            sim.SetOutline(Shapes::Plate(PLATE_HALF_LENGTH, PLATE_HALF_THICKNESS), 2.0f * PLATE_HALF_LENGTH);
            break;
        case Shape::Airfoil: {
            WindTunnelSim::Outline foil;
            if (!Shapes::Naca(NACA_CODES[nacaIndex], CHORD, foil)) {
                ME::LogError("Wind Tunnel: bad NACA code ", NACA_CODES[nacaIndex]);
            }
            sim.SetOutline(foil, CHORD);
            break;
        }
        case Shape::Aircraft:
            sim.SetOutline(Shapes::Aircraft(AIRCRAFT_LENGTH), AIRCRAFT_LENGTH);
            break;
        case Shape::Car:
            sim.SetOutline(Shapes::Car(CAR_LENGTH), CAR_LENGTH);
            break;
        case Shape::Count:
            break;
    }
    sim.SetObstacle(sim.GetObstacleX(), sim.GetObstacleY(), obstacleAngle, 0.0f, 0.0f);
    lift = 0.0f;
    drag = 0.0f;
    ShowInfo();
}

void Samples::GameWindTunnel::ShowForces() {
    char buffer[96];
    // Rounded first, so a tiny negative angle doesn't show as -0.
    const float degrees = std::round(-obstacleAngle * DEGREES) + 0.0f;
    snprintf(buffer, sizeof(buffer), "lift %+.2f   drag %+.2f   angle of attack %+.0f deg", lift, drag, degrees);
    tunnelUI->SetForces(buffer);
}

void Samples::GameWindTunnel::ApplyRows() {
    // A new grid starts still; the obstacle keeps its place in domain units.
    const float x = sim.GetCountX() > 0 ? sim.GetObstacleX() : START_X;
    const float y = sim.GetCountX() > 0 ? sim.GetObstacleY() : 0.5f * WindTunnelSim::DOMAIN_HEIGHT;
    const uint32_t rows = RowsThatFit(DETAIL_ROWS[detailIndex], aspect, SceneWindTunnel::MAX_CELLS);
    sim = WindTunnelSim();
    sim.SetInflowSpeed(SPEEDS[speedIndex]);
    sim.SetVorticity(SWIRLS[swirlIndex]);
    if (!sim.Init(rows, aspect)) {
        ME::LogError("Wind Tunnel: bad grid, ", rows, " rows at aspect ", aspect);
        return;
    }
    sim.SetObstacle(x, y, obstacleAngle, 0.0f, 0.0f);
    SelectShape(shape);
    sim.ResetFlow();
    tunnelScene->Layout(sim);
    tunnelScene->Sync(sim, view, bStreamlines);
    stepDebt = 0.0;
    ME::Log("Wind Tunnel: ", sim.GetCountX(), " x ", sim.GetCountY(), " cells");
    ShowInfo();
}

void Samples::GameWindTunnel::ShowInfo() {
    if (tunnelUI == nullptr) {
        return;
    }
    char name[24];
    if (shape == Shape::Airfoil) {
        snprintf(name, sizeof(name), "NACA %s", NACA_CODES[nacaIndex]);
    } else {
        snprintf(name, sizeof(name), "%s", SHAPE_NAMES[static_cast<size_t>(shape)]);
    }
    char buffer[160];
    snprintf(buffer, sizeof(buffer), "%s  |  %s%s  |  wind %.1f  |  %u x %u  |  swirl %s  |  sim %.1f ms", name,
             VIEW_NAMES[static_cast<size_t>(view)],
             bStreamlines ? " + lines" : "", sim.GetInflowSpeed(), sim.GetCountX(), sim.GetCountY(),
             SWIRL_NAMES[swirlIndex], shownSimTime);
    tunnelUI->SetInfo(buffer);
}

void Samples::GameWindTunnel::PointerToSim(float& outX, float& outY) const {
    const ME::Vec2i mouse = InputManager::GetMousePos();
    const float width = static_cast<float>(engine->window->width);
    const float height = static_cast<float>(engine->window->height);
    const float unitsPerPoint = (2.0f * SceneWindTunnel::VIEW_HALF_HEIGHT) / (height > 0.0f ? height : 1.0f);
    const float viewX = (static_cast<float>(mouse.x) - 0.5f * width) * unitsPerPoint;
    const float viewY = (0.5f * height - static_cast<float>(mouse.y)) * unitsPerPoint;
    outX = tunnelScene->ToSimX(sim, viewX);
    outY = tunnelScene->ToSimY(viewY);
}

void Samples::GameWindTunnel::Benchmark() {
    // A slow figure eight around the starting spot, the same on every machine.
    const float t = static_cast<float>(benchStep) * STEP;
    const float x = START_X + 0.15f * std::sin(t * 1.3f);
    const float y = 0.5f + 0.2f * std::sin(t * 2.6f);
    const float vx = 0.15f * 1.3f * std::cos(t * 1.3f);
    const float vy = 0.2f * 2.6f * std::cos(t * 2.6f);
    sim.SetObstacle(x, y, obstacleAngle, vx, vy);

    const auto start = std::chrono::steady_clock::now();
    sim.Step(STEP);
    const auto stop = std::chrono::steady_clock::now();
    simTimeSum += std::chrono::duration<double, std::milli>(stop - start).count();
    ++simSteps;
    ++benchStep;
    if (simSteps < BENCH_STEPS) {
        return;
    }

    char line[96];
    snprintf(line, sizeof(line), "%3u x %3u: sim %6.2f ms per step, checksum %08X", sim.GetCountX(), sim.GetCountY(),
             simTimeSum / simSteps, sim.Checksum());
    ME::Log("Wind Tunnel benchmark: ", line);
    simTimeSum = 0.0;
    simSteps = 0;
    benchStep = 0;
    if (detailIndex + 1 < DETAIL_COUNT) {
        ++detailIndex;
        ApplyRows();
    } else {
        engine->Quit();
    }
}
