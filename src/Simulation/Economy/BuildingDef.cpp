#include "BuildingDef.h"

#include <stdexcept>

namespace Simulation::Economy {
namespace {

    constexpr BuildingDef MANUAL_FOOD{SectorId::Food, BuildTier::Manual, 0, 1.0, 0, 0, 0, 0, 0, 0, 0, 12};
    constexpr BuildingDef MANUAL_MATERIALS{SectorId::Materials, BuildTier::Manual, 0, 1.0, 0, 0, 0, 0, 0, 0, 0, 2};
    constexpr BuildingDef MANUAL_FUEL{SectorId::Fuel, BuildTier::Manual, 0, 1.0, 0, 0, 0, 0, 0, 0, 0, 1};
    constexpr BuildingDef MANUAL_SERVICE{SectorId::Service, BuildTier::Manual, 0, 1.0, 0, 0, 0, 0, 0, 0, 0, 1};
    constexpr BuildingDef MANUAL_CONSUMER_GOODS{
        SectorId::ConsumerGoods, BuildTier::Manual, 0, 1.0, 0, 0, 0, 0, 0, 0, 1, 1};
    constexpr BuildingDef MANUAL_ENERGY{
        SectorId::Energy, BuildTier::Tier1, 100'000, 1.0, 300'000, 300'000, 15'000, 10'000, 0, 1, 0, 10};

    constexpr BuildingDef TIER1_FOOD{
        SectorId::Food, BuildTier::Tier1, 100'000, 2.0, 200'000, 100'000, 10'000, 5'000, 0, 0, 0, 12};
    constexpr BuildingDef TIER1_MATERIALS{
        SectorId::Materials, BuildTier::Tier1, 100'000, 2.0, 200'000, 100'000, 10'000, 5'000, 0, 0, 0, 2};
    constexpr BuildingDef TIER1_FUEL{
        SectorId::Fuel, BuildTier::Tier1, 100'000, 2.0, 200'000, 100'000, 10'000, 5'000, 0, 0, 0, 1};
    constexpr BuildingDef TIER1_CONSUMER_GOODS{
        SectorId::ConsumerGoods, BuildTier::Tier1, 100'000, 2.0, 200'000, 100'000, 10'000, 5'000, 0, 0, 1, 1};

    constexpr BuildingDef TIER2_FOOD{
        SectorId::Food, BuildTier::Tier2, 200'000, 5.0, 500'000, 400'000, 20'000, 15'000, 20'000, 0, 0, 12};
    constexpr BuildingDef TIER2_MATERIALS{
        SectorId::Materials, BuildTier::Tier2, 200'000, 5.0, 500'000, 400'000, 20'000, 15'000, 20'000, 0, 0, 2};
    constexpr BuildingDef TIER2_CONSUMER_GOODS{
        SectorId::ConsumerGoods, BuildTier::Tier2, 200'000, 5.0, 500'000, 400'000, 20'000, 15'000, 20'000, 0, 1, 1};

    [[nodiscard]] BuildingDef const &definitionFor(SectorId const sector, BuildTier const tier) {
        switch (sector) {
        case SectorId::Food:
            if (tier == BuildTier::Manual)
                return MANUAL_FOOD;
            if (tier == BuildTier::Tier1)
                return TIER1_FOOD;
            if (tier == BuildTier::Tier2)
                return TIER2_FOOD;
            break;
        case SectorId::Materials:
            if (tier == BuildTier::Manual)
                return MANUAL_MATERIALS;
            if (tier == BuildTier::Tier1)
                return TIER1_MATERIALS;
            if (tier == BuildTier::Tier2)
                return TIER2_MATERIALS;
            break;
        case SectorId::Fuel:
            if (tier == BuildTier::Manual)
                return MANUAL_FUEL;
            if (tier == BuildTier::Tier1)
                return TIER1_FUEL;
            break;
        case SectorId::Energy:
            if (tier == BuildTier::Tier1)
                return MANUAL_ENERGY;
            break;
        case SectorId::Service:
            if (tier == BuildTier::Manual)
                return MANUAL_SERVICE;
            break;
        case SectorId::ConsumerGoods:
            if (tier == BuildTier::Manual)
                return MANUAL_CONSUMER_GOODS;
            if (tier == BuildTier::Tier1)
                return TIER1_CONSUMER_GOODS;
            if (tier == BuildTier::Tier2)
                return TIER2_CONSUMER_GOODS;
            break;
        }
        throw std::invalid_argument("No production method exists for this sector and tier.");
    }

} // namespace

std::vector<BuildingDef> availableMethods(SectorId const sector) {
    switch (sector) {
    case SectorId::Food:
        return {MANUAL_FOOD, TIER1_FOOD, TIER2_FOOD};
    case SectorId::Materials:
        return {MANUAL_MATERIALS, TIER1_MATERIALS, TIER2_MATERIALS};
    case SectorId::Fuel:
        return {MANUAL_FUEL, TIER1_FUEL};
    case SectorId::Energy:
        return {MANUAL_ENERGY};
    case SectorId::Service:
        return {MANUAL_SERVICE};
    case SectorId::ConsumerGoods:
        return {MANUAL_CONSUMER_GOODS, TIER1_CONSUMER_GOODS, TIER2_CONSUMER_GOODS};
    }
    return {};
}

std::int64_t constructionValue(BuildingDef const &definition,
                               std::int64_t const labourPrice,
                               std::int64_t const materialsPrice) noexcept {
    return definition.constructionLabour * labourPrice + definition.constructionMaterials * materialsPrice;
}

double unitCostCenticreditsPerOutput(SectorId const sector, BuildTier const tier, InputPrices const prices) {
    BuildingDef const &definition = definitionFor(sector, tier);
    double const outputRate = static_cast<double>(definition.outputPerManualWorker) * definition.rateMultiplier;
    double unitCost = prices.labour / outputRate +
                      static_cast<double>(definition.materialsPerOutputUnit) * prices.materials +
                      static_cast<double>(definition.fuelPerOutputUnit) * prices.fuel;
    if (tier == BuildTier::Tier2) {
        unitCost += prices.energy * static_cast<double>(definition.energyPerTurnWhenStaffed) /
                    (static_cast<double>(definition.capacity) * outputRate);
    }
    return unitCost;
}

} // namespace Simulation::Economy
