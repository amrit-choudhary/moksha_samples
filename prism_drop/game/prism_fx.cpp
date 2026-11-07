#include "prism_drop/game/prism_fx.h"

#include <cmath>

#include "prism_drop/game/prism_palette.h"

static constexpr float TWO_PI = 6.2831853f;

void Samples::PrismFx::Init(ScenePrismDrop* inScene) {
    scene = inScene;
    for (AmbientMote& mote : motes) {
        ResetMote(mote, true);
    }
}

void Samples::PrismFx::Update(float deltaTime) {
    time += deltaTime;
    UpdateAmbient(deltaTime);

    for (size_t i = 0; i < ScenePrismDrop::EFFECT_SLOTS; ++i) {
        Particle& p = particles[i];
        const size_t slot = ScenePrismDrop::AMBIENT_SLOTS + i;
        if (!p.bActive) {
            continue;
        }
        p.life += deltaTime;
        const float t = p.life / p.maxLife;
        if (t >= 1.0f) {
            p.bActive = false;
            scene->HideFx(slot);
            continue;
        }
        const float decay = std::exp(-p.drag * deltaTime);
        p.vx *= decay;
        p.vy = p.vy * decay + p.gravity * deltaTime;
        p.x += p.vx * deltaTime;
        p.y += p.vy * deltaTime;
        p.angle += p.spin * deltaTime;

        // Sizes ease out; alpha fades in over fadeIn, then out with the square of the remaining life.
        const float ease = 1.0f - (1.0f - t) * (1.0f - t);
        const float width = p.width0 + (p.width1 - p.width0) * ease;
        const float height = p.height0 + (p.height1 - p.height0) * ease;
        float alpha = (1.0f - t) * (1.0f - t);
        if (p.fadeIn > 0.0f && t < p.fadeIn) {
            alpha *= t / p.fadeIn;
        }
        ME::Color color = p.color;
        color.a *= alpha;
        scene->SetFx(slot, p.x, p.y, width, height, p.angle, p.tile, color);
    }
}

void Samples::PrismFx::Clear() {
    for (size_t i = 0; i < ScenePrismDrop::EFFECT_SLOTS; ++i) {
        particles[i].bActive = false;
        scene->HideFx(ScenePrismDrop::AMBIENT_SLOTS + i);
    }
}

Samples::Particle& Samples::PrismFx::Emit() {
    // Prefer a free slot near the cursor; when none is free in a short scan, reuse the cursor's slot.
    size_t chosen = cursor;
    for (size_t scan = 0; scan < 64; ++scan) {
        const size_t index = (cursor + scan) % ScenePrismDrop::EFFECT_SLOTS;
        if (!particles[index].bActive) {
            chosen = index;
            break;
        }
    }
    cursor = (chosen + 1) % ScenePrismDrop::EFFECT_SLOTS;
    Particle& p = particles[chosen];
    p = Particle();
    p.bActive = true;
    return p;
}

void Samples::PrismFx::Shatter(float x, float y, const ME::Color& color, float power) {
    const int shards = static_cast<int>(3.0f * power + 0.5f);
    for (int i = 0; i < shards; ++i) {
        Particle& p = Emit();
        const float angle = RandomRange(0.0f, TWO_PI);
        const float speed = RandomRange(80.0f, 260.0f) * power;
        p.x = x + RandomRange(-8.0f, 8.0f);
        p.y = y + RandomRange(-8.0f, 8.0f);
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed + 120.0f;
        p.gravity = -900.0f;
        p.drag = 1.5f;
        p.spin = RandomRange(-12.0f, 12.0f);
        p.angle = RandomRange(0.0f, TWO_PI);
        const float size = RandomRange(7.0f, 13.0f);
        p.width0 = size;
        p.height0 = size;
        p.width1 = size * 0.3f;
        p.height1 = size * 0.3f;
        p.maxLife = RandomRange(0.55f, 0.9f);
        p.color = PrismPalette::Lighten(color, RandomRange(0.0f, 0.4f));
        p.tile = PrismFxTiles::SHARD;
    }
    for (int i = 0; i < 2; ++i) {
        Particle& p = Emit();
        const float angle = RandomRange(0.0f, TWO_PI);
        const float speed = RandomRange(150.0f, 420.0f) * power;
        p.x = x;
        p.y = y;
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.drag = 4.0f;
        p.width0 = p.height0 = RandomRange(4.0f, 7.0f);
        p.width1 = p.height1 = 1.0f;
        p.maxLife = RandomRange(0.3f, 0.55f);
        p.color = PrismPalette::Lighten(color, 0.6f);
        p.tile = PrismFxTiles::SPARK;
    }
    Particle& flash = Emit();
    flash.x = x;
    flash.y = y;
    flash.width0 = flash.height0 = ScenePrismDrop::CELL * 1.2f;
    flash.width1 = flash.height1 = ScenePrismDrop::CELL * 2.4f;
    flash.maxLife = 0.25f;
    flash.color = PrismPalette::Lighten(color, 0.5f);
    flash.color.a = 0.8f;
    flash.tile = PrismFxTiles::GLOW;
}

void Samples::PrismFx::Ring(float x, float y, const ME::Color& color, float fromSize, float toSize, float seconds) {
    Particle& p = Emit();
    p.x = x;
    p.y = y;
    p.width0 = p.height0 = fromSize;
    p.width1 = p.height1 = toSize;
    p.maxLife = seconds;
    p.color = color;
    p.tile = PrismFxTiles::RING;
}

void Samples::PrismFx::Streak(float x, float yBottom, float yTop, float width, const ME::Color& color) {
    if (yTop <= yBottom) {
        return;
    }
    Particle& p = Emit();
    p.x = x;
    p.y = (yTop + yBottom) * 0.5f;
    p.width0 = width;
    p.width1 = width * 0.2f;
    p.height0 = yTop - yBottom;
    p.height1 = (yTop - yBottom) * 0.6f;
    p.maxLife = 0.32f;
    p.color = color;
    p.color.a = 0.65f;
    p.tile = PrismFxTiles::STREAK;
}

void Samples::PrismFx::Dust(float x, float y, float width, const ME::Color& color, int count) {
    for (int i = 0; i < count; ++i) {
        Particle& p = Emit();
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        p.x = x + RandomRange(-width * 0.5f, width * 0.5f);
        p.y = y + RandomRange(-3.0f, 3.0f);
        p.vx = side * RandomRange(30.0f, 140.0f);
        p.vy = RandomRange(10.0f, 70.0f);
        p.drag = 3.5f;
        p.gravity = -60.0f;
        const float size = RandomRange(6.0f, 12.0f);
        p.width0 = p.height0 = size;
        p.width1 = p.height1 = size * 2.2f;
        p.maxLife = RandomRange(0.35f, 0.6f);
        p.color = PrismPalette::Lighten(color, 0.5f);
        p.color.a = 0.45f;
        p.tile = (i % 3 == 0) ? PrismFxTiles::SPARK : PrismFxTiles::SMOKE;
        if (p.tile == PrismFxTiles::SPARK) {
            p.width0 = p.height0 = 3.0f;
            p.width1 = p.height1 = 1.0f;
            p.color.a = 0.9f;
        }
    }
}

void Samples::PrismFx::Twinkles(float x0, float y0, float x1, float y1, const ME::Color& color, int count) {
    for (int i = 0; i < count; ++i) {
        Particle& p = Emit();
        p.x = RandomRange(x0, x1);
        p.y = RandomRange(y0, y1);
        p.vy = RandomRange(10.0f, 50.0f);
        p.spin = RandomRange(-2.0f, 2.0f);
        const float size = RandomRange(10.0f, 24.0f);
        p.width0 = p.height0 = size * 0.3f;
        p.width1 = p.height1 = size;
        p.maxLife = RandomRange(0.5f, 1.1f);
        p.fadeIn = 0.25f;
        p.color = PrismPalette::Lighten(color, RandomRange(0.2f, 0.7f));
        p.tile = PrismFxTiles::TWINKLE;
    }
}

void Samples::PrismFx::Fountain(float x, float y, float angle, int count) {
    for (int i = 0; i < count; ++i) {
        Particle& p = Emit();
        const float a = angle + RandomRange(-0.45f, 0.45f);
        const float speed = RandomRange(350.0f, 700.0f);
        p.x = x;
        p.y = y;
        p.vx = std::cos(a) * speed;
        p.vy = std::sin(a) * speed;
        p.gravity = -700.0f;
        p.drag = 1.2f;
        p.spin = RandomRange(-14.0f, 14.0f);
        p.angle = RandomRange(0.0f, TWO_PI);
        const float size = RandomRange(6.0f, 11.0f);
        p.width0 = size;
        p.height0 = size * 0.6f;
        p.width1 = size;
        p.height1 = size * 0.6f;
        p.maxLife = RandomRange(1.0f, 1.6f);
        p.color = PrismPalette::Piece(static_cast<size_t>(random.NextRange(0, PrismPalette::PIECE_COLOR_COUNT - 1)));
        p.tile = PrismFxTiles::SHARD;
    }
}

void Samples::PrismFx::TitleRain(float viewHalfWidth) {
    Particle& p = Emit();
    p.x = RandomRange(-viewHalfWidth, viewHalfWidth);
    p.y = ScenePrismDrop::VIEW_HALF_HEIGHT + 20.0f;
    p.vy = RandomRange(-160.0f, -70.0f);
    p.vx = RandomRange(-10.0f, 10.0f);
    p.spin = RandomRange(-1.5f, 1.5f);
    p.angle = RandomRange(0.0f, TWO_PI);
    const float size = RandomRange(10.0f, 22.0f);
    p.width0 = p.height0 = size;
    p.width1 = p.height1 = size;
    p.maxLife = 6.0f;
    p.fadeIn = 0.1f;
    p.color = PrismPalette::Piece(static_cast<size_t>(random.NextRange(0, PrismPalette::PIECE_COLOR_COUNT - 1)));
    p.color.a = 0.6f;
    p.tile = PrismFxTiles::SHARD;
}

float Samples::PrismFx::RandomRange(float low, float high) {
    return low + (high - low) * static_cast<float>(random.NextDouble());
}

void Samples::PrismFx::UpdateAmbient(float deltaTime) {
    const float top = ScenePrismDrop::VIEW_HALF_HEIGHT + 20.0f;
    for (size_t i = 0; i < ScenePrismDrop::AMBIENT_SLOTS; ++i) {
        AmbientMote& mote = motes[i];
        mote.y += mote.speed * ambientSpeed * deltaTime;
        mote.x += std::sin(time * 0.6f + mote.phase) * 6.0f * deltaTime;
        if (mote.y > top) {
            ResetMote(mote, false);
        }
        const float twinkle = 0.55f + 0.45f * std::sin(time * mote.twinkleRate + mote.phase);
        ME::Color color = mote.color;
        color.a *= twinkle;
        const float size = mote.size * (0.8f + 0.2f * twinkle);
        scene->SetFx(i, mote.x, mote.y, size, size, time * 0.3f + mote.phase, mote.tile, color);
    }
}

void Samples::PrismFx::ResetMote(AmbientMote& mote, bool bAnywhere) {
    const float halfWidth = scene->GetViewHalfWidth() + 20.0f;
    const float halfHeight = ScenePrismDrop::VIEW_HALF_HEIGHT + 20.0f;
    mote.x = RandomRange(-halfWidth, halfWidth);
    mote.y = bAnywhere ? RandomRange(-halfHeight, halfHeight) : -halfHeight;
    mote.speed = RandomRange(8.0f, 30.0f);
    mote.size = RandomRange(3.0f, 9.0f);
    mote.phase = RandomRange(0.0f, TWO_PI);
    mote.twinkleRate = RandomRange(1.0f, 4.0f);
    const bool bStar = random.NextRange(0, 4) == 0;
    mote.tile = bStar ? PrismFxTiles::TWINKLE : PrismFxTiles::DOT;
    if (bStar) {
        mote.size *= 2.2f;
    }
    const uint32_t hue = random.NextRange(0, 2);
    mote.color = hue == 0 ? ME::Color(0.5f, 0.95f, 1.0f, 0.7f)
                          : (hue == 1 ? ME::Color(1.0f, 0.5f, 0.9f, 0.6f) : ME::Color(1.0f, 1.0f, 1.0f, 0.55f));
}
