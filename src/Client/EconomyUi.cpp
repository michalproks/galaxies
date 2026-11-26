#include "EconomyUi.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>

namespace Client {
namespace {

    using Simulation::Economy::PlayerRecord;
    using Simulation::Economy::ResourceId;

    constexpr std::array<ResourceId, 4> KEY_RESOURCES{
        ResourceId::Food, ResourceId::Energy, ResourceId::Materials, ResourceId::ConsumerGoods};
    constexpr std::array<ResourceId, Simulation::Economy::RESOURCE_COUNT - 1> MARKET_RESOURCES{
        ResourceId::Labour,
        ResourceId::Energy,
        ResourceId::Food,
        ResourceId::Materials,
        ResourceId::Fuel,
        ResourceId::Service,
        ResourceId::ConsumerGoods,
    };
    constexpr std::array<ResourceId, 4> STORED_RESOURCES{
        ResourceId::Food, ResourceId::Materials, ResourceId::Fuel, ResourceId::ConsumerGoods};
    constexpr std::array<std::string_view, 6> CHART_TITLES{
        "Resource prices",
        "Food demand and filled",
        "Consumer demand and filled",
        "Unemployment",
        "Corporations by sector",
        "Credits by owner",
    };
    constexpr float INFO_WINDOW_WIDTH = 300.0F;
    constexpr float INFO_WINDOW_HEIGHT = 178.0F;
    constexpr float WINDOW_PADDING = 8.0F;
    constexpr float MODAL_WIDTH = 1180.0F;
    constexpr float MODAL_HEIGHT = 760.0F;

    [[nodiscard]] std::size_t resourceIndex(ResourceId const resource) {
        return static_cast<std::size_t>(resource);
    }

    [[nodiscard]] char const *directionArrow(PriceDirection const direction) noexcept {
        switch (direction) {
        case PriceDirection::DOWN:
            return "v";
        case PriceDirection::UNCHANGED:
            return "-";
        case PriceDirection::UP:
            return "^";
        }
        return "-";
    }

    [[nodiscard]] std::string warningLabel(EconomyWarning const warning) {
        switch (warning) {
        case EconomyWarning::STARVATION_RISK:
            return "Food reserve risk";
        case EconomyWarning::CONSUMER_MARKET_SHORTFALL:
            return "Consumer shortfall";
        }
        return "Economy warning";
    }

    [[nodiscard]] PlayerRecord const *findRecord(std::span<PlayerRecord const> records,
                                                 std::optional<std::int64_t> const requestedTurn) {
        if (records.empty()) {
            return nullptr;
        }
        if (!requestedTurn.has_value()) {
            return latestEconomyRecord(records);
        }
        std::int64_t const targetTurn = *requestedTurn;
        auto const found = std::lower_bound(
            records.begin(), records.end(), targetTurn, [](PlayerRecord const &record, std::int64_t const turn) {
                return record.turnNumber < turn;
            });
        if (found == records.end()) {
            return &records.back();
        }
        if (found == records.begin() || found->turnNumber == targetTurn) {
            return &*found;
        }
        PlayerRecord const &previous = *(found - 1);
        return targetTurn - previous.turnNumber <= found->turnNumber - targetTurn ? &previous : &*found;
    }

    [[nodiscard]] double quantityInTons(std::int64_t const quantity) noexcept {
        return static_cast<double>(quantity) / 1000.0;
    }

    [[nodiscard]] std::optional<double> coverageTurns(PlayerRecord const &record, ResourceId const resource) noexcept {
        std::int64_t consumption{};
        if (resource == ResourceId::Food) {
            consumption = record.foodConsumed;
        } else {
            consumption = record.buyFilled[resourceIndex(resource)];
        }
        if (consumption <= 0) {
            return std::nullopt;
        }
        return static_cast<double>(record.stocks[resourceIndex(resource)]) / static_cast<double>(consumption);
    }

    [[nodiscard]] char const *sectorName(std::size_t const index) noexcept {
        constexpr std::array<std::string_view, Simulation::Economy::SECTOR_COUNT> NAMES{
            "Food", "Materials", "Fuel", "Energy", "Services", "Consumer goods"};
        return index < NAMES.size() ? NAMES[index].data() : "Unknown";
    }

    void drawPriceRow(PlayerRecord const &record, ResourceId const resource) {
        std::size_t const index = resourceIndex(resource);
        PriceDirection const direction = priceDirection(record.priceChanges[index]);
        ImGui::Text("%.*s: %.2f %s",
                    static_cast<int>(Simulation::Economy::displayName(resource).size()),
                    Simulation::Economy::displayName(resource).data(),
                    static_cast<double>(record.settlementPrices[index]) / 100.0,
                    directionArrow(direction));
    }

    void drawStatusColumn(PlayerRecord const &record, std::optional<EconomyWarning> const highlight) {
        ImGui::Text("Turn %lld", static_cast<long long>(record.turnNumber));
        ImGui::Separator();
        ImGui::Text("Population: %lld", static_cast<long long>(record.population));
        ImGui::Text("Households: %lld", static_cast<long long>(record.householdCount));
        ImGui::Text("Avg. wealth: %.2f cr", record.averageHouseholdCredits / 100.0);
        ImGui::Text("Savings propensity: %.1f%%", record.averageSavingsPropensity * 100.0);
        ImGui::Text("Food reserves: %.2f turns", record.averageFoodReserveTurns);
        if (record.householdsBelowOneFoodReserveTurn > 0) {
            ImGui::SameLine();
            ImGui::TextColored({1.0F, 0.35F, 0.2F, 1.0F}, "FAMINE RISK");
        }

        ImGui::SeparatorText("Prices");
        for (ResourceId const resource : MARKET_RESOURCES) {
            drawPriceRow(record, resource);
        }
        ImGui::SeparatorText("Stocks and coverage");
        for (ResourceId const resource : STORED_RESOURCES) {
            std::optional<double> const turns = coverageTurns(record, resource);
            ImGui::Text("%.*s: %.2f t",
                        static_cast<int>(Simulation::Economy::displayName(resource).size()),
                        Simulation::Economy::displayName(resource).data(),
                        quantityInTons(record.stocks[resourceIndex(resource)]));
            ImGui::SameLine();
            if (turns.has_value()) {
                ImGui::Text("(%.1f turns)", *turns);
            } else {
                ImGui::Text("(no current demand)");
            }
        }

        ImGui::SeparatorText("Food balance");
        ImGui::Text("Demand %.2f t / filled %.2f t",
                    quantityInTons(record.buyDemand[resourceIndex(ResourceId::Food)]),
                    quantityInTons(record.buyFilled[resourceIndex(ResourceId::Food)]));
        if (highlight == EconomyWarning::STARVATION_RISK) {
            ImGui::SetScrollHereY();
        }

        ImGui::SeparatorText("Consumer markets");
        ImGui::Text("Services: %lld / %lld",
                    static_cast<long long>(record.buyFilled[resourceIndex(ResourceId::Service)]),
                    static_cast<long long>(record.buyDemand[resourceIndex(ResourceId::Service)]));
        ImGui::Text("Consumer goods: %.2f t / %.2f t",
                    quantityInTons(record.buyFilled[resourceIndex(ResourceId::ConsumerGoods)]),
                    quantityInTons(record.buyDemand[resourceIndex(ResourceId::ConsumerGoods)]));
        if (highlight == EconomyWarning::CONSUMER_MARKET_SHORTFALL) {
            ImGui::SetScrollHereY();
        }

        ImGui::SeparatorText("Labour");
        std::int64_t const unemployed = std::max<std::int64_t>(0, record.labourSupplied - record.labourEmployed);
        ImGui::Text("Supplied %lld / employed %lld / unemployed %lld",
                    static_cast<long long>(record.labourSupplied),
                    static_cast<long long>(record.labourEmployed),
                    static_cast<long long>(unemployed));
    }

    void drawCorporationTable(PlayerRecord const &record, std::optional<EconomyWarning> const highlight) {
        ImGui::TextUnformatted("Corporations");
        if (highlight == EconomyWarning::CONSUMER_MARKET_SHORTFALL) {
            ImGui::SetScrollHereY();
        }
        ImGuiTableFlags const tableFlags = static_cast<ImGuiTableFlags>(static_cast<unsigned>(ImGuiTableFlags_Borders) |
                                                                        static_cast<unsigned>(ImGuiTableFlags_RowBg));
        if (ImGui::BeginTable("##EconomyCorporations", 4, tableFlags)) {
            ImGui::TableSetupColumn("Sector");
            ImGui::TableSetupColumn("Companies");
            ImGui::TableSetupColumn("Produced");
            ImGui::TableSetupColumn("Sold");
            ImGui::TableHeadersRow();
            for (std::size_t sector = 0; sector < Simulation::Economy::SECTOR_COUNT; ++sector) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                bool const expanded = ImGui::TreeNodeEx(sectorName(sector), ImGuiTreeNodeFlags_SpanAllColumns);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%lld", static_cast<long long>(record.corporationCounts[sector]));
                ImGui::TableSetColumnIndex(2);
                if (sector == 0 || sector == 1 || sector == 2 || sector == 5) {
                    ImGui::Text("%.2f t", quantityInTons(record.produced[sector]));
                } else {
                    ImGui::Text("%lld", static_cast<long long>(record.produced[sector]));
                }
                ImGui::TableSetColumnIndex(3);
                if (sector == 0 || sector == 1 || sector == 2 || sector == 5) {
                    ImGui::Text("%.2f t", quantityInTons(record.sold[sector]));
                } else {
                    ImGui::Text("%lld", static_cast<long long>(record.sold[sector]));
                }
                if (expanded) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextWrapped(
                        "Per-corporation buildings, plans, and orders are not included in the player-facing history.");
                    ImGui::TreePop();
                }
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::Text("Corporation credits: %.2f cr", static_cast<double>(record.corporationCredits) / 100.0);
    }

} // namespace

PlayerRecord const *latestEconomyRecord(std::span<PlayerRecord const> const records) noexcept {
    return records.empty() ? nullptr : &records.back();
}

PriceDirection priceDirection(std::int64_t const change) noexcept {
    if (change > 0) {
        return PriceDirection::UP;
    }
    if (change < 0) {
        return PriceDirection::DOWN;
    }
    return PriceDirection::UNCHANGED;
}

std::vector<EconomyWarning> deriveEconomyWarnings(PlayerRecord const &record) {
    std::vector<EconomyWarning> warnings;
    if (record.householdsBelowOneFoodReserveTurn > 0) {
        warnings.push_back(EconomyWarning::STARVATION_RISK);
    }
    bool const servicesDemanded = record.buyDemand[resourceIndex(ResourceId::Service)] > 0;
    bool const servicesUnfilled = record.buyFilled[resourceIndex(ResourceId::Service)] == 0;
    bool const goodsDemanded = record.buyDemand[resourceIndex(ResourceId::ConsumerGoods)] > 0;
    bool const goodsUnfilled = record.buyFilled[resourceIndex(ResourceId::ConsumerGoods)] == 0;
    if (servicesDemanded && servicesUnfilled && goodsDemanded && goodsUnfilled) {
        warnings.push_back(EconomyWarning::CONSUMER_MARKET_SHORTFALL);
    }
    return warnings;
}

EconomySummary computeEconomySummary(std::span<PlayerRecord const> const records) {
    EconomySummary summary;
    PlayerRecord const *const record = latestEconomyRecord(records);
    if (record == nullptr) {
        return summary;
    }
    summary.population = record->population;
    summary.foodCoverageTurns = coverageTurns(*record, ResourceId::Food);
    summary.unemployed = std::max<std::int64_t>(0, record->labourSupplied - record->labourEmployed);
    summary.corporationCounts = record->corporationCounts;
    for (std::size_t index = 0; index < KEY_RESOURCES.size(); ++index) {
        std::size_t const resource = resourceIndex(KEY_RESOURCES[index]);
        summary.keyPrices[index] = record->settlementPrices[resource];
        summary.keyPriceDirections[index] = priceDirection(record->priceChanges[resource]);
    }
    summary.warnings = deriveEconomyWarnings(*record);
    return summary;
}

std::array<std::vector<ChartSeries>, 6> buildEconomyChartSeries(std::span<PlayerRecord const> const records) {
    std::array<std::vector<ChartSeries>, 6> charts;
    for (ResourceId const resource : MARKET_RESOURCES) {
        ChartSeries series{
            std::string(Simulation::Economy::displayName(resource)), ChartUnit::CREDITS, {}, std::nullopt};
        for (PlayerRecord const &record : records) {
            series.points.push_back(
                {record.turnNumber, static_cast<double>(record.settlementPrices[resourceIndex(resource)]) / 100.0});
        }
        charts[0].push_back(std::move(series));
    }

    ChartSeries foodDemand{"Demand", ChartUnit::TONS, {}, std::nullopt};
    ChartSeries foodFilled{"Filled", ChartUnit::TONS, {}, std::nullopt};
    ChartSeries servicesDemand{"Services demand", ChartUnit::COUNT, {}, std::nullopt};
    ChartSeries servicesFilled{"Services filled", ChartUnit::COUNT, {}, std::nullopt};
    ChartSeries consumerDemand{"Consumer goods demand", ChartUnit::TONS, {}, std::nullopt};
    ChartSeries consumerFilled{"Consumer goods filled", ChartUnit::TONS, {}, std::nullopt};
    ChartSeries unemployment{"Unemployed", ChartUnit::PERSON_HOURS, {}, std::nullopt};
    ChartSeries householdCredits{"Households", ChartUnit::CREDITS, {}, std::nullopt};
    ChartSeries corporationCredits{"Corporations", ChartUnit::CREDITS, {}, std::nullopt};
    std::array<ChartSeries, Simulation::Economy::SECTOR_COUNT> corporations;
    for (std::size_t sector = 0; sector < corporations.size(); ++sector) {
        corporations[sector] = {sectorName(sector), ChartUnit::COUNT, {}, std::nullopt};
    }

    for (PlayerRecord const &record : records) {
        std::int64_t const turn = record.turnNumber;
        foodDemand.points.push_back({turn, quantityInTons(record.buyDemand[resourceIndex(ResourceId::Food)])});
        foodFilled.points.push_back({turn, quantityInTons(record.buyFilled[resourceIndex(ResourceId::Food)])});
        servicesDemand.points.push_back(
            {turn, static_cast<double>(record.buyDemand[resourceIndex(ResourceId::Service)])});
        servicesFilled.points.push_back(
            {turn, static_cast<double>(record.buyFilled[resourceIndex(ResourceId::Service)])});
        consumerDemand.points.push_back(
            {turn, quantityInTons(record.buyDemand[resourceIndex(ResourceId::ConsumerGoods)])});
        consumerFilled.points.push_back(
            {turn, quantityInTons(record.buyFilled[resourceIndex(ResourceId::ConsumerGoods)])});
        unemployment.points.push_back(
            {turn, static_cast<double>(std::max<std::int64_t>(0, record.labourSupplied - record.labourEmployed))});
        householdCredits.points.push_back({turn, static_cast<double>(record.householdCredits) / 100.0});
        corporationCredits.points.push_back({turn, static_cast<double>(record.corporationCredits) / 100.0});
        for (std::size_t sector = 0; sector < corporations.size(); ++sector) {
            corporations[sector].points.push_back({turn, static_cast<double>(record.corporationCounts[sector])});
        }
    }
    charts[1] = {std::move(foodDemand), std::move(foodFilled)};
    charts[2] = {
        std::move(servicesDemand), std::move(servicesFilled), std::move(consumerDemand), std::move(consumerFilled)};
    charts[3].push_back(std::move(unemployment));
    charts[4].assign(std::make_move_iterator(corporations.begin()), std::make_move_iterator(corporations.end()));
    charts[5] = {std::move(householdCredits), std::move(corporationCredits)};
    return charts;
}

std::optional<Simulation::Economy::ColonyId> colonyIdForBody(Server::GameSnapshot const &snapshot,
                                                             std::size_t const bodyIndex) noexcept {
    if (snapshot.economyRecords == nullptr || bodyIndex >= snapshot.bodies.size()) {
        return std::nullopt;
    }
    Server::BodySnapshot const &selected = snapshot.bodies[bodyIndex];
    if (selected.kind == Simulation::BodyKind::STAR) {
        return std::nullopt;
    }
    std::size_t colonyOrdinal{};
    for (std::size_t index = 0; index < bodyIndex; ++index) {
        if (snapshot.bodies[index].kind != Simulation::BodyKind::STAR) {
            ++colonyOrdinal;
        }
    }
    if (colonyOrdinal >= snapshot.economyRecords->size()) {
        return std::nullopt;
    }
    return (*snapshot.economyRecords)[colonyOrdinal].colonyId;
}

void EconomyUi::draw(std::string_view const colonyName,
                     std::optional<Simulation::Economy::ColonyId> const selectedColony,
                     Client const &client) {
    if (selectedColony.has_value()) {
        Simulation::Economy::ColonyId const colonyId = *selectedColony;
        mLastColonyId = colonyId;
        ColonyEconomyHistory const &history = client.economyHistory(colonyId);
        PlayerRecord const *const record = latestEconomyRecord(history.playerRecords);
        ImGuiIO const &io = ImGui::GetIO();
        float const windowY = std::max(WINDOW_PADDING, io.DisplaySize.y - INFO_WINDOW_HEIGHT - WINDOW_PADDING);
        ImGui::SetNextWindowPos({WINDOW_PADDING, windowY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({INFO_WINDOW_WIDTH, INFO_WINDOW_HEIGHT}, ImGuiCond_Always);
        ImGuiWindowFlags const infoFlags = static_cast<ImGuiWindowFlags>(
            static_cast<unsigned>(ImGuiWindowFlags_NoTitleBar) | static_cast<unsigned>(ImGuiWindowFlags_NoResize) |
            static_cast<unsigned>(ImGuiWindowFlags_NoMove));
        ImGui::Begin("##ColonyEconomyInfo", nullptr, infoFlags);
        ImGui::TextUnformatted(colonyName.data(), colonyName.data() + colonyName.size());
        if (record != nullptr) {
            EconomySummary const summary = computeEconomySummary(history.playerRecords);
            ImGui::Text("Pop %lld  Food %.1f turns  Unemployed %lld",
                        static_cast<long long>(summary.population),
                        summary.foodCoverageTurns.value_or(0.0),
                        static_cast<long long>(summary.unemployed));
            for (std::size_t index = 0; index < KEY_RESOURCES.size(); ++index) {
                if (index != 0) {
                    ImGui::SameLine();
                }
                ImGui::Text("%.*s %.2f%s",
                            static_cast<int>(Simulation::Economy::displayName(KEY_RESOURCES[index]).size()),
                            Simulation::Economy::displayName(KEY_RESOURCES[index]).data(),
                            static_cast<double>(summary.keyPrices[index]) / 100.0,
                            directionArrow(summary.keyPriceDirections[index]));
            }
            ImGui::Text("Companies: F %lld M %lld Fu %lld E %lld S %lld G %lld",
                        static_cast<long long>(summary.corporationCounts[0]),
                        static_cast<long long>(summary.corporationCounts[1]),
                        static_cast<long long>(summary.corporationCounts[2]),
                        static_cast<long long>(summary.corporationCounts[3]),
                        static_cast<long long>(summary.corporationCounts[4]),
                        static_cast<long long>(summary.corporationCounts[5]));
            for (EconomyWarning const warning : summary.warnings) {
                ImGui::SameLine();
                if (ImGui::SmallButton(warningLabel(warning).c_str())) {
                    mLastColonyId = colonyId;
                    mHighlightedWarning = warning;
                    mModalOpen = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Economy")) {
                mLastColonyId = colonyId;
                mHighlightedWarning.reset();
                mModalOpen = true;
            }
        } else {
            ImGui::TextUnformatted("Economy records available after the first turn.");
        }
        ImGui::End();
    }

    if (mModalOpen && mLastColonyId.has_value()) {
        ImGui::OpenPopup("Colony Economy");
        drawModal(client);
    }
}

void EconomyUi::drawModal(Client const &client) {
    if (!mLastColonyId.has_value()) {
        return;
    }
    Simulation::Economy::ColonyId const colonyId = *mLastColonyId;
    ColonyEconomyHistory const &history = client.economyHistory(colonyId);
    std::span<PlayerRecord const> records(history.playerRecords);
    bool keepOpen = mModalOpen;
    ImGuiIO const &io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5F, io.DisplaySize.y * 0.5F}, ImGuiCond_Appearing, {0.5F, 0.5F});
    ImGui::SetNextWindowSize({MODAL_WIDTH, MODAL_HEIGHT}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Colony Economy", &keepOpen, ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            keepOpen = false;
        }
        PlayerRecord const *record = findRecord(records, mScrubTurn);
        if (record != nullptr) {
            mScrubTurn = record->turnNumber;
            ImGui::Text("Colony %lld  |  Turn %lld",
                        static_cast<long long>(colonyId.value),
                        static_cast<long long>(record->turnNumber));
            ImGui::Separator();

            float const contentHeight = std::max(180.0F, ImGui::GetContentRegionAvail().y - 245.0F);
            if (ImGui::BeginChild("##EconomyDetails", {0.0F, contentHeight}, ImGuiChildFlags_Borders)) {
                float const statusWidth = std::min(340.0F, ImGui::GetContentRegionAvail().x * 0.38F);
                if (ImGui::BeginChild("##EconomyStatus", {statusWidth, 0.0F}, ImGuiChildFlags_Borders)) {
                    drawStatusColumn(*record, mHighlightedWarning);
                }
                ImGui::EndChild();
                ImGui::SameLine();
                if (ImGui::BeginChild("##EconomyCorporations", {0.0F, 0.0F}, ImGuiChildFlags_Borders)) {
                    drawCorporationTable(*record, mHighlightedWarning);
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();

            std::int64_t firstTurn = records.front().turnNumber;
            std::int64_t lastTurn = records.back().turnNumber;
            std::int64_t scrubTurn = record->turnNumber;
            ImGui::SetNextItemWidth(-1.0F);
            if (ImGui::SliderScalar("Turn", ImGuiDataType_S64, &scrubTurn, &firstTurn, &lastTurn, "%lld")) {
                mScrubTurn = scrubTurn;
            }

            std::array<std::vector<ChartSeries>, 6> const charts = buildEconomyChartSeries(records);
            float const chartWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0F) / 3.0F;
            for (std::size_t index = 0; index < mCharts.size(); ++index) {
                if (index % 3 != 0) {
                    ImGui::SameLine();
                }
                std::string const childId = "##EconomyChartGroup" + std::to_string(index);
                ImGui::BeginChild(childId.c_str(), {chartWidth, 220.0F}, ImGuiChildFlags_None);
                EconomyChartOptions options;
                options.title = CHART_TITLES[index];
                options.height = 190.0F;
                options.showLegend = true;
                options.enableScrubber = false;
                mCharts[index].draw("##ColonyEconomyChart" + std::to_string(index), charts[index], options);
                ImGui::EndChild();
            }
            mHighlightedWarning.reset();
        } else {
            ImGui::TextUnformatted("No player-facing economy history is available.");
        }
        if (ImGui::Button("Close")) {
            keepOpen = false;
        }
        ImGui::EndPopup();
    }
    mModalOpen = keepOpen;
    if (!mModalOpen) {
        mScrubTurn.reset();
        mHighlightedWarning.reset();
    }
}

bool EconomyUi::modalOpen() const noexcept {
    return mModalOpen;
}

} // namespace Client
