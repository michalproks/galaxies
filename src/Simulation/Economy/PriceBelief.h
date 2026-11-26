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

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_PRICE_BELIEF_H
