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
#include "agents/random_player_agent.h"
#include "rules/random_deck.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "engine/knowledge_tracker.h"
#include "ml/cfr_util.h"
#include "ml/feature_extractor.h"

#include "open_spiel/spiel.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
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
        // What the decision asks (testing phase T1) travels with a clone.
        if (!a.IsTerminal()) {
            ASSERT_TRUE(rb(a).describeDecision(p) == rb(b).describeDecision(p)) << where;
        }
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
    EXPECT_EQ(game->NumDistinctActions(), ::riftbound::openspiel::kNumActions);
    EXPECT_EQ(game->NumDistinctActions(),
              ::riftbound::openspiel::kNumSlots * ::riftbound::openspiel::kTwinPlanes);
    EXPECT_EQ(game->ObservationTensorShape(),
              std::vector<int>{::riftbound::ml::kStateFeatureDim});
    EXPECT_EQ(game->MaxGameLength(), ::riftbound::openspiel::kMaxGameLength);
    auto state = game->NewInitialState();
    EXPECT_EQ(state->ObservationTensor(0).size(),
              static_cast<size_t>(::riftbound::ml::kStateFeatureDim));
}

// ─── CurrentPlayer / LegalActions ───────────────────────────────────────────

// 1.1 — CurrentPlayer() is whoever the engine is asking. 1.11 — LegalActions()
// is ascending and duplicate-free. 1.12 — it holds one id per distinct legal
// Intent: nothing the engine offers is dropped.
// V1-G4: the game_mode parameter. "duel" (the default) asks no setup
// battlefield; "match" asks each player (CR 487.5 / 488.5).
TEST(RiftboundStateTest, TheGameModeParameterDecidesWhetherTheSetupBattlefieldIsAsked) {
    auto firstDecision = [](const std::string& mode) {
        auto game = ::open_spiel::LoadGame("riftbound(deck1=" + deckFile("yi_rebuild.txt") +
                                           ",deck2=" + deckFile("fiora.txt") + ",seed=11" +
                                           (mode.empty() ? "" : ",game_mode=" + mode) + ")");
        RiftboundState state(game, 11);
        return state.decisionPrompt().kind;
    };
    EXPECT_NE(firstDecision(""), ::riftbound::PromptKind::BattlefieldSetup);
    EXPECT_NE(firstDecision("duel"), ::riftbound::PromptKind::BattlefieldSetup);
    EXPECT_EQ(firstDecision("match"), ::riftbound::PromptKind::BattlefieldSetup);
    EXPECT_ANY_THROW(firstDecision("best-of-five"));
}

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
            for (int id : ::riftbound::openspiel::actionIds(intents, gs)) {
                ASSERT_GE(id, 0);
                ASSERT_LT(id, game->NumDistinctActions());
                slots.insert(id);
            }
            ASSERT_EQ(std::vector<::open_spiel::Action>(slots.begin(), slots.end()),
                      legal);
            std::vector<Intent> distinct;
            for (const auto& intent : intents)
                if (std::find(distinct.begin(), distinct.end(), intent) == distinct.end())
                    distinct.push_back(intent);
            ASSERT_EQ(legal.size(), distinct.size())
                << "seed " << seed << " move " << state->MoveNumber()
                << ": a legal Intent has no action of its own";
            state->ApplyAction(pickAction(rng, legal));
        }
    }
    EXPECT_GT(decisions, 200);
    EXPECT_GT(off_turn, 4);
    EXPECT_LT(stale_closed * 20, decisions);   // an exception, not the rule
    RecordProperty("stale_closed_main_phase_decisions", stale_closed);
}

// plans/09 item 9.R3 (07 item 7.33): the answers of one int-coded MakeChoice
// decision must land in distinct action slots, or a policy cannot tell them
// apart. Checked on a real prompt: Party Favors 71 asks the opponent "Cards or
// Runes" (chosen_value 0 / 1).
TEST(ActionVocabTest, TheAnswersOfOneMakeChoiceDecisionGetDistinctActionIds) {
    using namespace ::riftbound;
    constexpr CardDefId kPartyFavors = 71;
    CardRegistry registry;
    registry.loadAll();
    CardDB db;
    db.buildFromClasses(registry);
    EventBus bus;
    GameEngine engine(db, bus, registry);
    auto& st = engine.mutableState();
    st.mode = ModeOfPlay{};
    st.players[0].id = PlayerId::Player1;
    st.players[1].id = PlayerId::Player2;
    st.turn.phase = TurnPhase::MainPhase;
    st.turn.turn_player = PlayerId::Player1;
    const auto spell = st.createObject();
    {
        auto& o = st.getObject(spell);
        const auto& def = db.get(kPartyFavors);
        o.card_def_id = kPartyFavors;
        o.owner = o.controller = PlayerId::Player1;
        o.name = def.name;
        o.card_type = def.card_type;
        o.keywords = def.keywords;
        o.domains = def.domains;
        o.zone = ZoneType::Hand;
        st.player(PlayerId::Player1).hand.push_back(spell);
        st.player(PlayerId::Player1).rune_pool.energy = def.energy_cost;
        st.player(PlayerId::Player1).rune_pool.universal_power = def.power_cost;
    }
    struct Recorder : AgentInterface {
        std::vector<std::vector<Intent>> value_prompts;
        std::vector<std::vector<int>> slots;
        Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
            const bool values = !legal.empty() &&
                std::all_of(legal.begin(), legal.end(), [](const Intent& i) {
                    return i.type == IntentType::MakeChoice && i.chosen_value.has_value();
                });
            if (values) {
                value_prompts.push_back(legal);
                slots.emplace_back();
                for (const auto& i : legal)
                    slots.back().push_back(::riftbound::openspiel::actionSlot(i, state));
            }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        }
    } p1, p2;
    engine.testHook_setAgents(&p1, &p2);
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = PlayerId::Player1;
    play.card = spell;
    engine.testHook_executeIntent(play);

    ASSERT_EQ(p2.value_prompts.size(), 1u) << "the opponent is asked Cards or Runes";
    const auto& prompt = p2.value_prompts[0];
    ASSERT_EQ(prompt.size(), 2u);
    EXPECT_NE(*prompt[0].chosen_value, *prompt[1].chosen_value);
    EXPECT_NE(p2.slots[0][0], p2.slots[0][1])
        << "two answers, two action ids";
    // And in general: every int answer the vocabulary encodes has a slot of its own.
    std::set<int> ids;
    for (int v = 0; v < ::riftbound::openspiel::kNumIntChoices; ++v) {
        Intent i = prompt[0];
        i.chosen_value = v;
        ids.insert(::riftbound::openspiel::actionSlot(i, engine.state()));
    }
    EXPECT_EQ(static_cast<int>(ids.size()), ::riftbound::openspiel::kNumIntChoices);
}

// plans/01 item 1.12 — every legal Intent can be chosen through OpenSpiel,
// and choosing an action applies exactly the Intent that action stands for.
// At every decision of whole games, every legal action is applied to a clone;
// the clone's engine must record the token of that action's Intent (a token
// is unique within a legal list: it fingerprints every field). Before
// the 2026-10 vocabulary only the first Intent of a slot could be applied.
TEST(RiftboundStateTest, EveryLegalIntentIsSelectableAndAppliesItself) {
    namespace os = ::riftbound::openspiel;
    long decisions = 0, applied = 0, twins_applied = 0, most_options = 0;
    for (int seed = 1; seed <= 3; ++seed) {
        for (bool mirror : {false, true}) {
            auto game = mirror
                ? ::open_spiel::LoadGame("riftbound(deck1=" + deckFile("fiora.txt") +
                                         ",deck2=" + deckFile("fiora.txt") +
                                         ",seed=" + std::to_string(seed) + ")")
                : loadGame(seed);
            auto state = game->NewInitialState();
            std::mt19937_64 rng(seed * 53 + (mirror ? 7 : 0));
            while (!state->IsTerminal()) {
                const std::vector<Intent> intents = rb(*state).legalIntents();
                const GameState& gs = rb(*state).engineState();
                const std::vector<int> ids = os::actionIds(intents, gs);
                const auto legal = state->LegalActions();
                most_options = std::max<long>(most_options, static_cast<long>(legal.size()));
                // Every Intent's id is on offer, and leads back to it.
                for (size_t i = 0; i < intents.size(); ++i) {
                    ASSERT_TRUE(std::binary_search(legal.begin(), legal.end(),
                                                   static_cast<::open_spiel::Action>(ids[i])))
                        << intents[i].describe();
                    const Intent* back = os::decodeAction(ids[i], intents, gs);
                    ASSERT_NE(back, nullptr);
                    ASSERT_TRUE(*back == intents[i]);
                }
                // Apply every one of them (a sample of the decisions with
                // many options, to bound the run).
                if (legal.size() <= 12 || decisions % 5 == 0) {
                    for (size_t i = 0; i < intents.size(); ++i) {
                        auto clone = state->Clone();
                        clone->ApplyAction(ids[i]);
                        const auto& history = rb(*clone).engineState().action_history;
                        ASSERT_EQ(history.size(), gs.action_history.size() + 1);
                        ASSERT_EQ(history.back(), os::historyToken(intents[i], gs))
                            << "seed " << seed << " move " << state->MoveNumber()
                            << ": action " << ids[i] << " applied something other than "
                            << intents[i].describe();
                        ASSERT_EQ(rb(*clone).actionHistory().back(), os::actionIdSlot(ids[i]));
                        ASSERT_EQ(clone->History().back(), ids[i]);
                        ++applied;
                        if (os::actionIdTwin(ids[i]) > 0) ++twins_applied;
                    }
                }
                ++decisions;
                state->ApplyAction(pickAction(rng, legal));
            }
        }
    }
    std::cout << "[ info     ] " << decisions << " decisions, " << applied
              << " actions applied to a clone (" << twins_applied
              << " of them twins: not the first Intent of their slot); most options at one "
              << "decision: " << most_options << "\n";
    EXPECT_GT(decisions, 1000);
    EXPECT_GT(applied, 3000);
    EXPECT_GT(twins_applied, 300);
}

TEST(RiftboundStateTest, ApplyingAnActionThatIsNotLegalThrows) {
    auto game = loadGame(3);
    auto state = game->NewInitialState();
    const auto legal = state->LegalActions();
    ::open_spiel::Action bogus = 0;   // slot 0 is reserved, never legal
    ASSERT_TRUE(std::find(legal.begin(), legal.end(), bogus) == legal.end());
    EXPECT_THROW(state->ApplyAction(bogus), std::invalid_argument);
    // Nor is an id past the range, or a twin of a legal action that no
    // second Intent stands behind.
    EXPECT_THROW(state->ApplyAction(game->NumDistinctActions()), std::invalid_argument);
    EXPECT_THROW(state->ApplyAction(::riftbound::openspiel::makeActionId(
                     ::riftbound::openspiel::actionIdSlot(static_cast<int>(legal.front())),
                     ::riftbound::openspiel::kTwinPlanes - 1)),
                 std::invalid_argument);
    // Nothing was applied.
    EXPECT_EQ(state->LegalActions(), legal);
    EXPECT_EQ(state->MoveNumber(), 0);
}

// 1.5 — one history entry per applied action: the slot of the Intent that was
// applied. (Until the 2026-10 vocabulary an action id was a slot; it now also
// holds a twin number, which OpenSpiel's own History() keeps.)
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
    std::vector<int64_t> applied_slots;
    for (int64_t a : applied)
        applied_slots.push_back(::riftbound::openspiel::actionIdSlot(static_cast<int>(a)));
    EXPECT_EQ(rb(*state).actionHistory(), applied_slots);
    EXPECT_EQ(state->History(), std::vector<::open_spiel::Action>(applied.begin(), applied.end()));
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
// Testing phase T1: the decision API says what each decision asks. The
// decider's description lists every legal action once, by its OpenSpiel id,
// in id order; the other player is told who decides and the kind only.
TEST(RiftboundStateTest, EveryDecisionSaysWhatItAsksWithTheActionIdsOnOffer) {
    for (int seed = 1; seed <= 3; ++seed) {
        auto game = loadGame(seed);
        auto state = game->NewInitialState();
        std::mt19937_64 rng(seed * 101);
        int n = 0;
        while (!state->IsTerminal() && n < 2000) {
            const auto& r = rb(*state);
            const auto decider = state->CurrentPlayer();
            EXPECT_NE(r.decisionPrompt().kind, ::riftbound::PromptKind::Unspecified);
            const auto mine = r.describeDecision(decider);
            std::vector<::open_spiel::Action> ids;
            for (const auto& o : mine.options) ids.push_back(o.action_id);
            ASSERT_EQ(ids, state->LegalActions()) << "seed " << seed << " decision " << n;
            const auto theirs = r.describeDecision(1 - decider);
            EXPECT_EQ(theirs.decider, 2);
            EXPECT_TRUE(theirs.options.empty());
            state->ApplyAction(pickAction(rng, state->LegalActions()));
            ++n;
        }
    }
}

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

// plans/01 item 1.9 — what the key does with the perspective. InfoSetId(p) is
// the hash of p's own masked feature vector, mixed with p's seat (so the two
// players' keys never coincide), with who is deciding and, for the decider
// only, with the action ids on offer. ISMCTS keys a node on
// (decider, ObservationString(decider)), so the key must be a function of what
// that player knows: re-deal everything the player cannot know — the
// identities of the opponent's hand, deck and facedown cards that the player
// has not been shown (by the knowledge tracker's record), and the order of
// both decks — and neither player's key
// of the re-dealt position may differ from its key of the real one.
TEST(RiftboundStateTest, InfoSetKeyIsAFunctionOfWhatThePlayerKnows) {
    using ::riftbound::CardDefId;
    using ::riftbound::GameObjectId;
    long checked = 0, redealt_cards = 0, shown_cards_kept = 0, decider_keys = 0;
    for (int seed = 1; seed <= 6; ++seed) {
        auto game = loadGame(seed);
        auto state = game->NewInitialState();
        std::mt19937_64 rng(seed * 101);
        while (!state->IsTerminal()) {
            const GameState& gs = rb(*state).engineState();
            ::riftbound::StepResult step;
            step.kind = ::riftbound::StepKind::NeedDecision;
            step.legal = rb(*state).legalIntents();
            step.perspective = state->CurrentPlayer() == 0 ? PlayerId::Player1
                                                           : PlayerId::Player2;
            for (::open_spiel::Player p : {0, 1}) {
                const PlayerId me = p == 0 ? PlayerId::Player1 : PlayerId::Player2;
                const PlayerId opp = ::riftbound::opponent(me);
                GameState redealt = gs;
                const ::riftbound::knowledge::GameKnowledgeView knows(gs, me);
                auto& ops = redealt.player(opp);
                std::vector<GameObjectId> hidden;
                // What the player has been shown is the knowledge tracker's
                // record alone (plans/09 item 9.SS1): a hand revealed to be
                // chosen from (Sabotage 156) and a unit returned to its
                // owner's hand with its ability still on the chain are both
                // in it.
                auto consider = [&](GameObjectId id) {
                    if (knows.about(id).identity_known)
                        ++shown_cards_kept;
                    else
                        hidden.push_back(id);
                };
                for (auto id : ops.hand) consider(id);
                for (auto id : ops.main_deck) consider(id);
                for (const auto& bf : redealt.battlefields)
                    for (auto id : bf.facedown)
                        if (redealt.getObject(id).controller == opp) consider(id);
                std::vector<CardDefId> defs;
                for (auto id : hidden) defs.push_back(redealt.getObject(id).card_def_id);
                std::shuffle(defs.begin(), defs.end(), rng);
                for (size_t i = 0; i < hidden.size(); ++i)
                    redealt.getObject(hidden[i]).card_def_id = defs[i];
                redealt_cards += static_cast<long>(hidden.size());
                std::shuffle(ops.main_deck.begin(), ops.main_deck.end(), rng);
                std::shuffle(redealt.player(me).main_deck.begin(),
                             redealt.player(me).main_deck.end(), rng);

                auto other = RiftboundState::makeFromSnapshot(
                    game, static_cast<uint64_t>(seed), redealt, step,
                    ::riftbound::GameResult{}, /*rng_seed=*/1);
                if (other->InfoSetId(p) != rb(*state).InfoSetId(p)) {
                    // Say which half of the key moved.
                    const auto f0 = state->ObservationTensor(p);
                    const auto f1 =
                        static_cast<const ::open_spiel::State&>(*other).ObservationTensor(p);
                    std::string why;
                    for (size_t i = 0; i < f0.size() && why.empty(); ++i)
                        if (f0[i] != f1[i])
                            why = "feature " + std::to_string(i) + ": " +
                                  std::to_string(f0[i]) + " / " + std::to_string(f1[i]);
                    if (why.empty()) why = "the features are equal";
                    std::string offered;
                    for (const auto& i : step.legal) {
                        offered += "\n  " + i.describe();
                        for (auto o : i.chosen_objects) {
                            const auto& obj = gs.getObject(o);
                            offered += " [" + obj.name.get() + ", zone " +
                                       std::to_string(static_cast<int>(obj.zone.get())) +
                                       ", owner " + toString(obj.owner) + "]";
                        }
                    }
                    if (gs.chain.resuming && gs.objectExists(gs.chain.resuming->source))
                        offered += "\n  resolving: " +
                                   gs.getObject(gs.chain.resuming->source).name.get();
                    for (const auto& item : gs.chain.items) {
                        if (!gs.objectExists(item.source)) continue;
                        const auto& src = gs.getObject(item.source);
                        offered += "\n  on the chain: " + src.name.get() + ", zone " +
                                   std::to_string(static_cast<int>(src.zone.get())) +
                                   ", owner " + toString(src.owner) +
                                   (item.is_ability ? " (ability)" : " (card)");
                    }
                    ADD_FAILURE() << why << "; offered:" << offered;
                }
                ASSERT_EQ(other->InfoSetId(p), rb(*state).InfoSetId(p))
                    << "seed " << seed << " move " << state->MoveNumber() << ": "
                    << toString(me) << "'s key changed with a re-deal of what it "
                    << "cannot know (it " << (p == state->CurrentPlayer() ? "is" : "is not")
                    << " the decider)";
                ASSERT_EQ(other->ObservationString(p), state->ObservationString(p));
                ++checked;
                if (p == state->CurrentPlayer()) {
                    ++decider_keys;
                    ASSERT_EQ(other->LegalActions(), state->LegalActions())
                        << "the action ids on offer depend on hidden cards";
                }
            }
            // Per seat: the two players' keys of one position differ.
            ASSERT_NE(rb(*state).InfoSetId(0), rb(*state).InfoSetId(1));
            state->ApplyAction(pickAction(rng, state->LegalActions()));
        }
    }
    std::cout << "[ info     ] " << checked << " keys compared (" << decider_keys
              << " of the decider), " << redealt_cards << " hidden cards re-dealt, "
              << shown_cards_kept << " left as they are because the player has seen them\n";
    EXPECT_GT(checked, 1500);
    EXPECT_GT(redealt_cards, 20000);
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

// ─── plans/09 item 9.E2: a search agent's game is a function of its seeds ───
//
// Same decks, same engine seed, same search seed, same opponent: the same
// game, decision for decision. ISMCTS did not have this property while it ran
// OpenSpiel's ISMCTSBot, whose hash-map iteration order (and with it the
// tie-breaks of its tree policy and the order of the policy it samples from)
// depends on addresses, not on the seed.

template <class Agent>
std::vector<int64_t> historyOfAgentGame(uint64_t seed, uint64_t search_seed, int sims) {
    auto game = loadGame(1);
    const auto& rb_game = static_cast<const RiftboundGame&>(*game);
    Agent agent(deckFile("yi_rebuild.txt"), deckFile("fiora.txt"), "ignored",
                seed, search_seed, sims);
    ::riftbound::RandomAgent random(seed + 1);
    ::riftbound::EventBus bus;
    GameEngine engine(rb_game.cardDb(), bus, rb_game.cardRegistry());
    engine.runGame(rb_game.deck1(), rb_game.deck2(), agent, random, seed);
    return engine.state().action_history;
}

TEST(IsMctsAgentTest, TheSameSeedsPlayTheSameGame) {
    for (uint64_t seed : {51001u, 51002u}) {
        // Allocate between the two games so the second search does not get
        // the first one's memory back: equal results must not depend on it.
        const auto first = historyOfAgentGame<::riftbound::IsMctsAgent>(seed, 7, 12);
        std::vector<std::unique_ptr<std::vector<int>>> ballast;
        for (int i = 0; i < 257; ++i) ballast.push_back(std::make_unique<std::vector<int>>(i + 1));
        const auto second = historyOfAgentGame<::riftbound::IsMctsAgent>(seed, 7, 12);
        ASSERT_GT(first.size(), 30u);
        EXPECT_EQ(first, second) << "engine seed " << seed;
    }
    // ...and the search seed matters: it is not ignored.
    const auto a = historyOfAgentGame<::riftbound::IsMctsAgent>(51001, 7, 12);
    const auto b = historyOfAgentGame<::riftbound::IsMctsAgent>(51001, 8, 12);
    const auto c = historyOfAgentGame<::riftbound::IsMctsAgent>(51001, 9, 12);
    EXPECT_TRUE(a != b || a != c) << "three search seeds played one game";
}

TEST(MctsAgentTest, TheSameSeedsPlayTheSameGame) {
    const auto first = historyOfAgentGame<::riftbound::MctsAgent>(52001, 7, 12);
    const auto second = historyOfAgentGame<::riftbound::MctsAgent>(52001, 7, 12);
    ASSERT_GT(first.size(), 30u);
    EXPECT_EQ(first, second);
}

// ─── plans/09 item 9.E1: a resumable decision resumes to itself ─────────────
//
// perf-search saw `GameEngine::resumeFromSnapshot` throw "the resumed engine
// did not reach the snapshotted decision" in two ISMCTS batches: a decision
// `isResumableDecision` accepted was asked again by a fresh engine with a
// different legal list. Every clone a search takes at a main-phase decision
// goes through that resume, so the property is checked directly, at EVERY
// resumable decision of whole games, with a seeded random policy (no search,
// no hash maps: a failure names its seed and decision and is repeatable).
//
// The default is a bounded run; set RIFTBOUND_RESUME_STRESS_GAMES to play
// that many games per seat order (the stress run for 9.E1 used 300).

int envInt(const char* name, int fallback) {
    const char* v = std::getenv(name);
    return v && *v ? std::atoi(v) : fallback;
}

std::string describeDifference(const std::vector<Intent>& want,
                               const std::vector<Intent>& got) {
    std::string out;
    for (const auto& i : want)
        if (std::find(got.begin(), got.end(), i) == got.end())
            out += "\n  only in the snapshot: " + i.describe();
    for (const auto& i : got)
        if (std::find(want.begin(), want.end(), i) == want.end())
            out += "\n  only after the resume: " + i.describe();
    if (out.empty()) out = "\n  same intents in a different order";
    return out;
}

/// Plays one game by the step API and resumes a fresh engine at every
/// resumable decision. Returns the number of resumes checked.
int checkEveryResumableDecision(const RiftboundGame& rb_game, bool swap_decks,
                                uint64_t seed, int* mismatches) {
    ::riftbound::EventBus bus;
    GameEngine engine(rb_game.cardDb(), bus, rb_game.cardRegistry());
    const auto& d1 = swap_decks ? rb_game.deck2() : rb_game.deck1();
    const auto& d2 = swap_decks ? rb_game.deck1() : rb_game.deck2();
    engine.beginGame(d1, d2, seed);
    std::mt19937_64 rng(seed * 2654435761u + (swap_decks ? 17 : 0));
    int checked = 0;
    for (int n = 0; n < 6000 && !engine.isStepDone(); ++n) {
        const ::riftbound::StepResult step = engine.currentStep();   // a copy
        if (GameEngine::isResumableDecision(engine.state(), step.legal)) {
            ++checked;
            ::riftbound::EventBus bus2;
            GameEngine resumed(rb_game.cardDb(), bus2, rb_game.cardRegistry());
            try {
                resumed.resumeFromSnapshot(GameState(engine.state()), engine.rngState(), step);
            } catch (const std::logic_error& e) {
                ++*mismatches;
                std::string got = "(no decision)";
                try {
                    if (!resumed.isStepDone())
                        got = describeDifference(step.legal, resumed.currentStep().legal);
                } catch (const std::exception&) {}
                ADD_FAILURE() << "seed " << seed << (swap_decks ? " (decks swapped)" : "")
                              << ", decision " << n << ", turn "
                              << engine.state().turn.turn_number << ": " << e.what() << got;
                if (*mismatches >= 5) return checked;
            }
        }
        std::uniform_int_distribution<size_t> d(0, step.legal.size() - 1);
        engine.applyChoice(static_cast<int>(d(rng)));
    }
    return checked;
}

TEST(ResumeStress, EveryResumableDecisionOfAGameResumesToItself) {
    const int games = envInt("RIFTBOUND_RESUME_STRESS_GAMES", 6);
    auto game = loadGame(1);
    const auto& rb_game = static_cast<const RiftboundGame&>(*game);
    int checked = 0, mismatches = 0;
    for (int g = 1; g <= games && mismatches < 5; ++g) {
        checked += checkEveryResumableDecision(rb_game, false, 9000 + g, &mismatches);
        checked += checkEveryResumableDecision(rb_game, true, 9000 + g, &mismatches);
    }
    EXPECT_EQ(mismatches, 0);
    EXPECT_GT(checked, games * 40) << "main-phase decisions were resumed";
    RecordProperty("resumes_checked", checked);
}

// The same property the way perf-search met it: whole ISMCTS games, both
// seats, across seeds. An engine or agent exception fails the test (runGame
// lets it through). RIFTBOUND_ISMCTS_STRESS_GAMES scales the run (the 9.E1
// stress used 150 per seat at 20 simulations).
TEST(ResumeStress, IsMctsGamesAcrossSeedsNeverFailToResume) {
    const int games = envInt("RIFTBOUND_ISMCTS_STRESS_GAMES", 2);
    const int sims = envInt("RIFTBOUND_ISMCTS_STRESS_SIMS", 20);
    auto game = loadGame(1);
    const auto& rb_game = static_cast<const RiftboundGame&>(*game);
    int finished = 0;
    for (int g = 1; g <= games; ++g) {
        for (bool agent_is_p1 : {true, false}) {
            const uint64_t seed = 20261010u + static_cast<uint64_t>(g);
            ::riftbound::IsMctsAgent agent(deckFile("yi_rebuild.txt"), deckFile("fiora.txt"),
                                           "ignored", seed, /*mcts_seed=*/seed * 3 + 1, sims);
            CheckedAgent checked(agent);
            ::riftbound::RandomAgent random(seed ^ 0x5eedu);
            ::riftbound::EventBus bus;
            GameEngine engine(rb_game.cardDb(), bus, rb_game.cardRegistry());
            try {
                if (agent_is_p1)
                    engine.runGame(rb_game.deck1(), rb_game.deck2(), checked, random, seed);
                else
                    engine.runGame(rb_game.deck1(), rb_game.deck2(), random, checked, seed);
            } catch (const std::exception& e) {
                FAIL() << "seed " << seed << (agent_is_p1 ? " (ISMCTS is P1)" : " (ISMCTS is P2)")
                       << " after " << checked.decisions << " agent decisions: " << e.what();
            }
            EXPECT_TRUE(engine.state().game_over) << "seed " << seed;
            EXPECT_EQ(checked.illegal, 0) << "seed " << seed;
            ++finished;
        }
    }
    EXPECT_EQ(finished, games * 2);
}


// ─── Batch 5 item T3: honest ISMCTS ─────────────────────────────────────────
//
// The search of IsMctsAgent (WorldModel::Sampled) runs in worlds drawn from
// what its player knows. The core property: two live games that differ only
// in what the searching player cannot see — the opponent's deck contents, the
// opponent's hand and deck order — give the same search, decision for
// decision (same option played, same root visits, same root value), for the
// same seeds. The perfect-information variant is the control: it does see
// the difference.

/// Plays a pass wherever one is offered, else the first option: an opponent
/// whose public actions do not depend on its cards.
class PasserAgent : public ::riftbound::AgentInterface {
public:
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        for (size_t k = 0; k < legal.size(); ++k)
            if (::riftbound::isPassOption(legal, k)) return legal[k];
        return legal.front();
    }
};

/// Records, at each of its decisions, the history so far and the search.
class RecordingSearcher : public ::riftbound::AgentInterface {
public:
    explicit RecordingSearcher(::riftbound::IsMctsAgent& inner) : inner_(inner) {}
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        Intent chosen = inner_.selectAction(state, legal);
        histories.push_back(state.action_history);
        searches.push_back(inner_.lastSearch());
        return chosen;
    }
    std::vector<std::vector<int64_t>> histories;
    std::vector<::riftbound::SearchSummary> searches;
private:
    ::riftbound::IsMctsAgent& inner_;
};

/// A deck list file under the test's temporary directory.
std::string writeDeck(const std::string& name, const ::riftbound::DeckSubmission& deck,
                      const ::riftbound::CardDB& db) {
    const std::string path = ::testing::TempDir() + "/" + name;
    std::ofstream f(path);
    f << ::riftbound::RandomDeckBuilder(db).toDeckList(deck);
    return path;
}

struct HonestRun {
    std::vector<std::vector<int64_t>> histories;
    std::vector<::riftbound::SearchSummary> searches;
};

HonestRun playSearcherGame(const std::string& searcher_deck, const std::string& passer_deck,
                           bool searcher_is_p1, uint64_t seed, int sims,
                           ::riftbound::IsMctsAgent::WorldModel model) {
    const std::string d1 = searcher_is_p1 ? searcher_deck : passer_deck;
    const std::string d2 = searcher_is_p1 ? passer_deck : searcher_deck;
    auto game = ::open_spiel::LoadGame("riftbound(deck1=" + d1 + ",deck2=" + d2 +
                                       ",seed=" + std::to_string(seed) + ")");
    const auto& rb_game = static_cast<const RiftboundGame&>(*game);
    ::riftbound::IsMctsAgent agent(d1, d2, "ignored", seed, /*mcts_seed=*/seed * 31 + 7, sims,
                                   model);
    RecordingSearcher searcher(agent);
    PasserAgent passer;
    ::riftbound::EventBus bus;
    GameEngine engine(rb_game.cardDb(), bus, rb_game.cardRegistry());
    if (searcher_is_p1)
        engine.runGame(rb_game.deck1(), rb_game.deck2(), searcher, passer, seed);
    else
        engine.runGame(rb_game.deck1(), rb_game.deck2(), passer, searcher, seed);
    return {searcher.histories, searcher.searches};
}

/// Compares two runs decision by decision while their histories agree.
/// Returns {decisions compared, decisions whose searches differ}.
std::pair<int, int> compareRuns(const HonestRun& a, const HonestRun& b) {
    int compared = 0, differ = 0;
    for (size_t i = 0; i < a.searches.size() && i < b.searches.size(); ++i) {
        if (a.histories[i] != b.histories[i]) break;
        ++compared;
        const auto& x = a.searches[i];
        const auto& y = b.searches[i];
        if (x.searched != y.searched || x.chosen != y.chosen || x.visits != y.visits ||
            x.value != y.value || x.root_value != y.root_value)
            ++differ;
    }
    return {compared, differ};
}

class IsMctsHonestTest : public ::testing::Test {
protected:
    void SetUp() override {
        game_ = loadGame(1);
        const auto& rb_game = static_cast<const RiftboundGame&>(*game_);
        const auto& db = rb_game.cardDb();
        const ::riftbound::DeckSubmission fiora = rb_game.deck2();
        // The same cards in another order: another deck order and opening hand.
        ::riftbound::DeckSubmission reordered = fiora;
        std::reverse(reordered.main_deck.begin(), reordered.main_deck.end());
        // Other cards: every card but the Chosen Champion's copies replaced
        // by other cards of the legend's pool (same count, legal).
        ::riftbound::DeckSubmission other = fiora;
        ::riftbound::RandomDeckBuilder builder(db);
        const auto* pool = builder.legendCards(fiora.legend);
        ASSERT_NE(pool, nullptr);
        std::vector<::riftbound::CardDefId> fresh;
        for (auto id : pool->main) {
            if (std::find(fiora.main_deck.begin(), fiora.main_deck.end(), id) ==
                    fiora.main_deck.end() &&
                !db.get(id).keywords.has(::riftbound::Keyword::Unique) &&
                db.get(id).super_type != ::riftbound::SuperType::Champion)
                fresh.push_back(id);
        }
        ASSERT_GE(fresh.size(), 10u);
        const std::string champ = db.get(fiora.chosen_champion).name;
        size_t next = 0;
        for (auto& id : other.main_deck) {
            if (db.get(id).name == champ) continue;
            id = fresh[(next++ / 3) % fresh.size()];
        }
        fiora_ = deckFile("fiora.txt");
        yi_ = deckFile("yi_rebuild.txt");
        reordered_ = writeDeck("fiora_reordered.txt", reordered, db);
        other_ = writeDeck("fiora_other_cards.txt", other, db);
    }
    std::shared_ptr<const ::open_spiel::Game> game_;
    std::string fiora_, yi_, reordered_, other_;
};

TEST_F(IsMctsHonestTest, GamesThatDifferOnlyInCardsTheSearcherCannotSeeGetTheSameSearch) {
    constexpr int kSims = 24;
    int total_compared = 0;
    for (bool p1 : {true, false}) {
        const uint64_t seed = p1 ? 61001 : 61002;
        const HonestRun base = playSearcherGame(yi_, fiora_, p1, seed, kSims,
                                                ::riftbound::IsMctsAgent::WorldModel::Sampled);
        for (const std::string& variant : {reordered_, other_}) {
            const HonestRun run = playSearcherGame(yi_, variant, p1, seed, kSims,
                                                   ::riftbound::IsMctsAgent::WorldModel::Sampled);
            const auto [compared, differ] = compareRuns(base, run);
            EXPECT_EQ(differ, 0) << variant << (p1 ? " (searcher P1)" : " (searcher P2)")
                                 << ": " << differ << " of " << compared
                                 << " searches differ between games the searcher cannot tell apart";
            EXPECT_GE(compared, 20) << variant << ": the games diverged too early to compare";
            total_compared += compared;
        }
    }
    std::cout << "[ honest search: " << total_compared
              << " searches compared between indistinguishable games ]\n";
    RecordProperty("searches_compared", total_compared);
}

// The control: the clairvoyant search reads the opponent's cards, and the
// comparison above can see that.
TEST_F(IsMctsHonestTest, ThePerfectInformationSearchDoesSeeTheDifference) {
    const HonestRun base = playSearcherGame(yi_, fiora_, true, 61001, 24,
                                            ::riftbound::IsMctsAgent::WorldModel::Perfect);
    const HonestRun run = playSearcherGame(yi_, other_, true, 61001, 24,
                                           ::riftbound::IsMctsAgent::WorldModel::Perfect);
    // A different search soon plays differently, after which the games are
    // no longer comparable: one compared decision that differs is the point.
    const auto [compared, differ] = compareRuns(base, run);
    EXPECT_GE(compared, 1);
    EXPECT_GT(differ, 0) << "world=perfect gave the same searches for different hidden cards";
}

// Every world a whole game's searches draw agrees with what the searcher
// knows (WorldSampler::check, run on each one), both seats; and the search
// reports its root.
TEST_F(IsMctsHonestTest, EveryWorldOfAGameAgreesWithWhatTheSearcherKnows) {
    const auto& rb_game = static_cast<const RiftboundGame&>(*game_);
    long worlds = 0;
    int searched = 0;
    for (bool p1 : {true, false}) {
        const uint64_t seed = p1 ? 62001 : 62002;
        constexpr int kSims = 10;
        ::riftbound::IsMctsAgent agent(yi_, fiora_, "ignored", seed, seed + 5, kSims);
        agent.setCheckWorlds(true);
        RecordingSearcher searcher(agent);
        ::riftbound::RandomPlayerAgent other(seed ^ 0x77u);
        ::riftbound::EventBus bus;
        GameEngine engine(rb_game.cardDb(), bus, rb_game.cardRegistry());
        try {
            if (p1) engine.runGame(rb_game.deck1(), rb_game.deck2(), searcher, other, seed);
            else    engine.runGame(rb_game.deck1(), rb_game.deck2(), other, searcher, seed);
        } catch (const std::exception& e) {
            FAIL() << "seed " << seed << ": " << e.what();
        }
        EXPECT_TRUE(engine.state().game_over);
        worlds += agent.worldsChecked();
        for (const auto& s : searcher.searches) {
            if (!s.searched) continue;
            ++searched;
            int total = 0, best = -1;
            for (int v : s.visits) { total += v; best = std::max(best, v); }
            EXPECT_EQ(total, kSims);
            EXPECT_EQ(s.simulations, kSims);
            ASSERT_GE(s.chosen, 0);
            EXPECT_EQ(s.visits[s.chosen], best) << "the move played is not the most visited";
        }
    }
    EXPECT_GT(searched, 100);
    EXPECT_EQ(worlds, static_cast<long>(searched) * 10);
    RecordProperty("worlds_checked", static_cast<int>(worlds));
}

TEST(IsMctsAgentTest, ThePerfectWorldVariantIsAlsoAFunctionOfItsSeeds) {
    auto game = loadGame(1);
    const auto& rb_game = static_cast<const RiftboundGame&>(*game);
    auto play = [&] {
        ::riftbound::IsMctsAgent agent(deckFile("yi_rebuild.txt"), deckFile("fiora.txt"),
                                       "ignored", 53001, 7, 12,
                                       ::riftbound::IsMctsAgent::WorldModel::Perfect);
        ::riftbound::RandomAgent random(53002);
        ::riftbound::EventBus bus;
        GameEngine engine(rb_game.cardDb(), bus, rb_game.cardRegistry());
        engine.runGame(rb_game.deck1(), rb_game.deck2(), agent, random, 53001);
        return engine.state().action_history;
    };
    const auto a = play();
    const auto b = play();
    ASSERT_GT(a.size(), 30u);
    EXPECT_EQ(a, b);
}

// A state whose hidden information was rewritten is not the product of its
// history: it refuses to be rebuilt by a replay rather than becoming the
// unedited game.
TEST(RiftboundStateTest, AnEditedStateIsNeverRebuiltByAReplay) {
    auto game = loadGame(5);
    RiftboundState state(game, 5);
    // The first decision (P1's mulligan in a Duel; the setup battlefield
    // choice in a Match) cannot be resumed from a snapshot: a clone of it
    // would be rebuilt by replaying.
    ASSERT_FALSE(state.IsTerminal());
    ASSERT_FALSE(GameEngine::isResumableDecision(state.engineState(), state.legalIntents()));
    state.editHiddenInformation([](GameState&) {}, 99);
    auto clone = state.Clone();
    EXPECT_THROW(clone->ApplyAction(clone->LegalActions().front()), std::logic_error);
    ::riftbound::StepResult step;
    step.kind = ::riftbound::StepKind::NeedDecision;
    step.legal = state.legalIntents();
    EXPECT_FALSE(state.syncToHistory(state.engineState(), step));

    // At a main-phase decision a clone resumes from the edited snapshot itself.
    RiftboundState live(game, 5);
    std::mt19937_64 rng(3);
    while (!live.IsTerminal() &&
           !GameEngine::isResumableDecision(live.engineState(), live.legalIntents())) {
        live.ApplyAction(pickAction(rng, live.LegalActions()));
    }
    ASSERT_FALSE(live.IsTerminal());
    live.editHiddenInformation([](GameState&) {}, 99);
    auto resumed = live.Clone();
    for (int k = 0; k < 20 && !resumed->IsTerminal(); ++k)
        resumed->ApplyAction(pickAction(rng, resumed->LegalActions()));
    SUCCEED();
}

// Rewriting what the decider cannot see must leave its options alone; the
// engine checks that where it can rebuild them.
TEST(RiftboundStateTest, AnEditThatChangesTheDecidersOptionsIsRefused) {
    auto game = loadGame(6);
    RiftboundState live(game, 6);
    std::mt19937_64 rng(4);
    // A main-phase decision at which the decider may play a card from its
    // hand (the game the walk reaches follows the Mode of Play: a Duel since
    // batch 8, so the walk looks for an offered hand play, not just a hand).
    auto handPlayOffered = [&]() {
        const auto& st = live.engineState();
        for (const auto& i : live.legalIntents())
            if (i.type == ::riftbound::IntentType::PlayCard && st.objectExists(i.card) &&
                st.getObject(i.card).zone == ::riftbound::ZoneType::Hand)
                return true;
        return false;
    };
    while (!live.IsTerminal() &&
           !(GameEngine::isResumableDecision(live.engineState(), live.legalIntents()) &&
             handPlayOffered())) {
        live.ApplyAction(pickAction(rng, live.LegalActions()));
    }
    ASSERT_FALSE(live.IsTerminal());
    const PlayerId decider = live.legalIntents().front().player;
    // Emptying the decider's own hand changes what it may play.
    EXPECT_THROW(live.editHiddenInformation(
                     [&](GameState& g) {
                         auto& hand = g.player(decider).hand;
                         for (auto id : hand) g.getObject(id).zone = ::riftbound::ZoneType::Trash;
                         g.player(decider).trash.insert(g.player(decider).trash.end(),
                                                        hand.begin(), hand.end());
                         hand.clear();
                     },
                     1),
                 std::logic_error);
}

} // namespace
