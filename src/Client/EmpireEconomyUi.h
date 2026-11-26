#ifndef GALAXIES_CLIENT_EMPIRE_ECONOMY_UI_H
#define GALAXIES_CLIENT_EMPIRE_ECONOMY_UI_H

#include "EconomyChart.h"

#include <Economy/EconomyTickOrchestrator.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Client {

enum class ColonyRankingMetric : std::uint8_t { FOOD_COVERAGE, UNEMPLOYMENT, CORPORATION_COUNT };

/** Aggregated player-facing totals for one completed empire turn. */
struct EmpireTurnSummary {
    std::int64_t turnNumber{};
    std::int64_t population{};
    std::int64_t creditsCenticredits{};
    std::int64_t householdCreditsCenticredits{};
    std::int64_t corporationCreditsCenticredits{};
    std::int64_t foodProduced{};
    std::int64_t foodConsumed{};
    std::int64_t unemployed{};
    std::int64_t corporationCount{};
    std::int64_t corporationsFounded{};
    std::int64_t colonyCount{};
    std::array<std::int64_t, Simulation::Economy::SECTOR_COUNT> corporationsBySector{};
    std::array<std::int64_t, Simulation::Economy::SECTOR_COUNT> outputBySector{};
    std::array<std::int64_t, Simulation::Economy::SECTOR_COUNT> demandBySector{};
    std::array<std::int64_t, Simulation::Economy::RESOURCE_COUNT> averagePricesCenticredits{};
};

/** Current player-visible comparison metrics for one colony. */
struct EmpireColonySummary {
    Simulation::Economy::ColonyId colonyId{};
    std::optional<double> foodCoverageTurns;
    std::int64_t unemployed{};
    std::int64_t corporationCount{};
};

/** Player-visible per-sector totals; capacity and sector employment are not in PlayerRecord. */
struct EmpireSectorSummary {
    Simulation::Economy::SectorId sector{Simulation::Economy::SectorId::Food};
    std::int64_t corporationCount{};
    std::int64_t output{};
    std::optional<std::int64_t> capacity;
    std::optional<std::int64_t> employment;
};

/** Sums records by turn, returning summaries in ascending turn order. */
[[nodiscard]] std::vector<EmpireTurnSummary>
aggregateEmpireHistory(std::span<Simulation::Economy::PlayerRecord const> records);

/** Returns the latest record per colony, sorted worst-first for the requested metric. */
[[nodiscard]] std::vector<EmpireColonySummary>
rankEmpireColonies(std::span<Simulation::Economy::PlayerRecord const> records, ColonyRankingMetric metric);

/** Aggregates the latest turn's corporation counts and output by sector. */
[[nodiscard]] std::array<EmpireSectorSummary, Simulation::Economy::SECTOR_COUNT>
aggregateEmpireSectors(std::span<Simulation::Economy::PlayerRecord const> records);

/** Draws the empire screen and exposes a colony selection request for the owning game UI. */
class EmpireEconomyUi {
public:
    /** Opens the screen when clicked in the top bar. */
    void drawTopBarButton();

    /** Draws the screen from player-facing colony histories only. */
    void draw(std::span<Simulation::Economy::ColonyEconomyRecordSnapshot const> records);

    /** Returns and clears the requested colony after a ranking-row activation. */
    [[nodiscard]] std::optional<Simulation::Economy::ColonyId> consumeRequestedColony() noexcept;

    /** Returns the current colony selection request without clearing it. */
    [[nodiscard]] std::optional<Simulation::Economy::ColonyId> requestedColony() const noexcept;

private:
    [[nodiscard]] static std::vector<Simulation::Economy::PlayerRecord>
    flattenRecords(std::span<Simulation::Economy::ColonyEconomyRecordSnapshot const> records);

    bool mIsOpen{};
    ColonyRankingMetric mRankingMetric{ColonyRankingMetric::FOOD_COVERAGE};
    std::optional<Simulation::Economy::ColonyId> mRequestedColony;
    std::array<EconomyChart, 5> mCharts;
};

} // namespace Client

#endif // GALAXIES_CLIENT_EMPIRE_ECONOMY_UI_H
