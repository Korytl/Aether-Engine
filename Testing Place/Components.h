#pragma once

#include "Core.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Transform Component
struct TransformComponent {
    glm::vec2 position{ 0.0f, 0.0f };
    float rotation{ 0.0f }; // radians
    glm::vec2 scale{ 1.0f, 1.0f };

    json toJson() const {
        return {
            {"position", {position.x, position.y}},
            {"rotation", rotation},
            {"scale", {scale.x, scale.y}}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("position")) {
            position = glm::vec2(j["position"][0], j["position"][1]);
        }
        if (j.contains("rotation")) rotation = j["rotation"];
        if (j.contains("scale")) {
            scale = glm::vec2(j["scale"][0], j["scale"][1]);
        }
    }
};

// Renderable Component
enum class ShapeType {
    Rectangle,
    Circle,
    Line
};

enum class RenderMode {
    Primitive,  // Use geometric shapes
    Sprite      // Use texture
};

struct RenderableComponent {
    Color color{ Color::White() };
    ShapeType shapeType{ ShapeType::Rectangle };
    int layer{ 0 };
    glm::vec2 size{ 32.0f, 32.0f };
    bool visible{ true };

    // Sprite/Texture rendering
    RenderMode renderMode{ RenderMode::Primitive };
    std::string textureID;
    glm::vec4 spriteRect{ 0, 0, 1, 1 }; // UV coordinates (x, y, width, height) normalized 0-1
    bool flipX{ false };
    bool flipY{ false };

    json toJson() const {
        return {
            {"color", {color.r, color.g, color.b, color.a}},
            {"shapeType", static_cast<int>(shapeType)},
            {"layer", layer},
            {"size", {size.x, size.y}},
            {"visible", visible},
            {"renderMode", static_cast<int>(renderMode)},
            {"textureID", textureID},
            {"spriteRect", {spriteRect.x, spriteRect.y, spriteRect.z, spriteRect.w}},
            {"flipX", flipX},
            {"flipY", flipY}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("color")) {
            auto c = j["color"];
            color = Color(c[0], c[1], c[2], c[3]);
        }
        if (j.contains("shapeType")) shapeType = static_cast<ShapeType>(j["shapeType"].get<int>());
        if (j.contains("layer")) layer = j["layer"];
        if (j.contains("size")) size = glm::vec2(j["size"][0], j["size"][1]);
        if (j.contains("visible")) visible = j["visible"];
        if (j.contains("renderMode")) renderMode = static_cast<RenderMode>(j["renderMode"].get<int>());
        if (j.contains("textureID")) textureID = j["textureID"];
        if (j.contains("spriteRect")) {
            auto r = j["spriteRect"];
            spriteRect = glm::vec4(r[0], r[1], r[2], r[3]);
        }
        if (j.contains("flipX")) flipX = j["flipX"];
        if (j.contains("flipY")) flipY = j["flipY"];
    }
};

// Animation Component
struct AnimationComponent {
    std::string currentAnimation;
    int currentFrame{ 0 };
    float frameTime{ 0.0f };
    float frameDuration{ 0.1f }; // Time per frame
    bool loop{ true };
    bool playing{ true };

    std::unordered_map<std::string, std::vector<glm::vec4>> animations; // animation name -> frame UVs

    void play(const std::string& animName) {
        if (currentAnimation != animName) {
            currentAnimation = animName;
            currentFrame = 0;
            frameTime = 0.0f;
            playing = true;
        }
    }

    void update(float deltaTime, RenderableComponent* renderable) {
        if (!playing || animations.find(currentAnimation) == animations.end()) return;

        frameTime += deltaTime;
        if (frameTime >= frameDuration) {
            frameTime = 0.0f;
            currentFrame++;

            const auto& frames = animations[currentAnimation];
            if (currentFrame >= static_cast<int>(frames.size())) {
                if (loop) {
                    currentFrame = 0;
                }
                else {
                    currentFrame = frames.size() - 1;
                    playing = false;
                }
            }

            // Update renderable sprite rect
            if (renderable && currentFrame < static_cast<int>(frames.size())) {
                renderable->spriteRect = frames[currentFrame];
            }
        }
    }

    json toJson() const {
        json j;
        j["currentAnimation"] = currentAnimation;
        j["frameDuration"] = frameDuration;
        j["loop"] = loop;
        return j;
    }

    void fromJson(const json& j) {
        if (j.contains("currentAnimation")) currentAnimation = j["currentAnimation"];
        if (j.contains("frameDuration")) frameDuration = j["frameDuration"];
        if (j.contains("loop")) loop = j["loop"];
    }
};

// Collider Component
struct ColliderComponent {
    glm::vec2 offset{ 0.0f, 0.0f };
    glm::vec2 size{ 32.0f, 32.0f };
    uint32_t collisionLayer{ 1 }; // Bitmask
    uint32_t collisionMask{ 0xFFFFFFFF }; // What layers can this collide with
    bool isTrigger{ false };
    bool isStatic{ false };

    Rect getBounds(const glm::vec2& position) const {
        return Rect(position.x + offset.x - size.x * 0.5f,
            position.y + offset.y - size.y * 0.5f,
            size.x, size.y);
    }

    json toJson() const {
        return {
            {"offset", {offset.x, offset.y}},
            {"size", {size.x, size.y}},
            {"collisionLayer", collisionLayer},
            {"collisionMask", collisionMask},
            {"isTrigger", isTrigger},
            {"isStatic", isStatic}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("offset")) offset = glm::vec2(j["offset"][0], j["offset"][1]);
        if (j.contains("size")) size = glm::vec2(j["size"][0], j["size"][1]);
        if (j.contains("collisionLayer")) collisionLayer = j["collisionLayer"];
        if (j.contains("collisionMask")) collisionMask = j["collisionMask"];
        if (j.contains("isTrigger")) isTrigger = j["isTrigger"];
        if (j.contains("isStatic")) isStatic = j["isStatic"];
    }
};

// Physics Component
struct PhysicsComponent {
    glm::vec2 velocity{ 0.0f, 0.0f };
    glm::vec2 acceleration{ 0.0f, 0.0f };
    float friction{ 0.9f };
    float gravity{ -980.0f };
    bool applyGravity{ true };
    float maxSpeed{ 500.0f };

    json toJson() const {
        return {
            {"velocity", {velocity.x, velocity.y}},
            {"acceleration", {acceleration.x, acceleration.y}},
            {"friction", friction},
            {"gravity", gravity},
            {"applyGravity", applyGravity},
            {"maxSpeed", maxSpeed}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("velocity")) velocity = glm::vec2(j["velocity"][0], j["velocity"][1]);
        if (j.contains("acceleration")) acceleration = glm::vec2(j["acceleration"][0], j["acceleration"][1]);
        if (j.contains("friction")) friction = j["friction"];
        if (j.contains("gravity")) gravity = j["gravity"];
        if (j.contains("applyGravity")) applyGravity = j["applyGravity"];
        if (j.contains("maxSpeed")) maxSpeed = j["maxSpeed"];
    }
};

// Health Component
struct HealthComponent {
    float currentHP{ 100.0f };
    float maxHP{ 100.0f };
    float hpRegen{ 5.0f };
    float invincibilityTime{ 0.0f };
    float invincibilityDuration{ 0.5f };
    bool isDead{ false };

    void damage(float amount) {
        if (invincibilityTime <= 0.0f && !isDead) {
            currentHP -= amount;
            if (currentHP <= 0) {
                currentHP = 0;
                isDead = true;
            }
            invincibilityTime = invincibilityDuration;
        }
    }

    void updateFromStats(int healthPoints)
    {
        // Base: 100 hp, +10 per VIT
        float newMaxHp = 100.0f + (healthPoints * 10.0f);

        // Base: 5 regen, +0.5 per VIT
        float newHpRegen = 5.0f + (healthPoints * 0.5f);

        // If max hp increased, scale current mana proportionally
        if (newMaxHp > maxHP && maxHP > 0) {
            float ratio = currentHP / maxHP;
            maxHP = newMaxHp;
            currentHP = maxHP * ratio;
        }
        else {
            maxHP = newMaxHp;
        }

        hpRegen = newHpRegen;
    }

    void heal(float amount) {
        if (!isDead) {
            currentHP = glm::min(currentHP + amount, maxHP);
        }
    }

    json toJson() const {
        return {
            {"currentHP", currentHP},
            {"maxHP", maxHP},
            {"invincibilityDuration", invincibilityDuration},
            {"isDead", isDead}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("currentHP")) currentHP = j["currentHP"];
        if (j.contains("maxHP")) maxHP = j["maxHP"];
        if (j.contains("invincibilityDuration")) invincibilityDuration = j["invincibilityDuration"];
        if (j.contains("isDead")) isDead = j["isDead"];
    }
};

// Item definition
struct Item {
    std::string id;
    std::string name;
    std::string description;
    std::string type;  // "weapon", "armor", "accessory", "consumable", "material", "currency"
    int maxStack{ 1 };
    float rarity{ 1.0 };
    bool consumable{ false };

    // UI sprite
    std::string iconTextureID;
    std::string iconTexturePath;
    glm::vec4 iconSpriteRect{ 0, 0, 1, 1 }; // UV coordinates for icon

    // Equipment stat bonuses
    int bonusStrength{ 0 };
    int bonusVitality{ 0 };
    int bonusDexterity{ 0 };
    int bonusIntelligence{ 0 };
    int bonusLuck{ 0 };

    float bonusPhysicalDamage{ 0.0f };
    float bonusMagicDamage{ 0.0f };
    float bonusHealth{ 0.0f };
    float bonusDefence{ 0.0f };
    float bonusCritChance{ 0.0f };
    float bonusCritDmg{ 0.0f };
    float bonusDropChance{ 0.0f };
    float bonusDropBoost{ 0.0f };

    json toJson() const {
        return {
            {"id", id},
            {"name", name},
            {"description", description},
            {"type", type},
            {"maxStack", maxStack},
            {"rarity", rarity},
            {"consumable", consumable},
            {"iconTextureID", iconTextureID},
            {"iconTexturePath", iconTexturePath},
            {"iconSpriteRect", {iconSpriteRect.x, iconSpriteRect.y, iconSpriteRect.z, iconSpriteRect.w}},
            {"bonusStrength", bonusStrength},
            {"bonusVitality", bonusVitality},
            {"bonusDexterity", bonusDexterity},
            {"bonusIntelligence", bonusIntelligence},
            {"bonusLuck", bonusLuck},
            {"bonusPhysicalDamage", bonusPhysicalDamage},
            {"bonusMagicDamage", bonusMagicDamage},
            {"bonusHealth", bonusHealth},
            {"bonusDefence", bonusDefence},
            {"bonusCritChance", bonusCritChance},
            {"bonusCritDmg", bonusCritDmg},
            {"bonusDropChance", bonusDropChance},
            {"bonusDropBoost", bonusDropBoost}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("id")) id = j["id"];
        if (j.contains("name")) name = j["name"];
        if (j.contains("description")) description = j["description"];
        if (j.contains("type")) type = j["type"];
        if (j.contains("maxStack")) maxStack = j["maxStack"];
        if (j.contains("rarity")) rarity = j["rarity"];
        if (j.contains("consumable")) consumable = j["consumable"];
        if (j.contains("iconTextureID")) iconTextureID = j["iconTextureID"];
        if (j.contains("iconTexturePath")) iconTexturePath = j["iconTexturePath"];
        if (j.contains("iconSpriteRect")) {
            auto r = j["iconSpriteRect"];
            iconSpriteRect = glm::vec4(r[0], r[1], r[2], r[3]);
        }
        if (j.contains("bonusStrength")) bonusStrength = j["bonusStrength"];
        if (j.contains("bonusVitality")) bonusVitality = j["bonusVitality"];
        if (j.contains("bonusDexterity")) bonusDexterity = j["bonusDexterity"];
        if (j.contains("bonusIntelligence")) bonusIntelligence = j["bonusIntelligence"];
        if (j.contains("bonusLuck")) bonusLuck = j["bonusLuck"];
        if (j.contains("bonusPhysicalDamage")) bonusPhysicalDamage = j["bonusPhysicalDamage"];
        if (j.contains("bonusMagicDamage")) bonusMagicDamage = j["bonusMagicDamage"];
        if (j.contains("bonusHealth")) bonusHealth = j["bonusHealth"];
        if (j.contains("bonusDefence")) bonusDefence = j["bonusDefence"];
        if (j.contains("bonusCritChance")) bonusCritChance = j["bonusCritChance"];
        if (j.contains("bonusCritDmg")) bonusCritDmg = j["bonusCritDmg"];
        if (j.contains("bonusDropChance")) bonusDropChance = j["bonusDropChance"];
        if (j.contains("bonusDropBoost")) bonusDropBoost = j["bonusDropBoost"];
    }
};

// Inventory slot
struct InventorySlot {
    std::string itemID;
    int quantity{ 0 };

    bool isEmpty() const { return itemID.empty() || quantity <= 0; }

    json toJson() const {
        return {
            {"itemID", itemID},
            {"quantity", quantity}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("itemID")) itemID = j["itemID"];
        if (j.contains("quantity")) quantity = j["quantity"];
    }
};

// Inventory Component
struct InventoryComponent {
    std::vector<InventorySlot> slots;
    int capacity{ 40 };

    // Equipment slots
    std::string equippedWeapon;
    std::string equippedArmor;
    std::string equippedAccessory1;
    std::string equippedAccessory2;

    //Tab state
    int currentTab = 0; //0=items, 1=Equipment, 2=Skills, 3=Magic

    InventoryComponent() {
        slots.resize(capacity);
    }

    bool addItem(const std::string& itemID, int quantity = 1);
    bool removeItem(const std::string& itemID, int quantity = 1);
    int getItemCount(const std::string& itemID) const;
    bool equipItem(const std::string& itemID);

    json toJson() const {
        json j;
        j["capacity"] = capacity;
        j["equippedWeapon"] = equippedWeapon;
        j["equippedArmor"] = equippedArmor;
        j["equippedAccessory1"] = equippedAccessory1;
        j["equippedAccessory2"] = equippedAccessory2;
        j["currentTab"] = currentTab;
        j["slots"] = json::array();
        for (const auto& slot : slots) {
            j["slots"].push_back(slot.toJson());
        }
        return j;
    }

    void fromJson(const json& j) {
        if (j.contains("capacity")) {
            capacity = j["capacity"];
            slots.resize(capacity);
        }
        if (j.contains("equippedWeapon")) equippedWeapon = j["equippedWeapon"];
        if (j.contains("equippedArmor")) equippedArmor = j["equippedArmor"];
        if (j.contains("equippedAccessory1")) equippedAccessory1 = j["equippedAccessory1"];
        if (j.contains("equippedAccessory2")) equippedAccessory2 = j["equippedAccessory2"];
        if (j.contains("currentTab")) currentTab = j["currentTab"];
        if (j.contains("slots")) {
            for (size_t i = 0; i < j["slots"].size() && i < slots.size(); ++i) {
                slots[i].fromJson(j["slots"][i]);
            }
        }
    }

    std::map<std::string, int> itemUpgradeLevels;

    int getUpgradeLevel(const std::string& itemID) const {
        auto it = itemUpgradeLevels.find(itemID);
        return (it != itemUpgradeLevels.end()) ? it->second : 0;
    }

    bool upgradeItem(const std::string& itemID) {
        int currentLevel = getUpgradeLevel(itemID);
        if (currentLevel >= 10) return false;
        itemUpgradeLevels[itemID] = currentLevel + 1;
        return true;
    }

    json upgradesToJson() const {
        json j;
        for (const auto& [itemID, level] : itemUpgradeLevels) {
            j[itemID] = level;
        }
        return j;
    }

    void upgradesFromJson(const json& j) {
        itemUpgradeLevels.clear();
        for (auto it = j.begin(); it != j.end(); ++it) {
            itemUpgradeLevels[it.key()] = it.value();
        }
    }
};

// Combat Component
struct CombatComponent {
    float attackDamage{ 10.0f };
    float attackRange{ 40.0f };
    float attackCooldown{ 0.5f };
    float lastAttackTime{ 0.0f };
    bool isAttacking{ false };

    bool canAttack(float currentTime) const {
        return (currentTime - lastAttackTime) >= attackCooldown;
    }

    json toJson() const {
        return {
            {"attackDamage", attackDamage},
            {"attackRange", attackRange},
            {"attackCooldown", attackCooldown}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("attackDamage")) attackDamage = j["attackDamage"];
        if (j.contains("attackRange")) attackRange = j["attackRange"];
        if (j.contains("attackCooldown")) attackCooldown = j["attackCooldown"];
    }
};

// Projectile Component
struct ProjectileComponent {
    EntityID owner{ INVALID_ENTITY };
    float damage{ 10.0f };
    float lifetime{ 3.0f };
    float timeAlive{ 0.0f };
    glm::vec2 direction{ 1.0f, 0.0f };

    json toJson() const {
        return {
            {"damage", damage},
            {"lifetime", lifetime},
            {"direction", {direction.x, direction.y}}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("damage")) damage = j["damage"];
        if (j.contains("lifetime")) lifetime = j["lifetime"];
        if (j.contains("direction")) direction = glm::vec2(j["direction"][0], j["direction"][1]);
    }
};

// AI State Component
enum class AIBehavior {
    Idle,
    Patrol,
    Chase,
    Attack,
    Flee
};

struct AIStateComponent {
    AIBehavior currentBehavior{ AIBehavior::Idle };
    EntityID targetEntity{ INVALID_ENTITY };
    std::vector<glm::vec2> patrolPoints;
    int currentPatrolIndex{ 0 };
    float sightRange{ 200.0f };
    float attackRange{ 50.0f };
    float stateTimer{ 0.0f };

    json toJson() const {
        json j;
        j["currentBehavior"] = static_cast<int>(currentBehavior);
        j["sightRange"] = sightRange;
        j["attackRange"] = attackRange;
        j["patrolPoints"] = json::array();
        for (const auto& point : patrolPoints) {
            j["patrolPoints"].push_back({ point.x, point.y });
        }
        return j;
    }

    void fromJson(const json& j) {
        if (j.contains("currentBehavior")) currentBehavior = static_cast<AIBehavior>(j["currentBehavior"].get<int>());
        if (j.contains("sightRange")) sightRange = j["sightRange"];
        if (j.contains("attackRange")) attackRange = j["attackRange"];
        if (j.contains("patrolPoints")) {
            patrolPoints.clear();
            for (const auto& point : j["patrolPoints"]) {
                patrolPoints.push_back(glm::vec2(point[0], point[1]));
            }
        }
    }
};

// Tag Component (for identification)
struct TagComponent {
    std::string tag;

    json toJson() const {
        return { {"tag", tag} };
    }

    void fromJson(const json& j) {
        if (j.contains("tag")) tag = j["tag"];
    }
};

// Crafting Recipe
struct CraftingRecipe {
    std::string resultItemID;
    int resultQuantity;
    std::unordered_map<std::string, int> ingredients; // itemID -> quantity

    bool canCraft(const InventoryComponent& inventory) const {
        for (const auto& [itemID, required] : ingredients) {
            if (inventory.getItemCount(itemID) < required) {
                return false;
            }
        }
        return true;
    }
};

// Interactable Component (for chests, NPCs, etc)
struct InteractableComponent {
    std::string interactType{ "chest" }; // "chest", "npc", "sign"
    float interactRange{ 50.0f };
    bool canInteract{ true };
    std::string lootTableID; // For chests
    bool showPrompt{ false }; // Show "E" prompt when near

    json toJson() const {
        return {
            {"interactType", interactType},
            {"interactRange", interactRange},
            {"lootTableID", lootTableID}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("interactType")) interactType = j["interactType"];
        if (j.contains("interactRange")) interactRange = j["interactRange"];
        if (j.contains("lootTableID")) lootTableID = j["lootTableID"];
    }
};