#include "MenuUI.h"
#include "stb_image.h"
#include <iostream>

MenuUI::MenuUI()
    : splashTexture(0)
    , splashTimer(0.0f)
    , splashDuration(3.0f)
    , splashAlpha(0.0f)
    , selectedSlot(0)
    , newGameMode(false)
    , playerNameInput("")
    , menuSelection(0)
{
}

MenuUI::~MenuUI() {
    Shutdown();
}

bool MenuUI::Initialize() {
    std::cout << "Menu UI initialized" << std::endl;
    return true;
}

void MenuUI::Shutdown() {
    if (splashTexture) {
        glDeleteTextures(1, &splashTexture);
        splashTexture = 0;
    }
}

GLuint MenuUI::LoadTexture(const std::string& filepath) {
    int width, height, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "Failed to load texture: " << filepath << std::endl;
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    std::cout << "Loaded splash texture: " << filepath << " (" << width << "x" << height << ")" << std::endl;
    return texture;
}

void MenuUI::SetSplashImage(const std::string& imagePath) {
    if (splashTexture) {
        glDeleteTextures(1, &splashTexture);
    }
    splashTexture = LoadTexture(imagePath);
}

void MenuUI::RenderSplashScreen(Renderer& renderer, float deltaTime, int windowWidth, int windowHeight) {
    splashTimer += deltaTime;

    // Fade in/out
    if (splashTimer < 1.0f) {
        splashAlpha = splashTimer;
    }
    else if (splashTimer > splashDuration - 1.0f) {
        splashAlpha = splashDuration - splashTimer;
    }
    else {
        splashAlpha = 1.0f;
    }

    splashAlpha = std::clamp(splashAlpha, 0.0f, 1.0f);

    // Draw splash image if available (fills entire window)
    if (splashTexture) {
        Color tint(1.0f, 1.0f, 1.0f, splashAlpha);
        renderer.drawSprite(splashTexture,
            glm::vec2(windowWidth / 2.0f, windowHeight / 2.0f),  // Center of window
            glm::vec2(windowWidth, windowHeight),                 // Full window size
            glm::vec4(0, 0, 1, 1),
            tint,
            0.0f,    // rotation
            false,   // flipX
            true);   // flipY - Fix the upside-down splash screen
    }
    else {
        // Fallback: draw colored rectangle
        renderer.drawRect(glm::vec2(windowWidth / 2.0f, windowHeight / 2.0f),
            glm::vec2(windowWidth, windowHeight),
            Color(0.1f, 0.1f, 0.2f, splashAlpha));
    }

    // Auto-advance after duration
    if (splashTimer >= splashDuration) {
        GameStateManager::GetInstance().SetState(GameState::MainMenu);
    }
}

void MenuUI::RenderMainMenu(Renderer& renderer) {
    // Background
    renderer.drawRect(glm::vec2(640, 360), glm::vec2(1280, 720), Color(0.1f, 0.1f, 0.15f, 1.0f));

    // Title
    renderer.drawRect(glm::vec2(640, 600), glm::vec2(400, 80), Color(0.2f, 0.3f, 0.5f, 1.0f));

    auto& stateManager = GameStateManager::GetInstance();
    const auto& saveSlots = stateManager.GetSaveSlots();

    if (newGameMode) {
        // New game slot selection
        renderer.drawRect(glm::vec2(640, 500), glm::vec2(600, 60), Color(0.3f, 0.3f, 0.35f, 1.0f));

        // Draw save slots
        for (int i = 0; i < 3; ++i) {
            float yPos = 400.0f - i * 80.0f;
            Color slotColor = (i == selectedSlot) ? Color(0.4f, 0.6f, 0.8f, 1.0f) : Color(0.25f, 0.25f, 0.3f, 1.0f);

            renderer.drawRect(glm::vec2(640, yPos), glm::vec2(500, 60), slotColor);

            if (!saveSlots[i].isEmpty) {
                // Show slot info
                renderer.drawRect(glm::vec2(640, yPos), glm::vec2(480, 40), Color(0.2f, 0.2f, 0.25f, 1.0f));
            }
        }

        // Back button
        Color backColor = Color(0.5f, 0.2f, 0.2f, 1.0f);
        renderer.drawRect(glm::vec2(640, 100), glm::vec2(200, 50), backColor);

    }
    else {
        // Main menu
        const char* menuItems[] = { "New Game", "Continue", "Quit" };

        for (int i = 0; i < 3; ++i) {
            float yPos = 400.0f - i * 80.0f;
            Color buttonColor = (i == menuSelection) ? Color(0.4f, 0.6f, 0.8f, 1.0f) : Color(0.3f, 0.3f, 0.35f, 1.0f);

            renderer.drawRect(glm::vec2(640, yPos), glm::vec2(300, 60), buttonColor);
        }
    }
}

void MenuUI::RenderGameOverScreen(Renderer& renderer, float respawnTimer) {
    // Semi-transparent overlay
    renderer.drawRect(glm::vec2(640, 360), glm::vec2(1280, 720), Color(0, 0, 0, 0.7f));

    // Game Over text box
    renderer.drawRect(glm::vec2(640, 400), glm::vec2(400, 100), Color(0.2f, 0.1f, 0.1f, 1.0f));

    if (respawnTimer > 0) {
        // Respawn timer box
        renderer.drawRect(glm::vec2(640, 280), glm::vec2(300, 60), Color(0.3f, 0.3f, 0.3f, 1.0f));

        // Timer bar
        float timerPercent = respawnTimer / 5.0f;
        renderer.drawRect(glm::vec2(640, 280), glm::vec2(280 * timerPercent, 40), Color(0.8f, 0.6f, 0.2f, 1.0f));
    }
    else {
        // Respawn button
        renderer.drawRect(glm::vec2(640, 280), glm::vec2(200, 60), Color(0.2f, 0.6f, 0.2f, 1.0f));
    }
}

void MenuUI::HandleSplashInput() {
    // Can skip splash screen by pressing any key
}

void MenuUI::HandleMenuInput() {
    // Input handling is done via ImGui or GLFW callbacks
}

void MenuUI::DrawButton(Renderer& renderer, const glm::vec2& position, const glm::vec2& size,
    const std::string& text, bool highlighted, const Color& color) {
    Color buttonColor = highlighted ? Color(color.r * 1.3f, color.g * 1.3f, color.b * 1.3f, color.a) : color;
    renderer.drawRect(position, size, buttonColor);

    // Border
    Color borderColor = highlighted ? Color::White() : Color(0.5f, 0.5f, 0.5f, 1.0f);
    // Draw border lines (simplified)
}