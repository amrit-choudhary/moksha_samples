#include "particle_life/scene/scene_particle_life.h"

#include <cmath>

#include "moksha/client/utils/json_utils.h"
#include "particle_life/game/particle_life_sim.h"

static constexpr const char* GLOW_TEXTURE = "textures/particle_glow.dds";
static constexpr const char* GLOW_ATLAS = "texture_data/atlas_glow.json";

static constexpr float PARTICLE_Z = 10.0f;
static constexpr float SIZE_PER_RADIUS = 1.1f;  // a sprite's width, in interaction radii
static constexpr float MIN_GLOW = 0.4f;         // brightness at rest; full at FAST_SPEED and above
static constexpr float FAST_SPEED = 3.0f;       // interaction radii per second
static constexpr float GLOW_ALPHA = 0.8f;       // what each sprite adds, before its brightness

// One vivid colour per particle type (ParticleLifeSim::TYPE_COUNT).
static constexpr ME::Color TYPE_COLORS[] = {
    {1.00f, 0.22f, 0.35f, 1.0f},  // rose
    {1.00f, 0.58f, 0.12f, 1.0f},  // amber
    {0.70f, 1.00f, 0.20f, 1.0f},  // lime
    {0.12f, 0.95f, 0.85f, 1.0f},  // aqua
    {0.25f, 0.45f, 1.00f, 1.0f},  // azure
    {0.82f, 0.30f, 1.00f, 1.0f},  // violet
};
static_assert(sizeof(TYPE_COLORS) / sizeof(TYPE_COLORS[0]) == Samples::ParticleLifeSim::TYPE_COUNT);

void Samples::SceneParticleLife::CreateResources() {
    ME::Scene::CreateResources();

    // Instanced set 0 draws slot 1; slot 0 is filled too, as the slots below a set's must be.
    for (size_t i = 0; i < 2; ++i) {
        spriteTexturePaths[i] = GLOW_TEXTURE;
        ME::JsonUtils::LoadTextureAtlasProps(GLOW_ATLAS, textureAtlasProperties[i]);
    }
    spriteTexturePaths.count = 2;
    textureAtlasProperties.count = 2;
}

void Samples::SceneParticleLife::BuildCamera() {
    ME::Scene::BuildCamera();
    spriteCamera->orthographicSize = VIEW_HALF_HEIGHT;
}

void Samples::SceneParticleLife::BuildInstancedSpriteTransforms() {
    ME::Scene::BuildInstancedSpriteTransforms();

    instancedSpriteTransforms0.count = ParticleLifeSim::MAX_PARTICLES;
    for (uint32_t i = 0; i < ParticleLifeSim::MAX_PARTICLES; ++i) {
        instancedSpriteTransforms0[i].SetPosition(0.0f, 0.0f, PARTICLE_Z);
        instancedSpriteTransforms0[i].SetScale(0.0f, 0.0f);
    }
}

void Samples::SceneParticleLife::BuildInstancedSpriteRenderers() {
    ME::Scene::BuildInstancedSpriteRenderers();

    instancedSpriteRenderers0.count = ParticleLifeSim::MAX_PARTICLES;
    for (uint32_t i = 0; i < ParticleLifeSim::MAX_PARTICLES; ++i) {
        instancedSpriteRenderers0[i] = ME::SpriteRenderer(0, 0, 1, 1, 0);
    }
    instancedSpriteBlend0 = ME::SpriteBlend::Additive;
    instancedSpriteDrawCount0 = 0;
}

const char* Samples::SceneParticleLife::GetDisplayName() const {
    return "Particle Life";
}

void Samples::SceneParticleLife::Sync(const ParticleLifeSim& sim) {
    const uint32_t count = sim.GetCount();
    const float* x = sim.GetX();
    const float* y = sim.GetY();
    const float* vx = sim.GetVelocityX();
    const float* vy = sim.GetVelocityY();
    const uint8_t* types = sim.GetTypes();

    // The world's origin is its corner; the view's is its centre.
    const float left = -0.5f * sim.GetWidth();
    const float bottom = -0.5f * sim.GetHeight();
    const float size = sim.GetRadius() * SIZE_PER_RADIUS;
    const float toGlow = (1.0f - MIN_GLOW) / (FAST_SPEED * sim.GetRadius());

    for (uint32_t i = 0; i < count; ++i) {
        ME::Transform& transform = instancedSpriteTransforms0[i];
        transform.SetPosition(left + x[i], bottom + y[i], PARTICLE_Z);
        transform.SetScale(size, size);

        const float speed = std::sqrt(vx[i] * vx[i] + vy[i] * vy[i]);
        const float glow = MIN_GLOW + speed * toGlow;
        ME::Color color = TYPE_COLORS[types[i]];
        color.a = GLOW_ALPHA * (glow < 1.0f ? glow : 1.0f);

        ME::SpriteRenderer& renderer = instancedSpriteRenderers0[i];
        renderer.color = color;
        renderer.bDirty = true;
    }
    instancedSpriteDrawCount0 = count;
}
