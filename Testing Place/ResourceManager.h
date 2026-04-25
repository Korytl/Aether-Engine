#pragma once

#include "Core.h"
#include "Components.h"
#include <nlohmann/json.hpp>
#include <GLEW/glew.h>
#include <fstream>
#include <filesystem>

using json = nlohmann::json;

// Texture data
struct Texture {
    GLuint id{ 0 };
    int width{ 0 };
    int height{ 0 };
    std::string filepath;
};

// Sprite definition from atlas
struct SpriteDefinition {
    std::string textureID;
    glm::vec4 rect; // Pixel coordinates (x, y, width, height)
    glm::vec2 pivot{ 0.5f, 0.5f }; // Pivot point (0-1 normalized)
};

// Animation definition
struct AnimationDefinition {
    std::string name;
    std::vector<std::string> frameNames; // Sprite names
    float frameDuration{ 0.1f };
    bool loop{ true };
};

struct EntityTemplate {
    std::string id;
    std::vector<std::pair<std::string, json>> components;

    json toJson() const {
        json j;
        j["id"] = id;
        j["components"] = json::object();
        for (const auto& [name, data] : components) {
            j["components"][name] = data;
        }
        return j;
    }

    void fromJson(const json& j) {
        if (j.contains("id")) id = j["id"];
        if (j.contains("components")) {
            components.clear();
            for (auto& [key, value] : j["components"].items()) {
                components.push_back({ key, value });
            }
        }
    }
};

struct TileDefinition {
    std::string id;
    bool solid{ false };
    Color color{ Color::White() };

    json toJson() const {
        return {
            {"id", id},
            {"solid", solid},
            {"color", {color.r, color.g, color.b, color.a}}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("id")) id = j["id"];
        if (j.contains("solid")) solid = j["solid"];
        if (j.contains("color")) {
            auto c = j["color"];
            color = Color(c[0], c[1], c[2], c[3]);
        }
    }
};

struct LootEntry {
    std::string itemID;
    float dropChance;
    int minQuantity;
    int maxQuantity;

    json toJson() const {
        return {
            {"itemID", itemID},
            {"dropChance", dropChance},
            {"minQuantity", minQuantity},
            {"maxQuantity", maxQuantity}
        };
    }

    void fromJson(const json& j) {
        if (j.contains("itemID")) itemID = j["itemID"];
        if (j.contains("dropChance")) dropChance = j["dropChance"];
        if (j.contains("minQuantity")) minQuantity = j["minQuantity"];
        if (j.contains("maxQuantity")) maxQuantity = j["maxQuantity"];
    }
};

struct LootTable {
    std::string id;
    std::vector<LootEntry> entries;

    json toJson() const {
        json j;
        j["id"] = id;
        j["entries"] = json::array();
        for (const auto& entry : entries) {
            j["entries"].push_back(entry.toJson());
        }
        return j;
    }

    void fromJson(const json& j) {
        if (j.contains("id")) id = j["id"];
        if (j.contains("entries")) {
            entries.clear();
            for (const auto& entryJson : j["entries"]) {
                LootEntry entry;
                entry.fromJson(entryJson);
                entries.push_back(entry);
            }
        }
    }
};

class ResourceManager {
public:
    static ResourceManager& getInstance() {
        static ResourceManager instance;
        return instance;
    }

    // Loading
    bool loadItems(const std::string& filepath);
    bool loadEntityTemplates(const std::string& filepath);
    bool loadTileDefinitions(const std::string& filepath);
    bool loadLootTables(const std::string& filepath);
    bool loadTexture(const std::string& textureID, const std::string& filepath);
    bool loadSpriteAtlas(const std::string& filepath); // Load TexturePacker/Aseprite JSON
    bool loadAnimations(const std::string& filepath);

    // Batch loading - scans directories
    void loadAllSpritesFromDirectory(const std::string& baseDir);
    void loadAllAnimationsFromDirectory(const std::string& baseDir);

    // Getters
    const Item* getItem(const std::string& id) const;
    const EntityTemplate* getEntityTemplate(const std::string& id) const;
    const TileDefinition* getTileDefinition(const std::string& id) const;
    const LootTable* getLootTable(const std::string& id) const;
    const Texture* getTexture(const std::string& id) const;
    const SpriteDefinition* getSprite(const std::string& id) const;
    const AnimationDefinition* getAnimation(const std::string& id) const;

    // Get all (for debugging/listing)
    const std::unordered_map<std::string, Texture>& getAllTextures() const { return textures; }
    const std::unordered_map<std::string, SpriteDefinition>& getAllSprites() const { return sprites; }

    // Hot reload
    void checkForChanges();
    void enableHotReload(bool enable) { hotReloadEnabled = enable; }

    // Utilities
    std::vector<std::string> generateLoot(const std::string& lootTableID);

private:
    ResourceManager() : hotReloadEnabled(false) {}

    std::unordered_map<std::string, Item> items;
    std::unordered_map<std::string, EntityTemplate> entityTemplates;
    std::unordered_map<std::string, TileDefinition> tileDefinitions;
    std::unordered_map<std::string, LootTable> lootTables;
    std::unordered_map<std::string, Texture> textures;
    std::unordered_map<std::string, SpriteDefinition> sprites;
    std::unordered_map<std::string, AnimationDefinition> animations;

    std::unordered_map<std::string, std::filesystem::file_time_type> fileTimestamps;
    bool hotReloadEnabled;

    bool loadJsonFile(const std::string& filepath, json& outJson);
    GLuint loadImageToTexture(const std::string& filepath, int& width, int& height);
};
