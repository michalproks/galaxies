#include "Simulation.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace Simulation {
namespace {

    constexpr std::array<int, 12> DAYS_PER_MONTH{28, 28, 35, 28, 28, 35, 28, 28, 35, 28, 28, 35};
    constexpr std::uint64_t HOURS_PER_DAY = 24;
    constexpr std::uint64_t DAYS_PER_YEAR = 364;
    constexpr std::uint64_t HOURS_PER_YEAR = DAYS_PER_YEAR * HOURS_PER_DAY;

} // namespace

double daysPerSecond(GameSpeed speed) {
    switch (speed) {
    case GameSpeed::NORMAL:
        return 1.0;
    case GameSpeed::FAST:
        return 2.0;
    case GameSpeed::ULTRA:
        return 4.0;
    case GameSpeed::PAUSED:
        return 0.0;
    }
    throw std::invalid_argument("The game speed is not recognized.");
}

DateTime dateTimeFromGameHours(double gameHours) {
    if (!std::isfinite(gameHours) || gameHours < 0.0) {
        throw std::invalid_argument("Game hours must be non-negative and finite.");
    }
    long double const wholeHours = std::floor(static_cast<long double>(gameHours));
    long double const maxHours =
        static_cast<long double>(std::numeric_limits<int>::max() - 2100) * HOURS_PER_YEAR + (HOURS_PER_YEAR - 1);
    if (wholeHours > maxHours) {
        throw std::invalid_argument("The game date year exceeds the supported range.");
    }

    std::uint64_t remainingHours = static_cast<std::uint64_t>(wholeHours);
    int const year = 2100 + static_cast<int>(remainingHours / HOURS_PER_YEAR);
    remainingHours %= HOURS_PER_YEAR;
    int month = 1;
    for (int days : DAYS_PER_MONTH) {
        std::uint64_t const monthHours = static_cast<std::uint64_t>(days) * HOURS_PER_DAY;
        if (remainingHours < monthHours) {
            break;
        }
        remainingHours -= monthHours;
        ++month;
    }
    int const day = static_cast<int>(remainingHours / HOURS_PER_DAY) + 1;
    int const hour = static_cast<int>(remainingHours % HOURS_PER_DAY);
    return {year, month, day, hour};
}

std::string format(DateTime const &dateTime) {
    std::ostringstream result;
    result << std::setfill('0') << std::setw(4) << dateTime.year << '-' << std::setw(2) << dateTime.month << '-'
           << std::setw(2) << dateTime.day << ' ' << std::setw(2) << dateTime.hour << ":00";
    return result.str();
}

PlanetarySystem::PlanetarySystem()
    : mBodies{{
          {1.0, {1.0F, 1.0F, 0.0F}, 0.0, 0.0, -1, BodyKind::STAR, "Sol"},
          {0.25, {0.0F, 0.0F, 1.0F}, 4.0, 365.0, 0, BodyKind::PLANET, "Earth"},
          {0.1, {0.8F, 0.8F, 0.8F}, 1.0, 28.0, 1, BodyKind::MOON, "Luna"},
          {0.2, {1.0F, 0.0F, 0.0F}, 6.0, 687.0, 0, BodyKind::PLANET, "Mars"},
      }} {
}

std::array<Body, 4> const &PlanetarySystem::bodies() const {
    return mBodies;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the public API follows worldPosition(bodies, bodyIndex, hours).
Vector2 worldPosition(std::span<Body const> bodies, std::size_t bodyIndex, double gameHours) {
    if (bodyIndex >= bodies.size()) {
        throw std::out_of_range("The body index is outside the planetary system.");
    }
    if (!std::isfinite(gameHours)) {
        throw std::invalid_argument("Game hours must be finite.");
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
        double const angle = 2.0 * std::numbers::pi_v<double> * gameHours / (body.orbitPeriod * 24.0);
        position.x += body.orbitRadius * std::cos(angle);
        position.y += body.orbitRadius * std::sin(angle);
    }
    return position;
}

} // namespace Simulation
