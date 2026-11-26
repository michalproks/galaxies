#ifndef GALAXIES_SIMULATION_ECONOMY_CORPORATION_H
#define GALAXIES_SIMULATION_ECONOMY_CORPORATION_H

#include "AgentId.h"
#include "BuildingDef.h"
#include "DemandCurve.h"
#include "Order.h"
#include "PriceBelief.h"
#include "Resources.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace Simulation::Economy {

/** Maximum demand-curve steps and per-building staffing decisions retained per planning trace. */
inline constexpr std::size_t MAX_PLANNING_DEMAND_STEPS = 64;
inline constexpr std::size_t MAX_PLANNING_STAFFING_ENTRIES = 64;

/** A single-sector company owned by exactly one household; each household owns at most one. */
struct Corporation {
    /** Stable identity used for deterministic order generation. */
    CorporationId id{};
    /** Sole household owner; corporations are never ownerless. */
    HouseholdId owner{};
    /** Fixed output sector that determines all production and sales. */
    SectorId sector{SectorId::Food};
    /** Global centicredit operating balance, including retained profits. */
    std::int64_t credits{};
    /** Buildings owned by this corporation; destroyed buildings are erased from this vector. */
    std::vector<Building> buildings;
    /** Current resource inventory; only standard-resource entries persist between turns. */
    ResourceAmounts stock{};
    /** Average centicredit cost basis per unit of the corporation's sector stock. */
    double stockUnitCost{};
    /** Previous buy book for the corporation's sector output, used to estimate monopoly demand. */
    DemandCurve sectorDemand;
    /** Beliefs for prices of all resources the corporation may buy or sell. */
    std::array<PriceBelief, RESOURCE_COUNT> priceBeliefs{};
    /** Last settled price of the corporation's output, used for tie-breaking and history. */
    std::int64_t lastSettlementPrice{};
    /** Cached break-even price of the most expensive production method currently planned. */
    double breakEvenPrice{};
    /** Quantity of sector output produced in the preceding production phase. */
    std::int64_t lastProducedQuantity{};
    /** True after the corporation has completed construction of its first building. */
    bool hasHadFirstBuilding{};
    /** True once the one-time first-building jump-start exception has been used. */
    bool jumpStartUsed{};
    /** Credits received from sales in the preceding turn. */
    std::int64_t lastIncome{};
};

/** Current settlement prices used for cost, affordability, and construction decisions. */
struct CorporationPrices {
    /** Current prices for labour, materials, energy, and fuel. */
    InputPrices inputs;
    /** Current settlement price of the corporation's output. */
    std::int64_t output{};
    /** Current settlement prices indexed by ResourceId, used during liquidation. */
    std::array<std::int64_t, RESOURCE_COUNT> resources{};
    /** First order identifier assigned to this corporation's plan. */
    OrderId firstOrderId{};
};

/** Workers assigned to one existing building for the current production turn. */
struct BuildingStaffing {
    /** Index of the building in Corporation::buildings. */
    std::size_t buildingIndex{};
    /** Workers assigned; zero means idle. */
    std::int64_t workers{};
};

/** Per-method cost and capacity values shown by the developer planning inspector. */
struct CorporationMethodDiagnostic {
    BuildTier tier{BuildTier::Manual};
    double unitCost{};
    std::int64_t outputCapacity{};
};

/** Planning decisions copied from the exact values calculated by planCorporation. */
struct CorporationPlanDiagnostic {
    std::vector<std::pair<std::int64_t, std::int64_t>> demandSteps;
    std::vector<CorporationMethodDiagnostic> methods;
    std::vector<BuildingStaffing> staffing;
    bool demandStepsTruncated{};
    bool staffingTruncated{};
    std::int64_t targetPrice{};
    std::int64_t targetQuantity{};
    double targetProfit{};
    double breakEvenPrice{};
    std::int64_t budget{};
    std::int64_t plannedOutput{};
    std::int64_t plannedCost{};
    std::int64_t idleOutputCapacity{};
    std::int64_t maintenanceLabour{};
    std::int64_t maintenanceMaterials{};
    bool surplusDecision{};
    bool constructionGatePassed{};
    bool budgetClampFired{};
};

/** Purely computed corporation decisions for one economic turn. */
struct CorporationPlan {
    /** Market buy and sell orders generated from this plan. */
    std::vector<Order> orders;
    /** Workers assigned to existing buildings, in corporation building order. */
    std::vector<BuildingStaffing> staffing;
    /** Buildings started this turn; they are usable starting next turn. */
    std::vector<Building> construction;
    /** Existing buildings that cannot pay maintenance and are destroyed at turn end. */
    std::vector<std::size_t> buildingsDestroyed;
    /** Profit-maximizing price from the demand-curve sweep. */
    std::int64_t targetPrice{};
    /** Profit-maximizing quantity before budget and staffing constraints. */
    std::int64_t targetQuantity{};
    /** Profit at the selected price and quantity before operating/maintenance costs. */
    double targetProfit{};
    /** Quantity actually planned for production after budget constraints. */
    std::int64_t plannedOutput{};
    /** Reservation price for the output sell order before integer market-price rounding. */
    double breakEvenPrice{};
    /** Average planned production cost per output unit for future stock accounting. */
    double outputUnitCost{};
    /** Total estimated cost of production, maintenance, and construction orders. */
    std::int64_t plannedCost{};
    /** End-of-turn corporate balance before dividends. */
    std::int64_t endCredits{};
    /** Credits transferred to the owner as dividends. */
    std::int64_t dividend{};
    /** Salvage and liquidation proceeds to transfer to the owner. */
    std::int64_t liquidationProceeds{};
    /** True when the end-of-turn balance is zero and the corporation is liquidated. */
    bool liquidate{};
    /** Exact intermediate planning values retained for diagnostics and player details. */
    CorporationPlanDiagnostic diagnostic;
};

/**
 * Selects production, staffing, construction, and market orders for a corporation.
 * The corporation and its inventory are not mutated. Settlement prices are used for planning;
 * input buy orders use the corporation's belief means. See economic-model.md, Production,
 * Order Submission, Income, and Dissolution.
 */
[[nodiscard]] CorporationPlan planCorporation(Corporation const &corporation, CorporationPrices const &prices);

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_CORPORATION_H
