#pragma once
/// @file card_test_fixture.h
/// Header-only test fixture for per-card unit tests under tests/cards/.
///
/// Goal: make it cheap to write a test like "Hard Bargain played against
/// a chain containing Lunar Boon, where P2 has 1 ready rune, results in
/// Lunar Boon being countered (P2 can't afford the 2E rescue)."
///
/// Two test patterns supported:
///
///   1. Shared base fixture + inline scenario building (default).
///      Each TEST_F calls helpers to construct exactly the state it needs.
///      Setup is visible at the test site — easiest to read and modify.
///
///        TEST_F(CardTest, MyScenario) {
///            auto spell = addToHand(P1, kHardBargain);
///            pushSpellOnChain(P2, kLunarBoon);  // chain target
///            addRune(P2, Domain::Chaos);        // 1 rune — can't afford 2
///            ...
///            driveCardThroughChain(spell, agent);
///            EXPECT_TRUE(...);
///        }
///
///   2. Subclass fixture for genuinely shared scenarios.
///      When 4+ tests want the same pre-built state, subclass and put the
///      construction in SetUp(). The fixture's SetUp() runs after the base
///      class SetUp() (which loads card_db / card_registry / battlefields).
///
///        class CounterChainFixture : public CardTestFixture {
///        protected:
///            void SetUp() override {
///                CardTestFixture::SetUp();
///                pushSpellOnChain(PlayerId::Player2, 687);  // Lunar Boon
///                addRune(PlayerId::Player2, Domain::Chaos);
///                addRune(PlayerId::Player2, Domain::Chaos);
///                addRune(PlayerId::Player2, Domain::Chaos);
///                addRune(PlayerId::Player2, Domain::Chaos);
///            }
///        };
///        TEST_F(CounterChainFixture, HardBargainCountersWhenP2Declines) { ... }
///
/// Mock agents:
///   FirstChoiceAgent       — always picks legal[0]
///   ScriptedChoiceAgent    — pre-loaded queue of choice-pickers
///   PickByPredicateAgent   — picks the first Intent matching a predicate
///   EnginePathAgent        — for the engine-driven helpers: passes priority,
///                            answers MakeChoice prompts, records them.
///                            DECLINES every "may" unless given a picker.
///   AcceptingEnginePathAgent — the same, accepting every "may"
///
/// THE REAL ENGINE PATH (use this for anything a card does when it is played
/// or when one of its triggers fires):
///
///   playThroughEngine(card, agent)              — play a card from hand
///   fireTriggerThroughEngine(src, trigger, agent) — one trigger of an object
///   resolveThroughEngine(cause, agent)          — whatever `cause` triggers
///
/// These run a real GameEngine on the fixture's `state`: the trigger is put
/// on the chain by TriggerManager, finalized (choices at finalization, CR
/// 383.3), offered for responses and resolved by the engine, which is the
/// only thing that decides what `targets` a card's onTrigger receives (today:
/// none). The direct-call helpers further down (invokeOnResolve,
/// fireTriggerAs, driveResumable*) call the card's callback themselves; they
/// are fine for poking at one branch of a card, but a test that passes with
/// them and has no engine-path counterpart does not show the card works.
///
/// Non-goals:
///   - Driving a full GameEngine turn loop. Use the existing batch runner
///     for that. This fixture is for surgical "play this card, watch what
///     happens" tests.

#include "agents/agent_interface.h"
#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test {

// NOTE: registry.json + ban-list.csv were retired — all card data (including ban
// status) is owned by the C++ classes and the CardDB is built from them
// (buildFromClasses). This helper survives only to derive the decks/ dir.
inline std::string findRegistryPath() {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    if (root) return std::string(root) + "/cards/registry.json";
    return "../cards/registry.json";
}

// ─── Mock agents ────────────────────────────────────────────────────────────

/// Picks legal[0] every time. Useful when you don't care which choice the
/// agent makes — you just want resolution to advance.
class FirstChoiceAgent : public AgentInterface {
public:
    int call_count = 0;
    Intent selectAction(const GameState&,
                        const std::vector<Intent>& legal) override {
        ++call_count;
        return legal.empty() ? Intent{} : legal.front();
    }
};

/// Picks via a per-call lambda. The lambda receives (call#, legal) and
/// returns either an explicit Intent or an index into legal.
class PickByPredicateAgent : public AgentInterface {
public:
    using Picker = std::function<Intent(int, const std::vector<Intent>&)>;
    Picker pick;
    int call_count = 0;

    PickByPredicateAgent() = default;
    explicit PickByPredicateAgent(Picker p) : pick(std::move(p)) {}

    Intent selectAction(const GameState&,
                        const std::vector<Intent>& legal) override {
        if (pick) return pick(call_count++, legal);
        return legal.empty() ? Intent{} : legal.front();
    }
};

/// Returns Intents from a pre-set queue. Each entry is a lambda that
/// inspects the offered legal-action set and returns the chosen Intent.
/// When the queue is exhausted, falls back to legal.front().
class ScriptedChoiceAgent : public AgentInterface {
public:
    using Step = std::function<Intent(const std::vector<Intent>&)>;
    std::vector<Step> queue;
    size_t cursor = 0;

    Intent selectAction(const GameState&,
                        const std::vector<Intent>& legal) override {
        if (cursor < queue.size()) return queue[cursor++](legal);
        return legal.empty() ? Intent{} : legal.front();
    }
};

/// Agent for the engine-driven fixture helpers. Whenever the engine offers
/// priority or focus it passes; a MakeChoice prompt (a "may", a mode, an X, a
/// target) is recorded in `prompts` and answered by `pick` (first option when
/// `pick` is null). Anything else gets the first legal action.
///
/// THE DEFAULT DECLINES EVERY "MAY": Card::confirmOptional lists "No" first,
/// so with no picker a "you may" / "[cost] to [effect]" prompt is answered No
/// and the test exercises the decline path. A test of what the card does when
/// the player accepts must use `AcceptingEnginePathAgent` (below) or a picker
/// (`EnginePathAgent::accepting()`, `choosing(id)`, `script({1, ...})`).
class EnginePathAgent : public AgentInterface {
public:
    using Picker = std::function<Intent(const std::vector<Intent>&)>;
    Picker pick;
    std::vector<std::vector<Intent>> prompts;   // every MakeChoice prompt, in order

    EnginePathAgent() = default;
    explicit EnginePathAgent(Picker p) : pick(std::move(p)) {}

    Intent selectAction(const GameState&,
                        const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        const bool is_choice = std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return i.type == IntentType::MakeChoice; });
        if (is_choice) {
            prompts.push_back(legal);
            return pick ? pick(legal) : legal.front();
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
        return legal.front();
    }

    /// True if any prompt offered `id` as an object to choose.
    bool wasOffered(GameObjectId id) const {
        for (const auto& p : prompts)
            for (const auto& i : p)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return true;
        return false;
    }

    // ── Pickers ──
    /// Choose object `id` when offered; say yes (1) to a yes/no; otherwise first.
    static Picker choosing(GameObjectId id) {
        return [id](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == id) return i;
            for (const auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
            return legal.front();
        };
    }
    /// Say yes to every yes/no (Card::confirmOptional labels its options
    /// "Yes — ..." / "No — ..."); first option of any other prompt, so a mode
    /// or X prompt is not mistaken for a "may".
    static Picker accepting() {
        return [](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (i.choice_label.rfind("Yes", 0) == 0) return i;
            return legal.front();
        };
    }
    /// Answer the n-th MakeChoice prompt with the option whose chosen_value is
    /// values[n] (a yes/no: 1/0; a mode or an X: its number), or — for a
    /// prompt of objects — whose object id is values[n]. First option once the
    /// script runs out or nothing matches.
    static Picker script(std::vector<int> values) {
        auto n = std::make_shared<size_t>(0);
        return [values = std::move(values), n](const std::vector<Intent>& legal) {
            if (*n >= values.size()) return legal.front();
            const int want = values[(*n)++];
            for (const auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == want) return i;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() &&
                    i.chosen_objects.front() == static_cast<GameObjectId>(want)) return i;
            return legal.front();
        };
    }
};

/// An EnginePathAgent that accepts every "may" (see the warning on
/// EnginePathAgent: the default one declines them all).
class AcceptingEnginePathAgent : public EnginePathAgent {
public:
    AcceptingEnginePathAgent() : EnginePathAgent(EnginePathAgent::accepting()) {}
};

// ─── Fixture base ───────────────────────────────────────────────────────────

class CardTestFixture : public ::testing::Test {
protected:
    CardDB card_db;
    CardRegistry card_registry;
    EventBus events;
    GameState state;

    static constexpr PlayerId P1 = PlayerId::Player1;
    static constexpr PlayerId P2 = PlayerId::Player2;

    void SetUp() override {
        card_registry.loadAll();
        card_db.buildFromClasses(card_registry);
        state.mode = ModeOfPlay{};
        state.players[0].id = P1;
        state.players[1].id = P2;
        // Two empty battlefields by default; tests that need more can
        // call addBattlefield() before exercising any card logic.
        addBattlefield();
        addBattlefield();
    }

    // ── Battlefield / mode setup ──

    BattlefieldId addBattlefield() {
        BattlefieldState bf;
        bf.id = static_cast<int>(state.battlefields.size());
        state.battlefields.push_back(bf);
        return bf.id;
    }

    // ── Object construction (mostly thin GameObject wrappers) ──

    /// The printed numbers GameEngine::instantiateCard copies from the def.
    /// Every helper below that builds an object from a def calls this, so a
    /// fixture object has the Might and the Assault / Shield / Deflect values
    /// the same card has in a real game.
    static void copyPrintedStats(GameObject& obj, const CardDef& def) {
        obj.base_might = def.might;
        obj.current_might = def.might;
        obj.assault_value = def.assault_value;
        obj.shield_value = def.shield_value;
        obj.deflect_value = def.deflect_value;
        obj.might_bonus = def.might_bonus;
    }

    /// Add a card-by-id to the given player's hand. If def_id is valid in
    /// CardDB, populates name/type/keywords/domains from the def.
    GameObjectId addToHand(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def_id;
        obj.owner = owner;
        obj.controller = owner;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.card_type = def.card_type;
            obj.super_type = def.super_type;
            obj.keywords = def.keywords;
            obj.domains = def.domains;
            obj.tags = def.tags;
            copyPrintedStats(obj, def);
        }
        obj.zone = ZoneType::Hand;
        state.player(owner).hand.push_back(id);
        return id;
    }

    /// Add a card-by-id to the given player's main deck (top = back).
    GameObjectId addToDeck(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def_id;
        obj.owner = owner;
        obj.controller = owner;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.card_type = def.card_type;
            obj.super_type = def.super_type;
            obj.keywords = def.keywords;
            obj.domains = def.domains;
            obj.tags = def.tags;
            copyPrintedStats(obj, def);
        }
        obj.zone = ZoneType::MainDeck;
        state.player(owner).main_deck.push_back(id);
        return id;
    }

    /// Add a unit at a battlefield (or controller's base if at_bf < 0).
    /// `might` overrides the def's might if > 0; otherwise the def value
    /// is used (or 0 if def_id is invalid).
    ///
    /// Note: at_bf is `int` (not BattlefieldId) so the negative sentinel
    /// works. BattlefieldId is unsigned; -1 would underflow to UINT_MAX
    /// and compare >= 0, silently routing the unit to a non-existent
    /// battlefield. Subagent surfaced this bug 2026-05-15.
    GameObjectId addUnit(PlayerId owner, CardDefId def_id, int might = 0,
                          int at_bf = -1) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Unit;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.super_type = def.super_type;
            obj.keywords = def.keywords;
            obj.domains = def.domains;
            obj.tags = def.tags;
            copyPrintedStats(obj, def);
            if (might > 0) obj.base_might = might;
        } else {
            obj.name = "TestUnit";
            obj.base_might = (might > 0) ? might : 1;
        }
        obj.current_might = obj.base_might;
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    /// Add a ready rune of the given domain in the player's base.
    GameObjectId addRune(PlayerId owner, Domain domain,
                          bool exhausted = false) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_type = CardType::Rune;
        obj.name = std::string(toString(domain)) + " Rune";
        obj.domains = {domain};
        obj.zone = ZoneType::Base;
        obj.location = BaseLocation{owner};
        obj.is_exhausted = exhausted;
        return id;
    }

    /// Set a player's current score (for testing score-gated triggers).
    void setScore(PlayerId p, int score) {
        state.player(p).score = score;
    }

    /// Set a player's XP (for level-gated effects).
    void setXp(PlayerId p, int xp) {
        state.player(p).xp = xp;
    }

    // ── Chain helpers ──

    /// Push a fake spell ChainItem for testing legality / counter shape.
    /// `targets` are the targeted GameObjectIds (e.g., for Repulse-style
    /// "targets a friendly" checks).
    GameObjectId pushSpellOnChain(PlayerId controller,
                                   CardDefId def_id = kInvalidId,
                                   const std::vector<GameObjectId>& targets = {}) {
        auto src = state.createObject();
        auto& obj = state.getObject(src);
        obj.owner = controller;
        obj.controller = controller;
        obj.card_type = CardType::Spell;
        obj.zone = ZoneType::Chain;
        obj.card_def_id = def_id;
        if (def_id != kInvalidId) obj.name = card_db.get(def_id).name;

        ChainItem item;
        item.id = state.chain.allocateId();
        item.source = src;
        item.controller = controller;
        item.is_spell = true;
        item.card_def_id = def_id;
        item.targets = targets;
        state.chain.items.push_back(item);
        return src;
    }

    // ── Cost payment (mirrors GameEngine::payCardCost) ──

    /// Count the player's ready runes (any domain). Useful for asserting
    /// "before play: N ready runes, after play: N-cost ready runes."
    int readyRuneCount(PlayerId p) {
        int n = 0;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isRune() || obj.controller != p) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
            if (!obj.is_exhausted) ++n;
        }
        return n;
    }
    int exhaustedRuneCount(PlayerId p) {
        int n = 0;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isRune() || obj.controller != p) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
            if (obj.is_exhausted) ++n;
        }
        return n;
    }
    /// Count ready runes of a specific domain.
    int readyRuneCount(PlayerId p, Domain d) {
        int n = 0;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isRune() || obj.controller != p) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
            if (obj.is_exhausted) continue;
            for (auto dd : obj.domains) if (dd == d) { ++n; break; }
        }
        return n;
    }

    /// Pay a card's cost: exhaust `energy_cost` ready runes for energy,
    /// then move `power_cost` matching-domain exhausted runes to deck for
    /// power. Returns true on success, false if the player can't afford.
    ///
    /// Mirrors `GameEngine::payCardCost` for tests that want to verify
    /// the rune-state invariants without running a full game. This is a
    /// CONTRACT test, not a test of the engine's cost-payment code —
    /// the engine's code is exercised by every real game.
    bool payCardCostManually(PlayerId p, CardDefId def_id) {
        const auto& def = card_db.get(def_id);
        // 1. Exhaust `energy_cost` ready runes (any domain).
        int need_energy = def.energy_cost;
        for (auto& [id, obj] : state.objects) {
            if (need_energy == 0) break;
            if (!obj.isRune() || obj.controller != p) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
            if (obj.is_exhausted) continue;
            obj.is_exhausted = true;
            --need_energy;
        }
        if (need_energy > 0) return false;  // couldn't pay energy

        // 2. Recycle `power_cost` matching-domain exhausted runes. The
        // card's matching domain is `def.domains[0]` (most cards have 1).
        int need_power = def.power_cost;
        if (need_power > 0 && !def.domains.empty()) {
            Domain dom = def.domains[0];
            std::vector<GameObjectId> to_recycle;
            for (auto& [id, obj] : state.objects) {
                if (static_cast<int>(to_recycle.size()) >= need_power) break;
                if (!obj.isRune() || obj.controller != p) continue;
                if (!obj.location.has_value()) continue;
                if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
                if (!obj.is_exhausted) continue;
                bool matches = false;
                for (auto dd : obj.domains) if (dd == dom) { matches = true; break; }
                if (!matches) continue;
                to_recycle.push_back(id);
            }
            if (static_cast<int>(to_recycle.size()) < need_power) return false;
            for (auto rid : to_recycle) {
                auto& r = state.getObject(rid);
                r.zone = ZoneType::MainDeck;
                r.location = std::nullopt;
                state.player(p).main_deck.insert(state.player(p).main_deck.begin(), rid);
            }
        }
        return true;
    }

    // ── The real engine path ──
    //
    // Each helper builds a GameEngine around a copy of `state`, runs, and
    // copies the result back, so the scenario is built and asserted on with
    // the same `state` / object ids / helpers as every other test. The engine
    // (and its TriggerManager subscription on `events`) lives only for the
    // call. `agent` decides for P1 and `p2_agent` for P2; a null agent passes
    // priority and takes the first option of any prompt.

    /// Run `body(engine)` on a real engine holding the fixture's state, in
    /// `turn_player`'s main phase with nothing on the chain yet.
    void withEngine(const std::function<void(GameEngine&)>& body,
                    AgentInterface* p1_agent = nullptr,
                    AgentInterface* p2_agent = nullptr,
                    PlayerId turn_player = P1) {
        EnginePathAgent default_p1, default_p2;
        GameEngine engine(card_db, events, card_registry);
        engine.mutableState() = state;
        auto& st = engine.mutableState();
        if (st.turn.phase == TurnPhase::Setup) {
            st.turn.phase = TurnPhase::MainPhase;
            st.turn.turn_player = turn_player;
        }
        engine.testHook_setAgents(p1_agent ? p1_agent : &default_p1,
                                  p2_agent ? p2_agent : &default_p2);
        body(engine);
        state = engine.mutableState();
    }

    /// Play `card` from its owner's hand the way a game does: pay its cost,
    /// put it on the chain, resolve it, and resolve every trigger that causes
    /// (its own "When you play me/this" included). A unit or gear goes to
    /// `where` (default: its controller's base). With `fund` the cost is
    /// covered by floating Energy / [A] added here, so no rune has to be set
    /// up or picked; pass false to pay from the runes the test laid out.
    void playThroughEngine(GameObjectId card, AgentInterface* p1_agent = nullptr,
                           std::optional<LocationId> where = std::nullopt,
                           AgentInterface* p2_agent = nullptr, bool fund = true) {
        ASSERT_TRUE(state.objectExists(card));
        const PlayerId player = state.getObject(card).controller;
        ASSERT_TRUE(inHand(player, card)) << "playThroughEngine: the card must be in hand";
        if (fund && state.getObject(card).card_def_id != kInvalidId) {
            const auto& def = card_db.get(state.getObject(card).card_def_id);
            state.player(player).rune_pool.energy += def.energy_cost;
            state.player(player).rune_pool.universal_power += def.power_cost;
        }
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = player;
        play.card = card;
        if (where.has_value()) play.play_location = where;
        else if (!state.getObject(card).isSpell())
            play.play_location = LocationId{BaseLocation{player}};
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); },
                   p1_agent, p2_agent, player);
    }

    /// Put `source`'s triggered ability for `trigger` on the chain as if its
    /// event had just happened (TriggerManager::queueTrigger), then let the
    /// engine finalize and resolve it. `subject` is the event's object (the
    /// unit that died, the card that was played) where the trigger has one.
    /// This is the engine-path replacement for fireTriggerAs: it takes no
    /// targets, because the engine is what supplies them.
    void fireTriggerThroughEngine(GameObjectId source, TriggerType trigger,
                                  AgentInterface* p1_agent = nullptr,
                                  GameObjectId subject = kInvalidId,
                                  AgentInterface* p2_agent = nullptr) {
        ASSERT_TRUE(state.objectExists(source));
        const PlayerId controller = state.getObject(source).controller;
        withEngine([&](GameEngine& e) {
            ASSERT_TRUE(e.testHook_executor().canQueueTriggers());
            e.testHook_executor().queueTriggeredAbility(source, controller, trigger, subject);
            e.testHook_runChain();
        }, p1_agent, p2_agent, controller);
    }

    /// Do `cause` with the engine's executor (kill a unit, move one, deal
    /// damage...), then resolve every trigger the resulting events put on the
    /// chain. For triggers whose event is easier to make happen than to name.
    void resolveThroughEngine(const std::function<void(EffectExecutor&, GameState&)>& cause,
                              AgentInterface* p1_agent = nullptr,
                              AgentInterface* p2_agent = nullptr,
                              PlayerId turn_player = P1) {
        withEngine([&](GameEngine& e) {
            cause(e.testHook_executor(), e.mutableState());
            e.testHook_runChain();
        }, p1_agent, p2_agent, turn_player);
    }

    // ── Card invocation helpers (direct calls — NOT the engine path) ──

    /// Drive a Card's onResolve directly, bypassing ChainManager. Use this
    /// for one-shot cards (no requestChoice). For resumable cards, use
    /// driveResumable() or driveThroughChain().
    void invokeOnResolve(GameObjectId source, CardDefId def_id,
                         PlayerId controller,
                         const std::vector<GameObjectId>& targets,
                         EffectExecutor& exec) {
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr) << "card_def_id " << def_id << " not registered";
        CardContext ctx{state, events, exec, controller, source};
        card->onResolve(ctx, targets);
    }

    /// Drive a resumable trigger (Card::onTrigger) through the
    /// requestChoice / takeChoice loop. Mirrors driveResumable but calls
    /// onTrigger so optional-trigger cards (Virtuoso, Gloomist, …) can be
    /// exercised with both yes and no agent picks.
    ///
    /// `picker` receives the published choice set on each iteration and
    /// returns one Intent — typically `legal[0]` (the "no" slot, since
    /// `Card::confirmOptional` publishes [no, yes] in that order) or
    /// `legal[1]` (the "yes" slot). Tests that want yes/no by name can
    /// inspect `pending.label` (starts with "optional:").
    void driveResumableTrigger(CardDefId def_id, PlayerId controller,
                                GameObjectId source,
                                std::function<Intent(const std::vector<Intent>&)> picker,
                                EffectExecutor& exec,
                                TriggerType firing = TriggerType::None,
                                GameObjectId subject = kInvalidId) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.is_activated_ability = false;
        ri.resume_point = 0;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.fired_trigger = firing;
        ri.triggering_subject = subject;   // the event's object, as TriggerManager records it
        state.chain.resuming = ri;

        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        ctx.firing_trigger = firing;

        card->onTrigger(ctx, {});
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            Intent picked = picker
                ? picker(pending.legal)
                : (pending.legal.empty() ? Intent{} : pending.legal.front());
            exec.recordChoice(picked);
            card->onTrigger(ctx, {});
        }
        state.chain.resuming.reset();
    }

    /// Fire a card's onTrigger directly with a specific firing trigger (for
    /// multi-trigger cards). No resume loop — use for non-resumable triggers.
    ///
    /// Takes no targets: no triggered ability is handed targets by
    /// TriggerManager / the chain (a trigger's target is a choice made at
    /// finalization, CR 383.3), so feeding some here tested a path that does
    /// not exist — that is how the target-less-trigger cards (plans/07 item
    /// 7.1) passed their tests. For a trigger that chooses something use
    /// fireTriggerThroughEngine / playThroughEngine.
    void fireTriggerAs(CardDefId def_id, PlayerId controller, GameObjectId source,
                       TriggerType firing, EffectExecutor& exec) {
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        ctx.firing_trigger = firing;
        card->onTrigger(ctx, {});
    }

    /// Drive a resumable Card through case 0 → case 1 manually using
    /// `picker` to choose from the published choice set. Stand-in for
    /// ChainManager::stepResolve when a test only wants to validate one
    /// card's behavior.
    void driveResumable(CardDefId def_id, PlayerId controller,
                         GameObjectId source,
                         std::function<Intent(const std::vector<Intent>&)> picker,
                         EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        ri.resume_point = 0;
        ri.source = source;
        ri.card_def_id = def_id;
        state.chain.resuming = ri;

        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};

        // case 0
        card->onResolve(ctx, {});
        // resume loop
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            Intent picked = picker
                ? picker(pending.legal)
                : (pending.legal.empty() ? Intent{} : pending.legal.front());
            exec.recordChoice(picked);
            // re-enter at advanced resume_point — read it back from the
            // (mutable) resuming slot, which the card has been updating.
            card->onResolve(ctx, {});
        }
        state.chain.resuming.reset();
    }

    /// Drive a card through the FULL ChainManager FEPR loop using `agent`
    /// for any priority / choice queries. Use this when:
    ///   - the card interacts with the chain (counter spells)
    ///   - you want to test that triggers fire correctly
    ///   - the card has a Repeat / multi-resolution shape
    ///
    /// `gen_actions` (optional) returns priority-pass actions for the
    /// closed-state phase. If null, both players auto-pass.
    void driveThroughChain(GameObjectId spell_source, PlayerId controller,
                           const std::vector<GameObjectId>& targets,
                           AgentInterface& agent,
                           EffectExecutor& exec,
                           std::function<std::vector<Intent>(PlayerId)> gen_actions = nullptr) {
        ChainManager cm(state, events, card_db);
        cm.setEffectExecutor(&exec);

        // Mimic executePlaySpell: remove spell from controller's hand if it's there.
        auto& ps = state.player(controller);
        ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), spell_source),
                      ps.hand.end());

        cm.addSpell(spell_source, controller, targets);

        cm.processFEPR(
            [&](PlayerId p, const std::vector<Intent>& legal) {
                return agent.selectAction(state, legal);
            },
            [&](const ChainItem& /*item*/) { /* permanent resolution: no-op */ },
            [&](const ChainItem& item) {
                Card* c = card_registry.get(item.card_def_id);
                if (!c) return;
                CardContext ctx{state, events, exec, item.controller, item.source};
                c->onResolve(ctx, item.targets);
            },
            gen_actions ? std::function<std::vector<Intent>(PlayerId)>(gen_actions) : nullptr
        );
    }

    // ── Assertion helpers ──

    bool inHand(PlayerId p, GameObjectId id) {
        auto& h = state.player(p).hand;
        return std::find(h.begin(), h.end(), id) != h.end();
    }
    bool inTrash(PlayerId p, GameObjectId id) {
        auto& t = state.player(p).trash;
        return std::find(t.begin(), t.end(), id) != t.end();
    }
    bool inDeck(PlayerId p, GameObjectId id) {
        auto& d = state.player(p).main_deck;
        return std::find(d.begin(), d.end(), id) != d.end();
    }
    int handSize(PlayerId p) { return state.player(p).hand.size(); }
    int deckSize(PlayerId p) { return state.player(p).main_deck.size(); }
    int trashSize(PlayerId p) { return state.player(p).trash.size(); }
};

}  // namespace riftbound::test
