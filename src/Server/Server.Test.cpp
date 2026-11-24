#include "Server.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

TEST_CASE("A server snapshot contains the planetary system at the requested time") {
    Server::Server const server;
    Server::GameSnapshot const snapshot = server.snapshot(7.5);

    REQUIRE(snapshot.time == Catch::Approx(7.5));
    REQUIRE(snapshot.bodies.size() == 4);

    CHECK(snapshot.bodies[0].kind == Simulation::BodyKind::STAR);
    CHECK(snapshot.bodies[1].kind == Simulation::BodyKind::PLANET);
    CHECK(snapshot.bodies[2].kind == Simulation::BodyKind::MOON);
    CHECK(snapshot.bodies[3].kind == Simulation::BodyKind::PLANET);
    CHECK(snapshot.bodies[0].name == "Sol");
    CHECK(snapshot.bodies[1].name == "Earth");
    CHECK(snapshot.bodies[2].name == "Luna");
    CHECK(snapshot.bodies[3].name == "Mars");

    CHECK(snapshot.bodies[0].radius == Catch::Approx(1.0));
    CHECK(snapshot.bodies[0].color.red == Catch::Approx(1.0F));
    CHECK(snapshot.bodies[0].color.green == Catch::Approx(1.0F));
    CHECK(snapshot.bodies[0].color.blue == Catch::Approx(0.0F));
    CHECK(snapshot.bodies[0].position.x == Catch::Approx(0.0));
    CHECK(snapshot.bodies[0].position.y == Catch::Approx(0.0));

    CHECK(snapshot.bodies[1].radius == Catch::Approx(0.25));
    CHECK(snapshot.bodies[1].color.blue == Catch::Approx(1.0F));
    CHECK(snapshot.bodies[1].position.x == Catch::Approx(0.0).margin(1.0e-12));
    CHECK(snapshot.bodies[1].position.y == Catch::Approx(4.0));

    CHECK(snapshot.bodies[2].radius == Catch::Approx(0.1));
    CHECK(snapshot.bodies[2].color.red == Catch::Approx(0.8F));
    CHECK(snapshot.bodies[2].position.x == Catch::Approx(0.0).margin(1.0e-12));
    CHECK(snapshot.bodies[2].position.y == Catch::Approx(3.0));

    CHECK(snapshot.bodies[3].radius == Catch::Approx(0.2));
    CHECK(snapshot.bodies[3].color.red == Catch::Approx(1.0F));
    CHECK(snapshot.bodies[3].color.green == Catch::Approx(0.0F));
    CHECK(snapshot.bodies[3].color.blue == Catch::Approx(0.0F));
    double const marsAngle = 2.0 * std::numbers::pi * snapshot.time / 40.0;
    CHECK(snapshot.bodies[3].position.x == Catch::Approx(6.0 * std::cos(marsAngle)));
    CHECK(snapshot.bodies[3].position.y == Catch::Approx(6.0 * std::sin(marsAngle)));

    Simulation::PlanetarySystem const system;
    for (std::size_t bodyIndex = 0; bodyIndex < system.bodies().size(); ++bodyIndex) {
        Simulation::Vector2 const expected = Simulation::worldPosition(system.bodies(), bodyIndex, snapshot.time);
        CHECK(snapshot.bodies[bodyIndex].position.x == Catch::Approx(expected.x));
        CHECK(snapshot.bodies[bodyIndex].position.y == Catch::Approx(expected.y));
    }
}

TEST_CASE("The server maps wall-clock time to simulation time") {
    Server::GameClock::TimePoint const start{};
    Server::GameClock const clock(start);
    CHECK(clock.timeAt(start + std::chrono::seconds(12)) == Catch::Approx(12.0));
    CHECK(clock.timeAt(start - std::chrono::seconds(2)) == Catch::Approx(0.0));
}

TEST_CASE("A server snapshot uses its owned clock for wall-clock input") {
    Server::Server const server;
    Server::GameClock::TimePoint const now = Server::GameClock::Clock::now() + std::chrono::seconds(5);
    Server::GameSnapshot const snapshot = server.snapshot(now);
    CHECK(snapshot.time == Catch::Approx(5.0).margin(0.1));
    CHECK(snapshot.bodies[1].position.x == Catch::Approx(4.0 * std::cos(2.0 * std::numbers::pi / 6.0)).margin(0.1));
}
