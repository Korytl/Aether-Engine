#include "WorldSystem.h"
#include <cmath>
#include <random>
#include <algorithm>

WorldSystem::WorldSystem(int height, float tileSize)
    : height(height), tileSize(tileSize), loadDistance(3) {
}

void WorldSystem::update(const glm::vec2& cameraPosition) {
    // Calculate which chunks need to be loaded
    int cameraTileX = static_cast<int>(cameraPosition.x / tileSize);
    int cameraChunkX = cameraTileX / Chunk::CHUNK_SIZE;

    // Load chunks around camera
    for (int dx = -loadDistance; dx <= loadDistance; ++dx) {
        int chunkX = cameraChunkX + dx;
        getOrCreateChunk(chunkX);
    }

    // Optional: Unload far chunks to save memory
    // (commented out for simplicity, can be added later)
}

Chunk* WorldSystem::getOrCreateChunk(int chunkX) {
    auto it = chunks.find(chunkX);
    if (it != chunks.end()) {
        return &it->second;
    }

    // Create and generate new chunk
    Chunk& chunk = chunks[chunkX];
    chunk.chunkX = chunkX;
    generateChunk(chunkX);

    return &chunk;
}

int WorldSystem::getChunkIndex(int worldX) const {
    return worldX / Chunk::CHUNK_SIZE;
}

int WorldSystem::getHeightAt(int x) const {
    // Smooth noise-based height generation
    float noise = std::sin(x * 0.08f) * 3.0f +
        std::cos(x * 0.04f) * 2.0f +
        std::sin(x * 0.15f) * 1.5f;

    int baseHeight = height / 3;
    int terrainHeight = baseHeight + static_cast<int>(noise);

    return std::max(3, std::min(height - 5, terrainHeight));
}

void WorldSystem::generateChunk(int chunkX) {
    Chunk* chunk = &chunks[chunkX];
    if (chunk->generated) return;

    chunk->tiles.clear();
    chunk->tiles.resize(Chunk::CHUNK_SIZE * height);

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> decorDis(0.0f, 1.0f);

    for (int localX = 0; localX < Chunk::CHUNK_SIZE; ++localX) {
        int worldX = chunkX * Chunk::CHUNK_SIZE + localX;
        int groundLevel = getHeightAt(worldX);

        for (int y = 0; y < height; ++y) {
            Tile& tile = chunk->tiles[y * Chunk::CHUNK_SIZE + localX];
            tile.position = tileToWorld(worldX, y);

            if (y < groundLevel - 3) {
                tile.tileID = "stone";
            }
            else if (y < groundLevel - 1) {
                tile.tileID = "dirt";
            }
            else if (y == groundLevel - 1) {
                tile.tileID = "grass";
            }
            else if (y == groundLevel && decorDis(gen) < 0.1f) {
                tile.tileID = "water";
            }
            else {
                tile.tileID = "grass"; // Air
            }
        }

        // Add occasional platforms
        if (worldX % 20 == 10 && decorDis(gen) < 0.5f) {
            int platformY = groundLevel + 3;
            if (platformY < height - 1) {
                for (int px = 0; px < 5 && localX + px < Chunk::CHUNK_SIZE; ++px) {
                    chunk->tiles[platformY * Chunk::CHUNK_SIZE + localX + px].tileID = "stone";
                }
            }
        }
    }

    chunk->generated = true;
}

void WorldSystem::render(Renderer& renderer, const glm::vec2& cameraPosition) {
    auto& resourceMgr = ResourceManager::getInstance();

    // Only render visible chunks
    int cameraTileX = static_cast<int>(cameraPosition.x / tileSize);
    int cameraChunkX = cameraTileX / Chunk::CHUNK_SIZE;

    for (int dx = -loadDistance; dx <= loadDistance; ++dx) {
        int chunkX = cameraChunkX + dx;
        auto it = chunks.find(chunkX);
        if (it == chunks.end()) continue;

        Chunk& chunk = it->second;

        for (int localX = 0; localX < Chunk::CHUNK_SIZE; ++localX) {
            int worldX = chunkX * Chunk::CHUNK_SIZE + localX;

            for (int y = 0; y < height; ++y) {
                const Tile& tile = chunk.tiles[y * Chunk::CHUNK_SIZE + localX];
                const TileDefinition* tileDef = resourceMgr.getTileDefinition(tile.tileID);

                if (!tileDef) continue;

                if (tileDef->solid || tile.tileID == "water" || tile.tileID == "sand") {
                    renderer.drawRect(tile.position,
                        glm::vec2(tileSize, tileSize),
                        tileDef->color);
                }
            }
        }
    }
}

Tile* WorldSystem::getTile(int x, int y) {
    if (!isValidTile(x, y)) return nullptr;

    int chunkX = x / Chunk::CHUNK_SIZE;
    int localX = x % Chunk::CHUNK_SIZE;
    if (localX < 0) localX += Chunk::CHUNK_SIZE;

    auto it = chunks.find(chunkX);
    if (it == chunks.end()) {
        getOrCreateChunk(chunkX);
        it = chunks.find(chunkX);
    }

    if (it == chunks.end()) return nullptr;

    return &it->second.tiles[y * Chunk::CHUNK_SIZE + localX];
}

const Tile* WorldSystem::getTile(int x, int y) const {
    if (!isValidTile(x, y)) return nullptr;

    int chunkX = x / Chunk::CHUNK_SIZE;
    int localX = x % Chunk::CHUNK_SIZE;
    if (localX < 0) localX += Chunk::CHUNK_SIZE;

    auto it = chunks.find(chunkX);
    if (it == chunks.end()) return nullptr;

    return &it->second.tiles[y * Chunk::CHUNK_SIZE + localX];
}

void WorldSystem::setTile(int x, int y, const std::string& tileID) {
    Tile* tile = getTile(x, y);
    if (tile) {
        tile->tileID = tileID;
    }
}

bool WorldSystem::isSolidAt(const glm::vec2& worldPos) const {
    glm::vec2 tilePos = worldToTile(worldPos);
    const Tile* tile = getTile(static_cast<int>(tilePos.x), static_cast<int>(tilePos.y));

    if (!tile) return false;

    auto& resourceMgr = ResourceManager::getInstance();
    const TileDefinition* tileDef = resourceMgr.getTileDefinition(tile->tileID);

    return tileDef ? tileDef->solid : false;
}

glm::vec2 WorldSystem::worldToTile(const glm::vec2& worldPos) const {
    return glm::vec2(
        std::floor(worldPos.x / tileSize),
        std::floor(worldPos.y / tileSize)
    );
}

glm::vec2 WorldSystem::tileToWorld(int x, int y) const {
    return glm::vec2(
        x * tileSize + tileSize * 0.5f,
        y * tileSize + tileSize * 0.5f
    );
}

bool WorldSystem::isValidTile(int x, int y) const {
    return y >= 0 && y < height; // X is infinite
}