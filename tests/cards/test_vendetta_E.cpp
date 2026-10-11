/// @file test_vendetta_E.cpp
/// Vendetta batch E: 19 spells + 14 gear. Each test drives the card's
/// onResolve / onTrigger / onActivate / onEquip / applyPassiveAura directly.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace riftbound::test {
namespace {

// Reference cards from outside the batch.
constexpr CardDefId kPunchingPoro = 794;   // 2 [M] unit
constexpr CardDefId kDredgeUp     = 836;   // [2] spell

// Batch E spells.
constexpr CardDefId kCataclysmicDuel  = 877;
constexpr CardDefId kUpFromTheDeep    = 887;
constexpr CardDefId kShadowsOfThePast = 890;
constexpr CardDefId kTwilightStep     = 892;
constexpr CardDefId kWindAndGhosts    = 893;
constexpr CardDefId kDecreeOfDiscord  = 894;
constexpr CardDefId kDragonForm       = 903;
constexpr CardDefId kKiBarrier        = 913;
constexpr CardDefId kLacerate         = 914;
constexpr CardDefId kDecreeOfUnity    = 918;
constexpr CardDefId kShurikenFlip     = 927;
constexpr CardDefId kDominus          = 929;
constexpr CardDefId kDeathMark        = 931;
constexpr CardDefId kSiphoningStrike  = 933;
constexpr CardDefId kShadowDash       = 935;
constexpr CardDefId kAccelerationGate = 937;
constexpr CardDefId kRebuttal         = 939;
constexpr CardDefId kPublicExecution  = 941;
constexpr CardDefId kLightningRush    = 943;

// Batch E gear.
constexpr CardDefId kPendulumBlade       = 798;
constexpr CardDefId kEndlessRiches       = 809;
constexpr CardDefId kHandHammer          = 814;
constexpr CardDefId kHelmOfSuppression   = 832;
constexpr CardDefId kQuestionableTome    = 841;
constexpr CardDefId kHextechFormula      = 849;
constexpr CardDefId kBottledConstellation = 854;
constexpr CardDefId kJaggedCutlass       = 860;
constexpr CardDefId kPlatewyrmEgg        = 862;
constexpr CardDefId kToolsOfEmpire       = 864;
constexpr CardDefId kHextechDisc         = 874;
constexpr CardDefId kForgottenRelic      = 895;
constexpr CardDefId kGlowstone           = 920;
constexpr CardDefId kShadySpectacles     = 924;

using Picker = std::function<Intent(const std::vector<Intent>&)>;

// One scripted answer: a specific object, or an int-coded option.
struct Step {
    bool is_obj;
    GameObjectId obj;
    int val;
};
Step O(GameObjectId id) { return {true, id, 0}; }
Step V(int v) { return {false, kInvalidId, v}; }

/// Answers the published choices in order; fails the test if a scripted answer
/// is not offered or if the card asks for more choices than scripted.
Picker script(std::vector<Step> steps) {
    auto idx = std::make_shared<size_t>(0);
    return [steps, idx](const std::vector<Intent>& legal) {
        if (*idx >= steps.size()) {
            ADD_FAILURE() << "card asked for more choices than scripted";
            return legal.front();
        }
        const Step s = steps[(*idx)++];
        for (const auto& i : legal) {
            if (s.is_obj && !i.chosen_objects.empty() && i.chosen_objects[0] == s.obj) return i;
            if (!s.is_obj && i.chosen_objects.empty() && i.chosen_value.has_value() &&
                *i.chosen_value == s.val) return i;
        }
        ADD_FAILURE() << "scripted choice #" << *idx << " was not offered";
        return legal.front();
    };
}

bool offers(const std::vector<Intent>& legal, GameObjectId id) {
    for (const auto& i : legal)
        if (!i.chosen_objects.empty() && i.chosen_objects[0] == id) return true;
    return false;
}

int auraMightSum(const GameObject& obj) {
    int n = 0;
    for (const auto& ae : obj.aura_effects) n += ae.might_bonus;
    return n;
}

class VendettaE : public CardTestFixture {
protected:
    GameObjectId addGear(PlayerId owner, CardDefId def_id) {
        auto id = addUnit(owner, def_id);
        auto& g = state.getObject(id);
        g.card_type = CardType::Gear;
        g.base_might = 0;
        g.current_might = 0;
        if (def_id != kInvalidId) g.might_bonus = card_db.get(def_id).might_bonus;
        return id;
    }
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& ps = state.player(owner);
        ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), id), ps.hand.end());
        state.getObject(id).zone = ZoneType::Trash;
        ps.trash.push_back(id);
        return id;
    }
    GameObjectId addToRuneDeck(PlayerId owner, Domain d) {
        auto id = addRune(owner, d);
        auto& r = state.getObject(id);
        r.location = std::nullopt;
        r.zone = ZoneType::RuneDeck;
        state.player(owner).rune_deck.push_back(id);
        return id;
    }
    void attach(GameObjectId gear, GameObjectId unit) {
        auto& g = state.getObject(gear);
        auto& u = state.getObject(unit);
        g.attached_to = unit;
        g.location = u.location;
        g.zone = u.zone;
        u.attachments.push_back(gear);
        u.attachment_might_bonus += g.might_bonus;
        u.recomputeMight();
    }
    bool onBoard(GameObjectId id) { return state.getObject(id).location.has_value(); }
    bool atBattlefield(GameObjectId id, BattlefieldId bf) {
        auto at = state.getObject(id).battlefieldId();
        return at.has_value() && *at == bf;
    }
    int tokenCount(PlayerId p, const std::string& name) {
        int n = 0;
        for (auto& [id, obj] : state.objects)
            if (obj.isToken() && obj.controller == p && obj.name == name &&
                obj.location.has_value()) ++n;
        return n;
    }
    CardContext ctxFor(EffectExecutor& exec, PlayerId p, GameObjectId source) {
        return CardContext{state, events, exec, p, source};
    }
    /// Drive a (possibly resumable) activated ability through the choice loop.
    void driveActivate(CardDefId def_id, PlayerId controller, GameObjectId source,
                       const std::vector<GameObjectId>& targets, Picker picker,
                       EffectExecutor& exec, int ability_index = 0) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.is_activated_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.ability_index = ability_index;
        state.chain.resuming = ri;
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        card->onActivate(ctx, ability_index, targets);
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker ? picker(pending.legal) : pending.legal.front());
            card->onActivate(ctx, ability_index, targets);
        }
        state.chain.resuming.reset();
    }
};

// ═══════════════════════════════ Spells ═════════════════════════════════════

TEST_F(VendettaE, Vendetta_CataclysmicDuel_EachPlayerKeepsOne) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto a = addUnit(P1, kInvalidId, 2);        // base
    auto b = addUnit(P1, kInvalidId, 3, 0);
    auto c = addUnit(P2, kInvalidId, 4, 0);
    auto d = addUnit(P2, kInvalidId, 5, 1);
    auto spell = addToHand(P1, kCataclysmicDuel);

    int call = 0;
    driveResumable(kCataclysmicDuel, P1, spell, [&](const std::vector<Intent>& legal) {
        ++call;
        // 1st prompt: P1 chooses among P1's units; 2nd: P2 among P2's.
        EXPECT_EQ(legal.size(), 2u);
        EXPECT_EQ(legal.front().player, call == 1 ? P1 : P2);
        EXPECT_EQ(offers(legal, a), call == 1);
        EXPECT_EQ(offers(legal, c), call == 2);
        return script({O(call == 1 ? b : d)})(legal);
    }, exec);

    EXPECT_EQ(call, 2);
    EXPECT_FALSE(onBoard(a));
    EXPECT_TRUE(onBoard(b));
    EXPECT_FALSE(onBoard(c));
    EXPECT_TRUE(onBoard(d));
}

TEST_F(VendettaE, Vendetta_CataclysmicDuel_PlayerWithNoUnitsIsSkipped) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kCataclysmicDuel);
    driveResumable(kCataclysmicDuel, P1, spell, script({O(a)}), exec);
    EXPECT_TRUE(onBoard(a));
    EXPECT_FALSE(onBoard(b));
}

TEST_F(VendettaE, Vendetta_UpFromTheDeep_TwoTentacles) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto spell = addToHand(P1, kUpFromTheDeep);
    invokeOnResolve(spell, kUpFromTheDeep, P1, {}, exec);
    EXPECT_EQ(tokenCount(P1, "Tentacle"), 2);
    for (auto& [id, obj] : state.objects) {
        if (obj.name != "Tentacle") continue;
        EXPECT_EQ(obj.current_might, 1);
        EXPECT_TRUE(obj.isAtBase());
        ASSERT_EQ(obj.tags.size(), 1u);
        EXPECT_EQ(obj.tags[0], "Bilgewater");
    }
    auto f = card_registry.get(kUpFromTheDeep)->flowCost();
    EXPECT_TRUE(f.valid);
    EXPECT_EQ(f.energy, 3);
    EXPECT_EQ(f.power, 0);
}

TEST_F(VendettaE, Vendetta_ShadowsOfThePast_ReturnsUpToTwoFromEitherTrash) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mine    = addToTrash(P1, kPunchingPoro);
    auto theirs  = addToTrash(P2, kPunchingPoro);
    auto extra   = addToTrash(P1, kPunchingPoro);
    auto nonunit = addToTrash(P1, kDredgeUp);
    auto spell = addToHand(P1, kShadowsOfThePast);

    int call = 0;
    driveResumable(kShadowsOfThePast, P1, spell, [&](const std::vector<Intent>& legal) {
        ++call;
        EXPECT_FALSE(offers(legal, nonunit)) << "only units may be returned";
        return script({O(call == 1 ? mine : theirs)})(legal);
    }, exec);

    EXPECT_EQ(call, 2) << "up to 2 - no third prompt";
    EXPECT_TRUE(inHand(P1, mine));
    EXPECT_TRUE(inHand(P2, theirs)) << "returned to its OWNER's hand";
    EXPECT_TRUE(inTrash(P1, extra));
    EXPECT_TRUE(inTrash(P1, nonunit));
}

TEST_F(VendettaE, Vendetta_ShadowsOfThePast_MayStopEarly) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto u1 = addToTrash(P1, kPunchingPoro);
    auto u2 = addToTrash(P1, kPunchingPoro);
    auto spell = addToHand(P1, kShadowsOfThePast);
    driveResumable(kShadowsOfThePast, P1, spell, script({O(u1), V(0)}), exec);
    EXPECT_TRUE(inHand(P1, u1));
    EXPECT_TRUE(inTrash(P1, u2));
}

TEST_F(VendettaE, Vendetta_TwilightStep_MovesSmallUnitOnly) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto small = addUnit(P1, kInvalidId, 3);       // base, exactly 3 [M]
    auto big   = addUnit(P2, kInvalidId, 4, 0);
    auto spell = addToHand(P1, kTwilightStep);

    int call = 0;
    driveResumable(kTwilightStep, P1, spell, [&](const std::vector<Intent>& legal) {
        ++call;
        if (call == 1) {
            EXPECT_TRUE(offers(legal, small));
            EXPECT_FALSE(offers(legal, big)) << "4 [M] is over the limit";
            return script({O(small)})(legal);
        }
        EXPECT_EQ(legal.size(), 2u) << "already at base: only the battlefields";
        return script({V(1 + 1)})(legal);           // battlefield #1
    }, exec);

    EXPECT_EQ(call, 2);
    EXPECT_TRUE(atBattlefield(small, 1));
    EXPECT_TRUE(atBattlefield(big, 0));
    auto f = card_registry.get(kTwilightStep)->flowCost();
    EXPECT_EQ(f.energy, 4);
    EXPECT_EQ(f.power, 1);
    EXPECT_EQ(f.power_domain, Domain::Chaos);
}

TEST_F(VendettaE, Vendetta_WindAndGhosts_BanishesThreeOrLess) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P2, kPunchingPoro, 3, 0);
    addUnit(P2, kInvalidId, 1);   // in base: not "at a battlefield", not a target
    auto spell = addToHand(P1, kWindAndGhosts);
    invokeOnResolve(spell, kWindAndGhosts, P1, {}, exec);
    EXPECT_EQ(state.getObject(unit).zone, ZoneType::Banishment);
    EXPECT_FALSE(onBoard(unit));
    EXPECT_FALSE(inHand(P2, unit));
}

TEST_F(VendettaE, Vendetta_WindAndGhosts_BouncesFourOrMore) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P2, kPunchingPoro, 4, 0);
    auto spell = addToHand(P1, kWindAndGhosts);
    invokeOnResolve(spell, kWindAndGhosts, P1, {}, exec);
    EXPECT_TRUE(inHand(P2, unit));
    EXPECT_NE(state.getObject(unit).zone, ZoneType::Banishment);
}

TEST_F(VendettaE, Vendetta_DecreeOfDiscord_TotalMightFiveOrLess) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto order = [&](PlayerId p, int might) {
        auto id = addUnit(p, kPunchingPoro, might, 0);
        state.getObject(id).domains = {Domain::Order};
        return id;
    };
    auto o3 = order(P2, 3);
    auto o2 = order(P2, 2);
    auto o4 = order(P2, 4);
    auto own = order(P1, 1);
    auto other = addUnit(P2, kPunchingPoro, 1, 0);
    state.getObject(other).domains = {Domain::Fury};
    auto spell = addToHand(P1, kDecreeOfDiscord);

    int call = 0;
    driveResumable(kDecreeOfDiscord, P1, spell, [&](const std::vector<Intent>& legal) {
        ++call;
        EXPECT_FALSE(offers(legal, own)) << "enemy units only";
        EXPECT_FALSE(offers(legal, other)) << "Order units only";
        if (call == 1) {
            EXPECT_TRUE(offers(legal, o4));
            return script({O(o3)})(legal);
        }
        EXPECT_FALSE(offers(legal, o4)) << "3 + 4 would exceed 5";
        return script({O(o2)})(legal);
    }, exec);

    EXPECT_EQ(call, 2) << "budget spent: no further prompt";
    EXPECT_TRUE(inHand(P2, o3));
    EXPECT_TRUE(inHand(P2, o2));
    EXPECT_TRUE(onBoard(o4));
    EXPECT_TRUE(onBoard(own));
    EXPECT_TRUE(onBoard(other));
}

TEST_F(VendettaE, Vendetta_DragonForm_BaseMightBecomesFiveThisTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(unit).buff_count = 1;
    state.getObject(unit).recomputeMight();
    ASSERT_EQ(state.getObject(unit).current_might, 3);
    auto spell = addToHand(P1, kDragonForm);
    invokeOnResolve(spell, kDragonForm, P1, {}, exec);
    EXPECT_EQ(state.getObject(unit).current_might, 6) << "base 5 + the buff";
    EXPECT_EQ(state.getObject(unit).base_might, 2) << "printed base is restored at end of turn";
    // End of turn: the this-turn Might goes away.
    state.getObject(unit).temp_might_bonus = 0;
    state.getObject(unit).recomputeMight();
    EXPECT_EQ(state.getObject(unit).current_might, 3);
    EXPECT_EQ(card_registry.get(kDragonForm)->flowCost().energy, 3);
}

TEST_F(VendettaE, Vendetta_DragonForm_LowersABiggerUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P2, kInvalidId, 8, 0);
    auto spell = addToHand(P1, kDragonForm);
    invokeOnResolve(spell, kDragonForm, P1, {}, exec);
    EXPECT_EQ(state.getObject(unit).current_might, 5);
}

TEST_F(VendettaE, Vendetta_KiBarrier_PreventsNextSevenDamage) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 5, 0);
    auto spell = addToHand(P1, kKiBarrier);
    invokeOnResolve(spell, kKiBarrier, P1, {}, exec);
    EXPECT_EQ(state.getObject(unit).prevent_damage_remaining, 7);

    exec.dealDamage(unit, 4, spell);
    EXPECT_EQ(state.getObject(unit).damage_marked, 0);
    exec.dealDamage(unit, 5, spell);
    EXPECT_EQ(state.getObject(unit).damage_marked, 2) << "only 3 of the 7 were left";
    EXPECT_EQ(state.getObject(unit).prevent_damage_remaining, 0);
    EXPECT_TRUE(card_db.get(kKiBarrier).keywords.has(Keyword::Reaction));
}

TEST_F(VendettaE, Vendetta_Lacerate_DisempowersThenKillsThreeOrLess) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P2, kInvalidId, 3, 0);
    exec.empower(unit);
    auto spell = addToHand(P1, kLacerate);
    invokeOnResolve(spell, kLacerate, P1, {}, exec);
    EXPECT_FALSE(state.getObject(unit).isEmpowered());
    EXPECT_FALSE(onBoard(unit));
}

TEST_F(VendettaE, Vendetta_Lacerate_SparesFourMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P2, kInvalidId, 4, 0);
    exec.empower(unit);
    auto spell = addToHand(P1, kLacerate);
    invokeOnResolve(spell, kLacerate, P1, {}, exec);
    EXPECT_FALSE(state.getObject(unit).isEmpowered()) << "still disempowered";
    EXPECT_TRUE(onBoard(unit));
    auto f = card_registry.get(kLacerate)->flowCost();
    EXPECT_EQ(f.energy, 4);
    EXPECT_EQ(f.power, 2);
    EXPECT_EQ(f.power_domain, Domain::Order);
    EXPECT_FALSE(f.any_domain);
}

TEST_F(VendettaE, Vendetta_DecreeOfUnity_KillsEnemyChaosUnitOrGear) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto chaos_unit = addUnit(P2, kInvalidId, 6, 0);
    state.getObject(chaos_unit).domains = {Domain::Chaos};
    auto chaos_gear = addGear(P2, kInvalidId);
    state.getObject(chaos_gear).domains = {Domain::Chaos};
    auto fury_unit = addUnit(P2, kInvalidId, 1, 0);
    state.getObject(fury_unit).domains = {Domain::Fury};
    auto own_chaos = addUnit(P1, kInvalidId, 1, 0);
    state.getObject(own_chaos).domains = {Domain::Chaos};

    const Card* card = card_registry.get(kDecreeOfUnity);
    ASSERT_NE(card, nullptr);
    auto legal = card->enumerateLegalTargets(state, P1);
    EXPECT_EQ(legal.size(), 2u);
    EXPECT_NE(std::find(legal.begin(), legal.end(), chaos_unit), legal.end());
    EXPECT_NE(std::find(legal.begin(), legal.end(), chaos_gear), legal.end());

    auto spell = addToHand(P1, kDecreeOfUnity);
    driveResumable(kDecreeOfUnity, P1, spell, script({O(chaos_gear)}), exec);
    EXPECT_FALSE(onBoard(chaos_gear));
    EXPECT_TRUE(onBoard(chaos_unit));
    EXPECT_TRUE(onBoard(fury_unit));
    EXPECT_TRUE(onBoard(own_chaos));
}

TEST_F(VendettaE, Vendetta_DecreeOfUnity_NoTargetNotPlayable) {
    auto fury_unit = addUnit(P2, kInvalidId, 1, 0);
    state.getObject(fury_unit).domains = {Domain::Fury};
    EXPECT_FALSE(card_registry.get(kDecreeOfUnity)->hasLegalTargets(state, P1));
}

TEST_F(VendettaE, Vendetta_ShurikenFlip_DamagesThenMoves) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto enemy   = addUnit(P2, kInvalidId, 3, 0);
    auto in_base = addUnit(P2, kInvalidId, 3);       // not at a battlefield
    auto mine    = addUnit(P1, kInvalidId, 2);
    auto spell = addToHand(P1, kShurikenFlip);

    int call = 0;
    driveResumable(kShurikenFlip, P1, spell, [&](const std::vector<Intent>& legal) {
        ++call;
        if (call == 1) {
            EXPECT_FALSE(offers(legal, in_base));
            return script({O(enemy)})(legal);
        }
        if (call == 2) {
            EXPECT_FALSE(offers(legal, enemy)) << "move a FRIENDLY unit";
            return script({O(mine)})(legal);
        }
        return script({V(1 + 0)})(legal);           // battlefield #0
    }, exec);

    EXPECT_EQ(call, 3);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 2);
    EXPECT_TRUE(atBattlefield(mine, 0));
}

TEST_F(VendettaE, Vendetta_ShurikenFlip_DamageIsOptional) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    auto mine  = addUnit(P1, kInvalidId, 2, 0);
    auto spell = addToHand(P1, kShurikenFlip);
    // "up to one": decline, then move my unit back to base.
    driveResumable(kShurikenFlip, P1, spell, script({V(0), O(mine), V(0)}), exec);
    EXPECT_EQ(state.getObject(enemy).damage_marked, 0);
    EXPECT_TRUE(state.getObject(mine).isAtBase());
    auto f = card_registry.get(kShurikenFlip)->flowCost();
    EXPECT_EQ(f.energy, 3);
    EXPECT_EQ(f.power, 1);
    EXPECT_TRUE(f.any_domain);
}

TEST_F(VendettaE, Vendetta_Dominus_DoublesMightThisTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kDominus);
    invokeOnResolve(spell, kDominus, P1, {}, exec);
    EXPECT_EQ(state.getObject(unit).current_might, 6);
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 3) << "a this-turn bonus";
    EXPECT_TRUE(card_db.get(kDominus).keywords.has(Keyword::Action));
}

TEST_F(VendettaE, Vendetta_Dominus_GrantsReadyMeAbilityThisTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kDominus);
    ASSERT_NE(card, nullptr);
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kDominus);
    invokeOnResolve(spell, kDominus, P1, {}, exec);

    // Turn-scoped grant: the aura pass re-merges temp_granted_abilities.
    const auto& temp = state.getObject(unit).temp_granted_abilities;
    ASSERT_EQ(temp.size(), 1u);
    EXPECT_EQ(temp[0].source_def_id, kDominus);
    EXPECT_EQ(temp[0].ability_index, 0);
    ASSERT_EQ(state.getObject(unit).granted_abilities.size(), 1u) << "usable right away";

    // The granted ability is "[A][A]: Ready me." with the bearer as source.
    auto abs = card->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_EQ(abs[0].cost.power, 2);
    EXPECT_TRUE(abs[0].cost.power_any_domain);
    EXPECT_EQ(abs[0].cost.energy, 0);
    EXPECT_FALSE(abs[0].cost.exhaust);
    state.getObject(unit).is_exhausted = true;
    auto ctx = ctxFor(exec, P1, unit);
    card->onActivate(ctx, 0, {});
    EXPECT_FALSE(state.getObject(unit).is_exhausted);
}

TEST_F(VendettaE, Vendetta_DeathMark_BurnsThreeAndMakesAShadowClone) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kDredgeUp);
    auto spell = addToHand(P1, kDeathMark);
    invokeOnResolve(spell, kDeathMark, P1, {}, exec);
    EXPECT_EQ(deckSize(P1), 1);
    EXPECT_EQ(trashSize(P1), 3);
    EXPECT_EQ(state.player(P1).cards_burned_this_turn, 3);
    EXPECT_EQ(tokenCount(P1, "Shadow Clone"), 1);
    for (auto& [id, obj] : state.objects)
        if (obj.name == "Shadow Clone") { EXPECT_EQ(obj.base_might, 0); }
    auto f = card_registry.get(kDeathMark)->flowCost();
    EXPECT_EQ(f.energy, 1);
    EXPECT_EQ(f.power, 2);
    EXPECT_TRUE(f.any_domain);
}

TEST_F(VendettaE, Vendetta_SiphoningStrike_FourOrSevenByRuneCount) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P2, kInvalidId, 20, 0);
    for (int i = 0; i < 6; ++i) addRune(P1, Domain::Calm);
    auto spell = addToHand(P1, kSiphoningStrike);
    invokeOnResolve(spell, kSiphoningStrike, P1, {}, exec);
    EXPECT_EQ(state.getObject(unit).damage_marked, 4) << "6 runes";

    addRune(P1, Domain::Mind, /*exhausted=*/true);   // exhausted runes still count
    auto spell2 = addToHand(P1, kSiphoningStrike);
    invokeOnResolve(spell2, kSiphoningStrike, P1, {}, exec);
    EXPECT_EQ(state.getObject(unit).damage_marked, 11) << "7 runes: 7 instead";
}

TEST_F(VendettaE, Vendetta_SiphoningStrike_DeathWatchChannelsExhaustedRune) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto unit = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kSiphoningStrike);
    invokeOnResolve(spell, kSiphoningStrike, P1, {}, exec);

    ASSERT_EQ(state.delayed_abilities.size(), 1u);
    const auto& da = state.delayed_abilities[0];
    EXPECT_EQ(da.trigger, TriggerType::WhenIDie);
    EXPECT_EQ(da.target_filter, unit) << "scoped to the damaged unit";
    EXPECT_EQ(da.controller, P1);
    EXPECT_EQ(da.card_def_id, kSiphoningStrike);

    auto rune = addToRuneDeck(P1, Domain::Calm);
    fireTriggerAs(kSiphoningStrike, P1, spell, TriggerType::WhenIDie, exec);
    EXPECT_TRUE(state.getObject(rune).isAtBase());
    EXPECT_TRUE(state.getObject(rune).is_exhausted) << "channelled EXHAUSTED";
    EXPECT_TRUE(state.player(P1).rune_deck.empty());
}

TEST_F(VendettaE, Vendetta_ShadowDash_MovesEnemyAndBuffsExactlyTwo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    auto enemy = addUnit(P2, kInvalidId, 4, 1);
    auto already = addUnit(P2, kInvalidId, 4, 0);   // already where my units are
    auto spell = addToHand(P1, kShadowDash);

    int call = 0;
    driveResumable(kShadowDash, P1, spell, [&](const std::vector<Intent>& legal) {
        ++call;
        if (call == 1) {
            EXPECT_FALSE(offers(legal, already)) << "nowhere legal to move it";
            return script({O(enemy)})(legal);
        }
        EXPECT_EQ(legal.size(), 1u) << "only battlefield #0 has my units";
        return script({V(1 + 0)})(legal);
    }, exec);

    EXPECT_EQ(call, 2);
    EXPECT_TRUE(atBattlefield(enemy, 0));
    EXPECT_EQ(state.getObject(a).temp_might_bonus, 1);
    EXPECT_EQ(state.getObject(b).temp_might_bonus, 1);
    EXPECT_EQ(state.getObject(enemy).temp_might_bonus, 0);
}

TEST_F(VendettaE, Vendetta_ShadowDash_NoBonusWithThreeUnits) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    addUnit(P1, kInvalidId, 2, 0);
    addUnit(P1, kInvalidId, 2, 0);
    auto enemy = addUnit(P2, kInvalidId, 4);        // in its base
    auto spell = addToHand(P1, kShadowDash);
    driveResumable(kShadowDash, P1, spell, script({O(enemy), V(1 + 0)}), exec);
    EXPECT_TRUE(atBattlefield(enemy, 0));
    EXPECT_EQ(state.getObject(a).temp_might_bonus, 0);

    const Card* card = card_registry.get(kShadowDash);
    auto f = card->flowCost();
    EXPECT_EQ(f.energy, 5);
    EXPECT_EQ(f.power, 2);
    EXPECT_TRUE(f.any_domain);
}

TEST_F(VendettaE, Vendetta_ShadowDash_NeedsABattlefieldWithMyUnits) {
    addUnit(P1, kInvalidId, 2);                     // only in base
    addUnit(P2, kInvalidId, 4, 0);
    EXPECT_FALSE(card_registry.get(kShadowDash)->hasLegalTargets(state, P1));
}

TEST_F(VendettaE, Vendetta_AccelerationGate_ReadiesUpToFour) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    auto u2 = addUnit(P1, kInvalidId, 2);
    auto g  = addGear(P1, kInvalidId);
    auto r1 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Body, /*exhausted=*/true);
    auto ready = addUnit(P1, kInvalidId, 2);
    for (auto id : {u1, u2, g}) state.getObject(id).is_exhausted = true;
    auto spell = addToHand(P1, kAccelerationGate);

    int call = 0;
    driveResumable(kAccelerationGate, P1, spell, [&](const std::vector<Intent>& legal) {
        ++call;
        EXPECT_FALSE(offers(legal, ready)) << "already ready";
        const GameObjectId order[] = {u1, g, r1, r2};
        return script({O(order[call - 1])})(legal);
    }, exec);

    EXPECT_EQ(call, 4) << "up to 4 - no fifth prompt";
    for (auto id : {u1, g, r1, r2}) EXPECT_FALSE(state.getObject(id).is_exhausted);
    EXPECT_TRUE(state.getObject(u2).is_exhausted);
}

TEST_F(VendettaE, Vendetta_AccelerationGate_MayStopEarly) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto u1 = addUnit(P1, kInvalidId, 2, 0);
    auto u2 = addUnit(P1, kInvalidId, 2, 0);
    for (auto id : {u1, u2}) state.getObject(id).is_exhausted = true;
    auto spell = addToHand(P1, kAccelerationGate);
    driveResumable(kAccelerationGate, P1, spell, script({O(u2), V(0)}), exec);
    EXPECT_TRUE(state.getObject(u1).is_exhausted);
    EXPECT_FALSE(state.getObject(u2).is_exhausted);
}

TEST_F(VendettaE, Vendetta_Rebuttal_CountersWhenUnableToPay) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto target = pushSpellOnChain(P2, kDredgeUp);
    auto spell = addToHand(P1, kRebuttal);
    EXPECT_TRUE(card_registry.get(kRebuttal)->hasLegalTargets(state, P1));
    // No runes / power: the [A] option is never offered.
    driveResumable(kRebuttal, P1, spell, script({}), exec);
    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_TRUE(inTrash(P2, target));
}

TEST_F(VendettaE, Vendetta_Rebuttal_PayToGainControl) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto target = pushSpellOnChain(P2, kDredgeUp);
    auto rune = addRune(P1, Domain::Mind);
    auto spell = addToHand(P1, kRebuttal);
    driveResumable(kRebuttal, P1, spell, script({V(1)}), exec);

    ASSERT_EQ(state.chain.items.size(), 1u) << "not countered";
    EXPECT_EQ(state.chain.items.back().controller, P1);
    EXPECT_EQ(state.getObject(target).controller, P1);
    EXPECT_FALSE(inTrash(P2, target));
    EXPECT_FALSE(onBoard(rune)) << "the rune was recycled to pay [A]";
    EXPECT_EQ(state.player(P1).rune_deck.size(), 1u);
}

TEST_F(VendettaE, Vendetta_Rebuttal_DeclineCounters) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto target = pushSpellOnChain(P2, kDredgeUp);
    auto rune = addRune(P1, Domain::Mind);
    auto spell = addToHand(P1, kRebuttal);
    driveResumable(kRebuttal, P1, spell, script({V(0)}), exec);
    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_TRUE(inTrash(P2, target));
    EXPECT_TRUE(onBoard(rune));
}

TEST_F(VendettaE, Vendetta_Rebuttal_IgnoresSpellsCostingMoreThanFour) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    ASSERT_GT(card_db.get(kCataclysmicDuel).energy_cost, 4);
    auto target = pushSpellOnChain(P2, kCataclysmicDuel);
    addRune(P1, Domain::Mind);
    auto spell = addToHand(P1, kRebuttal);
    driveResumable(kRebuttal, P1, spell, script({}), exec);
    ASSERT_EQ(state.chain.items.size(), 1u);
    EXPECT_EQ(state.chain.items.back().controller, P2);
    EXPECT_FALSE(inTrash(P2, target));
}

TEST_F(VendettaE, Vendetta_Rebuttal_NotPlayableOnEmptyChain) {
    EXPECT_FALSE(card_registry.get(kRebuttal)->hasLegalTargets(state, P1));
}

TEST_F(VendettaE, Vendetta_PublicExecution_KillsWeakerEnemy) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mine   = addUnit(P1, kInvalidId, 4, 0);
    auto weaker = addUnit(P2, kInvalidId, 3, 0);
    auto equal  = addUnit(P2, kInvalidId, 4, 0);
    auto spell = addToHand(P1, kPublicExecution);

    int call = 0;
    driveResumable(kPublicExecution, P1, spell, [&](const std::vector<Intent>& legal) {
        ++call;
        if (call == 1) return script({O(mine)})(legal);
        EXPECT_FALSE(offers(legal, equal)) << "needs strictly LESS Might";
        return script({O(weaker)})(legal);
    }, exec);

    EXPECT_EQ(call, 2);
    EXPECT_FALSE(onBoard(weaker));
    EXPECT_TRUE(onBoard(equal));
    EXPECT_TRUE(onBoard(mine));
    auto f = card_registry.get(kPublicExecution)->flowCost();
    EXPECT_EQ(f.energy, 5);
    EXPECT_EQ(f.power, 2);
    EXPECT_TRUE(f.any_domain);
}

TEST_F(VendettaE, Vendetta_PublicExecution_NoWeakerEnemyKillsNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mine  = addUnit(P1, kInvalidId, 4, 0);
    auto equal = addUnit(P2, kInvalidId, 4, 0);
    auto spell = addToHand(P1, kPublicExecution);
    driveResumable(kPublicExecution, P1, spell, script({O(mine)}), exec);
    EXPECT_TRUE(onBoard(equal));
    EXPECT_TRUE(onBoard(mine));
}

TEST_F(VendettaE, Vendetta_LightningRush_DrawOneTrashTheRest) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto c1 = addToDeck(P1, kDredgeUp);   // bottom - not looked at
    auto c2 = addToDeck(P1, kDredgeUp);
    auto c3 = addToDeck(P1, kPunchingPoro);
    auto c4 = addToDeck(P1, kDredgeUp);   // top
    auto spell = addToHand(P1, kLightningRush);
    const int hand_before = handSize(P1);

    int call = 0;
    driveResumable(kLightningRush, P1, spell, [&](const std::vector<Intent>& legal) {
        ++call;
        EXPECT_FALSE(offers(legal, c1)) << "only the top 3";
        return script({O(c3)})(legal);
    }, exec);

    EXPECT_EQ(call, 1);
    EXPECT_TRUE(inHand(P1, c3));
    EXPECT_EQ(handSize(P1), hand_before + 1);
    EXPECT_EQ(state.player(P1).draws_this_turn, 1) << "it is DRAWN";
    EXPECT_TRUE(inTrash(P1, c2));
    EXPECT_TRUE(inTrash(P1, c4));
    EXPECT_TRUE(inDeck(P1, c1));
    EXPECT_EQ(deckSize(P1), 1);
}

TEST_F(VendettaE, Vendetta_LightningRush_MayDrawNone) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kDredgeUp);
    auto spell = addToHand(P1, kLightningRush);
    const int hand_before = handSize(P1);
    driveResumable(kLightningRush, P1, spell, script({V(0)}), exec);
    EXPECT_EQ(handSize(P1), hand_before);
    EXPECT_EQ(trashSize(P1), 3);
    EXPECT_EQ(deckSize(P1), 0);
    auto f = card_registry.get(kLightningRush)->flowCost();
    EXPECT_EQ(f.energy, 2);
    EXPECT_EQ(f.power, 1);
    EXPECT_TRUE(f.any_domain);
}

// ═══════════════════════════════ Gear ═══════════════════════════════════════

TEST_F(VendettaE, Vendetta_PendulumBlade_MoveToBattlefieldGivesTwoMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kPendulumBlade);
    ASSERT_NE(card, nullptr);
    EXPECT_TRUE(card->hasEquipAbility());
    EXPECT_EQ(card->equippedTriggerType(), TriggerType::WhenIMoveToFB);

    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto gear = addGear(P1, kPendulumBlade);
    addRune(P1, Domain::Calm);
    auto ctx = ctxFor(exec, P1, gear);
    EXPECT_FALSE(equipThroughEngine(gear, unit)) << "[Equip] [R] needs a Fury rune";
    addRune(P1, Domain::Fury);
    ASSERT_TRUE(equipThroughEngine(gear, unit));
    EXPECT_EQ(state.getObject(unit).current_might, 3) << "+1 [M] from the Equipment";

    card->onEquippedTrigger(ctx, unit, {});
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 2);
    EXPECT_EQ(state.getObject(unit).current_might, 5);
}

TEST_F(VendettaE, Vendetta_EndlessRiches_BanishHandAndTrashThenBurnSeven) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addGear(P1, kEndlessRiches);
    auto h1 = addToHand(P1, kDredgeUp);
    auto h2 = addToHand(P1, kPunchingPoro);
    auto t1 = addToTrash(P1, kDredgeUp);
    for (int i = 0; i < 8; ++i) addToDeck(P1, kDredgeUp);

    fireTriggerAs(kEndlessRiches, P1, gear, TriggerType::WhenYouPlayThis, exec);

    auto& ps = state.player(P1);
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_EQ(ps.banishment.size(), 3u);
    for (auto id : {h1, h2, t1}) {
        EXPECT_EQ(state.getObject(id).zone, ZoneType::Banishment);
        EXPECT_FALSE(inTrash(P1, id));
    }
    EXPECT_EQ(trashSize(P1), 7) << "the 7 burned cards, burned AFTER the trash was banished";
    EXPECT_EQ(deckSize(P1), 1);
    EXPECT_EQ(ps.cards_burned_this_turn, 7);
    EXPECT_EQ(ps.cards_banished_this_turn, 3);
    EXPECT_TRUE(card_registry.get(kEndlessRiches)->firesOn(TriggerType::WhenYouPlayThis));
}

TEST_F(VendettaE, Vendetta_EndlessRiches_ContinuousFlags) {
    const Card* card = card_registry.get(kEndlessRiches);
    card->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.player(P1).skip_draw_phase) << "not on the board yet";

    addGear(P1, kEndlessRiches);
    card->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P1).skip_draw_phase);
    EXPECT_TRUE(state.player(P1).may_play_from_trash);
    EXPECT_TRUE(state.player(P1).trash_goes_to_banish);
    EXPECT_FALSE(state.player(P2).skip_draw_phase);
    EXPECT_FALSE(state.player(P2).may_play_from_trash);
    EXPECT_FALSE(state.player(P2).trash_goes_to_banish);
}

TEST_F(VendettaE, Vendetta_HandHammer_ExactlyOneOtherFriendlyUnit) {
    const Card* card = card_registry.get(kHandHammer);
    ASSERT_NE(card, nullptr);
    EXPECT_TRUE(card->hasEquipAbility());
    auto bearer = addUnit(P1, kInvalidId, 2, 0);
    auto gear = addGear(P1, kHandHammer);
    attach(gear, bearer);
    addUnit(P2, kInvalidId, 2, 0);                  // enemy units don't count
    auto clear = [&]() {
        for (auto& [id, obj] : state.objects) obj.aura_effects.clear();
    };

    card->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(bearer)), 0) << "alone";

    clear();
    addUnit(P1, kInvalidId, 2, 0);
    card->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(bearer)), 2) << "exactly one other";

    clear();
    addUnit(P1, kInvalidId, 2, 0);
    card->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(bearer)), 0) << "two others";
}

TEST_F(VendettaE, Vendetta_HandHammer_OnlyAtABattlefield) {
    const Card* card = card_registry.get(kHandHammer);
    auto bearer = addUnit(P1, kInvalidId, 2);       // base
    auto gear = addGear(P1, kHandHammer);
    attach(gear, bearer);
    addUnit(P1, kInvalidId, 2);                     // also in base
    card->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(bearer)), 0);
}

TEST_F(VendettaE, Vendetta_HelmOfSuppression_TaxesOpponentSpells) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* card = card_registry.get(kHelmOfSuppression);
    ASSERT_NE(card, nullptr);
    auto gear = addGear(P1, kHelmOfSuppression);

    card->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P2).spell_tax_energy, 1);
    EXPECT_EQ(state.player(P2).spell_tax_power, 0);
    EXPECT_EQ(state.player(P1).spell_tax_energy, 0) << "only OPPONENTS' spells";

    // Next aura pass (the engine resets the flags + aura effects), Empowered.
    state.player(P2).spell_tax_energy = 0;
    state.getObject(gear).aura_effects.clear();
    exec.empower(gear);
    card->applyPassiveAura(state, P1);
    EXPECT_EQ(state.player(P2).spell_tax_energy, 1);
    EXPECT_EQ(state.player(P2).spell_tax_power, 1) << "[1][A] more instead";

    const int idx = card->empowerAbilityIndex();
    ASSERT_GE(idx, 0);
    auto cost = card->activatedAbilities()[idx].cost;
    EXPECT_EQ(cost.energy, 4);
    EXPECT_EQ(cost.power, 1);
    EXPECT_EQ(cost.power_domain, Domain::Calm);
}

TEST_F(VendettaE, Vendetta_QuestionableTome_DisempowerToDraw) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kQuestionableTome);
    ASSERT_NE(card, nullptr);
    auto abs = card->activatedAbilities();
    ASSERT_EQ(abs.size(), 2u);
    EXPECT_TRUE(abs[0].cost.disempower_self);
    EXPECT_TRUE(abs[0].cost.exhaust);
    EXPECT_EQ(abs[0].cost.energy, 1);
    EXPECT_TRUE(abs[1].is_empower);
    EXPECT_TRUE(abs[1].cost.exhaust) << "[Empower] - [E]";
    EXPECT_EQ(abs[1].cost.energy, 0);

    auto gear = addGear(P1, kQuestionableTome);
    EXPECT_FALSE(card->canActivateAbilityAt(state, P1, gear, 0)) << "not Empowered yet";
    EXPECT_TRUE(card->canActivateAbilityAt(state, P1, gear, 1));
    exec.empower(gear);
    EXPECT_TRUE(card->canActivateAbilityAt(state, P1, gear, 0));
    EXPECT_FALSE(card->canActivateAbilityAt(state, P1, gear, 1));

    addToDeck(P1, kDredgeUp);
    const int before = handSize(P1);
    auto ctx = ctxFor(exec, P1, gear);
    card->onActivate(ctx, 0, {});
    EXPECT_EQ(handSize(P1), before + 1);
}

TEST_F(VendettaE, Vendetta_HextechFormula_EmpowersAnotherGear) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kHextechFormula);
    ASSERT_NE(card, nullptr);
    auto formula = addGear(P1, kHextechFormula);
    auto other = addGear(P1, kToolsOfEmpire);
    auto unit = addUnit(P1, kInvalidId, 2);

    auto legal = card->enumerateLegalTargets(state, P1);
    EXPECT_NE(std::find(legal.begin(), legal.end(), other), legal.end());
    EXPECT_EQ(std::find(legal.begin(), legal.end(), formula), legal.end()) << "ANOTHER gear";
    EXPECT_EQ(std::find(legal.begin(), legal.end(), unit), legal.end()) << "gear only";

    auto abs = card->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].cost.exhaust);

    auto ctx = ctxFor(exec, P1, formula);
    card->onActivate(ctx, 0, {formula});
    EXPECT_FALSE(state.getObject(formula).isEmpowered()) << "can't empower itself";
    card->onActivate(ctx, 0, {other});
    EXPECT_TRUE(state.getObject(other).isEmpowered());

    // "This enters exhausted." (honoured by GameEngine::resolvePermanent)
    EXPECT_TRUE(card->entersExhausted());
}

TEST_F(VendettaE, Vendetta_BottledConstellation_KillThreeToScore) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addGear(P1, kBottledConstellation);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addGear(P1, kInvalidId);
    auto c = addUnit(P1, kInvalidId, 2);
    auto d = addUnit(P1, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);

    int call = 0;
    driveResumableTrigger(kBottledConstellation, P1, gear,
        [&](const std::vector<Intent>& legal) {
            ++call;
            if (call == 1) return script({V(1)})(legal);     // yes
            EXPECT_FALSE(offers(legal, gear)) << "3 OTHER";
            EXPECT_FALSE(offers(legal, enemy)) << "friendly only";
            const GameObjectId order[] = {a, b, c};
            if (call == 3) { EXPECT_FALSE(offers(legal, a)) << "no double-pick"; }
            return script({O(order[call - 2])})(legal);
        }, exec, TriggerType::AtStartOfMain);

    EXPECT_EQ(call, 4);
    EXPECT_FALSE(onBoard(a));
    EXPECT_FALSE(onBoard(b));
    EXPECT_FALSE(onBoard(c));
    EXPECT_TRUE(onBoard(d));
    EXPECT_TRUE(onBoard(gear));
    EXPECT_TRUE(onBoard(enemy));
    EXPECT_EQ(state.player(P1).score, 1);
    EXPECT_TRUE(card_registry.get(kBottledConstellation)->firesOn(TriggerType::AtStartOfMain));
}

TEST_F(VendettaE, Vendetta_BottledConstellation_DeclineOrTooFew) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addGear(P1, kBottledConstellation);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);

    // Only two others: never offered.
    driveResumableTrigger(kBottledConstellation, P1, gear, script({}), exec,
                          TriggerType::AtStartOfMain);
    EXPECT_EQ(state.player(P1).score, 0);
    EXPECT_TRUE(onBoard(a));

    // Three others, but the player says no.
    auto c = addUnit(P1, kInvalidId, 2, 0);
    driveResumableTrigger(kBottledConstellation, P1, gear, script({V(0)}), exec,
                          TriggerType::AtStartOfMain);
    EXPECT_EQ(state.player(P1).score, 0);
    for (auto id : {a, b, c}) EXPECT_TRUE(onBoard(id));
}

TEST_F(VendettaE, Vendetta_JaggedCutlass_EquipForBodyPower) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kJaggedCutlass);
    ASSERT_NE(card, nullptr);
    ASSERT_TRUE(card->hasEquipAbility());
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto gear = addGear(P1, kJaggedCutlass);

    addRune(P1, Domain::Fury);
    EXPECT_FALSE(equipThroughEngine(gear, unit)) << "[O] needs a Body rune";
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());

    auto body = addRune(P1, Domain::Body);
    ASSERT_TRUE(equipThroughEngine(gear, unit));
    EXPECT_TRUE(state.getObject(gear).attached_to == std::optional<GameObjectId>(unit));
    EXPECT_EQ(state.getObject(unit).current_might, 4) << "+2 [M]";
    EXPECT_FALSE(onBoard(body)) << "the Body rune was recycled";
}

TEST_F(VendettaE, Vendetta_JaggedCutlass_BearerCantBeMovedByEnemyEffects) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* card = card_registry.get(kJaggedCutlass);
    ASSERT_NE(card, nullptr);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto bystander = addUnit(P1, kInvalidId, 2, 0);
    auto gear = addGear(P1, kJaggedCutlass);

    card->applyPassiveAura(state, P1);
    EXPECT_FALSE(state.getObject(unit).cant_be_moved_by_enemy) << "not attached yet";

    attach(gear, unit);
    card->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.getObject(unit).cant_be_moved_by_enemy);
    EXPECT_FALSE(state.getObject(bystander).cant_be_moved_by_enemy) << "only the bearer";

    // An enemy spell/ability resolving: the move is refused.
    ChainItem enemy_item;
    enemy_item.id = state.chain.allocateId();
    enemy_item.controller = P2;
    enemy_item.is_spell = true;
    state.chain.resuming = enemy_item;
    exec.moveToBase(unit);
    EXPECT_TRUE(atBattlefield(unit, 0));
    exec.moveToBattlefield(unit, 1);
    EXPECT_TRUE(atBattlefield(unit, 0));
    exec.moveToBase(bystander);
    EXPECT_FALSE(atBattlefield(bystander, 0)) << "an unequipped unit still moves";

    // A friendly effect may still move it.
    enemy_item.controller = P1;
    state.chain.resuming = enemy_item;
    exec.moveToBattlefield(unit, 1);
    EXPECT_TRUE(atBattlefield(unit, 1));
    state.chain.resuming.reset();
}

TEST_F(VendettaE, Vendetta_PlatewyrmEgg_AddsOneOrTwo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kPlatewyrmEgg);
    ASSERT_NE(card, nullptr);
    auto abs = card->activatedAbilities();
    ASSERT_EQ(abs.size(), 2u);
    EXPECT_TRUE(abs[0].cost.exhaust);
    EXPECT_TRUE(abs[0].is_reaction);
    EXPECT_FALSE(abs[0].cost.disempower_self);
    EXPECT_TRUE(abs[1].is_empower);
    EXPECT_EQ(abs[1].cost.energy, 1);
    EXPECT_TRUE(abs[1].cost.exhaust);

    auto gear = addGear(P1, kPlatewyrmEgg);
    auto ctx = ctxFor(exec, P1, gear);
    card->onActivate(ctx, 0, {});
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);

    exec.empower(gear);
    card->onActivate(ctx, 0, {});
    EXPECT_EQ(state.player(P1).rune_pool.energy, 3) << "+2 while Empowered";

    // "This enters exhausted." (honoured by GameEngine::resolvePermanent)
    EXPECT_TRUE(card->entersExhausted());
    EXPECT_FALSE(card_registry.get(kToolsOfEmpire)->entersExhausted()) << "gear normally enters ready";
}

TEST_F(VendettaE, Vendetta_ToolsOfEmpire_TwoOrFourMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kToolsOfEmpire);
    ASSERT_NE(card, nullptr);
    auto gear = addGear(P1, kToolsOfEmpire);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    auto ctx = ctxFor(exec, P1, gear);

    card->onActivate(ctx, 0, {unit});
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 2);

    exec.empower(gear);
    card->onActivate(ctx, 0, {unit});
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 6) << "+4 while Empowered";

    auto abs = card->activatedAbilities();
    ASSERT_EQ(abs.size(), 2u);
    EXPECT_TRUE(abs[0].cost.exhaust);
    EXPECT_EQ(abs[0].targets.count, 1);
    EXPECT_TRUE(abs[0].targets.must_be_unit);
    EXPECT_TRUE(abs[1].is_empower);
    EXPECT_EQ(abs[1].cost.energy, 2);
    EXPECT_FALSE(abs[1].cost.exhaust);
}

TEST_F(VendettaE, Vendetta_HextechDisc_DisempowerForMech) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kHextechDisc);
    ASSERT_NE(card, nullptr);
    auto abs = card->activatedAbilities();
    ASSERT_EQ(abs.size(), 2u);
    EXPECT_TRUE(abs[0].cost.disempower_self);
    EXPECT_TRUE(abs[0].cost.exhaust);
    EXPECT_EQ(abs[0].cost.energy, 1);
    EXPECT_TRUE(abs[1].is_empower);
    EXPECT_TRUE(abs[1].cost.exhaust);

    auto gear = addGear(P1, kHextechDisc);
    EXPECT_FALSE(card->canActivateAbilityAt(state, P1, gear, 0)) << "needs to be Empowered";

    auto ctx = ctxFor(exec, P1, gear);
    card->onActivate(ctx, 0, {});
    ASSERT_EQ(tokenCount(P1, "Mech"), 1);
    for (auto& [id, obj] : state.objects) {
        if (obj.name != "Mech") continue;
        EXPECT_EQ(obj.current_might, 3);
        EXPECT_TRUE(obj.isAtBase());
        EXPECT_TRUE(obj.isUnit());
    }
}

TEST_F(VendettaE, Vendetta_ForgottenRelic_BurnedUnitGivesItsMight) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const int poro_might = card_db.get(kPunchingPoro).might;
    ASSERT_GT(poro_might, 0);
    auto gear = addGear(P1, kForgottenRelic);
    auto a = addUnit(P1, kInvalidId, 1, 0);
    auto b = addUnit(P1, kInvalidId, 1, 0);
    auto enemy = addUnit(P2, kInvalidId, 1, 0);
    addToDeck(P1, kDredgeUp);
    auto top = addToDeck(P1, kPunchingPoro);

    int call = 0;
    // Through the engine: the text after "do this" is a reflexive trigger (CR
    // 387-389), a chain item of its own that only the engine queues and resolves.
    EnginePathAgent engine_agent([&](const std::vector<Intent>& legal) {
        ++call;
        EXPECT_FALSE(offers(legal, enemy)) << "a FRIENDLY unit";
        return script({O(b)})(legal);
    });
    fireTriggerThroughEngine(gear, TriggerType::AtStartOfBeginning, &engine_agent);

    EXPECT_EQ(call, 1);
    EXPECT_TRUE(inTrash(P1, top));
    EXPECT_EQ(deckSize(P1), 1) << "burned exactly one card despite the resume";
    EXPECT_EQ(state.getObject(b).temp_might_bonus, poro_might);
    EXPECT_EQ(state.getObject(a).temp_might_bonus, 0);

    const Card* card = card_registry.get(kForgottenRelic);
    EXPECT_TRUE(card->firesOn(TriggerType::WhenYouPlayThis));
    EXPECT_TRUE(card->firesOn(TriggerType::AtStartOfBeginning));
}

TEST_F(VendettaE, Vendetta_ForgottenRelic_BurnedNonUnitGivesNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addGear(P1, kForgottenRelic);
    auto a = addUnit(P1, kInvalidId, 1, 0);
    auto top = addToDeck(P1, kDredgeUp);
    driveResumableTrigger(kForgottenRelic, P1, gear, script({}), exec,
                          TriggerType::WhenYouPlayThis);
    EXPECT_TRUE(inTrash(P1, top));
    EXPECT_EQ(state.getObject(a).temp_might_bonus, 0);
}

TEST_F(VendettaE, Vendetta_Glowstone_HandsItselfToTheChosenPlayer) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kGlowstone);
    ASSERT_NE(card, nullptr);
    auto abs = card->activatedAbilities();
    ASSERT_EQ(abs.size(), 2u);
    EXPECT_TRUE(abs[0].cost.disempower_self);
    EXPECT_TRUE(abs[0].cost.exhaust);
    EXPECT_TRUE(abs[1].is_empower);
    EXPECT_EQ(abs[1].cost.energy, 0);
    EXPECT_EQ(abs[1].cost.power, 2);
    EXPECT_TRUE(abs[1].cost.power_any_domain);

    auto gear = addGear(P1, kGlowstone);
    driveActivate(kGlowstone, P1, gear, {}, script({V(1)}), exec);   // 1 = opponent
    const auto& g = state.getObject(gear);
    EXPECT_EQ(g.controller, P2);
    EXPECT_EQ(g.owner, P1);
    ASSERT_TRUE(g.location.has_value());
    EXPECT_TRUE(*g.location == LocationId{BaseLocation{P2}}) << "recalled to THEIR base";
}

TEST_F(VendettaE, Vendetta_Glowstone_EndOfTurnKillsItAndDealsFive) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto gear = addGear(P1, kGlowstone);
    auto mine_bf   = addUnit(P1, kInvalidId, 6, 0);
    auto mine_base = addUnit(P1, kInvalidId, 3);
    auto theirs    = addUnit(P2, kInvalidId, 6, 0);

    fireTriggerAs(kGlowstone, P1, gear, TriggerType::AtEndOfTurn, exec);

    EXPECT_FALSE(onBoard(gear));
    EXPECT_TRUE(inTrash(P1, gear));
    EXPECT_EQ(state.getObject(mine_bf).damage_marked, 5);
    EXPECT_EQ(state.getObject(mine_base).damage_marked, 5) << "ALL units you control";
    EXPECT_TRUE(state.getObject(mine_base).hasLethalDamage());
    EXPECT_EQ(state.getObject(theirs).damage_marked, 0);
    EXPECT_TRUE(card_registry.get(kGlowstone)->firesOn(TriggerType::AtEndOfTurn));
}

TEST_F(VendettaE, Vendetta_ShadySpectacles_BearerBecomesACopy) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* card = card_registry.get(kShadySpectacles);
    ASSERT_NE(card, nullptr);
    ASSERT_TRUE(card->hasEquipAbility());
    auto bearer = addUnit(P1, kInvalidId, 5, 0);
    auto model  = addUnit(P1, kPunchingPoro, 0, 0);
    auto gear = addGear(P1, kShadySpectacles);

    // [1][Y]: with no Order rune nothing happens.
    addRune(P1, Domain::Fury);
    EXPECT_FALSE(equipThroughEngine(gear, bearer));
    EXPECT_EQ(state.getObject(bearer).card_def_id, kInvalidId);

    addRune(P1, Domain::Order);
    ASSERT_TRUE(equipThroughEngine(gear, bearer));
    const auto& b = state.getObject(bearer);
    EXPECT_TRUE(state.getObject(gear).attached_to == std::optional<GameObjectId>(bearer));
    EXPECT_EQ(b.card_def_id, kPunchingPoro);
    EXPECT_EQ(b.name, card_db.get(kPunchingPoro).name);
    EXPECT_EQ(b.base_might, card_db.get(kPunchingPoro).might);
    EXPECT_EQ(state.getObject(model).card_def_id, kPunchingPoro) << "the model is unchanged";
    EXPECT_EQ(state.player(P1).rune_deck.size(), 1u) << "[Y] recycled the Order rune";
    EXPECT_EQ(readyRuneCount(P1, Domain::Order), 0);
}

TEST_F(VendettaE, Vendetta_ShadySpectacles_NoOtherUnitJustAttaches) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto bearer = addUnit(P1, kInvalidId, 5, 0);
    addUnit(P2, kPunchingPoro, 0, 0);               // enemy: not "another friendly unit"
    auto gear = addGear(P1, kShadySpectacles);
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Order);
    ASSERT_TRUE(equipThroughEngine(gear, bearer));
    EXPECT_EQ(state.getObject(bearer).card_def_id, kInvalidId);
    EXPECT_EQ(state.getObject(bearer).base_might, 5);
}

}  // namespace
}  // namespace riftbound::test
