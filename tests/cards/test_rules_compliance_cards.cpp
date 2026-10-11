/// @file test_rules_compliance_cards.cpp
/// Card-level rules-compliance fixes from docs/rules-audit-2026-10.md. Each test
/// names the rule (rules/core-rules.md) it enforces.
///
///   A. "Pay [N]" inside an effect may exhaust runes (CR 445.2, 164.2.a)
///   B. Split damage is the controller's division (CR 355.14)
///   C. [A] is power of any Domain (Shadow)
///   D. An effect's cost is paid exactly once (Ava Achiever, Icevale Archer,
///      Sinister Poro) even though the trigger is re-entered per choice
///   E. "When you play me from face down" only on a play from face down
///   F. Token characteristics (CR 187)
///   G. "Can't be chosen by enemy spells and abilities" in custom target lists
///   H. Mirror Image: "to your base", lasting [Temporary] (CR 816)

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_rules_compliance_cards {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kIronBallista     = 17;
constexpr CardDefId kVayneHunter35    = 35;
constexpr CardDefId kVolibearFurious  = 41;
constexpr CardDefId kCaitlynPatrolling = 68;
constexpr CardDefId kAvaAchiever      = 107;
constexpr CardDefId kGust             = 169;
constexpr CardDefId kFadingMemories   = 180;
constexpr CardDefId kGold             = 326;
constexpr CardDefId kProductionSurge  = 399;
constexpr CardDefId kAncientHenge     = 438;
constexpr CardDefId kJaxUnrelenting   = 440;
constexpr CardDefId kCull             = 455;
constexpr CardDefId kSpiritWheel      = 465;
constexpr CardDefId kCounterStrike    = 510;
constexpr CardDefId kEmperorOfSands   = 512;
constexpr CardDefId kArise            = 513;
constexpr CardDefId kHostileTakeover  = 516;
constexpr CardDefId kHallOfLegends    = 524;
constexpr CardDefId kSunkenTemple     = 532;
constexpr CardDefId kTreasureHoard    = 534;
constexpr CardDefId kVayneHunter537   = 537;
constexpr CardDefId kBladeDancer      = 555;
constexpr CardDefId kXerathFreed      = 588;
constexpr CardDefId kIcevaleArcher    = 627;
constexpr CardDefId kDeadlyFlourish   = 635;
constexpr CardDefId kDianaLunari      = 641;
constexpr CardDefId kBloodRose        = 671;
constexpr CardDefId kSinisterPoro     = 699;
constexpr CardDefId kTheList          = 700;
constexpr CardDefId kEvelynn          = 703;
constexpr CardDefId kDiviningShells   = 723;
constexpr CardDefId kViPeacekeeper    = 738;
constexpr CardDefId kAlphaStrike      = 751;
constexpr CardDefId kShadow           = 752;
constexpr CardDefId kMirrorImage      = 757;
constexpr CardDefId kVoidAssault      = 758;
constexpr CardDefId kKeepersVerdict   = 760;
constexpr CardDefId kRippersBay       = 770;
constexpr CardDefId kVoidreaver       = 787;
constexpr CardDefId kTornadoWarrior   = 886;   // a unit with [Hidden]
constexpr CardDefId kIllaoiProphet    = 896;
constexpr CardDefId kZedWithoutASound = 899;

using Picker = std::function<Intent(const std::vector<Intent>&)>;

class RulesCardsTest : public CardTestFixture {
protected:
    // ── pickers ──
    /// "Yes" to a confirmOptional prompt; first option otherwise.
    static Intent yes(const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
    static Intent no(const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
    /// Counts prompts and records every object offered.
    struct Seen {
        int prompts = 0;
        std::vector<GameObjectId> offered;
        bool wasOffered(GameObjectId id) const {
            return std::find(offered.begin(), offered.end(), id) != offered.end();
        }
    };
    /// "Yes" to yes/no prompts; for object prompts pick `want` (or the first
    /// option when `want` isn't offered).
    static Picker yesThen(GameObjectId want, Seen* seen = nullptr) {
        return [want, seen](const std::vector<Intent>& legal) -> Intent {
            if (seen) {
                ++seen->prompts;
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty()) seen->offered.push_back(i.chosen_objects[0]);
            }
            for (const auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == want) return i;
            return legal.empty() ? Intent{} : legal.front();
        };
    }
    /// Answer successive object prompts with `order[0]`, `order[1]`, ...
    static Picker inOrder(std::vector<GameObjectId> order, Seen* seen = nullptr) {
        auto next = std::make_shared<size_t>(0);
        return [order, seen, next](const std::vector<Intent>& legal) -> Intent {
            if (seen) {
                ++seen->prompts;
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty()) seen->offered.push_back(i.chosen_objects[0]);
            }
            GameObjectId want = order.empty() ? kInvalidId
                                              : order[std::min(*next, order.size() - 1)];
            ++*next;
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == want) return i;
            return legal.empty() ? Intent{} : legal.front();
        };
    }

    // ── object construction ──
    GameObjectId addGear(PlayerId owner, CardDefId def_id,
                         std::vector<std::string> tags = {}, int might_bonus = 0) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        obj.name = def_id != kInvalidId ? card_db.get(def_id).name : "TestGear";
        if (def_id != kInvalidId) obj.tags = card_db.get(def_id).tags;
        for (auto& t : tags) obj.tags.push_back(t);
        obj.might_bonus = might_bonus;
        obj.zone = ZoneType::Base;
        obj.location = BaseLocation{owner};
        withEquipmentCard(id);   // CR 821.1.c.4: see the fixture
        return id;
    }
    GameObjectId installBattlefieldCard(int bf, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def_id;
        obj.card_type = CardType::Battlefield;
        obj.name = card_db.get(def_id).name;
        obj.zone = ZoneType::BattlefieldZone;
        state.battlefields[bf].card_object_id = id;
        return id;
    }
    GameObjectId addLegend(PlayerId owner, CardDefId def_id, bool exhausted) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Legend;
        obj.zone = ZoneType::LegendZone;   // where a legend is (it was left in the default zone)
        obj.name = card_db.get(def_id).name;
        obj.is_exhausted = exhausted;
        state.player(owner).legend_zone = id;
        return id;
    }
    /// The (single) object named `name` that `p` controls on the board.
    const GameObject* findOnBoard(PlayerId p, const std::string& name) {
        for (const auto& [id, obj] : state.objects)
            if (obj.name == name && obj.controller == p && obj.location.has_value()) return &obj;
        return nullptr;
    }
    int countOnBoard(PlayerId p, const std::string& name) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.name == name && obj.controller == p && obj.location.has_value()) ++n;
        return n;
    }
    int runesInBase(PlayerId p) { return readyRuneCount(p) + exhaustedRuneCount(p); }

    // ── drivers ──
    /// Resolve a spell through the resume loop, passing its play-time targets
    /// on every entry (the base driveResumable always passes none).
    void driveSpell(CardDefId def_id, PlayerId controller, GameObjectId source,
                    const std::vector<GameObjectId>& targets, const Picker& picker,
                    EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.targets = targets;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        ctx.registry = &card_registry;
        card->onResolve(ctx, targets);
        int guard = 0;
        while (exec.hasPendingChoice()) {
            // ChainManager::stepResolve allows 1024 entries per resolution.
            ASSERT_LT(++guard, 1024) << "more choices than the chain's resume loop allows";
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onResolve(ctx, targets);
        }
        state.chain.resuming.reset();
    }
    /// Activate ability `index` through the resume loop.
    void driveAbility(CardDefId def_id, PlayerId controller, GameObjectId source,
                      int index, const Picker& picker, EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.is_activated_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        ctx.registry = &card_registry;
        card->onActivate(ctx, index, {});
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onActivate(ctx, index, {});
        }
        state.chain.resuming.reset();
    }
};

// ═══ A. Paying Energy inside an effect (CR 445.2 / 164.2.a) ═════════════════
// A rune's "[E]: Add [1]" is a [Reaction] ability usable whenever a player is
// told to pay (CR 445.2.c), so "you may pay [1]" is payable with a ready rune
// and no floating Energy. Every test below starts with an EMPTY rune pool.

TEST_F(RulesCardsTest, RulesCards_CR445_SunkenTemple_PaysWithAReadyRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bf_card = installBattlefieldCard(0, kSunkenTemple);
    addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);   // a [Mighty] unit here
    addToDeck(P1, kInvalidId);
    auto rune = addRune(P1, Domain::Fury);
    ASSERT_EQ(state.player(P1).rune_pool.energy, 0);

    driveResumableTrigger(kSunkenTemple, P1, bf_card, &RulesCardsTest::yes, exec,
                          TriggerType::WhenYouConquerHere);

    EXPECT_EQ(handSize(P1), 1) << "CR 445.2: [1] paid from a rune -> draw 1";
    EXPECT_TRUE(state.getObject(rune).is_exhausted) << "CR 164.2.a: the rune was exhausted for [1]";
}

TEST_F(RulesCardsTest, RulesCards_CR445_TreasureHoard_PaysWithAReadyRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bf_card = installBattlefieldCard(0, kTreasureHoard);
    auto rune = addRune(P1, Domain::Fury);

    driveResumableTrigger(kTreasureHoard, P1, bf_card, &RulesCardsTest::yes, exec,
                          TriggerType::WhenYouConquerHere);

    EXPECT_TRUE(state.getObject(rune).is_exhausted);
    ASSERT_EQ(countOnBoard(P1, "Gold"), 1) << "CR 445.2: [1] paid from a rune -> Gold token";
    const GameObject* gold = findOnBoard(P1, "Gold");
    EXPECT_TRUE(gold->is_exhausted);
    EXPECT_EQ(gold->card_def_id, kGold) << "CR 187.5: the token carries Gold's ability";
}

TEST_F(RulesCardsTest, RulesCards_CR445_HallOfLegends_PaysWithAReadyRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bf_card = installBattlefieldCard(0, kHallOfLegends);
    auto legend = addLegend(P1, kBladeDancer, /*exhausted=*/true);
    auto rune = addRune(P1, Domain::Calm);

    driveResumableTrigger(kHallOfLegends, P1, bf_card, &RulesCardsTest::yes, exec,
                          TriggerType::WhenYouConquerHere);

    EXPECT_FALSE(state.getObject(legend).is_exhausted) << "CR 445.2: paid [1] -> legend readied";
    EXPECT_TRUE(state.getObject(rune).is_exhausted);
}

TEST_F(RulesCardsTest, RulesCards_CR445_RippersBay_PaysWithAReadyRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bf_card = installBattlefieldCard(0, kRippersBay);
    auto rune = addRune(P1, Domain::Chaos);
    // One rune left in the Rune Deck to channel.
    auto deck_rune = state.createObject();
    {
        auto& r = state.getObject(deck_rune);
        r.owner = P1;
        r.controller = P1;
        r.card_type = CardType::Rune;
        r.name = "Chaos Rune";
        r.domains = {Domain::Chaos};
        r.zone = ZoneType::RuneDeck;
        state.player(P1).rune_deck.push_back(deck_rune);
    }

    driveResumableTrigger(kRippersBay, P1, bf_card, &RulesCardsTest::yes, exec,
                          TriggerType::WhenAUnitReturnsToHandHere);

    EXPECT_TRUE(state.getObject(rune).is_exhausted) << "CR 164.2.a: exhausted for the [1]";
    EXPECT_TRUE(state.player(P1).rune_deck.empty()) << "CR 445.2: paid -> channel 1 rune";
    EXPECT_TRUE(state.getObject(deck_rune).location.has_value());
    EXPECT_TRUE(state.getObject(deck_rune).is_exhausted) << "channeled exhausted";
}

TEST_F(RulesCardsTest, RulesCards_CR445_SpiritWheel_PaysWithAReadyRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto wheel = addGear(P1, kSpiritWheel);
    addToDeck(P1, kInvalidId);
    auto rune = addRune(P1, Domain::Calm);

    driveResumableTrigger(kSpiritWheel, P1, wheel, &RulesCardsTest::yes, exec,
                          TriggerType::WhenYouChooseAFriendlyUnit);

    EXPECT_EQ(handSize(P1), 1) << "CR 445.2: [1] paid from a rune -> draw 1";
    EXPECT_TRUE(state.getObject(wheel).is_exhausted) << "\"and exhaust this\"";
    EXPECT_TRUE(state.getObject(rune).is_exhausted);
}

TEST_F(RulesCardsTest, RulesCards_CR445_BloodRose_PaysWithAReadyRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto rose = addGear(P1, kBloodRose);
    auto rune = addRune(P1, Domain::Fury);

    driveResumableTrigger(kBloodRose, P1, rose, &RulesCardsTest::yes, exec,
                          TriggerType::WhenYouPlayAUnit);

    EXPECT_EQ(state.player(P1).xp, 1) << "CR 445.2: [1] paid from a rune -> 1 XP";
    EXPECT_TRUE(state.getObject(rune).is_exhausted);
}

TEST_F(RulesCardsTest, RulesCards_CR445_BloodRose_PaymentStaysOptional) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto rose = addGear(P1, kBloodRose);
    auto rune = addRune(P1, Domain::Fury);

    driveResumableTrigger(kBloodRose, P1, rose, &RulesCardsTest::no, exec,
                          TriggerType::WhenYouPlayAUnit);

    EXPECT_EQ(state.player(P1).xp, 0) << "CR 445.2: declining to pay negates the instruction";
    EXPECT_FALSE(state.getObject(rune).is_exhausted) << "nothing is paid on a decline";
}

TEST_F(RulesCardsTest, RulesCards_CR445_BloodRose_NotOfferedWithNothingToPayWith) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto rose = addGear(P1, kBloodRose);
    addRune(P1, Domain::Fury, /*exhausted=*/true);   // an exhausted rune adds no Energy
    Seen seen;

    driveResumableTrigger(kBloodRose, P1, rose, yesThen(kInvalidId, &seen), exec,
                          TriggerType::WhenYouPlayAUnit);

    EXPECT_EQ(seen.prompts, 0) << "CR 445.2: no resource to pay -> no payment offered";
    EXPECT_EQ(state.player(P1).xp, 0);
}

TEST_F(RulesCardsTest, RulesCards_CR445_BladeDancer_ConquerPaysWithAReadyRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto legend = addLegend(P1, kBladeDancer, /*exhausted=*/true);
    auto rune = addRune(P1, Domain::Calm);

    driveResumableTrigger(kBladeDancer, P1, legend, &RulesCardsTest::yes, exec,
                          TriggerType::WhenIConquer);

    EXPECT_FALSE(state.getObject(legend).is_exhausted) << "CR 445.2: paid [1] -> ready me";
    EXPECT_TRUE(state.getObject(rune).is_exhausted);
}

TEST_F(RulesCardsTest, RulesCards_CR164_BladeDancer_PaysPowerByRecyclingARune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto legend = addLegend(P1, kBladeDancer, /*exhausted=*/false);
    auto unit = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    state.getObject(unit).is_exhausted = true;
    auto rune = addRune(P1, Domain::Chaos);
    ASSERT_EQ(state.player(P1).rune_pool.totalPower(), 0);

    // "... to ready IT": the unit readied is the one that was chosen, which
    // the trigger carries as ChainItem::triggering_subject. The fixture's
    // driveResumableTrigger sets no subject, so the loop is run here.
    {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = P1;
        ri.is_ability = true;
        ri.source = legend;
        ri.card_def_id = kBladeDancer;
        ri.fired_trigger = TriggerType::WhenYouChooseAFriendlyUnit;
        ri.triggering_subject = unit;
        state.chain.resuming = ri;
        Card* card = card_registry.get(kBladeDancer);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, P1, legend};
        ctx.firing_trigger = TriggerType::WhenYouChooseAFriendlyUnit;
        card->onTrigger(ctx, {});
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(yes(pending.legal));
            card->onTrigger(ctx, {});
        }
        state.chain.resuming.reset();
    }

    EXPECT_FALSE(state.getObject(unit).is_exhausted) << "paid [A] -> ready the unit";
    EXPECT_TRUE(state.getObject(legend).is_exhausted) << "\"exhaust me\"";
    EXPECT_FALSE(state.getObject(rune).location.has_value())
        << "CR 164.2.b: [A] paid by recycling a rune, not only from floating power";
    EXPECT_EQ(runesInBase(P1), 0) << "exactly one rune recycled, once";
}

TEST_F(RulesCardsTest, RulesCards_CR445_VayneHunter_BothPrintings_PayWithAReadyRune) {
    for (CardDefId vayne_id : {kVayneHunter35, kVayneHunter537}) {
        SCOPED_TRACE("Vayne, Hunter id " + std::to_string(vayne_id));
        EffectExecutor exec(state, events, card_db, &card_registry);
        auto vayne = addUnit(P1, vayne_id, 0, /*at_bf=*/0);
        auto rune = addRune(P1, Domain::Fury);

        driveResumableTrigger(vayne_id, P1, vayne, &RulesCardsTest::yes, exec,
                              TriggerType::WhenIConquer);

        EXPECT_TRUE(inHand(P1, vayne)) << "CR 445.2: [1] paid from a rune -> return to hand";
        EXPECT_TRUE(state.getObject(rune).is_exhausted);
    }
}

TEST_F(RulesCardsTest, RulesCards_CR445_DianaLunari_PaysWithAReadyRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto diana = addUnit(P1, kDianaLunari, 0, /*at_bf=*/1);
    auto rune = addRune(P1, Domain::Mind);
    Seen seen;

    // Empty Main Deck: the Predict / reveal part has nothing to look at, which
    // isolates the payment.
    driveResumableTrigger(kDianaLunari, P1, diana, yesThen(kInvalidId, &seen), exec,
                          TriggerType::WhenAShowdownBeginsHere);

    EXPECT_EQ(seen.prompts, 1) << "CR 445.2: the payment is offered with only a ready rune";
    EXPECT_TRUE(state.getObject(rune).is_exhausted) << "CR 164.2.a";
}

TEST_F(RulesCardsTest, RulesCards_CR445_JaxUnrelenting_RiderPaysWithAReadyRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto jax = addUnit(P1, kJaxUnrelenting, 3, /*at_bf=*/0);
    auto gear = addGear(P1, kInvalidId, {"Equipment"}, /*might_bonus=*/2);
    addToDeck(P1, kInvalidId);
    auto rune = addRune(P1, Domain::Fury);

    driveResumableTrigger(kJaxUnrelenting, P1, jax, &RulesCardsTest::yes, exec,
                          TriggerType::WhenYouPlayMe);
    // "When you attach an Equipment to me, you may pay [1] to draw 1" is its
    // own triggered ability (CR 383), fired by the attach — not a rider run
    // inside the play trigger.
    driveResumableTrigger(kJaxUnrelenting, P1, jax, &RulesCardsTest::yes, exec,
                          TriggerType::WhenEquipmentAttachedToMe);

    EXPECT_EQ(state.getObject(gear).attached_to.value_or(kInvalidId), jax);
    EXPECT_EQ(handSize(P1), 1) << "CR 445.2: [1] paid from a rune -> draw 1";
    EXPECT_TRUE(state.getObject(rune).is_exhausted);
}

TEST_F(RulesCardsTest, RulesCards_CR445_AncientHenge_PaysAnyAmountFromRunes) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto henge = addGear(P1, kAncientHenge);
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Fury);
    int max_offered = -1;
    Picker pick_two = [&](const std::vector<Intent>& legal) -> Intent {
        Intent out = legal.empty() ? Intent{} : legal.front();
        for (const auto& i : legal) {
            if (!i.chosen_value.has_value()) continue;
            max_offered = std::max(max_offered, *i.chosen_value);
            if (*i.chosen_value == 2) out = i;
        }
        return out;
    };

    driveAbility(kAncientHenge, P1, henge, 0, pick_two, exec);

    EXPECT_EQ(max_offered, 3) << "CR 445.2: X ranges over Energy payable from ready runes";
    EXPECT_EQ(exhaustedRuneCount(P1), 2) << "CR 164.2.a: two runes exhausted for X = 2";
    EXPECT_EQ(readyRuneCount(P1), 1);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 2) << "[Add] that much [A]";
}

// ═══ D. A cost inside a resumable effect is paid exactly once ═══════════════
// confirmOptional answers 1 again on every later entry and pickTarget suspends
// the trigger, so a payment made between the two used to be charged twice.

TEST_F(RulesCardsTest, RulesCards_CR445_IcevaleArcher_PaysOnceFromRunes) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto archer = addUnit(P1, kIcevaleArcher, 0, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    addRune(P1, Domain::Order);
    addRune(P1, Domain::Order);

    driveResumableTrigger(kIcevaleArcher, P1, archer, yesThen(enemy), exec,
                          TriggerType::WhenIAttack);

    EXPECT_EQ(state.getObject(enemy).current_might, 2) << "-1 [M] this turn";
    EXPECT_EQ(exhaustedRuneCount(P1), 1) << "CR 445.2: [1] is paid once, not once per entry";
    EXPECT_EQ(readyRuneCount(P1), 1);
}

TEST_F(RulesCardsTest, RulesCards_CR445_SinisterPoro_PaysOnceFromRunes) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto poro = addUnit(P1, kSinisterPoro, 0, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    addRune(P1, Domain::Chaos);
    addRune(P1, Domain::Chaos);

    driveResumableTrigger(kSinisterPoro, P1, poro, yesThen(enemy), exec,
                          TriggerType::WhenIAttack);

    EXPECT_TRUE(state.getObject(enemy).isAtBase()) << "moved to its base";
    EXPECT_EQ(exhaustedRuneCount(P1), 1) << "CR 445.2: [1] is paid once, not once per entry";
    EXPECT_EQ(readyRuneCount(P1), 1);
}

TEST_F(RulesCardsTest, RulesCards_CR445_AvaAchiever_PaysPowerExactlyOnce) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ava = addUnit(P1, kAvaAchiever, 0, /*at_bf=*/0);
    auto hidden = addToHand(P1, kTornadoWarrior);
    ASSERT_TRUE(state.getObject(hidden).hasKeyword(Keyword::Hidden));
    addRune(P1, Domain::Mind);
    addRune(P1, Domain::Mind);

    driveResumableTrigger(kAvaAchiever, P1, ava, yesThen(hidden), exec,
                          TriggerType::WhenIAttack);

    EXPECT_FALSE(inHand(P1, hidden));
    EXPECT_EQ(state.getObject(hidden).battlefieldId().value_or(99), 0u)
        << "\"If it's a unit, play it here.\"";
    EXPECT_EQ(runesInBase(P1), 1)
        << "CR 445.2: [C] is paid once — one Mind rune recycled, not two";
}

TEST_F(RulesCardsTest, RulesCards_CR445_AvaAchiever_SpendsFloatingPowerFirst) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ava = addUnit(P1, kAvaAchiever, 0, /*at_bf=*/0);
    auto hidden = addToHand(P1, kTornadoWarrior);
    addRune(P1, Domain::Mind);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;

    driveResumableTrigger(kAvaAchiever, P1, ava, yesThen(hidden), exec,
                          TriggerType::WhenIAttack);

    EXPECT_FALSE(inHand(P1, hidden));
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)], 0)
        << "CR 445.1: paying removes the resource from the Rune Pool";
    EXPECT_EQ(runesInBase(P1), 1) << "no rune recycled while floating power covers [C]";
}

TEST_F(RulesCardsTest, RulesCards_CR445_AvaAchiever_NotOfferedWhenPowerCantBePaid) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ava = addUnit(P1, kAvaAchiever, 0, /*at_bf=*/0);
    auto hidden = addToHand(P1, kTornadoWarrior);
    addRune(P1, Domain::Fury);   // wrong Domain for [C] (Mind)
    Seen seen;

    driveResumableTrigger(kAvaAchiever, P1, ava, yesThen(hidden, &seen), exec,
                          TriggerType::WhenIAttack);

    EXPECT_EQ(seen.prompts, 0) << "CR 445.2: no Mind power available -> nothing to offer";
    EXPECT_TRUE(inHand(P1, hidden));
    EXPECT_EQ(runesInBase(P1), 1);
}

// ═══ B. Split damage (CR 355.14) ════════════════════════════════════════════

TEST_F(RulesCardsTest, RulesCards_CR355_14_AlphaStrike_ControllerMayPutAllDamageOnOneUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto striker = addUnit(P1, kInvalidId, 4, /*at_bf=*/-1);
    auto e1 = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    auto e2 = addUnit(P2, kInvalidId, 5, /*at_bf=*/1);
    auto in_base = addUnit(P2, kInvalidId, 2, /*at_bf=*/-1);   // not at a battlefield
    auto spell = addToHand(P1, kAlphaStrike);
    Seen seen;

    // Through the engine: the text after "do this" is a reflexive trigger (CR
    // 387-389), a chain item of its own that only the engine queues and resolves.
    {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = spell;
        play.targets = {striker};
        const auto& def = card_db.get(state.getObject(spell).card_def_id);
        state.player(P1).rune_pool.energy += def.energy_cost;
        state.player(P1).rune_pool.universal_power += def.power_cost;
        EnginePathAgent engine_agent(yesThen(e1, &seen));
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); }, &engine_agent);
    }

    // One target, then "No more targets": a single target takes everything, so
    // there is no division to ask about. (4 prompts, one per point, before
    // the targets were chosen up front — plans/09 item 9.M4.)
    EXPECT_EQ(seen.prompts, 2) << "CR 355.14.b: the target, and that there are no more";
    EXPECT_FALSE(seen.wasOffered(in_base)) << "\"enemy units at battlefields\" only";
    EXPECT_TRUE(inTrash(P2, e1))
        << "CR 355.14: all 4 damage on the 3 [M] unit kills it (an even spread would not)";
    EXPECT_EQ(state.getObject(e2).damage_marked, 0)
        << "CR 355.14.c: fewer units than damage may be chosen";
    EXPECT_EQ(state.player(P1).xp, 1) << "\"for each unit this kills ... Gain 1 XP\"";
}

TEST_F(RulesCardsTest, RulesCards_CR355_14_AlphaStrike_DivisionFollowsTheControllersPicks) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto striker = addUnit(P1, kInvalidId, 4, /*at_bf=*/-1);
    auto e1 = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    auto e2 = addUnit(P2, kInvalidId, 5, /*at_bf=*/1);
    auto spell = addToHand(P1, kAlphaStrike);

    driveSpell(kAlphaStrike, P1, spell, {striker}, inOrder({e1, e2, e2, e2}), exec);

    EXPECT_EQ(state.getObject(e1).damage_marked, 1) << "CR 355.14.f-g: at least 1 each";
    EXPECT_EQ(state.getObject(e2).damage_marked, 3) << "CR 355.14.e: 1 + 3, as chosen";
    EXPECT_EQ(state.player(P1).xp, 0) << "nothing died";
}

TEST_F(RulesCardsTest, RulesCards_CR355_14_AlphaStrike_CantChooseUntargetableUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto striker = addUnit(P1, kInvalidId, 3, /*at_bf=*/-1);
    auto e1 = addUnit(P2, kInvalidId, 6, /*at_bf=*/0);
    auto shrouded = addUnit(P2, kInvalidId, 6, /*at_bf=*/0);
    state.getObject(shrouded).untargetable_by_enemy = true;
    auto spell = addToHand(P1, kAlphaStrike);
    Seen seen;

    driveSpell(kAlphaStrike, P1, spell, {striker}, yesThen(shrouded, &seen), exec);

    EXPECT_FALSE(seen.wasOffered(shrouded))
        << "CR 355.14.a: split units are targets, so one that can't be chosen is not legal";
    EXPECT_EQ(state.getObject(shrouded).damage_marked, 0);
    EXPECT_EQ(state.getObject(e1).damage_marked, 3);
}

TEST_F(RulesCardsTest, RulesCards_CR355_14_AlphaStrike_LargeAmountStaysWithinTheResumeLoop) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto striker = addUnit(P1, kInvalidId, 24, /*at_bf=*/-1);
    auto e1 = addUnit(P2, kInvalidId, 40, /*at_bf=*/0);
    auto e2 = addUnit(P2, kInvalidId, 40, /*at_bf=*/1);
    auto spell = addToHand(P1, kAlphaStrike);
    Seen seen;

    // One point per pick, whatever the total: 24 damage is 24 picks, well
    // inside the resume loop's 1024. (This asserted "at most 12 prompts" while
    // splitDamageAmong capped its picks for the old 16-entry loop and made
    // each pick worth 2 — plans/09 item 9.D3.)
    driveSpell(kAlphaStrike, P1, spell, {striker}, inOrder({e1, e2}, &seen), exec);

    EXPECT_GE(seen.prompts, 24);
    EXPECT_LE(seen.prompts, 26);
    EXPECT_EQ(state.getObject(e1).damage_marked + state.getObject(e2).damage_marked, 24)
        << "CR 355.14: all of the damage is divided, none lost";
    EXPECT_GT(state.getObject(e1).damage_marked, 0);
    EXPECT_GT(state.getObject(e2).damage_marked, 0);
}

TEST_F(RulesCardsTest, RulesCards_CR355_14_Volibear_ControllerChoosesAmongEnemyUnitsHere) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto voli = addUnit(P1, kVolibearFurious, 0, /*at_bf=*/0);
    auto big = addUnit(P2, kInvalidId, 6, /*at_bf=*/0);
    auto small = addUnit(P2, kInvalidId, 2, /*at_bf=*/0);
    auto elsewhere = addUnit(P2, kInvalidId, 2, /*at_bf=*/1);
    auto shrouded = addUnit(P2, kInvalidId, 2, /*at_bf=*/0);
    state.getObject(shrouded).untargetable_by_enemy = true;
    Seen seen;

    driveResumableTrigger(kVolibearFurious, P1, voli, yesThen(big, &seen), exec,
                          TriggerType::WhenIAttack);

    EXPECT_EQ(seen.prompts, 2) << "the target, \"No more targets\" (was one prompt per point)";
    EXPECT_EQ(state.getObject(big).damage_marked, 5)
        << "CR 355.14.e: all 5 where the controller put them";
    EXPECT_EQ(state.getObject(small).damage_marked, 0)
        << "\"any number of enemy units here\" — an even spread is not forced";
    EXPECT_FALSE(seen.wasOffered(elsewhere)) << "\"here\"";
    EXPECT_FALSE(seen.wasOffered(shrouded)) << "can't be chosen by enemy spells and abilities";
    EXPECT_FALSE(seen.wasOffered(voli));
}

TEST_F(RulesCardsTest, RulesCards_CR355_14_Volibear_SingleEnemyTakesAllInOnePick) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto voli = addUnit(P1, kVolibearFurious, 0, /*at_bf=*/0);
    auto only = addUnit(P2, kInvalidId, 9, /*at_bf=*/0);
    Seen seen;

    driveResumableTrigger(kVolibearFurious, P1, voli, yesThen(only, &seen), exec,
                          TriggerType::WhenIAttack);

    EXPECT_EQ(seen.prompts, 1);
    EXPECT_EQ(state.getObject(only).damage_marked, 5);
}

// ═══ C. [A] is power of any Domain ══════════════════════════════════════════

TEST_F(RulesCardsTest, RulesCards_CR135_Shadow_AbilityPowerIsAnyDomain) {
    const Card* shadow = card_registry.get(kShadow);
    ASSERT_NE(shadow, nullptr);
    auto abs = shadow->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].cost.exhaust);
    EXPECT_EQ(abs[0].cost.energy, 1);
    EXPECT_EQ(abs[0].cost.power, 1);
    EXPECT_TRUE(abs[0].cost.power_any_domain)
        << "[1][A]: the power may be of any Domain, not Fury only";
}

// ═══ E. "When you play me from face down" ═══════════════════════════════════

TEST_F(RulesCardsTest, RulesCards_Evelynn_DoesNotTriggerWhenPlayedFromHand) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto eve = addUnit(P1, kEvelynn, 2, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/1);
    state.turn.turn_player = P1;
    Seen seen;

    driveResumableTrigger(kEvelynn, P1, eve, yesThen(enemy, &seen), exec,
                          TriggerType::WhenYouPlayMe);

    EXPECT_EQ(seen.prompts, 0) << "\"from face down\": a play from hand does not qualify";
    EXPECT_EQ(state.getObject(enemy).battlefieldId().value_or(99), 1u);
}

TEST_F(RulesCardsTest, RulesCards_Evelynn_TriggersWhenPlayedFromFaceDownOnYourTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto eve = addUnit(P1, kEvelynn, 2, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/1);
    state.turn.turn_player = P1;
    state.getObject(eve).card_counters["__played_from_facedown"] = 1;

    driveResumableTrigger(kEvelynn, P1, eve, yesThen(enemy), exec,
                          TriggerType::WhenYouPlayMe);

    EXPECT_EQ(state.getObject(enemy).battlefieldId().value_or(99), 0u)
        << "moved to Evelynn's battlefield";
    EXPECT_EQ(state.getObject(eve).card_counters["__played_from_facedown"], 0)
        << "the marker is consumed so a later play from hand doesn't reuse it";
}

TEST_F(RulesCardsTest, RulesCards_Evelynn_NotOnAnOpponentsTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto eve = addUnit(P1, kEvelynn, 2, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/1);
    state.turn.turn_player = P2;
    state.getObject(eve).card_counters["__played_from_facedown"] = 1;
    Seen seen;

    driveResumableTrigger(kEvelynn, P1, eve, yesThen(enemy, &seen), exec,
                          TriggerType::WhenYouPlayMe);

    EXPECT_EQ(seen.prompts, 0) << "\"on your turn\"";
    EXPECT_EQ(state.getObject(enemy).battlefieldId().value_or(99), 1u);
}

TEST_F(RulesCardsTest, RulesCards_Evelynn_CantChooseUntargetableEnemy) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto eve = addUnit(P1, kEvelynn, 2, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/1);
    state.getObject(enemy).untargetable_by_enemy = true;
    state.turn.turn_player = P1;
    state.getObject(eve).card_counters["__played_from_facedown"] = 1;
    Seen seen;

    driveResumableTrigger(kEvelynn, P1, eve, yesThen(enemy, &seen), exec,
                          TriggerType::WhenYouPlayMe);

    EXPECT_EQ(seen.prompts, 0) << "the only enemy can't be chosen by enemy abilities";
    EXPECT_EQ(state.getObject(enemy).battlefieldId().value_or(99), 1u);
}

// ═══ F. Token characteristics (CR 187) ══════════════════════════════════════

TEST_F(RulesCardsTest, RulesCards_CR187_3_Arise_SandSoldierHasShurimaTag) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addGear(P1, kInvalidId, {"Equipment"});
    auto spell = addToHand(P1, kArise);

    invokeOnResolve(spell, kArise, P1, {}, exec);

    ASSERT_EQ(countOnBoard(P1, "Sand Soldier"), 1);
    const GameObject* soldier = findOnBoard(P1, "Sand Soldier");
    EXPECT_TRUE(soldier->isToken());
    EXPECT_EQ(soldier->base_might, 2) << "CR 187.3: 2 Might";
    EXPECT_EQ(soldier->tags, std::vector<std::string>{"Shurima"})
        << "CR 187.3: the Shurima tag (\"Sand Soldier\" is the token's name)";
    EXPECT_TRUE(soldier->domains.empty()) << "CR 187.3: domainless";
}

TEST_F(RulesCardsTest, RulesCards_CR187_3_EmperorOfTheSands_RecognisesSandSoldiersByName) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addLegend(P1, kEmperorOfSands, /*exhausted=*/false);
    auto soldier = exec.createToken(P1, CardType::Unit, "Sand Soldier", 2, {"Shurima"},
                                    KeywordSet{}, BaseLocation{P1}, false);
    auto other = addUnit(P1, kInvalidId, 2, /*at_bf=*/-1);
    state.getObject(other).tags = {"Shurima"};   // Shurima, but not a Sand Soldier
    const Card* emperor = card_registry.get(kEmperorOfSands);
    ASSERT_NE(emperor, nullptr);

    emperor->applyPassiveAura(state, P1);

    auto hasWeaponmasterAura = [&](GameObjectId id) {
        for (const auto& ae : state.getObject(id).aura_effects)
            if (ae.keyword == Keyword::Weaponmaster) return true;
        return false;
    };
    EXPECT_TRUE(hasWeaponmasterAura(soldier)) << "\"Your Sand Soldiers have [Weaponmaster].\"";
    EXPECT_FALSE(hasWeaponmasterAura(other)) << "the Shurima tag alone is not a Sand Soldier";
}

TEST_F(RulesCardsTest, RulesCards_CR187_4_ProductionSurge_MechHasMechTag) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    addToDeck(P1, kInvalidId);
    auto spell = addToHand(P1, kProductionSurge);

    invokeOnResolve(spell, kProductionSurge, P1, {}, exec);

    ASSERT_EQ(countOnBoard(P1, "Mech"), 1);
    const GameObject* mech = findOnBoard(P1, "Mech");
    EXPECT_EQ(mech->base_might, 3) << "CR 187.4: 3 Might";
    EXPECT_EQ(mech->tags, std::vector<std::string>{"Mech"}) << "CR 187.4: the Mech tag";
}

TEST_F(RulesCardsTest, RulesCards_CR187_10_Illaoi_TentacleHasOnlyBilgewaterTag) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto illaoi = addUnit(P1, kIllaoiProphet, 0, /*at_bf=*/0);

    fireTriggerAs(kIllaoiProphet, P1, illaoi, TriggerType::WhenYouPlayMe, exec);

    ASSERT_EQ(countOnBoard(P1, "Tentacle"), 1);
    const GameObject* tentacle = findOnBoard(P1, "Tentacle");
    EXPECT_EQ(tentacle->base_might, 1) << "CR 187.10: 1 Might";
    EXPECT_EQ(tentacle->tags, std::vector<std::string>{"Bilgewater"})
        << "CR 187.10: the Bilgewater tag";
}

TEST_F(RulesCardsTest, RulesCards_CR187_11_ZedWithoutASound_ShadowCloneHasNoTags) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto zed = addUnit(P1, kZedWithoutASound, 0, /*at_bf=*/0);

    fireTriggerAs(kZedWithoutASound, P1, zed, TriggerType::WhenIConquer, exec);

    ASSERT_EQ(countOnBoard(P1, "Shadow Clone"), 1);
    const GameObject* clone = findOnBoard(P1, "Shadow Clone");
    EXPECT_EQ(clone->base_might, 0) << "CR 187.11: 0 Might";
    EXPECT_TRUE(clone->tags.empty()) << "CR 187.11 lists no tag for a Shadow Clone";
}

TEST_F(RulesCardsTest, RulesCards_CR187_5_Cull_GoldTokenHasItsAbilityAndEntersExhausted) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto cull = addGear(P1, kCull);
    Card* card = card_registry.get(kCull);
    ASSERT_NE(card, nullptr);
    CardContext ctx{state, events, exec, P1, cull};

    card->onEquippedTrigger(ctx, unit, {});

    ASSERT_EQ(countOnBoard(P1, "Gold"), 1);
    const GameObject* gold = findOnBoard(P1, "Gold");
    EXPECT_TRUE(gold->isGear());
    EXPECT_EQ(gold->card_def_id, kGold)
        << "CR 187.5: \"[Reaction][>] Kill this, [E]: [Add] [A]\" comes with the token";
    EXPECT_TRUE(gold->tags.empty());
    EXPECT_TRUE(gold->is_exhausted) << "\"play a Gold gear token exhausted\"";
    const Card* gold_card = card_registry.get(gold->card_def_id);
    ASSERT_NE(gold_card, nullptr);
    EXPECT_TRUE(gold_card->hasActivatedAbility());
}

TEST_F(RulesCardsTest, RulesCards_CR187_5_DeadlyFlourish_GoldTokenHasItsAbilityAndEntersExhausted) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto victim = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);

    // The delayed "when it dies this turn" ability resolving for P1.
    fireTriggerAs(kDeadlyFlourish, P1, victim, TriggerType::WhenIDie, exec);

    ASSERT_EQ(countOnBoard(P1, "Gold"), 1);
    const GameObject* gold = findOnBoard(P1, "Gold");
    EXPECT_EQ(gold->card_def_id, kGold) << "CR 187.5";
    EXPECT_TRUE(gold->is_exhausted) << "\"play a Gold gear token exhausted\"";
}

// ═══ G. "Can't be chosen by enemy spells and abilities" ═════════════════════

TEST_F(RulesCardsTest, RulesCards_Untargetable_CustomTargetListsSkipProtectedEnemies) {
    auto open = addUnit(P2, kInvalidId, 2, /*at_bf=*/0);
    auto shrouded = addUnit(P2, kInvalidId, 2, /*at_bf=*/0);
    state.getObject(open).tags = {"Poro"};
    state.getObject(shrouded).tags = {"Poro"};
    state.getObject(shrouded).untargetable_by_enemy = true;

    // Every card whose enumerateLegalTargets override can return enemy units.
    const std::vector<CardDefId> cards = {
        kIronBallista, kCaitlynPatrolling, kGust, kFadingMemories, kCounterStrike,
        kHostileTakeover, kXerathFreed, kTheList, kDiviningShells,
        // Vi, Peacekeeper builds her list in her trigger (07 item 7.1); covered by
        // C7Test.ViPeacekeeper_StunsTheEnemyUnitHereItsControllerChooses.
        kVoidAssault, kKeepersVerdict, kVoidreaver,
    };
    for (CardDefId def_id : cards) {
        const Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr) << def_id;
        SCOPED_TRACE(card->def().name);
        // The indexed overload falls through to the plain one by default;
        // index 0 is each of these cards' unit-choosing ability.
        auto enemy_view = card->enumerateLegalTargets(state, P1, 0);
        auto has = [&](const std::vector<GameObjectId>& v, GameObjectId id) {
            return std::find(v.begin(), v.end(), id) != v.end();
        };
        EXPECT_TRUE(has(enemy_view, open)) << "an unprotected enemy unit is a legal choice";
        EXPECT_FALSE(has(enemy_view, shrouded))
            << "a unit that can't be chosen by enemy spells and abilities was offered";
    }
}

TEST_F(RulesCardsTest, RulesCards_Untargetable_ControllerCanStillChooseItsOwnUnit) {
    auto shrouded = addUnit(P2, kInvalidId, 2, /*at_bf=*/0);
    state.getObject(shrouded).untargetable_by_enemy = true;

    for (CardDefId def_id : {kCounterStrike, kGust, kDiviningShells}) {
        const Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        SCOPED_TRACE(card->def().name);
        auto own_view = card->enumerateLegalTargets(state, P2, 0);
        EXPECT_NE(std::find(own_view.begin(), own_view.end(), shrouded), own_view.end())
            << "the protection only stops ENEMY spells and abilities";
    }
}

// ═══ H. Mirror Image ════════════════════════════════════════════════════════

TEST_F(RulesCardsTest, RulesCards_CR816_MirrorImage_TokenInControllersBaseWithLastingTemporary) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto original = addUnit(P2, kIcevaleArcher, 0, /*at_bf=*/0);
    auto spell = addToHand(P1, kMirrorImage);

    // Through the engine: the text after "do this" is a reflexive trigger (CR
    // 387-389), a chain item of its own that only the engine queues and resolves.
    {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = spell;
        play.targets = {original};
        const auto& def = card_db.get(state.getObject(spell).card_def_id);
        state.player(P1).rune_pool.energy += def.energy_cost;
        state.player(P1).rune_pool.universal_power += def.power_cost;
        EnginePathAgent engine_agent(EnginePathAgent::Picker{});
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); }, &engine_agent);
    }

    const GameObject* token = nullptr;
    for (const auto& [id, obj] : state.objects)
        if (obj.isToken() && obj.controller == P1) token = &obj;
    ASSERT_NE(token, nullptr);
    ASSERT_TRUE(token->location.has_value());
    EXPECT_TRUE(*token->location == LocationId{BaseLocation{P1}})
        << "\"to your base\": the controller's base, not a base with no player";
    EXPECT_FALSE(token->is_exhausted) << "\"a ready Reflection unit token\"";
    EXPECT_EQ(token->card_def_id, kIcevaleArcher) << "it became a copy of the chosen unit";
    EXPECT_TRUE(token->keywords.has(Keyword::Temporary)) << "\"Give it [Temporary].\"";
    EXPECT_FALSE(token->temp_keywords.has(Keyword::Temporary))
        << "CR 816: [Temporary] must outlive the turn — a this-turn grant is revoked in the "
           "expiration step, before the Beginning Phase that would kill the token";
}

}  // namespace
}  // namespace test_rules_compliance_cards
}  // namespace riftbound::test
