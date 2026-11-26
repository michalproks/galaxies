#ifndef GALAXIES_CLIENT_ECONOMY_UI_H
#define GALAXIES_CLIENT_ECONOMY_UI_H

#include "Client.h"
#include "EconomyChart.h"

#include <Server.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace Client {

/** Direction of a settled price change relative to the preceding turn. */
enum class PriceDirection : std::uint8_t { DOWN, UNCHANGED, UP };

/** Warning states derivable from fields in the player-facing economy history. */
enum class EconomyWarning : std::uint8_t { STARVATION_RISK, CONSUMER_MARKET_SHORTFALL };

/** ImGui-independent values needed by the selected-colony triage summary. */
struct EconomySummary {
    std::int64_t population{};
    std::optional<double> foodCoverageTurns;
    std::int64_t unemployed{};
    std::array<std::int64_t, 4> keyPrices{};
    std::array<PriceDirection, 4> keyPriceDirections{};
    std::array<std::int64_t, Simulation::Economy::SECTOR_COUNT> corporationCounts{};
    std::vector<EconomyWarning> warnings;
};

/** Returns the most recent record, or no value when the history is empty. */
[[nodiscard]] Simulation::Economy::PlayerRecord const *
latestEconomyRecord(std::span<Simulation::Economy::PlayerRecord const> records) noexcept;

/** Computes compact values from the latest player-facing record without accessing ImGui. */
[[nodiscard]] EconomySummary computeEconomySummary(std::span<Simulation::Economy::PlayerRecord const> records);

/** Classifies a price change in centicredits. */
[[nodiscard]] PriceDirection priceDirection(std::int64_t change) noexcept;

/** Derives only warning states exposed by the player-facing record contract. */
[[nodiscard]] std::vector<EconomyWarning> deriveEconomyWarnings(Simulation::Economy::PlayerRecord const &record);

/** Maps retained player records into the six chart groups used by the colony economy modal. */
[[nodiscard]] std::array<std::vector<ChartSeries>, 6>
buildEconomyChartSeries(std::span<Simulation::Economy::PlayerRecord const> records);

/** Selects a colony record by non-star body order and the matching snapshot-record order. */
[[nodiscard]] std::optional<Simulation::Economy::ColonyId> colonyIdForBody(Server::GameSnapshot const &snapshot,
                                                                           std::size_t bodyIndex) noexcept;

/** Stateful colony economy info-box summary and modal renderer. */
class EconomyUi {
public:
    /** Draws the selected-colony summary and any open modal using player history only. */
    void draw(std::string_view colonyName,
              std::optional<Simulation::Economy::ColonyId> selectedColony,
              Client const &client);

    /** Reports whether the colony modal is open and map camera input should be ignored. */
    [[nodiscard]] bool modalOpen() const noexcept;

private:
    void drawModal(Client const &client);

    std::optional<Simulation::Economy::ColonyId> mLastColonyId;
    std::optional<std::int64_t> mScrubTurn;
    std::optional<EconomyWarning> mHighlightedWarning;
    bool mModalOpen{};
    std::array<EconomyChart, 6> mCharts;
};

} // namespace Client

#endif // GALAXIES_CLIENT_ECONOMY_UI_H
