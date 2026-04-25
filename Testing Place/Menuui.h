#pragma once

#include "Core.h"
#include "Renderer.h"
#include "GameStateManager.h"
#include <GLEW/glew.h>
#include <string>

class MenuUI {
public:
    MenuUI();
    ~MenuUI();

    bool Initialize();
    void Shutdown();

    // Rendering
    void RenderSplashScreen(Renderer& renderer, float deltaTime, int windowWidth, int windowHeight);
    void RenderMainMenu(Renderer& renderer);
    void RenderGameOverScreen(Renderer& renderer, float respawnTimer);

    // Input handling
    void HandleSplashInput();
    void HandleMenuInput();

    // Splash screen
    void SetSplashImage(const std::string& imagePath);
    void SetSplashDuration(float duration) { splashDuration = duration; }
    float GetSplashTimer() const { return splashTimer; }

    // Menu state
    int GetSelectedSlot() const { return selectedSlot; }
    void SetSelectedSlot(int slot) { selectedSlot = slot; }
    bool IsNewGameMode() const { return newGameMode; }
    void SetNewGameMode(bool mode) { newGameMode = mode; }
    std::string& GetPlayerNameInput() { return playerNameInput; }

private:
    // Splash screen
    GLuint splashTexture;
    float splashTimer;
    float splashDuration;
    float splashAlpha;

    // Menu
    int selectedSlot;
    bool newGameMode;
    std::string playerNameInput;
    int menuSelection; // 0 = new game, 1 = continue, 2 = quit

    // Helper functions
    GLuint LoadTexture(const std::string& filepath);
    void DrawButton(Renderer& renderer, const glm::vec2& position, const glm::vec2& size,
        const std::string& text, bool highlighted, const Color& color);
};