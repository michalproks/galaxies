#include "Client.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

namespace {

using Client::ColonyEconomyHistory;
using Simulation::Economy::ColonyEconomyRecordSnapshot;
using Simulation::Economy::ColonyId;
using Simulation::Economy::DebugRecord;
using Simulation::Economy::PlayerRecord;

[[nodiscard]] PlayerRecord playerRecord(ColonyId const colonyId, std::int64_t const turnNumber) {
    PlayerRecord record;
    record.colonyId = colonyId;
    record.turnNumber = turnNumber;
    return record;
}

[[nodiscard]] DebugRecord debugRecord(ColonyId const colonyId, std::int64_t const turnNumber) {
    DebugRecord record;
    record.colonyId = colonyId;
    record.turnNumber = turnNumber;
    return record;
}

[[nodiscard]] Server::GameSnapshot snapshotWithRecords(std::vector<PlayerRecord> playerHistory,
                                                       std::deque<DebugRecord> debugRecords,
                                                       ColonyId const colonyId = ColonyId{7}) {
    ColonyEconomyRecordSnapshot records;
    records.colonyId = colonyId;
    records.playerHistory = std::move(playerHistory);
    records.debugRecords = std::move(debugRecords);
    Server::GameSnapshot snapshot;
    snapshot.economyRecords = std::make_shared<std::vector<ColonyEconomyRecordSnapshot> const>(
        std::vector<ColonyEconomyRecordSnapshot>{std::move(records)});
    return snapshot;
}

} // namespace

TEST_CASE("Client receives economy records from server snapshots") {
    Server::Server server;
    Server::GameSnapshot const snapshot = server.snapshot(168.0);
    Client::Client client;
    std::size_t gameUpdates{};
    std::size_t economyUpdates{};
    client.sigUpdatedGame = [&gameUpdates]() { ++gameUpdates; };
    client.sigEconomyUpdated = [&economyUpdates](ColonyId const colonyId) {
        CHECK(colonyId == ColonyId{0});
        ++economyUpdates;
    };

    client.receiveGameState(snapshot);
    ColonyEconomyHistory const &history = client.economyHistory(ColonyId{0});

    REQUIRE(history.playerRecords.size() == 1);
    CHECK(history.playerRecords[0].turnNumber == 1);
    REQUIRE(history.debugRecords.size() == 1);
    CHECK(history.debugRecords[0].turnNumber == 1);
    CHECK(gameUpdates == 1);
    CHECK(economyUpdates == 1);

    client.receiveGameState(snapshot);
    CHECK(client.economyHistory(ColonyId{0}).playerRecords.size() == 1);
    CHECK(client.economyHistory(ColonyId{0}).debugRecords.size() == 1);
    CHECK(gameUpdates == 2);
    CHECK(economyUpdates == 1);
}

TEST_CASE("Client merges overlapping and out-of-order economy batches contiguously") {
    ColonyId const colonyId{7};
    Client::Client client;
    std::size_t economyUpdates{};
    client.sigEconomyUpdated = [&economyUpdates](ColonyId const) { ++economyUpdates; };

    client.receiveGameState(snapshotWithRecords({playerRecord(colonyId, 0), playerRecord(colonyId, 1)},
                                                {debugRecord(colonyId, 0), debugRecord(colonyId, 1)},
                                                colonyId));
    client.receiveGameState(snapshotWithRecords(
        {playerRecord(colonyId, 4), playerRecord(colonyId, 1), playerRecord(colonyId, 2), playerRecord(colonyId, 2)},
        {debugRecord(colonyId, 4), debugRecord(colonyId, 1), debugRecord(colonyId, 2)},
        colonyId));

    ColonyEconomyHistory const &beforeGapFill = client.economyHistory(colonyId);
    REQUIRE(beforeGapFill.playerRecords.size() == 3);
    REQUIRE(beforeGapFill.debugRecords.size() == 3);
    CHECK(beforeGapFill.playerRecords.back().turnNumber == 2);
    CHECK(beforeGapFill.debugRecords.back().turnNumber == 2);

    client.receiveGameState(snapshotWithRecords({playerRecord(colonyId, 3)}, {debugRecord(colonyId, 3)}, colonyId));
    ColonyEconomyHistory const &afterGapFill = client.economyHistory(colonyId);
    REQUIRE(afterGapFill.playerRecords.size() == 5);
    REQUIRE(afterGapFill.debugRecords.size() == 5);
    for (std::size_t index = 0; index < 5; ++index) {
        CHECK(afterGapFill.playerRecords[index].turnNumber == static_cast<std::int64_t>(index));
        CHECK(afterGapFill.debugRecords[index].turnNumber == static_cast<std::int64_t>(index));
    }
    CHECK(economyUpdates == 3);
}

TEST_CASE("Client evicts oldest economy records at each stream capacity") {
    ColonyId const colonyId{7};
    std::vector<PlayerRecord> playerHistory;
    std::deque<DebugRecord> debugRecords;
    for (std::int64_t turnNumber = 0; turnNumber < 2005; ++turnNumber) {
        playerHistory.push_back(playerRecord(colonyId, turnNumber));
        debugRecords.push_back(debugRecord(colonyId, turnNumber));
    }
    Client::Client client;
    client.receiveGameState(snapshotWithRecords(std::move(playerHistory), std::move(debugRecords), colonyId));

    ColonyEconomyHistory const &history = client.economyHistory(colonyId);
    REQUIRE(history.playerRecords.size() == Client::PLAYER_ECONOMY_HISTORY_CAPACITY);
    CHECK(history.playerRecords.front().turnNumber == 5);
    CHECK(history.playerRecords.back().turnNumber == 2004);
    REQUIRE(history.debugRecords.size() == Client::DEBUG_ECONOMY_HISTORY_CAPACITY);
    CHECK(history.debugRecords.front().turnNumber == 1805);
    CHECK(history.debugRecords.back().turnNumber == 2004);
}
