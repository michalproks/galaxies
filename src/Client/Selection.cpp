#include "Selection.h"

#include <cmath>
#include <stdexcept>

namespace Client {

std::optional<std::size_t>
hitTest(Server::GameSnapshot const &snapshot, Camera const &camera, VkExtent2D viewport, Simulation::Vector2 pixel) {
    Simulation::Vector2 const world = camera.screenToWorld(pixel, viewport.width, viewport.height);
    std::optional<std::size_t> selected;
    for (std::size_t index = 0; index < snapshot.bodies.size(); ++index) {
        Server::BodySnapshot const &body = snapshot.bodies[index];
        if (std::hypot(world.x - body.position.x, world.y - body.position.y) <= body.radius) {
            selected = index;
        }
    }
    return selected;
}

Rectangle selectionBox(Server::BodySnapshot const &body, Camera const &camera, VkExtent2D viewport, double margin) {
    if (!std::isfinite(margin) || margin <= 0.0) {
        throw std::invalid_argument("A selection-box margin must be positive and finite.");
    }
    if (!std::isfinite(body.radius) || body.radius < 0.0) {
        throw std::invalid_argument("A selection box requires a non-negative, finite body radius.");
    }
    Simulation::Vector2 const center = camera.worldToPixel(body.position, viewport.width, viewport.height);
    double const halfSide = body.radius * camera.scale() * margin;
    return {
        {center.x - halfSide, center.y - halfSide},
        {center.x + halfSide, center.y + halfSide},
    };
}

} // namespace Client
