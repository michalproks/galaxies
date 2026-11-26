#include "Client.h"

#include "GameWindow.h"
#include "Selection.h"
#include "VulkanRenderer.h"

#include <Server.h>

#include <chrono>
#include <iterator>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace Client {
namespace {

    template <typename History, typename Record>
    void trimHistory(History &history, std::size_t const capacity) {
        std::size_t const excess = history.size() - capacity;
        if constexpr (std::is_same_v<History, std::vector<Record>>) {
            history.erase(history.begin(), history.begin() + static_cast<std::ptrdiff_t>(excess));
        } else {
            for (std::size_t index = 0; index < excess; ++index) {
                history.pop_front();
            }
        }
    }

    template <typename Record, typename History, typename Pending, typename Records>
    bool mergeRecords(History &history,
                      Pending &pending,
                      Records const &records,
                      Simulation::Economy::ColonyId colonyId,
                      std::size_t const capacity) {
        for (Record const &record : records) {
            if (record.colonyId == colonyId && (history.empty() || record.turnNumber > history.back().turnNumber)) {
                pending.try_emplace(record.turnNumber, record);
            }
        }

        bool appended{};
        while (!pending.empty()) {
            if (!history.empty() && history.back().turnNumber == std::numeric_limits<std::int64_t>::max()) {
                break;
            }
            std::int64_t const expectedTurn = history.empty() ? pending.begin()->first : history.back().turnNumber + 1;
            auto const next = pending.find(expectedTurn);
            if (next == pending.end()) {
                break;
            }
            history.push_back(std::move(next->second));
            pending.erase(next);
            appended = true;
        }
        if (history.size() > capacity) {
            trimHistory<History, Record>(history, capacity);
        }
        while (pending.size() > capacity) {
            pending.erase(std::prev(pending.end()));
        }
        return appended;
    }

} // namespace

void Client::receiveGameState(Server::GameSnapshot const &snapshot) {
    std::vector<Simulation::Economy::ColonyId> updatedColonies;
    if (snapshot.economyRecords != nullptr) {
        for (Simulation::Economy::ColonyEconomyRecordSnapshot const &snapshotRecords : *snapshot.economyRecords) {
            ColonyEconomyHistory &history = mEconomyHistories[snapshotRecords.colonyId];
            PendingHistory &pending = mPendingHistories[snapshotRecords.colonyId];
            bool const playerUpdated = mergeRecords<Simulation::Economy::PlayerRecord>(history.playerRecords,
                                                                                       pending.playerRecords,
                                                                                       snapshotRecords.playerHistory,
                                                                                       snapshotRecords.colonyId,
                                                                                       PLAYER_ECONOMY_HISTORY_CAPACITY);
            bool const debugUpdated = mergeRecords<Simulation::Economy::DebugRecord>(history.debugRecords,
                                                                                     pending.debugRecords,
                                                                                     snapshotRecords.debugRecords,
                                                                                     snapshotRecords.colonyId,
                                                                                     DEBUG_ECONOMY_HISTORY_CAPACITY);
            if (playerUpdated || debugUpdated) {
                updatedColonies.push_back(snapshotRecords.colonyId);
            }
        }
    }

    for (Simulation::Economy::ColonyId const colonyId : updatedColonies) {
        if (sigEconomyUpdated) {
            sigEconomyUpdated(colonyId);
        }
    }
    if (sigUpdatedGame) {
        sigUpdatedGame();
    }
}

ColonyEconomyHistory const &Client::economyHistory(Simulation::Economy::ColonyId const colonyId) const {
    static ColonyEconomyHistory const EMPTY_HISTORY;
    auto const found = mEconomyHistories.find(colonyId);
    return found == mEconomyHistories.end() ? EMPTY_HISTORY : found->second;
}

int runGame() {
    GameWindow window;
    VulkanRenderer renderer(window);
    Server::Server server;
    Client clientState;
    std::optional<std::size_t> selectedBody;
    auto previousFrameTime = std::chrono::steady_clock::now();
    Simulation::GameSpeed lastNonPausedSpeed{Simulation::GameSpeed::NORMAL};

    for (;;) {
        auto const now = std::chrono::steady_clock::now();
        double const frameSeconds = std::chrono::duration<double>(now - previousFrameTime).count();
        previousFrameTime = now;
        if (!window.processEvents(frameSeconds)) {
            break;
        }
        std::vector<Simulation::Vector2> const clicks = window.consumeLeftClicks();
        window.updateCamera();
        if (std::optional<Simulation::GameSpeed> const speed = window.consumeSpeedChange(); speed.has_value()) {
            server.setSpeed(speed.value());
            lastNonPausedSpeed = speed.value();
        }
        if (window.consumePauseToggle()) {
            if (server.speed() == Simulation::GameSpeed::PAUSED) {
                server.setSpeed(lastNonPausedSpeed);
            } else {
                lastNonPausedSpeed = server.speed();
                server.setSpeed(Simulation::GameSpeed::PAUSED);
            }
        }
        Server::GameSnapshot const snapshot = server.snapshot(now);
        clientState.receiveGameState(snapshot);
        for (Simulation::Vector2 const click : clicks) {
            bool const capture = renderer.wantsMouseCapture();
            if (!capture) {
                selectedBody = hitTest(snapshot, window.camera(), renderer.viewport(), click);
            }
        }
        renderer.renderFrame(window.camera(), snapshot, renderer.viewport(), selectedBody);
        if (std::optional<Simulation::Economy::ColonyId> const colony = renderer.requestedEconomyColony();
            colony.has_value() && colony->value == 0) {
            for (std::size_t bodyIndex = 0; bodyIndex < snapshot.bodies.size(); ++bodyIndex) {
                if (snapshot.bodies[bodyIndex].name == "Earth") {
                    selectedBody = bodyIndex;
                    break;
                }
            }
        }
    }
    return 0;
}

int runSmokeTest() {
    GameWindow window;
    VulkanRenderer renderer(window);
    Server::Server server;
    Client clientState;
    Server::GameSnapshot const snapshot = server.snapshot();
    clientState.receiveGameState(snapshot);
    renderer.renderFrame(window.camera(), snapshot, renderer.viewport(), 0);
    return 0;
}

} // namespace Client
