// Testing phase, batch 2: combat timing and continuous effects
// (plans/completed/10-testing-phase.md, findings D5, D13, D23, D26, D42, D49,
// D60, D61, D72, D76, B4, G3, G7 and G8).
//
// Every test drives the engine path. The situations are rebuilt from the
// traced games the findings cite (docs/testing-findings-decks-2026-10.md);
// the cited games themselves no longer replay to the same decisions since
// batch 1 changed the flow, so each situation is set up by hand.

#include "cards/card_test_fixture.h"

#include "cards/card_helpers.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test::batch2_combat {

constexpr PlayerId P1 = PlayerId::Player1;
constexpr PlayerId P2 = PlayerId::Player2;

// Real cards.
constexpr CardDefId kForbiddingWaste = 766;   // "While a unit here is defending alone, it has -2 [M]."
constexpr CardDefId kWujuBladesman = 317;     // legend: "While a friendly unit defends alone, it gets +2 [M]."
constexpr CardDefId kTrifarianWarCamp = 289;  // "Units here have +1 [M]."
constexpr CardDefId kAzirSovereign = 497;     // "When I attack, you may move any number of your token units to this battlefield."
constexpr CardDefId kFioraVictorious = 232;   // "While I'm [Mighty], I have [Deflect], [Ganking], and [Shield]."
constexpr CardDefId kVoidGate = 291;          // "Spells and abilities deal 1 Bonus Damage to units here."
constexpr CardDefId kAkaliDeadlyWeapon = 808; // "When I move, you may deal 1 to a unit at a battlefield I moved to or from."
constexpr CardDefId kInferna = 560;           // [Ambush], [Assault 2], 1 [M], [2]
constexpr CardDefId kVexCheerless = 467;      // "While I'm in combat, ... enemy spells cost [1][A] more."
constexpr CardDefId kConfront = 129;          // [Action], [2]: "Units you play this turn enter ready. Draw 1."
constexpr CardDefId kMobilize = 134;          // [2], no timing keyword
constexpr CardDefId kUpFromTheDeep = 887;     // [Flow] [3], no timing keyword
constexpr CardDefId kImperialDecree = 221;    // "When any unit takes damage this turn, kill it."
constexpr CardDefId kGloomist = 785;          // legend: "When you or an ally hold, you may exhaust me to draw 1."
constexpr CardDefId kSivirAmbitious = 441;    // "When I conquer after an attack, if you assigned 5 or more excess damage ..., you may deal that much to an enemy unit."
constexpr CardDefId kYetiBrawler = 580;       // "When I conquer, if you assigned 3 or more excess damage, play two Gold gear tokens exhausted."
constexpr CardDefId kSkulker = 175;           // a plain unit, deck filler
constexpr CardDefId kMechanizedMenace = 551;  // legend: "Your Mechs have [Shield]."
constexpr CardDefId kSivirMercenary = 464;    // "If you've spent at least [A][A] this turn, I have +2 [M] and [Ganking]."

// Test-only cards (ids in the spare range, never in a set's).
constexpr CardDefId kTNoteA = 1960;
constexpr CardDefId kTNoteB = 1961;
constexpr CardDefId kTNoteC = 1962;

// What happened, in order.
static std::vector<std::string>& notes() {
    static std::vector<std::string> l;
    return l;
}

/// A unit whose ability writes its name to notes() as it resolves.
class TNote : public UnitCard {
public:
    TNote(CardDefId id, std::string name, std::vector<TriggerType> triggers)
        : triggers_(std::move(triggers)) {
        def_.id = id;
        def_.name = std::move(name);
        def_.card_type = CardType::Unit;
        def_.might = 2;
        def_.energy_cost = 1;
        def_.domains = {Domain::Fury};
    }
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override { return triggers_; }
    void onTrigger(CardContext&, const std::vector<GameObjectId>&) override {
        notes().push_back(def_.name);
    }
private:
    CardDef def_;
    std::vector<TriggerType> triggers_;
};

/// EnginePathAgent that shows every decision to a callback first and lets the
/// test answer it (`act` returns an Intent to take instead of the default).
class Watcher : public EnginePathAgent {
public:
    using EnginePathAgent::EnginePathAgent;
    std::function<void(const std::vector<Intent>&)> see;
    std::function<std::optional<Intent>(const std::vector<Intent>&)> act;
    int priority = 0, focus = 0, assign = 0;
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        if (isPriority(legal)) ++priority;
        if (isFocus(legal)) ++focus;
        if (isAssign(legal)) ++assign;
        if (see) see(legal);
        if (act)
            if (auto chosen = act(legal)) return *chosen;
        return EnginePathAgent::selectAction(state, legal);
    }
    static bool has(const std::vector<Intent>& legal, IntentType t) {
        return std::any_of(legal.begin(), legal.end(), [t](const Intent& i) { return i.type == t; });
    }
    static bool isPriority(const std::vector<Intent>& legal) { return has(legal, IntentType::PassPriority); }
    static bool isFocus(const std::vector<Intent>& legal) { return has(legal, IntentType::PassFocus); }
    static bool isAssign(const std::vector<Intent>& legal) { return has(legal, IntentType::AssignCombatDamage); }
    /// The option that plays `card` (any play intent), if offered.
    static const Intent* playOf(const std::vector<Intent>& legal, GameObjectId card) {
        for (const auto& i : legal)
            if (i.card == card && (i.type == IntentType::PlayCard || i.type == IntentType::PlayActionCard ||
                                   i.type == IntentType::PlayReaction))
                return &i;
        return nullptr;
    }
};

class Batch2Combat : public CardTestFixture {
protected:
    void SetUp() override {
        CardTestFixture::SetUp();
        using T = TriggerType;
        card_registry.registerCard(kTNoteA, std::make_unique<TNote>(kTNoteA, "A",
            std::vector<T>{T::WhenIHold, T::WhenIConquer, T::WhenIAttack}));
        card_registry.registerCard(kTNoteB, std::make_unique<TNote>(kTNoteB, "B",
            std::vector<T>{T::WhenIHold, T::WhenIConquer}));
        card_registry.registerCard(kTNoteC, std::make_unique<TNote>(kTNoteC, "C",
            std::vector<T>{T::AtStartOfBeginning}));
        card_db.buildFromClasses(card_registry);
        notes().clear();
        // The DAMAGE_STEP line is what the findings quote: keep each one.
        log_conn_ = events.on_log.connect([this](const LogEvent& e) {
            const auto at = e.message.find("DAMAGE_STEP:");
            if (at != std::string::npos) damage_steps.push_back(e.message.substr(at));
        });
    }
    void TearDown() override {
        if (log_conn_.connected()) log_conn_.disconnect();
        CardTestFixture::TearDown();
    }
    boost::signals2::connection log_conn_;
    std::vector<std::string> damage_steps;

    GameObject& o(GameObjectId id) { return state.getObject(id); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }

    GameObjectId named(GameObjectId id, const std::string& name) {
        o(id).name = name;
        return id;
    }
    GameObjectId addLegend(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& l = state.getObject(id);
        l.owner = l.controller = owner;
        l.card_def_id = def_id;
        const auto& def = card_db.get(def_id);
        l.name = def.name;
        l.card_type = def.card_type;
        l.domains = def.domains;
        l.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }
    GameObjectId installBattlefield(int bf, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def_id;
        obj.card_type = CardType::Battlefield;
        obj.name = card_db.get(def_id).name;
        state.battlefields[bf].card_object_id = id;
        return id;
    }
    /// A combat is staged at `bf`: `attacker`'s units applied Contested to a
    /// battlefield the other player controls.
    void stageCombat(int bf, PlayerId attacker) {
        auto& b = state.battlefields[bf];
        b.controller = opponent(attacker);
        b.is_contested = true;
        b.contested_by = attacker;
        b.combat_staged = true;
    }
    /// Run the combat staged at `bf` on a real engine. `live` points at the
    /// engine while it runs, for a Watcher's callbacks.
    void runCombat(int bf, PlayerId attacker, AgentInterface* p1 = nullptr,
                   AgentInterface* p2 = nullptr) {
        withEngine([&](GameEngine& e) {
            live = &e;
            e.testHook_runCombat(BattlefieldId::fromRaw(bf));
            live = nullptr;
        }, p1, p2, attacker);
    }
    GameEngine* live = nullptr;
    const GameObject& liveObject(GameObjectId id) const { return live->state().getObject(id); }
};

// ═══ B9: D60, D76, D23 — closed by batch 1; pinned ══════════════════════════
//
// Batch 1 made runCombat settle the chain's loop, and with it a Cleanup,
// after the designations are given and before the first Focus decision. These
// are the situations in which the continuous effect used to be missing:
// nothing resolves between "COMBAT:" and "DAMAGE_STEP:".

// kaisa_sb__reksai_sb__s20262218 D:232-234: `COMBAT: BF#1` / `DAMAGE_STEP:
// ... DEF=[Ferrous Forerunner(6M)]` — a lone defender at Forbidding Waste
// with its printed Might. "While a unit here is defending alone, it has -2
// [M]."
TEST_F(Batch2Combat, D60_ForbiddingWaste_TheLoneDefenderHasMinus2WhenNothingResolvesBeforeTheDamageStep) {
    installBattlefield(0, kForbiddingWaste);
    auto attacker = named(addUnit(P1, kInvalidId, 5, 0), "Attacker");
    auto defender = named(addUnit(P2, kInvalidId, 6, 0), "Defender");
    stageCombat(0, P1);
    Watcher p1, p2;
    runCombat(0, P1, &p1, &p2);
    ASSERT_EQ(damage_steps.size(), 1u);
    EXPECT_NE(damage_steps[0].find("DEF=[Defender(4M)]"), std::string::npos) << damage_steps[0];
    // It deals 4, not 6: the 5 [M] attacker survives; 5 damage kills a 4 [M] defender.
    EXPECT_TRUE(onBoard(attacker));
    EXPECT_FALSE(onBoard(defender));
    EXPECT_EQ(state.battlefields[0].controller, std::optional<PlayerId>(P1));
    EXPECT_EQ(p1.priority + p2.priority, 0) << "nothing was on the chain in this combat";
}

// akali__yi_rebuild__s20261116 D:100: `DAMAGE_STEP: ... DEF=[Master Yi,
// Tempered(4M)]`, the only defender, and it is killed by 5 damage. "While a
// friendly unit defends alone, it gets +2 [M]."
TEST_F(Batch2Combat, D76_WujuBladesman_TheLoneDefenderHasPlus2WhenNothingResolvesBeforeTheDamageStep) {
    addLegend(P2, kWujuBladesman);
    auto attacker = named(addUnit(P1, kInvalidId, 5, 0), "Attacker");
    auto defender = named(addUnit(P2, kInvalidId, 4, 0), "Defender");
    stageCombat(0, P1);
    Watcher p1, p2;
    runCombat(0, P1, &p1, &p2);
    ASSERT_EQ(damage_steps.size(), 1u);
    EXPECT_NE(damage_steps[0].find("DEF=[Defender(6M)]"), std::string::npos) << damage_steps[0];
    // 6 damage kills the 5 [M] attacker; 5 damage does not kill a 6 [M] defender.
    EXPECT_FALSE(onBoard(attacker));
    EXPECT_TRUE(onBoard(defender));
    EXPECT_EQ(state.battlefields[0].controller, std::optional<PlayerId>(P2));
    EXPECT_EQ(p1.priority + p2.priority, 0);
}

// The +2 is gone with the combat (D76, "also seen" b: "the same lag the
// other way").
TEST_F(Batch2Combat, D76_WujuBladesman_ThePlus2EndsWithTheCombat) {
    addLegend(P2, kWujuBladesman);
    auto attacker = addUnit(P1, kInvalidId, 2, 0);
    auto defender = addUnit(P2, kInvalidId, 4, 0);
    stageCombat(0, P1);
    withEngine([&](GameEngine& e) {
        e.testHook_runCombat(BattlefieldId::fromRaw(0));
        settleLikeTheEngine(e);
    });
    EXPECT_FALSE(onBoard(attacker));
    ASSERT_TRUE(onBoard(defender));
    EXPECT_EQ(o(defender).current_might, 4);
}

// azir__azir__s20261119 D:362: `AZIR SOVEREIGN: attack -> moved 1 token
// unit(s) to this battlefield` / `DAMAGE_STEP: BF#1 ATK=[Recruit(2M), Azir,
// Sovereign(11M), Sand Soldier(2M)]` — the token that was just moved to
// Trifarian War Camp ("Units here have +1 [M]. (This includes attackers.)")
// fights without the +1.
TEST_F(Batch2Combat, D23_TrifarianWarCamp_ATokenMovedInByAzirsAttackAbilityHasThePlus1AtTheDamageStep) {
    installBattlefield(0, kTrifarianWarCamp);
    auto azir = addUnit(P1, kAzirSovereign, 0, 0);
    auto token = named(addUnit(P1, kInvalidId, 2, -1), "Sand Soldier");
    o(token).is_token = true;
    named(addUnit(P2, kInvalidId, 30, 0), "Wall");
    stageCombat(0, P1);
    // Azir's "you may" (batch 8 U2: asked first), then the token.
    Watcher p1(EnginePathAgent::choosing(token, EnginePathAgent::May::Yes)), p2;
    runCombat(0, P1, &p1, &p2);
    ASSERT_EQ(damage_steps.size(), 1u);
    EXPECT_NE(damage_steps[0].find("Sand Soldier(3M)"), std::string::npos) << damage_steps[0];
    const int azir_might = card_db.get(kAzirSovereign).might + 1;
    EXPECT_NE(damage_steps[0].find("Azir, Sovereign(" + std::to_string(azir_might) + "M)"),
              std::string::npos) << damage_steps[0];
    (void)azir;
}

// ═══ D42, D49, D61, D26: the same root, each on its own card ════════════════

// diana_sb__fiora_sb__s20262140 D:252-258: Fiora, Victorious defends with 6
// [M] (so she is Mighty and has [Shield]) and every board of the combat's
// showdown shows 6M. [Shield] is +1 [M] while she is a defender (CR 814).
TEST_F(Batch2Combat, D42_FioraVictorious_HerShieldCountsFromTheFirstDecisionOfTheCombat) {
    auto fiora = addUnit(P2, kFioraVictorious, 6, 0);
    named(addUnit(P1, kInvalidId, 3, 0), "Attacker");
    stageCombat(0, P1);
    std::vector<int> seen;
    Watcher p1, p2;
    auto look = [&](const std::vector<Intent>& legal) {
        if (Watcher::isFocus(legal)) seen.push_back(liveObject(fiora).current_might);
    };
    p1.see = look;
    p2.see = look;
    runCombat(0, P1, &p1, &p2);
    ASSERT_EQ(seen.size(), 2u) << "both players are given Focus once";
    EXPECT_EQ(seen[0], 7);
    EXPECT_EQ(seen[1], 7);
    ASSERT_EQ(damage_steps.size(), 1u);
    EXPECT_NE(damage_steps[0].find("Fiora, Victorious(7M)"), std::string::npos) << damage_steps[0];
}

// akali__ezreal__s20261103 D:116: Akali, Deadly Weapon moves to Void Gate and
// her "When I move" ability deals 1 to her there — `EFFECT: deal 1 damage`,
// no Bonus Damage. "Spells and abilities deal 1 Bonus Damage to units here."
TEST_F(Batch2Combat, D49_VoidGate_BonusDamageForAUnitThatMovedThereWithTheMoveThatTriggeredTheAbility) {
    installBattlefield(1, kVoidGate);
    auto akali = addUnit(P1, kAkaliDeadlyWeapon, 6, -1);
    EnginePathAgent p1(EnginePathAgent::choosing(akali, EnginePathAgent::May::Yes));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.moveToBattlefield(akali, BattlefieldId::fromRaw(1));
    }, &p1);
    ASSERT_TRUE(onBoard(akali));
    EXPECT_EQ(o(akali).damage_marked, 2) << "1 from her ability and 1 Bonus Damage from Void Gate";
}

// rengar__vex__s20261311 D:287-301: Inferna ([Ambush], [Assault 2]) is played
// as a Reaction, with a chain open, to the battlefield her side is attacking
// and is listed `ATK=[Inferna(1M), ...]`.
TEST_F(Batch2Combat, D61_Inferna_AmbushedIntoHerSidesAttackInAClosedStateHasHerAssaultAtTheDamageStep) {
    auto first = addUnit(P1, kTNoteA, 3, 0);   // "When I attack": a chain at the combat's start
    named(addUnit(P2, kInvalidId, 30, 0), "Wall");
    auto inferna = addToHand(P1, kInferna);
    state.player(P1).rune_pool.energy = 2;
    stageCombat(0, P1);
    Watcher p1, p2;
    bool played = false;
    p1.act = [&](const std::vector<Intent>& legal) -> std::optional<Intent> {
        if (played || !Watcher::isPriority(legal)) return std::nullopt;
        for (const auto& i : legal)
            if (i.type == IntentType::PlayReaction && i.card == inferna) { played = true; return i; }
        return std::nullopt;
    };
    runCombat(0, P1, &p1, &p2);
    ASSERT_TRUE(played) << "Inferna was not offered as a Reaction while her controller's attack ability was on the chain";
    ASSERT_TRUE(onBoard(inferna) || !damage_steps.empty());
    ASSERT_EQ(damage_steps.size(), 1u);
    EXPECT_NE(damage_steps[0].find("Inferna(3M)"), std::string::npos) << damage_steps[0];
    EXPECT_EQ(notes(), std::vector<std::string>{"A"});
    (void)first;
}

// vex_sb__irelia_sb__s20262339 D:293-296: Vex, Cheerless attacks and the
// defender plays a spell for its printed cost. "While I'm in combat, ...
// enemy spells cost [1][A] more."
TEST_F(Batch2Combat, D26_VexCheerless_AnEnemySpellIsTaxedFromTheFirstDecisionOfHerCombat) {
    addUnit(P2, kVexCheerless, 0, 0);
    named(addUnit(P1, kInvalidId, 30, 0), "Wall");
    auto confront = addToHand(P1, kConfront);          // [Action], [2]
    state.player(P1).rune_pool.energy = 2;             // its printed cost and no more
    stageCombat(0, P2);
    Watcher p1, p2;
    int p1_focus = 0;
    bool offered = false, affordable = true;
    p1.see = [&](const std::vector<Intent>& legal) {
        if (!Watcher::isFocus(legal)) return;
        ++p1_focus;
        offered = offered || Watcher::playOf(legal, confront) != nullptr;
        affordable = live->testHook_canAfford(P1, confront);
    };
    runCombat(0, P2, &p1, &p2);
    ASSERT_GE(p1_focus, 1);
    EXPECT_FALSE(affordable) << "[2] does not pay for a spell that costs [3][A] while Vex is in combat";
    EXPECT_FALSE(offered);
}

TEST_F(Batch2Combat, D26_VexCheerless_TheTaxedSpellIsOfferedWhenTheTaxCanBePaid) {
    addUnit(P2, kVexCheerless, 0, 0);
    named(addUnit(P1, kInvalidId, 30, 0), "Wall");
    auto confront = addToHand(P1, kConfront);
    state.player(P1).rune_pool.energy = 3;
    state.player(P1).rune_pool.universal_power = 1;
    stageCombat(0, P2);
    Watcher p1, p2;
    bool offered = false;
    p1.see = [&](const std::vector<Intent>& legal) {
        if (Watcher::isFocus(legal)) offered = offered || Watcher::playOf(legal, confront) != nullptr;
    };
    runCombat(0, P2, &p1, &p2);
    EXPECT_TRUE(offered);
}

// ═══ G8, G7: continuous effects at the first decision ═══════════════════════

// games19 g09 D:125: `COMBAT: BF#1 attacker=P2 defender=P1` / `DAMAGE_STEP:
// ... DEF=[Forecaster(2M), Rumble, Hotheaded(4M)]` — Mechanized Menace: "Your
// Mechs have [Shield]. (+1 [M] while they're defenders.)" — nothing resolved
// between the two lines.
TEST_F(Batch2Combat, G8_MechanizedMenace_ItsMechsHaveShieldWhenNothingResolvesBeforeTheDamageStep) {
    addLegend(P1, kMechanizedMenace);
    auto mech_a = named(addUnit(P1, kInvalidId, 2, 0), "Mech A");
    auto mech_b = named(addUnit(P1, kInvalidId, 4, 0), "Mech B");
    for (auto id : {mech_a, mech_b}) o(id).tags = std::vector<std::string>{"Mech"};
    named(addUnit(P2, kInvalidId, 30, 0), "Wall");
    stageCombat(0, P2);
    std::vector<int> seen;
    Watcher p1, p2;
    auto look = [&](const std::vector<Intent>& legal) {
        if (Watcher::isFocus(legal))
            seen.push_back(liveObject(mech_a).current_might + liveObject(mech_b).current_might);
    };
    p1.see = look;
    p2.see = look;
    runCombat(0, P2, &p1, &p2);
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], 8) << "3 + 5 from the first Focus decision";
    ASSERT_EQ(damage_steps.size(), 1u);
    EXPECT_NE(damage_steps[0].find("Mech A(3M)"), std::string::npos) << damage_steps[0];
    EXPECT_NE(damage_steps[0].find("Mech B(5M)"), std::string::npos) << damage_steps[0];
}

// games19 g12 D:169-171: Sivir, Mercenary is 8M ("If you've spent at least
// [A][A] this turn, I have +2 [M] and [Ganking]") at both priority decisions
// of the NEXT turn's Beginning Step, in which nothing has been spent.
TEST_F(Batch2Combat, G7_SivirMercenary_TheTurnScopedBonusIsGoneAtTheFirstDecisionOfTheNextTurn) {
    auto sivir = addUnit(P1, kSivirMercenary, 0, 0);
    auto watcher = addUnit(P2, kTNoteC, 0, -1);   // "At the start of your Beginning Phase"
    state.player(P1).power_spent_this_turn = 2;
    std::vector<int> might_at_priority;
    std::vector<bool> ganking_at_priority;
    Watcher p1, p2;
    auto look = [&](const std::vector<Intent>& legal) {
        if (!Watcher::isPriority(legal)) return;
        might_at_priority.push_back(liveObject(sivir).current_might);
        ganking_at_priority.push_back(liveObject(sivir).hasKeyword(Keyword::Ganking));
    };
    p1.see = look;
    p2.see = look;
    int might_in_p1s_turn = 0;
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_cleanup();
        might_in_p1s_turn = e.state().getObject(sivir).current_might;
        // The turn changes as GameEngine::runTurn changes it.
        auto& st = e.mutableState();
        st.turn.turn_player = P2;
        ++st.turn.turn_number;
        st.player(P1).resetTurnTracking();
        st.player(P2).resetTurnTracking();
        e.testHook_awakenPhase();
        e.testHook_beginningStep();
        live = nullptr;
    }, &p1, &p2, P1);
    EXPECT_EQ(might_in_p1s_turn, card_db.get(kSivirMercenary).might + 2) << "the control";
    ASSERT_FALSE(might_at_priority.empty()) << "the Beginning Phase ability gave both players priority";
    for (int m : might_at_priority) EXPECT_EQ(m, card_db.get(kSivirMercenary).might);
    for (bool g : ganking_at_priority) EXPECT_FALSE(g);
    EXPECT_EQ(notes(), std::vector<std::string>{"C"});
    (void)watcher;
}

// ═══ G3: who gains Focus first ═════════════════════════════════════════════
//
// CR 466.2.c.1: "The Attacker is the player whose unit(s) applied the
// Contested status to the Battlefield." 466.2.c.1.a: "If a showdown opens as
// part of combat, this player gains Focus as the showdown begins." CR 345: "As
// a Showdown begins, the player who applied Contested status to the
// Battlefield gains Focus."

// games19 g19: on P2's turn P2's Charm moves P1's Teemo, Scout to Dusk Rose
// Lab, which P2 holds: `COMBAT: BF#0 attacker=P1 defender=P2`, and P2 is asked
// for Focus first.
TEST_F(Batch2Combat, G3_InACombatTheAttackerHasFocusFirst_AlsoOnTheOtherPlayersTurn) {
    named(addUnit(P1, kInvalidId, 30, 0), "Holder");
    named(addUnit(P2, kInvalidId, 1, 0), "Moved In");
    stageCombat(0, P2);
    std::vector<PlayerId> focus;
    Watcher p1, p2;
    p1.see = [&](const std::vector<Intent>& legal) { if (Watcher::isFocus(legal)) focus.push_back(P1); };
    p2.see = [&](const std::vector<Intent>& legal) { if (Watcher::isFocus(legal)) focus.push_back(P2); };
    // On P1's turn: P2 is the attacker.
    withEngine([&](GameEngine& e) { e.testHook_runCombat(BattlefieldId::fromRaw(0)); }, &p1, &p2, P1);
    ASSERT_EQ(state.turn.turn_player, P1);
    EXPECT_EQ(focus, (std::vector<PlayerId>{P2, P1}));
}

TEST_F(Batch2Combat, G3_InACombatOnItsTurnTheTurnPlayerAttacksAndHasFocusFirst) {
    named(addUnit(P1, kInvalidId, 1, 0), "Attacker");
    named(addUnit(P2, kInvalidId, 30, 0), "Holder");
    stageCombat(0, P1);
    std::vector<PlayerId> focus;
    Watcher p1, p2;
    p1.see = [&](const std::vector<Intent>& legal) { if (Watcher::isFocus(legal)) focus.push_back(P1); };
    p2.see = [&](const std::vector<Intent>& legal) { if (Watcher::isFocus(legal)) focus.push_back(P2); };
    runCombat(0, P1, &p1, &p2);
    EXPECT_EQ(focus, (std::vector<PlayerId>{P1, P2}));
}

// games19 g10 / g14 / g07: on the turn player's turn an effect puts the
// OTHER player's unit alone on a battlefield nobody controls: a non-combat
// showdown opens, Contested applied by that unit (CR 190.3.a.1). The engine
// gives Focus to that unit's controller first — the reading of CR 345 that
// judge question J2 asks about (the alternative is the turn player, whose
// spell caused the move). Pinned.
TEST_F(Batch2Combat, G3_NonCombatShowdown_TheControllerOfTheUnitThatAppliedContestedHasFocusFirst_J2) {
    named(addUnit(P2, kInvalidId, 2, 0), "Moved In");
    state.battlefields[0].is_contested = true;
    state.battlefields[0].contested_by = P2;
    state.battlefields[0].showdown_staged = true;
    std::vector<PlayerId> focus;
    Watcher p1, p2;
    p1.see = [&](const std::vector<Intent>& legal) { if (Watcher::isFocus(legal)) focus.push_back(P1); };
    p2.see = [&](const std::vector<Intent>& legal) { if (Watcher::isFocus(legal)) focus.push_back(P2); };
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); }, &p1, &p2, P1);
    EXPECT_EQ(focus, (std::vector<PlayerId>{P2, P1}));
    EXPECT_EQ(state.battlefields[0].controller, std::optional<PlayerId>(P2)) << "P2 conquered it";
}

// ═══ D5: a combat is a Showdown State ═══════════════════════════════════════
//
// CR 308.1: "If a Showdown or Combat is in progress, the turn is in a
// Showdown State." 308.1.a: "Only cards and abilities with the Action or
// Reaction keywords can be played or activated in a Showdown State."
// akali_sb__ezreal_sb__s20262103 D:136 (P1's turn, combat): `INTENT: PlayCard
// by P2` / `SPELL: Singularity` — a spell with neither keyword, played by the
// defender on the opponent's turn.

TEST_F(Batch2Combat, D5_AtFocusInACombat_ASpellWithoutActionOrReactionIsNotOffered_ToEitherPlayer) {
    named(addUnit(P1, kInvalidId, 3, 0), "Attacker");
    named(addUnit(P2, kInvalidId, 30, 0), "Wall");
    auto p1_plain = addToHand(P1, kMobilize);
    auto p1_action = addToHand(P1, kConfront);
    auto p2_plain = addToHand(P2, kMobilize);
    auto p2_action = addToHand(P2, kConfront);
    for (PlayerId p : {P1, P2}) state.player(p).rune_pool.energy = 10;
    stageCombat(0, P1);
    Watcher p1, p2;
    std::vector<NeutralShowdownState> states;
    bool plain_offered = false;
    int action_offered = 0;
    auto look = [&](GameObjectId plain, GameObjectId action) {
        return [&, plain, action](const std::vector<Intent>& legal) {
            if (!Watcher::isFocus(legal)) return;
            states.push_back(live->state().turn.ns_state);
            plain_offered = plain_offered || Watcher::playOf(legal, plain) != nullptr;
            if (const Intent* i = Watcher::playOf(legal, action)) {
                ++action_offered;
                EXPECT_EQ(i->type, IntentType::PlayActionCard);
            }
        };
    };
    p1.see = look(p1_plain, p1_action);
    p2.see = look(p2_plain, p2_action);
    runCombat(0, P1, &p1, &p2);
    ASSERT_EQ(states.size(), 2u) << "attacker, then defender, are given Focus";
    EXPECT_EQ(states[0], NeutralShowdownState::Showdown);
    EXPECT_EQ(states[1], NeutralShowdownState::Showdown);
    EXPECT_FALSE(plain_offered) << "Mobilize has neither [Action] nor [Reaction]";
    EXPECT_EQ(action_offered, 2) << "an [Action] spell is offered to both players";
    EXPECT_EQ(state.turn.ns_state, NeutralShowdownState::Neutral) << "the combat is over";
}

// CR 829.1.b.2: "Playing a spell for its Flow cost does not change the timing
// at which it can be played." akali__khazix__s20263031 D:158: `SPELL: Up from
// the Deep` / `TRASH_REPLAY` by the defender in a combat.
TEST_F(Batch2Combat, D5_AtFocusInACombat_ASpellWithoutActionIsNotOfferedForItsFlowCost) {
    named(addUnit(P1, kInvalidId, 3, 0), "Attacker");
    named(addUnit(P2, kInvalidId, 30, 0), "Wall");
    // In P1's trash, as after it was played.
    auto flow = addToHand(P1, kUpFromTheDeep);
    state.player(P1).hand.clear();
    o(flow).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(flow);
    state.player(P1).rune_pool.energy = 10;
    state.player(P1).rune_pool.universal_power = 5;

    // The control: in a Neutral Open State on P1's turn it is offered.
    bool offered_in_main = false;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();   // the standing Flow permission is rebuilt by the aura pass
        for (const auto& i : e.generateLegalActions())
            if (i.card == flow) offered_in_main = true;
    });
    ASSERT_TRUE(offered_in_main) << "the control: Flow replay is offered in the Main Phase";

    stageCombat(0, P1);
    Watcher p1, p2;
    bool offered = false;
    int p1_focus = 0;
    p1.see = [&](const std::vector<Intent>& legal) {
        if (!Watcher::isFocus(legal)) return;
        ++p1_focus;
        for (const auto& i : legal)
            if (i.card == flow) offered = true;
    };
    runCombat(0, P1, &p1, &p2);
    ASSERT_GE(p1_focus, 1);
    EXPECT_FALSE(offered);
}

// A spell with neither keyword played at Focus used to resolve in the combat;
// here the defender is the non-turn player and holds only that spell: the
// combat has no play at all.
TEST_F(Batch2Combat, D5_TheDefenderOnTheOpponentsTurnCannotPlayAPlainSpellInTheCombat) {
    named(addUnit(P1, kInvalidId, 3, 0), "Attacker");
    named(addUnit(P2, kInvalidId, 30, 0), "Wall");
    auto plain = addToHand(P2, kMobilize);
    state.player(P2).rune_pool.energy = 10;
    stageCombat(0, P1);
    Watcher p1, p2;
    p2.act = [&](const std::vector<Intent>& legal) -> std::optional<Intent> {
        if (const Intent* i = Watcher::playOf(legal, plain)) return *i;   // play it if it is ever offered
        return std::nullopt;
    };
    runCombat(0, P1, &p1, &p2);
    EXPECT_TRUE(inHand(P2, plain));
    EXPECT_EQ(state.player(P2).rune_pool.energy, 10);
}

// CR 323.5: "Players lose control of any controlled Battlefields without
// their Units occupying them if the turn is in an Open State and there is no
// Showdown or Combat ongoing there." An Open State — Neutral or Showdown.
// The combat case is pinned so that making the combat a Showdown State does
// not change it; the non-combat showdown case was not handled.
TEST_F(Batch2Combat, D5_ControlOfABattlefieldWithoutUnitsIsLostInTheOpenStateOfACombatElsewhere) {
    named(addUnit(P1, kInvalidId, 3, 0), "Attacker");
    named(addUnit(P2, kInvalidId, 30, 0), "Wall");
    auto lone = addUnit(P2, kInvalidId, 1, 1);
    state.battlefields[1].controller = P2;
    auto confront = addToHand(P1, kConfront);
    state.player(P1).rune_pool.energy = 2;
    addToDeck(P1, kSkulker);
    stageCombat(0, P1);
    Watcher p1, p2;
    std::vector<std::optional<PlayerId>> controller_at_p2_focus;
    p1.act = [&](const std::vector<Intent>& legal) -> std::optional<Intent> {
        if (!Watcher::isFocus(legal)) return std::nullopt;
        const Intent* play = Watcher::playOf(legal, confront);
        if (!play) return std::nullopt;
        // P2's unit at the other battlefield dies while the spell is played.
        live->testHook_executor().killObject(lone);
        return *play;
    };
    p2.see = [&](const std::vector<Intent>& legal) {
        if (Watcher::isFocus(legal))
            controller_at_p2_focus.push_back(live->state().battlefields[1].controller);
    };
    runCombat(0, P1, &p1, &p2);
    ASSERT_FALSE(controller_at_p2_focus.empty());
    EXPECT_EQ(controller_at_p2_focus.front(), std::nullopt);
}

TEST_F(Batch2Combat, D5_ControlOfABattlefieldWithoutUnitsIsLostInTheOpenStateOfAShowdownElsewhere) {
    // A non-combat showdown at battlefield 0: only P1's unit is there.
    named(addUnit(P1, kInvalidId, 3, 0), "Contester");
    state.battlefields[0].is_contested = true;
    state.battlefields[0].contested_by = P1;
    state.battlefields[0].showdown_staged = true;
    auto lone = addUnit(P2, kInvalidId, 1, 1);
    state.battlefields[1].controller = P2;
    auto confront = addToHand(P1, kConfront);
    state.player(P1).rune_pool.energy = 2;
    addToDeck(P1, kSkulker);
    Watcher p1, p2;
    std::vector<std::optional<PlayerId>> controller_at_p2_focus;
    p1.act = [&](const std::vector<Intent>& legal) -> std::optional<Intent> {
        if (!Watcher::isFocus(legal)) return std::nullopt;
        const Intent* play = Watcher::playOf(legal, confront);
        if (!play) return std::nullopt;
        live->testHook_executor().killObject(lone);
        return *play;
    };
    p2.see = [&](const std::vector<Intent>& legal) {
        if (Watcher::isFocus(legal))
            controller_at_p2_focus.push_back(live->state().battlefields[1].controller);
    };
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_advanceMainPhase();
        live = nullptr;
    }, &p1, &p2, P1);
    ASSERT_FALSE(controller_at_p2_focus.empty());
    EXPECT_EQ(controller_at_p2_focus.front(), std::nullopt)
        << "Showdown Open is an Open State, and no showdown is going on at battlefield 1";
}

// ═══ D13: combat damage is assigned by both players, then dealt at once ═════
//
// CR 467.2.c: "Starting with the Attacker, each player assigns an amount of
// damage ..." 467.2.c.1: "Assigning Damage is not Dealing Damage."
// 467.2.c.1.a: "When all Damage is assigned, it will be Dealt simultaneously."

TEST_F(Batch2Combat, D13_TheDefenderAssignsBeforeTheAttackersDamageIsDealt) {
    auto attacker = named(addUnit(P1, kInvalidId, 3, 0), "Attacker");
    auto defender = named(addUnit(P2, kInvalidId, 4, 0), "Defender");
    stageCombat(0, P1);
    Watcher p1, p2;
    std::vector<std::string> order;
    int defender_damage_when_p2_assigns = -1;
    p1.see = [&](const std::vector<Intent>& legal) {
        if (Watcher::isAssign(legal)) order.push_back("P1");
    };
    p2.see = [&](const std::vector<Intent>& legal) {
        if (!Watcher::isAssign(legal)) return;
        order.push_back("P2");
        defender_damage_when_p2_assigns = liveObject(defender).damage_marked;
    };
    runCombat(0, P1, &p1, &p2);
    EXPECT_EQ(order, (std::vector<std::string>{"P1", "P2"})) << "the attacker assigns first";
    EXPECT_EQ(defender_damage_when_p2_assigns, 0) << "assigned, not yet dealt";
    // Then all of it was dealt: 4 kills the attacker, 3 does not kill the defender.
    EXPECT_FALSE(onBoard(attacker));
    EXPECT_TRUE(onBoard(defender));
}

// akali__viktor__s20261114 D:347: with Imperial Decree in effect, `-> Astral
// Heron takes 3` / `DELAYED_ABILITY: firing for Imperial Decree` and only
// then is the other player asked to assign, with that ability on the chain.
TEST_F(Batch2Combat, D13_AbilitiesThatTriggerOnTheDamageArePendingOnlyAfterBothPlayersAssigned) {
    auto attacker = named(addUnit(P1, kInvalidId, 3, 0), "Attacker");
    auto defender = named(addUnit(P2, kInvalidId, 9, 0), "Defender");
    // "When any unit takes damage this turn, kill it." — as Imperial Decree leaves it.
    auto decree = addToHand(P2, kImperialDecree);
    state.player(P2).hand.clear();
    o(decree).zone = ZoneType::Trash;
    state.player(P2).trash.push_back(decree);
    DelayedAbility da;
    da.source = decree;
    da.card_def_id = kImperialDecree;
    da.controller = P2;
    da.trigger = TriggerType::WhenAUnitTakesDamage;
    da.expires_on_turn = state.turn.turn_number;
    da.repeats = true;
    state.delayed_abilities.push_back(da);
    stageCombat(0, P1);
    Watcher p1, p2;
    int chain_when_p2_assigns = -1;
    p2.see = [&](const std::vector<Intent>& legal) {
        if (Watcher::isAssign(legal))
            chain_when_p2_assigns = static_cast<int>(live->state().chain.items.size());
    };
    runCombat(0, P1, &p1, &p2);
    EXPECT_EQ(chain_when_p2_assigns, 0) << "no damage has been dealt, so nothing has triggered";
    // Both took damage, so both are killed by the Decree's abilities.
    EXPECT_FALSE(onBoard(attacker));
    EXPECT_FALSE(onBoard(defender));
}

// ═══ D72: the turn player Holds all their battlefields as one task ══════════
//
// CR 315.2.b.2: "The Turn Player Holds all Battlefields they Control."
// kaisa__vex__s20263055 D:327-330: `SCORE: P2 +1 via Hold BF#0` / Gloomist's
// ability is finalized, both players pass, it resolves / `SCORE: P2 +1 via
// Hold BF#1`.

TEST_F(Batch2Combat, D72_BothBattlefieldsAreHeldBeforeAnyHoldAbilityIsGivenPriority) {
    auto legend = addLegend(P1, kGloomist);
    for (int bf : {0, 1}) {
        state.battlefields[bf].controller = P1;
        addUnit(P1, kInvalidId, 2, bf);
    }
    for (int i = 0; i < 3; ++i) addToDeck(P1, kSkulker);
    Watcher p1(EnginePathAgent::accepting()), p2;
    std::vector<int> score_at_priority;
    auto look = [&](const std::vector<Intent>& legal) {
        if (Watcher::isPriority(legal)) score_at_priority.push_back(live->state().player(P1).score);
    };
    p1.see = look;
    p2.see = look;
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_scoringStep();
        live = nullptr;
    }, &p1, &p2, P1);
    ASSERT_FALSE(score_at_priority.empty()) << "Gloomist's ability was accepted: a priority round";
    EXPECT_EQ(score_at_priority.front(), 2) << "both holds are done before anything is finalized";
    EXPECT_EQ(state.player(P1).score, 2);
    // One draw: she is exhausted for the first ability, so the second "may"
    // is not offered — the abilities of both holds were pending together.
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_TRUE(o(legend).is_exhausted);
}

// CR 383.3.d: abilities that trigger at the same time are put on the chain in
// an order their controller chooses. The hold abilities of two battlefields
// trigger together, so their controller is asked.
TEST_F(Batch2Combat, D72_TheHoldAbilitiesOfTwoBattlefieldsAreOrderedByTheirController) {
    for (int bf : {0, 1}) state.battlefields[bf].controller = P1;
    addUnit(P1, kTNoteA, 0, 0);
    addUnit(P1, kTNoteB, 0, 1);
    Watcher p1, p2;
    withEngine([&](GameEngine& e) { e.testHook_scoringStep(); }, &p1, &p2, P1);
    EXPECT_EQ(p1.order_prompts.size(), 1u) << "two different abilities of one player, one batch";
    // First option of the order prompt: the order they were queued in, A
    // lowest — B resolves first.
    EXPECT_EQ(notes(), (std::vector<std::string>{"B", "A"}));
    EXPECT_EQ(state.player(P1).score, 2);
}

TEST_F(Batch2Combat, D72_AHoldThatWinsTheGameEndsTheStep) {
    for (int bf : {0, 1}) {
        state.battlefields[bf].controller = P1;
        addUnit(P1, kInvalidId, 2, bf);
    }
    setScore(P1, state.effectiveVictoryScore() - 1);
    withEngine([&](GameEngine& e) { e.testHook_scoringStep(); });
    EXPECT_TRUE(state.game_over);
    EXPECT_EQ(state.winner, P1);
    EXPECT_EQ(state.player(P1).score, state.effectiveVictoryScore())
        << "the game is over at the winning point; the second hold is not scored";
}

// ═══ B4: a combat begins only with nothing pending ══════════════════════════
//
// CR 323.12: "If the current state is a Neutral Open State and one or more
// Combats are Staged at Battlefields, the Turn Player chooses one of those
// Battlefields. Combat begins there." (310.1: Neutral Open — "no Chain
// exists".) CR 468.6: "Resolve any items on the chain from establishing
// control and associated FEPR before performing this step" (the end of the
// combat). With two combats staged, the second used to begin with the first
// one's conquer abilities still pending, and they resolved inside it.

TEST_F(Batch2Combat, B4_TheConquerAbilitiesOfOneCombatResolveBeforeTheNextCombatBegins) {
    addUnit(P1, kTNoteA, 5, 0);                 // "When I conquer"
    addUnit(P2, kInvalidId, 1, 0);
    addUnit(P1, kInvalidId, 5, 1);
    addUnit(P2, kInvalidId, 1, 1);
    stageCombat(0, P1);
    stageCombat(1, P1);
    auto conn = events.on_combat_started.connect([](const CombatStartedEvent& e) {
        notes().push_back("combat@" + toString(e.battlefield));
    });
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); });
    conn.disconnect();
    // "A" twice: once for "When I attack", once for "When I conquer".
    EXPECT_EQ(notes(), (std::vector<std::string>{"combat@0", "A", "A", "combat@1"}));
}

// Sivir, Ambitious: "... you may deal that much to an enemy unit" reads the
// excess damage of her combat as her ability resolves. With a second combat
// staged, that combat's start had already reset the number to 0.
TEST_F(Batch2Combat, B4_SivirAmbitious_DealsTheExcessOfHerOwnCombatWhenAnotherCombatIsStaged) {
    addUnit(P1, kSivirAmbitious, 0, 0);          // 7 [M]
    addUnit(P2, kInvalidId, 1, 0);               // 7 assigned to a 1 [M] unit: 6 excess
    addUnit(P1, kInvalidId, 5, 1);
    addUnit(P2, kInvalidId, 1, 1);
    auto victim = named(addUnit(P2, kInvalidId, 9, -1), "Victim");
    stageCombat(0, P1);
    stageCombat(1, P1);
    EnginePathAgent p1(EnginePathAgent::choosing(victim, EnginePathAgent::May::Yes));
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); }, &p1);
    EXPECT_EQ(o(victim).damage_marked, 6);
}

// Yeti Brawler's condition is part of its trigger condition (CR 383.2.a.1)
// and is judged as the conquer happens, in the combat that led to it: a
// second staged combat never changed it. Pinned.
TEST_F(Batch2Combat, B4_YetiBrawler_ItsConditionIsJudgedAtTheConquer_AnotherStagedCombatDoesNotChangeIt) {
    addUnit(P1, kYetiBrawler, 0, 0);             // 6 [M]
    addUnit(P2, kInvalidId, 1, 0);               // 5 excess
    addUnit(P1, kInvalidId, 5, 1);
    addUnit(P2, kInvalidId, 4, 1);               // 1 excess in the second combat
    stageCombat(0, P1);
    stageCombat(1, P1);
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); });
    int gold = 0;
    for (auto& [id, obj] : state.objects)
        if (obj.isGear() && obj.isToken() && obj.location.has_value() && obj.controller == P1) ++gold;
    EXPECT_EQ(gold, 2);
}

}  // namespace riftbound::test::batch2_combat
