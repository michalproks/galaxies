#include "Household.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Simulation::Economy {

namespace {

    constexpr double FOOD_COMFORT_HEADROOM_PER_PERSON = static_cast<double>(FOOD_MAX_PER_PERSON - FOOD_PER_PERSON);

    double clampPrice(double const price) {
        if (!std::isfinite(price)) {
            throw std::invalid_argument("Household sampled prices must be finite");
        }
        return std::clamp(price, static_cast<double>(PRICE_FLOOR), static_cast<double>(PRICE_CAP));
    }

    double samplePrice(PriceBelief const &belief, Rng &rng) {
        double const halfWidth = std::max(0.0, belief.halfWidth);
        double const sample = belief.mean + (rng.uniformDouble() * 2.0 - 1.0) * halfWidth;
        return clampPrice(sample);
    }

    std::int64_t toQuantity(double const quantity) {
        if (!(quantity > 0.0)) {
            return 0;
        }
        if (!std::isfinite(quantity) || quantity >= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
            return std::numeric_limits<std::int64_t>::max();
        }
        return static_cast<std::int64_t>(std::floor(quantity));
    }

    std::int64_t toOrderPrice(double const price) {
        double const roundedPrice = std::ceil(clampPrice(price));
        return static_cast<std::int64_t>(roundedPrice);
    }

    std::int64_t multiply(std::int64_t const quantity, std::int64_t const price) noexcept {
        if (quantity <= 0 || price <= 0) {
            return 0;
        }
        if (quantity > std::numeric_limits<std::int64_t>::max() / price) {
            return std::numeric_limits<std::int64_t>::max();
        }
        return quantity * price;
    }

    std::int64_t add(std::int64_t const left, std::int64_t const right) {
        if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) {
            throw std::overflow_error("Household accounting exceeds int64_t");
        }
        if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right) {
            throw std::overflow_error("Household accounting exceeds int64_t");
        }
        return left + right;
    }

    std::int64_t floorCredits(long double const credits) {
        if (!(credits > 0.0L)) {
            return 0;
        }
        if (credits >= static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
            return std::numeric_limits<std::int64_t>::max();
        }
        return static_cast<std::int64_t>(std::floor(credits));
    }

    std::int64_t ceilCredits(long double const credits) {
        if (!(credits > 0.0L)) {
            return 0;
        }
        if (credits >= static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
            return std::numeric_limits<std::int64_t>::max();
        }
        return static_cast<std::int64_t>(std::ceil(credits));
    }

    std::int64_t appendBuyOrder(HouseholdOrderPlan &plan, Order order, std::int64_t &creditsRemaining) {
        std::int64_t const affordableQuantity = order.price > 0 ? creditsRemaining / order.price : 0;
        std::int64_t const boundedQuantity = std::min(order.quantity, affordableQuantity);
        if (boundedQuantity > 0) {
            order.quantity = boundedQuantity;
            plan.orders.push_back(order);
            creditsRemaining -= multiply(boundedQuantity, order.price);
        }
        return boundedQuantity;
    }

    std::int64_t addFills(HouseholdFill const &fill) {
        if (fill.quantity < 0 || fill.price < 0) {
            throw std::invalid_argument("Household settlement fills must be non-negative");
        }
        return multiply(fill.quantity, fill.price);
    }

} // namespace

HouseholdPriceSamples sampleHouseholdPrices(Household const &household, Rng &rng) {
    return {samplePrice(household.foodBelief, rng),
            samplePrice(household.labourBelief, rng),
            samplePrice(household.serviceBelief, rng),
            samplePrice(household.consumerGoodsBelief, rng)};
}

HouseholdOrderPlan makeHouseholdOrders(Household const &household,
                                       HouseholdPriceSamples const &priceSamples,
                                       HouseholdOrderIds const &orderIds) {
    if (household.population < 0 || household.credits < 0 || household.lastIncome < 0 || household.foodStock < 0) {
        throw std::invalid_argument("Household population, balances, and food stock must be valid");
    }
    if (!(household.propIncome >= 0.0 && household.propIncome <= 1.0) ||
        !(household.propSavings >= 0.0 && household.propSavings <= 1.0)) {
        throw std::invalid_argument("Household spending propensities must be in [0, 1]");
    }

    double const foodPrice = clampPrice(priceSamples.food);
    double const labourPrice = clampPrice(priceSamples.labour);
    double const servicePrice = clampPrice(priceSamples.service);
    double const consumerGoodsPrice = clampPrice(priceSamples.consumerGoods);
    HouseholdOrderPlan plan;
    plan.emergencyFoodOrderId = orderIds.emergencyFood;
    long double const budgetValue =
        static_cast<long double>(household.propIncome) * static_cast<long double>(household.lastIncome) +
        static_cast<long double>(household.propSavings) *
            static_cast<long double>(household.credits - household.lastIncome);
    plan.budget = std::min(household.credits, floorCredits(budgetValue));

    long double smoothedFoodPrice = 0.0L;
    for (std::int64_t const historicPrice : household.foodPriceHistory) {
        smoothedFoodPrice += static_cast<long double>(std::max<std::int64_t>(0, historicPrice));
    }
    smoothedFoodPrice /= static_cast<long double>(household.foodPriceHistory.size());
    plan.subsistenceWage =
        std::min(PRICE_CAP, ceilCredits(smoothedFoodPrice * static_cast<long double>(FOOD_PER_PERSON)));
    std::int64_t const labourOrderPrice = std::max(plan.subsistenceWage, toOrderPrice(labourPrice));
    if (household.population > 0) {
        plan.orders.push_back({orderIds.labour,
                               household.id,
                               OrderSide::Sell,
                               ResourceId::Labour,
                               std::max(PRICE_FLOOR, labourOrderPrice),
                               multiply(household.population, LABOUR_PER_PERSON)});
    }

    plan.survivalNeed = multiply(household.population, FOOD_PER_PERSON);
    plan.comfortCeiling = multiply(household.population, FOOD_MAX_PER_PERSON);
    std::int64_t const comfortHeadroom = plan.comfortCeiling - plan.survivalNeed;
    std::int64_t const emergencyGap = std::max<std::int64_t>(0, plan.survivalNeed - household.foodStock);
    std::int64_t creditsRemaining = household.credits;
    double const emergencyBid =
        std::min(static_cast<double>(PRICE_CAP),
                 std::max(foodPrice, static_cast<double>(household.lastFoodPrice) * EMERGENCY_BID));
    double const emergencyCostPrice = clampPrice(emergencyBid);
    std::int64_t const emergencyOrderPrice = toOrderPrice(emergencyCostPrice);

    if (emergencyGap > 0) {
        plan.emergencyFoodQuantity = appendBuyOrder(
            plan,
            {orderIds.emergencyFood, household.id, OrderSide::Buy, ResourceId::Food, emergencyOrderPrice, emergencyGap},
            creditsRemaining);
    }
    long double const emergencyCost = static_cast<long double>(plan.emergencyFoodQuantity) * emergencyCostPrice;
    std::int64_t const emergencyCostCredits = ceilCredits(emergencyCost);
    std::int64_t const remainingBudget = emergencyCostCredits >= plan.budget ? 0 : plan.budget - emergencyCostCredits;
    std::int64_t const categoryShare = remainingBudget / 3;
    double cheapness = 0.0;
    if (household.foodBelief.mean > 0.0) {
        cheapness =
            std::clamp(1.0 - static_cast<double>(household.lastFoodPrice) / household.foodBelief.mean, 0.0, 1.0);
    }
    double const reserveTurns = static_cast<double>(RESERVE_MAX) * cheapness;
    std::int64_t const reserveTarget = toQuantity(reserveTurns * static_cast<double>(plan.survivalNeed));
    std::int64_t const reserveGap = std::max<std::int64_t>(0, reserveTarget - household.foodStock);
    plan.reserveTarget = reserveTarget;
    plan.reserveGap = reserveGap;
    long double const foodHeadroomCost = static_cast<long double>(add(comfortHeadroom, reserveGap)) * foodPrice;
    plan.foodShare = std::min(categoryShare, floorCredits(foodHeadroomCost));
    plan.serviceShare = categoryShare;
    plan.consumerGoodsShare = categoryShare;

    double coverage = 0.0;
    if (comfortHeadroom > 0) {
        coverage = std::clamp(
            (static_cast<double>(plan.foodShare) / foodPrice) / static_cast<double>(comfortHeadroom), 0.0, 1.0);
    }
    plan.consumptionTarget = std::min(
        plan.comfortCeiling, add(plan.survivalNeed, toQuantity(static_cast<double>(comfortHeadroom) * coverage)));
    std::int64_t comfortQuantity = std::max<std::int64_t>(0, plan.consumptionTarget - plan.survivalNeed);
    comfortQuantity = std::min(comfortQuantity, toQuantity(static_cast<double>(plan.foodShare) / foodPrice));
    std::int64_t const foodOrderPrice = toOrderPrice(foodPrice);
    std::int64_t const filledComfortQuantity = appendBuyOrder(
        plan,
        {orderIds.comfortFood, household.id, OrderSide::Buy, ResourceId::Food, foodOrderPrice, comfortQuantity},
        creditsRemaining);
    std::int64_t const comfortCost = ceilCredits(static_cast<long double>(filledComfortQuantity) * foodPrice);
    std::int64_t const remainingFoodShare = std::max<std::int64_t>(0, plan.foodShare - comfortCost);
    std::int64_t const reserveQuantity =
        std::min(reserveGap, toQuantity(static_cast<double>(remainingFoodShare) / foodPrice));
    static_cast<void>(appendBuyOrder(
        plan,
        {orderIds.reserveFood, household.id, OrderSide::Buy, ResourceId::Food, foodOrderPrice, reserveQuantity},
        creditsRemaining));

    static_cast<void>(appendBuyOrder(plan,
                                     {orderIds.service,
                                      household.id,
                                      OrderSide::Buy,
                                      ResourceId::Service,
                                      toOrderPrice(servicePrice),
                                      toQuantity(static_cast<double>(plan.serviceShare) / servicePrice)},
                                     creditsRemaining));
    static_cast<void>(appendBuyOrder(plan,
                                     {orderIds.consumerGoods,
                                      household.id,
                                      OrderSide::Buy,
                                      ResourceId::ConsumerGoods,
                                      toOrderPrice(consumerGoodsPrice),
                                      toQuantity(static_cast<double>(plan.consumerGoodsShare) / consumerGoodsPrice)},
                                     creditsRemaining));

    return plan;
}

HouseholdTurnResult updateHouseholdConsumption(Household const &household,
                                               HouseholdOrderPlan const &orderPlan,
                                               HouseholdSettlement const &settlement) {
    std::int64_t const foodCost = addFills(settlement.food);
    std::int64_t const serviceCost = addFills(settlement.service);
    std::int64_t const consumerGoodsCost = addFills(settlement.consumerGoods);
    std::int64_t const labourRevenue = addFills(settlement.labour);
    if (settlement.dividends < 0 || orderPlan.consumptionTarget < 0) {
        throw std::invalid_argument("Household dividends and consumption target must be non-negative");
    }
    std::int64_t const totalCosts = add(add(foodCost, serviceCost), consumerGoodsCost);
    if (totalCosts > household.credits) {
        throw std::invalid_argument("Household settlement spends more credits than the household owns");
    }
    std::int64_t const income = add(labourRevenue, settlement.dividends);
    HouseholdTurnResult result;
    result.household = household;
    std::int64_t const availableFood = add(household.foodStock, settlement.food.quantity);
    result.foodEaten = std::min(availableFood, orderPlan.consumptionTarget);
    result.household.foodStock = availableFood - result.foodEaten;

    if (household.population > 0) {
        double const survivalNeed = static_cast<double>(household.population) * static_cast<double>(FOOD_PER_PERSON);
        double const comfortHeadroom = static_cast<double>(household.population) * FOOD_COMFORT_HEADROOM_PER_PERSON;
        double const shortfall = std::max(0.0, survivalNeed - static_cast<double>(result.foodEaten)) / survivalNeed;
        double const growth = comfortHeadroom > 0.0
                                  ? POP_GROWTH_RATE *
                                        std::max(0.0, static_cast<double>(result.foodEaten) - survivalNeed) /
                                        comfortHeadroom
                                  : 0.0;
        double const updatedPopulation = static_cast<double>(household.population) * (1.0 - shortfall + growth);
        result.household.population = updatedPopulation >= static_cast<double>(std::numeric_limits<std::int64_t>::max())
                                          ? std::numeric_limits<std::int64_t>::max()
                                          : static_cast<std::int64_t>(std::floor(updatedPopulation));
    }
    result.servicesConsumed = settlement.service.quantity;
    result.consumerGoodsConsumed = settlement.consumerGoods.quantity;
    result.foodSpent = foodCost;
    result.servicesSpent = serviceCost;
    result.consumerGoodsSpent = consumerGoodsCost;
    result.income = income;
    result.household.credits = add(household.credits - totalCosts, income);
    result.household.lastIncome = income;
    result.household.lastFoodPrice = settlement.food.price;
    for (std::size_t index = 1; index < result.household.foodPriceHistory.size(); ++index) {
        result.household.foodPriceHistory[index - 1] = result.household.foodPriceHistory[index];
    }
    if (!result.household.foodPriceHistory.empty()) {
        result.household.foodPriceHistory.back() = settlement.food.price;
    }
    result.dissolved = shouldDissolveHousehold(result.household);
    return result;
}

bool shouldDissolveHousehold(Household const &household) noexcept {
    return household.population < POP_MIN;
}

} // namespace Simulation::Economy
