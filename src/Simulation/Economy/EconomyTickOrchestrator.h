#ifndef GALAXIES_SIMULATION_ECONOMY_ECONOMY_TICK_ORCHESTRATOR_H
#define GALAXIES_SIMULATION_ECONOMY_ECONOMY_TICK_ORCHESTRATOR_H

#include "EconomyRecords.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace Simulation::Economy {

/** Events published together when one colony's economic turn commits. */
struct EconomyEventBatch {
    ColonyId colonyId{};
    std::int64_t turnNumber{};
    std::vector<SettlementOutcome> settlements;
    std::vector<FoundedCorporation> founded;
    std::vector<DissolvedCorporation> dissolvedCorporations;
    std::vector<HouseholdId> dissolvedHouseholds;
};

/** Player chart history and the separate diagnostic window for one colony. */
struct ColonyEconomyRecordSnapshot { // NOLINT(bugprone-exception-escape)
    ColonyId colonyId{};
    std::vector<PlayerRecord> playerHistory;
    std::deque<DebugRecord> debugRecords;
};

/** Coordinates atomic per-colony economic turns at game-time period boundaries. */
class EconomyTickOrchestrator {
public:
    /**
     * Creates an Earth-seeded orchestrator. workerCount bounds per-colony parallel work; zero
     * selects one worker. Colony work is independent and output slots retain sorted colony order.
     */
    explicit EconomyTickOrchestrator(std::size_t workerCount = 1, bool recordDebug = true);

    /** Creates an orchestrator from supplied colony states, sorted by stable colony identity. */
    EconomyTickOrchestrator(std::vector<ColonyEconomy> colonies, std::size_t workerCount, bool recordDebug = true);

    /**
     * Runs every missed 7-day turn up to gameHours and returns events from newly committed turns.
     * A failed turn throws and leaves that turn's states and records unchanged.
     */
    [[nodiscard]] std::vector<EconomyEventBatch> advanceTo(double gameHours);

    /** Returns the current states in their stable, sorted colony order. */
    [[nodiscard]] std::vector<ColonyEconomy> const &colonies() const noexcept;

    /** Returns retained player history and debug windows in stable colony order. */
    [[nodiscard]] std::vector<ColonyEconomyRecordSnapshot> recordSnapshots() const;

    /** Returns the number of successfully committed 7-day turns. */
    [[nodiscard]] std::int64_t completedPeriods() const noexcept;

    /** Returns and clears event batches from turns committed before a later turn failed. */
    [[nodiscard]] std::vector<EconomyEventBatch> takePendingEvents();

private:
    void runOneTurn();

    std::size_t mWorkerCount{};
    std::int64_t mCompletedPeriods{};
    std::vector<ColonyEconomy> mColonies;
    std::vector<FoundingMarketData> mPreviousMarketData;
    std::vector<std::vector<PlayerRecord>> mPlayerHistories;
    std::vector<DebugRecorder> mDebugRecorders;
    std::vector<EconomyEventBatch> mLatestEvents;
    std::vector<EconomyEventBatch> mPendingEvents;
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_ECONOMY_TICK_ORCHESTRATOR_H
