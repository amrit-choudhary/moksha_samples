#include "particle_life/game/particle_life_sim.h"

#include <cmath>
#include <cstring>

#include "moksha/shared/random/random_engine.h"

static constexpr float PI = 3.14159265358979f;

static float RadiusFor(uint32_t count, float width, float height) {
    const float n = static_cast<float>(count > 0 ? count : 1);
    return std::sqrt(Samples::ParticleLifeSim::NEIGHBOURS * width * height / (PI * n));
}

Samples::ParticleLifeSim::~ParticleLifeSim() {
    float* const floats[] = {x, y, vx, vy, spareX, spareY, spareVx, spareVy, forceX, forceY};
    for (float* array : floats) {
        delete[] array;
    }
    delete[] types;
    delete[] spareTypes;
    delete[] cellOf;
    delete[] cellStart;
    delete[] cellCursor;
}

bool Samples::ParticleLifeSim::Init(float inWidth, float inHeight) {
    if (!(inWidth > 0.0f) || !(inHeight > 0.0f) || x != nullptr) {
        return false;
    }
    width = inWidth;
    height = inHeight;

    // The most cells any count needs: the smallest radius, at the most particles.
    const float smallest = RadiusFor(MAX_PARTICLES, width, height);
    maxCells = static_cast<size_t>(width / smallest + 1.0f) * static_cast<size_t>(height / smallest + 1.0f);

    x = new float[MAX_PARTICLES];
    y = new float[MAX_PARTICLES];
    vx = new float[MAX_PARTICLES];
    vy = new float[MAX_PARTICLES];
    types = new uint8_t[MAX_PARTICLES];
    spareX = new float[MAX_PARTICLES];
    spareY = new float[MAX_PARTICLES];
    spareVx = new float[MAX_PARTICLES];
    spareVy = new float[MAX_PARTICLES];
    spareTypes = new uint8_t[MAX_PARTICLES];
    forceX = new float[MAX_PARTICLES];
    forceY = new float[MAX_PARTICLES];
    cellOf = new uint32_t[MAX_PARTICLES];
    cellStart = new uint32_t[maxCells + 1];
    cellCursor = new uint32_t[maxCells];

    RandomizeRules(1);
    Reset(0, 1);
    return true;
}

void Samples::ParticleLifeSim::Reset(uint32_t inCount, uint32_t seed) {
    if (x == nullptr) {
        return;
    }
    count = inCount < MAX_PARTICLES ? inCount : MAX_PARTICLES;
    FitGrid();

    ME::Random random{seed};
    for (uint32_t i = 0; i < count; ++i) {
        x[i] = static_cast<float>(random.NextDouble()) * width;
        y[i] = static_cast<float>(random.NextDouble()) * height;
        // NextDouble can return exactly 1; keep positions inside the half-open world.
        x[i] = x[i] < width ? x[i] : 0.0f;
        y[i] = y[i] < height ? y[i] : 0.0f;
        vx[i] = 0.0f;
        vy[i] = 0.0f;
        types[i] = static_cast<uint8_t>(random.NextRange(0, TYPE_COUNT - 1));
    }
}

void Samples::ParticleLifeSim::RandomizeRules(uint32_t seed) {
    ME::Random random{seed};
    for (float& rule : rules) {
        rule = static_cast<float>(random.NextDouble()) * 2.0f - 1.0f;
    }
}

void Samples::ParticleLifeSim::FitGrid() {
    radius = RadiusFor(count, width, height);
    // At least three cells each way, so the 3 × 3 neighbourhood never holds a cell twice.
    const float largest = (width < height ? width : height) / 3.0f;
    radius = radius < largest ? radius : largest;

    columns = static_cast<int32_t>(width / radius);
    rows = static_cast<int32_t>(height / radius);
    columns = columns >= 3 ? columns : 3;
    rows = rows >= 3 ? rows : 3;
    // Never more cells than Init allocated (the radius is never below the one at MAX_PARTICLES).
    while (static_cast<size_t>(columns) * static_cast<size_t>(rows) > maxCells) {
        columns > rows ? --columns : --rows;
    }
    cellWidth = width / static_cast<float>(columns);
    cellHeight = height / static_cast<float>(rows);
}

void Samples::ParticleLifeSim::Step(float deltaTime) {
    if (count == 0 || !(deltaTime > 0.0f)) {
        return;
    }
    const float dt = deltaTime < MAX_STEP_TIME ? deltaTime : MAX_STEP_TIME;

    SortByCell();
    AccumulateForces();

    const float friction = std::pow(0.5f, dt / FRICTION_HALF_LIFE);
    const float push = radius * FORCE_SCALE * dt;
    for (uint32_t i = 0; i < count; ++i) {
        vx[i] = vx[i] * friction + forceX[i] * push;
        vy[i] = vy[i] * friction + forceY[i] * push;

        float px = x[i] + vx[i] * dt;
        float py = y[i] + vy[i] * dt;
        // A step moves far less than the world's size, so one wrap is enough; the last checks catch rounding.
        px = px < 0.0f ? px + width : (px >= width ? px - width : px);
        py = py < 0.0f ? py + height : (py >= height ? py - height : py);
        x[i] = px >= 0.0f && px < width ? px : 0.0f;
        y[i] = py >= 0.0f && py < height ? py : 0.0f;
    }
}

void Samples::ParticleLifeSim::SortByCell() {
    const size_t cellCount = static_cast<size_t>(columns) * static_cast<size_t>(rows);
    const float toColumn = 1.0f / cellWidth;
    const float toRow = 1.0f / cellHeight;

    std::memset(cellCursor, 0, cellCount * sizeof(uint32_t));
    for (uint32_t i = 0; i < count; ++i) {
        int32_t column = static_cast<int32_t>(x[i] * toColumn);
        int32_t row = static_cast<int32_t>(y[i] * toRow);
        column = column < columns ? column : columns - 1;
        row = row < rows ? row : rows - 1;
        const uint32_t cell = static_cast<uint32_t>(row * columns + column);
        cellOf[i] = cell;
        ++cellCursor[cell];
    }

    uint32_t start = 0;
    for (size_t c = 0; c < cellCount; ++c) {
        cellStart[c] = start;
        start += cellCursor[c];
        cellCursor[c] = cellStart[c];
    }
    cellStart[cellCount] = start;

    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t to = cellCursor[cellOf[i]]++;
        spareX[to] = x[i];
        spareY[to] = y[i];
        spareVx[to] = vx[i];
        spareVy[to] = vy[i];
        spareTypes[to] = types[i];
    }

    float* swapX = x;
    x = spareX;
    spareX = swapX;
    float* swapY = y;
    y = spareY;
    spareY = swapY;
    float* swapVx = vx;
    vx = spareVx;
    spareVx = swapVx;
    float* swapVy = vy;
    vy = spareVy;
    spareVy = swapVy;
    uint8_t* swapTypes = types;
    types = spareTypes;
    spareTypes = swapTypes;
}

void Samples::ParticleLifeSim::AccumulateForces() {
    const float radiusSquared = radius * radius;
    const float toUnit = 1.0f / radius;
    const float toRepel = 1.0f / REPEL_DISTANCE;
    const float toPeak = 1.0f / (1.0f - REPEL_DISTANCE);

    for (int32_t row = 0; row < rows; ++row) {
        for (int32_t column = 0; column < columns; ++column) {
            // The 3 × 3 neighbourhood: each cell's range, and the shift that brings a wrapped cell next to this one.
            uint32_t begin[9];
            uint32_t end[9];
            float shiftX[9];
            float shiftY[9];
            int32_t n = 0;
            for (int32_t dy = -1; dy <= 1; ++dy) {
                int32_t r = row + dy;
                float sy = 0.0f;
                if (r < 0) {
                    r += rows;
                    sy = -height;
                } else if (r >= rows) {
                    r -= rows;
                    sy = height;
                }
                for (int32_t dx = -1; dx <= 1; ++dx) {
                    int32_t c = column + dx;
                    float sx = 0.0f;
                    if (c < 0) {
                        c += columns;
                        sx = -width;
                    } else if (c >= columns) {
                        c -= columns;
                        sx = width;
                    }
                    const uint32_t cell = static_cast<uint32_t>(r * columns + c);
                    begin[n] = cellStart[cell];
                    end[n] = cellStart[cell + 1];
                    shiftX[n] = sx;
                    shiftY[n] = sy;
                    ++n;
                }
            }

            const uint32_t cell = static_cast<uint32_t>(row * columns + column);
            for (uint32_t i = cellStart[cell]; i < cellStart[cell + 1]; ++i) {
                const float px = x[i];
                const float py = y[i];
                const float* rule = &rules[types[i] * TYPE_COUNT];
                float fx = 0.0f;
                float fy = 0.0f;
                for (int32_t k = 0; k < 9; ++k) {
                    const float ox = shiftX[k] - px;
                    const float oy = shiftY[k] - py;
                    for (uint32_t j = begin[k]; j < end[k]; ++j) {
                        const float dx = x[j] + ox;
                        const float dy = y[j] + oy;
                        const float distanceSquared = dx * dx + dy * dy;
                        if (distanceSquared >= radiusSquared || distanceSquared <= 0.0f) {
                            continue;
                        }
                        const float distance = std::sqrt(distanceSquared);
                        const float t = distance * toUnit;
                        // Repel up close; past that, a tent that peaks halfway out, scaled by the rule.
                        const float force =
                            t < REPEL_DISTANCE
                                ? t * toRepel - 1.0f
                                : rule[types[j]] * (1.0f - std::fabs(2.0f * t - 1.0f - REPEL_DISTANCE) * toPeak);
                        const float scale = force / distance;
                        fx += dx * scale;
                        fy += dy * scale;
                    }
                }
                forceX[i] = fx;
                forceY[i] = fy;
            }
        }
    }
}

uint32_t Samples::ParticleLifeSim::Checksum() const {
    // FNV-1a over positions quantized to 1/16 of a unit, and types.
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t values[3] = {static_cast<uint32_t>(x[i] * 16.0f), static_cast<uint32_t>(y[i] * 16.0f), types[i]};
        for (uint32_t value : values) {
            hash = (hash ^ value) * 16777619u;
        }
    }
    return hash;
}
