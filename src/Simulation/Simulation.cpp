#include "Simulation.h"

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace Simulation {

PlanetarySystem::PlanetarySystem()
    : mBodies{{
          {1.0, {1.0F, 1.0F, 0.0F}, 0.0, 0.0, -1, BodyKind::STAR},
          {0.25, {0.0F, 0.0F, 1.0F}, 4.0, 30.0, 0, BodyKind::PLANET},
          {0.1, {0.8F, 0.8F, 0.8F}, 1.0, 10.0, 1, BodyKind::MOON},
      }} {
}

std::array<Body, 3> const &PlanetarySystem::bodies() const {
    return mBodies;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the public API follows worldPosition(bodies, bodyIndex, time).
Vector2 worldPosition(std::span<Body const> bodies, std::size_t bodyIndex, double time) {
    if (bodyIndex >= bodies.size()) {
        throw std::out_of_range("The body index is outside the planetary system.");
    }
    if (!std::isfinite(time)) {
        throw std::invalid_argument("Simulation time must be finite.");
    }

    std::vector<std::size_t> ancestry;
    std::vector<bool> visited(bodies.size());
    std::size_t currentIndex = bodyIndex;
    for (;;) {
        if (currentIndex >= bodies.size()) {
            throw std::out_of_range("A body parent index is outside the planetary system.");
        }
        if (visited[currentIndex]) {
            throw std::invalid_argument("Planetary body parent relationships must not contain a cycle.");
        }
        visited[currentIndex] = true;
        ancestry.push_back(currentIndex);
        int const parent = bodies[currentIndex].parent;
        if (parent == -1) {
            break;
        }
        if (parent < -1) {
            throw std::out_of_range("A body parent index is outside the planetary system.");
        }
        currentIndex = static_cast<std::size_t>(parent);
    }

    Vector2 position{};
    for (auto iter = ancestry.rbegin(); iter != ancestry.rend(); ++iter) {
        Body const &body = bodies[*iter];
        if (*iter == ancestry.back()) {
            continue;
        }
        if (!std::isfinite(body.orbitRadius) || !std::isfinite(body.orbitPeriod) || body.orbitPeriod <= 0.0) {
            throw std::invalid_argument("Orbiting bodies must have a finite radius and positive, finite period.");
        }
        double const angle = 2.0 * std::numbers::pi_v<double> * time / body.orbitPeriod;
        position.x += body.orbitRadius * std::cos(angle);
        position.y += body.orbitRadius * std::sin(angle);
    }
    return position;
}

} // namespace Simulation
