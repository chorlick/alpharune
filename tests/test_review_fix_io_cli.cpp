/// @file test_review_fix_io_cli.cpp
/// Regression tests for plan 04 (play server, HTML output, runners).
///
/// The play-server tests run a real game (GameEngine::runGame on its own
/// thread, a HumanAgent in seat 1) behind a real PlayServer on an ephemeral
/// loopback port, and talk to it over a WebSocket exactly as the browser
/// does.

#include "io/batch_report.h"
#include "io/play_index_html.h"
#include "io/play_server.h"
#include "io/replay_writer.h"
#include "io/state_editor.h"
#include "io/state_renderer.h"

#include "agents/human_agent.h"
#include "agents/random_agent.h"
#include "cards/card_registry.h"
#include "engine/batch_runner.h"
#include "rules/deck_validator.h"

#include <gtest/gtest.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>

// Unity builds merge file scopes: everything here lives in this file's namespace.
namespace riftbound::test::review_fix_io_cli {

using namespace std::chrono_literals;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = beast::websocket;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;
using json      = nlohmann::json;

namespace {

// ─── Shared card data ────────────────────────────────────────────────────────

struct Cards {
    CardRegistry registry;
    CardDB db;
    DeckSubmission deck1, deck2;

    Cards() {
        registry.loadAll();
        db.buildFromClasses(registry);
        const char* env = std::getenv("RIFTBOUND_ROOT");
        const std::string root = env ? env : ".";
        deck1 = DeckValidator::loadFromDeckList(root + "/decks/fiora.txt", db);
        deck2 = DeckValidator::loadFromDeckList(root + "/decks/yi_rebuild.txt", db);
    }
};

Cards& cards() {
    static Cards c;
    return c;
}

// ─── A seat-2 AI the test can freeze mid-decision ────────────────────────────
// While `hold` is set the engine thread blocks inside selectAction: the
// engine is "busy" (not parked for the server) for as long as the test likes.

class GatedAgent : public AgentInterface {
public:
    explicit GatedAgent(uint64_t seed) : inner_(seed) {}

    Intent selectAction(const GameState& state,
                        const std::vector<Intent>& legal) override {
        {
            std::unique_lock<std::mutex> lock(mu_);
            if (hold_) {
                entered_ = true;
                cv_.notify_all();
                cv_.wait(lock, [&] { return !hold_; });
                entered_ = false;
            }
        }
        return inner_.selectAction(state, legal);
    }

    void hold() { std::lock_guard<std::mutex> lock(mu_); hold_ = true; }
    void release() {
        { std::lock_guard<std::mutex> lock(mu_); hold_ = false; }
        cv_.notify_all();
    }
    bool entered() { std::lock_guard<std::mutex> lock(mu_); return entered_; }

private:
    RandomAgent inner_;
    std::mutex mu_;
    std::condition_variable cv_;
    bool hold_ = false;
    bool entered_ = false;
};

// ─── WebSocket test client ───────────────────────────────────────────────────

class Client {
public:
    /// Returns false if the server declined the upgrade.
    bool connect(uint16_t port, const std::string& target = "/ws",
                 const std::string& origin = "", std::string host = "") {
        if (host.empty()) host = "127.0.0.1:" + std::to_string(port);
        beast::error_code ec;
        ws_.next_layer().connect(
            tcp::endpoint{net::ip::make_address("127.0.0.1"), port}, ec);
        if (ec) return false;
        if (!origin.empty()) {
            ws_.set_option(ws::stream_base::decorator(
                [origin](ws::request_type& req) { req.set(http::field::origin, origin); }));
        }
        ws_.handshake(host, target, ec);
        return !ec;
    }

    void send(const json& m) { sendRaw(m.dump()); }
    void sendRaw(const std::string& text) { ws_.write(net::buffer(text)); }

    /// Next message, or nullopt if none arrives within `timeout` (or the
    /// connection closed — see closed()).
    std::optional<json> read(std::chrono::milliseconds timeout = 3000ms) {
        if (closed_) return std::nullopt;
        if (!reading_) {
            reading_ = true;
            done_ = false;
            ws_.async_read(buf_, [this](beast::error_code ec, std::size_t) {
                done_ = true;
                if (ec) closed_ = true;
            });
        }
        ioc_.restart();
        ioc_.run_for(timeout);
        if (!done_) return std::nullopt;
        reading_ = false;
        if (closed_) return std::nullopt;
        json m = json::parse(beast::buffers_to_string(buf_.data()));
        buf_.consume(buf_.size());
        return m;
    }

    /// Reads until `pred(message)`; fails the test and returns null json on timeout.
    template <class Pred>
    json readUntil(Pred pred, std::chrono::milliseconds timeout = 5000ms) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline && !closed_) {
            auto m = read(200ms);
            if (m && pred(*m)) return *m;
        }
        ADD_FAILURE() << "timed out waiting for a message";
        return json();
    }

    json nextFrame() {
        return readUntil([](const json& m) {
            return m["type"] == "decision" || m["type"] == "state";
        });
    }

    /// The next `decision` frame for `player` whose id is greater than `after`.
    json nextDecision(const std::string& player, uint64_t after = 0) {
        return readUntil([&](const json& m) {
            return m["type"] == "decision" && m["player"] == player &&
                   m["decision_id"].get<uint64_t>() > after;
        });
    }

    /// The reply to a request: the next `err` / `edit_err` / `edit_ok`.
    json nextReply() {
        return readUntil([](const json& m) {
            return m["type"] == "err" || m["type"] == "edit_err" || m["type"] == "edit_ok";
        });
    }

    bool closed() const { return closed_; }

private:
    net::io_context ioc_;
    ws::stream<tcp::socket> ws_{ioc_};
    beast::flat_buffer buf_;
    bool reading_ = false;
    bool done_ = false;
    bool closed_ = false;
};

// ─── A live game behind a live server ────────────────────────────────────────

struct LiveGame {
    EventBus events;
    GameEngine engine{cards().db, events, cards().registry};
    StateRenderer renderer{cards().db};
    HumanAgent human{PlayerId::Player1};   // seat 1 (unless spectating)
    RandomAgent random1{11};          // seat 1 when spectating
    GatedAgent ai{22};                // seat 2
    std::unique_ptr<PlayServer> server;
    std::thread game_thread;
    bool spectating = false;
    std::atomic<int> trace_lines{0};   // engine log lines handed to the server
    std::mutex log_mu;
    std::vector<std::string> not_for_p1;   // lines P1 may not read (LogEvent::audience)

    /// `tweak` adjusts the config before the server starts.
    explicit LiveGame(std::function<void(PlayServerConfig&)> tweak = {},
                      bool spectator = false)
        : spectating(spectator) {
        renderer.show_hand = true;
        PlayServerConfig cfg;
        cfg.port = 0;   // ephemeral
        cfg.god_mode_enabled = true;
        cfg.reveal_hidden = true;
        cfg.spectator = spectator;
        if (!spectator) cfg.p1_human = &human;
        cfg.on_shutdown_signal = [this] { human.abort(); };
        if (tweak) tweak(cfg);
        server = std::make_unique<PlayServer>(engine, renderer, cards().db, std::move(cfg));
        server->start();
        // As main.cpp does with a human seat: the whole trace goes to the server.
        events.on_log.connect([this](const LogEvent& e) {
            trace_lines.fetch_add(1);
            if (!e.visibleTo(PlayerId::Player1)) {
                std::lock_guard<std::mutex> lock(log_mu);
                not_for_p1.push_back(e.message);
            }
            server->broadcastLog(e.level == LogLevel::Trace ? "TRC" : "DBG", e.message,
                                 e.audience);
        });
        engine.on_decision = [this](const GameState&, const std::vector<Intent>& legal,
                                    const Intent& chosen) {
            server->awaitSpectatorStep(legal, chosen);
            server->notifyDecisionMade();
        };
        game_thread = std::thread([this] {
            AgentInterface& seat1 = spectating ? static_cast<AgentInterface&>(random1)
                                               : static_cast<AgentInterface&>(human);
            engine.runGame(cards().deck1, cards().deck2, seat1, ai, 20261010);
        });
    }

    uint16_t port() const { return server->port(); }

    /// Ends the game the way a shutdown signal does and waits for the engine.
    void finishGame() {
        server->requestShutdown();
        ai.release();
        if (game_thread.joinable()) game_thread.join();
    }

    ~LiveGame() {
        finishGame();
        server->stop();
    }
};

/// Answers P1's decisions with option 0 until `done(frame)` holds for one of
/// them; returns that frame (P1 still parked on it).
template <class Pred>
json playUntil(Client& c, Pred done, int max_decisions = 200) {
    uint64_t last = 0;
    for (int i = 0; i < max_decisions; ++i) {
        json f = c.nextDecision("P1", last);
        if (f.is_null()) return f;
        if (done(f)) return f;
        last = f["decision_id"].get<uint64_t>();
        c.send({{"type", "choose"}, {"player", "P1"}, {"index", 0}, {"decision_id", last}});
    }
    ADD_FAILURE() << "condition not reached in " << max_decisions << " decisions";
    return json();
}

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int countOf(const std::string& hay, const std::string& needle) {
    int n = 0;
    for (auto pos = hay.find(needle); pos != std::string::npos;
         pos = hay.find(needle, pos + needle.size())) ++n;
    return n;
}

} // namespace

// ─── 4.2 — a malformed message must not take the IO thread down ──────────────

TEST(PlayServerProtocol, MalformedMessagesGetErrorsAndServerKeepsServing) {
    LiveGame game;
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    json first = c.nextDecision("P1");
    ASSERT_FALSE(first.is_null());

    // Each of these used to throw json::type_error out of the read handler,
    // which ended io_context::run() — and with it the signal handler.
    const std::vector<std::string> bad = {
        "this is not json",
        "[1,2,3]",
        R"({"type":5})",
        R"({"type":"choose","player":"P1","index":"zero","decision_id":"x"})",
        R"({"type":"choose","player":7,"index":0})",
        R"({"type":"edit_reorder_deck","player":"P1","order":["a",{}]})",
        R"({"type":"edit_reorder_deck","player":"P1","order":7})",
        R"({"type":"edit_move","object_id":"x","to_zone":5,"to_player":[]})",
        R"({"type":"edit_object","object_id":1e300,"field":"damage","value":"lots"})",
        R"({"type":"edit_player","player":{},"field":"score","value":[1]})",
        R"({"type":"edit_phase","field":"phase","value":"main"})",
    };
    for (const auto& text : bad) {
        c.sendRaw(text);
        json reply = c.nextReply();
        ASSERT_FALSE(reply.is_null()) << "no reply to: " << text;
        EXPECT_NE(reply["type"], "edit_ok") << text;
    }
    c.sendRaw(R"({"type":"spectator_autoplay","on":"yes","delay_ms":"fast"})");

    // Still alive, still parked on the same decision, and a new client can connect.
    c.send({{"type", "request_state"}});
    json again = c.nextDecision("P1");
    EXPECT_EQ(again["decision_id"], first["decision_id"]);
    Client second;
    EXPECT_TRUE(second.connect(game.port()));
    EXPECT_FALSE(second.nextFrame().is_null());
}

// ─── 4.3 — a choose answers only the decision it names ───────────────────────

TEST(PlayServerProtocol, StaleOrInvalidChooseIsRefused) {
    LiveGame game;
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    json d1 = c.nextDecision("P1");
    ASSERT_FALSE(d1.is_null());
    const uint64_t id1 = d1["decision_id"].get<uint64_t>();
    const int n = static_cast<int>(d1["legal"].size());
    ASSERT_GT(n, 0);

    auto refused = [&](json msg, const char* why) {
        c.send(msg);
        json reply = c.nextReply();
        EXPECT_EQ(reply["type"], "err") << why;
        // Nothing was applied: the engine is still waiting on decision id1.
        EXPECT_TRUE(game.human.atDecision()) << why;
        EXPECT_EQ(game.human.decisionId(), id1) << why;
    };
    refused({{"type", "choose"}, {"player", "P1"}, {"index", 0}}, "no decision_id");
    refused({{"type", "choose"}, {"player", "P1"}, {"index", 0}, {"decision_id", id1 + 1}},
            "decision_id from the future");
    refused({{"type", "choose"}, {"player", "P1"}, {"index", n}, {"decision_id", id1}},
            "index past the end");
    refused({{"type", "choose"}, {"player", "P1"}, {"index", -1}, {"decision_id", id1}},
            "negative index");
    refused({{"type", "choose"}, {"player", "P2"}, {"index", 0}, {"decision_id", id1}},
            "seat that is not human");

    // A valid answer goes through…
    c.send({{"type", "choose"}, {"player", "P1"}, {"index", 0}, {"decision_id", id1}});
    json d2 = c.nextDecision("P1", id1);
    ASSERT_FALSE(d2.is_null());
    const uint64_t id2 = d2["decision_id"].get<uint64_t>();

    // …and the same click arriving again (a double click, a second tab) is
    // not applied to the decision that followed.
    c.send({{"type", "choose"}, {"player", "P1"}, {"index", 0}, {"decision_id", id1}});
    json reply = c.nextReply();
    EXPECT_EQ(reply["type"], "err");
    EXPECT_NE(reply["msg"].get<std::string>().find("stale"), std::string::npos);
    EXPECT_TRUE(game.human.atDecision());
    EXPECT_EQ(game.human.decisionId(), id2);
}

// ─── 4.1 — no reads or edits of GameState while the engine is running ────────

TEST(PlayServerProtocol, EditsAndSnapshotsWaitForTheEngineToPark) {
    LiveGame game;
    game.ai.hold();   // seat 2 freezes inside its first decision
    Client c;
    ASSERT_TRUE(c.connect(game.port()));

    // Answer P1 until the engine is inside seat 2's selectAction.
    uint64_t last = 0;
    auto deadline = std::chrono::steady_clock::now() + 10s;
    while (!game.ai.entered() && std::chrono::steady_clock::now() < deadline) {
        auto m = c.read(100ms);
        if (!m || (*m)["type"] != "decision" || (*m)["player"] != "P1") continue;
        const uint64_t id = (*m)["decision_id"].get<uint64_t>();
        if (id <= last) continue;
        last = id;
        c.send({{"type", "choose"}, {"player", "P1"}, {"index", 0}, {"decision_id", id}});
    }
    ASSERT_TRUE(game.ai.entered()) << "seat 2 was never asked";

    // Engine thread is mid-decision for the AI. An edit must be refused…
    c.send({{"type", "edit_player"}, {"player", "P1"}, {"field", "score"}, {"value", 5}});
    json reply = c.readUntil([](const json& m) {
        return m["type"] == "edit_err" || m["type"] == "edit_ok";
    });
    EXPECT_EQ(reply["type"], "edit_err");
    EXPECT_NE(reply["msg"].get<std::string>().find("busy"), std::string::npos);
    // (reading the engine's state here is safe: its thread is held in the gate)
    EXPECT_EQ(game.engine.state().player(PlayerId::Player1).score, 0);

    // …and a state request is answered from the last snapshot, with nothing to click.
    c.send({{"type", "request_state"}});
    json busy = c.readUntil([](const json& m) { return m["type"] == "state"; });
    EXPECT_TRUE(busy["engine_busy"].get<bool>());
    EXPECT_TRUE(busy["legal"].empty());
    // A choose now is refused too: no decision is pending.
    c.send({{"type", "choose"}, {"player", "P1"}, {"index", 0}, {"decision_id", last}});
    EXPECT_EQ(c.nextReply()["type"], "err");

    // Once the engine is parked on P1 again the same edit applies.
    game.ai.release();
    json d = c.nextDecision("P1", last);
    ASSERT_FALSE(d.is_null());
    EXPECT_FALSE(d["engine_busy"].get<bool>());
    c.send({{"type", "edit_player"}, {"player", "P1"}, {"field", "score"}, {"value", 5}});
    reply = c.readUntil([](const json& m) {
        return m["type"] == "edit_err" || m["type"] == "edit_ok";
    });
    EXPECT_EQ(reply["type"], "edit_ok");
    json after = c.readUntil([](const json& m) {
        return m["type"] == "decision" && m["zones"]["P1"]["score"] == 5;
    });
    ASSERT_FALSE(after.is_null());
    EXPECT_EQ(after["decision_id"], d["decision_id"]);
}

// ─── 4.4 — cross-site WebSocket hijacking ────────────────────────────────────

TEST(PlayServerProtocol, UpgradeChecksOriginHostAndPath) {
    LiveGame game;
    const uint16_t port = game.port();
    const std::string self = "127.0.0.1:" + std::to_string(port);

    { Client c; EXPECT_TRUE(c.connect(port)) << "no Origin (non-browser client)"; }
    { Client c; EXPECT_TRUE(c.connect(port, "/ws", "http://" + self)) << "the served page"; }
    { Client c; EXPECT_TRUE(c.connect(port, "/ws", "http://localhost:" + std::to_string(port),
                                      "localhost:" + std::to_string(port))); }
    { Client c; EXPECT_FALSE(c.connect(port, "/ws", "http://evil.example"))
          << "another site's page must not be able to connect"; }
    { Client c; EXPECT_FALSE(c.connect(port, "/ws", "https://" + self)) << "different scheme"; }
    { Client c; EXPECT_FALSE(c.connect(port, "/ws", "http://evil.example:" + std::to_string(port),
                                       "evil.example:" + std::to_string(port)))
          << "DNS rebinding: Origin matches Host, but Host is not this machine"; }
    { Client c; EXPECT_FALSE(c.connect(port, "/elsewhere")) << "only /ws upgrades"; }
}

TEST(PlayServerProtocol, AccessTokenIsRequiredWhenConfigured) {
    LiveGame game([](PlayServerConfig& cfg) { cfg.access_token = "s3cret"; });
    const uint16_t port = game.port();

    { Client c; EXPECT_FALSE(c.connect(port, "/ws")); }
    { Client c; EXPECT_FALSE(c.connect(port, "/ws?token=wrong")); }
    { Client c; EXPECT_FALSE(c.connect(port, "/ws?token=s3cretX")); }
    { Client c; EXPECT_TRUE(c.connect(port, "/ws?token=s3cret")); }

    auto get = [&](const std::string& target) {
        net::io_context ioc;
        tcp::socket sock(ioc);
        sock.connect(tcp::endpoint{net::ip::make_address("127.0.0.1"), port});
        http::request<http::string_body> req{http::verb::get, target, 11};
        req.set(http::field::host, "127.0.0.1:" + std::to_string(port));
        http::write(sock, req);
        beast::flat_buffer buf;
        http::response<http::string_body> res;
        http::read(sock, buf, res);
        return res;
    };
    EXPECT_EQ(get("/").result(), http::status::forbidden);
    auto ok = get("/?token=s3cret");
    EXPECT_EQ(ok.result(), http::status::ok);
    EXPECT_NE(ok.body().find("<title>Riftbound"), std::string::npos);
}

// ─── 4.13 — a busy port is an exception the caller can report ───────────────

TEST(PlayServerProtocol, StartOnABusyPortThrowsAndLeavesTheServerStopped) {
    LiveGame game;
    PlayServerConfig cfg;
    cfg.port = game.port();   // taken
    PlayServer second(game.engine, game.renderer, cards().db, std::move(cfg));
    EXPECT_THROW(second.start(), std::exception);
    second.stop();            // no-op: nothing was started
    // The first server is unaffected.
    Client c;
    EXPECT_TRUE(c.connect(game.port()));
}

// ─── 4.5 — a frame carries only what its viewer may see──────────────────────

TEST(PlayServerProtocol, FairPlayFramesWithholdOpponentHandAndDeckOrder) {
    LiveGame game([](PlayServerConfig& cfg) {
        cfg.god_mode_enabled = false;
        cfg.reveal_hidden = false;
    });
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    json f = playUntil(c, [](const json& m) {
        return m["zones"]["P1"]["hand_count"].get<int>() > 0 &&
               m["zones"]["P2"]["hand_count"].get<int>() > 0;
    });
    ASSERT_FALSE(f.is_null());

    const json& p1 = f["zones"]["P1"];
    const json& p2 = f["zones"]["P2"];
    // Own hand: all of it. Opponent's: a count.
    EXPECT_EQ(static_cast<int>(p1["hand"].size()), p1["hand_count"].get<int>());
    EXPECT_TRUE(p2["hand"].empty());
    // Deck order is hidden from both players.
    for (const json* z : {&p1, &p2}) {
        EXPECT_TRUE((*z)["main_deck"].empty());
        EXPECT_TRUE((*z)["rune_deck"].empty());
        EXPECT_GT((*z)["main_deck_count"].get<int>(), 0);
    }
    // The ASCII board follows the same rule: exactly one hand is replaced by a count.
    const std::string board = f["render_text"].get<std::string>();
    EXPECT_EQ(countOf(board, "cards, hidden)") + countOf(board, "card, hidden)"), 1);
    // No name from the opponent's hand appears as a hand entry. (P1 is
    // parked on this decision, so GameState is stable.)
    const auto& st = game.engine.state();
    const auto p2_section = board.substr(board.rfind(" P2  "));
    for (auto id : st.player(PlayerId::Player2).hand) {
        const std::string& name = st.getObject(id).name;
        EXPECT_EQ(p2_section.find(" Hand:  " + name), std::string::npos) << name;
    }
    // By now cards were drawn and mulligans taken, and the engine logged
    // every one by name. The human in seat 1 gets the lines seat 1 may read
    // (LogEvent::audience) and none of the others: no P2 draw, no line for
    // neither player.
    game.server->broadcastLog("TRC", "sweep-marker: public", LogAudience::both());
    game.server->broadcastLog("TRC", "sweep-marker: P1 only", LogAudience::only(PlayerId::Player1));
    game.server->broadcastLog("TRC", "sweep-marker: P2 only", LogAudience::only(PlayerId::Player2));
    game.server->broadcastLog("TRC", "sweep-marker: nobody", LogAudience::neither());
    game.server->broadcastLog("TRC", "sweep-marker: untagged");
    c.send({{"type", "request_state"}});
    std::vector<std::string> received;
    while (auto m = c.read(300ms)) {
        if ((*m)["type"] == "log") received.push_back((*m)["msg"].get<std::string>());
    }
    auto got = [&](const std::string& text) {
        return std::find(received.begin(), received.end(), text) != received.end();
    };
    EXPECT_TRUE(got("sweep-marker: public"));
    EXPECT_TRUE(got("sweep-marker: P1 only"));
    EXPECT_FALSE(got("sweep-marker: P2 only"));
    EXPECT_FALSE(got("sweep-marker: nobody"));
    EXPECT_FALSE(got("sweep-marker: untagged")) << "no tag: for an all-seeing viewer only";
    {
        std::lock_guard<std::mutex> lock(game.log_mu);
        EXPECT_FALSE(game.not_for_p1.empty()) << "P2 drew its hand: the engine logged it privately";
        for (const auto& line : game.not_for_p1) EXPECT_FALSE(got(line)) << line;
    }
    EXPECT_GT(game.trace_lines.load(), 0) << "the engine did log during this game";
    // God-mode edits stay off.
    c.send({{"type", "edit_player"}, {"player", "P1"}, {"field", "score"}, {"value", 7}});
    EXPECT_EQ(c.nextReply()["type"], "edit_err");
}

TEST(PlayServerProtocol, RevealHiddenFramesCarryEverything) {
    LiveGame game;   // reveal_hidden = true (god mode / spectator)
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    json f = playUntil(c, [](const json& m) {
        return m["zones"]["P2"]["hand_count"].get<int>() > 0;
    });
    ASSERT_FALSE(f.is_null());
    const json& p2 = f["zones"]["P2"];
    EXPECT_EQ(static_cast<int>(p2["hand"].size()), p2["hand_count"].get<int>());
    EXPECT_EQ(static_cast<int>(p2["main_deck"].size()), p2["main_deck_count"].get<int>());
    EXPECT_EQ(f["render_text"].get<std::string>().find("hidden)"), std::string::npos);
    // With everything revealed the log stream is on: answering the
    // decision makes the engine log what it does next.
    c.send({{"type", "choose"}, {"player", "P1"}, {"index", 0},
            {"decision_id", f["decision_id"]}});
    EXPECT_FALSE(c.readUntil([](const json& m) { return m["type"] == "log"; }).is_null());
}

// ─── 4.6 — spectator stepping, and the terminal frame ────────────────────────

TEST(PlayServerProtocol, SpectatorStepsAdvanceTheBoardOneDecisionAtATime) {
    LiveGame game({}, /*spectator=*/true);
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    auto held = [](const json& m) {
        return m["type"] == "decision" && m["spectator"]["waiting"].get<bool>();
    };
    json d1 = c.readUntil(held);
    ASSERT_FALSE(d1.is_null());
    const uint64_t id1 = d1["decision_id"].get<uint64_t>();
    EXPECT_FALSE(d1["render_text"].get<std::string>().empty());
    EXPECT_GE(d1["spectator"]["chosen"].get<int>(), 0);
    EXPECT_LT(d1["spectator"]["chosen"].get<int>(), static_cast<int>(d1["legal"].size()));

    // Nothing moves until the viewer steps.
    c.send({{"type", "request_state"}});
    EXPECT_EQ(c.readUntil(held)["decision_id"].get<uint64_t>(), id1);

    c.send({{"type", "spectator_step"}});
    json d2 = c.readUntil([&](const json& m) {
        return held(m) && m["decision_id"].get<uint64_t>() > id1;
    });
    ASSERT_FALSE(d2.is_null());
    EXPECT_EQ(d2["decision_id"].get<uint64_t>(), id1 + 1);

    // Autoplay keeps the frames coming without further clicks.
    c.send({{"type", "spectator_autoplay"}, {"on", true}, {"delay_ms", 0}});
    json later = c.readUntil([&](const json& m) {
        return m["type"] == "decision" && m["decision_id"].get<uint64_t>() >= id1 + 5;
    });
    EXPECT_FALSE(later.is_null());
}

TEST(PlayServerProtocol, TerminalFrameIsDeliveredBeforeTheServerStops) {
    LiveGame game;
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    ASSERT_FALSE(c.nextDecision("P1").is_null());

    game.finishGame();                    // P1 concedes; engine thread is done
    game.server->notifyGameFinished();
    game.server->stop();                  // returns only once the frame is written

    json last;
    while (auto m = c.read(500ms)) {
        if ((*m)["type"] == "state" || (*m)["type"] == "decision") last = *m;
    }
    ASSERT_FALSE(last.is_null());
    EXPECT_TRUE(last["game_over"].get<bool>());
    EXPECT_EQ(last["type"], "state");
    EXPECT_TRUE(last["legal"].empty());
    // The aborted seat is the one that concedes, whoever's turn it was.
    EXPECT_EQ(last.value("winner", ""), "P2");
}

// ─── 4.7 — a prompt heads the decision it was asked for, and only that one ───

// ─── plans/09 9.PP2: seat-bound sessions ────────────────────────────────────
// Two human seats, hidden information withheld, PlayServerConfig::seat_bound.
// A client takes a seat or is a spectator; frames and log lines are per seat;
// only the seat being asked may answer.

namespace {

struct SeatedGame {
    EventBus events;
    GameEngine engine{cards().db, events, cards().registry};
    StateRenderer renderer{cards().db};
    HumanAgent human1{PlayerId::Player1};
    HumanAgent human2{PlayerId::Player2};
    std::unique_ptr<PlayServer> server;
    std::thread game_thread;
    std::mutex log_mu;
    std::vector<std::pair<std::string, LogAudience>> logged;   // every engine line, with who may read it

    explicit SeatedGame(bool seat_bound = true) {
        renderer.show_hand = true;
        PlayServerConfig cfg;
        cfg.port = 0;
        cfg.god_mode_enabled = false;
        cfg.reveal_hidden = false;
        cfg.p1_human = &human1;
        cfg.p2_human = &human2;
        cfg.seat_bound = seat_bound;
        cfg.on_shutdown_signal = [this] { human1.abort(); human2.abort(); };
        server = std::make_unique<PlayServer>(engine, renderer, cards().db, std::move(cfg));
        server->start();
        events.on_log.connect([this](const LogEvent& e) {
            {
                std::lock_guard<std::mutex> lock(log_mu);
                logged.emplace_back(e.message, e.audience);
            }
            server->broadcastLog(e.level == LogLevel::Trace ? "TRC" : "DBG", e.message,
                                 e.audience);
        });
        engine.on_decision = [this](const GameState&, const std::vector<Intent>&, const Intent&) {
            server->notifyDecisionMade();
        };
        game_thread = std::thread([this] {
            engine.runGame(cards().deck1, cards().deck2, human1, human2, 20261010);
        });
    }
    uint16_t port() const { return server->port(); }
    /// True if some logged line with this text may be read by `who` (both
    /// players for PlayerId::None).
    bool mayRead(const std::string& text, PlayerId who) {
        std::lock_guard<std::mutex> lock(log_mu);
        for (const auto& [msg, audience] : logged)
            if (msg == text && (who == PlayerId::None ? audience.isBoth() : audience.includes(who)))
                return true;
        return false;
    }
    /// True if every logged line with this text is for `who` alone.
    bool onlyFor(const std::string& text, PlayerId who) {
        std::lock_guard<std::mutex> lock(log_mu);
        bool any = false;
        for (const auto& [msg, audience] : logged) {
            if (msg != text) continue;
            any = true;
            if (!audience.includes(who) || audience.isBoth()) return false;
        }
        return any;
    }
    ~SeatedGame() {
        server->requestShutdown();
        if (game_thread.joinable()) game_thread.join();
        server->stop();
    }
};

/// One connected client of a SeatedGame and what it has been sent.
struct SeatClient {
    Client c;
    std::string seat;                       // "P1", "P2" or "" (spectator)
    std::vector<std::string> log;
    json last_frame;
    uint64_t answered = 0;

    /// Connects and, for a seat, takes it. Returns the server's reply (null
    /// for a spectator).
    json join(uint16_t port, const std::string& want) {
        EXPECT_TRUE(c.connect(port));
        if (want.empty()) return json();
        c.send({{"type", "seat"}, {"player", want}});
        json reply = c.readUntil([](const json& m) { return m["type"] == "seat"; });
        if (!reply.is_null() && reply["ok"].get<bool>()) seat = want;
        return reply;
    }
    /// Reads what has arrived; answers its own seat's decision with option 0
    /// when `play`. Returns true if anything was read.
    bool pump(bool play, std::chrono::milliseconds wait = 30ms) {
        bool any = false;
        while (auto m = c.read(wait)) {
            any = true;
            const std::string type = (*m)["type"].get<std::string>();
            if (type == "log") log.push_back((*m)["msg"].get<std::string>());
            if (type != "decision" && type != "state") continue;
            last_frame = *m;
            if (!play || type != "decision" || seat.empty()) continue;
            if ((*m)["player"] != seat || (*m)["legal"].empty()) continue;
            const uint64_t id = (*m)["decision_id"].get<uint64_t>();
            if (id == answered) continue;
            answered = id;
            c.send({{"type", "choose"}, {"player", seat}, {"index", 0}, {"decision_id", id}});
        }
        return any;
    }
};

}  // namespace

TEST(PlayServerSeats, EachSeatGetsItsOwnPrivateLinesAndASpectatorNeither) {
    SeatedGame game;
    SeatClient p1, p2, watcher;
    ASSERT_TRUE(p1.join(game.port(), "P1")["ok"].get<bool>());
    ASSERT_TRUE(p2.join(game.port(), "P2")["ok"].get<bool>());
    watcher.join(game.port(), "");

    // Play (option 0 for whoever is asked) until each seat has been sent a
    // line that is for it alone — the engine's draws ("DREW: <card>").
    auto hasPrivate = [&](SeatClient& who, PlayerId seat) {
        for (const auto& line : who.log) if (game.onlyFor(line, seat)) return true;
        return false;
    };
    const auto deadline = std::chrono::steady_clock::now() + 20s;
    while (std::chrono::steady_clock::now() < deadline &&
           !(hasPrivate(p1, PlayerId::Player1) && hasPrivate(p2, PlayerId::Player2))) {
        p1.pump(true);
        p2.pump(true);
        watcher.pump(false);
    }
    ASSERT_TRUE(hasPrivate(p1, PlayerId::Player1)) << "P1 was sent no private line";
    ASSERT_TRUE(hasPrivate(p2, PlayerId::Player2)) << "P2 was sent no private line";

    // A Void Hatchling look: the line EffectExecutor::lookBeforeReveal writes
    // for the looker (RIFT_TRACE_PRIVATE), handed to the server as the
    // engine's log hook would.
    const std::string peek = "  PEEKED: Immortal Phoenix (id=7) on top of the Main Deck [Void Hatchling]";
    game.server->broadcastLog("TRC", peek, LogAudience::only(PlayerId::Player1));
    game.server->broadcastLog("TRC", "seat-marker: public", LogAudience::both());
    game.server->broadcastLog("TRC", "seat-marker: nobody", LogAudience::neither());
    for (int i = 0; i < 10; ++i) { p1.pump(false, 50ms); p2.pump(false, 50ms); watcher.pump(false, 50ms); }
    auto got = [](const SeatClient& who, const std::string& text) {
        return std::find(who.log.begin(), who.log.end(), text) != who.log.end();
    };
    EXPECT_TRUE(got(p1, peek)) << "the looker reads what they looked at";
    EXPECT_FALSE(got(p2, peek));
    EXPECT_FALSE(got(watcher, peek));
    for (SeatClient* who : {&p1, &p2, &watcher}) {
        EXPECT_TRUE(got(*who, "seat-marker: public"));
        EXPECT_FALSE(got(*who, "seat-marker: nobody"));
    }

    // Everything each client was sent, it may read; and no client was sent a
    // line that is for the other seat alone.
    auto check = [&](SeatClient& who, PlayerId seat, PlayerId other) {
        int draws = 0;
        for (const auto& line : who.log) {
            if (line == peek || line.rfind("seat-marker", 0) == 0) continue;
            EXPECT_TRUE(game.mayRead(line, seat)) << who.seat << " was sent: " << line;
            if (other != PlayerId::None) {
                EXPECT_FALSE(game.onlyFor(line, other)) << who.seat << " was sent: " << line;
            }
            if (line.find("DREW: ") != std::string::npos) ++draws;
        }
        return draws;
    };
    EXPECT_GT(check(p1, PlayerId::Player1, PlayerId::Player2), 0) << "P1's own draws";
    EXPECT_GT(check(p2, PlayerId::Player2, PlayerId::Player1), 0) << "P2's own draws";
    EXPECT_EQ(check(watcher, PlayerId::None, PlayerId::None), 0) << "a spectator sees no draw by name";
    for (const auto& line : watcher.log) {
        EXPECT_FALSE(game.onlyFor(line, PlayerId::Player1)) << line;
        EXPECT_FALSE(game.onlyFor(line, PlayerId::Player2)) << line;
    }
    EXPECT_FALSE(watcher.log.empty()) << "public lines do reach a spectator";
}

TEST(PlayServerSeats, FramesAreBuiltPerSeat) {
    SeatedGame game;
    SeatClient p1, p2, watcher;
    ASSERT_TRUE(p1.join(game.port(), "P1")["ok"].get<bool>());
    ASSERT_TRUE(p2.join(game.port(), "P2")["ok"].get<bool>());
    watcher.join(game.port(), "");
    // Until both players hold cards and someone is being asked.
    auto ready = [](const json& f) {
        return !f.is_null() && f["type"] == "decision" &&
               f["zones"]["P1"]["hand_count"].get<int>() > 0 &&
               f["zones"]["P2"]["hand_count"].get<int>() > 0;
    };
    const auto deadline = std::chrono::steady_clock::now() + 20s;
    auto answer = [](SeatClient& who) {
        const json& f = who.last_frame;
        if (f.is_null() || f["type"] != "decision" || f["player"] != who.seat || f["legal"].empty())
            return;
        const uint64_t id = f["decision_id"].get<uint64_t>();
        if (id == who.answered) return;
        who.answered = id;
        who.c.send({{"type", "choose"}, {"player", who.seat}, {"index", 0}, {"decision_id", id}});
    };
    while (std::chrono::steady_clock::now() < deadline) {
        // Look before answering: the three frames must be of one decision.
        for (int i = 0; i < 3; ++i) { p1.pump(false); p2.pump(false); watcher.pump(false); }
        if (ready(p1.last_frame) && ready(p2.last_frame) && ready(watcher.last_frame) &&
            p1.last_frame["decision_id"] == p2.last_frame["decision_id"] &&
            p1.last_frame["decision_id"] == watcher.last_frame["decision_id"] &&
            p1.last_frame["decision_id"].get<uint64_t>() != p1.answered &&
            p1.last_frame["decision_id"].get<uint64_t>() != p2.answered)
            break;
        answer(p1);
        answer(p2);
    }
    ASSERT_TRUE(ready(p1.last_frame) && ready(p2.last_frame) && ready(watcher.last_frame));
    const json &f1 = p1.last_frame, &f2 = p2.last_frame, &fw = watcher.last_frame;
    EXPECT_EQ(f1["your_seat"], "P1");
    EXPECT_EQ(f2["your_seat"], "P2");
    EXPECT_EQ(fw["your_seat"], "");
    auto handShown = [](const json& f, const char* zone) {
        return static_cast<int>(f["zones"][zone]["hand"].size()) ==
               f["zones"][zone]["hand_count"].get<int>();
    };
    EXPECT_TRUE(handShown(f1, "P1"));
    EXPECT_TRUE(f1["zones"]["P2"]["hand"].empty());
    EXPECT_TRUE(handShown(f2, "P2"));
    EXPECT_TRUE(f2["zones"]["P1"]["hand"].empty());
    EXPECT_TRUE(fw["zones"]["P1"]["hand"].empty());
    EXPECT_TRUE(fw["zones"]["P2"]["hand"].empty());
    for (const json* f : {&f1, &f2, &fw})
        for (const char* zone : {"P1", "P2"}) {
            EXPECT_TRUE((*f)["zones"][zone]["main_deck"].empty());
            EXPECT_TRUE((*f)["zones"][zone]["rune_deck"].empty());
        }
    // The options go to the seat that is asked, and to nobody else.
    const std::string asked = f1["player"].get<std::string>();
    ASSERT_TRUE(asked == "P1" || asked == "P2");
    const json& mine = asked == "P1" ? f1 : f2;
    const json& theirs = asked == "P1" ? f2 : f1;
    EXPECT_FALSE(mine["legal"].empty());
    EXPECT_TRUE(theirs["legal"].empty());
    EXPECT_TRUE(fw["legal"].empty());
    EXPECT_FALSE(theirs.contains("prompt"));
    EXPECT_FALSE(fw.contains("prompt"));
    // The text board follows the same rule.
    auto hiddenHands = [](const json& f) {
        const std::string board = f["render_text"].get<std::string>();
        return countOf(board, "cards, hidden)") + countOf(board, "card, hidden)");
    };
    EXPECT_EQ(hiddenHands(f1), 1);
    EXPECT_EQ(hiddenHands(f2), 1);
    EXPECT_EQ(hiddenHands(fw), 2);
}

TEST(PlayServerSeats, OnlyTheSeatBeingAskedMayAnswerAndASeatIsHeldByOneClient) {
    SeatedGame game;
    SeatClient p1, p2, watcher, late;
    ASSERT_TRUE(p1.join(game.port(), "P1")["ok"].get<bool>());
    ASSERT_TRUE(p2.join(game.port(), "P2")["ok"].get<bool>());
    watcher.join(game.port(), "");

    // A taken seat cannot be taken again; a seat that does not exist, never.
    json taken = late.join(game.port(), "P1");
    ASSERT_FALSE(taken.is_null());
    EXPECT_FALSE(taken["ok"].get<bool>());
    EXPECT_NE(taken["msg"].get<std::string>().find("taken"), std::string::npos);
    late.c.send({{"type", "seat"}, {"player", "P3"}});
    json none = late.c.readUntil([](const json& m) { return m["type"] == "seat"; });
    EXPECT_FALSE(none["ok"].get<bool>());

    // The first decision: find who is asked.
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    auto asked_frame = [&]() -> const json* {
        for (SeatClient* who : {&p1, &p2})
            if (!who->last_frame.is_null() && who->last_frame["type"] == "decision" &&
                who->last_frame["player"] == who->seat && !who->last_frame["legal"].empty())
                return &who->last_frame;
        return nullptr;
    };
    while (std::chrono::steady_clock::now() < deadline && !asked_frame()) {
        p1.pump(false); p2.pump(false); watcher.pump(false);
    }
    const json* f = asked_frame();
    ASSERT_NE(f, nullptr);
    const std::string asked = (*f)["player"].get<std::string>();
    const uint64_t id = (*f)["decision_id"].get<uint64_t>();
    SeatClient& right = asked == "P1" ? p1 : p2;
    SeatClient& wrong = asked == "P1" ? p2 : p1;
    const json choose = {{"type", "choose"}, {"player", asked}, {"index", 0}, {"decision_id", id}};

    wrong.c.send(choose);
    json refused = wrong.c.readUntil([](const json& m) { return m["type"] == "err"; });
    EXPECT_NE(refused["msg"].get<std::string>().find("seated as " + wrong.seat), std::string::npos)
        << refused.dump();
    watcher.c.send(choose);
    refused = watcher.c.readUntil([](const json& m) { return m["type"] == "err"; });
    EXPECT_NE(refused["msg"].get<std::string>().find("spectator"), std::string::npos)
        << refused.dump();
    late.c.send(choose);
    refused = late.c.readUntil([](const json& m) { return m["type"] == "err"; });
    EXPECT_NE(refused["msg"].get<std::string>().find("spectator"), std::string::npos);
    // Nothing was answered: the same decision is still pending ...
    right.c.send({{"type", "request_state"}});
    json still = right.c.readUntil([&](const json& m) {
        return m["type"] == "decision" && m["player"] == asked;
    });
    EXPECT_EQ(still["decision_id"].get<uint64_t>(), id);
    // ... and the seat that is asked answers it.
    right.c.send(choose);
    json next = right.c.readUntil([&](const json& m) {
        return (m["type"] == "decision" || m["type"] == "state") &&
               m["decision_id"].get<uint64_t>() != id;
    });
    EXPECT_FALSE(next.is_null());

    // A seat is released when its client disconnects.
    late.c.send({{"type", "seat"}, {"player", wrong.seat}});
    json busy = late.c.readUntil([](const json& m) { return m["type"] == "seat"; });
    EXPECT_FALSE(busy["ok"].get<bool>());
}

// Not seat-bound (one human seat, or two with --hot-seat): nothing changes.
// A frame has no `your_seat`, a `seat` request is refused, and any client
// answers — what the tests above this section have always exercised.
TEST(PlayServerSeats, Pin_AServerThatIsNotSeatBoundBehavesAsBefore) {
    LiveGame game([](PlayServerConfig& cfg) {
        cfg.god_mode_enabled = false;
        cfg.reveal_hidden = false;
    });
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    json f = c.nextDecision("P1");
    ASSERT_FALSE(f.is_null());
    EXPECT_FALSE(f.contains("your_seat"));
    EXPECT_FALSE(f["legal"].empty());
    c.send({{"type", "seat"}, {"player", "P1"}});
    json reply = c.readUntil([](const json& m) { return m["type"] == "seat"; });
    EXPECT_FALSE(reply["ok"].get<bool>());
    // An unseated client answers, as ever.
    const uint64_t id = f["decision_id"].get<uint64_t>();
    c.send({{"type", "choose"}, {"player", "P1"}, {"index", 0}, {"decision_id", id}});
    json next = c.nextDecision("P1", id);
    EXPECT_FALSE(next.is_null());
}

// Two human seats on one screen (--hot-seat: not seat-bound): one stream,
// the hand of whoever is deciding — the frames the server built before.
TEST(PlayServerSeats, Pin_HotSeatSharesOneStream) {
    SeatedGame game(/*seat_bound=*/false);
    Client a, b;
    ASSERT_TRUE(a.connect(game.port()));
    ASSERT_TRUE(b.connect(game.port()));
    a.send({{"type", "request_state"}});
    auto decision = [](const json& m) { return m["type"] == "decision" && !m["legal"].empty(); };
    json fa = a.readUntil(decision);
    json fb = b.readUntil([&](const json& m) {
        return decision(m) && m["decision_id"] == fa["decision_id"];
    });
    ASSERT_FALSE(fa.is_null());
    ASSERT_FALSE(fb.is_null());
    EXPECT_EQ(fa.dump(), fb.dump()) << "every client is sent the same frame";
    EXPECT_FALSE(fa.contains("your_seat"));
}

TEST(PlayServerProtocol, PromptStaysWithItsDecision) {
    LiveGame game;
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    json d1 = c.nextDecision("P1");
    ASSERT_FALSE(d1.is_null());
    const uint64_t id1 = d1["decision_id"].get<uint64_t>();
    EXPECT_FALSE(d1.contains("prompt"));

    // The engine logs the prompt right before asking; here it is P1's.
    game.server->broadcastLog("TRC", "MODE_PROMPT: Choose a mode [Draw | Burn]");
    auto frame = [&] {
        c.send({{"type", "request_state"}});
        return c.nextDecision("P1");
    };
    EXPECT_EQ(frame().value("prompt", ""), "Choose a mode");
    // Every frame of that decision carries it (a reconnect, a re-render)…
    EXPECT_EQ(frame().value("prompt", ""), "Choose a mode");

    // …and once the decision is answered it is gone.
    c.send({{"type", "choose"}, {"player", "P1"}, {"index", 0}, {"decision_id", id1}});
    json d2 = c.nextDecision("P1", id1);
    ASSERT_FALSE(d2.is_null());
    EXPECT_FALSE(d2.contains("prompt"));

    // A prompt asked of the other seat (answered by the AI, which the server
    // only hears about through notifyDecisionMade) never reaches P1's frame.
    game.server->broadcastLog("TRC", "MAY_PROMPT: Pay 1 to draw?");
    game.server->notifyDecisionMade();
    EXPECT_FALSE(frame().contains("prompt"));
}

// ─── 4.8 — god-mode moves keep the state consistent ──────────────────────────

namespace {

GameState editorState() {
    GameState s{};
    BattlefieldState bf;
    bf.id = BattlefieldId::fromRaw(0);
    s.battlefields.push_back(bf);
    auto add = [&](GameObjectId id, PlayerId who, ZoneType zone) -> GameObject& {
        GameObject o{};
        o.id = id;
        o.owner = who;
        o.controller = who;
        o.zone = zone;
        return s.objects[id] = o;
    };
    // 1: P2's unit at the battlefield, carrying gear 3.
    auto& unit = add(1, PlayerId::Player2, ZoneType::BattlefieldZone);
    unit.location = BattlefieldLocation{0};
    unit.attachments = {3};
    auto& gear = add(3, PlayerId::Player2, ZoneType::BattlefieldZone);
    gear.location = BattlefieldLocation{0};
    gear.attached_to = 1;
    // 2: P1's facedown card at the battlefield.
    auto& hidden = add(2, PlayerId::Player1, ZoneType::FacedownZone);
    hidden.is_hidden = true;
    s.battlefields[0].facedown.push_back(2);
    // 4: a spell on the chain.
    add(4, PlayerId::Player1, ZoneType::Chain);
    // 5: a card in P1's hand.
    add(5, PlayerId::Player1, ZoneType::Hand);
    s.player(PlayerId::Player1).hand.push_back(5);
    return s;
}

bool contains(const std::vector<GameObjectId>& v, GameObjectId id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}

} // namespace

TEST(StateEditorReviewFix, MovingAFacedownCardTakesItOffTheBattlefield) {
    GameState s = editorState();
    StateEditor ed;
    ASSERT_TRUE(ed.moveObject(s, 2, PlayerId::Player1, ZoneType::Trash).ok);
    EXPECT_TRUE(s.battlefields[0].facedown.empty());   // was left behind as an orphan
    EXPECT_FALSE(s.getObject(2).is_hidden);
    EXPECT_TRUE(contains(s.player(PlayerId::Player1).trash, 2));
}

TEST(StateEditorReviewFix, MovingAUnitOffBoardDropsItsGearAndKeepsOwnership) {
    GameState s = editorState();
    StateEditor ed;
    // The UI sends the unit's own owner, not the battlefield's controller.
    ASSERT_TRUE(ed.moveObject(s, 1, PlayerId::Player2, ZoneType::Hand).ok);
    const auto& unit = s.getObject(1);
    EXPECT_EQ(unit.owner, PlayerId::Player2);
    EXPECT_EQ(unit.controller, PlayerId::Player2);
    EXPECT_TRUE(contains(s.player(PlayerId::Player2).hand, 1));
    EXPECT_FALSE(contains(s.player(PlayerId::Player1).hand, 1));
    EXPECT_TRUE(unit.attachments.empty());
    const auto& gear = s.getObject(3);
    EXPECT_FALSE(gear.attached_to.has_value());
    EXPECT_EQ(gear.zone, ZoneType::Base);
    ASSERT_TRUE(gear.location.has_value());
    EXPECT_EQ(*gear.location, LocationId{BaseLocation{PlayerId::Player2}});
}

TEST(StateEditorReviewFix, GivingACardToTheOtherPlayerChangesOwnerAndController) {
    GameState s = editorState();
    StateEditor ed;
    ASSERT_TRUE(ed.moveObject(s, 5, PlayerId::Player2, ZoneType::Hand).ok);
    EXPECT_EQ(s.getObject(5).owner, PlayerId::Player2);
    EXPECT_EQ(s.getObject(5).controller, PlayerId::Player2);
    EXPECT_FALSE(contains(s.player(PlayerId::Player1).hand, 5));
    EXPECT_TRUE(contains(s.player(PlayerId::Player2).hand, 5));
}

TEST(StateEditorReviewFix, MovingAttachedGearDetachesItFromItsUnit) {
    GameState s = editorState();
    StateEditor ed;
    ASSERT_TRUE(ed.moveObject(s, 3, PlayerId::Player2, ZoneType::Trash).ok);
    EXPECT_TRUE(s.getObject(1).attachments.empty());
    EXPECT_FALSE(s.getObject(3).attached_to.has_value());
}

TEST(StateEditorReviewFix, ChainAndFacedownMovesAreRefused) {
    GameState s = editorState();
    StateEditor ed;
    // To a zone that needs a container entry the editor cannot build.
    EXPECT_FALSE(ed.moveObject(s, 5, PlayerId::Player1, ZoneType::Chain).ok);
    EXPECT_FALSE(ed.moveObject(s, 5, PlayerId::Player1, ZoneType::FacedownZone).ok);
    EXPECT_EQ(s.getObject(5).zone, ZoneType::Hand);
    EXPECT_TRUE(contains(s.player(PlayerId::Player1).hand, 5));
    // Out of the chain, which would keep an item pointing at it.
    EXPECT_FALSE(ed.moveObject(s, 4, PlayerId::Player1, ZoneType::Hand).ok);
    EXPECT_FALSE(ed.moveObjectToBattlefield(s, 4, BattlefieldId::fromRaw(0)).ok);
    EXPECT_EQ(s.getObject(4).zone, ZoneType::Chain);
}

TEST(StateEditorReviewFix, SetPhaseRejectsValuesOutsideTheEnum) {
    GameState s = editorState();
    StateEditor ed;
    EXPECT_FALSE(ed.setPhase(s, -1).ok);
    EXPECT_FALSE(ed.setPhase(s, static_cast<int>(TurnPhase::GameOver) + 1).ok);
    EXPECT_FALSE(ed.setPhase(s, 300).ok);   // would wrap into range as a uint8
    EXPECT_TRUE(ed.setPhase(s, static_cast<int>(TurnPhase::MainPhase)).ok);
    EXPECT_EQ(s.turn.phase, TurnPhase::MainPhase);
}

TEST(PlayServerProtocol, UnknownZoneIsAnErrorNotAMoveToHand) {
    LiveGame game;
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    json f = playUntil(c, [](const json& m) {
        return m["zones"]["P1"]["main_deck_count"].get<int>() > 0;
    });
    ASSERT_FALSE(f.is_null());
    const int card = f["zones"]["P1"]["main_deck"][0]["id"].get<int>();
    const int hand_before = f["zones"]["P1"]["hand_count"].get<int>();
    c.send({{"type", "edit_move"}, {"object_id", card}, {"to_player", "P1"}, {"to_zone", "Graveyard"}});
    json reply = c.nextReply();
    EXPECT_EQ(reply["type"], "edit_err");
    EXPECT_EQ(c.nextDecision("P1")["zones"]["P1"]["hand_count"].get<int>(), hand_before);
    // Objects in a frame say whose they are, so the UI need not guess.
    EXPECT_EQ(f["zones"]["P1"]["main_deck"][0]["owner"], "P1");
}

// ─── 4.9 — the ASCII board lists every unit and every option in full ─────────

TEST(StateRendererReviewFix, CrowdedBattlefieldListsEveryUnit) {
    LiveGame game;
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    json f = playUntil(c, [](const json& m) {
        return !m["battlefields"].empty() &&
               m["zones"]["P1"]["main_deck_count"].get<int>() > 10;
    });
    ASSERT_FALSE(f.is_null());

    // P1 is parked: put six of its units on one battlefield.
    std::lock_guard<std::mutex> lock(game.server->stateMutex());
    auto& st = game.engine.mutableState();
    const BattlefieldId bf = st.battlefields[0].id;
    StateEditor ed;
    std::vector<GameObjectId> moved;
    for (auto id : std::vector<GameObjectId>(st.player(PlayerId::Player1).main_deck)) {
        if (!st.getObject(id).isUnit()) continue;
        ASSERT_TRUE(ed.moveObjectToBattlefield(st, id, bf).ok);
        moved.push_back(id);
        if (moved.size() == 6) break;
    }
    ASSERT_EQ(moved.size(), 6u);

    const std::string board = game.renderer.render(st);
    for (auto id : moved) {
        const std::string unit = game.renderer.renderObject(st, id);
        EXPECT_NE(board.find(unit), std::string::npos)
            << unit << " was cut off the board";
    }
}

TEST(StateRendererReviewFix, LongOptionLabelsAreNotTruncated) {
    GameState s{};
    StateRenderer renderer(cards().db);
    const std::string common(45, 'x');
    std::vector<Intent> options;
    for (const char* tail : {"-deal 2 damage", "-draw 2 cards"}) {
        Intent it{};
        it.type = IntentType::MakeChoice;
        it.player = PlayerId::Player1;
        it.chosen_value = static_cast<int>(options.size());
        it.choice_label = common + tail;
        options.push_back(it);
    }
    const std::string text = renderer.renderActions(s, options);
    // Both labels share their first 45 characters; the old 38-column cap
    // rendered them identically.
    EXPECT_NE(text.find(common + "-deal 2 damage"), std::string::npos);
    EXPECT_NE(text.find(common + "-draw 2 cards"), std::string::npos);
}

TEST(StateRendererReviewFix, HiddenHandShowsOnlyItsSize) {
    LiveGame game;
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    json f = playUntil(c, [](const json& m) {
        return m["zones"]["P2"]["hand_count"].get<int>() > 0 &&
               m["zones"]["P1"]["hand_count"].get<int>() > 0;
    });
    ASSERT_FALSE(f.is_null());
    std::lock_guard<std::mutex> lock(game.server->stateMutex());
    const auto& st = game.engine.state();
    const std::string all = game.renderer.render(st);
    const std::string p1_view = game.renderer.render(st, RenderVisibility{true, false});
    EXPECT_EQ(all.find("hidden)"), std::string::npos);
    EXPECT_EQ(countOf(p1_view, "hidden)"), 1);
    const auto n = st.player(PlayerId::Player2).hand.size();
    EXPECT_NE(p1_view.find(" Hand:  (" + std::to_string(n) + " card"), std::string::npos);
}

// ─── 4.10 / 4.11 — script-embedded strings; gallery parsed once ──────────────

TEST(HtmlOutputReviewFix, CardNamesCannotCloseTheScriptBlock) {
    const std::map<std::string, std::string> benign = {{"Plain Card", "https://x/y.png"}};
    const std::map<std::string, std::string> hostile = {
        {"Evil</script><script>alert(1)</script>", "https://x/</script>.png"}};
    const std::string a = buildPlayIndexHtml(benign, "v", "d", 1, true);
    const std::string b = buildPlayIndexHtml(hostile, "v", "d", 1, true);
    EXPECT_EQ(countOf(b, "</script"), countOf(a, "</script"));
    EXPECT_NE(b.find("Evil\\u003c/script>"), std::string::npos);
}

TEST(HtmlOutputReviewFix, ReplayStringsCannotCloseTheScriptBlock) {
    const auto dir = std::filesystem::path(testing::TempDir());
    auto write = [&](const std::string& name, const std::string& header) {
        const std::string path = (dir / name).string();
        ReplayWriter w(cards().db, path);
        w.setGameHeader(header);
        w.addTraceLine("[TRC] " + header);
        w.writeHtml();
        return readFile(path);
    };
    const std::string a = write("io_cli_plain.html", "seed 1");
    const std::string b = write("io_cli_hostile.html", "x</script><script>alert(1)</script>");
    ASSERT_FALSE(a.empty());
    EXPECT_EQ(countOf(b, "</script"), countOf(a, "</script"));
    EXPECT_NE(b.find("x\\x3c/script>"), std::string::npos);
}

TEST(HtmlOutputReviewFix, CardImageMapIsLoadedOnce) {
    const auto& a = ReplayWriter::loadCardImageMap();
    const auto& b = ReplayWriter::loadCardImageMap();
    EXPECT_EQ(&a, &b);
}

// ─── 4.14 / 4.18 / 4.19 — batch and game runner ──────────────────────────────

TEST(BatchRunnerReviewFix, ExceptionInAWorkerIsRethrownNotFatal) {
    GameConfig cfg;
    cfg.base_seed = 7;
    cfg.agent_factory = [](int, uint64_t) -> std::unique_ptr<AgentInterface> {
        throw std::runtime_error("agent factory failed");
    };
    AggregateResults results;
    BatchRunner batch(cards().db, cards().registry, 3);
    // Used to escape the pool task and std::terminate the process.
    try {
        batch.runBatch(cards().deck1, cards().deck2, 6, cfg, results);
        FAIL() << "expected the worker's exception";
    } catch (const std::runtime_error& e) {
        // Names the game and its seed, then the cause.
        const std::string what = e.what();
        EXPECT_NE(what.find("game "), std::string::npos) << what;
        EXPECT_NE(what.find("[seed="), std::string::npos) << what;
        EXPECT_NE(what.find("agent factory failed"), std::string::npos) << what;
    }
    EXPECT_EQ(results.games_completed.load(), 0);
}

TEST(GameRunnerReviewFix, ReplayGoesToTheConfiguredDirectory) {
    const auto dir = std::filesystem::path(testing::TempDir()) / "io_cli_replays";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    GameConfig cfg;
    cfg.base_seed = 20261010;
    cfg.do_render = true;
    cfg.replay_dir = dir.string();
    AggregateResults results;
    results.total_games = 1;
    GameRunner runner(cards().db, cards().registry, cards().deck1, cards().deck2, cfg, results);
    testing::internal::CaptureStdout();
    runner.run();
    testing::internal::GetCapturedStdout();
    EXPECT_TRUE(std::filesystem::exists(dir / "replay_game1.html"));
}

TEST(GameRunnerReviewFix, DebugLogIsPrinted) {
    GameConfig cfg;
    cfg.base_seed = 20261010;
    cfg.debug_mode = true;
    cfg.trace_mode = true;
    AggregateResults results;
    results.total_games = 1;
    GameRunner runner(cards().db, cards().registry, cards().deck1, cards().deck2, cfg, results);
    testing::internal::CaptureStdout();
    runner.run();
    const std::string out = testing::internal::GetCapturedStdout();
    // The log used to be collected into a buffer nobody read.
    EXPECT_NE(out.find("[TRC] "), std::string::npos);
    EXPECT_NE(out.find("Game 1 [seed=20261010] log"), std::string::npos);
}

// ─── 9.L3 — what a batch does after one game throws ─────────────────────────

namespace {

/// Plays at random, except that it throws at its first decision in the game
/// whose seed is `bad_seed`.
class ThrowsInOneGameAgent : public AgentInterface {
public:
    ThrowsInOneGameAgent(uint64_t seed, int seat, uint64_t bad_seed)
        : inner_(seed * 2 + static_cast<uint64_t>(seat)), fail_(seed == bad_seed && seat == 0) {}
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        if (fail_) throw std::runtime_error("injected fault");
        return inner_.selectAction(state, legal);
    }
private:
    RandomAgent inner_;
    bool fail_;
};

} // namespace

// Pins the behaviour, single-threaded so the order is fixed: the games before
// the failing one ran and are counted, the failing game is named by number
// and seed, the games after it are NOT run, and runBatch throws (the binary
// then prints "Error: a game failed: ... (k of n games completed)" and exits
// 1 without a results table).
TEST(BatchRunnerFaultInjection, OneThread_EarlierGamesCountLaterGamesAreSkipped) {
    constexpr uint64_t kBase = 4100;
    constexpr int kGames = 5;
    constexpr int kBadIndex = 2;   // the third game
    std::atomic<int> games_started{0};
    GameConfig cfg;
    cfg.base_seed = kBase;
    cfg.agent_factory = [&](int seat, uint64_t seed) -> std::unique_ptr<AgentInterface> {
        if (seat == 0) games_started.fetch_add(1);
        return std::make_unique<ThrowsInOneGameAgent>(seed, seat, kBase + kBadIndex);
    };
    AggregateResults results;
    BatchRunner batch(cards().db, cards().registry, 1);
    std::string what;
    try {
        batch.runBatch(cards().deck1, cards().deck2, kGames, cfg, results);
        FAIL() << "expected the failing game's exception";
    } catch (const std::runtime_error& e) {
        what = e.what();
    }
    EXPECT_NE(what.find("game 3 [seed=" + std::to_string(kBase + kBadIndex) + "]"),
              std::string::npos) << what;
    EXPECT_NE(what.find("injected fault"), std::string::npos) << what;
    EXPECT_EQ(results.games_completed.load(), kBadIndex) << "games 1 and 2 finished and count";
    EXPECT_EQ(results.p1_wins.load() + results.p2_wins.load() + results.draws.load(), kBadIndex);
    EXPECT_EQ(games_started.load(), kBadIndex + 1) << "games 4 and 5 were never started";

    // 9.S1: what the binary prints then. The table for the two games that
    // completed comes first, the error after it.
    std::ostringstream printed;
    reportFailedBatch(printed, printed, results, kGames, 1, what);
    const std::string text = printed.str();
    const auto table = text.find("Results (2 of 5 games completed):");
    const auto error = text.find("Error: a game failed: game 3 [seed=");
    ASSERT_NE(table, std::string::npos) << text;
    ASSERT_NE(error, std::string::npos) << text;
    EXPECT_LT(table, error) << text;
    EXPECT_NE(text.find("  P1 wins: " + std::to_string(results.p1_wins.load()) + "\n"),
              std::string::npos) << text;
    EXPECT_NE(text.find("  Avg turns: " + std::to_string(results.total_turns.load() / 2) + "\n"),
              std::string::npos) << "averaged over the completed games: " << text;
    EXPECT_NE(text.find("(2 of 5 games completed)\n"), std::string::npos) << text;
}

// A batch that ran to the end prints the table it always did.
TEST(BatchReport, ACompleteBatchPrintsTheUsualTable) {
    AggregateResults results;
    results.p1_wins = 3;
    results.p2_wins = 1;
    results.total_turns = 40;
    results.total_decisions = 800;
    results.games_completed = 4;
    std::ostringstream out;
    printBatchResults(out, results, 4, 2);
    EXPECT_EQ(out.str(), "\nResults (4 games):\n  P1 wins: 3\n  P2 wins: 1\n  Draws:   0\n"
                         "  Avg turns: 10\n  Avg decisions: 200\n  Threads: 2\n");
}

// With several workers the games already running finish and count; a game not
// yet started when the failure is recorded is skipped. So the number completed
// is anything from 0 to all but the failing one: what is pinned is that it is
// never the failing game, that the process survives, and the game is named.
TEST(BatchRunnerFaultInjection, ManyThreads_TheFailedGameIsNamedAndNeverCounted) {
    constexpr uint64_t kBase = 4200;
    constexpr int kGames = 6;
    constexpr int kBadIndex = 3;
    GameConfig cfg;
    cfg.base_seed = kBase;
    cfg.agent_factory = [&](int seat, uint64_t seed) -> std::unique_ptr<AgentInterface> {
        return std::make_unique<ThrowsInOneGameAgent>(seed, seat, kBase + kBadIndex);
    };
    AggregateResults results;
    BatchRunner batch(cards().db, cards().registry, 3);
    std::string what;
    try {
        batch.runBatch(cards().deck1, cards().deck2, kGames, cfg, results);
        FAIL() << "expected the failing game's exception";
    } catch (const std::runtime_error& e) {
        what = e.what();
    }
    EXPECT_NE(what.find("game 4 [seed=" + std::to_string(kBase + kBadIndex) + "]"),
              std::string::npos) << what;
    EXPECT_LE(results.games_completed.load(), kGames - 1);
    EXPECT_EQ(results.p1_wins.load() + results.p2_wins.load() + results.draws.load(),
              results.games_completed.load());
}

// ─── Carry-over (io-cli) — the first Ctrl-C ends a game with no human seat ───

namespace {

/// A random agent that counts how often the engine really consulted it.
class CountingAgent : public AgentInterface {
public:
    CountingAgent(uint64_t seed, std::atomic<int>& asked) : inner_(seed), asked_(asked) {}
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        asked_.fetch_add(1);
        return inner_.selectAction(state, legal);
    }
private:
    RandomAgent inner_;
    std::atomic<int>& asked_;
};

} // namespace

TEST(PlayServerShutdown, FirstShutdownEndsASpectatorGameAtTheNextDecision) {
    EventBus events;
    GameEngine engine{cards().db, events, cards().registry};
    StateRenderer renderer{cards().db};
    PlayServerConfig cfg;
    cfg.port = 0;
    cfg.spectator = true;        // no human seat: nothing for on_shutdown_signal to abort
    cfg.reveal_hidden = true;
    PlayServer server(engine, renderer, cards().db, std::move(cfg));
    server.start();

    std::atomic<int> asked{0};
    CountingAgent ai1{31, asked}, ai2{32, asked};
    ServedAiSeat seat1{ai1, server}, seat2{ai2, server};   // as main.cpp wraps AI seats
    engine.on_decision = [&](const GameState&, const std::vector<Intent>& legal,
                             const Intent& chosen) {
        server.awaitSpectatorStep(legal, chosen);
        server.notifyDecisionMade();
    };
    GameResult result;
    std::atomic<bool> done{false};
    std::thread game_thread([&] {
        result = engine.runGame(cards().deck1, cards().deck2, seat1, seat2, 20261011);
        done.store(true);
    });

    // Autoplay is off: the engine parks at the spectator gate after the
    // first decision it reports through on_decision and stays there. Wait
    // until the number of decisions asked has stopped moving.
    auto deadline = std::chrono::steady_clock::now() + 5s;
    int parked_at = -1;
    while (std::chrono::steady_clock::now() < deadline) {
        const int seen = asked.load();
        std::this_thread::sleep_for(300ms);
        if (seen > 0 && asked.load() == seen) { parked_at = seen; break; }
    }
    // (No ASSERT while game_thread is joinable.)
    EXPECT_GT(parked_at, 0) << "the engine never parked at the spectator gate";
    EXPECT_FALSE(done.load());

    EXPECT_FALSE(server.shutdownRequested());
    server.requestShutdown();    // what the first SIGINT / SIGTERM does
    EXPECT_TRUE(server.shutdownRequested());

    deadline = std::chrono::steady_clock::now() + 5s;
    while (!done.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(5ms);
    const bool finished = done.load();
    if (!finished) {
        // Do not hang the suite: without the fix the game plays on by itself.
        ADD_FAILURE() << "the game was still running 5 s after requestShutdown()";
    }
    game_thread.join();
    server.notifyGameFinished();
    server.stop();

    // It used to run the whole AI game (hundreds of decisions) to its
    // natural end. Now no agent is consulted again: the next seat asked
    // concedes.
    EXPECT_EQ(asked.load(), parked_at);
    EXPECT_NE(result.termination_reason.find("conceded"), std::string::npos)
        << result.termination_reason;
}

// ─── Carry-over (io-cli) — a god-mode edit refreshes the pending decision ────

TEST(PlayServerGodMode, EditRecomputesThePendingMainPhaseDecision) {
    LiveGame game;
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    auto typeHas = [](const json& action, const char* word) {
        return action["type"].get<std::string>().find(word) != std::string::npos;
    };
    // P1's main-phase decision with a card it could play.
    json f = playUntil(c, [&](const json& m) {
        bool end_turn = false, play = false;
        for (const auto& a : m["legal"]) {
            end_turn |= typeHas(a, "EndTurn");
            play |= typeHas(a, "PlayCard") && a.contains("card_object_id");
        }
        return end_turn && play;
    });
    ASSERT_FALSE(f.is_null());
    const uint64_t id0 = f["decision_id"].get<uint64_t>();
    int card = -1;
    for (const auto& a : f["legal"])
        if (typeHas(a, "PlayCard") && a.contains("card_object_id"))
            card = a["card_object_id"].get<int>();
    ASSERT_GE(card, 0);

    // Take that card out of P1's hand while the engine waits.
    c.send({{"type", "edit_move"}, {"object_id", card}, {"to_player", "P1"}, {"to_zone", "Trash"}});
    json reply = c.nextReply();
    ASSERT_EQ(reply["type"], "edit_ok") << reply.dump();
    EXPECT_TRUE(reply["legal_recomputed"].get<bool>());

    // The decision is offered again, as a new one, without the plays of the
    // card that is no longer in hand (they used to stay clickable).
    json g = c.nextDecision("P1", id0);
    ASSERT_FALSE(g.is_null());
    const uint64_t id1 = g["decision_id"].get<uint64_t>();
    int end_turn_index = -1;
    for (const auto& a : g["legal"]) {
        EXPECT_NE(a.value("card_object_id", -1), card) << a.dump();
        if (typeHas(a, "EndTurn")) end_turn_index = a["index"].get<int>();
    }
    ASSERT_GE(end_turn_index, 0);

    // A click made on the pre-edit list is refused, not applied.
    c.send({{"type", "choose"}, {"player", "P1"}, {"index", 0}, {"decision_id", id0}});
    reply = c.nextReply();
    EXPECT_EQ(reply["type"], "err");
    EXPECT_NE(reply.value("msg", "").find("stale"), std::string::npos) << reply.dump();

    // An answer from the new list is what the engine applies: the turn ends.
    c.send({{"type", "choose"}, {"player", "P1"}, {"index", end_turn_index}, {"decision_id", id1}});
    json next = c.readUntil([&](const json& m) {
        return (m["type"] == "decision" || m["type"] == "state") &&
               m["turn"].value("turn_player", "") == "P2";
    });
    EXPECT_FALSE(next.is_null());
}

TEST(PlayServerGodMode, EditAtANestedDecisionSaysTheListWasKept) {
    LiveGame game;
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    // The first decision of a game is not the main-phase one (no EndTurn).
    json first = c.nextDecision("P1");
    ASSERT_FALSE(first.is_null());
    for (const auto& a : first["legal"])
        ASSERT_EQ(a["type"].get<std::string>().find("EndTurn"), std::string::npos);
    const uint64_t id0 = first["decision_id"].get<uint64_t>();

    c.send({{"type", "edit_player"}, {"player", "P1"}, {"field", "score"}, {"value", 1}});
    json reply = c.nextReply();
    ASSERT_EQ(reply["type"], "edit_ok") << reply.dump();
    EXPECT_FALSE(reply["legal_recomputed"].get<bool>());
    // Same decision, same id: an answer to it is still accepted.
    json again = c.nextDecision("P1");
    EXPECT_EQ(again["decision_id"].get<uint64_t>(), id0);
    EXPECT_EQ(again["legal"], first["legal"]);
}

// ─── 9.P1 — a god-mode edit at a showdown refreshes that decision too ────────

// The engine checks a showdown answer against the list rebuilt from the edited
// state and throws on a stale one. The server therefore has to hand the human
// the rebuilt list, and refuse a click made on the old one, at a showdown as
// it does in the main phase.
namespace {

/// Play P1 until it is asked a decision that offers `pass_word` ("PassFocus":
/// a showdown; "PassPriority": chain priority) with a card it could play, take
/// that card out of its hand by a god-mode edit, and check the server's side
/// of it: the list is rebuilt, a click on the old list is refused, an answer
/// from the new one is accepted and the game goes on. `head_for`: the action
/// types to prefer, in order, at the decisions before that one.
void editRebuildsTheListAt(const char* pass_word, const std::vector<const char*>& head_for) {
    LiveGame game;
    Client c;
    ASSERT_TRUE(c.connect(game.port()));
    auto typeHas = [](const json& action, const char* word) {
        return action["type"].get<std::string>().find(word) != std::string::npos;
    };
    auto indexOf = [&](const json& frame, const char* word) {
        for (const auto& a : frame["legal"])
            if (typeHas(a, word)) return a["index"].get<int>();
        return -1;
    };
    auto edit = [&](json msg) {
        c.send(msg);
        json reply = c.nextReply();
        EXPECT_EQ(reply["type"], "edit_ok") << reply.dump();
        return c.nextDecision("P1");          // the frame broadcast after the edit
    };

    bool done = false;
    uint64_t last = 0;
    for (int i = 0; i < 400 && !done; ++i) {
        json f = c.nextDecision("P1", last);
        ASSERT_FALSE(f.is_null()) << "the game ended before a usable showdown, decision " << i;
        last = f["decision_id"].get<uint64_t>();
        const int pass_focus = indexOf(f, pass_word);
        if (pass_focus < 0) {
            // Not that decision: head for one.
            int pick = -1;
            for (const char* word : head_for)
                if (pick < 0) pick = indexOf(f, word);
            if (pick < 0) pick = 0;
            c.send({{"type", "choose"}, {"player", "P1"}, {"index", pick}, {"decision_id", last}});
            continue;
        }
        // The decision. Give P1 the means to play anything in hand...
        json cur = edit({{"type", "edit_player"}, {"player", "P1"}, {"field", "energy"}, {"value", 20}});
        cur = edit({{"type", "edit_player"}, {"player", "P1"}, {"field", "power"},
                    {"domain_idx", -1}, {"value", 10}});
        ASSERT_FALSE(cur.is_null());
        ASSERT_GE(indexOf(cur, pass_word), 0) << "still that decision";
        last = cur["decision_id"].get<uint64_t>();
        int card = -1;
        for (const auto& a : cur["legal"])
            if (typeHas(a, "Play") && a.contains("card_object_id"))
                card = a["card_object_id"].get<int>();
        if (card < 0) {   // nothing playable at this one: pass and try the next
            c.send({{"type", "choose"}, {"player", "P1"}, {"index", indexOf(cur, pass_word)},
                    {"decision_id", last}});
            continue;
        }
        // ...then take a playable card out of the hand while the engine waits.
        const uint64_t id_before = last;
        c.send({{"type", "edit_move"}, {"object_id", card}, {"to_player", "P1"}, {"to_zone", "Trash"}});
        json reply = c.nextReply();
        ASSERT_EQ(reply["type"], "edit_ok") << reply.dump();
        EXPECT_TRUE(reply["legal_recomputed"].get<bool>()) << "the list is rebuilt";
        json g = c.nextDecision("P1", id_before);
        ASSERT_FALSE(g.is_null());
        const uint64_t id_after = g["decision_id"].get<uint64_t>();
        for (const auto& a : g["legal"])
            EXPECT_NE(a.value("card_object_id", -1), card) << a.dump();
        const int pass = indexOf(g, pass_word);
        ASSERT_GE(pass, 0);

        // A click on the pre-edit list is refused; it does not reach the
        // engine (which would throw on it and end the game).
        c.send({{"type", "choose"}, {"player", "P1"}, {"index", 0}, {"decision_id", id_before}});
        reply = c.nextReply();
        EXPECT_EQ(reply["type"], "err");
        EXPECT_NE(reply.value("msg", "").find("stale"), std::string::npos) << reply.dump();

        // An answer from the new list is accepted and the game goes on.
        c.send({{"type", "choose"}, {"player", "P1"}, {"index", pass}, {"decision_id", id_after}});
        json next = c.readUntil([&](const json& m) {
            return (m["type"] == "decision" || m["type"] == "state") &&
                   m.value("decision_id", id_after) != id_after;
        });
        EXPECT_FALSE(next.is_null()) << "the game stopped after the answer";
        done = true;
    }
    EXPECT_TRUE(done) << "no " << pass_word << " decision with a playable card was reached";
}

} // namespace

TEST(PlayServerGodMode, EditAtAShowdownRecomputesAndAStaleClickDoesNotEndTheGame) {
    editRebuildsTheListAt("PassFocus", {"Move", "PlayCard"});
}

// 9.S2: the same at a chain priority decision.
TEST(PlayServerGodMode, EditAtAChainPriorityDecisionRecomputesAndAStaleClickDoesNotEndTheGame) {
    editRebuildsTheListAt("PassPriority", {"PlayCard", "Move"});
}

} // namespace riftbound::test::review_fix_io_cli
