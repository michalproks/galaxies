#include "DebugOverlay.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>

namespace {

using Simulation::Economy::ColonyId;
using Simulation::Economy::DebugRecord;

[[nodiscard]] Client::InvariantBadge const &badge(std::vector<Client::InvariantBadge> const &badges,
                                                  std::string const &name) {
    auto const found = std::find_if(
        badges.begin(), badges.end(), [&name](Client::InvariantBadge const &item) { return item.name == name; });
    REQUIRE(found != badges.end());
    return *found;
}

struct RecordSeed {
    std::int64_t turn{};
    std::int64_t credits{};
};

[[nodiscard]] DebugRecord record(RecordSeed seed) {
    std::int64_t const turn = seed.turn;
    std::int64_t const credits = seed.credits;
    DebugRecord value;
    value.colonyId = ColonyId{2};
    value.turnNumber = turn;
    value.beforeState.id = ColonyId{2};
    value.beforeState.turnNumber = turn;
    value.afterState.id = ColonyId{2};
    value.afterState.turnNumber = turn + 1;
    Simulation::Economy::Household household;
    household.id = Simulation::Economy::HouseholdId{7};
    household.population = 100;
    household.credits = credits;
    value.beforeState.households.push_back(household);
    value.afterState.households.push_back(household);
    return value;
}

} // namespace

TEST_CASE("Debug scrub steps by one or ten turns and clamps to the retained window") {
    CHECK(Client::stepDebugTurn(15, {10, 30}, -1, false) == 14);
    CHECK(Client::stepDebugTurn(15, {10, 30}, 1, false) == 16);
    CHECK(Client::stepDebugTurn(15, {10, 30}, -1, true) == 10);
    CHECK(Client::stepDebugTurn(15, {10, 30}, 1, true) == 25);
    CHECK(Client::stepDebugTurn(10, {10, 30}, -1, true) == 10);
    CHECK(Client::stepDebugTurn(30, {10, 30}, 1, true) == 30);
    CHECK(Client::clampDebugTurn(100, {10, 30}) == 30);
    CHECK(Client::stepDebugTurn(4, {10, 30}, 1, false) == 11);
}

TEST_CASE("Debug record selection uses exact turn identity") {
    std::deque<DebugRecord> records{record({4, 10}), record({5, 20})};
    REQUIRE(Client::debugRecordAtTurn({records, 5}) != nullptr);
    CHECK(Client::debugRecordAtTurn({records, 5})->turnNumber == 5);
    CHECK(Client::debugRecordAtTurn({records, 6}) == nullptr);
}

TEST_CASE("Invariant badges derive failures from retained colony state") {
    DebugRecord value = record({3, 10});
    value.afterState.households.front().credits = 11;
    value.afterState.households.front().foodStock = 5;
    value.settlements.push_back({Simulation::Economy::ResourceId::Food, Simulation::Economy::PRICE_CAP + 1, 0, {}});
    auto const badges = Client::deriveInvariantBadges(value);
    CHECK(badge(badges, "Credits conserved").state == Client::InvariantState::FAIL);
    CHECK(badge(badges, "Standard resources conserved").state == Client::InvariantState::FAIL);
    CHECK(badge(badges, "Prices in range").state == Client::InvariantState::FAIL);
    value.afterState.households.front().credits = 13;
    value.afterState.households.front().foodStock = 0;
    value.liquidationCredits = 3;
    CHECK(badge(Client::deriveInvariantBadges(value), "Credits conserved").state == Client::InvariantState::PASS);
    CHECK(badge(Client::deriveInvariantBadges(value), "Standard resources conserved").state ==
          Client::InvariantState::UNAVAILABLE);
    value.afterState.households.front().credits = -1;
    CHECK(badge(Client::deriveInvariantBadges(value), "No agent overspent").state == Client::InvariantState::FAIL);
}

TEST_CASE("Debug record diff counts fields and preserves pipeline stage grouping") {
    DebugRecord before = record({4, 10});
    DebugRecord after = record({5, 20});
    after.beforeState.turnNumber = before.beforeState.turnNumber;
    after.afterState.turnNumber = before.afterState.turnNumber;
    after.beforeState.households.front().credits = before.beforeState.households.front().credits;
    after.afterState.households.front().credits = 25;
    std::vector<Client::DebugRecordChange> const changes = Client::diffDebugRecords({before, after});
    CHECK(changes.size() == 2);
    CHECK(std::is_sorted(changes.begin(),
                         changes.end(),
                         [](Client::DebugRecordChange const &left, Client::DebugRecordChange const &right) {
                             return left.stage < right.stage;
                         }));
    CHECK(std::any_of(changes.begin(), changes.end(), [](Client::DebugRecordChange const &change) {
        return change.stage == "Household state" && change.field.find("household.7.credits") != std::string::npos;
    }));
    CHECK(std::any_of(changes.begin(), changes.end(), [](Client::DebugRecordChange const &change) {
        return change.stage == "Identity";
    }));
}
