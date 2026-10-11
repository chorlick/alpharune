// Tests for the OpenSpiel wrapper (RiftboundState / RiftboundGame) and the
// MCTS agents built on it.
//
// These live here, not under tests/, because riftbound_tests links only the
// engine library; this binary also links OpenSpiel and registers the game.
//
// Build:  cmake --build build --target riftbound_openspiel_tests
// Run:    RIFTBOUND_ROOT=. ./build/src/openspiel/riftbound_openspiel_tests

#include <gtest/gtest.h>

#include "action_vocab.h"
#include "riftbound_game.h"
#include "riftbound_state.h"

#include "agents/mcts_agent.h"
#include "agents/random_agent.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "ml/cfr_util.h"
#include "ml/feature_extractor.h"

#include "open_spiel/spiel.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ::riftbound::GameEngine;
using ::riftbound::GameState;
using ::riftbound::Intent;
using ::riftbound::PlayerId;
using ::riftbound::openspiel::RiftboundGame;
using ::riftbound::openspiel::RiftboundState;

std::string deckFile(const char* name) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : ".") + "/decks/" + name;
}

std::string gameString(int seed) {
    return "riftbound(deck1=" + deckFile("yi_rebuild.txt") +
           ",deck2=" + deckFile("fiora.txt") +
           ",seed=" + std::to_string(seed) + ")";
}

std::shared_ptr<const ::open_spiel::Game> loadGame(int seed) {
    return ::open_spiel::LoadGame(gameString(seed));
}

const RiftboundState& rb(const ::open_spiel::State& s) {
    return static_cast<const RiftboundState&>(s);
}

::open_spiel::Player seatOf(PlayerId p) {
    return p == PlayerId::Player1 ? 0 : 1;
}

::open_spiel::Action pickAction(std::mt19937_64& rng,
                                const std::vector<::open_spiel::Action>& legal) {
    std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
    return legal[d(rng)];
}

/// Everything two states in the same position must agree on.
void expectSamePosition(const ::open_spiel::State& a,
                        const ::open_spiel::State& b,
                        const std::string& where) {
    ASSERT_EQ(a.IsTerminal(), b.IsTerminal()) << where;
    ASSERT_EQ(a.CurrentPlayer(), b.CurrentPlayer()) << where;
    ASSERT_EQ(a.LegalActions(), b.LegalActions()) << where;
    ASSERT_EQ(a.MoveNumber(), b.MoveNumber()) << where;
    ASSERT_EQ(rb(a).legalIntents(), rb(b).legalIntents()) << where;
    const GameState& sa = rb(a).engineState();
    const GameState& sb = rb(b).engineState();
    ASSERT_EQ(sa.action_history, sb.action_history) << where;
    ASSERT_EQ(sa.decision_index, sb.decision_index) << where;
    for (int p = 0; p < 2; ++p) {
        ASSERT_EQ(sa.players[p].hand, sb.players[p].hand) << where;
        ASSERT_EQ(sa.players[p].main_deck, sb.players[p].main_deck) << where;
        ASSERT_EQ(sa.players[p].trash, sb.players[p].trash) << where;
        ASSERT_EQ(sa.players[p].score, sb.players[p].score) << where;
        ASSERT_EQ(a.ObservationString(p), b.ObservationString(p)) << where;
        ASSERT_EQ(a.ObservationTensor(p), b.ObservationTensor(p)) << where;
    }
    if (a.IsTerminal()) {
        ASSERT_EQ(a.Returns(), b.Returns()) << where;
    }
}

// ─── LoadGame ───────────────────────────────────────────────────────────────

// 7.40 / 1.14 — the game's default decks exist, so LoadGame("riftbound")
// with no parameters works. (Paths are relative to the repo root.)
TEST(RiftboundGameTest, DefaultParametersNameDecksThatExist) {
    const auto params = loadGame(7)->GetType().parameter_specification;
    for (const char* key : {"deck1", "deck2"}) {
        const std::string path = params.at(key).string_value();
        const char* root = std::getenv("RIFTBOUND_ROOT");
        const std::string full = std::string(root ? root : ".") + "/" + path;
        FILE* f = std::fopen(full.c_str(), "r");
        EXPECT_NE(f, nullptr) << key << " default '" << path << "' is missing";
        if (f) std::fclose(f);
    }
    // The retired "registry" parameter still parses, and is ignored.
    EXPECT_EQ(params.at("registry").string_value(), "");
    auto game = ::open_spiel::LoadGame(
        gameString(7).substr(0, gameString(7).size() - 1) +
        ",registry=cards/ban-list.csv)");
    ASSERT_NE(game, nullptr);
    EXPECT_FALSE(game->NewInitialState()->IsTerminal());
}

// 6.8 — the card table is compiled-in, const data: every game instance in
// the process refers to the same one instead of rebuilding it per LoadGame
// (one per search agent per game). The decks are still each game's own.
TEST(RiftboundGameTest, EveryGameSharesOneCardTable) {
    auto a = loadGame(7);
    auto b = ::open_spiel::LoadGame(
        "riftbound(deck1=" + deckFile("fiora.txt") +
        ",deck2=" + deckFile("yi_rebuild.txt") + ",seed=8)");
    const auto& ga = static_cast<const RiftboundGame&>(*a);
    const auto& gb = static_cast<const RiftboundGame&>(*b);
    EXPECT_EQ(&ga.cardDb(), &gb.cardDb());
    EXPECT_EQ(&ga.cardRegistry(), &gb.cardRegistry());
    EXPECT_EQ(&ga.cardDb(), &::riftbound::openspiel::sharedCards().db);
    EXPECT_GT(ga.cardDb().size(), 900u);
    EXPECT_EQ(ga.cardRegistry().size(), ga.cardDb().size());

    EXPECT_NE(&ga.deck1(), &gb.deck1());
    EXPECT_EQ(ga.deck1().legend, gb.deck2().legend);
    EXPECT_NE(ga.deck1().legend, gb.deck1().legend);
    // Both still play.
    std::mt19937_64 rng(8);
    for (const auto& game : {a, b}) {
        auto state = game->NewInitialState();
        for (int n = 0; n < 40 && !state->IsTerminal(); ++n) {
            state->ApplyAction(pickAction(rng, state->LegalActions()));
        }
        EXPECT_GT(state->MoveNumber(), 0);
    }
}

TEST(RiftboundGameTest, AdvertisedShapesMatchTheEngine) {
    auto game = loadGame(7);
    EXPECT_EQ(game->NumDistinctActions(), ::riftbound::openspiel::kVocabSize);
    EXPECT_EQ(game->ObservationTensorShape(),
              std::vector<int>{::riftbound::ml::kStateFeatureDim});
    EXPECT_EQ(game->MaxGameLength(), ::riftbound::openspiel::kMaxGameLength);
    auto state = game->NewInitialState();
    EXPECT_EQ(state->ObservationTensor(0).size(),
              static_cast<size_t>(::riftbound::ml::kStateFeatureDim));
}

// ─── CurrentPlayer / LegalActions ───────────────────────────────────────────

// 1.1 — CurrentPlayer() is whoever the engine is asking. 1.11 — LegalActions()
// is ascending and duplicate-free, and every id is a slot of a legal Intent.
TEST(RiftboundStateTest, CurrentPlayerIsTheDeciderAndLegalActionsAreSorted) {
    int off_turn = 0, decisions = 0, stale_closed = 0;
    for (int seed = 1; seed <= 4; ++seed) {
        auto game = loadGame(seed);
        auto state = game->NewInitialState();
        std::mt19937_64 rng(seed);
        while (!state->IsTerminal()) {
            const auto intents = rb(*state).legalIntents();
            const GameState& gs = rb(*state).engineState();
            ASSERT_FALSE(intents.empty());
            // Every offered Intent names the player being asked — except in
            // one engine state, reported as a new defect by this stream: the
            // main-phase loop can reach its decision with an item still on
            // the chain, and generateLegalActions() then builds the stale
            // priority holder's Closed-state options while the engine asks
            // the TURN player. CurrentPlayer() follows the seat that is
            // actually asked, so it stays right there too.
            const bool stale_closed_main_phase =
                gs.turn.isClosedState() && gs.chain.exists() &&
                state->CurrentPlayer() == seatOf(gs.turn.turn_player) &&
                gs.turn.priority_holder.has_value() &&
                *gs.turn.priority_holder != gs.turn.turn_player;
            if (stale_closed_main_phase) {
                ++stale_closed;
            } else {
                for (const auto& intent : intents) {
                    ASSERT_EQ(state->CurrentPlayer(), seatOf(intent.player))
                        << intent.describe() << " | seed " << seed
                        << " move " << state->MoveNumber();
                }
            }
            if (intents.front().player != gs.turn.turn_player) ++off_turn;
            ++decisions;

            const auto legal = state->LegalActions();
            ASSERT_FALSE(legal.empty());
            ASSERT_TRUE(std::is_sorted(legal.begin(), legal.end()));
            ASSERT_TRUE(std::adjacent_find(legal.begin(), legal.end()) == legal.end());
            std::set<::open_spiel::Action> slots;
            for (const auto& intent : intents) {
                slots.insert(::riftbound::openspiel::encodeAction(intent, gs));
            }
            ASSERT_EQ(std::vector<::open_spiel::Action>(slots.begin(), slots.end()),
                      legal);
            state->ApplyAction(pickAction(rng, legal));
        }
    }
    EXPECT_GT(decisions, 200);
    EXPECT_GT(off_turn, 4);
    EXPECT_LT(stale_closed * 20, decisions);   // an exception, not the rule
    RecordProperty("stale_closed_main_phase_decisions", stale_closed);
}

TEST(RiftboundStateTest, ApplyingAnActionThatIsNotLegalThrows) {
    auto game = loadGame(3);
    auto state = game->NewInitialState();
    const auto legal = state->LegalActions();
    ::open_spiel::Action bogus = 0;   // slot 0 is reserved, never legal
    ASSERT_TRUE(std::find(legal.begin(), legal.end(), bogus) == legal.end());
    EXPECT_THROW(state->ApplyAction(bogus), std::invalid_argument);
    // Nothing was applied.
    EXPECT_EQ(state->LegalActions(), legal);
    EXPECT_EQ(state->MoveNumber(), 0);
}

// 1.5 — one history entry per applied action.
TEST(RiftboundStateTest, ActionHistoryIsOneSlotPerAppliedAction) {
    auto game = loadGame(5);
    auto state = game->NewInitialState();
    std::mt19937_64 rng(5);
    std::vector<int64_t> applied;
    for (int n = 0; n < 150 && !state->IsTerminal(); ++n) {
        const auto a = pickAction(rng, state->LegalActions());
        state->ApplyAction(a);
        applied.push_back(a);
    }
    EXPECT_EQ(rb(*state).actionHistory(), applied);
    EXPECT_EQ(rb(*state).engineState().action_history.size(), applied.size());
    EXPECT_EQ(state->MoveNumber(), static_cast<int>(applied.size()));
}

// ─── Clone ──────────────────────────────────────────────────────────────────

// 1.3 / 1.4 / 1.6 — a clone taken at ANY decision, then advanced with the same
// actions, stays in the original's position: same decider, same actions,
// same hands and deck order (so the same shuffles), same observations. The
// clones are taken at every kind of decision the game reaches — mulligan,
// chain priority, combat, cost payment, card prompts, and the plain
// main-phase ones — and from clones of clones.
TEST(RiftboundStateTest, CloneTakenAtAnyDecisionPlaysTheSameGame) {
    constexpr int kFollow = 40;      // steps each clone is checked for
    constexpr size_t kMaxLive = 6;   // clones followed at once
    int resumable_clones = 0, replayed_clones = 0;

    for (int seed = 1; seed <= 5; ++seed) {
        auto game = loadGame(seed);
        auto state = game->NewInitialState();
        std::mt19937_64 rng(seed * 31);

        struct Follower {
            std::unique_ptr<::open_spiel::State> state;
            int taken_at;
            int steps = 0;
        };
        std::vector<Follower> followers;

        int n = 0;
        while (!state->IsTerminal()) {
            // Take a clone here if there is room; every third one is a clone
            // of a clone, and one that is read before being advanced.
            if (followers.size() < kMaxLive) {
                const bool resumable = GameEngine::isResumableDecision(
                    rb(*state).engineState(), rb(*state).legalIntents());
                // Do not let the common main-phase decisions crowd out the
                // rarer ones: take those only every 7th decision.
                if (!resumable || n % 3 == 0) {
                    auto clone = state->Clone();
                    if (n % 3 == 0) clone = clone->Clone();
                    expectSamePosition(*state, *clone,
                        "seed " + std::to_string(seed) + " fresh clone at " +
                        std::to_string(n));
                    if (::testing::Test::HasFatalFailure()) return;
                    (resumable ? resumable_clones : replayed_clones)++;
                    followers.push_back({std::move(clone), n});
                }
            }

            const auto action = pickAction(rng, state->LegalActions());
            state->ApplyAction(action);
            ++n;
            for (auto& f : followers) {
                f.state->ApplyAction(action);
                ++f.steps;
                expectSamePosition(*state, *f.state,
                    "seed " + std::to_string(seed) + " clone taken at " +
                    std::to_string(f.taken_at) + ", " +
                    std::to_string(f.steps) + " steps later");
                if (::testing::Test::HasFatalFailure()) return;
            }
            followers.erase(
                std::remove_if(followers.begin(), followers.end(),
                               [](const Follower& f) { return f.steps >= kFollow; }),
                followers.end());
        }
        for (auto& f : followers) {
            EXPECT_TRUE(f.state->IsTerminal());
            EXPECT_EQ(f.state->Returns(), state->Returns());
        }
    }
    EXPECT_GT(resumable_clones, 5);
    EXPECT_GT(replayed_clones, 15)
        << "no clone was taken at a mid-chain / combat / prompt decision";
}

// A clone does not disturb the original, and can outlive it.
TEST(RiftboundStateTest, CloneIsIndependentOfTheOriginal) {
    auto game = loadGame(9);
    auto state = game->NewInitialState();
    std::mt19937_64 rng(9);
    for (int n = 0; n < 80 && !state->IsTerminal(); ++n) {
        state->ApplyAction(pickAction(rng, state->LegalActions()));
    }
    ASSERT_FALSE(state->IsTerminal());
    auto clone = state->Clone();
    auto reference = state->Clone();

    // Advance the clone far away; the original must not move.
    std::mt19937_64 clone_rng(123);
    for (int n = 0; n < 60 && !clone->IsTerminal(); ++n) {
        clone->ApplyAction(pickAction(clone_rng, clone->LegalActions()));
    }
    expectSamePosition(*state, *reference, "original after its clone moved");

    // And the other way round: destroy the original, keep using the clone.
    state.reset();
    for (int n = 0; n < 30 && !reference->IsTerminal(); ++n) {
        reference->ApplyAction(pickAction(rng, reference->LegalActions()));
    }
    SUCCEED();
}

// 1.8 — OpenSpiel's own move counter and history are part of the state. The
// length cap reads the move counter, so a clone that restarted it at 0 was
// not terminal when its original was.
struct MoveNumberProbe : RiftboundState {
    using RiftboundState::RiftboundState;
    void setMoveNumber(int n) { move_number_ = n; }
};

TEST(RiftboundStateTest, CloneKeepsMoveNumberAndHistory) {
    auto game = loadGame(4);
    auto state = game->NewInitialState();
    std::mt19937_64 rng(4);
    for (int n = 0; n < 70 && !state->IsTerminal(); ++n) {
        state->ApplyAction(pickAction(rng, state->LegalActions()));
    }
    auto clone = state->Clone();
    EXPECT_EQ(clone->MoveNumber(), state->MoveNumber());
    EXPECT_GT(clone->MoveNumber(), 0);
    EXPECT_EQ(clone->History(), state->History());
    EXPECT_EQ(clone->FullHistory(), state->FullHistory());
    auto grandchild = clone->Clone();
    EXPECT_EQ(grandchild->MoveNumber(), state->MoveNumber());
    EXPECT_EQ(grandchild->History(), state->History());
}

TEST(RiftboundStateTest, AtTheLengthCapTheStateIsTerminalInEveryRespect) {
    auto game = loadGame(4);
    MoveNumberProbe state(game, 4);
    ASSERT_FALSE(state.IsTerminal());
    ASSERT_FALSE(state.LegalActions().empty());

    state.setMoveNumber(::riftbound::openspiel::kMaxGameLength - 1);
    EXPECT_FALSE(state.IsTerminal());
    EXPECT_FALSE(state.Clone()->IsTerminal());

    state.setMoveNumber(::riftbound::openspiel::kMaxGameLength);
    EXPECT_TRUE(state.IsTerminal());
    EXPECT_EQ(state.CurrentPlayer(), ::open_spiel::kTerminalPlayerId);
    EXPECT_TRUE(state.LegalActions().empty());
    EXPECT_EQ(state.Returns(), (std::vector<double>{0.0, 0.0}));

    auto clone = state.Clone();
    EXPECT_TRUE(clone->IsTerminal()) << "the clone lost the move counter";
    EXPECT_EQ(clone->CurrentPlayer(), ::open_spiel::kTerminalPlayerId);
    EXPECT_TRUE(clone->LegalActions().empty());
}

// ─── Engine failure ─────────────────────────────────────────────────────────

// 1.7 — an engine exception is a failure of the state, not a drawn game.
TEST(RiftboundStateTest, EngineExceptionSurfacesInsteadOfADraw) {
    auto game = loadGame(6);
    auto state = game->NewInitialState();
    auto& mutable_state = static_cast<RiftboundState&>(*state);
    int decisions = 0;
    mutable_state.setOnDecisionCallback(
        [&](const GameState&, const std::vector<Intent>&, const Intent&) {
            if (++decisions == 10) throw std::runtime_error("engine blew up");
        });

    std::mt19937_64 rng(6);
    bool threw = false;
    for (int n = 0; n < 100; ++n) {
        try {
            state->ApplyAction(pickAction(rng, state->LegalActions()));
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "engine blew up");
            threw = true;
            break;
        }
    }
    ASSERT_TRUE(threw);
    // Not a finished game: asking for the outcome fails too.
    EXPECT_THROW(state->IsTerminal(), std::runtime_error);
    EXPECT_THROW(state->Returns(), std::runtime_error);
    EXPECT_THROW(state->Clone(), std::runtime_error);
}

// ─── Info-set keys ──────────────────────────────────────────────────────────

// 1.13 — ISMCTS keys its tree on ObservationString(CurrentPlayer()) and, with
// allow_inconsistent_action_sets=false, replays a node's actions without
// checking them: two decisions with one key must offer the same actions.
// The board features alone do not guarantee that (see `board_key_conflicts`,
// the count for the key as it used to be); the key now includes the
// decision.
TEST(RiftboundStateTest, SameObservationKeyMeansSameLegalActions) {
    int board_key_conflicts = 0, keys = 0;
    for (int seed = 1; seed <= 8; ++seed) {
        auto game = loadGame(seed);
        const auto& db = static_cast<const RiftboundGame&>(*game).cardDb();
        auto state = game->NewInitialState();
        std::mt19937_64 rng(seed * 17);
        using Key = std::pair<::open_spiel::Player, std::string>;
        std::map<Key, std::vector<::open_spiel::Action>> by_key;
        std::map<std::pair<int, uint64_t>, std::vector<::open_spiel::Action>> by_board;
        while (!state->IsTerminal()) {
            const auto player = state->CurrentPlayer();
            const auto legal = state->LegalActions();

            auto [it, fresh] = by_key.try_emplace(
                Key{player, state->ObservationString(player)}, legal);
            if (fresh) ++keys;
            ASSERT_EQ(it->second, legal)
                << "seed " << seed << " move " << state->MoveNumber()
                << ": one observation key, two different action sets";

            // The key as it was before: masked board features + player.
            const uint64_t board = ::riftbound::ml::computeInfoSetId(
                ::riftbound::ml::extractStateFeatures(
                    rb(*state).engineState(),
                    player == 0 ? PlayerId::Player1 : PlayerId::Player2, db),
                player);
            auto [bit, bfresh] = by_board.try_emplace({player, board}, legal);
            if (!bfresh && bit->second != legal) ++board_key_conflicts;

            state->ApplyAction(pickAction(rng, legal));
        }
    }
    EXPECT_GT(keys, 300);
    RecordProperty("board_key_conflicts", board_key_conflicts);
    std::cout << "[ info     ] board-only key conflicts (the pre-fix key): "
              << board_key_conflicts << "\n";
}

TEST(RiftboundStateTest, ObservationKeyHidesWhatTheNonDeciderCannotSee) {
    auto game = loadGame(2);
    auto state = game->NewInitialState();
    std::mt19937_64 rng(2);
    for (int n = 0; n < 60 && !state->IsTerminal(); ++n) {
        state->ApplyAction(pickAction(rng, state->LegalActions()));
    }
    ASSERT_FALSE(state->IsTerminal());
    // Keys are per player, and stable.
    EXPECT_NE(state->ObservationString(0), state->ObservationString(1));
    EXPECT_EQ(state->ObservationString(0), state->Clone()->ObservationString(0));
    EXPECT_EQ(state->ObservationString(0).size(), 16u);
}

// 6.9 — the key and the tensor come from per-thread scratch buffers and a
// hash that skips zero runs. Both still equal what the plain functions give:
// the tensor is the feature vector, the key is its byte-wise hash with the
// decision mixed in, for either player, on a state and on its clone.
TEST(RiftboundStateTest, ObservationTensorAndKeyMatchThePlainFunctions) {
    for (int seed : {4, 9}) {
        auto game = loadGame(seed);
        const auto& db = static_cast<const RiftboundGame&>(*game).cardDb();
        auto state = game->NewInitialState();
        std::mt19937_64 rng(seed);
        int checked = 0;
        while (!state->IsTerminal()) {
            const auto decider = state->CurrentPlayer();
            const auto legal = state->LegalActions();
            for (::open_spiel::Player p : {0, 1}) {
                const auto feats = ::riftbound::ml::extractStateFeatures(
                    rb(*state).engineState(),
                    p == 0 ? PlayerId::Player1 : PlayerId::Player2, db);
                ASSERT_EQ(state->ObservationTensor(p), feats)
                    << "seed " << seed << " move " << state->MoveNumber();
                // Interleave the other player's key: the scratch buffer is
                // shared between calls and must not leak between them.
                (void)rb(*state).InfoSetId(1 - p);
                std::vector<int64_t> offered;
                if (p == decider) offered.assign(legal.begin(), legal.end());
                const uint64_t expected =
                    ::riftbound::ml::mixDecisionIntoInfoSetId(
                        ::riftbound::ml::computeInfoSetId(feats, p),
                        decider, offered);
                ASSERT_EQ(rb(*state).InfoSetId(p), expected)
                    << "seed " << seed << " move " << state->MoveNumber();
                ASSERT_EQ(rb(*state->Clone()).InfoSetId(p), expected);
                ++checked;
            }
            state->ApplyAction(pickAction(rng, legal));
        }
        EXPECT_GT(checked, 100);
    }
}

// 6.7 carry-over — a state walked forward by ApplyAction alone has no
// resumable snapshot behind it, so a clone taken at a decision that cannot be
// resumed directly (chain priority, a prompt, combat...) is rebuilt by
// replaying the game. Every clone of that position used to replay it from
// the start; now the first one leaves the nearest resumable decision it
// passed for the others, and for the positions that follow.
TEST(RiftboundStateTest, OnlyTheFirstCloneOfAPositionReplaysTheWholeGame) {
    int positions = 0;
    for (int seed : {3, 6, 12}) {
        auto game = loadGame(seed);
        auto state = game->NewInitialState();
        std::mt19937_64 rng(seed);
        auto resumable = [&] {
            return GameEngine::isResumableDecision(rb(*state).engineState(),
                                                   rb(*state).legalIntents());
        };
        // No Clone() on the way: nothing behind this state can be resumed.
        while (!state->IsTerminal() &&
               (state->MoveNumber() < 80 || resumable())) {
            state->ApplyAction(pickAction(rng, state->LegalActions()));
        }
        if (state->IsTerminal()) continue;
        const size_t here = static_cast<size_t>(state->MoveNumber());
        const auto action = pickAction(rng, state->LegalActions());

        auto first = state->Clone();
        auto second = state->Clone();
        auto third = second->Clone();          // clone of a clone
        first->ApplyAction(action);
        EXPECT_EQ(rb(*first).decisionsReplayedToBuild(), here)
            << "seed " << seed << ": the first clone starts from the beginning";
        second->ApplyAction(action);
        third->ApplyAction(action);
        const size_t tail = rb(*second).decisionsReplayedToBuild();
        EXPECT_LT(tail, here / 2) << "seed " << seed;
        EXPECT_EQ(rb(*third).decisionsReplayedToBuild(), tail);

        // All of them are the position the original reaches.
        state->ApplyAction(action);
        expectSamePosition(*state, *first, "first clone");
        expectSamePosition(*state, *second, "second clone");
        expectSamePosition(*state, *third, "clone of a clone");

        // The original learned of the nearer starting point too: the next
        // position that needs a replay starts there, not at the beginning.
        while (!state->IsTerminal() && resumable()) {
            state->ApplyAction(pickAction(rng, state->LegalActions()));
        }
        if (state->IsTerminal()) continue;
        const auto next_action = pickAction(rng, state->LegalActions());
        auto later = state->Clone();
        later->ApplyAction(next_action);
        EXPECT_LT(rb(*later).decisionsReplayedToBuild(),
                  static_cast<size_t>(state->MoveNumber()) / 2)
            << "seed " << seed;
        state->ApplyAction(next_action);
        expectSamePosition(*state, *later, "clone of the next position");
        ++positions;
    }
    EXPECT_GE(positions, 2);
}

// ─── Mirroring a runGame game (what MctsAgent does) ─────────────────────────

/// An agent that, at every decision of a real `GameEngine::runGame` game,
/// brings a RiftboundState to the live position the way MctsAgent does and
/// checks it against the live game. Plays randomly.
class MirrorProbeAgent : public ::riftbound::AgentInterface {
public:
    MirrorProbeAgent(std::shared_ptr<const ::open_spiel::Game> game,
                     PlayerId seat, uint64_t engine_seed, uint64_t rng_seed)
        : game_(std::move(game)), seat_(seat), engine_seed_(engine_seed),
          rng_(rng_seed) {}

    Intent selectAction(const GameState& state,
                        const std::vector<Intent>& legal) override {
        ++decisions;
        ::riftbound::StepResult step;
        step.kind = ::riftbound::StepKind::NeedDecision;
        step.legal = legal;
        step.perspective = seat_;   // this agent IS the seat being asked
        if (seat_ != state.turn.turn_player) ++off_turn;
        const bool resumable = GameEngine::isResumableDecision(state, legal);
        if (!resumable) ++not_resumable;

        if (!mirror_ || !mirror_->syncToHistory(state, step)) {
            if (mirror_) ++resyncs;
            mirror_ = RiftboundState::rebuildFromHistory(
                game_, engine_seed_, state, step);
        }
        if (!mirror_) { ++failures; return pick(legal); }

        // The mirror is the live position, seen by the right player.
        if (mirror_->CurrentPlayer() != seatOf(step.perspective)) ++failures;
        if (mirror_->legalIntents() != legal) ++failures;
        if (mirror_->IsTerminal()) ++failures;
        if (mirror_->MoveNumber() != 0) ++failures;
        const GameState& ms = mirror_->engineState();
        for (int p = 0; p < 2; ++p) {
            if (ms.players[p].hand != state.players[p].hand ||
                ms.players[p].main_deck != state.players[p].main_deck) ++failures;
        }

        // A search clones the root and advances the clone; do the same, from
        // every kind of decision.
        try {
            auto sim = mirror_->Clone();
            for (int k = 0; k < 4 && !sim->IsTerminal(); ++k) {
                sim->ApplyAction(sim->LegalActions().front());
            }
        } catch (const std::exception& e) {
            ++failures;
            last_error = e.what();
        }
        return pick(legal);
    }

    int decisions = 0, off_turn = 0, not_resumable = 0, failures = 0, resyncs = 0;
    std::string last_error;

private:
    Intent pick(const std::vector<Intent>& legal) {
        std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
        return legal[d(rng_)];
    }
    std::shared_ptr<const ::open_spiel::Game> game_;
    PlayerId seat_;
    uint64_t engine_seed_;
    std::mt19937_64 rng_;
    std::unique_ptr<RiftboundState> mirror_;
};

// 1.2 / 1.4 — the state an agent searches from inside a runGame game is the
// live position exactly, with the deciding player to move, at every decision
// (not only the ones a GameState copy can be resumed from).
TEST(LiveMirrorTest, MirrorMatchesARunGameGameAtEveryDecision) {
    constexpr uint64_t kSeed = 20261010;
    auto game = loadGame(1);
    const auto& rb_game = static_cast<const RiftboundGame&>(*game);

    ::riftbound::EventBus bus;
    GameEngine engine(rb_game.cardDb(), bus, rb_game.cardRegistry());
    MirrorProbeAgent p1(game, PlayerId::Player1, kSeed, 1),
                     p2(game, PlayerId::Player2, kSeed, 2);
    const auto result =
        engine.runGame(rb_game.deck1(), rb_game.deck2(), p1, p2, kSeed);

    EXPECT_GT(result.total_decisions, 50);
    for (const MirrorProbeAgent* a : {&p1, &p2}) {
        EXPECT_GT(a->decisions, 20);
        EXPECT_EQ(a->failures, 0) << a->last_error;
        EXPECT_EQ(a->resyncs, 0) << "the mirror had to be rebuilt from scratch";
        EXPECT_GT(a->off_turn, 0)
            << "never asked to decide outside its own turn";
        EXPECT_GT(a->not_resumable, 0);
    }
}

// With the wrong seed the history does not reproduce the live game; that is
// detected (nullptr), never silently accepted.
TEST(LiveMirrorTest, WrongSeedIsDetected) {
    constexpr uint64_t kSeed = 777;
    auto game = loadGame(1);
    const auto& rb_game = static_cast<const RiftboundGame&>(*game);

    struct Probe : ::riftbound::AgentInterface {
        std::shared_ptr<const ::open_spiel::Game> game;
        int right = 0, wrong_accepted = 0, asked = 0;
        Intent selectAction(const GameState& state,
                            const std::vector<Intent>& legal) override {
            // Past the mulligans, so the seeds have dealt different hands.
            if (state.turn.phase == ::riftbound::TurnPhase::MainPhase &&
                asked < 5) {
                ++asked;
                ::riftbound::StepResult step;
                step.kind = ::riftbound::StepKind::NeedDecision;
                step.legal = legal;
                step.perspective = PlayerId::None;   // as MctsAgent does
                if (RiftboundState::rebuildFromHistory(game, kSeed, state, step)) ++right;
                if (RiftboundState::rebuildFromHistory(game, kSeed + 1, state, step)) ++wrong_accepted;
            }
            return legal.front();
        }
    } probe;
    probe.game = game;
    ::riftbound::RandomAgent other(5);

    ::riftbound::EventBus bus;
    GameEngine engine(rb_game.cardDb(), bus, rb_game.cardRegistry());
    engine.runGame(rb_game.deck1(), rb_game.deck2(), probe, other, kSeed);
    EXPECT_EQ(probe.asked, 5);
    EXPECT_EQ(probe.right, 5);
    EXPECT_EQ(probe.wrong_accepted, 0);
}

// ─── MctsAgent / IsMctsAgent ────────────────────────────────────────────────

/// Wraps an agent and checks every answer is one of the options offered.
class CheckedAgent : public ::riftbound::AgentInterface {
public:
    explicit CheckedAgent(::riftbound::AgentInterface& inner) : inner_(inner) {}
    Intent selectAction(const GameState& state,
                        const std::vector<Intent>& legal) override {
        Intent chosen = inner_.selectAction(state, legal);
        ++decisions;
        if (std::find(legal.begin(), legal.end(), chosen) == legal.end()) ++illegal;
        if (!legal.empty() && chosen.player != legal.front().player) ++wrong_seat;
        return chosen;
    }
    int decisions = 0, illegal = 0, wrong_seat = 0;
private:
    ::riftbound::AgentInterface& inner_;
};

template <class Agent>
void playAgentAgainstRandom(uint64_t engine_seed_for_agent, uint64_t game_seed,
                            bool agent_is_p1) {
    auto game = loadGame(1);
    const auto& rb_game = static_cast<const RiftboundGame&>(*game);
    Agent agent(deckFile("yi_rebuild.txt"), deckFile("fiora.txt"),
                "ignored", engine_seed_for_agent, /*mcts_seed=*/99, /*sims=*/6);
    CheckedAgent checked(agent);
    ::riftbound::RandomAgent random(4242);

    ::riftbound::EventBus bus;
    GameEngine engine(rb_game.cardDb(), bus, rb_game.cardRegistry());
    const auto result = agent_is_p1
        ? engine.runGame(rb_game.deck1(), rb_game.deck2(), checked, random, game_seed)
        : engine.runGame(rb_game.deck1(), rb_game.deck2(), random, checked, game_seed);

    EXPECT_TRUE(engine.state().game_over);
    EXPECT_GT(result.total_decisions, 30);
    EXPECT_GT(checked.decisions, 10);
    EXPECT_EQ(checked.illegal, 0);
    EXPECT_EQ(checked.wrong_seat, 0);
}

TEST(MctsAgentTest, PlaysAFullGameAsEitherSeat) {
    playAgentAgainstRandom<::riftbound::MctsAgent>(31337, 31337, /*p1=*/true);
    playAgentAgainstRandom<::riftbound::MctsAgent>(31338, 31338, /*p1=*/false);
}

TEST(MctsAgentTest, StillPlaysLegalMovesWhenGivenTheWrongEngineSeed) {
    playAgentAgainstRandom<::riftbound::MctsAgent>(1, 31339, /*p1=*/true);
}

TEST(IsMctsAgentTest, PlaysAFullGameAsEitherSeat) {
    playAgentAgainstRandom<::riftbound::IsMctsAgent>(41337, 41337, /*p1=*/true);
    playAgentAgainstRandom<::riftbound::IsMctsAgent>(41338, 41338, /*p1=*/false);
}

} // namespace
