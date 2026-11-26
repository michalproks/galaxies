#include "PriceBelief.h"

#include "EconomyConstants.h"

#include <algorithm>

namespace Simulation::Economy {

PriceBelief updatedPriceBelief(PriceBelief const &belief, double settlementPrice, bool orderFilled) noexcept {
    PriceBelief updated = belief;
    updated.mean += BELIEF_ALPHA * (settlementPrice - belief.mean);
    updated.halfWidth *= orderFilled ? 1.0 - BELIEF_BETA : 1.0 + BELIEF_GAMMA;
    updated.halfWidth = std::clamp(updated.halfWidth, BELIEF_W_MIN, BELIEF_W_MAX);
    return updated;
}

} // namespace Simulation::Economy
