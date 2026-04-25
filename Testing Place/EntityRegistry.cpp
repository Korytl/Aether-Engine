#include "EntityRegistry.h"

EntityID EntityRegistry::createEntity() {
    EntityID id = nextEntityID++;
    entities.push_back(id);
    return id;
}

void EntityRegistry::destroyEntity(EntityID entity) {
    if (!isValid(entity)) return;

    // Remove from entity list
    auto it = std::find(entities.begin(), entities.end(), entity);
    if (it != entities.end()) {
        entities.erase(it);
    }

    // Remove all components
    for (auto& [typeID, pool] : componentPools) {
        pool.erase(entity);
    }
}

bool EntityRegistry::isValid(EntityID entity) const {
    if (entity == INVALID_ENTITY) return false;
    return std::find(entities.begin(), entities.end(), entity) != entities.end();
}

// Component.cpp implementations
bool InventoryComponent::addItem(const std::string& itemID, int quantity) {
    if (itemID.empty() || quantity <= 0) return false;

    // Try to stack with existing items first
    for (auto& slot : slots) {
        if (slot.itemID == itemID && !slot.isEmpty()) {
            slot.quantity += quantity;
            return true;
        }
    }

    // Find empty slot
    for (auto& slot : slots) {
        if (slot.isEmpty()) {
            slot.itemID = itemID;
            slot.quantity = quantity;
            return true;
        }
    }

    return false; // Inventory full
}

bool InventoryComponent::removeItem(const std::string& itemID, int quantity) {
    if (itemID.empty() || quantity <= 0) return false;

    for (auto& slot : slots) {
        if (slot.itemID == itemID && !slot.isEmpty()) {
            if (slot.quantity >= quantity) {
                slot.quantity -= quantity;
                if (slot.quantity <= 0) {
                    slot.itemID.clear();
                    slot.quantity = 0;
                }
                return true;
            }
        }
    }

    return false;
}

int InventoryComponent::getItemCount(const std::string& itemID) const {
    int total = 0;
    for (const auto& slot : slots) {
        if (slot.itemID == itemID) {
            total += slot.quantity;
        }
    }
    return total;
}

bool InventoryComponent::equipItem(const std::string& itemID) {
    if (itemID.empty()) return false;

    // For simplicity, assume weapons end with "sword", armor with "armor"
    if (itemID.find("sword") != std::string::npos) {
        equippedWeapon = itemID;
        return true;
    }
    else if (itemID.find("armor") != std::string::npos) {
        equippedArmor = itemID;
        return true;
    }

    return false;
}