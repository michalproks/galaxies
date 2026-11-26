#include "Server.h"

#include <algorithm>
#include <iostream>
#include <thread>

namespace Server {

Server::Server()
    : mEconomy(std::max(1U, std::thread::hardware_concurrency()))
    , mLastTick(Clock::now()) {
}

GameSnapshot Server::snapshot(double const gameHours) {
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
    if (mConsecutiveTickFailures >= MAX_TICK_FAILURES) {
        result.economyEvents = mEconomy.takePendingEvents();
    } else {
        try {
            result.economyEvents = mEconomy.advanceTo(gameHours);
            mConsecutiveTickFailures = 0;
        } catch (std::exception const &error) {
            // A failed period is rejected atomically. Retrying a deterministic failure forever
            // would stall every later turn, so log a bounded number of attempts and then stop
            // advancing the economy instead.
            ++mConsecutiveTickFailures;
            std::cerr << "Economic turn rejected: " << error.what() << '\n';
            if (mConsecutiveTickFailures == MAX_TICK_FAILURES) {
                std::cerr << "Economic ticking disabled after repeated failures\n";
            }
            result.economyEvents = mEconomy.takePendingEvents();
        }
    }
    if (mEconomyRecords == nullptr || mLastRecordPeriod != mEconomy.completedPeriods()) {
        mEconomyRecords = std::make_shared<std::vector<Simulation::Economy::ColonyEconomyRecordSnapshot> const>(
            mEconomy.recordSnapshots());
        mLastRecordPeriod = mEconomy.completedPeriods();
    }
    result.economyRecords = mEconomyRecords;
    return result;
}

GameSnapshot Server::snapshot(TimePoint const now) {
    double const elapsedSeconds = std::max(0.0, std::chrono::duration<double>(now - mLastTick).count());
    mGameHours += elapsedSeconds * Simulation::daysPerSecond(mSpeed) * 24.0;
    mLastTick = now;
    return snapshot(mGameHours);
}

GameSnapshot Server::snapshot() {
    return snapshot(Clock::now());
}

void Server::setSpeed(Simulation::GameSpeed const speed) {
    mSpeed = speed;
}

Simulation::GameSpeed Server::speed() const {
    return mSpeed;
}

} // namespace Server
