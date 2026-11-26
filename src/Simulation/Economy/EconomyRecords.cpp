#include "EconomyRecords.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace Simulation::Economy {
namespace {

    [[nodiscard]] std::int64_t checkedAdd(std::int64_t const left, std::int64_t const right) {
        if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) ||
            (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)) {
            throw std::overflow_error("Economy record total exceeds int64_t");
        }
        return left + right;
    }

    [[nodiscard]] std::int64_t populationOf(ColonyEconomy const &colony) {
        std::int64_t total{};
        for (Household const &household : colony.households) {
            total = checkedAdd(total, household.population);
        }
        return total;
    }

    [[nodiscard]] SectorId
    sectorFor(CorporationId const id, ColonyEconomy const &previous, ColonyEconomy const &after) {
        auto const current = std::find_if(after.corporations.begin(),
                                          after.corporations.end(),
                                          [id](Corporation const &corporation) { return corporation.id == id; });
        if (current != after.corporations.end()) {
            return current->sector;
        }
        auto const former = std::find_if(previous.corporations.begin(),
                                         previous.corporations.end(),
                                         [id](Corporation const &corporation) { return corporation.id == id; });
        return former == previous.corporations.end() ? SectorId::Food : former->sector;
    }

    [[nodiscard]] std::size_t sectorIndex(SectorId const sector) {
        std::size_t const index = static_cast<std::size_t>(sector);
        if (index >= SECTOR_COUNT) {
            throw std::invalid_argument("Economy record received an invalid corporation sector");
        }
        return index;
    }

} // namespace

PlayerRecord makePlayerRecord(ColonyEconomy const &previous, ColonyTickResult const &result) {
    PlayerRecord record;
    record.colonyId = result.colony.id;
    record.turnNumber = result.colony.turnNumber - 1;
    record.population = populationOf(result.colony);
    record.populationChange = record.population - populationOf(previous);
    record.householdCount = static_cast<std::int64_t>(result.colony.households.size());

    for (std::size_t index = 0; index < RESOURCE_COUNT; ++index) {
        ResourceId const resource = static_cast<ResourceId>(index);
        record.settlementPrices[index] = result.marketData.settlementPrices[index];
        record.priceChanges[index] = record.settlementPrices[index] - previous.market.lastPriceFor(resource);
        record.buyDemand[index] = result.marketData.demand[index];
        record.buyFilled[index] = result.marketData.filled[index];
        if (isStandardResource(resource)) {
            for (Corporation const &corporation : result.colony.corporations) {
                record.stocks[index] = checkedAdd(record.stocks[index], corporation.stock[resource]);
            }
            if (resource == ResourceId::Food) {
                for (Household const &household : result.colony.households) {
                    record.stocks[index] = checkedAdd(record.stocks[index], household.foodStock);
                }
            }
        }
    }

    std::int64_t totalReserveTurns{};
    for (Household const &household : result.colony.households) {
        record.householdCredits = checkedAdd(record.householdCredits, household.credits);
        record.averageSavingsPropensity += household.propSavings;
        if (household.population > 0) {
            double const reserveTurns = static_cast<double>(household.foodStock) /
                                        (static_cast<double>(household.population) * FOOD_PER_PERSON);
            totalReserveTurns = checkedAdd(totalReserveTurns, 1);
            record.averageFoodReserveTurns += reserveTurns;
            if (reserveTurns < 1.0) {
                record.householdsBelowOneFoodReserveTurn = checkedAdd(record.householdsBelowOneFoodReserveTurn, 1);
            }
        }
    }
    if (record.householdCount > 0) {
        record.averageHouseholdCredits =
            static_cast<double>(record.householdCredits) / static_cast<double>(record.householdCount);
        record.averageSavingsPropensity /= static_cast<double>(record.householdCount);
    }
    if (totalReserveTurns > 0) {
        record.averageFoodReserveTurns /= static_cast<double>(totalReserveTurns);
    }

    for (Corporation const &corporation : result.colony.corporations) {
        std::size_t const index = sectorIndex(corporation.sector);
        record.corporationCounts[index] = checkedAdd(record.corporationCounts[index], 1);
        record.corporationCredits = checkedAdd(record.corporationCredits, corporation.credits);
    }

    for (HouseholdTickDelta const &delta : result.householdDeltas) {
        record.foodConsumed = checkedAdd(record.foodConsumed, delta.foodEaten);
        record.servicesConsumed = checkedAdd(record.servicesConsumed, delta.servicesConsumed);
        record.consumerGoodsConsumed = checkedAdd(record.consumerGoodsConsumed, delta.consumerGoodsConsumed);
    }
    for (CorporationTickDelta const &delta : result.corporationDeltas) {
        std::size_t const index = sectorIndex(sectorFor(delta.id, previous, result.colony));
        record.produced[index] = checkedAdd(record.produced[index], delta.produced);
        record.sold[index] = checkedAdd(record.sold[index], delta.sold);
    }

    for (auto const &[resource, book] : result.colony.market.orderBooks) {
        if (resource != ResourceId::Labour) {
            continue;
        }
        for (Order const &order : book.sellOrders) {
            if (std::holds_alternative<HouseholdId>(order.owner)) {
                record.labourSupplied = checkedAdd(record.labourSupplied, order.quantity);
            }
        }
    }
    for (SettlementOutcome const &settlement : result.settlements) {
        if (settlement.resource != ResourceId::Labour) {
            continue;
        }
        for (Trade const &trade : settlement.trades) {
            if (trade.side == OrderSide::Sell && std::holds_alternative<HouseholdId>(trade.owner)) {
                record.labourEmployed = checkedAdd(record.labourEmployed, trade.quantity);
            }
        }
    }

    for (FoundedCorporation const &founding : result.founded) {
        std::size_t const index = sectorIndex(founding.sector);
        record.corporationsFounded[index] = checkedAdd(record.corporationsFounded[index], 1);
    }
    record.corporationsDissolved = static_cast<std::int64_t>(result.dissolvedCorporations.size());
    record.householdsDissolved = static_cast<std::int64_t>(result.dissolvedHouseholds.size());
    return record;
}

DebugRecord makeDebugRecord(ColonyEconomy const &previous, ColonyTickResult const &result) {
    DebugRecord record;
    record.colonyId = result.colony.id;
    record.turnNumber = previous.turnNumber;
    record.beforeState = previous;
    record.afterState = result.colony;
    record.settlements = result.settlements;
    record.marketData = result.marketData;
    record.founded = result.founded;
    record.dissolvedCorporations = result.dissolvedCorporations;
    record.dissolvedHouseholds = result.dissolvedHouseholds;
    record.householdDeltas = result.householdDeltas;
    record.corporationDeltas = result.corporationDeltas;
    record.liquidationCredits = result.liquidationCredits;
    return record;
}

DebugRecorder::DebugRecorder(std::size_t const windowSize, bool const enabled)
    : mWindowSize(windowSize)
    , mEnabled(enabled) {
}

void DebugRecorder::setEnabled(bool const enabled) noexcept {
    mEnabled = enabled;
}

bool DebugRecorder::enabled() const noexcept {
    return mEnabled;
}

void DebugRecorder::append(ColonyEconomy const &previous, ColonyTickResult const &result) {
    if (!mEnabled || mWindowSize == 0) {
        return;
    }
    mRecords.push_back(makeDebugRecord(previous, result));
    while (mRecords.size() > mWindowSize) {
        mRecords.pop_front();
    }
}

std::deque<DebugRecord> const &DebugRecorder::records() const noexcept {
    return mRecords;
}

std::size_t DebugRecorder::windowSize() const noexcept {
    return mWindowSize;
}

} // namespace Simulation::Economy
