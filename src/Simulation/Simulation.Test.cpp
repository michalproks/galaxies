#include "Simulation.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("The planetary system has the specified world positions") {
    Simulation::PlanetarySystem const system;
    auto const bodies = system.bodies();

    CHECK(bodies[0].kind == Simulation::BodyKind::STAR);
    CHECK(bodies[1].kind == Simulation::BodyKind::PLANET);
    CHECK(bodies[2].kind == Simulation::BodyKind::MOON);

    SECTION("at time zero") {
        CHECK(Simulation::worldPosition(bodies, 0, 0.0).x == Catch::Approx(0.0));
        CHECK(Simulation::worldPosition(bodies, 0, 0.0).y == Catch::Approx(0.0));
        CHECK(Simulation::worldPosition(bodies, 1, 0.0).x == Catch::Approx(4.0));
        CHECK(Simulation::worldPosition(bodies, 1, 0.0).y == Catch::Approx(0.0));
        CHECK(Simulation::worldPosition(bodies, 2, 0.0).x == Catch::Approx(5.0));
        CHECK(Simulation::worldPosition(bodies, 2, 0.0).y == Catch::Approx(0.0));
    }

    SECTION("at quarter and half planetary periods") {
        Simulation::Vector2 const quarter = Simulation::worldPosition(bodies, 1, 7.5);
        CHECK(quarter.x == Catch::Approx(0.0).margin(1.0e-12));
        CHECK(quarter.y == Catch::Approx(4.0));

        Simulation::Vector2 const half = Simulation::worldPosition(bodies, 1, 15.0);
        CHECK(half.x == Catch::Approx(-4.0));
        CHECK(half.y == Catch::Approx(0.0).margin(1.0e-12));
    }

    SECTION("the planetary orbit wraps after a full period") {
        Simulation::Vector2 const initial = Simulation::worldPosition(bodies, 1, 0.0);
        Simulation::Vector2 const wrapped = Simulation::worldPosition(bodies, 1, 30.0);
        CHECK(wrapped.x == Catch::Approx(initial.x));
        CHECK(wrapped.y == Catch::Approx(initial.y).margin(1.0e-12));
    }

    SECTION("the moon position is relative to the planet") {
        Simulation::Vector2 const planet = Simulation::worldPosition(bodies, 1, 2.5);
        Simulation::Vector2 const moon = Simulation::worldPosition(bodies, 2, 2.5);
        CHECK(moon.x - planet.x == Catch::Approx(0.0).margin(1.0e-12));
        CHECK(moon.y - planet.y == Catch::Approx(1.0));
    }

    SECTION("the star remains fixed") {
        Simulation::Vector2 const star = Simulation::worldPosition(bodies, 0, 123.456);
        CHECK(star.x == Catch::Approx(0.0));
        CHECK(star.y == Catch::Approx(0.0));
    }
}
