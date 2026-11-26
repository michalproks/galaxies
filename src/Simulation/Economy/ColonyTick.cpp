#include "ColonyTick.h"

#include "EconomyConstants.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace Simulation::Economy {
namespace {

    constexpr std::array<ResourceId, RESOURCE_COUNT - 1> TRADED_RESOURCES{
        ResourceId::Labour,
        ResourceId::Energy,
        ResourceId::Food,
        ResourceId::Materials,
        ResourceId::Fuel,
        ResourceId::Service,
        ResourceId::ConsumerGoods,
    };

    [[nodiscard]] std::int64_t checkedAdd(std::int64_t const left, std::int64_t const right) {
        if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) ||
            (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)) {
            throw std::overflow_error("Colony tick total exceeds int64_t");
        }
        return left + right;
    }

    [[nodiscard]] std::int64_t checkedMultiply(std::int64_t const left, std::int64_t const right) {
        if (left < 0 || right < 0) {
            throw std::invalid_argument("Economic quantities and prices must be non-negative");
        }
        if (right != 0 && left > std::numeric_limits<std::int64_t>::max() / right) {
            throw std::overflow_error("Colony tick value exceeds int64_t");
        }
        return left * right;
    }

    [[nodiscard]] std::int64_t seededPrice(ResourceId const resource) {
        switch (resource) {
        case ResourceId::Labour:
            return 40;
        case ResourceId::Energy:
            return 15;
        case ResourceId::Food:
            return 4;
        case ResourceId::Materials:
            return 6;
        case ResourceId::Fuel:
            return 10;
        case ResourceId::Service:
            return 40;
        case ResourceId::ConsumerGoods:
            return 30;
        case ResourceId::Credits:
            return 0;
        }
        return PRICE_FLOOR;
    }

    void seedBelief(PriceBelief &belief, ResourceId const resource) {
        if (belief.mean <= 0.0) {
            double const mean = static_cast<double>(seededPrice(resource));
            belief = {mean, mean * 0.25};
        }
    }

    void seedPrices(ColonyEconomy &colony) {
        if (colony.turnNumber != 1) {
            return;
        }
        for (ResourceId const resource : TRADED_RESOURCES) {
            auto const price = colony.market.lastPrice.find(resource);
            if (price == colony.market.lastPrice.end()) {
                colony.market.lastPrice.emplace(resource, seededPrice(resource));
            } else if (price->second <= 0) {
                price->second = seededPrice(resource);
            }
        }
        for (Household &household : colony.households) {
            seedBelief(household.foodBelief, ResourceId::Food);
            seedBelief(household.labourBelief, ResourceId::Labour);
            seedBelief(household.serviceBelief, ResourceId::Service);
            seedBelief(household.consumerGoodsBelief, ResourceId::ConsumerGoods);
            if (household.lastFoodPrice <= 0) {
                household.lastFoodPrice = seededPrice(ResourceId::Food);
            }
            if (std::all_of(household.foodPriceHistory.begin(),
                            household.foodPriceHistory.end(),
                            [](std::int64_t const price) { return price == 0; })) {
                household.foodPriceHistory.fill(seededPrice(ResourceId::Food));
            }
        }
        for (Corporation &corporation : colony.corporations) {
            for (ResourceId const resource : TRADED_RESOURCES) {
                seedBelief(corporation.priceBeliefs[static_cast<std::size_t>(resource)], resource);
            }
            auto const outputId = outputResource(corporation.sector);
            if (outputId.has_value() && corporation.lastSettlementPrice <= 0) {
                corporation.lastSettlementPrice = seededPrice(outputId.value());
            }
        }
    }

    [[nodiscard]] FoundingMarketData inferPreviousMarketData(ColonyEconomy const &colony) {
        FoundingMarketData data;
        for (auto const &[resource, book] : colony.market.orderBooks) {
            std::size_t const index = static_cast<std::size_t>(resource);
            if (index >= RESOURCE_COUNT) {
                continue;
            }
            for (Order const &order : book.buyOrders) {
                data.demand[index] = checkedAdd(data.demand[index], std::max<std::int64_t>(0, order.quantity));
            }
            data.settlementPrices[index] = colony.market.lastPriceFor(resource);
        }
        for (ResourceId const resource : TRADED_RESOURCES) {
            std::size_t const index = static_cast<std::size_t>(resource);
            if (data.settlementPrices[index] == 0) {
                data.settlementPrices[index] = colony.market.lastPriceFor(resource);
            }
        }
        return data;
    }

    [[nodiscard]] CorporationId nextCorporationId(ColonyEconomy const &colony) {
        std::int64_t nextId = 0;
        for (Corporation const &corporation : colony.corporations) {
            if (corporation.id.value >= nextId) {
                if (corporation.id.value == std::numeric_limits<std::int64_t>::max()) {
                    throw std::overflow_error("No corporation id remains available");
                }
                nextId = corporation.id.value + 1;
            }
        }
        return CorporationId{nextId};
    }

    [[nodiscard]] double priceFor(ColonyEconomy const &colony, ResourceId const resource) {
        std::int64_t const price = colony.market.lastPriceFor(resource);
        return static_cast<double>(std::max<std::int64_t>(PRICE_FLOOR, price));
    }

    [[nodiscard]] DemandCurve demandCurveFor(OrderBook const *const book) {
        DemandCurve curve;
        if (book == nullptr) {
            return curve;
        }
        std::map<std::int64_t, std::int64_t, std::greater<>> quantities;
        for (Order const &order : book->buyOrders) {
            if (order.quantity > 0) {
                quantities[order.price] = checkedAdd(quantities[order.price], order.quantity);
            }
        }
        for (auto const &[price, quantity] : quantities) {
            curve.steps.emplace_back(price, quantity);
        }
        return curve;
    }

    [[nodiscard]] std::int64_t salvageValue(Corporation const &corporation) {
        double value{};
        for (Building const &building : corporation.buildings) {
            value += static_cast<double>(building.constructionValue) * SALVAGE_RATE;
        }
        if (value >= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
            return std::numeric_limits<std::int64_t>::max();
        }
        return static_cast<std::int64_t>(std::floor(value));
    }

    struct ProductionInputs {
        ResourceAmounts standard{};
        ResourceAmounts fixedStandard{};
        std::int64_t output{};
        std::int64_t labour{};
        std::int64_t fixedLabour{};
        std::int64_t energy{};
    };

    [[nodiscard]] BuildingDef const *definitionFor(Building const &building,
                                                   std::vector<BuildingDef> const &definitions) {
        auto const found = std::find_if(definitions.begin(), definitions.end(), [&building](BuildingDef const &def) {
            return def.tier == building.tier;
        });
        return found == definitions.end() ? nullptr : &*found;
    }

    [[nodiscard]] ProductionInputs productionInputs(Corporation const &corporation, CorporationPlan const &plan) {
        ProductionInputs inputs;
        std::vector<BuildingDef> const definitions = availableMethods(corporation.sector);
        std::int64_t remainingOutput = plan.plannedOutput;

        for (Building const &building : corporation.buildings) {
            BuildingDef const *const definition = definitionFor(building, definitions);
            if (definition == nullptr) {
                continue;
            }
            inputs.standard[ResourceId::Materials] =
                checkedAdd(inputs.standard[ResourceId::Materials], definition->maintenanceMaterials);
            inputs.fixedStandard[ResourceId::Materials] =
                checkedAdd(inputs.fixedStandard[ResourceId::Materials], definition->maintenanceMaterials);
            inputs.fixedLabour = checkedAdd(inputs.fixedLabour, definition->maintenanceLabour);
            if (building.underConstruction) {
                continue;
            }
            auto const staffing = std::find_if(
                plan.staffing.begin(), plan.staffing.end(), [&corporation, &building](BuildingStaffing const &entry) {
                    std::size_t const index = static_cast<std::size_t>(&building - corporation.buildings.data());
                    return entry.buildingIndex == index;
                });
            if (staffing == plan.staffing.end() || staffing->workers <= 0 || remainingOutput <= 0) {
                continue;
            }
            std::int64_t const capacityOutput = static_cast<std::int64_t>(
                std::floor(static_cast<double>(staffing->workers) *
                           static_cast<double>(definition->outputPerManualWorker) * definition->rateMultiplier));
            std::int64_t const output = std::min(remainingOutput, capacityOutput);
            remainingOutput -= output;
            inputs.output = checkedAdd(inputs.output, output);
            inputs.labour = checkedAdd(inputs.labour, staffing->workers);
            inputs.standard[ResourceId::Materials] = checkedAdd(
                inputs.standard[ResourceId::Materials], checkedMultiply(output, definition->materialsPerOutputUnit));
            inputs.standard[ResourceId::Fuel] =
                checkedAdd(inputs.standard[ResourceId::Fuel], checkedMultiply(output, definition->fuelPerOutputUnit));
            if (definition->tier == BuildTier::Tier2) {
                inputs.energy = checkedAdd(inputs.energy, definition->energyPerTurnWhenStaffed);
            }
        }

        if (remainingOutput > 0) {
            auto const manual = std::find_if(definitions.begin(), definitions.end(), [](BuildingDef const &def) {
                return def.tier == BuildTier::Manual;
            });
            if (manual != definitions.end()) {
                std::int64_t const output = remainingOutput;
                double const rate = static_cast<double>(manual->outputPerManualWorker) * manual->rateMultiplier;
                std::int64_t const workers =
                    rate > 0.0 ? static_cast<std::int64_t>(std::ceil(static_cast<double>(output) / rate)) : 0;
                inputs.output = checkedAdd(inputs.output, output);
                inputs.labour = checkedAdd(inputs.labour, workers);
                inputs.standard[ResourceId::Materials] = checkedAdd(
                    inputs.standard[ResourceId::Materials], checkedMultiply(output, manual->materialsPerOutputUnit));
                inputs.standard[ResourceId::Fuel] =
                    checkedAdd(inputs.standard[ResourceId::Fuel], checkedMultiply(output, manual->fuelPerOutputUnit));
            }
        }

        for (Building const &building : plan.construction) {
            auto const constructionDefinition =
                std::find_if(definitions.begin(), definitions.end(), [&building](BuildingDef const &def) {
                    return def.tier == building.tier;
                });
            if (constructionDefinition != definitions.end()) {
                inputs.standard[ResourceId::Materials] =
                    checkedAdd(inputs.standard[ResourceId::Materials], constructionDefinition->constructionMaterials);
                inputs.fixedStandard[ResourceId::Materials] = checkedAdd(inputs.fixedStandard[ResourceId::Materials],
                                                                         constructionDefinition->constructionMaterials);
                inputs.fixedLabour = checkedAdd(inputs.fixedLabour, constructionDefinition->constructionLabour);
            }
        }
        inputs.output = std::min(inputs.output, plan.plannedOutput);
        return inputs;
    }

    [[nodiscard]] std::int64_t totalCredits(ColonyEconomy const &colony) {
        std::int64_t total{};
        for (Household const &household : colony.households) {
            total = checkedAdd(total, household.credits);
        }
        for (Corporation const &corporation : colony.corporations) {
            total = checkedAdd(total, corporation.credits);
        }
        return total;
    }

    [[nodiscard]] bool
    hasTradeFor(std::vector<SettlementOutcome> const &settlements, AgentRef const &owner, ResourceId const resource) {
        for (SettlementOutcome const &settlement : settlements) {
            if (settlement.resource != resource) {
                continue;
            }
            if (std::any_of(settlement.trades.begin(), settlement.trades.end(), [&owner](Trade const &trade) {
                    return trade.owner == owner;
                })) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::int64_t
    tradedQuantity(SettlementOutcome const &settlement, AgentRef const &owner, OrderSide const side) {
        std::int64_t quantity{};
        for (Trade const &trade : settlement.trades) {
            if (trade.owner == owner && trade.side == side) {
                quantity = checkedAdd(quantity, trade.quantity);
            }
        }
        return quantity;
    }

    [[nodiscard]] std::int64_t
    totalTradeValue(SettlementOutcome const &settlement, AgentRef const &owner, OrderSide const side) {
        std::int64_t value{};
        for (Trade const &trade : settlement.trades) {
            if (trade.owner == owner && trade.side == side) {
                value = checkedAdd(value, checkedMultiply(trade.quantity, trade.price));
            }
        }
        return value;
    }

    void applyCorporationTrades(Corporation &corporation,
                                std::vector<SettlementOutcome> const &settlements,
                                std::int64_t &soldQuantity) {
        for (SettlementOutcome const &settlement : settlements) {
            if (!isStandardResource(settlement.resource)) {
                continue;
            }
            std::int64_t const sold = tradedQuantity(settlement, AgentRef{corporation.id}, OrderSide::Sell);
            if (sold > 0) {
                corporation.stock[settlement.resource] -= sold;
                soldQuantity = checkedAdd(soldQuantity, sold);
            }
            std::int64_t const bought = tradedQuantity(settlement, AgentRef{corporation.id}, OrderSide::Buy);
            if (bought > 0) {
                corporation.stock[settlement.resource] = checkedAdd(corporation.stock[settlement.resource], bought);
            }
        }
    }

    [[nodiscard]] std::int64_t produceForCorporation(Corporation &corporation,
                                                     CorporationPlan const &plan,
                                                     std::vector<SettlementOutcome> const &settlements) {
        ProductionInputs const inputs = productionInputs(corporation, plan);
        std::int64_t laborAvailable{};
        std::int64_t energyAvailable{};
        for (SettlementOutcome const &settlement : settlements) {
            if (settlement.resource == ResourceId::Labour) {
                laborAvailable = tradedQuantity(settlement, AgentRef{corporation.id}, OrderSide::Buy);
            } else if (settlement.resource == ResourceId::Energy) {
                energyAvailable = tradedQuantity(settlement, AgentRef{corporation.id}, OrderSide::Buy);
            }
        }
        std::int64_t actualOutput = inputs.output;
        if (laborAvailable < inputs.fixedLabour || inputs.labour == 0) {
            actualOutput = 0;
        } else {
            std::int64_t const productiveLabor = std::min(inputs.labour, laborAvailable - inputs.fixedLabour);
            actualOutput = std::min(actualOutput,
                                    static_cast<std::int64_t>(static_cast<long double>(actualOutput) *
                                                              static_cast<long double>(productiveLabor) /
                                                              static_cast<long double>(inputs.labour)));
        }
        if (inputs.energy > 0) {
            actualOutput = std::min(actualOutput,
                                    static_cast<std::int64_t>(static_cast<long double>(actualOutput) *
                                                              static_cast<long double>(energyAvailable) /
                                                              static_cast<long double>(inputs.energy)));
        }

        for (ResourceId const resource : {ResourceId::Materials, ResourceId::Fuel}) {
            std::int64_t const available = corporation.stock[resource];
            std::int64_t const fixed = inputs.fixedStandard[resource];
            std::int64_t const variable = inputs.standard[resource] - fixed;
            if (available < fixed) {
                actualOutput = 0;
            } else if (variable > 0 && inputs.output > 0) {
                std::int64_t const variableAvailable = available - fixed;
                actualOutput = std::min(actualOutput,
                                        static_cast<std::int64_t>(static_cast<long double>(variableAvailable) *
                                                                  static_cast<long double>(inputs.output) /
                                                                  static_cast<long double>(variable)));
            }
            std::int64_t const variableUsed = inputs.output > 0
                                                  ? static_cast<std::int64_t>(static_cast<long double>(variable) *
                                                                              static_cast<long double>(actualOutput) /
                                                                              static_cast<long double>(inputs.output))
                                                  : 0;
            std::int64_t const required = checkedAdd(fixed, variableUsed);
            std::int64_t const used = std::min(available, required);
            corporation.stock[resource] -= used;
        }

        auto const outputResourceId = outputResource(corporation.sector);
        if (!outputResourceId.has_value() || actualOutput <= 0) {
            return 0;
        }
        if (!isStandardResource(outputResourceId.value())) {
            return actualOutput;
        }
        ResourceId const outputResourceValue = outputResourceId.value();
        std::int64_t const oldQuantity = corporation.stock[outputResourceValue];
        double const oldCost = std::max(0.0, corporation.stockUnitCost);
        corporation.stock[outputResourceValue] = checkedAdd(oldQuantity, actualOutput);
        corporation.stockUnitCost = (static_cast<double>(oldQuantity) * oldCost +
                                     static_cast<double>(actualOutput) * std::max(0.0, plan.outputUnitCost)) /
                                    static_cast<double>(corporation.stock[outputResourceValue]);
        return actualOutput;
    }

    [[nodiscard]] CorporationTickDelta
    corporationDelta(Corporation const &before, Corporation const &after, std::int64_t sold) {
        return {after.id, after.credits - before.credits, after.lastProducedQuantity, sold};
    }

} // namespace

ColonyTickResult tickColony(ColonyEconomy colony) {
    FoundingMarketData const inferredMarketData = inferPreviousMarketData(colony);
    return tickColony(std::move(colony), inferredMarketData);
}

ColonyTickResult tickColony(ColonyEconomy colony, FoundingMarketData const &previousMarketData) {
    if (colony.turnNumber < 1) {
        throw std::invalid_argument("Colony economic turns start at one");
    }
    if (colony.turnNumber == std::numeric_limits<std::int64_t>::max()) {
        throw std::overflow_error("Colony turn number cannot advance");
    }

    ColonyTickResult result;
    result.colony = std::move(colony);
    ColonyEconomy &next = result.colony;
    seedPrices(next);
    std::int64_t const startingCredits = totalCredits(next);
    std::vector<Household> const householdsAtStart = next.households;
    std::vector<Corporation> const corporationsAtStart = next.corporations;
    std::uint64_t const turn = static_cast<std::uint64_t>(next.turnNumber);
    Rng const rootRng{next.id, turn};
    Rng const turnRng = rootRng.substream(next.economySeed);

    // Stage 1 (economic-model.md, Turn sequence): roll now; founded agents are deferred to next turn.
    result.founded = rollFoundings(next, previousMarketData, turnRng).corporations;
    std::vector<Corporation> newCorporations;
    CorporationId newCorporationId = nextCorporationId(next);
    for (FoundedCorporation const &founding : result.founded) {
        auto household =
            std::find_if(next.households.begin(), next.households.end(), [&founding](Household const &candidate) {
                return candidate.id == founding.householdId;
            });
        if (household == next.households.end() || household->credits < founding.capitalTransfer) {
            throw std::logic_error("Founding roll selected a household without available capital");
        }
        household->credits -= founding.capitalTransfer;
        Corporation corporation;
        corporation.id = newCorporationId;
        if (newCorporationId.value == std::numeric_limits<std::int64_t>::max()) {
            throw std::overflow_error("No corporation id remains available");
        }
        ++newCorporationId.value;
        corporation.owner = founding.householdId;
        corporation.sector = founding.sector;
        corporation.credits = founding.capitalTransfer;
        for (ResourceId const resource : TRADED_RESOURCES) {
            seedBelief(corporation.priceBeliefs[static_cast<std::size_t>(resource)], resource);
        }
        auto const outputId = outputResource(corporation.sector);
        if (outputId.has_value()) {
            corporation.lastSettlementPrice = seededPrice(outputId.value());
        }
        newCorporations.push_back(std::move(corporation));
    }

    std::vector<HouseholdOrderPlan> householdPlans;
    householdPlans.reserve(next.households.size());
    std::int64_t nextOrderId{};
    for (Household const &household : next.households) {
        HouseholdOrderIds const ids{OrderId{nextOrderId++},
                                    OrderId{nextOrderId++},
                                    OrderId{nextOrderId++},
                                    OrderId{nextOrderId++},
                                    OrderId{nextOrderId++},
                                    OrderId{nextOrderId++}};
        Rng householdRng = turnRng.substream(static_cast<std::uint64_t>(household.id.value));
        HouseholdPriceSamples const samples = sampleHouseholdPrices(household, householdRng);
        householdPlans.push_back(makeHouseholdOrders(household, samples, ids));
    }

    std::vector<CorporationPlan> corporationPlans;
    corporationPlans.reserve(next.corporations.size());
    for (Corporation const &corporation : next.corporations) {
        CorporationPrices prices;
        prices.inputs = {priceFor(next, ResourceId::Labour),
                         priceFor(next, ResourceId::Materials),
                         priceFor(next, ResourceId::Energy),
                         priceFor(next, ResourceId::Fuel)};
        auto const outputId = outputResource(corporation.sector);
        prices.output = outputId.has_value() ? next.market.lastPriceFor(outputId.value()) : PRICE_FLOOR;
        for (std::size_t index = 0; index < RESOURCE_COUNT; ++index) {
            prices.resources[index] = next.market.lastPriceFor(static_cast<ResourceId>(index));
        }
        prices.firstOrderId = OrderId{nextOrderId};
        CorporationPlan plan = planCorporation(corporation, prices);
        nextOrderId = checkedAdd(nextOrderId, static_cast<std::int64_t>(plan.orders.size()));
        corporationPlans.push_back(std::move(plan));
    }

    // Stage 2: replace old books with this turn's household and corporate submissions.
    next.market.orderBooks.clear();
    for (HouseholdOrderPlan const &plan : householdPlans) {
        for (Order const &order : plan.orders) {
            OrderBook &book = next.market.orderBookFor(order.resource);
            (order.side == OrderSide::Buy ? book.buyOrders : book.sellOrders).push_back(order);
        }
    }
    for (CorporationPlan const &plan : corporationPlans) {
        for (Order const &order : plan.orders) {
            OrderBook &book = next.market.orderBookFor(order.resource);
            (order.side == OrderSide::Buy ? book.buyOrders : book.sellOrders).push_back(order);
        }
    }

    for (auto const &[resource, book] : next.market.orderBooks) {
        std::size_t const index = static_cast<std::size_t>(resource);
        for (Order const &order : book.buyOrders) {
            result.marketData.demand[index] = checkedAdd(result.marketData.demand[index], order.quantity);
        }
    }

    // Stages 3-4: settlement computes each resource price, then rationed fills.
    result.settlements.reserve(TRADED_RESOURCES.size());
    for (ResourceId const resource : TRADED_RESOURCES) {
        SettlementOutcome settlement = next.market.settle(resource);
        result.marketData.settlementPrices[static_cast<std::size_t>(resource)] = settlement.price;
        result.settlements.push_back(settlement);
    }

    // Stage 5: apply corporation market transfers. Household ledgers are updated with their
    // complete fills in Stage 6 so their spending and income remain coupled to consumption.
    std::vector<std::int64_t> corporateDividends(next.households.size());
    std::vector<bool> corporationLiquidated(next.corporations.size());
    std::vector<std::int64_t> corporationSold(next.corporations.size());
    std::vector<Corporation> corporationsBefore = next.corporations;
    for (std::size_t corporationIndex = 0; corporationIndex < next.corporations.size(); ++corporationIndex) {
        Corporation &corporation = next.corporations[corporationIndex];
        CorporationPlan const &plan = corporationPlans[corporationIndex];
        std::int64_t buyCost{};
        std::int64_t sellRevenue{};
        for (SettlementOutcome const &settlement : result.settlements) {
            buyCost = checkedAdd(buyCost, totalTradeValue(settlement, AgentRef{corporation.id}, OrderSide::Buy));
            sellRevenue =
                checkedAdd(sellRevenue, totalTradeValue(settlement, AgentRef{corporation.id}, OrderSide::Sell));
            result.marketData.filled[static_cast<std::size_t>(settlement.resource)] =
                checkedAdd(result.marketData.filled[static_cast<std::size_t>(settlement.resource)],
                           tradedQuantity(settlement, AgentRef{corporation.id}, OrderSide::Buy));
        }
        corporation.credits = checkedAdd(corporation.credits - buyCost, sellRevenue);
        applyCorporationTrades(corporation, result.settlements, corporationSold[corporationIndex]);
        auto const owner =
            std::find_if(next.households.begin(), next.households.end(), [&corporation](Household const &household) {
                return household.id == corporation.owner;
            });
        if (plan.liquidate) {
            corporationLiquidated[corporationIndex] = true;
            std::int64_t const salvage = salvageValue(corporation);
            result.liquidationCredits = checkedAdd(result.liquidationCredits, salvage);
            if (owner != next.households.end()) {
                std::size_t const ownerIndex = static_cast<std::size_t>(owner - next.households.begin());
                corporateDividends[ownerIndex] =
                    checkedAdd(corporateDividends[ownerIndex], checkedAdd(corporation.credits, salvage));
            }
            result.dissolvedCorporations.push_back(
                {corporation.id, corporation.owner, checkedAdd(corporation.credits, salvage)});
            corporation.credits = 0;
        } else {
            auto const ownerIndex = owner == next.households.end()
                                        ? next.households.size()
                                        : static_cast<std::size_t>(owner - next.households.begin());
            if (ownerIndex < corporateDividends.size() && corporation.credits > 0) {
                std::int64_t const dividend =
                    static_cast<std::int64_t>(std::floor(static_cast<double>(corporation.credits) * DIVIDEND_RATE));
                corporation.credits -= dividend;
                corporateDividends[ownerIndex] = checkedAdd(corporateDividends[ownerIndex], dividend);
                corporation.lastIncome = sellRevenue;
            }
        }
        if (corporation.credits == 0 && !corporationLiquidated[corporationIndex]) {
            corporationLiquidated[corporationIndex] = true;
            std::int64_t const salvage = salvageValue(corporation);
            result.liquidationCredits = checkedAdd(result.liquidationCredits, salvage);
            if (owner != next.households.end()) {
                std::size_t const ownerIndex = static_cast<std::size_t>(owner - next.households.begin());
                corporateDividends[ownerIndex] = checkedAdd(corporateDividends[ownerIndex], salvage);
            }
            result.dissolvedCorporations.push_back({corporation.id, corporation.owner, salvage});
        }
    }

    // Stage 6: settle household ledgers, food stocks, consumption, and population.
    for (std::size_t index = 0; index < next.households.size(); ++index) {
        Household &household = next.households[index];
        HouseholdSettlement settlement;
        settlement.dividends = corporateDividends[index];
        for (SettlementOutcome const &marketSettlement : result.settlements) {
            std::int64_t quantity{};
            switch (marketSettlement.resource) {
            case ResourceId::Food:
                settlement.food.price = marketSettlement.price;
                quantity = tradedQuantity(marketSettlement, AgentRef{household.id}, OrderSide::Buy);
                settlement.food.quantity = checkedAdd(settlement.food.quantity, quantity);
                result.marketData.filled[static_cast<std::size_t>(ResourceId::Food)] =
                    checkedAdd(result.marketData.filled[static_cast<std::size_t>(ResourceId::Food)], quantity);
                break;
            case ResourceId::Labour:
                settlement.labour.price = marketSettlement.price;
                settlement.labour.quantity = tradedQuantity(marketSettlement, AgentRef{household.id}, OrderSide::Sell);
                break;
            case ResourceId::Service:
                settlement.service.price = marketSettlement.price;
                settlement.service.quantity = tradedQuantity(marketSettlement, AgentRef{household.id}, OrderSide::Buy);
                result.marketData.filled[static_cast<std::size_t>(ResourceId::Service)] =
                    checkedAdd(result.marketData.filled[static_cast<std::size_t>(ResourceId::Service)],
                               settlement.service.quantity);
                break;
            case ResourceId::ConsumerGoods:
                settlement.consumerGoods.price = marketSettlement.price;
                settlement.consumerGoods.quantity =
                    tradedQuantity(marketSettlement, AgentRef{household.id}, OrderSide::Buy);
                result.marketData.filled[static_cast<std::size_t>(ResourceId::ConsumerGoods)] =
                    checkedAdd(result.marketData.filled[static_cast<std::size_t>(ResourceId::ConsumerGoods)],
                               settlement.consumerGoods.quantity);
                break;
            default:
                break;
            }
        }
        HouseholdTurnResult const turnResult = updateHouseholdConsumption(household, householdPlans[index], settlement);
        household = turnResult.household;
        result.householdDeltas.push_back({household.id,
                                          household.credits - householdsAtStart[index].credits,
                                          household.population - householdsAtStart[index].population,
                                          household.foodStock - householdsAtStart[index].foodStock,
                                          turnResult.foodEaten,
                                          turnResult.servicesConsumed,
                                          turnResult.consumerGoodsConsumed});

        for (auto const &[resource, belief, filled] :
             {std::tuple{ResourceId::Food, &household.foodBelief, settlement.food.quantity > 0},
              std::tuple{ResourceId::Labour, &household.labourBelief, settlement.labour.quantity > 0},
              std::tuple{ResourceId::Service, &household.serviceBelief, settlement.service.quantity > 0},
              std::tuple{
                  ResourceId::ConsumerGoods, &household.consumerGoodsBelief, settlement.consumerGoods.quantity > 0}}) {
            *belief = updatedPriceBelief(*belief, static_cast<double>(next.market.lastPriceFor(resource)), filled);
        }
    }

    // Stage 7: consume planned inputs, preserve unsold standard output, and add this turn's production.
    result.corporationDeltas.reserve(next.corporations.size() + newCorporations.size());
    for (std::size_t index = 0; index < next.corporations.size(); ++index) {
        Corporation &corporation = next.corporations[index];
        corporation.lastProducedQuantity =
            produceForCorporation(corporation, corporationPlans[index], result.settlements);
        for (BuildingStaffing const &staffing : corporationPlans[index].staffing) {
            if (staffing.buildingIndex < corporation.buildings.size()) {
                corporation.buildings[staffing.buildingIndex].staffedWorkers = staffing.workers;
            }
        }
        if (!corporationPlans[index].buildingsDestroyed.empty()) {
            std::vector<Building> retained;
            retained.reserve(corporation.buildings.size());
            for (std::size_t buildingIndex = 0; buildingIndex < corporation.buildings.size(); ++buildingIndex) {
                if (std::find(corporationPlans[index].buildingsDestroyed.begin(),
                              corporationPlans[index].buildingsDestroyed.end(),
                              buildingIndex) == corporationPlans[index].buildingsDestroyed.end()) {
                    retained.push_back(corporation.buildings[buildingIndex]);
                }
            }
            corporation.buildings = std::move(retained);
        }
        corporation.buildings.insert(corporation.buildings.end(),
                                     corporationPlans[index].construction.begin(),
                                     corporationPlans[index].construction.end());
        if (!corporationPlans[index].construction.empty()) {
            corporation.hasHadFirstBuilding = true;
            corporation.jumpStartUsed = true;
        }
        auto const outputId = outputResource(corporation.sector);
        if (outputId.has_value()) {
            corporation.priceBeliefs[static_cast<std::size_t>(outputId.value())] =
                updatedPriceBelief(corporation.priceBeliefs[static_cast<std::size_t>(outputId.value())],
                                   static_cast<double>(next.market.lastPriceFor(outputId.value())),
                                   hasTradeFor(result.settlements, AgentRef{corporation.id}, outputId.value()));
        }
        for (ResourceId const resource : {ResourceId::Labour,
                                          ResourceId::Energy,
                                          ResourceId::Food,
                                          ResourceId::Materials,
                                          ResourceId::Fuel,
                                          ResourceId::Service,
                                          ResourceId::ConsumerGoods}) {
            if (outputId.has_value() && resource == outputId.value()) {
                continue;
            }
            corporation.priceBeliefs[static_cast<std::size_t>(resource)] =
                updatedPriceBelief(corporation.priceBeliefs[static_cast<std::size_t>(resource)],
                                   static_cast<double>(next.market.lastPriceFor(resource)),
                                   hasTradeFor(result.settlements, AgentRef{corporation.id}, resource));
        }
        auto const currentOutput = outputResource(corporation.sector);
        if (currentOutput.has_value()) {
            corporation.sectorDemand = demandCurveFor(next.market.findOrderBook(currentOutput.value()));
            corporation.lastSettlementPrice = next.market.lastPriceFor(currentOutput.value());
        }
        result.corporationDeltas.push_back(
            corporationDelta(corporationsBefore[index], corporation, corporationSold[index]));
    }
    for (Corporation &corporation : newCorporations) {
        auto const currentOutput = outputResource(corporation.sector);
        if (currentOutput.has_value()) {
            corporation.sectorDemand = demandCurveFor(next.market.findOrderBook(currentOutput.value()));
        }
        result.corporationDeltas.push_back({corporation.id, corporation.credits, 0, 0});
    }
    for (Corporation &corporation : newCorporations) {
        next.corporations.push_back(std::move(corporation));
        corporationLiquidated.push_back(false);
        corporationSold.push_back(0);
        corporationsBefore.push_back(next.corporations.back());
    }

    // Stage 8: instant supplies have no persistent inventory; order fills are their only records.
    // Stage 9: beliefs for household and corporation orders were updated from these settlements.

    // Stage 10: dissolve starved households after liquidating their owned corporations.
    std::vector<std::size_t> dissolving;
    for (std::size_t index = 0; index < next.households.size(); ++index) {
        if (shouldDissolveHousehold(next.households[index])) {
            dissolving.push_back(index);
            result.dissolvedHouseholds.push_back(next.households[index].id);
        }
    }
    if (!dissolving.empty()) {
        for (std::size_t corporationIndex = 0; corporationIndex < next.corporations.size(); ++corporationIndex) {
            Corporation &corporation = next.corporations[corporationIndex];
            bool const ownerDissolves =
                std::any_of(dissolving.begin(), dissolving.end(), [&next, &corporation](std::size_t const index) {
                    return next.households[index].id == corporation.owner;
                });
            if (!ownerDissolves || corporationLiquidated[corporationIndex]) {
                continue;
            }
            std::int64_t const salvage = salvageValue(corporation);
            std::int64_t proceeds = checkedAdd(corporation.credits, salvage);
            result.liquidationCredits = checkedAdd(result.liquidationCredits, salvage);
            for (ResourceId const resource :
                 {ResourceId::Food, ResourceId::Materials, ResourceId::Fuel, ResourceId::ConsumerGoods}) {
                std::int64_t const stockProceeds =
                    checkedMultiply(corporation.stock[resource],
                                    std::max<std::int64_t>(PRICE_FLOOR, next.market.lastPriceFor(resource)));
                proceeds = checkedAdd(proceeds, stockProceeds);
                result.liquidationCredits = checkedAdd(result.liquidationCredits, stockProceeds);
                corporation.stock[resource] = 0;
            }
            auto owner =
                std::find_if(next.households.begin(),
                             next.households.end(),
                             [&corporation](Household const &household) { return household.id == corporation.owner; });
            if (owner != next.households.end()) {
                owner->credits = checkedAdd(owner->credits, proceeds);
            }
            result.dissolvedCorporations.push_back({corporation.id, corporation.owner, proceeds});
            corporationLiquidated[corporationIndex] = true;
        }

        std::vector<Household> survivors;
        survivors.reserve(next.households.size() - dissolving.size());
        for (std::size_t index = 0; index < next.households.size(); ++index) {
            if (std::find(dissolving.begin(), dissolving.end(), index) == dissolving.end()) {
                survivors.push_back(next.households[index]);
            }
        }
        if (!survivors.empty()) {
            std::int64_t transferCredits{};
            std::int64_t transferFood{};
            for (std::size_t const index : dissolving) {
                transferCredits = checkedAdd(transferCredits, next.households[index].credits);
                transferFood = checkedAdd(transferFood, next.households[index].foodStock);
            }
            std::int64_t const creditShare = transferCredits / static_cast<std::int64_t>(survivors.size());
            std::int64_t const foodShare = transferFood / static_cast<std::int64_t>(survivors.size());
            std::int64_t creditRemainder = transferCredits % static_cast<std::int64_t>(survivors.size());
            std::int64_t foodRemainder = transferFood % static_cast<std::int64_t>(survivors.size());
            for (Household &survivor : survivors) {
                survivor.credits = checkedAdd(survivor.credits, creditShare + (creditRemainder-- > 0 ? 1 : 0));
                survivor.foodStock = checkedAdd(survivor.foodStock, foodShare + (foodRemainder-- > 0 ? 1 : 0));
            }
        }
        next.households = std::move(survivors);
        next.corporations.erase(
            std::remove_if(next.corporations.begin(),
                           next.corporations.end(),
                           [&corporationLiquidated, &corporationsBefore](Corporation const &corporation) {
                               auto const original = std::find_if(corporationsBefore.begin(),
                                                                  corporationsBefore.end(),
                                                                  [&corporation](Corporation const &candidate) {
                                                                      return candidate.id == corporation.id;
                                                                  });
                               if (original == corporationsBefore.end()) {
                                   return false;
                               }
                               std::size_t const index =
                                   static_cast<std::size_t>(original - corporationsBefore.begin());
                               return static_cast<bool>(corporationLiquidated[index]);
                           }),
            next.corporations.end());
    }

    // Stage 10 also removes any corporation that exhausted its ledger during clearing.
    for (std::size_t index = 0; index < corporationLiquidated.size(); ++index) {
        if (corporationLiquidated[index] &&
            std::none_of(result.dissolvedCorporations.begin(),
                         result.dissolvedCorporations.end(),
                         [&corporationsBefore, index](DissolvedCorporation const &event) {
                             return event.id == corporationsBefore[index].id;
                         })) {
            result.dissolvedCorporations.push_back({corporationsBefore[index].id, corporationsBefore[index].owner, 0});
        }
    }
    next.corporations.erase(std::remove_if(next.corporations.begin(),
                                           next.corporations.end(),
                                           [&result](Corporation const &corporation) {
                                               return std::any_of(result.dissolvedCorporations.begin(),
                                                                  result.dissolvedCorporations.end(),
                                                                  [&corporation](DissolvedCorporation const &event) {
                                                                      return event.id == corporation.id;
                                                                  });
                                           }),
                            next.corporations.end());

    next.totalPopulation = 0;
    for (Household const &household : next.households) {
        next.totalPopulation = checkedAdd(next.totalPopulation, household.population);
    }
    next.turnNumber = checkedAdd(next.turnNumber, 1);
    for (HouseholdTickDelta &delta : result.householdDeltas) {
        auto const original = std::find_if(householdsAtStart.begin(),
                                           householdsAtStart.end(),
                                           [&delta](Household const &household) { return household.id == delta.id; });
        auto const current = std::find_if(next.households.begin(),
                                          next.households.end(),
                                          [&delta](Household const &household) { return household.id == delta.id; });
        if (original == householdsAtStart.end()) {
            continue;
        }
        delta.credits = current == next.households.end() ? -original->credits : current->credits - original->credits;
        delta.population =
            current == next.households.end() ? -original->population : current->population - original->population;
        delta.foodStock =
            current == next.households.end() ? -original->foodStock : current->foodStock - original->foodStock;
    }
    for (CorporationTickDelta &delta : result.corporationDeltas) {
        auto const original =
            std::find_if(corporationsAtStart.begin(),
                         corporationsAtStart.end(),
                         [&delta](Corporation const &corporation) { return corporation.id == delta.id; });
        auto const current =
            std::find_if(next.corporations.begin(), next.corporations.end(), [&delta](Corporation const &corporation) {
                return corporation.id == delta.id;
            });
        if (original == corporationsAtStart.end()) {
            delta.credits = current == next.corporations.end() ? 0 : current->credits;
        } else {
            delta.credits =
                current == next.corporations.end() ? -original->credits : current->credits - original->credits;
        }
    }
    if (totalCredits(next) != checkedAdd(startingCredits, result.liquidationCredits)) {
        throw std::logic_error("Colony tick credit transfers did not balance against liquidation proceeds");
    }
    return result;
}

} // namespace Simulation::Economy
