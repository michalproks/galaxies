#ifndef GALAXIES_SIMULATION_SIMULATION_H
#define GALAXIES_SIMULATION_SIMULATION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace Simulation {

/** A two-dimensional position or displacement measured in world units. */
struct Vector2 {
    double x{};
    double y{};
};

/** An RGB color with normalized channels in the range [0, 1]. */
struct Color {
    float red{};
    float green{};
    float blue{};
};

/** Identifies the visual category of a simulated celestial body. */
enum class BodyKind : std::uint8_t {
    STAR,
    PLANET,
    MOON,
};

struct Body {
    double radius{};      /**< Physical radius in world units. */
    Color color;          /**< Render color assigned by the authoritative simulation. */
    double orbitRadius{}; /**< Distance from the parent in world units. */
    double orbitPeriod{}; /**< Orbit duration in seconds; positive for orbiting bodies. */
    int parent{-1};       /**< Index into the body list, or -1 for a root body. */
    BodyKind kind{BodyKind::STAR};
    std::string name; /**< Display name of the body, e.g. "Sol". */
};

class PlanetarySystem {
public:
    /** Creates the fixed star, planet, and moon system. */
    PlanetarySystem();

    /** Returns the fixed Sol star, Earth and Mars planets, and Luna moon in parent-before-child order. */
    [[nodiscard]] std::array<Body, 4> const &bodies() const;

private:
    std::array<Body, 4> mBodies;
};

/** Returns a body's deterministic world position at simulation time in seconds.
 * The body index must be in range, time must be finite, parent indexes must be valid, and orbiting bodies must have a
 * positive finite period and finite orbit radius.
 * Throws std::out_of_range for an invalid index or parent and std::invalid_argument for a cycle or invalid period.
 */
[[nodiscard]] Vector2 worldPosition(std::span<Body const> bodies,
                                    std::size_t bodyIndex,
                                    double time); // NOLINT(bugprone-easily-swappable-parameters)

} // namespace Simulation

#endif // GALAXIES_SIMULATION_SIMULATION_H
