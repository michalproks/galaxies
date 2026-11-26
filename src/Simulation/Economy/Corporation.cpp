#include "Corporation.h"

#include "EconomyConstants.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace Simulation::Economy {
namespace {

    struct MethodPlan {
        BuildingDef definition;
        double unitCost{};
        std::int64_t outputCapacity{};
        std::vector<std::size_t> buildings;
    };

    struct Allocation {
        std::vector<std::int64_t> outputByMethod;
        double modeledCost{};
    };

    [[nodiscard]] std::int64_t ceilToInt64(double const value) {
        return value <= 0.0 ? 0 : static_cast<std::int64_t>(std::ceil(value));
    }

    [[nodiscard]] std::int64_t roundedPrice(double const value) {
        return std::clamp(static_cast<std::int64_t>(std::llround(value)), PRICE_FLOOR, PRICE_CAP);
    }

    [[nodiscard]] double orderPrice(Corporation const &corporation, ResourceId const resource) {
        return corporation.priceBeliefs[static_cast<std::size_t>(resource)].mean;
    }

    [[nodiscard]] std::vector<MethodPlan> makeMethods(Corporation const &corporation, InputPrices const &prices) {
        std::vector<MethodPlan> methods;
        for (BuildingDef const &definition : availableMethods(corporation.sector)) {
            MethodPlan method{
                definition,
                unitCostCenticreditsPerOutput(corporation.sector, definition.tier, prices),
                0,
                {},
            };
            if (definition.tier == BuildTier::Manual) {
                method.outputCapacity = std::numeric_limits<std::int64_t>::max();
            } else {
                for (std::size_t index = 0; index < corporation.buildings.size(); ++index) {
                    Building const &building = corporation.buildings[index];
                    if (building.sector == corporation.sector && building.tier == definition.tier &&
                        !building.underConstruction) {
                        method.buildings.push_back(index);
                        method.outputCapacity += definition.capacity * definition.outputPerManualWorker *
                                                 static_cast<std::int64_t>(definition.rateMultiplier);
                    }
                }
            }
            methods.push_back(std::move(method));
        }
        std::stable_sort(methods.begin(), methods.end(), [](MethodPlan const &left, MethodPlan const &right) {
            return std::tie(left.unitCost, left.definition.tier) < std::tie(right.unitCost, right.definition.tier);
        });
        return methods;
    }

    [[nodiscard]] double costForOutput(std::vector<MethodPlan> const &methods, std::int64_t quantity) {
        double cost{};
        std::int64_t remaining = quantity;
        for (MethodPlan const &method : methods) {
            std::int64_t const produced = std::min(remaining, method.outputCapacity);
            cost += static_cast<double>(produced) * method.unitCost;
            remaining -= produced;
            if (remaining == 0) {
                break;
            }
        }
        return cost;
    }

    [[nodiscard]] Allocation allocateOutput(std::vector<MethodPlan> const &methods, std::int64_t quantity) {
        Allocation allocation;
        allocation.outputByMethod.resize(methods.size());
        std::int64_t remaining = quantity;
        for (std::size_t index = 0; index < methods.size() && remaining > 0; ++index) {
            MethodPlan const &method = methods[index];
            std::int64_t const produced = std::min(remaining, method.outputCapacity);
            allocation.outputByMethod[index] = produced;
            allocation.modeledCost += static_cast<double>(produced) * method.unitCost;
            remaining -= produced;
        }
        return allocation;
    }

    [[nodiscard]] std::optional<std::pair<std::int64_t, std::int64_t>>
    profitSweep(Corporation const &corporation, std::vector<MethodPlan> const &methods) {
        std::optional<std::pair<std::int64_t, std::int64_t>> best;
        double bestProfit{};
        for (auto const &[price, unusedStepQuantity] : corporation.sectorDemand.steps) {
            static_cast<void>(unusedStepQuantity);
            std::int64_t const quantity = corporation.sectorDemand.quantityAtPrice(price);
            double const profit =
                static_cast<double>(price) * static_cast<double>(quantity) - costForOutput(methods, quantity);
            bool const closerTie = best.has_value() && std::abs(price - corporation.lastSettlementPrice) <
                                                           std::abs(best->first - corporation.lastSettlementPrice);
            bool const equalDistanceLowerPrice = best.has_value() &&
                                                 std::abs(price - corporation.lastSettlementPrice) ==
                                                     std::abs(best->first - corporation.lastSettlementPrice) &&
                                                 price < best->first;
            bool const profitTie =
                best.has_value() &&
                std::abs(profit - bestProfit) <= 1e-12 * std::max({1.0, std::abs(profit), std::abs(bestProfit)});
            if (profit > 0.0 && (!best.has_value() || (profit > bestProfit && !profitTie) ||
                                 (profitTie && (closerTie || equalDistanceLowerPrice)))) {
                best = std::pair{price, quantity};
                bestProfit = profit;
            }
        }
        return best;
    }

    [[nodiscard]] std::pair<std::int64_t, std::int64_t> maintenanceInputs(Corporation const &corporation) {
        std::int64_t labour{};
        std::int64_t materials{};
        for (Building const &building : corporation.buildings) {
            if (!building.underConstruction) {
                auto const methods = availableMethods(building.sector);
                auto const definition = std::find_if(methods.begin(), methods.end(), [&](BuildingDef const &candidate) {
                    return candidate.tier == building.tier;
                });
                if (definition != methods.end()) {
                    labour += definition->maintenanceLabour;
                    materials += definition->maintenanceMaterials;
                }
            }
        }
        return {labour, materials};
    }

    [[nodiscard]] BuildingDef const *definitionForTier(std::vector<BuildingDef> const &definitions, BuildTier tier) {
        auto const found = std::find_if(definitions.begin(), definitions.end(), [tier](BuildingDef const &definition) {
            return definition.tier == tier;
        });
        return found == definitions.end() ? nullptr : &*found;
    }

    [[nodiscard]] std::optional<BuildingDef> constructionCandidate(Corporation const &corporation,
                                                                   InputPrices const &prices,
                                                                   std::int64_t targetPrice,
                                                                   bool const requireProfitability) {
        std::optional<BuildingDef> best;
        double bestCostPerOutput = std::numeric_limits<double>::infinity();
        for (BuildingDef const &definition : availableMethods(corporation.sector)) {
            if (definition.tier == BuildTier::Manual) {
                continue;
            }
            std::int64_t const cost = constructionValue(
                definition, static_cast<std::int64_t>(prices.labour), static_cast<std::int64_t>(prices.materials));
            double const unitCost = unitCostCenticreditsPerOutput(corporation.sector, definition.tier, prices);
            double const outputCapacity = static_cast<double>(definition.capacity) *
                                          static_cast<double>(definition.outputPerManualWorker) *
                                          definition.rateMultiplier;
            if (requireProfitability &&
                (static_cast<double>(targetPrice) <= unitCost ||
                 static_cast<double>(cost) > (static_cast<double>(targetPrice) - unitCost) * outputCapacity *
                                                 static_cast<double>(PAYBACK_TURNS))) {
                continue;
            }
            double const costPerOutput = static_cast<double>(cost) / outputCapacity;
            if (!best.has_value() || costPerOutput < bestCostPerOutput) {
                best = definition;
                bestCostPerOutput = costPerOutput;
            }
        }
        return best;
    }

    [[nodiscard]] std::int64_t productionLabour(BuildingDef const &definition, std::int64_t output) {
        double const rate = static_cast<double>(definition.outputPerManualWorker) * definition.rateMultiplier;
        return ceilToInt64(static_cast<double>(output) / rate);
    }

    [[nodiscard]] std::int64_t outputCapacity(BuildingDef const &definition) {
        return definition.capacity * definition.outputPerManualWorker *
               static_cast<std::int64_t>(definition.rateMultiplier);
    }

} // namespace

CorporationPlan planCorporation(Corporation const &corporation, CorporationPrices const &prices) {
    CorporationPlan plan;
    std::optional<ResourceId> const outputResourceId = outputResource(corporation.sector);
    if (!outputResourceId.has_value()) {
        return plan;
    }
    std::int64_t nextOrderId = prices.firstOrderId.value;
    // Belief-priced bids can exceed the settlement-price estimates used by the plan.
    // Reserve cumulative limit-price spend so fills cannot debit more than current credits.
    std::int64_t remainingBuyCredits = std::max<std::int64_t>(0, corporation.credits);
    auto addOrder =
        [&](OrderSide const side, ResourceId const resource, std::int64_t const price, std::int64_t quantity) {
            if (side == OrderSide::Buy) {
                if (price < PRICE_FLOOR) {
                    throw std::invalid_argument("Corporation buy orders require a positive price");
                }
                quantity = std::min(quantity, remainingBuyCredits / price);
                remainingBuyCredits -= quantity * price;
            }
            if (quantity > 0) {
                plan.orders.push_back({OrderId{nextOrderId++}, corporation.id, side, resource, price, quantity});
            }
        };

    bool const noCompletedBuildings =
        corporation.buildings.empty() && !corporation.hasHadFirstBuilding && !corporation.jumpStartUsed;
    std::int64_t const budget = static_cast<std::int64_t>(
        std::floor(static_cast<double>(std::max<std::int64_t>(0, corporation.credits)) * (1.0 - RESERVE_FRACTION)));
    auto const [maintenanceLabour, maintenanceMaterials] = maintenanceInputs(corporation);
    double const maintenanceCost = static_cast<double>(maintenanceLabour) * prices.inputs.labour +
                                   static_cast<double>(maintenanceMaterials) * prices.inputs.materials;
    std::vector<MethodPlan> methods = makeMethods(corporation, prices.inputs);
    // economic-model.md, Production (Energy): leave tier 2 idle when its flat
    // energy bill cannot be covered after maintenance, allowing cheaper methods
    // to cover what they can.
    double const productionBudget = std::max(0.0, static_cast<double>(budget) - maintenanceCost);
    for (MethodPlan &method : methods) {
        if (method.definition.tier == BuildTier::Tier2 &&
            static_cast<double>(method.definition.energyPerTurnWhenStaffed) * prices.inputs.energy > productionBudget) {
            method.outputCapacity = 0;
        }
    }
    auto const best = profitSweep(corporation, methods);
    if (best.has_value()) {
        plan.targetPrice = best->first;
        plan.targetQuantity = best->second;
        plan.targetProfit = static_cast<double>(plan.targetPrice) * static_cast<double>(plan.targetQuantity) -
                            costForOutput(methods, plan.targetQuantity);
    }

    double targetBreakEven{};
    Allocation const targetAllocation = allocateOutput(methods, plan.targetQuantity);
    for (std::size_t index = 0; index < methods.size(); ++index) {
        if (targetAllocation.outputByMethod[index] > 0) {
            targetBreakEven = std::max(targetBreakEven, methods[index].unitCost);
        }
    }
    double const previousBreakEven =
        corporation.breakEvenPrice > 0.0 ? corporation.breakEvenPrice : corporation.stockUnitCost;
    std::int64_t desiredOutput = plan.targetQuantity;
    if (plan.targetQuantity > 0 && static_cast<double>(prices.output) >= targetBreakEven * (1.0 + SURPLUS_MARGIN)) {
        std::int64_t const demandAtLastPrice =
            corporation.sectorDemand.quantityAtPrice(corporation.lastSettlementPrice);
        desiredOutput += std::max<std::int64_t>(0, demandAtLastPrice - plan.targetQuantity);
    }
    Allocation allocation = allocateOutput(methods, desiredOutput);
    auto actualPlanCost = [&](Allocation const &current) {
        double cost = maintenanceCost;
        for (std::size_t index = 0; index < methods.size(); ++index) {
            std::int64_t const quantity = current.outputByMethod[index];
            if (quantity == 0) {
                continue;
            }
            MethodPlan const &method = methods[index];
            double const energyComponent = method.definition.tier == BuildTier::Tier2
                                               ? prices.inputs.energy *
                                                     static_cast<double>(method.definition.energyPerTurnWhenStaffed) /
                                                     (static_cast<double>(method.definition.capacity) *
                                                      static_cast<double>(method.definition.outputPerManualWorker) *
                                                      method.definition.rateMultiplier)
                                               : 0.0;
            cost += static_cast<double>(quantity) * std::max(0.0, method.unitCost - energyComponent);
            if (method.definition.tier == BuildTier::Tier2) {
                std::int64_t const workersPerBuilding = method.definition.capacity;
                std::int64_t const workersNeeded = productionLabour(method.definition, quantity);
                std::int64_t const buildingCount =
                    std::min<std::int64_t>(static_cast<std::int64_t>(method.buildings.size()),
                                           (workersNeeded + workersPerBuilding - 1) / workersPerBuilding);
                cost += static_cast<double>(buildingCount) *
                        static_cast<double>(method.definition.energyPerTurnWhenStaffed) * prices.inputs.energy;
            }
        }
        return cost;
    };

    // economic-model.md, Order Submission: surplus is the first budget cut.
    if (actualPlanCost(allocation) > static_cast<double>(budget) && desiredOutput > plan.targetQuantity) {
        desiredOutput = plan.targetQuantity;
        allocation = allocateOutput(methods, desiredOutput);
    }
    // Drop complete higher-cost methods first. If only one method remains, scale
    // its staffing to the affordable amount (manual-only new firms included).
    if (actualPlanCost(allocation) > static_cast<double>(budget)) {
        while (actualPlanCost(allocation) > static_cast<double>(budget)) {
            std::size_t activeMethods{};
            std::size_t mostExpensiveActive{};
            for (std::size_t index = 0; index < allocation.outputByMethod.size(); ++index) {
                if (allocation.outputByMethod[index] > 0) {
                    ++activeMethods;
                    mostExpensiveActive = index;
                }
            }
            if (activeMethods <= 1) {
                break;
            }
            allocation.outputByMethod[mostExpensiveActive] = 0;
            desiredOutput =
                std::accumulate(allocation.outputByMethod.begin(), allocation.outputByMethod.end(), std::int64_t{});
            allocation = allocateOutput(methods, desiredOutput);
        }
    }
    if (actualPlanCost(allocation) > static_cast<double>(budget)) {
        std::int64_t low{};
        std::int64_t high = desiredOutput;
        while (low < high) {
            std::int64_t const midpoint = low + (high - low + 1) / 2;
            Allocation const candidate = allocateOutput(methods, midpoint);
            if (actualPlanCost(candidate) <= static_cast<double>(budget)) {
                low = midpoint;
            } else {
                high = midpoint - 1;
            }
        }
        desiredOutput = low;
        allocation = allocateOutput(methods, desiredOutput);
    }

    // economic-model.md, Production: jump-start spending bypasses the payback gate.
    if (noCompletedBuildings && !isService(corporation.sector)) {
        auto const candidate = constructionCandidate(corporation, prices.inputs, plan.targetPrice, false);
        if (candidate.has_value()) {
            std::int64_t const cost = constructionValue(*candidate,
                                                        static_cast<std::int64_t>(prices.inputs.labour),
                                                        static_cast<std::int64_t>(prices.inputs.materials));
            std::int64_t const jumpStartLimit =
                static_cast<std::int64_t>(std::floor(static_cast<double>(corporation.credits) * JUMPSTART_BUDGET));
            if (cost <= jumpStartLimit &&
                actualPlanCost(allocation) + static_cast<double>(cost) <= static_cast<double>(budget)) {
                plan.construction.push_back({corporation.sector, candidate->tier, 0, true, cost});
            }
        }
    } else if (plan.targetQuantity > desiredOutput) {
        auto const candidate = constructionCandidate(corporation, prices.inputs, plan.targetPrice, true);
        if (candidate.has_value()) {
            std::int64_t const cost = constructionValue(*candidate,
                                                        static_cast<std::int64_t>(prices.inputs.labour),
                                                        static_cast<std::int64_t>(prices.inputs.materials));
            std::int64_t const capacity = outputCapacity(*candidate);
            std::int64_t const count = (plan.targetQuantity - desiredOutput + capacity - 1) / capacity;
            std::int64_t const available =
                std::max<std::int64_t>(0, budget - static_cast<std::int64_t>(std::ceil(allocation.modeledCost)));
            std::int64_t const buildCount = cost == 0 ? 0 : std::min(count, available / cost);
            for (std::int64_t index = 0; index < buildCount; ++index) {
                plan.construction.push_back({corporation.sector, candidate->tier, 0, true, cost});
            }
        }
    }

    std::int64_t plannedLabour = maintenanceLabour;
    std::int64_t plannedEnergy{};
    std::int64_t plannedMaterials = maintenanceMaterials;
    std::int64_t plannedFuel{};
    double maximumUsedCost{};
    for (std::size_t methodIndex = 0; methodIndex < methods.size(); ++methodIndex) {
        MethodPlan const &method = methods[methodIndex];
        std::int64_t const methodOutput = allocation.outputByMethod[methodIndex];
        plan.plannedOutput += methodOutput;
        if (methodOutput == 0) {
            continue;
        }
        maximumUsedCost = std::max(maximumUsedCost, method.unitCost);
        plannedLabour += productionLabour(method.definition, methodOutput);
        std::int64_t remainingWorkers = productionLabour(method.definition, methodOutput);
        for (std::size_t const buildingIndex : method.buildings) {
            std::int64_t const workers = std::min(remainingWorkers, method.definition.capacity);
            plan.staffing.push_back({buildingIndex, workers});
            remainingWorkers -= workers;
        }
        if (method.definition.tier == BuildTier::Tier2) {
            std::int64_t const workersNeeded = productionLabour(method.definition, methodOutput);
            std::int64_t const count =
                std::min<std::int64_t>(static_cast<std::int64_t>(method.buildings.size()),
                                       (workersNeeded + method.definition.capacity - 1) / method.definition.capacity);
            plannedEnergy += count * method.definition.energyPerTurnWhenStaffed;
        }
        plannedMaterials += methodOutput * method.definition.materialsPerOutputUnit;
        plannedFuel += methodOutput * method.definition.fuelPerOutputUnit;
    }
    for (std::size_t index = 0; index < corporation.buildings.size(); ++index) {
        auto const found =
            std::find_if(plan.staffing.begin(), plan.staffing.end(), [index](BuildingStaffing const &entry) {
                return entry.buildingIndex == index;
            });
        if (found == plan.staffing.end()) {
            plan.staffing.push_back({index, 0});
        }
    }
    std::sort(
        plan.staffing.begin(), plan.staffing.end(), [](BuildingStaffing const &left, BuildingStaffing const &right) {
            return left.buildingIndex < right.buildingIndex;
        });

    plan.breakEvenPrice =
        maximumUsedCost > 0.0 ? maximumUsedCost : (previousBreakEven > 0.0 ? previousBreakEven : PRICE_FLOOR);
    if (isService(corporation.sector)) {
        plan.breakEvenPrice = prices.inputs.labour;
    }
    if (plan.plannedOutput == 0 && corporation.stockUnitCost > 0.0) {
        plan.breakEvenPrice = corporation.stockUnitCost;
    }
    plan.outputUnitCost =
        plan.plannedOutput > 0 ? allocation.modeledCost / static_cast<double>(plan.plannedOutput) : 0.0;

    if (maintenanceCost > static_cast<double>(corporation.credits)) {
        for (std::size_t index = 0; index < corporation.buildings.size(); ++index) {
            if (!corporation.buildings[index].underConstruction) {
                plan.buildingsDestroyed.push_back(index);
            }
        }
    }
    std::int64_t constructionLabour{};
    std::int64_t constructionMaterials{};
    for (Building const &building : plan.construction) {
        std::vector<BuildingDef> const definitions = availableMethods(building.sector);
        BuildingDef const *definition = definitionForTier(definitions, building.tier);
        if (definition != nullptr) {
            constructionLabour += definition->constructionLabour;
            constructionMaterials += definition->constructionMaterials;
        }
    }
    plannedLabour += constructionLabour;
    plannedMaterials += constructionMaterials;

    std::int64_t const materialBuy =
        std::max<std::int64_t>(0, plannedMaterials - corporation.stock[ResourceId::Materials]);
    std::int64_t const fuelBuy = std::max<std::int64_t>(0, plannedFuel - corporation.stock[ResourceId::Fuel]);
    if (corporation.credits > 0) {
        addOrder(OrderSide::Buy,
                 ResourceId::Labour,
                 roundedPrice(orderPrice(corporation, ResourceId::Labour)),
                 plannedLabour);
        addOrder(OrderSide::Buy,
                 ResourceId::Energy,
                 roundedPrice(orderPrice(corporation, ResourceId::Energy)),
                 plannedEnergy);
        addOrder(OrderSide::Buy,
                 ResourceId::Materials,
                 roundedPrice(orderPrice(corporation, ResourceId::Materials)),
                 materialBuy);
        addOrder(OrderSide::Buy, ResourceId::Fuel, roundedPrice(orderPrice(corporation, ResourceId::Fuel)), fuelBuy);
    }

    ResourceId const outputId = outputResourceId.value();
    std::int64_t sellQuantity = corporation.stock[outputId];
    if (isInstantResource(outputId)) {
        sellQuantity = plan.plannedOutput;
    }

    double const constructionCost = std::accumulate(
        plan.construction.begin(), plan.construction.end(), 0.0, [](double total, Building const &building) {
            return total + static_cast<double>(building.constructionValue);
        });
    plan.plannedCost = static_cast<std::int64_t>(std::ceil(actualPlanCost(allocation) + constructionCost));
    // The worked corporation example books the first manual production run at its
    // target price; subsequent standard-resource production remains next-turn stock.
    std::int64_t const revenueQuantity =
        sellQuantity > 0 ? sellQuantity : (isStandardResource(outputId) ? plan.plannedOutput : sellQuantity);
    std::int64_t const reservationOrderPrice = std::max<std::int64_t>(PRICE_FLOOR, ceilToInt64(plan.breakEvenPrice));
    std::int64_t const revenuePrice = prices.output >= reservationOrderPrice ? prices.output : 0;
    std::int64_t const estimatedRevenue = revenueQuantity * std::max<std::int64_t>(0, revenuePrice);
    plan.endCredits = std::max<std::int64_t>(0, corporation.credits - plan.plannedCost + estimatedRevenue);
    plan.liquidate = corporation.credits == 0 || plan.endCredits == 0;
    if (plan.liquidate) {
        plan.liquidationProceeds = static_cast<std::int64_t>(std::floor(
            std::accumulate(corporation.buildings.begin(),
                            corporation.buildings.end(),
                            0.0,
                            [](double total, Building const &building) {
                                return total + static_cast<double>(building.constructionValue) * SALVAGE_RATE;
                            })));
        for (std::size_t index = 0; index < RESOURCE_COUNT; ++index) {
            ResourceId const resource = static_cast<ResourceId>(index);
            if (isStandardResource(resource) && corporation.stock[resource] > 0) {
                std::int64_t const liquidationPrice =
                    std::max<std::int64_t>(PRICE_FLOOR, prices.resources[static_cast<std::size_t>(resource)]);
                plan.liquidationProceeds += corporation.stock[resource] * liquidationPrice;
                addOrder(OrderSide::Sell, resource, liquidationPrice, corporation.stock[resource]);
            }
        }
    } else {
        addOrder(OrderSide::Sell,
                 outputId,
                 std::max<std::int64_t>(PRICE_FLOOR, ceilToInt64(plan.breakEvenPrice)),
                 sellQuantity);
        plan.dividend = static_cast<std::int64_t>(std::floor(static_cast<double>(plan.endCredits) * DIVIDEND_RATE));
    }
    return plan;
}

} // namespace Simulation::Economy
