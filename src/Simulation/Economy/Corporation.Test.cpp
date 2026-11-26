#include "Corporation.h"

#include "EconomyConstants.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>

namespace {

using namespace Simulation::Economy;

[[nodiscard]] Corporation corporation(SectorId const sector, std::int64_t const credits) {
    Corporation result;
    result.id = CorporationId{17};
    result.owner = HouseholdId{3};
    result.sector = sector;
    result.credits = credits;
    result.lastSettlementPrice = 4;
    result.priceBeliefs[static_cast<std::size_t>(ResourceId::Labour)] = {40.0, 0.0};
    result.priceBeliefs[static_cast<std::size_t>(ResourceId::Materials)] = {6.0, 0.0};
    result.priceBeliefs[static_cast<std::size_t>(ResourceId::Energy)] = {8.0, 0.0};
    result.priceBeliefs[static_cast<std::size_t>(ResourceId::Fuel)] = {5.0, 0.0};
    result.priceBeliefs[static_cast<std::size_t>(ResourceId::Food)] = {4.0, 0.0};
    result.priceBeliefs[static_cast<std::size_t>(ResourceId::ConsumerGoods)] = {30.0, 0.0};
    return result;
}

[[nodiscard]] CorporationPrices prices(std::int64_t const outputPrice = 4) {
    CorporationPrices result;
    result.inputs = {40.0, 6.0, 8.0, 5.0};
    result.output = outputPrice;
    result.firstOrderId = OrderId{100};
    result.resources[static_cast<std::size_t>(ResourceId::Food)] = outputPrice;
    result.resources[static_cast<std::size_t>(ResourceId::Materials)] = 6;
    result.resources[static_cast<std::size_t>(ResourceId::Fuel)] = 5;
    result.resources[static_cast<std::size_t>(ResourceId::Energy)] = 8;
    return result;
}

[[nodiscard]] Order const *findOrder(CorporationPlan const &plan, OrderSide const side, ResourceId const resource) {
    auto const order = std::find_if(plan.orders.begin(), plan.orders.end(), [=](Order const &candidate) {
        return candidate.side == side && candidate.resource == resource;
    });
    return order == plan.orders.end() ? nullptr : &*order;
}

} // namespace

TEST_CASE("Food profit sweep and staffing match the worked production example") {
    using namespace Simulation::Economy;

    Corporation company = corporation(SectorId::Food, 50'000'000);
    company.buildings = {{SectorId::Food, BuildTier::Tier1, 0, false, 14'000'000},
                         {SectorId::Food, BuildTier::Tier2, 0, false, 22'400'000}};
    company.stock[ResourceId::Food] = 3'000'000;
    company.sectorDemand.steps = {{5, 3'000'000}, {4, 12'000'000}};

    CorporationPlan const plan = planCorporation(company, prices());

    CHECK(plan.targetPrice == 4);
    CHECK(plan.targetQuantity == 15'000'000);
    CHECK(plan.targetProfit == Catch::Approx(45'840'000.0));
    CHECK(plan.plannedOutput == 15'000'000);
    CHECK(plan.breakEvenPrice == Catch::Approx(3.3333333333));
    CHECK(plan.outputUnitCost == Catch::Approx(0.944));
    REQUIRE(plan.staffing.size() == 2);
    CHECK(plan.staffing[0].workers == 100'000);
    CHECK(plan.staffing[1].workers == 200'000);
    CHECK(plan.plannedCost == 15'480'000);
    auto const labourOrder = findOrder(plan, OrderSide::Buy, ResourceId::Labour);
    REQUIRE(labourOrder != nullptr);
    CHECK(labourOrder->quantity == 380'000);
    auto const sellOrder = findOrder(plan, OrderSide::Sell, ResourceId::Food);
    REQUIRE(sellOrder != nullptr);
    CHECK(sellOrder->quantity == 3'000'000);
    CHECK(sellOrder->price == 4);
}

TEST_CASE("Manual food worked example clamps staffing to budget and retains the stated growth values") {
    using namespace Simulation::Economy;

    Corporation company = corporation(SectorId::Food, 10'000'000);
    company.sectorDemand.steps = {{5, 4'000'000}, {4, 6'000'000}};

    CorporationPlan const firstTurn = planCorporation(company, prices());

    CHECK(firstTurn.targetPrice == 4);
    CHECK(firstTurn.targetQuantity == 10'000'000);
    CHECK(firstTurn.targetProfit == Catch::Approx(6'666'666.667));
    CHECK(5 * 4'000'000 - 3.3333333333 * 4'000'000 == Catch::Approx(6'666'666.667));
    CHECK(firstTurn.plannedOutput == 2'400'000);
    CHECK(firstTurn.plannedCost == 8'000'000);
    CHECK(firstTurn.endCredits == 11'600'000);
    CHECK(firstTurn.dividend == 2'320'000);
    auto const firstLabour = findOrder(firstTurn, OrderSide::Buy, ResourceId::Labour);
    REQUIRE(firstLabour != nullptr);
    CHECK(firstLabour->quantity == 200'000);

    company.credits = 11'600'000;
    company.sectorDemand.steps = {{4, 2'784'000}};
    CorporationPlan const secondTurn = planCorporation(company, prices());
    CHECK(secondTurn.plannedOutput == 2'784'000);
    CHECK(secondTurn.targetPrice == 4);
    CHECK(secondTurn.plannedCost == 9'280'000);
    CHECK(secondTurn.endCredits == 13'456'000);
    auto const secondLabour = findOrder(secondTurn, OrderSide::Buy, ResourceId::Labour);
    REQUIRE(secondLabour != nullptr);
    CHECK(secondLabour->quantity == 232'000);

    company.credits = 13'456'000;
    company.stock[ResourceId::Food] = 2'784'000;
    company.stockUnitCost = 3.3333333333;
    company.sectorDemand.steps = {{3, 10'000'000}};
    CorporationPlan const belowBreakEven = planCorporation(company, prices(3));
    CHECK(belowBreakEven.plannedOutput == 0);
    CHECK(13'456'000 - 9'280'000 == 4'176'000);
    CHECK(belowBreakEven.endCredits == 13'456'000);
    CHECK(findOrder(belowBreakEven, OrderSide::Buy, ResourceId::Labour) == nullptr);
    auto const heldStock = findOrder(belowBreakEven, OrderSide::Sell, ResourceId::Food);
    REQUIRE(heldStock != nullptr);
    CHECK(heldStock->quantity == 2'784'000);
    CHECK(heldStock->price == 4);
}

TEST_CASE("Consumer goods worked example selects tier 2 at the 30-centicredit step") {
    using namespace Simulation::Economy;

    Corporation company = corporation(SectorId::ConsumerGoods, 20'000'000);
    company.buildings = {{SectorId::ConsumerGoods, BuildTier::Tier2, 0, false, 22'400'000}};
    company.sectorDemand.steps = {{30, 383'333}, {20, 100'000}};
    company.lastSettlementPrice = 30;
    company.breakEvenPrice = 14.16;

    CorporationPlan const plan = planCorporation(company, prices(30));

    CHECK(plan.targetPrice == 30);
    CHECK(plan.targetQuantity == 383'333);
    CHECK(plan.outputUnitCost == Catch::Approx(14.16));
    CHECK(plan.targetProfit == Catch::Approx(6'072'000.0));
    CHECK(plan.plannedOutput == 383'333);
    auto const energyOrder = findOrder(plan, OrderSide::Buy, ResourceId::Energy);
    REQUIRE(energyOrder != nullptr);
    CHECK(energyOrder->quantity == 20'000);
    auto const materialOrder = findOrder(plan, OrderSide::Buy, ResourceId::Materials);
    REQUIRE(materialOrder != nullptr);
    CHECK(materialOrder->quantity == 398'333);
}

TEST_CASE("Service worked example rejects output priced below break-even") {
    using namespace Simulation::Economy;

    Corporation company = corporation(SectorId::Service, 10'000'000);
    company.sectorDemand.steps = {{10, 1'150'000}};

    CorporationPlan const plan = planCorporation(company, prices(10));

    CHECK(plan.targetQuantity == 0);
    CHECK(plan.plannedOutput == 0);
    CHECK(plan.breakEvenPrice == 40.0);
    CHECK(findOrder(plan, OrderSide::Buy, ResourceId::Labour) == nullptr);
    CHECK(plan.endCredits == 10'000'000);
    CHECK(plan.dividend == 2'000'000);
}

TEST_CASE("Demand shortfall triggers a profitable tier 2 building within the spare-cash cap") {
    using namespace Simulation::Economy;

    Corporation company = corporation(SectorId::Food, 50'000'000);
    company.buildings = {{SectorId::Food, BuildTier::Tier1, 0, false, 14'000'000},
                         {SectorId::Food, BuildTier::Tier2, 0, false, 22'400'000}};
    company.sectorDemand.steps = {{4, 40'000'000}};

    CorporationPlan const plan = planCorporation(company, prices());

    CHECK(plan.targetQuantity == 40'000'000);
    CHECK(plan.plannedOutput == 14'400'000);
    REQUIRE(plan.construction.size() == 1);
    CHECK(plan.construction[0].tier == BuildTier::Tier2);
    CHECK(plan.construction[0].constructionValue == 22'400'000);
    CHECK(plan.construction[0].underConstruction);
    CHECK(plan.plannedCost == 35'880'000);
}

TEST_CASE("Tier 2 food full-capacity and power-plant examples preserve their input totals") {
    using namespace Simulation::Economy;

    Corporation food = corporation(SectorId::Food, 50'000'000);
    food.buildings = {{SectorId::Food, BuildTier::Tier2, 0, false, 22'400'000}};
    food.sectorDemand.steps = {{4, 12'000'000}};
    CorporationPlan const foodPlan = planCorporation(food, prices());
    CHECK(foodPlan.targetQuantity == 12'000'000);
    CHECK(foodPlan.plannedOutput == 12'000'000);
    CHECK(foodPlan.plannedCost == 9'050'000);
    auto const foodLabourOrder = findOrder(foodPlan, OrderSide::Buy, ResourceId::Labour);
    REQUIRE(foodLabourOrder != nullptr);
    CHECK(foodLabourOrder->quantity == 220'000);
    auto const foodEnergyOrder = findOrder(foodPlan, OrderSide::Buy, ResourceId::Energy);
    REQUIRE(foodEnergyOrder != nullptr);
    CHECK(foodEnergyOrder->quantity == 20'000);

    Corporation energy = corporation(SectorId::Energy, 20'000'000);
    energy.buildings = {{SectorId::Energy, BuildTier::Tier1, 0, false, 13'800'000}};
    energy.sectorDemand.steps = {{12, 1'000'000}};
    CorporationPrices energyPrices = prices(12);
    energyPrices.inputs = {40.0, 6.0, 8.0, 5.0};
    CorporationPlan const energyPlan = planCorporation(energy, energyPrices);
    CHECK(energyPlan.targetQuantity == 1'000'000);
    CHECK(energyPlan.plannedOutput == 1'000'000);
    CHECK(energyPlan.breakEvenPrice == 9.0);
    CHECK(energyPlan.plannedCost == 9'660'000);
    auto const energyLabourOrder = findOrder(energyPlan, OrderSide::Buy, ResourceId::Labour);
    REQUIRE(energyLabourOrder != nullptr);
    CHECK(energyLabourOrder->quantity == 115'000);
    auto const energyFuelOrder = findOrder(energyPlan, OrderSide::Buy, ResourceId::Fuel);
    REQUIRE(energyFuelOrder != nullptr);
    CHECK(energyFuelOrder->quantity == 1'000'000);
}

TEST_CASE("Jump-start builds the first sector building within its raised-capital budget") {
    using namespace Simulation::Economy;

    Corporation company = corporation(SectorId::Food, MIN_CAPITAL);
    company.sectorDemand.steps = {{4, 1'000'000}};

    CorporationPlan const plan = planCorporation(company, prices());

    REQUIRE(plan.construction.size() == 1);
    CHECK(plan.construction[0].tier == BuildTier::Tier2);
    CHECK(plan.construction[0].constructionValue == 22'400'000);
    CHECK(plan.construction[0].underConstruction);
}

TEST_CASE("Zero-credit corporations liquidate and salvage owned buildings") {
    using namespace Simulation::Economy;

    Corporation company = corporation(SectorId::Food, 0);
    company.buildings = {{SectorId::Food, BuildTier::Tier1, 0, false, 14'000'000}};
    company.stock[ResourceId::Fuel] = 3;

    CorporationPlan const plan = planCorporation(company, prices());

    CHECK(plan.liquidate);
    CHECK(plan.dividend == 0);
    CHECK(plan.liquidationProceeds == 7'000'015);
    auto const fuelLiquidation = findOrder(plan, OrderSide::Sell, ResourceId::Fuel);
    REQUIRE(fuelLiquidation != nullptr);
    CHECK(fuelLiquidation->quantity == 3);
}

TEST_CASE("Profit sweep is monotone in available capacity and handles empty or unprofitable demand") {
    using namespace Simulation::Economy;

    Corporation company = corporation(SectorId::Food, 50'000'000);
    company.sectorDemand.steps = {{5, 3'000}, {4, 10'000}};
    CorporationPlan const manualOnly = planCorporation(company, prices());
    company.buildings = {{SectorId::Food, BuildTier::Tier1, 0, false, 14'000'000}};
    CorporationPlan const withTier1 = planCorporation(company, prices());
    CHECK(manualOnly.targetProfit <= withTier1.targetProfit);

    company.sectorDemand.steps.clear();
    CHECK(planCorporation(company, prices()).targetQuantity == 0);
    company.sectorDemand.steps = {{1, 1'000}};
    CHECK(planCorporation(company, prices(1)).targetQuantity == 0);
}

TEST_CASE("Tier 2 staffing stays idle when its energy bill cannot fit the plan budget") {
    using namespace Simulation::Economy;

    Corporation company = corporation(SectorId::Food, 10'000'000);
    company.buildings = {{SectorId::Food, BuildTier::Tier2, 0, false, 22'400'000}};
    company.sectorDemand.steps = {{4, 12'000'000}};
    CorporationPrices expensiveEnergy = prices();
    expensiveEnergy.inputs.energy = 500.0;

    CorporationPlan const plan = planCorporation(company, expensiveEnergy);

    CHECK(plan.plannedOutput == 2'133'000);
    CHECK(plan.staffing[0].workers == 0);
    CHECK(findOrder(plan, OrderSide::Buy, ResourceId::Energy) == nullptr);
}
