#ifndef GALAXIES_SIMULATION_ECONOMY_ECONOMY_CONSTANTS_H
#define GALAXIES_SIMULATION_ECONOMY_ECONOMY_CONSTANTS_H

#include <cstdint>

namespace Simulation::Economy {

/** Individuals represented by a newly founded household (economic-model.md, Household Agents). */
inline constexpr std::int64_t POP_BASE = 1'000'000;
/** Minimum survival food in kilograms per person per turn (Household Agents). */
inline constexpr std::int64_t FOOD_PER_PERSON = 10;
/** Comfortable food ceiling in kilograms per person per turn (Household Agents). */
inline constexpr std::int64_t FOOD_MAX_PER_PERSON = 25;
/** Labour units supplied by each person per turn (Household Agents). */
inline constexpr std::int64_t LABOUR_PER_PERSON = 1;
/** Minimum population before a household is dissolved (Consumption, stocks and population). */
inline constexpr std::int64_t POP_MIN = 1'000;
/** Number of previous food prices averaged for the subsistence wage (Labour supply). */
inline constexpr std::int64_t WAGE_SMOOTH = 3;
/** Emergency food bid premium over the last food price (Food demand). */
inline constexpr double EMERGENCY_BID = 1.25;
/** Maximum turns of survival food held as reserves (Reserves). */
inline constexpr std::int64_t RESERVE_MAX = 4;
/** Fraction of corporate credits kept uncommitted for operating reserves (Production). */
inline constexpr double RESERVE_FRACTION = 0.20;
/** Maximum population growth at full comfort consumption (Consumption, stocks and population). */
inline constexpr double POP_GROWTH_RATE = 0.05;
/** Maximum per-turn probability multiplier for corporation founding (Founding). */
inline constexpr double FOUNDING_CHANCE = 0.05;
/** Minimum sector sampling weight that keeps every sector startable (Founding). */
inline constexpr std::int64_t FLOOR_WEIGHT = 1'000'000;
/** Fraction of building construction value recovered during liquidation (Dissolution). */
inline constexpr double SALVAGE_RATE = 0.5;
/** Required price margin above break-even for conditional surplus production (Production). */
inline constexpr double SURPLUS_MARGIN = 0.25;
/** Maximum number of turns allowed to recover a building's construction cost (Production). */
inline constexpr std::int64_t PAYBACK_TURNS = 10;
/** Maximum share of credits allocated to a first jump-start building (Production). */
inline constexpr double JUMPSTART_BUDGET = 0.50;
/** Dividend share of a corporation's end-of-turn balance (Income); the spec leaves it as a tuning constant. */
inline constexpr double DIVIDEND_RATE = 0.20;
/** Minimum capital in centicredits; raised from the spec's 10,000,000 so jump-starts are affordable. */
inline constexpr std::int64_t MIN_CAPITAL = 45'000'000;
/** Lowest allowed market price in centicredits per unit (Orders). */
inline constexpr std::int64_t PRICE_FLOOR = 1;
/** Highest allowed market price in centicredits per unit (Orders). */
inline constexpr std::int64_t PRICE_CAP = 1'000'000;
/** Economic turn duration in in-game days (Economic Model introduction). */
inline constexpr std::int64_t ECONOMY_TICK_DAYS = 7;
/** Economic turn duration in whole hours, using the simulation's 24-hour days. */
inline constexpr std::int64_t ECONOMY_TICK_HOURS = ECONOMY_TICK_DAYS * 24;

/** PLACEHOLDER belief reinforcement coefficient to be tuned (Price Beliefs and Order Submission). */
inline constexpr double BELIEF_ALPHA = 0.2;
/** PLACEHOLDER belief confidence shrink coefficient to be tuned (Price Beliefs and Order Submission). */
inline constexpr double BELIEF_BETA = 0.1;
/** PLACEHOLDER belief confidence expansion coefficient to be tuned (Price Beliefs and Order Submission). */
inline constexpr double BELIEF_GAMMA = 0.15;
/** PLACEHOLDER minimum belief half-width to be tuned (Price Beliefs and Order Submission). */
inline constexpr double BELIEF_W_MIN = 1.0;
/** PLACEHOLDER maximum belief half-width to be tuned (Price Beliefs and Order Submission). */
inline constexpr double BELIEF_W_MAX = PRICE_CAP / 4.0;

} // namespace Simulation::Economy

#endif // GALAXIES_SIMULATION_ECONOMY_ECONOMY_CONSTANTS_H
