#include "PhysicsSystem.h"
#include "WorldSystem.h"
#include <algorithm>

// SpatialGrid Implementation
void SpatialGrid::clear() {
    grid.clear();
}

void SpatialGrid::insert(EntityID entity, const Rect& bounds) {
    Cell minCell = getCell({ bounds.left(), bounds.bottom() });
    Cell maxCell = getCell({ bounds.right(), bounds.top() });

    for (int y = minCell.y; y <= maxCell.y; ++y) {
        for (int x = minCell.x; x <= maxCell.x; ++x) {
            grid[{x, y}].push_back(entity);
        }
    }
}

std::vector<EntityID> SpatialGrid::query(const Rect& bounds) const {
    std::unordered_set<EntityID> uniqueEntities;

    Cell minCell = getCell({ bounds.left(), bounds.bottom() });
    Cell maxCell = getCell({ bounds.right(), bounds.top() });

    for (int y = minCell.y; y <= maxCell.y; ++y) {
        for (int x = minCell.x; x <= maxCell.x; ++x) {
            auto it = grid.find({ x, y });
            if (it != grid.end()) {
                for (EntityID entity : it->second) {
                    uniqueEntities.insert(entity);
                }
            }
        }
    }

    return std::vector<EntityID>(uniqueEntities.begin(), uniqueEntities.end());
}

SpatialGrid::Cell SpatialGrid::getCell(const glm::vec2& position) const {
    return {
        static_cast<int>(floor(position.x / cellSize)),
        static_cast<int>(floor(position.y / cellSize))
    };
}

// PhysicsSystem Implementation
void PhysicsSystem::update(float deltaTime) {
    updatePhysics(deltaTime);
    checkTileCollisions(); // Check tile collisions BEFORE entity collisions
    updateSpatialGrid();
    detectCollisions();
}

void PhysicsSystem::updatePhysics(float deltaTime) {
    auto entities = registry->getEntitiesWith<TransformComponent, PhysicsComponent>();

    for (EntityID entity : entities) {
        auto* transform = registry->getComponent<TransformComponent>(entity);
        auto* physics = registry->getComponent<PhysicsComponent>(entity);

        if (!transform || !physics) continue;

        // Apply gravity
        if (physics->applyGravity) {
            physics->velocity.y += physics->gravity * deltaTime;
        }

        // Apply acceleration
        physics->velocity += physics->acceleration * deltaTime;

        // Apply friction (only to horizontal movement)
        physics->velocity.x *= physics->friction;

        // Stop very small velocities (prevent sliding)
        if (fabs(physics->velocity.x) < 1.0f) {
            physics->velocity.x = 0;
        }

        // Clamp to max speed
        float speed = glm::length(physics->velocity);
        if (speed > physics->maxSpeed) {
            physics->velocity = glm::normalize(physics->velocity) * physics->maxSpeed;
        }

        // Update position
        transform->position += physics->velocity * deltaTime;

        // Reset acceleration (it's applied each frame by input)
        physics->acceleration = glm::vec2(0.0f);
    }
}

void PhysicsSystem::updateSpatialGrid() {
    spatialGrid.clear();

    auto entities = registry->getEntitiesWith<TransformComponent, ColliderComponent>();

    for (EntityID entity : entities) {
        auto* transform = registry->getComponent<TransformComponent>(entity);
        auto* collider = registry->getComponent<ColliderComponent>(entity);

        if (!transform || !collider) continue;

        Rect bounds = collider->getBounds(transform->position);
        spatialGrid.insert(entity, bounds);
    }
}

void PhysicsSystem::detectCollisions() {
    lastCollisions.clear();

    auto entities = registry->getEntitiesWith<TransformComponent, ColliderComponent>();
    std::unordered_set<uint64_t> checkedPairs;

    for (EntityID entityA : entities) {
        auto* transformA = registry->getComponent<TransformComponent>(entityA);
        auto* colliderA = registry->getComponent<ColliderComponent>(entityA);

        if (!transformA || !colliderA) continue;

        Rect boundsA = colliderA->getBounds(transformA->position);
        auto nearby = spatialGrid.query(boundsA);

        for (EntityID entityB : nearby) {
            if (entityA >= entityB) continue;

            // Create unique pair ID
            uint64_t pairID = (static_cast<uint64_t>(std::min(entityA, entityB)) << 32) |
                static_cast<uint64_t>(std::max(entityA, entityB));

            if (checkedPairs.count(pairID)) continue;
            checkedPairs.insert(pairID);

            auto* transformB = registry->getComponent<TransformComponent>(entityB);
            auto* colliderB = registry->getComponent<ColliderComponent>(entityB);

            if (!transformB || !colliderB) continue;

            // Check collision layers
            if (!(colliderA->collisionMask & colliderB->collisionLayer) ||
                !(colliderB->collisionMask & colliderA->collisionLayer)) {
                continue;
            }

            Rect boundsB = colliderB->getBounds(transformB->position);

            glm::vec2 normal;
            float penetration;

            if (checkAABB(boundsA, boundsB, normal, penetration)) {
                CollisionInfo info{ entityA, entityB, normal, penetration };
                lastCollisions.push_back(info);

                if (collisionCallback) {
                    collisionCallback(info);
                }

                // Resolve collision if not triggers
                if (!colliderA->isTrigger && !colliderB->isTrigger) {
                    resolveCollision(entityA, entityB, normal, penetration);
                }
            }
        }
    }
}

bool PhysicsSystem::checkAABB(const Rect& a, const Rect& b, glm::vec2& normal, float& penetration) {
    if (!a.intersects(b)) return false;

    glm::vec2 centerA = a.center();
    glm::vec2 centerB = b.center();
    glm::vec2 delta = centerB - centerA;

    float overlapX = (a.width + b.width) * 0.5f - fabs(delta.x);
    float overlapY = (a.height + b.height) * 0.5f - fabs(delta.y);

    if (overlapX < overlapY) {
        normal = glm::vec2(delta.x > 0 ? 1.0f : -1.0f, 0.0f);
        penetration = overlapX;
    }
    else {
        normal = glm::vec2(0.0f, delta.y > 0 ? 1.0f : -1.0f);
        penetration = overlapY;
    }

    return true;
}

void PhysicsSystem::resolveCollision(EntityID a, EntityID b, const glm::vec2& normal, float penetration) {
    auto* transformA = registry->getComponent<TransformComponent>(a);
    auto* transformB = registry->getComponent<TransformComponent>(b);
    auto* colliderA = registry->getComponent<ColliderComponent>(a);
    auto* colliderB = registry->getComponent<ColliderComponent>(b);

    if (!transformA || !transformB || !colliderA || !colliderB) return;

    // Separate objects
    if (colliderA->isStatic && colliderB->isStatic) {
        return; // Both static, no resolution
    }
    else if (colliderA->isStatic) {
        transformB->position += normal * penetration;
    }
    else if (colliderB->isStatic) {
        transformA->position -= normal * penetration;
    }
    else {
        transformA->position -= normal * (penetration * 0.5f);
        transformB->position += normal * (penetration * 0.5f);
    }

    // Adjust velocities if physics components exist
    auto* physicsA = registry->getComponent<PhysicsComponent>(a);
    auto* physicsB = registry->getComponent<PhysicsComponent>(b);

    if (physicsA && !colliderA->isStatic) {
        float velocityAlongNormal = glm::dot(physicsA->velocity, normal);
        if (velocityAlongNormal < 0) {
            physicsA->velocity -= normal * velocityAlongNormal;
        }
    }

    if (physicsB && !colliderB->isStatic) {
        float velocityAlongNormal = glm::dot(physicsB->velocity, normal);
        if (velocityAlongNormal > 0) {
            physicsB->velocity -= normal * velocityAlongNormal;
        }
    }
}

bool PhysicsSystem::raycast(const glm::vec2& start, const glm::vec2& end, EntityID& hitEntity, glm::vec2& hitPoint) {
    // Simple raycast implementation
    auto entities = registry->getEntitiesWith<TransformComponent, ColliderComponent>();

    float closestDist = FLT_MAX;
    bool hit = false;

    for (EntityID entity : entities) {
        auto* transform = registry->getComponent<TransformComponent>(entity);
        auto* collider = registry->getComponent<ColliderComponent>(entity);

        if (!transform || !collider) continue;

        Rect bounds = collider->getBounds(transform->position);

        // Ray-AABB intersection (simplified)
        glm::vec2 dir = end - start;
        float length = glm::length(dir);
        if (length == 0) continue;

        dir /= length;

        // Check if ray intersects bounds (simple implementation)
        if (bounds.contains(start) || bounds.contains(end)) {
            float dist = glm::length(transform->position - start);
            if (dist < closestDist) {
                closestDist = dist;
                hitEntity = entity;
                hitPoint = transform->position;
                hit = true;
            }
        }
    }

    return hit;
}

std::vector<EntityID> PhysicsSystem::overlapRect(const Rect& bounds) {
    return spatialGrid.query(bounds);
}

void PhysicsSystem::checkTileCollisions() {
    if (!world) return;

    auto entities = registry->getEntitiesWith<TransformComponent, ColliderComponent, PhysicsComponent>();

    for (EntityID entity : entities) {
        auto* transform = registry->getComponent<TransformComponent>(entity);
        auto* collider = registry->getComponent<ColliderComponent>(entity);
        auto* physics = registry->getComponent<PhysicsComponent>(entity);

        if (!transform || !collider || !physics || collider->isStatic) continue;

        Rect bounds = collider->getBounds(transform->position);
        float tileSize = world->getTileSize();

        // Check tiles around entity with some padding
        int minTileX = static_cast<int>((bounds.left() - tileSize) / tileSize);
        int maxTileX = static_cast<int>((bounds.right() + tileSize) / tileSize);
        int minTileY = static_cast<int>((bounds.bottom() - tileSize) / tileSize);
        int maxTileY = static_cast<int>((bounds.top() + tileSize) / tileSize);

        std::vector<glm::vec2> corrections;

        for (int ty = minTileY; ty <= maxTileY; ++ty) {
            for (int tx = minTileX; tx <= maxTileX; ++tx) {
                glm::vec2 tileWorldPos = world->tileToWorld(tx, ty);
                if (!world->isSolidAt(tileWorldPos)) continue;

                // Create AABB for tile
                Rect tileRect(
                    tx * tileSize,
                    ty * tileSize,
                    tileSize,
                    tileSize
                );

                glm::vec2 normal;
                float penetration;

                if (checkAABB(bounds, tileRect, normal, penetration)) {
                    // Only apply correction if penetration is significant
                    if (penetration > 0.1f) {
                        corrections.push_back(normal * penetration);

                        // Adjust velocity based on collision normal
                        if (normal.y > 0.5f) {
                            // Hit ground from above - STOP downward velocity
                            if (physics->velocity.y < 0) {
                                physics->velocity.y = 0;
                            }
                        }
                        else if (normal.y < -0.5f) {
                            // Hit ceiling - STOP upward velocity
                            if (physics->velocity.y > 0) {
                                physics->velocity.y = 0;
                            }
                        }

                        if (fabs(normal.x) > 0.5f) {
                            // Hit wall
                            if ((normal.x > 0 && physics->velocity.x < 0) ||
                                (normal.x < 0 && physics->velocity.x > 0)) {
                                physics->velocity.x *= 0.5f;
                            }
                        }
                    }
                }
            }
        }

        // Apply all corrections
        for (const auto& correction : corrections) {
            transform->position -= correction;
            bounds = collider->getBounds(transform->position);
        }
    }
}