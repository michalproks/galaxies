#include "Server.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

TEST_CASE("A server snapshot contains the planetary system at the requested game hour") {
    Server::Server const server;
    Server::GameSnapshot const snapshot = server.snapshot(7.5);

    REQUIRE(snapshot.gameHours == Catch::Approx(7.5));
    REQUIRE(snapshot.speed == Simulation::GameSpeed::NORMAL);
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
    double const earthAngle = 2.0 * std::numbers::pi_v<double> * snapshot.gameHours / (365.0 * 24.0);
    CHECK(snapshot.bodies[1].position.x == Catch::Approx(4.0 * std::cos(earthAngle)));
    CHECK(snapshot.bodies[1].position.y == Catch::Approx(4.0 * std::sin(earthAngle)));

    CHECK(snapshot.bodies[2].radius == Catch::Approx(0.1));
    CHECK(snapshot.bodies[2].color.red == Catch::Approx(0.8F));
    CHECK(snapshot.bodies[3].radius == Catch::Approx(0.2));
    CHECK(snapshot.bodies[3].color.red == Catch::Approx(1.0F));
    CHECK(snapshot.bodies[3].color.green == Catch::Approx(0.0F));
    CHECK(snapshot.bodies[3].color.blue == Catch::Approx(0.0F));
    double const marsAngle = 2.0 * std::numbers::pi_v<double> * snapshot.gameHours / (687.0 * 24.0);
    CHECK(snapshot.bodies[3].position.x == Catch::Approx(6.0 * std::cos(marsAngle)));
    CHECK(snapshot.bodies[3].position.y == Catch::Approx(6.0 * std::sin(marsAngle)));
}

TEST_CASE("Server snapshots integrate wall-clock time at the current speed") {
    Server::Server::TimePoint const start = Server::Server::Clock::now();
    Server::Server server;
    Server::GameSnapshot const initial = server.snapshot(start);
    CHECK(initial.gameHours == Catch::Approx(0.0));
    CHECK(initial.speed == Simulation::GameSpeed::NORMAL);

    Server::GameSnapshot const normal = server.snapshot(start + std::chrono::seconds(2));
    CHECK(normal.gameHours == Catch::Approx(48.0));

    server.setSpeed(Simulation::GameSpeed::FAST);
    CHECK(server.speed() == Simulation::GameSpeed::FAST);
    Server::GameSnapshot const fast = server.snapshot(start + std::chrono::seconds(3));
    CHECK(fast.gameHours == Catch::Approx(96.0));

    server.setSpeed(Simulation::GameSpeed::ULTRA);
    Server::GameSnapshot const ultra = server.snapshot(start + std::chrono::seconds(4));
    CHECK(ultra.gameHours == Catch::Approx(192.0));
    CHECK(ultra.speed == Simulation::GameSpeed::ULTRA);
}
