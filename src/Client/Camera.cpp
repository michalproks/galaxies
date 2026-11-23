#include "Camera.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Client {

Camera::Camera(Simulation::Vector2 center, double scale)
    : mCenter(center)
    , mScale(scale) {
    if (!std::isfinite(mCenter.x) || !std::isfinite(mCenter.y) || !std::isfinite(mScale) || mScale <= 0.0) {
        throw std::invalid_argument("A camera requires a finite center and positive, finite scale.");
    }
}

void Camera::pan(double dx, double dy) {
    Simulation::Vector2 const nextCenter{mCenter.x + dx, mCenter.y + dy};
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(nextCenter.x) || !std::isfinite(nextCenter.y)) {
        throw std::invalid_argument("A camera pan requires a finite displacement and representable center.");
    }
    mCenter = nextCenter;
}

void Camera::zoomIn() {
    zoomAt(mCenter, ZOOM_FACTOR);
}

void Camera::zoomOut() {
    zoomAt(mCenter, 1.0 / ZOOM_FACTOR);
}

void Camera::zoomAt(Simulation::Vector2 worldPoint, double factor) {
    if (!std::isfinite(worldPoint.x) || !std::isfinite(worldPoint.y) || !std::isfinite(factor) || factor <= 0.0) {
        throw std::invalid_argument("A camera zoom requires a finite world point and positive, finite factor.");
    }

    double const nextScale = mScale * factor;
    if (!std::isfinite(nextScale) || nextScale <= 0.0) {
        throw std::invalid_argument("The requested camera zoom is outside the representable scale range.");
    }
    Simulation::Vector2 const nextCenter{
        worldPoint.x - (worldPoint.x - mCenter.x) / factor,
        worldPoint.y - (worldPoint.y - mCenter.y) / factor,
    };
    if (!std::isfinite(nextCenter.x) || !std::isfinite(nextCenter.y)) {
        throw std::invalid_argument("The requested camera zoom produced an unrepresentable center.");
    }
    mCenter = nextCenter;
    mScale = nextScale;
}

Simulation::Vector2
Camera::worldToNDC(Simulation::Vector2 worldPosition, std::uint32_t viewportW, std::uint32_t viewportH) const {
    if (viewportW == 0 || viewportH == 0) {
        throw std::invalid_argument("Camera projection requires a non-zero viewport.");
    }
    if (!std::isfinite(worldPosition.x) || !std::isfinite(worldPosition.y)) {
        throw std::invalid_argument("Camera projection requires a finite world position.");
    }
    return {
        2.0 * (worldPosition.x - mCenter.x) * mScale / static_cast<double>(viewportW),
        -2.0 * (worldPosition.y - mCenter.y) * mScale / static_cast<double>(viewportH),
    };
}

Simulation::Vector2
Camera::screenToWorld(Simulation::Vector2 screenPosition, std::uint32_t viewportW, std::uint32_t viewportH) const {
    if (viewportW == 0 || viewportH == 0) {
        throw std::invalid_argument("Camera unprojection requires a non-zero viewport.");
    }
    if (!std::isfinite(screenPosition.x) || !std::isfinite(screenPosition.y)) {
        throw std::invalid_argument("Camera unprojection requires a finite screen position.");
    }
    return {
        mCenter.x + (screenPosition.x - static_cast<double>(viewportW) / 2.0) / mScale,
        mCenter.y - (screenPosition.y - static_cast<double>(viewportH) / 2.0) / mScale,
    };
}

double Camera::initialScale(std::uint32_t viewportW, std::uint32_t viewportH) {
    if (viewportW == 0 || viewportH == 0) {
        throw std::invalid_argument("Camera initialization requires a non-zero viewport.");
    }
    return 0.75 * static_cast<double>(std::min(viewportW, viewportH)) / 8.0;
}

Simulation::Vector2 Camera::center() const {
    return mCenter;
}

double Camera::scale() const {
    return mScale;
}

} // namespace Client
