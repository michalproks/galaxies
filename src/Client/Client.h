#ifndef GALAXIES_CLIENT_CLIENT_H
#define GALAXIES_CLIENT_CLIENT_H

#include <Server.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <vector>

namespace Client {

/** Maximum client-side history retained for each colony economy stream. */
inline constexpr std::size_t PLAYER_ECONOMY_HISTORY_CAPACITY = 2000;
inline constexpr std::size_t DEBUG_ECONOMY_HISTORY_CAPACITY = 200;

/** The two independently retained economy streams exposed to UI consumers. */
struct ColonyEconomyHistory {
    std::vector<Simulation::Economy::PlayerRecord> playerRecords;
    std::deque<Simulation::Economy::DebugRecord> debugRecords;
};

/** Client-side snapshot receiver and rolling economy-history cache. */
class Client {
public:
    /** Receives a game-state snapshot, merges its economy records, then emits update callbacks. */
    void receiveGameState(Server::GameSnapshot const &snapshot);

    /** Returns the retained economy records for a colony, or an empty history when unknown. */
    [[nodiscard]] ColonyEconomyHistory const &economyHistory(Simulation::Economy::ColonyId colonyId) const;

    /** Called after any game-state snapshot has been received. */
    std::function<void()> sigUpdatedGame;

    /** Called once per colony whose player or debug history gained a contiguous record. */
    std::function<void(Simulation::Economy::ColonyId)> sigEconomyUpdated;

private:
    struct PendingHistory {
        std::map<std::int64_t, Simulation::Economy::PlayerRecord> playerRecords;
        std::map<std::int64_t, Simulation::Economy::DebugRecord> debugRecords;
    };

    std::map<Simulation::Economy::ColonyId, ColonyEconomyHistory> mEconomyHistories;
    std::map<Simulation::Economy::ColonyId, PendingHistory> mPendingHistories;
};

/** Runs the SDL event loop, updates the client camera, and renders server snapshots until the window closes. */
int runGame();

/** Initializes SDL and Vulkan, presents one planetary-system frame, and exits for automated smoke testing. */
int runSmokeTest();

} // namespace Client

#endif // GALAXIES_CLIENT_CLIENT_H
