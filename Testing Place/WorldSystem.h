#pragma once

#include "Core.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include <vector>
#include <unordered_map>

struct Tile {
    std::string tileID;
    glm::vec2 position;

    Tile() : tileID("grass"), position(0, 0) {}
    Tile(const std::string& id, const glm::vec2& pos) : tileID(id), position(pos) {}
};

struct Chunk {
    static constexpr int CHUNK_SIZE = 16;
    std::vector<Tile> tiles;
    int chunkX;
    bool generated;

    Chunk() : chunkX(0), generated(false) {
        tiles.resize(CHUNK_SIZE);
    }
};

class WorldSystem {
public:
    WorldSystem(int height, float tileSize = 32.0f);

    void update(const glm::vec2& cameraPosition);
    void render(Renderer& renderer, const glm::vec2& cameraPosition);

    // Tile access
    Tile* getTile(int x, int y);
    const Tile* getTile(int x, int y) const;
    void setTile(int x, int y, const std::string& tileID);

    // World queries
    bool isSolidAt(const glm::vec2& worldPos) const;
    glm::vec2 worldToTile(const glm::vec2& worldPos) const;
    glm::vec2 tileToWorld(int x, int y) const;

    // Getters
    int getHeight() const { return height; }
    float getTileSize() const { return tileSize; }

private:
    void generateChunk(int chunkX);
    Chunk* getOrCreateChunk(int chunkX);
    int getChunkIndex(int worldX) const;
    int getHeightAt(int x) const;
    bool isValidTile(int x, int y) const;

    std::unordered_map<int, Chunk> chunks;
    int height;
    float tileSize;
    int loadDistance; // Chunks to load around camera
};
