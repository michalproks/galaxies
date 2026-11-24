#include "Selection.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

TEST_CASE("Hit testing selects each body and misses empty space") {
    Client::Camera const camera({}, 100.0);
    Server::GameSnapshot const snapshot{0.0,
                                        Simulation::GameSpeed::NORMAL,
                                        {{{-2.0, 0.0}, 1.0, {}, Simulation::BodyKind::STAR, "Sol"},
                                         {{0.0, 0.0}, 0.25, {}, Simulation::BodyKind::PLANET, "Earth"},
                                         {{2.0, 0.0}, 0.1, {}, Simulation::BodyKind::MOON, "Luna"}}};

    CHECK(Client::hitTest(snapshot, camera, {800, 600}, {200.0, 300.0}) == 0);
    CHECK(Client::hitTest(snapshot, camera, {800, 600}, {400.0, 300.0}) == 1);
    CHECK(Client::hitTest(snapshot, camera, {800, 600}, {600.0, 300.0}) == 2);
    CHECK(!Client::hitTest(snapshot, camera, {800, 600}, {700.0, 300.0}).has_value());
}

TEST_CASE("Hit testing chooses the last body drawn when bodies overlap") {
    Client::Camera const camera({}, 100.0);
    Server::GameSnapshot const snapshot{0.0,
                                        Simulation::GameSpeed::NORMAL,
                                        {{{0.0, 0.0}, 1.0, {}, Simulation::BodyKind::PLANET, "Earth"},
                                         {{0.0, 0.0}, 0.1, {}, Simulation::BodyKind::MOON, "Luna"}}};

    CHECK(Client::hitTest(snapshot, camera, {800, 600}, {400.0, 300.0}) == 1);
}

TEST_CASE("Selection boxes are symmetric and retain off-screen geometry") {
    Client::Camera const camera({}, 100.0);
    Server::BodySnapshot const body{{0.0, 0.0}, 1.0, {}, Simulation::BodyKind::STAR, "Sol"};

    Client::Rectangle const box = Client::selectionBox(body, camera, {800, 600}, 1.25);
    CHECK(box.min.x == Catch::Approx(275.0));
    CHECK(box.max.x == Catch::Approx(525.0));
    CHECK(box.min.y == Catch::Approx(175.0));
    CHECK(box.max.y == Catch::Approx(425.0));

    Server::BodySnapshot const offScreen{{10.0, 0.0}, 1.0, {}, Simulation::BodyKind::STAR, "Sol"};
    Client::Rectangle const offScreenBox = Client::selectionBox(offScreen, camera, {800, 600}, 1.25);
    CHECK(offScreenBox.min.x > 800.0);
}

TEST_CASE("Selection rejects zero-sized viewports") {
    Client::Camera const camera;
    Server::GameSnapshot const snapshot;

    CHECK_THROWS_AS(Client::hitTest(snapshot, camera, {0, 600}, {0.0, 0.0}), std::invalid_argument);
    Server::BodySnapshot const body{{}, 1.0, {}, Simulation::BodyKind::STAR, "Sol"};
    CHECK_THROWS_AS(Client::selectionBox(body, camera, {800, 0}, 1.25), std::invalid_argument);
}
