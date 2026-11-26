#ifndef GALAXIES_SERVER_SERVER_H
#define GALAXIES_SERVER_SERVER_H

#include "Economy/EconomyTickOrchestrator.h"
#include "Simulation.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace Server {

/** Render-ready body state; all position and radius values use Simulation world units. */
struct BodySnapshot {
    Simulation::Vector2 position;
    double radius{};
    Simulation::Color color;
    Simulation::BodyKind kind{Simulation::BodyKind::STAR};
    std::string name; /**< Display name of the body. */
};

/** Render-ready game time, speed, bodies, and committed economy data for a single frame. */
struct GameSnapshot {
    double gameHours{}; /**< Authoritative in-game hours since the calendar epoch. */
    Simulation::GameSpeed speed{Simulation::GameSpeed::NORMAL}; /**< Current game speed. */
    std::vector<BodySnapshot> bodies;
    std::shared_ptr<std::vector<Simulation::Economy::ColonyEconomyRecordSnapshot> const> economyRecords;
    std::vector<Simulation::Economy::EconomyEventBatch> economyEvents;
};

class Server {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    /** Creates the authoritative planetary system, Earth economy, and game clock. */
    Server();

    /** Builds a render-ready snapshot and commits any crossed 7-day economic periods.
     * Throws std::invalid_argument if gameHours is not finite.
     */
    [[nodiscard]] GameSnapshot snapshot(double gameHours);

    /** Accumulates game hours since the prior tick using the current speed and returns a snapshot. */
    [[nodiscard]] GameSnapshot snapshot(TimePoint now);

    /** Advances to the current steady-clock time and returns a snapshot. */
    [[nodiscard]] GameSnapshot snapshot();

    /** Sets the speed used to advance game time on subsequent snapshot ticks. */
    void setSpeed(Simulation::GameSpeed speed);

    /** Returns the current game speed. */
    [[nodiscard]] Simulation::GameSpeed speed() const;

private:
    Simulation::PlanetarySystem mPlanetarySystem;
    Simulation::Economy::EconomyTickOrchestrator mEconomy;
    std::shared_ptr<std::vector<Simulation::Economy::ColonyEconomyRecordSnapshot> const> mEconomyRecords;
    std::int64_t mLastRecordPeriod{-1};
    double mGameHours{};
    Simulation::GameSpeed mSpeed{Simulation::GameSpeed::NORMAL};
    TimePoint mLastTick;
};

} // namespace Server

#endif // GALAXIES_SERVER_SERVER_H
