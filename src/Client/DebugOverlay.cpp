#include "DebugOverlay.h"

#include "Client.h"
#include "EconomyUi.h"
#include <Economy/EconomyConstants.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

namespace Client {
namespace {

    using namespace Simulation::Economy;
    using Fields = std::map<std::string, std::pair<std::string, std::string>>;

    [[nodiscard]] std::string text(std::int64_t value) {
        return std::to_string(value);
    }
    [[nodiscard]] std::string text(std::size_t value) {
        return std::to_string(value);
    }
    [[nodiscard]] std::string text(double value) {
        std::ostringstream stream;
        stream << std::setprecision(12) << value;
        return stream.str();
    }
    [[nodiscard]] std::string text(bool value) {
        return value ? "true" : "false";
    }
    [[nodiscard]] std::string text(std::string const &value);

    [[nodiscard]] std::string sectorText(SectorId sector) {
        switch (sector) {
        case SectorId::Food:
            return "Food";
        case SectorId::Materials:
            return "Materials";
        case SectorId::Fuel:
            return "Fuel";
        case SectorId::Energy:
            return "Energy";
        case SectorId::Service:
            return "Service";
        case SectorId::ConsumerGoods:
            return "Consumer goods";
        }
        return "Unknown";
    }
    [[nodiscard]] std::string tierText(BuildTier tier) {
        switch (tier) {
        case BuildTier::Manual:
            return "Manual";
        case BuildTier::Tier1:
            return "Tier 1";
        case BuildTier::Tier2:
            return "Tier 2";
        }
        return "Unknown";
    }
    [[nodiscard]] char const *priceChoiceText(SettlementPriceChoice choice) noexcept {
        switch (choice) {
        case SettlementPriceChoice::NO_TRADE:
            return "no trade";
        case SettlementPriceChoice::PRIOR_PRICE:
            return "prior price";
        case SettlementPriceChoice::LOWEST_MAXIMIZER:
            return "lowest maximizer";
        case SettlementPriceChoice::HIGHEST_MAXIMIZER:
            return "highest maximizer";
        }
        return "unknown";
    }
    [[nodiscard]] std::string ownerText(AgentRef const &owner) {
        if (auto const *household = std::get_if<HouseholdId>(&owner)) {
            return "H" + text(household->value);
        }
        return "C" + text(std::get<CorporationId>(owner).value);
    }

    template <typename T>
    void add(Fields &fields, std::string stage, std::string key, T const &value) {
        fields.emplace(std::move(key), std::pair{std::move(stage), text(value)});
    }

    void addState(Fields &fields, ColonyEconomy const &state, std::string const &side) {
        std::string const prefix = side + ".";
        add(fields, "Colony state", prefix + "turn", state.turnNumber);
        add(fields, "Colony state", prefix + "population", state.totalPopulation);
        add(fields, "Colony state", prefix + "household-count", state.households.size());
        add(fields, "Colony state", prefix + "corporation-count", state.corporations.size());
        for (Household const &h : state.households) {
            std::string const key = prefix + "household." + text(h.id.value) + ".";
            add(fields, "Household state", key + "population", h.population);
            add(fields, "Household state", key + "credits", h.credits);
            add(fields, "Household state", key + "prop-income", h.propIncome);
            add(fields, "Household state", key + "prop-savings", h.propSavings);
            add(fields, "Household state", key + "food-stock", h.foodStock);
            add(fields, "Household state", key + "last-income", h.lastIncome);
            add(fields, "Household state", key + "last-food-price", h.lastFoodPrice);
            const std::array<std::pair<std::string_view, PriceBelief>, 4> beliefs{
                {{"food", h.foodBelief},
                 {"labour", h.labourBelief},
                 {"service", h.serviceBelief},
                 {"consumer-goods", h.consumerGoodsBelief}}};
            for (auto const &[name, belief] : beliefs) {
                std::string beliefKey = key;
                beliefKey.append(name).append(".");
                add(fields, "Household state", beliefKey + "mean", belief.mean);
                add(fields, "Household state", beliefKey + "half-width", belief.halfWidth);
            }
            for (std::size_t i = 0; i < h.foodPriceHistory.size(); ++i) {
                add(fields, "Household state", key + "food-price-history." + text(i), h.foodPriceHistory[i]);
            }
        }
        for (Corporation const &c : state.corporations) {
            std::string const key = prefix + "corporation." + text(c.id.value) + ".";
            add(fields, "Corporation state", key + "sector", sectorText(c.sector));
            add(fields, "Corporation state", key + "credits", c.credits);
            add(fields, "Corporation state", key + "stock-unit-cost", c.stockUnitCost);
            add(fields, "Corporation state", key + "break-even-price", c.breakEvenPrice);
            add(fields, "Corporation state", key + "last-income", c.lastIncome);
            add(fields, "Corporation state", key + "last-produced", c.lastProducedQuantity);
            add(fields, "Corporation state", key + "last-settlement-price", c.lastSettlementPrice);
            add(fields, "Corporation state", key + "building-count", c.buildings.size());
            for (std::size_t ri = 0; ri < RESOURCE_COUNT; ++ri) {
                ResourceId const resource = static_cast<ResourceId>(ri);
                std::string const label(displayName(resource));
                std::string stockKey = key;
                stockKey.append("stock.").append(label);
                add(fields, "Corporation state", stockKey, c.stock[resource]);
                std::string beliefKey = key;
                beliefKey.append("belief.").append(label).append(".");
                add(fields, "Corporation state", beliefKey + "mean", c.priceBeliefs[ri].mean);
                add(fields, "Corporation state", beliefKey + "half-width", c.priceBeliefs[ri].halfWidth);
            }
            for (std::size_t i = 0; i < c.sectorDemand.steps.size(); ++i) {
                add(fields, "Planning state", key + "demand." + text(i) + ".price", c.sectorDemand.steps[i].first);
                add(fields, "Planning state", key + "demand." + text(i) + ".quantity", c.sectorDemand.steps[i].second);
            }
            for (std::size_t i = 0; i < c.buildings.size(); ++i) {
                Building const &b = c.buildings[i];
                std::string const building = key + "building." + text(i) + ".";
                add(fields, "Buildings", building + "sector", sectorText(b.sector));
                add(fields, "Buildings", building + "tier", tierText(b.tier));
                add(fields, "Buildings", building + "staffed-workers", b.staffedWorkers);
                add(fields, "Buildings", building + "under-construction", b.underConstruction);
                add(fields, "Buildings", building + "construction-value", b.constructionValue);
            }
        }
        for (auto const &[resource, book] : state.market.orderBooks) {
            auto addOrder = [&fields, &resource, &prefix](Order const &order) {
                std::string const key = prefix + "order." + text(order.id.value) + ".";
                add(fields, "Market orders", key + "resource", std::string(displayName(resource)));
                add(fields, "Market orders", key + "side", order.side == OrderSide::Buy ? "buy" : "sell");
                add(fields, "Market orders", key + "owner", ownerText(order.owner));
                add(fields, "Market orders", key + "price", order.price);
                add(fields, "Market orders", key + "quantity", order.quantity);
            };
            for (Order const &order : book.buyOrders)
                addOrder(order);
            for (Order const &order : book.sellOrders)
                addOrder(order);
        }
    }

    [[nodiscard]] Fields flatten(DebugRecord const &record) {
        Fields fields;
        add(fields, "Identity", "colony", record.colonyId.value);
        add(fields, "Identity", "turn", record.turnNumber);
        addState(fields, record.beforeState, "before");
        addState(fields, record.afterState, "after");
        add(fields, "Agent flows", "liquidation-credits", record.liquidationCredits);
        for (SettlementOutcome const &s : record.settlements) {
            std::string const key = "settlement." + std::string(displayName(s.resource)) + ".";
            add(fields, "Settlement", key + "price", s.price);
            add(fields, "Settlement", key + "traded-quantity", s.tradedQuantity);
            add(fields, "Settlement sweep", key + "prior-price", s.priorPrice);
            add(fields, "Settlement sweep", key + "maximizer-low", s.maximizingPriceLow);
            add(fields, "Settlement sweep", key + "maximizer-high", s.maximizingPriceHigh);
            add(fields, "Settlement sweep", key + "price-choice", static_cast<std::int64_t>(s.priceChoice));
            for (std::size_t index = 0; index < s.candidateEvaluations.size(); ++index) {
                MarketCandidateEvaluation const &candidate = s.candidateEvaluations[index];
                std::string const candidateKey = key + "candidate." + text(index) + ".";
                add(fields, "Settlement sweep", candidateKey + "price", candidate.price);
                add(fields, "Settlement sweep", candidateKey + "supply", candidate.supply);
                add(fields, "Settlement sweep", candidateKey + "demand", candidate.demand);
                add(fields, "Settlement sweep", candidateKey + "traded", candidate.tradedQuantity);
            }
            for (Trade const &trade : s.trades) {
                std::string const t = key + "trade." + text(trade.orderId.value) + ".";
                add(fields, "Settlement", t + "owner", ownerText(trade.owner));
                add(fields, "Settlement", t + "side", trade.side == OrderSide::Buy ? "buy" : "sell");
                add(fields, "Settlement", t + "quantity", trade.quantity);
                add(fields, "Settlement", t + "price", trade.price);
            }
        }
        for (HouseholdTickDelta const &d : record.householdDeltas) {
            std::string const k = "household-delta." + text(d.id.value) + ".";
            add(fields, "Agent flows", k + "credits", d.credits);
            add(fields, "Agent flows", k + "population", d.population);
            add(fields, "Agent flows", k + "food-stock", d.foodStock);
            add(fields, "Agent flows", k + "food-eaten", d.foodEaten);
            add(fields, "Agent flows", k + "services", d.servicesConsumed);
            add(fields, "Agent flows", k + "consumer-goods", d.consumerGoodsConsumed);
        }
        for (CorporationTickDelta const &d : record.corporationDeltas) {
            std::string const k = "corporation-delta." + text(d.id.value) + ".";
            add(fields, "Agent flows", k + "credits", d.credits);
            add(fields, "Agent flows", k + "produced", d.produced);
            add(fields, "Agent flows", k + "sold", d.sold);
        }
        for (HouseholdTickOutcome const &outcome : record.householdOutcomes) {
            std::string const key = "household-outcome." + text(outcome.id.value) + ".";
            add(fields, "Household outcomes", key + "budget", outcome.budget);
            add(fields, "Household outcomes", key + "survival-need", outcome.survivalNeed);
            add(fields, "Household outcomes", key + "reserve-target", outcome.reserveTarget);
            add(fields, "Household outcomes", key + "reserve-gap", outcome.reserveGap);
            add(fields, "Household outcomes", key + "emergency-requested", outcome.emergencyFoodRequested);
            add(fields, "Household outcomes", key + "emergency-filled", outcome.emergencyFoodFilled);
            add(fields, "Household outcomes", key + "food-bought", outcome.foodBought);
            add(fields, "Household outcomes", key + "food-share", outcome.foodShare);
            add(fields, "Household outcomes", key + "services-share", outcome.servicesShare);
            add(fields, "Household outcomes", key + "goods-share", outcome.consumerGoodsShare);
            add(fields, "Household outcomes", key + "consumption-target", outcome.consumptionTarget);
            add(fields, "Household outcomes", key + "food-spent", outcome.foodSpent);
            add(fields, "Household outcomes", key + "services-spent", outcome.servicesSpent);
            add(fields, "Household outcomes", key + "goods-spent", outcome.consumerGoodsSpent);
            add(fields, "Household outcomes", key + "food-eaten", outcome.foodEaten);
            add(fields, "Household outcomes", key + "reserve-turns", outcome.reserveTurns);
            add(fields, "Household outcomes", key + "emergency", outcome.emergencyMode);
        }
        for (CorporationTickOutcome const &outcome : record.corporationOutcomes) {
            std::string const key = "corporation-outcome." + text(outcome.id.value) + ".";
            add(fields, "Corporation outcomes", key + "materials-consumed", outcome.materialsConsumed);
            add(fields, "Corporation outcomes", key + "fuel-consumed", outcome.fuelConsumed);
            add(fields, "Corporation outcomes", key + "maintenance-paid", outcome.maintenancePaid);
            add(fields, "Corporation outcomes", key + "unpaid-buildings", outcome.unpaidMaintenanceBuildings);
            add(fields, "Planning", key + "target-price", outcome.planning.targetPrice);
            add(fields, "Planning", key + "target-quantity", outcome.planning.targetQuantity);
            add(fields, "Planning", key + "budget", outcome.planning.budget);
            add(fields, "Planning", key + "planned-output", outcome.planning.plannedOutput);
            add(fields, "Planning", key + "planned-cost", outcome.planning.plannedCost);
            add(fields, "Planning", key + "idle-capacity", outcome.planning.idleOutputCapacity);
            add(fields, "Planning", key + "break-even", outcome.planning.breakEvenPrice);
            add(fields, "Planning", key + "surplus", outcome.planning.surplusDecision);
            add(fields, "Planning", key + "construction-gate", outcome.planning.constructionGatePassed);
            add(fields, "Planning", key + "budget-clamp", outcome.planning.budgetClampFired);
            add(fields, "Planning", key + "demand-truncated", outcome.planning.demandStepsTruncated);
            add(fields, "Planning", key + "staffing-truncated", outcome.planning.staffingTruncated);
            for (std::size_t index = 0; index < outcome.planning.methods.size(); ++index) {
                CorporationMethodDiagnostic const &method = outcome.planning.methods[index];
                std::string const methodKey = key + "method." + text(index) + ".";
                add(fields, "Planning", methodKey + "tier", static_cast<std::int64_t>(method.tier));
                add(fields, "Planning", methodKey + "unit-cost", method.unitCost);
                add(fields, "Planning", methodKey + "capacity", method.outputCapacity);
            }
            for (std::size_t index = 0; index < outcome.planning.demandSteps.size(); ++index) {
                std::string const demandKey = key + "demand." + text(index) + ".";
                add(fields, "Planning", demandKey + "price", outcome.planning.demandSteps[index].first);
                add(fields, "Planning", demandKey + "quantity", outcome.planning.demandSteps[index].second);
            }
            for (BuildingStaffing const &staffing : outcome.planning.staffing) {
                add(fields, "Planning", key + "staffing." + text(staffing.buildingIndex), staffing.workers);
            }
        }
        for (EconomyInvariantResult const &invariant : record.invariantResults) {
            add(fields, "Invariants", "invariant." + invariant.id, invariant.passed);
        }
        for (std::size_t index = 0; index < RESOURCE_COUNT; ++index) {
            add(fields, "Liquidation", "liquidated-stock." + std::to_string(index), record.liquidatedResources[index]);
        }
        for (FoundedCorporation const &f : record.founded) {
            std::string const k = "founded." + text(f.householdId.value) + ".";
            add(fields, "Founding and dissolution", k + "sector", sectorText(f.sector));
            add(fields, "Founding and dissolution", k + "capital-transfer", f.capitalTransfer);
        }
        for (std::size_t i = 0; i < RESOURCE_COUNT; ++i) {
            ResourceId const resource = static_cast<ResourceId>(i);
            std::string const key = "founding-market." + std::string(displayName(resource)) + ".";
            add(fields, "Founding and dissolution", key + "demand", record.marketData.demand[i]);
            add(fields, "Founding and dissolution", key + "filled", record.marketData.filled[i]);
            add(fields, "Founding and dissolution", key + "price", record.marketData.settlementPrices[i]);
        }
        for (DissolvedCorporation const &d : record.dissolvedCorporations) {
            std::string const k = "dissolved-corporation." + text(d.id.value) + ".";
            add(fields, "Founding and dissolution", k + "owner", d.owner.value);
            add(fields, "Founding and dissolution", k + "proceeds", d.proceeds);
        }
        for (HouseholdId const id : record.dissolvedHouseholds) {
            add(fields, "Founding and dissolution", "dissolved-household." + text(id.value), true);
        }
        return fields;
    }

    [[nodiscard]] std::string text(std::string const &value) {
        return value;
    }
    [[nodiscard]] std::optional<std::int64_t> checkedAdd(std::int64_t lhs, std::int64_t rhs) noexcept {
        if ((rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() - rhs) ||
            (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::min() - rhs))
            return std::nullopt;
        return lhs + rhs;
    }

    [[nodiscard]] std::optional<std::int64_t> checkedSubtract(std::int64_t lhs, std::int64_t rhs) noexcept {
        if ((rhs > 0 && lhs < std::numeric_limits<std::int64_t>::min() + rhs) ||
            (rhs < 0 && lhs > std::numeric_limits<std::int64_t>::max() + rhs))
            return std::nullopt;
        return lhs - rhs;
    }

    [[nodiscard]] std::optional<std::int64_t> credits(ColonyEconomy const &state) noexcept {
        std::int64_t sum{};
        auto addBalance = [&sum](std::int64_t value) {
            auto const next = checkedAdd(sum, value);
            if (!next)
                return false;
            sum = *next;
            return true;
        };
        for (Household const &h : state.households)
            if (!addBalance(h.credits))
                return std::nullopt;
        for (Corporation const &c : state.corporations)
            if (!addBalance(c.credits))
                return std::nullopt;
        return sum;
    }

    [[nodiscard]] std::optional<std::int64_t> stock(ColonyEconomy const &state, ResourceId resource) noexcept {
        std::int64_t sum{};
        auto addStock = [&sum](std::int64_t value) {
            auto const next = checkedAdd(sum, value);
            if (!next)
                return false;
            sum = *next;
            return true;
        };
        if (resource == ResourceId::Food) {
            for (Household const &h : state.households)
                if (!addStock(h.foodStock))
                    return std::nullopt;
        }
        for (Corporation const &c : state.corporations)
            if (!addStock(c.stock[resource]))
                return std::nullopt;
        return sum;
    }

    [[nodiscard]] std::optional<std::int64_t> production(DebugRecord const &record, ResourceId resource) noexcept {
        std::int64_t sum{};
        for (CorporationTickDelta const &delta : record.corporationDeltas) {
            std::optional<SectorId> sector;
            auto locate = [&delta](ColonyEconomy const &state) -> std::optional<SectorId> {
                auto const found = std::find_if(state.corporations.begin(),
                                                state.corporations.end(),
                                                [&delta](Corporation const &c) { return c.id == delta.id; });
                return found == state.corporations.end() ? std::nullopt : std::optional<SectorId>(found->sector);
            };
            sector = locate(record.beforeState);
            if (!sector)
                sector = locate(record.afterState);
            if (!sector || outputResource(*sector) != resource)
                continue;
            auto const next = checkedAdd(sum, delta.produced);
            if (!next)
                return std::nullopt;
            sum = *next;
        }
        return sum;
    }

    [[nodiscard]] std::optional<std::int64_t> consumed(DebugRecord const &record, ResourceId resource) noexcept {
        std::int64_t sum{};
        for (HouseholdTickDelta const &delta : record.householdDeltas) {
            std::int64_t const amount = resource == ResourceId::Food ? delta.foodEaten : delta.consumerGoodsConsumed;
            auto const next = checkedAdd(sum, amount);
            if (!next)
                return std::nullopt;
            sum = *next;
        }
        return sum;
    }

    [[nodiscard]] InvariantBadge resourceCheck(DebugRecord const &record) {
        std::string const name = "Standard resources conserved";
        if (!record.dissolvedCorporations.empty() || !record.dissolvedHouseholds.empty()) {
            return {
                name, InvariantState::UNAVAILABLE, "Dissolution transfers/liquidations are not retained per resource."};
        }
        for (ResourceId const resource : {ResourceId::Food, ResourceId::ConsumerGoods}) {
            auto const start = stock(record.beforeState, resource);
            auto const output = production(record, resource);
            auto const use = consumed(record, resource);
            auto const finish = stock(record.afterState, resource);
            if (!start || !output || !use || !finish) {
                return {name, InvariantState::UNAVAILABLE, "Recorded stock/flow values are incomplete or overflowed."};
            }
            auto const producedTotal = checkedAdd(*start, *output);
            auto const expected = producedTotal ? checkedSubtract(*producedTotal, *use) : std::nullopt;
            if (!expected)
                return {name, InvariantState::UNAVAILABLE, "Stock/flow arithmetic overflowed."};
            if (*expected != *finish) {
                return {name,
                        InvariantState::FAIL,
                        std::string(displayName(resource)) +
                            " stock does not reconcile with production and consumption."};
            }
        }
        return {name,
                InvariantState::UNAVAILABLE,
                "Food and consumer goods reconcile; materials/fuel consumption flows are not retained."};
    }

    [[nodiscard]] char const *statusText(InvariantState state) noexcept {
        switch (state) {
        case InvariantState::PASS:
            return "PASS";
        case InvariantState::FAIL:
            return "FAIL";
        case InvariantState::UNAVAILABLE:
            return "N/A";
        }
        return "N/A";
    }

    template <typename... Flags>
    [[nodiscard]] constexpr unsigned combinedFlags(Flags... flags) noexcept {
        unsigned result{};
        ((result |= static_cast<unsigned>(flags)), ...);
        return result;
    }

    void drawInvariants(DebugRecord const &record, bool &open) {
        ImGui::Begin(debugWindowTitle(DebugWindowId::INVARIANTS), &open);
        for (InvariantBadge const &badge : deriveInvariantBadges(record)) {
            ImGui::Text("[%s] %s - colony %lld, turn %lld: %s",
                        statusText(badge.state),
                        badge.name.c_str(),
                        static_cast<long long>(record.colonyId.value),
                        static_cast<long long>(record.turnNumber),
                        badge.detail.c_str());
        }
        ImGui::End();
    }

    void drawColony(DebugRecord const &record, bool &openDiff, ResourceId &selectedResource, bool &open) {
        ImGui::Begin(debugWindowTitle(DebugWindowId::COLONY), &open);
        ColonyEconomy const &state = record.afterState;
        ImGui::Text("Colony %lld - turn %lld | population %lld | households %zu | corporations %zu",
                    static_cast<long long>(record.colonyId.value),
                    static_cast<long long>(record.turnNumber),
                    static_cast<long long>(state.totalPopulation),
                    state.households.size(),
                    state.corporations.size());
        if (ImGui::BeginTable("household-ledger",
                              8,
                              static_cast<ImGuiTableFlags>(combinedFlags(
                                  ImGuiTableFlags_Borders, ImGuiTableFlags_RowBg, ImGuiTableFlags_ScrollX)))) {
            for (char const *label : {"ID",
                                      "Population",
                                      "Credits",
                                      "Income prop.",
                                      "Savings prop.",
                                      "Food stock",
                                      "Food eaten",
                                      "Food belief"})
                ImGui::TableSetupColumn(label);
            ImGui::TableHeadersRow();
            for (Household const &h : state.households) {
                auto const d = std::find_if(record.householdDeltas.begin(),
                                            record.householdDeltas.end(),
                                            [&h](HouseholdTickDelta const &item) { return item.id == h.id; });
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(h.id.value));
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(h.population));
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(h.credits));
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", h.propIncome);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", h.propSavings);
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(h.foodStock));
                ImGui::TableNextColumn();
                ImGui::Text("%s", d == record.householdDeltas.end() ? "-" : text(d->foodEaten).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.2f +/- %.2f", h.foodBelief.mean, h.foodBelief.halfWidth);
            }
            ImGui::EndTable();
        }
        for (Household const &h : state.households) {
            std::string const label = "Household " + text(h.id.value) + " details";
            if (ImGui::TreeNode(label.c_str())) {
                auto const outcome = std::find_if(record.householdOutcomes.begin(),
                                                  record.householdOutcomes.end(),
                                                  [&h](HouseholdTickOutcome const &item) { return item.id == h.id; });
                if (outcome != record.householdOutcomes.end()) {
                    ImGui::Text("Budget %lld | survival need %lld | target %lld | reserve target %lld (gap %lld) | "
                                "reserve %.3f turns | emergency: %s",
                                static_cast<long long>(outcome->budget),
                                static_cast<long long>(outcome->survivalNeed),
                                static_cast<long long>(outcome->consumptionTarget),
                                static_cast<long long>(outcome->reserveTarget),
                                static_cast<long long>(outcome->reserveGap),
                                outcome->reserveTurns,
                                outcome->emergencyMode ? "yes" : "no");
                    ImGui::Text("Shares: food %lld | services %lld | goods %lld | emergency food %lld requested / "
                                "%lld filled (%lld total food bought)",
                                static_cast<long long>(outcome->foodShare),
                                static_cast<long long>(outcome->servicesShare),
                                static_cast<long long>(outcome->consumerGoodsShare),
                                static_cast<long long>(outcome->emergencyFoodRequested),
                                static_cast<long long>(outcome->emergencyFoodFilled),
                                static_cast<long long>(outcome->foodBought));
                    ImGui::Text("Spent: food %lld | services %lld | consumer goods %lld",
                                static_cast<long long>(outcome->foodSpent),
                                static_cast<long long>(outcome->servicesSpent),
                                static_cast<long long>(outcome->consumerGoodsSpent));
                }
                ImGui::Text(
                    "Beliefs: food %.2f +/- %.2f; labour %.2f +/- %.2f; service %.2f +/- %.2f; goods %.2f +/- %.2f",
                    h.foodBelief.mean,
                    h.foodBelief.halfWidth,
                    h.labourBelief.mean,
                    h.labourBelief.halfWidth,
                    h.serviceBelief.mean,
                    h.serviceBelief.halfWidth,
                    h.consumerGoodsBelief.mean,
                    h.consumerGoodsBelief.halfWidth);
                ImGui::Text("Last income %lld | last food price %lld | food price history:",
                            static_cast<long long>(h.lastIncome),
                            static_cast<long long>(h.lastFoodPrice));
                for (std::size_t i = 0; i < h.foodPriceHistory.size(); ++i) {
                    ImGui::SameLine();
                    ImGui::Text("%s%lld", i == 0 ? "" : ", ", static_cast<long long>(h.foodPriceHistory[i]));
                }
                auto const delta = std::find_if(record.householdDeltas.begin(),
                                                record.householdDeltas.end(),
                                                [&h](HouseholdTickDelta const &item) { return item.id == h.id; });
                if (delta != record.householdDeltas.end()) {
                    ImGui::Text("Turn deltas: credits %lld, population %lld, stock %lld, food eaten %lld, services "
                                "%lld, goods %lld",
                                static_cast<long long>(delta->credits),
                                static_cast<long long>(delta->population),
                                static_cast<long long>(delta->foodStock),
                                static_cast<long long>(delta->foodEaten),
                                static_cast<long long>(delta->servicesConsumed),
                                static_cast<long long>(delta->consumerGoodsConsumed));
                }
                ImGui::TreePop();
            }
        }
        if (ImGui::BeginTable("corporation-ledger",
                              7,
                              static_cast<ImGuiTableFlags>(combinedFlags(
                                  ImGuiTableFlags_Borders, ImGuiTableFlags_RowBg, ImGuiTableFlags_ScrollX)))) {
            for (char const *label : {"ID", "Sector", "Credits", "Output", "Sold", "Stock cost", "Break-even"})
                ImGui::TableSetupColumn(label);
            ImGui::TableHeadersRow();
            for (Corporation const &c : state.corporations) {
                auto const d = std::find_if(record.corporationDeltas.begin(),
                                            record.corporationDeltas.end(),
                                            [&c](CorporationTickDelta const &item) { return item.id == c.id; });
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(c.id.value));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(sectorText(c.sector).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(c.credits));
                ImGui::TableNextColumn();
                ImGui::Text("%s", d == record.corporationDeltas.end() ? "-" : text(d->produced).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", d == record.corporationDeltas.end() ? "-" : text(d->sold).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", c.stockUnitCost);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", c.breakEvenPrice);
            }
            ImGui::EndTable();
        }
        for (Corporation const &c : state.corporations) {
            if (ImGui::TreeNode(("Corporation " + text(c.id.value) + " inventory").c_str())) {
                ImGui::Text("Owner household %lld | last settlement %lld | first building %s | jump-start used %s",
                            static_cast<long long>(c.owner.value),
                            static_cast<long long>(c.lastSettlementPrice),
                            c.hasHadFirstBuilding ? "yes" : "no",
                            c.jumpStartUsed ? "yes" : "no");
                std::string const corporationId = text(c.id.value);
                ImGui::PushID(corporationId.c_str());
                for (std::size_t i = 0; i < RESOURCE_COUNT; ++i) {
                    ResourceId const r = static_cast<ResourceId>(i);
                    std::string const stockLabel = std::string(displayName(r)) + " stock " + text(c.stock[r]);
                    if (ImGui::Selectable(stockLabel.c_str()))
                        selectedResource = r;
                    ImGui::SameLine();
                    ImGui::Text("belief %.2f +/- %.2f", c.priceBeliefs[i].mean, c.priceBeliefs[i].halfWidth);
                }
                ImGui::PopID();
                for (std::size_t i = 0; i < c.buildings.size(); ++i) {
                    Building const &b = c.buildings[i];
                    ImGui::Text("Building %zu: %s %s, staffed %lld, construction %s",
                                i,
                                sectorText(b.sector).c_str(),
                                tierText(b.tier).c_str(),
                                static_cast<long long>(b.staffedWorkers),
                                b.underConstruction ? "yes" : "no");
                    ImGui::SameLine();
                    ImGui::Text("construction value %lld", static_cast<long long>(b.constructionValue));
                }
                ImGui::TreePop();
            }
        }
        for (CorporationTickOutcome const &outcome : record.corporationOutcomes) {
            ImGui::Text("Corp %lld materials consumed %lld | fuel consumed %lld | maintenance %s (%lld unpaid)",
                        static_cast<long long>(outcome.id.value),
                        static_cast<long long>(outcome.materialsConsumed),
                        static_cast<long long>(outcome.fuelConsumed),
                        outcome.maintenancePaid ? "paid" : "unpaid",
                        static_cast<long long>(outcome.unpaidMaintenanceBuildings));
        }
        ImGui::Separator();
        if (ImGui::Button("Compare recorded turns"))
            openDiff = true;
        ImGui::SameLine();
        ImGui::TextUnformatted("Record-level comparison only; the colony tick is not re-run.");
        ImGui::End();
    }

    void drawMarket(DebugRecord const &record, ResourceId &resource, bool &open) {
        ImGui::Begin(debugWindowTitle(DebugWindowId::MARKET), &open);
        char const *const names[] = {
            "Credits", "Labour", "Energy", "Food", "Materials", "Fuel", "Service", "Consumer goods"};
        int selected = static_cast<int>(resource);
        if (ImGui::Combo("Resource", &selected, names, static_cast<int>(RESOURCE_COUNT)))
            resource = static_cast<ResourceId>(selected);
        OrderBook const *const book = record.afterState.market.findOrderBook(resource);
        if (book == nullptr || (book->buyOrders.empty() && book->sellOrders.empty())) {
            ImGui::TextUnformatted("No retained order book for this resource.");
        } else if (ImGui::BeginTable(
                       "order-book",
                       5,
                       static_cast<ImGuiTableFlags>(combinedFlags(ImGuiTableFlags_Borders, ImGuiTableFlags_RowBg)))) {
            for (char const *label : {"Side", "Order", "Owner", "Price", "Quantity"})
                ImGui::TableSetupColumn(label);
            ImGui::TableHeadersRow();
            auto show = [](Order const &order) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(order.side == OrderSide::Buy ? "Buy" : "Sell");
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(order.id.value));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ownerText(order.owner).c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(order.price));
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(order.quantity));
            };
            for (Order const &order : book->buyOrders)
                show(order);
            for (Order const &order : book->sellOrders)
                show(order);
            ImGui::EndTable();
        }
        auto const settlement =
            std::find_if(record.settlements.begin(), record.settlements.end(), [&resource](SettlementOutcome const &s) {
                return s.resource == resource;
            });
        if (settlement == record.settlements.end()) {
            ImGui::TextUnformatted("No settlement snapshot for this resource.");
        } else {
            ImGui::Text("Settlement price %lld | traded %lld | positive fills %zu",
                        static_cast<long long>(settlement->price),
                        static_cast<long long>(settlement->tradedQuantity),
                        settlement->trades.size());
            if (ImGui::BeginTable(
                    "settlement-fills",
                    5,
                    static_cast<ImGuiTableFlags>(combinedFlags(ImGuiTableFlags_Borders, ImGuiTableFlags_RowBg)))) {
                for (char const *label : {"Order", "Owner", "Side", "Filled quantity", "Price"})
                    ImGui::TableSetupColumn(label);
                ImGui::TableHeadersRow();
                for (Trade const &trade : settlement->trades) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%lld", static_cast<long long>(trade.orderId.value));
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(ownerText(trade.owner).c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(trade.side == OrderSide::Buy ? "Buy" : "Sell");
                    ImGui::TableNextColumn();
                    ImGui::Text("%lld", static_cast<long long>(trade.quantity));
                    ImGui::TableNextColumn();
                    ImGui::Text("%lld", static_cast<long long>(trade.price));
                }
                ImGui::EndTable();
            }
        }
        if (settlement != record.settlements.end()) {
            ImGui::Text("Maximizer interval [%lld, %lld] | prior %lld | choice %s",
                        static_cast<long long>(settlement->maximizingPriceLow),
                        static_cast<long long>(settlement->maximizingPriceHigh),
                        static_cast<long long>(settlement->priorPrice),
                        priceChoiceText(settlement->priceChoice));
            if (ImGui::BeginTable(
                    "market-candidate-sweep",
                    4,
                    static_cast<ImGuiTableFlags>(combinedFlags(ImGuiTableFlags_Borders, ImGuiTableFlags_RowBg)))) {
                for (char const *label : {"Candidate price", "Qs(p)", "Qd(p)", "Q(p)"})
                    ImGui::TableSetupColumn(label);
                ImGui::TableHeadersRow();
                for (MarketCandidateEvaluation const &candidate : settlement->candidateEvaluations) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%lld", static_cast<long long>(candidate.price));
                    ImGui::TableNextColumn();
                    ImGui::Text("%lld", static_cast<long long>(candidate.supply));
                    ImGui::TableNextColumn();
                    ImGui::Text("%lld", static_cast<long long>(candidate.demand));
                    ImGui::TableNextColumn();
                    ImGui::Text("%lld", static_cast<long long>(candidate.tradedQuantity));
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    void drawPlanning(DebugRecord const &record, std::optional<CorporationId> &selectedId, bool &open) {
        ImGui::Begin(debugWindowTitle(DebugWindowId::PLANNING), &open);
        auto const &corporations = record.afterState.corporations;
        if (corporations.empty()) {
            ImGui::TextUnformatted("No corporation state is retained.");
            ImGui::End();
            return;
        }
        auto selected = std::find_if(corporations.begin(), corporations.end(), [&selectedId](Corporation const &c) {
            return selectedId == c.id;
        });
        if (selected == corporations.end()) {
            selected = corporations.begin();
            selectedId = selected->id;
        }
        std::vector<std::string> labels;
        labels.reserve(corporations.size());
        std::vector<char const *> pointers;
        pointers.reserve(corporations.size());
        for (Corporation const &c : corporations) {
            std::string label = "Corp ";
            label.append(text(c.id.value)).append(" - ").append(sectorText(c.sector));
            labels.push_back(std::move(label));
        }
        for (std::string const &label : labels)
            pointers.push_back(label.c_str());
        int index = static_cast<int>(std::distance(corporations.begin(), selected));
        if (ImGui::Combo("Corporation", &index, pointers.data(), static_cast<int>(pointers.size()))) {
            selectedId = corporations[static_cast<std::size_t>(index)].id;
            selected = corporations.begin() + index;
        }
        Corporation const &corporation = *selected;
        auto const trace = std::find_if(
            record.corporationOutcomes.begin(),
            record.corporationOutcomes.end(),
            [&corporation](CorporationTickOutcome const &outcome) { return outcome.id == corporation.id; });
        ImGui::Text("Recorded demand steps %zu | stock unit cost %.3f | break-even %.3f",
                    trace == record.corporationOutcomes.end() ? corporation.sectorDemand.steps.size()
                                                              : trace->planning.demandSteps.size(),
                    corporation.stockUnitCost,
                    corporation.breakEvenPrice);
        ImGui::Text("Last output %lld | last income %lld | buildings %zu",
                    static_cast<long long>(corporation.lastProducedQuantity),
                    static_cast<long long>(corporation.lastIncome),
                    corporation.buildings.size());
        if (ImGui::BeginTable(
                "demand-curve",
                2,
                static_cast<ImGuiTableFlags>(combinedFlags(ImGuiTableFlags_Borders, ImGuiTableFlags_RowBg)))) {
            ImGui::TableSetupColumn("Demand price");
            ImGui::TableSetupColumn("Quantity");
            ImGui::TableHeadersRow();
            std::vector<std::pair<std::int64_t, std::int64_t>> const &demandSteps =
                trace == record.corporationOutcomes.end() ? corporation.sectorDemand.steps
                                                          : trace->planning.demandSteps;
            for (auto const &[price, quantity] : demandSteps) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(price));
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(quantity));
            }
            ImGui::EndTable();
        }
        if (trace != record.corporationOutcomes.end()) {
            CorporationPlanDiagnostic const &planning = trace->planning;
            ImGui::Text("Target p* %lld / Q* %lld | profit %.3f | break-even %.3f",
                        static_cast<long long>(planning.targetPrice),
                        static_cast<long long>(planning.targetQuantity),
                        planning.targetProfit,
                        planning.breakEvenPrice);
            ImGui::Text("Budget %lld | planned output %lld | idle capacity %lld | planned cost %lld",
                        static_cast<long long>(planning.budget),
                        static_cast<long long>(planning.plannedOutput),
                        static_cast<long long>(planning.idleOutputCapacity),
                        static_cast<long long>(planning.plannedCost));
            ImGui::Text("Maintenance labour %lld | materials %lld | surplus %s | construction gate %s | budget "
                        "clamp %s",
                        static_cast<long long>(planning.maintenanceLabour),
                        static_cast<long long>(planning.maintenanceMaterials),
                        planning.surplusDecision ? "yes" : "no",
                        planning.constructionGatePassed ? "passed" : "not passed",
                        planning.budgetClampFired ? "fired" : "not fired");
            if (ImGui::BeginTable(
                    "planning-method-costs",
                    3,
                    static_cast<ImGuiTableFlags>(combinedFlags(ImGuiTableFlags_Borders, ImGuiTableFlags_RowBg)))) {
                for (char const *label : {"Method", "Unit cost", "Output capacity"})
                    ImGui::TableSetupColumn(label);
                ImGui::TableHeadersRow();
                for (CorporationMethodDiagnostic const &method : planning.methods) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(tierText(method.tier).c_str());
                    ImGui::TableNextColumn();
                    ImGui::Text("%.3f", method.unitCost);
                    ImGui::TableNextColumn();
                    ImGui::Text("%lld", static_cast<long long>(method.outputCapacity));
                }
                ImGui::EndTable();
            }
            for (BuildingStaffing const &staffing : planning.staffing) {
                ImGui::Text("Building %zu staffing decision: %lld workers",
                            staffing.buildingIndex,
                            static_cast<long long>(staffing.workers));
            }
            if (planning.demandStepsTruncated || planning.staffingTruncated) {
                ImGui::Text("Trace bounded: demand steps %s | staffing entries %s",
                            planning.demandStepsTruncated ? "truncated" : "complete",
                            planning.staffingTruncated ? "truncated" : "complete");
            }
        }
        ImGui::TextUnformatted("Saved planning outputs; the planner is never re-run by this reader.");
        ImGui::End();
    }

    void drawDiff(std::deque<DebugRecord> const &records, DebugRecord const &selected, bool &open) {
        ImGui::Begin(debugWindowTitle(DebugWindowId::DIFF), &open);
        if (records.size() < 2) {
            ImGui::TextUnformatted("At least two retained turns are needed for a record diff.");
            ImGui::End();
            return;
        }
        DebugRecord const *other =
            selected.turnNumber == records.back().turnNumber ? &records[records.size() - 2] : &records.back();
        std::vector<DebugRecordChange> const changes = diffDebugRecords({selected, *other});
        if (ImGui::Button("Close diff"))
            open = false;
        ImGui::Text("Turn %lld -> turn %lld: %zu changed fields",
                    static_cast<long long>(selected.turnNumber),
                    static_cast<long long>(other->turnNumber),
                    changes.size());
        if (ImGui::BeginTable("diff-fields",
                              4,
                              static_cast<ImGuiTableFlags>(combinedFlags(
                                  ImGuiTableFlags_Borders, ImGuiTableFlags_RowBg, ImGuiTableFlags_ScrollY)))) {
            ImGui::TableSetupColumn("Pipeline stage");
            ImGui::TableSetupColumn("Field");
            ImGui::TableSetupColumn("Selected record");
            ImGui::TableSetupColumn("Comparison record");
            ImGui::TableHeadersRow();
            for (DebugRecordChange const &change : changes) {
                ImGui::TableNextRow();
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, static_cast<ImU32>(0x64694E69U));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(change.stage.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(change.field.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(change.beforeValue.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(change.afterValue.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }

} // namespace

char const *debugWindowTitle(DebugWindowId window) noexcept {
    switch (window) {
    case DebugWindowId::COLONY:
        return "Colony inspector";
    case DebugWindowId::MARKET:
        return "Market inspector";
    case DebugWindowId::PLANNING:
        return "Planning inspector";
    case DebugWindowId::INVARIANTS:
        return "Economy invariants";
    case DebugWindowId::DIFF:
        return "Replay / record diff";
    case DebugWindowId::MAP_SELECTION:
        return "Map selection";
    case DebugWindowId::COUNT:
        break;
    }
    return "Debug window";
}

std::int64_t clampDebugTurn(std::int64_t current, DebugTurnRange range) noexcept {
    if (range.firstTurn > range.lastTurn)
        return current;
    return std::clamp(current, range.firstTurn, range.lastTurn);
}

std::int64_t stepDebugTurn(std::int64_t current, DebugTurnRange range, int direction, bool fast) noexcept {
    if (range.firstTurn > range.lastTurn || direction == 0)
        return current;
    std::int64_t const position = clampDebugTurn(current, range);
    std::int64_t const amount = fast ? 10 : 1;
    if (direction < 0)
        return position - range.firstTurn < amount ? range.firstTurn : position - amount;
    return range.lastTurn - position < amount ? range.lastTurn : position + amount;
}

Simulation::Economy::DebugRecord const *debugRecordAtTurn(DebugRecordQuery query) noexcept {
    auto const found = std::find_if(query.records.begin(), query.records.end(), [&query](DebugRecord const &record) {
        return record.turnNumber == query.turn;
    });
    return found == query.records.end() ? nullptr : &*found;
}

std::vector<InvariantBadge> deriveInvariantBadges(DebugRecord const &record) {
    std::vector<InvariantBadge> badges;
    if (!record.invariantResults.empty()) {
        for (EconomyInvariantResult const &result : record.invariantResults) {
            std::string name = result.id;
            if (result.id == "credits-conserved")
                name = "Credits conserved";
            else if (result.id == "agent-balances-nonnegative")
                name = "No agent overspent";
            else if (result.id == "settlement-prices-in-range")
                name = "Prices in range";
            else if (result.id == "households-within-credit-balance")
                name = "Households within budget";
            else if (result.id == "standard-resources-conserved")
                name = "Standard resources conserved";
            else if (result.id == "standard-resource-stocks-nonnegative")
                name = "Standard-resource stocks non-negative";
            badges.push_back({std::move(name),
                              result.passed ? InvariantState::PASS : InvariantState::FAIL,
                              result.passed ? "Explicit tick evaluation passed." : "Explicit tick evaluation failed."});
        }
        return badges;
    }
    auto const beforeCredits = credits(record.beforeState);
    auto const afterCredits = credits(record.afterState);
    auto const expectedCredits = beforeCredits ? checkedAdd(*beforeCredits, record.liquidationCredits) : std::nullopt;
    if (!expectedCredits || !afterCredits) {
        badges.push_back({"Credits conserved", InvariantState::UNAVAILABLE, "Credit summation overflowed."});
    } else {
        bool const matches = *expectedCredits == *afterCredits;
        badges.push_back({"Credits conserved",
                          matches ? InvariantState::PASS : InvariantState::FAIL,
                          matches ? "Post-tick balances equal prior balances plus recorded liquidation credits."
                                  : "Aggregate credits do not reconcile with recorded liquidation."});
    }
    badges.push_back(resourceCheck(record));
    bool balancesValid = true;
    for (Household const &h : record.afterState.households)
        balancesValid = balancesValid && h.credits >= 0;
    for (Corporation const &c : record.afterState.corporations)
        balancesValid = balancesValid && c.credits >= 0;
    badges.push_back(
        {"No agent overspent",
         balancesValid ? InvariantState::PASS : InvariantState::FAIL,
         balancesValid ? "All retained post-tick balances are non-negative." : "A retained balance is negative."});
    if (record.settlements.empty()) {
        badges.push_back({"Prices in range", InvariantState::UNAVAILABLE, "No settlements were retained."});
    } else {
        auto const bad =
            std::find_if(record.settlements.begin(), record.settlements.end(), [](SettlementOutcome const &s) {
                return s.price < PRICE_FLOOR || s.price > PRICE_CAP;
            });
        bool const valid = bad == record.settlements.end();
        badges.push_back({"Prices in range",
                          valid ? InvariantState::PASS : InvariantState::FAIL,
                          valid ? "All retained settlement prices are in range."
                                : std::string(displayName(bad->resource)) + " price is outside configured bounds."});
    }
    return badges;
}

std::vector<DebugRecordChange> diffDebugRecords(DebugRecordPair records) {
    Fields const leftFields = flatten(records.selected);
    Fields const rightFields = flatten(records.comparison);
    std::vector<DebugRecordChange> changes;
    changes.reserve(leftFields.size() + rightFields.size());
    auto left = leftFields.begin();
    auto right = rightFields.begin();
    while (left != leftFields.end() || right != rightFields.end()) {
        if (right == rightFields.end() || (left != leftFields.end() && left->first < right->first)) {
            changes.push_back({left->second.first, left->first, left->second.second, "<absent>"});
            ++left;
        } else if (left == leftFields.end() || right->first < left->first) {
            changes.push_back({right->second.first, right->first, "<absent>", right->second.second});
            ++right;
        } else {
            if (left->second.second != right->second.second) {
                changes.push_back({left->second.first, left->first, left->second.second, right->second.second});
            }
            ++left;
            ++right;
        }
    }
    std::stable_sort(changes.begin(), changes.end(), [](DebugRecordChange const &left, DebugRecordChange const &right) {
        if (left.stage != right.stage)
            return left.stage < right.stage;
        return left.field < right.field;
    });
    return changes;
}

void DebugOverlay::draw(Client const &client,
                        Server::GameSnapshot const &snapshot,
                        std::optional<std::size_t> selectedBody) {
    if (ImGui::IsKeyPressed(ImGuiKey_F9, false)) {
        if (!mVisible) {
            mVisible = true;
            mMenuOpen = true;
        } else if (mMenuOpen) {
            mVisible = false;
            mMenuOpen = false;
        } else {
            mMenuOpen = true;
        }
    }
    if (!mVisible)
        return;

    std::vector<ColonyId> ids;
    if (snapshot.economyRecords != nullptr) {
        ids.reserve(snapshot.economyRecords->size());
        for (ColonyEconomyRecordSnapshot const &records : *snapshot.economyRecords)
            ids.push_back(records.colonyId);
    }
    if (ids.empty()) {
        ImGui::Begin("Economy debug overlay");
        ImGui::TextUnformatted("No economy debug records in this snapshot.");
        ImGui::End();
        return;
    }
    std::sort(ids.begin(), ids.end());
    if (selectedBody && *selectedBody < snapshot.bodies.size()) {
        std::optional<ColonyId> const bodyColony = colonyIdForBody(snapshot, *selectedBody);
        if (bodyColony && std::find(ids.begin(), ids.end(), *bodyColony) != ids.end()) {
            mSelectedColony = bodyColony;
        }
    }
    if (!mSelectedColony || std::find(ids.begin(), ids.end(), *mSelectedColony) == ids.end()) {
        mSelectedColony = ids.front();
        mScrubTurn.reset();
    }
    ImGuiIO const &io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0.0F, io.DisplaySize.y - 76.0F}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({io.DisplaySize.x, 76.0F}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags FLAGS = static_cast<ImGuiWindowFlags>(combinedFlags(ImGuiWindowFlags_NoTitleBar,
                                                                                   ImGuiWindowFlags_NoMove,
                                                                                   ImGuiWindowFlags_NoResize,
                                                                                   ImGuiWindowFlags_NoSavedSettings));
    ImGui::Begin("Economy turn scrubber", nullptr, FLAGS);
    ImGui::TextUnformatted("Developer economy overlay (F9 for the window menu)");
    std::vector<std::string> labels;
    labels.reserve(ids.size());
    std::vector<char const *> labelPointers;
    labelPointers.reserve(ids.size());
    for (ColonyId id : ids)
        labels.push_back("Colony " + text(id.value));
    for (std::string const &label : labels)
        labelPointers.push_back(label.c_str());
    int colonyIndex = static_cast<int>(std::distance(ids.begin(), std::find(ids.begin(), ids.end(), *mSelectedColony)));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0F);
    if (ImGui::Combo("##debug-colony", &colonyIndex, labelPointers.data(), static_cast<int>(labelPointers.size()))) {
        mSelectedColony = ids[static_cast<std::size_t>(colonyIndex)];
        mScrubTurn.reset();
    }
    ColonyEconomyHistory const &history = client.economyHistory(*mSelectedColony);
    if (history.debugRecords.empty()) {
        ImGui::SameLine();
        ImGui::TextUnformatted("No retained DebugRecord.");
        ImGui::End();
        return;
    }
    std::int64_t const first = history.debugRecords.front().turnNumber;
    std::int64_t const last = history.debugRecords.back().turnNumber;
    DebugTurnRange const range{first, last};
    if (!mScrubTurn)
        mScrubTurn = last;
    mScrubTurn = clampDebugTurn(*mScrubTurn, range);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
        *mScrubTurn = stepDebugTurn(*mScrubTurn, range, -1, io.KeyShift);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
        *mScrubTurn = stepDebugTurn(*mScrubTurn, range, 1, io.KeyShift);
    std::int64_t sliderTurn = *mScrubTurn;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(180.0F, io.DisplaySize.x - 520.0F));
    if (first == last) {
        ImGui::Text("Turn %lld", static_cast<long long>(first));
    } else if (ImGui::SliderScalar("Turn", ImGuiDataType_S64, &sliderTurn, &first, &last, "%lld")) {
        mScrubTurn = clampDebugTurn(sliderTurn, range);
    }
    ImGui::SameLine();
    ImGui::Text("Window %lld-%lld | Left/-> step, Shift+Left/-> step 10",
                static_cast<long long>(first),
                static_cast<long long>(last));
    ImGui::SameLine();
    if (ImGui::SmallButton("Debug windows"))
        mMenuOpen = true;
    ImGui::End();

    if (mMenuOpen) {
        ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5F, io.DisplaySize.y * 0.5F}, ImGuiCond_Appearing, {0.5F, 0.5F});
        if (ImGui::Begin("Debug windows", &mMenuOpen)) {
            ImGui::TextUnformatted("Select a window to open it; open windows are focused, not duplicated.");
            for (std::size_t index = 0; index < static_cast<std::size_t>(DebugWindowId::COUNT); ++index) {
                char const *const title = debugWindowTitle(static_cast<DebugWindowId>(index));
                bool const isOpen = mWindowsOpen[index];
                std::string const label = (isOpen ? "  " : "● ") + std::string(title);
                if (ImGui::Selectable(label.c_str(), false)) {
                    mWindowsOpen[index] = true;
                    ImGui::SetWindowFocus(title);
                    mMenuOpen = false;
                }
            }
        }
        ImGui::End();
    }

    std::size_t const mapIndex = static_cast<std::size_t>(DebugWindowId::MAP_SELECTION);
    int appearedOrder{};
    auto positionOnOpen = [&](DebugWindowId window) -> bool & {
        float const offset = static_cast<float>(appearedOrder++) * 32.0F;
        ImGui::SetNextWindowPos({io.DisplaySize.x * 0.08F + offset, io.DisplaySize.y * 0.1F + offset},
                                ImGuiCond_Appearing);
        return mWindowsOpen[static_cast<std::size_t>(window)];
    };
    if (mWindowsOpen[mapIndex]) {
        bool &open = positionOnOpen(DebugWindowId::MAP_SELECTION);
        if (ImGui::Begin(debugWindowTitle(DebugWindowId::MAP_SELECTION), &open)) {
            if (selectedBody && *selectedBody < snapshot.bodies.size()) {
                ImGui::Text("Selected: %s", snapshot.bodies[*selectedBody].name.c_str());
                std::optional<ColonyId> const bodyColony = colonyIdForBody(snapshot, *selectedBody);
                std::string const mappingStatus =
                    bodyColony ? "Mapped to colony " + text(bodyColony->value) : "Selected body has no colony record.";
                ImGui::TextUnformatted(mappingStatus.c_str());
            } else {
                ImGui::TextUnformatted("No body is selected on the map.");
            }
        }
        ImGui::End();
    }

    DebugRecord const *const record = debugRecordAtTurn({history.debugRecords, *mScrubTurn});
    if (record == nullptr)
        return;
    static ResourceId selectedResource{ResourceId::Food};
    static std::optional<CorporationId> selectedCorporation;
    std::size_t const diffIndex = static_cast<std::size_t>(DebugWindowId::DIFF);
    if (mWindowsOpen[static_cast<std::size_t>(DebugWindowId::COLONY)])
        drawColony(*record, mWindowsOpen[diffIndex], selectedResource, positionOnOpen(DebugWindowId::COLONY));
    if (mWindowsOpen[static_cast<std::size_t>(DebugWindowId::MARKET)])
        drawMarket(*record, selectedResource, positionOnOpen(DebugWindowId::MARKET));
    if (mWindowsOpen[static_cast<std::size_t>(DebugWindowId::PLANNING)])
        drawPlanning(*record, selectedCorporation, positionOnOpen(DebugWindowId::PLANNING));
    if (mWindowsOpen[static_cast<std::size_t>(DebugWindowId::INVARIANTS)])
        drawInvariants(*record, positionOnOpen(DebugWindowId::INVARIANTS));
    if (mWindowsOpen[diffIndex])
        drawDiff(history.debugRecords, *record, positionOnOpen(DebugWindowId::DIFF));
}

} // namespace Client
