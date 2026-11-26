#ifndef GALAXIES_SIMULATION_ECONOMY_ECONOMY_RECORDS_H
#define GALAXIES_SIMULATION_ECONOMY_ECONOMY_RECORDS_H

#include "ColonyTick.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace Simulation::Economy {

/** Fixed-size player-facing summary for one committed colony turn. */
struct PlayerRecord {
    /** Colony and completed-turn identity. */
    ColonyId colonyId{};
    std::int64_t turnNumber{};
    /** Settled centicredit prices and their changes from the preceding turn. */
    std::array<std::int64_t, RESOURCE_COUNT> settlementPrices{};
    std::array<std::int64_t, RESOURCE_COUNT> priceChanges{};
    /** Colony stock, submitted buy demand, and filled buy demand, indexed by ResourceId. */
    std::array<std::int64_t, RESOURCE_COUNT> stocks{};
    std::array<std::int64_t, RESOURCE_COUNT> buyDemand{};
    std::array<std::int64_t, RESOURCE_COUNT> buyFilled{};
    /** End-of-turn corporation counts and production/sales, indexed by SectorId. */
    std::array<std::int64_t, SECTOR_COUNT> corporationCounts{};
    std::array<std::int64_t, SECTOR_COUNT> produced{};
    std::array<std::int64_t, SECTOR_COUNT> sold{};
    /** Colony population and net change across the completed turn. */
    std::int64_t population{};
    std::int64_t populationChange{};
    /** Household and corporation counts plus top-line credit distribution. */
    std::int64_t householdCount{};
    std::int64_t householdCredits{};
    std::int64_t corporationCredits{};
    double averageHouseholdCredits{};
    double averageSavingsPropensity{};
    /** Aggregate household consumption and reserve-health indicators. */
    std::int64_t foodConsumed{};
    std::int64_t servicesConsumed{};
    std::int64_t consumerGoodsConsumed{};
    double averageFoodReserveTurns{};
    std::int64_t householdsBelowOneFoodReserveTurn{};
    /** Household labour offers and the quantity employed at settlement. */
    std::int64_t labourSupplied{};
    std::int64_t labourEmployed{};
    /** Fixed event counters; no per-agent or order-level detail is retained here. */
    std::array<std::int64_t, SECTOR_COUNT> corporationsFounded{};
    std::int64_t corporationsDissolved{};
    std::int64_t householdsDissolved{};

    /** Compares the complete fixed player contract. */
    bool operator==(PlayerRecord const &) const = default;
};

/** Builds a small player record from the committed tick result and its prior colony state. */
[[nodiscard]] PlayerRecord makePlayerRecord(ColonyEconomy const &previous, ColonyTickResult const &result);

/** Full diagnostic snapshot for one colony turn; never part of player-facing data. */
struct DebugRecord { // NOLINT(bugprone-exception-escape)
    /** Colony and completed-turn identity. */
    ColonyId colonyId{};
    std::int64_t turnNumber{};
    /** Full state before and after the tick, including submitted order books and agent details. */
    ColonyEconomy beforeState;
    ColonyEconomy afterState;
    /** Settlement prices and positive per-order fills for every traded resource. */
    std::vector<SettlementOutcome> settlements;
    /** Turn founding, dissolution, agent-delta, and liquidation diagnostics. */
    FoundingMarketData marketData;
    std::vector<FoundedCorporation> founded;
    std::vector<DissolvedCorporation> dissolvedCorporations;
    std::vector<HouseholdId> dissolvedHouseholds;
    std::vector<HouseholdTickDelta> householdDeltas;
    std::vector<CorporationTickDelta> corporationDeltas;
    std::int64_t liquidationCredits{};
};

/** Creates an independent debug snapshot from a tick result. */
[[nodiscard]] DebugRecord makeDebugRecord(ColonyEconomy const &previous, ColonyTickResult const &result);

/** Default diagnostic rolling-window size when the visualization spec leaves N tunable. */
inline constexpr std::size_t DEFAULT_DEBUG_RECORD_WINDOW = 50;

/** Bounded in-memory debug record stream with an optional zero-work disabled path. */
class DebugRecorder {
public:
    /** Creates a recorder with a turn capacity and recording switch; zero capacity retains nothing. */
    explicit DebugRecorder(std::size_t windowSize = DEFAULT_DEBUG_RECORD_WINDOW, bool enabled = true);

    /** Enables or disables recording without changing existing snapshots. */
    void setEnabled(bool enabled) noexcept;

    /** Returns whether appends currently create and retain debug snapshots. */
    [[nodiscard]] bool enabled() const noexcept;

    /** Captures a turn only when enabled and capacity is non-zero. */
    void append(ColonyEconomy const &previous, ColonyTickResult const &result);

    /** Returns retained snapshots in chronological order; the reference lasts until mutation. */
    [[nodiscard]] std::deque<DebugRecord> const &records() const noexcept;

    /** Returns the configured maximum number of retained turn snapshots. */
    [[nodiscard]] std::size_t windowSize() const noexcept;

private:
    std::size_t mWindowSize{};
    bool mEnabled{};
    std::deque<DebugRecord> mRecords;
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_ECONOMY_RECORDS_H
