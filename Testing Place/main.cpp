#include "Game.h"
#include <iostream>

int main() {
    Game game;

    if (!game.initialize()) {
        std::cerr << "Failed to initialize game" << std::endl;
        return -1;
    }

    std::cout << "Game initialized successfully!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD - Move" << std::endl;
    std::cout << "  SPACE - Jump" << std::endl;
    std::cout << "  F1 - Toggle debug draw" << std::endl;
    std::cout << "  F2 - Toggle editor mode" << std::endl;
    std::cout << "  ESC - Quit" << std::endl;

    game.run();
    game.shutdown();

    return 0;
}