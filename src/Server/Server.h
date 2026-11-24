#ifndef GALAXIES_SERVER_SERVER_H
#define GALAXIES_SERVER_SERVER_H

#include "Simulation.h"

#include <chrono>
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

struct GameSnapshot {
    double time{}; /**< Authoritative simulation time in seconds. */
    std::vector<BodySnapshot> bodies;
};

class GameClock {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    /** Starts a clock at the current steady-clock time. */
    GameClock();

    /** Starts a clock at a supplied steady-clock timestamp. */
    explicit GameClock(TimePoint startTime);

    /** Maps a steady-clock timestamp to elapsed simulation seconds since this clock's start. */
    [[nodiscard]] double timeAt(TimePoint now) const;

private:
    TimePoint mStartTime;
};

class Server {
public:
    /** Creates the authoritative planetary system and starts its game clock. */
    Server();

    /** Builds a render-ready snapshot for a finite explicit simulation time, in seconds. */
    [[nodiscard]] GameSnapshot snapshot(double simulationTime) const;

    /** Advances the owned game clock to the supplied steady-clock time and returns its snapshot. */
    [[nodiscard]] GameSnapshot snapshot(GameClock::TimePoint now) const;

    /** Returns a snapshot at the current steady-clock time. */
    [[nodiscard]] GameSnapshot snapshot() const;

private:
    Simulation::PlanetarySystem mPlanetarySystem;
    GameClock mClock;
};

} // namespace Server

#endif // GALAXIES_SERVER_SERVER_H
