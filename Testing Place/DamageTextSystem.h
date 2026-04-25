#pragma once

#include "Core.h"
#include <glm/glm.hpp>
#include <string>

// Damage text component for floating damage numbers
struct DamageTextComponent {
    float damage;
    glm::vec2 position;
    glm::vec2 velocity;  // Movement direction
    float lifetime = 0.0f;
    float maxLifetime = 2.0f;
    glm::vec3 color;
    float fontSize = 24.0f;
    bool isCritical = false;
    bool isPlayerDamage = false;  // true if player took damage, false if enemy

    DamageTextComponent() = default;

    DamageTextComponent(float dmg, const glm::vec2& pos, bool isPlayer = false, bool isCrit = false)
        : damage(dmg), position(pos), isPlayerDamage(isPlayer), isCritical(isCrit)
    {
        // Velocity - float upward with slight random horizontal
        velocity.x = (rand() % 40 - 20) / 10.0f;  // -2 to +2
        velocity.y = 80.0f;  // Float upward

        // Color based on context
        if (isCritical) {
            color = glm::vec3(1.0f, 0.5f, 0.0f);  // Orange for crits
            fontSize = 32.0f;  // Bigger for crits
        }
        else if (isPlayerDamage) {
            color = glm::vec3(1.0f, 0.2f, 0.2f);  // Red for player taking damage
        }
        else {
            color = glm::vec3(1.0f, 1.0f, 0.5f);  // Yellow for enemy damage
        }
    }
};

// Helper functions for damage text system
namespace DamageTextSystem {
    // Get text to display (with K/M abbreviation for large numbers)
    inline std::string getDamageText(float damage) {
        if (damage >= 1000000.0f) {
            return std::to_string((int)(damage / 1000000.0f)) + "M";
        }
        else if (damage >= 1000.0f) {
            return std::to_string((int)(damage / 1000.0f)) + "K";
        }
        else {
            return std::to_string((int)damage);
        }
    }
}