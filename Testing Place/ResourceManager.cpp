#include "ResourceManager.h"
#include <iostream>
#include <random>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

bool ResourceManager::loadJsonFile(const std::string& filepath, json& outJson) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return false;
    }

    try {
        file >> outJson;

        // Store timestamp for hot reload
        if (hotReloadEnabled) {
            fileTimestamps[filepath] = std::filesystem::last_write_time(filepath);
        }

        return true;
    }
    catch (const json::exception& e) {
        std::cerr << "JSON parse error in " << filepath << ": " << e.what() << std::endl;
        return false;
    }
}

bool ResourceManager::loadItems(const std::string& filepath) {
    json j;
    if (!loadJsonFile(filepath, j)) return false;

    items.clear();

    if (j.contains("items") && j["items"].is_array()) {
        for (const auto& itemJson : j["items"]) {
            Item item;
            item.fromJson(itemJson);

            std::cout << "\n=== Processing item: " << item.name << " ===" << std::endl;
            std::cout << "  iconTextureID: '" << item.iconTextureID << "'" << std::endl;
            std::cout << "  iconTexturePath: '" << item.iconTexturePath << "'" << std::endl;

            // Check if texture already loaded by sprite scanner
            const Texture* existingTexture = getTexture(item.iconTextureID);
            if (existingTexture) {
                std::cout << "  ✓ Texture already loaded (from sprite scanner)" << std::endl;
            }
            else if (!item.iconTexturePath.empty()) {
                std::cout << "  Attempting to load texture..." << std::endl;

                if (std::filesystem::exists(item.iconTexturePath)) {
                    std::cout << "  ✓ File exists: " << item.iconTexturePath << std::endl;
                    if (loadTexture(item.iconTextureID, item.iconTexturePath)) {
                        std::cout << "Loaded icon successfully" << std::endl;
                    }
                    else {
                        std::cout << "Failed to load texture" << std::endl;
                    }
                }
                else {
                    std::cout << "  File not found: " << item.iconTexturePath << std::endl;
                    std::cout << "  Current dir: " << std::filesystem::current_path() << std::endl;
                }
            }
            else {
                std::cout << "  (No icon path specified)" << std::endl;
            }

            items[item.id] = item;
        }
    }

    std::cout << "\nLoaded " << items.size() << " items total" << std::endl;
    return true;
}

bool ResourceManager::loadEntityTemplates(const std::string& filepath) {
    json j;
    if (!loadJsonFile(filepath, j)) return false;

    entityTemplates.clear();

    if (j.contains("templates") && j["templates"].is_array()) {
        for (const auto& templateJson : j["templates"]) {
            EntityTemplate templ;
            templ.fromJson(templateJson);
            entityTemplates[templ.id] = templ;
        }
    }

    std::cout << "Loaded " << entityTemplates.size() << " entity templates from " << filepath << std::endl;
    return true;
}

bool ResourceManager::loadTileDefinitions(const std::string& filepath) {
    json j;
    if (!loadJsonFile(filepath, j)) return false;

    tileDefinitions.clear();

    if (j.contains("tiles") && j["tiles"].is_array()) {
        for (const auto& tileJson : j["tiles"]) {
            TileDefinition tile;
            tile.fromJson(tileJson);
            tileDefinitions[tile.id] = tile;
        }
    }

    std::cout << "Loaded " << tileDefinitions.size() << " tile definitions from " << filepath << std::endl;
    return true;
}

bool ResourceManager::loadLootTables(const std::string& filepath) {
    json j;
    if (!loadJsonFile(filepath, j)) return false;

    lootTables.clear();

    if (j.contains("lootTables") && j["lootTables"].is_array()) {
        for (const auto& tableJson : j["lootTables"]) {
            LootTable table;
            table.fromJson(tableJson);
            lootTables[table.id] = table;
        }
    }

    std::cout << "Loaded " << lootTables.size() << " loot tables from " << filepath << std::endl;
    return true;
}

const Item* ResourceManager::getItem(const std::string& id) const {
    auto it = items.find(id);
    return it != items.end() ? &it->second : nullptr;
}

const EntityTemplate* ResourceManager::getEntityTemplate(const std::string& id) const {
    auto it = entityTemplates.find(id);
    return it != entityTemplates.end() ? &it->second : nullptr;
}

const TileDefinition* ResourceManager::getTileDefinition(const std::string& id) const {
    auto it = tileDefinitions.find(id);
    return it != tileDefinitions.end() ? &it->second : nullptr;
}

const LootTable* ResourceManager::getLootTable(const std::string& id) const {
    auto it = lootTables.find(id);
    return it != lootTables.end() ? &it->second : nullptr;
}

void ResourceManager::checkForChanges() {
    if (!hotReloadEnabled) return;

    for (auto& [filepath, lastTime] : fileTimestamps) {
        try {
            auto currentTime = std::filesystem::last_write_time(filepath);
            if (currentTime != lastTime) {
                std::cout << "File changed: " << filepath << " - Reloading..." << std::endl;

                // Determine file type and reload
                if (filepath.find("items") != std::string::npos) {
                    loadItems(filepath);
                }
                else if (filepath.find("templates") != std::string::npos) {
                    loadEntityTemplates(filepath);
                }
                else if (filepath.find("tiles") != std::string::npos) {
                    loadTileDefinitions(filepath);
                }
                else if (filepath.find("loot") != std::string::npos) {
                    loadLootTables(filepath);
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "File system error: " << e.what() << std::endl;
        }
    }
}

std::vector<std::string> ResourceManager::generateLoot(const std::string& lootTableID) {
    std::vector<std::string> result;

    const LootTable* table = getLootTable(lootTableID);
    if (!table) return result;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);

    for (const auto& entry : table->entries) {
        if (chanceDist(gen) <= entry.dropChance) {
            std::uniform_int_distribution<int> quantityDist(entry.minQuantity, entry.maxQuantity);
            int quantity = quantityDist(gen);

            for (int i = 0; i < quantity; ++i) {
                result.push_back(entry.itemID);
            }
        }
    }

    return result;
}

bool ResourceManager::loadTexture(const std::string& textureID, const std::string& filepath) {
    // Check if already loaded
    if (textures.find(textureID) != textures.end()) {
        std::cout << "Texture already loaded: " << textureID << std::endl;
        return true;
    }

    Texture texture;
    texture.filepath = filepath;
    texture.id = loadImageToTexture(filepath, texture.width, texture.height);

    if (texture.id == 0) {
        std::cerr << "Failed to load texture: " << filepath << std::endl;
        return false;
    }

    textures[textureID] = texture;
    std::cout << "Loaded texture: " << textureID << " (" << texture.width << "x" << texture.height << ")" << std::endl;

    return true;
}

GLuint ResourceManager::loadImageToTexture(const std::string& filepath, int& width, int& height) {
    // Load image using stb_image
    int channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 4); // Force RGBA

    if (!data) {
        std::cerr << "Failed to load image: " << filepath << std::endl;
        return 0;
    }

    // Create OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // Pixel art style
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);

    return textureID;
}

bool ResourceManager::loadSpriteAtlas(const std::string& filepath) {
    json j;
    if (!loadJsonFile(filepath, j)) return false;

    // Support TexturePacker format
    if (j.contains("meta") && j.contains("frames")) {
        std::string textureFile = j["meta"]["image"];
        std::string textureID = j["meta"].value("image", "atlas");

        // Load the texture
        std::string texturePath = std::filesystem::path(filepath).parent_path().string() + "/" + textureFile;
        loadTexture(textureID, texturePath);

        const Texture* texture = getTexture(textureID);
        if (!texture) return false;

        // Load sprite definitions
        for (auto& [spriteName, frameData] : j["frames"].items()) {
            SpriteDefinition sprite;
            sprite.textureID = textureID;

            auto frame = frameData["frame"];
            sprite.rect = glm::vec4(
                frame["x"].get<float>(),
                frame["y"].get<float>(),
                frame["w"].get<float>(),
                frame["h"].get<float>()
            );

            sprites[spriteName] = sprite;
        }

        std::cout << "Loaded sprite atlas: " << filepath << " (" << sprites.size() << " sprites)" << std::endl;
        return true;
    }

    return false;
}

bool ResourceManager::loadAnimations(const std::string& filepath) {
    json j;
    if (!loadJsonFile(filepath, j)) return false;

    if (j.contains("animations") && j["animations"].is_array()) {
        for (const auto& animJson : j["animations"]) {
            AnimationDefinition anim;
            anim.name = animJson["name"];
            anim.frameDuration = animJson.value("frameDuration", 0.1f);
            anim.loop = animJson.value("loop", true);

            if (animJson.contains("frames")) {
                for (const auto& frameName : animJson["frames"]) {
                    anim.frameNames.push_back(frameName);
                }
            }

            animations[anim.name] = anim;
        }

        std::cout << "Loaded " << animations.size() << " animations" << std::endl;
        return true;
    }

    return false;
}

const Texture* ResourceManager::getTexture(const std::string& id) const {
    auto it = textures.find(id);
    return it != textures.end() ? &it->second : nullptr;
}

const SpriteDefinition* ResourceManager::getSprite(const std::string& id) const {
    auto it = sprites.find(id);
    return it != sprites.end() ? &it->second : nullptr;
}

const AnimationDefinition* ResourceManager::getAnimation(const std::string& id) const {
    auto it = animations.find(id);
    return it != animations.end() ? &it->second : nullptr;
}

void ResourceManager::loadAllSpritesFromDirectory(const std::string& baseDir) {
    std::cout << "\n=== Scanning for sprite atlases in: " << baseDir << " ===" << std::endl;

    if (!std::filesystem::exists(baseDir)) {
        std::cerr << "Directory does not exist: " << baseDir << std::endl;
        return;
    }

    int atlasesLoaded = 0;
    int texturesLoaded = 0;

    try {
        // Recursively scan all subdirectories
        for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDir)) {
            if (entry.is_regular_file()) {
                std::string filepath = entry.path().string();
                std::string filename = entry.path().filename().string();
                std::string extension = entry.path().extension().string();

                // Load JSON sprite atlases
                if (extension == ".json") {
                    std::cout << "Found JSON: " << filepath << std::endl;
                    if (loadSpriteAtlas(filepath)) {
                        atlasesLoaded++;
                        std::cout << "  ✓ Loaded sprite atlas" << std::endl;
                    }
                }
                // Load individual PNG textures (not part of an atlas)
                else if (extension == ".png") {
                    // Check if there's a corresponding JSON file
                    std::string jsonPath = entry.path().string();
                    jsonPath = jsonPath.substr(0, jsonPath.length() - 4) + ".json";

                    if (!std::filesystem::exists(jsonPath)) {
                        // Standalone texture without atlas
                        std::string textureID = entry.path().stem().string();
                        std::cout << "Found standalone texture: " << filename << std::endl;
                        if (loadTexture(textureID, filepath)) {
                            texturesLoaded++;
                            std::cout << "  ✓ Loaded as '" << textureID << "'" << std::endl;
                        }
                    }
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    std::cout << "\n=== Sprite Loading Summary ===" << std::endl;
    std::cout << "Sprite Atlases: " << atlasesLoaded << std::endl;
    std::cout << "Standalone Textures: " << texturesLoaded << std::endl;
    std::cout << "Total Sprites: " << sprites.size() << std::endl;
    std::cout << "Total Textures: " << textures.size() << std::endl;
    std::cout << "==============================\n" << std::endl;
}

void ResourceManager::loadAllAnimationsFromDirectory(const std::string& baseDir) {
    std::cout << "\n=== Scanning for animations in: " << baseDir << " ===" << std::endl;

    if (!std::filesystem::exists(baseDir)) {
        return;
    }

    int animationsLoaded = 0;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDir)) {
            if (entry.is_regular_file()) {
                std::string filepath = entry.path().string();
                std::string filename = entry.path().filename().string();

                // Look for animation files (typically named animations.json or *_anim.json)
                if (filename.find("anim") != std::string::npos &&
                    entry.path().extension() == ".json") {
                    std::cout << "Found animation file: " << filepath << std::endl;
                    if (loadAnimations(filepath)) {
                        animationsLoaded++;
                        std::cout << "  ✓ Loaded animations" << std::endl;
                    }
                }
            }
        }
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    std::cout << "Animation files loaded: " << animationsLoaded << std::endl;
    std::cout << "Total animations: " << animations.size() << std::endl;
    std::cout << "==============================\n" << std::endl;
}