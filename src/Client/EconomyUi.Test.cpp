#include "EconomyUi.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using Simulation::Economy::ColonyEconomyRecordSnapshot;
using Simulation::Economy::ColonyId;
using Simulation::Economy::PlayerRecord;
using Simulation::Economy::ResourceId;

[[nodiscard]] PlayerRecord record(std::int64_t const turn) {
    PlayerRecord value;
    value.colonyId = ColonyId{4};
    value.turnNumber = turn;
    return value;
}

} // namespace

TEST_CASE("Economy summary derives current population, food coverage, unemployment, prices, and sectors") {
    PlayerRecord value = record(3);
    value.population = 20;
    value.stocks[static_cast<std::size_t>(ResourceId::Food)] = 2400;
    value.foodConsumed = 600;
    value.labourSupplied = 100;
    value.labourEmployed = 75;
    value.settlementPrices[static_cast<std::size_t>(ResourceId::Food)] = 250;
    value.priceChanges[static_cast<std::size_t>(ResourceId::Food)] = 10;
    value.corporationCounts = {1, 2, 3, 4, 5, 6};

    Client::EconomySummary const summary = Client::computeEconomySummary({&value, 1});

    CHECK(summary.population == 20);
    CHECK(summary.foodCoverageTurns == 4.0);
    CHECK(summary.unemployed == 25);
    CHECK(summary.keyPrices[0] == 250);
    CHECK(summary.keyPriceDirections[0] == Client::PriceDirection::UP);
    CHECK(summary.corporationCounts == value.corporationCounts);
    CHECK(summary.warnings.empty());
}

TEST_CASE("Economy summary classifies price changes and derives supported warnings") {
    CHECK(Client::priceDirection(1) == Client::PriceDirection::UP);
    CHECK(Client::priceDirection(0) == Client::PriceDirection::UNCHANGED);
    CHECK(Client::priceDirection(-1) == Client::PriceDirection::DOWN);

    PlayerRecord value = record(5);
    value.householdsBelowOneFoodReserveTurn = 2;
    value.buyDemand[static_cast<std::size_t>(ResourceId::Service)] = 7;
    value.buyDemand[static_cast<std::size_t>(ResourceId::ConsumerGoods)] = 2;
    std::vector<Client::EconomyWarning> const warnings = Client::deriveEconomyWarnings(value);

    REQUIRE(warnings.size() == 2);
    CHECK(warnings[0] == Client::EconomyWarning::STARVATION_RISK);
    CHECK(warnings[1] == Client::EconomyWarning::CONSUMER_MARKET_SHORTFALL);

    value.buyFilled[static_cast<std::size_t>(ResourceId::Service)] = 7;
    CHECK(Client::deriveEconomyWarnings(value).size() == 1);
}

TEST_CASE("Economy chart mapping preserves turns and produces all six player trend groups") {
    PlayerRecord first = record(1);
    first.settlementPrices[static_cast<std::size_t>(ResourceId::Food)] = 200;
    first.buyDemand[static_cast<std::size_t>(ResourceId::Food)] = 4000;
    first.buyFilled[static_cast<std::size_t>(ResourceId::Food)] = 3000;
    first.labourSupplied = 100;
    first.labourEmployed = 80;
    first.householdCredits = 1200;
    first.corporationCredits = 800;
    first.corporationCounts[0] = 2;

    PlayerRecord second = first;
    second.turnNumber = 2;
    second.settlementPrices[static_cast<std::size_t>(ResourceId::Food)] = 300;
    second.buyDemand[static_cast<std::size_t>(ResourceId::Food)] = 6000;

    std::array<PlayerRecord, 2> const records{first, second};
    std::array<std::vector<Client::ChartSeries>, 6> const charts = Client::buildEconomyChartSeries(records);

    CHECK(charts.size() == 6);
    REQUIRE(charts[0].size() == Simulation::Economy::RESOURCE_COUNT - 1);
    REQUIRE(charts[0][2].points.size() == 2);
    CHECK(charts[0][2].points[0].turn == 1);
    CHECK(charts[0][2].points[0].value == 2.0);
    CHECK(charts[0][2].points[1].value == 3.0);
    REQUIRE(charts[1].size() == 2);
    CHECK(charts[1][0].points[1].value == 6.0);
    CHECK(charts[1][1].points[0].value == 3.0);
    REQUIRE(charts[3].size() == 1);
    CHECK(charts[3][0].points[0].value == 20.0);
    REQUIRE(charts[4].size() == Simulation::Economy::SECTOR_COUNT);
    CHECK(charts[4][0].points[0].value == 2.0);
    REQUIRE(charts[5].size() == 2);
    CHECK(charts[5][0].points[0].value == 12.0);
    CHECK(charts[5][1].points[0].value == 8.0);
}

TEST_CASE("Selected non-star bodies map to economy records in stable order") {
    Server::GameSnapshot snapshot;
    snapshot.bodies = {
        {{}, 1.0, {}, Simulation::BodyKind::STAR, "Star"},
        {{}, 1.0, {}, Simulation::BodyKind::PLANET, "First"},
        {{}, 1.0, {}, Simulation::BodyKind::MOON, "Second"},
    };
    ColonyEconomyRecordSnapshot first;
    first.colonyId = ColonyId{9};
    ColonyEconomyRecordSnapshot second;
    second.colonyId = ColonyId{12};
    snapshot.economyRecords = std::make_shared<std::vector<ColonyEconomyRecordSnapshot> const>(
        std::vector<ColonyEconomyRecordSnapshot>{first, second});

    CHECK(!(Client::colonyIdForBody(snapshot, 0).has_value()));
    REQUIRE(Client::colonyIdForBody(snapshot, 1).has_value());
    CHECK(Client::colonyIdForBody(snapshot, 1) == ColonyId{9});
    CHECK(Client::colonyIdForBody(snapshot, 2) == ColonyId{12});
}
