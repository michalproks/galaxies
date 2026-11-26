#include "EconomyRecords.h"

#include "EconomyConstants.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

namespace {

using namespace Simulation::Economy;

[[nodiscard]] Household makeHousehold() {
    Household household;
    household.id = HouseholdId{1};
    household.population = POP_BASE;
    household.credits = 1'000'000;
    household.foodStock = 30'000'000;
    household.propIncome = 0.1;
    household.propSavings = 0.2;
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
    colony.households.push_back(makeHousehold());

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
    return colony;
}

[[nodiscard]] ColonyTickResult makeEmptyTickResult(ColonyEconomy const &previous) {
    ColonyTickResult result;
    result.colony = previous;
    ++result.colony.turnNumber;
    return result;
}

} // namespace

TEST_CASE("Player economy records are deterministic and fixed-size") {
    using namespace Simulation::Economy;

    ColonyEconomy const previous = makeColony();
    ColonyTickResult const first = tickColony(previous);
    ColonyTickResult const repeated = tickColony(previous);
    PlayerRecord const firstRecord = makePlayerRecord(previous, first);
    PlayerRecord const repeatedRecord = makePlayerRecord(previous, repeated);

    CHECK(firstRecord == repeatedRecord);
    CHECK(firstRecord.colonyId == previous.id);
    CHECK(firstRecord.turnNumber == 1);
    CHECK(firstRecord.population == first.colony.totalPopulation);
    CHECK(firstRecord.populationChange == first.colony.totalPopulation - previous.totalPopulation);
    CHECK(firstRecord.householdCount == 1);
    CHECK(firstRecord.corporationCounts[static_cast<std::size_t>(SectorId::Food)] == 1);
    CHECK(sizeof(PlayerRecord) <= 1024);
}

TEST_CASE("Debug economy records evict the oldest snapshot at the exact window boundary") {
    using namespace Simulation::Economy;

    ColonyEconomy previous;
    previous.id = ColonyId{9};
    previous.turnNumber = 1;
    DebugRecorder recorder(2);

    ColonyTickResult first = makeEmptyTickResult(previous);
    recorder.append(previous, first);
    CHECK(recorder.records().size() == 1);
    CHECK(recorder.records().front().turnNumber == 1);

    previous = first.colony;
    ColonyTickResult second = makeEmptyTickResult(previous);
    recorder.append(previous, second);
    CHECK(recorder.records().size() == 2);
    CHECK(recorder.records().front().turnNumber == 1);
    CHECK(recorder.records().back().turnNumber == 2);

    previous = second.colony;
    ColonyTickResult third = makeEmptyTickResult(previous);
    recorder.append(previous, third);
    CHECK(recorder.records().size() == 2);
    CHECK(recorder.records().front().turnNumber == 2);
    CHECK(recorder.records().back().turnNumber == 3);
}

TEST_CASE("Player and debug economy record streams are independent") {
    using namespace Simulation::Economy;

    ColonyEconomy const previous = makeColony();
    ColonyTickResult const result = tickColony(previous);
    PlayerRecord const withoutDebug = makePlayerRecord(previous, result);

    DebugRecorder disabledRecorder(5, false);
    disabledRecorder.append(previous, result);
    CHECK(disabledRecorder.records().empty());
    DebugRecord debug = makeDebugRecord(previous, result);
    DebugRecorder enabledRecorder(5);
    enabledRecorder.append(previous, result);
    debug.afterState.households.front().credits = -1;

    PlayerRecord const withDebug = makePlayerRecord(previous, result);
    CHECK(withoutDebug == withDebug);
    CHECK(result.colony.households.front().credits != -1);
    CHECK(enabledRecorder.records().size() == 1);
    CHECK(enabledRecorder.records().front().afterState.households.front().credits != -1);
}

TEST_CASE("Player and debug streams capture three real colony tick turns") {
    using namespace Simulation::Economy;

    ColonyEconomy state = makeColony();
    DebugRecorder recorder(3);
    std::int64_t completedTurns{};
    for (std::int64_t turn = 0; turn < 3; ++turn) {
        ColonyTickResult result =
            turn == 0 ? tickColony(state) : tickColony(state, recorder.records().back().marketData);
        PlayerRecord const playerRecord = makePlayerRecord(state, result);
        recorder.append(state, result);
        CHECK(playerRecord.turnNumber == turn + 1);
        CHECK(playerRecord.colonyId == state.id);
        CHECK(recorder.records().back().turnNumber == turn + 1);
        state = result.colony;
        ++completedTurns;
    }

    CHECK(completedTurns == 3);
    CHECK(recorder.records().size() == 3);
    CHECK(state.turnNumber == 4);
}
