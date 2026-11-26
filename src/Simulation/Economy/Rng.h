#ifndef GALAXIES_SIMULATION_ECONOMY_RNG_H
#define GALAXIES_SIMULATION_ECONOMY_RNG_H

#include "AgentId.h"

#include <array>
#include <cstdint>

namespace Simulation::Economy {

/**
 * Deterministic random stream for one colony and turn, with independently keyed sub-streams.
 */
class Rng {
public:
    /**
     * Creates a reproducible stream derived from the colony identity and turn.
     * The colony identity and turn may be any values; equal inputs always produce equal streams.
     */
    Rng(ColonyId colonyId, std::uint64_t turn);

    /**
     * Returns the next 64-bit value in this stream and advances only this stream.
     */
    std::uint64_t next();

    /**
     * Returns a uniformly distributed double in the half-open interval [0, 1).
     */
    double uniformDouble();

    /**
     * Returns a uniformly distributed integer in the inclusive interval [min, max].
     * Requires min <= max.
     */
    std::uint64_t uniformInt(std::uint64_t min, std::uint64_t max);

    /**
     * Returns true with the given probability in [0, 1].
     * Requires a finite probability in [0, 1].
     */
    bool bernoulli(double probability);

    /**
     * Creates a fresh, reproducible stream keyed by streamId without advancing this stream.
     * The same parent seed and streamId always produce the same sub-stream.
     */
    Rng substream(std::uint64_t streamId) const;

private:
    struct SeedTag {};

    Rng(std::uint64_t seed, SeedTag);

    std::uint64_t mSeed;
    std::array<std::uint64_t, 4> mState;
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_RNG_H
