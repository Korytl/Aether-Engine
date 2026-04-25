#pragma once

#include "Core.h"
#include "EntityRegistry.h"
#include "Renderer.h"
#include "PhysicsSystem.h"
#include "ResourceManager.h"
#include "WorldSystem.h"
#include "AudioManager.h"        
#include "GameStateManager.h"    
#include "MenuUI.h"              
#include "LaserSystem.h"         
#include "StatsSystem.h"         
#include "MagicSystem.h"         
#include "DamageTextSystem.h"    
#include <GLFW/glfw3.h>

class Game {
public:
    Game();
    ~Game();

    bool initialize();
    void run();
    void shutdown();

private:
    void processInput(float deltaTime);
    void update(float deltaTime);
    void render();
    void renderImGui();
    void renderHealthBars();
    void renderInteractionPrompts();
    void renderInventoryUI();
    void renderAnimationEditor();

    // NEW: State-based updates
    void updateSplashScreen(float deltaTime);
    void updateMainMenu(float deltaTime);
    void updateGameplay(float deltaTime);
    void updateGameOver(float deltaTime);

    // NEW: State-based rendering
    void renderSplashScreen();
    void renderMainMenu();
    void renderGameplay();
    void renderGameOver();

    // Systems
    void updateRenderSystem();
    void updatePlayerController(float deltaTime);
    void updateAISystem(float deltaTime);
    void updateHealthSystem(float deltaTime);
    void updateProjectileSystem(float deltaTime);
    void updateAnimationSystem(float deltaTime);
    void updateInteractionSystem(float deltaTime);
    void updateSpawnSystem(float deltaTime);

    // Entity management
    EntityID spawnEntity(const std::string& templateID, const glm::vec2& position); 
    void removeEntity(EntityID entity);
    void destroyEntity(EntityID entity); 
    void performAttack(EntityID attacker);
    void shootMagicMissile(EntityID shooter);
    EntityID createMagicMissile(const glm::vec2& position, const glm::vec2& direction, EntityID owner, float damage);

    // NEW: Audio management
    void loadAudioAssets();

    // NEW: Save/Load system
    void saveGame();
    void autosave();  // Auto-save at checkpoints
    void loadGame(int slotIndex);
    json serializeGameState();
    void deserializeGameState(const json& data);

    // NEW: Player death/respawn
    void handlePlayerDeath();
    void respawnPlayer();

    // NEW: Laser system
    void shootLaserBeam();
    void updateLaserSystem(float deltaTime);
    void renderLaserBeams();
    EntityID createLaserBeam(const glm::vec2& startPos, const glm::vec2& direction, EntityID owner);

    //NEW: Stats system
    void updatePlayerHealthFromStats();
    void updatePlayerManaFromStats();
    void updatePlayerStatsFromEquipment();

    // NEW: Magic system
    void castMagicSkill(int slotIndex);
    void updateMagicSystem(float deltaTime);
    void renderMagicEffects();
    EntityID createMagicEffect(MagicSkillType skillType, const glm::vec2& position, EntityID owner);

    // Magic VFX rendering
    void renderLightningStorm(MagicEffectComponent* effect);
    void renderBlackhole(MagicEffectComponent* effect);
    void renderOmniLaser(MagicEffectComponent* effect);
    void renderFireBeam(MagicEffectComponent* effect);
    void renderDarkCut(MagicEffectComponent* effect);
    void renderVerticalSlash(MagicEffectComponent* effect);
    void renderTimeStop(MagicEffectComponent* effect);
    void renderErasureBeam(MagicEffectComponent* effect);

    // Magic scroll system
    void tryDropMagicScroll(const glm::vec2& position);
    void dropMagicScrollItem(const glm::vec2& position, const MagicScrollItem& scroll);
    void useMagicScroll(const std::string& scrollID);

    //NEW: Inventory system
    void useItem(const std::string& itemID);
    void renderItemsTab(InventoryComponent* inventory);
    void renderEquipmentTab(InventoryComponent* inventory);
    void renderSkillsTab(PlayerMagicComponent* magic);
    void renderMagicTab(PlayerMagicComponent* magic);

    //NEW: Damage text display
    void createDamageText(float damage, const glm::vec2& position, bool isPlayerDamage, bool isCritical);
    void updateDamageTextSystem(float deltaTime);
    void renderDamageText();
    glm::vec2 worldToScreen(const glm::vec2& worldPos) const;

    //NEW: Equipment details UI
    void renderEquipmentDetailsUI();

    // Input callbacks
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    // Helper functions
    glm::vec2 screenToWorld(const glm::vec2& screenPos) const;

    GLFWwindow* window;
    EntityRegistry registry;
    Renderer renderer;
    PhysicsSystem physicsSystem;
    WorldSystem* worldSystem;
    MenuUI menuUI;  // NEW

    // Game state
    EntityID playerEntity;
    glm::vec2 cameraPosition;
    float cameraZoom;

    bool editorMode;
    bool debugDrawEnabled;
    bool running;
    bool showInventory;
    bool showCrafting;
    bool showUI;
    bool useSpriteMode; // Toggle between primitive (false) and sprite (true) rendering
    bool showAnimationEditor;
    bool isPaused = false;

    // Timing
    float lastFrameTime;
    float deltaAccumulator;
    int frameCount;
    float fps;
    float totalPlayTime;  // NEW: Track total playtime for saves

    // Window size
    int windowWidth;
    int windowHeight;

    // Spawn system
    float spawnTimer;
    float idleTimer;
    glm::vec2 lastPlayerPosition;
    float playerMovementDistance;
    std::vector<glm::vec2> spawnedChestPositions;

    // Mouse tracking
    glm::vec2 mouseWorldPos;

    // NEW: Respawn system
    glm::vec2 lastCheckpointPosition;

    // NEW: Save system tracking
    int currentSaveSlot; // -1 = no active slot, 0-2 = slot index

    // NEW: Damage text system
    bool showDamageText = true;

    // NEW: Equipment details UI
    std::string selectedEquipmentID;
    bool showEquipmentDetails = false;
};