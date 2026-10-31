#include "rally/scene/scene_rally.h"

#include "moksha/client/utils/json_utils.h"
#include "shared/rally/rally_rules.h"

static constexpr ME::Color OWN_COLOR{0.3f, 0.85f, 1.0f, 1.0f};
static constexpr ME::Color OTHER_COLOR{1.0f, 0.55f, 0.2f, 1.0f};
static constexpr ME::Color WALL_COLOR{0.45f, 0.45f, 0.55f, 1.0f};
static constexpr ME::Color DASH_COLOR{0.25f, 0.25f, 0.32f, 1.0f};

void Samples::SceneRally::CreateResources() {
    ME::Scene::CreateResources();

    spriteTexturePaths[0] = "textures/white.dds";
    spriteTexturePaths.count = 1;
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_white.json", textureAtlasProperties[0]);
    textureAtlasProperties.count = 1;
}

void Samples::SceneRally::BuildSpriteTransforms() {
    ME::Scene::BuildSpriteTransforms();

    const float paddleWidth = Rally::PADDLE_HALF_WIDTH * 2.0f;
    const float paddleHeight = Rally::PADDLE_HALF_HEIGHT * 2.0f;
    spriteTransforms[OWN_PADDLE].SetPosition(0.0f, -Rally::PADDLE_Y, 0.0f);
    spriteTransforms[OWN_PADDLE].SetScale(paddleWidth, paddleHeight);
    spriteTransforms[OTHER_PADDLE].SetPosition(0.0f, Rally::PADDLE_Y, 0.0f);
    spriteTransforms[OTHER_PADDLE].SetScale(paddleWidth, paddleHeight);
    spriteTransforms[BALL].SetPosition(0.0f, 0.0f, 0.0f);
    spriteTransforms[BALL].SetScale(Rally::BALL_HALF_SIZE * 2.0f, Rally::BALL_HALF_SIZE * 2.0f);

    // z = 1 keeps the court markings behind the paddles and the ball.
    const float wallX = Rally::COURT_HALF_WIDTH + WALL_THICKNESS * 0.5f;
    const float wallHeight = Rally::COURT_HALF_HEIGHT * 2.0f;
    spriteTransforms[LEFT_WALL].SetPosition(-wallX, 0.0f, 1.0f);
    spriteTransforms[LEFT_WALL].SetScale(WALL_THICKNESS, wallHeight);
    spriteTransforms[RIGHT_WALL].SetPosition(wallX, 0.0f, 1.0f);
    spriteTransforms[RIGHT_WALL].SetScale(WALL_THICKNESS, wallHeight);

    const float dashStep = Rally::COURT_HALF_WIDTH * 2.0f / static_cast<float>(DASH_COUNT);
    for (size_t i = 0; i < DASH_COUNT; ++i) {
        const float x = -Rally::COURT_HALF_WIDTH + dashStep * (static_cast<float>(i) + 0.5f);
        spriteTransforms[FIRST_DASH + i].SetPosition(x, 0.0f, 1.0f);
        spriteTransforms[FIRST_DASH + i].SetScale(dashStep * 0.5f, 1.0f);
    }
    spriteTransforms.count = SPRITE_COUNT;
}

void Samples::SceneRally::BuildSpriteRenderers() {
    ME::Scene::BuildSpriteRenderers();

    spriteRenderers[OWN_PADDLE] = ME::SpriteRenderer(0, 0, 0, 0, 0, OWN_COLOR);
    spriteRenderers[OTHER_PADDLE] = ME::SpriteRenderer(0, 0, 0, 0, 0, OTHER_COLOR);
    spriteRenderers[BALL] = ME::SpriteRenderer(0, 0, 0, 0, 0, ME::Color::White());
    spriteRenderers[LEFT_WALL] = ME::SpriteRenderer(0, 0, 0, 0, 0, WALL_COLOR);
    spriteRenderers[RIGHT_WALL] = ME::SpriteRenderer(0, 0, 0, 0, 0, WALL_COLOR);
    for (size_t i = 0; i < DASH_COUNT; ++i) {
        spriteRenderers[FIRST_DASH + i] = ME::SpriteRenderer(0, 0, 0, 0, 0, DASH_COLOR);
    }
    spriteRenderers.count = SPRITE_COUNT;
}

const char* Samples::SceneRally::GetDisplayName() const {
    return "Rally Scene";
}

void Samples::SceneRally::FitCamera(float aspectRatio) {
    spriteCamera->aspectRatio = aspectRatio;

    const float halfWidth = Rally::COURT_HALF_WIDTH + WALL_THICKNESS + VIEW_MARGIN;
    float halfHeight = Rally::COURT_HALF_HEIGHT + VIEW_MARGIN;
    if (halfHeight * aspectRatio < halfWidth) {
        halfHeight = halfWidth / aspectRatio;
    }
    spriteCamera->orthographicSize = halfHeight;
}

void Samples::SceneRally::Place(float ownPaddleX, float otherPaddleX, float ballX, float ballY, bool bBallVisible) {
    spriteTransforms[OWN_PADDLE].SetPosition(ownPaddleX, -Rally::PADDLE_Y, 0.0f);
    spriteTransforms[OTHER_PADDLE].SetPosition(otherPaddleX, Rally::PADDLE_Y, 0.0f);
    if (bBallVisible) {
        spriteTransforms[BALL].SetPosition(ballX, ballY, 0.0f);
    } else {
        spriteTransforms[BALL].SetPosition(9000.0f, 9000.0f, 0.0f);
    }
}

float Samples::SceneRally::ScreenToWorldX(float normalizedX) const {
    const float halfWidth = spriteCamera->orthographicSize * spriteCamera->aspectRatio;
    return (normalizedX * 2.0f - 1.0f) * halfWidth;
}
