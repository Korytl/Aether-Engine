#pragma once

#include "Core.h"
#include "ResourceManager.h"
#include "StatsSystem.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

//Magic skill types
enum class MagicSkillType
{
	None,
	AOE_Lightning,		//1. Lightning storm
	AOE_Blackhole,		//2. Pulls enemies and damages
	AOE_OmniLaser,		//3. 360 degrees laser beams
	AOE_FireBeam,		//4. Rotating fire beam
	AOE_DarkCut,		//5. Dimensional slash
	AOE_VerticalSlash,	//6. Vertical energy wave
	AOE_TimeStop,		//7. Freeze enemies in time
	AOE_ErasureBeam		//8. Disintegration beam
};

//Magic skill data
struct MagicSkillData
{
	MagicSkillType type;
	std::string name;
	std::string description;

	//Stats
	float baseDamage = 100.0f;
	float manaCost = 50.0f;
	float cooldown = 5.0f;
	float radius = 300.0f;
	float duration = 2.0f;

	//VFX properties
	Color primaryColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
	Color secondaryColor = Color(0.5f, 0.5f, 1.0f, 1.0f);

	//Rarity - affects drop chance from magic scrolls
	float dropChance = 0.05f; //5% drop chance - base

	json toJson() const
	{
		return
		{
			{"type", (int)type},
			{"name", name},
			{"baseDamage", baseDamage},
			{"manaCost", manaCost},
			{"cooldown", cooldown}
		};
	}

	void fromJson(const json& j)
	{
		if (j.contains("type")) type = (MagicSkillType)j["type"].get<int>();
		if (j.contains("name")) name = j["name"];
		if (j.contains("baseDamage")) baseDamage = j["baseDamage"];
		if (j.contains("manaCost")) manaCost = j["manaCost"];
		if (j.contains("cooldown")) cooldown = j["cooldown"];
	}
};

//Player magic component
struct PlayerMagicComponent
{
	//Mana system
	float currentMana = 100.0f;
	float maxMana = 100.0f;
	float manaRegen = 5.0f; //per second

	//Equipped skill keys (1-4 keys)
	MagicSkillType equippedSkills[4] =
	{
		MagicSkillType::None,
		MagicSkillType::None,
		MagicSkillType::None,
		MagicSkillType::None
	};

	//skill cooldowns
	float skillCooldowns[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	//unlocked skills - learned from magic scrolls
	std::vector<MagicSkillType> unlockedSkills;

	bool hasSkill(MagicSkillType skill) const
	{
		return std::find(unlockedSkills.begin(), unlockedSkills.end(), skill) != unlockedSkills.end();
	}

	void unlockSkill(MagicSkillType skill)
	{
		if (!hasSkill(skill))
		{
			unlockedSkills.push_back(skill);
		}
	}

	// Update mana based on INT stat
	void updateFromStats(int intelligence)
	{
		// Base: 100 mana, +10 per INT
		float newMaxMana = 100.0f + (intelligence * 10.0f);

		// Base: 5 regen, +0.5 per INT
		float newManaRegen = 5.0f + (intelligence * 0.5f);

		// If max mana increased, scale current mana proportionally
		if (newMaxMana > maxMana && maxMana > 0) {
			float ratio = currentMana / maxMana;
			maxMana = newMaxMana;
			currentMana = maxMana * ratio;
		}
		else {
			maxMana = newMaxMana;
		}

		manaRegen = newManaRegen;
	}

	bool canCast(int slotIndex, const MagicSkillData& skillData) const
	{
		if (slotIndex < 0 || slotIndex >= 4) return false;
		if (equippedSkills[slotIndex] == MagicSkillType::None) return false;
		if (skillCooldowns[slotIndex] > 0.0f) return false;
		if (currentMana < skillData.manaCost) return false;
		return true;
	}

	json toJson() const
	{
		json j;
		j["currentMana"] = currentMana;
		j["maxMana"] = maxMana;
		j["manaRegen"] = manaRegen;

		j["equippedSkills"] = json::array();
		for (int i = 0; i < 4; ++i)
		{
			j["equippedSkills"].push_back((int)equippedSkills[i]);
		}

		j["unlockedSkills"] = json::array();
		for (auto skill : unlockedSkills)
		{
			j["unlockedSkills"].push_back((int)skill);
		}

		return j;
	}

	void fromJson(const json& j)
	{
		if (j.contains("currentMana")) currentMana = j["currentMana"];
		if (j.contains("maxMana")) maxMana = j["maxMana"];
		if (j.contains("manaRegen")) manaRegen = j["manaRegen"];

		if (j.contains("equippedSkills"))
		{
			for (int i = 0; i < 4 && i < j["equippedSkills"].size(); ++i)
			{
				equippedSkills[i] = (MagicSkillType)j["equippedSkills"][i].get<int>();
			}
		}

		if (j.contains("unlockedSkills"))
		{
			unlockedSkills.clear();
			for (const auto& skill : j["unlockedSkills"])
			{
				unlockedSkills.push_back((MagicSkillType)skill.get<int>());
			}
		}
	}
};

//Magic effect component
struct MagicEffectComponent
{
	MagicSkillType skillType;

	glm::vec2 position;
	float lifetime = 0.0f;
	float maxLifetime = 2.0f;
	float radius = 300.0f;
	float damage = 100.0f;

	bool isCritical = false;

	// Tick system
	float tickRate = 0.5f;            // Seconds between damage ticks
	float tickTimer = 0.0f;           // Time accumulated toward next tick

	EntityID owner = INVALID_ENTITY;
	std::vector<EntityID> hitEntities;

	//VFX state
	float rotation = 0.0f;
	float rotationSpeed = 2.0f; //2 radians/sec
	Color color1 = Color(1, 1, 1, 1);
	Color color2 = Color(0.5f, 0.5f, 1, 1);

	// Type-specific data
	bool isPulling = false; // For blackhole
	float pullStrength = 500.0f;
	int laserCount = 16; // For omni-laser
	float beamWidth = 10.0f;
	bool freezesTime = false; // For time stop - freezes all enemies

};

//Particle for magic vfx
struct MagicParticle
{
	glm::vec2 position;
	glm::vec2 velocity;
	float lifetime = 0.0f;
	float maxLifetime = 1.0f;
	float size = 4.0f;
	Color color;
	float alpha = 1.0f;
	float rotation = 0.0f;
	float rotationSpeed = 0.0f;
};

// Magic Scroll Item (drops from enemies)
struct MagicScrollItem {
	MagicSkillType skillType;
	std::string scrollName;
	std::string description;
	float dropChance; // % chance to drop

	std::string getItemID() const {
		switch (skillType) {
		case MagicSkillType::AOE_Lightning: return "scroll_lightning";
		case MagicSkillType::AOE_Blackhole: return "scroll_blackhole";
		case MagicSkillType::AOE_OmniLaser: return "scroll_omnilaser";
		case MagicSkillType::AOE_FireBeam: return "scroll_firebeam";
		case MagicSkillType::AOE_DarkCut: return "scroll_darkcut";
		case MagicSkillType::AOE_VerticalSlash: return "scroll_verticalslash";
		case MagicSkillType::AOE_TimeStop: return "scroll_timestop";
		case MagicSkillType::AOE_ErasureBeam: return "scroll_erasure";
		default: return "scroll_unknown";
		}
	}
};

// Get skill data by type
inline MagicSkillData GetSkillData(MagicSkillType type) {
	MagicSkillData data;
	data.type = type;

	switch (type) {
	case MagicSkillType::AOE_Lightning:
		data.name = "Lightning Storm";
		data.description = "Summons lightning bolts in an area";
		data.baseDamage = 150.0f;
		data.manaCost = 60.0f;
		data.cooldown = 8.0f;
		data.radius = 400.0f;
		data.duration = 3.0f;
		data.primaryColor = Color(0.3f, 0.3f, 1.0f, 1.0f); // Blue
		data.secondaryColor = Color(1.0f, 1.0f, 0.5f, 1.0f); // Yellow
		data.dropChance = 0.08f; // 8%
		break;

	case MagicSkillType::AOE_Blackhole:
		data.name = "Void Blackhole";
		data.description = "Creates a blackhole that pulls and damages";
		data.baseDamage = 200.0f;
		data.manaCost = 80.0f;
		data.cooldown = 12.0f;
		data.radius = 350.0f;
		data.duration = 4.0f;
		data.primaryColor = Color(0.1f, 0.0f, 0.3f, 1.0f); // Dark purple
		data.secondaryColor = Color(0.5f, 0.0f, 1.0f, 1.0f); // Purple
		data.dropChance = 0.05f; // 5% (rare)
		break;

	case MagicSkillType::AOE_OmniLaser:
		data.name = "Omnidirectional Laser";
		data.description = "Fires lasers in all directions";
		data.baseDamage = 120.0f;
		data.manaCost = 70.0f;
		data.cooldown = 10.0f;
		data.radius = 500.0f;
		data.duration = 2.0f;
		data.primaryColor = Color(1.0f, 0.2f, 0.2f, 1.0f); // Red
		data.secondaryColor = Color(1.0f, 0.8f, 0.2f, 1.0f); // Orange
		data.dropChance = 0.07f; // 7%
		break;

	case MagicSkillType::AOE_FireBeam:
		data.name = "Inferno Beam";
		data.description = "Rotating beam of pure fire";
		data.baseDamage = 180.0f;
		data.manaCost = 75.0f;
		data.cooldown = 9.0f;
		data.radius = 450.0f;
		data.duration = 3.0f;
		data.primaryColor = Color(1.0f, 0.3f, 0.0f, 1.0f); // Orange
		data.secondaryColor = Color(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
		data.dropChance = 0.06f; // 6%
		break;

	case MagicSkillType::AOE_DarkCut:
		data.name = "Dark Dimensional Cut";
		data.description = "Slash through dimensions";
		data.baseDamage = 250.0f;
		data.manaCost = 90.0f;
		data.cooldown = 15.0f;
		data.radius = 600.0f;
		data.duration = 1.5f;
		data.primaryColor = Color(0.2f, 0.0f, 0.2f, 1.0f); // Dark purple
		data.secondaryColor = Color(0.8f, 0.0f, 0.8f, 1.0f); // Magenta
		data.dropChance = 0.03f; // 3% (very rare)
		break;

	case MagicSkillType::AOE_VerticalSlash:
		data.name = "Vertical Energy Slash";
		data.description = "Massive vertical energy wave";
		data.baseDamage = 220.0f;
		data.manaCost = 85.0f;
		data.cooldown = 11.0f;
		data.radius = 550.0f;
		data.duration = 2.5f;
		data.primaryColor = Color(0.0f, 1.0f, 0.5f, 1.0f); // Cyan
		data.secondaryColor = Color(0.5f, 1.0f, 1.0f, 1.0f); // Light cyan
		data.dropChance = 0.04f; // 4%
		break;

	case MagicSkillType::AOE_TimeStop:
		data.name = "Temporal Stasis";
		data.description = "Freezes enemies in time";
		data.baseDamage = 50.0f; // Low damage, high utility
		data.manaCost = 100.0f;
		data.cooldown = 20.0f;
		data.radius = 400.0f;
		data.duration = 5.0f;
		data.primaryColor = Color(0.5f, 0.5f, 1.0f, 1.0f); // Light blue
		data.secondaryColor = Color(1.0f, 1.0f, 1.0f, 1.0f); // White
		data.dropChance = 0.02f; // 2% (legendary)
		break;

	case MagicSkillType::AOE_ErasureBeam:
		data.name = "Erasure Beam";
		data.description = "Beam that erases existence";
		data.baseDamage = 300.0f;
		data.manaCost = 120.0f;
		data.cooldown = 25.0f;
		data.radius = 700.0f;
		data.duration = 2.0f;
		data.primaryColor = Color(1.0f, 1.0f, 1.0f, 1.0f); // White
		data.secondaryColor = Color(0.8f, 0.8f, 1.0f, 1.0f); // Light purple
		data.dropChance = 0.01f; // 1% (mythic)
		break;

	default:
		data.name = "Unknown";
		data.description = "???";
		break;
	}

	return data;
}

// Get all possible scrolls
inline std::vector<MagicScrollItem> GetAllScrolls() {
	static std::vector<MagicScrollItem> scrolls;
	static bool initialized = false;

	if (!initialized) {
		initialized = true;
		auto& rm = ResourceManager::getInstance();

		// List of scroll item IDs (matching items.json)
		const std::vector<std::string> scrollIDs = {
			"scroll_lightning", "scroll_blackhole", "scroll_omnilaser",
			"scroll_firebeam", "scroll_darkcut", "scroll_verticalslash",
			"scroll_timestop", "scroll_erasure"
		};

		for (const auto& id : scrollIDs) {
			const Item* item = rm.getItem(id);
			if (item) {
				// Map item ID to MagicSkillType
				MagicSkillType skillType = MagicSkillType::None;
				if (id == "scroll_lightning") skillType = MagicSkillType::AOE_Lightning;
				else if (id == "scroll_blackhole") skillType = MagicSkillType::AOE_Blackhole;
				else if (id == "scroll_omnilaser") skillType = MagicSkillType::AOE_OmniLaser;
				else if (id == "scroll_firebeam") skillType = MagicSkillType::AOE_FireBeam;
				else if (id == "scroll_darkcut") skillType = MagicSkillType::AOE_DarkCut;
				else if (id == "scroll_verticalslash") skillType = MagicSkillType::AOE_VerticalSlash;
				else if (id == "scroll_timestop") skillType = MagicSkillType::AOE_TimeStop;
				else if (id == "scroll_erasure") skillType = MagicSkillType::AOE_ErasureBeam;

				if (skillType != MagicSkillType::None) {
					MagicSkillData data = GetSkillData(skillType); // for name/description
					MagicScrollItem scroll;
					scroll.skillType = skillType;
					scroll.scrollName = data.name + " Scroll";
					scroll.description = data.description;
					scroll.dropChance = item->rarity;   // Use the rarity from items.json
					scrolls.push_back(scroll);
				}
			}
		}
	}
	return scrolls;
}