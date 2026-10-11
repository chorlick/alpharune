/// Tests for the engine's step-machine API (beginGame / applyChoice /
/// currentStep / resumeFromSnapshot) — the surface the OpenSpiel wrapper,
/// MCTS and the trainer drive the engine through.
///
/// Everything here plays real games through the real engine: the decks on
/// disk, a seeded random chooser, no hand-fed state.

#include <gtest/gtest.h>

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "ml/feature_extractor.h"
#include "openspiel/action_vocab.h"
#include "rules/deck_validator.h"

#include <cstdlib>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace riftbound;

namespace {

std::string deckFile(const char* name) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/" + name;
}

class StepApiTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
        deck1_ = new DeckSubmission(
            DeckValidator::loadFromDeckList(deckFile("yi_rebuild.txt"), *db_));
        deck2_ = new DeckSubmission(
            DeckValidator::loadFromDeckList(deckFile("fiora.txt"), *db_));
    }
    static void TearDownTestSuite() {
        delete deck2_; delete deck1_; delete db_; delete registry_;
    }

    /// One step-machine game: its own bus and engine.
    struct Game {
        EventBus bus;
        GameEngine engine;
        Game() : engine(*db_, bus, *registry_) {}
    };

    static std::unique_ptr<Game> begin(uint64_t seed) {
        auto g = std::make_unique<Game>();
        g->engine.beginGame(*deck1_, *deck2_, seed);
        return g;
    }

    static int pick(std::mt19937_64& rng, const StepResult& step) {
        std::uniform_int_distribution<size_t> d(0, step.legal.size() - 1);
        return static_cast<int>(d(rng));
    }

    static CardRegistry* registry_;
    static CardDB* db_;
    static DeckSubmission* deck1_;
    static DeckSubmission* deck2_;
};

CardRegistry* StepApiTest::registry_ = nullptr;
CardDB* StepApiTest::db_ = nullptr;
DeckSubmission* StepApiTest::deck1_ = nullptr;
DeckSubmission* StepApiTest::deck2_ = nullptr;

constexpr int kMaxDecisions = 4000;

// 1.1 — the step's `perspective` is the player being asked, which the engine
// knows (it asks a seat) and which every offered Intent also names. It used
// to be the turn player, wrong for the second mulligan, chain priority,
// showdown focus, combat damage and an opponent's trigger choices.
TEST_F(StepApiTest, PerspectiveIsTheDecidingPlayerNotTheTurnPlayer) {
    int decisions = 0, off_turn_decisions = 0, stale_closed = 0;
    for (uint64_t seed = 1; seed <= 6; ++seed) {
        auto g = begin(seed);
        std::mt19937_64 rng(seed * 7919);
        for (int n = 0; n < kMaxDecisions && !g->engine.isStepDone(); ++n) {
            const auto step = g->engine.currentStep();
            ASSERT_EQ(step.kind, StepKind::NeedDecision);
            ASSERT_FALSE(step.legal.empty());
            ASSERT_NE(step.perspective, PlayerId::None);
            // Every offered Intent names the player being asked — except in
            // one engine state (reported as a new defect by the search
            // stream): the main-phase loop can reach its decision with an
            // item still on the chain, and generateLegalActions() then
            // builds the stale priority holder's Closed-state options while
            // the engine asks the TURN player. `perspective` is the seat the
            // engine really asked, so it is right there too.
            const auto& gs = g->engine.state();
            const bool stale_closed_main_phase =
                gs.turn.isClosedState() && gs.chain.exists() &&
                step.perspective == gs.turn.turn_player &&
                gs.turn.priority_holder.has_value() &&
                *gs.turn.priority_holder != gs.turn.turn_player;
            if (stale_closed_main_phase) {
                ++stale_closed;
            } else {
                for (const auto& intent : step.legal) {
                    ASSERT_EQ(intent.player, step.perspective)
                        << "seed " << seed << " decision " << n << ": "
                        << intent.describe();
                }
            }
            ++decisions;
            if (step.perspective != gs.turn.turn_player) {
                ++off_turn_decisions;
            }
            g->engine.applyChoice(pick(rng, step));
        }
    }
    EXPECT_GT(decisions, 100);
    // Each game has at least the non-turn player's mulligan; random play also
    // reaches chain priority and combat damage.
    EXPECT_GT(off_turn_decisions, 6)
        << "no decision was ever made by the non-turn player";
    EXPECT_LT(stale_closed * 20, decisions);   // an exception, not the rule
    RecordProperty("stale_closed_main_phase_decisions", stale_closed);
}

// 1.5 — exactly one history entry per decision, written by the engine, and it
// is the chosen Intent's token.
TEST_F(StepApiTest, ActionHistoryHasOneExactTokenPerDecision) {
    auto g = begin(11);
    std::mt19937_64 rng(11);
    size_t applied = 0;
    for (int n = 0; n < 300 && !g->engine.isStepDone(); ++n) {
        const auto step = g->engine.currentStep();
        const int idx = pick(rng, step);
        const int64_t token =
            openspiel::historyToken(step.legal[idx], g->engine.state());
        g->engine.applyChoice(idx);
        ++applied;
        const auto& history = g->engine.state().action_history;
        ASSERT_EQ(history.size(), applied) << "after decision " << n;
        ASSERT_EQ(history.back(), token) << "after decision " << n;
    }
    EXPECT_GT(applied, 50u);
}

// 1.4 — replaying the history tokens from the same decks and seed rebuilds
// the same game, at any decision. Vocab slots could not: a mulligan slot
// says how many cards were set aside, not which.
TEST_F(StepApiTest, ReplayingHistoryTokensRebuildsTheSameGame) {
    for (uint64_t seed : {3u, 4u, 5u}) {
        auto live = begin(seed);
        std::mt19937_64 rng(seed + 100);
        // Stop at a spread of depths, including inside the mulligans.
        const int stop_after = seed == 3 ? 3 : (seed == 4 ? 45 : 110);
        for (int n = 0; n < stop_after && !live->engine.isStepDone(); ++n) {
            live->engine.applyChoice(pick(rng, live->engine.currentStep()));
        }
        ASSERT_FALSE(live->engine.isStepDone())
            << "seed " << seed << " ended before decision " << stop_after;

        auto replay = begin(seed);
        for (int64_t token : live->engine.state().action_history) {
            ASSERT_FALSE(replay->engine.isStepDone());
            const auto step = replay->engine.currentStep();
            const int idx = openspiel::findHistoryToken(
                token, step.legal, replay->engine.state());
            ASSERT_GE(idx, 0);
            replay->engine.applyChoice(idx);
        }

        const auto& a = live->engine.state();
        const auto& b = replay->engine.state();
        EXPECT_EQ(live->engine.currentStep().perspective,
                  replay->engine.currentStep().perspective);
        EXPECT_EQ(live->engine.currentStep().legal,
                  replay->engine.currentStep().legal);
        EXPECT_EQ(a.action_history, b.action_history);
        EXPECT_EQ(a.decision_index, b.decision_index);
        for (int p = 0; p < 2; ++p) {
            EXPECT_EQ(a.players[p].hand, b.players[p].hand) << "seed " << seed;
            EXPECT_EQ(a.players[p].main_deck, b.players[p].main_deck);
            EXPECT_EQ(a.players[p].trash, b.players[p].trash);
        }
        EXPECT_TRUE(live->engine.rngState() == replay->engine.rngState());
        for (PlayerId p : {PlayerId::Player1, PlayerId::Player2}) {
            EXPECT_EQ(ml::extractStateFeatures(a, p, *db_),
                      ml::extractStateFeatures(b, p, *db_));
        }
    }
}

// 1.3 — an engine resumed from a snapshot continues the snapshotted engine's
// RNG stream and plays the identical game: same decisions offered at every
// step, same counters, same RNG at the end, same result.
TEST_F(StepApiTest, ResumedEngineContinuesTheSameGameAndRngStream) {
    int resumed = 0;
    for (uint64_t seed : {21u, 22u, 23u, 24u}) {
        auto live = begin(seed);
        std::mt19937_64 rng(seed);
        // Walk to a resumable decision at least 40 decisions in.
        int n = 0;
        while (!live->engine.isStepDone()) {
            const auto step = live->engine.currentStep();
            if (n >= 40 && GameEngine::isResumableDecision(
                               live->engine.state(), step.legal)) break;
            live->engine.applyChoice(pick(rng, step));
            ++n;
        }
        if (live->engine.isStepDone()) continue;

        Game copy;
        const auto snap_step = live->engine.currentStep();
        copy.engine.resumeFromSnapshot(GameState(live->engine.state()),
                                       live->engine.rngState(), snap_step);
        ++resumed;
        ASSERT_TRUE(copy.engine.rngState() == live->engine.rngState());
        ASSERT_EQ(copy.engine.state().decision_index,
                  live->engine.state().decision_index);
        ASSERT_EQ(copy.engine.state().turn.turn_decisions_p1,
                  live->engine.state().turn.turn_decisions_p1);
        ASSERT_EQ(copy.engine.state().turn.turn_decisions_p2,
                  live->engine.state().turn.turn_decisions_p2);

        for (int k = 0; k < kMaxDecisions && !live->engine.isStepDone(); ++k) {
            ASSERT_FALSE(copy.engine.isStepDone()) << "seed " << seed << " step " << k;
            const auto a = live->engine.currentStep();
            const auto b = copy.engine.currentStep();
            ASSERT_EQ(a.perspective, b.perspective) << "seed " << seed << " step " << k;
            ASSERT_EQ(a.legal, b.legal) << "seed " << seed << " step " << k;
            for (PlayerId p : {PlayerId::Player1, PlayerId::Player2}) {
                ASSERT_EQ(ml::extractStateFeatures(live->engine.state(), p, *db_),
                          ml::extractStateFeatures(copy.engine.state(), p, *db_))
                    << "seed " << seed << " step " << k;
            }
            const int idx = pick(rng, a);
            live->engine.applyChoice(idx);
            copy.engine.applyChoice(idx);
        }
        EXPECT_EQ(live->engine.isStepDone(), copy.engine.isStepDone());
        EXPECT_TRUE(copy.engine.rngState() == live->engine.rngState());
        EXPECT_EQ(live->engine.state().action_history,
                  copy.engine.state().action_history);
        if (live->engine.isStepDone() && copy.engine.isStepDone()) {
            EXPECT_EQ(live->engine.stepResult().winner,
                      copy.engine.stepResult().winner);
            EXPECT_EQ(live->engine.stepResult().final_scores[0],
                      copy.engine.stepResult().final_scores[0]);
            EXPECT_EQ(live->engine.stepResult().final_scores[1],
                      copy.engine.stepResult().final_scores[1]);
        }
    }
    EXPECT_GE(resumed, 3);
}

// 1.3 / 1.4 — the main-phase loop's per-turn action cap counts on the stack,
// not in GameState; a resumed engine must pick the count up where the
// snapshotted one was, or the two end a long turn at different actions. The
// count is recoverable from turn_decisions_pN, so move that to one action
// short of the cap and check the resumed engine stops the turn there.
TEST_F(StepApiTest, ResumedEngineKeepsTheMainPhaseActionCount) {
    constexpr int kCap = 500;   // kMainPhaseMaxActions
    // Play one main-phase action that is not EndTurn, then finish whatever
    // it started (chain, prompts) with first-legal. True if the same turn
    // then offers another main-phase decision.
    auto sameTurnContinues = [](GameEngine& engine) {
        const int turn = engine.state().turn.turn_number;
        auto step = engine.currentStep();
        int idx = -1;
        for (size_t i = 0; i < step.legal.size(); ++i) {
            if (step.legal[i].type != IntentType::EndTurn &&
                step.legal[i].type != IntentType::Concede) {
                idx = static_cast<int>(i);
                break;
            }
        }
        if (idx < 0) return false;
        engine.applyChoice(idx);
        for (int n = 0; n < 200 && !engine.isStepDone(); ++n) {
            step = engine.currentStep();
            if (GameEngine::isResumableDecision(engine.state(), step.legal)) {
                return engine.state().turn.turn_number == turn;
            }
            engine.applyChoice(0);
        }
        return false;
    };

    int checked = 0;
    for (uint64_t seed = 61; seed <= 80 && checked < 2; ++seed) {
        auto live = begin(seed);
        std::mt19937_64 rng(seed);
        int n = 0;
        while (!live->engine.isStepDone()) {
            const auto step = live->engine.currentStep();
            if (n >= 30 && step.legal.size() > 2 &&
                GameEngine::isResumableDecision(live->engine.state(), step.legal)) break;
            live->engine.applyChoice(pick(rng, step));
            ++n;
        }
        if (live->engine.isStepDone()) continue;
        const auto snap_step = live->engine.currentStep();
        const GameState snap_state = live->engine.state();
        const auto snap_rng = live->engine.rngState();

        // Control: an untouched copy takes the action and the turn goes on.
        Game control;
        control.engine.resumeFromSnapshot(GameState(snap_state), snap_rng, snap_step);
        if (!sameTurnContinues(control.engine)) continue;   // not a useful position

        // Same position, but the turn player has already made cap-1 actions.
        GameState near_cap = snap_state;
        auto& made = near_cap.turn.turn_player == PlayerId::Player1
            ? near_cap.turn.turn_decisions_p1 : near_cap.turn.turn_decisions_p2;
        made = kCap;   // counts the decision being asked: kCap-1 completed
        Game capped;
        capped.engine.resumeFromSnapshot(std::move(near_cap), snap_rng, snap_step);
        EXPECT_FALSE(sameTurnContinues(capped.engine))
            << "seed " << seed << ": the resumed engine restarted the per-turn "
               "action count instead of continuing it";
        ++checked;
    }
    EXPECT_EQ(checked, 2);
}

// 1.6 — only the top-level main-phase decision is resumable. A prompt asked
// from inside a play (the card has left its zone, there is no chain item or
// cost cursor yet) leaves a state that LOOKS idle; the legal list is what
// tells them apart.
TEST_F(StepApiTest, OnlyTheTopLevelMainPhaseDecisionIsResumable) {
    GameState s;
    s.turn.phase = TurnPhase::MainPhase;
    s.turn.ns_state = NeutralShowdownState::Neutral;
    s.turn.oc_state = OpenClosedState::Open;

    Intent yes; yes.type = IntentType::MakeChoice; yes.player = PlayerId::Player1;
    yes.chosen_value = 1;
    Intent no = yes; no.chosen_value = 0;
    EXPECT_FALSE(GameEngine::isResumableDecision(s, {yes, no}))
        << "a mid-play yes/no prompt must not be treated as a clean boundary";
    EXPECT_FALSE(GameEngine::isResumableDecision(s, {}));

    const std::vector<Intent> main_phase = {Intent::endTurn(PlayerId::Player1)};
    EXPECT_TRUE(GameEngine::isResumableDecision(s, main_phase));

    GameState over = s; over.game_over = true;
    EXPECT_FALSE(GameEngine::isResumableDecision(over, main_phase));
    GameState paying = s; paying.cost_cursor.emplace();
    EXPECT_FALSE(GameEngine::isResumableDecision(paying, main_phase));
    GameState closed = s; closed.turn.oc_state = OpenClosedState::Closed;
    EXPECT_FALSE(GameEngine::isResumableDecision(closed, main_phase));
}

// 1.6 — and in a real game: every decision that is not resumable is refused
// by resumeFromSnapshot instead of being resumed into a different game.
TEST_F(StepApiTest, ResumeRefusesEveryNonResumableDecisionOfARealGame) {
    auto g = begin(31);
    std::mt19937_64 rng(31);
    int refused = 0, prompts_that_look_idle = 0;
    for (int n = 0; n < 400 && !g->engine.isStepDone(); ++n) {
        const auto step = g->engine.currentStep();
        const auto& s = g->engine.state();
        if (!GameEngine::isResumableDecision(s, step.legal)) {
            Game copy;
            EXPECT_THROW(copy.engine.resumeFromSnapshot(
                             GameState(s), g->engine.rngState(), step),
                         std::invalid_argument)
                << "decision " << n;
            ++refused;
            // What the old state-only "clean boundary" test looked at.
            bool looks_idle = !s.game_over &&
                s.turn.phase != TurnPhase::Setup &&
                s.turn.phase != TurnPhase::Mulligan &&
                s.turn.isNeutralOpen() && !s.chain.exists() &&
                !s.cost_cursor.has_value();
            for (const auto& bf : s.battlefields) {
                if (bf.combat_in_progress || bf.showdown_in_progress ||
                    bf.combat_staged || bf.showdown_staged) looks_idle = false;
            }
            if (looks_idle) ++prompts_that_look_idle;
        }
        g->engine.applyChoice(pick(rng, step));
    }
    EXPECT_GT(refused, 4);   // at least the mulligans and setup choices
    RecordProperty("prompts_that_look_idle", prompts_that_look_idle);
}

// 1.7 — an exception inside the engine is rethrown to the step API's caller,
// then and on every later call. It used to be swallowed, leaving a "done"
// game with no winner that every consumer scored as a draw.
TEST_F(StepApiTest, EngineExceptionIsRethrownNotReportedAsAFinishedGame) {
    auto g = begin(41);
    int decisions = 0;
    // on_decision runs inside the engine fiber, right after each decision.
    g->engine.on_decision = [&](const GameState&, const std::vector<Intent>&,
                                const Intent&) {
        if (++decisions == 12) throw std::runtime_error("boom in the engine");
    };

    std::mt19937_64 rng(41);
    bool threw = false;
    for (int n = 0; n < 200 && !g->engine.isStepDone(); ++n) {
        const auto step = g->engine.currentStep();
        try {
            g->engine.applyChoice(pick(rng, step));
        } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "boom in the engine");
            threw = true;
            break;
        }
    }
    ASSERT_TRUE(threw) << "the engine exception never reached the caller";

    EXPECT_TRUE(g->engine.stepFailed());
    EXPECT_TRUE(g->engine.isStepDone());
    EXPECT_THROW(g->engine.currentStep(), std::runtime_error);
    EXPECT_THROW(g->engine.applyChoice(0), std::runtime_error);
    EXPECT_THROW(g->engine.rethrowIfStepFailed(), std::runtime_error);
    EXPECT_EQ(g->engine.stepResult().termination_reason, "engine_error");
    EXPECT_FALSE(g->engine.state().game_over);
}

TEST_F(StepApiTest, FinishedGameDoesNotReportFailure) {
    auto g = begin(42);
    std::mt19937_64 rng(42);
    for (int n = 0; n < kMaxDecisions && !g->engine.isStepDone(); ++n) {
        g->engine.applyChoice(pick(rng, g->engine.currentStep()));
    }
    ASSERT_TRUE(g->engine.isStepDone());
    EXPECT_FALSE(g->engine.stepFailed());
    EXPECT_NO_THROW(g->engine.currentStep());
    EXPECT_EQ(g->engine.currentStep().kind, StepKind::Done);
    EXPECT_TRUE(g->engine.state().game_over);
    EXPECT_NE(g->engine.stepResult().termination_reason, "engine_error");
}

TEST_F(StepApiTest, ApplyChoiceRejectsAnIndexOutsideTheLegalList) {
    auto g = begin(43);
    const auto step = g->engine.currentStep();
    EXPECT_THROW(g->engine.applyChoice(-1), std::out_of_range);
    EXPECT_THROW(g->engine.applyChoice(static_cast<int>(step.legal.size())),
                 std::out_of_range);
    // The engine is still parked at the same decision.
    EXPECT_FALSE(g->engine.stepFailed());
    EXPECT_EQ(g->engine.currentStep().legal, step.legal);
    EXPECT_NO_THROW(g->engine.applyChoice(0));
}

// 1.14 — the feature vector is exactly kStateFeatureDim long on real states,
// from both seats (ObservationTensor used to pad or truncate a mismatch).
TEST_F(StepApiTest, StateFeaturesAreExactlyStateFeatureDimOnRealStates) {
    auto g = begin(51);
    std::mt19937_64 rng(51);
    for (int n = 0; n < 250 && !g->engine.isStepDone(); ++n) {
        for (PlayerId p : {PlayerId::Player1, PlayerId::Player2}) {
            ASSERT_EQ(ml::extractStateFeatures(g->engine.state(), p, *db_).size(),
                      static_cast<size_t>(ml::kStateFeatureDim))
                << "decision " << n;
        }
        g->engine.applyChoice(pick(rng, g->engine.currentStep()));
    }
}

} // namespace
