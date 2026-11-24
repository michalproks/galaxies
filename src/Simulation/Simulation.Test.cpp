#include "Simulation.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

TEST_CASE("Game speed maps to in-game days per second") {
    CHECK(Simulation::daysPerSecond(Simulation::GameSpeed::NORMAL) == Catch::Approx(1.0));
    CHECK(Simulation::daysPerSecond(Simulation::GameSpeed::FAST) == Catch::Approx(2.0));
    CHECK(Simulation::daysPerSecond(Simulation::GameSpeed::ULTRA) == Catch::Approx(4.0));
}

TEST_CASE("Game hours convert to the fixed game calendar") {
    CHECK((Simulation::dateTimeFromGameHours(0.0) == Simulation::DateTime{2100, 1, 1, 0}));
    CHECK((Simulation::dateTimeFromGameHours(1.0) == Simulation::DateTime{2100, 1, 1, 1}));
    CHECK((Simulation::dateTimeFromGameHours(24.0) == Simulation::DateTime{2100, 1, 2, 0}));
    CHECK((Simulation::dateTimeFromGameHours(28.0 * 24.0) == Simulation::DateTime{2100, 2, 1, 0}));
    CHECK((Simulation::dateTimeFromGameHours((28.0 + 28.0) * 24.0) == Simulation::DateTime{2100, 3, 1, 0}));
    CHECK(Simulation::dateTimeFromGameHours((28.0 + 28.0 + 34.0) * 24.0) == (Simulation::DateTime{2100, 3, 35, 0}));
    CHECK(Simulation::dateTimeFromGameHours((28.0 + 28.0 + 35.0) * 24.0) == (Simulation::DateTime{2100, 4, 1, 0}));
    CHECK((Simulation::dateTimeFromGameHours(364.0 * 24.0) == Simulation::DateTime{2101, 1, 1, 0}));
    CHECK((Simulation::dateTimeFromGameHours(1.99) == Simulation::DateTime{2100, 1, 1, 1}));
    CHECK((Simulation::dateTimeFromGameHours(2.0 * 364.0 * 24.0) == Simulation::DateTime{2102, 1, 1, 0}));
}

TEST_CASE("Game calendar formatting is zero-padded") {
    CHECK(Simulation::format({2100, 1, 1, 0}) == "2100-01-01 00:00");
    CHECK(Simulation::format({2100, 12, 35, 23}) == "2100-12-35 23:00");
}

TEST_CASE("Invalid game hours are rejected") {
    CHECK_THROWS_AS(Simulation::dateTimeFromGameHours(-1.0), std::invalid_argument);
    CHECK_THROWS_AS(Simulation::dateTimeFromGameHours(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
}

TEST_CASE("The planetary system has the specified world positions") {
    Simulation::PlanetarySystem const system;
    auto const &bodies = system.bodies();

    CHECK(bodies[0].kind == Simulation::BodyKind::STAR);
    CHECK(bodies[1].kind == Simulation::BodyKind::PLANET);
    CHECK(bodies[2].kind == Simulation::BodyKind::MOON);
    CHECK(bodies[3].kind == Simulation::BodyKind::PLANET);
    CHECK(bodies[3].parent == 0);
    CHECK(bodies[3].radius == Catch::Approx(0.2));
    CHECK(bodies[3].orbitRadius == Catch::Approx(6.0));
    CHECK(bodies[1].orbitPeriod == Catch::Approx(365.0));
    CHECK(bodies[2].orbitPeriod == Catch::Approx(28.0));
    CHECK(bodies[3].orbitPeriod == Catch::Approx(687.0));

    CHECK(bodies[0].name == "Sol");
    CHECK(bodies[1].name == "Earth");
    CHECK(bodies[2].name == "Luna");
    CHECK(bodies[3].name == "Mars");

    SECTION("at game hour zero") {
        CHECK(Simulation::worldPosition(bodies, 0, 0.0).x == Catch::Approx(0.0));
        CHECK(Simulation::worldPosition(bodies, 0, 0.0).y == Catch::Approx(0.0));
        CHECK(Simulation::worldPosition(bodies, 1, 0.0).x == Catch::Approx(4.0));
        CHECK(Simulation::worldPosition(bodies, 1, 0.0).y == Catch::Approx(0.0));
        CHECK(Simulation::worldPosition(bodies, 2, 0.0).x == Catch::Approx(5.0));
        CHECK(Simulation::worldPosition(bodies, 2, 0.0).y == Catch::Approx(0.0));
        CHECK(Simulation::worldPosition(bodies, 3, 0.0).x == Catch::Approx(6.0));
        CHECK(Simulation::worldPosition(bodies, 3, 0.0).y == Catch::Approx(0.0));
    }

    SECTION("a full orbit returns to the original position") {
        for (std::size_t bodyIndex = 1; bodyIndex < bodies.size(); ++bodyIndex) {
            if (bodyIndex == 2) {
                continue;
            }
            double const periodHours = bodies[bodyIndex].orbitPeriod * 24.0;
            Simulation::Vector2 const initial = Simulation::worldPosition(bodies, bodyIndex, 0.0);
            Simulation::Vector2 const wrapped = Simulation::worldPosition(bodies, bodyIndex, periodHours);
            CHECK(wrapped.x == Catch::Approx(initial.x).margin(1.0e-12));
            CHECK(wrapped.y == Catch::Approx(initial.y).margin(1.0e-12));
        }

        double const moonPeriodHours = bodies[2].orbitPeriod * 24.0;
        Simulation::Vector2 const initialPlanet = Simulation::worldPosition(bodies, 1, 0.0);
        Simulation::Vector2 const initialMoon = Simulation::worldPosition(bodies, 2, 0.0);
        Simulation::Vector2 const wrappedPlanet = Simulation::worldPosition(bodies, 1, moonPeriodHours);
        Simulation::Vector2 const wrappedMoon = Simulation::worldPosition(bodies, 2, moonPeriodHours);
        CHECK(wrappedMoon.x - wrappedPlanet.x == Catch::Approx(initialMoon.x - initialPlanet.x).margin(1.0e-12));
        CHECK(wrappedMoon.y - wrappedPlanet.y == Catch::Approx(initialMoon.y - initialPlanet.y).margin(1.0e-12));
    }

    SECTION("positions use in-game hours for orbital angles") {
        double const quarterYearHours = bodies[1].orbitPeriod * 24.0 / 4.0;
        Simulation::Vector2 const quarter = Simulation::worldPosition(bodies, 1, quarterYearHours);
        CHECK(quarter.x == Catch::Approx(0.0).margin(1.0e-12));
        CHECK(quarter.y == Catch::Approx(4.0));
        double const marsAngle = 2.0 * std::numbers::pi_v<double> * 1234.0 / (bodies[3].orbitPeriod * 24.0);
        Simulation::Vector2 const mars = Simulation::worldPosition(bodies, 3, 1234.0);
        CHECK(mars.x == Catch::Approx(6.0 * std::cos(marsAngle)));
        CHECK(mars.y == Catch::Approx(6.0 * std::sin(marsAngle)));
    }

    SECTION("the moon position is relative to the planet") {
        double const gameHours = 7.0 * 24.0;
        Simulation::Vector2 const planet = Simulation::worldPosition(bodies, 1, gameHours);
        Simulation::Vector2 const moon = Simulation::worldPosition(bodies, 2, gameHours);
        double const moonAngle = 2.0 * std::numbers::pi_v<double> * gameHours / (28.0 * 24.0);
        CHECK(moon.x - planet.x == Catch::Approx(std::cos(moonAngle)).margin(1.0e-12));
        CHECK(moon.y - planet.y == Catch::Approx(std::sin(moonAngle)).margin(1.0e-12));
    }
}
