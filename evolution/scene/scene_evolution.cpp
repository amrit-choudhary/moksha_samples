#include "evolution/scene/scene_evolution.h"

#include <cstdio>

#include "moksha/client/debug/debug_system.h"
#include "moksha/client/utils/json_utils.h"
#include "moksha/shared/math/math.h"
#include "moksha/shared/random/perlin_noise.h"
#include "moksha/shared/random/random_engine.h"

using namespace ME;

// Local helpers.
static void TerrainGen(ME::Grid<Samples::TerrainType>* oceanBase, ME::Grid<Samples::TerrainType>* ocean,
                       ME::Grid<Samples::TerrainType>* biome, ME::Grid<Samples::TerrainType>* terrain,
                       size_t oceanMapSize, size_t biomeMapSize, size_t mapSize);

static void CreatureGen(ME::Pool<Samples::Creature>* creaturePool, size_t initialCreatureCount,
                        ME::Grid<uint8_t>* walkableMap, size_t mapSize);

// End local helpers.

Samples::SceneEvolution::SceneEvolution() {}

Samples::SceneEvolution::~SceneEvolution() {
    delete oceanBase;
    delete ocean;
    delete biome;
    delete terrain;
    delete walkableMap;
    delete creaturePool;
}

void Samples::SceneEvolution::Init() {
    oceanBase = new ME::Grid<TerrainType>(oceanMapSize, oceanMapSize);
    ocean = new ME::Grid<TerrainType>(oceanMapSize, oceanMapSize);
    biome = new ME::Grid<TerrainType>(biomeMapSize, biomeMapSize);
    terrain = new ME::Grid<TerrainType>(mapSize, mapSize);
    TerrainGen(oceanBase, ocean, biome, terrain, oceanMapSize, biomeMapSize, mapSize);
    walkableMap = new ME::Grid<uint8_t>(mapSize, mapSize);

    for (size_t i = 0; i < mapSize * mapSize; ++i) {
        Samples::TerrainType tileType = *(terrain->GetUnsafe(i));
        if (tileType == Samples::TerrainType::Ocean || tileType == Samples::TerrainType::Lake ||
            tileType == Samples::TerrainType::Rocky || tileType == Samples::TerrainType::Forest) {
            *(walkableMap->GetUnsafe(i)) = 0;
        } else {
            *(walkableMap->GetUnsafe(i)) = 1;
        }
    }

    creaturePool = new ME::Pool<Creature>(creatureCount);
    CreatureGen(creaturePool, initialCreatureCount, walkableMap, mapSize);

    Scene::Init();
}

void Samples::SceneEvolution::CreateResources() {
    Scene::CreateResources();

    // Slot 1 is unused now. Only there to prevent Renderer crash. Fix later.

    spriteTexturePaths[0] = "textures/world/tileset_evolution.dds";
    spriteTexturePaths[1] = "textures/world/tileset_evolution.dds";
    spriteTexturePaths[2] = "textures/world/tileset_evolution.dds";
    spriteTexturePaths.count = 3;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_tileset_evolution.json", textureAtlasProperties[0]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_tileset_evolution.json", textureAtlasProperties[1]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_tileset_evolution.json", textureAtlasProperties[2]);
    textureAtlasProperties.count = 3;
}

void Samples::SceneEvolution::BuildLights() {
    Scene::BuildLights();
}

void Samples::SceneEvolution::BuildCamera() {
    Scene::BuildCamera();
    spriteCamera->orthographicSize = 100.0f;
}

void Samples::SceneEvolution::BuildTransforms() {
    Scene::BuildTransforms();
}

void Samples::SceneEvolution::BuildMeshRenderers() {
    Scene::BuildMeshRenderers();
}

void Samples::SceneEvolution::BuildSpriteTransforms() {
    Scene::BuildSpriteTransforms();
}

void Samples::SceneEvolution::BuildSpriteRenderers() {
    Scene::BuildSpriteRenderers();
}

void Samples::SceneEvolution::BuildInstancedSpriteTransforms() {
    Scene::BuildInstancedSpriteTransforms();

    // Add terrain.
    instancedSpriteTransforms0.count = mapSize * mapSize;
    for (size_t i = 0; i < mapSize * mapSize; ++i) {
        float x = static_cast<float>(i % mapSize) * tileSize + originX;
        float y = static_cast<float>(i / mapSize) * tileSize + originY;

        // Adding to instance buffer 0; pushed behind buffer 1 (creatures) in depth so they draw on top.
        instancedSpriteTransforms0[i].SetPosition(x, y, 1.0f);
        instancedSpriteTransforms0[i].SetScale(ME::Vec3(tileSize, tileSize, 1.0f));
    }

    // Add creatures. Pre-create one instanced transform per pool slot (not just the currently
    // active ones), so the DX instance buffer is sized for the pool's full capacity up front.
    // Slots beyond the pool's active count aren't spawned yet, so they're parked off-screen
    // until Acquire()'d.
    instancedSpriteTransforms1.count = creatureCount;
    for (size_t i = 0; i < creatureCount; ++i) {
        instancedSpriteTransforms1[i].SetScale(ME::Vec3(tileSize, tileSize, 1.0f));
        if (i < creaturePool->GetActiveCount()) {
            Samples::Creature* creature = &(*creaturePool)[i];
            float x = static_cast<float>(creature->position.x) * tileSize + originX;
            float y = static_cast<float>(creature->position.y) * tileSize + originY;
            instancedSpriteTransforms1[i].SetPosition(x, y, 0.0f);
        } else {
            instancedSpriteTransforms1[i].SetPosition(creatureParkPos);
        }
    }
}

void Samples::SceneEvolution::BuildInstancedSpriteRenderers() {
    Scene::BuildInstancedSpriteRenderers();

    // Add terrain.
    instancedSpriteRenderers0.count = mapSize * mapSize;
    for (size_t i = 0; i < mapSize * mapSize; ++i) {
        uint8_t tileIndex = static_cast<uint8_t>(*(terrain->GetUnsafe(i)));
        instancedSpriteRenderers0[i] = ME::SpriteRenderer(0, 0, 0, 0, tileIndex, ME::Color::White(), 0);
    }

    // Add creatures. Matches the full pool capacity used above, not just the active count.
    instancedSpriteRenderers1.count = creatureCount;
    for (size_t i = 0; i < creatureCount; ++i) {
        instancedSpriteRenderers1[i] = ME::SpriteRenderer(0, 0, 0, 0, 10, ME::Color::White(), 0);
    }
}

const char* Samples::SceneEvolution::GetDisplayName() const {
    return "Evolution Scene";
}

void Samples::SceneEvolution::UpdateCreatures(float deltaTime) {
    ME::Random creatureRnd{"creatureMove", true};

    for (size_t i = 0; i < creaturePool->GetActiveCount(); ++i) {
        Samples::Creature* creature = &(*creaturePool)[i];
        creature->Update(deltaTime, walkableMap, creatureRnd);
    }

    // Reproduction: spawn a fresh creature at the parent's position, then send the parent back to Moving
    size_t birthsThisFrame = 0;
    for (size_t i = 0; i < creaturePool->GetActiveCount(); ++i) {
        Samples::Creature* creature = &(*creaturePool)[i];
        if (creature->state == CreatureState::Reproducing) {
            Samples::Creature* offspring = creaturePool->Acquire();
            if (offspring != nullptr) {
                offspring->Spawn(creature->position, creatureRnd);
                ++birthsThisFrame;
            }

            creature->state = CreatureState::Moving;
            creature->reproCooldown = 0.0f;
        }
    }

    size_t activeCountBeforeRemoval = creaturePool->GetActiveCount();

    for (size_t i = creaturePool->GetActiveCount(); i-- > 0;) {
        Samples::Creature* creature = &(*creaturePool)[i];
        if (creature->state == CreatureState::Dying) {
            creaturePool->Release(creature);
        }
    }

    size_t activeCount = creaturePool->GetActiveCount();

    // Resync every remaining active slot's sprite against the pool's final state for this
    // frame, since slot identities may have shifted above.
    for (size_t i = 0; i < activeCount; ++i) {
        Samples::Creature* creature = &(*creaturePool)[i];
        float x = creature->position.x * tileSize + originX;
        float y = creature->position.y * tileSize + originY;
        instancedSpriteTransforms1[i].SetPosition(x, y, 0.0f);
        instancedSpriteRenderers1[i].bDirty = true;
    }

    // Park any slots freed this frame so released creatures actually disappear.
    // Just for little optimization, we only need to park the slots that were released this frame, not all the
    for (size_t i = activeCount; i < activeCountBeforeRemoval; ++i) {
        instancedSpriteTransforms1[i].SetPosition(creatureParkPos.x, creatureParkPos.y, creatureParkPos.z);
        instancedSpriteRenderers1[i].bDirty = true;
    }

    size_t deathsThisFrame = activeCountBeforeRemoval - activeCount;

    char aliveBuf[32];
    snprintf(aliveBuf, sizeof(aliveBuf), "Creatures alive: %zu", activeCount);
    ME::DebugSystem::ScreenPrintSlot(3, aliveBuf);

    char birthsBuf[32];
    snprintf(birthsBuf, sizeof(birthsBuf), "Births this frame: %zu", birthsThisFrame);
    ME::DebugSystem::ScreenPrintSlot(4, birthsBuf);

    char deathsBuf[32];
    snprintf(deathsBuf, sizeof(deathsBuf), "Deaths this frame: %zu", deathsThisFrame);
    ME::DebugSystem::ScreenPrintSlot(5, deathsBuf);
}

static void TerrainGen(ME::Grid<Samples::TerrainType>* oceanBase, ME::Grid<Samples::TerrainType>* ocean,
                       ME::Grid<Samples::TerrainType>* biome, ME::Grid<Samples::TerrainType>* terrain,
                       size_t oceanMapSize, size_t biomeMapSize, size_t mapSize) {
    biome->Fill(Samples::TerrainType::Grass);  // For now, biome is not used. Fill with Grass.
    biomeMapSize = 0;                          // Not used for now.

RETRY_OCEAN_GEN:
    ME::Random rnd{"oceanBase", true};
    for (size_t i = 0; i < oceanMapSize * oceanMapSize; ++i) {
        double f = rnd.NextDouble();
        if (f < 0.3f) {
            *(oceanBase->GetUnsafe(i)) = Samples::TerrainType::Ocean;
        } else {
            *(oceanBase->GetUnsafe(i)) = Samples::TerrainType::Grass;
        }
    }

    ocean->CopyFrom(*oceanBase);

    for (size_t y = 0; y < oceanMapSize; y++) {
        for (size_t x = 0; x < oceanMapSize; x++) {
            Samples::TerrainType* neighs[8];
            oceanBase->GetNeighbors8(x, y, neighs);

            size_t oceanNeighbors = 0;
            for (size_t n = 0; n < 8; n++) {
                if (neighs[n] != nullptr && *(neighs[n]) == Samples::TerrainType::Ocean) {
                    oceanNeighbors++;
                }
            }

            // If a cell is land and has 4 or more ocean neighbors, it becomes ocean.
            if (*(oceanBase->GetUnsafe(x, y)) == Samples::TerrainType::Grass && oceanNeighbors >= 4) {
                *(ocean->GetUnsafe(x, y)) = Samples::TerrainType::Ocean;
            }

            // If a cell is ocean and has 4 or more land neighbors, it becomes land.
            if (*(oceanBase->GetUnsafe(x, y)) == Samples::TerrainType::Ocean && oceanNeighbors <= 4) {
                *(ocean->GetUnsafe(x, y)) = Samples::TerrainType::Grass;
            }
        }
    }

    oceanBase->CopyFrom(*ocean);

    for (size_t y = 0; y < oceanMapSize; y++) {
        for (size_t x = 0; x < oceanMapSize; x++) {
            Samples::TerrainType* neighs[8];
            oceanBase->GetNeighbors8(x, y, neighs);

            size_t oceanNeighbors = 0;
            for (size_t n = 0; n < 8; n++) {
                if (neighs[n] != nullptr && *(neighs[n]) == Samples::TerrainType::Ocean) {
                    oceanNeighbors++;
                }
            }

            // If a cell is land and has 4 or more ocean neighbors, it becomes ocean.
            if (*(oceanBase->GetUnsafe(x, y)) == Samples::TerrainType::Grass && oceanNeighbors >= 4) {
                *(ocean->GetUnsafe(x, y)) = Samples::TerrainType::Ocean;
            }

            // If a cell is ocean and has 4 or more land neighbors, it becomes land.
            if (*(oceanBase->GetUnsafe(x, y)) == Samples::TerrainType::Ocean && oceanNeighbors <= 4) {
                *(ocean->GetUnsafe(x, y)) = Samples::TerrainType::Grass;
            }
        }
    }

    // Check if less than 30% ocean tiles. If so, regenerate.
    size_t oceanCount = 0;
    for (size_t i = 0; i < oceanMapSize * oceanMapSize; ++i) {
        if (*(ocean->GetUnsafe(i)) == Samples::TerrainType::Ocean) {
            oceanCount++;
        }
    }

    if (oceanCount < (oceanMapSize * oceanMapSize * 0.2f) || oceanCount > (oceanMapSize * oceanMapSize * 0.4f)) {
        goto RETRY_OCEAN_GEN;
    }

    terrain->FillFrom(*ocean, mapSize / oceanMapSize);

    // Fill height map based on fractal perlin noise.
    ME::Random rnd2{"perlin", true};
    ME::PerlinNoise noise{rnd2};
    // lacunarity=2.0, gain=0.5, octaves=6 -- typical fbm defaults (see stb_perlin.h).
    const float kFbmLacunarity = 2.0f;
    const float kFbmGain = 0.5f;
    const int kFbmOctaves = 6;
    // Octaves partially cancel each other out, so fbm's output almost never approaches its
    // analytic max amplitude -- normalizing against that max leaves values clustered near 0.
    // kFbmContrast pulls the typically-observed spread back out to fill [-1, 1]; retune by eye
    // if the level distribution still looks off.
    const float kFbmContrast = 3.5f;
    const float kFbmMaxAmplitude = ME::PerlinNoise::FbmMaxAmplitude(kFbmGain, kFbmOctaves);
    for (size_t y = 0; y < mapSize; y++) {
        for (size_t x = 0; x < mapSize; x++) {
            if (*(terrain->GetUnsafe(x, y)) == Samples::TerrainType::Grass) {  // Only for land tiles
                float noiseValue = noise.Fbm(static_cast<float>(x) * 0.1f, static_cast<float>(y) * 0.1f, 0.0f,
                                             kFbmLacunarity, kFbmGain, kFbmOctaves) /
                                   kFbmMaxAmplitude * kFbmContrast;
                noiseValue = ME::Math::Clamp(noiseValue, -1.0f, 1.0f);
                const uint8_t kHeightLevels = 16;
                uint8_t level = static_cast<uint8_t>((noiseValue + 1.0f) * 0.5f * kHeightLevels);
                if (level >= kHeightLevels) {
                    level = kHeightLevels - 1;
                }

                uint8_t terrainIndex = 0;
                switch (level) {
                    case 0:
                        terrainIndex = 7;
                        break;
                    case 1:
                        terrainIndex = 7;
                        break;
                    case 2:
                        terrainIndex = 7;
                        break;
                    case 3:
                        terrainIndex = 6;
                        break;
                    case 4:
                        terrainIndex = 6;
                        break;
                    case 5:
                        terrainIndex = 6;
                        break;
                    case 6:
                        terrainIndex = 0;
                        break;
                    case 7:
                        terrainIndex = 0;
                        break;
                    case 8:
                        terrainIndex = 0;
                        break;
                    case 9:
                        terrainIndex = 0;
                        break;
                    case 10:
                        terrainIndex = 0;
                        break;
                    case 11:
                        terrainIndex = 0;
                        break;
                    case 12:
                        terrainIndex = 1;
                        break;
                    case 13:
                        terrainIndex = 1;
                        break;
                    case 14:
                        terrainIndex = 2;
                        break;
                    case 15:
                        terrainIndex = 3;
                        break;
                    case 16:
                        terrainIndex = 3;
                        break;
                    default:
                        terrainIndex = 0;
                        break;
                }

                *(terrain->GetUnsafe(x, y)) = static_cast<Samples::TerrainType>(terrainIndex);
            }
        }
    }
}

void CreatureGen(ME::Pool<Samples::Creature>* creaturePool, size_t initialCreatureCount, ME::Grid<uint8_t>* walkableMap,
                 size_t mapSize) {
    // Create initial pool.
    creaturePool->Acquire(initialCreatureCount);

    // Single rnd stream shared across the whole batch, so creatures spawned in the same tight
    // loop don't collide on the same seed and end up with identical positions/lifetimes.
    ME::Random rnd{"creatureGen2", true};
    for (size_t i = 0; i < initialCreatureCount; ++i) {
        Samples::Creature* creature = &(*creaturePool)[i];

        // Randomly place creature on walkable tile.
        while (true) {
            size_t x = rnd.NextRange(0, mapSize - 1);
            size_t y = rnd.NextRange(0, mapSize - 1);
            if (*(walkableMap->GetUnsafe(x, y)) == 1) {
                creature->Spawn(ME::Vec2(static_cast<float>(x), static_cast<float>(y)), rnd);
                break;
            }
        }
    }
}
