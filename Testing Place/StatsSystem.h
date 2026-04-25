#pragma once

#include <iostream>
#include "Core.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

//Player stats component - RPG Style
struct PlayerStatsComponent
{
	//level & experience
	int level = 1;
	int experience = 0;
	int experienceToNextLevel = 100;

	//Stat points (gained on lvl up)
	int availableStatPoints = 0;

	//Base stats (player can allocate stats)
	int strength = 10;		//+2 physical damage per point
	int vitality = 10;		//+5 max HP per point
	int dexterity = 10;		//+0.5% crit chance per point (max 40%) per player (cap reached,+5.0% crit dmg.)
	int intelligence = 10;	//+2 magic damage per point
	int luck = 10;			//+0.01% drop chance and drop multiplier chance +1

	//Derived stats (calculated from base stats)
	float physicalDamage = 20.0f;
	float magicDamage = 20.0f;
	float critChance = 5.0f;	//% +0.5% crit chance
	float critDamage = 150.0f;	//% +5% crit dmg.
	float dropChance = 0.1f;	//% +0.01% chance
	float dropBoost = 7.5f;		//+0.75 added drops (materials), +1.00 (items)

	//Methods
	void calculateDerivedStats()
	{
		physicalDamage = 20.0f + (strength * 80.0f) + (level * 15.0f);
		magicDamage = 20.0f + (intelligence * 80.0f) + (level * 15.0f);
		critChance = 5.0f + (dexterity * 0.8f);
		critDamage = 150.0f + (dexterity * 1.5f);
		dropChance = 0.1f + (luck * 0.5f);
		dropBoost = 7.5f + (luck * 3.0f);	

		//cap
		//1.Crit chance to 40%
		if (critChance > 40.0f) critChance = 40.0f;

		//2. Crit damage to 1000%
		if (critDamage > 1000.0f) critDamage = 1000.0f;

		//3. Drop chance to 50%
		if (dropChance > 50.0f) dropChance = 50.0f; //if rarity is 0.0000001% it will be increased by 50% = 0.00000015
		//Note we also will cap the max drop chance to 100, so if item rarity reaches 100% it cannot be increased or go
		//past 100% e.g. if item rarity is 80% if we increase it by 50% then it will be 120% which is wrong, so we cap
		//it to 100% max. (a guaranteed drop). This only is useful for powerful rarirties (e.g 0.1% and below)

		//4. Drop boost to 40
		if (dropBoost > 40.0f) dropBoost = 40.0f; //max drop per item is 40
	}

	void addExperience(int exp)
	{
		experience += exp;

		//level up if enough experience
		while (experience >=  experienceToNextLevel && level < 100) //Current lvl cap is 100
		{
			levelUp();
		}
	}

	void levelUp()
	{
		level++;
		experience -= experienceToNextLevel;
		availableStatPoints += 10; //Gain 10 per lvl subject to change

		//Exponential xp curve
		experienceToNextLevel = (int)(100 * pow(1.2f, level - 1));
		std::cout << "LEVEL UP! Now level " << level << std::endl;
		std::cout << "  Stat points: +" << availableStatPoints << std::endl;
		std::cout << "  Next level: " << experienceToNextLevel << " XP" << std::endl;
	}

	bool allocateStat(const std::string& statName, int points = 1)
	{
		if (availableStatPoints < 1) return false;
		
		if (statName == "strength")
		{
			strength += points;
		}
		else if (statName == "vitality")
		{
			vitality += points;
		}
		else if (statName == "dexterity")
		{
			dexterity += points;
		}
		else if (statName == "intelligence")
		{
			intelligence += points;
		}
		else if (statName == "luck")
		{
			luck += points;
		}
		else
		{
			return false;
		}

		availableStatPoints -= points;
		calculateDerivedStats();
		return true;
	}

	//Serialization
	json toJson() const
	{
		return
		{
			{"level", level},
			{"experience", experience},
			{"experienceToNextLevel", experienceToNextLevel},
			{"availableStatPoints", availableStatPoints},
			{"strength", strength},
			{"vitality", vitality},
			{"dexterity", dexterity},
			{"intelligence", intelligence},
			{"luck", luck}
		};
	}

	void fromJson(const json& j)
	{
		if (j.contains("level")) level = j["level"];
		if (j.contains("experience")) experience = j["experience"];
		if (j.contains("experienceToNextLevel")) experienceToNextLevel = j["experienceToNextLevel"];
		if (j.contains("availableStatPoints")) availableStatPoints = j["availableStatPoints"];
		if (j.contains("strength")) strength = j["strength"];
		if (j.contains("vitality")) vitality = j["vitality"];
		if (j.contains("dexterity")) dexterity = j["dexterity"];
		if (j.contains("intelligence")) intelligence = j["intelligence"];
		if (j.contains("luck")) luck = j["luck"];

		calculateDerivedStats();
	}
};

struct EnemyStatsComponent
{
	int baseLevel = 1;

	//Base stats at level 1
	float baseHealth = 50.0f;
	float baseDamage = 10.0f;
	float baseDefence = 5.0f;
	int baseExpReward = 25;

	//Current scaled stats
	float scaledHealth = 50.0f;
	float scaledDamage = 10.0f;
	float scaledDefence = 5.0f;
	int scaledExpReward = 25;

	void scaleToPlayerLevel(int playerLevel)
	{
		baseLevel = playerLevel;

		//Exponential scaling
		float scaleFactor = pow(1.2f, playerLevel - 1) * 2.0f;

		scaledHealth = baseHealth * scaleFactor;
		scaledDamage = baseDamage * scaleFactor;
		scaledDefence = baseDefence * scaleFactor;
		scaledExpReward = (int)(baseExpReward * pow(1.15f, playerLevel - 1));

		std::cout << "Enemy scaled to level " << playerLevel << std::endl;
		std::cout << "  HP: " << scaledHealth << " | DMG: " << scaledDamage << std::endl;
	}
};

//Equipment stats - bonus given by gear
struct EquipmentStatsComponent
{
	//Equipment slots
	std::string weaponID;
	std::string armorID;
	std::string accessory1ID;
	std::string accessory2ID;

	//Total bonuses from all equipment
	int bonusStrength = 0;
	int bonusVitality = 0;
	int bonusDexterity = 0;
	int bonusIntelligence = 0;
	int bonusLuck = 0;

	float bonusPhysicalDamage = 0.0f;
	float bonusMagicDamage = 0.0f;
	float bonusHealth = 0.0f;
	float bonusDefence = 0.0f;
	float bonusCritChance = 0.0f;
	float bonusCritDmg = 0.0f;
	float bonusDropChance = 0.0f;
	float bonusDropBoost = 0.0f;

	void recalculateBonuses()
	{
		//Filled when we load items, default is 0
		bonusStrength = 0;
		bonusVitality = 0;
		bonusDexterity = 0;
		bonusIntelligence = 0;
		bonusLuck = 0;
		bonusPhysicalDamage = 0.0f;
		bonusMagicDamage = 0.0f;
		bonusHealth = 0.0f;
		bonusDefence = 0.0f;
		bonusCritChance = 0.0f;
		bonusCritDmg = 0.0f;
		bonusDropChance = 0.0f;
		bonusDropBoost = 0.0f;
	}

	json toJson() const
	{
		return
		{
			{"weaponID", weaponID},
			{"armorID", armorID},
			{"accessory1ID", accessory1ID},
			{"accessory2ID", accessory2ID}
		};
	}

	void fromJson(const json& j)
	{
		if (j.contains("weaponID")) weaponID = j["weaponID"];
		if (j.contains("armorID")) armorID = j["armorID"];
		if (j.contains("accessory1ID")) accessory1ID = j["accessory1ID"];
		if (j.contains("accessory2ID")) accessory2ID = j["accessory2ID"];
		recalculateBonuses();
	}
};