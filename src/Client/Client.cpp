#include "Client.h"

#include "GameWindow.h"
#include "VulkanRenderer.h"

namespace Client {

int runGame() {
    GameWindow window;
    VulkanRenderer renderer(window);

    while (window.processEvents()) {
        renderer.renderBlackFrame();
    }
    return 0;
}

int runSmokeTest() {
    GameWindow window;
    VulkanRenderer renderer(window);
    renderer.renderBlackFrame();
    return 0;
}

} // namespace Client
