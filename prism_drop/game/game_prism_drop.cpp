#include "prism_drop/game/game_prism_drop.h"

#include <cmath>
#include <cstdio>

#include "moksha/client/app/engine.h"
#include "moksha/client/audio/audio_system.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/file_io/vfs.h"
#include "moksha/shared/serialization/byte_reader.h"
#include "moksha/shared/serialization/byte_writer.h"
#include "prism_drop/game/prism_autoplay.h"
#include "prism_drop/game/prism_palette.h"

using ME::Input::InputManager;
using ME::Input::KeyCode;
using Samples::PrismPalette::Lighten;
using Samples::PrismPalette::Mix;
using Samples::PrismPalette::WithAlpha;
using S = Samples::ScenePrismDrop;

static constexpr float HALF_PI = 1.5707963f;

// Exponential approach of value to target at rate per second.
static float Approach(float value, float target, float rate, float deltaTime) {
    return target + (value - target) * std::exp(-rate * deltaTime);
}

// ---------------------------------------------------------------------------------------------------------------------
// Lifecycle

void Samples::GamePrismDrop::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    const float width = static_cast<float>(engine->window->width);
    const float height = static_cast<float>(engine->window->height);

    prismScene = new ScenePrismDrop();
    scene = prismScene;
    scene->Init();
    prismScene->SetAspect(width / height);

    prismUI = new SceneUIPrismDrop(height / (S::VIEW_HALF_HEIGHT * 2.0f));
    uiScene = prismUI;
    uiScene->Init();

    fx.Init(prismScene);
}

void Samples::GamePrismDrop::Start() {
    Game::Start();

    LoadBest();
    engine->audio->PlayMusic(0, true);
    StartGame(true);
}

void Samples::GamePrismDrop::Update(double deltaTime) {
    Game::Update(deltaTime);

    // A long hitch (loading, a breakpoint) shouldn't fast-forward the game.
    const float dt = static_cast<float>(deltaTime < 0.1 ? deltaTime : 0.1);
    totalTime += dt;
    stateTime += dt;

    if (engine->input->GetKeyPressed(KeyCode::Escape)) {
        engine->Quit();
    }
    if (engine->input->GetKeyPressed(KeyCode::M)) {
        ToggleMute();
    }

    if (bDemo && IsStartPressed()) {
        StartGame(false);
    } else if (bDemo) {
        titleRainTimer += dt;
        while (titleRainTimer > 0.3f) {
            titleRainTimer -= 0.3f;
            fx.TitleRain(prismScene->GetViewHalfWidth());
        }
    }

    switch (state) {
        case State::Playing:
            if (bDemo) {
                UpdateDemo(dt);
                UpdatePlaying(dt);
                break;
            }
            ReadKeys(dt);
            if (state == State::Playing) {
                ReadTouch(dt);
            }
            if (state == State::Playing) {
                UpdatePlaying(dt);
            }
            break;
        case State::Clearing:
            // Moves during the clear animation are dropped, but pausing still works.
            if (!bDemo) {
                ReadTouch(dt);
                if (engine->input->GetKeyPressed(KeyCode::P)) {
                    SetPaused(true);
                }
            }
            if (state == State::Clearing) {
                UpdateClearing(dt);
            }
            break;
        case State::GameOver:
            UpdateGameOver(dt);
            break;
        case State::Paused:
            if (IsStartPressed() || engine->input->GetKeyPressed(KeyCode::P)) {
                SetPaused(false);
            }
            break;
    }

    if (state != State::Paused) {
        UpdateVisuals(dt);
    }
    prismUI->Tick(dt);
}

void Samples::GamePrismDrop::End() {
    delete scene;
    scene = nullptr;
    prismScene = nullptr;
    delete uiScene;
    uiScene = nullptr;
    prismUI = nullptr;

    Game::End();
}

void Samples::GamePrismDrop::OnPause() {
    Game::OnPause();
    if (!bDemo && (state == State::Playing || state == State::Clearing)) {
        SetPaused(true);
    }
}

const char* Samples::GamePrismDrop::GetDisplayName() const {
    return "Prism Drop";
}

// ---------------------------------------------------------------------------------------------------------------------
// Input

void Samples::GamePrismDrop::ReadKeys(float deltaTime) {
    const ME::Input::InputManager* input = engine->input;

    if (input->GetKeyPressed(KeyCode::P)) {
        SetPaused(true);
        return;
    }
    if (input->GetKeyPressed(KeyCode::C) || input->GetKeyPressed(KeyCode::LeftShift) ||
        input->GetKeyPressed(KeyCode::RightShift)) {
        DoHold();
    }
    if (input->GetKeyPressed(KeyCode::UpArrow) || input->GetKeyPressed(KeyCode::X)) {
        TryRotate(1);
    }
    if (input->GetKeyPressed(KeyCode::Z)) {
        TryRotate(-1);
    }

    // Sideways: the newest pressed direction wins, then auto-repeats after a delay.
    const bool bLeft = InputManager::GetKeyDown(KeyCode::LeftArrow);
    const bool bRight = InputManager::GetKeyDown(KeyCode::RightArrow);
    if (input->GetKeyPressed(KeyCode::LeftArrow)) {
        dasDirection = -1;
        dasTimer = 0.0f;
        TryMove(-1);
    } else if (input->GetKeyPressed(KeyCode::RightArrow)) {
        dasDirection = 1;
        dasTimer = 0.0f;
        TryMove(1);
    } else if ((dasDirection < 0 && bLeft) || (dasDirection > 0 && bRight)) {
        dasTimer += deltaTime;
        while (dasTimer >= DAS_SECONDS + ARR_SECONDS) {
            dasTimer -= ARR_SECONDS;
            TryMove(dasDirection);
        }
    } else {
        dasDirection = bLeft ? -1 : (bRight ? 1 : 0);
        dasTimer = 0.0f;
    }

    bSoftDropKey = InputManager::GetKeyDown(KeyCode::DownArrow);

    if (input->GetKeyPressed(KeyCode::Space)) {
        DoHardDrop();
    }
}

void Samples::GamePrismDrop::ReadTouch(float deltaTime) {
    float x = 0.0f;
    float y = 0.0f;
    MouseWorld(x, y);

    if (InputManager::GetMouseButtonPressed(ME::Input::MouseButton::Left)) {
        gesture = Gesture();
        gesture.bActive = true;
        gesture.startX = gesture.anchorX = x;
        gesture.startY = gesture.anchorY = y;
    }
    if (!gesture.bActive) {
        return;
    }
    gesture.seconds += deltaTime;

    const float totalX = x - gesture.startX;
    const float totalDown = gesture.startY - y;
    const float absX = std::fabs(totalX);
    const float absY = std::fabs(totalDown);

    if (state == State::Playing) {
        // Sideways steps while the drag is mostly sideways; soft drop steps while it is mostly downward.
        if (absX >= absY * 0.6f) {
            while (x - gesture.anchorX >= DRAG_STEP) {
                gesture.anchorX += DRAG_STEP;
                gesture.bDragged = true;
                TryMove(1);
            }
            while (gesture.anchorX - x >= DRAG_STEP) {
                gesture.anchorX -= DRAG_STEP;
                gesture.bDragged = true;
                TryMove(-1);
            }
        } else {
            gesture.anchorX = x;
        }
        if (absY > absX) {
            while (gesture.anchorY - y >= DRAG_STEP) {
                gesture.anchorY -= DRAG_STEP;
                gesture.bDragged = true;
                TrySoftDrop();
            }
        } else {
            gesture.anchorY = y;
        }
    }

    if (!InputManager::GetMouseButtonReleased(ME::Input::MouseButton::Left) &&
        InputManager::GetMouseButtonDown(ME::Input::MouseButton::Left)) {
        return;
    }
    gesture.bActive = false;

    const bool bQuick = gesture.seconds <= SWIPE_SECONDS;
    if (state == State::Playing && bQuick && totalDown >= SWIPE_DISTANCE && absY > absX * 1.5f) {
        DoHardDrop();
    } else if (state == State::Playing && bQuick && -totalDown >= SWIPE_DISTANCE && absY > absX * 1.5f) {
        DoHold();
    } else if (!gesture.bDragged && gesture.seconds <= TAP_SECONDS && absX < DRAG_STEP && absY < DRAG_STEP) {
        OnTap(gesture.startX, gesture.startY);
    }
}

void Samples::GamePrismDrop::OnTap(float x, float y) {
    const float boxHalf = S::BOX_SIZE * 0.5f + 10.0f;
    if (std::fabs(x + S::BOX_X) < boxHalf && std::fabs(y - S::BOX_Y) < boxHalf) {
        if (state == State::Playing) {
            DoHold();
        }
        return;
    }
    if (y > S::BOARD_BOTTOM + S::BOARD_HEIGHT_UNITS + S::WELL_BORDER) {
        SetPaused(true);
        return;
    }
    if (state == State::Playing) {
        TryRotate(1);
    }
}

bool Samples::GamePrismDrop::IsStartPressed() const {
    return InputManager::GetMouseButtonPressed(ME::Input::MouseButton::Left) ||
           engine->input->GetKeyPressed(KeyCode::Space) || engine->input->GetKeyPressed(KeyCode::Enter);
}

void Samples::GamePrismDrop::MouseWorld(float& outX, float& outY) const {
    const ME::Vec2i mouse = InputManager::GetMousePos();
    const float height = static_cast<float>(engine->window->height);
    const float width = static_cast<float>(engine->window->width);
    const float unitsPerPixel = (S::VIEW_HALF_HEIGHT * 2.0f) / (height > 0.0f ? height : 1.0f);
    outX = (static_cast<float>(mouse.x) - width * 0.5f) * unitsPerPixel;
    outY = S::VIEW_HALF_HEIGHT - static_cast<float>(mouse.y) * unitsPerPixel;
}

// ---------------------------------------------------------------------------------------------------------------------
// Actions

void Samples::GamePrismDrop::StartGame(bool bDemoGame) {
    bDemo = bDemoGame;
    board.Reset(seedRandom.Next());
    fx.Clear();
    for (size_t row = 0; row < VISIBLE_HEIGHT; ++row) {
        rowOffset[row] = 0.0f;
        rowSpeed[row] = 0.0f;
    }
    bShattered = false;
    lockGlow = 0.0f;
    gesture = Gesture();
    dasDirection = 0;
    if (bDemo) {
        prismUI->ShowTitle(bestScore);
    } else {
        prismUI->ShowPlaying();
    }
    prismUI->SetScore(0);
    prismUI->SetStats(board.GetLevel(), board.GetLines());
    Play(PrismSfx::START);
    fx.Ring(0.0f, S::BOARD_BOTTOM + S::BOARD_HEIGHT_UNITS * 0.5f, WithAlpha(PrismPalette::CYAN, 0.8f), 60.0f, 700.0f,
            0.7f);
    state = State::Playing;
    stateTime = 0.0f;
    SpawnNext();
    shownNext = board.PeekNext(0);
}

void Samples::GamePrismDrop::UpdateDemo(float deltaTime) {
    demoTimer += deltaTime;
    if (demoTimer < DEMO_STEP_SECONDS || !board.HasPiece()) {
        return;
    }
    demoTimer = 0.0f;
    if (plannedSerial != pieceSerial) {
        const Placement placement = FindBestPlacement(board);
        plannedSerial = pieceSerial;
        planRotation = placement.rotation;
        planX = placement.x;
    }
    // Turn first, then slide, then drop; anything blocked drops where the piece is.
    const ActivePiece& piece = board.GetPiece();
    if (piece.rotation != planRotation) {
        if (!board.Rotate(1)) {
            DoHardDrop();
            return;
        }
        pieceAngle += HALF_PI;
    } else if (piece.x != planX) {
        if (!board.Move(piece.x < planX ? 1 : -1)) {
            DoHardDrop();
        }
    } else {
        DoHardDrop();
    }
}

void Samples::GamePrismDrop::Play(uint8_t sound) {
    if (!bDemo) {
        engine->audio->PlayAudio(sound, false);
    }
}

void Samples::GamePrismDrop::TryMove(int dx) {
    if (board.Move(dx)) {
        Play(PrismSfx::MOVE);
        AfterMoveOnFloor();
    }
}

void Samples::GamePrismDrop::TryRotate(int direction) {
    if (board.Rotate(direction)) {
        // Show the old orientation and turn into the new one.
        pieceAngle += static_cast<float>(direction) * HALF_PI;
        Play(PrismSfx::ROTATE);
        AfterMoveOnFloor();
    }
}

void Samples::GamePrismDrop::TrySoftDrop() {
    if (board.StepDown()) {
        board.AddDropPoints(1, false);
        gravityTimer = 0.0f;
        prismUI->SetScore(board.GetScore());
    }
}

void Samples::GamePrismDrop::DoHardDrop() {
    if (!board.HasPiece()) {
        return;
    }
    BoardCell before[PIECE_CELLS];
    board.GetCells(board.GetPiece(), before);
    const int rows = board.HardDrop();
    board.AddDropPoints(rows, true);

    // A light trail down each column the piece covers, from its old top to its new bottom.
    const ME::Color color = PrismPalette::Piece(static_cast<size_t>(board.GetPiece().type));
    BoardCell after[PIECE_CELLS];
    board.GetCells(board.GetPiece(), after);
    for (int column = 0; column < BOARD_WIDTH; ++column) {
        int top = -1;
        int bottom = BOARD_HEIGHT;
        for (size_t i = 0; i < PIECE_CELLS; ++i) {
            if (before[i].x == column && before[i].y > top) {
                top = before[i].y;
            }
            if (after[i].x == column && after[i].y < bottom) {
                bottom = after[i].y;
            }
        }
        if (top >= 0 && rows > 0) {
            const int visibleTop = top < VISIBLE_HEIGHT ? top : VISIBLE_HEIGHT - 1;
            fx.Streak(CellX(static_cast<float>(column)), CellY(static_cast<float>(bottom)) - S::CELL * 0.5f,
                      CellY(static_cast<float>(visibleTop)) + S::CELL * 0.5f, S::CELL * 0.9f, Lighten(color, 0.3f));
        }
    }
    Play(PrismSfx::HARD_DROP);
    Shake(3.0f + static_cast<float>(rows) * 0.35f);
    SnapPieceVisual();
    LockPiece();
}

void Samples::GamePrismDrop::DoHold() {
    if (!board.Hold()) {
        return;
    }
    Play(PrismSfx::HOLD);
    holdPop = 0.4f;
    fx.Ring(-S::BOX_X, S::BOX_Y, WithAlpha(PrismPalette::CYAN, 0.7f), 30.0f, 140.0f, 0.4f);
    SnapPieceVisual();
    ++pieceSerial;
    gravityTimer = 0.0f;
    lockTimer = 0.0f;
    lockResets = 0;
    lowestRow = board.GetPiece().y;
    if (board.PeekNext(0) != shownNext) {
        shownNext = board.PeekNext(0);
        nextPop = 0.4f;
    }
}

void Samples::GamePrismDrop::SetPaused(bool bPaused) {
    if (bPaused == (state == State::Paused)) {
        return;
    }
    if (bPaused) {
        pausedFrom = state;
        state = State::Paused;
        Play(PrismSfx::PAUSE);
        engine->audio->SetMasterVolume(0.0f);
        gesture = Gesture();
    } else {
        state = pausedFrom;
        engine->audio->SetMasterVolume(bMuted ? 0.0f : 1.0f);
        Play(PrismSfx::PAUSE);
    }
    prismUI->ShowPaused(bPaused);
}

void Samples::GamePrismDrop::ToggleMute() {
    bMuted = !bMuted;
    if (state != State::Paused) {
        engine->audio->SetMasterVolume(bMuted ? 0.0f : 1.0f);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Flow

void Samples::GamePrismDrop::UpdatePlaying(float deltaTime) {
    if (!board.HasPiece()) {
        return;
    }
    float interval = static_cast<float>(board.GravityInterval());
    if (bSoftDropKey && interval > SOFT_DROP_SECONDS) {
        interval = SOFT_DROP_SECONDS;
    }

    if (!board.IsOnFloor()) {
        lockTimer = 0.0f;
        gravityTimer += deltaTime;
        while (gravityTimer >= interval) {
            gravityTimer -= interval;
            if (!board.StepDown()) {
                gravityTimer = 0.0f;
                break;
            }
            if (bSoftDropKey) {
                board.AddDropPoints(1, false);
                prismUI->SetScore(board.GetScore());
            }
        }
        // Reaching a new lowest row gives the lock delay its resets back.
        if (board.GetPiece().y < lowestRow) {
            lowestRow = board.GetPiece().y;
            lockResets = 0;
        }
    } else {
        gravityTimer = 0.0f;
        lockTimer += deltaTime;
        if (lockTimer >= LOCK_DELAY_SECONDS) {
            LockPiece();
        }
    }
}

void Samples::GamePrismDrop::UpdateClearing(float deltaTime) {
    const float before = stateTime - deltaTime;
    if (before < CLEAR_BURST_AT && stateTime >= CLEAR_BURST_AT) {
        // Every cell of the cleared rows bursts; a quad bursts harder.
        const float power = pendingClear.lines >= 4 ? 1.6f : 1.0f;
        for (uint8_t i = 0; i < pendingClear.lines; ++i) {
            const int row = pendingClear.rows[i];
            for (int column = 0; column < BOARD_WIDTH; ++column) {
                const uint8_t cell = board.GetCell(column, row);
                if (cell != 0) {
                    fx.Shatter(CellX(static_cast<float>(column)), CellY(static_cast<float>(row)),
                               PrismPalette::Piece(cell - 1u), power);
                }
            }
            fx.Ring(0.0f, CellY(static_cast<float>(row)), WithAlpha(ME::Color::White(), 0.7f), 40.0f, 520.0f, 0.5f);
        }
    }
    if (stateTime < CLEAR_SECONDS) {
        return;
    }

    // Each remaining row starts above its new place by the rows cleared beneath it, and falls into it.
    int below = 0;
    float offsets[BOARD_HEIGHT] = {};
    int newRow = 0;
    for (int row = 0; row < BOARD_HEIGHT; ++row) {
        bool bCleared = false;
        for (uint8_t i = 0; i < pendingClear.lines; ++i) {
            bCleared = bCleared || pendingClear.rows[i] == row;
        }
        if (bCleared) {
            ++below;
            continue;
        }
        offsets[newRow++] = static_cast<float>(below) * S::CELL;
    }
    board.ClearFullRows(pendingClear);
    for (int row = 0; row < VISIBLE_HEIGHT; ++row) {
        rowOffset[row] += offsets[row];
        rowSpeed[row] = 0.0f;
    }

    // The demo ends itself after a while, so the title keeps cycling.
    if (bDemo && board.GetLines() >= DEMO_LINES) {
        BeginGameOver();
        return;
    }
    state = State::Playing;
    stateTime = 0.0f;
    SpawnNext();
}

void Samples::GamePrismDrop::UpdateGameOver(float deltaTime) {
    const float before = stateTime - deltaTime;
    if (before < GAME_OVER_SHATTER_AT && stateTime >= GAME_OVER_SHATTER_AT) {
        for (int row = 0; row < VISIBLE_HEIGHT; ++row) {
            for (int column = 0; column < BOARD_WIDTH; ++column) {
                const uint8_t cell = board.GetCell(column, row);
                if (cell != 0) {
                    fx.Shatter(CellX(static_cast<float>(column)), CellY(static_cast<float>(row)),
                               PrismPalette::Piece(cell - 1u), 0.8f);
                }
            }
        }
        bShattered = true;
        Shake(10.0f);
        if (bDemo) {
            ME::Log("Prism Drop: demo topped out at ", board.GetLines(), " lines.");
            return;
        }
        const uint64_t score = board.GetScore();
        const bool bNewBest = score > bestScore;
        if (bNewBest) {
            bestScore = score;
            SaveBest();
            fx.Fountain(-S::BOARD_WIDTH_UNITS * 0.5f, S::BOARD_BOTTOM, 1.2f, 60);
            fx.Fountain(S::BOARD_WIDTH_UNITS * 0.5f, S::BOARD_BOTTOM, HALF_PI * 2.0f - 1.2f, 60);
        }
        prismUI->ShowGameOver(score, bestScore, bNewBest);
        ME::Log("Prism Drop: game over at ", score, " points, level ", board.GetLevel(), ", ", board.GetLines(),
                " lines.");
    }
    if (bDemo && stateTime >= GAME_OVER_TAP_AFTER) {
        StartGame(true);
    } else if (!bDemo && stateTime >= GAME_OVER_TAP_AFTER && IsStartPressed()) {
        StartGame(false);
    }
}

void Samples::GamePrismDrop::LockPiece() {
    const PieceType type = board.GetPiece().type;
    board.GetCells(board.GetPiece(), lockCells);
    const LockResult result = board.Lock();
    lockGlow = 1.0f;
    Play(PrismSfx::LOCK);

    // Dust from under the cells that rest on something.
    const ME::Color color = PrismPalette::Piece(static_cast<size_t>(type));
    for (const BoardCell& cell : lockCells) {
        if (cell.y < VISIBLE_HEIGHT && (cell.y == 0 || board.GetCell(cell.x, cell.y - 1) != 0)) {
            fx.Dust(CellX(static_cast<float>(cell.x)), CellY(static_cast<float>(cell.y)) - S::CELL * 0.5f, S::CELL,
                    color, 3);
        }
    }

    prismUI->SetScore(board.GetScore());
    prismUI->SetStats(board.GetLevel(), board.GetLines());

    if (result.bLockOut) {
        BeginGameOver();
        return;
    }
    if (result.lines > 0 || result.bTSpin) {
        AnnounceClear(result);
    }
    if (result.lines > 0) {
        pendingClear = result;
        state = State::Clearing;
        stateTime = 0.0f;
        return;
    }
    SpawnNext();
}

void Samples::GamePrismDrop::SpawnNext() {
    if (!board.Spawn()) {
        BeginGameOver();
        return;
    }
    SnapPieceVisual();
    ++pieceSerial;
    gravityTimer = 0.0f;
    lockTimer = 0.0f;
    lockResets = 0;
    lowestRow = board.GetPiece().y;
    if (board.PeekNext(0) != shownNext) {
        shownNext = board.PeekNext(0);
        nextPop = 0.4f;
    }
}

void Samples::GamePrismDrop::BeginGameOver() {
    state = State::GameOver;
    stateTime = 0.0f;
    Play(PrismSfx::GAME_OVER);
    Shake(6.0f);
}

void Samples::GamePrismDrop::AfterMoveOnFloor() {
    // A move or turn on the floor restarts the lock delay, a limited number of times.
    if (board.IsOnFloor() && lockResets < MAX_LOCK_RESETS) {
        lockTimer = 0.0f;
        ++lockResets;
    }
}

void Samples::GamePrismDrop::AnnounceClear(const LockResult& result) {
    static const char* const NAMES[5] = {"", "SINGLE", "DOUBLE", "TRIPLE", "QUAD!"};
    static const char* const TSPIN_NAMES[5] = {"T-SPIN", "T-SPIN SINGLE", "T-SPIN DOUBLE", "T-SPIN TRIPLE",
                                               "T-SPIN TRIPLE"};
    const uint8_t lines = result.lines < 4 ? result.lines : 4;
    const char* headline = result.bTSpin ? TSPIN_NAMES[lines] : NAMES[lines];
    if (result.bPerfectClear) {
        headline = "PERFECT CLEAR!";
    }

    char detail[32] = "";
    if (result.bBackToBack && result.combo > 0) {
        snprintf(detail, sizeof(detail), "BACK-TO-BACK  COMBO x%u", result.combo);
    } else if (result.bBackToBack) {
        snprintf(detail, sizeof(detail), "BACK-TO-BACK");
    } else if (result.combo > 0) {
        snprintf(detail, sizeof(detail), "COMBO x%u", result.combo);
    }

    ME::Color color = PrismPalette::CYAN;
    if (lines >= 4 || result.bPerfectClear) {
        color = PrismPalette::GOLD;
    } else if (result.bTSpin) {
        color = PrismPalette::Piece(static_cast<size_t>(PieceType::T));
    } else if (lines == 3) {
        color = PrismPalette::PINK;
    }
    float y = 0.0f;
    if (lines > 0) {
        y = CellY(static_cast<float>(result.rows[lines - 1])) + S::CELL * 1.5f;
    }
    y = y < S::BOARD_BOTTOM + 140.0f ? S::BOARD_BOTTOM + 140.0f : (y > 160.0f ? 160.0f : y);
    prismUI->Popup(headline, detail, result.points, color, y);

    const uint8_t sounds[5] = {PrismSfx::LOCK, PrismSfx::CLEAR1, PrismSfx::CLEAR2, PrismSfx::CLEAR3, PrismSfx::QUAD};
    Play(result.bTSpin && lines < 4 ? PrismSfx::CLEAR3 : sounds[lines]);

    frameFlash = lines >= 4 ? 1.0f : 0.25f + 0.15f * static_cast<float>(lines);
    Shake(lines >= 4 ? 14.0f : 2.0f + 2.0f * static_cast<float>(lines));
    if (lines >= 4 || result.bPerfectClear) {
        fx.Fountain(-S::BOARD_WIDTH_UNITS * 0.5f - 10.0f, S::BOARD_BOTTOM, 1.1f, 50);
        fx.Fountain(S::BOARD_WIDTH_UNITS * 0.5f + 10.0f, S::BOARD_BOTTOM, HALF_PI * 2.0f - 1.1f, 50);
        fx.Twinkles(S::BOARD_LEFT, S::BOARD_BOTTOM, S::BOARD_LEFT + S::BOARD_WIDTH_UNITS,
                    S::BOARD_BOTTOM + S::BOARD_HEIGHT_UNITS, PrismPalette::GOLD, 40);
    }
    if (result.bLevelUp) {
        Play(PrismSfx::LEVEL_UP);
        ME::Log("Prism Drop: level ", board.GetLevel(), bDemo ? " (demo)" : "", ", score ", board.GetScore(), ".");
        char level[24];
        snprintf(level, sizeof(level), "LEVEL %u", board.GetLevel());
        // The level-up line replaces the detail line.
        prismUI->Popup(headline, level, result.points, color, y);
        const float centerY = S::BOARD_BOTTOM + S::BOARD_HEIGHT_UNITS * 0.5f;
        fx.Ring(0.0f, centerY, WithAlpha(PrismPalette::PINK, 0.9f), 80.0f, 900.0f, 0.9f);
        fx.Twinkles(-prismScene->GetViewHalfWidth(), -S::VIEW_HALF_HEIGHT, prismScene->GetViewHalfWidth(),
                    S::VIEW_HALF_HEIGHT, PrismPalette::PINK, 50);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing

void Samples::GamePrismDrop::UpdateVisuals(float deltaTime) {
    // Collapsing rows fall with gravity into place.
    for (int row = 0; row < VISIBLE_HEIGHT; ++row) {
        if (rowOffset[row] <= 0.0f) {
            continue;
        }
        rowSpeed[row] += 5200.0f * deltaTime;
        rowOffset[row] -= rowSpeed[row] * deltaTime;
        if (rowOffset[row] <= 0.0f) {
            rowOffset[row] = 0.0f;
            rowSpeed[row] = 0.0f;
        }
    }

    pieceVisX = Approach(pieceVisX, static_cast<float>(board.GetPiece().x), 30.0f, deltaTime);
    pieceVisY = Approach(pieceVisY, static_cast<float>(board.GetPiece().y), 30.0f, deltaTime);
    pieceAngle = Approach(pieceAngle, 0.0f, 26.0f, deltaTime);
    lockGlow = lockGlow > 0.0f ? lockGlow - deltaTime * 4.0f : 0.0f;
    holdPop = Approach(holdPop, 1.0f, 14.0f, deltaTime);
    nextPop = Approach(nextPop, 1.0f, 14.0f, deltaTime);

    DrawBoard();
    DrawPiece();
    DrawFlashes();

    const bool bInGame = state == State::Playing || state == State::Clearing;
    DrawPreview(S::HOLD_SLOT, bInGame && board.HasHeld(), board.GetHeld(), -S::BOX_X, S::BOX_Y - 8.0f, holdPop,
                !board.CanHold());
    DrawPreview(S::NEXT_SLOT, bInGame, board.PeekNext(0), S::BOX_X, S::BOX_Y - 8.0f, nextPop, false);

    UpdateMood(deltaTime);
    fx.Update(deltaTime);
}

void Samples::GamePrismDrop::DrawBoard() {
    const bool bClearing = state == State::Clearing;
    const bool bOver = state == State::GameOver;
    const float sweepRow = stateTime * GAME_OVER_SWEEP_ROWS_PER_SECOND;

    for (int row = 0; row < VISIBLE_HEIGHT; ++row) {
        bool bClearedRow = false;
        if (bClearing) {
            for (uint8_t i = 0; i < pendingClear.lines; ++i) {
                bClearedRow = bClearedRow || pendingClear.rows[i] == row;
            }
        }
        for (int column = 0; column < BOARD_WIDTH; ++column) {
            const size_t slot = S::BOARD_SLOT + static_cast<size_t>(row * BOARD_WIDTH + column);
            const uint8_t cell = board.GetCell(column, row);
            if (cell == 0 || (bOver && bShattered)) {
                prismScene->HideBlock(slot);
                continue;
            }
            const float x = CellX(static_cast<float>(column));
            const float y = CellY(static_cast<float>(row)) + rowOffset[row];
            uint16_t tile = static_cast<uint16_t>(cell - 1u);
            ME::Color tint = ME::Color::White();
            float size = S::CELL;
            if (bClearedRow) {
                // Blink white, then shrink away after the burst.
                const bool bBlink = static_cast<int>(stateTime / 0.05f) % 2 == 0;
                tile = (bBlink || stateTime >= CLEAR_BURST_AT) ? PrismTiles::WHITE : tile;
                if (stateTime >= CLEAR_BURST_AT) {
                    const float t = (stateTime - CLEAR_BURST_AT) / (CLEAR_SECONDS - CLEAR_BURST_AT);
                    size *= t < 1.0f ? (1.0f - t) * (1.0f - t) : 0.0f;
                }
            } else if (bOver && static_cast<float>(row) < sweepRow) {
                tile = PrismTiles::WHITE;
                tint = ME::Color(0.42f, 0.40f, 0.52f, 1.0f);
            }
            const float dim = bDemo ? DEMO_DIM : 1.0f;
            prismScene->SetBlock(slot, x, y, size, 0.0f, tile,
                                 ME::Color(tint.r * dim, tint.g * dim, tint.b * dim, 1.0f));
        }
    }
}

void Samples::GamePrismDrop::DrawPiece() {
    const bool bShow = state == State::Playing && board.HasPiece();
    if (!bShow) {
        for (size_t i = 0; i < PIECE_CELLS; ++i) {
            prismScene->HideBlock(S::PIECE_SLOT + i);
            prismScene->HideBlock(S::GHOST_SLOT + i);
        }
        return;
    }
    const ActivePiece& piece = board.GetPiece();
    const size_t typeIndex = static_cast<size_t>(piece.type);
    const ME::Color color = PrismPalette::Piece(typeIndex);
    BoardCell shape[PIECE_CELLS];
    PrismBoard::GetShape(piece.type, piece.rotation, shape);
    const float box = static_cast<float>(PrismBoard::BoxSize(piece.type));
    const float boardTop = S::BOARD_BOTTOM + S::BOARD_HEIGHT_UNITS;

    // Ghost: where a hard drop would land, pulsing gently.
    const int ghostY = board.GhostY();
    const float ghostAlpha = (0.45f + 0.15f * std::sin(totalTime * 5.0f)) * (bDemo ? DEMO_DIM : 1.0f);
    const ME::Color ghostTint = WithAlpha(Lighten(color, 0.2f), ghostAlpha);
    for (size_t i = 0; i < PIECE_CELLS; ++i) {
        const int row = ghostY - shape[i].y;
        if (row >= VISIBLE_HEIGHT || ghostY == piece.y) {
            prismScene->HideBlock(S::GHOST_SLOT + i);
            continue;
        }
        prismScene->SetBlock(S::GHOST_SLOT + i, CellX(static_cast<float>(piece.x + shape[i].x)),
                             CellY(static_cast<float>(row)), S::CELL, 0.0f, PrismTiles::GHOST, ghostTint);
    }

    // The piece turns about its box centre, so a rotation animates in place.
    const float left = S::BOARD_LEFT + pieceVisX * S::CELL;
    const float top = S::BOARD_BOTTOM + (pieceVisY + 1.0f) * S::CELL;
    const float pivotX = left + box * 0.5f * S::CELL;
    const float pivotY = top - box * 0.5f * S::CELL;
    const float c = std::cos(pieceAngle);
    const float s = std::sin(pieceAngle);
    // Resting on the floor dims the piece towards the lock, a cue that it is about to set.
    const float lockT = board.IsOnFloor() ? lockTimer / LOCK_DELAY_SECONDS : 0.0f;
    const float pulse = lockT > 0.0f ? 0.5f + 0.5f * std::cos(totalTime * 18.0f) : 1.0f;
    const float shade = (1.0f - 0.35f * lockT * (1.0f - pulse)) * (bDemo ? DEMO_DIM : 1.0f);
    const ME::Color tint(shade, shade, shade, 1.0f);
    for (size_t i = 0; i < PIECE_CELLS; ++i) {
        const float ox = (static_cast<float>(shape[i].x) + 0.5f - box * 0.5f) * S::CELL;
        const float oy = -(static_cast<float>(shape[i].y) + 0.5f - box * 0.5f) * S::CELL;
        const float x = pivotX + ox * c - oy * s;
        const float y = pivotY + ox * s + oy * c;
        if (y > boardTop) {
            prismScene->HideBlock(S::PIECE_SLOT + i);
            continue;
        }
        prismScene->SetBlock(S::PIECE_SLOT + i, x, y, S::CELL, pieceAngle, static_cast<uint16_t>(typeIndex), tint);
    }
}

void Samples::GamePrismDrop::DrawPreview(size_t firstSlot, bool bShow, PieceType type, float centerX, float centerY,
                                         float pop, bool bDimmed) {
    if (!bShow) {
        for (size_t i = 0; i < PIECE_CELLS; ++i) {
            prismScene->HideBlock(firstSlot + i);
        }
        return;
    }
    BoardCell shape[PIECE_CELLS];
    PrismBoard::GetShape(type, 0, shape);
    float minX = 10.0f;
    float maxX = -10.0f;
    float minY = 10.0f;
    float maxY = -10.0f;
    for (const BoardCell& cell : shape) {
        minX = static_cast<float>(cell.x) < minX ? static_cast<float>(cell.x) : minX;
        maxX = static_cast<float>(cell.x) > maxX ? static_cast<float>(cell.x) : maxX;
        minY = static_cast<float>(cell.y) < minY ? static_cast<float>(cell.y) : minY;
        maxY = static_cast<float>(cell.y) > maxY ? static_cast<float>(cell.y) : maxY;
    }
    const float cell = S::PREVIEW_CELL * pop;
    const float midX = (minX + maxX) * 0.5f;
    const float midY = (minY + maxY) * 0.5f;
    const float shade = bDimmed ? 0.45f : 1.0f;
    for (size_t i = 0; i < PIECE_CELLS; ++i) {
        const float x = centerX + (static_cast<float>(shape[i].x) - midX) * cell;
        const float y = centerY - (static_cast<float>(shape[i].y) - midY) * cell;
        prismScene->SetBlock(firstSlot + i, x, y, cell, 0.0f, static_cast<uint16_t>(type),
                             ME::Color(shade, shade, shade, 1.0f));
    }
}

void Samples::GamePrismDrop::DrawFlashes() {
    // A white glow over the cells that just locked.
    for (size_t i = 0; i < PIECE_CELLS; ++i) {
        const BoardCell& cell = lockCells[i];
        if (lockGlow <= 0.0f || cell.y >= VISIBLE_HEIGHT || state == State::GameOver) {
            prismScene->HideBlock(S::LOCK_GLOW_SLOT + i);
            continue;
        }
        const float grow = 1.0f + (1.0f - lockGlow) * 0.35f;
        prismScene->SetBlock(S::LOCK_GLOW_SLOT + i, CellX(static_cast<float>(cell.x)),
                             CellY(static_cast<float>(cell.y)) + rowOffset[cell.y], S::CELL * grow, 0.0f,
                             PrismTiles::GLOW, WithAlpha(ME::Color::White(), lockGlow * 0.8f));
    }

    // Light bars sweeping out of the cleared rows.
    for (size_t i = 0; i < 4; ++i) {
        const size_t slot = S::ROW_FLASH_SLOT + i;
        if (state != State::Clearing || i >= pendingClear.lines) {
            prismScene->HideBlock(slot);
            continue;
        }
        const float t = stateTime / CLEAR_SECONDS;
        const float alpha = t < 0.3f ? t / 0.3f : (1.0f - t) / 0.7f;
        const ME::Color color = pendingClear.lines >= 4 ? PrismPalette::GOLD : ME::Color::White();
        prismScene->SetBlockStretched(slot, 0.0f + S::BOARD_LEFT + S::BOARD_WIDTH_UNITS * 0.5f,
                                      CellY(static_cast<float>(pendingClear.rows[i])),
                                      S::BOARD_WIDTH_UNITS * (1.0f + t * 0.4f), S::CELL * (1.0f + t * 2.2f),
                                      PrismTiles::BAR, WithAlpha(color, alpha));
    }
}

void Samples::GamePrismDrop::UpdateMood(float deltaTime) {
    // Danger rises as the stack nears the top.
    const int height = board.StackHeight();
    const float targetDanger = state == State::Playing || state == State::Clearing
                                   ? (height >= 13 ? static_cast<float>(height - 12) / 7.0f : 0.0f)
                                   : 0.0f;
    danger = Approach(danger, targetDanger > 1.0f ? 1.0f : targetDanger, 3.0f, deltaTime);

    const ME::Color levelTint = PrismPalette::LEVEL_TINTS[(board.GetLevel() - 1) % PrismPalette::LEVEL_TINT_COUNT];
    backdropTint = Mix(backdropTint, levelTint, 1.0f - std::exp(-2.0f * deltaTime));
    const float alarm = danger * (0.6f + 0.4f * std::sin(totalTime * 6.0f));
    prismScene->SetBackdropTint(Mix(backdropTint, PrismPalette::DANGER, alarm * 0.6f));

    frameFlash = frameFlash > 0.0f ? frameFlash - deltaTime * 2.5f : 0.0f;
    const float base = 0.82f + 0.06f * std::sin(totalTime * 1.7f);
    const float bright = base + (1.0f - base) * (frameFlash > 0.0f ? frameFlash : 0.0f);
    prismScene->SetFrameTint(Mix(ME::Color(bright, bright, bright, 1.0f), PrismPalette::DANGER, alarm * 0.5f));

    fx.SetAmbientSpeed(1.0f + static_cast<float>(board.GetLevel() - 1) * 0.25f);

    // Shake: a decaying wobble from two out-of-phase sines.
    shake = shake > 0.05f ? shake * std::exp(-9.0f * deltaTime) : 0.0f;
    prismScene->SetCameraOffset(std::sin(totalTime * 71.0f) * shake, std::cos(totalTime * 53.0f) * shake);
}

void Samples::GamePrismDrop::Shake(float amount) {
    shake = shake > amount ? shake : amount;
}

void Samples::GamePrismDrop::SnapPieceVisual() {
    pieceVisX = static_cast<float>(board.GetPiece().x);
    pieceVisY = static_cast<float>(board.GetPiece().y);
    pieceAngle = 0.0f;
}

float Samples::GamePrismDrop::CellX(float column) {
    return S::BOARD_LEFT + (column + 0.5f) * S::CELL;
}

float Samples::GamePrismDrop::CellY(float row) {
    return S::BOARD_BOTTOM + (row + 0.5f) * S::CELL;
}

// ---------------------------------------------------------------------------------------------------------------------
// Save

void Samples::GamePrismDrop::LoadBest() {
    uint8_t buffer[16];
    size_t size = 0;
    if (ME::Vfs::ReadBytes(ME::FileRoot::UserData, SAVE_PATH, buffer, sizeof(buffer), size)) {
        ME::ByteReader reader(buffer, size);
        uint32_t low = 0;
        uint32_t high = 0;
        if (reader.ReadU32(low) && reader.ReadU32(high)) {
            bestScore = (static_cast<uint64_t>(high) << 32) | low;
        }
    }
}

void Samples::GamePrismDrop::SaveBest() const {
    uint8_t buffer[16];
    ME::ByteWriter writer(buffer, sizeof(buffer));
    writer.WriteU32(static_cast<uint32_t>(bestScore & 0xFFFFFFFFu));
    writer.WriteU32(static_cast<uint32_t>(bestScore >> 32));
    if (!ME::Vfs::WriteBytes(ME::FileRoot::UserData, SAVE_PATH, buffer, writer.GetSize())) {
        ME::LogWarning("Prism Drop: couldn't save the best score.");
    }
}
