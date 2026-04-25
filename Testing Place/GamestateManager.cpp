#include "GameStateManager.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>

void GameStateManager::SetState(GameState newState) {
    previousState = currentState;
    currentState = newState;
}

bool GameStateManager::LoadSaveSlots() {
    std::string savePath = "saves/slots.json";

    if (!std::filesystem::exists(savePath)) {
        std::cout << "No save slots file found, using defaults" << std::endl;
        return true;
    }

    std::ifstream file(savePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open save slots file" << std::endl;
        return false;
    }

    try {
        json j;
        file >> j;

        if (j.contains("slots") && j["slots"].is_array()) {
            for (size_t i = 0; i < j["slots"].size() && i < saveSlots.size(); ++i) {
                saveSlots[i].fromJson(j["slots"][i]);
            }
        }

        std::cout << "Loaded save slots" << std::endl;
        return true;
    }
    catch (const json::exception& e) {
        std::cerr << "Error loading save slots: " << e.what() << std::endl;
        return false;
    }
}

bool GameStateManager::SaveSaveSlots() {
    // Create saves directory if it doesn't exist
    std::filesystem::create_directories("saves");

    json j;
    j["slots"] = json::array();

    for (const auto& slot : saveSlots) {
        j["slots"].push_back(slot.toJson());
    }

    std::ofstream file("saves/slots.json");
    if (!file.is_open()) {
        std::cerr << "Failed to open save slots file for writing" << std::endl;
        return false;
    }

    file << j.dump(4);
    std::cout << "Saved save slots" << std::endl;
    return true;
}

SaveSlot* GameStateManager::GetSaveSlot(int index) {
    if (index < 0 || index >= static_cast<int>(saveSlots.size())) {
        return nullptr;
    }
    return &saveSlots[index];
}

SaveSlot* GameStateManager::GetCurrentSaveSlot() {
    return GetSaveSlot(currentSlotIndex);
}

bool GameStateManager::DeleteSaveSlot(int index) {
    SaveSlot* slot = GetSaveSlot(index);
    if (!slot) return false;

    // Delete the save file
    std::string saveFile = "saves/slot_" + std::to_string(index) + ".json";
    if (std::filesystem::exists(saveFile)) {
        std::filesystem::remove(saveFile);
    }

    // Reset slot
    slot->isEmpty = true;
    slot->playerName = "";
    slot->playTime = 0.0f;
    slot->playerLevel = 1;
    slot->playerPosition = glm::vec2(0, 0);
    slot->lastSaved = "";

    SaveSaveSlots();
    std::cout << "Deleted save slot " << index << std::endl;
    return true;
}

bool GameStateManager::CreateNewGame(int slotIndex, const std::string& playerName) {
    SaveSlot* slot = GetSaveSlot(slotIndex);
    if (!slot) return false;

    // Get current time
    auto now = std::time(nullptr);
    std::tm tm;
#if defined(_MSC_VER)
    localtime_s(&tm, &now);
#else
    tm = *std::localtime(&now);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

    slot->isEmpty = false;
    slot->playerName = playerName;
    slot->playTime = 0.0f;
    slot->playerLevel = 1;
    slot->playerPosition = glm::vec2(100, 200);
    slot->lastSaved = oss.str();

    currentSlotIndex = slotIndex;
    SaveSaveSlots();

    std::cout << "Created new game in slot " << slotIndex << std::endl;
    return true;
}

bool GameStateManager::SaveGame(const json& gameData) {
    SaveSlot* slot = GetCurrentSaveSlot();
    if (!slot) {
        std::cerr << "No current save slot" << std::endl;
        return false;
    }

    // Create saves directory
    std::filesystem::create_directories("saves");

    // Get current time
    auto now = std::time(nullptr);
    std::tm tm;
#if defined(_MSC_VER)
    localtime_s(&tm, &now);
#else
    tm = *std::localtime(&now);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

    slot->lastSaved = oss.str();

    // Update slot info from game data
    if (gameData.contains("playTime")) slot->playTime = gameData["playTime"];
    if (gameData.contains("playerLevel")) slot->playerLevel = gameData["playerLevel"];
    if (gameData.contains("playerPosition")) {
        slot->playerPosition = glm::vec2(
            gameData["playerPosition"][0],
            gameData["playerPosition"][1]
        );
    }

    // Save slot info
    SaveSaveSlots();

    // Save full game data
    std::string saveFile = "saves/slot_" + std::to_string(currentSlotIndex) + ".json";
    std::ofstream file(saveFile);
    if (!file.is_open()) {
        std::cerr << "Failed to create save file" << std::endl;
        return false;
    }

    file << gameData.dump(4);
    std::cout << "Game saved to slot " << currentSlotIndex << std::endl;
    return true;
}

bool GameStateManager::loadGame(int slotIndex, json& gameData) {
    SaveSlot* slot = GetSaveSlot(slotIndex);
    if (!slot || slot->isEmpty) {
        std::cerr << "Invalid or empty save slot: " << slotIndex << std::endl;
        return false;
    }

    std::string saveFile = "saves/slot_" + std::to_string(slotIndex) + ".json";
    if (!std::filesystem::exists(saveFile)) {
        std::cerr << "Save file not found: " << saveFile << std::endl;
        return false;
    }

    std::ifstream file(saveFile);
    if (!file.is_open()) {
        std::cerr << "Failed to open save file" << std::endl;
        return false;
    }

    try {
        file >> gameData;
        currentSlotIndex = slotIndex;
        std::cout << "Game loaded from slot " << slotIndex << std::endl;
        return true;
    }
    catch (const json::exception& e) {
        std::cerr << "Error loading game: " << e.what() << std::endl;
        return false;
    }
}

void GameStateManager::UpdateRespawnTimer(float deltaTime) {
    if (playerDied && respawnTimer > 0.0f) {
        respawnTimer -= deltaTime;
        if (respawnTimer < 0.0f) {
            respawnTimer = 0.0f;
        }
    }
}

// ============================================================================
// AUTOSAVE SYSTEM
// ============================================================================

bool GameStateManager::SaveAutosave(const json& gameData) {
    std::filesystem::create_directories("saves");

    std::string autosaveFile = "saves/autosave.json";
    std::ofstream file(autosaveFile);

    if (!file.is_open()) {
        std::cerr << "Failed to create autosave file" << std::endl;
        return false;
    }

    file << gameData.dump(4);
    file.close();

    hasAutosave = true;
    std::cout << "Autosave created" << std::endl;
    return true;
}

bool GameStateManager::LoadAutosave(json& gameData) {
    std::string autosaveFile = "saves/autosave.json";

    if (!std::filesystem::exists(autosaveFile)) {
        std::cerr << "No autosave file found" << std::endl;
        return false;
    }

    std::ifstream file(autosaveFile);
    if (!file.is_open()) {
        std::cerr << "Failed to open autosave file" << std::endl;
        return false;
    }

    try {
        file >> gameData;
        std::cout << "Autosave loaded successfully" << std::endl;
        return true;
    }
    catch (const json::exception& e) {
        std::cerr << "Error loading autosave: " << e.what() << std::endl;
        return false;
    }
}