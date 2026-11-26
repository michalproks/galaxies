#ifndef GALAXIES_SIMULATION_ECONOMY_COLONY_ECONOMY_H
#define GALAXIES_SIMULATION_ECONOMY_COLONY_ECONOMY_H

#include "AgentId.h"
#include "Corporation.h"
#include "Household.h"
#include "Market.h"

#include <cstdint>
#include <vector>

namespace Simulation::Economy {

/** Complete serializable colony economy state consumed and produced by a pure future tick. */
struct ColonyEconomy { // NOLINT(bugprone-exception-escape)
    /** Stable colony identity used to derive its deterministic random stream. */
    ColonyId id{};
    /** First settled economic turn is one; turn zero exists only for fictional seed prices. */
    std::int64_t turnNumber{1};
    /** All resident household balances, population, reserves, and beliefs. */
    std::vector<Household> households;
    /** All resident corporations, buildings, inventories, and beliefs. */
    std::vector<Corporation> corporations;
    /** Colony-local order books and last settlement prices. */
    Market market;
    /** Cached sum of resident household populations for colony-level economic calculations. */
    std::int64_t totalPopulation{};
    /** Base seed for this colony's RNG stream; future turns derive streams from id and turnNumber. */
    std::uint64_t economySeed{};
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_COLONY_ECONOMY_H
