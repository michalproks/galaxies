#include "Household.h"

#include "EconomyConstants.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <set>

namespace {

using namespace Simulation::Economy;

Household makeWorkedExampleHousehold() {
    Household household;
    household.id = {1};
    household.population = 1'000'000;
    household.credits = 45'000'000;
    household.propIncome = 0.8;
    household.propSavings = 0.5;
    household.foodStock = 2'000'000;
    household.foodBelief = {5.0, 0.0};
    household.labourBelief = {38.0, 0.0};
    household.serviceBelief = {10.0, 0.0};
    household.consumerGoodsBelief = {30.0, 0.0};
    household.foodPriceHistory = {4, 4, 5};
    household.lastIncome = 40'000'000;
    household.lastFoodPrice = 4;
    return household;
}

HouseholdPriceSamples makeWorkedExamplePrices(double const foodPrice) {
    return {foodPrice, 38.0, 10.0, 30.0};
}

HouseholdOrderIds makeOrderIds() {
    return {{101}, {102}, {103}, {104}, {105}, {106}};
}

Order const *findOrder(HouseholdOrderPlan const &plan, OrderId const orderId) {
    auto const found = std::find_if(
        plan.orders.begin(), plan.orders.end(), [orderId](Order const &order) { return order.id == orderId; });
    return found == plan.orders.end() ? nullptr : &*found;
}

} // namespace

TEST_CASE("Household worked example 1 submits emergency food and applies famine outcomes") {
    Household household = makeWorkedExampleHousehold();
    HouseholdOrderPlan const plan = makeHouseholdOrders(household, makeWorkedExamplePrices(5.0), makeOrderIds());

    CHECK(plan.budget == 34'500'000);
    CHECK(plan.survivalNeed == 10'000'000);
    CHECK(plan.comfortCeiling == 25'000'000);
    CHECK(plan.emergencyFoodQuantity == 8'000'000);
    CHECK(plan.consumptionTarget == 10'000'000);
    CHECK(plan.subsistenceWage == 44);
    REQUIRE(plan.orders.size() == 2);
    Order const *labour = findOrder(plan, {101});
    Order const *survival = findOrder(plan, {102});
    REQUIRE(labour != nullptr);
    REQUIRE(survival != nullptr);
    CHECK(labour->resource == ResourceId::Labour);
    CHECK(labour->side == OrderSide::Sell);
    CHECK(labour->quantity == 1'000'000);
    CHECK(labour->price == 44);
    CHECK(survival->resource == ResourceId::Food);
    CHECK(survival->quantity == 8'000'000);
    CHECK(survival->price == 5);
    CHECK(findOrder(plan, {103}) == nullptr);
    CHECK(findOrder(plan, {104}) == nullptr);
    CHECK(findOrder(plan, {105}) == nullptr);
    CHECK(findOrder(plan, {106}) == nullptr);

    HouseholdSettlement const settlement{{7'000'000, 5}, {0, 0}, {0, 0}, {900'000, 45}, 0};
    HouseholdTurnResult const result = updateHouseholdConsumption(household, plan, settlement);
    CHECK(result.foodEaten == 9'000'000);
    CHECK(result.household.foodStock == 0);
    CHECK(result.household.population == 900'000);
    CHECK(result.household.credits == 50'500'000);
    CHECK(result.household.lastIncome == 40'500'000);
    CHECK(result.income == 40'500'000);
    CHECK((result.household.foodPriceHistory == std::array<std::int64_t, 3>{4, 5, 5}));
    CHECK(!(result.dissolved));
}

TEST_CASE("Household worked example 2 spends shares on comfort and utility while stocked") {
    Household household = makeWorkedExampleHousehold();
    household.foodStock = 12'000'000;
    household.foodBelief.mean = 4.0;
    household.foodPriceHistory = {4, 4, 4};
    HouseholdOrderPlan const plan = makeHouseholdOrders(household, makeWorkedExamplePrices(4.0), makeOrderIds());

    CHECK(plan.budget == 34'500'000);
    CHECK(plan.emergencyFoodQuantity == 0);
    CHECK(plan.foodShare == 11'500'000);
    CHECK(plan.serviceShare == 11'500'000);
    CHECK(plan.consumerGoodsShare == 11'500'000);
    CHECK(plan.consumptionTarget == 12'875'000);
    Order const *comfort = findOrder(plan, {103});
    Order const *services = findOrder(plan, {105});
    Order const *consumerGoods = findOrder(plan, {106});
    REQUIRE(comfort != nullptr);
    REQUIRE(services != nullptr);
    REQUIRE(consumerGoods != nullptr);
    CHECK(comfort->quantity == 2'875'000);
    CHECK(comfort->price == 4);
    CHECK(services->quantity == 1'150'000);
    CHECK(services->price == 10);
    CHECK(consumerGoods->quantity == 383'333);
    CHECK(consumerGoods->price == 30);
    CHECK(findOrder(plan, {102}) == nullptr);
    CHECK(findOrder(plan, {104}) == nullptr);

    HouseholdSettlement const settlement{{2'875'000, 4}, {1'150'000, 10}, {383'333, 30}, {0, 40}, 0};
    HouseholdTurnResult const result = updateHouseholdConsumption(household, plan, settlement);
    CHECK(result.foodEaten == 12'875'000);
    CHECK(result.household.foodStock == 2'000'000);
    CHECK(result.household.population == 1'009'583);
    CHECK(result.servicesConsumed == 1'150'000);
    CHECK(result.consumerGoodsConsumed == 383'333);
}

TEST_CASE("Household worked example 3 prioritizes emergency food before comfort and reserve demand") {
    Household household = makeWorkedExampleHousehold();
    household.foodBelief.mean = 4.0;
    household.lastFoodPrice = 2;
    HouseholdOrderPlan const plan = makeHouseholdOrders(household, makeWorkedExamplePrices(2.5), makeOrderIds());

    CHECK(plan.budget == 34'500'000);
    CHECK(plan.emergencyFoodQuantity == 8'000'000);
    CHECK(plan.foodShare == 4'833'333);
    CHECK(plan.serviceShare == 4'833'333);
    CHECK(plan.consumerGoodsShare == 4'833'333);
    CHECK(plan.consumptionTarget == 11'933'333);
    Order const *survival = findOrder(plan, {102});
    Order const *comfort = findOrder(plan, {103});
    Order const *services = findOrder(plan, {105});
    Order const *consumerGoods = findOrder(plan, {106});
    REQUIRE(survival != nullptr);
    REQUIRE(comfort != nullptr);
    REQUIRE(services != nullptr);
    REQUIRE(consumerGoods != nullptr);
    CHECK(survival->quantity == 8'000'000);
    CHECK(survival->price == 3);
    CHECK(comfort->quantity == 1'933'333);
    CHECK(comfort->price == 3);
    CHECK(services->quantity == 483'333);
    CHECK(consumerGoods->quantity == 161'111);
    CHECK(findOrder(plan, {104}) == nullptr);
    CHECK(survival->quantity + comfort->quantity == 9'933'333);
}

TEST_CASE("Household worked example 4 converts cheap food savings into services and consumer goods") {
    Household household = makeWorkedExampleHousehold();
    household.foodStock = 10'000'000;
    household.foodBelief = {2.0, 0.0};
    household.foodPriceHistory = {2, 2, 2};
    household.lastFoodPrice = 2;
    HouseholdPriceSamples const prices{2.0, 38.0, 10.0, 30.0};
    HouseholdOrderPlan const plan = makeHouseholdOrders(household, prices, makeOrderIds());

    CHECK(plan.budget == 34'500'000);
    CHECK(plan.foodShare == 11'500'000);
    CHECK(plan.consumptionTarget == 15'750'000);
    Order const *comfort = findOrder(plan, {103});
    Order const *services = findOrder(plan, {105});
    Order const *consumerGoods = findOrder(plan, {106});
    REQUIRE(comfort != nullptr);
    REQUIRE(services != nullptr);
    REQUIRE(consumerGoods != nullptr);
    CHECK(comfort->quantity == 5'750'000);
    CHECK(comfort->price == 2);
    CHECK(services->quantity == 1'150'000);
    CHECK(consumerGoods->quantity == 383'333);

    HouseholdSettlement const settlement{{5'750'000, 2}, {1'150'000, 10}, {383'333, 30}, {0, 40}, 0};
    HouseholdTurnResult const result = updateHouseholdConsumption(household, plan, settlement);
    CHECK(result.foodEaten == 15'750'000);
    // Applying foodStock' = stock + fills - foodEaten yields zero; the example's stated 5,000,000 is arithmetically
    // inconsistent.
    CHECK(result.household.foodStock == 0);
    // Applying the exact growth ratio before rounding down gives 1,019,166; the example rounds the displayed ratio
    // first.
    CHECK(result.household.population == 1'019'166);
}

TEST_CASE("Household uses food-share remainder to build cheap-food reserves") {
    Household household = makeWorkedExampleHousehold();
    household.foodStock = 10'000'000;
    household.foodBelief = {4.0, 0.0};
    household.lastFoodPrice = 1;
    household.credits = 1'000'000'000;
    household.lastIncome = 1'000'000'000;
    household.propIncome = 1.0;
    household.propSavings = 1.0;
    HouseholdPriceSamples const prices{1.0, 40.0, 10.0, 30.0};

    HouseholdOrderPlan const plan = makeHouseholdOrders(household, prices, makeOrderIds());
    Order const *comfort = findOrder(plan, {103});
    Order const *reserves = findOrder(plan, {104});
    REQUIRE(comfort != nullptr);
    REQUIRE(reserves != nullptr);
    CHECK(plan.emergencyFoodQuantity == 0);
    CHECK(comfort->quantity == 15'000'000);
    CHECK(reserves->quantity == 20'000'000);
    CHECK(reserves->price == 1);
}

TEST_CASE("Household starvation dissolves only below the population threshold") {
    Household household = makeWorkedExampleHousehold();
    household.population = POP_MIN;
    household.credits = 0;
    household.lastIncome = 0;
    household.foodStock = 0;
    household.foodPriceHistory = {1, 1, 1};
    HouseholdOrderPlan plan;
    plan.consumptionTarget = 0;
    HouseholdSettlement const settlement{{0, 1}, {0, 0}, {0, 0}, {0, 0}, 0};

    HouseholdTurnResult const result = updateHouseholdConsumption(household, plan, settlement);
    CHECK(result.household.population == 0);
    CHECK(result.dissolved);
    CHECK(shouldDissolveHousehold(result.household));

    household.population = POP_MIN;
    CHECK(!(shouldDissolveHousehold(household)));
}

TEST_CASE("Household order plans preserve market shape, price, quantity, and credit invariants") {
    Household household = makeWorkedExampleHousehold();
    HouseholdOrderPlan const plan = makeHouseholdOrders(household, makeWorkedExamplePrices(2.5), makeOrderIds());
    std::set<std::int64_t> orderIds;
    std::int64_t buyCost = 0;
    int labourSells = 0;
    int foodBuys = 0;
    int serviceBuys = 0;
    int consumerGoodsBuys = 0;

    for (Order const &order : plan.orders) {
        CHECK(order.owner == AgentRef{household.id});
        CHECK(order.quantity > 0);
        CHECK(order.price >= PRICE_FLOOR);
        CHECK(order.price <= PRICE_CAP);
        CHECK(orderIds.insert(order.id.value).second);
        if (order.side == OrderSide::Sell) {
            CHECK(order.resource == ResourceId::Labour);
            ++labourSells;
        } else {
            buyCost += order.quantity * order.price;
            switch (order.resource) {
            case ResourceId::Food:
                ++foodBuys;
                break;
            case ResourceId::Service:
                ++serviceBuys;
                break;
            case ResourceId::ConsumerGoods:
                ++consumerGoodsBuys;
                break;
            default:
                FAIL("Households do not buy other resources");
            }
        }
    }

    CHECK(labourSells == 1);
    CHECK(foodBuys <= 3);
    CHECK(serviceBuys <= 1);
    CHECK(consumerGoodsBuys <= 1);
    CHECK(buyCost <= household.credits);
}
