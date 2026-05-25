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

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test {

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
        card_db.loadFromRegistry(findRegistryPath());
        card_registry.loadAll();
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
            obj.base_might = (might > 0) ? might : def.might;
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

    // ── Card invocation helpers ──

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
                                TriggerType firing = TriggerType::None) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.is_activated_ability = false;
        ri.resume_point = 0;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.fired_trigger = firing;
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
    void fireTriggerAs(CardDefId def_id, PlayerId controller, GameObjectId source,
                       TriggerType firing, EffectExecutor& exec,
                       const std::vector<GameObjectId>& targets = {}) {
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        ctx.firing_trigger = firing;
        card->onTrigger(ctx, targets);
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
