#include "play_server.h"
#include "play_index_html.h"
#include "replay_writer.h"
#include "state_editor.h"
#include "state_renderer.h"

#include "agents/human_agent.h"
#include "core/card_db.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "engine/game_engine.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/post.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <deque>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <set>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = beast::websocket;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;
using json      = nlohmann::json;

namespace riftbound {

// ─── Forward decls ─────────────────────────────────────────────────────────
namespace {
class WsSession;

std::string playerToStr(PlayerId p) {
    switch (p) {
        case PlayerId::Player1: return "P1";
        case PlayerId::Player2: return "P2";
        default: return "none";
    }
}

PlayerId strToPlayer(const std::string& s) {
    if (s == "P1") return PlayerId::Player1;
    if (s == "P2") return PlayerId::Player2;
    return PlayerId::None;
}

std::optional<ZoneType> strToZone(const std::string& s) {
    if (s == "MainDeck")        return ZoneType::MainDeck;
    if (s == "RuneDeck")        return ZoneType::RuneDeck;
    if (s == "Hand")            return ZoneType::Hand;
    if (s == "Base")            return ZoneType::Base;
    if (s == "Trash")           return ZoneType::Trash;
    if (s == "Banishment")      return ZoneType::Banishment;
    if (s == "ChampionZone")    return ZoneType::ChampionZone;
    if (s == "LegendZone")      return ZoneType::LegendZone;
    if (s == "BattlefieldZone") return ZoneType::BattlefieldZone;
    if (s == "Chain")           return ZoneType::Chain;
    if (s == "FacedownZone")    return ZoneType::FacedownZone;
    return std::nullopt;
}

// ── Message field readers ──
// Client JSON is untrusted: a field may be missing or of any type. These
// never throw (nlohmann's `value()` / `get<T>()` do on a type mismatch).

std::string strField(const json& m, const char* key) {
    auto it = m.find(key);
    return (it != m.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

std::optional<int64_t> intField(const json& m, const char* key) {
    auto it = m.find(key);
    if (it == m.end() || !it->is_number_integer()) return std::nullopt;
    if (it->is_number_unsigned() &&
        it->get<uint64_t>() > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        return std::nullopt;
    }
    return it->get<int64_t>();
}

/// An int field clamped into `int` range; `fallback` when absent / not an integer.
int intFieldOr(const json& m, const char* key, int fallback) {
    auto v = intField(m, key);
    if (!v) return fallback;
    return static_cast<int>(std::clamp<int64_t>(*v, std::numeric_limits<int>::min(),
                                                std::numeric_limits<int>::max()));
}

bool boolFieldOr(const json& m, const char* key, bool fallback) {
    auto it = m.find(key);
    return (it != m.end() && it->is_boolean()) ? it->get<bool>() : fallback;
}

/// An object / battlefield id; kInvalidId when absent or not a valid id.
uint32_t idField(const json& m, const char* key) {
    auto v = intField(m, key);
    if (!v || *v < 0 || *v > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
        return static_cast<uint32_t>(kInvalidId.raw());
    }
    return static_cast<uint32_t>(*v);
}

// ── HTTP request checks ──

/// The Host header must name this machine by IP literal or as "localhost".
/// A page on another site can reach a local server under its own DNS name
/// (DNS rebinding); it cannot make the browser send a Host it does not own.
bool hostHeaderAllowed(std::string host) {
    if (host.empty()) return false;
    if (host.front() == '[') {                 // [v6]:port
        auto end = host.find(']');
        if (end == std::string::npos) return false;
        host = host.substr(1, end - 1);
    } else if (auto colon = host.rfind(':'); colon != std::string::npos) {
        host = host.substr(0, colon);
    }
    if (beast::iequals(host, "localhost")) return true;
    beast::error_code ec;
    net::ip::make_address(host, ec);
    return !ec;
}

/// Browsers send Origin on every WebSocket handshake; it must be the page
/// this server served. Without the check any site open in the user's
/// browser could connect to ws://127.0.0.1 and drive the game. Clients
/// that send no Origin (websocat, scripts) are not browsers and pass.
bool originAllowed(const http::request<http::string_body>& req) {
    auto origin = req[http::field::origin];
    if (origin.empty()) return true;
    const std::string expected = "http://" + std::string(req[http::field::host]);
    return beast::iequals(origin, expected);
}

/// True if `query` ("a=1&token=x") carries `token=<expected>`.
bool queryHasToken(const std::string& query, const std::string& expected) {
    size_t i = 0;
    while (i <= query.size()) {
        auto amp = query.find('&', i);
        if (amp == std::string::npos) amp = query.size();
        const std::string pair = query.substr(i, amp - i);
        if (pair.rfind("token=", 0) == 0 && pair.substr(6) == expected) return true;
        i = amp + 1;
    }
    return false;
}

} // namespace

// ─── Impl ──────────────────────────────────────────────────────────────────

struct PlayServer::Impl {
    PlayServer& parent;
    net::io_context ioc;
    std::optional<tcp::acceptor> acceptor;
    std::optional<net::signal_set> signals;
    std::thread io_thread;
    std::thread watcher_thread;

    // Precomputed HTML index page, built once at server start so the
    // CARD_IMAGES JSON / version banner are baked in.
    std::string index_html;

    // Active WS sessions — used to broadcast decision frames.
    std::mutex sessions_mu;
    std::list<std::weak_ptr<WsSession>> sessions;

    std::atomic<bool> stopping{false};

    // Frames posted to sessions and not yet written; stop() waits for it
    // to drain so the terminal frame is not lost.
    std::atomic<int> pending_writes{0};

    // ── Guarded by PlayServer::state_mutex_ ──
    // Decision ids already answered by a `choose`. A HumanAgent still
    // reports atDecision() for a moment after it is answered (until the
    // engine thread wakes); from the answer on, the engine may be running.
    uint64_t answered_p1 = 0;
    uint64_t answered_p2 = 0;
    // Shutdown signal received: human seats are aborted, so the engine no
    // longer waits for the server and is never again considered parked.
    bool engine_released = false;
    // The engine thread has finished the game (notifyGameFinished).
    bool engine_finished = false;
    // What the last broadcast frame was about ("H1:7", "S:12", "busy",
    // "finished"); the watcher broadcasts when this changes.
    std::string last_key;
    // The board part of the most recent frame built from GameState. Sent
    // again, marked engine_busy, while GameState may not be read. One per
    // audience (see Audience): only [Shared] is used unless seat-bound.
    json last_snapshot[4];

    StateEditor editor;

    // Most recent prompt-style trace line, captured by broadcastLog as
    // it streams. Cards / engine emit `<X>_PROMPT: <text>` right before
    // requesting an agent choice (MAY_PROMPT, X_PROMPT, MODE_PROMPT,
    // TGT_PROMPT), so it belongs to the next decision anyone makes. It is
    // attached to every frame of that decision and dropped once the
    // decision is made, so the UI can show "what are we choosing?"
    // alongside the otherwise opaque MakeChoice options.
    std::mutex prompt_mu;
    std::string last_prompt;

    // Spectator gate (see PlayServer::awaitSpectatorStep). The engine
    // thread parks on spec_cv with spec_waiting set; the IO thread wakes
    // it via spectatorStep / setSpectatorAutoplay.
    std::mutex spec_mu;
    std::condition_variable spec_cv;
    std::vector<Intent> spec_legal;
    int spec_chosen = -1;
    PlayerId spec_player = PlayerId::None;
    uint64_t spec_decision_id = 0;
    bool spec_waiting = false;
    bool spec_step_requested = false;
    bool spec_autoplay = false;
    int spec_delay_ms = 600;
    bool spec_released = false;   // shutdown: never block again

    explicit Impl(PlayServer& p) : parent(p) {}

    void spectatorStep();
    void setSpectatorAutoplay(bool on, int delay_ms);
    void releaseSpectator();

    /// After a god-mode edit, with the engine parked on `agent`'s decision:
    /// replace the legal actions the agent holds with what the edited state
    /// offers. Returns false if they were left as they were (see the
    /// definition for when). Caller holds the state mutex.
    bool recomputeLegalForParkedHuman(HumanAgent& agent);

    /// Where the engine thread is, as far as a server thread may rely on.
    struct Park {
        enum class Kind { Busy, Human, Spectator, Finished };
        Kind kind = Kind::Busy;
        PlayerId player = PlayerId::None;   // who is deciding
        uint64_t decision_id = 0;
        HumanAgent* agent = nullptr;        // Kind::Human only
    };

    // The *Locked members require PlayServer::state_mutex_.
    Park parkedLocked();
    static std::string keyOf(const Park& park);
    RenderVisibility visibilityFor(const Park& park) const;
    json buildSnapshotLocked(const Park& park, const RenderVisibility& vis);

    /// Who a frame or a log line is for. Shared: every client gets the same
    /// (the server is not seat-bound). Otherwise one per kind of client.
    enum Audience { Shared = 0, SeatP1 = 1, SeatP2 = 2, Spectators = 3 };
    static Audience audienceOf(PlayerId seat) {
        return seat == PlayerId::Player1 ? SeatP1
             : seat == PlayerId::Player2 ? SeatP2 : Spectators;
    }
    bool seatBound() const { return parent.config_.seat_bound; }
    /// Frames differ per seat only while hidden information is withheld.
    bool perSeatViews() const { return seatBound() && !parent.config_.reveal_hidden; }
    json buildFrameLocked(const Park& park, bool can_read, Audience audience);
    bool hasClients(Audience audience);
    /// True if a connected client other than `asking` holds `seat`.
    bool seatTaken(PlayerId seat, const WsSession* asking);
    void sendTo(Audience audience, const std::string& text);
    void broadcastLocked(bool on_engine_thread);
    void broadcast(bool on_engine_thread = false);
    void clearPrompt();
    void sendAll(const std::string& text);

    void runWatcher();
    void armSignalSet();
};

// ─── WS Session ────────────────────────────────────────────────────────────
namespace {

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
    WsSession(tcp::socket socket, PlayServer::Impl& server_impl, PlayServer& server)
        : ws_(std::move(socket)), impl_(server_impl), server_(server) {}

    void run(http::request<http::string_body> req) {
        ws_.set_option(ws::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(ws::stream_base::decorator(
            [](ws::response_type& res) {
                res.set(http::field::server, "riftbound_play");
            }));
        auto self = shared_from_this();
        ws_.async_accept(req,
            [self](beast::error_code ec) {
                if (ec) return;
                self->onAccept();
            });
    }

    /// The seat this client holds (PlayerId::None: none — a spectator when
    /// the server is seat-bound). Read from the engine and watcher threads.
    PlayerId seat() const { return static_cast<PlayerId>(seat_.load()); }

    /// Thread-safe send. Posts onto the ws's executor so that the
    /// write queue and async_write call run on the IO thread.
    void send(std::string text) {
        auto self = shared_from_this();
        impl_.pending_writes.fetch_add(1);
        net::post(ws_.get_executor(),
            [self, t = std::move(text)]() mutable {
                self->queue_.push_back(std::move(t));
                if (!self->writing_) self->doWrite();
            });
    }

private:
    void onAccept() {
        // Only now may frames be written to this socket: a log line or frame
        // broadcast while the upgrade response was still being sent would
        // be written into the middle of the handshake.
        {
            std::lock_guard<std::mutex> lock(impl_.sessions_mu);
            impl_.sessions.push_back(weak_from_this());
        }
        // Push the current frame so a client that connects mid-game has a
        // board to show straight away.
        impl_.broadcast();
        doRead();
    }

    void doRead() {
        auto self = shared_from_this();
        ws_.async_read(read_buf_,
            [self](beast::error_code ec, std::size_t /*n*/) {
                if (ec == ws::error::closed || ec == net::error::operation_aborted) return;
                if (ec) return;
                // Nothing a client sends may take the IO thread down: it
                // also serves every other client and the signal handler.
                try {
                    self->onMessage();
                } catch (const std::exception& e) {
                    self->sendErr(std::string("bad message: ") + e.what());
                }
                self->read_buf_.consume(self->read_buf_.size());
                self->doRead();
            });
    }

    void sendErr(const std::string& msg) {
        send(json{{"type", "err"}, {"msg", msg}}.dump());
    }

    void onMessage() {
        std::string msg(beast::buffers_to_string(read_buf_.data()));
        json m;
        try { m = json::parse(msg); }
        catch (const std::exception& e) {
            sendErr(std::string("bad json: ") + e.what());
            return;
        }
        if (!m.is_object()) {
            sendErr("message must be a JSON object");
            return;
        }
        auto type = strField(m, "type");
        if      (type == "choose")             handleChoose(m);
        else if (type == "seat")               handleSeat(m);
        else if (type == "edit_move")          handleEditMove(m);
        else if (type == "edit_object")        handleEditObject(m);
        else if (type == "edit_player")        handleEditPlayer(m);
        else if (type == "edit_phase")         handleEditPhase(m);
        else if (type == "edit_reorder_deck")  handleEditReorderDeck(m);
        else if (type == "request_state")      impl_.broadcast();
        else if (type == "spectator_step")     impl_.spectatorStep();
        else if (type == "spectator_autoplay") {
            impl_.setSpectatorAutoplay(boolFieldOr(m, "on", false),
                                       intFieldOr(m, "delay_ms", 600));
            impl_.broadcast();
        }
        else {
            sendErr("unknown message type: " + type);
        }
    }

    /// `{"type":"seat","player":"P1"|"P2"|""}`: take a seat (or give it up
    /// with ""). Only on a seat-bound server; only a human seat; only a seat
    /// no other connected client holds. The seat is released when this
    /// client disconnects.
    void handleSeat(const json& m) {
        const std::string player_str = strField(m, "player");
        auto refuse = [&](const std::string& why) {
            send(json{{"type", "seat"}, {"ok", false}, {"player", playerToStr(seat())},
                      {"msg", why}}.dump());
        };
        if (!impl_.seatBound()) {
            refuse("this server is not seat-bound: every client plays the human seat");
            return;
        }
        PlayerId want = PlayerId::None;
        if (!player_str.empty()) {
            want = strToPlayer(player_str);
            HumanAgent* agent = want == PlayerId::Player1 ? server_.config_p1()
                              : want == PlayerId::Player2 ? server_.config_p2() : nullptr;
            if (!agent) { refuse("'" + player_str + "' is not a human seat"); return; }
            if (impl_.seatTaken(want, this)) {
                refuse("seat " + player_str + " is taken by another client");
                return;
            }
        }
        seat_.store(static_cast<uint8_t>(want));
        send(json{{"type", "seat"}, {"ok", true}, {"player", playerToStr(want)}}.dump());
        // Its view changed: send it (and everyone) the current frame.
        impl_.broadcast();
    }

    /// A `choose` answers exactly the decision it names. Anything else —
    /// a click that arrives after the engine moved on, a second click, an
    /// index that is not on offer — is refused, never guessed at.
    void handleChoose(const json& m) {
        auto player_str = strField(m, "player");
        PlayerId pid = strToPlayer(player_str);
        HumanAgent* agent = nullptr;
        if (pid == PlayerId::Player1) agent = server_.config_p1();
        if (pid == PlayerId::Player2) agent = server_.config_p2();
        if (!agent) {
            sendErr("no human agent for '" + player_str + "'");
            return;
        }
        // Seat-bound: only the client seated as that player answers for it.
        if (impl_.seatBound() && seat() != pid) {
            sendErr(seat() == PlayerId::None
                ? "a spectator cannot answer for " + player_str + " (take the seat first)"
                : "this client is seated as " + playerToStr(seat()) +
                  " and cannot answer for " + player_str);
            return;
        }
        auto idx = intField(m, "index");
        auto decision_id = intField(m, "decision_id");

        std::lock_guard<std::mutex> lock(server_.stateMutex());
        auto park = impl_.parkedLocked();
        std::string refusal;
        if (park.kind != PlayServer::Impl::Park::Kind::Human || park.agent != agent) {
            refusal = "no decision is pending for " + player_str;
        } else if (!decision_id) {
            refusal = "choose needs the decision_id of the decision it answers";
        } else if (static_cast<uint64_t>(*decision_id) != park.decision_id) {
            refusal = "stale choose: decision " + std::to_string(*decision_id) +
                      " is no longer pending (now " + std::to_string(park.decision_id) + ")";
        } else if (!idx || *idx < 0 ||
                   static_cast<size_t>(*idx) >= agent->pendingLegal().size()) {
            refusal = "index is not one of the legal actions";
        }
        if (!refusal.empty()) {
            sendErr(refusal);
            // The client disabled its buttons when it sent the click; give
            // it the real current frame back.
            impl_.broadcastLocked(false);
            return;
        }
        // The agent makes the same checks atomically with accepting the
        // answer; it has the last word.
        if (!agent->setChosenAction(static_cast<int>(*idx), park.decision_id)) {
            sendErr("stale choose: the decision is no longer pending");
            impl_.broadcastLocked(false);
            return;
        }
        (pid == PlayerId::Player1 ? impl_.answered_p1 : impl_.answered_p2) = park.decision_id;
        impl_.clearPrompt();
        // The engine is running from here on: tell clients right away so
        // the answered decision's buttons go.
        impl_.broadcastLocked(false);
    }

    bool requireGodMode() {
        if (!server_.godModeEnabled()) {
            send(json{{"type", "edit_err"}, {"msg", "god mode disabled"}}.dump());
            return false;
        }
        return true;
    }

    /// Run one god-mode edit. `fn(GameState&)` is called only while the
    /// engine thread is parked (see play_server.h); otherwise the edit is
    /// refused — the engine would be mutating the same state.
    template <class Fn>
    void applyEdit(Fn&& fn) {
        if (!requireGodMode()) return;
        std::lock_guard<std::mutex> lock(server_.stateMutex());
        EditResult res;
        const auto park = impl_.parkedLocked();
        if (park.kind == PlayServer::Impl::Park::Kind::Busy) {
            res = EditResult::fail("engine is busy; edits apply only while it waits at a decision");
        } else {
            res = fn(server_.engineMutableState());
        }
        // The decision being waited on was computed from the state as it was
        // before the edit.
        bool recomputed = false;
        if (res.ok && park.kind == PlayServer::Impl::Park::Kind::Human) {
            recomputed = impl_.recomputeLegalForParkedHuman(*park.agent);
        }
        replyEdit(res, recomputed);
    }

    void handleEditMove(const json& m) {
        applyEdit([&](GameState& state) {
            GameObjectId oid = GameObjectId::fromRaw(idField(m, "object_id"));
            std::string zone_str = strField(m, "to_zone");
            auto zone = strToZone(zone_str);
            if (!zone) return EditResult::fail("unknown to_zone: '" + zone_str + "'");
            if (*zone == ZoneType::BattlefieldZone) {
                BattlefieldId bid = BattlefieldId::fromRaw(idField(m, "battlefield_id"));
                return impl_.editor.moveObjectToBattlefield(state, oid, bid);
            }
            return impl_.editor.moveObject(state, oid, strToPlayer(strField(m, "to_player")),
                                           *zone, boolFieldOr(m, "to_top", true));
        });
    }

    void handleEditObject(const json& m) {
        applyEdit([&](GameState& state) {
            GameObjectId oid = GameObjectId::fromRaw(idField(m, "object_id"));
            std::string field = strField(m, "field");
            if (field == "exhausted") return impl_.editor.setObjectExhausted(state, oid, boolFieldOr(m, "value", false));
            if (field == "damage")    return impl_.editor.setObjectDamage(state, oid, intFieldOr(m, "value", 0));
            if (field == "might")     return impl_.editor.setObjectMight(state, oid, intFieldOr(m, "value", 0));
            return EditResult::fail("unknown object field: " + field);
        });
    }

    void handleEditPlayer(const json& m) {
        applyEdit([&](GameState& state) {
            PlayerId pid = strToPlayer(strField(m, "player"));
            std::string field = strField(m, "field");
            if (field == "score")  return impl_.editor.setPlayerScore(state, pid, intFieldOr(m, "value", 0));
            if (field == "energy") return impl_.editor.setPlayerEnergy(state, pid, intFieldOr(m, "value", 0));
            if (field == "power")  return impl_.editor.setPlayerPower(state, pid, intFieldOr(m, "domain_idx", -1), intFieldOr(m, "value", 0));
            return EditResult::fail("unknown player field: " + field);
        });
    }

    void handleEditPhase(const json& m) {
        applyEdit([&](GameState& state) {
            std::string field = strField(m, "field");
            if (field == "phase") {
                auto p = intField(m, "value");
                if (!p) return EditResult::fail("phase value must be an integer");
                return impl_.editor.setPhase(state, intFieldOr(m, "value", -1));
            }
            if (field == "turn_player") {
                return impl_.editor.setTurnPlayer(state, strToPlayer(strField(m, "value")));
            }
            return EditResult::fail("unknown phase field: " + field);
        });
    }

    void handleEditReorderDeck(const json& m) {
        applyEdit([&](GameState& state) {
            auto it = m.find("order");
            if (it == m.end() || !it->is_array()) {
                return EditResult::fail("order must be an array of object ids");
            }
            std::vector<GameObjectId> order;
            for (const auto& v : *it) {
                if (!v.is_number_integer()) {
                    return EditResult::fail("order must be an array of object ids");
                }
                order.push_back(GameObjectId::fromRaw(
                    idField(json{{"id", v}}, "id")));
            }
            return impl_.editor.reorderMainDeck(state, strToPlayer(strField(m, "player")), order);
        });
    }

    /// Caller holds the state mutex. `legal_recomputed`: the pending
    /// decision's legal actions were rebuilt from the edited state (the frame
    /// broadcast below then carries a new decision_id).
    void replyEdit(const EditResult& res, bool legal_recomputed) {
        if (res.ok) send(json{{"type", "edit_ok"}, {"legal_recomputed", legal_recomputed}}.dump());
        else        send(json{{"type", "edit_err"}, {"msg", res.error}}.dump());
        // Re-broadcast state so the UI re-renders post-edit.
        impl_.broadcastLocked(false);
    }

    void doWrite() {
        if (queue_.empty()) return;
        writing_ = true;
        auto self = shared_from_this();
        ws_.text(true);
        ws_.async_write(net::buffer(queue_.front()),
            [self](beast::error_code ec, std::size_t /*n*/) {
                self->writing_ = false;
                if (ec) {
                    self->impl_.pending_writes.fetch_sub(static_cast<int>(self->queue_.size()));
                    self->queue_.clear();
                    return;
                }
                self->impl_.pending_writes.fetch_sub(1);
                self->queue_.pop_front();
                if (!self->queue_.empty()) self->doWrite();
            });
    }

    ws::stream<tcp::socket>      ws_;
    std::atomic<uint8_t>         seat_{static_cast<uint8_t>(PlayerId::None)};
    PlayServer::Impl&            impl_;
    PlayServer&                  server_;
    beast::flat_buffer           read_buf_;
    std::deque<std::string>      queue_;
    bool                         writing_ = false;
};

// ─── HTTP-then-WS routing ───────────────────────────────────────────────────

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(tcp::socket socket, PlayServer::Impl& impl, PlayServer& server)
        : socket_(std::move(socket)), impl_(impl), server_(server) {}

    void run() {
        doRead();
    }

private:
    void doRead() {
        req_ = {};
        auto self = shared_from_this();
        http::async_read(socket_, buf_, req_,
            [self](beast::error_code ec, std::size_t /*n*/) {
                if (ec) return;
                self->onRequest();
            });
    }

    void onRequest() {
        const std::string target(req_.target());
        const auto qmark = target.find('?');
        const std::string path  = target.substr(0, qmark);
        const std::string query = qmark == std::string::npos ? "" : target.substr(qmark + 1);
        const auto& cfg = server_.config();

        if (!hostHeaderAllowed(std::string(req_[http::field::host]))) {
            return respond(http::status::forbidden, "text/plain",
                           "Forbidden: unexpected Host header");
        }
        if (!cfg.access_token.empty() && !queryHasToken(query, cfg.access_token)) {
            return respond(http::status::forbidden, "text/plain",
                           "Forbidden: missing or wrong access token");
        }

        if (ws::is_upgrade(req_)) {
            if (path != "/ws") {
                return respond(http::status::not_found, "text/plain", "Not Found");
            }
            if (!originAllowed(req_)) {
                return respond(http::status::forbidden, "text/plain",
                               "Forbidden: cross-origin WebSocket");
            }
            // Hand off to a WsSession that owns the socket.
            // It joins the broadcast list once the handshake is done.
            auto session = std::make_shared<WsSession>(std::move(socket_), impl_, server_);
            session->run(std::move(req_));
            return;
        }
        // Otherwise: serve embedded index for GET /, 404 elsewhere.
        if (req_.method() == http::verb::get && path == "/") {
            respond(http::status::ok, "text/html; charset=utf-8", impl_.index_html);
        } else {
            respond(http::status::not_found, "text/plain", "Not Found");
        }
    }

    void respond(http::status status, const char* content_type, std::string body) {
        http::response<http::string_body> res;
        res.result(status);
        res.set(http::field::content_type, content_type);
        res.body() = std::move(body);
        res.set(http::field::server, "riftbound_play");
        res.prepare_payload();
        auto self = shared_from_this();
        auto sp = std::make_shared<http::response<http::string_body>>(std::move(res));
        http::async_write(socket_, *sp,
            [self, sp](beast::error_code, std::size_t) {
                beast::error_code ec;
                self->socket_.shutdown(tcp::socket::shutdown_send, ec);
            });
    }

    tcp::socket                       socket_;
    beast::flat_buffer                buf_;
    http::request<http::string_body>  req_;
    PlayServer::Impl&                 impl_;
    PlayServer&                       server_;
};

// ─── Listener ───────────────────────────────────────────────────────────────

class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(tcp::acceptor& acceptor, PlayServer::Impl& impl, PlayServer& server)
        : acceptor_(acceptor), impl_(impl), server_(server) {}

    void start() { doAccept(); }

private:
    void doAccept() {
        auto self = shared_from_this();
        acceptor_.async_accept(
            [self](beast::error_code ec, tcp::socket socket) {
                if (ec) {
                    if (ec == net::error::operation_aborted) return;
                    self->doAccept();
                    return;
                }
                std::make_shared<HttpSession>(std::move(socket),
                                              self->impl_, self->server_)->run();
                self->doAccept();
            });
    }

    tcp::acceptor&    acceptor_;
    PlayServer::Impl& impl_;
    PlayServer&       server_;
};

} // namespace

// ─── State snapshot builder ────────────────────────────────────────────────

namespace {

json buildLegalActionJson(const CardDB& db,
                          const StateRenderer& renderer,
                          const GameState& state,
                          const std::vector<Intent>& actions) {
    json arr = json::array();
    for (size_t i = 0; i < actions.size(); ++i) {
        const auto& a = actions[i];
        std::string label = renderer.renderChosenAction(state, a);

        // Special-case the pre-game ChooseBattlefield decision: the
        // generic renderer just emits "ChooseBattlefield bf=N" where N
        // is the deck index — useless to a player. Look up the actual
        // BF CardDef name from the controller's battlefield_pool and
        // surface it. (After game start, the same intent type is also
        // used by some card effects targeting an EXISTING battlefield;
        // the renderer's regular path handles those.)
        if (a.type == IntentType::ChooseBattlefield) {
            const auto& pool = state.player(a.player).battlefield_pool;
            auto idx = static_cast<size_t>(a.battlefield_pool_index.raw());
            if (idx < pool.size()) {
                const auto& def = db.get(pool[idx]);
                label = "Choose battlefield: " + def.name;
                if (!def.ability_text.empty()) {
                    // Trim to the first sentence so the button stays compact.
                    auto first_dot = def.ability_text.find('.');
                    std::string flavour = first_dot == std::string::npos
                        ? def.ability_text
                        : def.ability_text.substr(0, first_dot + 1);
                    label += "  — " + flavour;
                }
            }
        }

        // The renderer can include newlines / "→" arrows; collapse for buttons.
        for (auto& c : label) if (c == '\n') c = ' ';
        json item = {
            {"index", static_cast<int>(i)},
            {"label", label.empty() ? a.describe() : label},
            {"type",  a.describe().substr(0, a.describe().find('('))},
        };
        if (a.card != kInvalidId) item["card_object_id"] = static_cast<int>(a.card.raw());
        if (a.ability_source != kInvalidId) item["ability_source"] = static_cast<int>(a.ability_source.raw());
        arr.push_back(std::move(item));
    }
    return arr;
}

json objToJson(const CardDB& db, const GameState& state, GameObjectId oid) {
    const auto& obj = state.getObject(oid);
    std::string name;
    if (obj.card_def_id != kInvalidId) {
        name = db.get(obj.card_def_id).name;
    } else {
        name = obj.name.empty() ? "(token)" : obj.name.get();
    }
    return json{
        {"id", static_cast<int>(oid.raw())},
        {"name", name},
        {"exhausted", obj.is_exhausted},
        {"damage", obj.damage_marked},
        {"might", obj.current_might},
        {"controller", playerToStr(obj.controller)},
        {"owner", playerToStr(obj.owner)},
    };
}

/// `show_hand` / `show_decks`: whether the frame's viewer may see this
/// player's hand and the order of their decks. A withheld zone is sent as
/// an empty array, named in `hidden`; `<zone>_count` always has its size.
json buildZoneJson(const CardDB& db, const GameState& state, PlayerId pid,
                   bool show_hand, bool show_decks) {
    const auto& ps = state.player(pid);
    auto vecToJson = [&](const std::vector<GameObjectId>& v, bool show) {
        json a = json::array();
        if (!show) return a;
        for (auto oid : v) a.push_back(objToJson(db, state, oid));
        return a;
    };
    json hidden = json::array();
    if (!show_hand)  hidden.push_back("hand");
    if (!show_decks) { hidden.push_back("main_deck"); hidden.push_back("rune_deck"); }

    // Sweep all GameObjects for ones at this player's base. Battlefield
    // units live in a separate per-BF bucket — see buildBattlefieldsJson.
    json base = json::array();
    for (const auto& [oid, obj] : state.objects) {
        if (obj.zone != ZoneType::Base) continue;
        if (!obj.location.has_value()) continue;
        if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
        if (std::get<BaseLocation>(*obj.location).player != pid) continue;
        base.push_back(objToJson(db, state, oid));
    }

    json z = {
        {"hand",       vecToJson(ps.hand, show_hand)},
        {"main_deck",  vecToJson(ps.main_deck, show_decks)},
        {"rune_deck",  vecToJson(ps.rune_deck, show_decks)},
        {"trash",      vecToJson(ps.trash, true)},
        {"banishment", vecToJson(ps.banishment, true)},
        {"base",       std::move(base)},
        {"hand_count",      ps.hand.size()},
        {"main_deck_count", ps.main_deck.size()},
        {"rune_deck_count", ps.rune_deck.size()},
        {"hidden",     std::move(hidden)},
        {"score",      ps.score},
        {"energy",     ps.rune_pool.energy},
        {"power_total", ps.rune_pool.totalPower()},
    };
    return z;
}

json buildBattlefieldsJson(const CardDB& db, const GameState& state) {
    json arr = json::array();
    for (const auto& bf : state.battlefields) {
        json units = json::array();
        for (const auto& [oid, obj] : state.objects) {
            if (obj.zone != ZoneType::BattlefieldZone) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BattlefieldLocation>(*obj.location)) continue;
            if (std::get<BattlefieldLocation>(*obj.location).id != bf.id) continue;
            units.push_back(objToJson(db, state, oid));
        }
        std::string controller_str = "none";
        if (bf.controller.has_value()) controller_str = playerToStr(*bf.controller);
        // Friendly name that disambiguates mirror-match BFs (two
        // "Vilemaw's Lair" → "Vilemaw's Lair (P1's)" / "(P2's)"). The
        // UI uses this for the move-destination dropdown and the
        // god-mode Battlefields section so the player can tell which
        // is which without reading the integer BF id.
        std::string name = StateRenderer::battlefieldLabel(state, bf.id);
        std::string contributor_str = "none";
        if (bf.contributed_by == PlayerId::Player1) contributor_str = "P1";
        else if (bf.contributed_by == PlayerId::Player2) contributor_str = "P2";
        arr.push_back(json{
            {"id", static_cast<int>(bf.id.raw())},
            {"name", std::move(name)},
            {"controller", controller_str},
            {"contributed_by", contributor_str},
            {"units", std::move(units)},
            {"is_token", bf.is_token},
        });
    }
    return arr;
}

} // namespace

PlayServer::Impl::Park PlayServer::Impl::parkedLocked() {
    Park park;
    if (engine_finished) {
        park.kind = Park::Kind::Finished;
        return park;
    }
    if (!engine_released) {
        auto human = [&](HumanAgent* agent, PlayerId seat, uint64_t answered) {
            if (!agent || !agent->atDecision()) return false;
            const uint64_t id = agent->decisionId();
            if (id == answered) return false;   // answered; engine may be running
            park.kind = Park::Kind::Human;
            park.player = seat;
            park.decision_id = id;
            park.agent = agent;
            return true;
        };
        if (human(parent.config_.p1_human, PlayerId::Player1, answered_p1)) return park;
        if (human(parent.config_.p2_human, PlayerId::Player2, answered_p2)) return park;
    }
    if (parent.config_.spectator) {
        std::lock_guard<std::mutex> lock(spec_mu);
        if (spec_waiting) {
            park.kind = Park::Kind::Spectator;
            park.player = spec_player;
            park.decision_id = spec_decision_id;
        }
    }
    return park;
}

std::string PlayServer::Impl::keyOf(const Park& park) {
    switch (park.kind) {
        case Park::Kind::Human:
            return "H" + playerToStr(park.player) + ":" + std::to_string(park.decision_id);
        case Park::Kind::Spectator: return "S:" + std::to_string(park.decision_id);
        case Park::Kind::Finished:  return "finished";
        case Park::Kind::Busy:      break;
    }
    return "busy";
}

RenderVisibility PlayServer::Impl::visibilityFor(const Park& park) const {
    const auto& cfg = parent.config_;
    if (cfg.reveal_hidden) return RenderVisibility{true, true};
    const bool h1 = cfg.p1_human != nullptr;
    const bool h2 = cfg.p2_human != nullptr;
    RenderVisibility v{false, false};
    if (h1 && h2) {
        // Hot seat, one screen: only the hand of whoever is deciding.
        if (park.kind == Park::Kind::Human) {
            v.p1 = park.player == PlayerId::Player1;
            v.p2 = park.player == PlayerId::Player2;
        }
    } else {
        v.p1 = h1;
        v.p2 = h2;
    }
    return v;
}

json PlayServer::Impl::buildSnapshotLocked(const Park& /*park*/, const RenderVisibility& vis) {
    const auto& st = parent.engine_.state();
    const auto& cfg = parent.config_;

    json snap = json::object();
    snap["render_text"] = parent.renderer_.render(st, vis);
    snap["zones"] = {
        {"P1", buildZoneJson(parent.card_db_, st, PlayerId::Player1, vis.p1, cfg.reveal_hidden)},
        {"P2", buildZoneJson(parent.card_db_, st, PlayerId::Player2, vis.p2, cfg.reveal_hidden)},
    };
    snap["battlefields"] = buildBattlefieldsJson(parent.card_db_, st);
    // Phase name string — int-only was a UX miss, the integer phase
    // tells the user nothing without a CR cross-reference.
    const char* phase_name = "?";
    switch (st.turn.phase) {
        case TurnPhase::Setup:           phase_name = "Setup";         break;
        case TurnPhase::Mulligan:        phase_name = "Mulligan";      break;
        case TurnPhase::AwakenPhase:     phase_name = "Awaken";        break;
        case TurnPhase::BeginningStep:   phase_name = "Beginning";     break;
        case TurnPhase::ScoringStep:     phase_name = "Scoring";       break;
        case TurnPhase::ChannelPhase:    phase_name = "Channel";       break;
        case TurnPhase::DrawPhase:       phase_name = "Draw";          break;
        case TurnPhase::MainPhase:       phase_name = "Main";          break;
        case TurnPhase::EndingStep:      phase_name = "Ending";        break;
        case TurnPhase::ExpirationStep:  phase_name = "Expiration";    break;
        default:                          phase_name = "?";            break;
    }
    snap["turn"] = {
        {"phase",       static_cast<int>(st.turn.phase)},
        {"phase_name",  phase_name},
        {"turn_player", playerToStr(st.turn.turn_player)},
        {"turn_number", st.turn.turn_number},
        {"p1_score",    st.player(PlayerId::Player1).score},
        {"p2_score",    st.player(PlayerId::Player2).score},
        {"victory_score", st.mode.victory_score},
    };
    snap["game_over"] = st.game_over;
    if (st.game_over) {
        snap["winner"] = playerToStr(st.winner);
        snap["game_over_reason"] = st.game_over_reason;
    }
    return snap;
}

json PlayServer::Impl::buildFrameLocked(const Park& park, bool can_read, Audience audience) {
    const bool at_decision = park.kind == Park::Kind::Human ||
                             park.kind == Park::Kind::Spectator;
    // What this audience may see. Shared: as the server always did. A seat:
    // its own hand. Spectators: neither hand.
    RenderVisibility vis = visibilityFor(park);
    if (audience == SeatP1) vis = RenderVisibility{true, false};
    if (audience == SeatP2) vis = RenderVisibility{false, true};
    if (audience == Spectators) vis = RenderVisibility{false, false};
    // The options (their labels name cards in a hand) and the prompt belong
    // to the seat that is being asked.
    const bool asked_here = audience == Shared ||
        (park.kind == Park::Kind::Human && audience == audienceOf(park.player));

    json frame;
    if (can_read) {
        frame = buildSnapshotLocked(park, vis);
        last_snapshot[audience] = frame;
    } else if (!last_snapshot[audience].is_null()) {
        frame = last_snapshot[audience];
    } else {
        // Nothing snapshotted yet (client connected before the first decision).
        json empty_zone = {
            {"hand", json::array()}, {"main_deck", json::array()},
            {"rune_deck", json::array()}, {"trash", json::array()},
            {"banishment", json::array()}, {"base", json::array()},
            {"score", 0}, {"energy", 0}, {"power_total", 0},
        };
        frame = {
            {"render_text", ""},
            {"zones", {{"P1", empty_zone}, {"P2", empty_zone}}},
            {"battlefields", json::array()},
            {"turn", json::object()},
            {"game_over", false},
        };
    }

    frame["type"] = at_decision ? "decision" : "state";
    frame["decision_id"] = park.decision_id;
    frame["player"] = playerToStr(park.player);
    frame["god_mode"] = parent.config_.god_mode_enabled;
    frame["engine_busy"] = park.kind == Park::Kind::Busy;
    // Only seat-bound frames say whose they are: a Shared frame is exactly
    // what it was before sessions had seats.
    if (audience != Shared)
        frame["your_seat"] = audience == SeatP1 ? "P1" : audience == SeatP2 ? "P2" : "";

    std::vector<Intent> legal;
    if (park.kind == Park::Kind::Human && asked_here) legal = park.agent->pendingLegal();

    // Spectator mode: the "decision" is the one an AI seat just made,
    // held at the gate until the viewer steps.
    if (parent.config_.spectator) {
        std::lock_guard<std::mutex> lock(spec_mu);
        json spectator = {
            {"waiting",  spec_waiting},
            {"autoplay", spec_autoplay},
            {"delay_ms", spec_delay_ms},
        };
        if (park.kind == Park::Kind::Spectator) {
            legal = spec_legal;
            spectator["chosen"] = spec_chosen;
        }
        frame["spectator"] = std::move(spectator);
    }
    // Labels name cards, so they need GameState: only decisions have any.
    frame["legal"] = at_decision
        ? buildLegalActionJson(parent.card_db_, parent.renderer_,
                               parent.engine_.state(), legal)
        : json::array();

    if (at_decision && asked_here) {
        std::lock_guard<std::mutex> lock(prompt_mu);
        if (!last_prompt.empty()) frame["prompt"] = last_prompt;
    }
    return frame;
}

void PlayServer::Impl::broadcastLocked(bool on_engine_thread) {
    const Park park = parkedLocked();
    // GameState may be read on the engine thread itself, or while the
    // engine is parked (and this lock keeps it parked). Otherwise clients
    // get the last snapshot again.
    const bool can_read = on_engine_thread || park.kind != Park::Kind::Busy;

    if (!perSeatViews()) {
        const json frame = buildFrameLocked(park, can_read, Shared);
        last_key = keyOf(park);
        sendAll(frame.dump());
        return;
    }
    // Seat-bound: each seat its own view, spectators theirs. A view nobody
    // is connected for is still built while the state can be read, so a
    // client that connects while the engine is busy has a board to show.
    for (Audience audience : {SeatP1, SeatP2, Spectators}) {
        if (!can_read && !hasClients(audience)) continue;
        const json frame = buildFrameLocked(park, can_read, audience);
        if (hasClients(audience)) sendTo(audience, frame.dump());
    }
    last_key = keyOf(park);
}

void PlayServer::Impl::broadcast(bool on_engine_thread) {
    std::lock_guard<std::mutex> lock(parent.state_mutex_);
    broadcastLocked(on_engine_thread);
}

void PlayServer::Impl::clearPrompt() {
    std::lock_guard<std::mutex> lock(prompt_mu);
    last_prompt.clear();
}

void PlayServer::Impl::sendAll(const std::string& text) {
    std::vector<std::shared_ptr<WsSession>> live;
    {
        std::lock_guard<std::mutex> lock(sessions_mu);
        for (auto it = sessions.begin(); it != sessions.end();) {
            if (auto s = it->lock()) { live.push_back(std::move(s)); ++it; }
            else it = sessions.erase(it);
        }
    }
    for (auto& s : live) s->send(text);
}

void PlayServer::Impl::sendTo(Audience audience, const std::string& text) {
    std::vector<std::shared_ptr<WsSession>> live;
    {
        std::lock_guard<std::mutex> lock(sessions_mu);
        for (auto it = sessions.begin(); it != sessions.end();) {
            if (auto s = it->lock()) {
                if (audienceOf(s->seat()) == audience) live.push_back(std::move(s));
                ++it;
            } else {
                it = sessions.erase(it);
            }
        }
    }
    for (auto& s : live) s->send(text);
}

bool PlayServer::Impl::hasClients(Audience audience) {
    std::lock_guard<std::mutex> lock(sessions_mu);
    for (auto& weak : sessions)
        if (auto s = weak.lock())
            if (audienceOf(s->seat()) == audience) return true;
    return false;
}

bool PlayServer::Impl::seatTaken(PlayerId seat, const WsSession* asking) {
    std::lock_guard<std::mutex> lock(sessions_mu);
    for (auto& weak : sessions)
        if (auto s = weak.lock())
            if (s.get() != asking && s->seat() == seat) return true;
    return false;
}

void PlayServer::Impl::spectatorStep() {
    std::lock_guard<std::mutex> lock(spec_mu);
    // A click that lands while the engine is busy is dropped, so a
    // double-click can't skip a decision the viewer never saw.
    if (!spec_waiting) return;
    spec_step_requested = true;
    spec_cv.notify_all();
}

void PlayServer::Impl::setSpectatorAutoplay(bool on, int delay_ms) {
    std::lock_guard<std::mutex> lock(spec_mu);
    spec_autoplay = on;
    spec_delay_ms = std::clamp(delay_ms, 0, 10000);
    spec_cv.notify_all();
}

bool PlayServer::Impl::recomputeLegalForParkedHuman(HumanAgent& agent) {
    // The engine knows which decision it is parked on and can rebuild the
    // list of the three whose options are a function of the state alone: the
    // main-phase decision, a showdown's focus decision and a chain priority
    // decision (GameEngine::recomputePendingLegalActions). For every other
    // one — a mulligan, combat damage, a cost payment, a card's prompt — it
    // returns nullopt and the list stays; the reply says so
    // (`legal_recomputed`).
    //
    // This matters beyond tidiness: the engine checks the answer to those
    // three against the list rebuilt from the edited state and throws on one
    // that is no longer legal. A human left looking at the pre-edit list could
    // click such an option and end the game.
    std::optional<std::vector<Intent>> fresh = parent.engine_.recomputePendingLegalActions();
    if (!fresh.has_value() || fresh->empty()) return false;
    const std::vector<Intent> pending = agent.pendingLegal();
    // Every option of one decision names the player being asked. If the edit
    // changed who that is (the turn player or the phase was edited), this
    // parked agent is not the one to hand the new list to.
    if (fresh->front().player != agent.pendingPlayer()) return false;
    if (*fresh == pending) return false;
    return agent.replacePendingLegal(std::move(*fresh));
}

void PlayServer::Impl::releaseSpectator() {
    std::lock_guard<std::mutex> lock(spec_mu);
    spec_released = true;
    spec_cv.notify_all();
}

void PlayServer::awaitSpectatorStep(const std::vector<Intent>& legal, const Intent& chosen) {
    if (!config_.spectator || !running_.load()) return;
    auto& im = *impl_;
    {
        std::lock_guard<std::mutex> state_lock(state_mutex_);
        {
            std::lock_guard<std::mutex> lock(im.spec_mu);
            if (im.spec_released) return;
            im.spec_legal = legal;
            auto it = std::find(legal.begin(), legal.end(), chosen);
            im.spec_chosen = it == legal.end() ? -1 : static_cast<int>(it - legal.begin());
            im.spec_player = chosen.player;
            ++im.spec_decision_id;
            im.spec_step_requested = false;
            im.spec_waiting = true;
        }
        im.broadcastLocked(true);
    }
    {
        std::unique_lock<std::mutex> lock(im.spec_mu);
        auto stepped = [&] { return im.spec_step_requested || im.spec_released; };
        while (!stepped()) {
            if (im.spec_autoplay) {
                auto delay = std::chrono::milliseconds(im.spec_delay_ms);
                // Timed out with autoplay still on → advance. Paused
                // mid-wait → fall back to waiting for a step.
                if (!im.spec_cv.wait_for(lock, delay, [&] { return stepped() || !im.spec_autoplay; })) break;
            } else {
                im.spec_cv.wait(lock, [&] { return stepped() || im.spec_autoplay; });
            }
        }
    }
    // Leave the gate under the state mutex: a server thread that saw
    // spec_waiting is reading GameState and must finish before the engine
    // moves on.
    std::lock_guard<std::mutex> state_lock(state_mutex_);
    {
        std::lock_guard<std::mutex> lock(im.spec_mu);
        im.spec_waiting = false;
        im.spec_step_requested = false;
    }
    im.clearPrompt();
    // Clear the held decision so the UI shows "engine busy" while the
    // next agent thinks.
    im.broadcastLocked(true);
}

void PlayServer::publishEngineState() {
    if (!running_.load()) return;
    impl_->broadcast(/*on_engine_thread=*/true);
}

void PlayServer::notifyDecisionMade() {
    impl_->clearPrompt();
}

void PlayServer::notifyGameFinished() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    impl_->engine_finished = true;
    if (running_.load()) impl_->broadcastLocked(false);
}

void PlayServer::Impl::runWatcher() {
    using namespace std::chrono_literals;
    while (!stopping.load()) {
        std::this_thread::sleep_for(100ms);
        if (stopping.load()) break;

        // Broadcast when what there is to say changes: a human seat
        // reached a new decision, or the engine went from waiting to busy
        // (a HumanAgent was aborted, say). Everything else is pushed by
        // whoever causes it.
        std::lock_guard<std::mutex> lock(parent.state_mutex_);
        if (keyOf(parkedLocked()) != last_key) broadcastLocked(false);
    }
}

// ─── PlayServer member functions ───────────────────────────────────────────

PlayServer::PlayServer(GameEngine& engine,
                       StateRenderer& renderer,
                       const CardDB& card_db,
                       PlayServerConfig config)
    : impl_(std::make_unique<Impl>(*this)),
      engine_(engine),
      renderer_(renderer),
      card_db_(card_db),
      config_(std::move(config)) {}

PlayServer::~PlayServer() {
    stop();
}

namespace {
std::string serverUrl(const PlayServerConfig& cfg, uint16_t port) {
    std::string url = "http://" + cfg.bind_address + ":" + std::to_string(port);
    if (!cfg.access_token.empty()) url += "/?token=" + cfg.access_token;
    return url;
}
} // namespace

uint16_t PlayServer::port() const noexcept {
    beast::error_code ec;
    if (impl_->acceptor) {
        auto ep = impl_->acceptor->local_endpoint(ec);
        if (!ec) return ep.port();
    }
    return config_.port;
}

std::string PlayServer::start() {
    if (running_.load()) return serverUrl(config_, port());

    // Build the served HTML once; reused across all GET /.
    const auto& images = ReplayWriter::loadCardImageMap();
    impl_->index_html = buildPlayIndexHtml(
        images, config_.version_tag, config_.build_date, config_.seed,
        config_.god_mode_enabled);

    // Both throw on failure (bad address, port in use). Nothing is running
    // yet, so the caller gets the exception and a server it can discard.
    auto addr = net::ip::make_address(config_.bind_address);
    impl_->acceptor.emplace(impl_->ioc, tcp::endpoint{addr, config_.port});
    std::make_shared<Listener>(*impl_->acceptor, *impl_, *this)->start();
    impl_->armSignalSet();
    running_.store(true);

    impl_->io_thread = std::thread([this] {
        // A handler that throws must not end the loop: the signal handler
        // and every connection live on it.
        while (!impl_->stopping.load()) {
            try {
                impl_->ioc.run();
                break;   // stopped
            } catch (const std::exception& e) {
                std::cerr << "[play_server] io_context exception: " << e.what() << "\n";
            }
        }
    });
    impl_->watcher_thread = std::thread([this] { impl_->runWatcher(); });

    return serverUrl(config_, port());
}

void PlayServer::Impl::armSignalSet() {
    signals.emplace(ioc, SIGINT, SIGTERM);
    signals->async_wait([this](beast::error_code ec, int sig) {
        if (ec) return;  // cancelled during shutdown — fine
        std::cerr << "\n[riftbound_play] received signal " << sig
                  << "; shutting down (signal again to force-quit)\n";
        parent.requestShutdown();
        // Re-arm so a second signal does NOT get handled here — we want
        // the second signal to use the default handler (kernel kills us).
        // Boost.Asio resets pending registrations once a handler fires;
        // re-installing the default disposition for SIGINT/SIGTERM lets
        // a second Ctrl-C terminate immediately if cleanup wedges.
        std::signal(SIGINT,  SIG_DFL);
        std::signal(SIGTERM, SIG_DFL);
    });
}

void PlayServer::requestShutdown() {
    // Before anything is woken: an AI seat asked from here on concedes.
    shutdown_requested_.store(true, std::memory_order_release);
    {
        // The callback aborts the human seats, which wakes the engine:
        // from here on it is never parked for the server again.
        std::lock_guard<std::mutex> lock(state_mutex_);
        impl_->engine_released = true;
        if (config_.on_shutdown_signal) config_.on_shutdown_signal();
    }
    impl_->releaseSpectator();
}

void PlayServer::stop() {
    if (!running_.exchange(false)) return;
    impl_->stopping.store(true);
    impl_->releaseSpectator();
    // Push the last frame (the terminal state, once notifyGameFinished has
    // been called) and wait for the IO thread to write it out.
    impl_->broadcast();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (impl_->pending_writes.load() > 0 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    beast::error_code ec;
    if (impl_->acceptor) impl_->acceptor->close(ec);
    impl_->ioc.stop();
    if (impl_->io_thread.joinable()) impl_->io_thread.join();
    if (impl_->watcher_thread.joinable()) impl_->watcher_thread.join();
}

void PlayServer::broadcastLog(const std::string& level, const std::string& message) {
    // No audience given: nobody but an all-seeing viewer may read it.
    broadcastLog(level, message, LogAudience::neither());
}

void PlayServer::broadcastLog(const std::string& level, const std::string& message,
                              LogAudience audience) {
    if (!running_.load()) return;

    // Capture *_PROMPT trace lines into impl_->last_prompt so the next
    // decision frame can render them as a header. Format the engine
    // uses: "MAY_PROMPT: <text>", "X_PROMPT: <text>", "MODE_PROMPT:
    // <text> [...]", "TGT_PROMPT: <text> [...]". We strip the prefix
    // for display and keep the question.
    // (Captured even when the stream itself is withheld, below: a prompt
    // only ever heads the decision of the seat it was asked of.)
    auto pos = message.find("_PROMPT:");
    if (pos != std::string::npos) {
        // Trim past the "_PROMPT: " marker (9 chars).
        std::string prompt = message.substr(pos + 9);
        // Drop the bracketed option summary at the end if present —
        // the legal-actions list already shows the options.
        auto bracket = prompt.rfind(" [");
        if (bracket != std::string::npos) prompt = prompt.substr(0, bracket);
        std::lock_guard<std::mutex> lock(impl_->prompt_mu);
        impl_->last_prompt = std::move(prompt);
    }

    // A line that names a card as it is drawn, mulliganed, hidden or looked
    // at is tagged with the players who may read it (LogEvent::audience).
    // When frames withhold hidden information a line is sent only if every
    // human seat may read it: all clients get the same stream.
    json frame = {
        {"type", "log"},
        {"level", level},
        {"msg", message},
    };
    // Seat-bound: a line goes to each seat that may read it, and to the
    // spectators when both players may (plans/09 9.PP2).
    if (config_.seat_bound && !config_.reveal_hidden) {
        const std::string text = frame.dump();
        if (audience.includes(PlayerId::Player1)) impl_->sendTo(Impl::SeatP1, text);
        if (audience.includes(PlayerId::Player2)) impl_->sendTo(Impl::SeatP2, text);
        if (audience.isBoth()) impl_->sendTo(Impl::Spectators, text);
        return;
    }
    if (!config_.reveal_hidden) {
        if (config_.p1_human && !audience.includes(PlayerId::Player1)) return;
        if (config_.p2_human && !audience.includes(PlayerId::Player2)) return;
        if (!config_.p1_human && !config_.p2_human && !audience.isBoth()) return;
    }

    impl_->sendAll(frame.dump());
}

// ─── Accessor shims used by the session classes ────────────────────────────
// (declared inline rather than in the header to keep Beast out of the
//  public interface).

HumanAgent* PlayServer::config_p1() const noexcept { return config_.p1_human; }
HumanAgent* PlayServer::config_p2() const noexcept { return config_.p2_human; }
bool PlayServer::godModeEnabled() const noexcept   { return config_.god_mode_enabled; }
GameState& PlayServer::engineMutableState() noexcept { return engine_.mutableState(); }

} // namespace riftbound
