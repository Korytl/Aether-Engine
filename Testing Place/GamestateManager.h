#pragma once

#include "Core.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>

using json = nlohmann::json;

enum class GameState {
	SplashScreen,
	MainMenu,
	Playing,
	Paused,
	GameOver
};

struct SaveSlot
{
	int slotIndex;
	bool isEmpty;
	std::string playerName;
	float playTime;
	int playerLevel;
	glm::vec2 playerPosition;
	std::string lastSaved;

	json toJson() const {
		return {
			{"slotIndex", slotIndex},
			{"isEmpty", isEmpty},
			{"playerName", playerName},
			{"playTime", playTime},
			{"playerLevel", playerLevel},
			{"playerPosition", {playerPosition.x, playerPosition.y}},
			{"lastSaved", lastSaved}
		};
	}

	void fromJson(const json& j)
	{
		if (j.contains("slotIndex")) slotIndex = j["slotIndex"];
		if (j.contains("isEmpty")) isEmpty = j["isEmpty"];
		if (j.contains("playerName")) playerName = j["playerName"];
		if (j.contains("playTime")) playTime = j["playTime"];
		if (j.contains("playerLevel")) playerLevel = j["playerLevel"];
		if (j.contains("playerPosition"))
		{
			playerPosition = glm::vec2(j["playerPosition"][0], j["playerPosition"][1]);
		}
		if (j.contains("lastSaved")) lastSaved = j["lastSaved"];
	}
};

class GameStateManager
{
public:
	static GameStateManager& GetInstance()
	{
		static GameStateManager instance;
		return instance;
	}

	//state management
	void SetState(GameState newState);
	GameState GetState() const { return currentState; }
	GameState GetPreviousState() const { return previousState; }

	//Save slot management
	bool LoadSaveSlots();
	bool SaveSaveSlots();
	const std::vector<SaveSlot>& GetSaveSlots() const { return saveSlots; }
	SaveSlot* GetSaveSlot(int index);
	bool DeleteSaveSlot(int index);
	bool CreateNewGame(int slotIndex, const std::string& playerName);

	//current save
	void SetCurrentSlot(int index) { currentSlotIndex = index; }
	int GetCurrentSlot() const { return currentSlotIndex; }
	SaveSlot* GetCurrentSaveSlot();

	//Save/load game state
	bool SaveGame(const json& gameData);
	bool loadGame(int slotIndex, json& gameData);

	// Autosave management
	bool SaveAutosave(const json& gameData);
	bool LoadAutosave(json& gameData);
	bool HasAutosave() const { return hasAutosave; }

	//Player death/respawn
	void SetPlayerDied(bool died) { playerDied = died; }
	bool HasPlayerDied() const { return playerDied; }
	void UpdateRespawnTimer(float deltaTime);
	bool CanRespawn() const { return respawnTimer <= 0.0f; }
	float GetRespawnTimer() const { return respawnTimer; }
	void ResetRespawnTimer() { respawnTimer = respawnDelay; }

private:
	GameStateManager()
		: currentState(GameState::SplashScreen)
		, previousState(GameState::SplashScreen)
		, currentSlotIndex(-1)
		, playerDied(false)
		, respawnTimer(0.0f)
		, respawnDelay(5.0f)
		, hasAutosave(false)
	{
		// Initialize 3 save slots
		saveSlots.resize(3);
		for (int i = 0; i < 3; ++i) {
			saveSlots[i].slotIndex = i;
			saveSlots[i].isEmpty = true;
			saveSlots[i].playerName = "";
			saveSlots[i].playTime = 0.0f;
			saveSlots[i].playerLevel = 1;
			saveSlots[i].playerPosition = glm::vec2(0, 0);
		}
	}

	GameState currentState;
	GameState previousState;

	std::vector<SaveSlot> saveSlots;
	int currentSlotIndex;

	// Autosave
	bool hasAutosave;

	// Respawn system
	bool playerDied;
	float respawnTimer;
	float respawnDelay;
};