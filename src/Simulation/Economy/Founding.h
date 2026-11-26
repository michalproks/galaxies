#ifndef GALAXIES_SIMULATION_ECONOMY_FOUNDING_H
#define GALAXIES_SIMULATION_ECONOMY_FOUNDING_H

#include "ColonyEconomy.h"
#include "Rng.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Simulation::Economy {

/** Number of producible corporate sectors, in SectorId order. */
inline constexpr std::size_t SECTOR_COUNT = 6;

/** Demand, fills, and settlement prices from the preceding colony market turn. */
struct FoundingMarketData {
    /** Total buy-order quantities submitted, indexed by ResourceId. */
    std::array<std::int64_t, RESOURCE_COUNT> demand{};
    /** Filled buy-order quantities, indexed by ResourceId. */
    std::array<std::int64_t, RESOURCE_COUNT> filled{};
    /** Settlement prices in centicredits per unit, indexed by ResourceId. */
    std::array<std::int64_t, RESOURCE_COUNT> settlementPrices{};
};

/** Inclusive credit bounds used to calculate household-relative founding chances. */
struct FoundingWealthRange {
    /** Minimum credit balance among all households in the colony. */
    std::int64_t minWealth{};
    /** Maximum credit balance among all households in the colony. */
    std::int64_t maxWealth{};
};

/** The sampled sector's integer weight, in SectorId order. */
using SectorWeights = std::array<std::uint64_t, SECTOR_COUNT>;

/** One corporation founding event; capital is transferred from household to corporation. */
struct FoundedCorporation {
    /** Household that passed eligibility and its founding roll. */
    HouseholdId householdId{};
    /** Binding sector chosen from last turn's weighted unmet demand. */
    SectorId sector{SectorId::Food};
    /** Centicredits transferred from the household to the new corporation. */
    std::int64_t capitalTransfer{};
};

/** Founding events produced for a colony during one turn. */
struct FoundingResult {
    /** Successful founding outcomes in the colony's household iteration order. */
    std::vector<FoundedCorporation> corporations;
};

/**
 * Returns relative-wealth founding probability, including the equal-wealth rule
 * (economic-model.md, Founding). Requires wealthRange.minWealth <= wealthRange.maxWealth.
 */
[[nodiscard]] double foundingProbability(std::int64_t credits, FoundingWealthRange wealthRange) noexcept;

/**
 * Returns the six demand-and-price weights using preceding-turn market data
 * (economic-model.md, Founding). Negative demand, fill, or price entries are treated as zero.
 */
[[nodiscard]] SectorWeights sectorWeightsFor(FoundingMarketData const &marketData) noexcept;

/**
 * Produces eligible households' successful founding rolls and sector assignments without
 * mutating the colony. RNG sub-streams are keyed by household identity, so one household's
 * outcome does not shift another's (economic-model.md, Founding; economy-implementation.md,
 * Determinism). Requires valid, unique household identities and preceding-turn market data.
 */
[[nodiscard]] FoundingResult
rollFoundings(ColonyEconomy const &colony, FoundingMarketData const &marketData, Rng const &rng);

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_FOUNDING_H
