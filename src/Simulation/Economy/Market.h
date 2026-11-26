#ifndef GALAXIES_SIMULATION_ECONOMY_MARKET_H
#define GALAXIES_SIMULATION_ECONOMY_MARKET_H

#include "Order.h"

#include <cstdint>
#include <map>
#include <vector>

namespace Simulation::Economy {

/** Buy and sell sides of one resource's order book, without settlement behavior. */
struct OrderBook {
    /** Submitted buy orders for the resource. */
    std::vector<Order> buyOrders;
    /** Submitted sell orders for the resource. */
    std::vector<Order> sellOrders;
};

/** Colony-local order books and remembered per-resource settlement prices. */
struct Market {
    /** Current turn's order book, created on access for any traded resource. */
    std::map<ResourceId, OrderBook> orderBooks;
    /** Previous settlement prices in centicredits per unit; credits are not traded. */
    std::map<ResourceId, std::int64_t> lastPrice;

    /** Returns a mutable order book, inserting an empty book if none exists yet. */
    [[nodiscard]] OrderBook &orderBookFor(ResourceId resource);

    /** Returns the existing read-only order book, or nullptr if this resource has no book. */
    [[nodiscard]] OrderBook const *findOrderBook(ResourceId resource) const noexcept;

    /** Returns mutable access to a resource's previous settlement price, inserting zero if absent. */
    [[nodiscard]] std::int64_t &lastPriceFor(ResourceId resource);

    /** Returns the previous settlement price, or zero when no price has been recorded. */
    [[nodiscard]] std::int64_t lastPriceFor(ResourceId resource) const noexcept;
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_MARKET_H
