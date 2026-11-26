#include "DemandCurve.h"

namespace Simulation::Economy {

std::int64_t DemandCurve::quantityAtPrice(double const price) const noexcept {
    std::int64_t quantity{};
    for (auto const &[stepPrice, stepQuantity] : steps) {
        if (static_cast<double>(stepPrice) >= price) {
            quantity += stepQuantity;
        }
    }
    return quantity;
}

std::int64_t DemandCurve::quantityAtPrice(std::int64_t const price) const noexcept {
    std::int64_t quantity{};
    for (auto const &[stepPrice, stepQuantity] : steps) {
        if (stepPrice >= price) {
            quantity += stepQuantity;
        }
    }
    return quantity;
}

} // namespace Simulation::Economy
