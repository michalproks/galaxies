#ifndef GALAXIES_SIMULATION_ECONOMY_DEMAND_CURVE_H
#define GALAXIES_SIMULATION_ECONOMY_DEMAND_CURVE_H

#include <cstdint>
#include <utility>
#include <vector>

namespace Simulation::Economy {

/** Last turn's buy book for one resource, aggregated into descending-price demand steps. */
struct DemandCurve {
    /** (price in centicredits, quantity) steps sorted by descending price. */
    std::vector<std::pair<std::int64_t, std::int64_t>> steps;

    /** Returns cumulative demand at or above a floating-point centicredit price. */
    [[nodiscard]] std::int64_t quantityAtPrice(double price) const noexcept;

    /** Returns cumulative demand at or above an integer centicredit price. */
    [[nodiscard]] std::int64_t quantityAtPrice(std::int64_t price) const noexcept;
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_DEMAND_CURVE_H
