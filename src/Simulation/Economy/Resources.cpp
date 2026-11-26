#include "Resources.h"

namespace Simulation::Economy {

std::string_view displayName(ResourceId const resource) noexcept {
    switch (resource) {
    case ResourceId::Credits:
        return "Credits";
    case ResourceId::Labour:
        return "Labour";
    case ResourceId::Energy:
        return "Energy";
    case ResourceId::Food:
        return "Food";
    case ResourceId::Materials:
        return "Materials";
    case ResourceId::Fuel:
        return "Fuel";
    case ResourceId::Service:
        return "Service";
    case ResourceId::ConsumerGoods:
        return "Consumer Goods";
    }
    return "Unknown";
}

std::int64_t &ResourceAmounts::operator[](ResourceId const resource) noexcept {
    return amounts[static_cast<std::size_t>(resource)];
}

std::int64_t const &ResourceAmounts::operator[](ResourceId const resource) const noexcept {
    return amounts[static_cast<std::size_t>(resource)];
}

} // namespace Simulation::Economy
