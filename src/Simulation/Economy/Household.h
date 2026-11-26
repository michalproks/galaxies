#ifndef GALAXIES_SIMULATION_ECONOMY_HOUSEHOLD_H
#define GALAXIES_SIMULATION_ECONOMY_HOUSEHOLD_H

#include "AgentId.h"
#include "EconomyConstants.h"
#include "Order.h"
#include "PriceBelief.h"
#include "Rng.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Simulation::Economy {

/** A population slice with its own credits, food reserves, spending propensities, and beliefs. */
struct Household {
    /** Stable identity used for deterministic order generation and ownership. */
    HouseholdId id{};
    /** Number of individuals; labour, survival needs, and comfort scale with this value. */
    std::int64_t population{};
    /** Global centicredit balance used to clamp all household buy orders. */
    std::int64_t credits{};
    /** Fraction of prior-turn income the household plans to spend. */
    double propIncome{};
    /** Fraction of wealth beyond prior-turn income the household plans to spend. */
    double propSavings{};
    /** Stored food kilograms used first to meet survival needs and reserve targets. */
    std::int64_t foodStock{};
    /** Belief used to price food demand and derive reserve cheapness. */
    PriceBelief foodBelief{};
    /** Belief used to price the household's full labour supply. */
    PriceBelief labourBelief{};
    /** Belief used to price instant service demand. */
    PriceBelief serviceBelief{};
    /** Belief used to price consumer-goods demand. */
    PriceBelief consumerGoodsBelief{};
    /** Rolling prices used to compute the smoothed subsistence wage. */
    std::array<std::int64_t, static_cast<std::size_t>(WAGE_SMOOTH)> foodPriceHistory{};
    /** Credits received from labour and dividends in the preceding turn. */
    std::int64_t lastIncome{};
    /** Last food settlement price, used to calculate emergency bids and reserve cheapness. */
    std::int64_t lastFoodPrice{};
};

/** Sampled willingness-to-pay and labour price inputs for one turn. */
struct HouseholdPriceSamples {
    /** Sampled food price in centicredits per kilogram; may be fractional. */
    double food{};
    /** Sampled labour price in centicredits per unit; may be fractional. */
    double labour{};
    /** Sampled service price in centicredits per unit; may be fractional. */
    double service{};
    /** Sampled consumer-goods price in centicredits per unit; may be fractional. */
    double consumerGoods{};
};

/** Deterministically assigned IDs for the household's possible market orders. */
struct HouseholdOrderIds {
    /** ID for the household's full labour sell order. */
    OrderId labour{};
    /** ID for the inelastic survival food buy order. */
    OrderId emergencyFood{};
    /** ID for the elastic comfort food buy order. */
    OrderId comfortFood{};
    /** ID for the intertemporal reserve food buy order. */
    OrderId reserveFood{};
    /** ID for the household's service buy order. */
    OrderId service{};
    /** ID for the household's consumer-goods buy order. */
    OrderId consumerGoods{};
};

/** Orders and intermediate quantities calculated for a household in one turn. */
struct HouseholdOrderPlan {
    /** Non-zero labour, food-tier, service, and consumer-goods orders in stable order. */
    std::vector<Order> orders;
    /** Credits allocated by the household's income/savings propensities. */
    std::int64_t budget{};
    /** Survival food quantity required before market purchases. */
    std::int64_t survivalNeed{};
    /** Maximum food consumed at full comfort. */
    std::int64_t comfortCeiling{};
    /** Target food consumption after applying the food budget. */
    std::int64_t consumptionTarget{};
    /** Survival order quantity after stock and credit limits. */
    std::int64_t emergencyFoodQuantity{};
    /** Remaining share allocated to comfort food and reserves. */
    std::int64_t foodShare{};
    /** Equal category share available to services. */
    std::int64_t serviceShare{};
    /** Equal category share available to consumer goods. */
    std::int64_t consumerGoodsShare{};
    /** Subsistence reservation wage, rounded up to whole centicredits. */
    std::int64_t subsistenceWage{};
    /** Desired food reserve stock and the remaining quantity gap after current stock. */
    std::int64_t reserveTarget{};
    std::int64_t reserveGap{};
    OrderId emergencyFoodOrderId{};
};

/** A resource fill and its settlement price, with quantity measured in that resource. */
struct HouseholdFill {
    /** Filled resource quantity; zero when no order from this household traded. */
    std::int64_t quantity{};
    /** Settlement price in centicredits per resource unit. */
    std::int64_t price{};
};

/** Settlement fills needed to update one household's ledger and consumption. */
struct HouseholdSettlement {
    /** Combined fills of the household's survival, comfort, and reserve food orders. */
    HouseholdFill food{};
    /** Filled service quantity. */
    HouseholdFill service{};
    /** Filled consumer-goods quantity. */
    HouseholdFill consumerGoods{};
    /** Filled labour quantity sold by the household. */
    HouseholdFill labour{};
    /** Dividends received from the household's corporation this turn. */
    std::int64_t dividends{};
};

/** Updated household state and the turn's consumption/accounting results. */
struct HouseholdTurnResult {
    /** Updated state; population below POP_MIN signals dissolution to the caller. */
    Household household{};
    /** Food eaten from market fills and household stocks. */
    std::int64_t foodEaten{};
    /** Services consumed in the purchase turn. */
    std::int64_t servicesConsumed{};
    /** Consumer goods consumed in the purchase turn. */
    std::int64_t consumerGoodsConsumed{};
    /** Credits earned from labour and dividends. */
    std::int64_t income{};
    /** True when the updated population is below POP_MIN. The caller dissolves its corporation first. */
    bool dissolved{};
    /** Actual spending by category, after applying settlement fills. */
    std::int64_t foodSpent{};
    std::int64_t servicesSpent{};
    std::int64_t consumerGoodsSpent{};
};

/**
 * Samples all four household prices uniformly from their beliefs and clamps them to market bounds.
 * Advances rng; pass the returned values to makeHouseholdOrders for pure order planning.
 * (economic-model.md — Price Beliefs and Order Submission)
 */
[[nodiscard]] HouseholdPriceSamples sampleHouseholdPrices(Household const &household, Rng &rng);

/**
 * Produces household orders without modifying the household or RNG.
 * IDs must be unique within the market. The sampled prices may be fractional, while Order stores
 * integer centicredits; order prices are rounded up so a survival bid is not weakened. Budget and
 * demand quantities follow the sampled prices, with a final credit-balance clamp.
 * (economic-model.md — Labour supply, Food demand, Services demand, Consumer goods demand, Budget)
 */
[[nodiscard]] HouseholdOrderPlan makeHouseholdOrders(Household const &household,
                                                     HouseholdPriceSamples const &priceSamples,
                                                     HouseholdOrderIds const &orderIds);

/**
 * Applies this household's fills, consumption, income, stocks, and population change to a copy.
 * Settlement quantities and prices must be non-negative and correspond to the household's orders.
 * The caller dissolves the owned corporation before transferring a dissolved household's balance/stock.
 * (economic-model.md — Income, Consumption, stocks and population, Dissolution)
 */
[[nodiscard]] HouseholdTurnResult updateHouseholdConsumption(Household const &household,
                                                             HouseholdOrderPlan const &orderPlan,
                                                             HouseholdSettlement const &settlement);

/**
 * Reports whether this household is below the population threshold for dissolution.
 * Its corporation must be liquidated before household balances/stocks are transferred.
 * (economic-model.md — Consumption, stocks and population; Dissolution)
 */
[[nodiscard]] bool shouldDissolveHousehold(Household const &household) noexcept;

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_HOUSEHOLD_H
