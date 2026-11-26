#ifndef GALAXIES_CLIENT_DEBUG_OVERLAY_H
#define GALAXIES_CLIENT_DEBUG_OVERLAY_H

#include <Economy/EconomyRecords.h>
#include <Server.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Client {

class Client;

enum class InvariantState : std::uint8_t { PASS, FAIL, UNAVAILABLE };

/** A record-derived invariant result for the scrubbed colony turn. */
struct InvariantBadge {
    std::string name;
    InvariantState state{InvariantState::UNAVAILABLE};
    std::string detail;
};

/** One changed field in a record-level comparison, grouped by the tick pipeline stage. */
struct DebugRecordChange {
    std::string stage;
    std::string field;
    std::string beforeValue;
    std::string afterValue;
};

/** Inclusive turn window retained for the selected colony. */
struct DebugTurnRange {
    std::int64_t firstTurn{};
    std::int64_t lastTurn{};
};

/** Clamps the requested turn to an inclusive history range, returning current for an invalid range. */
[[nodiscard]] std::int64_t clampDebugTurn(std::int64_t current, DebugTurnRange range) noexcept;

/** Moves the debug cursor by one or ten turns and clamps at either end of the rolling history. */
[[nodiscard]] std::int64_t stepDebugTurn(std::int64_t current, DebugTurnRange range, int direction, bool fast) noexcept;

/** Record history and exact turn requested from the rolling window. */
struct DebugRecordQuery {
    std::deque<Simulation::Economy::DebugRecord> const &records;
    std::int64_t turn{};
};

/** Returns the retained debug snapshot for an exact turn, or nullptr when it is unavailable. */
[[nodiscard]] Simulation::Economy::DebugRecord const *debugRecordAtTurn(DebugRecordQuery query) noexcept;

/** Derives the invariant strip from fields retained in one diagnostic record. */
[[nodiscard]] std::vector<InvariantBadge> deriveInvariantBadges(Simulation::Economy::DebugRecord const &record);

/** Two snapshots to compare, ordered from selected/older to comparison/newer. */
struct DebugRecordPair {
    Simulation::Economy::DebugRecord const &selected;
    Simulation::Economy::DebugRecord const &comparison;
};

/** Produces a deterministic field-level diff grouped by economy pipeline stage. */
[[nodiscard]] std::vector<DebugRecordChange> diffDebugRecords(DebugRecordPair records);

/** Stateless renderer for the developer-only economy inspectors and turn scrubber. */
class DebugOverlay {
public:
    /** Draws the overlay over an active ImGui frame; F9 toggles visibility without pausing simulation. */
    void draw(Client const &client, Server::GameSnapshot const &snapshot, std::optional<std::size_t> selectedBody);

private:
    std::optional<Simulation::Economy::ColonyId> mSelectedColony;
    std::optional<std::int64_t> mScrubTurn;
};

} // namespace Client

#endif // GALAXIES_CLIENT_DEBUG_OVERLAY_H
