/// @file test_review_fix_play_combat.cpp
/// Regression tests for the 2026-10 engine review, play / combat / scoring
/// stream (plans/02-engine-rules.md items 2.6-2.30, plans/08-cards-full-review.md
/// items 8.6-8.12). Every test drives a real GameEngine: intents go through
/// executeIntent / the generators, triggers through TriggerManager and the
/// chain. Where an item is about an engine mechanism rather than one printed
/// card, a small test-only card registered in a reserved id range stands in,
/// so the test does not depend on any real card's implementation.

#include "cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <boost/signals2/connection.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_play_combat {  // file-unique: test sources are unity-built
namespace {

// ─── Test-only cards (ids in the reserved 956..1100 range) ──────────────────

constexpr CardDefId kTVanilla = 1081;       // unit, free, 2 [M]
constexpr CardDefId kTPlayHereBf = 1082;    // battlefield: "when you play a unit here"
constexpr CardDefId kTReactionUnit = 1083;  // unit that is itself a [Reaction] card
constexpr CardDefId kTActionUnit = 1084;    // unit with two [Action] abilities
constexpr CardDefId kTRecycleGear = 1085;   // gear with a recycle-self cost
constexpr CardDefId kTChosenWatcher = 1086; // unit: "when you choose a friendly unit"
constexpr CardDefId kTTwoFriendly = 1087;   // spell: two friendly units, in order
constexpr CardDefId kTUpToThree = 1088;     // spell: up to three units
constexpr CardDefId kTQuickDraw = 1089;     // [Quick-Draw] Equipment, +2 [M]
constexpr CardDefId kTClimbBf = 1090;       // battlefield: +1 point needed to win
constexpr CardDefId kTUnplayable = 1091;    // spell whose canBePlayed is false
constexpr CardDefId kTNoopSpell = 1092;     // spell, free, [Reaction], no effect
constexpr CardDefId kTAbilityOnlyReaction = 1093;  // unit with Keyword::Reaction for an ability
constexpr CardDefId kTBfAdder = 1094;       // unit: "when I attack", adds a battlefield
constexpr CardDefId kTMighty = 1095;        // unit, free, 5 [M]

constexpr CardDefId kPunchFirst = 419;      // spell, [1] + 2 Body: +5 [M] to a unit
constexpr CardDefId kVoidAssault = 758;     // "Move a friendly unit, then move an enemy unit."
constexpr CardDefId kRengarPouncing = 348;  // "I can be played to a battlefield you're attacking."

CardDef makeDef(CardDefId id, const char* name, CardType type) {
    CardDef d;
    d.id = id;
    d.name = name;
    d.card_type = type;
    return d;
}

#define TEST_CARD_DEF(expr)                                   \
    const CardDef& def() const override {                     \
        static const CardDef d = [] { return (expr); }();     \
        return d;                                             \
    }

class TVanilla : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTVanilla, "Test Vanilla", CardType::Unit);
                       d.might = 2; return d; }())
};

class TMighty : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTMighty, "Test Mighty", CardType::Unit);
                       d.might = 5; return d; }())
};

class TPlayHereBf : public BattlefieldCard {
public:
    TEST_CARD_DEF(makeDef(kTPlayHereBf, "Test Play-Here Field", CardType::Battlefield))
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.getObject(ctx.source).card_counters["fired"]++;
    }
};

class TClimbBf : public BattlefieldCard {
public:
    TEST_CARD_DEF(makeDef(kTClimbBf, "Test Climb Field", CardType::Battlefield))
    int victoryScoreModifier() const override { return 1; }
};

class TReactionUnit : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTReactionUnit, "Test Reaction Unit", CardType::Unit);
                       d.might = 2; d.keywords.set(Keyword::Reaction); return d; }())
    bool hasReactionTiming() const override { return true; }
};

// Keyword::Reaction on the def because an ABILITY has it (the Seals, Gold):
// the card itself is not a [Reaction] card.
class TAbilityOnlyReaction : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTAbilityOnlyReaction, "Test Ability-Only Reaction",
                                        CardType::Unit);
                       d.might = 2; d.keywords.set(Keyword::Reaction); return d; }())
};

class TActionUnit : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTActionUnit, "Test Action Unit", CardType::Unit);
                       d.might = 2; return d; }())
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility pay_energy;
        pay_energy.cost.energy = 2;
        pay_energy.is_action = true;
        ActivatedAbility pay_xp;
        pay_xp.cost.xp_cost = 3;
        pay_xp.is_action = true;
        return {pay_energy, pay_xp};
    }
    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>&) override {
        ctx.state.getObject(ctx.source)
            .card_counters["resolved_" + std::to_string(ability_index)]++;
    }
};

class TRecycleGear : public GearCard {
public:
    TEST_CARD_DEF(makeDef(kTRecycleGear, "Test Recycle Gear", CardType::Gear))
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.recycle_self = true;
        return c;
    }
};

class TChosenWatcher : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTChosenWatcher, "Test Chosen Watcher", CardType::Unit);
                       d.might = 2; return d; }())
    TriggerType triggerType() const override {
        return TriggerType::WhenYouChooseAFriendlyUnit;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.getObject(ctx.source).card_counters["fired"]++;
    }
};

// Records the targets it resolved with on the caster's legend-free state:
// card_counters of the spell object ("t0", "t1", "t2", "n").
class TTwoFriendly : public SpellCard {
public:
    TEST_CARD_DEF(makeDef(kTTwoFriendly, "Test Two Friendly", CardType::Spell))
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true, .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& self = ctx.state.getObject(ctx.source);
        self.card_counters["n"] = static_cast<int>(targets.size());
        for (size_t i = 0; i < targets.size(); ++i)
            self.card_counters["t" + std::to_string(i)] = static_cast<int>(targets[i]);
    }
};

class TUpToThree : public SpellCard {
public:
    TEST_CARD_DEF(makeDef(kTUpToThree, "Test Up To Three", CardType::Spell))
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 3, .must_be_unit = true, .optional = true};
    }
};

class TQuickDraw : public GearCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTQuickDraw, "Test Quick-Draw Blade", CardType::Gear);
                       d.tags = {"Equipment"};
                       d.might_bonus = 2;
                       d.keywords.set(Keyword::QuickDraw);
                       return d; }())
};

class TUnplayable : public SpellCard {
public:
    TEST_CARD_DEF(makeDef(kTUnplayable, "Test Unplayable", CardType::Spell))
    bool canBePlayed(const GameState&, PlayerId) const override { return false; }
};

class TNoopSpell : public SpellCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTNoopSpell, "Test Noop Spell", CardType::Spell);
                       d.keywords.set(Keyword::Reaction); return d; }())
};

class TBfAdder : public UnitCard {
public:
    TEST_CARD_DEF([] { auto d = makeDef(kTBfAdder, "Test Field Adder", CardType::Unit);
                       d.might = 5; return d; }())
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Grows state.battlefields, which reallocates the vector.
        ctx.executor.addBattlefieldToken("Test Pit", false);
    }
};

// ─── Agents ─────────────────────────────────────────────────────────────────

/// Passes priority / focus when it can; answers object choices with `want`
/// (or the first offered) and yes/no choices with `yes`. Logs what it saw.
class DrivingAgent : public AgentInterface {
public:
    GameObjectId want = kInvalidId;
    bool yes = false;
    std::vector<std::string> log;
    /// Called every time this agent is asked for priority.
    std::function<void(const GameState&)> on_priority;
    /// Returns an intent to take instead of passing (once), if it wants one.
    std::function<const Intent*(const std::vector<Intent>&)> respond;

    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        bool object_choice = false, value_choice = false;
        for (const auto& i : legal) {
            if (i.type != IntentType::MakeChoice) continue;
            if (!i.chosen_objects.empty()) object_choice = true;
            else if (i.chosen_value.has_value()) value_choice = true;
        }
        if (object_choice) {
            log.push_back("object");
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
            return legal.front();
        }
        if (value_choice) {
            log.push_back("value");
            for (const auto& i : legal)
                if (i.chosen_value.value_or(-1) == (yes ? 1 : 0)) return i;
            return legal.front();
        }
        for (const auto& i : legal) {
            if (i.type != IntentType::PassPriority) continue;
            if (on_priority) on_priority(st);
            if (respond) {
                if (const Intent* r = respond(legal)) {
                    Intent out = *r;
                    respond = nullptr;
                    return out;
                }
            }
            log.push_back("pass");
            return i;
        }
        for (const auto& i : legal)
            if (i.type == IntentType::PassFocus) { log.push_back("pass_focus"); return i; }
        log.push_back("other");
        return legal.front();
    }
};

// ─── Fixture ────────────────────────────────────────────────────────────────

class PlayCombatFixTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    DrivingAgent a1_, a2_;
    std::vector<boost::signals2::scoped_connection> conns_;

    GameState& s() { return engine_->mutableState(); }

    void SetUp() override {
        CardTestFixture::SetUp();
        card_registry.registerCard(kTVanilla, std::make_unique<TVanilla>());
        card_registry.registerCard(kTMighty, std::make_unique<TMighty>());
        card_registry.registerCard(kTPlayHereBf, std::make_unique<TPlayHereBf>());
        card_registry.registerCard(kTClimbBf, std::make_unique<TClimbBf>());
        card_registry.registerCard(kTReactionUnit, std::make_unique<TReactionUnit>());
        card_registry.registerCard(kTAbilityOnlyReaction,
                                   std::make_unique<TAbilityOnlyReaction>());
        card_registry.registerCard(kTActionUnit, std::make_unique<TActionUnit>());
        card_registry.registerCard(kTRecycleGear, std::make_unique<TRecycleGear>());
        card_registry.registerCard(kTChosenWatcher, std::make_unique<TChosenWatcher>());
        card_registry.registerCard(kTTwoFriendly, std::make_unique<TTwoFriendly>());
        card_registry.registerCard(kTUpToThree, std::make_unique<TUpToThree>());
        card_registry.registerCard(kTQuickDraw, std::make_unique<TQuickDraw>());
        card_registry.registerCard(kTUnplayable, std::make_unique<TUnplayable>());
        card_registry.registerCard(kTNoopSpell, std::make_unique<TNoopSpell>());
        card_registry.registerCard(kTBfAdder, std::make_unique<TBfAdder>());
        card_db.buildFromClasses(card_registry);
    }

    /// An engine in P1's main phase, Neutral Open, with `n_bfs` battlefields
    /// and a blank legend each.
    void makeEngine(int n_bfs = 2) {
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& st = s();
        st.mode = ModeOfPlay{};
        st.players[0].id = P1;
        st.players[1].id = P2;
        for (int i = 0; i < n_bfs; ++i) {
            BattlefieldState bf;
            bf.id = static_cast<int>(st.battlefields.size());
            st.battlefields.push_back(bf);
        }
        engine_->testHook_setAgents(&a1_, &a2_);
        st.turn.turn_player = P1;
        st.turn.phase = TurnPhase::MainPhase;
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Open;
        for (PlayerId p : {P1, P2}) {
            auto id = st.createObject();
            auto& lo = st.getObject(id);
            lo.owner = lo.controller = p;
            lo.card_type = CardType::Legend;
            lo.zone = ZoneType::LegendZone;
            st.player(p).legend_zone = id;
        }
    }

    /// A card object built the way GameEngine::instantiateCard builds one.
    GameObjectId makeCard(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        const auto& def = card_db.get(def_id);
        c.card_def_id = def_id;
        c.owner = c.controller = p;
        c.name = def.name;
        c.card_type = def.card_type;
        c.super_type = def.super_type;
        c.tags = def.tags;
        c.domains = def.domains;
        c.base_might = c.current_might = def.might;
        c.keywords = def.keywords;
        c.assault_value = def.assault_value;
        c.shield_value = def.shield_value;
        c.deflect_value = def.deflect_value;
        c.might_bonus = def.might_bonus;
        return id;
    }
    GameObjectId inHand(PlayerId p, CardDefId def_id) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId inChampionZone(PlayerId p, CardDefId def_id) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::ChampionZone;
        s().player(p).champion_zone = id;
        return id;
    }
    GameObjectId inDeck(PlayerId p, CardDefId def_id) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::MainDeck;
        s().player(p).main_deck.push_back(id);
        return id;
    }
    GameObjectId inTrash(PlayerId p, CardDefId def_id) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::Trash;
        s().player(p).trash.push_back(id);
        return id;
    }
    /// On the board: at battlefield `at_bf`, or in `p`'s base when negative.
    GameObjectId onBoard(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = makeCard(p, def_id);
        auto& c = s().getObject(id);
        if (at_bf >= 0) {
            c.zone = ZoneType::BattlefieldZone;
            c.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            c.zone = ZoneType::Base;
            c.location = BaseLocation{p};
        }
        return id;
    }
    /// Give battlefield `bf` a card object of `def_id`.
    GameObjectId setBattlefieldCard(int bf, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        c.card_def_id = def_id;
        c.card_type = CardType::Battlefield;
        c.name = card_db.get(def_id).name;
        c.owner = c.controller = PlayerId::None;
        s().battlefields[bf].card_object_id = id;
        return id;
    }
    void control(int bf, PlayerId p) { s().battlefields[bf].controller = p; }

    Intent playIntent(PlayerId p, GameObjectId card,
                      std::optional<LocationId> loc = std::nullopt) {
        Intent i;
        i.type = IntentType::PlayCard;
        i.player = p;
        i.card = card;
        i.play_location = loc;
        return i;
    }
    std::vector<Intent> playsOf(const std::vector<Intent>& legal, GameObjectId card) {
        std::vector<Intent> out;
        for (const auto& i : legal)
            if ((i.type == IntentType::PlayCard || i.type == IntentType::PlayActionCard ||
                 i.type == IntentType::PlayReaction) && i.card == card) out.push_back(i);
        return out;
    }
    static bool playsTo(const std::vector<Intent>& plays, LocationId loc) {
        return std::any_of(plays.begin(), plays.end(), [&](const Intent& i) {
            return i.play_location.has_value() && *i.play_location == loc; });
    }
    /// Put the engine in a showdown at `bf` with `focus` holding focus.
    void enterShowdown(PlayerId focus) {
        s().turn.ns_state = NeutralShowdownState::Showdown;
        s().turn.oc_state = OpenClosedState::Open;
        s().turn.focus_holder = focus;
        s().turn.priority_holder = focus;
    }
    /// Put the engine in a Closed State with `holder` holding priority.
    void enterClosedState(PlayerId holder) {
        s().turn.oc_state = OpenClosedState::Closed;
        s().turn.priority_holder = holder;
    }
};

// ─── 2.6: spell metadata goes on the spell's own chain item ─────────────────

TEST_F(PlayCombatFixTest, SpellMetadata_LandsOnTheSpell_NotOnATriggerPushedAboveIt) {
    // A spell that chooses a friendly unit makes "when you choose a friendly
    // unit" abilities trigger as it is put on the chain; they sit above it.
    // The spell's own item must still carry what was paid for it and the
    // "target already chosen" mark (so it is not asked for again).
    makeEngine();
    auto watcher = onBoard(P1, kTChosenWatcher);
    auto target = onBoard(P1, kTVanilla);
    a1_.want = target;
    s().player(P1).rune_pool.energy = 1;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 2;
    auto spell = inHand(P1, kPunchFirst);

    int spell_energy_seen = -1, trigger_energy_seen = -1, items_seen = 0;
    a2_.on_priority = [&](const GameState& st) {
        if (items_seen != 0) return;           // first look only: both still on the chain
        items_seen = static_cast<int>(st.chain.items.size());
        for (const auto& ci : st.chain.items) {
            if (ci.is_spell && ci.source == spell) spell_energy_seen = ci.total_energy_spent;
            if (ci.is_ability && ci.source == watcher)
                trigger_energy_seen = ci.total_energy_spent;
        }
    };

    engine_->testHook_executeIntent(playIntent(P1, spell)); settleLikeTheEngine(*engine_);

    EXPECT_EQ(s().getObject(watcher).card_counters["fired"], 1)
        << "the watcher's trigger did go on the chain";
    EXPECT_EQ(items_seen, 2);
    EXPECT_EQ(spell_energy_seen, 1) << "energy spent is recorded on the spell";
    EXPECT_EQ(trigger_energy_seen, 0) << "and not on the trigger above it";
    EXPECT_EQ(std::count(a1_.log.begin(), a1_.log.end(), "object"), 1)
        << "the target chosen at play is not asked for again at resolution";
    EXPECT_EQ(s().getObject(target).temp_might_bonus, 5);
}

// ─── 2.7: Reaction-typed intents chosen in a showdown execute ───────────────

TEST_F(PlayCombatFixTest, Showdown_PlayReactionIntent_PlaysThePermanent) {
    // Rengar, Pouncing: "I can be played to a battlefield you're attacking."
    // The showdown generator offers it as a PlayReaction intent.
    makeEngine();
    control(0, P2);
    onBoard(P1, kTVanilla, 0);
    onBoard(P2, kTVanilla, 0);
    auto& bf = s().battlefields[0];
    bf.combat_in_progress = true;
    bf.is_contested = true;
    bf.contested_by = P1;
    bf.attacker = P1;
    bf.defender = P2;
    enterShowdown(P1);
    s().player(P1).rune_pool.energy = 3;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    auto rengar = inHand(P1, kRengarPouncing);

    const auto legal = engine_->generateLegalActions();
    std::optional<Intent> pounce;
    for (const auto& i : legal)
        if (i.type == IntentType::PlayReaction && i.card == rengar) pounce = i;
    ASSERT_TRUE(pounce.has_value()) << "the pounce play is offered";

    int count = 0;
    engine_->testHook_resolveShowdownDecision(0, *pounce, P1, count);

    const auto& r = s().getObject(rengar);
    ASSERT_TRUE(r.location.has_value()) << "the intent was executed, not dropped";
    EXPECT_EQ(*r.location, LocationId{BattlefieldLocation{0}});
    EXPECT_TRUE(s().player(P1).hand.empty());
}

// ─── 2.8: [Action] abilities in a showdown pass the usual gates ─────────────

TEST_F(PlayCombatFixTest, Showdown_ActionAbility_NotOfferedWhenItsCostCannotBePaid) {
    makeEngine();
    auto unit = onBoard(P1, kTActionUnit);
    enterShowdown(P1);

    auto offered = [&](int ability) {
        for (const auto& i : engine_->generateLegalActions())
            if (i.type == IntentType::ActivateActionAbility && i.ability_source == unit &&
                i.ability_index == ability) return true;
        return false;
    };
    EXPECT_FALSE(offered(0)) << "[2]: no energy and no runes";
    EXPECT_FALSE(offered(1)) << "3 XP: the player has none";

    s().player(P1).rune_pool.energy = 2;
    s().player(P1).xp = 3;
    EXPECT_TRUE(offered(0));
    EXPECT_TRUE(offered(1));
}

TEST_F(PlayCombatFixTest, ActivatedAbility_UnpayableCostIsRefused_NotQueuedForFree) {
    makeEngine();
    auto unit = onBoard(P1, kTActionUnit);
    enterShowdown(P1);

    for (int ability : {0, 1}) {
        Intent act;
        act.type = IntentType::ActivateActionAbility;
        act.player = P1;
        act.ability_source = unit;
        act.ability_index = ability;
        engine_->testHook_executeIntent(act); settleLikeTheEngine(*engine_);
    }

    EXPECT_EQ(s().getObject(unit).card_counters["resolved_0"], 0);
    EXPECT_EQ(s().getObject(unit).card_counters["resolved_1"], 0);
    EXPECT_EQ(s().player(P1).xp, 0) << "XP never goes negative";
    EXPECT_EQ(s().player(P1).abilities_activated_this_turn, 0);
}

// ─── 2.9: trash-replay plays pass the play gates and pay cost increases ─────

TEST_F(PlayCombatFixTest, TrashReplay_RespectsCanBePlayedAndTheNameLock) {
    makeEngine();
    auto ok = inTrash(P1, kTNoopSpell);
    auto gated = inTrash(P1, kTUnplayable);
    for (auto id : {ok, gated}) {
        PlayerState::TrashReplayGrant g;
        g.card = id;
        s().player(P1).trash_replay_grants.push_back(g);
    }
    auto replays = [&](GameObjectId card) {
        int n = 0;
        for (const auto& i : engine_->generateLegalActions())
            if (i.play_source == Intent::PlaySource::Trash && i.card == card) ++n;
        return n;
    };
    EXPECT_EQ(replays(ok), 1);
    EXPECT_EQ(replays(gated), 0) << "Card::canBePlayed applies to a play from trash";

    s().player(P1).locked_spell_names.push_back(card_db.get(kTNoopSpell).name);
    EXPECT_EQ(replays(ok), 0) << "a name-locked spell can't be replayed either";
}

TEST_F(PlayCombatFixTest, TrashReplay_PaysTheSpellTaxOnTopOfTheGrant) {
    // CR 356.1.a / 356.3: the grant replaces the base cost; increases still apply.
    makeEngine();
    auto spell = inTrash(P1, kTNoopSpell);
    PlayerState::TrashReplayGrant g;
    g.card = spell;                      // "play this from your trash for [0]"
    s().player(P1).trash_replay_grants.push_back(g);
    s().player(P1).spell_tax_energy = 2; // "spells cost [2] more"

    auto replay = [&]() -> std::optional<Intent> {
        for (const auto& i : engine_->generateLegalActions())
            if (i.play_source == Intent::PlaySource::Trash && i.card == spell) return i;
        return std::nullopt;
    };
    s().player(P1).rune_pool.energy = 1;
    EXPECT_FALSE(replay().has_value()) << "[0] + [2] tax is not affordable with [1]";

    s().player(P1).rune_pool.energy = 3;
    auto offer = replay();
    ASSERT_TRUE(offer.has_value());
    engine_->testHook_executeIntent(*offer); settleLikeTheEngine(*engine_);
    EXPECT_EQ(s().player(P1).rune_pool.energy, 1) << "the [2] tax was charged";
}

// ─── 2.12 / 2.23: where a played unit is, as it is played and after ─────────

TEST_F(PlayCombatFixTest, PlayUnitToBattlefield_FiresPlayHereTrigger_AndSetsBattlefieldZone) {
    makeEngine();
    control(0, P1);
    onBoard(P1, kTVanilla, 0);               // so P1 keeps control of BF 0
    auto bf_card = setBattlefieldCard(0, kTPlayHereBf);

    auto to_base = inHand(P1, kTVanilla);
    engine_->testHook_executeIntent(playIntent(P1, to_base, BaseLocation{P1})); settleLikeTheEngine(*engine_);
    EXPECT_EQ(s().getObject(bf_card).card_counters["fired"], 0)
        << "a unit played to base was not played 'here'";
    EXPECT_EQ(s().getObject(to_base).zone, ZoneType::Base);

    auto to_bf = inHand(P1, kTVanilla);
    engine_->testHook_executeIntent(playIntent(P1, to_bf, BattlefieldLocation{0})); settleLikeTheEngine(*engine_);
    EXPECT_EQ(s().getObject(bf_card).card_counters["fired"], 1)
        << "\"when you play a unit here\" fires on a normal play from hand";
    const auto& u = s().getObject(to_bf);
    ASSERT_TRUE(u.location.has_value());
    EXPECT_EQ(*u.location, LocationId{BattlefieldLocation{0}});
    EXPECT_EQ(u.zone, ZoneType::BattlefieldZone)
        << "a unit that lands on a battlefield is in the battlefield zone";
}

// ─── 2.18: the combat result (CR 468.3) ─────────────────────────────────────

class CombatResultTest : public PlayCombatFixTest {
protected:
    std::vector<std::optional<PlayerId>> results_;
    void stageCombat() {
        makeEngine();
        control(0, P2);
        auto& bf = s().battlefields[0];
        bf.is_contested = true;
        bf.contested_by = P1;
        bf.combat_staged = true;
        conns_.emplace_back(events.on_combat_ended.connect(
            [this](const CombatEndedEvent& e) { results_.push_back(e.winner); }));
    }
    GameObjectId fighter(PlayerId p, int might, bool stunned = false) {
        auto id = onBoard(p, kTVanilla, 0);
        auto& u = s().getObject(id);
        u.base_might = u.current_might = might;
        u.is_stunned = stunned;
        return id;
    }
};

TEST_F(CombatResultTest, AttackerWhoWipesTheDefendersWins) {
    stageCombat();
    fighter(P1, 5);
    fighter(P2, 2);
    engine_->testHook_runCombat(0);
    ASSERT_EQ(results_.size(), 1u);
    ASSERT_TRUE(results_[0].has_value());
    EXPECT_EQ(*results_[0], P1);
    EXPECT_EQ(s().battlefields[0].controller, std::optional<PlayerId>(P1));
}

TEST_F(CombatResultTest, DoubleWipeHasNoWinner) {
    // CR 468.3.d: neither player has units left — "No Result".
    stageCombat();
    fighter(P1, 2);
    fighter(P2, 2);
    engine_->testHook_runCombat(0);
    ASSERT_EQ(results_.size(), 1u);
    EXPECT_FALSE(results_[0].has_value())
        << "no winner: the event must not carry an engaged PlayerId::None";
    EXPECT_FALSE(s().battlefields[0].controller.has_value());
}

TEST_F(CombatResultTest, TieWithRecalledAttackersHasNoWinner) {
    // CR 468.3.d: units were recalled in the Combat Cleanup — "No Result",
    // even though the defender is then the only player left there.
    stageCombat();
    auto att = fighter(P1, 3, /*stunned=*/true);   // stunned units deal no damage
    auto def = fighter(P2, 3, /*stunned=*/true);
    engine_->testHook_runCombat(0);
    ASSERT_EQ(results_.size(), 1u);
    EXPECT_FALSE(results_[0].has_value()) << "the defender did not win a tie";
    EXPECT_TRUE(s().getObject(att).isAtBase()) << "attackers are recalled";
    EXPECT_TRUE(s().getObject(def).isAtBattlefield());
    EXPECT_EQ(s().battlefields[0].controller, std::optional<PlayerId>(P2))
        << "the defender keeps control";
}

// ─── 2.19 / 2.20: scoring ───────────────────────────────────────────────────

class ScoringFixTest : public PlayCombatFixTest {
protected:
    std::vector<ScoreEvent> scores_;
    void watchScores() {
        conns_.emplace_back(events.on_score.connect(
            [this](const ScoreEvent& e) { scores_.push_back(e); }));
    }
};

TEST_F(ScoringFixTest, FinalPointCheck_UsesTheEffectiveVictoryScore) {
    // "Increase the points needed to win the game by 1": at 7 of 9, a conquer
    // is an ordinary point, not the Final Point.
    makeEngine();
    setBattlefieldCard(1, kTClimbBf);
    inDeck(P1, kTVanilla);
    s().player(P1).score = 7;

    engine_->testHook_scoreConquer(P1, 0);

    EXPECT_EQ(s().player(P1).score, 8) << "the point is gained";
    EXPECT_TRUE(s().player(P1).hand.empty()) << "nothing is drawn instead";
    EXPECT_FALSE(s().game_over) << "9 are needed";
}

TEST_F(ScoringFixTest, ConquerWhoseFinalPointBecomesADraw_StillTriggersConquerAbilities) {
    // CR 473.1.b.1 replaces the point with a draw; CR 473.2 still triggers the
    // battlefield's Score abilities.
    makeEngine();
    watchScores();
    inDeck(P1, kTVanilla);
    s().player(P1).score = 7;             // 1 from the Victory Score of 8

    engine_->testHook_scoreConquer(P1, 0);   // BF 1 was not scored this turn

    EXPECT_EQ(s().player(P1).score, 7);
    EXPECT_EQ(s().player(P1).hand.size(), 1u) << "drew a card instead";
    ASSERT_EQ(scores_.size(), 1u) << "the conquer is announced";
    EXPECT_EQ(scores_[0].method, ScoreMethod::Conquer);
    EXPECT_EQ(scores_[0].battlefield, 0u);
    EXPECT_TRUE(s().player(P1).battlefields_scored_this_turn.count(0));
}

TEST_F(ScoringFixTest, CantGainPoints_StillScoresTheBattlefield) {
    // "Opponents can't gain points": Scoring is distinct from Gaining Points
    // (CR 470.1), so the conquer / hold happens and triggers; no point.
    makeEngine();
    watchScores();
    s().player(P1).cannot_gain_points = true;
    s().player(P1).score = 2;

    engine_->testHook_scoreConquer(P1, 0);
    engine_->testHook_scoreHold(P1, 1);

    EXPECT_EQ(s().player(P1).score, 2);
    ASSERT_EQ(scores_.size(), 2u);
    EXPECT_EQ(scores_[0].method, ScoreMethod::Conquer);
    EXPECT_EQ(scores_[1].method, ScoreMethod::Hold);
    EXPECT_TRUE(s().player(P1).battlefields_scored_this_turn.count(0));
    EXPECT_TRUE(s().player(P1).battlefields_scored_this_turn.count(1));
}

// ─── 2.24: the Chosen Champion is played like a unit from hand ──────────────

TEST_F(PlayCombatFixTest, ChampionZonePlay_RespectsTheLockoutAndGetsControlledBattlefields) {
    makeEngine();
    control(0, P1);
    onBoard(P1, kTVanilla, 0);
    auto champ = inChampionZone(P1, kTVanilla);

    auto plays = playsOf(engine_->generateLegalActions(), champ);
    EXPECT_TRUE(playsTo(plays, BaseLocation{P1}));
    EXPECT_TRUE(playsTo(plays, BattlefieldLocation{0}))
        << "CR 420.1.a / 355.2.a: a battlefield you control is a valid location";
    EXPECT_FALSE(playsTo(plays, BattlefieldLocation{1}));

    s().player(P1).cant_play_cards_this_turn = true;
    EXPECT_TRUE(playsOf(engine_->generateLegalActions(), champ).empty())
        << "\"can't play cards this turn\" covers the champion zone too";
}

// ─── 2.27: a recycle-self cost is a real departure from the board ───────────

TEST_F(PlayCombatFixTest, RecycleSelfCost_GoesToTheOwnersDeckAndAnnouncesLeavingTheBoard) {
    makeEngine();
    auto gear = onBoard(P1, kTRecycleGear);
    s().getObject(gear).owner = P2;          // P1 controls a card P2 owns
    std::vector<LeftBoardEvent> left;
    conns_.emplace_back(events.on_left_board.connect(
        [&](const LeftBoardEvent& e) { left.push_back(e); }));
    // 9.D4: the recycle is announced ("When you recycle one or more cards").
    int recycled_announced = 0;
    conns_.emplace_back(events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.object == gear && e.what_changed == "recycled_main") ++recycled_announced;
        }));

    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = gear;
    engine_->testHook_executeIntent(act); settleLikeTheEngine(*engine_);

    const auto& g = s().getObject(gear);
    EXPECT_EQ(g.zone, ZoneType::MainDeck);
    EXPECT_FALSE(g.location.has_value());
    const auto& owner_deck = s().player(P2).main_deck;
    EXPECT_EQ(std::count(owner_deck.begin(), owner_deck.end(), gear), 1)
        << "recycled into its OWNER's deck";
    EXPECT_TRUE(s().player(P1).main_deck.empty());
    ASSERT_EQ(left.size(), 1u);
    EXPECT_EQ(left[0].object, gear);
    EXPECT_EQ(left[0].destination, ZoneType::MainDeck);
    EXPECT_EQ(recycled_announced, 1);
}

TEST_F(PlayCombatFixTest, RecycleSelfCost_TokenCeasesToExistInstead) {
    makeEngine();
    auto gear = onBoard(P1, kTRecycleGear);
    s().getObject(gear).super_type = SuperType::Token;
    int recycled_announced = 0;
    conns_.emplace_back(events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.what_changed == "recycled_main") ++recycled_announced;
        }));

    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = gear;
    engine_->testHook_executeIntent(act); settleLikeTheEngine(*engine_);

    EXPECT_EQ(s().getObject(gear).zone, ZoneType::Banishment) << "CR 183.1";
    EXPECT_TRUE(s().player(P1).main_deck.empty()) << "a token is never shuffled into a deck";
    EXPECT_EQ(recycled_announced, 0) << "nothing was recycled into a deck";
}

// ─── 2.29: "becomes Mighty" re-arms when the unit leaves the board ──────────

TEST_F(PlayCombatFixTest, BecameMighty_FiresAgainForAUnitThatReturnsToTheBoard) {
    makeEngine();
    auto unit = onBoard(P1, kTMighty);
    int became = 0;
    conns_.emplace_back(events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.object == unit && e.what_changed == "became_mighty") ++became;
        }));

    engine_->testHook_cleanup();
    ASSERT_EQ(became, 1);
    engine_->testHook_cleanup();
    EXPECT_EQ(became, 1) << "no repeat while it stays Mighty";

    engine_->testHook_executor().killObject(unit);
    engine_->testHook_cleanup();
    ASSERT_EQ(s().getObject(unit).zone, ZoneType::Trash);

    // It comes back (replayed, returned by an effect, ...).
    auto& u = s().getObject(unit);
    auto& trash = s().player(P1).trash;
    trash.erase(std::remove(trash.begin(), trash.end(), unit), trash.end());
    u.zone = ZoneType::Base;
    u.location = BaseLocation{P1};
    u.recomputeMight();
    engine_->testHook_cleanup();
    EXPECT_EQ(became, 2) << "a returning 5 [M] unit becomes Mighty anew";
}

// ─── 2.30: no BattlefieldState reference survives a chain that adds one ─────

TEST_F(PlayCombatFixTest, MainPhase_CombatThatAddsABattlefield_ClearsTheRightStagedFlag) {
    // The attacker's "when I attack" ability adds a battlefield while the
    // combat is running, which reallocates state.battlefields. The staged
    // flag must be cleared on the real battlefield afterwards.
    makeEngine();
    control(0, P2);
    onBoard(P1, kTBfAdder, 0);
    auto def = onBoard(P2, kTVanilla, 0);
    (void)def;
    auto& bf = s().battlefields[0];
    bf.is_contested = true;
    bf.contested_by = P1;
    bf.combat_staged = true;

    engine_->testHook_advanceMainPhase();

    ASSERT_EQ(s().battlefields.size(), 3u) << "the trigger added a battlefield";
    EXPECT_FALSE(s().battlefields[0].combat_staged);
    EXPECT_FALSE(s().battlefields[0].combat_in_progress);
    EXPECT_EQ(s().battlefields[0].combat_phase, CombatPhase::None);
    EXPECT_EQ(s().battlefields[0].controller, std::optional<PlayerId>(P1))
        << "the 5 [M] attacker won";
}

// ─── 8.6: multi-target spells ───────────────────────────────────────────────

TEST_F(PlayCombatFixTest, MultiTarget_TwoFriendlyTargets_AreOfferedAsOrderedPairs) {
    // "A friendly unit ... another friendly unit": no enemy is involved, and
    // the two slots are different roles, so both orders are distinct plays.
    makeEngine();
    auto a = onBoard(P1, kTVanilla);
    auto b = onBoard(P1, kTVanilla);
    onBoard(P2, kTVanilla);
    auto spell = inHand(P1, kTTwoFriendly);

    auto plays = playsOf(engine_->generateLegalActions(), spell);
    ASSERT_EQ(plays.size(), 2u);
    auto has = [&](std::vector<GameObjectId> t) {
        return std::any_of(plays.begin(), plays.end(),
                           [&](const Intent& i) { return i.targets == t; });
    };
    EXPECT_TRUE(has({a, b}));
    EXPECT_TRUE(has({b, a}));

    // And the chosen pair reaches the card in that order.
    for (const auto& i : plays)
        if (i.targets == std::vector<GameObjectId>{b, a}) { engine_->testHook_executeIntent(i); settleLikeTheEngine(*engine_); }
    const auto& sp = s().getObject(spell);
    EXPECT_EQ(sp.card_counters.at("n"), 2);
    EXPECT_EQ(sp.card_counters.at("t0"), static_cast<int>(b));
    EXPECT_EQ(sp.card_counters.at("t1"), static_cast<int>(a));
}

TEST_F(PlayCombatFixTest, MultiTarget_UpToThree_OffersEverySetOnce) {
    makeEngine();
    onBoard(P1, kTVanilla);
    onBoard(P1, kTVanilla);
    onBoard(P2, kTVanilla);
    auto spell = inHand(P1, kTUpToThree);

    auto plays = playsOf(engine_->generateLegalActions(), spell);
    // 3 single units + 3 pairs + 1 triple.
    ASSERT_EQ(plays.size(), 7u);
    int singles = 0, pairs = 0, triples = 0;
    std::vector<std::vector<GameObjectId>> seen;
    for (const auto& i : plays) {
        auto t = i.targets;
        std::sort(t.begin(), t.end());
        EXPECT_EQ(std::adjacent_find(t.begin(), t.end()), t.end()) << "no unit chosen twice";
        EXPECT_EQ(std::find(seen.begin(), seen.end(), t), seen.end()) << "each set once";
        seen.push_back(t);
        if (t.size() == 1) ++singles;
        if (t.size() == 2) ++pairs;
        if (t.size() == 3) ++triples;
    }
    EXPECT_EQ(singles, 3);
    EXPECT_EQ(pairs, 3);
    EXPECT_EQ(triples, 1);
}

TEST_F(PlayCombatFixTest, MultiTarget_VoidAssault_StillPairsAFriendlyWithAnEnemy) {
    // "Move a friendly unit, then move an enemy unit." The card lists its own
    // pairs now that the engine no longer assumes friendly-plus-enemy.
    makeEngine();
    auto f1 = onBoard(P1, kTVanilla);
    auto f2 = onBoard(P1, kTVanilla);
    auto e1 = onBoard(P2, kTVanilla);
    s().player(P1).rune_pool.energy = 10;
    for (int d = 0; d < static_cast<int>(Domain::Count); ++d)
        s().player(P1).rune_pool.power[d] = 5;
    auto spell = inHand(P1, kVoidAssault);

    auto plays = playsOf(engine_->generateLegalActions(), spell);
    ASSERT_EQ(plays.size(), 2u);
    for (const auto& i : plays) {
        ASSERT_EQ(i.targets.size(), 2u);
        EXPECT_TRUE(i.targets[0] == f1 || i.targets[0] == f2);
        EXPECT_EQ(i.targets[1], e1);
    }
}

// ─── 8.10: units that are themselves [Reaction] cards ───────────────────────

TEST_F(PlayCombatFixTest, ReactionUnit_IsOfferedInAClosedState_ToBaseAndControlledBattlefields) {
    makeEngine();
    control(0, P1);
    onBoard(P1, kTVanilla, 0);
    auto reaction = inHand(P1, kTReactionUnit);
    auto ability_only = inHand(P1, kTAbilityOnlyReaction);
    enterClosedState(P1);

    const auto legal = engine_->generateLegalActions();
    auto plays = playsOf(legal, reaction);
    EXPECT_TRUE(playsTo(plays, BaseLocation{P1}));
    EXPECT_TRUE(playsTo(plays, BattlefieldLocation{0})) << "CR 813.3.a";
    EXPECT_FALSE(playsTo(plays, BattlefieldLocation{1}));
    for (const auto& i : plays) EXPECT_EQ(i.type, IntentType::PlayReaction);
    EXPECT_TRUE(playsOf(legal, ability_only).empty())
        << "a unit with only a [Reaction] ABILITY is not a [Reaction] card";
}

TEST_F(PlayCombatFixTest, ReactionUnit_IsOfferedAndPlayedInAShowdown) {
    makeEngine();
    auto reaction = inHand(P1, kTReactionUnit);
    enterShowdown(P1);

    auto plays = playsOf(engine_->generateLegalActions(), reaction);
    ASSERT_FALSE(plays.empty()) << "CR 813.1.b: Reaction includes Action's permissions";
    int count = 0;
    engine_->testHook_resolveShowdownDecision(0, plays.front(), P1, count);
    EXPECT_TRUE(s().getObject(reaction).location.has_value());
}

TEST_F(PlayCombatFixTest, ReactionUnit_PlayedFromTheChampionZoneInResponseToASpell) {
    // P2 plays a spell; P1 responds with their Chosen Champion, a [Reaction]
    // unit, from the Champion Zone (CR 420.1.a) to a battlefield they control.
    makeEngine();
    s().turn.turn_player = P2;
    control(0, P1);
    onBoard(P1, kTVanilla, 0);
    auto champ = inChampionZone(P1, kTReactionUnit);
    auto spell = inHand(P2, kTNoopSpell);

    std::optional<Intent> response;
    a1_.respond = [&](const std::vector<Intent>& legal) -> const Intent* {
        for (const auto& i : legal)
            if (i.type == IntentType::PlayReaction && i.card == champ &&
                i.play_location.has_value() &&
                *i.play_location == LocationId{BattlefieldLocation{0}}) {
                response = i;
                return &*response;
            }
        return nullptr;
    };

    engine_->testHook_executeIntent(playIntent(P2, spell)); settleLikeTheEngine(*engine_);

    ASSERT_TRUE(response.has_value()) << "the response was offered";
    const auto& c = s().getObject(champ);
    ASSERT_TRUE(c.location.has_value());
    EXPECT_EQ(*c.location, LocationId{BattlefieldLocation{0}});
    EXPECT_EQ(c.zone, ZoneType::BattlefieldZone);
    EXPECT_EQ(s().player(P1).champion_zone, kInvalidId)
        << "it left the Champion Zone";
}

// ─── 8.12: [Quick-Draw] attaches whenever the gear is played ────────────────

TEST_F(PlayCombatFixTest, QuickDraw_PlayedInTheMainPhase_AttachesToAUnitYouChoose) {
    // CR 819.1.d: "When you play this, attach it to a unit you control."
    makeEngine();
    onBoard(P1, kTVanilla);
    auto chosen = onBoard(P1, kTVanilla);
    onBoard(P2, kTVanilla);
    a1_.want = chosen;
    auto gear = inHand(P1, kTQuickDraw);

    auto plays = playsOf(engine_->generateLegalActions(), gear);
    ASSERT_EQ(plays.size(), 1u);
    engine_->testHook_executeIntent(plays.front()); settleLikeTheEngine(*engine_);

    const auto& g = s().getObject(gear);
    ASSERT_TRUE(g.attached_to.has_value()) << "played in the main phase, it still attaches";
    EXPECT_EQ(*g.attached_to, chosen);
    EXPECT_EQ(s().getObject(chosen).attachment_might_bonus, 2);
}

TEST_F(PlayCombatFixTest, QuickDraw_AtReactionTiming_IsOneActionAndAttaches) {
    makeEngine();
    s().turn.turn_player = P2;
    onBoard(P1, kTVanilla);
    auto chosen = onBoard(P1, kTVanilla);
    a1_.want = chosen;
    auto gear = inHand(P1, kTQuickDraw);
    auto spell = inHand(P2, kTNoopSpell);

    std::optional<Intent> response;
    size_t offers = 0;
    a1_.respond = [&](const std::vector<Intent>& legal) -> const Intent* {
        for (const auto& i : legal)
            if (i.type == IntentType::PlayReaction && i.card == gear) {
                ++offers;
                response = i;
            }
        return response.has_value() ? &*response : nullptr;
    };

    engine_->testHook_executeIntent(playIntent(P2, spell)); settleLikeTheEngine(*engine_);

    EXPECT_EQ(offers, 1u) << "one play, not one identical play per friendly unit";
    const auto& g = s().getObject(gear);
    ASSERT_TRUE(g.attached_to.has_value());
    EXPECT_EQ(*g.attached_to, chosen);
}

}  // namespace
}  // namespace test_review_fix_play_combat
}  // namespace riftbound::test
