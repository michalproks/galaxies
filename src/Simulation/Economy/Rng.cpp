#include "Rng.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace Simulation::Economy {

namespace {

    constexpr std::uint64_t SPLITMIX_GAMMA = 0x9E3779B97F4A7C15ULL;
    constexpr std::uint64_t TURN_DOMAIN = 0xD1B54A32D192ED03ULL;
    constexpr std::uint64_t STREAM_DOMAIN = 0x8CB92BA72F3D8DD7ULL;
    constexpr double UINT64_TO_UNIT_DOUBLE = 1.0 / 9007199254740992.0;

    std::uint64_t splitMix64(std::uint64_t value) {
        value += SPLITMIX_GAMMA;
        value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
        return value ^ (value >> 31U);
    }

    std::array<std::uint64_t, 4> makeState(std::uint64_t seed) {
        std::array<std::uint64_t, 4> state{};
        for (std::size_t index = 0; index < state.size(); ++index) {
            state[index] = splitMix64(seed + static_cast<std::uint64_t>(index) * SPLITMIX_GAMMA);
        }
        return state;
    }

} // namespace

Rng::Rng(ColonyId colonyId, std::uint64_t turn)
    : mSeed(splitMix64(static_cast<std::uint64_t>(colonyId.value) ^ splitMix64(turn ^ TURN_DOMAIN)))
    , mState(makeState(mSeed)) {
}

Rng::Rng(std::uint64_t seed, SeedTag)
    : mSeed(seed)
    , mState(makeState(seed)) {
}

std::uint64_t Rng::next() {
    std::uint64_t const result = std::rotl(mState[1] * 5ULL, 7) * 9ULL;
    std::uint64_t const temporary = mState[1] << 17U;

    mState[2] ^= mState[0];
    mState[3] ^= mState[1];
    mState[1] ^= mState[2];
    mState[0] ^= mState[3];
    mState[2] ^= temporary;
    mState[3] = std::rotl(mState[3], 45);

    return result;
}

double Rng::uniformDouble() {
    return static_cast<double>(next() >> 11U) * UINT64_TO_UNIT_DOUBLE;
}

std::uint64_t Rng::uniformInt(std::uint64_t min, std::uint64_t max) {
    if (min > max) {
        throw std::invalid_argument("Rng::uniformInt requires min <= max");
    }

    std::uint64_t const range = max - min + 1ULL;
    if (range == 0ULL) {
        return next();
    }

    std::uint64_t const rejectionThreshold = (0ULL - range) % range;
    std::uint64_t value = next();
    while (value < rejectionThreshold) {
        value = next();
    }
    return min + value % range;
}

bool Rng::bernoulli(double probability) {
    if (!(probability >= 0.0 && probability <= 1.0) || !std::isfinite(probability)) {
        throw std::invalid_argument("Rng::bernoulli requires a finite probability in [0, 1]");
    }
    return uniformDouble() < probability;
}

Rng Rng::substream(std::uint64_t streamId) const {
    return Rng(splitMix64(mSeed ^ splitMix64(streamId + STREAM_DOMAIN)), SeedTag{});
}

} // namespace Simulation::Economy
