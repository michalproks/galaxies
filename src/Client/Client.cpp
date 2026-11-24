#include "Client.h"

#include "GameWindow.h"
#include "Selection.h"
#include "VulkanRenderer.h"

#include <Server.h>

#include <chrono>
#include <optional>
#include <vector>

namespace Client {

int runGame() {
    GameWindow window;
    VulkanRenderer renderer(window);
    Server::Server server;
    std::optional<std::size_t> selectedBody;
    auto previousFrameTime = std::chrono::steady_clock::now();

    for (;;) {
        auto const now = std::chrono::steady_clock::now();
        double const frameSeconds = std::chrono::duration<double>(now - previousFrameTime).count();
        previousFrameTime = now;
        if (!window.processEvents(frameSeconds)) {
            break;
        }
        std::vector<Simulation::Vector2> const clicks = window.consumeLeftClicks();
        window.updateCamera();
        Server::GameSnapshot const snapshot = server.snapshot(now);
        for (Simulation::Vector2 const click : clicks) {
            bool const capture = renderer.wantsMouseCapture();
            if (!capture) {
                selectedBody = hitTest(snapshot, window.camera(), renderer.viewport(), click);
            }
        }
        renderer.renderFrame(window.camera(), snapshot, renderer.viewport(), selectedBody);
    }
    return 0;
}

int runSmokeTest() {
    GameWindow window;
    VulkanRenderer renderer(window);
    Server::Server server;
    Server::GameSnapshot const snapshot = server.snapshot();
    renderer.renderFrame(window.camera(), snapshot, renderer.viewport(), 0);
    return 0;
}

} // namespace Client
