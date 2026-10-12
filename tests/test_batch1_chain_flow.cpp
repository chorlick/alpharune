// Testing phase, batch 1: where and when triggered abilities are queued,
// ordered, finalized and resolved, and the Cleanup between the items of a
// chain (plans/completed/10-testing-phase.md, findings F1, D1, D4, D6, D7,
// D10, D29, D35, D36, D39, D40, D44, D53, D68).
//
// Every test drives the engine path. The situations are rebuilt from the
// traced games the findings cite (docs/testing-findings-decks-2026-10.md);
// test-only cards stand in where a real card would bring a whole deck along.

#include "cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/chain_manager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace riftbound::test::batch1_chain_flow {

constexpr PlayerId P1 = PlayerId::Player1;
constexpr PlayerId P2 = PlayerId::Player2;

// Real cards.
constexpr CardDefId kViktorInnovator = 117;
constexpr CardDefId kGrandDuelist = 519;      // legend: "When one of your units becomes [Mighty], you may exhaust me ..."
constexpr CardDefId kFioraWorthy = 500;       // "When a unit you control becomes [Mighty], you may pay [Y] to ready it."
constexpr CardDefId kDianaLunari = 641;       // "When a showdown begins here, you may pay [1] to ..."
constexpr CardDefId kSacrifice = 735;         // "As an additional cost ..., kill a friendly [Mighty] unit. Draw 2 ..."
constexpr CardDefId kRiftHerald = 741;        // "[Deathknell] Play a unit from your hand to your base ..."
constexpr CardDefId kSkulker = 175;           // Shipyard Skulker, a plain 3-cost unit
constexpr CardDefId kLongSword = 345;
constexpr CardDefId kSeaMonkey = 420;         // "You may pay [1] as an additional cost to play me. ..."
constexpr CardDefId kAlphaStrike = 751;

// Test-only cards (ids in the spare range, never in a set's).
constexpr CardDefId kTLogA = 1990;
constexpr CardDefId kTLogB = 1991;
constexpr CardDefId kTMayLog = 1992;
constexpr CardDefId kTZap = 1993;
constexpr CardDefId kTKill = 1994;
constexpr CardDefId kTPump = 1995;
constexpr CardDefId kTDipRise = 1996;
constexpr CardDefId kTDipRiseInOne = 1997;
constexpr CardDefId kTAccel = 1998;
constexpr CardDefId kTNeedsTarget = 1999;

// What resolved, in order.
static std::vector<std::string>& log() {
    static std::vector<std::string> l;
    return l;
}

static GameObjectId subjectOf(const CardContext& ctx) {
    return ctx.state.chain.resuming.has_value() ? ctx.state.chain.resuming->triggering_subject
                                                : kInvalidId;
}

/// A unit whose ability writes its name to the log as it resolves. With
/// `may`: "you may ..." as the first part of its effect.
class TLog : public UnitCard {
public:
    TLog(CardDefId id, std::string name, std::vector<TriggerType> triggers, bool may = false)
        : triggers_(std::move(triggers)), may_(may) {
        def_.id = id;
        def_.name = std::move(name);
        def_.card_type = CardType::Unit;
        def_.might = 2;
        def_.energy_cost = 1;
        def_.domains = {Domain::Fury};
    }
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    std::vector<TriggerType> triggerTypes() const override { return triggers_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (may_) {
            const int go = confirmOptional(ctx, def_.name + ": write to the log?", [] { return true; });
            if (go < 1) return;   // waiting, or declined
        }
        if (finalizingChoices(ctx)) return;
        log().push_back(def_.name);
    }
protected:
    CardDef def_;
private:
    std::vector<TriggerType> triggers_;
    bool may_;
};

/// A unit with [Accelerate] and a "When you play me, you may ..." ability.
class TAccel : public TLog {
public:
    TAccel() : TLog(kTAccel, "Test Accel", {TriggerType::WhenYouPlayMe}, /*may=*/true) {
        def_.keywords.set(Keyword::Accelerate);
    }
};

/// A unit whose ability must choose an enemy unit, then also writes the log.
class TNeedsTarget : public UnitCard {
public:
    TNeedsTarget() {
        def_.id = kTNeedsTarget;
        def_.name = "Test Needs Target";
        def_.card_type = CardType::Unit;
        def_.might = 2;
    }
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAGear; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const GameObjectId enemy = pickTarget(ctx, "Test Needs Target: choose an enemy unit",
            chooseableUnits(ctx.state, ctx.controller,
                [&](const GameObject& o) { return o.controller != ctx.controller; }));
        if (finalizingChoices(ctx)) return;
        log().push_back(enemy == kInvalidId ? "no target" : "target");
    }
private:
    CardDef def_;
};

/// A unit whose ability does something to the event's object.
class TActs : public UnitCard {
public:
    enum class Does { Zap3, Kill, Pump2, DipThenRise, DipAndRiseInOne };
    TActs(CardDefId id, std::string name, Does does) : does_(does) {
        def_.id = id;
        def_.name = std::move(name);
        def_.card_type = CardType::Unit;
        def_.might = 2;
    }
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override {
        return does_ == Does::DipThenRise ? kFinalizeTrigger : 0u;
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAGear; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const GameObjectId it = subjectOf(ctx);
        if (it == kInvalidId || !ctx.state.objectExists(it)) return;
        switch (does_) {
            case Does::Zap3: ctx.executor.dealDamage(it, 3, ctx.source); break;   // no kill of its own
            case Does::Kill: ctx.executor.killObject(it); break;
            case Does::Pump2: ctx.executor.giveTemporaryMight(it, 2); break;
            case Does::DipThenRise:
                // "-1 [M]" is the cost, paid as the ability is finalized;
                // "+4 [M]" the effect (the shape of Sett, Brawler's buff).
                if (costDueNow(ctx)) ctx.executor.giveTemporaryMight(it, -1);
                if (finalizingChoices(ctx)) return;
                ctx.executor.giveTemporaryMight(it, 4);
                break;
            case Does::DipAndRiseInOne:
                ctx.executor.giveTemporaryMight(it, -1);
                ctx.executor.giveTemporaryMight(it, 4);
                break;
        }
        log().push_back(def_.name);
    }
private:
    CardDef def_;
    Does does_;
};

/// EnginePathAgent that shows every decision to a callback first.
class Spy : public EnginePathAgent {
public:
    using EnginePathAgent::EnginePathAgent;
    std::function<void(const std::vector<Intent>&)> see;
    int priority = 0, focus = 0;
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        for (const auto& i : legal) {
            if (i.type == IntentType::PassPriority) { ++priority; break; }
            if (i.type == IntentType::PassFocus) { ++focus; break; }
        }
        if (see) see(legal);
        return EnginePathAgent::selectAction(state, legal);
    }
    static bool isPriority(const std::vector<Intent>& legal) {
        return std::any_of(legal.begin(), legal.end(),
                           [](const Intent& i) { return i.type == IntentType::PassPriority; });
    }
    static bool isFocus(const std::vector<Intent>& legal) {
        return std::any_of(legal.begin(), legal.end(),
                           [](const Intent& i) { return i.type == IntentType::PassFocus; });
    }
    static bool isMay(const std::vector<Intent>& legal, const std::string& text) {
        return legal.size() == 2 && legal[1].choice_label.rfind("Yes", 0) == 0 &&
               legal[1].choice_label.find(text) != std::string::npos;
    }
};

class Batch1ChainFlow : public CardTestFixture {
protected:
    void SetUp() override {
        CardTestFixture::SetUp();
        using T = TriggerType;
        card_registry.registerCard(kTLogA, std::make_unique<TLog>(kTLogA, "A",
            std::vector<T>{T::WhenAnEnemyUnitDies, T::WhenIDie, T::WhenIDefend,
                           T::AtStartOfBeginning}));
        card_registry.registerCard(kTLogB, std::make_unique<TLog>(kTLogB, "B",
            std::vector<T>{T::WhenAFriendlyUnitDies, T::WhenAnEnemyUnitDies, T::WhenIDie}));
        card_registry.registerCard(kTMayLog, std::make_unique<TLog>(kTMayLog, "May",
            std::vector<T>{T::WhenYouPlayAGear}, /*may=*/true));
        card_registry.registerCard(kTZap, std::make_unique<TActs>(kTZap, "Zap", TActs::Does::Zap3));
        card_registry.registerCard(kTKill, std::make_unique<TActs>(kTKill, "Kill", TActs::Does::Kill));
        card_registry.registerCard(kTPump, std::make_unique<TActs>(kTPump, "Pump", TActs::Does::Pump2));
        card_registry.registerCard(kTDipRise,
            std::make_unique<TActs>(kTDipRise, "DipRise", TActs::Does::DipThenRise));
        card_registry.registerCard(kTDipRiseInOne,
            std::make_unique<TActs>(kTDipRiseInOne, "DipRiseInOne", TActs::Does::DipAndRiseInOne));
        card_registry.registerCard(kTAccel, std::make_unique<TAccel>());
        card_registry.registerCard(kTNeedsTarget, std::make_unique<TNeedsTarget>());
        card_db.buildFromClasses(card_registry);
        log().clear();
    }

    GameObject& o(GameObjectId id) { return state.getObject(id); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }

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
    /// P1 holds battlefield `bf` (control needs a unit there, CR 323.5).
    GameObjectId hold(PlayerId p, int bf) {
        state.battlefields[bf].controller = p;
        return addUnit(p, kInvalidId, 1, bf);
    }
    /// Queue `source`'s ability about `subject`, as an effect would
    /// (EffectExecutor::queueTriggeredAbility): no event, so no batch.
    static void queue(GameEngine& e, GameObjectId source, PlayerId controller,
                      GameObjectId subject = kInvalidId,
                      TriggerType t = TriggerType::WhenYouPlayAGear) {
        e.testHook_executor().queueTriggeredAbility(source, controller, t, subject);
    }
    int finalizedDuring(const std::function<void()>& body) {
        int n = 0;
        auto conn = events.on_chain_item_finalized.connect(
            [&](const ChainItemFinalizedEvent&) { ++n; });
        body();
        conn.disconnect();
        return n;
    }
};

// ═══ D4: a triggered ability that is not performed leaves the chain ═════════
//
// akali__akali__s20261100 D:055-057: "MAY_DECLINED: Akali, Deadly Weapon" and
// then both players were given priority and the item "resolved". CR 403.1.a:
// "If they choose not to perform the Triggered Ability now, it is removed
// from the chain."

TEST_F(Batch1ChainFlow, D4_ADeclinedMayLeavesTheChain_NobodyGetsPriority) {
    auto source = addUnit(P1, kTMayLog, 0);
    Spy p1(EnginePathAgent::script({0}));   // the "may": no
    Spy p2;
    const int finalized = finalizedDuring([&] {
        withEngine([&](GameEngine& e) {
            queue(e, source, P1);
            settleLikeTheEngine(e);
            EXPECT_FALSE(e.mutableState().chain.exists());
            EXPECT_EQ(e.mutableState().turn.oc_state, OpenClosedState::Open)
                << "the only item left: no Closed State";
        }, &p1, &p2);
    });
    EXPECT_EQ(p1.prompts.size(), 1u) << "asked once, as it would be finalized";
    EXPECT_EQ(finalized, 0) << "it never becomes a Finalized Chain Item";
    EXPECT_EQ(p1.priority + p2.priority, 0) << "no priority round for an ability that is gone";
    EXPECT_TRUE(log().empty());
}

TEST_F(Batch1ChainFlow, D4_AnAcceptedMayIsFinalizedAndBothPlayersGetPriority) {
    auto source = addUnit(P1, kTMayLog, 0);
    Spy p1(EnginePathAgent::script({1}));
    Spy p2;
    const int finalized = finalizedDuring([&] {
        withEngine([&](GameEngine& e) { queue(e, source, P1); settleLikeTheEngine(e); }, &p1, &p2);
    });
    EXPECT_EQ(finalized, 1);
    EXPECT_EQ(p1.priority, 1);
    EXPECT_EQ(p2.priority, 1);
    EXPECT_EQ(log(), std::vector<std::string>{"May"});
}

// CR 405.2 / 383.3.b.1: Fiora, Worthy's [Y] cannot be paid — the "may" is not
// offered and the ability is removed (akali_sb__fiora_sb__s20262104 D:182-184:
// MAY_NOT_OFFERED, then a priority round, then RESOLVE).
TEST_F(Batch1ChainFlow, D4_ACostThatCannotBePaidRemovesTheAbility) {
    auto fiora = addUnit(P1, kFioraWorthy, 0);
    auto big = addUnit(P1, kInvalidId, 6);
    o(big).is_exhausted = true;
    Spy p1(EnginePathAgent::script({}));   // nothing may be asked
    Spy p2;
    const int finalized = finalizedDuring([&] {
        withEngine([&](GameEngine& e) {
            queue(e, fiora, P1, big, TriggerType::WhenAUnitBecomesMighty);
            settleLikeTheEngine(e);
        }, &p1, &p2);
    });
    EXPECT_EQ(finalized, 0);
    EXPECT_EQ(p1.priority + p2.priority, 0);
    EXPECT_TRUE(o(big).is_exhausted);
}

// CR 403.4: "If there are not enough options to make legal choices for a
// Triggered Ability that has been put on the chain, remove it from the Chain
// now." — the whole ability, an instruction that needs no choice included.
TEST_F(Batch1ChainFlow, D4_ARequiredChoiceWithNoLegalOptionRemovesTheWholeAbility) {
    auto source = addUnit(P1, kTNeedsTarget, 0);
    Spy p1, p2;
    const int finalized = finalizedDuring([&] {
        withEngine([&](GameEngine& e) { queue(e, source, P1); settleLikeTheEngine(e); }, &p1, &p2);
    });
    EXPECT_EQ(finalized, 0);
    EXPECT_EQ(p1.priority + p2.priority, 0);
    EXPECT_TRUE(log().empty()) << "nothing of it happens (CR 403.4, strict reading)";

    // With an enemy unit to choose it is finalized and resolves in full.
    auto enemy = addUnit(P2, kInvalidId, 2);
    Spy q1(EnginePathAgent::choosing(enemy)), q2;
    withEngine([&](GameEngine& e) { queue(e, source, P1); settleLikeTheEngine(e); }, &q1, &q2);
    EXPECT_EQ(q2.priority, 1);
    EXPECT_EQ(log(), std::vector<std::string>{"target"});
}

// A mistarget is a different case (the brief's caution): the ability was
// finalized with a legal target, so it stays, gives priority, and resolves
// with the instruction ignored (CR 359.3.e.6).
TEST_F(Batch1ChainFlow, D4_AMistargetedAbilityStaysOnTheChain) {
    auto source = addUnit(P1, kTNeedsTarget, 0);
    auto enemy = addUnit(P2, kInvalidId, 2);
    Spy p1(EnginePathAgent::choosing(enemy));
    Spy p2;
    GameEngine* live = nullptr;
    p2.see = [&](const std::vector<Intent>& legal) {
        // In response: the chosen unit goes back to its owner's hand.
        if (Spy::isPriority(legal) && live && o(enemy).location.has_value())
            live->testHook_executor().bounceToHand(enemy);
    };
    const int finalized = finalizedDuring([&] {
        withEngine([&](GameEngine& e) {
            live = &e;
            queue(e, source, P1);
            settleLikeTheEngine(e);
            live = nullptr;
        }, &p1, &p2);
    });
    EXPECT_EQ(finalized, 1);
    EXPECT_GE(p2.priority, 1);
    EXPECT_EQ(log(), std::vector<std::string>{"no target"});
}

// ═══ D44 / D10: a Cleanup between the items of a chain ══════════════════════
//
// akali__ezreal__s20261103 D:083-091: Wages of Pain dealt 3 to a 3 [M]
// Ezreal, Prodigy, who then stood on the board through four priority
// decisions and a gear play until the chain was empty. CR 319.5 + 323.4.

TEST_F(Batch1ChainFlow, D44_LethalDamageIsKilledBeforeTheNextItemGetsPriority) {
    auto lower = addUnit(P1, kTMayLog, 0);          // the older item: "May"
    auto zapper = addUnit(P1, kTZap, 0);            // the newer: deals 3, kills nothing itself
    auto victim = addUnit(P2, kTLogB, 3);           // 3 [M], "When I die": "B"
    std::vector<bool> victim_seen;                  // on the board at each priority decision
    GameState* live = nullptr;
    Spy p1(EnginePathAgent::accepting()), p2;
    p2.see = [&](const std::vector<Intent>& legal) {
        if (Spy::isPriority(legal) && live)
            victim_seen.push_back(live->getObject(victim).location.has_value());
    };
    withEngine([&](GameEngine& e) {
        live = &e.mutableState();
        queue(e, lower, P1);
        queue(e, zapper, P1, victim);
        settleLikeTheEngine(e);
        live = nullptr;
    }, &p1, &p2);
    ASSERT_GE(victim_seen.size(), 2u);
    EXPECT_TRUE(victim_seen.front()) << "before Zap resolves it is alive";
    EXPECT_FALSE(victim_seen[1]) << "the Cleanup after Zap killed it before anyone had priority";
    // Its "When I die" went on the chain above the older item and resolved first.
    EXPECT_EQ(log(), (std::vector<std::string>{"Zap", "B", "May"}));
}

// fiora__vex__s20264185 D:244-247: Boots of Swiftness lay unattached at the
// battlefield through two priority decisions and a Main Phase decision after
// their bearer died. CR 319.5 / 319.6 + 323.6.
TEST_F(Batch1ChainFlow, D10_EquipmentOfADeadBearerIsRecalledBeforeTheNextPriority) {
    hold(P2, 0);
    auto lower = addUnit(P1, kTLogA, 0);
    auto killer = addUnit(P1, kTKill, 0);
    auto bearer = addUnit(P2, kInvalidId, 3, 0);
    auto sword = addUnit(P2, kInvalidId, 0);        // re-typed below: a gear with Long Sword's card
    {
        auto& g = o(sword);
        g.card_type = CardType::Gear;
        g.card_def_id = kLongSword;
        g.name = "Long Sword";
        g.tags = {"Equipment"};
    }
    std::vector<bool> sword_in_base;
    GameState* live = nullptr;
    Spy p1, p2;
    p2.see = [&](const std::vector<Intent>& legal) {
        if (Spy::isPriority(legal) && live)
            sword_in_base.push_back(live->getObject(sword).isAtBase());
    };
    withEngine([&](GameEngine& e) {
        ASSERT_TRUE(e.testHook_executor().attachGear(sword, bearer));
        live = &e.mutableState();
        queue(e, lower, P1, kInvalidId, TriggerType::AtStartOfBeginning);
        queue(e, killer, P1, bearer);
        settleLikeTheEngine(e);
        live = nullptr;
    }, &p1, &p2);
    ASSERT_GE(sword_in_base.size(), 2u);
    EXPECT_FALSE(sword_in_base.front()) << "attached, at the battlefield";
    EXPECT_TRUE(sword_in_base[1]) << "recalled by the Cleanup that follows the kill (CR 323.6)";
    EXPECT_FALSE(o(sword).attached_to.has_value());
}

// ═══ D35 / D36: "becomes [Mighty]" is noticed at the Cleanup after each step ═

// akali_sb__fiora_sb__s20262104 D:177-179: +2 [M] made Fiora 5 [M]; two
// priority decisions and another resolution later the trigger appeared.
TEST_F(Batch1ChainFlow, D35_BecomesMightyTriggersBeforeTheNextItemOfTheChainResolves) {
    auto duelist = addLegend(P1, kGrandDuelist);
    auto lower = addUnit(P1, kTLogA, 0);
    auto pump = addUnit(P1, kTPump, 0);
    auto four = addUnit(P1, kInvalidId, 4);
    std::vector<std::string> log_at_may;
    bool asked = false;
    Spy p1(EnginePathAgent::accepting());
    p1.see = [&](const std::vector<Intent>& legal) {
        if (Spy::isMay(legal, "Grand Duelist")) { asked = true; log_at_may = log(); }
    };
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();   // the board as the game knows it: nobody is Mighty
        queue(e, lower, P1, kInvalidId, TriggerType::AtStartOfBeginning);
        queue(e, pump, P1, four);
        settleLikeTheEngine(e);
    }, &p1);
    ASSERT_TRUE(asked) << "4 [M] + 2: the unit became Mighty";
    EXPECT_EQ(log_at_may, std::vector<std::string>{"Pump"})
        << "asked right after the pump, before the older item resolved";
    EXPECT_TRUE(o(duelist).is_exhausted);
    EXPECT_EQ(log(), (std::vector<std::string>{"Pump", "A"}));
}

// diana_sb__fiora_sb__s20262140 D:165-166: "-5 [M] this turn" expired in the
// Expiration Step and the trigger appeared in the NEXT turn's Beginning Step.
// CR 317.2: the expiry is part of the Ending Special Cleanup.
TEST_F(Batch1ChainFlow, D35_AMightPenaltyThatExpiresTriggersInTheExpirationStepOfThatTurn) {
    auto duelist = addLegend(P1, kGrandDuelist);
    auto six = addUnit(P1, kInvalidId, 6);
    // (The first "may" is for the unit as the game first sees it at 6 [M]:
    // declined. The second is the one this test is about: accepted.)
    Spy p1(EnginePathAgent::script({0, 1}));
    int turn_at_may = -1;
    TurnPhase phase_at_may = TurnPhase::Setup;
    GameState* live = nullptr;
    p1.see = [&](const std::vector<Intent>& legal) {
        if (Spy::isMay(legal, "Grand Duelist") && live) {
            turn_at_may = live->turn.turn_number;
            phase_at_may = live->turn.phase;
        }
    };
    withEngine([&](GameEngine& e) {
        live = &e.mutableState();
        settleLikeTheEngine(e);                                // Mighty, and known to be
        e.testHook_executor().giveTemporaryMight(six, -5);     // 1 [M] this turn
        e.testHook_cleanup();                                  // no longer Mighty
        const int turn = e.mutableState().turn.turn_number;
        e.testHook_expirationStep();
        EXPECT_EQ(turn_at_may, turn);
        live = nullptr;
    }, &p1);
    EXPECT_EQ(phase_at_may, TurnPhase::ExpirationStep);
    EXPECT_TRUE(o(duelist).is_exhausted);
    EXPECT_EQ(o(six).current_might, 6);
}

// ezreal_sb__fiora_sb__s20262158 D:197-200: Sett, Brawler spent his buff as
// the cost (5 -> 4, at finalization) and got +4 as the ability resolved
// (-> 8): CR 709, he became Mighty; no trigger followed.
TEST_F(Batch1ChainFlow, D36_BelowFiveAtFinalizationAndBackAtResolutionIsBecomingMighty) {
    auto duelist = addLegend(P1, kGrandDuelist);
    auto source = addUnit(P1, kTDipRise, 0);
    auto five = addUnit(P1, kInvalidId, 5);
    // (The first "may" is for the unit as the game first sees it at 5 [M]:
    // declined. A second one is what this test is about.)
    Spy p1(EnginePathAgent::script({0, 1}));
    withEngine([&](GameEngine& e) {
        settleLikeTheEngine(e);        // 5 [M]: Mighty already, and known to be
        queue(e, source, P1, five);
        settleLikeTheEngine(e);
    }, &p1);
    EXPECT_EQ(o(five).current_might, 8);
    EXPECT_EQ(p1.prompts.size(), 2u) << "5 -> 4 (cost) -> 8 (effect): it became Mighty again";
    EXPECT_TRUE(o(duelist).is_exhausted);
}

// The limit, pinned (orchestrator's decision on D36): "becomes Mighty" is
// noticed at Cleanups; none runs inside a resolution (CR 321), so a dip and
// return within ONE resolution is not an event for the game.
TEST_F(Batch1ChainFlow, D36_Pin_ADipAndReturnInsideOneResolutionIsNotBecomingMighty) {
    auto duelist = addLegend(P1, kGrandDuelist);
    auto source = addUnit(P1, kTDipRiseInOne, 0);
    auto five = addUnit(P1, kInvalidId, 5);
    Spy p1(EnginePathAgent::script({0}));   // only the first sight of the 5 [M] unit asks
    withEngine([&](GameEngine& e) {
        settleLikeTheEngine(e);
        queue(e, source, P1, five);
        settleLikeTheEngine(e);
    }, &p1);
    EXPECT_EQ(o(five).current_might, 8);
    EXPECT_FALSE(o(duelist).is_exhausted);
    EXPECT_EQ(p1.prompts.size(), 1u) << "no second \"becomes Mighty\"";
}

// ═══ D7 / D29: a showdown's start, in a showdown and in a combat ════════════

// diana__azir__s20261137 D:064: the trigger fired as the showdown began, Focus
// was passed by both players, the battlefield was conquered, and only then was
// Diana's "may" asked. CR 335.1: Focus only with no pending Chain Items.
TEST_F(Batch1ChainFlow, D7_AShowdownStartAbilityIsAskedBeforeTheFirstFocusDecision) {
    auto diana = addUnit(P1, kDianaLunari, 0, 0);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kSkulker);
    state.player(P1).rune_pool.energy = 1;
    std::vector<std::string> seq;
    Spy p1(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    auto note = [&](const std::vector<Intent>& legal) {
        if (Spy::isFocus(legal)) seq.push_back("focus");
        else if (Spy::isMay(legal, "Diana")) seq.push_back("may");
    };
    p1.see = note;
    Spy p2;
    p2.see = note;
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); }, &p1, &p2);
    ASSERT_FALSE(seq.empty());
    EXPECT_EQ(seq.front(), "may") << "before anyone has Focus";
    EXPECT_GE(std::count(seq.begin(), seq.end(), "focus"), 2);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "the [1] was paid";
    (void)diana;
}

// diana__akali__s20261136 D:268: COMBAT, then DAMAGE_STEP, no Diana trigger
// in any of 52 combats. CR 344.1 / 466.2.b: a combat opens with a showdown.
TEST_F(Batch1ChainFlow, D29_AShowdownBeginsHereTriggersWhenACombatOpens) {
    hold(P2, 0);
    auto diana = addUnit(P1, kDianaLunari, 0, 0);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kSkulker);
    state.player(P1).rune_pool.energy = 1;
    bool asked = false;
    Spy p1(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    p1.see = [&](const std::vector<Intent>& legal) {
        if (Spy::isMay(legal, "Diana")) asked = true;
    };
    Spy p2;
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); }, &p1, &p2);
    EXPECT_TRUE(asked) << "the combat's showdown began at her battlefield";
    (void)diana;
}

// ═══ D6: what is offered at chain priority can be executed ══════════════════

// akali__zed__s20261117 D:052-056: with Sigil of the Storm's ability on the
// chain and the state left Open by the showdown, "Play Death Mark" (neither
// [Action] nor [Reaction]) was offered at chain priority; choosing it did
// nothing and the decision was asked again, three times.
TEST_F(Batch1ChainFlow, D6_ChainPriorityOffersNoPlayThatIsNotAReaction) {
    auto source = addUnit(P1, kTLogA, 0);
    auto skulker = addToHand(P1, kSkulker);          // a unit: not a Reaction
    state.player(P1).rune_pool.energy = 10;
    std::vector<IntentType> offered;
    Spy p1;
    p1.see = [&](const std::vector<Intent>& legal) {
        if (!Spy::isPriority(legal)) return;
        for (const auto& i : legal) offered.push_back(i.type);
    };
    withEngine([&](GameEngine& e) {
        queue(e, source, P1, kInvalidId, TriggerType::AtStartOfBeginning);
        // What a showdown's start did with an ability pending: the state
        // written Open with an item on the chain.
        e.mutableState().turn.oc_state = OpenClosedState::Open;
        e.testHook_runChain();
    }, &p1);
    ASSERT_FALSE(offered.empty());
    for (IntentType t : offered)
        EXPECT_TRUE(t == IntentType::PassPriority || t == IntentType::PlayReaction ||
                    t == IntentType::ActivateReactionAbility)
            << "offered at chain priority: " << toString(t);
    EXPECT_TRUE(inHand(P1, skulker));
}

// "A silently dropped choice must be impossible": an intent chain priority
// has no way to execute is an engine error, not a decision asked again.
TEST_F(Batch1ChainFlow, D6_AnIntentChainPriorityCannotExecuteThrows) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    ChainManager cm(state, events, card_db);
    cm.setEffectExecutor(&exec);
    auto source = addUnit(P1, kInvalidId, 2);
    cm.addAbility(source, P1, kInvalidId);
    int asked = 0;
    auto agent = [&](PlayerId p, const std::vector<Intent>&) {
        ++asked;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = p;
        return play;
    };
    EXPECT_THROW(cm.processFEPR(agent, [](const ChainItem&) {}, [](const ChainItem&) {}),
                 std::logic_error);
    EXPECT_EQ(asked, 1) << "not asked again";
}

// ═══ D1: a permanent's play ═════════════════════════════════════════════════

// rengar__diana__s20261300 D:113-119: Kai'Sa's cost was paid, Pridestalker's
// play ability was finalized (its target chosen), and only then was her
// Accelerate asked. CR 355.1.a (the choice comes before the cost), 383.4.a.2
// (play effects go on the chain after the permanent has entered).
TEST_F(Batch1ChainFlow, D1_AccelerateIsChosenBeforeTheCost_AndThePlayAbilityAfterTheUnitEntered) {
    auto unit = addToHand(P1, kTAccel);
    state.player(P1).rune_pool.energy = 2;                                   // [1] + Accelerate's [1]
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;    // Accelerate's [R]
    std::vector<std::string> seq;
    bool on_board_at_may = false, ready_at_may = false;
    int energy_at_accelerate = -1;
    GameState* live = nullptr;
    Spy p1(EnginePathAgent::script({1, 1}));   // Accelerate: pay; the "may": yes
    p1.see = [&](const std::vector<Intent>& legal) {
        if (Spy::isPriority(legal) || !live) return;
        if (Spy::isMay(legal, "Test Accel")) {
            seq.push_back("may");
            on_board_at_may = live->getObject(unit).zone == ZoneType::Base;
            ready_at_may = !live->getObject(unit).is_exhausted;
        } else {
            seq.push_back("accelerate");
            energy_at_accelerate = live->player(P1).rune_pool.energy;
        }
    };
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = unit;
    play.play_location = LocationId{BaseLocation{P1}};
    withEngine([&](GameEngine& e) {
        live = &e.mutableState();
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
        live = nullptr;
    }, &p1);
    ASSERT_EQ(seq, (std::vector<std::string>{"accelerate", "may"}));
    EXPECT_EQ(energy_at_accelerate, 2) << "asked before anything was paid";
    EXPECT_TRUE(on_board_at_may) << "its play ability is finalized with the unit on the board";
    EXPECT_TRUE(ready_at_may) << "ready: the Accelerate was paid with the cost";
    EXPECT_FALSE(o(unit).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(log(), std::vector<std::string>{"Test Accel"});
}

TEST_F(Batch1ChainFlow, D1_AccelerateIsNotOfferedWhenItCannotBePaidOnTopOfTheCost) {
    auto unit = addToHand(P1, kTAccel);
    state.player(P1).rune_pool.energy = 1;                                   // the card's [1] only
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    Spy p1(EnginePathAgent::script({1}));   // only the "may" is asked
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = unit;
    play.play_location = LocationId{BaseLocation{P1}};
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); settleLikeTheEngine(e); }, &p1);
    EXPECT_EQ(p1.prompts.size(), 1u);
    EXPECT_TRUE(o(unit).is_exhausted);
}

// akali__fiora__s20261104 D:078-082 (reader 4, R4-13): Rampage's Energy was
// paid, then "pay additional cost for Rampage?" was asked. CR 355.1.a.
TEST_F(Batch1ChainFlow, D1_TheOptionalAdditionalCostIsChosenBeforeTheCardIsPaidFor) {
    auto monkey = addToHand(P1, kSeaMonkey);
    const int cost = card_db.get(kSeaMonkey).energy_cost;
    for (int i = 0; i < cost + 1; ++i) addRune(P1, card_db.get(kSeaMonkey).domains.front());
    std::vector<std::string> seq;
    Spy p1(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    p1.see = [&](const std::vector<Intent>& legal) {
        if (Spy::isPriority(legal)) return;
        const bool objects = std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return !i.chosen_objects.empty(); });
        seq.push_back(objects ? "rune" : "yes/no");
    };
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = monkey;
    play.play_location = LocationId{BaseLocation{P1}};
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); settleLikeTheEngine(e); }, &p1);
    ASSERT_FALSE(seq.empty());
    EXPECT_EQ(seq.front(), "yes/no") << "the additional cost is decided before a rune is exhausted";
    EXPECT_TRUE(onBoard(monkey));
    EXPECT_EQ(o(monkey).buff_count, 1) << "paid: \"buff me\"";
}

// ═══ D39: what paying for a card triggers is above that card ════════════════

// fiora__fiora__s20261176 D:082-089: Sacrifice killed Rift Herald as its
// additional cost; the Deathknell went UNDER Sacrifice, so Sacrifice drew two
// first and the Deathknell then played a unit that had just been drawn.
TEST_F(Batch1ChainFlow, D39_ADeathknellFromASpellsAdditionalCostResolvesBeforeTheSpell) {
    auto herald = addUnit(P1, kRiftHerald, 0);
    auto in_hand = addToHand(P1, kSkulker);
    auto spell = addToHand(P1, kSacrifice);
    auto top1 = addToDeck(P1, kSkulker);
    auto top2 = addToDeck(P1, kSkulker);
    const auto& def = card_db.get(kSacrifice);
    state.player(P1).rune_pool.energy = def.energy_cost;
    state.player(P1).rune_pool.universal_power = def.power_cost + 3;
    Spy p1(EnginePathAgent::script({static_cast<int>(herald.raw()), EnginePathAgent::kRest}));
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
    }, &p1);
    EXPECT_FALSE(onBoard(herald));
    EXPECT_TRUE(onBoard(in_hand)) << "the Deathknell played the unit that was in hand";
    EXPECT_FALSE(p1.wasOffered(top1)) << "Sacrifice had not drawn yet when the Deathknell resolved";
    EXPECT_FALSE(p1.wasOffered(top2));
    EXPECT_TRUE(inHand(P1, top1));
    EXPECT_TRUE(inHand(P1, top2));
}

// ═══ D40: simultaneous triggers are ordered by their controllers ════════════

// akali__fiora__s20261104 D:040: Fiora, Worthy and Grand Duelist triggered on
// one unit becoming Mighty and were always resolved in the same order; no
// decision existed. CR 383.3.d.
TEST_F(Batch1ChainFlow, D40_OnePlayerOrdersTheirSimultaneousTriggers) {
    auto a = addUnit(P1, kTLogA, 0);     // "When an enemy unit dies": A
    auto b = addUnit(P1, kTLogB, 0);     // "When an enemy unit dies": B
    auto victim = addUnit(P2, kInvalidId, 2);
    (void)a; (void)b;
    {   // First option: the one the engine queued first goes on the chain
        // first — it is the lowest and resolves last.
        Spy p1;
        resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(victim); }, &p1);
        ASSERT_EQ(p1.order_prompts.size(), 1u) << "two abilities: one question";
        ASSERT_EQ(p1.order_prompts[0].size(), 2u);
        const bool a_first =
            p1.order_prompts[0][0].choice_label.find("A (") != std::string::npos;
        EXPECT_EQ(log(), a_first ? (std::vector<std::string>{"B", "A"})
                                 : (std::vector<std::string>{"A", "B"}));
    }
    log().clear();
    {   // B first on the chain: it resolves last.
        auto victim2 = addUnit(P2, kInvalidId, 2);
        Spy p1;
        p1.order_pick = EnginePathAgent::orderFirst("B (");
        resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(victim2); }, &p1);
        ASSERT_EQ(p1.order_prompts.size(), 1u);
        EXPECT_EQ(log(), (std::vector<std::string>{"A", "B"}));
    }
}

// CR 383.3.d.1: "starting with the Turn Player ..., each player orders their
// Triggered Abilities on the Chain" — the turn player's are lowest and
// resolve last, whatever order the engine found the sources in.
TEST_F(Batch1ChainFlow, D40_TheTurnPlayersTriggersGoOnTheChainFirst) {
    auto theirs = addUnit(P2, kTLogB, 0);    // P2: "When a friendly unit dies": B (found first)
    auto mine = addUnit(P1, kTLogA, 0);      // P1: "When an enemy unit dies": A
    auto victim = addUnit(P2, kInvalidId, 2);
    (void)theirs; (void)mine;
    Spy p1, p2;
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) { ex.killObject(victim); }, &p1, &p2,
                         /*turn_player=*/P1);
    EXPECT_TRUE(p1.order_prompts.empty()) << "one ability each: nothing to order";
    EXPECT_TRUE(p2.order_prompts.empty());
    EXPECT_EQ(log(), (std::vector<std::string>{"B", "A"})) << "P1's is lowest: it resolves last";
}

// The same ability twice (one source, one event, one subject) asks nothing;
// and abilities queued by different events are not simultaneous.
TEST_F(Batch1ChainFlow, D40_NothingIsAskedWhenTheOrderCannotMatter) {
    auto a = addUnit(P1, kTLogA, 0);
    auto v1 = addUnit(P2, kInvalidId, 2);
    auto v2 = addUnit(P2, kInvalidId, 2);
    (void)a;
    Spy p1;
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.killObject(v1);     // one event
        ex.killObject(v2);     // another: A triggers again, at another time
    }, &p1);
    EXPECT_TRUE(p1.order_prompts.empty());
    EXPECT_EQ(log(), (std::vector<std::string>{"A", "A"}));
}

// ═══ D68: a unit that joins a combat under way ══════════════════════════════

// teemo__viktor__s20261330 D:146-158: Teemo, Strategist was played from
// facedown into a combat, defended, and "When I defend" never triggered.
// CR 383.4.f, 466.2.c.3.a.
TEST_F(Batch1ChainFlow, D68_AUnitThatJoinsACombatTriggersItsDefendAbilityOnce) {
    hold(P1, 0);
    addUnit(P2, kInvalidId, 3, 0);
    combatInProgress(0, P2);                       // P2 attacks; the units there are in it
    auto late = addUnit(P1, kTLogA, 0, 0);         // arrives now: "When I defend": A
    Spy p1, p2;
    withEngine([&](GameEngine& e) { settleLikeTheEngine(e); }, &p1, &p2);
    EXPECT_EQ(o(late).combat_designation, CombatDesignation::Defender);
    EXPECT_EQ(log(), std::vector<std::string>{"A"}) << "it gained the designation: it defends";
    // Once per combat (CR 383.4.f.2.a): later Cleanups of the same combat do not ask again.
    withEngine([&](GameEngine& e) { settleLikeTheEngine(e); }, &p1, &p2);
    EXPECT_EQ(log().size(), 1u);
}

// ═══ D53: [Temporary] is a triggered ability (CR 816) ═══════════════════════
//
// azir__leblanc__s20263035 D:078: "── Beginning Step ──" / "KILL: Honest
// Broker" — killed directly as the step began, with no ability on the chain,
// no priority, and always before the "start of your Beginning Phase"
// abilities. CR 816.1.b: "At the start of this permanent's controller's
// Beginning Phase, before scoring, kill this."

class Batch1Temporary : public Batch1ChainFlow {
protected:
    GameObjectId temporary(PlayerId owner, int bf = -1, int might = 2) {
        auto id = addUnit(owner, kInvalidId, might, bf);
        o(id).keywords.set(Keyword::Temporary);
        o(id).name = "Temp";
        return id;
    }
};

TEST_F(Batch1Temporary, D53_ALoneTemporaryUnitDiesBeforeScoring_NoHoldAndNoControl) {
    state.battlefields[0].controller = P1;
    auto unit = temporary(P1, 0);
    Spy p1, p2;
    const int finalized = finalizedDuring([&] {
        withEngine([&](GameEngine& e) {
            e.testHook_beginningStep();
            EXPECT_FALSE(e.mutableState().getObject(unit).location.has_value())
                << "dead when the Beginning Step is over";
            EXPECT_FALSE(e.mutableState().battlefields[0].controller.has_value())
                << "control is settled by the Cleanup before the Scoring Step";
            e.testHook_scoringStep();
        }, &p1, &p2, P1);
    });
    EXPECT_EQ(state.player(P1).score, 0) << "\"before scoring\": no Hold";
    EXPECT_EQ(finalized, 1) << "the kill is an ability on the chain";
    EXPECT_EQ(p1.priority, 1);
    EXPECT_EQ(p2.priority, 1) << "the opponent may respond to it";
    EXPECT_TRUE(inTrash(P1, unit));
}

TEST_F(Batch1Temporary, D53_OnlyInItsControllersBeginningPhase) {
    auto mine = temporary(P1);
    auto theirs = temporary(P2);
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, nullptr, nullptr, P2);
    EXPECT_TRUE(onBoard(mine)) << "P2's Beginning Phase: P1's permanent does not trigger";
    EXPECT_FALSE(onBoard(theirs));
}

// The opponent responds: the unit is returned to its owner's hand. The
// ability resolves and finds nothing ("this" is gone) — also when the card is
// back on the board by then: it is a new object (CR 124.1).
TEST_F(Batch1Temporary, D53_ReturnedToHandInResponse_ItIsNotKilled_EvenIfPlayedAgain) {
    for (bool replay : {false, true}) {
        SCOPED_TRACE(replay ? "played again in response" : "left in hand");
        auto unit = addUnit(P1, kSkulker, 0);
        o(unit).keywords.set(Keyword::Temporary);
        o(unit).granted_keywords.set(Keyword::Temporary);
        GameEngine* live = nullptr;
        bool responded = false;
        Spy p1, p2;
        p2.see = [&](const std::vector<Intent>& legal) {
            if (!Spy::isPriority(legal) || !live || responded) return;
            responded = true;
            live->testHook_executor().bounceToHand(unit);
            if (replay) live->testHook_executor().playIgnoringCost(P1, unit);
        };
        withEngine([&](GameEngine& e) {
            live = &e;
            e.testHook_beginningStep();
            live = nullptr;
        }, &p1, &p2, P1);
        ASSERT_TRUE(responded);
        EXPECT_FALSE(inTrash(P1, unit));
        if (replay) EXPECT_TRUE(onBoard(unit)); else EXPECT_TRUE(inHand(P1, unit));
    }
}

// The opponent responds by taking control of it: "kill this" still kills the
// permanent the ability triggered on, and it goes to its owner's trash.
TEST_F(Batch1Temporary, D53_ControlChangesInResponse_ItIsStillKilled) {
    auto unit = temporary(P1);
    GameEngine* live = nullptr;
    bool responded = false;
    Spy p1, p2;
    p2.see = [&](const std::vector<Intent>& legal) {
        if (!Spy::isPriority(legal) || !live || responded) return;
        responded = true;
        live->testHook_executor().takeControl(unit, P2);
    };
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_beginningStep();
        live = nullptr;
    }, &p1, &p2, P1);
    ASSERT_TRUE(responded);
    EXPECT_FALSE(onBoard(unit));
    EXPECT_TRUE(inTrash(P1, unit));
}

// Two [Temporary] units and an "At the start of your Beginning Phase" ability
// trigger together: their controller orders them (CR 383.3.d) — so the other
// ability can resolve while the units are still there (Dusk Rose Lab killing
// a Reflection that is about to die anyway).
TEST_F(Batch1Temporary, D53_TemporaryKillsAndAStartOfPhaseAbilityAreOneBatch) {
    auto died = events.on_unit_died.connect([&](const UnitDiedEvent&) { log().push_back("died"); });
    addUnit(P1, kTLogA, 0);          // "At the start of your Beginning Phase": A
    temporary(P1);
    temporary(P1);
    Spy p1;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, &p1, nullptr, P1);
    died.disconnect();
    ASSERT_EQ(p1.order_prompts.size(), 2u) << "three abilities: two questions";
    EXPECT_EQ(p1.order_prompts[0].size(), 3u) << "A and the two units' [Temporary]";
    EXPECT_EQ(p1.order_prompts[1].size(), 2u);
    // First options: as queued. A went on first and resolves last.
    EXPECT_EQ(log(), (std::vector<std::string>{"died", "died", "A"}));
}

TEST_F(Batch1Temporary, D53_TheControllerMayHaveTheOtherAbilityResolveBeforeTheKills) {
    auto died = events.on_unit_died.connect([&](const UnitDiedEvent&) { log().push_back("died"); });
    addUnit(P1, kTLogA, 0);
    temporary(P1);
    temporary(P1);
    Spy p1;
    p1.order_pick = EnginePathAgent::orderFirst("[Temporary]");
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, &p1, nullptr, P1);
    died.disconnect();
    EXPECT_EQ(log(), (std::vector<std::string>{"A", "died", "died"}))
        << "the [Temporary] kills were put on the chain first: A resolved before them";
}

// ═══ F1: Viktor, Innovator and tokens ═══════════════════════════════════════

// deck_fuzz --replay 8392194388957710676 6231710079672200936 412255519: on an
// opponent's turn Viktor's Recruit triggered Viktor again, without end.
TEST_F(Batch1ChainFlow, F1_ViktorDoesNotTriggerOnAToken_AndTriggersOncePerCard) {
    addUnit(P1, kViktorInnovator, 0);
    auto recruits = [&] {
        int n = 0;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.isToken() && obj.controller == P1 && obj.name == "Recruit") ++n;
        return n;
    };
    // A token played on the opponent's turn: not a card (CR 350.2).
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.createToken(P1, CardType::Unit, "Sprite", 1, {"Sprite"}, KeywordSet{},
                       LocationId{BaseLocation{P1}});
    }, nullptr, nullptr, /*turn_player=*/P2);
    EXPECT_EQ(recruits(), 0);
    // A card played on the opponent's turn: one Recruit, and the chain ends.
    auto card = addToHand(P1, kSkulker);
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.playIgnoringCost(P1, card);
    }, nullptr, nullptr, /*turn_player=*/P2);
    EXPECT_EQ(recruits(), 1);
    // On its controller's own turn: nothing.
    auto another = addToHand(P1, kSkulker);
    state.turn.turn_player = P1;
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.playIgnoringCost(P1, another);
    }, nullptr, nullptr, /*turn_player=*/P1);
    EXPECT_EQ(recruits(), 1);
}

// ═══ F2: Alpha Strike without a play-time target ════════════════════════════

// deck_fuzz --replay 15524873237644084445 9693623397702818148 464654890:
// Blind Fury played a revealed Alpha Strike; its finalize pass ended at
// "if (targets.empty()) return;" and the engine threw.
TEST_F(Batch1ChainFlow, F2_AlphaStrikePlayedByAnEffectChoosesItsUnitInTheFinalizePass) {
    auto strike = addToHand(P1, kAlphaStrike);
    auto mine = addUnit(P1, kInvalidId, 3);
    hold(P2, 0);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    Spy p1(EnginePathAgent::script(
        {static_cast<int>(mine.raw()), EnginePathAgent::kRest}));
    EXPECT_NO_THROW(resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.playIgnoringCost(P1, strike);
    }, &p1));
    EXPECT_TRUE(p1.wasOffered(mine)) << "\"Choose a friendly unit\" is asked";
    EXPECT_FALSE(p1.wasOffered(enemy) && p1.prompts.size() == 1u);
    EXPECT_TRUE(inTrash(P1, strike));
}

TEST_F(Batch1ChainFlow, F2_AlphaStrikePlayedByAnEffectWithNoFriendlyUnitDoesNothing) {
    auto strike = addToHand(P1, kAlphaStrike);
    hold(P2, 0);
    Spy p1(EnginePathAgent::script({}));
    EXPECT_NO_THROW(resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        ex.playIgnoringCost(P1, strike);
    }, &p1));
    EXPECT_TRUE(inTrash(P1, strike)) << "a spell resolves whatever it could choose (CR 359.3.e.1)";
}

}  // namespace riftbound::test::batch1_chain_flow
