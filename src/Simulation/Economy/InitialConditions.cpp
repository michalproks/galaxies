#include "InitialConditions.h"

#include "BuildingDef.h"
#include "EconomyConstants.h"
#include "Resources.h"
#include "Rng.h"
#include "Sector.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Simulation::Economy {
namespace {

    constexpr std::int64_t NUM_HOUSEHOLDS = 500;
    constexpr std::int64_t NUM_CORPORATIONS_PER_SECTOR = 3;
    constexpr std::int64_t FOOD_STOCK_TURNS = 3;
    constexpr double PROP_INCOME_MIN = 0.5;
    constexpr double PROP_INCOME_MAX = 0.9;
    constexpr double PROP_SAVINGS_MIN = 0.2;
    constexpr double PROP_SAVINGS_MAX = 0.6;
    constexpr double SEED_HALFWIDTH = 0.25;
    constexpr std::int64_t HOUSEHOLD_CREDITS = 100'000'000;
    constexpr std::int64_t HOUSEHOLD_LAST_INCOME = 17'104'000;
    constexpr std::int64_t SEED_FOOD_PRICE = 4;
    constexpr std::int64_t SEED_LABOUR_PRICE = 40;
    constexpr std::int64_t SEED_MATERIALS_PRICE = 6;
    constexpr std::int64_t SEED_FUEL_PRICE = 10;
    constexpr std::int64_t SEED_ENERGY_PRICE = 15;
    constexpr std::int64_t SEED_SERVICE_PRICE = 40;
    constexpr std::int64_t SEED_CONSUMER_GOODS_PRICE = 30;

    struct SectorSeed {
        SectorId sector;
        std::int64_t buildingCount;
        std::int64_t creditsPerCorporation;
        std::int64_t demandPrice;
        std::int64_t demandQuantity;
    };

    constexpr std::array<SectorSeed, 6> SECTOR_SEEDS{{
        {SectorId::Food, 477, 1'850'000'000, SEED_FOOD_PRICE, 5'712'670'000},
        {SectorId::Materials, 54, 210'000'000, SEED_MATERIALS_PRICE, 105'730'000},
        {SectorId::Fuel, 13, 50'000'000, SEED_FUEL_PRICE, 12'800'000},
        {SectorId::Energy, 13, 80'000'000, SEED_ENERGY_PRICE, 12'800'000},
        {SectorId::ConsumerGoods, 96, 620'000'000, SEED_CONSUMER_GOODS_PRICE, 95'020'000},
        {SectorId::Service, 0, 1'200'000'000, SEED_SERVICE_PRICE, 71'270'000},
    }};

    constexpr std::array<std::pair<ResourceId, std::int64_t>, 7> SEED_PRICES{{
        {ResourceId::Labour, SEED_LABOUR_PRICE},
        {ResourceId::Food, SEED_FOOD_PRICE},
        {ResourceId::Materials, SEED_MATERIALS_PRICE},
        {ResourceId::Fuel, SEED_FUEL_PRICE},
        {ResourceId::Energy, SEED_ENERGY_PRICE},
        {ResourceId::Service, SEED_SERVICE_PRICE},
        {ResourceId::ConsumerGoods, SEED_CONSUMER_GOODS_PRICE},
    }};

    [[nodiscard]] PriceBelief makeSeedBelief(std::int64_t const price) {
        return {static_cast<double>(price),
                std::clamp(SEED_HALFWIDTH * static_cast<double>(price), BELIEF_W_MIN, BELIEF_W_MAX)};
    }

    void seedCorporationBeliefs(Corporation &corporation) {
        for (auto const &[resource, price] : SEED_PRICES) {
            corporation.priceBeliefs[static_cast<std::size_t>(resource)] = makeSeedBelief(price);
        }
    }

    [[nodiscard]] std::int64_t seedPriceFor(ResourceId const resource) {
        const auto price = std::find_if(
            SEED_PRICES.begin(), SEED_PRICES.end(), [resource](auto const &entry) { return entry.first == resource; });
        return price == SEED_PRICES.end() ? 0 : price->second;
    }

    [[nodiscard]] BuildTier seededTier(SectorId const sector) {
        if (sector == SectorId::Energy) {
            return BuildTier::Tier1;
        }
        return sector == SectorId::Service ? BuildTier::Manual : BuildTier::Tier2;
    }

    [[nodiscard]] std::int64_t buildingsForCorporation(SectorSeed const &seed, std::int64_t const index) {
        return seed.buildingCount / NUM_CORPORATIONS_PER_SECTOR +
               (index < seed.buildingCount % NUM_CORPORATIONS_PER_SECTOR ? 1 : 0);
    }

    [[nodiscard]] std::int64_t demandForCorporation(SectorSeed const &seed, std::int64_t const index) {
        if (seed.buildingCount == 0) {
            return seed.demandQuantity / NUM_CORPORATIONS_PER_SECTOR +
                   (index < seed.demandQuantity % NUM_CORPORATIONS_PER_SECTOR ? 1 : 0);
        }
        std::int64_t precedingBuildings{};
        for (std::int64_t corporationIndex = 0; corporationIndex < index; ++corporationIndex) {
            precedingBuildings += buildingsForCorporation(seed, corporationIndex);
        }
        std::int64_t const currentBuildings = buildingsForCorporation(seed, index);
        std::int64_t const demandBefore = seed.demandQuantity * precedingBuildings / seed.buildingCount;
        std::int64_t const demandAfter =
            seed.demandQuantity * (precedingBuildings + currentBuildings) / seed.buildingCount;
        return demandAfter - demandBefore;
    }

    // Turn-1 production is gated on the material and fuel stocks a corporation already
    // holds (maintenance inputs plus per-unit variable inputs), and standard-sector
    // corporations only sell stock they held at the start of the turn. Seeding one turn of
    // steady-state inputs and output keeps the initial economy in the self-sustaining
    // state the specifications describe: every sector produces its seeded capacity and
    // supply approximately equals demand from the first tick onward.
    void seedSteadyStateStocks(Corporation &corporation, SectorSeed const &seed, std::int64_t const buildingCount) {
        if (buildingCount <= 0) {
            return;
        }
        BuildTier const tier = seededTier(seed.sector);
        std::vector<BuildingDef> const methods = availableMethods(seed.sector);
        auto const definition = std::find_if(
            methods.begin(), methods.end(), [tier](BuildingDef const &method) { return method.tier == tier; });
        if (definition == methods.end()) {
            return;
        }
        std::int64_t const capacityPerBuilding = static_cast<std::int64_t>(
            static_cast<double>(definition->capacity) * static_cast<double>(definition->outputPerManualWorker) *
            definition->rateMultiplier);
        std::int64_t const turnOutput = capacityPerBuilding * buildingCount;
        corporation.stock[ResourceId::Materials] +=
            definition->maintenanceMaterials * buildingCount + definition->materialsPerOutputUnit * turnOutput;
        corporation.stock[ResourceId::Fuel] += definition->fuelPerOutputUnit * turnOutput;
        auto const output = outputResource(seed.sector);
        if (output.has_value() && isStandardResource(output.value())) {
            corporation.stock[output.value()] += turnOutput;
        }
    }

    [[nodiscard]] Corporation makeCorporation(SectorSeed const &seed,
                                              CorporationId const id,
                                              std::int64_t const sectorCorporationIndex,
                                              HouseholdId const owner) {
        Corporation corporation;
        corporation.id = id;
        corporation.owner = owner;
        corporation.sector = seed.sector;
        corporation.credits = seed.creditsPerCorporation;
        auto const output = outputResource(seed.sector);
        if (output.has_value()) {
            corporation.lastSettlementPrice = seedPriceFor(output.value());
        }
        // A one-cent planning margin lets the zero-profit service sector staff;
        // the realized reservation and market prices remain at the 40 seed anchor.
        std::int64_t const demandPrice = seed.sector == SectorId::Service ? seed.demandPrice + 1 : seed.demandPrice;
        corporation.sectorDemand.steps = {{demandPrice, demandForCorporation(seed, sectorCorporationIndex)}};
        corporation.hasHadFirstBuilding = true;
        seedCorporationBeliefs(corporation);
        return corporation;
    }

} // namespace

ColonyEconomy buildEarthSeed(ColonyId const colonyId, std::uint64_t const economySeed) {
    ColonyEconomy colony;
    colony.id = colonyId;
    colony.turnNumber = 1;
    colony.economySeed = economySeed;
    colony.totalPopulation = NUM_HOUSEHOLDS * POP_BASE;

    Rng const rootRng{colonyId, 0};
    Rng ownershipRng = rootRng.substream(economySeed).substream(0);
    Rng propensityRng = rootRng.substream(economySeed).substream(1);

    std::vector<std::int64_t> householdIndices(static_cast<std::size_t>(NUM_HOUSEHOLDS));
    for (std::int64_t index = 0; index < NUM_HOUSEHOLDS; ++index) {
        householdIndices[static_cast<std::size_t>(index)] = index;
    }
    for (std::int64_t index = 0; index < NUM_CORPORATIONS_PER_SECTOR * static_cast<std::int64_t>(SECTOR_SEEDS.size());
         ++index) {
        std::uint64_t const selected =
            ownershipRng.uniformInt(static_cast<std::uint64_t>(index), static_cast<std::uint64_t>(NUM_HOUSEHOLDS - 1));
        std::swap(householdIndices[static_cast<std::size_t>(index)],
                  householdIndices[static_cast<std::size_t>(selected)]);
    }

    colony.households.reserve(static_cast<std::size_t>(NUM_HOUSEHOLDS));
    for (std::int64_t index = 0; index < NUM_HOUSEHOLDS; ++index) {
        Household household;
        household.id = HouseholdId{index};
        household.population = POP_BASE;
        household.credits = HOUSEHOLD_CREDITS;
        household.propIncome = PROP_INCOME_MIN + propensityRng.uniformDouble() * (PROP_INCOME_MAX - PROP_INCOME_MIN);
        household.propSavings =
            PROP_SAVINGS_MIN + propensityRng.uniformDouble() * (PROP_SAVINGS_MAX - PROP_SAVINGS_MIN);
        household.foodStock = FOOD_STOCK_TURNS * FOOD_PER_PERSON * POP_BASE;
        household.foodBelief = makeSeedBelief(SEED_FOOD_PRICE);
        household.labourBelief = makeSeedBelief(SEED_LABOUR_PRICE);
        household.serviceBelief = makeSeedBelief(SEED_SERVICE_PRICE);
        household.consumerGoodsBelief = makeSeedBelief(SEED_CONSUMER_GOODS_PRICE);
        household.foodPriceHistory.fill(SEED_FOOD_PRICE);
        household.lastIncome = HOUSEHOLD_LAST_INCOME;
        household.lastFoodPrice = SEED_FOOD_PRICE;
        colony.households.push_back(household);
    }

    std::int64_t corporationIndex{};
    colony.corporations.reserve(SECTOR_SEEDS.size() * static_cast<std::size_t>(NUM_CORPORATIONS_PER_SECTOR));
    for (SectorSeed const &seed : SECTOR_SEEDS) {
        for (std::int64_t index = 0; index < NUM_CORPORATIONS_PER_SECTOR; ++index) {
            std::int64_t const ownerIndex = householdIndices[static_cast<std::size_t>(corporationIndex)];
            Corporation corporation =
                makeCorporation(seed, CorporationId{corporationIndex}, index, HouseholdId{ownerIndex});
            BuildTier const tier = seededTier(seed.sector);
            std::vector<BuildingDef> const methods = availableMethods(seed.sector);
            auto const definition = std::find_if(
                methods.begin(), methods.end(), [tier](BuildingDef const &method) { return method.tier == tier; });
            std::int64_t const count = buildingsForCorporation(seed, index);
            if (definition != methods.end()) {
                corporation.buildings.reserve(static_cast<std::size_t>(count));
                for (std::int64_t buildingIndex = 0; buildingIndex < count; ++buildingIndex) {
                    corporation.buildings.push_back(
                        {seed.sector,
                         tier,
                         0,
                         false,
                         constructionValue(*definition, SEED_LABOUR_PRICE, SEED_MATERIALS_PRICE)});
                }
                seedSteadyStateStocks(corporation, seed, count);
            }
            colony.corporations.push_back(corporation);
            ++corporationIndex;
        }
    }

    for (auto const &[resource, price] : SEED_PRICES) {
        colony.market.lastPriceFor(resource) = price;
    }
    return colony;
}

} // namespace Simulation::Economy
