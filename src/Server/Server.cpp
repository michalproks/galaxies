#include "Server.h"

#include <algorithm>

namespace Server {

Server::Server()
    : mLastTick(Clock::now()) {
}

GameSnapshot Server::snapshot(double gameHours) const {
    GameSnapshot result;
    result.gameHours = gameHours;
    result.speed = mSpeed;
    std::array<Simulation::Body, 4> const &bodies = mPlanetarySystem.bodies();
    result.bodies.reserve(bodies.size());
    for (std::size_t bodyIndex = 0; bodyIndex < bodies.size(); ++bodyIndex) {
        Simulation::Body const &body = bodies[bodyIndex];
        result.bodies.push_back(
            {Simulation::worldPosition(bodies, bodyIndex, gameHours), body.radius, body.color, body.kind, body.name});
    }
    return result;
}

GameSnapshot Server::snapshot(TimePoint now) {
    double const elapsedSeconds = std::max(0.0, std::chrono::duration<double>(now - mLastTick).count());
    mGameHours += elapsedSeconds * Simulation::daysPerSecond(mSpeed) * 24.0;
    mLastTick = now;
    return snapshot(mGameHours);
}

GameSnapshot Server::snapshot() {
    return snapshot(Clock::now());
}

void Server::setSpeed(Simulation::GameSpeed speed) {
    mSpeed = speed;
}

Simulation::GameSpeed Server::speed() const {
    return mSpeed;
}

} // namespace Server
