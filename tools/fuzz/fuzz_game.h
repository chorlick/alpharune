#pragma once
/// @file fuzz_game.h
/// One fuzz game: two decks, random agents, one seed. Used by the fuzz driver
/// (tools/deck_fuzz.cpp) and by the small default run in the test suite
/// (tests/test_deck_fuzz.cpp). Header-only so both can include it.
///
/// A game is a function of (deck A, deck B, game seed, the two agent specs):
/// the agents are seeded as GameRunner seeds them (`seed * 2 + seat`), so a
/// game played here is the game `riftbound --seed <seed> --games 1` plays
/// with the same two decks and agents.
///
/// An engine exception does not escape: it is the game's outcome
/// (`termination == "engine_error"`, the text in `exception`).
///
/// Per-card coverage is taken from engine events and the chain, not from log
/// text: see CardCoverage.

#include "agents/agent_interface.h"
#include "agents/random_agent.h"
#include "agents/random_player_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "ml/feature_extractor.h"
#include "rules/deck_validator.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <ostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace riftbound::fuzz {

/// What is counted per card. A game counts once per kind however often it
/// happened in that game; `events` counts every occurrence.
enum CoverageKind : int {
    kInDeck = 0,     // the card was in a deck of the game (legend, champion, main, runes, battlefields)
    kInPlay,         // it was played or put into play: a CardPlayedEvent for it, an object of it
                     // entering the board, a token of it created, a rune of it channeled,
                     // or it was a game's legend or one of its battlefields
    kTriggered,      // a triggered ability of it was finalized on the chain
    kActivated,      // an activated ability of it was finalized on the chain
    kResolved,       // an ability of it, or the spell itself, resolved (ChainItemResolvedEvent)
    kCoverageKinds
};

inline const char* coverageKindName(int k) {
    static const char* names[] = {"in_deck", "in_play", "triggered", "activated", "resolved"};
    return names[k];
}

/// Card ids are below this (ml::kCardVocabSize is the id space).
inline constexpr std::size_t kCardSlots = static_cast<std::size_t>(ml::kCardVocabSize) + 1;

/// Sums over games. One per worker thread; add them up at the end.
struct CardCoverage {
    std::array<std::vector<uint32_t>, kCoverageKinds> games;    // games with at least one
    std::array<std::vector<uint64_t>, kCoverageKinds> events;   // occurrences
    uint64_t games_counted = 0;

    CardCoverage() {
        for (auto& v : games) v.assign(kCardSlots, 0);
        for (auto& v : events) v.assign(kCardSlots, 0);
    }
    void add(const CardCoverage& o) {
        for (int k = 0; k < kCoverageKinds; ++k)
            for (std::size_t i = 0; i < kCardSlots; ++i) {
                games[k][i] += o.games[k][i];
                events[k][i] += o.events[k][i];
            }
        games_counted += o.games_counted;
    }
};

struct GameOptions {
    /// A game that asks more decisions than this is stopped and reported as
    /// `termination == "decision_cap"`: the engine caps turns (200), not the
    /// decisions of one turn.
    long max_decisions = 20000;
    /// A game still asking decisions after this many seconds is stopped and
    /// reported as `termination == "time_cap"` (0 = no limit). Not a function
    /// of the seeds: it is there so a batch survives a game that crawls.
    double max_seconds = 0;
    /// A chain that has had this many items finalized without emptying is a
    /// trigger feeding itself: the game is stopped and reported as
    /// `termination == "chain_runaway"` (0 = no limit). The engine's own bound
    /// (1000 FEPR passes) drops such a chain and plays on, on a board of a
    /// thousand tokens where one decision can take minutes or all the memory.
    int max_chain = 500;
    /// The two seats: "random" (RandomAgent) or "random-player[:pass=P]"
    /// (RandomPlayerAgent: acts whenever it can). Seeded as GameRunner seeds
    /// them, so the game is the one `riftbound --agent1 .. --agent2 ..` plays.
    std::string agents[2] = {"random", "random"};
    CardCoverage* coverage = nullptr;       // nullptr = no listener attached
    /// Log lines kept (the last `log_tail`, 0 = none, <0 = all). Attaching a
    /// log reader makes the engine build every line: for one game, not a batch.
    int log_tail = 0;
    /// Every log line as it is emitted (for a game that does not come back:
    /// the tail is only available once the game has ended).
    std::ostream* log_stream = nullptr;
};

struct GameOutcome {
    uint64_t game_seed = 0;
    /// "engine_error" (an exception: see `exception`), "decision_cap",
    /// "chain_runaway", "time_cap", or the engine's own reason ("Player1 reached 8 points", "Max turns reached", ...).
    std::string termination;
    std::string exception;      // what() of the exception, empty if none
    bool failed = false;        // any of the first four
    PlayerId winner = PlayerId::None;
    int scores[2] = {0, 0};
    int turns = 0;
    int decisions = 0;
    /// The most chain items finalized between two moments the chain was empty.
    /// An ordinary chain holds a handful; the chain's safety bound (1000 FEPR
    /// passes, ChainManager::processFEPR) drops a chain that never empties
    /// without failing the game, so a number in the hundreds is how an
    /// ability that keeps triggering itself shows.
    int longest_chain = 0;
    std::vector<std::string> log;   // see GameOptions::log_tail
};

/// The seed the engine is given: 31 bits (as GameRunner masks it) and never 0
/// (0 asks the engine for a random seed).
inline uint64_t engineSeed(uint64_t s) {
    s &= 0x7FFFFFFFu;
    return s == 0 ? 1 : s;
}

struct DecisionCapReached : std::runtime_error {
    DecisionCapReached() : std::runtime_error("decision cap reached") {}
};
struct TimeCapReached : std::runtime_error {
    TimeCapReached() : std::runtime_error("time cap reached") {}
};
struct ChainRunaway : std::runtime_error {
    ChainRunaway() : std::runtime_error("chain runaway") {}
};

/// A seat: the agent the spec names, behind the two caps that are checked
/// at a decision.
class FuzzAgent : public AgentInterface {
public:
    FuzzAgent(const std::string& spec, uint64_t seed, long max_decisions,
              std::chrono::steady_clock::time_point deadline = {})
        : max_decisions_(max_decisions), deadline_(deadline) {
        double pass = 0.0;
        if (spec == "random") inner_ = std::make_unique<RandomAgent>(seed);
        else if (isRandomPlayerSpec(spec, &pass))
            inner_ = std::make_unique<RandomPlayerAgent>(seed, pass);
        else throw std::invalid_argument("fuzz: unknown agent '" + spec +
                                         "' (random, random-player[:pass=P])");
    }

    Intent selectAction(const GameState& state,
                        const std::vector<Intent>& legal_actions) override {
        if (state.decision_index > max_decisions_) throw DecisionCapReached();
        if (deadline_ != std::chrono::steady_clock::time_point{} &&
            (state.decision_index & 63) == 0 && std::chrono::steady_clock::now() > deadline_)
            throw TimeCapReached();
        return inner_->selectAction(state, legal_actions);
    }

private:
    std::unique_ptr<AgentInterface> inner_;
    long max_decisions_;
    std::chrono::steady_clock::time_point deadline_;
};

namespace detail {

/// The per-game half of CardCoverage: which (kind, card) pairs this game has
/// already counted.
class GameCoverage {
public:
    explicit GameCoverage(CardCoverage& total) : total_(total) {
        for (auto& v : seen_) v.assign(kCardSlots, 0);
    }
    void note(int kind, CardDefId id) {
        if (!id.valid()) return;
        const std::size_t i = id.raw();
        if (i >= kCardSlots) return;
        ++total_.events[kind][i];
        if (!seen_[kind][i]) {
            seen_[kind][i] = 1;
            ++total_.games[kind][i];
        }
    }

private:
    CardCoverage& total_;
    std::array<std::vector<uint8_t>, kCoverageKinds> seen_;
};

/// What a chain item was when it was finalized, kept until it resolves (the
/// item is off the chain by the time ChainItemResolvedEvent is emitted).
struct ItemNote {
    CardDefId card = kInvalidId;
    bool counts = false;   // an ability or a spell (not a permanent being played)
};

} // namespace detail

/// Plays one game. Never throws for something the engine did.
inline GameOutcome playGame(const CardDB& db, const CardRegistry& registry,
                            const DeckSubmission& deck_a, const DeckSubmission& deck_b,
                            uint64_t game_seed, const GameOptions& opt = {}) {
    GameOutcome out;
    out.game_seed = engineSeed(game_seed);

    EventBus events;
    GameEngine engine(db, events, registry);

    std::deque<std::string> log;
    if (opt.log_tail != 0 || opt.log_stream) {
        events.on_log.connect([&log, tail = opt.log_tail,
                               stream = opt.log_stream](const LogEvent& e) {
            const char* prefix = "[TRC]";
            if (e.level == LogLevel::Debug) prefix = "[DBG]";
            if (e.level == LogLevel::Info) prefix = "[INF]";
            if (e.level == LogLevel::Warning) prefix = "[WRN]";
            if (stream) *stream << "  " << prefix << " " << e.message << "\n";
            if (tail == 0) return;
            log.push_back(std::string(prefix) + " " + e.message);
            if (tail > 0 && static_cast<int>(log.size()) > tail) log.pop_front();
        });
    }

    std::chrono::steady_clock::time_point deadline{};
    if (opt.max_seconds > 0)
        deadline = std::chrono::steady_clock::now() +
                   std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                       std::chrono::duration<double>(opt.max_seconds));

    // The two stops that do not wait for a decision. Both throw out of an
    // event slot, which ends the game like any engine exception.
    int chain_run = 0;
    std::vector<std::pair<std::string, int>> run_items;   // whose items, this chain
    events.on_chain_item_finalized.connect([&](const ChainItemFinalizedEvent& e) {
        if (++chain_run > out.longest_chain) out.longest_chain = chain_run;
        if (opt.max_chain <= 0) return;
        // Whose item: in a runaway the card with the most items is the
        // ability that keeps coming back (a bystander that triggers on each
        // round has at most as many).
        std::string name = "?";
        const GameState& st = engine.state();
        for (const auto& ci : st.chain.items) {
            if (ci.id != e.item) continue;
            if (ci.keyword_ability.has_value())
                name = std::string("the keyword ") + toString(*ci.keyword_ability);
            else if (ci.card_def_id.valid()) name = db.get(ci.card_def_id).name;
            else if (st.objectExists(ci.source)) name = st.getObject(ci.source).name;
        }
        auto it = std::find_if(run_items.begin(), run_items.end(),
                               [&](const auto& p) { return p.first == name; });
        if (it == run_items.end()) run_items.emplace_back(name, 1);
        else ++it->second;
        if (chain_run >= opt.max_chain) {
            std::stable_sort(run_items.begin(), run_items.end(),
                             [](const auto& a, const auto& b) { return a.second > b.second; });
            out.exception = "a chain had " + std::to_string(chain_run) +
                            " items finalized without emptying; most are of " +
                            run_items.front().first;
            throw ChainRunaway();
        }
    });
    events.on_chain_emptied.connect([&](const ChainEmptiedEvent&) {
        chain_run = 0;
        run_items.clear();
    });
    if (opt.max_seconds > 0) {
        // Every cleanup ends in a StateSettledEvent, so a loop inside one
        // decision passes here too.
        events.on_state_settled.connect([&, n = 0u](const StateSettledEvent&) mutable {
            if ((++n & 255u) == 0 && std::chrono::steady_clock::now() > deadline)
                throw TimeCapReached();
        });
    }

    std::unique_ptr<detail::GameCoverage> cov;
    std::vector<detail::ItemNote> items;   // by chain item id
    if (opt.coverage) {
        ++opt.coverage->games_counted;
        cov = std::make_unique<detail::GameCoverage>(*opt.coverage);
        auto deckCards = [&](const DeckSubmission& d) {
            cov->note(kInDeck, d.legend);
            cov->note(kInDeck, d.chosen_champion);
            for (CardDefId id : d.main_deck) cov->note(kInDeck, id);
            for (CardDefId id : d.rune_deck) cov->note(kInDeck, id);
            for (CardDefId id : d.battlefields) cov->note(kInDeck, id);
        };
        deckCards(deck_a);
        deckCards(deck_b);

        auto cardOf = [&engine](GameObjectId obj) -> CardDefId {
            const GameState& s = engine.state();
            return s.objectExists(obj) ? s.getObject(obj).card_def_id : CardDefId{};
        };
        auto* c = cov.get();
        events.on_card_played.connect(
            [c, cardOf](const CardPlayedEvent& e) { c->note(kInPlay, cardOf(e.object)); });
        events.on_entered_board.connect(
            [c, cardOf](const EnteredBoardEvent& e) { c->note(kInPlay, cardOf(e.object)); });
        events.on_token_created.connect(
            [c, cardOf](const TokenCreatedEvent& e) { c->note(kInPlay, cardOf(e.object)); });
        events.on_rune_channeled.connect(
            [c, cardOf](const RuneChanneledEvent& e) { c->note(kInPlay, cardOf(e.rune)); });
        // The first phase change comes after setup: the legends are in play
        // from then on. (Battlefields are counted when the game stops.)
        events.on_phase_changed.connect([c, cardOf, &engine, done = false](
                                            const PhaseChangedEvent&) mutable {
            if (done) return;
            done = true;
            for (const auto& p : engine.state().players) c->note(kInPlay, cardOf(p.legend_zone));
        });
        events.on_chain_item_finalized.connect(
            [c, &engine, &items](const ChainItemFinalizedEvent& e) {
                const ChainItem* item = nullptr;
                for (const auto& ci : engine.state().chain.items)
                    if (ci.id == e.item) { item = &ci; break; }
                if (!item) return;
                detail::ItemNote note;
                if (item->keyword_ability.has_value()) {
                    // A keyword's ability ([Vision], [Weaponmaster]) is the
                    // keyword's, not the card's.
                } else if (item->is_ability) {
                    note.card = item->granted_ability_def.valid() ? item->granted_ability_def
                                                                   : item->card_def_id;
                    note.counts = true;
                    c->note(item->is_activated_ability ? kActivated : kTriggered, note.card);
                } else if (item->is_spell) {
                    note.card = item->card_def_id;
                    note.counts = true;
                }
                const std::size_t i = e.item.raw();
                if (items.size() <= i) items.resize(i + 1);
                items[i] = note;
            });
        events.on_chain_item_resolved.connect(
            [c, &items](const ChainItemResolvedEvent& e) {
                const std::size_t i = e.item.raw();
                if (i < items.size() && items[i].counts) c->note(kResolved, items[i].card);
            });
    }

    FuzzAgent agent1(opt.agents[0], out.game_seed * 2, opt.max_decisions, deadline);
    FuzzAgent agent2(opt.agents[1], out.game_seed * 2 + 1, opt.max_decisions, deadline);

    auto fromState = [&] {
        const GameState& s = engine.state();
        out.turns = s.turn.turn_number;
        out.decisions = s.decision_index;
        out.scores[0] = s.players[0].score;
        out.scores[1] = s.players[1].score;
    };
    try {
        const GameResult r = engine.runGame(deck_a, deck_b, agent1, agent2, out.game_seed);
        out.winner = r.winner;
        out.scores[0] = r.final_scores[0];
        out.scores[1] = r.final_scores[1];
        out.turns = r.total_turns;
        out.decisions = r.total_decisions;
        out.termination = r.termination_reason;
    } catch (const DecisionCapReached&) {
        out.failed = true;
        out.termination = "decision_cap";
        fromState();
    } catch (const ChainRunaway&) {
        out.failed = true;
        out.termination = "chain_runaway";
        fromState();
    } catch (const TimeCapReached&) {
        out.failed = true;
        out.termination = "time_cap";
        fromState();
    } catch (const std::exception& e) {
        out.failed = true;
        out.termination = "engine_error";
        out.exception = e.what();
        fromState();
    } catch (...) {
        out.failed = true;
        out.termination = "engine_error";
        out.exception = "(an exception that is not a std::exception)";
        fromState();
    }
    if (cov) {
        // The battlefields of the game, counted when it stops: one added
        // during the game (a token battlefield) announces no event of its own.
        const GameState& s = engine.state();
        for (const auto& bf : s.battlefields)
            if (s.objectExists(bf.card_object_id))
                cov->note(kInPlay, s.getObject(bf.card_object_id).card_def_id);
    }
    out.log.assign(log.begin(), log.end());
    return out;
}

} // namespace riftbound::fuzz
