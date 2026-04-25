#include "Renderer.h"
#include <iostream>

// Shader Implementation
Shader::Shader() : programID(0) {}

Shader::~Shader() {
    if (programID) {
        glDeleteProgram(programID);
    }
}

GLuint Shader::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
        return 0;
    }

    return shader;
}

bool Shader::loadFromSource(const char* vertexSrc, const char* fragmentSrc) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSrc);

    if (!vertexShader || !fragmentShader) {
        return false;
    }

    programID = glCreateProgram();
    glAttachShader(programID, vertexShader);
    glAttachShader(programID, fragmentShader);
    glLinkProgram(programID);

    GLint success;
    glGetProgramiv(programID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(programID, 512, nullptr, infoLog);
        std::cerr << "Shader linking error: " << infoLog << std::endl;
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

void Shader::use() const {
    glUseProgram(programID);
}

void Shader::setMat4(const char* name, const glm::mat4& mat) const {
    GLint location = glGetUniformLocation(programID, name);
    glUniformMatrix4fv(location, 1, GL_FALSE, &mat[0][0]);
}

void Shader::setVec4(const char* name, const glm::vec4& vec) const {
    GLint location = glGetUniformLocation(programID, name);
    glUniform4fv(location, 1, &vec[0]);
}

void Shader::setFloat(const char* name, float value) const {
    GLint location = glGetUniformLocation(programID, name);
    glUniform1f(location, value);
}

void Shader::setInt(const char* name, int value) const {
    GLint location = glGetUniformLocation(programID, name);
    glUniform1i(location, value);
}

// Renderer Implementation
Renderer::Renderer()
    : VAO(0), VBO(0), EBO(0)
    , viewportWidth(800), viewportHeight(600)
    , debugDrawEnabled(false)
    , currentTexture(0) {
    vertices.reserve(MAX_VERTICES);
    indices.reserve(MAX_INDICES);
    textureSlots.resize(MAX_TEXTURES, 0);
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(int windowWidth, int windowHeight) {
    viewportWidth = windowWidth;
    viewportHeight = windowHeight;

    // Initialize OpenGL state
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Setup orthographic projection
    projectionMatrix = glm::ortho(0.0f, (float)windowWidth, 0.0f, (float)windowHeight, -1.0f, 1.0f);
    viewMatrix = glm::mat4(1.0f);

    // Load shader (sources defined in shader files)
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec4 aColor;
        layout (location = 2) in vec2 aTexCoord;
        layout (location = 3) in float aTexID;
        
        out vec4 vColor;
        out vec2 vTexCoord;
        out float vTexID;
        
        uniform mat4 uViewProjection;
        
        void main() {
            gl_Position = uViewProjection * vec4(aPos, 0.0, 1.0);
            vColor = aColor;
            vTexCoord = aTexCoord;
            vTexID = aTexID;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec4 vColor;
        in vec2 vTexCoord;
        in float vTexID;
        
        out vec4 FragColor;
        
        uniform sampler2D uTextures[16];
        
        void main() {
            vec4 texColor = vColor;
            
            if (vTexID > 0.5) {
                int texIndex = int(vTexID - 0.5);
                texColor = texture(uTextures[texIndex], vTexCoord) * vColor;
            }
            
            FragColor = texColor;
        }
    )";

    if (!shader.loadFromSource(vertexShaderSource, fragmentShaderSource)) {
        return false;
    }

    // Bind texture units
    shader.use();
    int samplers[MAX_TEXTURES];
    for (int i = 0; i < MAX_TEXTURES; ++i) {
        samplers[i] = i;
    }
    glUniform1iv(glGetUniformLocation(shader.programID, "uTextures"), MAX_TEXTURES, samplers);

    setupBuffers();

    return true;
}

void Renderer::setupBuffers() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_INDICES * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

    // Color attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

    // TexCoord attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

    // TexID attribute
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texID));

    glBindVertexArray(0);
}

void Renderer::shutdown() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
}

void Renderer::beginFrame(const glm::vec4& clearColor) {
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT);

    vertices.clear();
    indices.clear();
}

void Renderer::endFrame() {
    flush();
}

void Renderer::flush() {
    if (vertices.empty()) return;

    // Bind textures
    for (size_t i = 0; i < textureSlots.size(); ++i) {
        if (textureSlots[i] != 0) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, textureSlots[i]);
        }
    }

    shader.use();
    shader.setMat4("uViewProjection", getViewProjectionMatrix());

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indices.size() * sizeof(uint32_t), indices.data());

    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);

    vertices.clear();
    indices.clear();
    std::fill(textureSlots.begin(), textureSlots.end(), 0);
}

void Renderer::drawRect(const glm::vec2& position, const glm::vec2& size, const Color& color, float rotation) {
    if (vertices.size() + 4 > MAX_VERTICES || indices.size() + 6 > MAX_INDICES) {
        flush();
    }

    glm::vec4 col = color.toVec4();
    uint32_t baseIndex = vertices.size();

    // Define corners
    glm::vec2 halfSize = size * 0.5f;
    glm::vec2 corners[4] = {
        {-halfSize.x, -halfSize.y},
        { halfSize.x, -halfSize.y},
        { halfSize.x,  halfSize.y},
        {-halfSize.x,  halfSize.y}
    };

    // Apply rotation if needed
    float cosR = cos(rotation);
    float sinR = sin(rotation);

    for (int i = 0; i < 4; ++i) {
        glm::vec2 rotated;
        if (rotation != 0.0f) {
            rotated.x = corners[i].x * cosR - corners[i].y * sinR;
            rotated.y = corners[i].x * sinR + corners[i].y * cosR;
        }
        else {
            rotated = corners[i];
        }

        vertices.push_back({ position + rotated, col, glm::vec2(0, 0), 0.0f });
    }

    // Indices for two triangles
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 3);
}

void Renderer::drawSprite(GLuint textureID, const glm::vec2& position, const glm::vec2& size,
    const glm::vec4& spriteRect, const Color& tint,
    float rotation, bool flipX, bool flipY) {
    if (vertices.size() + 4 > MAX_VERTICES || indices.size() + 6 > MAX_INDICES) {
        flush();
    }

    // Find or add texture slot
    float texSlot = 0.0f;
    for (size_t i = 0; i < textureSlots.size(); ++i) {
        if (textureSlots[i] == textureID) {
            texSlot = i + 1.0f;
            break;
        }
        if (textureSlots[i] == 0) {
            textureSlots[i] = textureID;
            texSlot = i + 1.0f;
            break;
        }
    }

    if (texSlot == 0.0f) {
        flush();
        textureSlots[0] = textureID;
        texSlot = 1.0f;
    }

    glm::vec4 col = tint.toVec4();
    uint32_t baseIndex = vertices.size();

    // Calculate UV coordinates from sprite rect
    // spriteRect should be normalized 0-1
    glm::vec2 uvMin(spriteRect.x, spriteRect.y);
    glm::vec2 uvMax(spriteRect.x + spriteRect.z, spriteRect.y + spriteRect.w);

    if (flipX) std::swap(uvMin.x, uvMax.x);
    if (flipY) std::swap(uvMin.y, uvMax.y);

    glm::vec2 texCoords[4] = {
        {uvMin.x, uvMin.y},
        {uvMax.x, uvMin.y},
        {uvMax.x, uvMax.y},
        {uvMin.x, uvMax.y}
    };

    // Define corners
    glm::vec2 halfSize = size * 0.5f;
    glm::vec2 corners[4] = {
        {-halfSize.x, -halfSize.y},
        { halfSize.x, -halfSize.y},
        { halfSize.x,  halfSize.y},
        {-halfSize.x,  halfSize.y}
    };

    // Apply rotation
    float cosR = cos(rotation);
    float sinR = sin(rotation);

    for (int i = 0; i < 4; ++i) {
        glm::vec2 rotated;
        if (rotation != 0.0f) {
            rotated.x = corners[i].x * cosR - corners[i].y * sinR;
            rotated.y = corners[i].x * sinR + corners[i].y * cosR;
        }
        else {
            rotated = corners[i];
        }

        vertices.push_back({ position + rotated, col, texCoords[i], texSlot });
    }

    // Indices
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 3);
}

void Renderer::drawCircle(const glm::vec2& position, float radius, const Color& color, int segments) {
    if (vertices.size() + segments + 1 > MAX_VERTICES || indices.size() + segments * 3 > MAX_INDICES) {
        flush();
    }

    glm::vec4 col = color.toVec4();
    uint32_t baseIndex = vertices.size();

    // Center vertex
    vertices.push_back({ position, col, glm::vec2(0, 0), 0.0f });

    // Outer vertices
    for (int i = 0; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * Math::PI;
        glm::vec2 offset(cos(angle) * radius, sin(angle) * radius);
        vertices.push_back({ position + offset, col, glm::vec2(0, 0), 0.0f });
    }

    // Indices
    for (int i = 0; i < segments; ++i) {
        indices.push_back(baseIndex);
        indices.push_back(baseIndex + i + 1);
        indices.push_back(baseIndex + i + 2);
    }
}

void Renderer::drawLine(const glm::vec2& start, const glm::vec2& end, const Color& color, float thickness) {
    glm::vec2 direction = glm::normalize(end - start);
    glm::vec2 perpendicular(-direction.y, direction.x);
    glm::vec2 offset = perpendicular * (thickness * 0.5f);

    drawRect((start + end) * 0.5f,
        glm::vec2(glm::length(end - start), thickness),
        color,
        atan2(direction.y, direction.x));
}

void Renderer::setCamera(const glm::vec2& position, float zoom) {
    viewMatrix = glm::mat4(1.0f);
    viewMatrix = glm::translate(viewMatrix, glm::vec3(-position.x + viewportWidth * 0.5f,
        -position.y + viewportHeight * 0.5f, 0.0f));
    viewMatrix = glm::scale(viewMatrix, glm::vec3(zoom, zoom, 1.0f));
}

void Renderer::drawDebugRect(const Rect& rect, const Color& color) {
    if (!debugDrawEnabled) return;

    drawLine({ rect.left(), rect.bottom() }, { rect.right(), rect.bottom() }, color, 2.0f);
    drawLine({ rect.right(), rect.bottom() }, { rect.right(), rect.top() }, color, 2.0f);
    drawLine({ rect.right(), rect.top() }, { rect.left(), rect.top() }, color, 2.0f);
    drawLine({ rect.left(), rect.top() }, { rect.left(), rect.bottom() }, color, 2.0f);
}

void Renderer::setViewport(int width, int height) {
    viewportWidth = width;
    viewportHeight = height;
    glViewport(0, 0, width, height);
    projectionMatrix = glm::ortho(0.0f, (float)width, 0.0f, (float)height, -1.0f, 1.0f);
}