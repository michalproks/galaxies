#include "EconomyTickOrchestrator.h"

#include "EconomyConstants.h"
#include "InitialConditions.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using namespace Simulation::Economy;

template <typename Value>
void appendValueBytes(std::string &bytes, Value const &value) {
    static_assert(std::is_trivially_copyable_v<Value>);
    bytes.append(reinterpret_cast<char const *>(&value), sizeof(value));
}

[[nodiscard]] std::string serializePlayerHistory(std::vector<PlayerRecord> const &history) {
    std::string bytes;
    for (PlayerRecord const &record : history) {
        appendValueBytes(bytes, record.colonyId.value);
        appendValueBytes(bytes, record.turnNumber);
        auto const appendArray = [&bytes](auto const &values) {
            for (std::int64_t const value : values) {
                appendValueBytes(bytes, value);
            }
        };
        appendArray(record.settlementPrices);
        appendArray(record.priceChanges);
        appendArray(record.stocks);
        appendArray(record.buyDemand);
        appendArray(record.buyFilled);
        appendArray(record.corporationCounts);
        appendArray(record.produced);
        appendArray(record.sold);
        appendValueBytes(bytes, record.population);
        appendValueBytes(bytes, record.populationChange);
        appendValueBytes(bytes, record.householdCount);
        appendValueBytes(bytes, record.householdCredits);
        appendValueBytes(bytes, record.corporationCredits);
        appendValueBytes(bytes, record.averageHouseholdCredits);
        appendValueBytes(bytes, record.averageSavingsPropensity);
        appendValueBytes(bytes, record.foodConsumed);
        appendValueBytes(bytes, record.servicesConsumed);
        appendValueBytes(bytes, record.consumerGoodsConsumed);
        appendValueBytes(bytes, record.averageFoodReserveTurns);
        appendValueBytes(bytes, record.householdsBelowOneFoodReserveTurn);
        appendValueBytes(bytes, record.labourSupplied);
        appendValueBytes(bytes, record.labourEmployed);
        for (std::int64_t const value : record.corporationsFounded) {
            appendValueBytes(bytes, value);
        }
        appendValueBytes(bytes, record.corporationsDissolved);
        appendValueBytes(bytes, record.householdsDissolved);
    }
    return bytes;
}

[[nodiscard]] std::int64_t totalCredits(ColonyEconomy const &colony) {
    std::int64_t total{};
    for (Household const &household : colony.households) {
        total += household.credits;
    }
    for (Corporation const &corporation : colony.corporations) {
        total += corporation.credits;
    }
    return total;
}

TEST_CASE("Earth economy records are deterministic across worker counts") {
    std::vector<ColonyEconomy> const seeds{
        buildEarthSeed(ColonyId{9}, 0xA8U),
        buildEarthSeed(ColonyId{2}, 0xA8U),
    };
    EconomyTickOrchestrator singleWorker{seeds, 1};
    EconomyTickOrchestrator multipleWorkers{seeds, 4};

    static_cast<void>(singleWorker.advanceTo(3.0 * ECONOMY_TICK_HOURS));
    static_cast<void>(multipleWorkers.advanceTo(3.0 * ECONOMY_TICK_HOURS));
    std::vector<ColonyEconomyRecordSnapshot> const singleRecords = singleWorker.recordSnapshots();
    std::vector<ColonyEconomyRecordSnapshot> const multipleRecords = multipleWorkers.recordSnapshots();

    REQUIRE(singleRecords.size() == multipleRecords.size());
    for (std::size_t colonyIndex = 0; colonyIndex < singleRecords.size(); ++colonyIndex) {
        CHECK(singleRecords[colonyIndex].colonyId == multipleRecords[colonyIndex].colonyId);
        CHECK(singleRecords[colonyIndex].playerHistory == multipleRecords[colonyIndex].playerHistory);
        CHECK(serializePlayerHistory(singleRecords[colonyIndex].playerHistory) ==
              serializePlayerHistory(multipleRecords[colonyIndex].playerHistory));
        CHECK(singleRecords[colonyIndex].playerHistory.size() == 3);
    }
}

TEST_CASE("Earth seed maintains money and near-seed prices across turns 1 through 3") {
    EconomyTickOrchestrator orchestrator{1};
    std::int64_t expectedCredits = 62'030'000'000;
    constexpr std::array<std::int64_t, RESOURCE_COUNT> SEED_PRICES{0, 40, 15, 4, 6, 10, 40, 30};

    for (std::int64_t period = 1; period <= 3; ++period) {
        std::vector<EconomyEventBatch> const events =
            orchestrator.advanceTo(static_cast<double>(period * ECONOMY_TICK_HOURS));
        REQUIRE(events.size() == 1);
        std::vector<ColonyEconomyRecordSnapshot> const records = orchestrator.recordSnapshots();
        REQUIRE(records.size() == 1);
        REQUIRE(records[0].playerHistory.size() == static_cast<std::size_t>(period));
        REQUIRE(records[0].debugRecords.size() == static_cast<std::size_t>(period));

        DebugRecord const &debug = records[0].debugRecords.back();
        if (period == 1) {
            auto const labour = std::find_if(
                debug.settlements.begin(), debug.settlements.end(), [](SettlementOutcome const &settlement) {
                    return settlement.resource == ResourceId::Labour;
                });
            REQUIRE(labour != debug.settlements.end());
            CHECK(!(labour->candidateEvaluations.empty()));
            CHECK(debug.corporationOutcomes.size() == debug.beforeState.corporations.size());
            for (Corporation const &corporation : debug.beforeState.corporations) {
                CHECK(std::any_of(
                    debug.corporationOutcomes.begin(),
                    debug.corporationOutcomes.end(),
                    [&corporation](CorporationTickOutcome const &outcome) { return outcome.id == corporation.id; }));
            }
            CHECK(std::any_of(
                debug.invariantResults.begin(), debug.invariantResults.end(), [](EconomyInvariantResult const &result) {
                    return result.id == "credits-conserved" && result.passed;
                }));
            CHECK(std::any_of(
                debug.invariantResults.begin(), debug.invariantResults.end(), [](EconomyInvariantResult const &result) {
                    return result.id == "standard-resources-conserved" && result.passed;
                }));
        }
        expectedCredits += debug.liquidationCredits;
        CHECK(totalCredits(debug.afterState) == expectedCredits);
        CHECK(debug.afterState.turnNumber == period + 1);
        for (Household const &household : debug.afterState.households) {
            CHECK(household.credits >= 0);
        }
        for (Corporation const &corporation : debug.afterState.corporations) {
            CHECK(corporation.credits >= 0);
        }
        PlayerRecord const &player = records[0].playerHistory.back();
        CHECK(player.turnNumber == period);
        for (std::size_t resourceIndex = 1; resourceIndex < RESOURCE_COUNT; ++resourceIndex) {
            CHECK(player.settlementPrices[resourceIndex] >= SEED_PRICES[resourceIndex] / 2);
            CHECK(player.settlementPrices[resourceIndex] <= SEED_PRICES[resourceIndex] * 2);
        }
    }
}

} // namespace

TEST_CASE("Earth economy commits three turns without orchestrator rejection") {
    EconomyTickOrchestrator orchestrator{1};
    std::vector<EconomyEventBatch> const events = orchestrator.advanceTo(3.0 * ECONOMY_TICK_HOURS);
    CHECK(orchestrator.completedPeriods() == 3);
    REQUIRE(events.size() == 3);
    std::vector<ColonyEconomyRecordSnapshot> const records = orchestrator.recordSnapshots();
    REQUIRE(records.size() == 1);
    CHECK(records[0].playerHistory.size() == 3);
    CHECK(records[0].debugRecords.size() == 3);
    CHECK(orchestrator.colonies()[0].turnNumber == 4);
    for (Corporation const &corporation : orchestrator.colonies()[0].corporations) {
        CHECK(corporation.credits >= 0);
    }
}
