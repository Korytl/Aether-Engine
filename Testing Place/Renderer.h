#pragma once

#include "Core.h"
#include <GLEW/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

struct Vertex {
    glm::vec2 position;
    glm::vec4 color;
    glm::vec2 texCoord;
    float texID; // Texture index (0 = no texture)
};

class Shader {
public:
    Shader();
    ~Shader();

    bool loadFromSource(const char* vertexSrc, const char* fragmentSrc);
    void use() const;

    void setMat4(const char* name, const glm::mat4& mat) const;
    void setVec4(const char* name, const glm::vec4& vec) const;
    void setFloat(const char* name, float value) const;
    void setInt(const char* name, int value) const;
    GLuint programID;

private:
    
    GLuint compileShader(GLenum type, const char* source);
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool initialize(int windowWidth, int windowHeight);
    void shutdown();

    void beginFrame(const glm::vec4& clearColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
    void endFrame();

    // Drawing primitives
    void drawRect(const glm::vec2& position, const glm::vec2& size, const Color& color, float rotation = 0.0f);
    void drawCircle(const glm::vec2& position, float radius, const Color& color, int segments = 32);
    void drawLine(const glm::vec2& start, const glm::vec2& end, const Color& color, float thickness = 1.0f);

    // Drawing sprites
    void drawSprite(GLuint textureID, const glm::vec2& position, const glm::vec2& size,
        const glm::vec4& spriteRect, const Color& tint = Color::White(),
        float rotation = 0.0f, bool flipX = false, bool flipY = false);

    // Camera
    void setCamera(const glm::vec2& position, float zoom = 1.0f);
    glm::mat4 getViewProjectionMatrix() const { return projectionMatrix * viewMatrix; }

    // Debug rendering
    void enableDebugDraw(bool enable) { debugDrawEnabled = enable; }
    void drawDebugRect(const Rect& rect, const Color& color);

    // Viewport
    void setViewport(int width, int height);

private:
    void flush();
    void setupBuffers();

    GLuint VAO, VBO, EBO;
    Shader shader;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glm::mat4 projectionMatrix;
    glm::mat4 viewMatrix;

    int viewportWidth, viewportHeight;
    bool debugDrawEnabled;

    GLuint currentTexture;
    std::vector<GLuint> textureSlots;

    static constexpr size_t MAX_VERTICES = 10000;
    static constexpr size_t MAX_INDICES = 15000;
    static constexpr size_t MAX_TEXTURES = 16;
};

