/**
 * Prism Drop game: a falling block puzzle for a portrait phone. The rules live in PrismBoard; this class runs the
 * timing (gravity, lock delay, auto-repeat), reads keys and touch gestures, and turns every event into animation,
 * particles, popups, screen shake and sound. The best score is saved to UserData.
 * The title screen is attract mode: the game plays itself (dimmed, silent) until the first tap or key.
 * Touch (the first finger, or the mouse): drag sideways to move, drag down to soft drop, swipe down to hard drop,
 * swipe up or tap the hold box to hold, tap to rotate, tap above the well to pause.
 * Keys: Left/Right move, Down soft drop, Space hard drop, Up/X rotate clockwise, Z rotate counter-clockwise,
 * C/Shift hold, P pause, M mute, Escape quit.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/game/game.h"
#include "moksha/shared/random/random_engine.h"
#include "prism_drop/game/prism_board.h"
#include "prism_drop/game/prism_fx.h"
#include "prism_drop/scene/scene_prism_drop.h"
#include "prism_drop/scene/scene_ui_prism_drop.h"

namespace Samples {

class GamePrismDrop : public ME::Game {
   public:
    virtual void Init(ME::Engine& inEngine) override;
    virtual void Start() override;
    virtual void Update(double deltaTime) override;
    virtual void End() override;
    virtual void OnPause() override;

    virtual const char* GetDisplayName() const override;

   private:
    enum class State : uint8_t {
        Playing,   // a piece is falling
        Clearing,  // full rows flash and burst; gravity waits
        GameOver,  // the stack greys out and shatters, then waits for a tap
        Paused,
    };

    // One touch or mouse gesture, in world units.
    class Gesture {
       public:
        float startX = 0.0f;
        float startY = 0.0f;
        float anchorX = 0.0f;  // where the next drag step is measured from
        float anchorY = 0.0f;
        float seconds = 0.0f;
        bool bActive = false;
        bool bDragged = false;
    };

    // Input.
    void ReadKeys(float deltaTime);
    void ReadTouch(float deltaTime);
    void OnTap(float x, float y);
    bool IsStartPressed() const;
    void MouseWorld(float& outX, float& outY) const;

    // Actions; each plays its sound and effects. A demo game is the title screen's attract mode.
    void StartGame(bool bDemoGame);
    void UpdateDemo(float deltaTime);
    void Play(uint8_t sound);
    void TryMove(int dx);
    void TryRotate(int direction);
    void TrySoftDrop();
    void DoHardDrop();
    void DoHold();
    void SetPaused(bool bPaused);
    void ToggleMute();

    // Flow.
    void UpdatePlaying(float deltaTime);
    void UpdateClearing(float deltaTime);
    void UpdateGameOver(float deltaTime);
    void LockPiece();
    void SpawnNext();
    void BeginGameOver();
    void AfterMoveOnFloor();
    void AnnounceClear(const LockResult& result);

    // Drawing.
    void UpdateVisuals(float deltaTime);
    void DrawBoard();
    void DrawPiece();
    void DrawPreview(size_t firstSlot, bool bShow, PieceType type, float centerX, float centerY, float pop,
                     bool bDimmed);
    void DrawFlashes();
    void UpdateMood(float deltaTime);
    void Shake(float amount);
    void SnapPieceVisual();

    // Cell centre in world units.
    static float CellX(float column);
    static float CellY(float row);

    void LoadBest();
    void SaveBest() const;

    static constexpr float DAS_SECONDS = 0.16f;  // hold before a key auto-repeats
    static constexpr float ARR_SECONDS = 0.045f;
    static constexpr float SOFT_DROP_SECONDS = 0.03f;  // per row while soft dropping
    static constexpr float LOCK_DELAY_SECONDS = 0.5f;
    static constexpr int MAX_LOCK_RESETS = 15;
    static constexpr float CLEAR_SECONDS = 0.42f;
    static constexpr float CLEAR_BURST_AT = 0.16f;
    static constexpr float GAME_OVER_SWEEP_ROWS_PER_SECOND = 40.0f;
    static constexpr float GAME_OVER_SHATTER_AT = 0.75f;
    static constexpr float GAME_OVER_TAP_AFTER = 1.4f;
    static constexpr float DRAG_STEP = ScenePrismDrop::CELL * 0.85f;
    static constexpr float SWIPE_DISTANCE = ScenePrismDrop::CELL * 2.2f;
    static constexpr float SWIPE_SECONDS = 0.28f;
    static constexpr float TAP_SECONDS = 0.3f;
    static constexpr float DEMO_STEP_SECONDS = 0.07f;  // between the demo's moves
    static constexpr float DEMO_DIM = 0.5f;
    static constexpr uint32_t DEMO_LINES = 30;  // the demo well is dimmed under the title
    static constexpr const char* SAVE_PATH = "prism_drop/best.bin";

    ScenePrismDrop* prismScene = nullptr;
    SceneUIPrismDrop* prismUI = nullptr;
    PrismBoard board;
    PrismFx fx;
    ME::Random seedRandom{"prismSeed", true};

    State state = State::Playing;
    bool bDemo = true;
    float demoTimer = 0.0f;
    uint32_t pieceSerial = 0;  // counts pieces, so the demo knows when to plan again
    uint32_t plannedSerial = 0;
    uint8_t planRotation = 0;
    int planX = 0;
    State pausedFrom = State::Playing;

    // Timing.
    float gravityTimer = 0.0f;
    float lockTimer = 0.0f;
    int lockResets = 0;
    int lowestRow = 0;
    float stateTime = 0.0f;
    float totalTime = 0.0f;
    float titleRainTimer = 0.0f;
    int dasDirection = 0;
    float dasTimer = 0.0f;
    bool bSoftDropKey = false;
    Gesture gesture;
    LockResult pendingClear;
    bool bShattered = false;

    // Piece visuals: the box position eases towards the logical one, the angle back to zero after a turn.
    float pieceVisX = 0.0f;
    float pieceVisY = 0.0f;
    float pieceAngle = 0.0f;
    float rowOffset[VISIBLE_HEIGHT] = {};  // collapse animation, world units above the row's place
    float rowSpeed[VISIBLE_HEIGHT] = {};
    BoardCell lockCells[PIECE_CELLS] = {};
    float lockGlow = 0.0f;  // 1 at a lock, fading
    float holdPop = 1.0f;   // preview scale animation, 1 at rest
    float nextPop = 1.0f;
    PieceType shownNext = PieceType::I;

    // Mood.
    float shake = 0.0f;
    float frameFlash = 0.0f;
    float danger = 0.0f;
    ME::Color backdropTint = ME::Color::White();

    uint64_t bestScore = 0;
    bool bMuted = false;
};

}  // namespace Samples
