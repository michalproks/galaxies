#include "Market.h"

#include "EconomyConstants.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Simulation::Economy {
namespace {

    /** Adds quantities while rejecting books whose totals cannot be represented. */
    std::int64_t checkedAdd(std::int64_t const left, std::int64_t const right) {
        if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) {
            throw std::overflow_error("Market quantity total exceeds int64_t");
        }
        if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right) {
            throw std::overflow_error("Market quantity total exceeds int64_t");
        }
        return left + right;
    }

    /**
     * Computes (multiplicand * multiplier) / divisor and its remainder without
     * overflowing int64_t. The market's rationing inputs satisfy both factors <= divisor.
     */
    struct RatioProduct {
        std::int64_t multiplicand{};
        std::int64_t multiplier{};
        std::int64_t divisor{};
    };

    std::pair<std::int64_t, std::int64_t> multiplyDivide(RatioProduct const product) {
        std::uint64_t const multiplier = static_cast<std::uint64_t>(product.multiplier);
        std::int64_t quotient = 0;
        std::int64_t remainder = 0;
        for (int bit = 62; bit >= 0; --bit) {
            quotient *= 2;
            std::int64_t carry = 0;
            std::uint64_t const bitMask = std::uint64_t{1} << static_cast<unsigned int>(bit);
            if (remainder >= product.divisor - remainder) {
                remainder -= product.divisor - remainder;
                carry = 1;
            } else {
                remainder += remainder;
            }
            if ((multiplier & bitMask) != 0) {
                if (product.multiplicand == product.divisor) {
                    ++carry;
                } else if (remainder >= product.divisor - product.multiplicand) {
                    remainder -= product.divisor - product.multiplicand;
                    ++carry;
                } else {
                    remainder += product.multiplicand;
                }
            }
            quotient += carry;
        }
        return {quotient, remainder};
    }

    struct RationedOrder {
        Order const *order{};
        std::int64_t fill{};
        std::int64_t remainder{};
    };

    /** Appends positive fills, distributing an excess side by largest remainder. */
    void appendFills(std::vector<Trade> &trades,
                     std::vector<Order const *> const &eligible,
                     std::int64_t const totalQuantity,
                     std::int64_t const tradedQuantity,
                     bool const isRationed,
                     ResourceId const resource,
                     std::int64_t const price,
                     OrderSide const side) {
        if (eligible.empty()) {
            return;
        }
        if (!isRationed) {
            for (Order const *const order : eligible) {
                trades.push_back({order->id, order->owner, resource, side, order->quantity, price});
            }
            return;
        }

        std::vector<RationedOrder> fills;
        fills.reserve(eligible.size());
        std::int64_t baseTotal = 0;
        for (Order const *const order : eligible) {
            auto const [base, remainder] = multiplyDivide({order->quantity, tradedQuantity, totalQuantity});
            fills.push_back({order, base, remainder});
            baseTotal = checkedAdd(baseTotal, base);
        }

        std::int64_t const remaining = tradedQuantity - baseTotal;
        std::vector<std::size_t> priority(fills.size());
        for (std::size_t index = 0; index < priority.size(); ++index) {
            priority[index] = index;
        }
        std::sort(priority.begin(), priority.end(), [&fills](std::size_t const left, std::size_t const right) {
            if (fills[left].remainder != fills[right].remainder) {
                return fills[left].remainder > fills[right].remainder;
            }
            return fills[left].order->id < fills[right].order->id;
        });
        for (std::int64_t index = 0; index < remaining; ++index) {
            ++fills[priority[static_cast<std::size_t>(index)]].fill;
        }
        for (RationedOrder const &fill : fills) {
            if (fill.fill > 0) {
                trades.push_back({fill.order->id, fill.order->owner, resource, side, fill.fill, price});
            }
        }
    }

} // namespace

OrderBook &Market::orderBookFor(ResourceId const resource) {
    return orderBooks[resource];
}

OrderBook const *Market::findOrderBook(ResourceId const resource) const noexcept {
    auto const iterator = orderBooks.find(resource);
    return iterator == orderBooks.end() ? nullptr : &iterator->second;
}

std::int64_t &Market::lastPriceFor(ResourceId const resource) {
    return lastPrice[resource];
}

std::int64_t Market::lastPriceFor(ResourceId const resource) const noexcept {
    auto const iterator = lastPrice.find(resource);
    return iterator == lastPrice.end() ? 0 : iterator->second;
}

SettlementOutcome Market::settle(ResourceId const resource) {
    std::int64_t const previousPrice = lastPriceFor(resource);
    SettlementOutcome outcome;
    outcome.resource = resource;
    outcome.price = previousPrice;
    outcome.priorPrice = previousPrice;
    OrderBook const *const book = findOrderBook(resource);
    if (book == nullptr) {
        return outcome;
    }

    std::vector<Order const *> buyers;
    std::vector<Order const *> sellers;
    std::vector<std::int64_t> candidates{PRICE_FLOOR};
    buyers.reserve(book->buyOrders.size());
    sellers.reserve(book->sellOrders.size());
    candidates.reserve(1 + book->buyOrders.size() + book->sellOrders.size());
    for (Order const &order : book->buyOrders) {
        if (order.resource == resource && order.quantity > 0) {
            buyers.push_back(&order);
            candidates.push_back(order.price);
        }
    }
    for (Order const &order : book->sellOrders) {
        if (order.resource == resource && order.quantity > 0) {
            sellers.push_back(&order);
            candidates.push_back(order.price);
        }
    }
    if (buyers.empty() || sellers.empty()) {
        return outcome;
    }

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    outcome.candidateEvaluations.reserve(std::min(candidates.size(), MAX_MARKET_TRACE_CANDIDATES));
    std::sort(sellers.begin(), sellers.end(), [](Order const *const left, Order const *const right) {
        if (left->price != right->price) {
            return left->price < right->price;
        }
        return left->id < right->id;
    });
    std::sort(buyers.begin(), buyers.end(), [](Order const *const left, Order const *const right) {
        if (left->price != right->price) {
            return left->price < right->price;
        }
        return left->id < right->id;
    });

    std::int64_t supply = 0;
    std::int64_t demand = 0;
    for (Order const *const buyer : buyers) {
        demand = checkedAdd(demand, buyer->quantity);
    }
    std::size_t sellerIndex = 0;
    std::size_t buyerIndex = 0;
    std::int64_t maximumQuantity = 0;
    std::int64_t lowestMaximizingPrice = PRICE_FLOOR;
    std::int64_t highestMaximizingPrice = PRICE_FLOOR;
    bool hasMaximum = false;
    for (std::int64_t const candidate : candidates) {
        while (sellerIndex < sellers.size() && sellers[sellerIndex]->price <= candidate) {
            supply = checkedAdd(supply, sellers[sellerIndex]->quantity);
            ++sellerIndex;
        }
        while (buyerIndex < buyers.size() && buyers[buyerIndex]->price < candidate) {
            demand -= buyers[buyerIndex]->quantity;
            ++buyerIndex;
        }
        std::int64_t const traded = std::min(supply, demand);
        if (outcome.candidateEvaluations.size() < MAX_MARKET_TRACE_CANDIDATES) {
            outcome.candidateEvaluations.push_back({candidate, supply, demand, traded});
        }
        if (!hasMaximum || traded > maximumQuantity) {
            maximumQuantity = traded;
            lowestMaximizingPrice = candidate;
            highestMaximizingPrice = candidate;
            hasMaximum = true;
        } else if (traded == maximumQuantity) {
            highestMaximizingPrice = candidate;
        }
    }
    if (maximumQuantity == 0) {
        return outcome;
    }
    outcome.maximizingPriceLow = lowestMaximizingPrice;
    outcome.maximizingPriceHigh = highestMaximizingPrice;

    // Q(p) is constant between adjacent candidate prices, so the prior price may itself
    // be a maximizer even when it is not one of the submitted order prices.
    std::int64_t const settlementPrice = std::clamp(previousPrice, lowestMaximizingPrice, highestMaximizingPrice);
    outcome.priceChoice = settlementPrice == previousPrice           ? SettlementPriceChoice::PRIOR_PRICE
                          : settlementPrice == lowestMaximizingPrice ? SettlementPriceChoice::LOWEST_MAXIMIZER
                                                                     : SettlementPriceChoice::HIGHEST_MAXIMIZER;
    std::vector<Order const *> eligibleBuyers;
    std::vector<Order const *> eligibleSellers;
    std::int64_t eligibleDemand = 0;
    std::int64_t eligibleSupply = 0;
    for (Order const *const buyer : buyers) {
        if (buyer->price >= settlementPrice) {
            eligibleBuyers.push_back(buyer);
            eligibleDemand = checkedAdd(eligibleDemand, buyer->quantity);
        }
    }
    for (Order const *const seller : sellers) {
        if (seller->price <= settlementPrice) {
            eligibleSellers.push_back(seller);
            eligibleSupply = checkedAdd(eligibleSupply, seller->quantity);
        }
    }
    std::int64_t const tradedQuantity = std::min(eligibleSupply, eligibleDemand);
    appendFills(outcome.trades,
                eligibleBuyers,
                eligibleDemand,
                tradedQuantity,
                eligibleDemand > eligibleSupply,
                resource,
                settlementPrice,
                OrderSide::Buy);
    appendFills(outcome.trades,
                eligibleSellers,
                eligibleSupply,
                tradedQuantity,
                eligibleSupply > eligibleDemand,
                resource,
                settlementPrice,
                OrderSide::Sell);
    std::sort(outcome.trades.begin(), outcome.trades.end(), [](Trade const &left, Trade const &right) {
        return left.orderId < right.orderId;
    });
    outcome.price = settlementPrice;
    outcome.tradedQuantity = tradedQuantity;
    lastPrice[resource] = settlementPrice;
    return outcome;
}

} // namespace Simulation::Economy
