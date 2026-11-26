#ifndef GALAXIES_SIMULATION_ECONOMY_AGENT_ID_H
#define GALAXIES_SIMULATION_ECONOMY_AGENT_ID_H

#include <compare>
#include <cstdint>
#include <variant>

namespace Simulation::Economy {

/** Identifies a household; -1 is the invalid sentinel and non-negative values are valid ids. */
struct HouseholdId {
    /** Stable numeric identity, defaulting to the invalid sentinel. */
    std::int64_t value{-1};

    /** Compares household identities by their numeric value. */
    auto operator<=>(HouseholdId const &) const = default;
};

/** Identifies a corporation; -1 is the invalid sentinel and non-negative values are valid ids. */
struct CorporationId {
    /** Stable numeric identity, defaulting to the invalid sentinel. */
    std::int64_t value{-1};

    /** Compares corporation identities by their numeric value. */
    auto operator<=>(CorporationId const &) const = default;
};

/** Identifies a colony; -1 is the invalid sentinel and non-negative values are valid ids. */
struct ColonyId {
    /** Stable numeric identity, defaulting to the invalid sentinel. */
    std::int64_t value{-1};

    /** Compares colony identities by their numeric value. */
    auto operator<=>(ColonyId const &) const = default;
};

/** Identifies an order within its market; -1 is invalid and non-negative values are valid ids. */
struct OrderId {
    /** Stable numeric identity, defaulting to the invalid sentinel. */
    std::int64_t value{-1};

    /** Compares order identities by their numeric value. */
    auto operator<=>(OrderId const &) const = default;
};

/** Tags a market order owner as either a household or a corporation. */
using AgentRef = std::variant<HouseholdId, CorporationId>;

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_AGENT_ID_H
