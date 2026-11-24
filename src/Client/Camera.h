#ifndef GALAXIES_CLIENT_CAMERA_H
#define GALAXIES_CLIENT_CAMERA_H

#include "Simulation.h"

#include <cstdint>

namespace Client {

class Camera {
public:
    /** Creates a camera centered at the origin with a scale of one pixel per world unit. */
    Camera() = default;

    /** Creates a camera with the supplied finite center and positive, finite pixels-per-unit scale. */
    Camera(Simulation::Vector2 center, double scale);

    /** Moves the camera center by a finite world-space displacement. */
    void pan(double dx, double dy);

    /** Multiplies the pixels-per-world-unit scale by the fixed zoom factor around the viewport center.
     * Throws std::invalid_argument if the new scale is not representable.
     */
    void zoomIn();

    /** Divides the pixels-per-world-unit scale by the fixed zoom factor around the viewport center.
     * Throws std::invalid_argument if the new scale is not representable.
     */
    void zoomOut();

    /** Changes the scale while keeping a finite worldPoint at the same screen location; factor must be positive and
     * finite.
     */
    void zoomAt(Simulation::Vector2 worldPoint, double factor);

    /** Converts a world position to NDC, inverting world Y for the screen's downward-positive axis.
     * The position must be finite and viewport dimensions must be non-zero.
     */
    [[nodiscard]] Simulation::Vector2
    worldToNDC(Simulation::Vector2 worldPosition, std::uint32_t viewportW, std::uint32_t viewportH) const;

    /** Returns the top-left-origin pixel position of a finite world position; viewport dimensions must be non-zero.
     */
    [[nodiscard]] Simulation::Vector2
    worldToPixel(Simulation::Vector2 worldPosition, std::uint32_t viewportW, std::uint32_t viewportH) const;

    /** Returns the world point under a finite top-left-origin pixel position; viewport dimensions must be non-zero. */
    [[nodiscard]] Simulation::Vector2
    screenToWorld(Simulation::Vector2 screenPosition, std::uint32_t viewportW, std::uint32_t viewportH) const;

    /** Returns a scale that makes the eight-unit orbit diameter occupy 75% of the smaller viewport dimension.
     * Both viewport dimensions must be positive.
     */
    [[nodiscard]] static double initialScale(std::uint32_t viewportW, std::uint32_t viewportH);

    /** Returns the current world-space camera center. */
    [[nodiscard]] Simulation::Vector2 center() const;

    /** Returns the current scale in pixels per world unit. */
    [[nodiscard]] double scale() const;

private:
    static constexpr double ZOOM_FACTOR = 1.1;

    Simulation::Vector2 mCenter{};
    double mScale{1.0};
};

} // namespace Client

#endif // GALAXIES_CLIENT_CAMERA_H
