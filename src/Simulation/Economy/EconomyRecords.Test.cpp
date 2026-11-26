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

TEST_CASE("Player economy records are deterministic with compact headlines") {
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
    REQUIRE(firstRecord.corporationDetails.size() == 1);
    CHECK(firstRecord.corporationDetails.front().id == CorporationId{10});
    CHECK(firstRecord.corporationDetails.front().plannedOutput >= 0);
    CHECK(firstRecord.sectorCapacity[static_cast<std::size_t>(SectorId::Food)] >= 0);
    CHECK(sizeof(PlayerRecord) <= 1024);
}

TEST_CASE("Debug records retain enriched traces including empty and bounded sweep cases") {
    using namespace Simulation::Economy;

    ColonyEconomy previous;
    previous.id = ColonyId{12};
    previous.turnNumber = 1;
    ColonyTickResult result = makeEmptyTickResult(previous);
    HouseholdTickOutcome householdOutcome;
    householdOutcome.id = HouseholdId{3};
    householdOutcome.budget = 500;
    householdOutcome.survivalNeed = 100;
    householdOutcome.reserveTarget = 40;
    householdOutcome.reserveGap = 20;
    householdOutcome.emergencyFoodRequested = 10;
    householdOutcome.emergencyFoodFilled = 8;
    householdOutcome.foodBought = 90;
    householdOutcome.foodSpent = 40;
    householdOutcome.servicesSpent = 20;
    householdOutcome.consumerGoodsSpent = 10;
    householdOutcome.foodEaten = 90;
    householdOutcome.reserveTurns = 2.5;
    result.householdOutcomes.push_back(householdOutcome);
    CorporationTickOutcome corporation;
    corporation.id = CorporationId{8};
    corporation.planning.targetPrice = 42;
    corporation.planning.targetQuantity = 17;
    corporation.planning.methods.push_back({BuildTier::Manual, 12.5, 0});
    corporation.materialsConsumed = 9;
    corporation.maintenancePaid = false;
    corporation.unpaidMaintenanceBuildings = 1;
    result.corporationOutcomes.push_back(corporation);
    result.invariantResults.push_back({"credits-conserved", true});

    Market market;
    OrderBook &book = market.orderBookFor(ResourceId::Food);
    book.sellOrders.push_back({OrderId{0}, HouseholdId{1}, OrderSide::Sell, ResourceId::Food, 1, 1});
    for (std::int64_t index = 0; index < static_cast<std::int64_t>(MAX_MARKET_TRACE_CANDIDATES) + 6; ++index) {
        book.buyOrders.push_back({OrderId{index + 1}, HouseholdId{2}, OrderSide::Buy, ResourceId::Food, index + 2, 1});
    }
    result.settlements.push_back(market.settle(ResourceId::Food));

    DebugRecorder recorder(1);
    recorder.append(previous, result);
    REQUIRE(recorder.records().size() == 1);
    DebugRecord const &record = recorder.records().front();
    REQUIRE(record.householdOutcomes.size() == 1);
    CHECK(record.householdOutcomes.front().foodSpent == 40);
    CHECK(record.householdOutcomes.front().emergencyFoodFilled == 8);
    REQUIRE(record.corporationOutcomes.size() == 1);
    CHECK(record.corporationOutcomes.front().planning.targetQuantity == 17);
    CHECK(record.corporationOutcomes.front().materialsConsumed == 9);
    CHECK(record.corporationOutcomes.front().unpaidMaintenanceBuildings == 1);
    REQUIRE(record.invariantResults.size() == 1);
    CHECK(record.invariantResults.front().id == "credits-conserved");
    REQUIRE(record.settlements.size() == 1);
    CHECK(record.settlements.front().candidateEvaluations.size() == MAX_MARKET_TRACE_CANDIDATES);
    CHECK(record.settlements.front().candidateEvaluations.back().price ==
          PRICE_FLOOR + static_cast<std::int64_t>(MAX_MARKET_TRACE_CANDIDATES) - 1);

    DebugRecord const empty = makeDebugRecord(previous, makeEmptyTickResult(previous));
    CHECK(empty.householdOutcomes.empty());
    CHECK(empty.corporationOutcomes.empty());
    CHECK(empty.invariantResults.empty());
    CHECK(empty.settlements.empty());
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

TEST_CASE("Planning traces cap demand and staffing lists at their declared boundary") {
    using namespace Simulation::Economy;

    Corporation corporation = makeColony().corporations.front();
    for (std::int64_t index = 0; index < static_cast<std::int64_t>(MAX_PLANNING_DEMAND_STEPS) + 1; ++index) {
        corporation.sectorDemand.steps.emplace_back(200 - index, 100'000);
    }
    corporation.buildings.resize(MAX_PLANNING_STAFFING_ENTRIES + 1,
                                 Building{SectorId::Food, BuildTier::Tier1, 0, false, 1'000});
    CorporationPrices prices;
    prices.inputs = {40.0, 6.0, 15.0, 10.0};
    prices.output = 4;

    CorporationPlan const plan = planCorporation(corporation, prices);

    CHECK(plan.diagnostic.demandSteps.size() == MAX_PLANNING_DEMAND_STEPS);
    CHECK(plan.diagnostic.demandStepsTruncated);
    CHECK(plan.diagnostic.staffing.size() == MAX_PLANNING_STAFFING_ENTRIES);
    CHECK(plan.diagnostic.staffingTruncated);

    ColonyEconomy previous;
    ColonyTickResult result = makeEmptyTickResult(previous);
    CorporationTickOutcome outcome;
    outcome.id = corporation.id;
    outcome.planning = plan.diagnostic;
    result.corporationOutcomes.push_back(outcome);
    DebugRecord const record = makeDebugRecord(previous, result);
    REQUIRE(record.corporationOutcomes.size() == 1);
    CHECK(record.corporationOutcomes.front().planning.demandSteps.size() == MAX_PLANNING_DEMAND_STEPS);
    CHECK(record.corporationOutcomes.front().planning.staffing.size() == MAX_PLANNING_STAFFING_ENTRIES);
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
