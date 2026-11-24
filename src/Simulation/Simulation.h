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

/** Selects the rate at which wall-clock seconds advance in-game days. */
enum class GameSpeed : std::uint8_t {
    NORMAL,
    FAST,
    ULTRA,
};

/** A calendar date and whole-hour time in the game's fixed 364-day calendar. */
struct DateTime {
    int year{};
    int month{};
    int day{};
    int hour{};

    /** Compares all calendar components. */
    bool operator==(DateTime const &) const = default;
};

/** Returns the number of in-game days advanced per wall-clock second for a speed mode.
 * Throws std::invalid_argument if speed is not a recognized mode.
 */
[[nodiscard]] double daysPerSecond(GameSpeed speed);

/** Converts non-negative, finite hours since 2100-01-01 00:00 to the fixed game calendar.
 * Fractional hours are floored. Throws std::invalid_argument for negative or non-finite input,
 * or when the resulting year cannot be represented by an int.
 */
[[nodiscard]] DateTime dateTimeFromGameHours(double gameHours);

/** Formats a date-time as zero-padded "YYYY-MM-DD HH:00". */
[[nodiscard]] std::string format(DateTime const &dateTime);

struct Body {
    double radius{};      /**< Physical radius in world units. */
    Color color;          /**< Render color assigned by the authoritative simulation. */
    double orbitRadius{}; /**< Distance from the parent in world units. */
    double orbitPeriod{}; /**< Orbit duration in in-game days; positive for orbiting bodies. */
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

/** Returns a body's deterministic world position at the supplied in-game hour.
 * The body index must be in range, gameHours must be finite, parent indexes must be valid, and orbiting bodies must
 * have a positive finite period and finite orbit radius.
 * Throws std::out_of_range for an invalid index or parent and std::invalid_argument for a cycle or invalid period.
 */
[[nodiscard]] Vector2 worldPosition(std::span<Body const> bodies,
                                    std::size_t bodyIndex,
                                    double gameHours); // NOLINT(bugprone-easily-swappable-parameters)

} // namespace Simulation

#endif // GALAXIES_SIMULATION_SIMULATION_H
