#include "Client.h"

#include "GameWindow.h"
#include "VulkanRenderer.h"

#include <Server.h>

#include <chrono>

namespace Client {

int runGame() {
    GameWindow window;
    VulkanRenderer renderer(window);
    Server::Server server;
    auto previousFrameTime = std::chrono::steady_clock::now();

    for (;;) {
        auto const now = std::chrono::steady_clock::now();
        double const frameSeconds = std::chrono::duration<double>(now - previousFrameTime).count();
        previousFrameTime = now;
        if (!window.processEvents(frameSeconds)) {
            break;
        }
        window.updateCamera();
        renderer.renderFrame(window.camera(), server.snapshot(now), renderer.viewport());
    }
    return 0;
}

int runSmokeTest() {
    GameWindow window;
    VulkanRenderer renderer(window);
    Server::Server server;
    renderer.renderFrame(window.camera(), server.snapshot(), renderer.viewport());
    return 0;
}

} // namespace Client
