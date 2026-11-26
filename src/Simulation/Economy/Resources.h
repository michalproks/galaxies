#ifndef GALAXIES_SIMULATION_ECONOMY_RESOURCES_H
#define GALAXIES_SIMULATION_ECONOMY_RESOURCES_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Simulation::Economy {

/** Identifies one of the model's virtual, instant, or storable resources. */
enum class ResourceId : std::uint8_t {
    Credits,
    Labour,
    Energy,
    Food,
    Materials,
    Fuel,
    Service,
    ConsumerGoods,
};

/** Number of resource identifiers, used to size fixed resource arrays. */
inline constexpr std::size_t RESOURCE_COUNT = 8;

/** Reports whether credits are globally available rather than market-traded. */
[[nodiscard]] constexpr bool isVirtualCredit(ResourceId resource) noexcept {
    return resource == ResourceId::Credits;
}

/** Reports whether a resource must be consumed in its production turn. */
[[nodiscard]] constexpr bool isInstantResource(ResourceId resource) noexcept {
    return resource == ResourceId::Labour || resource == ResourceId::Energy || resource == ResourceId::Service;
}

/** Reports whether a resource can be stored between turns. */
[[nodiscard]] constexpr bool isStandardResource(ResourceId resource) noexcept {
    return resource == ResourceId::Food || resource == ResourceId::Materials || resource == ResourceId::Fuel ||
           resource == ResourceId::ConsumerGoods;
}

/** Returns the display label for a resource identifier. */
[[nodiscard]] std::string_view displayName(ResourceId resource) noexcept;

/** Holds signed integer quantities indexed by resource; instant/credit slots are not inventory. */
struct ResourceAmounts {
    /** Quantities in resource order, with standard goods measured in kilograms. */
    std::array<std::int64_t, RESOURCE_COUNT> amounts{};

    /** Returns mutable access to the amount for the supplied resource. */
    [[nodiscard]] std::int64_t &operator[](ResourceId resource) noexcept;

    /** Returns read-only access to the amount for the supplied resource. */
    [[nodiscard]] std::int64_t const &operator[](ResourceId resource) const noexcept;
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_RESOURCES_H
