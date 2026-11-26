#include "EmpireEconomyUi.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

namespace {

using Simulation::Economy::ColonyId;
using Simulation::Economy::PlayerRecord;
using Simulation::Economy::ResourceId;
using Simulation::Economy::SectorId;

[[nodiscard]] PlayerRecord record(ColonyId const colonyId, std::int64_t const turn) {
    PlayerRecord result;
    result.colonyId = colonyId;
    result.turnNumber = turn;
    return result;
}

constexpr std::size_t resourceIndex(ResourceId const resource) {
    return static_cast<std::size_t>(resource);
}

constexpr std::size_t sectorIndex(SectorId const sector) {
    return static_cast<std::size_t>(sector);
}

} // namespace

TEST_CASE("Empire history aggregates player records by completed turn") {
    PlayerRecord earth = record(ColonyId{0}, 1);
    earth.population = 100;
    earth.householdCredits = 1200;
    earth.corporationCredits = 300;
    earth.produced[sectorIndex(SectorId::Food)] = 12000;
    earth.foodConsumed = 10000;
    earth.labourSupplied = 10;
    earth.labourEmployed = 6;
    earth.corporationCounts[sectorIndex(SectorId::Food)] = 2;
    earth.corporationsFounded[sectorIndex(SectorId::Food)] = 1;
    earth.settlementPrices[resourceIndex(ResourceId::Food)] = 101;

    PlayerRecord mars = record(ColonyId{1}, 1);
    mars.population = 50;
    mars.householdCredits = 200;
    mars.corporationCredits = 300;
    mars.produced[sectorIndex(SectorId::Food)] = 2000;
    mars.foodConsumed = 1000;
    mars.labourSupplied = 3;
    mars.labourEmployed = 3;
    mars.corporationCounts[sectorIndex(SectorId::Materials)] = 1;
    mars.corporationsFounded[sectorIndex(SectorId::Materials)] = 2;
    mars.settlementPrices[resourceIndex(ResourceId::Food)] = 201;

    PlayerRecord nextEarth = record(ColonyId{0}, 2);
    nextEarth.population = 120;
    std::array<PlayerRecord, 3> const records{earth, mars, nextEarth};

    std::vector<Client::EmpireTurnSummary> const history = Client::aggregateEmpireHistory(records);
    REQUIRE(history.size() == 2);
    CHECK(history[0].turnNumber == 1);
    CHECK(history[0].population == 150);
    CHECK(history[0].creditsCenticredits == 2000);
    CHECK(history[0].foodProduced == 14000);
    CHECK(history[0].foodConsumed == 11000);
    CHECK(history[0].unemployed == 4);
    CHECK(history[0].colonyCount == 2);
    CHECK(history[0].corporationCount == 3);
    CHECK(history[0].corporationsFounded == 3);
    CHECK(history[0].averagePricesCenticredits[resourceIndex(ResourceId::Food)] == 151);
    CHECK(history[1].turnNumber == 2);
    CHECK(history[1].population == 120);
}

TEST_CASE("Colony rankings order worst values first and break ties by colony identity") {
    PlayerRecord wellFed = record(ColonyId{9}, 1);
    wellFed.foodConsumed = 100;
    wellFed.stocks[resourceIndex(ResourceId::Food)] = 400;
    wellFed.labourSupplied = 5;
    wellFed.labourEmployed = 4;
    wellFed.corporationCounts[sectorIndex(SectorId::Food)] = 1;

    PlayerRecord shortFood = record(ColonyId{3}, 1);
    shortFood.foodConsumed = 100;
    shortFood.stocks[resourceIndex(ResourceId::Food)] = 50;
    shortFood.labourSupplied = 8;
    shortFood.labourEmployed = 3;
    shortFood.corporationCounts[sectorIndex(SectorId::Materials)] = 4;

    PlayerRecord noFoodUse = record(ColonyId{1}, 1);
    noFoodUse.labourSupplied = 7;
    noFoodUse.labourEmployed = 1;
    noFoodUse.corporationCounts[sectorIndex(SectorId::Fuel)] = 4;
    std::array<PlayerRecord, 3> const records{wellFed, shortFood, noFoodUse};

    std::vector<Client::EmpireColonySummary> const foodRanking =
        Client::rankEmpireColonies(records, Client::ColonyRankingMetric::FOOD_COVERAGE);
    REQUIRE(foodRanking.size() == 3);
    CHECK(foodRanking[0].colonyId == ColonyId{3});
    CHECK(foodRanking[1].colonyId == ColonyId{9});
    CHECK(foodRanking[2].colonyId == ColonyId{1});

    std::vector<Client::EmpireColonySummary> const unemploymentRanking =
        Client::rankEmpireColonies(records, Client::ColonyRankingMetric::UNEMPLOYMENT);
    CHECK(unemploymentRanking[0].colonyId == ColonyId{1});

    std::vector<Client::EmpireColonySummary> const corporationRanking =
        Client::rankEmpireColonies(records, Client::ColonyRankingMetric::CORPORATION_COUNT);
    CHECK(corporationRanking[0].colonyId == ColonyId{1});
}

TEST_CASE("Empire aggregations handle empty and single-record histories") {
    std::vector<PlayerRecord> const empty;
    CHECK(Client::aggregateEmpireHistory(empty).empty());
    CHECK(Client::rankEmpireColonies(empty, Client::ColonyRankingMetric::FOOD_COVERAGE).empty());
    std::array<Client::EmpireSectorSummary, Simulation::Economy::SECTOR_COUNT> const noSectors =
        Client::aggregateEmpireSectors(empty);
    CHECK(noSectors[sectorIndex(SectorId::Food)].output == 0);

    PlayerRecord only = record(ColonyId{4}, 1);
    only.population = 12;
    only.foodConsumed = 100;
    only.stocks[resourceIndex(ResourceId::Food)] = 250;
    only.corporationCounts[sectorIndex(SectorId::Energy)] = 2;
    only.produced[sectorIndex(SectorId::Energy)] = 700;
    std::array<PlayerRecord, 1> const records{only};

    std::vector<Client::EmpireTurnSummary> const history = Client::aggregateEmpireHistory(records);
    REQUIRE(history.size() == 1);
    CHECK(history.front().population == 12);
    std::vector<Client::EmpireColonySummary> const ranking =
        Client::rankEmpireColonies(records, Client::ColonyRankingMetric::FOOD_COVERAGE);
    REQUIRE(ranking.size() == 1);
    REQUIRE(ranking.front().foodCoverageTurns.has_value());
    CHECK(ranking.front().foodCoverageTurns.value_or(0.0) == 2.5);
    std::array<Client::EmpireSectorSummary, Simulation::Economy::SECTOR_COUNT> const sectors =
        Client::aggregateEmpireSectors(records);
    CHECK(sectors[sectorIndex(SectorId::Energy)].corporationCount == 2);
    CHECK(sectors[sectorIndex(SectorId::Energy)].output == 700);
    CHECK(!(sectors[sectorIndex(SectorId::Energy)].capacity.has_value()));
    CHECK(!(sectors[sectorIndex(SectorId::Energy)].employment.has_value()));
}
