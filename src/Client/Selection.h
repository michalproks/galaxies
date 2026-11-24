#ifndef GALAXIES_CLIENT_SELECTION_H
#define GALAXIES_CLIENT_SELECTION_H

#include "Camera.h"

#include <Server.h>
#include <vulkan/vulkan.h>

#include <cstddef>
#include <optional>

namespace Client {

/** A top-left-origin pixel rectangle. */
struct Rectangle {
    Simulation::Vector2 min;
    Simulation::Vector2 max;
};

/** Returns the last-drawn body containing the finite pixel position, or no value for empty space.
 * The viewport dimensions must be non-zero.
 */
[[nodiscard]] std::optional<std::size_t>
hitTest(Server::GameSnapshot const &snapshot, Camera const &camera, VkExtent2D viewport, Simulation::Vector2 pixel);

/** Returns a screen-space square centered on the body; margin must be positive and finite.
 * Bodies off-screen are represented without clipping.
 */
[[nodiscard]] Rectangle
selectionBox(Server::BodySnapshot const &body, Camera const &camera, VkExtent2D viewport, double margin);

} // namespace Client

#endif // GALAXIES_CLIENT_SELECTION_H
