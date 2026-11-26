#include "InitialConditions.h"

#include "ColonyTick.h"
#include "EconomyConstants.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <set>
#include <utility>

namespace {

using namespace Simulation::Economy;

struct ExpectedPrice {
    ResourceId resource;
    std::int64_t price;
    double halfWidth;
};

constexpr std::array<ExpectedPrice, 7> EXPECTED_PRICES{{
    {ResourceId::Labour, 40, 10.0},
    {ResourceId::Food, 4, 1.0},
    {ResourceId::Materials, 6, 1.5},
    {ResourceId::Fuel, 10, 2.5},
    {ResourceId::Energy, 15, 3.75},
    {ResourceId::Service, 40, 10.0},
    {ResourceId::ConsumerGoods, 30, 7.5},
}};

struct ExpectedSector {
    SectorId sector;
    std::int64_t buildings;
    std::int64_t creditsPerCorporation;
    ResourceId output;
    std::int64_t demandPrice;
    std::int64_t demandQuantity;
    std::int64_t operatingCost;
};

constexpr std::array<ExpectedSector, 6> EXPECTED_SECTORS{{
    {SectorId::Food, 477, 1'850'000'000, ResourceId::Food, 4, 5'712'670'000, 4'383'630'000},
    {SectorId::Materials, 54, 210'000'000, ResourceId::Materials, 6, 105'730'000, 496'260'000},
    {SectorId::Fuel, 13, 50'000'000, ResourceId::Fuel, 10, 12'800'000, 119'470'000},
    {SectorId::Energy, 13, 80'000'000, ResourceId::Energy, 15, 12'800'000, 188'580'000},
    {SectorId::ConsumerGoods, 96, 620'000'000, ResourceId::ConsumerGoods, 30, 95'020'000, 1'479'240'000},
    {SectorId::Service, 0, 1'200'000'000, ResourceId::Service, 40, 71'270'000, 2'860'000'000},
}};

[[nodiscard]] std::int64_t totalCredits(ColonyEconomy const &colony) {
    std::int64_t total{};
    for (Household const &household : colony.households) {
        total += household.credits;
    }
    for (Corporation const &corporation : colony.corporations) {
        total += corporation.credits;
    }
    return total;
}

[[nodiscard]] std::int64_t buildingCount(ColonyEconomy const &colony, SectorId const sector) {
    std::int64_t count{};
    for (Corporation const &corporation : colony.corporations) {
        count += static_cast<std::int64_t>(std::count_if(
            corporation.buildings.begin(), corporation.buildings.end(), [sector](Building const &building) {
                return building.sector == sector;
            }));
    }
    return count;
}

[[nodiscard]] std::int64_t totalSupply(ColonyEconomy const &colony, SectorId const sector) {
    std::int64_t quantity{};
    for (Corporation const &corporation : colony.corporations) {
        for (Building const &building : corporation.buildings) {
            if (building.sector != sector) {
                continue;
            }
            auto const methods = availableMethods(sector);
            auto const definition =
                std::find_if(methods.begin(), methods.end(), [&building](BuildingDef const &method) {
                    return method.tier == building.tier;
                });
            if (definition != methods.end()) {
                quantity += definition->capacity * definition->outputPerManualWorker *
                            static_cast<std::int64_t>(definition->rateMultiplier);
            }
        }
    }
    return quantity;
}

TEST_CASE("Earth seed satisfies the initial-state population, money, ownership, and inventory invariants") {
    ColonyEconomy const colony = buildEarthSeed(ColonyId{7}, 0xA8U);

    REQUIRE(colony.households.size() == 500);
    REQUIRE(colony.corporations.size() == 18);
    CHECK(colony.turnNumber == 1);
    CHECK(colony.id == ColonyId{7});
    CHECK(colony.economySeed == 0xA8U);
    CHECK(colony.totalPopulation == 500'000'000);
    CHECK(totalCredits(colony) == 62'030'000'000);

    std::int64_t population{};
    std::int64_t householdCredits{};
    std::int64_t foodStock{};
    std::set<std::int64_t> householdIds;
    std::set<std::int64_t> corporationOwners;
    for (Household const &household : colony.households) {
        population += household.population;
        householdCredits += household.credits;
        foodStock += household.foodStock;
        CHECK(household.population == POP_BASE);
        CHECK(household.credits >= MIN_CAPITAL);
        CHECK(household.foodStock == 30'000'000);
        CHECK(household.propIncome >= 0.5);
        CHECK(household.propIncome <= 0.9);
        CHECK(household.propSavings >= 0.2);
        CHECK(household.propSavings <= 0.6);
        CHECK(householdIds.insert(household.id.value).second);
    }
    CHECK(population == 500'000'000);
    CHECK(householdCredits == 50'000'000'000);
    CHECK(foodStock == 15'000'000'000);

    std::int64_t corporationCredits{};
    for (Corporation const &corporation : colony.corporations) {
        corporationCredits += corporation.credits;
        CHECK(corporation.credits >= MIN_CAPITAL);
        CHECK(corporationOwners.insert(corporation.owner.value).second);
        CHECK(householdIds.contains(corporation.owner.value));
        CHECK(corporation.lastIncome == 0);
        CHECK(corporation.stockUnitCost == 0.0);
        CHECK(corporation.lastSettlementPrice >= PRICE_FLOOR);
        CHECK(corporation.lastSettlementPrice <= PRICE_CAP);
        // Corporations start with one turn of steady-state inputs and saleable output so
        // the seeded economy produces and trades from the very first tick (the production
        // gate zeroes output when maintenance inputs are missing from stock).
        std::int64_t const buildings = static_cast<std::int64_t>(corporation.buildings.size());
        std::int64_t const unitOutput = corporation.sector == SectorId::Energy      ? 1'000'000
                                        : corporation.sector == SectorId::Food      ? 12'000'000
                                        : corporation.sector == SectorId::Materials ? 2'000'000
                                                                                    : 1'000'000;
        std::int64_t const maintenanceMaterials = corporation.sector == SectorId::Energy ? 10'000 : 15'000;
        std::int64_t expectedMaterials = maintenanceMaterials * buildings;
        if (corporation.sector == SectorId::Materials || corporation.sector == SectorId::ConsumerGoods) {
            // Materials holds its own saleable output; consumer goods embed one kg of
            // materials per unit produced.
            expectedMaterials += unitOutput * buildings;
        }
        std::int64_t const expectedFood = corporation.sector == SectorId::Food ? unitOutput * buildings : 0;
        std::int64_t const expectedFuel =
            (corporation.sector == SectorId::Energy || corporation.sector == SectorId::Fuel)
                ? unitOutput * buildings
                : 0; // energy holds its variable fuel input; the fuel sector holds its own saleable output
        std::int64_t const expectedConsumerGoods =
            corporation.sector == SectorId::ConsumerGoods ? unitOutput * buildings : 0;
        CHECK(corporation.stock[ResourceId::Food] == expectedFood);
        CHECK(corporation.stock[ResourceId::Materials] == expectedMaterials);
        CHECK(corporation.stock[ResourceId::Fuel] == expectedFuel);
        CHECK(corporation.stock[ResourceId::ConsumerGoods] == expectedConsumerGoods);
        for (Building const &building : corporation.buildings) {
            CHECK(building.sector == corporation.sector);
            CHECK(!(building.underConstruction));
            auto const methods = availableMethods(building.sector);
            CHECK(std::any_of(methods.begin(), methods.end(), [&building](BuildingDef const &method) {
                return method.tier == building.tier;
            }));
        }
    }
    CHECK(corporationCredits == 12'030'000'000);
    CHECK(corporationOwners.size() == colony.corporations.size());

    std::int64_t const smoothedFoodPrice = std::accumulate(colony.households.front().foodPriceHistory.begin(),
                                                           colony.households.front().foodPriceHistory.end(),
                                                           std::int64_t{}) /
                                           WAGE_SMOOTH;
    CHECK(smoothedFoodPrice == 4);
    CHECK(smoothedFoodPrice * FOOD_PER_PERSON == 40);
    for (Household const &household : colony.households) {
        CHECK(std::all_of(household.foodPriceHistory.begin(),
                          household.foodPriceHistory.end(),
                          [](std::int64_t const price) { return price == 4; }));
        CHECK(household.lastFoodPrice == 4);
        CHECK(household.lastIncome == 17'104'000);
    }
}

TEST_CASE("Earth seed prices, beliefs, demand, supported buildings, and table capital are initialized") {
    ColonyEconomy const colony = buildEarthSeed(ColonyId{3}, 41);
    InputPrices const inputs{40.0, 6.0, 15.0, 10.0};

    for (ExpectedPrice const &expected : EXPECTED_PRICES) {
        CHECK(colony.market.lastPriceFor(expected.resource) == expected.price);
        CHECK(expected.price >= PRICE_FLOOR);
        CHECK(expected.price <= PRICE_CAP);
    }
    for (Household const &household : colony.households) {
        CHECK(household.foodBelief.mean == 4.0);
        CHECK(household.foodBelief.halfWidth == 1.0);
        CHECK(household.labourBelief.mean == 40.0);
        CHECK(household.labourBelief.halfWidth == 10.0);
        CHECK(household.serviceBelief.mean == 40.0);
        CHECK(household.serviceBelief.halfWidth == 10.0);
        CHECK(household.consumerGoodsBelief.mean == 30.0);
        CHECK(household.consumerGoodsBelief.halfWidth == 7.5);
    }

    for (ExpectedSector const &expected : EXPECTED_SECTORS) {
        CHECK(buildingCount(colony, expected.sector) == expected.buildings);
        if (expected.sector != SectorId::Service) {
            CHECK(totalSupply(colony, expected.sector) >= expected.demandQuantity);
        }
        CHECK(colony.market.lastPriceFor(expected.output) == expected.demandPrice);
        CHECK(std::count_if(
                  colony.corporations.begin(), colony.corporations.end(), [&expected](Corporation const &corporation) {
                      return corporation.sector == expected.sector;
                  }) == 3);
        std::int64_t const minOperatingCapital = (expected.operatingCost + 2) / 3;
        std::int64_t sectorDemand{};
        for (Corporation const &corporation : colony.corporations) {
            if (corporation.sector != expected.sector) {
                continue;
            }
            CHECK(corporation.credits == expected.creditsPerCorporation);
            CHECK(static_cast<std::int64_t>(corporation.credits * (1.0 - RESERVE_FRACTION)) >= minOperatingCapital);
            CHECK(corporation.sectorDemand.steps.size() == 1);
            CHECK(corporation.sectorDemand.steps.front().first ==
                  expected.demandPrice + (expected.sector == SectorId::Service ? 1 : 0));
            sectorDemand += corporation.sectorDemand.steps.front().second;
            for (ExpectedPrice const &seedPrice : EXPECTED_PRICES) {
                PriceBelief const belief = corporation.priceBeliefs[static_cast<std::size_t>(seedPrice.resource)];
                CHECK(belief.mean == static_cast<double>(seedPrice.price));
                CHECK(belief.halfWidth == seedPrice.halfWidth);
            }
            for (Building const &building : corporation.buildings) {
                auto const methods = availableMethods(building.sector);
                auto const method = std::find_if(methods.begin(), methods.end(), [&building](BuildingDef const &entry) {
                    return entry.tier == building.tier;
                });
                REQUIRE(method != methods.end());
                double const cost = unitCostCenticreditsPerOutput(building.sector, building.tier, inputs);
                auto const output = outputResource(building.sector);
                CHECK(output.has_value());
                std::int64_t const price = colony.market.lastPriceFor(output.value_or(ResourceId::Credits));
                if (building.tier == BuildTier::Tier2) {
                    CHECK(static_cast<double>(price) > cost);
                }
                CHECK(method->capacity > 0);
            }
        }
        CHECK(sectorDemand == expected.demandQuantity);
    }

    CHECK(buildingCount(colony, SectorId::Service) == 0);
    CHECK(std::count_if(colony.corporations.begin(), colony.corporations.end(), [](Corporation const &corporation) {
              return corporation.sector == SectorId::Service;
          }) == 3);
    std::int64_t labourPurchased{};
    for (Corporation const &corporation : colony.corporations) {
        for (Building const &building : corporation.buildings) {
            auto const methods = availableMethods(building.sector);
            auto const definition =
                std::find_if(methods.begin(), methods.end(), [&building](BuildingDef const &method) {
                    return method.tier == building.tier;
                });
            if (definition != methods.end()) {
                labourPurchased += definition->capacity;
                labourPurchased += definition->maintenanceLabour;
            }
        }
    }
    labourPurchased += 71'500'000; // Manual service staffing from the seed-capacity table.
    CHECK(labourPurchased == 213'795'000);
    CHECK(labourPurchased <= colony.totalPopulation);

    ColonyEconomy const repeat = buildEarthSeed(ColonyId{3}, 41);
    REQUIRE(repeat.households.size() == colony.households.size());
    REQUIRE(repeat.corporations.size() == colony.corporations.size());
    for (std::size_t index = 0; index < colony.households.size(); ++index) {
        CHECK(repeat.households[index].propIncome == colony.households[index].propIncome);
        CHECK(repeat.households[index].propSavings == colony.households[index].propSavings);
    }
    for (std::size_t index = 0; index < colony.corporations.size(); ++index) {
        CHECK(repeat.corporations[index].owner == colony.corporations[index].owner);
    }
}

TEST_CASE("Earth seed turn-one settlements stay near the fictional seed prices") {
    ColonyTickResult const result = tickColony(buildEarthSeed(ColonyId{19}, 0xA8U));
    CHECK(result.colony.turnNumber == 2);

    for (ExpectedPrice const &expected : EXPECTED_PRICES) {
        auto const settlement = std::find_if(
            result.settlements.begin(), result.settlements.end(), [&expected](SettlementOutcome const &outcome) {
                return outcome.resource == expected.resource;
            });
        REQUIRE(settlement != result.settlements.end());
        std::int64_t const tolerance = std::max<std::int64_t>(1, expected.price / 4);
        CHECK(std::abs(settlement->price - expected.price) <= tolerance);
    }
    CHECK(result.colony.market.lastPriceFor(ResourceId::Labour) == 40);
    auto const labourSettlement =
        std::find_if(result.settlements.begin(), result.settlements.end(), [](SettlementOutcome const &outcome) {
            return outcome.resource == ResourceId::Labour;
        });
    REQUIRE(labourSettlement != result.settlements.end());
    CHECK(labourSettlement->price == 40);
    std::int64_t const unsoldLabour = 500'000'000 - labourSettlement->tradedQuantity;
    CHECK(std::abs(unsoldLabour - 286'205'000) <= 2'000'000);
}
} // namespace
