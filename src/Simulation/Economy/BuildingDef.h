#ifndef GALAXIES_SIMULATION_ECONOMY_BUILDING_DEF_H
#define GALAXIES_SIMULATION_ECONOMY_BUILDING_DEF_H

#include "Sector.h"

#include <cstdint>
#include <vector>

namespace Simulation::Economy {

/** Identifies manual work or a building production tier. */
enum class BuildTier : std::uint8_t {
    Manual,
    Tier1,
    Tier2,
};

/** Static production inputs, output rates, capacity, construction, and upkeep for a method. */
struct BuildingDef {
    /** Sector resource produced by this method. */
    SectorId sector{SectorId::Food};
    /** Method tier; Energy/Tier1 represents the distinct power-plant method. */
    BuildTier tier{BuildTier::Manual};
    /** Maximum workers per building; zero denotes unbounded manual production. */
    std::int64_t capacity{};
    /** Multiplier applied to the sector's manual per-worker output rate. */
    double rateMultiplier{1.0};
    /** Labour units consumed to construct one building. */
    std::int64_t constructionLabour{};
    /** Materials kilograms consumed to construct one building. */
    std::int64_t constructionMaterials{};
    /** Labour units required for upkeep per turn. */
    std::int64_t maintenanceLabour{};
    /** Materials kilograms required for upkeep per turn. */
    std::int64_t maintenanceMaterials{};
    /** Flat energy units required each turn when a Tier2 building is staffed. */
    std::int64_t energyPerTurnWhenStaffed{};
    /** Fuel kilograms required per energy output unit for the power-plant method. */
    std::int64_t fuelPerOutputUnit{};
    /** Materials kilograms required per consumer-good output unit. */
    std::int64_t materialsPerOutputUnit{};
    /** Base output units per manual worker, or per power-plant worker for Energy. */
    std::int64_t outputPerManualWorker{};
};

/** Returns the available production methods for a sector in the model's production table. */
[[nodiscard]] std::vector<BuildingDef> availableMethods(SectorId sector);

/** Values one building's labour and material construction inputs in centicredits. */
[[nodiscard]] std::int64_t
constructionValue(BuildingDef const &definition, std::int64_t labourPrice, std::int64_t materialsPrice) noexcept;

/** Last-settlement input prices used to derive unit production costs. */
struct InputPrices {
    double labour{0.0};
    double materials{0.0};
    double energy{0.0};
    double fuel{0.0};
};

/** Calculates per-output centicredit cost using the model's labour, input, and flat-energy formulas. */
[[nodiscard]] double unitCostCenticreditsPerOutput(SectorId sector, BuildTier tier, InputPrices prices);

/** A corporation-owned building; destroyed buildings are removed from the owning vector. */
struct Building {
    /** Sector output fixed for the lifetime of the building. */
    SectorId sector{SectorId::Food};
    /** Production tier used to look up this building's static definition. */
    BuildTier tier{BuildTier::Manual};
    /** Workers assigned this turn; zero means idle and requires no Tier2 energy. */
    std::int64_t staffedWorkers{};
    /** True until the end of the turn construction starts; usable production begins next turn. */
    bool underConstruction{};
    /** Construction cost in centicredits at construction-turn settlement prices, used for salvage. */
    std::int64_t constructionValue{};
};

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_BUILDING_DEF_H
