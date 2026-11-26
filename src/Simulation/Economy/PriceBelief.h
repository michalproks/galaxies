#ifndef GALAXIES_SIMULATION_ECONOMY_PRICE_BELIEF_H
#define GALAXIES_SIMULATION_ECONOMY_PRICE_BELIEF_H

namespace Simulation::Economy {

/** Stores a price estimate and confidence range used to sample willingness-to-pay. */
struct PriceBelief {
    /** Expected centicredit price per unit; sampling is centered on this value. */
    double mean{};
    /** Half-width of the confidence interval, so sampled prices lie around mean ± halfWidth. */
    double halfWidth{};
};

/**
 * Returns a belief updated toward the last settlement price.
 * The mean moves by BELIEF_ALPHA; the half-width contracts when filled and expands otherwise,
 * clamped to [BELIEF_W_MIN, BELIEF_W_MAX] (economic-model.md, Price Beliefs and Order Submission).
 * settlementPrice must be finite.
 */
[[nodiscard]] PriceBelief
updatedPriceBelief(PriceBelief const &belief, double settlementPrice, bool orderFilled) noexcept;

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_PRICE_BELIEF_H
