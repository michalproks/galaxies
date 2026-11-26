#ifndef GALAXIES_SIMULATION_ECONOMY_COLONY_TICK_H
#define GALAXIES_SIMULATION_ECONOMY_COLONY_TICK_H

#include "ColonyEconomy.h"
#include "Founding.h"
#include "Market.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Simulation::Economy {

/** Per-household state changes captured for later player/debug records. */
struct HouseholdTickDelta {
    HouseholdId id{};
    std::int64_t credits{};
    std::int64_t population{};
    std::int64_t foodStock{};
    std::int64_t foodEaten{};
    std::int64_t servicesConsumed{};
    std::int64_t consumerGoodsConsumed{};
};

/** Per-corporation state changes captured for later player/debug records. */
struct CorporationTickDelta {
    CorporationId id{};
    std::int64_t credits{};
    std::int64_t produced{};
    std::int64_t sold{};
};

/** Household planning and realized spending/food-reserve outcome for one turn. */
struct HouseholdTickOutcome {
    HouseholdId id{};
    std::int64_t budget{};
    std::int64_t survivalNeed{};
    std::int64_t reserveTarget{};
    std::int64_t reserveGap{};
    std::int64_t emergencyFoodRequested{};
    std::int64_t emergencyFoodFilled{};
    std::int64_t foodBought{};
    std::int64_t foodShare{};
    std::int64_t servicesShare{};
    std::int64_t consumerGoodsShare{};
    std::int64_t consumptionTarget{};
    std::int64_t foodSpent{};
    std::int64_t servicesSpent{};
    std::int64_t consumerGoodsSpent{};
    std::int64_t foodEaten{};
    double reserveTurns{};
    bool emergencyMode{};
};

/** Corporation planning and material-flow result retained from the actual tick. */
struct CorporationTickOutcome {
    CorporationId id{};
    SectorId sector{SectorId::Food};
    CorporationPlanDiagnostic planning;
    std::int64_t materialsConsumed{};
    std::int64_t fuelConsumed{};
    std::int64_t unpaidMaintenanceBuildings{};
    bool maintenancePaid{true};
};

/** Explicit result of one named economy invariant evaluation. */
struct EconomyInvariantResult {
    std::string id;
    bool passed{};
};

/** Corporation removed at the end of the turn, with the value transferred to its owner. */
struct DissolvedCorporation {
    CorporationId id{};
    HouseholdId owner{};
    std::int64_t proceeds{};
};

/** Full deterministic outcome of one colony's economic turn. */
struct ColonyTickResult { // NOLINT(bugprone-exception-escape)
    ColonyEconomy colony;
    /** One outcome per tradable resource, including resources with no trades. */
    std::vector<SettlementOutcome> settlements;
    /** Market demand, fills, and prices produced this turn for the next founding roll. */
    FoundingMarketData marketData;
    /** Successful founding outcomes; these corporations enter the market next turn. */
    std::vector<FoundedCorporation> founded;
    /** Corporations and households removed during the dissolution stage. */
    std::vector<DissolvedCorporation> dissolvedCorporations;
    std::vector<HouseholdId> dissolvedHouseholds;
    /** Agent-level deltas, preserving the iteration order of the input agents. */
    std::vector<HouseholdTickDelta> householdDeltas;
    std::vector<CorporationTickDelta> corporationDeltas;
    /** Credits realized from salvage and owner-dissolution inventory liquidation this turn. */
    std::int64_t liquidationCredits{};
    /** Exact household and corporation outcomes retained for player/debug records. */
    std::vector<HouseholdTickOutcome> householdOutcomes;
    std::vector<CorporationTickOutcome> corporationOutcomes;
    std::vector<EconomyInvariantResult> invariantResults;
    /** Standard-resource inventory removed when a corporation or final household dissolves. */
    std::array<std::int64_t, RESOURCE_COUNT> liquidatedResources{};
};

/**
 * Advances one colony by one economic turn without mutating the input state or global state.
 * The optional prior-market snapshot carries demand and fills from the preceding turn; when
 * omitted, demand is reconstructed from the colony's previous order books and fills default
 * to zero because Market intentionally stores no historical fill ledger.
 */
[[nodiscard]] ColonyTickResult tickColony(ColonyEconomy colony);

/** Advances one colony using the exact prior-turn demand/fill data for founding sector weights. */
[[nodiscard]] ColonyTickResult tickColony(ColonyEconomy colony, FoundingMarketData const &previousMarketData);

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_COLONY_TICK_H
