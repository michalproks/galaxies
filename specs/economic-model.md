# Economic Model

The economy is based in economic ticks or turns. There is one economic turn at the end of each 7-day period of in-game time.

We're running an agent-based economic model where individual agents produce and consume resources and consume them on a market.

The high-level shape of the implementation is specified in [economy-implementation.md](./economy-implementation.md). The data the player and the developer see is specified in [economy-visualization.md](./economy-visualization.md).

## Resources

There are three types of resources:

1. **Virtual** resources are available in global pool and can be consumed anywhere without any need for transport.
2. **Instant** resources must be consumed at the same location where they are produced. Instant resources cannot be transported and must be consumed in the same economic turn they get produced, otherwise they get destroyed.
3. **Standard** resources are the usual kind which can be stored indefinitely and can be transported to different location for consumption.

Internally all resource amounts are represented as `std::int64_t`.

The actual resource in the game:

- **Credits**: a virtual resource representing the in-game currency.
- **Labour**: an instant resource representing the available labour that can be used to produce something.
- **Energy**: an instant resource representing the electrical (and similar) energy to be consumed.
- **Food**: a standard resource needed as a basic sustenance for the population.
- **Materials**: a standard resource representing various materials used for construction of everything else.
- **Fuel**: a standard resource representing any kind of fuel that can be converted to energy.
- **Service**: an instant resource produced from pure labour (transport, maintenance, entertainment, personal services). Consumed in the turn it is produced.
- **Consumer Goods**: a standard resource of finished goods the population buys and uses. Made from labour + materials.

The amounts of individual resources are expressed in units of:

- **Credits** in centicredits, so `uint64_t` value of `1` represents `0.01` credits and value of `147` represents `1.47` credits.
- **Labour**, **Energy** and **Service** are dimensionless.
- **Food**, **Materials**, **Fuel** and **Consumer Goods** are represented in kilograms, but in game expressed as metric tons (i.e. `1` maps to `0.001 tons` and `1234` maps to `1.234 tons`). One consumer good is 1 kg of finished product, so consumer goods are counted in the same kilogram units as the materials they are made from.

## Agents

At this moment there exist just two kinds of agents in the economic model:

1. **Households**: Each household represents a slice of population of some colony that shares similar beliefs and economic status.
2. **Corporations**: Started and owned by households. Each corporation is owned by a single household and each household can own at most one corporation. Each corporation focuses on a single **economic sector** — one specific resource it targets for production. Like households, corporations exist in a single colony and trade only on that colony's market.

### Household Agents

A household is a group of individuals living in exactly one colony. Each household has a **population**, and all of its per-turn flows scale with it.

```cpp
// Constants
constexpr int64_t POP_BASE       = 1'000'000;  // individuals per household at founding
constexpr int64_t FOOD_PER_PERSON = 10;        // kg of food an individual must eat per turn to survive
constexpr int64_t FOOD_MAX_PER_PERSON = 25;    // kg an individual eats when the household is comfortable
constexpr int64_t LABOUR_PER_PERSON = 1;       // units of labour supplied per individual per turn
constexpr int64_t POP_MIN        = 1'000;      // household dissolved below this population
constexpr int64_t WAGE_SMOOTH    = 3;          // turns averaged into the subsistence wage price
constexpr float   EMERGENCY_BID  = 1.25f;      // premium over last food price when buying to survive
constexpr int64_t RESERVE_MAX    = 4;          // maximum turns of food reserves a household targets
constexpr float   POP_GROWTH_RATE = 0.05f;     // population growth per turn at full comfort consumption

struct Household {
    AgentId      id;
    std::int64_t population;    // individuals
    std::int64_t credits;       // global balance (virtual resource)
    std::int64_t lastIncome;    // credits received in the previous turn
    std::int64_t foodStock;     // kg of food held in the household's colony
    float        propIncome;    // propensity to spend income, in [0, 1]
    float        propSavings;   // propensity to spend savings, in [0, 1]
    PriceBelief  foodBelief;    // see "Price Beliefs and Order Submission"
    PriceBelief  labourBelief;
    PriceBelief  serviceBelief;
    PriceBelief  consumerGoodsBelief;
    // rolling window of the last WAGE_SMOOTH food settlement prices
    std::array<std::int64_t, WAGE_SMOOTH> foodPriceHistory;
};
```

The household's colony is **implicit**: households are stored in the colony's data structures, so the owning colony is known from the containing colony and is not stored in the household. Food stocks are part of the colony's standard-resource inventory, so the resource-conservation invariant counts household stocks.

`propIncome` and `propSavings` are drawn uniformly from `[0, 1]` at creation and stay fixed for the household's lifetime. `propIncome` is the fraction of last turn's income the household wants to spend; `propSavings` is the fraction of its wealth beyond last turn's income it wants to spend.

Per turn, the household's flows are:

$$\text{labour supply} = \text{population} \times \text{LABOUR\_PER\_PERSON}$$
$$\text{survival need} = \text{population} \times \text{FOOD\_PER\_PERSON}$$
$$\text{comfort ceiling} = \text{population} \times \text{FOOD\_MAX\_PER\_PERSON}$$
$$\text{budget} = \text{propIncome} \times \text{lastIncome} + \text{propSavings} \times (\text{credits} - \text{lastIncome})$$
$$\text{foodShare} = \text{serviceShare} = \text{consumerGoodsShare} = \frac{\text{budget} - \text{emergencyFoodCost}}{3}$$

At founding, `population = POP_BASE` and `foodStock = 0`, so a household supplies 1 000 000 units of labour, must eat 10 000 000 kg (10 000 tons) of food per turn, and eats up to 25 000 000 kg when it is comfortable.

#### Labour supply

Labour is an instant resource: unsold labour is destroyed at the end of the turn, so offering it costs nothing. The household **always offers its entire labour supply**.

The household sets a **subsistence (reservation) wage**: one unit of labour must feed one individual, so

$$w_{\text{subsistence}} = \text{smoothedFoodPrice} \times \text{FOOD\_PER\_PERSON}$$

where `smoothedFoodPrice` is the **moving average of the last `WAGE_SMOOTH` food settlement prices**, not last turn's price. The order price is sampled from the labour belief range and then raised to at least `w_subsistence`. Orders below the subsistence wage are never submitted — the household will not sell labour it cannot feed itself with.

Smoothing is what keeps a famine from becoming a wage/price spiral. A one-turn food spike raises the wage floor by only `1 / WAGE_SMOOTH`, so corporations' labour costs do not jump with the spike, their break-even prices stay reachable, and they can keep producing into the shortage. If the wage floor tracked the raw settlement price, a price spike would raise every corporation's break-even, force them to withhold supply, and spike the price further.

#### Food demand

Food demand has **three parts**, submitted as up to three orders:

**1. Survival order (inelastic, priced above the market).** If `foodStock < survival need`, the household must buy the shortfall this turn. It submits a survival order for `survival need − foodStock` kg, priced at

$$p_{\text{survival}} = \max(\tilde{p},\ \text{lastFoodPrice} \times \text{EMERGENCY\_BID})$$

clamped to `PRICE_CAP`, where `p̃` is the price sampled from the food belief. **The budget clamp does not apply to the survival order** — a household at risk of starvation spends everything it has on food. It is clamped only by the submission-time credit limit.

Pricing above the settlement price gives the survival order **rationing priority**: the long side is rationed pro-rata among orders eligible at the settlement price, and orders priced below it are excluded. A survival order priced above the market is eligible at every settlement price up to its bid, so it is never excluded, while comfort orders are excluded as soon as the price rises above them. This is the mechanism that lets a shortage produce a real price spike: `Qd(p)` no longer collapses at high prices, so the settlement price can rise far enough to make food production attractive.

**2. Comfort consumption (elastic, budget-limited).** The household eats between `FOOD_PER_PERSON` and `FOOD_MAX_PER_PERSON` kg per individual, depending on what it can afford. Let `foodShare` be the household's food budget for this turn (see the Budget section — one third of the budget in a non-emergency turn). At the sampled price `p̃`:

$$\text{coverage} = \min\left(1, \frac{\text{foodShare} / \tilde{p}}{\text{comfort ceiling} - \text{survival need}}\right)$$
$$\text{consumption target} = \text{population} \times \left(\text{FOOD\_PER\_PERSON} + (\text{FOOD\_MAX\_PER\_PERSON} - \text{FOOD\_PER\_PERSON}) \times \text{coverage}\right)$$

`coverage` is the fraction of the household's **comfort headroom** (the 15 kg per person between the minimum and the maximum) that its food budget can buy. At full affordability the household eats 25 kg per person; at a quarter affordability it eats 13.75 kg. Comfort quantity is `consumption target − survival need`, ordered at the sampled price `p̃`, clamped by `foodShare`.

This turns the food demand curve from a vertical step into a **downward-sloping curve**: a household that can afford more eats more, and eating above the minimum is partly waste. Corporations' profit-maximising rule now has a real trade-off — raising the price loses comfort quantity, lowering it sells more.

**3. Reserves (intertemporal, budget-limited).** The household targets a food stock of `reserveTurns` turns of its survival need, where the target depends on how cheap food is relative to what the household expects:

$$\text{cheapness} = 1 - \frac{\text{lastFoodPrice}}{\text{foodBelief.mean}}$$
$$\text{reserveTurns} = \text{clamp}\left(\text{RESERVE\_MAX} \times \text{cheapness},\ 0,\ \text{RESERVE\_MAX}\right)$$
$$\text{reserve target} = \text{reserveTurns} \times \text{survival need}$$
$$\text{reserve gap} = \max(0,\ \text{reserve target} - \text{foodStock})$$

The reserve gap is ordered at the sampled price `p̃`, clamped by the food share. When food is expensive relative to the household's expectation, `cheapness` is negative, the reserve target is zero, and the household draws down its stock instead of buying. When food is cheap, it stocks up to 4 turns ahead.

Reserves compete with `propSavings` for the budget, so **stocking up competes with funding corporation founding** — a colony that spends its savings on food reserves founds fewer corporations. That is a real trade-off the player can see, not a bug.

#### Services demand

Services are an instant resource: the household consumes them in the turn it buys them, and there is no service stock. Demand is purely budget-driven — services have no survival requirement, so their quantity is simply what the household's service budget can afford at the sampled price:

$$\text{service quantity} = \frac{\text{serviceShare}}{\tilde{p}_{\text{service}}}$$

clamped by the remaining credits. The household submits one buy order for that quantity at the sampled service price. There is no elasticity curve and no minimum: if the service price is high, the household buys fewer services; if it is low, it buys more. Demand scales linearly with population, because `serviceShare` is derived from income, which scales with population.

#### Consumer goods demand

Consumer goods are a standard resource, but households **consume them in the turn they buy them** — there is no household consumer-goods stock. Demand is budget-driven in the same way as services:

$$\text{consumer goods quantity} = \frac{\text{consumerGoodsShare}}{\tilde{p}_{\text{cg}}}$$

clamped by the remaining credits. Consumer goods leave the colony's inventory when they are consumed, so consumer goods are the second exit path from the colony's standard-resource inventory (the first being food eaten).

Consumer goods do not affect population growth. Population growth stays driven by food alone; services and consumer goods are pure utility demand.

#### Budget

The household's spending budget for this turn is:

$$\text{budget} = \text{propIncome} \times \text{lastIncome} + \text{propSavings} \times (\text{credits} - \text{lastIncome})$$

Since `propIncome, propSavings ∈ [0, 1]`, `budget ≤ credits` always, so the budget is never more demanding than the market's submission-time credit clamp.

The budget is divided into **three equal category shares** — food, services, consumer goods:

$$\text{foodShare} = \text{serviceShare} = \text{consumerGoodsShare} = \frac{\text{budget} - \text{emergencyFoodCost}}{3}$$

where `emergencyFoodCost` is the cost of the survival order, **subtracted from the budget first**. The survival order bypasses the budget clamp and is bounded only by credits, so `emergencyFoodCost` can exceed `budget`; in that case the remaining budget is zero and all three shares are zero.

The food share is then **clamped to the household's food needs** (in credits, at the sampled food price):

$$\text{foodShare} = \text{clamp}\left(\text{foodShare},\ 0,\ \left((\text{comfort ceiling} - \text{survival need}) + \text{reserve gap}\right) \times \tilde{p}_{\text{food}}\right)$$

where the **reserve gap** is the food still needed to reach the 4-turn reserve target:

$$\text{reserve gap} = \max\left(0,\ \text{reserve target} - \text{foodStock}\right)$$

The upper bound is the cost of eating up to the comfort ceiling **and** reaching the reserve target: the household never buys more than 25 kg per person, and never buys more than 4 turns of food ahead. The lower bound is zero, because the survival need is already paid for by the emergency order, which bypasses the budget — **the guarantee that the household never buys less than 10 kg per person comes from the emergency order, not from the food share.**

Comfort consumption and reserves compete **within** the food share: comfort first, reserves second, both clamped by the share.

**The food share is always fully spent, so the services and consumer goods shares stay at one third of the remaining budget.** This follows from the clamp: if the share is below the household's food headroom, comfort consumption uses exactly the whole share; if the share is above the headroom, it is clamped to the headroom and comfort plus reserves use exactly the clamped share. There is never a leftover food share to redistribute, so no rollover rule is needed.

Spending order within a turn: emergency food (bypassing the budget), then comfort food, then reserves, then services, then consumer goods. Unspent budget is retained as savings.

The three-way split is the mechanism that makes consumerism visible in the colony. A colony with high food prices spends most of its budget on food and buys few services and consumer goods, so its service and consumer goods industries stay small. A well-fed colony with cheap food has a large non-food budget, so service and consumer goods corporations get founded and grow.

**Emergency bidding is a rationing priority, not a guarantee of affordability.** A household with no credits cannot buy food at any price, and it starves. The survival order wins the queue; it does not create money. In a famine, the poor starve first — that is the intended behaviour.

**Tuning constraint:** with a persistent shortage, the survival bid price ratchets upward by `EMERGENCY_BID` each turn (the bid is `lastFoodPrice × EMERGENCY_BID`, and last turn's settlement price is the bid). `EMERGENCY_BID` must be small enough that the supply response — manual food production becoming profitable, new food corporations being founded — breaks the shortage before the price runs away.

#### Income

Household income in a turn is the sum of:

1. Credits from selling labour on the colony market.
2. **Dividends** paid by the corporation it owns.

The corporation retains its profits; dividends are a fraction `DIVIDEND_RATE` of the corporation's end-of-turn credits (a tuning constant). Corporate behaviour is specified separately.

#### Consumption, stocks and population

Food eaten this turn comes from the market **and from the household's own stock**:

$$\text{foodEaten} = \min(\text{foodFilled} + \text{foodStock},\ \text{consumption target})$$
$$\text{foodStock}' = \text{foodStock} + \text{foodFilled} - \text{foodEaten}$$

Population responds to how much of the survival need was actually eaten:

$$\text{shortfall} = \frac{\max(0,\ \text{survival need} - \text{foodEaten})}{\text{survival need}}$$
$$\text{growth} = \text{POP\_GROWTH\_RATE} \times \frac{\max(0,\ \text{foodEaten} - \text{survival need})}{\text{comfort ceiling} - \text{survival need}}$$
$$\text{population}' = \text{population} \times (1 - \text{shortfall} + \text{growth})$$

`population` is rounded down. A household whose population falls below `POP_MIN` is dissolved. **Its corporation is dissolved first** (see the Dissolution section), so the corporation's liquidation proceeds are added to the household's credits; the household's credits and stocks then stay in the colony and are transferred to the colony's remaining households.

This closes two loops that point in opposite directions:

- **Famine shrinks the colony.** A shrinking population supplies less labour, which lowers its income, which lowers its food demand.
- **Plenty grows the colony.** Eating above the survival minimum grows the population, which raises both labour supply and food demand next turn. A colony that overproduces food and drives prices down grows, and its extra demand pulls prices back up.

Stocks damp both loops. A household with 4 turns of reserves does not bid at the famine price, so the same shortage produces a smaller spike in a stocked colony than in a colony with no stocks.

Services and consumer goods are consumed in the turn they are bought:

$$\text{servicesConsumed} = \text{servicesFilled}, \qquad \text{consumerGoodsConsumed} = \text{consumerGoodsFilled}$$

Nothing is stored, nothing carries over. Consumer goods consumed are removed from the colony's inventory, so consumer goods are the second exit path from the colony's standard-resource stock (the first is food eaten). Services are an instant resource, so unsold service supply is destroyed at the end of the turn regardless of consumption.

Utility consumption does not feed back into population. A household that buys services and consumer goods is happier, but happiness is not modelled; population growth stays driven by food alone.

#### Self-consistency of the subsistence wage

The subsistence wage ties the labour and food markets together. If a household sells all of its labour at `w_subsistence`, its income is exactly

$$\text{population} \times \text{smoothedFoodPrice} \times \text{FOOD\_PER\_PERSON} = \text{survival need} \times \text{smoothedFoodPrice}$$

which is the cost of buying all of its survival food at the smoothed price. So **full employment in the labour market is approximately the condition for full survival food consumption** — the approximation comes from the wage being based on the smoothed price rather than the current settlement price.

Elastic consumption deliberately breaks the exact one-to-one mapping: a household with full employment can eat up to 2.5× its survival need, and a household with reserves can eat its full need without buying anything this turn. The wage floor is no longer the exact break-even point of the food market, but it remains the single most important variable in a colony's economy, because it sets the floor for the labour price and therefore the break-even cost of every corporation.

#### Worked examples

**Example 1 — a household in emergency mode.** Household H1, population 1 000 000, colony A. Last turn: food settled at 4 centicredits/kg, labour at 40 centicredits/unit. `lastIncome = 40 000 000`, `credits = 45 000 000`, `propIncome = 0.8`, `propSavings = 0.5`, `foodStock = 2 000 000` kg, `foodPriceHistory = [4, 4, 5]` → `smoothedFoodPrice = 4.33`, `foodBelief.mean = 5`.

- `budget = 0.8 × 40 000 000 + 0.5 × (45 000 000 − 40 000 000) = 34 500 000` centicredits.
- Survival need = 10 000 000 kg. `foodStock = 2 000 000 < 10 000 000` → **emergency mode**. Survival order = `10 000 000 − 2 000 000 = 8 000 000` kg at `max(p̃, 4 × 1.25) = max(5, 5) = 5`. Cost `8 000 000 × 5 = 40 000 000` — **more than the budget**, allowed because the budget clamp is bypassed; it is within the 45 000 000 credit limit.
- Remaining budget = `max(0, 34 500 000 − 40 000 000) = 0` → all three shares are 0. `coverage = 0` → consumption target = 10 000 000 kg (minimum only), so no comfort order. Reserve target = `4 × (1 − 4/5) × 10 000 000 = 8 000 000` kg, gap = 6 000 000 kg, no budget left → no reserve order. No services, no consumer goods.
- Subsistence wage = `4.33 × 10 = 43.3` → 44 centicredits/unit. The household samples a labour price of 38 and raises it to 44. Labour order: 1 000 000 units at 44.

Suppose food supply is scarce and the market settles food at 5, filling 7 000 000 kg of the survival order, and labour settles at 45, filling 900 000 units:

- `foodEaten = min(7 000 000 + 2 000 000, 10 000 000) = 9 000 000` kg → `foodStock' = 2 000 000 + 7 000 000 − 9 000 000 = 0`.
- `shortfall = (10 000 000 − 9 000 000) / 10 000 000 = 0.1`, `growth = 0` → `population' = 900 000`.
- Food payment = `7 000 000 × 5 = 35 000 000`; labour income = `900 000 × 45 = 40 500 000`.
- End-of-turn credits = `45 000 000 − 35 000 000 + 40 500 000 = 50 500 000`; `lastIncome = 40 500 000`.
- Next turn: labour supply 900 000 units, survival need 9 000 000 kg, comfort ceiling 22 500 000 kg, `foodStock = 0` → emergency mode again. The settlement price rose to the emergency bid price, which is exactly what makes food production profitable and pulls supply in.

**Example 2 — a stocked household, no emergency.** Same household, `foodStock = 12 000 000` kg (1.2 turns), `foodPriceHistory = [4, 4, 4]`, `foodBelief.mean = 4`.

- Survival need = 10 000 000 kg; `foodStock = 12 000 000 ≥ 10 000 000` → **no emergency**, budget clamp applies.
- Shares: `34 500 000 / 3 = 11 500 000` each for food, services and consumer goods.
- `cheapness = 1 − 4/4 = 0` → `reserveTurns = 0` → reserve target 0, reserve gap 0. Food share upper bound = `(15 000 000 + 0) × 4 = 60 000 000`, so the food share stays 11 500 000.
- `coverage = min(1, (11 500 000 / 4) / 15 000 000) = 0.192` → consumption target = `1 000 000 × (10 + 15 × 0.192) = 12 875 000` kg; comfort quantity = 2 875 000 kg at 4, cost 11 500 000 — **the whole food share is spent on comfort**, so no reserve order.
- Services: `11 500 000 / 10 = 1 150 000` units. Consumer goods: `11 500 000 / 30 = 383 333` units.
- Subsistence wage = `4 × 10 = 40`.
- `foodEaten = min(2 875 000 + 12 000 000, 12 875 000) = 12 875 000` kg → `foodStock' = 12 000 000 + 2 875 000 − 12 875 000 = 2 000 000` kg.
- `shortfall = 0`; `growth = 0.05 × 2 875 000 / 15 000 000 = 0.00958` → `population' = 1 009 583`. A stocked household eats above its minimum, spends two thirds of its budget on services and goods, and grows modestly.

**Example 3 — cheap food, stocking up.** Same household, `foodStock = 2 000 000` kg, last food price 2, `foodBelief.mean = 4`. Since `2 000 000 < 10 000 000`, emergency mode applies.

- Survival order: 8 000 000 kg at `max(p̃, 2 × 1.25 = 2.5) = 2.5`, cost 20 000 000. Remaining budget = `34 500 000 − 20 000 000 = 14 500 000`.
- Shares: `14 500 000 / 3 = 4 833 333` each.
- `cheapness = 1 − 2/4 = 0.5` → `reserveTurns = 2` → reserve target = 20 000 000 kg, gap = 18 000 000 kg, upper bound = `18 000 000 × 2.5 = 45 000 000` → the food share stays 4 833 333.
- `coverage = min(1, (4 833 333 / 2.5) / 15 000 000) = min(1, 1 933 333 / 15 000 000) = 0.129` → consumption target = `1 000 000 × (10 + 15 × 0.129) = 11 933 333` kg; comfort quantity = 1 933 333 kg at 2.5, cost 4 833 333 — **the whole food share is consumed by comfort**, so there is nothing left for a reserve order.
- Services and consumer goods shares stay at 4 833 333 each.
- Total food ordered: `8 000 000 + 1 933 333 = 9 933 333` kg. The household eats close to its minimum and buys little else, because a famine-price food bill eats two thirds of its budget.

**Example 4 — a well-fed colony, budget freed for services and goods.** Same household, `foodStock = 10 000 000` kg (exactly 1 turn), `foodPriceHistory = [2, 2, 2]`, `foodBelief.mean = 2`, `serviceBelief.mean = 10`, `consumerGoodsBelief.mean = 30`.

- No emergency. `budget = 34 500 000`, shares = 11 500 000 each.
- `cheapness = 1 − 2/2 = 0` → reserve target 0, reserve gap 0. Food share upper bound = `15 000 000 × 2 = 30 000 000`, so the food share stays 11 500 000.
- Comfort: `coverage = min(1, (11 500 000 / 2) / 15 000 000) = min(1, 5 750 000 / 15 000 000) = 0.383` → consumption target = `1 000 000 × (10 + 15 × 0.383) = 15 750 000` kg; comfort quantity = 5 750 000 kg at 2, cost 11 500 000 — exactly the food share.
- Services: `11 500 000 / 10 = 1 150 000` units. Consumer goods: `11 500 000 / 30 = 383 333` units.
- `foodEaten = min(5 750 000 + 10 000 000, 15 750 000) = 15 750 000` kg → `foodStock' = 5 000 000` kg. `growth = 0.05 × 5 750 000 / 15 000 000 = 0.01917` → `population' = 1 019 170`.

This is the consumerism loop: cheap food means the household's food share is spent on comfort, and the other two thirds of the budget become demand for services and consumer goods, which makes those sectors profitable and attracts new corporations.

### Corporation Agents

A corporation is a production agent owned by exactly one household, operating in the same colony as its owner. It buys inputs on the colony market, transforms them into its **sector** resource, sells the output, and keeps its profits.

Like a household, a corporation exists in **exactly one colony** and trades **only on that colony's market**. It cannot buy or sell in any other colony's market, and its buildings, stocks and production are all located in that colony. A corporation that wants to operate in another colony must be founded there as a separate corporation. Inter-colony (interplanetary) megacorporations that span multiple colonies are out of scope for now.

```cpp
struct Corporation {
    AgentId      id;
    ResourceId   sector;       // the single resource this corporation produces
    std::int64_t credits;      // global balance (virtual resource)
    std::int64_t lastIncome;   // credits received in the previous turn
    PriceBelief  sectorBelief; // price belief for the sector resource
    DemandCurve  sectorDemand; // last turn's buy order book for the sector resource
    std::int64_t stockUnitCost;// average cost per unit of the current sector stock
    // owned buildings, resource stocks
};

struct DemandCurve {
    // Last turn's buy orders for the sector resource, sorted by descending price.
    // Qd(p) = sum of the quantities of all steps with price >= p.
    std::vector<std::pair<std::int64_t, std::int64_t>> steps;  // (price, quantity)
};
```

Constants used by corporations (`MIN_CAPITAL`, `FOUNDING_CHANCE`, `FLOOR_WEIGHT`, `SALVAGE_RATE`) are declared in the Founding section below; planning constants (`RESERVE_FRACTION`, `SURPLUS_MARGIN`, `PAYBACK_TURNS`, `JUMPSTART_BUDGET`) are declared in the Production section.

Both relationships are **implicit**: the corporation is stored in the owning household's data structures, and the household is stored in its colony. So the corporation's owner and its colony are known from the containing structures and are not stored in the corporation.

Ownership is one-to-one in one direction: a corporation has exactly one owning household, and a household owns **at most one** corporation.

#### Sector

A corporation's sector is one of **Food**, **Materials**, **Fuel**, **Energy**, **Service** or **Consumer Goods**. It is sampled at founding (see the Founding section below) and is **fixed for the corporation's lifetime** — a corporation never changes what it produces.

The sector is enforced **strictly**:

- A corporation may **produce only its sector resource**.
- It may buy any resource as an input (labour, energy, materials, fuel, food for its workers).
- It may **sell only its sector resource**.
- It may build only buildings that produce its sector resource.

The consequence is that **power plants belong to Energy-sector corporations**: a food corporation cannot generate its own energy, it must buy energy on the market from an energy corporation. Energy is the one instant resource that is traded between corporations.

##### Service corporations

Service corporations are a degenerate case, and deliberately so. Services are produced from **pure labour with no buildings**, so a service corporation has:

- **No buildings, no construction, no maintenance.** Its entire budget goes to buying labour.
- **No inventory valve.** Services are an instant resource: unsold service supply is destroyed at the end of the turn, so a service corporation cannot withhold supply to defend its price.
- **No jump-start.** There is nothing to build, so the jump-start rule never applies.
- **Break-even = the labour price.** One unit of labour produces one unit of service, so `breakEven = labourPrice`.

The planning algorithm collapses to a simple form: buy as much labour as the budget allows, produce `labour bought` services, and submit sell orders at `breakEven` (the labour price paid). The profit-maximising quantity sweep still applies — the corporation chooses the price that maximises `p × Qd(p) − labourPrice × Qd(p)` — so it will not produce more than the demand curve can absorb at a profitable price.

The consequence is that **service prices are far more volatile than prices in standard-resource sectors**. A standard-resource corporation can withhold unsold stock and re-offer it next turn, damping the price; a service corporation's unsold output vanishes, so its supply next turn is decided afresh. Service corporations also **never grow capital**: with no buildings, there is nothing to invest in, so their credits accumulate as dividends to the owner household. They are the pure arbitrage sector — hire labour, sell services, pay out dividends.

#### Founding

Corporation founding is a stochastic process driven by relative wealth, evaluated for every household at the start of each economic turn.

```cpp
// Constants
constexpr int64_t MIN_CAPITAL   = 10'000'000;  // minimum founding capital, in centicredits
constexpr float   FOUNDING_CHANCE = 0.05f;     // maximum per-turn founding probability
constexpr int64_t FLOOR_WEIGHT    = 1'000'000; // minimum sector sampling weight
constexpr float   SALVAGE_RATE    = 0.5f;      // fraction of construction cost recovered on dissolution
```

**Eligibility.** A household is eligible to found a corporation in a turn if:

1. It does not already own a corporation, and
2. Its credits are at least `MIN_CAPITAL`.

**Founding roll.** Each eligible household rolls once per turn. Its probability is its **relative wealth** in the colony, scaled by `FOUNDING_CHANCE`:

$$\text{wealthFraction} = \frac{\text{credits} - \text{minWealth}}{\text{maxWealth} - \text{minWealth}}$$
$$p_{\text{found}} = \text{wealthFraction} \times \text{FOUNDING\_CHANCE}$$

where `minWealth` and `maxWealth` are the minimum and maximum credit balances among the households of the colony. The richest household has `wealthFraction = 1` and therefore the maximum chance `FOUNDING_CHANCE`; the poorest has `wealthFraction = 0` and never founds.

**Degenerate case:** if `maxWealth = minWealth` (all households in the colony have the same balance, as in a freshly founded colony), define `wealthFraction = 1` for all of them, so every eligible household has the full chance. Without this rule a new colony would never produce a corporation.

**Capital.** On a successful roll, the household transfers exactly `MIN_CAPITAL` centicredits from its balance to the corporation. The household keeps the rest.

**Sector choice.** The sector is sampled at random, weighted by how much the colony wanted each resource last turn and how much it cost:

$$\text{weight}_r = \max\left(0,\ \text{demand}_r - \text{filled}_r\right) \times \text{settlementPrice}_r + \text{FLOOR\_WEIGHT}$$

where `demand_r` is the total quantity of buy orders submitted for resource `r` in the colony last turn, `filled_r` is the quantity actually filled, and `settlementPrice_r` is the colony's last settlement price for `r`. The sector is drawn with probability `weight_r / Σ weight_r` over the six producible resources (Food, Materials, Fuel, Energy, Service, Consumer Goods).

The `FLOOR_WEIGHT` term is what keeps the economy startable. A resource with no unsatisfied demand — Energy in a colony with no power plants, where no tier 2 building can be powered — still has a non-zero chance of being chosen, so an energy industry can be jump-started from nothing. The sampled sector is **binding**: the corporation is founded with that sector.

**Timing.** The roll happens before order submission, so a corporation founded this turn appears at the start of the **next** turn and participates in the market from that turn on. Founding consumes no labour or materials.

A newly founded corporation has no buildings and no stocks, so in its first turn it can only buy inputs and produce its sector resource by hand.

**Worked example.** A colony has four households with credits 12 000 000, 30 000 000, 60 000 000 and 100 000 000. `minWealth = 12 000 000`, `maxWealth = 100 000 000`, range 88 000 000. With `FOUNDING_CHANCE = 0.05`:

| Household | wealthFraction | p_found |
|-----------|----------------|---------|
| H1 (12 M) | 0 | 0 |
| H2 (30 M) | 0.2045 | 1.02 % |
| H3 (60 M) | 0.5455 | 2.73 % |
| H4 (100 M) | 1 | 5 % |

Last turn in this colony: Food demand 12 000 000 kg, filled 10 000 000 kg, settlement price 4; Materials demand 3 000 000 kg, filled 2 500 000 kg, price 6; Fuel demand fully filled (price 5); Energy demand fully filled (price 2); Service demand fully filled (price 10); Consumer Goods demand 400 000 units, filled 250 000 units, price 30.

| Resource | unsatisfied | weight | P(sector) |
|----------|-------------|--------|-----------|
| Food | 2 000 000 × 4 = 8 000 000 | 9 000 000 | 41.9 % |
| Materials | 500 000 × 6 = 3 000 000 | 4 000 000 | 18.6 % |
| Fuel | 0 | 1 000 000 | 4.7 % |
| Energy | 0 | 1 000 000 | 4.7 % |
| Service | 0 | 1 000 000 | 4.7 % |
| Consumer Goods | 150 000 × 30 = 4 500 000 | 5 500 000 | 25.6 % |

Total weight 21 500 000. If H3's roll succeeds, it pays 10 000 000 centicredits (balance drops to 50 000 000) and founds a corporation with the sampled sector — most likely Food, but with a 4.7 % chance of starting the colony's first power plant, and a 25.6 % chance of joining the consumer goods industry.

Note that consumer goods get a large weight from a **small unsatisfied quantity**, because their price is high. That is the intended behaviour: the weight is `unsatisfied × price`, so a scarce luxury good pulls new entrants into the sector faster than a cheap staple.

#### Dissolution

A corporation is dissolved when its **credits reach zero at the end of an economic turn**. A corporation with zero credits cannot submit buy orders next turn, so it cannot operate; keeping it alive would only clutter the market.

Dissolution is a **liquidation** in the colony market, executed at the end of the turn:

1. All standard-resource stocks are submitted as sell orders at the market's current settlement price for that resource (or `PRICE_FLOOR` if the market has no settlement price yet). They are filled by the normal rationing rules.
2. All owned buildings are destroyed. Each pays `SALVAGE_RATE × construction cost` into the liquidation proceeds.
3. Instant resources (energy, labour) are destroyed — they cannot be sold after the turn ends.
4. The total proceeds are transferred to the owning household's credits.
5. The corporation is removed. The household is then free to found a new one.

Liquidation is a normal market event: the liquidation sell orders participate in settlement like any other orders, so liquidation can push the colony's settlement price down.

**Owner dissolution.** When the owning household is dissolved (population below `POP_MIN`), its corporation is dissolved in the same turn, **before** the household's assets are distributed. The corporation is liquidated by the rules above, its proceeds are added to the household's credits, and those credits then go to the colony's remaining households with the rest of the household's wealth. A corporation never exists without an owner, so there is no ownerless state.

The corporation is not sold to another household: its buildings are destroyed and its stock is sold on the market. Ownership transfer of a live corporation is out of scope.

#### Worked example

Household H2 (credits 50 000 000) rolls successfully and founds a Food-sector corporation, C1. H2 transfers `MIN_CAPITAL = 10 000 000` centicredits; its balance drops to 40 000 000 and C1 starts with 10 000 000. Manual productivity is 12 kg of food per unit of labour.

Manual food production is profitable when `foodPrice × 12 ≥ labourPrice`, i.e. `foodPrice ≥ 40 / 12 ≈ 3.33` at the subsistence labour price of 40. This is C1's **break-even (marginal) cost**: 3.33 centicredits per kg.

- **Turn 1:** budget = `10 000 000 × 0.8 = 8 000 000`. Last turn's food demand curve: 10 000 000 kg demanded at price 4, plus 4 000 000 kg of emergency survival bids at price 5 (`4 × EMERGENCY_BID`). Profit at price 4 = `4 × 10 000 000 − 10 000 000 × 3.333 = 6 666 667`; profit at price 5 = `5 × 4 000 000 − 4 000 000 × 3.333 = 6 666 667`. Tie → `p* = 4` (closest to last turn's settlement price), `Q* = 10 000 000` kg. The plan costs 33 333 333, above the budget, so manual staffing is dropped to what the budget allows: `8 000 000 / 40 = 200 000` labour units → 2 400 000 kg, offered with reservation price `breakEven = 3.33`. Revenue `2 400 000 × 4 = 9 600 000`. End of turn: `credits = 10 000 000 − 8 000 000 + 9 600 000 = 11 600 000` (profit 1 600 000; the unspent reserve stays in the balance).
- **Turn 2:** budget 9 280 000 → 232 000 labour → 2 784 000 kg → revenue 11 136 000. End of turn: `credits = 11 600 000 − 9 280 000 + 11 136 000 = 13 456 000`. C1 grows.
- **Turn 3:** the colony's food market is oversupplied; food settles at 3, below C1's break-even of 3.33. C1's sell order carries the reservation price 3.33, so it is **excluded** from the market at price 3 and sells nothing. The 2 784 000 kg stays in stock (food is storable). End of turn: `credits = 13 456 000 − 9 280 000 = 4 176 000`, stock 2 784 000 kg.
- **Turn 4:** producing at price 3 is dominated, so C1 buys no labour and holds its stock. The other corporations withhold their stock for the same reason, food becomes scarce, the settlement price rises above 3.33, and C1's stock sells at the new price.
- **Liquidation (if it ever comes):** C1 has no buildings; its stock is sold as liquidation sell orders at the current settlement price, and the proceeds go to H2.

The point of the example: the corporation **never sells below its break-even**. When the market price falls below break-even, supply is withheld as stock, not dumped. Prices recover instead of collapsing to `PRICE_FLOOR`.

**Consumer goods corporation worked example.** C2 is a Consumer Goods corporation with credits 20 000 000. Prices: labour 40, materials 6, energy 8. Unit costs: manual 46, tier 1 26, tier 2 14.16. Last turn's consumer goods demand curve: 383 333 units demanded at price 30 (the household share-driven demand), plus 100 000 units at 20.

- Budget = `20 000 000 × 0.8 = 16 000 000`.
- Profit at price 30: `30 × 383 333 − 383 333 × 14.16 = 6 072 000` — the plan needs `383 333 × 14.16 = 5 428 000` in inputs, within budget. C2 staffs tier 2 buildings (capacity 200 000 each, 5 goods per labour unit) and buys the materials and energy.
- Profit at price 20: `20 × 383 333 − 383 333 × 14.16 = 2 238 666` — worse, so `p* = 30`, `Q* = 383 333` goods.
- Sell orders carry reservation price 14.16. If consumer goods settle at 30, revenue is `383 333 × 30 = 11 500 000` and C2 makes a profit of `11 500 000 − 5 428 000 = 6 072 000`.
- If the colony's households are hit by a famine and their consumer goods shares collapse to zero, demand drops to nothing. C2's goods are storable, so it holds 383 333 goods in stock, sells nothing, and waits for the food price to fall. Consumer goods corporations are the most demand-sensitive sector in the colony: their demand is the *disposable* third of the household budget, so a food crisis removes their market entirely.

**Service corporation worked example.** C3 is a Service corporation with credits 10 000 000. Labour price 40, so `breakEven = 40` per service unit. Last turn's service demand: 1 150 000 units at price 10.

- Budget = `10 000 000 × 0.8 = 8 000 000` → affordable labour = 200 000 units → 200 000 services.
- Profit at price 10: `10 × 200 000 − 40 × 200 000 = −6 000 000` — negative. Profit at every price below 40 is negative, and the demand curve has no steps above 40. So the profit-maximising quantity is **zero**: C3 buys no labour and produces nothing this turn.
- C3 sells nothing, spends nothing, ends the turn with 10 000 000. Its belief widens; the founding roll sees `FLOOR_WEIGHT` for services, so new service corporations are not founded.
- Next turn the settlement price for services rises (the households' service demand is unchanged, supply vanished), and C3 produces when the price exceeds 40.

This is the service-sector instability: services have no stock to withhold, so the supply response is all-or-nothing, and prices oscillate around the labour price. A service corporation's profit margin is `p_service − p_labour`, and the labour price is set by the food price, so **the service industry is a leveraged bet on the food market** — a food spike pushes the labour price up and can make the entire service sector unprofitable in one turn.

#### Production

A corporation turns inputs into its **sector** resource. All production happens in the corporation's colony, using inputs acquired on that colony's market.

**The instant-resource constraint.** Labour and energy must be consumed in the **same turn** they are bought on the market. Standard resources (food, materials, fuel) are bought in one turn and consumed from stock in a later turn. Standard output produced in turn T is added to the corporation's stocks and sold in turn T+1.

**Manual production.** A corporation can convert pure labour, with no other inputs, into **Food**, **Materials**, **Fuel** or **Service** at the manual rate for that resource. Consumer goods require labour **plus 1 kg of materials per good**, so they have a manual rate too but not a pure-labour one. Energy cannot be produced by hand — it requires a power plant. Manual production has no capacity limit; it is bounded only by the labour the corporation can afford.

**Why manual food is 12 kg per labour unit.** A household of 1 000 000 individuals needs 10 000 000 kg of food per turn and supplies 1 000 000 units of labour. At 12 kg per labour unit, feeding one household takes `10 000 000 / 12 ≈ 833 333` units of labour — about 83 % of the household's own labour force. So a colony with zero infrastructure can feed itself, with roughly 17 % of the labour force left over as surplus to staff materials, fuel and (once a power plant exists) energy industries. If the rate were lower, the whole labour force would be locked into subsistence farming and no industry could ever start.

**Buildings.** Buildings are owned by the corporation that built them, live in its colony, and produce only the corporation's sector resource.

```cpp
struct BuildingDef {
    BuildingTypeId type;          // Tier1 | Tier2 | PowerPlant
    ResourceId     sector;        // resource produced
    int64_t        capacity;      // max labour units per turn
    int64_t        rate;          // output units per labour unit
    int64_t        energyCost;    // energy units per turn, 0 if none
    int64_t        inputPerOutput;// kg of materials/fuel per output unit, 0 if none
    int64_t        constructionLabour;
    int64_t        constructionMaterials;  // kg
    int64_t        maintenanceLabour;
    int64_t        maintenanceMaterials;   // kg
};

struct Building {
    BuildingTypeId type;
    bool           underConstruction;  // true until the end of the turn it was built
    int64_t        constructionValue;  // centicredits, used for salvage
};
```

| Building | Sector | Capacity | Rate (output per labour) | Energy/turn | Inputs per output | Construction (labour, materials kg) | Maintenance (labour, materials kg) |
|----------|--------|----------|--------------------------|-------------|-------------------|--------------------------------------|-------------------------------------|
| Manual | Food | — | 12 kg | 0 | — | — | — |
| Manual | Materials | — | 2 kg | 0 | — | — | — |
| Manual | Fuel | — | 1 kg | 0 | — | — | — |
| Manual | Service | — | 1 unit | 0 | — | — | — |
| Manual | Consumer Goods | — | 1 unit | 0 | 1 kg materials | — | — |
| Tier 1 | Food | 100 000 | 24 kg | 0 | — | 200 000, 100 000 | 10 000, 5 000 |
| Tier 1 | Materials | 100 000 | 4 kg | 0 | — | 200 000, 100 000 | 10 000, 5 000 |
| Tier 1 | Fuel | 100 000 | 2 kg | 0 | — | 200 000, 100 000 | 10 000, 5 000 |
| Tier 1 | Consumer Goods | 100 000 | 2 units | 0 | 1 kg materials | 200 000, 100 000 | 10 000, 5 000 |
| Tier 2 | Food | 200 000 | 60 kg | 20 000 | — | 500 000, 400 000 | 20 000, 15 000 |
| Tier 2 | Materials | 200 000 | 10 kg | 20 000 | — | 500 000, 400 000 | 20 000, 15 000 |
| Tier 2 | Fuel | 200 000 | 5 kg | 20 000 | — | 500 000, 400 000 | 20 000, 15 000 |
| Tier 2 | Consumer Goods | 200 000 | 5 units | 20 000 | 1 kg materials | 500 000, 400 000 | 20 000, 15 000 |
| Power plant | Energy | 100 000 | 10 units | 0 | 1 kg fuel | 300 000, 300 000 | 15 000, 10 000 |

All of these numbers are tuning constants.

**The productivity ladder.** Food rates are manual 12 → tier 1 24 (×2) → tier 2 60 (×5 over manual). The same ×2 / ×5 ladder applies to materials, fuel and consumer goods. The ladder is what makes buildings worth building: tier 1 doubles the output of the same labour, tier 2 multiplies it by 5 at the price of a flat energy bill. Keeping manual food at 12 kg keeps the subsistence floor intact at every tier — feeding one household takes 833 333 labour units manually, 416 667 in tier 1, 166 667 in tier 2, so each tier releases more of the labour force into other industries.

Consumer goods are the first sector with **two variable inputs** (labour + materials), so its unit cost has two terms. At labour 40 and materials 6, the per-good unit costs are: manual `40/1 + 6 = 46`; tier 1 `40/2 + 6 = 26`; tier 2 `40/5 + 6 + (20 000 × 8)/(200 000 × 5) = 14.16` (at energy price 8). Tier 2 is the cheapest per unit of output, consistent with the ladder, and the materials term is what ties consumer goods to the materials sector — a consumer goods corporation is a permanent customer of the materials industry.

**Construction.** A building is built with labour + materials. Both are consumed in the turn construction starts; the building appears at the **end of that turn** and is usable from the next turn. A corporation may start as many buildings in a turn as its spare cash allows (see the planning section). `constructionValue` is recorded as `constructionLabour × labourPrice + constructionMaterials × materialsPrice` at the settlement prices of the construction turn, and is the basis for salvage on dissolution.

**Maintenance.** Every building requires a per-turn labour + materials payment. A building whose maintenance is not paid is **destroyed at the end of that turn**.

**Tier 1 buildings** have a maximum number of workers and produce more per unit of labour than manual work. Labour beyond the capacity is not usable in that building.

**Tier 2 buildings** have a higher productivity multiplier than tier 1, and consume a **flat amount of energy per turn when they are staffed**. An idle tier 2 building draws **no energy** and pays only its maintenance. If the corporation cannot buy the energy for a staffed tier 2 building, the building **produces nothing that turn and its maintenance must still be paid** — a staffed-but-unpowered tier 2 building is a pure loss, and is destroyed if maintenance is skipped.

**Power plants** convert labour + fuel into energy. They are the only source of energy in a colony, and belong to Energy-sector corporations.

**Production planning (profit-maximising).** A corporation does not dump all its credits into labour and buildings, and it does not plan in units of demand. Each turn it estimates the colony's demand curve for its sector, computes the quantity that maximises **its own profit**, and produces exactly that. The guiding rule is **marginal revenue = marginal cost**: a unit is produced only if the price it will fetch covers the cost of producing it.

```cpp
// Planning constants
constexpr float RESERVE_FRACTION = 0.20f;  // fraction of credits kept uncommitted
constexpr float SURPLUS_MARGIN   = 0.25f;  // price margin above break-even required to justify surplus
constexpr int64_t PAYBACK_TURNS  = 10;     // maximum turns to recover a building's construction cost
constexpr float JUMPSTART_BUDGET = 0.50f;  // fraction of credits a new corporation may spend on its first building
```

**Demand estimate.** The corporation keeps `sectorDemand`, the colony's buy order book for its sector resource from last turn, as a `DemandCurve`. `Qd(p)` = the total quantity demanded at price `p` (the sum of the quantities of all steps with price ≥ `p`). This is the demand curve the corporation plans against. It is a one-turn estimate: household order books change slowly, so last turn's book is a good approximation of this turn's.

For **food** the curve is genuinely downward-sloping, because household consumption is elastic between 10 and 25 kg per person: `Qd(p)` falls as `p` rises, since households that can afford less eat less. Above the famine threshold the curve has a vertical tail: every household in emergency mode bids `lastFoodPrice × EMERGENCY_BID`, so `Qd(p)` stays flat up to that bid price and drops to zero above it. A food corporation's profit-maximising price therefore sits inside a band — it can raise price up to the emergency bid level without losing survival demand, and it loses comfort demand as soon as price pushes households' coverage down.

**Marginal cost.** The cost of producing one more unit of the sector resource, using the cheapest method with spare capacity. Every production method has a unit cost:

$$\text{unitCost}_{\text{manual}} = \frac{\text{labourPrice}}{\text{rate}} + \text{inputPerOutput} \times \text{inputPrice} \qquad \text{unitCost}_{\text{tier1}} = \frac{\text{labourPrice}}{\text{rate}} + \text{inputPerOutput} \times \text{inputPrice}$$
$$\text{unitCost}_{\text{tier2}} = \frac{\text{labourPrice}}{\text{rate}} + \frac{\text{energyPrice} \times \text{energyCost}}{\text{capacity} \times \text{rate}} + \text{inputPerOutput} \times \text{inputPrice} \qquad \text{unitCost}_{\text{powerplant}} = \frac{\text{labourPrice}}{\text{rate}} + \text{inputPerOutput} \times \text{fuelPrice}$$

The tier 2 energy bill is a flat per-building charge, so it is spread over the building's full capacity. Methods are ranked by unit cost. The corporation's **break-even price** `breakEven` is the unit cost of the most expensive method used in the plan.

**Profit-maximising quantity.** For each price `p` appearing in the demand curve, compute the quantity `Qd(p)`, the cost of producing it (cheapest methods first, capped by total building capacity plus unbounded manual production), and the profit:

$$\text{profit}(p) = p \times Q_d(p) - \text{cost}(Q_d(p))$$

Choose `p*` with the maximum profit, and `Q* = Qd(p*)`. Ties are broken by the price closest to last turn's settlement price. If `max profit ≤ 0`, the corporation produces **nothing** this turn.

This is standard monopoly pricing applied to the estimated demand curve, and it replaces the old "produce captured demand plus a surplus" rule. The old rule made every corporation over-supply: `captureShare` measured share of *supply*, not of demand, so each corporation's target was a share of last turn's supply multiplied by 1.10 — total planned supply was permanently 110 % of demand, a prisoner's dilemma that drove the price down on every unit sold. Extra units beyond `Q*` add no revenue, so producing more than `Q*` is strictly dominated.

**Budget.** The corporation may commit at most `budget = credits × (1 - RESERVE_FRACTION)` centicredits to the plan. The reserve covers maintenance shocks, price swings and liquidation costs, and prevents a corporation from spending everything on one production cycle and dissolving when the price moves against it.

**Reservation price.** Every sell order carries a **reservation price** equal to `breakEven` — the unit cost of the most expensive unit in the plan. The rationing rule already excludes sellers priced above the settlement price, so this single change implements the price floor: **a corporation never sells below its break-even.** If the market settles below `breakEven`, its sell order receives zero fill and the output stays in its stock.

**Inventory as a supply valve.** Standard resources are storable, so unsold output is retained. A corporation's stock is a supply reserve it can sell when the price recovers. Stocks are recorded with `stockUnitCost`, the average cost basis of the units in stock, used in the next turn's plan. Instant resources (energy, labour, services) cannot be stored: an energy corporation whose price is below break-even produces nothing that turn, and its unsold energy is destroyed as usual. A service corporation has no valve at all — see the Service corporations section.

Withholding supply is what stops the price collapse. When the market price falls below the producers' break-even, supply leaves the market, scarcity builds, and the price rises back above break-even. Prices oscillate around the profitable band instead of sliding to `PRICE_FLOOR`.

**Conditional surplus.** If last turn's settlement price `p_last` was comfortably profitable — `p_last ≥ breakEven × (1 + SURPLUS_MARGIN)` — and the profit-maximising plan leaves demand unsatisfied at that price, the corporation produces a surplus:

$$\text{surplus} = \max\left(0,\ Q_d(p_{\text{last}}) - Q^*\right)$$

The surplus is sold into demand that is already being paid for at a profitable price, so it earns at the margin. Its purpose is defensive: unsatisfied demand at a high price is exactly what the sector-sampling weight uses to found new corporations, so filling it protects the corporation's position. The surplus is limited by the remaining budget and by capacity.

Elastic consumption gives this rule real headroom. When food is cheap, households eat up to 25 kg per person and stock up to 4 turns ahead, so `Qd(p_last)` is far larger than the profit-maximising quantity at a higher price; the surplus is the volume of that comfort and reserve demand the corporation can serve at a price that stays profitable. When food is scarce, `Qd(p_last)` collapses to the survival orders, so the surplus is zero and the corporation sells into the shortage at a high price instead of flooding it.

**Staffing.** Staffing follows the profit plan, not the capacity. Buildings are staffed in order of **increasing unit cost** — with the current table tier 1 (1.67 per kg) is cheaper than tier 2 (2.0 per kg), so tier 1 is staffed first — and manual production is used last, for whatever output remains. Capacity beyond the plan is left **idle**: an idle building is staffed with zero labour, produces nothing, consumes no energy, and its maintenance is still paid. Maintenance for every owned building is allocated before any production labour.

**Energy.** A staffed tier 2 building must buy its flat energy bill; an idle tier 2 building buys none. If the plan cannot afford the energy for a tier 2 building, that building is left idle and its output is covered by cheaper methods. A tier 2 building whose energy is unaffordable is never staffed.

**Construction (profitability gate).** A building is built only if it pays for itself. Let `margin = p* - unitCost_of_building`. The building is approved when

$$\text{constructionCost} \le \text{margin} \times \text{capacity} \times \text{rate} \times \text{PAYBACK\_TURNS}$$

where `constructionCost = constructionLabour × labourPrice + constructionMaterials × materialsPrice` at this turn's settlement prices. If several building types pass the gate, the one with the lowest construction cost per unit of capacity is chosen. Number of buildings = `ceil(shortfall / (capacity × rate))`, capped by the spare cash. Buildings take a turn to build, so construction started in turn T closes next turn's shortfall; this turn's output is capped by the existing capacity.

**The jump-start exception.** A corporation with **no completed buildings** may build its first building of its sector, spending up to `JUMPSTART_BUDGET` of its credits, **without the payback gate**. This is the mechanism that starts an industry from nothing: a colony with no power plant cannot power tier 2 buildings, so no tier 2 building is profitable, so no power plant has any demand, so no power plant is profitable — the market is stuck. The first power plant is a loss-making jump-start investment, and the founding sector sampling (with `FLOOR_WEIGHT`) is what produces the corporation that builds it.

**Tuning constraint:** `MIN_CAPITAL` must be at least the construction cost of the cheapest jump-start building. With the current placeholder table a power plant costs `300 000 × 40 + 300 000 × 6 = 13 800 000` centicredits, which exceeds `MIN_CAPITAL = 10 000 000` — so either `MIN_CAPITAL` must be raised or the power plant made cheaper for the jump-start rule to be usable.

**Order submission.** The plan becomes orders in one submission:

- One labour buy order for maintenance + staffing + construction labour.
- Energy buy orders for staffed tier 2 buildings.
- Materials and fuel buy orders for the standard inputs the plan consumes. Standard inputs are bought one turn ahead of consumption, so this turn's submission covers next turn's maintenance, construction and fuel.
- One sell order for the stock available at the start of the turn (output produced last turn), priced at `breakEven`.

Buy orders are priced at the corporation's price belief for the input resource (see the Price Beliefs section). If the total plan cost exceeds `budget`, drop steps in reverse priority order: surplus first, then construction, then staffing from the most expensive method down (manual, then tier 2, then tier 1). **Maintenance is never dropped.** If dropping leaves the plan unprofitable, the corporation produces nothing.

**Worked example — profit-maximising food.** C1 (Food sector) owns one tier 1 food building (capacity 100 000, rate 24) and one tier 2 food building (capacity 200 000, rate 60, energy 20 000/turn). Credits 50 000 000, budget 40 000 000. Prices: food 4, labour 40, materials 6, energy 8. Stock: 3 000 000 kg.

Unit costs: tier 2 = `40 / 60 + (20 000 × 8) / (200 000 × 60) = 0.667 + 0.013 = 0.680` per kg; tier 1 = `40 / 24 = 1.667` per kg; manual = `40 / 12 = 3.333` per kg. **Tier 2 is the cheapest method**, so it is staffed first.

Last turn's food demand curve: 12 000 000 kg ordered at price 4 (comfort consumption), plus 3 000 000 kg of emergency survival bids at price 5 (`4 × EMERGENCY_BID`). So `Qd(5) = 3 000 000` and `Qd(4) = 15 000 000`.

- `Qd(5) = 3 000 000` → cost `3 000 000 × 0.680 = 2 040 000` (tier 2 only) → profit `5 × 3 000 000 − 2 040 000 = 12 960 000`.
- `Qd(4) = 15 000 000` → tier 2 at full capacity 12 000 000 kg (8 160 000), tier 1 at full capacity 2 400 000 kg (4 000 000), manual 600 000 kg (2 000 000) → cost 14 160 000 → profit `4 × 15 000 000 − 14 160 000 = 45 840 000`.
- So `p* = 4`, `Q* = 15 000 000` kg. Raising the price to the emergency bid level is **not** profitable: it sells a quarter of the volume. `breakEven = 3.333` (the marginal unit is manual).
- Staffing: tier 2 full (200 000 labour, 8 000 000), tier 1 full (100 000 labour, 4 000 000), manual 50 000 labour units (2 000 000).
- Costs: staffing 14 000 000 + energy 160 000 + tier 2 maintenance `20 000 × 40 + 15 000 × 6 = 890 000` + tier 1 maintenance `10 000 × 40 + 5 000 × 6 = 430 000` = **11 480 000**, well within the budget.
- Surplus: `p_last = 4 < breakEven × 1.25 = 4.1665` → the surplus gate is not met, surplus 0. The corporation is profitable, but the margin over break-even is not large enough to justify extra volume.
- Sell order: 3 000 000 kg of stock at reservation price 3.333 → sells at 4 → 12 000 000.
- Next turn the 15 000 000 kg produced this turn goes to market.

**Worked example — shortfall triggers construction, then the budget clamp.** Demand grows: last turn households ordered 40 000 000 kg of food at price 4. Credits 50 000 000, budget 40 000 000.

- `Qd(4) = 40 000 000` kg. Cost at the cheapest methods: tier 2 12 000 000 kg (8 160 000), tier 1 2 400 000 kg (4 000 000), manual 25 600 000 kg (85 333 333) → total 97 493 333. Revenue 160 000 000, profit 62 506 667 — the plan is profitable, so `p* = 4`, `Q* = 40 000 000`.
- The plan costs 97 493 333 against a budget of 40 000 000. Drop the most expensive staffing first: manual production (25 600 000 kg at 3.333) is dropped → cost 12 160 000, output 14 400 000 kg.
- Shortfall = `40 000 000 − 14 400 000 = 25 600 000` kg.
- Construction gate for tier 2: `constructionCost = 500 000 × 40 + 400 000 × 6 = 22 400 000`; `margin = 4 − 0.680 = 3.320`; `margin × capacity × rate × PAYBACK_TURNS = 3.320 × 12 000 000 × 10 = 398 400 000`. **22 400 000 ≤ 398 400 000 → approved.**
- Buildings needed `ceil(25 600 000 / 12 000 000) = 3`, cost 67 200 000. Spare cash = `40 000 000 − 12 160 000 = 27 840 000` → **build 1** (22 400 000).
- Next turn capacity is `14 400 000 + 12 000 000 = 26 400 000` kg, and the new building's maintenance starts.

**Profit and dividends.** End-of-turn profit is revenue minus input costs. The corporation pays `DIVIDEND_RATE` of its end-of-turn credits to the owning household and retains the rest.

**Worked example — tier 2 food at full capacity.** C1 owns one tier 2 food building. Prices: labour 40, energy 8, materials 6. Last turn's demand curve: 12 000 000 kg demanded at price 4.

- `Qd(4) = 12 000 000` = capacity, so no manual production is needed (manual at 3.333 per kg would be profitable at price 4, but there is no demand beyond capacity).
- Plan: staffing 200 000 labour (8 000 000), energy 20 000 (160 000), maintenance 20 000 labour + 15 000 kg materials (890 000). Total 9 050 000.
- Output 12 000 000 kg food, sold next turn at 4 → 48 000 000. Profit ≈ 38 950 000.

**Worked example — power plant.** C2 (Energy sector) owns one power plant: capacity 100 000, rate 10 energy per labour, 1 kg fuel per energy unit. Prices: labour 40, fuel 5, energy 12, materials 6. Credits 20 000 000, budget 16 000 000.

- Unit cost = `40 / 10 + 1 × 5 = 9` centicredits per energy unit. Last turn's demand curve: 1 000 000 units demanded at price 12.
- `Qd(12) = 1 000 000` = capacity. Cost `1 000 000 × 9 = 9 000 000` + maintenance `15 000 × 40 + 10 000 × 6 = 660 000` = 9 660 000. Revenue `1 000 000 × 12 = 12 000 000`. Profit 2 340 000, within budget.
- `breakEven = 9` → sell order priced at 9. Energy is instant: produced and sold in the **same turn**; unsold energy is destroyed.

The break-even condition: a power plant is profitable only when `energyPrice × 10 ≥ labourPrice + fuelPrice`, i.e. `energyPrice ≥ (40 + 5) / 10 = 4.5`. At the energy price of 2 in the founding example above, `Q* = 0` — the corporation **produces nothing** and its energy is not created at all. It does not keep producing until its credits run out. The only way out of a colony with no power plants is the jump-start exception, which lets a new energy corporation build its first plant as a loss-making investment; once tier 2 buildings exist and demand energy, the price rises above 4.5 and the plant becomes profitable.

### Per-Colony Market

There is one market per colony. Each market is a set of **order books, one per resource** — the settlement price is computed independently for each `(colony, resource)` pair. Credits are the medium of exchange and are never traded on a market.

```cpp
struct Market {
    ColonyId colony;
    // Settlement price per resource, in centicredits per unit.
    // Doubles as the "previous settlement price" used for tie-breaking and belief updates.
    std::array<std::int64_t, RESOURCE_COUNT> settlementPrice;
};
```

Credits are a **virtual** resource: each agent has a single global credit balance, not a per-colony one. Since every agent lives in exactly one colony, all of its orders go to a single market, so the submission-time credit clamp is applied against one pool and no cross-market pre-allocation is needed.

Agents trade in the market of the colony where their production and consumption take place. Every household and corporation lives in exactly one colony, so each agent participates in exactly one market. Instant resources (labour, energy) can only be traded in the market of the colony where they were produced this turn.

### Orders

Each turn, every agent submits its orders to the market of its colony.

```cpp
struct Order {
    OrderId      id;       // unique within the market, assigned deterministically
    AgentId      agent;    // household or corporation
    ResourceId   resource;
    Side         side;      // Buy | Sell
    std::int64_t quantity;  // kg for standard resources, dimensionless for labour/energy
    std::int64_t price;     // centicredits per unit
};
```

The order's colony is **implicit**: orders live in the order book of the market they were submitted to, so the colony is known from the containing market and is not stored in the order.

Order ids are assigned in a deterministic order (by agent id, then side, then resource) so settlement is reproducible.

Rules:

- An agent submits **at most one order per `(side, resource)` per colony market**.
- `quantity > 0` and `price` is in `[PRICE_FLOOR, PRICE_CAP]`, where `PRICE_FLOOR = 1` centicredit and `PRICE_CAP = 1 000 000` centicredits (10 000 credits).
- **Buy orders are validated against the agent's credits at submission**: cumulative `quantity * limitPrice` across the agent's buy orders cannot exceed its credits. Orders are considered in deterministic submission order; each quantity is clamped to the remaining credit commitment divided by its limit price (integer division), and zero-quantity orders are dropped. This applies the credit cap across all resources, so settlement at any eligible price cannot make an agent go into debt.
- **Sell orders are validated against the agent's holdings at submission**: `quantity <= stock(agent, resource)` in that colony. For instant resources, `stock` is the amount produced this turn. Orders clamped to zero are dropped.

### Settlement price

For a given market, let `Qs(p)` be the total quantity of sell orders with `price <= p` and `Qd(p)` be the total quantity of buy orders with `price >= p`. The quantity traded at price `p` is:

$$Q(p) = \min(Q_s(p), Q_d(p))$$

`Qs` is non-decreasing and `Qd` is non-increasing in `p`, so `Q(p)` is quasi-concave. The settlement price is the price that maximises `Q(p)`.

`Q(p)` only changes at order prices, so the maximum is found by sweeping the candidate set `{PRICE_FLOOR} ∪ {price of every order in the market}` in ascending order while maintaining `Qs` and `Qd` incrementally. The set of maximisers is a contiguous interval `[p_lo, p_hi]`.

**Tie-breaking:** choose the maximiser closest to the market's previous settlement price. If the previous settlement price lies inside `[p_lo, p_hi]`, it is chosen; if it is below `p_lo`, then `p_lo`; if above `p_hi`, then `p_hi`.

**No-trade case:** if `max Q(p) = 0` (one side of the market is empty), no trade is executed and the settlement price stays at the previous value.

The `PRICE_FLOOR` of 1 centicredit is essential: at price 0 both `Qs` and `Qd` are unbounded, so the objective degenerates.

### Execution and rationing

At the settlement price `p`, supply and demand rarely match exactly. The traded quantity is `Q = min(Qs(p), Qd(p))`, and the excess side is rationed **pro-rata**:

- If `Qd(p) > Qs(p)` (excess demand), buyers are rationed: `fill_i = q_i * Q / Qd(p)`.
- If `Qs(p) > Qd(p)` (excess supply), sellers are rationed: `fill_i = q_i * Q / Qs(p)`.
- If `Qs(p) = Qd(p)`, the market clears exactly and every eligible order is filled in full.
- Orders on the short side are filled in full.
- Orders whose price is on the wrong side of `p` (a buy order with `price < p`, a sell order with `price > p`) are not eligible and receive `fill = 0`.

An agent may submit both a buy and a sell order for the same resource in the same market (e.g. a corporation that both buys food for its workers and sells surplus food). The two orders are rationed independently.

Fills are integers, computed with the **largest remainder method**: `base_i = floor(q_i * Q / X)`, where `X` is the total quantity of the rationed side; the remaining `R = Q - Σ base_i` units are handed out one by one to the orders with the largest remainders `(q_i * Q) mod X`, breaking ties by ascending order id. `R` is always less than the number of eligible orders, so this terminates.

### Clearing

For every order with `fill_i > 0`:

- Sellers receive `fill_i * p` centicredits and lose `fill_i` units of the resource.
- Buyers pay `fill_i * p` centicredits and gain `fill_i` units of the resource.

Standard resources move between agents' stocks at that colony and are carried over to the next turn. Instant resources consumed by a buyer are used up; **unsold instant supply is destroyed at the end of the turn** (it cannot be stored). Unfilled demand for instant resources is discarded.

### Turn sequence

One economic turn in a colony market runs in this order:

1. **Founding roll:** eligible households roll for the chance to found a corporation (see the Founding section). Corporations founded this turn appear at the start of the next turn.
2. **Order submission:** households submit labour supply, food demand (emergency, comfort, reserves), services demand and consumer goods demand; corporations submit their planned buy orders (labour, materials, fuel, energy) and sell orders (their sector resource, produced last turn). Orders are validated and clamped.
3. **Settlement:** each `(colony, resource)` order book computes its settlement price.
4. **Rationing:** fills are computed for each order book.
5. **Clearing:** credits and resource stocks are transferred, including food into household stocks.
6. **Household consumption and population:** each household eats from its stock plus its fill, updates `foodStock`, consumes its services and consumer goods fills (which leave the colony's inventory), and updates `population` from the shortfall/growth formulas. The new food settlement price is appended to `foodPriceHistory`.
7. **Production:** corporations consume the labour and energy bought this turn, plus standard inputs from stock, and add their output to their stocks. Buildings whose maintenance was not paid are destroyed.
8. **Instant supply that was not sold is destroyed** (unsold labour from households, unsold energy from power plants, unsold services from service corporations).
9. **Price beliefs** are updated using the settlement prices.
10. **Dissolution check:** households whose population is below `POP_MIN` are dissolved; each such household's corporation is liquidated first, then the household's wealth and food stock are transferred to the colony's remaining households. Corporations whose credits are zero are liquidated and their proceeds paid to their owners.

Corporation output produced in turn T is sold in turn T+1, so a corporation's sell orders in a turn reflect the production plan of the previous turn.

### Price seeding

When a colony is founded, each of its markets is seeded with a settlement price derived from the initial money supply:

$$p_{\text{resource}} = \frac{\text{money\_supply}}{\text{num\_resources} \times \text{total\_stock}_{\text{resource}}}$$

clamped to `[PRICE_FLOOR, PRICE_CAP]`, with a minimum of 1 unit of stock to avoid division by zero. Agents' price beliefs are initialised around this seed.

### Worked example

Market for Food in colony A. Sell orders (price in centicredits per kg, quantity in kg):

| Order | Price | Quantity |
|-------|-------|----------|
| S1 | 2 | 100 |
| S2 | 4 | 200 |
| S3 | 6 | 300 |

Buy orders:

| Order | Price | Quantity |
|-------|-------|----------|
| B1 | 7 | 150 |
| B2 | 5 | 250 |
| B3 | 3 | 400 |

| p | Qs(p) | Qd(p) | Q(p) |
|---|-------|-------|------|
| 1 | 0 | 800 | 0 |
| 2 | 100 | 800 | 100 |
| 3 | 100 | 800 | 100 |
| 4 | 300 | 400 | 300 |
| 5 | 300 | 400 | 300 |
| 6 | 600 | 150 | 150 |
| 7 | 600 | 150 | 150 |

Max `Q = 300` at `p ∈ [4, 5]`. With a previous settlement price of 3, the closest maximiser is `p = 4`.

At `p = 4`: `Qs = 300`, `Qd = 400` → `Q = 300`, excess demand, so buyers are rationed. Eligible buyers are B1 (150) and B2 (250); B3's price 3 is below `p` and is excluded.

- B1: `150 * 300 / 400 = 112.5` → base 112, remainder 200
- B2: `250 * 300 / 400 = 187.5` → base 187, remainder 200
- `R = 300 - 299 = 1`; remainders tie, so B1 (lower order id) gets it. B1 = 113, B2 = 187.

Sellers are on the short side and fill fully: S1 = 100, S2 = 200.

Money: `300 × 4 = 1200` centicredits. S1 receives 400, S2 receives 800; B1 pays 452, B2 pays 748. Total paid = total received = 1200.

### Invariants

- **Credits are conserved**: total buyer payments equal total seller receipts (`Q × p`).
- **Standard resources are conserved**: units move between agents, never created or destroyed by the market. Household food stocks count as agent inventory. Food leaves the colony's inventory when households eat it; consumer goods leave when households consume them. These are the only two exit paths for standard resources.
- **Instant resources are never stored**: labour, energy and services exist only within a turn. Unsold instant supply is destroyed at the end of the turn; unfilled instant demand is discarded.
- **No agent sells more than it owns** and **no agent spends more than it has** — both guaranteed by submission-time validation.
- **Settlement is deterministic**: the same set of orders produces the same result. Randomness lives only in order generation, not in settlement.
- **Prices stay in `[PRICE_FLOOR, PRICE_CAP]`**, which keeps `quantity × price` within `int64_t` range.

### Complexity

Per market: sort orders by price, sweep candidate prices maintaining `Qs` and `Qd`, then ration — `O(n log n)` in the number of orders. With `C` colonies, `R` resources and `N` agents, a turn costs `O(C · R · N log N)`, which is not a bottleneck at expected scale.

## Price Beliefs and Order Submission

Each agent keeps a price belief per resource: a mean and a half-width.

```cpp
struct PriceBelief {
    std::int64_t mean;      // centicredits per unit
    std::int64_t halfwidth; // centicredits per unit
};
```

When submitting an order, the agent samples a price uniformly from `[mean - halfwidth, mean + halfwidth]`, clamped to `[PRICE_FLOOR, PRICE_CAP]`.

After settlement at price `p`, the belief is updated:

- **Order filled** (`fill > 0`): `mean += α · (p - mean)`, `halfwidth *= (1 - β)` — the belief is reinforced.
- **Order not filled**: `mean += α · (p - mean)`, `halfwidth *= (1 + γ)` — the agent moves toward the market price but admits it does not know it precisely.

`halfwidth` is clamped to `[W_MIN, W_MAX]`. All three coefficients are tuning constants.

### Beliefs used by corporations

Corporations use **the same belief machinery** for every resource they trade (labour, materials, fuel, energy, and their sector resource). Only the way the belief is used differs:

- **Buying:** the corporation samples a price from its belief, exactly as a household does.
- **Selling:** the corporation offers at its break-even cost, not at a sampled price. The sell-side belief does not set the offer price; it is used to judge the surplus condition (is the market clearing well above break-even?) and to estimate how much of the demand it can capture.
- **Updating:** a corporation's sell order being filled or not updates its belief for that resource as above. Repeated failure to sell at break-even widens the sell-side belief, which makes the corporation's *buy-side* estimates noisier. That is the intended feedback: a sector that keeps losing money becomes a less attractive place to build, and the `FLOOR_WEIGHT` in the founding roll is what eventually lets a starved sector get going again.

## Random Ideas

List of ideas not yet properly included:

- Inter-colony (interplanetary) megacorporations that operate in multiple colonies.
- Household consumption beyond food, services and consumer goods (materials, fuel, energy as consumer goods).
- Population growth driven by more than food: wealth, housing, healthcare and education as growth factors, so population growth is not purely a function of calories. Consumer goods and services are the natural candidates for a happiness/utility term.
- Transport of standard resources between colonies.
- **Service industry depth**: split services into sub-sectors (transport, maintenance, research, entertainment) with different production shapes, so services are not a single pure-labour resource. Services consumed by corporations (maintenance contracts, research) would give corporations a non-labour input they currently lack.
- **Consumer goods variety**: multiple consumer goods tiers (durables vs. non-durables), so consumer goods can be stored by households and the "consume in the purchase turn" rule becomes a choice rather than a constraint.
- **Banking**: households deposit savings and corporations take loans, so the savings propensity becomes an interest-bearing asset and the credit supply of a colony becomes endogenous. A bank is a corporation with a sector that produces credits.
- **Central bank**: a colony-wide (or empire-wide) authority that sets the money supply, the price seed, and an interest rate, replacing the fixed money supply used by the price seeding formula.
- **Taxes**: a per-turn levy on households and corporations, funding the colony's non-market spending. Taxes are the mechanism that gives the colony a budget and make the settlement price of public goods visible.
- **Unemployment benefits**: a payment to households whose labour is not hired, which keeps a colony's consumption alive during a downturn and puts a floor under the labour price.
- **Household stock quality and spoilage**: food reserves decay over turns, so stockpiling has a carrying cost and reserves are not a free hedge against scarcity.
- **Households as sellers of stock**: a household with surplus reserves can sell food on the market, becoming a small seller and adding a second source of supply during a shortage.
- **Corporations holding reserves of resources**: corporations stockpile inputs (materials, fuel, energy-equivalent inputs) ahead of need, so production is not fully exposed to a single turn's supply. This decouples the instant-resource constraint from turn-to-turn scarcity.
