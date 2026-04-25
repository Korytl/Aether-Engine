#pragma once

#include "Core.h"
#include <glm/glm.hpp>
#include <vector>

//Laser beam component for the player attack (M1's)
struct LaserBeamComponent
{
	glm::vec2 startPos;
	glm::vec2 endPos;
	glm::vec2 direction;

	float lifetime = 0.0f;
	float maxLifetime = 0.3f; //Beam lasts for 0.3 seconds (SUBJECT TO CHANGE!!)
	
	float damage = 25.0f;
	float width = 8.0f;

	//Visual properties
	Color coreColor = Color(0.2f, 0.8f, 1.0f, 1.0f); //Bright cyan
	Color glowColor = Color(0.6f, 0.9f, 1.0f, 0.5f); //Lighter cyan glow

	EntityID owner = INVALID_ENTITY;

	//Track hits - purpose - avoid multi-hits - otherwise will be overpowered
	std::vector<EntityID> hitEntities;

	bool active = true;
	bool isCritical = false;
};

//Laser attack cooldown component
struct LaserCooldownComponent
{
	float cooldownTimer = 0.0f;
	float cooldownDuration = 1.0f; //1 second cooldown
	bool canFire = true;
};

// Particle effect for laser trail
struct LaserParticle {
	glm::vec2 position;
	glm::vec2 velocity;
	float lifetime = 0.0f;
	float maxLifetime = 0.5f;
	Color color;
	float size = 4.0f;
	float alpha = 1.0f;
};