#pragma once

#include "Core.h"
#include "EntityRegistry.h"
#include <unordered_map>
#include <unordered_set>

// Forward declaration
class WorldSystem;

struct CollisionInfo {
    EntityID entityA;
    EntityID entityB;
    glm::vec2 normal;
    float penetration;
};

// Spatial partitioning grid
class SpatialGrid {
public:
    SpatialGrid(float cellSize = 64.0f) : cellSize(cellSize) {}

    void clear();
    void insert(EntityID entity, const Rect& bounds);
    std::vector<EntityID> query(const Rect& bounds) const;

private:
    struct Cell {
        int x, y;
        bool operator==(const Cell& other) const {
            return x == other.x && y == other.y;
        }
    };

    struct CellHash {
        size_t operator()(const Cell& cell) const {
            return std::hash<int>()(cell.x) ^ (std::hash<int>()(cell.y) << 1);
        }
    };

    Cell getCell(const glm::vec2& position) const;

    float cellSize;
    std::unordered_map<Cell, std::vector<EntityID>, CellHash> grid;
};

class WorldSystem; // Forward declaration

class PhysicsSystem {
public:
    PhysicsSystem(EntityRegistry* registry) : registry(registry), spatialGrid(64.0f), world(nullptr) {}

    void setWorld(WorldSystem* worldSystem) { world = worldSystem; }

    void update(float deltaTime);

    // Collision queries
    bool raycast(const glm::vec2& start, const glm::vec2& end, EntityID& hitEntity, glm::vec2& hitPoint);
    std::vector<EntityID> overlapRect(const Rect& bounds);

    // Collision callbacks
    using CollisionCallback = std::function<void(const CollisionInfo&)>;
    void setCollisionCallback(CollisionCallback callback) { collisionCallback = callback; }

    // Debug
    void setDebugDraw(bool enable) { debugDraw = enable; }
    const std::vector<CollisionInfo>& getLastCollisions() const { return lastCollisions; }

private:
    void updatePhysics(float deltaTime);
    void updateSpatialGrid();
    void detectCollisions();
    void resolveCollision(EntityID a, EntityID b, const glm::vec2& normal, float penetration);
    void checkTileCollisions();

    bool checkAABB(const Rect& a, const Rect& b, glm::vec2& normal, float& penetration);

    EntityRegistry* registry;
    WorldSystem* world;
    SpatialGrid spatialGrid;
    std::vector<CollisionInfo> lastCollisions;
    CollisionCallback collisionCallback;
    bool debugDraw = false;
};
