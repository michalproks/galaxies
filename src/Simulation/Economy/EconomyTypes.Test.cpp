#include "BuildingDef.h"
#include "EconomyConstants.h"
#include "Resources.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

TEST_CASE("Resource categories separate credits, instant flows, and storable goods") {
    using Simulation::Economy::ResourceId;

    CHECK(Simulation::Economy::isVirtualCredit(ResourceId::Credits));
    CHECK(!(Simulation::Economy::isVirtualCredit(ResourceId::Food)));
    CHECK(Simulation::Economy::isInstantResource(ResourceId::Labour));
    CHECK(Simulation::Economy::isInstantResource(ResourceId::Energy));
    CHECK(Simulation::Economy::isInstantResource(ResourceId::Service));
    CHECK(!(Simulation::Economy::isInstantResource(ResourceId::Credits)));
    CHECK(!(Simulation::Economy::isInstantResource(ResourceId::Food)));
    CHECK(Simulation::Economy::isStandardResource(ResourceId::Food));
    CHECK(Simulation::Economy::isStandardResource(ResourceId::Materials));
    CHECK(Simulation::Economy::isStandardResource(ResourceId::Fuel));
    CHECK(Simulation::Economy::isStandardResource(ResourceId::ConsumerGoods));
    CHECK(!(Simulation::Economy::isStandardResource(ResourceId::Labour)));
    CHECK(!(Simulation::Economy::isStandardResource(ResourceId::Credits)));
}

TEST_CASE("Production methods match the sector availability table") {
    using Simulation::Economy::BuildTier;
    using Simulation::Economy::SectorId;

    auto const fuelMethods = Simulation::Economy::availableMethods(SectorId::Fuel);
    REQUIRE(fuelMethods.size() == 2);
    CHECK(fuelMethods[0].tier == BuildTier::Manual);
    CHECK(fuelMethods[1].tier == BuildTier::Tier1);
    CHECK(std::none_of(fuelMethods.begin(), fuelMethods.end(), [](Simulation::Economy::BuildingDef const &definition) {
        return definition.tier == BuildTier::Tier2;
    }));

    auto const serviceMethods = Simulation::Economy::availableMethods(SectorId::Service);
    REQUIRE(serviceMethods.size() == 1);
    CHECK(serviceMethods[0].tier == BuildTier::Manual);

    auto const energyMethods = Simulation::Economy::availableMethods(SectorId::Energy);
    REQUIRE(energyMethods.size() == 1);
    CHECK(energyMethods[0].sector == SectorId::Energy);
    CHECK(energyMethods[0].capacity == 100'000);
    CHECK(energyMethods[0].fuelPerOutputUnit == 1);

    CHECK(Simulation::Economy::availableMethods(SectorId::Food).size() == 3);
    CHECK(Simulation::Economy::availableMethods(SectorId::Materials).size() == 3);
    CHECK(Simulation::Economy::availableMethods(SectorId::ConsumerGoods).size() == 3);
}

TEST_CASE("Unit cost calculation follows the production-table worked example") {
    using Simulation::Economy::BuildTier;
    using Simulation::Economy::SectorId;

    double const manualFood =
        Simulation::Economy::unitCostCenticreditsPerOutput(SectorId::Food, BuildTier::Manual, {40.0, 6.0, 8.0, 10.0});
    double const tier1Food =
        Simulation::Economy::unitCostCenticreditsPerOutput(SectorId::Food, BuildTier::Tier1, {40.0, 6.0, 8.0, 10.0});
    double const tier2Food =
        Simulation::Economy::unitCostCenticreditsPerOutput(SectorId::Food, BuildTier::Tier2, {40.0, 6.0, 8.0, 10.0});

    CHECK(manualFood == Catch::Approx(3.3333333333));
    CHECK(tier1Food == Catch::Approx(1.6666666667));
    CHECK(tier2Food == Catch::Approx(0.680));
    CHECK(tier2Food < tier1Food);
    CHECK(tier1Food < manualFood);

    double const tier2FoodAtEnergyPrice15 =
        Simulation::Economy::unitCostCenticreditsPerOutput(SectorId::Food, BuildTier::Tier2, {40.0, 6.0, 15.0, 10.0});
    CHECK(tier2FoodAtEnergyPrice15 == Catch::Approx(0.6916666667));

    CHECK(Simulation::Economy::unitCostCenticreditsPerOutput(
              SectorId::ConsumerGoods, BuildTier::Manual, {40.0, 6.0, 8.0, 10.0}) == Catch::Approx(46.0));
    CHECK(Simulation::Economy::unitCostCenticreditsPerOutput(
              SectorId::ConsumerGoods, BuildTier::Tier1, {40.0, 6.0, 8.0, 10.0}) == Catch::Approx(26.0));
    CHECK(Simulation::Economy::unitCostCenticreditsPerOutput(
              SectorId::ConsumerGoods, BuildTier::Tier2, {40.0, 6.0, 8.0, 10.0}) == Catch::Approx(14.16));
}

TEST_CASE("Jump-start capital is raised to cover the construction cost") {
    CHECK(Simulation::Economy::MIN_CAPITAL == 45'000'000);
}
