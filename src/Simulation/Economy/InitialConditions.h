#ifndef GALAXIES_SIMULATION_ECONOMY_INITIAL_CONDITIONS_H
#define GALAXIES_SIMULATION_ECONOMY_INITIAL_CONDITIONS_H

#include "ColonyEconomy.h"

#include <cstdint>

namespace Simulation::Economy {

/**
 * Builds the complete fictional turn-zero Earth economy used as the turn-one snapshot.
 * The colony identity and economy seed determine the random household propensities and ownership.
 * The returned state is deterministic for equal arguments and starts at economic turn one.
 */
[[nodiscard]] ColonyEconomy buildEarthSeed(ColonyId colonyId, std::uint64_t economySeed);

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_INITIAL_CONDITIONS_H
