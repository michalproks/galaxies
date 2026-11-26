#include "Founding.h"
#include "EconomyConstants.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

namespace {

std::size_t resourceIndex(Simulation::Economy::ResourceId resource) {
    return static_cast<std::size_t>(resource);
}

} // namespace

TEST_CASE("Founding probabilities and sector weights match the worked example") {
    using Simulation::Economy::FoundingMarketData;
    using Simulation::Economy::FoundingWealthRange;
    using Simulation::Economy::ResourceId;

    FoundingWealthRange const wealthRange{12'000'000, 100'000'000};
    CHECK(Simulation::Economy::foundingProbability(12'000'000, wealthRange) == Catch::Approx(0.0));
    CHECK(Simulation::Economy::foundingProbability(30'000'000, wealthRange) == Catch::Approx(0.0102272727));
    CHECK(Simulation::Economy::foundingProbability(60'000'000, wealthRange) == Catch::Approx(0.0272727273));
    CHECK(Simulation::Economy::foundingProbability(100'000'000, wealthRange) == Catch::Approx(0.05));

    FoundingMarketData marketData;
    marketData.demand[resourceIndex(ResourceId::Food)] = 12'000'000;
    marketData.filled[resourceIndex(ResourceId::Food)] = 10'000'000;
    marketData.settlementPrices[resourceIndex(ResourceId::Food)] = 4;
    marketData.demand[resourceIndex(ResourceId::Materials)] = 3'000'000;
    marketData.filled[resourceIndex(ResourceId::Materials)] = 2'500'000;
    marketData.settlementPrices[resourceIndex(ResourceId::Materials)] = 6;
    marketData.settlementPrices[resourceIndex(ResourceId::Fuel)] = 5;
    marketData.settlementPrices[resourceIndex(ResourceId::Energy)] = 2;
    marketData.settlementPrices[resourceIndex(ResourceId::Service)] = 10;
    marketData.demand[resourceIndex(ResourceId::ConsumerGoods)] = 400'000;
    marketData.filled[resourceIndex(ResourceId::ConsumerGoods)] = 250'000;
    marketData.settlementPrices[resourceIndex(ResourceId::ConsumerGoods)] = 30;

    auto const weights = Simulation::Economy::sectorWeightsFor(marketData);
    CHECK(weights[0] == 9'000'000);
    CHECK(weights[1] == 4'000'000);
    CHECK(weights[2] == 1'000'000);
    CHECK(weights[3] == 1'000'000);
    CHECK(weights[4] == 1'000'000);
    CHECK(weights[5] == 5'500'000);
}

TEST_CASE("Equal wealth receives the full founding chance and no eligible households found") {
    using Simulation::Economy::ColonyEconomy;
    using Simulation::Economy::Corporation;
    using Simulation::Economy::FoundingMarketData;
    using Simulation::Economy::FoundingWealthRange;
    using Simulation::Economy::Household;
    using Simulation::Economy::HouseholdId;
    using Simulation::Economy::MIN_CAPITAL;
    using Simulation::Economy::Rng;

    CHECK(Simulation::Economy::foundingProbability(50'000'000, {50'000'000, 50'000'000}) ==
          Catch::Approx(Simulation::Economy::FOUNDING_CHANCE));

    ColonyEconomy colony;
    Household underCapital;
    underCapital.id = HouseholdId{1};
    underCapital.credits = MIN_CAPITAL - 1;
    colony.households.push_back(underCapital);
    Household alreadyFounded;
    alreadyFounded.id = HouseholdId{2};
    alreadyFounded.credits = MIN_CAPITAL;
    colony.households.push_back(alreadyFounded);
    Corporation corporation;
    corporation.owner = alreadyFounded.id;
    colony.corporations.push_back(corporation);

    Rng const rng(Simulation::Economy::ColonyId{7}, 1);
    auto const result = Simulation::Economy::rollFoundings(colony, FoundingMarketData{}, rng);
    CHECK(result.corporations.empty());
}

TEST_CASE("Founding outcomes and capital transfers are deterministic") {
    using Simulation::Economy::ColonyEconomy;
    using Simulation::Economy::FoundingMarketData;
    using Simulation::Economy::Household;
    using Simulation::Economy::HouseholdId;
    using Simulation::Economy::MIN_CAPITAL;
    using Simulation::Economy::Rng;

    ColonyEconomy colony;
    for (std::int64_t index = 0; index < 1'000; ++index) {
        Household household;
        household.id = HouseholdId{index};
        household.credits = MIN_CAPITAL;
        colony.households.push_back(household);
    }

    Rng const rng(Simulation::Economy::ColonyId{42}, 9);
    auto const first = Simulation::Economy::rollFoundings(colony, FoundingMarketData{}, rng);
    auto const second = Simulation::Economy::rollFoundings(colony, FoundingMarketData{}, rng);
    REQUIRE(!(first.corporations.empty()));
    REQUIRE(first.corporations.size() == second.corporations.size());
    for (std::size_t index = 0; index < first.corporations.size(); ++index) {
        CHECK(first.corporations[index].householdId == second.corporations[index].householdId);
        CHECK(first.corporations[index].sector == second.corporations[index].sector);
        CHECK(first.corporations[index].capitalTransfer == MIN_CAPITAL);
        CHECK(second.corporations[index].capitalTransfer == MIN_CAPITAL);
    }
}
