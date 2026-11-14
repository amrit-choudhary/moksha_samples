/**
 * Colony scene. The world fills the view with a fixed camera. Instanced set 0 holds one white quad per world cell
 * (57,600), placed once and coloured every frame from the cell: ground tinted by the colony's markers, walls, food and
 * the nest. Instanced set 1 holds one stretched quad per ant slot, turned to its heading and coloured by its state
 * (searching follower or explorer, carrying, stuck);
 * its draw count is the ants' slot end, and free slots below it have zero scale.
 */

#pragma once

#include "moksha/client/scene/scene.h"

namespace Samples {

class ColonyAnts;
class ColonyWorld;

class SceneColony : public ME::Scene {
   public:
    virtual void CreateResources() override;
    virtual void BuildCamera() override;
    virtual void BuildInstancedSpriteTransforms() override;
    virtual void BuildInstancedSpriteRenderers() override;

    virtual const char* GetDisplayName() const override;

    // Colours every cell from the world (CellSprites). Every cell is marked for upload, as markers change every step.
    void SyncCells(const ColonyWorld& world);

    // Places and colours every ant slot in use (AntSprites).
    void SyncAnts(const ColonyAnts& ants);

    // Draws nothing until the next Sync: for the Drawing load setting, which leaves the engine no sprites to upload.
    void HideAll();

    // Zooms the view by notches (positive in) about the pointer, given as a fraction of the window (0 to 1, bottom-left
    // origin); the world point under it stays put. Between MAX_ZOOM and the whole world, never showing past its edges.
    void Zoom(float notches, float pointerX, float pointerY);

    // Moves the view by a distance in view heights (so the same keys feel the same at every zoom), within the world.
    void Pan(float viewsX, float viewsY);

    // Half the view's height in world units, zoomed out: the world is 720 units high and fills the view.
    static constexpr float VIEW_HALF_HEIGHT = 360.0f;
    static constexpr float MAX_ZOOM = 16.0f;
    static constexpr float ZOOM_PER_NOTCH = 1.2f;

   private:
    // Moves the camera to (x, y) with half height `half`, clamped so the view never shows past the world's edges.
    void PlaceView(float x, float y, float half);
};

}  // namespace Samples
