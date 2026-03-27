#include "SignalingServer.h"

#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    // Получаем порт из аргументов или используем по умолчанию
    uint16_t port = 9002;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    signaling::SignalingServer server;

    if (!server.initialize()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    if (!server.run(port)) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    return 0;
}
