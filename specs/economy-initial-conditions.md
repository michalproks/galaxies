# Economy Initial Conditions

The state the economy starts in for the prototype: one colony, no inter-colony trade, no government.

The economic rules are in [economic-model.md](./economic-model.md), the implementation shape in [economy-implementation.md](./economy-implementation.md), and the visualization in [economy-visualization.md](./economy-visualization.md).

## Scope

The initial state is a complete, deterministic snapshot: every agent, every building, every stock, every belief, and a set of "last turn" prices that the turn-1 rules read. **There is no turn 0** — the seed prices are a fiction that makes the turn-1 formulas well-defined.

The snapshot is a **tier 2 economy in a steady state**: the colony starts with the power plants, fuel and materials industry needed to feed itself, and its production capacity is sized to the demand the households can sustain turn after turn. Seeding a manual-only colony is not viable, and seeding capacity against turn-1 demand is not stable — see [Scale problem](#scale-problem) and [The seed is a fixed point](#the-seed-is-a-fixed-point).

## Colony

- One colony, on Earth. It is the only colony in the game, so there is no inter-colony trade and no transport.
- The colony owns the market data structures; households and corporations are stored in it.

## Seed Prices ("last turn prices")

Hand-picked anchors, in centicredits per unit. These are the settlement prices of the fictional turn 0, and they are the values every "last turn" reference reads on turn 1: `lastFoodPrice`, the emergency bid base, the seeding of every belief, and the basis of `lastIncome`.

| Resource | Seed price | Seeded method | Unit cost | Profit per unit |
|----------|-----------|---------------|-----------|-----------------|
| Labour | 40 | — | — | — |
| Food | 4 | tier 2 | 0.6917 | 3.3083 |
| Materials | 6 | tier 2 | 4.1500 | 1.8500 |
| Fuel | 10 | tier 2 | 8.3000 | 1.7000 |
| Energy | 15 | power plant | 14.0000 | 1.0000 |
| Service | 40 | manual | 40.0000 | 0.0000 |
| Consumer Goods | 30 | tier 2 | 14.3000 | 15.7000 |

Every seed price is inside `[PRICE_FLOOR, PRICE_CAP]`.

**The seeding formula from the model spec is not used here.** It gives `p_food = money_supply / (num_resources × total_food_stock) = 62 030 000 000 / (7 × 15 000 000 000) = 0.59`, clamped to `PRICE_FLOOR = 1`. The starting food stock is so large relative to the money supply that the formula collapses every price to the floor. Anchors are the right choice for the prototype.

**The one hard consistency rule:** the seed labour price must equal `smoothedFoodPrice × FOOD_PER_PERSON`. With `foodPriceHistory = [4, 4, 4]`, the smoothed price is 4 and the subsistence wage is 40. If the seed labour price were lower, every household's labour order would be raised to the wage floor on turn 1 and the labour market would jump; if it were higher, the wage floor would be a no-op.

**The seed prices must make every seeded sector profitable at the tier it uses.** This is what fixes fuel and energy. A tier 2 building draws 20 000 energy per turn, so the energy price must cover the power plant's unit cost `labourPrice / 10 + fuelPrice`, and the fuel price must cover the tier 2 fuel unit cost `labourPrice / 5 + energyPrice × 20 000 / 1 000 000`. With labour 40 that gives `fuel ≥ 8 + 0.02 × energy` and `energy ≥ 4 + fuel`, solved by fuel 10 and energy 15. The earlier anchors (fuel 5, energy 8) make both sectors loss-making, so no power plant is ever built and the colony is locked at tier 1.

**Service is the wart in the table.** Service has no buildings, so its unit cost is exactly the labour price: `40 / 1 = 40`. At the seed price 40, service corporations break even and earn nothing. Any price above 40 is profitable, and the steady-state service demand (71.26M units) is well inside the labour supply, so the market clears. Raising the seed service price above 40 gives the service sector a margin; keeping it at 40 makes it a break-even sector that never grows. This is a tuning decision, not a consistency requirement.

## Seed Capacity

The colony's seeded production, sized to the households' **sustainable** demand at the seed prices (see [The seed is a fixed point](#the-seed-is-a-fixed-point)). All buildings are seeded as **completed** — the snapshot is the state at the end of the fictional turn 0.

| Sector | Method | Buildings | Output per turn | Staffing labour | Maintenance labour |
|--------|--------|-----------|-----------------|-----------------|--------------------|
| Food | tier 2 | 477 | 5 724 000 000 kg | 95 400 000 | 9 540 000 |
| Materials | tier 2 | 54 | 108 000 000 kg | 10 800 000 | 1 080 000 |
| Fuel | tier 2 | 13 | 13 000 000 kg | 2 600 000 | 260 000 |
| Energy | power plant | 13 | 13 000 000 units | 1 300 000 | 195 000 |
| Consumer Goods | tier 2 | 96 | 96 000 000 units | 19 200 000 | 1 920 000 |
| Service | manual | 0 | 71 500 000 units | 71 500 000 | 0 |

Total labour purchased: **213 795 000 of 500 000 000 units — 42.8 % employment.** The remaining 286.2M labour units are the colony's slack, which is what the founding rolls on turn 1 will hire.

Consistency checks (supply ≥ demand at the seed prices):

- Energy demand = (477 + 54 + 13 + 96) × 20 000 = 12.8M units; supply 13M. ✓
- Fuel demand = 1 kg per energy unit = 12.8M kg; supply 13M. ✓
- Materials demand = 96M kg for consumer goods + 9.73M kg of maintenance = 105.73M kg; supply 108M. ✓
- Food demand = 5 712 600 000 kg; supply 5 724 000 000 kg. ✓
- Consumer goods demand = 95 020 000 units; supply 96 000 000. ✓
- Service demand = 71 260 000 units; supply 71 500 000. ✓

Building counts are rounded up so that supply covers demand. The surplus is small on purpose: a large seeded surplus would collapse the price to the producers' break-even on turn 1.

## The Seed Is a Fixed Point

Sizing capacity to turn-1 demand is a trap. Turn 1's demand is inflated by the households' starting savings: with `credits = 100 000 000` and `lastIncome = 40 000 000`, the midpoint budget is `0.7 × 40M + 0.4 × 60M = 52M`, which buys 4.33M kg of comfort food per household. That demand cannot be sustained: households spend more than they earn, their credits drain, and the comfort demand collapses within three turns. Capacity sized to it is permanently too large, and the price falls to the break-even floor.

The seed is sized to the **steady state**, the fixed point where household spending equals income:

$$\text{budget} = \text{lastIncome} \implies \text{propIncome} \times I + \text{propSavings} \times (C - I) = I$$

At the propensity midpoints, the steady-state credit level is `C = 1.75 × I`. With employment fraction `e`, income per household is `I = e × 1 000 000 × 40`. Solving the capacity, demand and employment equations together gives **e = 0.428**, `I = 17 104 000` centicredits, steady-state credits `C ≈ 30 000 000`:

| Quantity | Per household | Colony |
|----------|---------------|--------|
| Labour sold | 427 600 units | 213.8M units |
| Income | 17 104 000 | 8 552 000 000 |
| Budget / category share | 17 104 000 / 5 701 333 | — |
| Food comfort | 1 425 333 kg | 712.67M kg |
| Food consumption | 11 425 333 kg (11.43 kg/person) | 5 712.67M kg |
| Services | 142 533 units | 71.27M units |
| Consumer goods | 190 044 units | 95.02M units |
| Population growth | 0.475 % per turn | — |

Population grows slowly (0.475 % per turn) because comfort coverage is only 0.095 — households eat 11.43 kg per person, not 25. The colony is at subsistence plus a little comfort, which is the intended steady state for a colony with no reserves and no government.

**The food price is pinned by the emergency bid.** Food demand has an inelastic tail: every household with `foodStock < survival need` bids `lastFoodPrice × EMERGENCY_BID`. In the steady state, households run with near-zero stock, so the emergency order is the bulk of food demand, and the profit-maximising food price sits at the bid level. With `lastFoodPrice = 4` and `EMERGENCY_BID = 1.25`, the bid is 5, and the food corporations' profit-maximising plan is to produce ~5.57 bn kg and sell at 5. The seed price 4 is inside that band, so turn 1 settles near it.

## Households

500 households, each starting at:

| Field | Initial value | Notes |
|-------|---------------|-------|
| `population` | 1 000 000 | `POP_BASE` |
| `credits` | 100 000 000 | 1 000 000 credits |
| `foodStock` | 30 000 000 kg | `FOOD_STOCK_TURNS` = 3 turns of survival food |
| `lastIncome` | 17 104 000 | steady-state income at 42.8 % employment |
| `propIncome` | uniform `[PROP_INCOME_MIN, PROP_INCOME_MAX]` | |
| `propSavings` | uniform `[PROP_SAVINGS_MIN, PROP_SAVINGS_MAX]` | |
| `foodBelief` | `{4, 1}` | |
| `labourBelief` | `{40, 10}` | |
| `materialsBelief` | `{6, 1.5}` | |
| `fuelBelief` | `{10, 2.5}` | |
| `energyBelief` | `{15, 3.75}` | |
| `serviceBelief` | `{40, 10}` | |
| `consumerGoodsBelief` | `{30, 7.5}` | |
| `foodPriceHistory` | `[4, 4, 4]` | all `WAGE_SMOOTH` entries = seed food price |

```cpp
constexpr int64_t NUM_HOUSEHOLDS   = 500;
constexpr int64_t FOOD_STOCK_TURNS = 3;
constexpr float   SEED_HALFWIDTH   = 0.25f;   // halfwidth = SEED_HALFWIDTH × mean
constexpr float   PROP_INCOME_MIN  = 0.5f;
constexpr float   PROP_INCOME_MAX  = 0.9f;
constexpr float   PROP_SAVINGS_MIN = 0.2f;
constexpr float   PROP_SAVINGS_MAX = 0.6f;
```

Colony totals: population 500 000 000, labour supply 500 000 000 units per turn, household credits 50 000 000 000 centicredits, food stock 15 000 000 000 kg (15 million tonnes).

`lastIncome` is set to the **steady-state** income, not to full employment. Setting it to full employment (40 000 000) makes turn 1's budget 52M, which is 3× the sustainable budget; the resulting demand spike is the instability described in [The seed is a fixed point](#the-seed-is-a-fixed-point). Setting it to 0 makes turn 1's budget depend entirely on savings.

Belief halfwidths are `SEED_HALFWIDTH × mean`, rounded down, clamped to `[W_MIN, W_MAX]`.

**The starting food stock is a one-shot endowment.** At `cheapness = 1 − lastFoodPrice / foodBelief.mean = 0`, the reserve target is zero, so households never buy reserves at the seed. Comfort eating consumes the 30 000 000 kg stock in about three turns, after which households run on production alone. If the seed is meant to show a reserve-building phase, set `foodBelief.mean` above the seed price (e.g. `{5, 1.25}` gives `cheapness = 0.2`, `reserveTurns = 0.8`, target 8M kg — below the stock, so the stock is drawn down).

## Corporations

15 corporations: 3 each in **Food**, **Materials**, **Fuel**, **Energy** and **Consumer Goods**. Service has no buildings, so service production is seeded as pure manual capacity hired by the service corporations.

| Field | Initial value |
|-------|---------------|
| `credits` | 1.25 × one turn of the sector's operating costs, split evenly among the 3 corporations |
| `lastIncome` | 0 |
| buildings | the seeded count for the sector, all completed |
| stocks | empty |
| `stockUnitCost` | 0 |
| `sectorBelief` | `{seed price of the sector, SEED_HALFWIDTH × seed price}` |
| `labourBelief`, `materialsBelief`, `fuelBelief`, `energyBelief` | seeded at the anchors |
| `sectorDemand` | seeded demand curve, below |

Capital is sized so the `budget = credits × (1 - RESERVE_FRACTION)` rule covers exactly one turn of operation, leaving the 20 % reserve intact. Construction capital is **sunk in the fictional turn 0** and is not part of the snapshot's money supply.

| Sector | Staffing | Maintenance (labour + materials) | Energy / fuel bills | Input materials | Operating cost | Credits per corporation |
|--------|----------|----------------------------------|--------------------|-----------------|----------------|-------------------------|
| Food | 3 816 000 000 | 424 530 000 | 143 100 000 | — | 4 383 630 000 | 1 850 000 000 |
| Materials | 432 000 000 | 48 060 000 | 16 200 000 | — | 496 260 000 | 210 000 000 |
| Fuel | 104 000 000 | 11 570 000 | 3 900 000 | — | 119 470 000 | 50 000 000 |
| Energy | 52 000 000 | 8 580 000 | 128 000 000 (fuel) | — | 188 580 000 | 80 000 000 |
| Consumer Goods | 768 000 000 | 85 440 000 | 28 800 000 | 576 000 000 | 1 479 240 000 | 620 000 000 |
| Service | 2 860 000 000 | 0 | 0 | — | 2 860 000 000 | 1 200 000 000 |
| **Total** | | | | | **9 127 980 000** | **12 030 000 000** |

Check: `0.8 × credits per corporation ≥ operating cost / 3` for every sector. ✓

**Service corporations have no buildings and no inventory valve.** Their unit cost is the labour price, so their profit margin is `servicePrice − labourPrice`. At the seed price 40 they earn zero. This is the sector most exposed to the money supply: service demand is one third of the household budget, and the only way to satisfy more of it is to hire more labour.

### Seed demand curves

Corporations plan against last turn's demand, and there is no last turn. The curves are the **steady-state** demand computed above, so each corporation's profit-maximising plan on turn 1 is its seeded capacity.

| Resource | Seed demand |
|----------|-------------|
| Food | `[(4, 5 712 670 000)]` |
| Service | `[(40, 71 270 000)]` |
| Consumer Goods | `[(30, 95 020 000)]` |
| Materials | `[(6, 105 730 000)]` — 1 kg per consumer good + maintenance |
| Fuel | `[(10, 12 800 000)]` — 1 kg per energy unit |
| Energy | `[(15, 12 800 000)]` — 20 000 per staffed tier 2 building |
| Labour | households sell 500 000 000 units at the subsistence wage 40 |

Fuel, energy and materials demand are **intermediate demand created by the corporations' own plans**, not household orders. Seeding them is what lets a tier 2 corporation plan a staffed building on turn 1; without them, a corporation with no last-turn demand for its sector produces nothing.

The food curve has no emergency tail at the seed, because every household starts with 3 turns of stock. The tail appears from turn 3 onward.

## Ownership

15 households are chosen at random to own the 15 corporations, one each. The other 485 households have no corporation.

All 485 are eligible to found on turn 1 (`credits = 100 000 000 ≥ MIN_CAPITAL = 10 000 000`). Because all households have identical wealth, `maxWealth == minWealth`, so the degenerate rule in the founding section applies: `wealthFraction = 1` for every eligible household, and each rolls at `FOUNDING_CHANCE` = 5 %. **Expect ~24 new corporations founded on turn 1**, each taking `MIN_CAPITAL` from its founder.

New corporations start with no buildings, so they enter as manual producers. At the seed prices, the profitable manual sectors are food (unit cost 3.333 at price 4) and service (break-even at 40); materials, fuel, energy and consumer goods are all unprofitable at manual rates. A new corporation that samples one of those sectors must use its `JUMPSTART_BUDGET` on its first building — which `MIN_CAPITAL` cannot afford (see [Scale problem](#scale-problem)).

## Turn 1 and the First Turns

- **Turn 1:** no household is in emergency mode. Every sector produces its seeded capacity; supply ≈ demand in every sector, so prices settle at the seed values by the tie-break rule. Labour settles at 40 with 286.2M units unsold (42.8 % employment). Households eat 11.43 kg per person, population grows 0.475 %, food stock drops to 28.58M kg. The founding roll adds ~24 corporations.
- **Turns 2–4:** the founding roll keeps firing (485 eligible households, 5 % each). New food and service corporations hire from the labour slack, raising employment. Household credits drift down toward the steady-state level `1.75 × income`; the starting 1M-credit endowment is spent over the first few turns.
- **Turn 3 onward:** household food stock reaches zero, so every household submits an emergency order for its survival need at `lastFoodPrice × EMERGENCY_BID`. Food demand becomes inelastic, the food corporations' profit-maximising price rises to the bid, and the food price ratchets upward by `EMERGENCY_BID` per turn until the supply response breaks the shortage. This is the intended famine-adjacent dynamics of the model, and it is the first interesting thing the player sees.

## Scale Problem

**A manual-only colony cannot feed 500M people.** Feeding them at survival level requires 5 000 000 000 kg per turn. At the manual rate of 12 kg per labour unit, that is 416 666 667 labour units — **83 % of the entire labour force** — costing 16 666 666 680 centicredits (166.7M credits) per turn. Fifteen corporations at `MIN_CAPITAL` have 1.5M credits in total, 0.9 % of that. With 3 weeks of food stock, the trajectory per household is:

| Turn | `foodStock` at start | survival need | eaten | population at end |
|------|----------------------|---------------|-------|-------------------|
| 1 | 30 000 000 | 10 000 000 | 14 333 333 | 1 014 444 |
| 2 | 15 681 067 | 10 144 440 | 13 479 000 | 1 025 543 |
| 3 | 2 216 467 | 10 255 430 | 2 230 866 | 222 000 |
| 4 | 0 | 2 220 000 | 14 400 | 1 441 |
| 5 | 0 | 14 410 | 14 400 | 1 440 |

Colony population: 500 000 000 → 720 000. **A 99.86 % die-off in 4 turns.** The economy survives (from turn 5 the food supply exceeds the surviving demand), but the starting population is destroyed.

**Tier 2 is profitable with the current building table.** Tier 2 food: 200 000 labour slots × 60 kg = 12 000 000 kg per building per turn, unit cost `40/60 + 15 × 20 000 / 12 000 000 = 0.6917` at labour 40 and energy 15. Feeding 500M people takes 417 tier 2 buildings, 83.4M labour units (16.7 % of the workforce), 8.34M energy units per turn, and 9.37 bn centicredits of construction capital. The productivity ladder in the model spec is intact; no change to the building table is needed.

**The binding constraint is capital, not productivity.** The seeded snapshot needs 14 328 000 000 centicredits of construction capital (paid in the fictional turn 0) and 9 127 980 000 of working capital. The household pool at 1M credits per household is 50 bn centicredits, so the snapshot is affordable. At 100M credits per household (10 bn centicredits each) the money supply is 250× the price scale: the service share alone demands 83 billion services against 500M labour units, and every price blows to `PRICE_CAP` on turn 1.

**`MIN_CAPITAL` is too small for the jump-start rule.** A tier 2 building costs `500 000 × 40 + 400 000 × 6 = 22 400 000` centicredits; a power plant costs `300 000 × 40 + 300 000 × 6 = 13 800 000`. With `JUMPSTART_BUDGET = 0.50` and `MIN_CAPITAL = 10 000 000`, a new corporation can spend 5 000 000 — it cannot afford any building. `MIN_CAPITAL` must be at least `construction cost / JUMPSTART_BUDGET`, i.e. **27 600 000 for a power plant** and **44 800 000 for a tier 2 building**. Raising `MIN_CAPITAL` to 45 000 000 makes the jump-start rule usable for every sector; it also raises the founding eligibility threshold, which is the intended effect.

## What else must be defined

The initial state is not complete until all of these are fixed. The ones marked ✓ are defined above.

- ✓ Seed prices for all seven traded resources (labour included), chosen so every seeded sector is profitable at the tier it uses.
- ✓ Seed capacity: building counts, staffing, and the intermediate demand that keeps them powered.
- ✓ Seed beliefs for every agent-resource pair, including the halfwidth rule.
- ✓ `foodPriceHistory` initialised to `WAGE_SMOOTH` copies of the seed food price.
- ✓ `lastIncome` for households and corporations.
- ✓ Seed `sectorDemand` curves for every corporation.
- ✓ Propensity ranges.
- ✓ Ownership assignment.
- ✓ Starting buildings, stocks and corporation capital.
- **`MIN_CAPITAL`** — must be raised to make the jump-start rule usable (see above).
- **Agent ID assignment** — household and corporation IDs, and the order they are stored in, since rationing tie-breaks use ascending order ID.
- **RNG seed** — recorded with the snapshot so a debug replay reproduces propensities, ownership and the turn-1 founding rolls exactly.
- **Turn numbering** — turn 1 is the first tick; the seed prices are labelled turn 0 and never run.
- **Initial player-visible state** — the colony modal and empire screen must render from the snapshot with no history, so charts start empty and trend arrows show no trend.
- **Not defined here**: government, taxes, banking, central bank, unemployment benefits, inter-colony trade and transport.

## Initial-state invariants

The snapshot must satisfy:

- Total credits = household credits + corporation credits = 62 030 000 000 centicredits. The initial state creates no credits.
- All standard resources are in agent inventories: food in households (15 000 000 000 kg), nothing in corporations.
- No agent sells more than it owns. With empty corporation stocks, no corporation can submit a sell order on turn 1; they sell only what they produce that turn.
- Every seed price is in `[PRICE_FLOOR, PRICE_CAP]`.
- `foodPriceHistory` has exactly `WAGE_SMOOTH` entries, all equal to the seed food price.
- `smoothedFoodPrice × FOOD_PER_PERSON == seedLabourPrice`.
- Every corporation has exactly one owner; every household owns at most one.
- `population == POP_BASE` for every household.
- Every seeded tier 2 building's sector has a seed price above its unit cost.
- Seeded energy supply ≥ seeded energy demand; seeded fuel supply ≥ seeded fuel demand; seeded materials supply ≥ seeded materials demand.
- For every corporation, `credits × (1 - RESERVE_FRACTION) ≥` one turn of its operating costs (staffing, maintenance, energy, fuel, input materials).
- Total labour purchased by the seeded plan (staffing + maintenance + service) ≤ total labour supply.
- Seeded capacity satisfies the steady-state demand at the seed prices, so turn 1 settles at the seed prices.
