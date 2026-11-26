#include "PriceBelief.h"
#include "EconomyConstants.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Price beliefs move toward settlement prices and reinforce filled orders") {
    using Simulation::Economy::PriceBelief;

    PriceBelief const belief{10.0, 10.0};
    PriceBelief const updated = Simulation::Economy::updatedPriceBelief(belief, 30.0, true);

    CHECK(updated.mean == Catch::Approx(14.0));
    CHECK(updated.halfWidth == Catch::Approx(9.0));
    CHECK(belief.mean == Catch::Approx(10.0));
    CHECK(belief.halfWidth == Catch::Approx(10.0));

    PriceBelief converging{0.0, 100.0};
    for (int turn = 0; turn < 20; ++turn) {
        converging = Simulation::Economy::updatedPriceBelief(converging, 80.0, true);
    }
    CHECK(converging.mean > 79.0);
    CHECK(converging.mean < 80.0);
}

TEST_CASE("Price belief half-width stays within configured bounds") {
    using Simulation::Economy::PriceBelief;

    PriceBelief const shrunk =
        Simulation::Economy::updatedPriceBelief({10.0, Simulation::Economy::BELIEF_W_MIN}, 10.0, true);
    PriceBelief const expanded =
        Simulation::Economy::updatedPriceBelief({10.0, Simulation::Economy::BELIEF_W_MAX}, 10.0, false);

    CHECK(shrunk.halfWidth == Simulation::Economy::BELIEF_W_MIN);
    CHECK(expanded.halfWidth == Simulation::Economy::BELIEF_W_MAX);
    CHECK(shrunk.halfWidth >= Simulation::Economy::BELIEF_W_MIN);
    CHECK(expanded.halfWidth <= Simulation::Economy::BELIEF_W_MAX);
}
