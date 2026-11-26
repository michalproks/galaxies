#include "Market.h"

namespace Simulation::Economy {

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

} // namespace Simulation::Economy
