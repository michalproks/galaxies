#include "Rng.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

TEST_CASE("Economy RNG produces the same sequence for the same colony and turn") {
    Simulation::Economy::Rng first({42}, 7);
    Simulation::Economy::Rng second({42}, 7);

    for (int index = 0; index < 32; ++index) {
        CHECK(first.next() == second.next());
    }
}

TEST_CASE("Economy RNG sequences diverge for different colony-turn seeds") {
    Simulation::Economy::Rng first({42}, 7);
    Simulation::Economy::Rng differentColony({43}, 7);
    Simulation::Economy::Rng differentTurn({42}, 8);

    std::array<std::uint64_t, 8> firstValues{};
    std::array<std::uint64_t, 8> colonyValues{};
    std::array<std::uint64_t, 8> turnValues{};
    for (std::size_t index = 0; index < firstValues.size(); ++index) {
        firstValues[index] = first.next();
        colonyValues[index] = differentColony.next();
        turnValues[index] = differentTurn.next();
    }

    CHECK(firstValues != colonyValues);
    CHECK(firstValues != turnValues);
}

TEST_CASE("Economy RNG sampling stays within requested ranges") {
    Simulation::Economy::Rng rng({1}, 99);

    for (int index = 0; index < 512; ++index) {
        double const realValue = rng.uniformDouble();
        CHECK(realValue >= 0.0);
        CHECK(realValue < 1.0);

        std::uint64_t const integerValue = rng.uniformInt(13, 29);
        CHECK(integerValue >= 13);
        CHECK(integerValue <= 29);

        CHECK(!(rng.bernoulli(0.0)));
        CHECK(rng.bernoulli(1.0));
    }

    Simulation::Economy::Rng fullRangeRng({2}, 4);
    Simulation::Economy::Rng expectedRng({2}, 4);
    CHECK(fullRangeRng.uniformInt(0, std::numeric_limits<std::uint64_t>::max()) == expectedRng.next());
    CHECK(rng.uniformInt(17, 17) == 17);
}

TEST_CASE("Economy RNG rejects invalid sampling bounds") {
    Simulation::Economy::Rng rng({1}, 99);

    CHECK_THROWS_AS(rng.uniformInt(5, 4), std::invalid_argument);
    CHECK_THROWS_AS(rng.bernoulli(-0.1), std::invalid_argument);
    CHECK_THROWS_AS(rng.bernoulli(1.1), std::invalid_argument);
    CHECK_THROWS_AS(rng.bernoulli(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
}

TEST_CASE("Economy RNG sub-streams are reproducible and independent of draw order") {
    Simulation::Economy::Rng firstParent({12}, 3);
    Simulation::Economy::Rng secondParent({12}, 3);
    Simulation::Economy::Rng firstStream = firstParent.substream(100);
    Simulation::Economy::Rng otherStream = firstParent.substream(200);
    Simulation::Economy::Rng matchingStream = secondParent.substream(100);

    for (int index = 0; index < 5; ++index) {
        static_cast<void>(otherStream.next());
    }

    Simulation::Economy::Rng differentKeyStream = firstParent.substream(200);
    Simulation::Economy::Rng freshMatchingStream = firstParent.substream(100);
    CHECK(differentKeyStream.next() != freshMatchingStream.next());

    for (int index = 0; index < 32; ++index) {
        CHECK(firstStream.next() == matchingStream.next());
    }

    Simulation::Economy::Rng parentStream = firstParent.substream(100);
    Simulation::Economy::Rng untouchedParentStream = secondParent.substream(100);
    CHECK(parentStream.next() == untouchedParentStream.next());
}
