#include "Founding.h"

#include "EconomyConstants.h"

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>

namespace Simulation::Economy {

namespace {

    constexpr std::array<ResourceId, SECTOR_COUNT> SECTOR_RESOURCES{
        ResourceId::Food,
        ResourceId::Materials,
        ResourceId::Fuel,
        ResourceId::Energy,
        ResourceId::Service,
        ResourceId::ConsumerGoods,
    };

    constexpr std::uint64_t ROLL_STREAM = 0;
    constexpr std::uint64_t SECTOR_STREAM = 1;

    [[nodiscard]] std::uint64_t chooseSector(SectorWeights const &weights, Rng const &rng, std::uint64_t streamId) {
        std::uint64_t const totalWeight = std::accumulate(weights.begin(), weights.end(), std::uint64_t{});
        std::uint64_t const draw = rng.substream(streamId).uniformInt(0, totalWeight - 1);
        std::uint64_t cumulativeWeight = 0;
        for (std::size_t index = 0; index < weights.size(); ++index) {
            cumulativeWeight += weights[index];
            if (draw < cumulativeWeight) {
                return static_cast<std::uint64_t>(index);
            }
        }
        return static_cast<std::uint64_t>(weights.size() - 1);
    }

    [[nodiscard]] bool alreadyOwnsCorporation(ColonyEconomy const &colony, HouseholdId householdId) {
        return std::any_of(colony.corporations.begin(),
                           colony.corporations.end(),
                           [householdId](Corporation const &corporation) { return corporation.owner == householdId; });
    }

} // namespace

double foundingProbability(std::int64_t credits, FoundingWealthRange wealthRange) noexcept {
    if (wealthRange.maxWealth == wealthRange.minWealth) {
        return FOUNDING_CHANCE;
    }
    double const wealthFraction =
        (static_cast<double>(credits) - static_cast<double>(wealthRange.minWealth)) /
        (static_cast<double>(wealthRange.maxWealth) - static_cast<double>(wealthRange.minWealth));
    return std::clamp(wealthFraction, 0.0, 1.0) * FOUNDING_CHANCE;
}

SectorWeights sectorWeightsFor(FoundingMarketData const &marketData) noexcept {
    SectorWeights weights{};
    constexpr std::uint64_t MAX_SECTOR_WEIGHT = std::numeric_limits<std::uint64_t>::max() / SECTOR_COUNT;
    std::uint64_t const floorWeight = static_cast<std::uint64_t>(FLOOR_WEIGHT);
    for (std::size_t index = 0; index < SECTOR_COUNT; ++index) {
        std::size_t const resourceIndex = static_cast<std::size_t>(SECTOR_RESOURCES[index]);
        std::int64_t const demand = std::max<std::int64_t>(0, marketData.demand[resourceIndex]);
        std::int64_t const filled = std::max<std::int64_t>(0, marketData.filled[resourceIndex]);
        std::int64_t const price = std::max<std::int64_t>(0, marketData.settlementPrices[resourceIndex]);
        std::int64_t const unsatisfied = demand > filled ? demand - filled : 0;
        std::uint64_t const maximumDemandValue = MAX_SECTOR_WEIGHT - floorWeight;
        std::uint64_t unmetDemandValue = static_cast<std::uint64_t>(unsatisfied);
        std::uint64_t const priceValue = static_cast<std::uint64_t>(price);
        if (priceValue != 0 && unmetDemandValue > maximumDemandValue / priceValue) {
            unmetDemandValue = maximumDemandValue;
        } else {
            unmetDemandValue *= priceValue;
        }
        weights[index] = unmetDemandValue + floorWeight;
    }
    return weights;
}

FoundingResult rollFoundings(ColonyEconomy const &colony, FoundingMarketData const &marketData, Rng const &rng) {
    FoundingResult result;
    if (colony.households.empty()) {
        return result;
    }

    auto const wealthBounds =
        std::minmax_element(colony.households.begin(),
                            colony.households.end(),
                            [](Household const &left, Household const &right) { return left.credits < right.credits; });
    FoundingWealthRange const wealthRange{wealthBounds.first->credits, wealthBounds.second->credits};
    SectorWeights const weights = sectorWeightsFor(marketData);

    for (Household const &household : colony.households) {
        if (household.credits < MIN_CAPITAL || alreadyOwnsCorporation(colony, household.id)) {
            continue;
        }

        std::uint64_t const householdStreamId = static_cast<std::uint64_t>(household.id.value) * 2;
        Rng rollRng = rng.substream(householdStreamId + ROLL_STREAM);
        if (!rollRng.bernoulli(foundingProbability(household.credits, wealthRange))) {
            continue;
        }

        Rng const sectorRoot = rng.substream(householdStreamId + SECTOR_STREAM);
        std::uint64_t const sectorIndex = chooseSector(weights, sectorRoot, 0);
        result.corporations.push_back({household.id, static_cast<SectorId>(sectorIndex), MIN_CAPITAL});
    }

    return result;
}

} // namespace Simulation::Economy
