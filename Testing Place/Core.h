#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <glm/glm.hpp>

// Core type definitions
using EntityID = uint64_t;
using ComponentTypeID = uint32_t;

constexpr EntityID INVALID_ENTITY = 0;

// Component type ID generator
template<typename T>
struct ComponentType {
    static ComponentTypeID getID() {
        static ComponentTypeID id = nextID++;
        return id;
    }
private:
    static ComponentTypeID nextID;
};

template<typename T>
ComponentTypeID ComponentType<T>::nextID = 1;

// Forward declarations
class Entity;
class EntityRegistry;

// Utility macros
#define UNUSED(x) (void)(x)

// Math helpers
namespace Math {
    constexpr float PI = 3.14159265359f;
    constexpr float DEG2RAD = PI / 180.0f;
    constexpr float RAD2DEG = 180.0f / PI;

    inline float lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    inline glm::vec2 lerp(const glm::vec2& a, const glm::vec2& b, float t) {
        return a + (b - a) * t;
    }
}

// Color utility
struct Color {
    float r, g, b, a;

    Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}

    glm::vec4 toVec4() const { return glm::vec4(r, g, b, a); }

    static Color White() { return Color(1, 1, 1, 1); }
    static Color Black() { return Color(0, 0, 0, 1); }
    static Color Red() { return Color(1, 0, 0, 1); }
    static Color Green() { return Color(0, 1, 0, 1); }
    static Color Blue() { return Color(0, 0, 1, 1); }
    static Color Yellow() { return Color(1, 1, 0, 1); }
};

// Rectangle helper
struct Rect {
    float x, y, width, height;

    Rect() : x(0), y(0), width(0), height(0) {}
    Rect(float x, float y, float w, float h) : x(x), y(y), width(w), height(h) {}

    float left() const { return x; }
    float right() const { return x + width; }
    float top() const { return y + height; }
    float bottom() const { return y; }
    glm::vec2 center() const { return glm::vec2(x + width * 0.5f, y + height * 0.5f); }

    bool intersects(const Rect& other) const {
        return !(right() < other.left() || left() > other.right() ||
            top() < other.bottom() || bottom() > other.top());
    }

    bool contains(const glm::vec2& point) const {
        return point.x >= left() && point.x <= right() &&
            point.y >= bottom() && point.y <= top();
    }
};
