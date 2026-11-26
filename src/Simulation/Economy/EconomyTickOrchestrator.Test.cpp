#include "EconomyTickOrchestrator.h"

#include "EconomyConstants.h"
#include "InitialConditions.h"

#include <catch2/catch_test_macros.hpp>

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

    static_cast<void>(singleWorker.advanceTo(1.0 * ECONOMY_TICK_HOURS));
    static_cast<void>(multipleWorkers.advanceTo(1.0 * ECONOMY_TICK_HOURS));
    std::vector<ColonyEconomyRecordSnapshot> const singleRecords = singleWorker.recordSnapshots();
    std::vector<ColonyEconomyRecordSnapshot> const multipleRecords = multipleWorkers.recordSnapshots();

    REQUIRE(singleRecords.size() == multipleRecords.size());
    for (std::size_t colonyIndex = 0; colonyIndex < singleRecords.size(); ++colonyIndex) {
        CHECK(singleRecords[colonyIndex].colonyId == multipleRecords[colonyIndex].colonyId);
        CHECK(singleRecords[colonyIndex].playerHistory == multipleRecords[colonyIndex].playerHistory);
        CHECK(serializePlayerHistory(singleRecords[colonyIndex].playerHistory) ==
              serializePlayerHistory(multipleRecords[colonyIndex].playerHistory));
        CHECK(singleRecords[colonyIndex].playerHistory.size() == 1);
    }
}

TEST_CASE("Earth seed maintains money accounting and near-seed prices for its first turn") {
    EconomyTickOrchestrator orchestrator{1};
    std::int64_t expectedCredits = 62'030'000'000;
    constexpr std::array<std::int64_t, RESOURCE_COUNT> SEED_PRICES{0, 40, 15, 4, 6, 10, 40, 30};

    std::vector<EconomyEventBatch> const events = orchestrator.advanceTo(ECONOMY_TICK_HOURS);
    REQUIRE(events.size() == 1);
    std::vector<ColonyEconomyRecordSnapshot> const records = orchestrator.recordSnapshots();
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].playerHistory.size() == 1);
    REQUIRE(records[0].debugRecords.size() == 1);

    DebugRecord const &debug = records[0].debugRecords.back();
    expectedCredits += debug.liquidationCredits;
    CHECK(totalCredits(debug.afterState) == expectedCredits);
    CHECK(debug.afterState.turnNumber == 2);
    PlayerRecord const &player = records[0].playerHistory.back();
    CHECK(player.turnNumber == 1);
    for (std::size_t resourceIndex = 1; resourceIndex < RESOURCE_COUNT; ++resourceIndex) {
        CHECK(player.settlementPrices[resourceIndex] >= SEED_PRICES[resourceIndex] / 2);
        CHECK(player.settlementPrices[resourceIndex] <= SEED_PRICES[resourceIndex] * 2);
    }
}

} // namespace

TEST_CASE("Invalid second Earth turn is rejected without changing committed state or records") {
    EconomyTickOrchestrator orchestrator{1};
    CHECK_THROWS(static_cast<void>(orchestrator.advanceTo(2.0 * ECONOMY_TICK_HOURS)));
    CHECK(orchestrator.completedPeriods() == 1);
    REQUIRE(orchestrator.colonies().size() == 1);
    std::vector<ColonyEconomyRecordSnapshot> const committedRecords = orchestrator.recordSnapshots();
    REQUIRE(committedRecords[0].debugRecords.size() == 1);
    ColonyEconomy const &committedColony = committedRecords[0].debugRecords.back().afterState;
    CHECK(totalCredits(orchestrator.colonies()[0]) == totalCredits(committedColony));
    CHECK(orchestrator.colonies()[0].turnNumber == committedColony.turnNumber);
    CHECK(committedRecords[0].playerHistory.size() == 1);
    std::vector<EconomyEventBatch> const pendingEvents = orchestrator.takePendingEvents();
    REQUIRE(pendingEvents.size() == 1);
    CHECK(pendingEvents[0].turnNumber == 1);
}
