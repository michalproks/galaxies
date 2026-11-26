#include "InitialConditions.h"

#include "EconomyConstants.h"
#include "Rng.h"

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

    // The requested 15-agent count excludes the separate three-corporation service row in the spec.
    constexpr std::array<SectorSeed, 5> SECTOR_SEEDS{{
        {SectorId::Food, 477, 1'850'000'000, SEED_FOOD_PRICE, 5'712'670'000},
        {SectorId::Materials, 54, 210'000'000, SEED_MATERIALS_PRICE, 105'730'000},
        // The current production table has no Fuel Tier2; Tier1 count preserves nominal capacity.
        {SectorId::Fuel, 65, 50'000'000, SEED_FUEL_PRICE, 12'800'000},
        {SectorId::Energy, 13, 80'000'000, SEED_ENERGY_PRICE, 12'800'000},
        {SectorId::ConsumerGoods, 96, 620'000'000, SEED_CONSUMER_GOODS_PRICE, 95'020'000},
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
        return sector == SectorId::Energy || sector == SectorId::Fuel ? BuildTier::Tier1 : BuildTier::Tier2;
    }

    [[nodiscard]] Corporation
    makeCorporation(SectorSeed const &seed, std::int64_t const index, HouseholdId const owner) {
        Corporation corporation;
        corporation.id = CorporationId{index};
        corporation.owner = owner;
        corporation.sector = seed.sector;
        corporation.credits = seed.creditsPerCorporation;
        auto const output = outputResource(seed.sector);
        if (output.has_value()) {
            corporation.lastSettlementPrice = seedPriceFor(output.value());
        }
        corporation.sectorDemand.steps = {{seed.demandPrice, seed.demandQuantity}};
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
            Corporation corporation = makeCorporation(seed, corporationIndex, HouseholdId{ownerIndex});
            BuildTier const tier = seededTier(seed.sector);
            std::vector<BuildingDef> const methods = availableMethods(seed.sector);
            auto const definition = std::find_if(
                methods.begin(), methods.end(), [tier](BuildingDef const &method) { return method.tier == tier; });
            std::int64_t const count = seed.buildingCount / NUM_CORPORATIONS_PER_SECTOR +
                                       (index < seed.buildingCount % NUM_CORPORATIONS_PER_SECTOR ? 1 : 0);
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
