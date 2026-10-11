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
/// `main.cpp` before the engine thread begins runGame(). The server owns
/// its own asio io_context + worker thread; the engine runs in a separate
/// thread and blocks at decision points via the HumanAgent it was
/// constructed with.
///
/// Synchronisation. The engine thread mutates GameState without taking
/// any lock, so a server thread may touch GameState only while the engine
/// is *parked* — and must keep it parked until it is done:
///   - at a human decision: the engine waits in HumanAgent::selectAction
///     and is woken only by a `choose`, which is applied under
///     `state_mutex_`;
///   - at the spectator gate: the engine leaves awaitSpectatorStep only
///     after taking `state_mutex_`;
///   - after the game: notifyGameFinished() says the engine thread is done.
/// Every snapshot read and every god-mode edit holds `state_mutex_` and
/// first checks one of these holds. While the engine is running, clients
/// are sent the last snapshot taken (marked `engine_busy`) and edits are
/// refused. The engine thread itself may publish a fresh snapshot at any
/// time (publishEngineState), also under `state_mutex_`.
///
/// Wire protocol — see docs/play-api.md.

#include "core/events.h"
#include "core/types.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace riftbound {

class GameEngine;
class HumanAgent;
class StateRenderer;
class CardDB;
class StateEditor;
struct GameState;
struct Intent;

struct PlayServerConfig {
    /// TCP port. Defaults to 8080.
    uint16_t port = 8080;

    /// Bind address. Defaults to loopback for local-dev safety.
    std::string bind_address = "127.0.0.1";

    /// When true, accept `edit_*` messages and dispatch to StateEditor.
    /// When false, edit messages are rejected with an `edit_err` reply.
    /// `main.cpp` enables this by default when at least one seat is human.
    bool god_mode_enabled = false;

    /// When true, frames carry every hand and both decks in order. When
    /// false a frame shows only what a seated human may see: their own
    /// hand (in hot-seat play, the hand of whoever is deciding), sizes for
    /// everything else, and `broadcastLog` sends only the log lines every
    /// human seat may read (LogEvent::audience). `main.cpp` sets it when god mode is on (the editor
    /// lists every zone) or no seat is human.
    bool reveal_hidden = false;

    /// When non-empty, `GET /` and the WebSocket upgrade must carry
    /// `?token=<access_token>` or are refused. `main.cpp` generates one
    /// when binding to a non-loopback address.
    std::string access_token;

    /// Surfaced in the served HTML banner so the user can tie their
    /// session back to a specific build + RNG draw.
    std::string version_tag;
    std::string build_date;
    uint64_t    seed = 0;

    /// Slots for the two seats. Null = AI seat (no human input expected).
    /// At least one MUST be non-null for the server to be useful.
    HumanAgent* p1_human = nullptr;
    HumanAgent* p2_human = nullptr;

    /// Spectator mode (no human seat): the engine thread parks in
    /// `PlayServer::awaitSpectatorStep` after every agent decision until
    /// the browser steps (or autoplay's delay elapses).
    bool spectator = false;

    /// Fired by `requestShutdown()` — that is, on the first SIGINT /
    /// SIGTERM received after `start()`.
    /// Runs on the server's IO thread (not in signal-handler context),
    /// so it may take locks, allocate, etc. `main.cpp` uses this
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
    /// Returns the URL the user should open in their browser. Throws
    /// (leaving the server stopped) if the address cannot be bound.
    std::string start();

    /// The TCP port actually bound (differs from the configured one when
    /// that was 0). Valid after start().
    uint16_t port() const noexcept;

    /// Signal shutdown + join the IO worker thread. Idempotent.
    void stop();

    /// Stop holding the game: fires `on_shutdown_signal` (which is expected
    /// to abort the human seats) and opens the spectator gate for good, so
    /// the engine runs to the end of the game on its own. What the first
    /// SIGINT / SIGTERM does. The server keeps serving until stop().
    void requestShutdown();

    /// Push a single log line to every connected WebSocket client — unless
    /// `PlayServerConfig::reveal_hidden` is off, in which case the line is
    /// not sent (it may name a card the viewer must not see).
    /// Safe to call from any thread (engine, game runner, etc.).
    /// `level` is the engine LogLevel as a short tag — "TRC", "DBG",
    /// "INF", "WRN". The UI's log panel filters and styles by level.
    /// `audience` is the line's LogEvent::audience. With `reveal_hidden` off
    /// the line is sent only if every human seat may read it (clients are not
    /// told apart by seat: in hot-seat play both humans watch one screen).
    void broadcastLog(const std::string& level, const std::string& message,
                      LogAudience audience);
    /// As above for a line with no audience tag: withheld unless
    /// `reveal_hidden` is on.
    void broadcastLog(const std::string& level, const std::string& message);

    /// Spectator gate. Called on the engine thread from the engine's
    /// `on_decision` callback: publishes the decision the agent just made
    /// (state is still pre-resolution) and blocks until a `spectator_step`
    /// message arrives, autoplay's delay elapses, or the server shuts
    /// down. No-op unless `PlayServerConfig::spectator` is set.
    void awaitSpectatorStep(const std::vector<Intent>& legal, const Intent& chosen);

    /// Call on the engine thread, whenever it is about to be busy for a
    /// while without reaching a human decision (an AI seat starting to
    /// think): snapshots the board as it is now and sends it to every
    /// client, so the UI is not left showing the position from before the
    /// last click.
    void publishEngineState();

    /// Call on the engine thread after every agent decision (from the
    /// engine's `on_decision`). A `*_PROMPT` line belongs to the decision
    /// it preceded; this retires it so it cannot head a later one.
    void notifyDecisionMade();

    /// Call once the engine thread has finished the game: from then on
    /// GameState is stable. Sends the terminal frame.
    void notifyGameFinished();

    /// Lock held while a server thread reads or edits GameState (see the
    /// file comment for when that is allowed).
    std::mutex& stateMutex() { return state_mutex_; }

    // ── Accessors used by the internal session classes. ──
    // Defined in play_server.cpp to keep Beast/asio out of this header.
    HumanAgent* config_p1() const noexcept;
    HumanAgent* config_p2() const noexcept;
    bool        godModeEnabled() const noexcept;
    GameState&  engineMutableState() noexcept;
    const PlayServerConfig& config() const noexcept { return config_; }

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
