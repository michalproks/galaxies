#ifndef GALAXIES_SIMULATION_ECONOMY_CORPORATION_H
#define GALAXIES_SIMULATION_ECONOMY_CORPORATION_H

#include "AgentId.h"
#include "BuildingDef.h"
#include "DemandCurve.h"
#include "PriceBelief.h"
#include "Resources.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Simulation::Economy {

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

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_CORPORATION_H
