#include "ColonyTick.h"
#include "InitialConditions.h"

#include "EconomyConstants.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace {

using namespace Simulation::Economy;

struct HouseholdSeed {
    HouseholdId id{};
    std::int64_t population{};
    std::int64_t credits{};
    std::int64_t foodStock{};
};

[[nodiscard]] Household makeHousehold(HouseholdSeed const seed) {
    Household household;
    household.id = seed.id;
    household.population = seed.population;
    household.credits = seed.credits;
    household.propIncome = 0.1;
    household.propSavings = 0.0;
    household.foodStock = seed.foodStock;
    household.foodBelief = {4.0, 0.0};
    household.labourBelief = {40.0, 0.0};
    household.serviceBelief = {40.0, 0.0};
    household.consumerGoodsBelief = {30.0, 0.0};
    household.foodPriceHistory = {4, 4, 4};
    household.lastFoodPrice = 4;
    return household;
}

[[nodiscard]] ColonyEconomy makeColony() {
    ColonyEconomy colony;
    colony.id = ColonyId{73};
    colony.turnNumber = 1;
    colony.economySeed = 0xA7U;
    return colony;
}

[[nodiscard]] std::int64_t creditsIn(ColonyEconomy const &colony) {
    std::int64_t credits{};
    for (Household const &household : colony.households) {
        credits += household.credits;
    }
    for (Corporation const &corporation : colony.corporations) {
        credits += corporation.credits;
    }
    return credits;
}

[[nodiscard]] std::int64_t standardStock(ColonyEconomy const &colony, ResourceId const resource) {
    std::int64_t stock{};
    for (Household const &household : colony.households) {
        if (resource == ResourceId::Food) {
            stock += household.foodStock;
        }
    }
    for (Corporation const &corporation : colony.corporations) {
        stock += corporation.stock[resource];
    }
    return stock;
}

[[nodiscard]] bool sameSettlements(ColonyTickResult const &left, ColonyTickResult const &right) {
    if (left.settlements.size() != right.settlements.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.settlements.size(); ++index) {
        SettlementOutcome const &leftOutcome = left.settlements[index];
        SettlementOutcome const &rightOutcome = right.settlements[index];
        if (leftOutcome.resource != rightOutcome.resource || leftOutcome.price != rightOutcome.price ||
            leftOutcome.tradedQuantity != rightOutcome.tradedQuantity ||
            leftOutcome.trades.size() != rightOutcome.trades.size()) {
            return false;
        }
        for (std::size_t tradeIndex = 0; tradeIndex < leftOutcome.trades.size(); ++tradeIndex) {
            Trade const &leftTrade = leftOutcome.trades[tradeIndex];
            Trade const &rightTrade = rightOutcome.trades[tradeIndex];
            if (leftTrade.orderId != rightTrade.orderId || leftTrade.owner != rightTrade.owner ||
                leftTrade.resource != rightTrade.resource || leftTrade.side != rightTrade.side ||
                leftTrade.quantity != rightTrade.quantity || leftTrade.price != rightTrade.price) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] std::int64_t findPrice(ColonyTickResult const &result, ResourceId const resource) {
    for (SettlementOutcome const &settlement : result.settlements) {
        if (settlement.resource == resource) {
            return settlement.price;
        }
    }
    return 0;
}

} // namespace

TEST_CASE("Colony tick is deterministic and conserves credits, food, and materials over multiple turns") {
    using namespace Simulation::Economy;

    ColonyEconomy colony = makeColony();
    colony.households.push_back(makeHousehold({HouseholdId{1}, POP_BASE, 1'000'000, 30'000'000}));
    Corporation food;
    food.id = CorporationId{10};
    food.owner = HouseholdId{1};
    food.sector = SectorId::Food;
    food.credits = 10'000'000;
    food.sectorDemand.steps = {{4, 10'000'000}};
    food.priceBeliefs[static_cast<std::size_t>(ResourceId::Labour)] = {40.0, 0.0};
    food.priceBeliefs[static_cast<std::size_t>(ResourceId::Food)] = {4.0, 0.0};
    colony.corporations.push_back(food);
    colony.totalPopulation = POP_BASE;

    ColonyEconomy const initial = colony;
    ColonyTickResult const first = tickColony(colony);
    ColonyTickResult const repeated = tickColony(colony);
    CHECK(sameSettlements(first, repeated));
    REQUIRE(first.colony.corporations.size() == repeated.colony.corporations.size());
    CHECK(first.colony.corporations[0].credits == repeated.colony.corporations[0].credits);
    CHECK(first.colony.corporations[0].stock[ResourceId::Food] ==
          repeated.colony.corporations[0].stock[ResourceId::Food]);
    CHECK(first.colony.households[0].credits == repeated.colony.households[0].credits);
    CHECK(first.colony.households[0].population == repeated.colony.households[0].population);
    CHECK(creditsIn(first.colony) == creditsIn(initial));
    CHECK(first.colony.turnNumber == 2);

    std::int64_t const firstProduced = first.corporationDeltas[0].produced;
    std::int64_t const firstEaten = first.householdDeltas[0].foodEaten;
    CHECK(standardStock(first.colony, ResourceId::Food) - standardStock(initial, ResourceId::Food) ==
          firstProduced - firstEaten);
    CHECK(standardStock(first.colony, ResourceId::Materials) == standardStock(initial, ResourceId::Materials));
    CHECK(first.colony.households[0].population == POP_BASE);

    ColonyTickResult const second = tickColony(first.colony, first.marketData);
    CHECK(creditsIn(second.colony) == creditsIn(first.colony));
    std::int64_t secondProduced{};
    std::int64_t secondEaten{};
    for (CorporationTickDelta const &delta : second.corporationDeltas) {
        secondProduced += delta.produced;
    }
    for (HouseholdTickDelta const &delta : second.householdDeltas) {
        secondEaten += delta.foodEaten;
    }
    CHECK(standardStock(second.colony, ResourceId::Food) - standardStock(first.colony, ResourceId::Food) ==
          secondProduced - secondEaten);
    CHECK(standardStock(second.colony, ResourceId::Materials) == standardStock(first.colony, ResourceId::Materials));
}

TEST_CASE("Turn-one standard output is stocked, not booked as same-turn corporate revenue") {
    using namespace Simulation::Economy;

    ColonyEconomy colony = makeColony();
    Household household = makeHousehold({HouseholdId{1}, POP_BASE, 1'000'000, 10'000'000});
    household.propIncome = 0.0;
    household.lastIncome = 0;
    colony.households.push_back(household);

    Corporation food;
    food.id = CorporationId{2};
    food.owner = household.id;
    food.sector = SectorId::Food;
    food.credits = 10'000'000;
    food.sectorDemand.steps = {{5, 4'000'000}, {4, 6'000'000}};
    food.priceBeliefs[static_cast<std::size_t>(ResourceId::Labour)] = {40.0, 0.0};
    colony.corporations.push_back(food);

    ColonyTickResult const result = tickColony(colony);

    // The worked C1 balance walk at economic-model.md:429-430 books same-turn sales of new food.
    // Turn-sequence stages 5-7 and the next-turn stock rule at line 725 take precedence: output
    // produced now is unsold stock, so actual cash is 10M - 8M, then the 20% dividend is paid.
    CHECK(result.colony.corporations[0].credits == 1'600'000);
    CHECK(result.colony.corporations[0].stock[ResourceId::Food] == 2'400'000);
    CHECK(result.colony.households[0].credits == 9'400'000);
    CHECK(result.corporationDeltas[0].produced == 2'400'000);
    CHECK(result.corporationDeltas[0].sold == 0);
    CHECK(result.colony.market.lastPriceFor(ResourceId::Food) == 4);
}

TEST_CASE("Emergency food bids can set the settlement price at the emergency bid") {
    using namespace Simulation::Economy;

    ColonyEconomy colony = makeColony();
    Household household = makeHousehold({HouseholdId{1}, POP_BASE, 45'000'000, 2'000'000});
    household.lastIncome = 40'000'000;
    household.propIncome = 0.8;
    household.propSavings = 0.5;
    colony.households.push_back(household);

    Corporation food;
    food.id = CorporationId{2};
    food.owner = household.id;
    food.sector = SectorId::Food;
    food.credits = 1'000'000;
    food.stock[ResourceId::Food] = 7'000'000;
    food.stockUnitCost = 5.0;
    food.lastSettlementPrice = 4;
    food.priceBeliefs[static_cast<std::size_t>(ResourceId::Food)] = {4.0, 0.0};
    colony.corporations.push_back(food);

    ColonyTickResult const result = tickColony(colony);

    // Matches economic-model.md's emergency example: an 8M kg survival order at 5,
    // partly filled from stock, sets the food settlement price to the emergency bid.
    CHECK(result.colony.market.lastPriceFor(ResourceId::Food) == 5);
    CHECK(result.marketData.demand[static_cast<std::size_t>(ResourceId::Food)] == 8'000'000);
    CHECK(result.marketData.filled[static_cast<std::size_t>(ResourceId::Food)] == 7'000'000);
    CHECK(result.householdDeltas[0].foodEaten == 9'000'000);
    CHECK(result.colony.households[0].lastFoodPrice == 5);
    CHECK(result.colony.households[0].population == 900'000);
    CHECK(result.colony.corporations[0].stock[ResourceId::Food] == 0);
    CHECK(creditsIn(result.colony) == creditsIn(colony));
}

TEST_CASE("Seed prices persist when a resource has no trades") {
    using namespace Simulation::Economy;

    ColonyEconomy colony = makeColony();
    colony.households.push_back(makeHousehold({HouseholdId{1}, POP_BASE, 0, 10'000'000}));

    ColonyTickResult const result = tickColony(colony);

    CHECK(result.colony.market.lastPriceFor(ResourceId::Labour) == 40);
    CHECK(result.colony.market.lastPriceFor(ResourceId::Food) == 4);
    CHECK(result.colony.market.lastPriceFor(ResourceId::Materials) == 6);
    CHECK(result.colony.market.lastPriceFor(ResourceId::Fuel) == 10);
    CHECK(result.colony.market.lastPriceFor(ResourceId::Energy) == 15);
    CHECK(result.colony.market.lastPriceFor(ResourceId::Service) == 40);
    CHECK(result.colony.market.lastPriceFor(ResourceId::ConsumerGoods) == 30);
    CHECK(findPrice(result, ResourceId::Food) == 4);
    CHECK(result.colony.households[0].population == POP_BASE);
}

TEST_CASE("Founding rolls transfer capital and add corporations for the following turn") {
    using namespace Simulation::Economy;

    ColonyEconomy colony = makeColony();
    for (std::int64_t index = 0; index < 100; ++index) {
        colony.households.push_back(makeHousehold({HouseholdId{index}, POP_BASE, MIN_CAPITAL, 10'000'000}));
    }

    ColonyTickResult const first = tickColony(colony);
    REQUIRE(!(first.founded.empty()));
    CHECK(first.colony.corporations.size() == first.founded.size());
    CHECK(creditsIn(first.colony) == creditsIn(colony));
    for (FoundedCorporation const &founding : first.founded) {
        CHECK(founding.capitalTransfer == MIN_CAPITAL);
    }
}

TEST_CASE("Starved households dissolve and their assets transfer to surviving households") {
    using namespace Simulation::Economy;

    ColonyEconomy colony = makeColony();
    colony.households.push_back(makeHousehold({HouseholdId{1}, POP_MIN - 1, 0, 0}));
    colony.households.push_back(makeHousehold({HouseholdId{2}, POP_BASE, 2'000'000, 10'000'000}));
    colony.totalPopulation = POP_MIN - 1 + POP_BASE;

    ColonyTickResult const result = tickColony(colony);

    REQUIRE(result.dissolvedHouseholds.size() == 1);
    CHECK(result.dissolvedHouseholds[0].value == 1);
    REQUIRE(result.colony.households.size() == 1);
    CHECK(result.colony.households[0].id.value == 2);
    CHECK(result.colony.totalPopulation == result.colony.households[0].population);
    CHECK(creditsIn(result.colony) == creditsIn(colony));
}

TEST_CASE("Zero-credit corporations dissolve at the end of the turn") {
    using namespace Simulation::Economy;

    ColonyEconomy colony = makeColony();
    Household household = makeHousehold({HouseholdId{1}, POP_BASE, 2'000'000, 10'000'000});
    colony.households.push_back(household);

    Corporation food;
    food.id = CorporationId{2};
    food.owner = household.id;
    food.sector = SectorId::Food;
    food.credits = 0;
    colony.corporations.push_back(food);

    ColonyTickResult const result = tickColony(colony);

    REQUIRE(result.dissolvedCorporations.size() == 1);
    CHECK(result.dissolvedCorporations[0].id.value == 2);
    CHECK(result.dissolvedCorporations[0].owner.value == 1);
    CHECK(result.colony.corporations.empty());
    CHECK(creditsIn(result.colony) == creditsIn(colony));
}

TEST_CASE("Earth corporations never overspend their credits on the second turn") {
    ColonyEconomy const seed = buildEarthSeed(ColonyId{0}, 0xA8U);
    ColonyTickResult const first = tickColony(seed);
    ColonyTickResult const second = tickColony(first.colony, first.marketData);
    for (Corporation const &corporation : second.colony.corporations) {
        std::int64_t previousCredits{};
        bool existedBefore{};
        for (Corporation const &previous : first.colony.corporations) {
            if (previous.id == corporation.id) {
                previousCredits = previous.credits;
                existedBefore = true;
                break;
            }
        }
        CHECK(corporation.credits >= 0);
        if (existedBefore) {
            std::int64_t buyOrderCommitment{};
            for (auto const &[resource, book] : second.colony.market.orderBooks) {
                static_cast<void>(resource);
                for (Order const &order : book.buyOrders) {
                    if (order.owner == AgentRef{corporation.id}) {
                        buyOrderCommitment += order.price * order.quantity;
                    }
                }
            }
            CHECK(buyOrderCommitment <= previousCredits);
        }
    }
}

TEST_CASE("Seeded Earth economy keeps ticking twelve periods without stalling") {
    // Regression guard for the turn-9 stall: previously the seed booked zero steady-state
    // stocks, the input-material production gate zeroed every sector on turn 1, corporations
    // went bankrupt by turn 3, the colony collapsed, and the credit-conservation invariant
    // threw at turn 9, making the server retry the poisoned period forever. A tick must now
    // always complete and advance the turn number. Long-run population equilibrium under the
    // Earth seed is a separate calibration concern (the model clears goods but not full
    // employment), so this test asserts liveness, not survival.
    ColonyEconomy colony = buildEarthSeed(ColonyId{42}, 0xA8U);
    FoundingMarketData marketData{};
    std::int64_t peakPopulation = colony.totalPopulation;
    std::int64_t creditsAfterFirstTick{};
    for (int period = 0; period < 12; ++period) {
        ColonyTickResult result = period == 0 ? tickColony(colony) : tickColony(colony, marketData);
        colony = std::move(result.colony);
        marketData = result.marketData;
        REQUIRE(colony.turnNumber == period + 2);
        peakPopulation = std::max(peakPopulation, colony.totalPopulation);
        if (colony.turnNumber == 2) {
            for (Household const &household : colony.households) {
                creditsAfterFirstTick += household.credits;
            }
            for (Corporation const &corporation : colony.corporations) {
                creditsAfterFirstTick += corporation.credits;
            }
        }
        for (Household const &household : colony.households) {
            CHECK(household.credits >= 0);
        }
        for (Corporation const &corporation : colony.corporations) {
            CHECK(corporation.credits >= 0);
        }
    }
    // The economy must have been alive after the first tick with a substantial conserved
    // ledger, and never throw the credit-conservation invariant (the turn-9 stall symptom).
    // Long-run equilibrium under the Earth seed is an open calibration concern tracked
    // separately; this test guards the stall regression.
    CHECK(creditsAfterFirstTick > 10'000'000'000);
    CHECK(peakPopulation > 0);
}
