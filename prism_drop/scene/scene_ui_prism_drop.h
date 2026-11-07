/**
 * Prism Drop UI: score, hold and next captions and the level and line counts around the well; the title, pause and
 * game-over messages; and popups for clears that rise and fade. Elements are placed in the scene's world units and
 * converted with the window's scale, so the HUD lines up with the sprites on any window height.
 * Loads the engine's UI atlas at slot 0 and font at slot 1, the slots the UI passes draw from.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/rendering/shared/color.h"
#include "moksha/client/scene/scene_ui.h"
#include "moksha/client/ui/label.h"

namespace Samples {

class SceneUIPrismDrop : public ME::SceneUI {
   public:
    // pixelsPerUnit: window units per world unit (window height / 800).
    explicit SceneUIPrismDrop(float pixelsPerUnit);
    virtual ~SceneUIPrismDrop() override;

    virtual void CreateResources() override;
    virtual void BuildUIElements() override;

    // Animates the popups and the blinking hints.
    void Tick(float deltaTime);

    void ShowTitle(uint64_t best);
    void ShowPlaying();
    void ShowPaused(bool bPaused);
    void ShowGameOver(uint64_t score, uint64_t best, bool bNewBest);

    void SetScore(uint64_t score);
    void SetStats(uint32_t level, uint32_t lines);

    // A rising popup: a headline, an optional second line and points, over the well at worldY.
    void Popup(const char* headline, const char* detail, uint64_t points, const ME::Color& color, float worldY);

   private:
    class PopupLine {
       public:
        ME::Label* label = nullptr;
        ME::Color color = ME::Color::White();
        float baseY = 0.0f;  // world units
        float age = 0.0f;
        float delay = 0.0f;
        bool bActive = false;
    };

    ME::Label* MakeLabel(const char* text, const ME::Color& color, uint16_t glyph, float worldX, float worldY);
    void PlaceLabel(ME::Label* label, float worldX, float worldY);
    void StartLine(PopupLine& line, const char* text, const ME::Color& color, float worldY, float delay);
    void SetMessageVisible(bool bVisible);

    static constexpr size_t POPUP_LINES = 3;
    static constexpr float POPUP_SECONDS = 1.3f;
    static constexpr float POPUP_RISE = 70.0f;

    float pixelsPerUnit = 1.0f;
    float blinkTime = 0.0f;

    ME::Label* scoreCaption = nullptr;
    ME::Label* scoreLabel = nullptr;
    ME::Label* holdCaption = nullptr;
    ME::Label* nextCaption = nullptr;
    ME::Label* statsLabel = nullptr;

    ME::Label* titleLabel = nullptr;
    ME::Label* subtitleLabel = nullptr;
    ME::Label* infoLabel = nullptr;
    ME::Label* hintLabel = nullptr;
    ME::Label* controlsLabel = nullptr;
    ME::Label* controlsLabel2 = nullptr;
    bool bHintBlinks = false;

    PopupLine popups[POPUP_LINES];
};

}  // namespace Samples
