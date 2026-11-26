#include "EmpireEconomyUi.h"

#include <imgui.h>

#include <Economy/Resources.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace Client {
namespace {

    using Simulation::Economy::ColonyId;
    using Simulation::Economy::PlayerRecord;
    using Simulation::Economy::ResourceId;
    using Simulation::Economy::SectorId;

    constexpr std::array<SectorId, Simulation::Economy::SECTOR_COUNT> SECTORS{SectorId::Food,
                                                                              SectorId::Materials,
                                                                              SectorId::Fuel,
                                                                              SectorId::Energy,
                                                                              SectorId::Service,
                                                                              SectorId::ConsumerGoods};
    constexpr std::array<ResourceId, Simulation::Economy::SECTOR_COUNT> SECTOR_RESOURCES{ResourceId::Food,
                                                                                         ResourceId::Materials,
                                                                                         ResourceId::Fuel,
                                                                                         ResourceId::Energy,
                                                                                         ResourceId::Service,
                                                                                         ResourceId::ConsumerGoods};

    [[nodiscard]] constexpr std::size_t sectorIndex(SectorId const sector) noexcept {
        return static_cast<std::size_t>(sector);
    }

    [[nodiscard]] constexpr std::size_t resourceIndex(ResourceId const resource) noexcept {
        return static_cast<std::size_t>(resource);
    }

    [[nodiscard]] std::int64_t sumCorporations(PlayerRecord const &record) noexcept {
        std::int64_t total{};
        for (std::int64_t const count : record.corporationCounts) {
            total += count;
        }
        return total;
    }

    [[nodiscard]] std::int64_t sumFoundings(PlayerRecord const &record) noexcept {
        std::int64_t total{};
        for (std::int64_t const count : record.corporationsFounded) {
            total += count;
        }
        return total;
    }

    [[nodiscard]] std::string colonyLabel(ColonyId const colonyId) {
        return "Colony " + std::to_string(colonyId.value);
    }

    [[nodiscard]] ChartSeries makeSeries(std::string const &label,
                                         ChartUnit unit,
                                         std::span<EmpireTurnSummary const> history,
                                         std::int64_t EmpireTurnSummary::*field,
                                         double const scale = 1.0) {
        ChartSeries series{label, unit, {}, std::nullopt};
        series.points.reserve(history.size());
        for (EmpireTurnSummary const &turn : history) {
            series.points.push_back({turn.turnNumber, static_cast<double>(turn.*field) / scale});
        }
        return series;
    }

    [[nodiscard]] std::string formatted(double const value, ChartUnit const unit, double const scale = 1.0) {
        return formatChartValue(value / scale, unit);
    }

} // namespace

std::vector<EmpireTurnSummary> aggregateEmpireHistory(std::span<PlayerRecord const> records) {
    std::map<std::int64_t, EmpireTurnSummary> byTurn;
    std::map<std::int64_t, std::array<std::int64_t, Simulation::Economy::RESOURCE_COUNT>> priceSums;
    std::map<std::int64_t, std::int64_t> priceCounts;
    for (PlayerRecord const &record : records) {
        EmpireTurnSummary &turn = byTurn[record.turnNumber];
        turn.turnNumber = record.turnNumber;
        turn.population += record.population;
        turn.householdCreditsCenticredits += record.householdCredits;
        turn.corporationCreditsCenticredits += record.corporationCredits;
        turn.creditsCenticredits += record.householdCredits + record.corporationCredits;
        turn.foodProduced += record.produced[sectorIndex(SectorId::Food)];
        turn.foodConsumed += record.foodConsumed;
        turn.unemployed += std::max<std::int64_t>(0, record.labourSupplied - record.labourEmployed);
        turn.corporationCount += sumCorporations(record);
        turn.corporationsFounded += sumFoundings(record);
        ++turn.colonyCount;
        for (std::size_t index = 0; index < Simulation::Economy::SECTOR_COUNT; ++index) {
            turn.corporationsBySector[index] += record.corporationCounts[index];
            turn.outputBySector[index] += record.produced[index];
            turn.demandBySector[index] += record.buyDemand[resourceIndex(SECTOR_RESOURCES[index])];
        }
        std::array<std::int64_t, Simulation::Economy::RESOURCE_COUNT> &prices = priceSums[record.turnNumber];
        for (std::size_t index = 0; index < prices.size(); ++index) {
            prices[index] += record.settlementPrices[index];
        }
        ++priceCounts[record.turnNumber];
    }

    std::vector<EmpireTurnSummary> result;
    result.reserve(byTurn.size());
    for (auto &[turnNumber, turn] : byTurn) {
        std::int64_t const priceCount = priceCounts[turnNumber];
        for (std::size_t index = 0; index < turn.averagePricesCenticredits.size(); ++index) {
            turn.averagePricesCenticredits[index] = priceSums[turnNumber][index] / priceCount;
        }
        result.push_back(turn);
    }
    return result;
}

std::vector<EmpireColonySummary> rankEmpireColonies(std::span<PlayerRecord const> records,
                                                    ColonyRankingMetric const metric) {
    std::map<ColonyId, PlayerRecord> latest;
    for (PlayerRecord const &record : records) {
        auto const found = latest.find(record.colonyId);
        if (found == latest.end() || found->second.turnNumber < record.turnNumber) {
            latest.insert_or_assign(record.colonyId, record);
        }
    }

    std::vector<EmpireColonySummary> result;
    result.reserve(latest.size());
    for (auto const &[colonyId, record] : latest) {
        std::int64_t const foodConsumption = record.foodConsumed;
        std::optional<double> coverage;
        if (foodConsumption > 0) {
            coverage = static_cast<double>(record.stocks[resourceIndex(ResourceId::Food)]) /
                       static_cast<double>(foodConsumption);
        }
        result.push_back({colonyId,
                          coverage,
                          std::max<std::int64_t>(0, record.labourSupplied - record.labourEmployed),
                          sumCorporations(record)});
    }
    std::sort(
        result.begin(), result.end(), [metric](EmpireColonySummary const &left, EmpireColonySummary const &right) {
            switch (metric) {
            case ColonyRankingMetric::FOOD_COVERAGE:
                if (left.foodCoverageTurns != right.foodCoverageTurns) {
                    if (!left.foodCoverageTurns.has_value()) {
                        return false;
                    }
                    if (!right.foodCoverageTurns.has_value()) {
                        return true;
                    }
                    return left.foodCoverageTurns.value() < right.foodCoverageTurns.value();
                }
                break;
            case ColonyRankingMetric::UNEMPLOYMENT:
                if (left.unemployed != right.unemployed) {
                    return left.unemployed > right.unemployed;
                }
                break;
            case ColonyRankingMetric::CORPORATION_COUNT:
                if (left.corporationCount != right.corporationCount) {
                    return left.corporationCount > right.corporationCount;
                }
                break;
            }
            return left.colonyId < right.colonyId;
        });
    return result;
}

std::array<EmpireSectorSummary, Simulation::Economy::SECTOR_COUNT>
aggregateEmpireSectors(std::span<PlayerRecord const> records) {
    std::array<EmpireSectorSummary, Simulation::Economy::SECTOR_COUNT> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index].sector = SECTORS[index];
    }
    if (records.empty()) {
        return result;
    }
    std::int64_t latestTurn{};
    for (PlayerRecord const &record : records) {
        latestTurn = std::max(latestTurn, record.turnNumber);
    }
    for (PlayerRecord const &record : records) {
        if (record.turnNumber != latestTurn) {
            continue;
        }
        for (std::size_t index = 0; index < result.size(); ++index) {
            result[index].corporationCount += record.corporationCounts[index];
            result[index].output += record.produced[index];
            result[index].capacity = result[index].capacity.value_or(0) + record.sectorCapacity[index];
            result[index].employment = result[index].employment.value_or(0) + record.sectorEmployment[index];
        }
    }
    return result;
}

void EmpireEconomyUi::drawTopBarButton() {
    if (ImGui::Button("Empire Economy")) {
        mIsOpen = true;
    }
}

void EmpireEconomyUi::draw(std::span<Simulation::Economy::ColonyEconomyRecordSnapshot const> records) {
    if (!mIsOpen) {
        return;
    }
    constexpr ImGuiWindowFlags WINDOW_FLAGS = static_cast<ImGuiWindowFlags>(
        static_cast<unsigned>(ImGuiWindowFlags_NoResize) | static_cast<unsigned>(ImGuiWindowFlags_NoMove));
    ImGuiIO const &io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.04F, io.DisplaySize.y * 0.08F}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({io.DisplaySize.x * 0.92F, io.DisplaySize.y * 0.84F}, ImGuiCond_Always);
    if (!ImGui::Begin("Empire Economy", &mIsOpen, WINDOW_FLAGS)) {
        ImGui::End();
        return;
    }

    std::vector<PlayerRecord> const flatRecords = flattenRecords(records);
    std::vector<EmpireTurnSummary> const history = aggregateEmpireHistory(flatRecords);
    if (history.empty()) {
        ImGui::TextUnformatted("No completed economy turns yet.");
        ImGui::End();
        return;
    }

    EmpireTurnSummary const &current = history.back();
    ImGui::Text("Population: %s", formatted(static_cast<double>(current.population), ChartUnit::PEOPLE).c_str());
    ImGui::SameLine();
    ImGui::Text("Credits: %s",
                formatted(static_cast<double>(current.creditsCenticredits), ChartUnit::CREDITS, 100.0).c_str());
    ImGui::SameLine();
    ImGui::Text("Food: +%s / -%s tons",
                formatted(static_cast<double>(current.foodProduced), ChartUnit::TONS, 1000.0).c_str(),
                formatted(static_cast<double>(current.foodConsumed), ChartUnit::TONS, 1000.0).c_str());
    ImGui::SameLine();
    ImGui::Text(
        "Food net: %s tons",
        formatted(static_cast<double>(current.foodProduced - current.foodConsumed), ChartUnit::TONS, 1000.0).c_str());
    ImGui::SameLine();
    ImGui::Text("Unemployed: %s", formatted(static_cast<double>(current.unemployed), ChartUnit::PERSON_HOURS).c_str());
    ImGui::SameLine();
    ImGui::Text("Colonies: %lld", static_cast<long long>(current.colonyCount));
    ImGui::SameLine();
    ImGui::Text("Corporations: %lld", static_cast<long long>(current.corporationCount));
    ImGui::Text("Materials net: %s tons",
                formatted(static_cast<double>(current.outputBySector[sectorIndex(SectorId::Materials)] -
                                              current.demandBySector[sectorIndex(SectorId::Materials)]),
                          ChartUnit::TONS,
                          1000.0)
                    .c_str());
    ImGui::SameLine();
    ImGui::Text("Fuel net: %s tons",
                formatted(static_cast<double>(current.outputBySector[sectorIndex(SectorId::Fuel)] -
                                              current.demandBySector[sectorIndex(SectorId::Fuel)]),
                          ChartUnit::TONS,
                          1000.0)
                    .c_str());
    ImGui::SameLine();
    ImGui::Text("Energy net: %s",
                formatted(static_cast<double>(current.outputBySector[sectorIndex(SectorId::Energy)] -
                                              current.demandBySector[sectorIndex(SectorId::Energy)]),
                          ChartUnit::TERAWATTS)
                    .c_str());

    ImGui::SameLine();
    ImGui::Text("Foundings this turn: %lld", static_cast<long long>(current.corporationsFounded));

    std::array<ChartSeries, 1> population{
        makeSeries("Population", ChartUnit::PEOPLE, history, &EmpireTurnSummary::population)};
    EconomyChartOptions populationChartOptions;
    populationChartOptions.title = "Population over time";
    populationChartOptions.height = 220.0F;
    populationChartOptions.enableScrubber = false;
    mCharts[0].draw("Empire population", population, populationChartOptions);

    std::array<ChartSeries, 2> credits{
        makeSeries("Households", ChartUnit::CREDITS, history, &EmpireTurnSummary::householdCreditsCenticredits, 100.0),
        makeSeries(
            "Corporations", ChartUnit::CREDITS, history, &EmpireTurnSummary::corporationCreditsCenticredits, 100.0)};
    EconomyChartOptions creditsChartOptions;
    creditsChartOptions.title = "Credits by owner";
    creditsChartOptions.height = 220.0F;
    creditsChartOptions.enableScrubber = false;
    mCharts[1].draw("Empire credits by owner", credits, creditsChartOptions);

    std::array<ChartSeries, 2> foodBalance{
        makeSeries("Produced", ChartUnit::TONS, history, &EmpireTurnSummary::foodProduced, 1000.0),
        makeSeries("Consumed", ChartUnit::TONS, history, &EmpireTurnSummary::foodConsumed, 1000.0)};
    EconomyChartOptions foodChartOptions;
    foodChartOptions.title = "Food production and consumption";
    foodChartOptions.height = 220.0F;
    foodChartOptions.enableScrubber = false;
    mCharts[2].draw("Empire food balance", foodBalance, foodChartOptions);

    std::vector<ChartSeries> corporationCounts;
    corporationCounts.reserve(SECTORS.size());
    for (std::size_t index = 0; index < SECTORS.size(); ++index) {
        ChartSeries series{
            std::string(Simulation::Economy::displayName(SECTOR_RESOURCES[index])), ChartUnit::COUNT, {}, std::nullopt};
        series.points.reserve(history.size());
        for (EmpireTurnSummary const &turn : history) {
            series.points.push_back({turn.turnNumber, static_cast<double>(turn.corporationsBySector[index])});
        }
        corporationCounts.push_back(std::move(series));
    }
    EconomyChartOptions corporationChartOptions;
    corporationChartOptions.title = "Corporations by sector";
    corporationChartOptions.height = 220.0F;
    corporationChartOptions.enableScrubber = false;
    mCharts[3].draw("Empire corporations by sector", corporationCounts, corporationChartOptions);

    std::vector<ChartSeries> prices;
    prices.reserve(Simulation::Economy::RESOURCE_COUNT);
    for (std::size_t resource = 0; resource < Simulation::Economy::RESOURCE_COUNT; ++resource) {
        ChartSeries series{std::string(Simulation::Economy::displayName(static_cast<ResourceId>(resource))),
                           ChartUnit::CREDITS,
                           {},
                           std::nullopt};
        series.points.reserve(history.size());
        for (EmpireTurnSummary const &turn : history) {
            series.points.push_back(
                {turn.turnNumber, static_cast<double>(turn.averagePricesCenticredits[resource]) / 100.0});
        }
        prices.push_back(std::move(series));
    }
    EconomyChartOptions priceChartOptions;
    priceChartOptions.title = "Average settlement prices";
    priceChartOptions.height = 220.0F;
    priceChartOptions.enableScrubber = false;
    mCharts[4].draw("Empire average prices", prices, priceChartOptions);

    if (ImGui::CollapsingHeader("Per-sector breakdown", ImGuiTreeNodeFlags_DefaultOpen)) {
        constexpr ImGuiTableFlags TABLE_FLAGS = static_cast<ImGuiTableFlags>(
            static_cast<unsigned>(ImGuiTableFlags_Borders) | static_cast<unsigned>(ImGuiTableFlags_RowBg));
        if (ImGui::BeginTable("EmpireSectorTable", 5, TABLE_FLAGS)) {
            ImGui::TableSetupColumn("Sector");
            ImGui::TableSetupColumn("Corporations");
            ImGui::TableSetupColumn("Capacity");
            ImGui::TableSetupColumn("Output");
            ImGui::TableSetupColumn("Employment");
            ImGui::TableHeadersRow();
            for (EmpireSectorSummary const &sector : aggregateEmpireSectors(flatRecords)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                std::string const sectorName(
                    Simulation::Economy::displayName(SECTOR_RESOURCES[sectorIndex(sector.sector)]));
                ImGui::TextUnformatted(sectorName.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(sector.corporationCount));
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(sector.capacity.value_or(0)));
                ImGui::TableNextColumn();
                ImGui::Text("%s",
                            formatted(static_cast<double>(sector.output),
                                      sector.sector == SectorId::Energy ? ChartUnit::TERAWATTS : ChartUnit::TONS,
                                      sector.sector == SectorId::Energy ? 1.0 : 1000.0)
                                .c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(sector.employment.value_or(0)));
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Colony rankings", ImGuiTreeNodeFlags_DefaultOpen)) {
        constexpr std::array<char const *, 3> RANKING_LABELS{"Food coverage", "Unemployment", "Corporations"};
        int rankingIndex = static_cast<int>(mRankingMetric);
        if (ImGui::Combo(
                "Worst first", &rankingIndex, RANKING_LABELS.data(), static_cast<int>(RANKING_LABELS.size()))) {
            mRankingMetric = static_cast<ColonyRankingMetric>(rankingIndex);
        }
        constexpr ImGuiTableFlags TABLE_FLAGS = static_cast<ImGuiTableFlags>(
            static_cast<unsigned>(ImGuiTableFlags_Borders) | static_cast<unsigned>(ImGuiTableFlags_RowBg));
        if (ImGui::BeginTable("EmpireColonyTable", 4, TABLE_FLAGS)) {
            ImGui::TableSetupColumn("Colony");
            ImGui::TableSetupColumn("Food coverage (turns)");
            ImGui::TableSetupColumn("Unemployed");
            ImGui::TableSetupColumn("Corporations");
            ImGui::TableHeadersRow();
            for (EmpireColonySummary const &colony : rankEmpireColonies(flatRecords, mRankingMetric)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                std::string const label = colonyLabel(colony.colonyId);
                if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                    mRequestedColony = colony.colonyId;
                    mIsOpen = false;
                }
                ImGui::TableNextColumn();
                if (colony.foodCoverageTurns.has_value()) {
                    ImGui::Text("%.2f", colony.foodCoverageTurns.value());
                } else {
                    ImGui::TextUnformatted("N/A");
                }
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(colony.unemployed));
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(colony.corporationCount));
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

std::optional<Simulation::Economy::ColonyId> EmpireEconomyUi::consumeRequestedColony() noexcept {
    std::optional<Simulation::Economy::ColonyId> const result = mRequestedColony;
    mRequestedColony.reset();
    return result;
}

std::optional<Simulation::Economy::ColonyId> EmpireEconomyUi::requestedColony() const noexcept {
    return mRequestedColony;
}

std::vector<Simulation::Economy::PlayerRecord>
EmpireEconomyUi::flattenRecords(std::span<Simulation::Economy::ColonyEconomyRecordSnapshot const> records) {
    std::vector<Simulation::Economy::PlayerRecord> result;
    for (Simulation::Economy::ColonyEconomyRecordSnapshot const &colony : records) {
        result.insert(result.end(), colony.playerHistory.begin(), colony.playerHistory.end());
    }
    return result;
}

} // namespace Client
