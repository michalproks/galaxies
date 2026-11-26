#ifndef GALAXIES_SIMULATION_ECONOMY_ORDER_H
#define GALAXIES_SIMULATION_ECONOMY_ORDER_H

#include "AgentId.h"
#include "Resources.h"

#include <cstdint>

namespace Simulation::Economy {

/** Distinguishes an order offering a resource from one requesting it. */
enum class OrderSide : std::uint8_t {
    Buy,
    Sell,
};

/** A single market instruction; an agent submits at most one per side and resource per turn. */
struct Order {
    /** Unique deterministic identifier within the containing market. */
    OrderId id{};
    /** Tagged household or corporation submitting this order. */
    AgentRef owner{HouseholdId{}};
    /** Whether the owner is buying or selling the selected resource. */
    OrderSide side{OrderSide::Buy};
    /** Market resource being traded; credits are virtual and are not orderable. */
    ResourceId resource{ResourceId::Credits};
    /** Reservation or bid price, in centicredits per resource unit. */
    std::int64_t price{};
    /** Quantity in resource units; standard resources use kilograms and instant resources are dimensionless. */
    std::int64_t quantity{};
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_ORDER_H
