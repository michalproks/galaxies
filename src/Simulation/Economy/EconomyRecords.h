#ifndef GALAXIES_SIMULATION_ECONOMY_ECONOMY_RECORDS_H
#define GALAXIES_SIMULATION_ECONOMY_ECONOMY_RECORDS_H

#include "ColonyTick.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace Simulation::Economy {

/** Player-visible condition of one corporation-owned building at turn end. */
enum class PlayerBuildingCondition : std::uint8_t { OPERATIONAL, IDLE, UNDER_CONSTRUCTION, MAINTENANCE_UNPAID };

struct PlayerBuildingDetail {
    BuildTier tier{BuildTier::Manual};
    std::int64_t staffedWorkers{};
    std::int64_t workerCapacity{};
    std::int64_t outputCapacity{};
    std::int64_t constructionValue{};
    PlayerBuildingCondition condition{PlayerBuildingCondition::OPERATIONAL};
    double occupancy{};
    bool operator==(PlayerBuildingDetail const &) const = default;
};

struct PlayerOrderDetail {
    OrderId id{};
    ResourceId resource{ResourceId::Credits};
    OrderSide side{OrderSide::Buy};
    std::int64_t price{};
    std::int64_t quantity{};
    std::int64_t filled{};
    bool operator==(PlayerOrderDetail const &) const = default;
};

/** Compact player-facing detail for one corporation in a completed turn. */
struct PlayerCorporationDetail {
    CorporationId id{};
    HouseholdId owner{};
    SectorId sector{SectorId::Food};
    std::int64_t credits{};
    std::int64_t creditChange{};
    std::int64_t plannedOutput{};
    std::int64_t actualOutput{};
    std::int64_t sold{};
    std::int64_t plannedCost{};
    std::int64_t targetPrice{};
    std::int64_t targetQuantity{};
    double breakEvenPrice{};
    ResourceAmounts stocks{};
    std::vector<PlayerBuildingDetail> buildings;
    std::vector<PlayerOrderDetail> orders;
    std::int64_t unpaidMaintenanceBuildings{};
    bool nearBankruptcy{};
    bool operator==(PlayerCorporationDetail const &other) const {
        return id == other.id && owner == other.owner && sector == other.sector && credits == other.credits &&
               creditChange == other.creditChange && plannedOutput == other.plannedOutput &&
               actualOutput == other.actualOutput && sold == other.sold && plannedCost == other.plannedCost &&
               targetPrice == other.targetPrice && targetQuantity == other.targetQuantity &&
               breakEvenPrice == other.breakEvenPrice && stocks.amounts == other.stocks.amounts &&
               buildings == other.buildings && orders == other.orders &&
               unpaidMaintenanceBuildings == other.unpaidMaintenanceBuildings && nearBankruptcy == other.nearBankruptcy;
    }
};

/** Compact player-facing summary plus expandable corporation details for one colony turn. */
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
    /** Per-sector rated output capacity and actual corporate labour employment. */
    std::array<std::int64_t, SECTOR_COUNT> sectorCapacity{};
    std::array<std::int64_t, SECTOR_COUNT> sectorEmployment{};
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
    /** Fixed event counters; household- and order-level diagnostics remain debug-only. */
    std::array<std::int64_t, SECTOR_COUNT> corporationsFounded{};
    std::int64_t corporationsDissolved{};
    std::int64_t householdsDissolved{};
    /** Expandable corporation details; kept separate from the compact headline fields. */
    std::vector<PlayerCorporationDetail> corporationDetails;

    /** Compares the complete player-facing record, including expandable corporation details. */
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
    std::vector<HouseholdTickOutcome> householdOutcomes;
    std::vector<CorporationTickOutcome> corporationOutcomes;
    std::vector<EconomyInvariantResult> invariantResults;
    std::array<std::int64_t, RESOURCE_COUNT> liquidatedResources{};
};

/** Creates an independent debug snapshot from a tick result. */
[[nodiscard]] DebugRecord makeDebugRecord(ColonyEconomy const &previous, ColonyTickResult const &result);

/** Default diagnostic rolling-window size when the visualization spec leaves N tunable. */
inline constexpr std::size_t DEFAULT_DEBUG_RECORD_WINDOW = 50;
/** Player warning threshold for corporations with a critically low operating balance. */
inline constexpr std::int64_t PLAYER_BANKRUPTCY_WARNING_CREDITS = 10'000;

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
