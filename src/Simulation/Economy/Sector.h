#ifndef GALAXIES_SIMULATION_ECONOMY_SECTOR_H
#define GALAXIES_SIMULATION_ECONOMY_SECTOR_H

#include "Resources.h"

#include <cstdint>
#include <optional>

namespace Simulation::Economy {

/** Selects the single output industry assigned to a corporation. */
enum class SectorId : std::uint8_t {
    Food,
    Materials,
    Fuel,
    Energy,
    Service,
    ConsumerGoods,
};

/** Maps a valid corporation sector to its output resource; invalid enum values yield nullopt. */
[[nodiscard]] constexpr std::optional<ResourceId> outputResource(SectorId sector) noexcept {
    switch (sector) {
    case SectorId::Food:
        return ResourceId::Food;
    case SectorId::Materials:
        return ResourceId::Materials;
    case SectorId::Fuel:
        return ResourceId::Fuel;
    case SectorId::Energy:
        return ResourceId::Energy;
    case SectorId::Service:
        return ResourceId::Service;
    case SectorId::ConsumerGoods:
        return ResourceId::ConsumerGoods;
    }
    return std::nullopt;
}

/** Reports whether the sector produces instant services without buildings. */
[[nodiscard]] constexpr bool isService(SectorId sector) noexcept {
    return sector == SectorId::Service;
}

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_SECTOR_H
