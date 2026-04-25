#include "Game.h"
#include "AudioManager.h"
#include "GameStateManager.h"
#include "Imgui/imgui.h"
#include "Imgui/imgui_impl_glfw.h"
#include "Imgui/imgui_impl_opengl3.h"
#include <GLEW/glew.h>
#include <iostream>
#include <random>
#include <algorithm>
#include <filesystem>
#include <cmath>

Game::Game()
    : window(nullptr)
    , physicsSystem(&registry)
    , worldSystem(nullptr)
    , playerEntity(INVALID_ENTITY)
    , cameraPosition(0, 0)
    , cameraZoom(1.0f)
    , editorMode(false)
    , debugDrawEnabled(false)
    , running(true)
    , showInventory(false)
    , showCrafting(false)
    , showUI(true)
    , useSpriteMode(false)
    , showAnimationEditor(false)
    , spawnTimer(0)
    , idleTimer(0)
    , lastPlayerPosition(0, 0)
    , playerMovementDistance(0)
    , mouseWorldPos(0, 0)
    , lastFrameTime(0)
    , deltaAccumulator(0)
    , frameCount(0)
    , fps(0)
    , totalPlayTime(0.0f)
    , windowWidth(1280)
    , windowHeight(720)
    , lastCheckpointPosition(100, 200) {
}

Game::~Game() {
    shutdown();
}

bool Game::initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Set OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    window = glfwCreateWindow(windowWidth, windowHeight, "2D RPG Framework", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }

    // Set callbacks
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
        return false;
    }

    std::cout << "ImGui initialized successfully" << std::endl;

    // Initialize renderer
    if (!renderer.initialize(windowWidth, windowHeight)) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return false;
    }

    // NEW: Initialize audio system
    if (!AudioManager::GetInstance().Initialize()) {
        std::cerr << "Warning: Audio system failed to initialize" << std::endl;
    }

    // NEW: Initialize menu UI
    if (!menuUI.Initialize()) {
        std::cerr << "Failed to initialize menu UI" << std::endl;
        return false;
    }
    menuUI.SetSplashImage("assets/images/splash_screen.png");
    menuUI.SetSplashDuration(3.0f);

    // NEW: Load save slots
    GameStateManager::GetInstance().LoadSaveSlots();

    // Load resources
    auto& resourceMgr = ResourceManager::getInstance();

    resourceMgr.enableHotReload(true);

    // Load all sprites from textures directory and subdirectories
    std::cout << "\n========================================" << std::endl;
    std::cout << "LOADING SPRITE SYSTEM" << std::endl;
    std::cout << "========================================" << std::endl;

    std::string texturesPath = "assets/data/textures";
    if (std::filesystem::exists(texturesPath)) {
        resourceMgr.loadAllSpritesFromDirectory(texturesPath);
        resourceMgr.loadAllAnimationsFromDirectory(texturesPath);

        if (resourceMgr.getTexture("player") || !resourceMgr.getAllTextures().empty()) {
            std::cout << "? Sprite system ready!" << std::endl;
            std::cout << "  Press F3 to toggle between Primitives/Sprites" << std::endl;
        }
        else {
            std::cout << "? No sprites loaded - using primitives only" << std::endl;
        }
    }
    else {
        std::cout << "? Textures directory not found: " << texturesPath << std::endl;
        std::cout << "  Using primitives only" << std::endl;
    }

    std::cout << "========================================\n" << std::endl;

    // Load items
    resourceMgr.loadItems("assets/data/items.json");
    resourceMgr.loadEntityTemplates("assets/data/entity_templates.json");
    resourceMgr.loadTileDefinitions("assets/data/tiles.json");
    resourceMgr.loadLootTables("assets/data/loot_tables.json");

    // Create world
    worldSystem = new WorldSystem(20, 32.0f);  // Infinite width, 20 tiles height

    // Connect world to physics system
    physicsSystem.setWorld(worldSystem);

    // Spawn player on ground
    glm::vec2 playerSpawnPos(200, 300);
    // Find ground below spawn position
    for (int y = 15; y >= 0; --y) {
        if (worldSystem->isSolidAt(glm::vec2(playerSpawnPos.x, y * 32.0f))) {
            playerSpawnPos.y = (y + 2) * 32.0f; // Spawn 2 tiles above ground
            break;
        }
    }
    playerEntity = spawnEntity("player", playerSpawnPos);
    lastPlayerPosition = playerSpawnPos;

    // NEW: Set checkpoint
    lastCheckpointPosition = playerSpawnPos;

    // NEW: Load audio assets
    loadAudioAssets();

    // Note: Enemies and chests now spawn dynamically through the spawn system
    std::cout << "Dynamic spawning system active!" << std::endl;

    lastFrameTime = glfwGetTime();

    auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);
    if (inventory) {
        std::cout << "Player inventory found! Capacity: " << inventory->capacity << std::endl;

        bool added1 = inventory->addItem("scroll_lightning", 1);
        bool added2 = inventory->addItem("scroll_blackhole", 1);
        bool added3 = inventory->addItem("scroll_omnilaser", 1);
        bool added4 = inventory->addItem("scroll_firebeam", 1);

        std::cout << "Scroll additions - Lightning: " << (added1 ? "?" : "?")
            << " Blackhole: " << (added2 ? "?" : "?")
            << " OmniLaser: " << (added3 ? "?" : "?")
            << " FireBeam: " << (added4 ? "?" : "?") << std::endl;

        // Count items
        int itemCount = 0;
        for (const auto& slot : inventory->slots) {
            if (!slot.isEmpty()) itemCount++;
        }
        std::cout << "Items in inventory: " << itemCount << std::endl;
    }
    else {
        std::cout << "ERROR: Player has no inventory component!" << std::endl;
    }

    return true;
}

void Game::run() {
    while (!glfwWindowShouldClose(window) && running) {
        float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        // Cap delta time to prevent spiral of death
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // FPS calculation
        frameCount++;
        deltaAccumulator += deltaTime;
        if (deltaAccumulator >= 1.0f) {
            fps = frameCount / deltaAccumulator;
            frameCount = 0;
            deltaAccumulator = 0;
        }

        // Poll events - ALWAYS needed for input
        glfwPollEvents();

        // Check for window size changes
        int currentWidth, currentHeight;
        glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
        if (currentWidth != windowWidth || currentHeight != windowHeight) {
            framebufferSizeCallback(window, currentWidth, currentHeight);
        }

        // Check for hot reload
        ResourceManager::getInstance().checkForChanges();

        // Update audio
        AudioManager::GetInstance().Update();

        // State-based update routing
        auto& stateMgr = GameStateManager::GetInstance();

        switch (stateMgr.GetState()) {
        case GameState::SplashScreen:
            updateSplashScreen(deltaTime);
            break;
        case GameState::MainMenu:
            updateMainMenu(deltaTime);
            break;
        case GameState::Playing:
            processInput(deltaTime);  // Only process game input when playing
            updateGameplay(deltaTime);
            break;
        case GameState::GameOver:
            updateGameOver(deltaTime);
            break;
        }

        // Render
        render();

        glfwSwapBuffers(window);
    }
}

void Game::shutdown() {
    std::cout << "Shutting down game..." << std::endl;

    // Shutdown ImGui FIRST (before any OpenGL/window cleanup)
    // Check if ImGui context exists before shutting down
    if (ImGui::GetCurrentContext() != nullptr) {
        std::cout << "  Shutting down ImGui..." << std::endl;
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    // Then shutdown audio
    std::cout << "  Shutting down audio..." << std::endl;
    AudioManager::GetInstance().Shutdown();

    // Shutdown menu (after ImGui)
    std::cout << "  Shutting down menu..." << std::endl;
    menuUI.Shutdown();

    // Shutdown renderer (after ImGui)
    std::cout << "  Shutting down renderer..." << std::endl;
    renderer.shutdown();

    // Clean up world
    if (worldSystem) {
        std::cout << "  Cleaning up world..." << std::endl;
        delete worldSystem;
        worldSystem = nullptr;
    }

    // Destroy window (after all rendering cleanup)
    if (window) {
        std::cout << "  Destroying window..." << std::endl;
        glfwDestroyWindow(window);
        window = nullptr;
    }

    // Terminate GLFW last
    std::cout << "  Terminating GLFW..." << std::endl;
    glfwTerminate();

    std::cout << "Shutdown complete." << std::endl;
}
// ============================================================================
// PROCESS INPUT SYSTEM
// ============================================================================

void Game::processInput(float deltaTime) {
    UNUSED(deltaTime);

    // Update mouse world position
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    mouseWorldPos = screenToWorld(glm::vec2(mouseX, mouseY));

    if (!editorMode && registry.isValid(playerEntity)) {
        auto* physics = registry.getComponent<PhysicsComponent>(playerEntity);
        auto* combat = registry.getComponent<CombatComponent>(playerEntity);
        auto* transform = registry.getComponent<TransformComponent>(playerEntity);
        auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);
        auto* health = registry.getComponent<HealthComponent>(playerEntity);

        if (physics) {
            const float moveForce = 15000.0f;

            // Horizontal movement
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                physics->acceleration.x = -moveForce;
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                physics->acceleration.x = moveForce;
            }

            // Jump - check if near ground by checking tiles below
            static bool wasSpacePressed = false;
            bool isSpacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

            bool isGrounded = false;
            if (transform && worldSystem) {
                // Check tiles directly below player
                auto* collider = registry.getComponent<ColliderComponent>(playerEntity);
                if (collider) {
                    Rect bounds = collider->getBounds(transform->position);
                    float checkY = bounds.bottom() - 2.0f; // Check just below feet

                    // Check 3 points below player
                    for (int i = -1; i <= 1; ++i) {
                        float checkX = transform->position.x + i * 10.0f;
                        if (worldSystem->isSolidAt(glm::vec2(checkX, checkY))) {
                            isGrounded = true;
                            break;
                        }
                    }
                }
            }

            if (isSpacePressed && !wasSpacePressed && isGrounded) {
                physics->velocity.y = 500.0f;
                std::cout << "Jump!" << std::endl;
            }
            wasSpacePressed = isSpacePressed;

            // Use health potion
            static bool wasEPressed = false;
            bool isEPressed = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;

            if (isEPressed && !wasEPressed && inventory && health) {
                if (inventory->getItemCount("health_potion") > 0) {
                    inventory->removeItem("health_potion", 1);
                    health->heal(50.0f);
                    AudioManager::GetInstance().PlaySound("item_use", 70.0f);
                    std::cout << "Used health potion! HP: " << health->currentHP << "/" << health->maxHP << std::endl;
                }
            }
            wasEPressed = isEPressed;

            // UPDATE: Attack with left mouse button (LASER BEAM)
            static bool wasAttackPressed = false;
            bool isAttackPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

            if (isAttackPressed && !wasAttackPressed) {
                shootLaserBeam();  // Use laser instead of magic missile
            }
            wasAttackPressed = isAttackPressed;
        }
    }
}

// ============================================================================
// GAMEPLAY SYSTEM
// ============================================================================

void Game::updateGameplay(float deltaTime) {
    // NEW: Track playtime
    totalPlayTime += deltaTime;

    //Pause Check - Skip all gameplay updates if paused.
    if (isPaused)
    {
        //Continue updating camera to see the game state
        if (registry.isValid(playerEntity))
        {
            auto* transform = registry.getComponent<TransformComponent>(playerEntity);
            if (transform)
            {
                cameraPosition = Math::lerp(cameraPosition, transform->position, 0.1f);
            }
        }
        //skip everything else - game is paused
        return;
    }

    // NEW: Autosave every 60 seconds (or when reaching checkpoints)
    static float autosaveTimer = 0.0f;
    autosaveTimer += deltaTime;

    if (autosaveTimer >= 60.0f) {
        autosaveTimer = 0.0f;
        autosave();
    }

    // Check if player reached a checkpoint (significant distance moved)
    if (registry.isValid(playerEntity)) {
        auto* transform = registry.getComponent<TransformComponent>(playerEntity);
        if (transform) {
            float distanceFromLastCheckpoint = glm::length(transform->position - lastCheckpointPosition);

            // Create checkpoint every 500 units
            if (distanceFromLastCheckpoint > 500.0f) {
                lastCheckpointPosition = transform->position;
                autosave();
                std::cout << "Checkpoint reached! Autosave created at ("
                    << lastCheckpointPosition.x << ", "
                    << lastCheckpointPosition.y << ")" << std::endl;
            }
        }
    }

    // Update world chunks based on camera
    if (worldSystem) {
        worldSystem->update(cameraPosition);
    }

    updateHealthSystem(deltaTime);
    updatePlayerController(deltaTime);
    updateAISystem(deltaTime);
    updateProjectileSystem(deltaTime);
    updateLaserSystem(deltaTime);
    updateMagicSystem(deltaTime);
    updateDamageTextSystem(deltaTime);
    updateAnimationSystem(deltaTime);
    updateInteractionSystem(deltaTime);
    updateSpawnSystem(deltaTime);
    physicsSystem.update(deltaTime);

    // Update camera to follow player
    if (registry.isValid(playerEntity)) {
        auto* transform = registry.getComponent<TransformComponent>(playerEntity);
        if (transform) {
            cameraPosition = Math::lerp(cameraPosition, transform->position, 0.1f);
        }
    }
}

// ============================================================================
// ANIMATION SYSTEM-TO BE WORKED ON LATER
// ============================================================================

void Game::updateAnimationSystem(float deltaTime) {
    auto entities = registry.getEntitiesWith<AnimationComponent, RenderableComponent>();

    for (EntityID entity : entities) {
        auto* anim = registry.getComponent<AnimationComponent>(entity);
        auto* renderable = registry.getComponent<RenderableComponent>(entity);

        if (anim && renderable) {
            anim->update(deltaTime, renderable);
        }
    }
}

// ============================================================================
// INTERACTION AND SPAWN SYSTEM
// ============================================================================

void Game::updateInteractionSystem(float deltaTime) {
    UNUSED(deltaTime);

    if (!registry.isValid(playerEntity)) return;

    auto* playerTransform = registry.getComponent<TransformComponent>(playerEntity);
    if (!playerTransform) return;

    // Get all interactable entities
    auto interactables = registry.getEntitiesWith<TransformComponent, InteractableComponent>();

    EntityID nearestInteractable = INVALID_ENTITY;
    float nearestDistance = FLT_MAX;

    for (EntityID entity : interactables) {
        auto* transform = registry.getComponent<TransformComponent>(entity);
        auto* interactable = registry.getComponent<InteractableComponent>(entity);

        if (!transform || !interactable || !interactable->canInteract) continue;

        float distance = glm::length(transform->position - playerTransform->position);

        if (distance <= interactable->interactRange && distance < nearestDistance) {
            nearestDistance = distance;
            nearestInteractable = entity;
        }

        // Update prompt visibility
        interactable->showPrompt = (distance <= interactable->interactRange);
    }

    // Handle interaction input
    static bool wasQPressed = false;
    bool isQPressed = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;

    if (isQPressed && !wasQPressed && nearestInteractable != INVALID_ENTITY) {
        auto* interactable = registry.getComponent<InteractableComponent>(nearestInteractable);
        auto* playerInventory = registry.getComponent<InventoryComponent>(playerEntity);

        if (interactable && playerInventory) {
            if (interactable->interactType == "chest") {
                // Generate loot
                auto loot = ResourceManager::getInstance().generateLoot(interactable->lootTableID);

                std::cout << "Opened chest! Received:" << std::endl;
                for (const auto& itemID : loot) {
                    playerInventory->addItem(itemID, 1);

                    const Item* item = ResourceManager::getInstance().getItem(itemID);
                    std::cout << "  - " << (item ? item->name : itemID) << std::endl;
                }

                // Destroy chest after opening
                destroyEntity(nearestInteractable);
            }
        }
    }

    wasQPressed = isQPressed;
}

void Game::updateSpawnSystem(float deltaTime) {
    if (!registry.isValid(playerEntity) || !worldSystem) return;

    auto* playerTransform = registry.getComponent<TransformComponent>(playerEntity);
    if (!playerTransform) return;

    // Track player movement
    float distanceMoved = glm::length(playerTransform->position - lastPlayerPosition);
    playerMovementDistance += distanceMoved;
    lastPlayerPosition = playerTransform->position;

    // Track idle time
    if (distanceMoved < 1.0f) {
        idleTimer += deltaTime;
    }
    else {
        idleTimer = 0;
    }

    // Update spawn timer
    spawnTimer += deltaTime;

    // Spawn parameters
    float baseSpawnInterval = 8.0f; // Base time between spawns
    float idleSpawnInterval = 5.0f; // Faster spawning when idle
    float movementBonus = playerMovementDistance * 0.01f; // More movement = more spawns

    // Determine spawn interval based on player behavior
    float currentSpawnInterval = baseSpawnInterval;

    if (idleTimer > 5.0f) {
        // Player idle for more than 5 seconds - spawn enemies nearby
        currentSpawnInterval = idleSpawnInterval;
    }
    else if (playerMovementDistance > 100.0f) {
        // Player moving a lot - increase spawn chance
        currentSpawnInterval = baseSpawnInterval - movementBonus;
        currentSpawnInterval = std::max(currentSpawnInterval, 3.0f); // Min 3 seconds
    }

    // Check if it's time to spawn
    if (spawnTimer >= currentSpawnInterval) {
        spawnTimer = 0;
        playerMovementDistance = 0; // Reset movement tracker

        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> spawnChance(0.0f, 1.0f);
        std::uniform_int_distribution<int> groupSize(1, 3); // 1-3 enemies per group
        std::uniform_real_distribution<float> angleRand(0.0f, 2.0f * 3.14159f);
        std::uniform_real_distribution<float> distRand(200.0f, 400.0f);
        // 70% chance to spawn enemies
        if (spawnChance(gen) < 0.7f) {
            int numEnemies = groupSize(gen);

            // Choose spawn location around player
            float spawnAngle = angleRand(gen);
            float spawnDist = distRand(gen);

            glm::vec2 spawnCenter = playerTransform->position + glm::vec2(
                cos(spawnAngle) * spawnDist,
                sin(spawnAngle) * spawnDist
            );

            // Spawn enemy group
            for (int i = 0; i < numEnemies; ++i) {
                glm::vec2 enemyOffset(
                    (i - numEnemies / 2) * 50.0f,
                    0
                );

                glm::vec2 enemyPos = spawnCenter + enemyOffset;

                // Find ground below spawn position
                for (int y = 15; y >= 0; --y) {
                    if (worldSystem->isSolidAt(glm::vec2(enemyPos.x, y * 32.0f))) {
                        enemyPos.y = (y + 2) * 32.0f;
                        spawnEntity("goblin", enemyPos);
                        break;
                    }
                }
            }

            if (numEnemies == 1) {
                std::cout << "Spawned 1 enemy nearby" << std::endl;
            }
            else {
                std::cout << "Spawned a group of " << numEnemies << " enemies" << std::endl;
            }
        }

        // 30% chance to spawn chest (separate from enemy spawning)
        if (spawnChance(gen) < 0.3f) {
            // Random position within exploration range
            float chestAngle = angleRand(gen);
            float chestDist = distRand(gen);

            glm::vec2 chestPos = playerTransform->position + glm::vec2(
                cos(chestAngle) * chestDist,
                sin(chestAngle) * chestDist
            );

            // Check if chest already exists nearby
            bool tooClose = false;
            for (const auto& existingPos : spawnedChestPositions) {
                if (glm::length(chestPos - existingPos) < 300.0f) {
                    tooClose = true;
                    break;
                }
            }

            if (!tooClose) {
                // Find ground below
                for (int y = 15; y >= 0; --y) {
                    if (worldSystem->isSolidAt(glm::vec2(chestPos.x, y * 32.0f))) {
                        chestPos.y = (y + 2.0f) * 32.0f;
                        spawnEntity("chest", chestPos);
                        spawnedChestPositions.push_back(chestPos);

                        // Keep only last 20 chest positions to prevent memory bloat
                        if (spawnedChestPositions.size() > 20) {
                            spawnedChestPositions.erase(spawnedChestPositions.begin());
                        }

                        std::cout << "Spawned chest in the distance" << std::endl;
                        break;
                    }
                }
            }
        }
    }
}

// ============================================================================
// RENDER SYSTEM
// ============================================================================

void Game::render() {
    renderer.beginFrame(glm::vec4(0.53f, 0.81f, 0.92f, 1.0f));

    auto& stateMgr = GameStateManager::GetInstance();

    switch (stateMgr.GetState()) {
    case GameState::SplashScreen:
        renderSplashScreen();
        break;
    case GameState::MainMenu:
        renderMainMenu();
        break;
    case GameState::Playing:
        renderGameplay();
        // Show death overlay if player died
        if (stateMgr.HasPlayerDied()) {
            menuUI.RenderGameOverScreen(renderer, stateMgr.GetRespawnTimer());
        }
        break;
    case GameState::GameOver:
        renderGameplay();
        menuUI.RenderGameOverScreen(renderer, stateMgr.GetRespawnTimer());
        break;
    }

    // CRITICAL: End renderer BEFORE ImGui
    renderer.endFrame();

    // THEN render ImGui on top
    renderImGui();
}

void Game::updateRenderSystem() {
    auto entities = registry.getEntitiesWith<TransformComponent, RenderableComponent>();

    // Sort by layer
    std::sort(entities.begin(), entities.end(), [this](EntityID a, EntityID b) {
        auto* ra = registry.getComponent<RenderableComponent>(a);
        auto* rb = registry.getComponent<RenderableComponent>(b);
        return ra->layer < rb->layer;
        });

    auto& resourceMgr = ResourceManager::getInstance();

    for (EntityID entity : entities) {
        auto* transform = registry.getComponent<TransformComponent>(entity);
        auto* renderable = registry.getComponent<RenderableComponent>(entity);

        if (!transform || !renderable || !renderable->visible) continue;

        // Check global render mode toggle
        bool shouldUseSprite = useSpriteMode &&
            renderable->renderMode == RenderMode::Sprite &&
            !renderable->textureID.empty();

        if (shouldUseSprite) {
            // Render as sprite
            const Texture* texture = resourceMgr.getTexture(renderable->textureID);
            if (texture) {
                renderer.drawSprite(
                    texture->id,
                    transform->position,
                    renderable->size * transform->scale,
                    renderable->spriteRect,
                    renderable->color,
                    transform->rotation,
                    renderable->flipX,
                    renderable->flipY
                );
            }
            else {
                // Fallback to primitive if texture not found
                renderer.drawRect(transform->position, renderable->size * transform->scale,
                    renderable->color, transform->rotation);
            }
        }
        else {
            // Render as primitive (fallback/testing)
            switch (renderable->shapeType) {
            case ShapeType::Rectangle:
                renderer.drawRect(transform->position, renderable->size * transform->scale,
                    renderable->color, transform->rotation);
                break;
            case ShapeType::Circle:
                renderer.drawCircle(transform->position, renderable->size.x * 0.5f * transform->scale.x,
                    renderable->color);
                break;
            default:
                break;
            }
        }
    }
}

// ============================================================================
// PLAYER AND AI SYSTEM
// ============================================================================

void Game::updateHealthSystem(float deltaTime) {
    auto entities = registry.getEntitiesWith<HealthComponent>();

    // Debug: Check if we're even getting entities
    static float debugTimer = 0.0f;
    debugTimer += deltaTime;
    if (debugTimer >= 2.0f) {
        std::cout << "[HEALTH SYSTEM] Total entities with health: " << entities.size() << std::endl;
        debugTimer = 0.0f;
    }

    for (EntityID entity : entities) {
        auto* health = registry.getComponent<HealthComponent>(entity);
        if (!health) continue;

        // ---- NEW: Health regeneration ----
        if (!health->isDead && health->currentHP < health->maxHP) {
            health->currentHP += health->hpRegen * deltaTime;
            if (health->currentHP > health->maxHP)
                health->currentHP = health->maxHP;
        }

        // Update invincibility timer
        if (health->invincibilityTime > 0) {
            health->invincibilityTime -= deltaTime;
        }

        // Debug player health every 2 seconds
        if (entity == playerEntity) {
            static float playerDebugTimer = 0.0f;
            playerDebugTimer += deltaTime;
            if (playerDebugTimer >= 2.0f) {
                std::cout << "[PLAYER HEALTH] Current: " << health->currentHP
                    << "/" << health->maxHP
                    << " | isDead: " << (health->isDead ? "YES" : "NO") << std::endl;
                playerDebugTimer = 0.0f;
            }

            // CRITICAL FIX: If player is dead but game state is not GameOver, force it
            auto& stateMgr = GameStateManager::GetInstance();
            if (health->isDead && stateMgr.GetState() != GameState::GameOver) {
                std::cout << "========================================" << std::endl;
                std::cout << "FIXING STUCK DEATH STATE!" << std::endl;
                std::cout << "  Player isDead: YES" << std::endl;
                std::cout << "  Current State: " << (int)stateMgr.GetState() << std::endl;
                std::cout << "  Forcing GameOver state..." << std::endl;
                std::cout << "========================================" << std::endl;

                stateMgr.SetPlayerDied(true);
                stateMgr.ResetRespawnTimer();
                stateMgr.SetState(GameState::GameOver);
                AudioManager::GetInstance().PlaySound("death", 80.0f);
                AudioManager::GetInstance().StopMusic();
            }
        }

        // NEW: Check for player death (first time only)
        if (entity == playerEntity && health->currentHP <= 0 && !health->isDead) {
            std::cout << "========================================" << std::endl;
            std::cout << "DEATH DETECTED!" << std::endl;
            std::cout << "  Entity ID: " << entity << std::endl;
            std::cout << "  Player Entity ID: " << playerEntity << std::endl;
            std::cout << "  Current HP: " << health->currentHP << std::endl;
            std::cout << "  Max HP: " << health->maxHP << std::endl;
            std::cout << "  isDead flag: " << (health->isDead ? "true" : "false") << std::endl;
            std::cout << "  Setting isDead = true..." << std::endl;
            std::cout << "========================================" << std::endl;

            health->isDead = true;
            handlePlayerDeath();
        }
        // Destroy dead entities (except player)
        else if (health->isDead && entity != playerEntity)
        {
            auto* tag = registry.getComponent<TagComponent>(entity);
            if (tag && tag->tag == "enemy")
            {
                //Give XP to player
                if (playerEntity != INVALID_ENTITY)
                {
                    auto* playerStats = registry.getComponent<PlayerStatsComponent>(playerEntity);
                    auto* enemyStats = registry.getComponent<EnemyStatsComponent>(entity);

                    if (playerStats && enemyStats)
                    {
                        int xpGained = enemyStats->scaledExpReward;
                        playerStats->addExperience(xpGained);

                        std::cout << "Gained: " << xpGained << "XP! (" << playerStats->experience << "/"
                            << playerStats->experienceToNextLevel << ")" << std::endl;

                        // Update health if leveled up
                        updatePlayerHealthFromStats();
                        updatePlayerManaFromStats();
                        updatePlayerManaFromStats();
                    }
                }

                // Drop loot
                auto* transform = registry.getComponent<TransformComponent>(entity);
                if (transform) {
                    // Generate loot from loot table
                    auto loot = ResourceManager::getInstance().generateLoot("goblin_drops");

                    // Add items to player inventory
                    auto* playerInventory = registry.getComponent<InventoryComponent>(playerEntity);
                    if (playerInventory) {
                        for (const auto& itemID : loot) {
                            if (playerInventory->addItem(itemID, 1)) {
                                const Item* item = ResourceManager::getInstance().getItem(itemID);
                                std::cout << "✨ Enemy dropped: " << (item ? item->name : itemID) << std::endl;

                                // Play pickup sound
                                AudioManager::GetInstance().PlaySound("pickup", 80.0f);
                            }
                            else {
                                std::cout << "Inventory full! Couldn't pick up: " << itemID << std::endl;
                            }
                        }
                    }
                }
            }
            destroyEntity(entity);
        }
    }
}

void Game::updatePlayerController(float deltaTime) {
    UNUSED(deltaTime);
    // Additional player-specific logic can be added here
    // For example: ability cooldowns, stamina management, etc.
}

void Game::updateAISystem(float deltaTime) {
    auto entities = registry.getEntitiesWith<TransformComponent, AIStateComponent, PhysicsComponent>();

    for (EntityID entity : entities) {
        auto* transform = registry.getComponent<TransformComponent>(entity);
        auto* aiState = registry.getComponent<AIStateComponent>(entity);
        auto* physics = registry.getComponent<PhysicsComponent>(entity);

        if (!transform || !aiState || !physics) continue;

        aiState->stateTimer += deltaTime;

        // Simple patrol behavior
        if (aiState->currentBehavior == AIBehavior::Patrol && !aiState->patrolPoints.empty()) {
            glm::vec2 target = aiState->patrolPoints[aiState->currentPatrolIndex];
            glm::vec2 direction = target - transform->position;
            float distance = glm::length(direction);

            if (distance < 10.0f) {
                // Reached patrol point, move to next
                aiState->currentPatrolIndex = (aiState->currentPatrolIndex + 1) % aiState->patrolPoints.size();
            }
            else {
                // Move towards patrol point
                direction = glm::normalize(direction);
                physics->acceleration = direction * 20000.0f; // Much faster movement
            }
        }

        // Check for player in sight range (simple chase behavior)
        if (registry.isValid(playerEntity)) {
            auto* playerTransform = registry.getComponent<TransformComponent>(playerEntity);
            if (playerTransform) {
                float distToPlayer = glm::length(playerTransform->position - transform->position);

                if (distToPlayer < aiState->sightRange) {
                    // Switch to chase behavior
                    aiState->currentBehavior = AIBehavior::Chase;
                    aiState->targetEntity = playerEntity;

                    glm::vec2 direction = playerTransform->position - transform->position;
                    float horizontalDist = fabs(direction.x);
                    float verticalDist = direction.y;

                    if (glm::length(direction) > 0) {
                        // Horizontal movement
                        glm::vec2 horizontalDir = glm::normalize(glm::vec2(direction.x, 0));
                        physics->acceleration.x = horizontalDir.x * 25000.0f; // 85-90% of player speed

                        // Jump if player is above or obstacle in way
                        if ((verticalDist > 30.0f || horizontalDist < 50.0f) &&
                            fabs(physics->velocity.y) < 10.0f &&
                            aiState->stateTimer > 0.5f) {
                            physics->velocity.y = 450.0f; // Jump
                            aiState->stateTimer = 0.0f; // Cooldown
                        }
                    }

                    // Attack if in range
                    if (distToPlayer < aiState->attackRange) {
                        aiState->currentBehavior = AIBehavior::Attack;
                        auto* playerHealth = registry.getComponent<HealthComponent>(playerEntity);
                        auto* enemyStats = registry.getComponent<EnemyStatsComponent>(entity);

                        if (playerHealth && aiState->stateTimer > 1.0f) {
                            // Use SCALED damage (not hardcoded 10.0!)
                            float damageToApply = enemyStats ? enemyStats->scaledDamage : 10.0f;

                            std::cout << "========================================" << std::endl;
                            std::cout << "?? ENEMY ATTACKING PLAYER!" << std::endl;
                            std::cout << "  Enemy Level: " << (enemyStats ? enemyStats->baseLevel : 1) << std::endl;
                            std::cout << "  Player HP BEFORE: " << playerHealth->currentHP << "/" << playerHealth->maxHP << std::endl;
                            std::cout << "  Damage: " << damageToApply << " (SCALED!)" << std::endl;
                            std::cout << "  Invincibility: " << playerHealth->invincibilityTime << std::endl;

                            playerHealth->damage(damageToApply);
                            createDamageText(damageToApply, playerTransform->position, true, false);

                            std::cout << "  Player HP AFTER: " << playerHealth->currentHP << "/" << playerHealth->maxHP << std::endl;
                            std::cout << "========================================" << std::endl;

                            aiState->stateTimer = 0.0f;
                            AudioManager::GetInstance().PlaySound("hit", 80.0f);
                        }
                    }
                }
                else if (aiState->currentBehavior == AIBehavior::Chase) {
                    // Lost sight, return to patrol
                    aiState->currentBehavior = AIBehavior::Patrol;
                    aiState->targetEntity = INVALID_ENTITY;
                }
            }
        }
    }
}

// ============================================================================
// ENTITY SPAWN AND DESTROY SYSTEM
// ============================================================================

EntityID Game::spawnEntity(const std::string& templateID, const glm::vec2& position) {
    const EntityTemplate* templ = ResourceManager::getInstance().getEntityTemplate(templateID);
    if (!templ) {
        std::cerr << "Entity template not found: " << templateID << std::endl;
        return INVALID_ENTITY;
    }

    EntityID entity = registry.createEntity();

    // Load all template components FIRST
    for (const auto& [componentName, componentData] : templ->components) {
        if (componentName == "transform") {
            TransformComponent comp;
            comp.fromJson(componentData);
            comp.position = position; // Override position
            registry.addComponent(entity, comp);
        }
        else if (componentName == "renderable") {
            RenderableComponent comp;
            comp.fromJson(componentData);
            registry.addComponent(entity, comp);
        }
        else if (componentName == "collider") {
            ColliderComponent comp;
            comp.fromJson(componentData);
            registry.addComponent(entity, comp);
        }
        else if (componentName == "physics") {
            PhysicsComponent comp;
            comp.fromJson(componentData);
            registry.addComponent(entity, comp);
        }
        else if (componentName == "health") {
            HealthComponent comp;
            comp.fromJson(componentData);
            registry.addComponent(entity, comp);
        }
        else if (componentName == "inventory") {
            InventoryComponent comp;
            comp.fromJson(componentData);
            registry.addComponent(entity, comp);
        }
        else if (componentName == "aiState") {
            AIStateComponent comp;
            comp.fromJson(componentData);
            registry.addComponent(entity, comp);
        }
        else if (componentName == "tag") {
            TagComponent comp;
            comp.fromJson(componentData);
            registry.addComponent(entity, comp);
        }
        else if (componentName == "combat") {
            CombatComponent comp;
            comp.fromJson(componentData);
            registry.addComponent(entity, comp);
        }
        else if (componentName == "interactable") {
            InteractableComponent comp;
            comp.fromJson(componentData);
            registry.addComponent(entity, comp);
        }

    }

    // THEN scale enemy stats (so it overwrites template values)
    if (templateID.find("goblin") != std::string::npos || templateID.find("enemy") != std::string::npos)
    {
        //Add enemy stats
        EnemyStatsComponent enemyStats;

        //Get player level
        int playerLevel = 1;
        if (playerEntity != INVALID_ENTITY)
        {
            auto* playerStats = registry.getComponent<PlayerStatsComponent>(playerEntity);
            if (playerStats)
            {
                playerLevel = playerStats->level;
            }
        }

        //scale enemy to player level
        enemyStats.scaleToPlayerLevel(playerLevel);
        registry.addComponent(entity, enemyStats);

        //Update enemy health to scaled amount (AFTER template loaded)
        auto* health = registry.getComponent<HealthComponent>(entity);
        if (health)
        {
            health->maxHP = enemyStats.scaledHealth;
            health->currentHP = enemyStats.scaledHealth;
        }

        //Update enemy damage
        auto* combat = registry.getComponent<CombatComponent>(entity);
        if (combat)
        {
            combat->attackDamage = enemyStats.scaledDamage;
        }
    }

    //Add player stats component
    if (templateID == "player")
    {
        PlayerStatsComponent stats;
        stats.calculateDerivedStats();
        registry.addComponent(entity, stats);

        std::cout << "Player stats initialized at level " << stats.level << std::endl;

        // Initialize magic component
        PlayerMagicComponent magic;
        magic.currentMana = 100.0f;
        magic.maxMana = 100.0f;
        magic.manaRegen = 5.0f;
        registry.addComponent(entity, magic);

        std::cout << "Player magic initialized" << std::endl;

        // Initialize equipment stats component
        EquipmentStatsComponent equipment;
        equipment.recalculateBonuses();
        registry.addComponent(entity, equipment);

        std::cout << "Player equipment system initialized" << std::endl;
    }

    return entity;
}

void Game::removeEntity(EntityID entity)
{
    //removing a component from entity
    //registry.removeComponent<PhysicsComponent>(entity);

    //checking if entity has component
   /*if (registry.hasComponent<PhysicsComponent>(entity))
    {
        auto* physics = registry.getComponent<PhysicsComponent>(entity);
    }*/
}

void Game::destroyEntity(EntityID entity) {
    
    if (entity == playerEntity) {
        playerEntity = INVALID_ENTITY;
    }
    registry.destroyEntity(entity);
}

// ============================================================================
// INVENTORY SYSTEM
// ============================================================================

void Game::renderInventoryUI()
{
    if (!showInventory) return;

    ImGui::SetNextWindowPos(ImVec2(100, 100));
    ImGui::SetNextWindowSize(ImVec2(800, 600));
    ImGui::Begin("Inventory", &showInventory, ImGuiWindowFlags_NoResize);

    auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);
    auto* magic = registry.getComponent<PlayerMagicComponent>(playerEntity);

    if (!inventory) {
        ImGui::Text("No inventory component!");
        ImGui::End();
        return;
    }

    // TAB BAR
    if (ImGui::BeginTabBar("InventoryTabs")) {

        // TAB 1: ITEMS
        if (ImGui::BeginTabItem("Items")) {
            renderItemsTab(inventory);
            ImGui::EndTabItem();
        }

        // TAB 2: EQUIPMENT
        if (ImGui::BeginTabItem("Equipment")) {
            renderEquipmentTab(inventory);
            ImGui::EndTabItem();
        }

        // TAB 3: SKILLS (only if magic component exists)
        if (magic && ImGui::BeginTabItem("Skills")) {
            renderSkillsTab(magic);
            ImGui::EndTabItem();
        }

        // TAB 4: MAGIC (only if magic component exists)
        if (magic && ImGui::BeginTabItem("Magic")) {
            renderMagicTab(magic);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void Game::useItem(const std::string& itemID) {
    auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);
    auto* health = registry.getComponent<HealthComponent>(playerEntity);
    auto* magic = registry.getComponent<PlayerMagicComponent>(playerEntity);

    if (!inventory) return;

    std::cout << "Using item: " << itemID << std::endl;

    if (itemID == "magic_scroll")
    {
        if (!magic)
        {
            std::cout << "No magic component found!" << std::endl;
            return;
        }

        // Get all skill scrolls (already loaded from items.json)
        auto scrolls = GetAllScrolls();

        // Filter out scrolls for skills the player already has
        std::vector<MagicScrollItem> availableScrolls;
        for (const auto& scroll : scrolls)
        {
            if (!magic->hasSkill(scroll.skillType))
            {
                availableScrolls.push_back(scroll);
            }
        }

        if (availableScrolls.empty())
        {
            std::cout << "You already know all magic skills!" << std::endl;
            AudioManager::GetInstance().PlaySound("click", 70.0f);
            return;
        }

        // Weighted random selection based on rarity
        static std::random_device rd;
        static std::mt19937 gen(rd());

        // Calculate total weight (sum of rarities)
        float totalWeight = 0.0f;
        for (const auto& scroll : availableScrolls)
        {
            totalWeight += scroll.dropChance;   // this is the rarity value
        }

        // Choose a scroll
        std::uniform_real_distribution<float> dist(0.0f, totalWeight);
        float roll = dist(gen);
        float cumulative = 0.0f;
        int selectedIndex = 0;
        for (size_t i = 0; i < availableScrolls.size(); ++i)
        {
            cumulative += availableScrolls[i].dropChance;
            if (roll <= cumulative)
            {
                selectedIndex = i;
                break;
            }
        }

        const MagicScrollItem& selected = availableScrolls[selectedIndex];
        std::string skillScrollID = selected.getItemID();  // returns e.g., "scroll_lightning"

        // Add the skill scroll to inventory
        if (inventory->addItem(skillScrollID, 1))
        {
            // Remove the consumed magic_scroll
            inventory->removeItem(itemID, 1);
            std::cout << "You received: " << selected.scrollName << "!" << std::endl;
            AudioManager::GetInstance().PlaySound("click", 100.0f);
        }
        else
        {
            std::cout << "Inventory is full! Cannot receive the scroll." << std::endl;
        }
        return;
    }

    if (itemID.find("scroll_") == 0) {
        useMagicScroll(itemID);
        inventory->removeItem(itemID, 1);
        return;
    }

    // Handle consumables
    if (itemID == "health_potion" || itemID == "potion") {
        if (health) {
            health->heal(50.0f);
            inventory->removeItem(itemID, 1);
            std::cout << "Healed 50 HP!" << std::endl;
            AudioManager::GetInstance().PlaySound("click", 70.0f);
        }
    }
    else if (itemID == "mana_potion") {
        if (magic) {
            magic->currentMana += 50.0f;
            if (magic->currentMana > magic->maxMana) {
                magic->currentMana = magic->maxMana;
            }
            inventory->removeItem(itemID, 1);
            std::cout << "Restored 50 mana!" << std::endl;
            AudioManager::GetInstance().PlaySound("click", 70.0f);
        }
    }
    else {
        std::cout << "Unknown item type: " << itemID << std::endl;
    }
}

void Game::renderItemsTab(InventoryComponent* inventory)
{
    ImGui::Text("Consumables & Materials");
    ImGui::Separator();

    const int columns = 8;
    const ImVec2 buttonSize(64, 64);

    for (int i = 0; i < inventory->capacity; ++i)
    {
        auto& slot = inventory->slots[i];

        ImGui::PushID(i);

        if (!slot.isEmpty())
        {
            // Get the texture for this item using its ID
            const Texture* tex = ResourceManager::getInstance().getTexture(slot.itemID);

            if (tex && tex->id != 0)
            {
                // Draw the item icon as an image button
                // Provide a unique ID (itemID) and the texture handle
                ImGui::ImageButton(slot.itemID.c_str(),
                    (ImTextureID)(intptr_t)tex->id,
                    buttonSize);
            }
            else
            {
                // Fallback: text button
                ImGui::Button(slot.itemID.c_str(), buttonSize);
            }

            // Tooltip (show item name and quantity)
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s x%d", slot.itemID.c_str(), slot.quantity);
            }

            // Right‑click context menu
            if (ImGui::IsItemClicked(1))
            {
                ImGui::OpenPopup("item_context");
            }

            if (ImGui::BeginPopup("item_context"))
            {
                if (ImGui::MenuItem("Use"))
                {
                    useItem(slot.itemID);
                }
                if (ImGui::MenuItem("Drop"))
                {
                    inventory->removeItem(slot.itemID, 1);
                }
                ImGui::EndPopup();
            }
        }
        else
        {
            // Empty slot placeholder
            ImGui::Button("Empty", buttonSize);
        }

        ImGui::PopID();

        // Layout: next column or new line
        if ((i + 1) % columns != 0)
            ImGui::SameLine();
    }
}

void Game::renderEquipmentTab(InventoryComponent* inventory)
{
    ImGui::Text("Equipment");
    ImGui::Separator();

    // Left: Character paper doll
    ImGui::BeginChild("paperdoll", ImVec2(200, 500), true);
    ImGui::Text("Equipped:");
    ImGui::Separator();

    // Weapon slot
    ImGui::Text("Weapon:");
    if (!inventory->equippedWeapon.empty()) {
        if (ImGui::Button(inventory->equippedWeapon.c_str(), ImVec2(-1, 50))) {
            // Left click unequips
            inventory->equippedWeapon = "";
            updatePlayerStatsFromEquipment();
        }
        // Right click shows details
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            selectedEquipmentID = inventory->equippedWeapon;
            showEquipmentDetails = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Left-click: Unequip | Right-click: Details");
        }
    }
    else {
        ImGui::Button("[ Empty ]", ImVec2(-1, 50));
    }

    // Armor slot
    ImGui::Text("Armor:");
    if (!inventory->equippedArmor.empty()) {
        if (ImGui::Button(inventory->equippedArmor.c_str(), ImVec2(-1, 50))) {
            // Left click unequips
            inventory->equippedArmor = "";
            updatePlayerStatsFromEquipment();
        }
        // Right click shows details
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            selectedEquipmentID = inventory->equippedArmor;
            showEquipmentDetails = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Left-click: Unequip | Right-click: Details");
        }
    }
    else {
        ImGui::Button("[ Empty ]", ImVec2(-1, 50));
    }

    // Accessory slots
    ImGui::Text("Accessory 1:");
    if (!inventory->equippedAccessory1.empty()) {
        if (ImGui::Button(inventory->equippedAccessory1.c_str(), ImVec2(-1, 50))) {
            // Left click unequips
            inventory->equippedAccessory1 = "";
            updatePlayerStatsFromEquipment();
        }
        // Right click shows details
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            selectedEquipmentID = inventory->equippedAccessory1;
            showEquipmentDetails = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Left-click: Unequip | Right-click: Details");
        }
    }
    else {
        ImGui::Button("[ Empty ]", ImVec2(-1, 50));
    }

    ImGui::Text("Accessory 2:");
    if (!inventory->equippedAccessory2.empty()) {
        if (ImGui::Button(inventory->equippedAccessory2.c_str(), ImVec2(-1, 50))) {
            // Left click unequips
            inventory->equippedAccessory2 = "";
            updatePlayerStatsFromEquipment();
        }
        // Right click shows details
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            selectedEquipmentID = inventory->equippedAccessory2;
            showEquipmentDetails = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Left-click: Unequip | Right-click: Details");
        }
    }
    else {
        ImGui::Button("[ Empty ]", ImVec2(-1, 50));
    }

    ImGui::EndChild();

    // Right: Equipment inventory
    ImGui::SameLine();
    ImGui::BeginChild("equipment_inv", ImVec2(550, 500), true);
    ImGui::Text("Equipment Inventory:");
    ImGui::Separator();

    // Show equipment items from inventory
    for (int i = 0; i < inventory->capacity; ++i) {
        auto& slot = inventory->slots[i];
        if (slot.isEmpty()) continue;

        // Check if it's equipment
        if (slot.itemID.find("sword") != std::string::npos ||
            slot.itemID.find("armor") != std::string::npos ||
            slot.itemID.find("ring") != std::string::npos ||
            slot.itemID.find("amulet") != std::string::npos) {

            ImGui::PushID(i);
            if (ImGui::Button(slot.itemID.c_str(), ImVec2(150, 40))) {
                // Left click to equip
                inventory->equipItem(slot.itemID);
                updatePlayerStatsFromEquipment();
            }

            // Right click to view details
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                selectedEquipmentID = slot.itemID;
                showEquipmentDetails = true;
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Left-click: Equip | Right-click: Details");
            }
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
}

void Game::renderSkillsTab(PlayerMagicComponent* magic)
{
    ImGui::Text("Magic Skills");
    ImGui::Separator();

    ImGui::Text("Unlocked Skills:");
    ImGui::Separator();

    // Show all unlocked skills
    for (auto skillType : magic->unlockedSkills) {
        MagicSkillData data = GetSkillData(skillType);

        ImGui::PushID((int)skillType);

        if (ImGui::Button(data.name.c_str(), ImVec2(200, 50))) {
            // Equip to first empty slot
            for (int i = 0; i < 4; ++i) {
                if (magic->equippedSkills[i] == MagicSkillType::None) {
                    magic->equippedSkills[i] = skillType;
                    std::cout << "Equipped " << data.name << " to slot " << (i + 1) << std::endl;
                    break;
                }
            }
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", data.description.c_str());
            ImGui::Separator();
            ImGui::Text("Damage: %.0f", data.baseDamage);
            ImGui::Text("Mana Cost: %.0f", data.manaCost);
            ImGui::Text("Cooldown: %.1fs", data.cooldown);
            ImGui::Text("Radius: %.0f", data.radius);
            ImGui::EndTooltip();
        }

        ImGui::PopID();
    }

    if (magic->unlockedSkills.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "No skills unlocked yet!");
        ImGui::Text("Find magic scrolls by defeating enemies");
    }
}

void Game::renderMagicTab(PlayerMagicComponent* magic)
{
    ImGui::Text("Equipped Magic");
    ImGui::Separator();

    // Mana bar
    ImGui::Text("Mana: %.0f / %.0f", magic->currentMana, magic->maxMana);
    float manaPercent = magic->currentMana / magic->maxMana;
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.5f, 1.0f, 1.0f));
    ImGui::ProgressBar(manaPercent, ImVec2(-1, 30));
    ImGui::PopStyleColor();

    ImGui::Separator();

    // Equipped skills grid
    const char* slotKeys[] = { "1", "2", "3", "4" };

    for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);

        ImGui::Text("Slot %d [%s]:", i + 1, slotKeys[i]);

        if (magic->equippedSkills[i] != MagicSkillType::None) {
            MagicSkillData data = GetSkillData(magic->equippedSkills[i]);

            // Skill button
            bool onCooldown = magic->skillCooldowns[i] > 0.0f;
            if (onCooldown) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            }

            if (ImGui::Button(data.name.c_str(), ImVec2(250, 60))) {
                if (!onCooldown) {
                    // Unequip
                    magic->equippedSkills[i] = MagicSkillType::None;
                }
            }

            if (onCooldown) {
                ImGui::PopStyleColor();
            }

            // Cooldown bar
            if (onCooldown) {
                float cooldownPercent = 1.0f - (magic->skillCooldowns[i] / data.cooldown);
                ImGui::ProgressBar(cooldownPercent, ImVec2(250, 10));
            }

            // Info
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::Text("Damage: %.0f", data.baseDamage);
            ImGui::Text("Mana: %.0f", data.manaCost);
            ImGui::Text("Cooldown: %.1fs", data.cooldown);
            ImGui::EndGroup();

        }
        else {
            ImGui::Button("[ Empty ]", ImVec2(250, 60));
            ImGui::SameLine();
            ImGui::Text("No skill equipped");
        }

        ImGui::PopID();
        ImGui::Separator();
    }
}

// ============================================================================
// ANIMATION SYSTEM - TO BE WORKED ON AFTER
// ============================================================================

void Game::renderAnimationEditor() {
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Animation Editor", &showAnimationEditor);

    auto& resourceMgr = ResourceManager::getInstance();
    auto entities = registry.getEntitiesWith<AnimationComponent, RenderableComponent>();

    ImGui::Text("=== ANIMATION EDITOR ===");
    ImGui::Text("Edit animations in real-time");
    ImGui::Separator();

    if (entities.empty()) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No entities with animations found");
        ImGui::Text("Animations require both AnimationComponent and RenderableComponent");
        ImGui::End();
        return;
    }

    // Entity selector
    static int selectedEntityIndex = 0;
    static EntityID selectedEntity = entities[0];

    if (ImGui::BeginCombo("Entity", "Select Entity")) {
        for (size_t i = 0; i < entities.size(); ++i) {
            auto* tag = registry.getComponent<TagComponent>(entities[i]);
            std::string label = tag ? tag->tag : ("Entity " + std::to_string(entities[i]));

            if (ImGui::Selectable(label.c_str(), selectedEntityIndex == (int)i)) {
                selectedEntityIndex = i;
                selectedEntity = entities[i];
            }
        }
        ImGui::EndCombo();
    }

    auto* anim = registry.getComponent<AnimationComponent>(selectedEntity);
    auto* renderable = registry.getComponent<RenderableComponent>(selectedEntity);

    if (!anim || !renderable) {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::Text("=== ANIMATION PROPERTIES ===");

    // Current animation display
    ImGui::Text("Current Animation: %s", anim->currentAnimation.c_str());
    ImGui::Text("Frame: %d", anim->currentFrame);
    ImGui::ProgressBar(anim->frameTime / anim->frameDuration, ImVec2(-1, 0));

    ImGui::Separator();

    // Animation controls
    ImGui::Checkbox("Playing", &anim->playing);
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        anim->currentFrame = 0;
        anim->frameTime = 0.0f;
    }

    static float frameDurationBackup = anim->frameDuration;
    if (ImGui::DragFloat("Frame Duration", &anim->frameDuration, 0.01f, 0.01f, 5.0f)) {
        frameDurationBackup = anim->frameDuration;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##duration")) {
        anim->frameDuration = 0.1f;
    }

    ImGui::Checkbox("Loop", &anim->loop);

    ImGui::Separator();
    ImGui::Text("=== AVAILABLE ANIMATIONS ===");

    // List all animations in this component
    for (auto& [animName, frames] : anim->animations) {
        ImGui::PushID(animName.c_str());

        bool isCurrent = (anim->currentAnimation == animName);
        if (isCurrent) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        }

        if (ImGui::Button(animName.c_str(), ImVec2(200, 0))) {
            anim->play(animName);
        }

        if (isCurrent) {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        ImGui::Text("(%zu frames)", frames.size());

        // Show frame UVs
        if (ImGui::TreeNode("Frames")) {
            for (size_t i = 0; i < frames.size(); ++i) {
                ImGui::Text("Frame %zu: UV(%.2f, %.2f, %.2f, %.2f)",
                    i, frames[i].x, frames[i].y, frames[i].z, frames[i].w);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Text("=== SPRITE PROPERTIES ===");

    // Sprite rendering controls
    if (renderable->renderMode == RenderMode::Sprite) {
        ImGui::Text("Texture ID: %s", renderable->textureID.c_str());

        const Texture* tex = resourceMgr.getTexture(renderable->textureID);
        if (tex) {
            ImGui::Text("Texture Size: %dx%d", tex->width, tex->height);

            // Preview current frame
            if (tex->id > 0) {
                ImGui::Text("Current Frame Preview:");
                ImVec2 previewSize(128, 128);
                ImVec2 uvMin(renderable->spriteRect.x, renderable->spriteRect.y);
                ImVec2 uvMax(renderable->spriteRect.x + renderable->spriteRect.z,
                    renderable->spriteRect.y + renderable->spriteRect.w);

                ImGui::Image((void*)(intptr_t)tex->id, previewSize, uvMin, uvMax);
            }
        }

        ImGui::Checkbox("Flip X", &renderable->flipX);
        ImGui::Checkbox("Flip Y", &renderable->flipY);

        ImVec4 colorEdit(renderable->color.r, renderable->color.g, renderable->color.b, renderable->color.a);
        if (ImGui::ColorEdit4("Tint Color", &colorEdit.x)) {
            renderable->color = Color(colorEdit.x, colorEdit.y, colorEdit.z, colorEdit.w);
        }
    }
    else {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Entity is in Primitive render mode");
        ImGui::Text("Switch to Sprite mode (F3) to see animation");
    }

    ImGui::Separator();

    if (ImGui::Button("Close Editor")) {
        showAnimationEditor = false;
    }

    ImGui::End();
}

// ============================================================================
// ATTACK SYSTEM
// ============================================================================

void Game::performAttack(EntityID attacker) {
    auto* attackerTransform = registry.getComponent<TransformComponent>(attacker);
    auto* attackerCombat = registry.getComponent<CombatComponent>(attacker);

    if (!attackerTransform || !attackerCombat) return;

    // Find enemies in range
    auto entities = registry.getEntitiesWith<TransformComponent, HealthComponent>();

    for (EntityID target : entities) {
        if (target == attacker) continue;

        auto* targetTransform = registry.getComponent<TransformComponent>(target);
        auto* targetHealth = registry.getComponent<HealthComponent>(target);
        auto* targetTag = registry.getComponent<TagComponent>(target);

        if (!targetTransform || !targetHealth) continue;

        // Check if target is an enemy
        if (targetTag && targetTag->tag == "enemy") {
            float distance = glm::length(targetTransform->position - attackerTransform->position);

            if (distance <= attackerCombat->attackRange) {
                targetHealth->damage(attackerCombat->attackDamage);
                std::cout << "Player attacked enemy! Damage: " << attackerCombat->attackDamage << std::endl;
            }
        }
    }
}

//MAGIC MISSILE - EXISTS/ACTS AS AN EXAMPLE OF DIFFERENT ATTACK TYPES
void Game::shootMagicMissile(EntityID shooter) {
    auto* transform = registry.getComponent<TransformComponent>(shooter);
    auto* combat = registry.getComponent<CombatComponent>(shooter);

    if (!transform || !combat) return;

    // Calculate direction towards mouse cursor
    glm::vec2 toMouse = mouseWorldPos - transform->position;
    float distance = glm::length(toMouse);

    glm::vec2 direction;
    if (distance > 0.1f) {
        direction = glm::normalize(toMouse);
    }
    else {
        // If mouse is on player, shoot right by default
        direction = glm::vec2(1.0f, 0.0f);
    }

    // Spawn offset in the direction of shooting
    glm::vec2 spawnPos = transform->position + direction * 25.0f;

    createMagicMissile(spawnPos, direction, shooter, combat->attackDamage);
    std::cout << "Magic missile shot towards cursor!" << std::endl;
}

EntityID Game::createMagicMissile(const glm::vec2& position, const glm::vec2& direction, EntityID owner, float damage) {
    EntityID missile = registry.createEntity();

    // Transform
    TransformComponent transform;
    transform.position = position;
    transform.scale = glm::vec2(0.5f, 0.5f);
    registry.addComponent(missile, transform);

    // Renderable - bright cyan/blue magic effect
    RenderableComponent renderable;
    renderable.color = Color(0.3f, 0.8f, 1.0f, 1.0f);
    renderable.shapeType = ShapeType::Circle;
    renderable.size = glm::vec2(12.0f, 12.0f);
    renderable.layer = 15;
    registry.addComponent(missile, renderable);

    // Physics - fast moving projectile
    PhysicsComponent physics;
    physics.velocity = glm::normalize(direction) * 600.0f;
    physics.applyGravity = false;
    physics.friction = 1.0f; // No friction
    registry.addComponent(missile, physics);

    // Collider - small trigger
    ColliderComponent collider;
    collider.size = glm::vec2(12.0f, 12.0f);
    collider.isTrigger = true;
    collider.collisionLayer = 8;
    registry.addComponent(missile, collider);

    // Projectile
    ProjectileComponent projectile;
    projectile.owner = owner;
    projectile.damage = damage;
    projectile.direction = direction;
    projectile.lifetime = 3.0f;
    registry.addComponent(missile, projectile);

    // Tag
    TagComponent tag;
    tag.tag = "projectile";
    registry.addComponent(missile, tag);

    return missile;
}

void Game::updateProjectileSystem(float deltaTime) {
    auto projectiles = registry.getEntitiesWith<ProjectileComponent, TransformComponent>();

    for (EntityID projEntity : projectiles) {
        auto* proj = registry.getComponent<ProjectileComponent>(projEntity);
        auto* projTransform = registry.getComponent<TransformComponent>(projEntity);
        auto* projCollider = registry.getComponent<ColliderComponent>(projEntity);

        if (!proj || !projTransform) continue;

        // Update lifetime
        proj->timeAlive += deltaTime;
        if (proj->timeAlive >= proj->lifetime) {
            destroyEntity(projEntity);
            continue;
        }

        // Check collision with enemies
        if (projCollider) {
            Rect projBounds = projCollider->getBounds(projTransform->position);

            auto entities = registry.getEntitiesWith<TransformComponent, ColliderComponent, HealthComponent>();
            for (EntityID target : entities) {
                if (target == proj->owner) continue;

                auto* targetTransform = registry.getComponent<TransformComponent>(target);
                auto* targetCollider = registry.getComponent<ColliderComponent>(target);
                auto* targetHealth = registry.getComponent<HealthComponent>(target);
                auto* targetTag = registry.getComponent<TagComponent>(target);

                if (!targetTransform || !targetCollider || !targetHealth) continue;

                // Only damage enemies
                if (targetTag && targetTag->tag == "enemy") {
                    Rect targetBounds = targetCollider->getBounds(targetTransform->position);

                    if (projBounds.intersects(targetBounds)) {
                        targetHealth->damage(proj->damage);
                        std::cout << "Magic missile hit! Damage: " << proj->damage << std::endl;
                        destroyEntity(projEntity);
                        break;
                    }
                }
            }
        }

        // Check collision with tiles
        if (worldSystem && worldSystem->isSolidAt(projTransform->position)) {
            destroyEntity(projEntity);
        }
    }
}

// ============================================================================
// LASER BEAM SYSTEM
// ============================================================================

// Helper function for line-rect intersection
bool lineIntersectsRect(const glm::vec2& lineStart, const glm::vec2& lineEnd, const Rect& rect) {
    // Simple AABB check - sample points along line
    glm::vec2 lineDir = lineEnd - lineStart;
    float lineLength = glm::length(lineDir);

    if (lineLength < 0.01f) return false;

    lineDir = glm::normalize(lineDir);

    // Sample points along line
    int samples = 20;
    for (int i = 0; i <= samples; ++i) {
        float t = (float)i / samples;
        glm::vec2 point = lineStart + lineDir * (lineLength * t);

        if (point.x >= rect.x && point.x <= rect.x + rect.width &&
            point.y >= rect.y && point.y <= rect.y + rect.height) {
            return true;
        }
    }

    return false;
}

EntityID Game::createLaserBeam(const glm::vec2& startPos, const glm::vec2& direction, EntityID owner) {
    EntityID beam = registry.createEntity();

    LaserBeamComponent laser;
    laser.startPos = startPos;
    laser.direction = glm::normalize(direction);
    laser.owner = owner;

    // Raycast to find end position (max 800 units)
    float maxDistance = 800.0f;
    laser.endPos = startPos + laser.direction * maxDistance;

    if (worldSystem) {
        float stepSize = 16.0f;
        for (float dist = 0; dist < maxDistance; dist += stepSize) {
            glm::vec2 testPos = startPos + laser.direction * dist;
            if (worldSystem->isSolidAt(testPos)) {
                laser.endPos = testPos;
                break;
            }
        }
    }

    // Get player stats and set damage BEFORE adding component
    auto* stats = registry.getComponent<PlayerStatsComponent>(owner);
    if (stats) {
        laser.damage = stats->physicalDamage;
        float critRoll = (float)(rand() % 100);
        if (critRoll < stats->critChance) {
            laser.damage *= (stats->critDamage / 100.0f);
            laser.isCritical = true;
        }
        std::cout << "Laser beam created! Damage: " << laser.damage
            << " (Physical: " << stats->physicalDamage << ")" << std::endl;
    }
    else {
        std::cout << "WARNING: Player has no stats, using default damage 25" << std::endl;
        // laser.damage stays at default 25 (or you could set it explicitly)
    }

    // Now add the fully configured component
    registry.addComponent(beam, laser);

    TagComponent tag;
    tag.tag = "laser";
    registry.addComponent(beam, tag);

    return beam;
}

void Game::shootLaserBeam() {
    if (playerEntity == INVALID_ENTITY) return;

    auto* cooldown = registry.getComponent<LaserCooldownComponent>(playerEntity);
    if (!cooldown) {
        // Add cooldown component if missing
        LaserCooldownComponent newCooldown;
        registry.addComponent(playerEntity, newCooldown);
        cooldown = registry.getComponent<LaserCooldownComponent>(playerEntity);
    }

    if (!cooldown->canFire) {
        std::cout << "Laser on cooldown: " << cooldown->cooldownTimer << "s" << std::endl;
        return;
    }

    auto* transform = registry.getComponent<TransformComponent>(playerEntity);
    auto* combat = registry.getComponent<CombatComponent>(playerEntity);

    if (!transform || !combat) return;

    // Get direction to mouse
    glm::vec2 direction = mouseWorldPos - transform->position;

    if (glm::length(direction) < 10.0f) {
        // Too close, shoot right
        direction = glm::vec2(1, 0);
    }

    createLaserBeam(transform->position, direction, playerEntity);

    // Start cooldown
    cooldown->canFire = false;
    cooldown->cooldownTimer = cooldown->cooldownDuration;

    // Play laser sound
    AudioManager::GetInstance().PlaySound("hit", 90.0f);

    std::cout << "Laser fired! Cooldown: " << cooldown->cooldownDuration << "s" << std::endl;
}

void Game::updateLaserSystem(float deltaTime) {

    // Update cooldowns
    auto players = registry.getEntitiesWith<LaserCooldownComponent>();
    for (EntityID entity : players) {
        auto* cooldown = registry.getComponent<LaserCooldownComponent>(entity);
        if (!cooldown) continue;

        if (!cooldown->canFire) {
            cooldown->cooldownTimer -= deltaTime;
            if (cooldown->cooldownTimer <= 0.0f) {
                cooldown->canFire = true;
                cooldown->cooldownTimer = 0.0f;
            }
        }
    }

    // Update laser beams
    auto beams = registry.getEntitiesWith<LaserBeamComponent>();
    for (EntityID beamEntity : beams) {
        auto* laser = registry.getComponent<LaserBeamComponent>(beamEntity);
        if (!laser) continue;

        // Update lifetime
        laser->lifetime += deltaTime;
        if (laser->lifetime >= laser->maxLifetime) {
            destroyEntity(beamEntity);
            continue;
        }

        // Check for enemy hits
        auto entities = registry.getEntitiesWith<TransformComponent, ColliderComponent, HealthComponent>();
        for (EntityID target : entities) {
            if (target == laser->owner) continue;

            // Skip already hit entities
            if (std::find(laser->hitEntities.begin(), laser->hitEntities.end(), target)
                != laser->hitEntities.end()) {
                continue;
            }

            auto* targetTransform = registry.getComponent<TransformComponent>(target);
            auto* targetCollider = registry.getComponent<ColliderComponent>(target);
            auto* targetHealth = registry.getComponent<HealthComponent>(target);
            auto* targetTag = registry.getComponent<TagComponent>(target);

            if (!targetTransform || !targetCollider || !targetHealth) continue;

            // Only damage enemies
            if (targetTag && targetTag->tag == "enemy") {
                // Check if laser line intersects enemy bounds
                Rect enemyBounds = targetCollider->getBounds(targetTransform->position);

                if (lineIntersectsRect(laser->startPos, laser->endPos, enemyBounds)) {

                    targetHealth->damage(laser->damage);
                    createDamageText(laser->damage, targetTransform->position, false, laser->isCritical);
                    laser->hitEntities.push_back(target);

                    std::cout << "Laser hit enemy! Damage: " << laser->damage << std::endl;
                }
            }
        }
    }
}

void Game::renderLaserBeams() {
    auto beams = registry.getEntitiesWith<LaserBeamComponent>();

    for (EntityID beamEntity : beams) {
        auto* laser = registry.getComponent<LaserBeamComponent>(beamEntity);
        if (!laser || !laser->active) continue;

        // Fade out over lifetime
        float fadeAlpha = 1.0f - (laser->lifetime / laser->maxLifetime);

        // Draw glow (wider, transparent)
        Color glowColor = laser->glowColor;
        glowColor.a *= fadeAlpha;
        renderer.drawLine(laser->startPos, laser->endPos, glowColor, laser->width * 3.0f);

        // Draw core (narrower, opaque)
        Color coreColor = laser->coreColor;
        coreColor.a *= fadeAlpha;
        renderer.drawLine(laser->startPos, laser->endPos, coreColor, laser->width);

        // Draw bright core (thinnest)
        renderer.drawLine(laser->startPos, laser->endPos,
            Color(1, 1, 1, fadeAlpha), laser->width * 0.3f);
    }
}

// ============================================================================
// HEALTH BARS AND INTERACTION SYSTEM
// ============================================================================

void Game::renderHealthBars() {
    auto entities = registry.getEntitiesWith<TransformComponent, HealthComponent>();

    for (EntityID entity : entities) {
        auto* transform = registry.getComponent<TransformComponent>(entity);
        auto* health = registry.getComponent<HealthComponent>(entity);

        if (!transform || !health) continue;

        // Don't show health bar if dead (except during respawn invincibility)
        if (health->isDead && health->invincibilityTime <= 0.0f) continue;

        // Don't render if health is at max (optional - shows for all entities)
        // if (health->currentHP >= health->maxHP) continue;

        // Health bar dimensions
        float barWidth = 40.0f;
        float barHeight = 4.0f;
        glm::vec2 barPos = transform->position + glm::vec2(0, 20);

        // Background (red)
        renderer.drawRect(barPos, glm::vec2(barWidth, barHeight), Color::Red());

        // Foreground (green) - ensure we don't divide by zero
        float healthPercent = (health->maxHP > 0) ? (health->currentHP / health->maxHP) : 1.0f;
        healthPercent = std::clamp(healthPercent, 0.0f, 1.0f); // Clamp to 0-1

        if (healthPercent > 0.0f) {
            renderer.drawRect(barPos - glm::vec2((barWidth - barWidth * healthPercent) * 0.5f, 0),
                glm::vec2(barWidth * healthPercent, barHeight),
                Color::Green());
        }
    }
}

void Game::renderInteractionPrompts() {
    auto interactables = registry.getEntitiesWith<TransformComponent, InteractableComponent>();

    for (EntityID entity : interactables) {
        auto* transform = registry.getComponent<TransformComponent>(entity);
        auto* interactable = registry.getComponent<InteractableComponent>(entity);

        if (!transform || !interactable || !interactable->showPrompt) continue;

        // Draw "Q" prompt above interactable
        glm::vec2 promptPos = transform->position + glm::vec2(0, 30);

        // Background circle
        renderer.drawCircle(promptPos, 12.0f, Color(0.2f, 0.2f, 0.2f, 0.8f));

        // "Q" indicator (just a small colored square for now)
        renderer.drawRect(promptPos, glm::vec2(8, 8), Color::Yellow());
    }
}

// ============================================================================
// IMGUISYSTEM
// ============================================================================

void Game::renderImGui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    auto& stateMgr = GameStateManager::GetInstance();

    // Game Over Screen UI
    if (stateMgr.GetState() == GameState::GameOver) {
        float menuX = (windowWidth - 500) / 2.0f;
        float menuY = (windowHeight - 350) / 2.0f;

        ImGui::SetNextWindowPos(ImVec2(menuX, menuY));
        ImGui::SetNextWindowSize(ImVec2(500, 350));
        ImGui::SetNextWindowBgAlpha(0.95f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.05f, 0.05f, 0.95f));
        ImGui::Begin("Game Over", nullptr,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar);

        // Title
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
        ImGui::SetWindowFontScale(2.5f);
        ImGui::SetCursorPosX((500 - ImGui::CalcTextSize("GAME OVER").x * 2.5f) / 2.0f);
        ImGui::Text("GAME OVER");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();

        float respawnTimer = stateMgr.GetRespawnTimer();

        if (respawnTimer > 0) {
            // Show countdown
            ImGui::SetWindowFontScale(1.5f);
            std::string timerText = "Respawning in " + std::to_string((int)std::ceil(respawnTimer)) + "s";
            ImGui::SetCursorPosX((500 - ImGui::CalcTextSize(timerText.c_str()).x * 1.5f) / 2.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", timerText.c_str());
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Spacing();

            // Progress bar
            float progress = 1.0f - (respawnTimer / 5.0f);
            ImGui::SetCursorPosX((500 - 400) / 2.0f);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
            ImGui::ProgressBar(progress, ImVec2(400, 30));
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Spacing();

            // Press R hint - EMPHASIZED
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f)); // Green
            ImGui::SetWindowFontScale(1.2f);
            ImGui::SetCursorPosX((500 - ImGui::CalcTextSize("Press R to Respawn NOW").x * 1.2f) / 2.0f);
            ImGui::Text("Press R to Respawn NOW");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Timer is just informational
            ImGui::SetCursorPosX((500 - ImGui::CalcTextSize("(Auto-respawn in 5s if you don't press R)").x) / 2.0f);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.8f), "(Auto-respawn in %ds if you don't press R)", (int)std::ceil(respawnTimer));
        }
        else {
            // Timer expired
            ImGui::SetWindowFontScale(1.5f);
            ImGui::SetCursorPosX((500 - ImGui::CalcTextSize("Press R to Respawn").x * 1.5f) / 2.0f);
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Press R to Respawn");
            ImGui::SetWindowFontScale(1.0f);
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Additional options
        ImGui::SetCursorPosX((500 - ImGui::CalcTextSize("ESC - Return to Menu").x) / 2.0f);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "ESC - Return to Menu");

        ImGui::PopStyleColor(); // Window bg
        ImGui::End();

        // Render and exit early
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return;
    }

    // Main Menu UI
    if (stateMgr.GetState() == GameState::MainMenu) {
        // Center the menu
        float menuX = (windowWidth - 480) / 2.0f;
        float menuY = (windowHeight - 500) / 2.0f;

        ImGui::SetNextWindowPos(ImVec2(menuX, menuY));
        ImGui::SetNextWindowSize(ImVec2(480, 500));
        ImGui::Begin("Main Menu", nullptr,
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove);

        // Title
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text("2D RPG GAME");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();

        if (!menuUI.IsNewGameMode()) {
            // Main menu buttons
            ImGui::SetCursorPosX((480 - 300) / 2.0f);
            if (ImGui::Button("New Game", ImVec2(300, 60))) {
                menuUI.SetNewGameMode(true);
                AudioManager::GetInstance().PlaySound("click", 70.0f);
            }

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::SetCursorPosX((480 - 300) / 2.0f);
            if (ImGui::Button("LOAD GAME", ImVec2(300, 60))) {
                menuUI.SetNewGameMode(true); // Use same slot selection UI
                menuUI.SetSelectedSlot(-1);  // Reset selection
                AudioManager::GetInstance().PlaySound("click", 70.0f);
            }

            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::SetCursorPosX((480 - 300) / 2.0f);
            if (ImGui::Button("Quit", ImVec2(300, 60))) {
                running = false;
                AudioManager::GetInstance().PlaySound("click", 70.0f);
            }
        }
        else {
            // Save slot selection (for both new game and load game)
            ImGui::Text("Select a Save Slot:");
            ImGui::Separator();
            ImGui::Spacing();

            auto& saveSlots = stateMgr.GetSaveSlots();

            for (int i = 0; i < 3; ++i) {
                ImGui::PushID(i);

                std::string buttonText;
                if (saveSlots[i].isEmpty) {
                    buttonText = "Slot " + std::to_string(i + 1) + " - [ EMPTY ]";
                }
                else {
                    buttonText = "Slot " + std::to_string(i + 1) + " - " +
                        saveSlots[i].playerName +
                        " Lv." + std::to_string(saveSlots[i].playerLevel);
                }

                if (ImGui::Button(buttonText.c_str(), ImVec2(380, 50))) {
                    menuUI.SetSelectedSlot(i);
                    AudioManager::GetInstance().PlaySound("click", 70.0f);
                }

                if (!saveSlots[i].isEmpty) {
                    ImGui::SameLine();
                    if (ImGui::Button("X", ImVec2(40, 50))) {
                        stateMgr.DeleteSaveSlot(i);
                        AudioManager::GetInstance().PlaySound("click", 70.0f);
                    }
                }

                ImGui::PopID();
                ImGui::Spacing();
            }

            ImGui::Separator();
            ImGui::Spacing();

            if (menuUI.GetSelectedSlot() >= 0) {
                int selectedSlot = menuUI.GetSelectedSlot();
                bool isSlotEmpty = saveSlots[selectedSlot].isEmpty;

                if (isSlotEmpty) {
                    // New game - ask for player name
                    ImGui::Text("Enter Player Name:");
                    static char nameBuf[32] = "Hero";
                    ImGui::InputText("##name", nameBuf, 32);

                    ImGui::Spacing();

                    if (ImGui::Button("START NEW GAME", ImVec2(300, 50))) {
                        std::string playerName = nameBuf;
                        if (!playerName.empty()) {
                            stateMgr.CreateNewGame(selectedSlot, playerName);
                            stateMgr.SetState(GameState::Playing);
                            menuUI.SetNewGameMode(false);

                            // Reinitialize game
                            registry.clear();
                            if (worldSystem) delete worldSystem;
                            worldSystem = new WorldSystem(20, 32.0f);
                            physicsSystem.setWorld(worldSystem);

                            glm::vec2 spawnPos(200, 300);
                            for (int y = 15; y >= 0; --y) {
                                if (worldSystem->isSolidAt(glm::vec2(spawnPos.x, y * 32.0f))) {
                                    spawnPos.y = (y + 2) * 32.0f;
                                    break;
                                }
                            }
                            playerEntity = spawnEntity("player", spawnPos);
                            lastPlayerPosition = spawnPos;
                            lastCheckpointPosition = spawnPos;
                            totalPlayTime = 0.0f;

                            AudioManager::GetInstance().PlayMusic("gameplay_music", true, 30.0f);
                            std::cout << "Started new game in slot " << selectedSlot << std::endl;
                        }
                    }
                }
                else {
                    // Load existing game
                    ImGui::Text("Load Game:");
                    ImGui::Text("Player: %s", saveSlots[selectedSlot].playerName.c_str());
                    ImGui::Text("Level: %d", saveSlots[selectedSlot].playerLevel);
                    ImGui::Text("Last Saved: %s", saveSlots[selectedSlot].lastSaved.c_str());

                    ImGui::Spacing();

                    if (ImGui::Button("LOAD GAME", ImVec2(300, 50))) {
                        // Load the game
                        loadGame(selectedSlot);
                        stateMgr.SetState(GameState::Playing);
                        menuUI.SetNewGameMode(false);

                        AudioManager::GetInstance().PlayMusic("gameplay_music", true, 30.0f);
                        std::cout << "Loaded game from slot " << selectedSlot << std::endl;
                    }
                }
            }

            ImGui::Spacing();

            if (ImGui::Button("< BACK", ImVec2(150, 40))) {
                menuUI.SetNewGameMode(false);
                menuUI.SetSelectedSlot(-1);
                AudioManager::GetInstance().PlaySound("click", 70.0f);
            }
        }

        ImGui::End();

        // Render and return early - don't show game UI
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return;
    }

    // Don't show UI if disabled
    if (!showUI) {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return;
    }

    // Player HUD (only when playing)
    if (stateMgr.GetState() == GameState::Playing && registry.isValid(playerEntity)) {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_Always);  // Increased height for mana/XP
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGui::Begin("Player HUD", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        auto* health = registry.getComponent<HealthComponent>(playerEntity);
        if (health) {
            // Clamp health values to prevent display issues
            float currentHP = std::max(0.0f, std::min(health->currentHP, health->maxHP));
            float maxHP = std::max(1.0f, health->maxHP); // Prevent division by zero
            float healthPercent = currentHP / maxHP;

            ImGui::Text("Health: %.0f / %.0f", currentHP, maxHP);

            // Color the progress bar based on health
            ImVec4 barColor;
            if (healthPercent > 0.6f) {
                barColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
            }
            else if (healthPercent > 0.3f) {
                barColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
            }
            else {
                barColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
            }

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
            ImGui::ProgressBar(healthPercent, ImVec2(-1, 0));
            ImGui::PopStyleColor();

            // Show invincibility status
            if (health->invincibilityTime > 0.0f) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f),
                    "Invincible: %.1fs", health->invincibilityTime);
            }
        }

        // MANA BAR (right below health)
        auto* magic = registry.getComponent<PlayerMagicComponent>(playerEntity);
        if (magic) {
            float manaPercent = magic->currentMana / magic->maxMana;
            ImGui::Text("Mana: %.0f / %.0f", magic->currentMana, magic->maxMana);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.5f, 1.0f, 1.0f)); // Blue
            ImGui::ProgressBar(manaPercent, ImVec2(-1, 0));
            ImGui::PopStyleColor();
        }

        // XP BAR (right below mana)
        auto* stats = registry.getComponent<PlayerStatsComponent>(playerEntity);
        if (stats) {
            float xpPercent = (float)stats->experience / (float)stats->experienceToNextLevel;
            ImGui::Text("Level %d - XP: %d / %d", stats->level, stats->experience, stats->experienceToNextLevel);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0f, 0.8f, 0.2f, 1.0f)); // Gold
            ImGui::ProgressBar(xpPercent, ImVec2(-1, 0));
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Text("Controls:");
        ImGui::Text("WASD: Move | Space: Jump | LClick: Laser");
        ImGui::Text("1-4: Magic | I: Inv | C: Stats | F5: Save");

        ImGui::End();
    }

    // Inventory window - NEW TABBED VERSION
    renderInventoryUI();
    renderEquipmentDetailsUI();

    // Pause Menu
    if (isPaused && stateMgr.GetState() == GameState::Playing)
    {
        //Dark Overlay
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)windowWidth, (float)windowHeight));
        ImGui::SetNextWindowBgAlpha(0.7f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));
        ImGui::Begin("##PauseOverlay", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoInputs);
        ImGui::End();
        ImGui::PopStyleColor();

        // Pause menu centered
        float menuWidth = 500.0f;
        float menuHeight = 400.0f;
        float menuX = (windowWidth - menuWidth) / 2.0f;
        float menuY = (windowHeight - menuHeight) / 2.0f;

        ImGui::SetNextWindowPos(ImVec2(menuX, menuY));
        ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight));
        ImGui::SetNextWindowBgAlpha(0.95f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.15f, 0.95f));
        ImGui::Begin("PAUSED", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        // Title
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.5f, 1.0f));
        ImGui::SetWindowFontScale(2.0f);
        float titleWidth = ImGui::CalcTextSize("PAUSED").x * 2.0f;
        ImGui::SetCursorPosX((menuWidth - titleWidth) / 2.0f);
        ImGui::Text("PAUSED");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();

        // Resume button
        ImGui::SetCursorPosX((menuWidth - 300) / 2.0f);
        if (ImGui::Button("Resume (P)", ImVec2(300, 60))) {
            isPaused = false;
            AudioManager::GetInstance().PlaySound("click", 60.0f);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Save Game button
        ImGui::SetCursorPosX((menuWidth - 300) / 2.0f);
        if (ImGui::Button("Save Game (F5)", ImVec2(300, 60))) {
            saveGame();
            AudioManager::GetInstance().PlaySound("click", 60.0f);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Settings section
        ImGui::SetCursorPosX((menuWidth - 300) / 2.0f);
        ImGui::BeginChild("PauseSettings", ImVec2(300, 80), true);
        ImGui::Text("Settings:");
        ImGui::Separator();
        ImGui::Checkbox("Show Damage Text", &showDamageText);
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Spacing();

        // Return to Main Menu button
        ImGui::SetCursorPosX((menuWidth - 300) / 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("Return to Main Menu", ImVec2(300, 60))) {
            isPaused = false;
            stateMgr.SetState(GameState::MainMenu);
            AudioManager::GetInstance().StopMusic();
            AudioManager::GetInstance().PlayMusic("menu_music", true, 40.0f);
            AudioManager::GetInstance().PlaySound("click", 60.0f);
        }
        ImGui::PopStyleColor(3);

        ImGui::End();
        ImGui::PopStyleColor();
    }

    if (ImGui::CollapsingHeader("Settings"))
    {
        ImGui::Checkbox("Show damage text", &showDamageText);
    }

    // Crafting window
    if (showCrafting && registry.isValid(playerEntity)) {
        ImGui::SetNextWindowPos(ImVec2(450, 150), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.95f);
        ImGui::Begin("Crafting", &showCrafting);

        auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);
        if (inventory) {
            ImGui::Text("=== CRAFTING RECIPES ===");
            ImGui::Separator();

            // Health Potion Recipe
            if (ImGui::TreeNode("Health Potion")) {
                ImGui::Text("Ingredients:");
                ImGui::BulletText("2x Magic Herb");
                ImGui::BulletText("1x Crystal");

                int herbs = inventory->getItemCount("magic_herb");
                int crystals = inventory->getItemCount("crystal");
                bool canCraft = herbs >= 2 && crystals >= 1;

                if (canCraft) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Can craft!");
                    if (ImGui::Button("Craft Health Potion")) {
                        inventory->removeItem("magic_herb", 2);
                        inventory->removeItem("crystal", 1);
                        inventory->addItem("health_potion", 1);
                        std::cout << "Crafted Health Potion!" << std::endl;
                    }
                }
                else {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Missing ingredients");
                }

                ImGui::TreePop();
            }

            ImGui::Separator();

            // Magic Scroll Recipe
            if (ImGui::TreeNode("Magic Scroll")) {
                ImGui::Text("Ingredients:");
                ImGui::BulletText("3x Crystal");
                ImGui::BulletText("5x Gold Coin");

                int crystals = inventory->getItemCount("crystal");
                int gold = inventory->getItemCount("gold_coin");
                bool canCraft = crystals >= 3 && gold >= 5;

                if (canCraft) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Can craft!");
                    if (ImGui::Button("Craft Magic Scroll")) {
                        inventory->removeItem("crystal", 3);
                        inventory->removeItem("gold_coin", 5);
                        inventory->addItem("magic_scroll", 1);
                        std::cout << "Crafted Magic Scroll!" << std::endl;
                    }
                }
                else {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Missing ingredients");
                }

                ImGui::TreePop();
            }

            ImGui::Separator();
            ImGui::Text("Add crafting materials for testing:");
            if (ImGui::Button("Add Magic Herbs")) inventory->addItem("magic_herb", 3);
            ImGui::SameLine();
            if (ImGui::Button("Add Crystals")) inventory->addItem("crystal", 3);
        }

        ImGui::End();
    }

    //Stats Window (Character Stats) - position below Player HUD
    if (stateMgr.GetState() == GameState::Playing && registry.isValid(playerEntity))
    {
        ImGui::SetNextWindowPos(ImVec2(10, 260));  // Below Player HUD (which now ends at y=250)
        ImGui::SetNextWindowSize(ImVec2(300, 450));
        ImGui::Begin("Character Stats", nullptr, ImGuiWindowFlags_NoResize);

        auto* stats = registry.getComponent<PlayerStatsComponent>(playerEntity);
        if (stats)
        {
            //Level and Xp
            ImGui::Text("Level: %d", stats->level);
            ImGui::ProgressBar((float)stats->experience / stats->experienceToNextLevel);
            ImGui::Text("XP: %d / %d", stats->experience, stats->experienceToNextLevel);

            ImGui::Separator();

            //Available points
            if (stats->availableStatPoints > 0)
            {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Stat Points: %d", stats->availableStatPoints);
            }
            else
            {
                ImGui::Text("Stat Points: 0");
            }

            ImGui::Separator();

            //Base stats
            ImGui::Text("STR: %d", stats->strength);
            ImGui::SameLine();
            if (ImGui::Button("+##str") && stats->availableStatPoints > 0)
            {
                stats->allocateStat("strength");
                updatePlayerStatsFromEquipment();
            }

            ImGui::Text("VIT: %d", stats->vitality);
            ImGui::SameLine();
            if (ImGui::Button("+##vit") && stats->availableStatPoints > 0)
            {
                stats->allocateStat("vitality");
                updatePlayerStatsFromEquipment();
            }

            ImGui::Text("DEX: %d", stats->dexterity);
            ImGui::SameLine();
            if (ImGui::Button("+##dex") && stats->availableStatPoints > 0)
            {
                stats->allocateStat("dexterity");
                updatePlayerStatsFromEquipment();
            }

            ImGui::Text("INT: %d", stats->intelligence);
            ImGui::SameLine();
            if (ImGui::Button("+##int") && stats->availableStatPoints > 0)
            {
                stats->allocateStat("intelligence");
                updatePlayerStatsFromEquipment();
            }

            ImGui::Text("LUCK: %d", stats->luck);
            ImGui::SameLine();
            if (ImGui::Button("+##luck") && stats->availableStatPoints > 0)
            {
                stats->allocateStat("luck");
                updatePlayerStatsFromEquipment();
            }

            ImGui::Separator();

            //Derived stats
            ImGui::Text("Physical Damage: %.0f", stats->physicalDamage);
            ImGui::Text("Magic Damage: %.0f", stats->magicDamage);
            ImGui::Text("Crit Chance: %.1f%%", stats->critChance);
            ImGui::Text("Crit Damage: %.0f%%", stats->critDamage);
            ImGui::Text("Drop Chance: +%.1f%%", stats->dropChance);
            ImGui::Text("Drop Boost: +%.1f", stats->dropBoost);
        }
        else
        {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No stats component!");
            ImGui::Text("PlayerEntity: %d", playerEntity);
        }

        ImGui::End();
    }

    // Stats window
    ImGui::SetNextWindowPos(ImVec2(windowWidth - 310, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.8f);
    ImGui::Begin("Game Stats", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Entities: %zu", registry.getAllEntities().size());
    ImGui::Text("Window: %dx%d", windowWidth, windowHeight);
    ImGui::Separator();

    // Render mode toggle
    ImGui::Text("=== RENDER MODE ===");
    if (ImGui::Checkbox("Use Sprite Mode", &useSpriteMode)) {
        std::cout << "Render mode: " << (useSpriteMode ? "SPRITES" : "PRIMITIVES") << std::endl;
    }
    ImGui::Text("Current: %s", useSpriteMode ? "Sprites" : "Primitives");

    ImGui::Separator();
    ImGui::Checkbox("Debug Draw (F1)", &debugDrawEnabled);
    ImGui::Checkbox("Editor Mode (F2)", &editorMode);

    ImGui::Separator();
    ImGui::Text("=== SPAWN ENTITIES ===");
    if (ImGui::Button("Spawn Goblin", ImVec2(-1, 0))) {
        glm::vec2 spawnPos = cameraPosition + glm::vec2(100, 0);
        for (int y = 15; y >= 0; --y) {
            if (worldSystem->isSolidAt(glm::vec2(spawnPos.x, y * 32.0f))) {
                spawnPos.y = (y + 2) * 32.0f;
                break;
            }
        }
        spawnEntity("goblin", spawnPos);
    }

    if (ImGui::Button("Spawn Chest", ImVec2(-1, 0))) {
        glm::vec2 spawnPos = cameraPosition + glm::vec2(150, 100);
        // Find ground below
        for (int y = 15; y >= 0; --y) {
            if (worldSystem->isSolidAt(glm::vec2(spawnPos.x, y * 32.0f))) {
                spawnPos.y = (y + 2.0f) * 32.0f;
                break;
            }
        }
        spawnEntity("chest", spawnPos);
        std::cout << "Spawned chest at (" << spawnPos.x << ", " << spawnPos.y << ")" << std::endl;
    }

    ImGui::End();

    // Entity inspector
    if (editorMode) {
        ImGui::Begin("Entity Inspector");

        for (EntityID entity : registry.getAllEntities()) {
            if (ImGui::TreeNode((void*)(intptr_t)entity, "Entity %llu", entity)) {

                auto* tag = registry.getComponent<TagComponent>(entity);
                if (tag) {
                    ImGui::Text("Tag: %s", tag->tag.c_str());
                }

                if (ImGui::Button("Destroy")) {
                    destroyEntity(entity);
                    ImGui::TreePop();
                    continue;
                }

                auto* transform = registry.getComponent<TransformComponent>(entity);
                if (transform && ImGui::TreeNode("Transform")) {
                    ImGui::DragFloat2("Position", &transform->position.x);
                    ImGui::DragFloat("Rotation", &transform->rotation, 0.01f);
                    ImGui::DragFloat2("Scale", &transform->scale.x, 0.01f);
                    ImGui::TreePop();
                }

                auto* renderable = registry.getComponent<RenderableComponent>(entity);
                if (renderable && ImGui::TreeNode("Renderable")) {
                    ImGui::Checkbox("Visible", &renderable->visible);
                    ImGui::ColorEdit4("Color", &renderable->color.r);
                    ImGui::DragInt("Layer", &renderable->layer);
                    ImGui::DragFloat2("Size", &renderable->size.x);
                    ImGui::TreePop();
                }

                auto* physics = registry.getComponent<PhysicsComponent>(entity);
                if (physics && ImGui::TreeNode("Physics")) {
                    ImGui::DragFloat2("Velocity", &physics->velocity.x);
                    ImGui::DragFloat("Friction", &physics->friction, 0.01f, 0.0f, 1.0f);
                    ImGui::DragFloat("Max Speed", &physics->maxSpeed);
                    ImGui::Checkbox("Apply Gravity", &physics->applyGravity);
                    ImGui::TreePop();
                }

                auto* health = registry.getComponent<HealthComponent>(entity);
                if (health && ImGui::TreeNode("Health")) {
                    ImGui::Text("HP: %.0f / %.0f", health->currentHP, health->maxHP);
                    ImGui::ProgressBar(health->currentHP / health->maxHP);
                    ImGui::Checkbox("Dead", &health->isDead);

                    if (ImGui::Button("Damage 10")) {
                        health->damage(10.0f);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Heal 10")) {
                        health->heal(10.0f);
                    }

                    ImGui::TreePop();
                }

                auto* collider = registry.getComponent<ColliderComponent>(entity);
                if (collider && ImGui::TreeNode("Collider")) {
                    ImGui::DragFloat2("Offset", &collider->offset.x);
                    ImGui::DragFloat2("Size", &collider->size.x);
                    ImGui::Checkbox("Is Trigger", &collider->isTrigger);
                    ImGui::Checkbox("Is Static", &collider->isStatic);
                    ImGui::TreePop();
                }

                auto* inventory = registry.getComponent<InventoryComponent>(entity);
                if (inventory && ImGui::TreeNode("Inventory")) {
                    ImGui::Text("Capacity: %d", inventory->capacity);

                    for (size_t i = 0; i < inventory->slots.size(); ++i) {
                        auto& slot = inventory->slots[i];
                        if (!slot.isEmpty()) {
                            ImGui::Text("[%zu] %s x%d", i, slot.itemID.c_str(), slot.quantity);
                        }
                    }

                    ImGui::TreePop();
                }

                auto* ai = registry.getComponent<AIStateComponent>(entity);
                if (ai && ImGui::TreeNode("AI State")) {
                    const char* behaviors[] = { "Idle", "Patrol", "Chase", "Attack", "Flee" };
                    int currentBehavior = static_cast<int>(ai->currentBehavior);
                    if (ImGui::Combo("Behavior", &currentBehavior, behaviors, 5)) {
                        ai->currentBehavior = static_cast<AIBehavior>(currentBehavior);
                    }
                    ImGui::DragFloat("Sight Range", &ai->sightRange);
                    ImGui::DragFloat("Attack Range", &ai->attackRange);
                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }
        }

        ImGui::End();
    }

    // Animation Editor
    if (showAnimationEditor && editorMode) {
        renderAnimationEditor();
    }

    //Damage text
    if (stateMgr.GetState() == GameState::Playing)
    {
        renderDamageText();
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ============================================================================
// CALLBACKS & CAMERA SETTING
// ============================================================================

void Game::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    UNUSED(scancode);
    UNUSED(mods);

    Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));
    auto& stateMgr = GameStateManager::GetInstance();

    if (action == GLFW_PRESS) {
        // NEW: Skip splash screen
        if (key == GLFW_KEY_SPACE && stateMgr.GetState() == GameState::SplashScreen) {
            stateMgr.SetState(GameState::MainMenu);
        }

        // NEW: Respawn player (IMMEDIATE - no timer wait required)
        if (key == GLFW_KEY_R) {
            std::cout << "R KEY PRESSED!" << std::endl;
            std::cout << "  Current State: " << (int)stateMgr.GetState() << std::endl;
            std::cout << "  GameOver State Value: " << (int)GameState::GameOver << std::endl;
            std::cout << "  Has Player Died: " << (stateMgr.HasPlayerDied() ? "YES" : "NO") << std::endl;
            std::cout << "  Respawn Timer: " << stateMgr.GetRespawnTimer() << std::endl;

            // Allow IMMEDIATE respawn if in GameOver state OR player died
            if (stateMgr.GetState() == GameState::GameOver || stateMgr.HasPlayerDied()) {
                std::cout << "  -> RESPAWNING PLAYER IMMEDIATELY!" << std::endl;
                game->respawnPlayer();
                stateMgr.SetState(GameState::Playing);
            }
            else {
                std::cout << "  -> Not in Game Over state" << std::endl;
            }
        }

        // NEW: Save game
        if (key == GLFW_KEY_F5 && stateMgr.GetState() == GameState::Playing) {
            game->saveGame();
        }

        // Inventory with sound
        if (key == GLFW_KEY_I && stateMgr.GetState() == GameState::Playing) {
            game->showInventory = !game->showInventory;
            AudioManager::GetInstance().PlaySound("click", 60.0f);
        }

        // Magic skills (1-4 keys)
        if (key == GLFW_KEY_1 && stateMgr.GetState() == GameState::Playing) {
            game->castMagicSkill(0);
        }
        if (key == GLFW_KEY_2 && stateMgr.GetState() == GameState::Playing) {
            game->castMagicSkill(1);
        }
        if (key == GLFW_KEY_3 && stateMgr.GetState() == GameState::Playing) {
            game->castMagicSkill(2);
        }
        if (key == GLFW_KEY_4 && stateMgr.GetState() == GameState::Playing) {
            game->castMagicSkill(3);
        }

        // Pausing the game
        if (key == GLFW_KEY_P)
        {
            if(stateMgr.GetState() == GameState::Playing)
            {
                //Toggle pause
                game->isPaused = !game->isPaused;
                std::cout << (game->isPaused ? "GAME PAUSED" : "GAME RESUMED") << std::endl;
                AudioManager::GetInstance().PlaySound("click", 60.0f);
            }
        }

        // ESC key handling - context sensitive
        if (key == GLFW_KEY_ESCAPE) {
            if (stateMgr.GetState() == GameState::Playing) {
                // Return to main menu from gameplay
                std::cout << "Returning to main menu..." << std::endl;
                stateMgr.SetState(GameState::MainMenu);
                AudioManager::GetInstance().StopMusic();
                AudioManager::GetInstance().PlayMusic("menu_music", true, 40.0f);
            }
            else if (stateMgr.GetState() == GameState::GameOver) {
                // Return to main menu from Game Over screen
                std::cout << "Returning to main menu from Game Over..." << std::endl;
                stateMgr.SetPlayerDied(false); // Clear death flag
                stateMgr.SetState(GameState::MainMenu);
                AudioManager::GetInstance().StopMusic();
                AudioManager::GetInstance().PlayMusic("menu_music", true, 40.0f);
            }
            else if (stateMgr.GetState() == GameState::MainMenu) {
                // Quit game from main menu
                game->running = false;
            }
        }
        if (key == GLFW_KEY_F1) {
            game->debugDrawEnabled = !game->debugDrawEnabled;
        }
        if (key == GLFW_KEY_F2) {
            game->editorMode = !game->editorMode;
        }
        if (key == GLFW_KEY_F3) {
            game->useSpriteMode = !game->useSpriteMode;
            std::cout << "Render mode: " << (game->useSpriteMode ? "SPRITES" : "PRIMITIVES") << std::endl;
        }
        if (key == GLFW_KEY_C) {
            game->showCrafting = !game->showCrafting;
        }
        if (key == GLFW_KEY_H) {
            game->showUI = !game->showUI;
        }
        if (key == GLFW_KEY_F4) {
            game->showAnimationEditor = !game->showAnimationEditor;
            if (game->showAnimationEditor) {
                game->editorMode = true; // Auto-enable editor mode
            }
        }
    }
}

void Game::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    UNUSED(window);
    UNUSED(button);
    UNUSED(action);
    UNUSED(mods);
    // Mouse input handling can be implemented here
}

glm::vec2 Game::screenToWorld(const glm::vec2& screenPos) const {
    // Convert screen coordinates to world coordinates
    // Screen origin is top-left, world origin is bottom-left
    glm::vec2 normalizedPos;
    normalizedPos.x = screenPos.x;
    normalizedPos.y = windowHeight - screenPos.y; // Flip Y axis

    // Convert to world space considering camera
    glm::vec2 worldPos;
    worldPos.x = normalizedPos.x - windowWidth * 0.5f + cameraPosition.x;
    worldPos.y = normalizedPos.y - windowHeight * 0.5f + cameraPosition.y;

    return worldPos / cameraZoom;
}

void Game::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Game* game = static_cast<Game*>(glfwGetWindowUserPointer(window));

    if (game && width > 0 && height > 0) {
        game->windowWidth = width;
        game->windowHeight = height;

        // Update renderer viewport
        glViewport(0, 0, width, height);
        game->renderer.setViewport(width, height);

        std::cout << "Window resized to: " << width << "x" << height << std::endl;
    }
}
// ============================================================================
// NEW STATE UPDATE METHODS
// ============================================================================

void Game::updateSplashScreen(float deltaTime) {
    menuUI.RenderSplashScreen(renderer, deltaTime, windowWidth, windowHeight);
}

void Game::updateMainMenu(float deltaTime) {
    UNUSED(deltaTime);
    static bool musicStarted = false;
    if (!musicStarted) {
        AudioManager::GetInstance().PlayMusic("menu_music", true, 40.0f);
        musicStarted = true;
    }
}

void Game::updateGameOver(float deltaTime) {
    auto& stateMgr = GameStateManager::GetInstance();
    stateMgr.UpdateRespawnTimer(deltaTime);

    if (stateMgr.CanRespawn()) {
        respawnPlayer();
        stateMgr.SetState(GameState::Playing);
    }
}

// ============================================================================
// NEW STATE RENDER METHODS
// ============================================================================

void Game::renderSplashScreen() {
    // Splash is rendered by MenuUI
    menuUI.RenderSplashScreen(renderer, 0.0f, windowWidth, windowHeight);
}

void Game::renderMainMenu() {
    // Menu is now rendered entirely through ImGui in renderImGui()
    // Just render a simple background
    renderer.setCamera(glm::vec2(640, 360), 1.0f);
    renderer.drawRect(glm::vec2(640, 360), glm::vec2(1280, 720), Color(0.1f, 0.1f, 0.15f, 1.0f));
}

void Game::renderGameplay() {
    // Set camera
    renderer.setCamera(cameraPosition, cameraZoom);

    // Render world tiles first (background)
    if (worldSystem) {
        worldSystem->render(renderer, cameraPosition);
    }

    // Then render entities on top
    updateRenderSystem();

    // Render health bars above entities
    renderHealthBars();

    // Render laser beams (on top of everything)
    renderLaserBeams();

    // Render magic effects
    renderMagicEffects();

    // Render damage text
    //renderDamageText();

    // Render interaction prompts
    renderInteractionPrompts();

    // Debug: Render cursor position
    if (debugDrawEnabled) {
        renderer.drawCircle(mouseWorldPos, 5.0f, Color::Red());

        // Draw line from player to cursor
        if (registry.isValid(playerEntity)) {
            auto* playerTransform = registry.getComponent<TransformComponent>(playerEntity);
            if (playerTransform) {
                renderer.drawLine(playerTransform->position, mouseWorldPos, Color::Yellow(), 2.0f);
            }
        }
    }

    // Debug draw colliders
    if (debugDrawEnabled) {
        renderer.enableDebugDraw(true);
        auto entities = registry.getEntitiesWith<TransformComponent, ColliderComponent>();
        for (EntityID entity : entities) {
            auto* transform = registry.getComponent<TransformComponent>(entity);
            auto* collider = registry.getComponent<ColliderComponent>(entity);
            if (transform && collider) {
                renderer.drawDebugRect(collider->getBounds(transform->position), Color::Green());
            }
        }
    }
}

void Game::renderGameOver() {
    renderGameplay();
}

// ============================================================================
// AUDIO LOADING
// ============================================================================

void Game::loadAudioAssets() {
    auto& audio = AudioManager::GetInstance();

    audio.LoadMusic("menu_music", "assets/audio/music/liecio-loop-menu-preview-109594.mp3");
    audio.LoadMusic("gameplay_music", "assets/audio/music/xtremefreddy-game-music-loop-7-145285.mp3");

    audio.LoadSound("click", "assets/audio/sfx/humordome-magic-button-click-453258.mp3");
    audio.LoadSound("hit", "assets/audio/sfx/freesound_community-gun-sound-fire-94436.mp3");
    audio.LoadSound("death", "assets/audio/sfx/freesound_community-dead-8bit-41400.mp3");
    audio.LoadSound("pickup", "assets/audio/sfx/freesound_community-item-pickup-37089.mp3");
    audio.LoadSound("respawn", "assets/audio/sfx/freesound_community-item_respawn-91422.mp3");
    audio.LoadSound("item_use", "assets/audio/sfx/musicholder-use-tinderbox2-212653.mp3");
    audio.LoadSound("item_drop", "assets/audio/sfx/yodguard-drop-or-pickup-item-1-387916.mp3");

    std::cout << "Audio assets loaded" << std::endl;

    //Playing sounds
    //audio.PlaySound("click", 70.0f); //volume 70% (relative to sound volume)

    //Music streaming
    /*audio.PlayMusic("gameplay_music", true, 30.0f); // loop, volume 30%
    audio.StopMusic();
    audio.PauseMusic();
    audio.ResumeMusic();*/
}

// ============================================================================
// PLAYER DEATH/RESPAWN SYSTEM
// ============================================================================

void Game::handlePlayerDeath() {
    std::cout << "Player died!" << std::endl;

    auto& stateMgr = GameStateManager::GetInstance();
    stateMgr.SetPlayerDied(true);
    stateMgr.ResetRespawnTimer();

    AudioManager::GetInstance().PlaySound("death", 80.0f);
    AudioManager::GetInstance().StopMusic();
}

void Game::respawnPlayer() {
    std::cout << "Player respawning..." << std::endl;
    std::cout << "  Loading from last autosave..." << std::endl;

    auto& stateMgr = GameStateManager::GetInstance();

    // Try to load from autosave
    json gameData;
    if (stateMgr.LoadAutosave(gameData)) {
        std::cout << "  Autosave loaded successfully!" << std::endl;

        // Clear world and recreate
        registry.clear();
        if (worldSystem) delete worldSystem;
        worldSystem = new WorldSystem(20, 32.0f);
        physicsSystem.setWorld(worldSystem);

        // Load game state from autosave
        deserializeGameState(gameData);

        // Ensure player is alive
        if (playerEntity != INVALID_ENTITY) {
            auto* health = registry.getComponent<HealthComponent>(playerEntity);
            if (health) {
                health->isDead = false;
                health->invincibilityTime = 2.0f;
                std::cout << "  Player restored from autosave" << std::endl;
                std::cout << "  Health: " << health->currentHP << "/" << health->maxHP << std::endl;
            }
        }
    }
    else {
        std::cout << "  No autosave found - using fallback respawn" << std::endl;

        // Fallback: Simple respawn at checkpoint
        if (playerEntity != INVALID_ENTITY) {
            auto* health = registry.getComponent<HealthComponent>(playerEntity);
            auto* transform = registry.getComponent<TransformComponent>(playerEntity);
            auto* physics = registry.getComponent<PhysicsComponent>(playerEntity);

            if (health) {
                float maxHealth = health->maxHP;
                health->currentHP = maxHealth;
                health->isDead = false;
                health->invincibilityTime = 2.0f;
                std::cout << "  Health restored: " << health->currentHP << "/" << health->maxHP << std::endl;
            }

            if (transform) {
                transform->position = lastCheckpointPosition;
                std::cout << "  Position reset to checkpoint: (" << lastCheckpointPosition.x << ", " << lastCheckpointPosition.y << ")" << std::endl;
            }

            if (physics) {
                physics->velocity = glm::vec2(0, 0);
                physics->acceleration = glm::vec2(0, 0);
            }
        }
    }

    stateMgr.SetPlayerDied(false);
    AudioManager::GetInstance().PlaySound("respawn", 60.0f);
    AudioManager::GetInstance().PlayMusic("gameplay_music", true, 30.0f);

    std::cout << "Player respawn complete!" << std::endl;
}

// ============================================================================
// SAVE/LOAD SYSTEM
// ============================================================================

json Game::serializeGameState() {
    json data;
    data["version"] = "1.0";
    data["playTime"] = totalPlayTime;

    if (playerEntity != INVALID_ENTITY) {
        auto* transform = registry.getComponent<TransformComponent>(playerEntity);
        auto* health = registry.getComponent<HealthComponent>(playerEntity);
        auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);

        if (transform) {
            data["playerPosition"] = { transform->position.x, transform->position.y };
        }
        if (health) {
            data["playerHealth"] = health->toJson();
        }
        if (inventory) {
            data["inventory"] = inventory->toJson();
        }
    }

    data["checkpoint"] = { lastCheckpointPosition.x, lastCheckpointPosition.y };

    //save player stats
    auto* playerStats = registry.getComponent<PlayerStatsComponent>(playerEntity);
    if (playerStats)
    {
        data["playerStats"] = playerStats->toJson();

        // Also save level at top level for GameStateManager's slot metadata
        data["playerLevel"] = playerStats->level;
    }

    // Save player magic
    auto* playerMagic = registry.getComponent<PlayerMagicComponent>(playerEntity);
    if (playerMagic)
    {
        data["playerMagic"] = playerMagic->toJson();
    }

    return data;
}

void Game::deserializeGameState(const json& data) {
    if (data.contains("playTime")) {
        totalPlayTime = data["playTime"];
    }

    glm::vec2 spawnPos(100, 200);
    if (data.contains("playerPosition")) {
        spawnPos = glm::vec2(data["playerPosition"][0], data["playerPosition"][1]);
    }

    if (data.contains("checkpoint")) {
        lastCheckpointPosition = glm::vec2(data["checkpoint"][0], data["checkpoint"][1]);
    }

    playerEntity = spawnEntity("player", spawnPos);

    if (playerEntity != INVALID_ENTITY) {
        if (data.contains("playerHealth")) {
            auto* health = registry.getComponent<HealthComponent>(playerEntity);
            if (health) health->fromJson(data["playerHealth"]);
        }
        if (data.contains("inventory")) {
            auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);
            if (inventory) inventory->fromJson(data["inventory"]);
        }
    }

    //load player stats
    if (data.contains("playerStats"))
    {
        auto* stats = registry.getComponent<PlayerStatsComponent>(playerEntity);
        if (stats)
        {
            stats->fromJson(data["playerStats"]);
            updatePlayerHealthFromStats();
            updatePlayerManaFromStats();
        }
    }

    // Load player magic
    if (data.contains("playerMagic"))
    {
        auto* magic = registry.getComponent<PlayerMagicComponent>(playerEntity);
        if (magic)
        {
            magic->fromJson(data["playerMagic"]);
        }
    }
}

void Game::saveGame() {
    json gameData = serializeGameState();
    if (GameStateManager::GetInstance().SaveGame(gameData)) {
        std::cout << "Game saved!" << std::endl;
        AudioManager::GetInstance().PlaySound("click", 70.0f);
    }
}

void Game::autosave() {
    json gameData = serializeGameState();
    if (GameStateManager::GetInstance().SaveAutosave(gameData)) {
        // Silent autosave - don't spam console or play sound
    }
}

void Game::loadGame(int slotIndex) {
    json gameData;
    if (GameStateManager::GetInstance().loadGame(slotIndex, gameData)) {
        // Reinitialize game world
        registry.clear();
        if (worldSystem) delete worldSystem;
        worldSystem = new WorldSystem(20, 32.0f);
        physicsSystem.setWorld(worldSystem);

        // Load game state
        deserializeGameState(gameData);

        std::cout << "Game loaded successfully from slot " << slotIndex << std::endl;

        /*TEST: Add magic scrolls AFTER loading(so they don't get overwritten)
        auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);
        if (inventory) {
            std::cout << "POST-LOAD: Adding test scrolls to inventory..." << std::endl;

            bool added1 = inventory->addItem("scroll_lightning", 1);
            bool added2 = inventory->addItem("scroll_blackhole", 1);
            bool added3 = inventory->addItem("scroll_omnilaser", 1);
            bool added4 = inventory->addItem("scroll_firebeam", 1);
            bool added5 = inventory->addItem("leather_armor", 1);
            bool added6 = inventory->addItem("mage_ring", 1);
            bool added7 = inventory->addItem("iron_sword", 1);

            std::cout << "POST-LOAD Scroll additions - Lightning: " << (added1 ? "?" : "?")
                << " Blackhole: " << (added2 ? "?" : "?")
                << " OmniLaser: " << (added3 ? "?" : "?")
                << " FireBeam: " << (added4 ? "?" : "?") << std::endl;

            // Count items
            int itemCount = 0;
            for (const auto& slot : inventory->slots) {
                if (!slot.isEmpty()) {
                    std::cout << "  - " << slot.itemID << " x" << slot.quantity << std::endl;
                    itemCount++;
                }
            }
            std::cout << "Total items after load: " << itemCount << std::endl;
        }*/
    }
    else {
        std::cerr << "Failed to load game from slot " << slotIndex << std::endl;
    }
}

// ============================================================================
// STATS SYSTEM
// ============================================================================
void Game::updatePlayerHealthFromStats() {
    if (playerEntity == INVALID_ENTITY) return;

    auto* stats = registry.getComponent<PlayerStatsComponent>(playerEntity);
    auto* health = registry.getComponent<HealthComponent>(playerEntity);
    auto* equipment = registry.getComponent<EquipmentStatsComponent>(playerEntity);

    if (stats && health) {
        int totalVitality = stats->vitality;
        if (equipment) {
            totalVitality += equipment->bonusVitality;
        }
        
        /*Base: HP 100 + (vitality * 40) + (level * 70)
        float newMaxHP = 100.0f + (totalVitality * 40.0f) + (stats->level * 70.0f);

        if (newMaxHP > health->maxHP) {
            float hpPercent = health->currentHP / health->maxHP;
            health->maxHP = newMaxHP;
            health->currentHP = newMaxHP * hpPercent;
            
        }
        else {
            health->maxHP = newMaxHP;
        }*/

        health->updateFromStats(totalVitality * 50);
    }
}

void Game::updatePlayerManaFromStats() {
    if (playerEntity == INVALID_ENTITY) return;

    auto* stats = registry.getComponent<PlayerStatsComponent>(playerEntity);
    auto* magic = registry.getComponent<PlayerMagicComponent>(playerEntity);
    auto* equipment = registry.getComponent<EquipmentStatsComponent>(playerEntity);

    if (stats && magic) {
        int totalIntelligence = stats->intelligence;
        if (equipment) {
            totalIntelligence += equipment->bonusIntelligence;
        }

        // Update mana based on INT (formula as in existing code)
        magic->updateFromStats(totalIntelligence * 50.0f);
    }
}

void Game::updatePlayerStatsFromEquipment() {
    if (playerEntity == INVALID_ENTITY) return;

    auto* stats = registry.getComponent<PlayerStatsComponent>(playerEntity);
    auto* equipment = registry.getComponent<EquipmentStatsComponent>(playerEntity);
    auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);

    if (!stats || !equipment || !inventory) return;

    // Reset all bonuses
    equipment->recalculateBonuses();

    // Helper lambda to apply item bonuses with upgrade multiplier
    auto applyItemBonuses = [&](const std::string& itemID) {
        if (itemID.empty()) return;

        const Item* item = ResourceManager::getInstance().getItem(itemID);
        if (!item) return;

        int upgradeLevel = inventory->getUpgradeLevel(itemID);
        float multiplier = 1.0f + (upgradeLevel * 0.15f);

        equipment->bonusStrength += (int)(item->bonusStrength * multiplier);
        equipment->bonusVitality += (int)(item->bonusVitality * multiplier);
        equipment->bonusDexterity += (int)(item->bonusDexterity * multiplier);
        equipment->bonusIntelligence += (int)(item->bonusIntelligence * multiplier);
        equipment->bonusLuck += (int)(item->bonusLuck * multiplier);

        equipment->bonusPhysicalDamage += item->bonusPhysicalDamage * multiplier;
        equipment->bonusMagicDamage += item->bonusMagicDamage * multiplier;
        equipment->bonusHealth += item->bonusHealth * multiplier;
        equipment->bonusDefence += item->bonusDefence * multiplier;
        equipment->bonusCritChance += item->bonusCritChance * multiplier;
        equipment->bonusCritDmg += item->bonusCritDmg * multiplier;
        equipment->bonusDropChance += item->bonusDropChance * multiplier;
        equipment->bonusDropBoost += item->bonusDropBoost * multiplier;
        };

    // Apply equipped items
    applyItemBonuses(inventory->equippedWeapon);
    applyItemBonuses(inventory->equippedArmor);
    applyItemBonuses(inventory->equippedAccessory1);
    applyItemBonuses(inventory->equippedAccessory2);

    // --- NEW: Compute final derived stats using base + equipment ---
    int totalStr = stats->strength + equipment->bonusStrength;
    int totalVit = stats->vitality + equipment->bonusVitality;
    int totalDex = stats->dexterity + equipment->bonusDexterity;
    int totalInt = stats->intelligence + equipment->bonusIntelligence;
    int totalLuck = stats->luck + equipment->bonusLuck;

    // Derived from base stats + equipment base bonuses
    stats->physicalDamage = 20.0f + (totalStr * 80.0f) + (stats->level * 15.0f);
    stats->magicDamage = 20.0f + (totalInt * 80.0f) + (stats->level * 15.0f);
    stats->critChance = 5.0f + (totalDex * 0.8f);
    stats->critDamage = 150.0f + (totalDex * 1.5f);
    stats->dropChance = 0.1f + (totalLuck * 0.5f);
    stats->dropBoost = 7.5f + (totalLuck * 3.0f);

    // Add direct bonuses from equipment
    stats->physicalDamage += equipment->bonusPhysicalDamage;
    stats->magicDamage += equipment->bonusMagicDamage;
    stats->critChance += equipment->bonusCritChance;
    stats->critDamage += equipment->bonusCritDmg;
    stats->dropChance += equipment->bonusDropChance;
    stats->dropBoost += equipment->bonusDropBoost;

    // Apply caps (as defined in calculateDerivedStats)
    if (stats->critChance > 40.0f) stats->critChance = 40.0f;
    if (stats->critDamage > 1000.0f) stats->critDamage = 1000.0f;
    if (stats->dropChance > 50.0f) stats->dropChance = 50.0f;
    if (stats->dropBoost > 40.0f) stats->dropBoost = 40.0f;

    // Update health and mana (they now use total vitality/intelligence)
    updatePlayerHealthFromStats();
    updatePlayerManaFromStats();
}

// ============================================================================
// MAGIC SYSTEM
// ============================================================================

void Game::castMagicSkill(int slotIndex) {
    if (playerEntity == INVALID_ENTITY) return;

    auto* magic = registry.getComponent<PlayerMagicComponent>(playerEntity);
    auto* transform = registry.getComponent<TransformComponent>(playerEntity);

    if (!magic || !transform) {
        std::cout << "No magic component on player!" << std::endl;
        return;
    }

    // Check if slot has a skill
    if (magic->equippedSkills[slotIndex] == MagicSkillType::None) {
        std::cout << "No skill equipped in slot " << (slotIndex + 1) << std::endl;
        return;
    }

    MagicSkillData skillData = GetSkillData(magic->equippedSkills[slotIndex]);

    // Check if can cast
    if (!magic->canCast(slotIndex, skillData)) {
        if (magic->skillCooldowns[slotIndex] > 0.0f) {
            std::cout << "Skill on cooldown: " << magic->skillCooldowns[slotIndex] << "s" << std::endl;
        }
        else if (magic->currentMana < skillData.manaCost) {
            std::cout << "Not enough mana! Need: " << skillData.manaCost
                << " Have: " << magic->currentMana << std::endl;
        }
        return;
    }

    // Cast the skill!
    createMagicEffect(magic->equippedSkills[slotIndex], transform->position, playerEntity);

    // Consume mana
    magic->currentMana -= skillData.manaCost;
    if (magic->currentMana < 0.0f) magic->currentMana = 0.0f;

    // Start cooldown
    magic->skillCooldowns[slotIndex] = skillData.cooldown;

    // Play sound
    AudioManager::GetInstance().PlaySound("hit", 100.0f);

    std::cout << "Cast " << skillData.name << "! Mana: " << magic->currentMana
        << "/" << magic->maxMana << std::endl;
}

EntityID Game::createMagicEffect(MagicSkillType skillType, const glm::vec2& position, EntityID owner) {
    EntityID effect = registry.createEntity();

    MagicSkillData skillData = GetSkillData(skillType);

    MagicEffectComponent magicEffect;
    magicEffect.skillType = skillType;
    magicEffect.position = position;
    magicEffect.radius = skillData.radius;
    magicEffect.damage = skillData.baseDamage;
    magicEffect.maxLifetime = skillData.duration;
    magicEffect.owner = owner;
    magicEffect.color1 = skillData.primaryColor;
    magicEffect.color2 = skillData.secondaryColor;

    auto* stats = registry.getComponent<PlayerStatsComponent>(owner);
    if (stats)
    {
        magicEffect.damage += stats->magicDamage;

        // Apply critical hit chance
        float critRoll = (float)(rand() % 10000) / 100.0f; // 0.00 to 99.99
        if (critRoll < stats->critChance) {
            magicEffect.damage *= (stats->critDamage / 100.0f);
            magicEffect.isCritical = true;
            std::cout << "CRITICAL MAGIC! Damage: " << magicEffect.damage << std::endl;
        }
    }

    // Set tick rate based on skill type
    switch (skillType) {
    case MagicSkillType::AOE_Lightning:
        magicEffect.tickRate = 0.2f;   // 5 ticks per second
        break;
    case MagicSkillType::AOE_Blackhole:
        magicEffect.tickRate = 1.0f;   // 1 tick per second
        break;
    case MagicSkillType::AOE_OmniLaser:
        magicEffect.tickRate = 0.3f;   // ~3.3 ticks per second
        break;
    case MagicSkillType::AOE_FireBeam:
        magicEffect.tickRate = 0.5f;   // 2 ticks per second
        break;
    case MagicSkillType::AOE_DarkCut:
        magicEffect.tickRate = 0.8f;   // 1.25 ticks per second
        break;
    case MagicSkillType::AOE_VerticalSlash:
        magicEffect.tickRate = 0.4f;   // 2.5 ticks per second
        break;
    case MagicSkillType::AOE_TimeStop:
        magicEffect.tickRate = 2.0f;   // 0.5 ticks per second (slow, utility)
        break;
    case MagicSkillType::AOE_ErasureBeam:
        magicEffect.tickRate = 0.1f;   // 10 ticks per second (very fast)
        break;
    default:
        magicEffect.tickRate = 0.5f;
        break;
    }

    // Type-specific setup
    switch (skillType) {
    case MagicSkillType::AOE_Blackhole:
        magicEffect.isPulling = true;
        magicEffect.pullStrength = 500.0f;
        magicEffect.rotationSpeed = 3.0f;
        break;

    case MagicSkillType::AOE_OmniLaser:
        magicEffect.laserCount = 8;
        magicEffect.rotationSpeed = 2.0f;
        break;

    case MagicSkillType::AOE_FireBeam:
        magicEffect.beamWidth = 50.0f;
        magicEffect.rotationSpeed = 1.5f;
        break;

    default:
        magicEffect.rotationSpeed = 2.0f;
        break;
    }

    registry.addComponent(effect, magicEffect);

    // Add tag
    TagComponent tag;
    tag.tag = "magic_effect";
    registry.addComponent(effect, tag);

    std::cout << "Magic effect created: " << skillData.name << std::endl;

    return effect;
}

void Game::updateMagicSystem(float deltaTime) {
    // Update mana regen and cooldowns (keep as is)
    auto players = registry.getEntitiesWith<PlayerMagicComponent>();
    for (EntityID entity : players) {
        auto* magic = registry.getComponent<PlayerMagicComponent>(entity);
        if (!magic) continue;

        magic->currentMana += magic->manaRegen * deltaTime;
        if (magic->currentMana > magic->maxMana)
            magic->currentMana = magic->maxMana;

        for (int i = 0; i < 4; ++i) {
            if (magic->skillCooldowns[i] > 0.0f) {
                magic->skillCooldowns[i] -= deltaTime;
                if (magic->skillCooldowns[i] < 0.0f)
                    magic->skillCooldowns[i] = 0.0f;
            }
        }
    }

    // Update magic effects
    auto effects = registry.getEntitiesWith<MagicEffectComponent>();
    for (EntityID effectEntity : effects) {
        auto* effect = registry.getComponent<MagicEffectComponent>(effectEntity);
        if (!effect) continue;

        // Update lifetime
        effect->lifetime += deltaTime;
        if (effect->lifetime >= effect->maxLifetime) {
            destroyEntity(effectEntity);
            continue;
        }

        // Update rotation (for visual effects)
        effect->rotation += effect->rotationSpeed * deltaTime;

        // --- Damage Tick ---
        effect->tickTimer += deltaTime;
        while (effect->tickTimer >= effect->tickRate) {
            effect->tickTimer -= effect->tickRate;

            // Apply damage to all enemies within radius
            auto enemies = registry.getEntitiesWith<TransformComponent, HealthComponent>();
            for (EntityID target : enemies) {
                if (target == effect->owner) continue;

                auto* targetTransform = registry.getComponent<TransformComponent>(target);
                auto* targetHealth = registry.getComponent<HealthComponent>(target);
                auto* targetTag = registry.getComponent<TagComponent>(target);

                if (!targetTransform || !targetHealth) continue;

                // Only damage enemies
                if (targetTag && targetTag->tag == "enemy") {
                    float dist = glm::length(targetTransform->position - effect->position);
                    if (dist <= effect->radius) {
                        targetHealth->damage(effect->damage);
                        createDamageText(effect->damage, targetTransform->position, false, effect->isCritical);
                        std::cout << "Magic tick damage: " << effect->damage << std::endl;

                        // Blackhole pull (applied every tick as well – optional, but we keep it smooth)
                        if (effect->isPulling) {
                            auto* targetPhysics = registry.getComponent<PhysicsComponent>(target);
                            if (targetPhysics) {
                                glm::vec2 pullDir = glm::normalize(effect->position - targetTransform->position);
                                targetPhysics->velocity += pullDir * effect->pullStrength * deltaTime; // using deltaTime here is fine for smooth pull
                            }
                        }
                    }
                }
            }
        }

        if (effect->isPulling) {
            // Apply pull every frame (smooth)
            auto enemies = registry.getEntitiesWith<TransformComponent, HealthComponent, PhysicsComponent>();
            for (EntityID target : enemies) {
                if (target == effect->owner) continue;
                auto* targetTransform = registry.getComponent<TransformComponent>(target);
                auto* targetTag = registry.getComponent<TagComponent>(target);
                if (!targetTransform || !targetTag || targetTag->tag != "enemy") continue;

                float dist = glm::length(targetTransform->position - effect->position);
                if (dist <= effect->radius) {
                    auto* targetPhysics = registry.getComponent<PhysicsComponent>(target);
                    if (targetPhysics) {
                        glm::vec2 pullDir = glm::normalize(effect->position - targetTransform->position);
                        targetPhysics->velocity += pullDir * effect->pullStrength * deltaTime;
                    }
                }
            }
        }
    }
}

void Game::renderMagicEffects() {
    auto effects = registry.getEntitiesWith<MagicEffectComponent>();

    for (EntityID entity : effects) {
        auto* effect = registry.getComponent<MagicEffectComponent>(entity);
        if (!effect) continue;

        switch (effect->skillType) {
        case MagicSkillType::AOE_Lightning:
            renderLightningStorm(effect);
            break;
        case MagicSkillType::AOE_Blackhole:
            renderBlackhole(effect);
            break;
        case MagicSkillType::AOE_OmniLaser:
            renderOmniLaser(effect);
            break;
        case MagicSkillType::AOE_FireBeam:
            renderFireBeam(effect);
            break;
        case MagicSkillType::AOE_DarkCut:
            renderDarkCut(effect);
            break;
        case MagicSkillType::AOE_VerticalSlash:
            renderVerticalSlash(effect);
            break;
        case MagicSkillType::AOE_TimeStop:
            renderTimeStop(effect);
            break;
        case MagicSkillType::AOE_ErasureBeam:
            renderErasureBeam(effect);
            break;
        }
    }
}

// VFX Rendering Functions
void Game::renderLightningStorm(MagicEffectComponent* effect) {
    float progress = effect->lifetime / effect->maxLifetime;
    float alpha = 1.0f - progress;

    // Electric storm center
    float pulseSize = 60.0f + sin(effect->lifetime * 15.0f) * 20.0f;
    renderer.drawCircle(effect->position, pulseSize,
        Color(0.9f, 0.95f, 1.0f, alpha * 0.6f), 32);
    renderer.drawCircle(effect->position, pulseSize * 0.7f,
        Color(1.0f, 1.0f, 1.0f, alpha * 0.9f), 24);

    // Realistic jagged lightning bolts
    int boltCount = 12;
    for (int i = 0; i < boltCount; ++i) {
        // Randomized timing for each bolt
        float boltPhase = fmod(effect->lifetime * 8.0f + i * 0.5f, 2.0f);
        if (boltPhase > 1.0f) continue; // Bolt flickers on/off

        float boltAlpha = alpha * (1.0f - boltPhase) * (0.8f + 0.2f * sin(effect->lifetime * 20.0f + i));

        float angle = (i * (2.0f * Math::PI / boltCount)) + sin(effect->lifetime * 3.0f) * 0.3f;
        float distance = effect->radius * (0.6f + 0.4f * sin(effect->lifetime * 4.0f + i));

        glm::vec2 boltEnd = effect->position + glm::vec2(
            cos(angle) * distance,
            sin(angle) * distance
        );

        // Create jagged lightning path
        glm::vec2 currentPos = effect->position;
        int segments = 8;
        for (int j = 0; j < segments; ++j) {
            float t = (j + 1) / (float)segments;
            glm::vec2 nextPos = glm::mix(effect->position, boltEnd, t);

            // Add jagged offset
            float jitterX = (sin(effect->lifetime * 30.0f + i * 10.0f + j * 5.0f) - 0.5f) * 25.0f;
            float jitterY = (cos(effect->lifetime * 25.0f + i * 8.0f + j * 3.0f) - 0.5f) * 25.0f;
            nextPos += glm::vec2(jitterX, jitterY) * (1.0f - t);

            // Outer glow
            renderer.drawLine(currentPos, nextPos,
                Color(0.4f, 0.5f, 1.0f, boltAlpha * 0.4f), 12.0f);
            // Bright core
            renderer.drawLine(currentPos, nextPos,
                Color(0.9f, 0.95f, 1.0f, boltAlpha * 0.9f), 5.0f);
            // Ultra-bright center
            renderer.drawLine(currentPos, nextPos,
                Color(1.0f, 1.0f, 1.0f, boltAlpha), 2.0f);

            currentPos = nextPos;
        }

        // Impact flash at endpoint
        float flashSize = 25.0f * (1.0f - boltPhase);
        renderer.drawCircle(boltEnd, flashSize,
            Color(1.0f, 1.0f, 0.8f, boltAlpha * 0.9f), 16);
        renderer.drawCircle(boltEnd, flashSize * 0.5f,
            Color(1.0f, 1.0f, 1.0f, boltAlpha), 12);
    }

    // Ground electric waves
    for (int i = 0; i < 3; ++i) {
        float waveProgress = fmod(effect->lifetime * 1.5f + i * 0.33f, 1.0f);
        float waveRadius = effect->radius * waveProgress;
        float waveAlpha = (1.0f - waveProgress) * alpha * 0.3f;

        renderer.drawCircle(effect->position, waveRadius,
            Color(0.6f, 0.7f, 1.0f, waveAlpha), 64);
    }
}

void Game::renderBlackhole(MagicEffectComponent* effect) {
    float progress = effect->lifetime / effect->maxLifetime;
    float alpha = 1.0f - progress;

    // Singularity core - pure black with event horizon glow
    renderer.drawCircle(effect->position, 25.0f,
        Color(0.0f, 0.0f, 0.0f, alpha), 32);

    // Event horizon glow (Hawking radiation-like)
    renderer.drawCircle(effect->position, 35.0f,
        Color(0.15f, 0.05f, 0.25f, alpha * 0.9f), 32);
    renderer.drawCircle(effect->position, 50.0f,
        Color(0.25f, 0.1f, 0.4f, alpha * 0.7f), 32);

    // Accretion disk - swirling matter spiraling inward
    int diskLayers = 8;
    for (int layer = 0; layer < diskLayers; ++layer) {
        float layerRadius = 80.0f + layer * 30.0f;
        int particles = 32;

        for (int p = 0; p < particles; ++p) {
            // Spiral angle based on layer and time
            float baseAngle = (p / (float)particles) * 2.0f * Math::PI;
            float spiralSpeed = 3.0f - (layer * 0.2f); // Inner particles move faster
            float angle = baseAngle - effect->rotation * spiralSpeed - (layer * 0.3f);

            // Distance varies to create thickness
            float distVariation = sin(effect->lifetime * 5.0f + p * 0.5f) * 15.0f;
            float radius = layerRadius + distVariation;

            glm::vec2 particlePos = effect->position + glm::vec2(
                cos(angle) * radius,
                sin(angle) * radius
            );

            // Color shifts from blue (outer) to red (inner) to purple (near event horizon)
            float colorShift = 1.0f - (layer / (float)diskLayers);
            Color particleColor;
            if (colorShift > 0.7f) {
                // Outer disk - blue
                particleColor = Color(0.3f, 0.4f, 1.0f, alpha * 0.6f);
            }
            else if (colorShift > 0.4f) {
                // Middle disk - purple
                particleColor = Color(0.6f, 0.2f, 0.8f, alpha * 0.7f);
            }
            else {
                // Inner disk - red/orange (hot matter)
                particleColor = Color(0.9f, 0.3f, 0.5f, alpha * 0.8f);
            }

            float particleSize = 3.0f + colorShift * 4.0f;
            renderer.drawCircle(particlePos, particleSize, particleColor, 8);
        }
    }

    // Gravitational lensing rings
    for (int i = 0; i < 6; ++i) {
        float ringDelay = i * 0.3f;
        float ringProgress = fmod(effect->lifetime * 0.8f + ringDelay, 2.0f) / 2.0f;
        float ringRadius = 120.0f + ringProgress * effect->radius;
        float ringAlpha = (1.0f - ringProgress) * alpha * 0.25f;

        // Distorted ring effect
        float distortion = sin(ringProgress * Math::PI) * 10.0f;

        renderer.drawCircle(effect->position, ringRadius + distortion,
            Color(0.4f, 0.2f, 0.6f, ringAlpha), 64);
    }

    // Energy jets from poles (perpendicular to disk)
    if (progress < 0.7f) {
        float jetLength = 150.0f;
        float jetWidth = 8.0f + sin(effect->lifetime * 8.0f) * 3.0f;

        // Top jet
        glm::vec2 jetTop = effect->position + glm::vec2(0, -jetLength);
        renderer.drawLine(effect->position, jetTop,
            Color(0.7f, 0.5f, 1.0f, alpha * 0.3f), jetWidth * 2.0f);
        renderer.drawLine(effect->position, jetTop,
            Color(0.9f, 0.7f, 1.0f, alpha * 0.6f), jetWidth);

        // Bottom jet
        glm::vec2 jetBottom = effect->position + glm::vec2(0, jetLength);
        renderer.drawLine(effect->position, jetBottom,
            Color(0.7f, 0.5f, 1.0f, alpha * 0.3f), jetWidth * 2.0f);
        renderer.drawLine(effect->position, jetBottom,
            Color(0.9f, 0.7f, 1.0f, alpha * 0.6f), jetWidth);
    }

    // Outer distortion field
    renderer.drawCircle(effect->position, effect->radius,
        Color(0.15f, 0.05f, 0.25f, alpha * 0.15f), 96);
}

void Game::renderOmniLaser(MagicEffectComponent* effect) {
    float progress = effect->lifetime / effect->maxLifetime;
    float alpha = 1.0f - progress;

    // Central fusion core - pulsing energy
    float coreSize = 60.0f + sin(effect->lifetime * 12.0f) * 15.0f;
    renderer.drawCircle(effect->position, coreSize,
        Color(1.0f, 0.9f, 0.7f, alpha * 0.8f), 32);
    renderer.drawCircle(effect->position, coreSize * 0.7f,
        Color(1.0f, 0.95f, 0.85f, alpha * 0.95f), 24);
    renderer.drawCircle(effect->position, coreSize * 0.4f,
        Color(1.0f, 1.0f, 1.0f, alpha), 16);

    // 16 beams instead of 8 - DOUBLED!
    int laserCount = 16;
    for (int i = 0; i < laserCount; ++i) {
        float angle = effect->rotation + i * (2.0f * Math::PI / laserCount);

        glm::vec2 laserEnd = effect->position + glm::vec2(
            cos(angle) * effect->radius,
            sin(angle) * effect->radius
        );

        // Heat shimmer effect - wavy beam
        float shimmer = sin(effect->lifetime * 15.0f + i) * 8.0f;
        glm::vec2 perpendicular = glm::vec2(-sin(angle), cos(angle));
        glm::vec2 midPoint = (effect->position + laserEnd) * 0.5f;
        midPoint += perpendicular * shimmer;

        // Outer glow layer
        renderer.drawLine(effect->position, midPoint,
            Color(1.0f, 0.5f, 0.0f, alpha * 0.3f), 35.0f);
        renderer.drawLine(midPoint, laserEnd,
            Color(1.0f, 0.5f, 0.0f, alpha * 0.3f), 35.0f);

        // Middle intense layer
        renderer.drawLine(effect->position, midPoint,
            Color(1.0f, 0.8f, 0.2f, alpha * 0.7f), 18.0f);
        renderer.drawLine(midPoint, laserEnd,
            Color(1.0f, 0.8f, 0.2f, alpha * 0.7f), 18.0f);

        // Bright core
        renderer.drawLine(effect->position, midPoint,
            Color(1.0f, 0.95f, 0.7f, alpha * 0.9f), 8.0f);
        renderer.drawLine(midPoint, laserEnd,
            Color(1.0f, 0.95f, 0.7f, alpha * 0.9f), 8.0f);

        // Ultra-bright center
        renderer.drawLine(effect->position, laserEnd,
            Color(1.0f, 1.0f, 1.0f, alpha), 3.0f);

        // Endpoint impact
        float endGlow = 20.0f + sin(effect->lifetime * 10.0f + i) * 5.0f;
        renderer.drawCircle(laserEnd, endGlow,
            Color(1.0f, 0.8f, 0.3f, alpha * 0.8f), 16);
        renderer.drawCircle(laserEnd, endGlow * 0.5f,
            Color(1.0f, 1.0f, 0.8f, alpha), 12);
    }

    // Beam intersection glow - where beams cross
    for (int ring = 0; ring < 3; ++ring) {
        float ringRadius = effect->radius * (0.3f + ring * 0.2f);
        float ringAlpha = alpha * (0.4f - ring * 0.1f);
        renderer.drawCircle(effect->position, ringRadius,
            Color(1.0f, 0.9f, 0.5f, ringAlpha), 48);
    }
}

void Game::renderFireBeam(MagicEffectComponent* effect) {
    float progress = effect->lifetime / effect->maxLifetime;
    float alpha = 1.0f - progress;

    glm::vec2 beamEnd = effect->position + glm::vec2(
        cos(effect->rotation) * effect->radius,
        sin(effect->rotation) * effect->radius
    );

    // Animated flame particles along beam
    int particleCount = 24;
    for (int i = 0; i < particleCount; ++i) {
        float t = i / (float)particleCount;
        glm::vec2 particlePos = glm::mix(effect->position, beamEnd, t);

        // Add turbulent movement
        float turbulence = sin(effect->lifetime * 8.0f + i * 0.5f) * 25.0f;
        glm::vec2 perpendicular = glm::vec2(-sin(effect->rotation), cos(effect->rotation));
        particlePos += perpendicular * turbulence;

        // Particle size and color vary
        float particleSize = 15.0f + sin(effect->lifetime * 10.0f + i) * 8.0f;
        float particleAlpha = alpha * (1.0f - t * 0.3f);

        // Color gradient: yellow (core) -> orange -> red (outer)
        if (i % 3 == 0) {
            renderer.drawCircle(particlePos, particleSize,
                Color(1.0f, 1.0f, 0.5f, particleAlpha * 0.8f), 12);
        }
        else if (i % 3 == 1) {
            renderer.drawCircle(particlePos, particleSize,
                Color(1.0f, 0.6f, 0.2f, particleAlpha * 0.7f), 12);
        }
        else {
            renderer.drawCircle(particlePos, particleSize,
                Color(1.0f, 0.3f, 0.1f, particleAlpha * 0.6f), 12);
        }
    }

    // Main beam layers - heat distortion waves
    for (int wave = 0; wave < 3; ++wave) {
        float waveOffset = sin(effect->lifetime * 6.0f + wave * 2.0f) * 15.0f;
        glm::vec2 perpendicular = glm::vec2(-sin(effect->rotation), cos(effect->rotation));
        glm::vec2 waveBeamEnd = beamEnd + perpendicular * waveOffset;

        // Outer yellow glow
        renderer.drawLine(effect->position, waveBeamEnd,
            Color(1.0f, 1.0f, 0.3f, alpha * 0.25f), 90.0f);
        // Middle orange
        renderer.drawLine(effect->position, waveBeamEnd,
            Color(1.0f, 0.6f, 0.1f, alpha * 0.5f), 60.0f);
        // Inner red-orange
        renderer.drawLine(effect->position, waveBeamEnd,
            Color(1.0f, 0.4f, 0.05f, alpha * 0.75f), 35.0f);
    }

    // Core white-hot beam
    renderer.drawLine(effect->position, beamEnd,
        Color(1.0f, 0.95f, 0.9f, alpha * 0.9f), 15.0f);
    renderer.drawLine(effect->position, beamEnd,
        Color(1.0f, 1.0f, 1.0f, alpha), 6.0f);

    // Source fire ball
    float sourceSize = 45.0f + sin(effect->lifetime * 12.0f) * 10.0f;
    renderer.drawCircle(effect->position, sourceSize,
        Color(1.0f, 0.8f, 0.2f, alpha * 0.8f), 32);
    renderer.drawCircle(effect->position, sourceSize * 0.6f,
        Color(1.0f, 1.0f, 0.6f, alpha * 0.95f), 24);
    renderer.drawCircle(effect->position, sourceSize * 0.3f,
        Color(1.0f, 1.0f, 1.0f, alpha), 16);

    // Impact explosion
    float impactSize = 35.0f + sin(effect->lifetime * 15.0f) * 12.0f;
    renderer.drawCircle(beamEnd, impactSize,
        Color(1.0f, 0.5f, 0.1f, alpha * 0.9f), 32);
    renderer.drawCircle(beamEnd, impactSize * 0.5f,
        Color(1.0f, 0.8f, 0.3f, alpha), 24);

    // Ember particles trailing
    for (int i = 0; i < 12; ++i) {
        float emberOffset = (effect->lifetime * 100.0f + i * 30.0f);
        float t = fmod(emberOffset / 300.0f, 1.0f);
        glm::vec2 emberPos = glm::mix(effect->position, beamEnd, t);

        // Random spiral
        float spiralAngle = effect->lifetime * 5.0f + i;
        emberPos += glm::vec2(cos(spiralAngle), sin(spiralAngle)) * 20.0f;

        float emberSize = 8.0f * (1.0f - t);
        float emberAlpha = alpha * (1.0f - t);
        renderer.drawCircle(emberPos, emberSize,
            Color(1.0f, 0.6f, 0.0f, emberAlpha), 8);
    }
}

void Game::renderDarkCut(MagicEffectComponent* effect) {
    float progress = effect->lifetime / effect->maxLifetime;
    float alpha = 1.0f - progress;

    glm::vec2 slashStart = effect->position + glm::vec2(-effect->radius, 0);
    glm::vec2 slashEnd = effect->position + glm::vec2(effect->radius, 0);

    // Reality distortion waves
    for (int wave = 0; wave < 5; ++wave) {
        float waveOffset = sin(effect->lifetime * 4.0f + wave) * 30.0f;
        glm::vec2 waveStart = slashStart + glm::vec2(0, waveOffset);
        glm::vec2 waveEnd = slashEnd + glm::vec2(0, waveOffset);

        // Outer void aura
        renderer.drawLine(waveStart, waveEnd,
            Color(0.15f, 0.0f, 0.25f, alpha * 0.3f), 120.0f);
        // Dark purple
        renderer.drawLine(waveStart, waveEnd,
            Color(0.3f, 0.0f, 0.4f, alpha * 0.5f), 70.0f);
    }

    // Main slash layers
    renderer.drawLine(slashStart, slashEnd,
        Color(0.2f, 0.0f, 0.3f, alpha * 0.6f), 110.0f);
    renderer.drawLine(slashStart, slashEnd,
        Color(0.4f, 0.0f, 0.5f, alpha * 0.8f), 65.0f);
    renderer.drawLine(slashStart, slashEnd,
        Color(0.7f, 0.0f, 0.8f, alpha * 0.95f), 35.0f);
    renderer.drawLine(slashStart, slashEnd,
        Color(1.0f, 0.2f, 1.0f, alpha), 18.0f);
    renderer.drawLine(slashStart, slashEnd,
        Color(1.0f, 0.8f, 1.0f, alpha), 6.0f);

    // Void particles bleeding from cut
    int voidParticles = 32;
    for (int i = 0; i < voidParticles; ++i) {
        float t = i / (float)voidParticles;
        glm::vec2 particlePos = glm::mix(slashStart, slashEnd, t);

        // Particles drift up and down
        float drift = sin(effect->lifetime * 6.0f + i * 0.3f) * 50.0f;
        particlePos.y += drift;

        // Particles fade in/out
        float particlePhase = fmod(effect->lifetime * 3.0f + i * 0.1f, 1.0f);
        float particleAlpha = alpha * sin(particlePhase * Math::PI);

        float particleSize = 12.0f + sin(effect->lifetime * 8.0f + i) * 6.0f;

        // Dark purple void particles
        renderer.drawCircle(particlePos, particleSize,
            Color(0.4f, 0.0f, 0.5f, particleAlpha * 0.7f), 12);
        renderer.drawCircle(particlePos, particleSize * 0.5f,
            Color(0.7f, 0.2f, 0.8f, particleAlpha), 8);
    }

    // Dimensional tears along the cut
    for (int tear = 0; tear < 8; ++tear) {
        float tearProgress = fmod(effect->lifetime * 2.0f + tear * 0.125f, 1.0f);
        glm::vec2 tearPos = glm::mix(slashStart, slashEnd, tearProgress);

        float tearSize = 25.0f * sin(tearProgress * Math::PI);
        float tearAlpha = alpha * sin(tearProgress * Math::PI);

        // Reality tear - pure black
        renderer.drawCircle(tearPos, tearSize,
            Color(0.0f, 0.0f, 0.0f, tearAlpha * 0.9f), 16);
        // Purple edge
        renderer.drawCircle(tearPos, tearSize * 1.3f,
            Color(0.5f, 0.0f, 0.6f, tearAlpha * 0.5f), 16);
    }

    // Dark energy wisps
    for (int wisp = 0; wisp < 6; ++wisp) {
        float wispAngle = (wisp / 6.0f) * 2.0f * Math::PI + effect->lifetime * 2.0f;
        float wispRadius = 100.0f + sin(effect->lifetime * 3.0f + wisp) * 50.0f;

        glm::vec2 wispPos = effect->position + glm::vec2(
            cos(wispAngle) * wispRadius,
            sin(wispAngle) * wispRadius
        );

        // Wisp trail
        glm::vec2 wispTrail = wispPos - glm::vec2(cos(wispAngle), sin(wispAngle)) * 40.0f;

        renderer.drawLine(wispTrail, wispPos,
            Color(0.6f, 0.1f, 0.7f, alpha * 0.6f), 8.0f);
        renderer.drawCircle(wispPos, 15.0f,
            Color(0.8f, 0.3f, 0.9f, alpha * 0.8f), 12);
    }
}

void Game::renderVerticalSlash(MagicEffectComponent* effect) {
    float progress = effect->lifetime / effect->maxLifetime;
    float alpha = 1.0f - progress;

    // MUCH WIDER - 800px instead of 150px!
    float waveWidth = 400.0f; // 800px total width
    float waveHeight = effect->radius * 2.0f;

    // Multiple energy layers - screen-cutting effect
    // Layer 1: Outermost cyan glow
    renderer.drawRect(effect->position, glm::vec2(waveWidth * 2.2f, waveHeight * 2.5f),
        Color(0.0f, 0.8f, 1.0f, alpha * 0.15f));

    // Layer 2: Bright cyan
    renderer.drawRect(effect->position, glm::vec2(waveWidth * 1.8f, waveHeight * 2.2f),
        Color(0.2f, 0.9f, 1.0f, alpha * 0.3f));

    // Layer 3: Intense cyan-white
    renderer.drawRect(effect->position, glm::vec2(waveWidth * 1.4f, waveHeight * 1.9f),
        Color(0.4f, 1.0f, 1.0f, alpha * 0.5f));

    // Layer 4: Pure cyan core
    renderer.drawRect(effect->position, glm::vec2(waveWidth * 1.0f, waveHeight * 1.5f),
        Color(0.6f, 1.0f, 1.0f, alpha * 0.75f));

    // Layer 5: Ultra-bright center
    renderer.drawRect(effect->position, glm::vec2(waveWidth * 0.6f, waveHeight * 1.2f),
        Color(0.9f, 1.0f, 1.0f, alpha * 0.95f));

    // Layer 6: Pure white cutting edge
    renderer.drawRect(effect->position, glm::vec2(waveWidth * 0.3f, waveHeight),
        Color(1.0f, 1.0f, 1.0f, alpha));

    // Shockwave expanding outward
    for (int i = 0; i < 4; ++i) {
        float shockProgress = fmod(effect->lifetime * 2.0f + i * 0.25f, 1.0f);
        float shockWidth = waveWidth * (1.0f + shockProgress * 0.5f);
        float shockHeight = waveHeight * (1.0f + shockProgress * 0.3f);
        float shockAlpha = (1.0f - shockProgress) * alpha * 0.4f;

        renderer.drawRect(effect->position, glm::vec2(shockWidth * 2.0f, shockHeight * 2.0f),
            Color(0.5f, 1.0f, 1.0f, shockAlpha));
    }

    // Energy particles along the slash
    int particleCount = 48;
    for (int i = 0; i < particleCount; ++i) {
        // Particles distributed along height
        float particleY = (i / (float)particleCount - 0.5f) * waveHeight * 2.0f;
        glm::vec2 particlePos = effect->position + glm::vec2(0, particleY);

        // Particles move outward
        float particleOffset = sin(effect->lifetime * 8.0f + i * 0.5f) * waveWidth * 0.3f;
        particlePos.x += particleOffset;

        float particleSize = 15.0f + sin(effect->lifetime * 10.0f + i) * 8.0f;
        float particleAlpha = alpha * (0.7f + sin(effect->lifetime * 6.0f + i) * 0.3f);

        renderer.drawCircle(particlePos, particleSize,
            Color(0.7f, 1.0f, 1.0f, particleAlpha * 0.8f), 12);
        renderer.drawCircle(particlePos, particleSize * 0.5f,
            Color(1.0f, 1.0f, 1.0f, particleAlpha), 8);
    }

    // Lingering energy trail
    for (int trail = 0; trail < 6; ++trail) {
        float trailOffset = (trail - 3) * 60.0f;
        float trailAlpha = alpha * (1.0f - abs(trail - 3) / 3.0f) * 0.3f;

        renderer.drawRect(effect->position + glm::vec2(trailOffset, 0),
            glm::vec2(waveWidth * 0.4f, waveHeight * 1.3f),
            Color(0.5f, 1.0f, 1.0f, trailAlpha));
    }

    // Impact flash at center
    float flashSize = 80.0f + sin(effect->lifetime * 15.0f) * 30.0f;
    renderer.drawCircle(effect->position, flashSize,
        Color(0.8f, 1.0f, 1.0f, alpha * 0.7f), 32);
    renderer.drawCircle(effect->position, flashSize * 0.5f,
        Color(1.0f, 1.0f, 1.0f, alpha * 0.9f), 24);
}

void Game::renderTimeStop(MagicEffectComponent* effect) {
    float progress = effect->lifetime / effect->maxLifetime;
    float alpha = 1.0f;

    // Frozen time field - translucent blue dome
    renderer.drawCircle(effect->position, effect->radius,
        Color(0.6f, 0.7f, 1.0f, 0.12f), 96);

    // Time ripples - expanding slowly
    for (int i = 0; i < 5; ++i) {
        float ringProgress = fmod(effect->lifetime * 0.3f + i * 0.2f, 1.0f);
        float ringRadius = effect->radius * ringProgress;
        float ringAlpha = (1.0f - ringProgress) * 0.25f;

        renderer.drawCircle(effect->position, ringRadius,
            Color(0.7f, 0.8f, 1.0f, ringAlpha), 64);
    }

    // Clock visualization - hour markers
    for (int hour = 0; hour < 12; ++hour) {
        float angle = (hour / 12.0f) * 2.0f * Math::PI - Math::PI / 2.0f;
        float markerRadius = effect->radius * 0.7f;

        glm::vec2 markerPos = effect->position + glm::vec2(
            cos(angle) * markerRadius,
            sin(angle) * markerRadius
        );

        float markerSize = (hour % 3 == 0) ? 20.0f : 12.0f; // Larger at 12, 3, 6, 9
        renderer.drawCircle(markerPos, markerSize,
            Color(0.8f, 0.9f, 1.0f, 0.6f), 12);
    }

    // Clock hands - frozen at current time
    float hourAngle = effect->lifetime * 0.1f;
    float minuteAngle = effect->lifetime * 1.2f;

    // Hour hand
    glm::vec2 hourEnd = effect->position + glm::vec2(
        cos(hourAngle) * effect->radius * 0.4f,
        sin(hourAngle) * effect->radius * 0.4f
    );
    renderer.drawLine(effect->position, hourEnd,
        Color(0.9f, 0.95f, 1.0f, 0.8f), 12.0f);

    // Minute hand
    glm::vec2 minuteEnd = effect->position + glm::vec2(
        cos(minuteAngle) * effect->radius * 0.6f,
        sin(minuteAngle) * effect->radius * 0.6f
    );
    renderer.drawLine(effect->position, minuteEnd,
        Color(0.9f, 0.95f, 1.0f, 0.8f), 8.0f);

    // Frozen particles suspended in time
    int frozenParticles = 64;
    for (int i = 0; i < frozenParticles; ++i) {
        float particleAngle = (i / (float)frozenParticles) * 2.0f * Math::PI;
        float particleDistance = (effect->radius * 0.5f) + sin(i * 0.5f) * (effect->radius * 0.3f);

        glm::vec2 particlePos = effect->position + glm::vec2(
            cos(particleAngle) * particleDistance,
            sin(particleAngle) * particleDistance
        );

        // Particles slowly drift
        particlePos.x += sin(effect->lifetime * 0.5f + i) * 3.0f;
        particlePos.y += cos(effect->lifetime * 0.5f + i) * 3.0f;

        float particleSize = 8.0f + sin(i * 0.8f) * 4.0f;

        // Crystalline appearance
        renderer.drawCircle(particlePos, particleSize,
            Color(0.7f, 0.85f, 1.0f, 0.6f), 8);
        renderer.drawCircle(particlePos, particleSize * 0.5f,
            Color(0.9f, 0.95f, 1.0f, 0.9f), 6);
    }

    // Crystalline freeze effect - angular ice crystals
    for (int crystal = 0; crystal < 12; ++crystal) {
        float crystalAngle = (crystal / 12.0f) * 2.0f * Math::PI;
        float crystalDistance = effect->radius * 0.8f;

        glm::vec2 crystalPos = effect->position + glm::vec2(
            cos(crystalAngle) * crystalDistance,
            sin(crystalAngle) * crystalDistance
        );

        // Draw ice crystal shape (star-like)
        for (int spike = 0; spike < 6; ++spike) {
            float spikeAngle = crystalAngle + (spike / 6.0f) * 2.0f * Math::PI;
            glm::vec2 spikeEnd = crystalPos + glm::vec2(
                cos(spikeAngle) * 25.0f,
                sin(spikeAngle) * 25.0f
            );

            renderer.drawLine(crystalPos, spikeEnd,
                Color(0.85f, 0.9f, 1.0f, 0.7f), 4.0f);
        }

        renderer.drawCircle(crystalPos, 8.0f,
            Color(0.9f, 0.95f, 1.0f, 0.8f), 6);
    }

    // Central clock core - pulsing
    float coreSize = 60.0f + sin(effect->lifetime * 4.0f) * 10.0f;
    renderer.drawCircle(effect->position, coreSize,
        Color(0.7f, 0.8f, 1.0f, 0.6f), 32);
    renderer.drawCircle(effect->position, coreSize * 0.7f,
        Color(0.85f, 0.9f, 1.0f, 0.8f), 24);
    renderer.drawCircle(effect->position, coreSize * 0.4f,
        Color(1.0f, 1.0f, 1.0f, 0.95f), 16);
}

void Game::renderErasureBeam(MagicEffectComponent* effect) {
    float progress = effect->lifetime / effect->maxLifetime;
    float alpha = 1.0f - progress;

    glm::vec2 beamEnd = effect->position + glm::vec2(
        cos(effect->rotation) * effect->radius,
        sin(effect->rotation) * effect->radius
    );

    // Particle deletion effect - reality fading
    int deletionParticles = 32;
    for (int i = 0; i < deletionParticles; ++i) {
        float t = i / (float)deletionParticles;
        glm::vec2 particlePos = glm::mix(effect->position, beamEnd, t);

        // Particles get erased progressively
        float deletionProgress = fmod(effect->lifetime * 2.0f + t, 1.0f);
        float particleAlpha = alpha * (1.0f - deletionProgress);

        if (particleAlpha > 0.1f) {
            // Glitching/pixelating effect
            float glitchOffset = (sin(effect->lifetime * 30.0f + i * 5.0f) - 0.5f) * 20.0f;
            particlePos.x += glitchOffset;

            float particleSize = 18.0f * (1.0f - deletionProgress);

            // Fading from purple to white
            float colorShift = deletionProgress;
            renderer.drawCircle(particlePos, particleSize,
                Color(0.5f + colorShift * 0.5f, colorShift * 0.5f, 1.0f, particleAlpha * 0.7f), 12);
            renderer.drawCircle(particlePos, particleSize * 0.5f,
                Color(0.8f + colorShift * 0.2f, 0.6f + colorShift * 0.4f, 1.0f, particleAlpha), 8);
        }
    }

    // Main disintegration beam layers
    // Outer void layer
    renderer.drawLine(effect->position, beamEnd,
        Color(0.4f, 0.0f, 0.8f, alpha * 0.25f), 140.0f);

    // Middle purple
    renderer.drawLine(effect->position, beamEnd,
        Color(0.6f, 0.3f, 1.0f, alpha * 0.45f), 100.0f);

    // Inner bright purple
    renderer.drawLine(effect->position, beamEnd,
        Color(0.8f, 0.6f, 1.0f, alpha * 0.65f), 70.0f);

    // Bright core transitioning to white
    renderer.drawLine(effect->position, beamEnd,
        Color(0.95f, 0.85f, 1.0f, alpha * 0.85f), 45.0f);

    // Ultra-bright white center
    renderer.drawLine(effect->position, beamEnd,
        Color(1.0f, 1.0f, 1.0f, alpha), 20.0f);

    // Pure erasure line
    renderer.drawLine(effect->position, beamEnd,
        Color(1.0f, 1.0f, 1.0f, alpha), 8.0f);

    // Energy crackle along beam
    for (int crackle = 0; crackle < 16; ++crackle) {
        float crackleT = (crackle / 16.0f);
        glm::vec2 cracklePos = glm::mix(effect->position, beamEnd, crackleT);

        // Random energy arcs
        float arcAngle = effect->rotation + Math::PI / 2.0f;
        float arcDistance = sin(effect->lifetime * 20.0f + crackle) * 40.0f;
        glm::vec2 arcEnd = cracklePos + glm::vec2(
            cos(arcAngle) * arcDistance,
            sin(arcAngle) * arcDistance
        );

        float crackleAlpha = alpha * abs(sin(effect->lifetime * 15.0f + crackle));
        renderer.drawLine(cracklePos, arcEnd,
            Color(0.9f, 0.8f, 1.0f, crackleAlpha * 0.7f), 6.0f);
        renderer.drawLine(cracklePos, arcEnd,
            Color(1.0f, 1.0f, 1.0f, crackleAlpha), 2.0f);
    }

    // Source - reality tear
    float sourceSize = 55.0f + sin(effect->lifetime * 10.0f) * 15.0f;
    renderer.drawCircle(effect->position, sourceSize,
        Color(0.6f, 0.3f, 0.9f, alpha * 0.7f), 32);
    renderer.drawCircle(effect->position, sourceSize * 0.7f,
        Color(0.85f, 0.7f, 1.0f, alpha * 0.9f), 24);
    renderer.drawCircle(effect->position, sourceSize * 0.4f,
        Color(1.0f, 1.0f, 1.0f, alpha), 16);

    // Impact - void aftermath
    float impactSize = 50.0f + sin(effect->lifetime * 12.0f) * 18.0f;

    // Black void core
    renderer.drawCircle(beamEnd, impactSize * 0.4f,
        Color(0.0f, 0.0f, 0.0f, alpha * 0.8f), 24);

    // Purple void edge
    renderer.drawCircle(beamEnd, impactSize,
        Color(0.7f, 0.4f, 1.0f, alpha * 0.8f), 32);
    renderer.drawCircle(beamEnd, impactSize * 0.7f,
        Color(0.9f, 0.8f, 1.0f, alpha * 0.95f), 24);

    renderer.drawCircle(beamEnd, 100.0f * (1.0f + progress),
        Color(1.0f, 1.0f, 1.0f, alpha * 0.5f), 32);

    // Void aftermath trail
    for (int trail = 0; trail < 8; ++trail) {
        float trailT = (trail / 8.0f);
        glm::vec2 trailPos = glm::mix(effect->position, beamEnd, trailT);

        float trailPhase = fmod(effect->lifetime * 3.0f + trail * 0.125f, 1.0f);
        float trailAlpha = alpha * sin(trailPhase * Math::PI) * 0.5f;

        float trailSize = 30.0f * (1.0f - trailPhase);
        renderer.drawCircle(trailPos, trailSize,
            Color(0.8f, 0.6f, 1.0f, trailAlpha), 16);
    }

    // Reality fade-out effect - expanding ring
    for (int ring = 0; ring < 4; ++ring) {
        float ringProgress = fmod(effect->lifetime * 1.5f + ring * 0.25f, 1.0f);
        glm::vec2 ringPos = glm::mix(effect->position, beamEnd, ringProgress);

        float ringSize = 60.0f * (1.0f + ringProgress * 0.5f);
        float ringAlpha = (1.0f - ringProgress) * alpha * 0.3f;

        renderer.drawCircle(ringPos, ringSize,
            Color(0.9f, 0.7f, 1.0f, ringAlpha), 32);
    }
}

// ============================================================================
// DAMAGE TEXT SYSTEM
// ============================================================================

void Game::createDamageText(float damage, const glm::vec2& position, bool isPlayerDamage, bool isCritical)
{
    EntityID textEntity = registry.createEntity();

    DamageTextComponent damageText(damage, position, isPlayerDamage, isCritical);
    registry.addComponent(textEntity, damageText);
}

void Game::updateDamageTextSystem(float deltaTime)
{
    auto entities = registry.getEntitiesWith<DamageTextComponent>();

    std::vector<EntityID> toDestroy;

    for (EntityID entity : entities)
    {
        auto* damageText = registry.getComponent<DamageTextComponent>(entity);
        if (!damageText) continue;

        // Update lifetime
        damageText->lifetime += deltaTime;

        // Move position based on velocity
        damageText->position += damageText->velocity * deltaTime;

        // Slow down horizontal movement over time
        damageText->velocity.x *= 0.95f;

        // Gravity effect (slow down upward movement)
        damageText->velocity.y -= 50.0f * deltaTime;

        // Mark for destruction when lifetime expires
        if (damageText->lifetime >= damageText->maxLifetime) {
            toDestroy.push_back(entity);
        }
    }

    // Clean up expired damage texts
    for (EntityID entity : toDestroy)
    {
        registry.destroyEntity(entity);
    }
}

void Game::renderDamageText()
{
    if (!showDamageText) return;

    auto entities = registry.getEntitiesWith<DamageTextComponent>();

    for (EntityID entity : entities) {
        auto* damageText = registry.getComponent<DamageTextComponent>(entity);
        if (!damageText) continue;

        // Calculate alpha (fade out over lifetime)
        float progress = damageText->lifetime / damageText->maxLifetime;
        float alpha = 1.0f - progress;

        // Scale effect for critical hits
        float scale = 1.0f;
        if (damageText->isCritical) {
            scale = 1.0f + (0.3f * (1.0f - progress));  // Starts big, shrinks
        }

        // Convert world position to screen position
        glm::vec2 screenPos = worldToScreen(damageText->position);

        // Get damage text
        std::string text = DamageTextSystem::getDamageText(damageText->damage);

        // Add "CRIT!" prefix for critical hits
        if (damageText->isCritical) {
            text = "CRIT! " + text;
        }

        // Calculate font size with scale
        float fontSize = damageText->fontSize * scale;

        // Render text using ImGui
        ImDrawList* drawList = ImGui::GetForegroundDrawList();

        // Add shadow for readability
        drawList->AddText(
            nullptr,  // Default font
            fontSize,
            ImVec2(screenPos.x + 2, screenPos.y + 2),
            IM_COL32(0, 0, 0, (int)(alpha * 180)),  // Black shadow
            text.c_str()
        );

        // Add main text
        drawList->AddText(
            nullptr,  // Default font
            fontSize,
            ImVec2(screenPos.x, screenPos.y),
            IM_COL32(
                (int)(damageText->color.r * 255),
                (int)(damageText->color.g * 255),
                (int)(damageText->color.b * 255),
                (int)(alpha * 255)
            ),
            text.c_str()
        );
    }
}

glm::vec2 Game::worldToScreen(const glm::vec2& worldPos) const
{
    // Convert world coordinates to screen coordinates
    glm::vec2 relativePos = worldPos - cameraPosition;

    glm::vec2 screenPos;
    screenPos.x = (windowWidth / 2.0f) + (relativePos.x * cameraZoom);
    screenPos.y = (windowHeight / 2.0f) - (relativePos.y * cameraZoom);

    return screenPos;
}

// ============================================================================
// EQUIPMENT DETAILS UI (Using new function, test)
// ============================================================================
/*void Game::renderEquipmentDetailsUI() {
    if (!showEquipmentDetails || selectedEquipmentID.empty()) return;

    auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);
    if (!inventory) return;

    const Item* item = ResourceManager::getInstance().getItem(selectedEquipmentID);
    if (!item) {
        showEquipmentDetails = false;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(400, 550), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(windowWidth / 2 - 200, windowHeight / 2 - 275), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Equipment Details", &showEquipmentDetails, ImGuiWindowFlags_NoResize)) {
        // Item name (gold color)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
        ImGui::TextWrapped("%s", item->name.c_str());
        ImGui::PopStyleColor();

        ImGui::Separator();

        // Type
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Type: %s", item->type.c_str());

        // Description
        ImGui::Spacing();
        ImGui::TextWrapped("%s", item->description.c_str());

        ImGui::Separator();

        // === UPGRADE SECTION ===
        int upgradeLevel = inventory->getUpgradeLevel(selectedEquipmentID);
        float multiplier = 1.0f + (upgradeLevel * 0.15f);

        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Upgrade Level: %d / 10", upgradeLevel);
        ImGui::ProgressBar(upgradeLevel / 10.0f, ImVec2(-1, 0));

        if (upgradeLevel > 0) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f),
                "Bonus Multiplier: +%.0f%%", (multiplier - 1.0f) * 100);
        }

        ImGui::Separator();

        // === STATS (Base and Upgraded) ===
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Stats:");

        auto showStat = [&](const char* name, float baseValue, bool isInt = false) {
            if (baseValue > 0) {
                float upgradedValue = baseValue * multiplier;

                if (isInt) {
                    ImGui::Text("  %s: %d", name, (int)baseValue);
                    if (upgradeLevel > 0) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                            " ? %d", (int)upgradedValue);
                    }
                }
                else {
                    ImGui::Text("  %s: %.1f", name, baseValue);
                    if (upgradeLevel > 0) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                            " ? %.1f", upgradedValue);
                    }
                }
            }
            };

        showStat("Strength", item->bonusStrength, true);
        showStat("Vitality", item->bonusVitality, true);
        showStat("Dexterity", item->bonusDexterity, true);
        showStat("Intelligence", item->bonusIntelligence, true);
        showStat("Luck", item->bonusLuck, true);
        showStat("Physical Damage", item->bonusPhysicalDamage);
        showStat("Magic Damage", item->bonusMagicDamage);
        showStat("Health", item->bonusHealth);
        showStat("Defense", item->bonusDefence);
        showStat("Crit Chance", item->bonusCritChance);

        ImGui::Separator();

        // === UPGRADE BUTTON ===
        if (upgradeLevel < 10) {
            int upgradeCost = (int)(100 * pow(1.5, upgradeLevel));

            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                "Upgrade Cost: %d gold", upgradeCost);

            // Check if player has enough gold
            int goldCount = 0;
            for (const auto& slot : inventory->slots) {
                if (slot.itemID == "gold_coin") {
                    goldCount = slot.quantity;
                    break;
                }
            }

            bool canAfford = (goldCount >= upgradeCost);

            if (!canAfford) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            }

            if (ImGui::Button("UPGRADE EQUIPMENT", ImVec2(-1, 40))) {
                if (canAfford) {
                    // Deduct gold
                    inventory->removeItem("gold_coin", upgradeCost);

                    // Upgrade item
                    inventory->upgradeItem(selectedEquipmentID);

                    // Recalculate stats
                    updatePlayerStatsFromEquipment();

                    // Play sound
                    AudioManager::GetInstance().PlaySound("item_use", 100.0f);

                    std::cout << "? Upgraded " << item->name << " to level "
                        << inventory->getUpgradeLevel(selectedEquipmentID) << "!" << std::endl;
                }
            }

            if (!canAfford) {
                ImGui::PopStyleColor(3);
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                    "Not enough gold! (Have: %d)", goldCount);
            }
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "? MAX LEVEL REACHED ?");
        }

        ImGui::Separator();

        // === EQUIP / UNEQUIP BUTTON ===
        bool isEquipped = false;
        if (item->type == "weapon") {
            isEquipped = (inventory->equippedWeapon == selectedEquipmentID);
        }
        else if (item->type == "armor") {
            isEquipped = (inventory->equippedArmor == selectedEquipmentID);
        }
        else if (item->type == "accessory") {
            isEquipped = (inventory->equippedAccessory1 == selectedEquipmentID ||
                inventory->equippedAccessory2 == selectedEquipmentID);
        }

        if (!isEquipped) {
            if (ImGui::Button("EQUIP", ImVec2(-1, 35))) {
                if (item->type == "weapon") {
                    inventory->equippedWeapon = selectedEquipmentID;
                }
                else if (item->type == "armor") {
                    inventory->equippedArmor = selectedEquipmentID;
                }
                else if (item->type == "accessory") {
                    if (inventory->equippedAccessory1.empty()) {
                        inventory->equippedAccessory1 = selectedEquipmentID;
                    }
                    else if (inventory->equippedAccessory2.empty()) {
                        inventory->equippedAccessory2 = selectedEquipmentID;
                    }
                    else {
                        // Replace accessory 1
                        inventory->equippedAccessory1 = selectedEquipmentID;
                    }
                }

                updatePlayerStatsFromEquipment();
                AudioManager::GetInstance().PlaySound("item_use", 100.0f);
            }
        }
        else {
            if (ImGui::Button("UNEQUIP", ImVec2(-1, 35))) {
                if (item->type == "weapon") {
                    inventory->equippedWeapon = "";
                }
                else if (item->type == "armor") {
                    inventory->equippedArmor = "";
                }
                else if (item->type == "accessory") {
                    if (inventory->equippedAccessory1 == selectedEquipmentID) {
                        inventory->equippedAccessory1 = "";
                    }
                    else {
                        inventory->equippedAccessory2 = "";
                    }
                }

                updatePlayerStatsFromEquipment();
            }
        }
    }

    ImGui::End();
}*/

// Placeholder stubs - implement later
void Game::tryDropMagicScroll(const glm::vec2& position) {
    auto* playerMagic = registry.getComponent<PlayerMagicComponent>(playerEntity);
    if (!playerMagic) return;

    auto* playerStats = registry.getComponent<PlayerStatsComponent>(playerEntity);

    auto scrolls = GetAllScrolls();

    for (const auto& scroll : scrolls) {
        // Skip if player already has this skill
        if (playerMagic->hasSkill(scroll.skillType)) continue;

        // Calculate drop chance (affected by LUCK stat)
        float dropChance = scroll.dropChance;
        if (playerStats) {
            // Luck increases drop chance
            dropChance *= (1.0f + playerStats->dropChance / 100.0f);
        }

        // Roll for drop
        float roll = (float)(rand() % 10000) / 10000.0f;
        if (roll < dropChance) {
            // Drop the scroll!
            dropMagicScrollItem(position, scroll);
            std::cout << "? MAGIC SCROLL DROPPED: " << scroll.scrollName << " ("
                << (dropChance * 100.0f) << "% chance)" << std::endl;
            break; // Only one scroll per enemy
        }
    }
}

void Game::dropMagicScrollItem(const glm::vec2& position, const MagicScrollItem& scroll) {
    // Add scroll to player's inventory directly (or create pickup entity)
    auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);
    if (inventory) {
        if (inventory->addItem(scroll.getItemID(), 1)) {
            std::cout << "?? " << scroll.scrollName << " added to inventory!" << std::endl;
            AudioManager::GetInstance().PlaySound("click", 100.0f);

            // Visual effect - sparkles at drop position
            for (int i = 0; i < 10; ++i) {
                float angle = (float)i / 10.0f * 2.0f * Math::PI;
                glm::vec2 sparklePos = position + glm::vec2(cos(angle) * 30.0f, sin(angle) * 30.0f);
                // Could create particle effect here
            }
        }
    }
}

void Game::useMagicScroll(const std::string& scrollID) {
    auto* magic = registry.getComponent<PlayerMagicComponent>(playerEntity);
    if (!magic) return;

    // Determine skill type from scrollID
    MagicSkillType skillType = MagicSkillType::None;

    if (scrollID == "scroll_lightning") skillType = MagicSkillType::AOE_Lightning;
    else if (scrollID == "scroll_blackhole") skillType = MagicSkillType::AOE_Blackhole;
    else if (scrollID == "scroll_omnilaser") skillType = MagicSkillType::AOE_OmniLaser;
    else if (scrollID == "scroll_firebeam") skillType = MagicSkillType::AOE_FireBeam;
    else if (scrollID == "scroll_darkcut") skillType = MagicSkillType::AOE_DarkCut;
    else if (scrollID == "scroll_verticalslash") skillType = MagicSkillType::AOE_VerticalSlash;
    else if (scrollID == "scroll_timestop") skillType = MagicSkillType::AOE_TimeStop;
    else if (scrollID == "scroll_erasure") skillType = MagicSkillType::AOE_ErasureBeam;

    if (skillType != MagicSkillType::None) {
        magic->unlockSkill(skillType);
        MagicSkillData data = GetSkillData(skillType);
        std::cout << "Learned new skill: " << data.name << "!" << std::endl;
        AudioManager::GetInstance().PlaySound("click", 100.0f);
    }
}

// ============================================================================
// EQUIPMENT DETAILS UI
// ============================================================================

void Game::renderEquipmentDetailsUI() {
    if (!showEquipmentDetails || selectedEquipmentID.empty()) return;

    auto* inventory = registry.getComponent<InventoryComponent>(playerEntity);
    if (!inventory) return;

    const Item* item = ResourceManager::getInstance().getItem(selectedEquipmentID);
    if (!item) {
        showEquipmentDetails = false;
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(400, 550), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(windowWidth / 2 - 200, windowHeight / 2 - 275), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Equipment Details", &showEquipmentDetails, ImGuiWindowFlags_NoResize)) {
        // Item name (gold color)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
        ImGui::TextWrapped("%s", item->name.c_str());
        ImGui::PopStyleColor();

        ImGui::Separator();

        // Type
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Type: %s", item->type.c_str());

        // Description
        ImGui::Spacing();
        ImGui::TextWrapped("%s", item->description.c_str());

        ImGui::Separator();

        // === UPGRADE SECTION ===
        int upgradeLevel = inventory->getUpgradeLevel(selectedEquipmentID);
        float multiplier = 1.0f + (upgradeLevel * 0.15f);

        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Upgrade Level: %d / 10", upgradeLevel);
        ImGui::ProgressBar(upgradeLevel / 10.0f, ImVec2(-1, 0));

        if (upgradeLevel > 0) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f),
                "Bonus Multiplier: +%.0f%%", (multiplier - 1.0f) * 100);
        }

        ImGui::Separator();

        // === STATS (Base and Upgraded) ===
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Stats:");

        auto showStat = [&](const char* name, float baseValue, bool isInt = false) {
            if (baseValue > 0) {
                float upgradedValue = baseValue * multiplier;

                if (isInt) {
                    ImGui::Text("  %s: %d", name, (int)baseValue);
                    if (upgradeLevel > 0) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                            " → %d", (int)upgradedValue);
                    }
                }
                else {
                    ImGui::Text("  %s: %.1f", name, baseValue);
                    if (upgradeLevel > 0) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                            " → %.1f", upgradedValue);
                    }
                }
            }
            };

        showStat("Strength", item->bonusStrength, true);
        showStat("Vitality", item->bonusVitality, true);
        showStat("Dexterity", item->bonusDexterity, true);
        showStat("Intelligence", item->bonusIntelligence, true);
        showStat("Luck", item->bonusLuck, true);
        showStat("Physical Damage", item->bonusPhysicalDamage);
        showStat("Magic Damage", item->bonusMagicDamage);
        showStat("Health", item->bonusHealth);
        showStat("Defense", item->bonusDefence);
        showStat("Crit Chance", item->bonusCritChance);

        ImGui::Separator();

        // === UPGRADE BUTTON ===
        if (upgradeLevel < 10) {
            int upgradeCost = (int)(100 * pow(1.5, upgradeLevel));

            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                "Upgrade Cost: %d gold", upgradeCost);

            // Check if player has enough gold
            int goldCount = 0;
            for (const auto& slot : inventory->slots) {
                if (slot.itemID == "gold_coin") {
                    goldCount = slot.quantity;
                    break;
                }
            }

            bool canAfford = (goldCount >= upgradeCost);

            if (!canAfford) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            }

            if (ImGui::Button("UPGRADE EQUIPMENT", ImVec2(-1, 40))) {
                if (canAfford) {
                    // Deduct gold
                    inventory->removeItem("gold_coin", upgradeCost);

                    // Upgrade item
                    inventory->upgradeItem(selectedEquipmentID);

                    // Recalculate stats
                    updatePlayerStatsFromEquipment();

                    // Play sound
                    AudioManager::GetInstance().PlaySound("item_use", 100.0f);

                    std::cout << "✨ Upgraded " << item->name << " to level "
                        << inventory->getUpgradeLevel(selectedEquipmentID) << "!" << std::endl;
                }
            }

            if (!canAfford) {
                ImGui::PopStyleColor(3);
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                    "Not enough gold! (Have: %d)", goldCount);
            }
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "★ MAX LEVEL REACHED ★");
        }

        ImGui::Separator();

        // === EQUIP / UNEQUIP BUTTON ===
        bool isEquipped = false;
        if (item->type == "weapon") {
            isEquipped = (inventory->equippedWeapon == selectedEquipmentID);
        }
        else if (item->type == "armor") {
            isEquipped = (inventory->equippedArmor == selectedEquipmentID);
        }
        else if (item->type == "accessory") {
            isEquipped = (inventory->equippedAccessory1 == selectedEquipmentID ||
                inventory->equippedAccessory2 == selectedEquipmentID);
        }

        if (!isEquipped) {
            if (ImGui::Button("EQUIP", ImVec2(-1, 35))) {
                if (item->type == "weapon") {
                    inventory->equippedWeapon = selectedEquipmentID;
                }
                else if (item->type == "armor") {
                    inventory->equippedArmor = selectedEquipmentID;
                }
                else if (item->type == "accessory") {
                    if (inventory->equippedAccessory1.empty()) {
                        inventory->equippedAccessory1 = selectedEquipmentID;
                    }
                    else if (inventory->equippedAccessory2.empty()) {
                        inventory->equippedAccessory2 = selectedEquipmentID;
                    }
                    else {
                        // Replace accessory 1
                        inventory->equippedAccessory1 = selectedEquipmentID;
                    }
                }

                updatePlayerStatsFromEquipment();
                AudioManager::GetInstance().PlaySound("item_use", 100.0f);
            }
        }
        else {
            if (ImGui::Button("UNEQUIP", ImVec2(-1, 35))) {
                if (item->type == "weapon") {
                    inventory->equippedWeapon = "";
                }
                else if (item->type == "armor") {
                    inventory->equippedArmor = "";
                }
                else if (item->type == "accessory") {
                    if (inventory->equippedAccessory1 == selectedEquipmentID) {
                        inventory->equippedAccessory1 = "";
                    }
                    else {
                        inventory->equippedAccessory2 = "";
                    }
                }

                updatePlayerStatsFromEquipment();
            }
        }
    }

    ImGui::End();
}