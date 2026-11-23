#include "Camera.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Initial camera scale fits the orbit diameter to 75% of the smaller viewport dimension") {
    CHECK(Client::Camera::initialScale(1280, 800) == Catch::Approx(75.0));
    CHECK(Client::Camera::initialScale(600, 900) == Catch::Approx(56.25));
}

TEST_CASE("Camera projection maps its center and viewport edges to NDC") {
    Client::Camera const camera({2.0, -3.0}, 100.0);

    Simulation::Vector2 const center = camera.worldToNDC({2.0, -3.0}, 800, 600);
    CHECK(center.x == Catch::Approx(0.0));
    CHECK(center.y == Catch::Approx(0.0));

    Simulation::Vector2 const rightEdge = camera.worldToNDC({6.0, -3.0}, 800, 600);
    CHECK(rightEdge.x == Catch::Approx(1.0));
    CHECK(rightEdge.y == Catch::Approx(0.0));

    Simulation::Vector2 const topEdge = camera.worldToNDC({2.0, 0.0}, 800, 600);
    CHECK(topEdge.x == Catch::Approx(0.0));
    CHECK(topEdge.y == Catch::Approx(-1.0));
}

TEST_CASE("Camera pan and centered zoom change view state") {
    Client::Camera camera({1.0, 2.0}, 50.0);
    camera.pan(-3.0, 4.0);
    CHECK(camera.center().x == Catch::Approx(-2.0));
    CHECK(camera.center().y == Catch::Approx(6.0));

    camera.zoomIn();
    CHECK(camera.scale() == Catch::Approx(55.0));
    camera.zoomOut();
    CHECK(camera.scale() == Catch::Approx(50.0));
}

TEST_CASE("Zooming at a world point keeps it fixed at the cursor") {
    Client::Camera camera({1.0, 2.0}, 80.0);
    Simulation::Vector2 const cursorWorld = camera.screenToWorld({100.0, 500.0}, 800, 600);

    camera.zoomAt(cursorWorld, 1.1);

    Simulation::Vector2 const cursorAfterZoom = camera.worldToNDC(cursorWorld, 800, 600);
    CHECK(cursorAfterZoom.x == Catch::Approx(-0.75));
    CHECK(cursorAfterZoom.y == Catch::Approx(2.0 / 3.0));
    CHECK(camera.scale() == Catch::Approx(88.0));
}
