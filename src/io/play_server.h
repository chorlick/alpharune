#pragma once
/// @file play_server.h
/// Boost.Beast HTTP + WebSocket server for the human-play web UI.
///
/// The server has two responsibilities:
///   1. Serve the embedded single-page HTML at GET /.
///   2. Accept WebSocket upgrades at GET /ws, then exchange JSON
///      messages with the browser to surface engine decisions and
///      accept human input.
///
/// One PlayServer instance per game. Constructed and started by
/// `main_play.cpp` before the engine thread begins runGame(). The
/// server owns its own asio io_context + worker thread; the engine
/// runs in a separate thread and blocks at decision points via the
/// HumanAgent it was constructed with.
///
/// Synchronisation: a single std::mutex (`state_mutex`) is held by
/// the WS handler when mutating GameState via StateEditor. The engine
/// does not hold this lock — it only mutates GameState from its own
/// thread, and is parked in HumanAgent::selectAction whenever a WS
/// edit might fire. The contract is structural: edits arrive ONLY
/// while a human seat is waiting, which is exactly when the engine
/// has stopped touching GameState.
///
/// Wire protocol — see docs/play-api.md.

#include "core/types.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace riftbound {

class GameEngine;
class HumanAgent;
class StateRenderer;
class CardDB;
class StateEditor;
struct GameState;

struct PlayServerConfig {
    /// TCP port. Defaults to 8080.
    uint16_t port = 8080;

    /// Bind address. Defaults to loopback for local-dev safety.
    std::string bind_address = "127.0.0.1";

    /// When true, accept `edit_*` messages and dispatch to StateEditor.
    /// When false, edit messages are rejected with an `edit_err` reply.
    /// `main_play.cpp` enables this only when at least one seat is human.
    bool god_mode_enabled = false;

    /// Surfaced in the served HTML banner so the user can tie their
    /// session back to a specific build + RNG draw.
    std::string version_tag;
    std::string build_date;
    uint64_t    seed = 0;

    /// Slots for the two seats. Null = AI seat (no human input expected).
    /// At least one MUST be non-null for the server to be useful.
    HumanAgent* p1_human = nullptr;
    HumanAgent* p2_human = nullptr;

    /// Fired on the first SIGINT / SIGTERM received after `start()`.
    /// Runs on the server's IO thread (not in signal-handler context),
    /// so it may take locks, allocate, etc. `main_play.cpp` uses this
    /// to call `HumanAgent::abort()` on each human seat, which wakes
    /// the engine thread out of `selectAction` so the game terminates
    /// cleanly. A second signal arriving while the callback is still
    /// processing will be observed by std::signal as well so the user
    /// can force-quit via `_exit` if shutdown wedges.
    std::function<void()> on_shutdown_signal;
};

class PlayServer {
public:
    PlayServer(GameEngine& engine,
               StateRenderer& renderer,
               const CardDB& card_db,
               PlayServerConfig config);
    ~PlayServer();

    PlayServer(const PlayServer&) = delete;
    PlayServer& operator=(const PlayServer&) = delete;

    /// Begin listening + spawn the IO worker thread. Non-blocking.
    /// Returns the URL the user should open in their browser.
    std::string start();

    /// Signal shutdown + join the IO worker thread. Idempotent.
    void stop();

    /// Push a single log line to every connected WebSocket client.
    /// Safe to call from any thread (engine, game runner, etc.).
    /// `level` is the engine LogLevel as a short tag — "TRC", "DBG",
    /// "INF", "WRN". The UI's log panel filters and styles by level.
    void broadcastLog(const std::string& level, const std::string& message);

    /// Lock held while StateEditor mutates GameState. Exposed for
    /// callers that need to read state coherently with edits (e.g.,
    /// the snapshot builder uses this too).
    std::mutex& stateMutex() { return state_mutex_; }

    // ── Accessors used by the internal session classes. ──
    // Defined in play_server.cpp to keep Beast/asio out of this header.
    HumanAgent* config_p1() const noexcept;
    HumanAgent* config_p2() const noexcept;
    bool        godModeEnabled() const noexcept;
    GameState&  engineMutableState() noexcept;

    // Forward-declared here so play_server.cpp can define the pimpl. The
    // body lives entirely inside the .cpp; external callers cannot
    // construct or inspect Impl. Exposed (rather than nested-private)
    // so the in-.cpp session classes — declared in an anonymous
    // namespace — can hold an Impl& without name-lookup friction.
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;

    GameEngine& engine_;
    StateRenderer& renderer_;
    const CardDB& card_db_;
    PlayServerConfig config_;
    std::mutex state_mutex_;
    std::atomic<bool> running_{false};
};

} // namespace riftbound
