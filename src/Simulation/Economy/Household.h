#ifndef GALAXIES_SIMULATION_ECONOMY_HOUSEHOLD_H
#define GALAXIES_SIMULATION_ECONOMY_HOUSEHOLD_H

#include "AgentId.h"
#include "EconomyConstants.h"
#include "PriceBelief.h"

#include <array>
#include <cstdint>

namespace Simulation::Economy {

/** A population slice with its own credits, food reserves, spending propensities, and beliefs. */
struct Household {
    /** Stable identity used for deterministic order generation and ownership. */
    HouseholdId id{};
    /** Number of individuals; labour, survival needs, and comfort scale with this value. */
    std::int64_t population{};
    /** Global centicredit balance used to clamp all household buy orders. */
    std::int64_t credits{};
    /** Fraction of prior-turn income the household plans to spend. */
    double propIncome{};
    /** Fraction of wealth beyond prior-turn income the household plans to spend. */
    double propSavings{};
    /** Stored food kilograms used first to meet survival needs and reserve targets. */
    std::int64_t foodStock{};
    /** Belief used to price food demand and derive reserve cheapness. */
    PriceBelief foodBelief{};
    /** Belief used to price the household's full labour supply. */
    PriceBelief labourBelief{};
    /** Belief used to price instant service demand. */
    PriceBelief serviceBelief{};
    /** Belief used to price consumer-goods demand. */
    PriceBelief consumerGoodsBelief{};
    /** Rolling prices used to compute the smoothed subsistence wage. */
    std::array<std::int64_t, static_cast<std::size_t>(WAGE_SMOOTH)> foodPriceHistory{};
    /** Credits received from labour and dividends in the preceding turn. */
    std::int64_t lastIncome{};
    /** Last food settlement price, used to calculate emergency bids and reserve cheapness. */
    std::int64_t lastFoodPrice{};
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_HOUSEHOLD_H
