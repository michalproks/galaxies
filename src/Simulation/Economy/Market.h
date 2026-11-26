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

/** One order's executed quantity at the resource's settlement price. */
struct Trade {
    /** Identifier of the submitted order. */
    OrderId orderId{};
    /** Tagged household or corporation that owns the order. */
    AgentRef owner{HouseholdId{}};
    /** Resource exchanged by this order. */
    ResourceId resource{ResourceId::Credits};
    /** Side of the market on which the resource quantity was executed. */
    OrderSide side{OrderSide::Buy};
    /** Executed resource units, after rationing. */
    std::int64_t quantity{};
    /** Settlement price in centicredits per resource unit. */
    std::int64_t price{};
};

/** Result of settling one resource order book. */
struct SettlementOutcome {
    /** Resource whose order book was settled. */
    ResourceId resource{ResourceId::Credits};
    /** Selected settlement price in centicredits per resource unit. */
    std::int64_t price{};
    /** Total resource quantity exchanged on each side. */
    std::int64_t tradedQuantity{};
    /** Orders with positive executed quantities, in ascending order-id order. */
    std::vector<Trade> trades;
};

/** Colony-local order books and remembered per-resource settlement prices. */
struct Market { // NOLINT(bugprone-exception-escape)
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

    /**
     * Selects a price maximizing traded quantity, applies deterministic pro-rata rationing,
     * and updates the remembered price only when a trade occurs. Submitted order quantities
     * and aggregate quantities must fit in int64_t; no agent balances or inventories are changed.
     * Throws std::overflow_error if an aggregate order quantity cannot be represented.
     */
    [[nodiscard]] SettlementOutcome settle(ResourceId resource);
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_MARKET_H
