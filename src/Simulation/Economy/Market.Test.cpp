#include "Market.h"
#include "EconomyConstants.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <numeric>
#include <vector>

namespace {

using Simulation::Economy::AgentRef;
using Simulation::Economy::CorporationId;
using Simulation::Economy::HouseholdId;
using Simulation::Economy::Market;
using Simulation::Economy::Order;
using Simulation::Economy::OrderId;
using Simulation::Economy::OrderSide;
using Simulation::Economy::ResourceId;
using Simulation::Economy::SettlementOutcome;
using Simulation::Economy::Trade;

Order makeOrder(std::int64_t const id, OrderSide const side, std::int64_t const price, std::int64_t const quantity) {
    AgentRef const owner = side == OrderSide::Buy ? AgentRef{HouseholdId{id}} : AgentRef{CorporationId{id}};
    return {OrderId{id}, owner, side, ResourceId::Food, price, quantity};
}

Market makeInvariantMarket() {
    Market market;
    market.lastPriceFor(ResourceId::Food) = 4;
    auto &book = market.orderBookFor(ResourceId::Food);
    book.sellOrders = {makeOrder(11, OrderSide::Sell, 1, 7),
                       makeOrder(12, OrderSide::Sell, 4, 8),
                       makeOrder(13, OrderSide::Sell, 6, 9)};
    book.buyOrders = {makeOrder(21, OrderSide::Buy, 8, 10),
                      makeOrder(22, OrderSide::Buy, 5, 6),
                      makeOrder(23, OrderSide::Buy, 2, 20)};
    return market;
}

bool sameOutcome(SettlementOutcome const &left, SettlementOutcome const &right) {
    if (left.resource != right.resource || left.price != right.price || left.tradedQuantity != right.tradedQuantity ||
        left.trades.size() != right.trades.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.trades.size(); ++index) {
        Trade const &leftTrade = left.trades[index];
        Trade const &rightTrade = right.trades[index];
        if (leftTrade.orderId != rightTrade.orderId || leftTrade.owner != rightTrade.owner ||
            leftTrade.resource != rightTrade.resource || leftTrade.side != rightTrade.side ||
            leftTrade.quantity != rightTrade.quantity || leftTrade.price != rightTrade.price) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("Market settlement matches the economic-model worked example") {
    Market market;
    market.lastPriceFor(ResourceId::Food) = 3;
    auto &book = market.orderBookFor(ResourceId::Food);
    book.sellOrders = {makeOrder(1, OrderSide::Sell, 2, 100),
                       makeOrder(2, OrderSide::Sell, 4, 200),
                       makeOrder(3, OrderSide::Sell, 6, 300)};
    book.buyOrders = {makeOrder(4, OrderSide::Buy, 7, 150),
                      makeOrder(5, OrderSide::Buy, 5, 250),
                      makeOrder(6, OrderSide::Buy, 3, 400)};

    SettlementOutcome const result = market.settle(ResourceId::Food);
    std::map<std::int64_t, std::int64_t> fills;
    std::int64_t buyerPayments = 0;
    std::int64_t sellerReceipts = 0;
    for (Trade const &trade : result.trades) {
        fills.emplace(trade.orderId.value, trade.quantity);
        if (trade.side == OrderSide::Buy) {
            buyerPayments += trade.quantity * trade.price;
        } else {
            sellerReceipts += trade.quantity * trade.price;
        }
    }

    CHECK(result.price == 4);
    CHECK(result.tradedQuantity == 300);
    CHECK(fills[1] == 100);
    CHECK(fills[2] == 200);
    CHECK(fills[3] == 0);
    CHECK(fills[4] == 113);
    CHECK(fills[5] == 187);
    CHECK(fills[6] == 0);
    CHECK(buyerPayments == 1200);
    CHECK(sellerReceipts == 1200);
    CHECK(market.lastPriceFor(ResourceId::Food) == 4);
}

TEST_CASE("Market settlement obeys quantity, price-floor, and determinism invariants") {
    Market firstMarket = makeInvariantMarket();
    Market secondMarket = makeInvariantMarket();

    SettlementOutcome const firstResult = firstMarket.settle(ResourceId::Food);
    SettlementOutcome const secondResult = secondMarket.settle(ResourceId::Food);
    CHECK(sameOutcome(firstResult, secondResult));
    CHECK(firstResult.price == 4);
    CHECK(firstResult.price >= Simulation::Economy::PRICE_FLOOR);

    std::int64_t const executedBuys =
        std::accumulate(firstResult.trades.begin(),
                        firstResult.trades.end(),
                        std::int64_t{0},
                        [](std::int64_t total, Trade const &trade) {
                            return trade.side == OrderSide::Buy ? total + trade.quantity : total;
                        });
    std::int64_t const executedSells =
        std::accumulate(firstResult.trades.begin(),
                        firstResult.trades.end(),
                        std::int64_t{0},
                        [](std::int64_t total, Trade const &trade) {
                            return trade.side == OrderSide::Sell ? total + trade.quantity : total;
                        });
    CHECK(executedBuys == firstResult.tradedQuantity);
    CHECK(executedSells == firstResult.tradedQuantity);

    std::map<std::int64_t, std::int64_t> orderedQuantities;
    for (Order const &order : firstMarket.findOrderBook(ResourceId::Food)->buyOrders) {
        orderedQuantities.emplace(order.id.value, order.quantity);
    }
    for (Order const &order : firstMarket.findOrderBook(ResourceId::Food)->sellOrders) {
        orderedQuantities.emplace(order.id.value, order.quantity);
    }
    for (Trade const &trade : firstResult.trades) {
        CHECK(trade.quantity <= orderedQuantities.at(trade.orderId.value));
    }

    std::int64_t const totalDemandAtPrice = 10 + 6;
    std::int64_t const totalSupplyAtPrice = 7 + 8;
    CHECK(firstResult.tradedQuantity == std::min(totalDemandAtPrice, totalSupplyAtPrice));
}

TEST_CASE("Market settlement preserves the previous price when there is no trade") {
    Market market;
    market.lastPriceFor(ResourceId::Food) = 19;
    auto &book = market.orderBookFor(ResourceId::Food);
    book.buyOrders = {makeOrder(1, OrderSide::Buy, 3, 8)};

    SettlementOutcome const result = market.settle(ResourceId::Food);
    CHECK(result.price == 19);
    CHECK(result.tradedQuantity == 0);
    CHECK(result.trades.empty());
    CHECK(market.lastPriceFor(ResourceId::Food) == 19);
}

TEST_CASE("Market settlement keeps any previous price inside the maximizing interval") {
    Market market;
    market.lastPriceFor(ResourceId::Food) = 5;
    auto &book = market.orderBookFor(ResourceId::Food);
    book.sellOrders = {makeOrder(1, OrderSide::Sell, 2, 100), makeOrder(2, OrderSide::Sell, 4, 200)};
    book.buyOrders = {makeOrder(3, OrderSide::Buy, 7, 150), makeOrder(4, OrderSide::Buy, 5, 250)};

    SettlementOutcome const result = market.settle(ResourceId::Food);
    CHECK(result.price == 5);
    CHECK(result.tradedQuantity == 300);
}

TEST_CASE("Market settlement includes PRICE_FLOOR as a candidate price") {
    Market market;
    auto &book = market.orderBookFor(ResourceId::Food);
    book.sellOrders = {makeOrder(1, OrderSide::Sell, Simulation::Economy::PRICE_FLOOR, 12)};
    book.buyOrders = {makeOrder(2, OrderSide::Buy, Simulation::Economy::PRICE_FLOOR, 9)};

    SettlementOutcome const result = market.settle(ResourceId::Food);
    CHECK(result.price == Simulation::Economy::PRICE_FLOOR);
    CHECK(result.tradedQuantity == 9);
}
