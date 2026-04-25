#pragma once

#include "Core.h"
#include "Components.h"
#include <any>
#include <typeindex>

class EntityRegistry {
public:
    EntityRegistry() : nextEntityID(1) {}

    // Entity management
    EntityID createEntity();
    void destroyEntity(EntityID entity);
    bool isValid(EntityID entity) const;

    // Component management
    template<typename T>
    T* addComponent(EntityID entity, const T& component = T()) {
        if (!isValid(entity)) return nullptr;

        auto typeID = std::type_index(typeid(T));
        componentPools[typeID][entity] = component;

        return getComponent<T>(entity);
    }

    template<typename T>
    T* getComponent(EntityID entity) {
        if (!isValid(entity)) return nullptr;

        auto typeID = std::type_index(typeid(T));
        auto poolIt = componentPools.find(typeID);
        if (poolIt == componentPools.end()) return nullptr;

        auto& pool = poolIt->second;
        auto it = pool.find(entity);
        if (it == pool.end()) return nullptr;

        try {
            return &std::any_cast<T&>(it->second);
        }
        catch (...) {
            return nullptr;
        }
    }

    template<typename T>
    bool hasComponent(EntityID entity) const {
        if (!isValid(entity)) return false;

        auto typeID = std::type_index(typeid(T));
        auto poolIt = componentPools.find(typeID);
        if (poolIt == componentPools.end()) return false;

        return poolIt->second.find(entity) != poolIt->second.end();
    }

    template<typename T>
    void removeComponent(EntityID entity) {
        if (!isValid(entity)) return;

        auto typeID = std::type_index(typeid(T));
        auto poolIt = componentPools.find(typeID);
        if (poolIt != componentPools.end()) {
            poolIt->second.erase(entity);
        }
    }

    // Get all entities with specific component
    template<typename T>
    std::vector<EntityID> getEntitiesWith() const {
        std::vector<EntityID> result;
        auto typeID = std::type_index(typeid(T));
        auto poolIt = componentPools.find(typeID);

        if (poolIt != componentPools.end()) {
            for (const auto& [entity, _] : poolIt->second) {
                if (isValid(entity)) {
                    result.push_back(entity);
                }
            }
        }

        return result;
    }

    // Get all entities with multiple components
    template<typename T1, typename T2, typename... Rest>
    std::vector<EntityID> getEntitiesWith() const {
        auto entities = getEntitiesWith<T1>();
        std::vector<EntityID> result;

        for (EntityID entity : entities) {
            if (hasComponent<T2>(entity) && (hasComponent<Rest>(entity) && ...)) {
                result.push_back(entity);
            }
        }

        return result;
    }

    // Get all valid entities
    const std::vector<EntityID>& getAllEntities() const { return entities; }

    // Clear all
    void clear() {
        entities.clear();
        componentPools.clear();
        nextEntityID = 1;
    }

private:
    EntityID nextEntityID;
    std::vector<EntityID> entities;
    std::unordered_map<std::type_index, std::unordered_map<EntityID, std::any>> componentPools;
};

