#include "graphics_stress/scene/scene_graphics_stress.h"

#include <cmath>

#include "moksha/client/utils/json_utils.h"
#include "moksha/shared/math/math.h"
#include "moksha/shared/random/random_engine.h"

static constexpr const char* CLOUD_TEXTURE = "textures/galaxy/clouds.dds";
static constexpr const char* STAR_TEXTURE = "textures/galaxy/stars.dds";
static constexpr const char* CLOUD_ATLAS = "texture_data/atlas_clouds.json";
static constexpr const char* STAR_ATLAS = "texture_data/atlas_stars.json";
static constexpr uint32_t ATLAS_TILES = 4;

static constexpr float VIEW_HALF_HEIGHT = 100.0f;
static constexpr float GALAXY_RADIUS = 115.0f;  // past a portrait screen's corners, so any roll fills the view

static constexpr float CORE_SHARE = 0.2f;  // of all sprites; the rest lie along the arms
static constexpr float CORE_RADIUS = 22.0f;
static constexpr float ARM_TWIST = 2.4f;  // radians per unit of log radius
static constexpr float ARM_SCALE = 12.0f;
static constexpr float ARM_SPREAD = 0.45f;  // radians to either side of an arm

static constexpr float MIN_CLOUD_SIZE = 30.0f;
static constexpr float MAX_CLOUD_SIZE = 50.0f;
static constexpr float MIN_STAR_SIZE = 0.5f;
static constexpr float MAX_STAR_SIZE = 1.4f;

static constexpr float MIN_GIANT_SIZE = 3.0f;
static constexpr float MAX_GIANT_SIZE = 7.0f;
static constexpr uint16_t GIANT_TILE = 2;   // the four-point flare
static constexpr float ORBIT_SPEED = 6.0f;  // world units per second, the same at every radius, as in a real galaxy

// Depth test is Less with writes on, so each instance must be nearer than the last to blend over it. Equal depths
// would hide all but the first sprite and remove the overdraw this sample measures.
static constexpr float FARTHEST_GIANT_Z = 95.0f;  // plain sprites draw first, so they go behind everything
static constexpr float GIANT_Z_STEP = 0.005f;
static constexpr float FARTHEST_CLOUD_Z = 80.0f;
static constexpr float CLOUD_Z_STEP = 0.005f;
static constexpr float FARTHEST_STAR_Z = 51.0f;
static constexpr float STAR_Z_STEP = 0.0005f;

static float Random01(ME::Random& random) {
    return static_cast<float>(random.NextDouble());
}

// A rough bell curve over [-1, 1].
static float RandomBell(ME::Random& random) {
    return (Random01(random) + Random01(random) + Random01(random) - 1.5f) / 1.5f;
}

// A point on the galaxy: in the central bulge, or along one of its two arms.
static ME::Vec3 RandomGalaxyPoint(ME::Random& random, float z) {
    const float tau = static_cast<float>(ME::TAU);
    float radius;
    float angle;
    if (Random01(random) < CORE_SHARE) {
        radius = CORE_RADIUS * Random01(random);
        angle = Random01(random) * tau;
    } else {
        radius = GALAXY_RADIUS * std::sqrt(Random01(random));
        const float arm = random.NextRange(0, 1) == 0 ? 0.0f : tau * 0.5f;
        angle = arm + ARM_TWIST * std::log(1.0f + radius / ARM_SCALE) + RandomBell(random) * ARM_SPREAD;
    }
    return ME::Vec3(radius * std::cos(angle), radius * std::sin(angle), z);
}

static float OrbitRate(const ME::Vec3& position) {
    return ORBIT_SPEED / (std::sqrt(position.x * position.x + position.y * position.y) + ARM_SCALE);
}

// One orbit step, without trigonometry. Using the new x for y keeps the orbit closed.
static void Orbit(ME::Transform& transform, float angle) {
    const ME::Vec3 position = transform.GetPosition();
    const float x = position.x - position.y * angle;
    transform.SetPosition(x, position.y + x * angle, position.z);
}

static ME::Color Mix(const ME::Color& a, const ME::Color& b, float t) {
    return ME::Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1.0f);
}

// Warm at the core through magenta to blue at the rim.
static ME::Color GalaxyColor(const ME::Vec3& position) {
    constexpr ME::Color CORE{1.0f, 0.72f, 0.38f, 1.0f};
    constexpr ME::Color MIDDLE{0.78f, 0.25f, 0.62f, 1.0f};
    constexpr ME::Color RIM{0.18f, 0.32f, 0.95f, 1.0f};
    const float t = std::sqrt(position.x * position.x + position.y * position.y) / GALAXY_RADIUS;
    return t < 0.35f ? Mix(CORE, MIDDLE, t / 0.35f) : Mix(MIDDLE, RIM, t > 1.0f ? 1.0f : (t - 0.35f) / 0.65f);
}

void Samples::SceneGraphicsStress::CreateResources() {
    ME::Scene::CreateResources();

    // Slot 0: plain sprites (giants). Slot 1: instanced set 0 (clouds). Slot 2: instanced set 1 (stars).
    const char* const textures[] = {STAR_TEXTURE, CLOUD_TEXTURE, STAR_TEXTURE};
    const char* const atlases[] = {STAR_ATLAS, CLOUD_ATLAS, STAR_ATLAS};
    for (size_t i = 0; i < 3; ++i) {
        spriteTexturePaths[i] = textures[i];
        ME::JsonUtils::LoadTextureAtlasProps(atlases[i], textureAtlasProperties[i]);
    }
    spriteTexturePaths.count = 3;
    textureAtlasProperties.count = 3;
}

void Samples::SceneGraphicsStress::BuildCamera() {
    ME::Scene::BuildCamera();
    spriteCamera->orthographicSize = VIEW_HALF_HEIGHT;
}

void Samples::SceneGraphicsStress::BuildSpriteTransforms() {
    ME::Scene::BuildSpriteTransforms();

    ME::Random random{909u};
    spriteTransforms.count = GIANT_COUNT;
    for (uint32_t i = 0; i < GIANT_COUNT; ++i) {
        const float size = MIN_GIANT_SIZE + (MAX_GIANT_SIZE - MIN_GIANT_SIZE) * Random01(random);
        spriteTransforms[i].SetPosition(
            RandomGalaxyPoint(random, FARTHEST_GIANT_Z - static_cast<float>(i) * GIANT_Z_STEP));
        spriteTransforms[i].SetScale(size, size);
        giantOrbitRates[i] = OrbitRate(spriteTransforms[i].GetPosition());
    }
}

void Samples::SceneGraphicsStress::BuildSpriteRenderers() {
    ME::Scene::BuildSpriteRenderers();

    spriteRenderers.count = GIANT_COUNT;
    for (uint32_t i = 0; i < GIANT_COUNT; ++i) {
        const ME::Color color = Mix(GalaxyColor(spriteTransforms[i].GetPosition()), ME::Color::White(), 0.5f);
        spriteRenderers[i] = ME::SpriteRenderer(0, 0, 0, 0, GIANT_TILE, color);
    }
}

void Samples::SceneGraphicsStress::BuildInstancedSpriteTransforms() {
    ME::Scene::BuildInstancedSpriteTransforms();

    // Fixed seed: the same galaxy, and so the same load, on every run and device.
    ME::Random random{4242u};
    const float tau = static_cast<float>(ME::TAU);

    instancedSpriteTransforms0.count = MAX_CLOUDS;
    for (uint32_t i = 0; i < MAX_CLOUDS; ++i) {
        ME::Transform& transform = instancedSpriteTransforms0[i];
        cloudSizes[i] = MIN_CLOUD_SIZE + (MAX_CLOUD_SIZE - MIN_CLOUD_SIZE) * Random01(random);
        transform.SetPosition(RandomGalaxyPoint(random, FARTHEST_CLOUD_Z - static_cast<float>(i) * CLOUD_Z_STEP));
        transform.SetRotation(0.0f, 0.0f, Random01(random) * tau);
        transform.SetScale(cloudSizes[i], cloudSizes[i]);
    }

    instancedSpriteTransforms1.count = MAX_STARS;
    for (uint32_t i = 0; i < MAX_STARS; ++i) {
        ME::Transform& transform = instancedSpriteTransforms1[i];
        const float t = Random01(random);
        starSizes[i] = MIN_STAR_SIZE + (MAX_STAR_SIZE - MIN_STAR_SIZE) * t * t;
        transform.SetPosition(RandomGalaxyPoint(random, FARTHEST_STAR_Z - static_cast<float>(i) * STAR_Z_STEP));
        transform.SetRotation(0.0f, 0.0f, Random01(random) * tau);
        transform.SetScale(starSizes[i], starSizes[i]);
        starOrbitRates[i] = OrbitRate(transform.GetPosition());
    }
}

void Samples::SceneGraphicsStress::BuildInstancedSpriteRenderers() {
    ME::Scene::BuildInstancedSpriteRenderers();

    ME::Random random{2424u};

    instancedSpriteRenderers0.count = MAX_CLOUDS;
    for (uint32_t i = 0; i < MAX_CLOUDS; ++i) {
        // The texture's alpha stops at the shader's discard level, so this low alpha is what fades the edges out.
        ME::Color color = GalaxyColor(instancedSpriteTransforms0[i].GetPosition());
        color.a = 0.05f + 0.05f * Random01(random);
        const uint16_t tile = static_cast<uint16_t>(random.NextRange(0, ATLAS_TILES - 1));
        instancedSpriteRenderers0[i] = ME::SpriteRenderer(0, 0, 1, 1, tile, color);
    }

    instancedSpriteRenderers1.count = MAX_STARS;
    for (uint32_t i = 0; i < MAX_STARS; ++i) {
        // Mostly white, with a hint of the arm a star sits in.
        const ME::Color color = Mix(GalaxyColor(instancedSpriteTransforms1[i].GetPosition()), ME::Color::White(),
                                    0.45f + 0.55f * Random01(random));
        const uint16_t tile = static_cast<uint16_t>(random.NextRange(0, ATLAS_TILES - 1));
        instancedSpriteRenderers1[i] = ME::SpriteRenderer(0, 0, 2, 2, tile, color);
    }
}

const char* Samples::SceneGraphicsStress::GetDisplayName() const {
    return "Galaxy";
}

void Samples::SceneGraphicsStress::SetStarCount(uint32_t count) {
    const uint32_t newCount = count < MAX_STARS ? count : MAX_STARS;
    for (uint32_t i = newCount; i < starCount; ++i) {
        instancedSpriteTransforms1[i].SetScale(0.0f, 0.0f);
        instancedSpriteRenderers1[i].bDirty = true;
    }
    for (uint32_t i = starCount; i < newCount; ++i) {
        instancedSpriteTransforms1[i].SetScale(starSizes[i], starSizes[i]);
        instancedSpriteRenderers1[i].bDirty = true;
    }
    starCount = newCount;
}

void Samples::SceneGraphicsStress::SetCloudCount(uint32_t count) {
    const uint32_t newCount = count < MAX_CLOUDS ? count : MAX_CLOUDS;
    for (uint32_t i = newCount; i < cloudCount; ++i) {
        instancedSpriteTransforms0[i].SetScale(0.0f, 0.0f);
        instancedSpriteRenderers0[i].bDirty = true;
    }
    for (uint32_t i = cloudCount; i < newCount; ++i) {
        const float size = cloudSizes[i] * cloudScale;
        instancedSpriteTransforms0[i].SetScale(size, size);
        instancedSpriteRenderers0[i].bDirty = true;
    }
    cloudCount = newCount;
}

void Samples::SceneGraphicsStress::SetCloudScale(float scale) {
    cloudScale = scale;
    for (uint32_t i = 0; i < cloudCount; ++i) {
        const float size = cloudSizes[i] * cloudScale;
        instancedSpriteTransforms0[i].SetScale(size, size);
        instancedSpriteRenderers0[i].bDirty = true;
    }
}

void Samples::SceneGraphicsStress::MoveStars(uint32_t count, float deltaTime) {
    const uint32_t moving = count < starCount ? count : starCount;
    for (uint32_t i = 0; i < moving; ++i) {
        Orbit(instancedSpriteTransforms1[i], starOrbitRates[i] * deltaTime);
        instancedSpriteRenderers1[i].bDirty = true;
    }
}

void Samples::SceneGraphicsStress::MoveGiants(float deltaTime) {
    for (uint32_t i = 0; i < GIANT_COUNT; ++i) {
        Orbit(spriteTransforms[i], giantOrbitRates[i] * deltaTime);
    }
}

void Samples::SceneGraphicsStress::SetRoll(float angle) {
    spriteCamera->up = ME::Vec3(std::sin(angle), std::cos(angle), 0.0f);
}
