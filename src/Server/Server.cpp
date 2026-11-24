#include "Server.h"

#include <algorithm>

namespace Server {

GameClock::GameClock()
    : GameClock(Clock::now()) {
}

GameClock::GameClock(TimePoint startTime)
    : mStartTime(startTime) {
}

double GameClock::timeAt(TimePoint now) const {
    return std::max(0.0, std::chrono::duration<double>(now - mStartTime).count());
}

Server::Server() = default;

GameSnapshot Server::snapshot(double simulationTime) const {
    GameSnapshot result;
    result.time = simulationTime;
    std::array<Simulation::Body, 3> const &bodies = mPlanetarySystem.bodies();
    result.bodies.reserve(bodies.size());
    for (std::size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex) {
        Simulation::Body const &body = bodies[bodyIndex];
        result.bodies.push_back(
            {Simulation::worldPosition(bodies, bodyIndex, simulationTime), body.radius, body.color, body.kind});
    }
    return result;
}

GameSnapshot Server::snapshot(GameClock::TimePoint now) const {
    return snapshot(mClock.timeAt(now));
}

GameSnapshot Server::snapshot() const {
    return snapshot(GameClock::Clock::now());
}

} // namespace Server
