#include "EconomyTickOrchestrator.h"

#include "EconomyConstants.h"
#include "InitialConditions.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace Simulation::Economy {
namespace {

    [[nodiscard]] std::int64_t sumCredits(ColonyEconomy const &colony) {
        std::int64_t total{};
        for (Household const &household : colony.households) {
            if (household.credits < 0 || total > std::numeric_limits<std::int64_t>::max() - household.credits) {
                throw std::logic_error("Household credits are negative or overflow the colony total");
            }
            total += household.credits;
        }
        for (Corporation const &corporation : colony.corporations) {
            if (corporation.credits < 0 || total > std::numeric_limits<std::int64_t>::max() - corporation.credits) {
                throw std::logic_error("Corporation credits are negative or overflow the colony total");
            }
            total += corporation.credits;
        }
        return total;
    }

    void validateResult(ColonyEconomy const &previous, ColonyTickResult const &result) {
        if (result.colony.id != previous.id || result.colony.turnNumber != previous.turnNumber + 1) {
            throw std::logic_error("Colony tick changed its identity or did not advance exactly one turn");
        }
        std::int64_t population{};
        for (Household const &household : result.colony.households) {
            if (household.population < 0 || household.foodStock < 0) {
                throw std::logic_error("Colony tick produced a negative household population or food stock");
            }
            if (population > std::numeric_limits<std::int64_t>::max() - household.population) {
                throw std::logic_error("Colony population total overflowed");
            }
            population += household.population;
        }
        if (population != result.colony.totalPopulation) {
            throw std::logic_error("Colony population cache does not match household populations");
        }
        for (Corporation const &corporation : result.colony.corporations) {
            if (corporation.credits < 0) {
                throw std::logic_error("Colony tick produced corporation " + std::to_string(corporation.id.value) +
                                       " with credits " + std::to_string(corporation.credits));
            }
            for (std::size_t resourceIndex = 0; resourceIndex < RESOURCE_COUNT; ++resourceIndex) {
                if (corporation.stock.amounts[resourceIndex] < 0) {
                    throw std::logic_error("Colony tick produced negative corporate inventory");
                }
            }
        }
        for (auto const &[resource, price] : result.colony.market.lastPrice) {
            static_cast<void>(resource);
            if (resource != ResourceId::Credits && (price < PRICE_FLOOR || price > PRICE_CAP)) {
                throw std::logic_error("Colony tick produced a settlement price outside the allowed range");
            }
        }
        std::int64_t const previousCredits = sumCredits(previous);
        if (result.liquidationCredits < 0 ||
            previousCredits > std::numeric_limits<std::int64_t>::max() - result.liquidationCredits ||
            sumCredits(result.colony) != previousCredits + result.liquidationCredits) {
            throw std::logic_error("Colony tick failed credit conservation, including liquidation proceeds");
        }
    }

    [[nodiscard]] std::int64_t targetPeriod(double const gameHours) {
        if (!std::isfinite(gameHours)) {
            throw std::invalid_argument("Game hours must be finite");
        }
        if (gameHours <= 0.0) {
            return 0;
        }
        long double const periods = std::floor(static_cast<long double>(gameHours) / ECONOMY_TICK_HOURS);
        if (periods > static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
            throw std::overflow_error("Game time exceeds the economic period counter");
        }
        return static_cast<std::int64_t>(periods);
    }

} // namespace

EconomyTickOrchestrator::EconomyTickOrchestrator(std::size_t const workerCount, bool const recordDebug)
    : EconomyTickOrchestrator({buildEarthSeed(ColonyId{0}, 0xA8U)}, workerCount, recordDebug) {
}

EconomyTickOrchestrator::EconomyTickOrchestrator(std::vector<ColonyEconomy> colonies,
                                                 std::size_t const workerCount,
                                                 bool const recordDebug)
    : mWorkerCount(std::max<std::size_t>(1, workerCount))
    , mColonies(std::move(colonies)) {
    std::sort(mColonies.begin(), mColonies.end(), [](ColonyEconomy const &left, ColonyEconomy const &right) {
        return left.id < right.id;
    });
    for (std::size_t index = 1; index < mColonies.size(); ++index) {
        if (mColonies[index - 1].id == mColonies[index].id) {
            throw std::invalid_argument("Economy orchestrator requires unique colony identities");
        }
    }
    mPreviousMarketData.resize(mColonies.size());
    mPlayerHistories.resize(mColonies.size());
    mDebugRecorders.assign(mColonies.size(), DebugRecorder{DEFAULT_DEBUG_RECORD_WINDOW, recordDebug});
}

std::vector<EconomyEventBatch> EconomyTickOrchestrator::advanceTo(double const gameHours) {
    std::int64_t const target = targetPeriod(gameHours);
    while (mCompletedPeriods < target) {
        runOneTurn();
        ++mCompletedPeriods;
        mPendingEvents.insert(mPendingEvents.end(), mLatestEvents.begin(), mLatestEvents.end());
    }
    return takePendingEvents();
}

std::vector<ColonyEconomy> const &EconomyTickOrchestrator::colonies() const noexcept {
    return mColonies;
}

std::vector<ColonyEconomyRecordSnapshot> EconomyTickOrchestrator::recordSnapshots() const {
    std::vector<ColonyEconomyRecordSnapshot> snapshots;
    snapshots.reserve(mColonies.size());
    for (std::size_t index = 0; index < mColonies.size(); ++index) {
        snapshots.push_back({mColonies[index].id, mPlayerHistories[index], mDebugRecorders[index].records()});
    }
    return snapshots;
}

std::int64_t EconomyTickOrchestrator::completedPeriods() const noexcept {
    return mCompletedPeriods;
}

std::vector<EconomyEventBatch> EconomyTickOrchestrator::takePendingEvents() {
    std::vector<EconomyEventBatch> events;
    events.swap(mPendingEvents);
    return events;
}

void EconomyTickOrchestrator::runOneTurn() {
    std::vector<ColonyTickResult> results(mColonies.size());
    std::vector<char> resultReady(mColonies.size());
    std::vector<std::exception_ptr> failures(mColonies.size());
    std::atomic_size_t nextIndex{};
    auto const work = [this, &results, &resultReady, &failures, &nextIndex]() {
        for (;;) {
            std::size_t const index = nextIndex.fetch_add(1);
            if (index >= mColonies.size()) {
                return;
            }
            try {
                results[index] = tickColony(mColonies[index], mPreviousMarketData[index]);
                resultReady[index] = 1;
            } catch (...) {
                failures[index] = std::current_exception();
            }
        }
    };

    std::size_t const workerCount = std::min(mWorkerCount, mColonies.size());
    if (workerCount <= 1) {
        work();
    } else {
        std::vector<std::thread> workers;
        workers.reserve(workerCount);
        for (std::size_t worker = 0; worker < workerCount; ++worker) {
            workers.emplace_back(work);
        }
        for (std::thread &worker : workers) {
            worker.join();
        }
    }

    // The barrier above completes before validation or any live-state mutation.
    std::vector<PlayerRecord> stagedPlayerRecords;
    stagedPlayerRecords.reserve(mColonies.size());
    std::vector<ColonyEconomy> stagedColonies;
    stagedColonies.reserve(mColonies.size());
    std::vector<DebugRecorder> stagedDebugRecorders = mDebugRecorders;
    std::vector<FoundingMarketData> stagedMarketData;
    stagedMarketData.reserve(mColonies.size());
    std::vector<EconomyEventBatch> stagedEvents;
    stagedEvents.reserve(mColonies.size());
    for (std::size_t index = 0; index < mColonies.size(); ++index) {
        if (failures[index] != nullptr) {
            try {
                std::rethrow_exception(failures[index]);
            } catch (std::exception const &error) {
                throw std::runtime_error("Economy tick failed for colony " + std::to_string(mColonies[index].id.value) +
                                         ": " + error.what());
            } catch (...) {
                throw std::runtime_error("Economy tick failed for colony " + std::to_string(mColonies[index].id.value) +
                                         ": unknown error");
            }
        }
        if (resultReady[index] == 0) {
            throw std::runtime_error("Economy tick produced no result for colony " +
                                     std::to_string(mColonies[index].id.value));
        }
        ColonyTickResult const &result = results[index];
        try {
            validateResult(mColonies[index], result);
        } catch (std::exception const &error) {
            throw std::runtime_error("Economy validation failed for colony " +
                                     std::to_string(mColonies[index].id.value) + ": " + error.what());
        }
        stagedPlayerRecords.push_back(makePlayerRecord(mColonies[index], result));
        stagedColonies.push_back(result.colony);
        stagedDebugRecorders[index].append(mColonies[index], result);
        stagedMarketData.push_back(result.marketData);
        stagedEvents.push_back({result.colony.id,
                                result.colony.turnNumber - 1,
                                result.settlements,
                                result.founded,
                                result.dissolvedCorporations,
                                result.dissolvedHouseholds});
    }

    std::vector<std::vector<PlayerRecord>> stagedHistories = mPlayerHistories;
    for (std::size_t index = 0; index < mColonies.size(); ++index) {
        stagedHistories[index].push_back(stagedPlayerRecords[index]);
    }
    // All colonies, records, and event data become visible only after the whole turn validates.
    mColonies.swap(stagedColonies);
    mPreviousMarketData.swap(stagedMarketData);
    mPlayerHistories.swap(stagedHistories);
    mDebugRecorders.swap(stagedDebugRecorders);
    mLatestEvents.swap(stagedEvents);
}

} // namespace Simulation::Economy
