# Economy Visualization

Two audiences, **two separate data streams**. The player needs a small set of readable numbers and trends; the developer needs everything. These are captured as two distinct outputs of the tick, deliberately not merged, so the debug stream can be switched off later without affecting the player UI (see the "Two separate output streams" section of [economy-implementation.md](./economy-implementation.md)).

The economy module emits both records as part of the tick commit. Visualization is a **reader** of those records: it never recomputes the economy, and it never mutates it. The player UI reads only the player-facing stream; the debug overlay reads only the debug stream.

## The UI

### Starting point

The current UI has one relevant element: when the player selects an in-game object (a star, a planet or moon with or without a colony, later ships, bases and other objects), an **info box in the lower-left corner** shows the object's name. The economy UI builds on that selection model rather than inventing a new one — the colony economy is reached by selecting a colony.

### Info box: economy summary

When the selected object is a colony, the info box shows the object's name plus a **compact economy summary** — enough to tell the player whether the colony needs attention, not enough to fill the box:

- Population, food coverage in turns, unemployment.
- The four prices that matter most right now (food, energy, materials, consumer goods), each with an up/down arrow.
- Corporation count per sector, and a warning badge if any warning flag is set (starvation risk, unpaid maintenance, corporation near bankruptcy, consumer market collapse).

The info box stays small. It is a triage surface: its job is to say "this colony is fine" or "open this colony".

### Colony economy panel

The info box has an **Economy** action. Clicking it opens a **centered modal panel** over the map, for that colony. The modal is used rather than a side panel because the per-colony view is dense (tables plus charts) and a centered window has the most room without permanently covering the map.

The modal has three regions:

- **Left column — status**: prices for all resources with trend arrows, stocks with turns of coverage, food demand vs filled, **household food reserves as turns of stock**, services and consumer goods demand vs filled, labour supplied / employed / unemployed, household count with average wealth and savings propensity, and a **famine badge** when any household is in emergency mode.
- **Right column — corporations**: a table of the colony's corporations (sector, credits, buildings, output, stock held), one row per corporation. Selecting a row expands that corporation's detail: its buildings, its plan for the current turn, and what it is buying and selling. Service corporations show no buildings and no stock; their row shows labour hired and services sold.
- **Bottom strip — charts**: the per-colony charts (price per resource, food demand vs filled, services and consumer goods demand vs filled, unemployment, corporation count per sector, credits split household/corporation).

The modal is **modal**: it blocks map input, and Esc closes it. It does not pause the game. The last colony opened stays the default target when the modal is reopened.

### Empire economy screen

The aggregated empire view lives on a **dedicated screen opened from the top bar**, not in the colony modal. It is a separate screen because it is read across colonies, and there is no single colony to select.

The screen shows:

- **Empire totals** as a row of headline numbers: population, total credits, food produced and consumed, materials, fuel, energy, colony count.
- **Trend charts** for those totals over time.
- **Per-sector breakdown** — totals split by resource, so a sector-wide shortage is visible.
- **Colony ranking table** — colonies sorted by food coverage, unemployment, and corporation count, with the worst colonies at the top. Selecting a colony in this table closes the screen, selects that colony on the map, and opens its economy modal.

### Chart behaviour

Charts are **hover-only**: hovering a chart shows a tooltip with the exact values at that turn (the value of each series, and the turn number). There is no click interaction on charts in the player UI. Tooltips show numbers, not scaled shapes, so the player can read the exact price or quantity.

### Debug overlay

The debug overlay is entered with a **developer-mode keybind**, and it **layers on top of the normal UI** rather than replacing it. This keeps the map, the info box and the colony modal visible while debugging, so the developer can see what the player sees at the same time as the internal state.

Developer mode is a global switch: when it is on, the overlay panels are available and the debug recording switch is on. When it is off, no debug panels exist and no debug data is recorded.

The overlay consists of four dockable panels, all reading the debug stream:

- **Turn scrubber** — a **timeline slider at the bottom of the screen** spanning the recorded rolling window, with keyboard stepping (arrow keys move one turn, Shift+arrow moves ten). The scrubber sets the turn that every debug panel shows. The game continues to run at the current turn; the scrubber does not move the game, it moves the debug view.
- **Colony inspector** — the full state of one colony at the scrubbed turn: every household (population, credits, propensities, beliefs including `serviceBelief` and `consumerGoodsBelief`, `foodStock`, consumption target, `foodEaten`, `reserveTurns`, emergency-mode flag, `foodPriceHistory`, the three budget shares and what was spent in each), every corporation (sector, credits, buildings, `sectorDemand`, `stockUnitCost`, sector belief), every building (type, staffed/idle, under construction, maintenance paid), every stock.
- **Market inspector** — for a chosen `(colony, resource)`: the order book with every order's price, quantity and fill, and the settlement trace: `Qs(p)`, `Qd(p)`, `Q(p)` for each candidate price, the maximiser interval, and the tie-break that picked the price.
- **Planning inspector** — for a chosen corporation: the demand curve it planned against, the profit-maximising price `p*` and quantity `Q*`, the unit cost of every method, the break-even price, the budget, staffing order, idle capacity, the surplus decision, the construction gate result, and whether the budget clamp fired.

The colony and market inspectors are **linked to the map selection**: selecting a colony on the map makes it the subject of the colony inspector; clicking a resource in the colony inspector makes that market the subject of the market inspector. This is what makes the overlay fast to use — the developer navigates by selecting objects in the game, not by typing identifiers.

The overlay also shows the **invariant checks** as a pass/fail strip (credits conserved, standard resources conserved, no agent overspent, prices in range), with the failing colony and turn named.

**Replay** is triggered from the colony inspector: a button re-runs that colony's tick from the scrubbed snapshot, and the result is shown side by side with the recorded result. Because the tick is pure with a per-colony RNG seed, the two must match; a mismatch is a bug, and the diff is shown.

### Interaction rules

- **Selection is the entry point for everything.** Colony data is reached by selecting a colony; corporation and market data are reached by selecting within the colony's view. There is no search box and no free-text navigation.
- **Warnings are clickable.** A warning badge in the info box opens the colony modal with the relevant section highlighted.
- **Numbers are shown as in-game units** (tons for food/materials/fuel, credits not centicredits, population as headcount), with the raw internal values available only in the debug overlay.
- **The player UI never shows internal state that the model does not expose to the player** — no price beliefs, no demand curves, no break-even costs, no RNG seeds. Those are debug-only.
- **The debug overlay never changes game state.** It is read-only, with the single exception of replay, which computes a result without committing it.

## What the player sees

### Per colony

The colony view answers one question: *is this colony healthy, and what is it missing?*

- **Prices** — the current settlement price for each resource, with the direction of change from the previous turn. Prices are the most legible signal in the model: a rising food price means the colony is short of food, a rising energy price means industry is being throttled.
- **Stocks** — food, materials and fuel held in the colony, expressed as **turns of coverage** (stock divided by the colony's current consumption rate) rather than raw kilograms. Turns of coverage is the number the player can act on.
- **Food balance** — total food demand versus total food filled, as a fraction of the population fed. This is the colony's survival indicator.
- **Food reserves** — the colony's household food stock expressed as **turns of reserve**, with the distribution across households (how many households have less than one turn of stock). A colony with no reserves is fragile: a single bad turn of supply becomes a famine. A colony with 4 turns of reserve can absorb a shortage, and its households are not bidding at famine prices.
- **Population trend** — net population change for the turn, split into starvation losses and growth from overconsumption. This is the clearest read on whether the colony is feeding its people.
- **Labour** — total labour supplied, total labour employed, and unemployment. High unemployment with unmet demand means the colony has idle capacity; full employment with unmet demand means it needs buildings.
- **Consumer market** — services and consumer goods demand versus filled, and the household budget split between food, services and consumer goods. This is the read on consumerism: a colony spending 90 % of its budget on food has a starved consumer sector, and a colony spending a third on services and goods has a thriving one. A consumer market that goes unfilled means service and consumer goods corporations are about to become unprofitable.
- **Corporations** — count, sector, credits, buildings owned, and for each corporation: planned output, actual output, stock held, and profit for the turn. A corporation holding large stock while the price is falling is the visible signal that the sector is oversupplied; a corporation producing far below its plan means it cannot afford to staff its buildings.
- **Households** — count, average wealth, and average savings propensity. Savings are the fuel for new corporations, so a colony with no savings has no new corporations.
- **Warnings** — explicit flags for the conditions the model can reach: households in emergency mode (stock below one turn of need), households approaching starvation, corporations near zero credits, buildings whose maintenance was not paid, tier 2 buildings left idle for lack of energy, and a food settlement price at the emergency bid ceiling (a sign the shortage is not being resolved).

### Empire overview

The empire view answers: *where is the empire as a whole going, and which colonies are failing?*

- **Empire totals** — population, total credits, total food produced and consumed, total materials, fuel and energy, number of colonies.
- **Trends** — the same totals over time, so the player sees growth or collapse rather than a single snapshot.
- **Per-sector breakdown** — totals split by resource, which is where a sector-wide shortage shows up (an empire with no energy sector cannot staff tier 2 buildings anywhere).
- **Colony comparison** — colonies ranked by food coverage, unemployment, and corporation count. The point is to find the outliers: the colony that is starving, and the colony that is producing a surplus the empire is not using.

### Charts

Key trends get charts in the game UI, not just numbers. The charts are placed as described in the UI section (per-colony charts in the colony modal, empire trends on the empire screen):

- Price per resource over time (one chart per resource, all resources comparable on a shared scale).
- Food demand versus food filled over time.
- Services and consumer goods demand versus filled over time.
- Unemployment over time.
- Corporation count per sector over time.
- Total credits over time, split into household credits and corporation credits.

Charts are limited to these; every chart must answer a decision the player can make (build a power plant, found a corporation, expand a sector). Charts that cannot drive a decision are not shown.

### Visibility in multiplayer

The player sees **their own colonies in full**. Other colonies are shown at a public, aggregate level: prices and stock coverage, not the household and corporation ledgers. Economic data is per-colony, so this is a filter on what the server sends to the client, not a separate view.

## Debug visualization

### Recording

Every turn, for every colony, the economy records a **full snapshot**: households (population, credits, propensities, beliefs including `serviceBelief` and `consumerGoodsBelief`, `foodStock`, consumption target, `foodEaten`, `reserveTurns`, emergency-mode flag, `foodPriceHistory`, the three budget shares and the spending in each category), corporations (sector, credits, buildings, `sectorDemand`, `stockUnitCost`, sector belief), building inventory, resource stocks, the settlement price for each resource, the fills per order, and the events (founding, dissolution, starvation, overconsumption growth, maintenance failure, emergency-mode entry, consumer market collapse).

Full snapshots make any question answerable after the fact, which is what debugging an agent-based model requires: the interesting failures are emergent, and they cannot be reproduced from summary metrics.

The record is **kept in a rolling window in memory** (the last N turns, N tunable), and **persisted on demand** — a specific turn range, a specific colony, or a full run can be written to disk when the developer asks. Nothing is persisted for the whole game by default.

This is the **debug stream**, and it is gated by a recording switch from day one: when the switch is off, nothing in this section is produced, and the debug overlay has no data. The player-facing stream is unaffected by the switch.

### The in-game debug overlay

The overlay's panels, layout and controls are specified in the **Debug overlay** section above. What matters here is what they read: the recorded full snapshots, which make any question answerable after the fact. The overlay reuses the game's own rendering and needs no external tooling, which keeps the debug loop short: run the game, enter developer mode, scrub to the turn where something went wrong.

### The questions the overlay must be able to answer

The overlay is judged by whether these are answerable, not by how it looks:

- Why is the food price in this colony rising every turn?
- Why did this corporation dissolve on this turn?
- Why was no corporation founded in this colony for 20 turns?
- Why is this tier 2 building idle?
- Why did this market settle at this price when the order book looked like that?
- Why is the colony's labour unemployed while demand is unmet?
- Where did the credits go — who bought what, and who received it?
- Why did the consumer goods market collapse this turn — did the food price eat the household budget?
- Why is this service corporation producing nothing — is the labour price above the service price?

## Non-goals

- **No external dashboard.** The overlay is in-game; no separate tool reads exported files.
- **No continuous-time visualization.** The economy is turn-based; the overlay shows discrete turns.
- **No inter-colony views.** Transport between colonies and megacorporations are out of scope in the model, so there is nothing to show across colonies.
- **No live streaming of the tick.** The tick is blocking and atomic; the overlay reads committed turns, never a tick in progress.
