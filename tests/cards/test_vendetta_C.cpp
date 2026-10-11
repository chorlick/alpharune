/// @file test_vendetta_C.cpp
/// Vendetta batch C: Chaos / Order units (886-925). Each card's main effect
/// and its key condition boundary, driven directly through the Card hooks.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_vendetta_C {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kPunchingPoro    = 794;   // 2-cost unit, no Power cost
constexpr CardDefId kOnslaught       = 868;   // spell
constexpr CardDefId kTornadoWarrior  = 886;
constexpr CardDefId kGustMonk        = 888;
constexpr CardDefId kRavenbloom      = 889;
constexpr CardDefId kMatriarch       = 891;
constexpr CardDefId kIllaoi          = 896;
constexpr CardDefId kMel             = 897;
constexpr CardDefId kMinah           = 898;
constexpr CardDefId kZed             = 899;
constexpr CardDefId kKennenStorm     = 900;
constexpr CardDefId kKharox          = 901;
constexpr CardDefId kOceanDrake      = 902;
constexpr CardDefId kDiscipleOfShen  = 904;
constexpr CardDefId kHornsOfDragon   = 905;
constexpr CardDefId kKeeperOfLaw     = 906;
constexpr CardDefId kMasa            = 907;
constexpr CardDefId kReluctantLeader = 908;
constexpr CardDefId kSolariSunhawk   = 909;
constexpr CardDefId kSoulspinner     = 910;
constexpr CardDefId kEscapedGrayback = 911;
constexpr CardDefId kHungryWolf      = 912;
constexpr CardDefId kNoxianEmissary  = 915;
constexpr CardDefId kSacredProtector = 916;
constexpr CardDefId kAurokGeneral    = 917;
constexpr CardDefId kFallenFeline    = 919;
constexpr CardDefId kKayle           = 921;
constexpr CardDefId kKennenBalance   = 922;
constexpr CardDefId kAmbessa         = 923;
constexpr CardDefId kShen            = 925;

int auraMightSum(const GameObject& obj) {
    int n = 0;
    for (const auto& ae : obj.aura_effects) n += ae.might_bonus;
    return n;
}

bool hasAuraKeyword(const GameObject& obj, Keyword kw, int value) {
    for (const auto& ae : obj.aura_effects)
        if (ae.keyword == kw && ae.keyword_value == value) return true;
    return false;
}

bool suppressesCombatDamage(const GameObject& obj) {
    for (const auto& ae : obj.aura_effects)
        if (ae.suppress_combat_damage) return true;
    return false;
}

/// Picker for the resumable loop: answers int-coded prompts (yes/no, mode)
/// with `value` and object prompts with `object`.
std::function<Intent(const std::vector<Intent>&)> answer(int value,
                                                         GameObjectId object = kInvalidId) {
    return [value, object](const std::vector<Intent>& legal) {
        for (const auto& i : legal) {
            if (i.chosen_value.has_value()) {
                if (*i.chosen_value == value) return i;
            } else if (!i.chosen_objects.empty() && i.chosen_objects[0] == object) {
                return i;
            }
        }
        return legal.empty() ? Intent{} : legal.front();
    };
}

class VendettaCTest : public CardTestFixture {
protected:
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& ps = state.player(owner);
        ps.hand.erase(std::remove(ps.hand.begin(), ps.hand.end(), id), ps.hand.end());
        state.getObject(id).zone = ZoneType::Trash;
        ps.trash.push_back(id);
        return id;
    }

    void clearAuras() {
        for (auto& [id, obj] : state.objects) obj.aura_effects.clear();
    }

    int countUnitsNamed(PlayerId controller, const std::string& name) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == controller && obj.location.has_value() &&
                obj.name == name) ++n;
        return n;
    }

    /// driveResumableTrigger, but with a triggering_subject on the chain item
    /// (what TriggerManager::fireTrigger records for subject-carrying events).
    void driveTriggerWithSubject(CardDefId def_id, PlayerId controller, GameObjectId source,
                                 GameObjectId subject, TriggerType firing,
                                 std::function<Intent(const std::vector<Intent>&)> picker,
                                 EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_ability = true;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.fired_trigger = firing;
        ri.triggering_subject = subject;
        state.chain.resuming = ri;

        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        ctx.firing_trigger = firing;
        card->onTrigger(ctx, {});
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            exec.recordChoice(picker(pending.legal));
            card->onTrigger(ctx, {});
        }
        state.chain.resuming.reset();
    }
};

// ── 886 Tornado Warrior ──────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_TornadoWarrior_FromFaceDownEmpowersUntilEndOfTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto warrior = addUnit(P1, kTornadoWarrior, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    auto elsewhere = addUnit(P1, kInvalidId, 2, 1);
    // Stamped by GameEngine::executePlayFromHidden.
    state.getObject(warrior).card_counters["__played_from_facedown"] = 1;

    driveResumableTrigger(kTornadoWarrior, P1, warrior, answer(1, ally), exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(state.getObject(ally).isEmpowered());
    EXPECT_EQ(state.getObject(ally).empower_flip_eot, -1) << "disempower it at end of turn";
    EXPECT_FALSE(state.getObject(elsewhere).isEmpowered());
    EXPECT_FALSE(state.getObject(warrior).isEmpowered());
    EXPECT_EQ(state.getObject(warrior).card_counters["__played_from_facedown"], 0)
        << "consumed: a later replay from hand must not re-trigger";
}

TEST_F(VendettaCTest, Vendetta_TornadoWarrior_PlayedFromHandDoesNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto warrior = addUnit(P1, kTornadoWarrior, 0, 0);
    // Hidden earlier but NOT played from face down: no stamp, so no trigger.
    state.getObject(warrior).hidden_on_turn = 0;
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    driveResumableTrigger(kTornadoWarrior, P1, warrior, answer(1, ally), exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(state.getObject(ally).isEmpowered());
    EXPECT_FALSE(state.getObject(warrior).isEmpowered());
}

TEST_F(VendettaCTest, Vendetta_TornadoWarrior_MayDecline) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto warrior = addUnit(P1, kTornadoWarrior, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 2, 0);
    state.getObject(warrior).card_counters["__played_from_facedown"] = 1;
    driveResumableTrigger(kTornadoWarrior, P1, warrior, answer(0, ally), exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(state.getObject(ally).isEmpowered());
}

// ── 888 Gust Monk ────────────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_GustMonk_PaidBanishesFromAnyTrashForAssault) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto cost = card_registry.get(kGustMonk)->optionalAdditionalCost();
    EXPECT_TRUE(cost.valid);
    EXPECT_EQ(cost.energy, 1);
    EXPECT_EQ(cost.power, 0);

    auto monk = addUnit(P1, kGustMonk, 0, 0);
    auto fodder = addToTrash(P2, kOnslaught);            // an opponent's trash counts
    state.getObject(monk).card_counters[cost.paid_flag] = 1;

    fireTriggerAs(kGustMonk, P1, monk, TriggerType::WhenYouPlayMe, exec);
    EXPECT_FALSE(inTrash(P2, fodder));
    EXPECT_EQ(state.getObject(fodder).zone, ZoneType::Banishment);
    auto& ban = state.player(P2).banishment;
    EXPECT_NE(std::find(ban.begin(), ban.end(), fodder), ban.end());
    EXPECT_EQ(state.getObject(monk).assault_value, 2);
    EXPECT_EQ(state.player(P1).cards_banished_this_turn, 0) << "not a card P1 owns";
}

TEST_F(VendettaCTest, Vendetta_GustMonk_UnpaidOrEmptyTrashDoesNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto monk = addUnit(P1, kGustMonk, 0, 0);
    auto fodder = addToTrash(P1, kOnslaught);
    fireTriggerAs(kGustMonk, P1, monk, TriggerType::WhenYouPlayMe, exec);
    EXPECT_TRUE(inTrash(P1, fodder)) << "additional cost not paid";
    EXPECT_EQ(state.getObject(monk).assault_value, 0);

    // Paid, but nothing to banish: "banish a card ... to give" gives nothing.
    auto& ps = state.player(P1);
    ps.trash.clear();
    state.getObject(monk).card_counters["__gust_monk_paid"] = 1;
    fireTriggerAs(kGustMonk, P1, monk, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(state.getObject(monk).assault_value, 0);
}

// ── 889 Ravenbloom Prefect ───────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_RavenbloomPrefect_BanishesSelfAndGear) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_registry.get(kRavenbloom)->firesOn(TriggerType::WhenOpponentPlaysAGear));
    auto prefect = addUnit(P1, kRavenbloom, 0, 0);
    auto gear = addUnit(P2, kInvalidId);
    state.getObject(gear).card_type = CardType::Gear;

    driveTriggerWithSubject(kRavenbloom, P1, prefect, gear,
                            TriggerType::WhenOpponentPlaysAGear, answer(1), exec);
    EXPECT_EQ(state.getObject(prefect).zone, ZoneType::Banishment);
    EXPECT_FALSE(state.getObject(prefect).location.has_value());
    EXPECT_EQ(state.getObject(gear).zone, ZoneType::Banishment);
    EXPECT_FALSE(state.getObject(gear).location.has_value());
}

TEST_F(VendettaCTest, Vendetta_RavenbloomPrefect_DeclineKeepsBoth) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto prefect = addUnit(P1, kRavenbloom, 0, 0);
    auto gear = addUnit(P2, kInvalidId);
    state.getObject(gear).card_type = CardType::Gear;

    driveTriggerWithSubject(kRavenbloom, P1, prefect, gear,
                            TriggerType::WhenOpponentPlaysAGear, answer(0), exec);
    EXPECT_TRUE(state.getObject(prefect).location.has_value());
    EXPECT_TRUE(state.getObject(gear).location.has_value());
}

TEST_F(VendettaCTest, Vendetta_RavenbloomPrefect_RemovesGearStillOnChain) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto prefect = addUnit(P1, kRavenbloom, 0, 0);
    auto gear = pushSpellOnChain(P2);                 // a pending play on the chain
    state.getObject(gear).card_type = CardType::Gear;
    state.chain.items.back().is_spell = false;
    state.chain.items.back().is_permanent = true;

    driveTriggerWithSubject(kRavenbloom, P1, prefect, gear,
                            TriggerType::WhenOpponentPlaysAGear, answer(1), exec);
    EXPECT_TRUE(state.chain.items.empty()) << "the gear never finishes being played";
    EXPECT_EQ(state.getObject(gear).zone, ZoneType::Banishment);
}

// ── 891 Tail-Cloaked Matriarch ───────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_TailCloakedMatriarch_PlaysCheapUnitFromTrash) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kMatriarch);
    const int idx = c->empowerAbilityIndex();
    ASSERT_GE(idx, 0);
    auto ab = c->activatedAbilities()[idx];
    EXPECT_EQ(ab.cost.energy, 2);
    EXPECT_EQ(ab.cost.power, 1);
    EXPECT_EQ(ab.cost.power_domain, Domain::Chaos);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIBecomeEmpowered));

    auto matriarch = addUnit(P1, kMatriarch, 0, 0);
    auto cheap = addToTrash(P1, kPunchingPoro);     // [2]
    auto pricey = addToTrash(P1, kKharox);          // [6] — over the Energy limit
    auto theirs = addToTrash(P2, kPunchingPoro);    // not MY trash

    fireTriggerAs(kMatriarch, P1, matriarch, TriggerType::WhenIBecomeEmpowered, exec);
    EXPECT_FALSE(inTrash(P1, cheap));
    EXPECT_TRUE(state.getObject(cheap).isAtBase());
    EXPECT_TRUE(inTrash(P1, pricey));
    EXPECT_TRUE(inTrash(P2, theirs));
}

TEST_F(VendettaCTest, Vendetta_TailCloakedMatriarch_NothingEligible) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto matriarch = addUnit(P1, kMatriarch, 0, 0);
    auto pricey = addToTrash(P1, kKharox);
    auto spell = addToTrash(P1, kOnslaught);
    fireTriggerAs(kMatriarch, P1, matriarch, TriggerType::WhenIBecomeEmpowered, exec);
    EXPECT_TRUE(inTrash(P1, pricey));
    EXPECT_TRUE(inTrash(P1, spell));
}

// ── 896 Illaoi, Prophet of the Great Kraken ──────────────────────────────

TEST_F(VendettaCTest, Vendetta_IllaoiProphet_TokenOnPlayAndScore) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kIllaoi);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenYouPlayMe));
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIConquer));
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIHold));

    auto illaoi = addUnit(P1, kIllaoi, 0, 0);
    fireTriggerAs(kIllaoi, P1, illaoi, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(countUnitsNamed(P1, "Tentacle"), 1);
    fireTriggerAs(kIllaoi, P1, illaoi, TriggerType::WhenIHold, exec);
    EXPECT_EQ(countUnitsNamed(P1, "Tentacle"), 2);
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Tentacle") continue;
        EXPECT_TRUE(obj.isToken());
        EXPECT_EQ(obj.base_might, 1);
    }
}

TEST_F(VendettaCTest, Vendetta_IllaoiProphet_MightPerTokenUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kIllaoi);
    auto illaoi = addUnit(P1, kIllaoi, 0, 0);
    addUnit(P1, kInvalidId, 2, 0);                               // non-token: doesn't count
    auto enemy_token = addUnit(P2, kInvalidId, 1, 0);
    state.getObject(enemy_token).super_type = SuperType::Token;  // not mine

    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(illaoi)), 0);

    clearAuras();
    fireTriggerAs(kIllaoi, P1, illaoi, TriggerType::WhenYouPlayMe, exec);
    fireTriggerAs(kIllaoi, P1, illaoi, TriggerType::WhenIConquer, exec);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(illaoi)), 2);
}

// ── 897 Mel, Defiant Soul ────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_MelDefiantSoul_EmpowerCostIsDiscardASpell) {
    const Card* c = card_registry.get(kMel);
    auto abs = c->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].is_empower);
    EXPECT_TRUE(abs[0].cost.discard);
    EXPECT_EQ(abs[0].cost.discard_count, 1);
    EXPECT_EQ(abs[0].cost.discard_type_filter, 1 + static_cast<int>(CardType::Spell));
}

TEST_F(VendettaCTest, Vendetta_MelDefiantSoul_BanishesSmallEnemyAtBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto mel = addUnit(P1, kMel, 0, 0);
    auto big = addUnit(P2, kInvalidId, 4, 0);        // 4 [M]: too big
    auto in_base = addUnit(P2, kInvalidId, 3);       // not at a battlefield
    auto mine = addUnit(P1, kInvalidId, 1, 1);       // not an enemy
    auto small = addUnit(P2, kInvalidId, 3, 1);      // the only legal choice

    fireTriggerAs(kMel, P1, mel, TriggerType::WhenIBecomeEmpowered, exec);
    EXPECT_EQ(state.getObject(small).zone, ZoneType::Banishment);
    EXPECT_TRUE(state.getObject(big).location.has_value());
    EXPECT_TRUE(state.getObject(in_base).location.has_value());
    EXPECT_TRUE(state.getObject(mine).location.has_value());
}

// ── 898 Minah Swiftfoot ──────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_MinahSwiftfoot_EachPlayerDraws) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_registry.get(kMinah)->firesOn(TriggerType::WhenIMoveToFB));
    auto minah = addUnit(P1, kMinah, 0, 0);
    addToDeck(P1, kOnslaught);
    addToDeck(P2, kOnslaught);
    addToHand(P1, kOnslaught);
    addToHand(P2, kOnslaught);

    driveResumableTrigger(kMinah, P1, minah, answer(1), exec, TriggerType::WhenIMoveToFB);
    EXPECT_EQ(handSize(P1), 2);
    EXPECT_EQ(handSize(P2), 2);
    EXPECT_EQ(trashSize(P1), 0);
}

TEST_F(VendettaCTest, Vendetta_MinahSwiftfoot_EachPlayerDiscards) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto minah = addUnit(P1, kMinah, 0, 0);
    addToDeck(P1, kOnslaught);
    addToDeck(P2, kOnslaught);
    addToHand(P1, kOnslaught);
    addToHand(P2, kOnslaught);

    driveResumableTrigger(kMinah, P1, minah, answer(0), exec, TriggerType::WhenIMoveToFB);
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_EQ(handSize(P2), 0);
    EXPECT_EQ(trashSize(P1), 1);
    EXPECT_EQ(trashSize(P2), 1);
    EXPECT_EQ(deckSize(P1), 1) << "the discard mode draws nothing";
}

// ── 899 Zed, Without a Sound ─────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_ZedWithoutASound_ConquerMakesCloneInBase) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto zed = addUnit(P1, kZed, 0, 0);
    fireTriggerAs(kZed, P1, zed, TriggerType::WhenIConquer, exec);
    int clones = 0;
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Shadow Clone") continue;
        ++clones;
        EXPECT_TRUE(obj.isToken());
        EXPECT_EQ(obj.base_might, 0);
        EXPECT_TRUE(obj.isAtBase());
        EXPECT_EQ(obj.controller, P1);
    }
    EXPECT_EQ(clones, 1);
}

TEST_F(VendettaCTest, Vendetta_ZedWithoutASound_SwapsWithClone) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* c = card_registry.get(kZed);
    auto abs = c->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].is_action);
    EXPECT_EQ(abs[0].cost.energy, 1);
    EXPECT_EQ(abs[0].cost.power, 1);
    EXPECT_EQ(abs[0].cost.power_domain, Domain::Chaos);
    EXPECT_FALSE(abs[0].cost.exhaust);

    auto zed = addUnit(P1, kZed, 0, 0);
    auto other = addUnit(P1, kInvalidId, 2);          // not a Shadow Clone
    EXPECT_TRUE(c->enumerateLegalTargets(state, P1, 0).empty()) << "no clone, no ability";

    fireTriggerAs(kZed, P1, zed, TriggerType::WhenIConquer, exec);
    auto legal = c->enumerateLegalTargets(state, P1, 0);
    ASSERT_EQ(legal.size(), 1u);
    const GameObjectId clone = legal[0];
    EXPECT_NE(clone, other);

    CardContext ctx{state, events, exec, P1, zed};
    c->onActivate(ctx, 0, {clone});
    EXPECT_TRUE(state.getObject(zed).isAtBase());
    ASSERT_TRUE(state.getObject(clone).battlefieldId().has_value());
    EXPECT_EQ(*state.getObject(clone).battlefieldId(), BattlefieldId::fromRaw(0));

    // A non-clone target is rejected.
    c->onActivate(ctx, 0, {other});
    EXPECT_TRUE(state.getObject(zed).isAtBase());
    EXPECT_TRUE(state.getObject(other).isAtBase());
}

// ── 900 Kennen, Storm of Shuriken ────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_KennenStorm_BurnsTwoOnPlay) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto kennen = addUnit(P1, kKennenStorm, 0, 0);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kOnslaught);
    fireTriggerAs(kKennenStorm, P1, kennen, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(deckSize(P1), 1);
    EXPECT_EQ(trashSize(P1), 2);
    EXPECT_EQ(state.player(P1).cards_burned_this_turn, 2);
}

TEST_F(VendettaCTest, Vendetta_KennenStorm_ConquerGrantsFlowThisTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    state.turn.turn_number = 5;
    auto kennen = addUnit(P1, kKennenStorm, 0, 0);
    auto spell = addToTrash(P1, kOnslaught);
    auto unit = addToTrash(P1, kPunchingPoro);        // not a spell
    for (int i = 0; i < 3; ++i) addToDeck(P1, kOnslaught);

    fireTriggerAs(kKennenStorm, P1, kennen, TriggerType::WhenIConquer, exec);
    EXPECT_EQ(deckSize(P1), 3) << "conquering does not burn";
    auto& counters = state.getObject(spell).card_counters;
    ASSERT_EQ(counters.count("__flow_granted_turn"), 1u);
    EXPECT_EQ(counters["__flow_granted_turn"], 5);
    EXPECT_EQ(state.getObject(unit).card_counters.count("__flow_granted_turn"), 0u);
}

// ── 901 Kharox ───────────────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_Kharox_BurnsThreeThenStealsUnit) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kKharox);
    auto ab = c->activatedAbilities().at(c->empowerAbilityIndex());
    EXPECT_EQ(ab.cost.energy, 6);
    EXPECT_EQ(ab.cost.power, 2);
    EXPECT_EQ(ab.cost.power_domain, Domain::Chaos);

    auto kharox = addUnit(P1, kKharox, 0, 0);
    for (int i = 0; i < 3; ++i) addToDeck(P2, kOnslaught);
    addToDeck(P2, kOnslaught);
    addToDeck(P2, kOnslaught);
    auto unit = addToDeck(P2, kPunchingPoro);         // top of deck

    // Through the engine: the text after "do this" is a reflexive trigger (CR
    // 387-389), a chain item of its own that only the engine queues and resolves.
    // "Then you MAY do this": yes, and that unit.
    EnginePathAgent engine_agent(answer(1, unit));
    fireTriggerThroughEngine(kharox, TriggerType::WhenIBecomeEmpowered, &engine_agent);
    EXPECT_EQ(deckSize(P2), 3);
    EXPECT_EQ(state.player(P2).cards_burned_this_turn, 3);
    EXPECT_FALSE(inTrash(P2, unit));
    EXPECT_EQ(state.getObject(unit).controller, P1);
    EXPECT_TRUE(state.getObject(unit).isAtBase());
    EXPECT_EQ(trashSize(P2), 2);
}

TEST_F(VendettaCTest, Vendetta_Kharox_DeclineStillBurnsExactlyThree) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto kharox = addUnit(P1, kKharox, 0, 0);
    for (int i = 0; i < 5; ++i) addToDeck(P2, kOnslaught);
    auto unit = addToDeck(P2, kPunchingPoro);

    driveResumableTrigger(kKharox, P1, kharox, answer(0), exec,
                          TriggerType::WhenIBecomeEmpowered);
    EXPECT_EQ(deckSize(P2), 3) << "re-entry after the choice must not burn again";
    EXPECT_TRUE(inTrash(P2, unit));
    EXPECT_EQ(state.getObject(unit).controller, P2);
}

TEST_F(VendettaCTest, Vendetta_Kharox_AcceptThroughResumeLoop) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto kharox = addUnit(P1, kKharox, 0, 0);
    for (int i = 0; i < 5; ++i) addToDeck(P2, kOnslaught);
    auto unit = addToDeck(P2, kPunchingPoro);

    // Through the engine: the text after "do this" is a reflexive trigger (CR
    // 387-389), a chain item of its own that only the engine queues and resolves.
    EnginePathAgent engine_agent(answer(1, unit));
    fireTriggerThroughEngine(kharox, TriggerType::WhenIBecomeEmpowered, &engine_agent);
    EXPECT_EQ(deckSize(P2), 3);
    EXPECT_EQ(state.getObject(unit).controller, P1);
    EXPECT_TRUE(state.getObject(unit).isAtBase());
}

// ── 902 Ocean Drake ──────────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_OceanDrake_ReturnsNonDragonToOwnersHand) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_NE(card_db.get(kOceanDrake).ability_text.find("play me to an open battlefield"),
              std::string::npos) << "the engine keys the open-battlefield play off this text";
    auto drake = addUnit(P1, kOceanDrake, 0, 0);
    auto enemy = addUnit(P2, kPunchingPoro, 0, 0);

    fireTriggerAs(kOceanDrake, P1, drake, TriggerType::WhenYouPlayMe, exec);
    EXPECT_TRUE(inHand(P2, enemy));
    EXPECT_TRUE(state.getObject(drake).location.has_value()) << "Dragons are not legal";
}

TEST_F(VendettaCTest, Vendetta_OceanDrake_OnlyDragonsOrDeclined) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto drake = addUnit(P1, kOceanDrake, 0, 0);
    auto other_drake = addUnit(P2, kOceanDrake, 0, 1);
    fireTriggerAs(kOceanDrake, P1, drake, TriggerType::WhenYouPlayMe, exec);
    EXPECT_TRUE(state.getObject(other_drake).location.has_value());
    EXPECT_EQ(handSize(P2), 0);

    auto enemy = addUnit(P2, kPunchingPoro, 0, 0);
    driveResumableTrigger(kOceanDrake, P1, drake, answer(0, enemy), exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(inHand(P2, enemy)) << "\"you may\" — declined";
}

// ── 904 Disciple of Shen ─────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_DiscipleOfShen_ShieldWithExactlyOneOtherFriendly) {
    const Card* c = card_registry.get(kDiscipleOfShen);
    auto disciple = addUnit(P1, kDiscipleOfShen, 0, 0);
    addUnit(P2, kInvalidId, 2, 0);                    // enemies don't count

    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(hasAuraKeyword(state.getObject(disciple), Keyword::Shield, 3)) << "alone";

    clearAuras();
    addUnit(P1, kInvalidId, 2, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(hasAuraKeyword(state.getObject(disciple), Keyword::Shield, 3));
    EXPECT_EQ(auraMightSum(state.getObject(disciple)), 0) << "Shield only adds Might defending";

    clearAuras();
    state.getObject(disciple).combat_designation = CombatDesignation::Defender;
    c->applyPassiveAura(state, P1);
    // The +3 [M] while defending is the engine's aura fold of keyword_value
    // (CR 814) — AurasCostsTest.AuraKeywordHelper_*; the effect carries none.
    EXPECT_TRUE(hasAuraKeyword(state.getObject(disciple), Keyword::Shield, 3));
    EXPECT_EQ(auraMightSum(state.getObject(disciple)), 0);

    clearAuras();
    addUnit(P1, kInvalidId, 2, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(hasAuraKeyword(state.getObject(disciple), Keyword::Shield, 3)) << "two others";
}

TEST_F(VendettaCTest, Vendetta_DiscipleOfShen_NoShieldInBase) {
    const Card* c = card_registry.get(kDiscipleOfShen);
    auto disciple = addUnit(P1, kDiscipleOfShen);     // base
    addUnit(P1, kInvalidId, 2);                       // exactly one other, but at base
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(hasAuraKeyword(state.getObject(disciple), Keyword::Shield, 3));
}

// ── 905 Horns of the Dragon / 910 Soulspinner (keyword-only) ─────────────

TEST_F(VendettaCTest, Vendetta_KeywordOnlyUnits) {
    EXPECT_TRUE(card_db.get(kHornsOfDragon).keywords.has(Keyword::Tank));
    EXPECT_EQ(card_db.get(kHornsOfDragon).might, 6);
    EXPECT_TRUE(card_db.get(kSoulspinner).keywords.has(Keyword::Ambush));
    EXPECT_TRUE(card_registry.get(kHornsOfDragon)->triggerTypes().empty());
    EXPECT_TRUE(card_registry.get(kSoulspinner)->activatedAbilities().empty());
}

// ── 906 Keeper of Law ────────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_KeeperOfLaw_CheaperWithExactlyTwoUnitsAtControlledBattlefield) {
    const Card* c = card_registry.get(kKeeperOfLaw);
    EXPECT_EQ(c->selfCostReduction(state, P1), 0);

    addUnit(P1, kInvalidId, 2, 0);
    addUnit(P1, kInvalidId, 2, 0);
    EXPECT_EQ(c->selfCostReduction(state, P1), 0) << "battlefield not controlled";

    state.battlefields[0].controller = P1;
    EXPECT_EQ(c->selfCostReduction(state, P1), 2);
    EXPECT_EQ(c->selfCostReduction(state, P2), 0) << "the opponent doesn't control it";

    addUnit(P1, kInvalidId, 2, 0);
    EXPECT_EQ(c->selfCostReduction(state, P1), 0) << "three units is not exactly two";
}

// ── 907 Masa, Crashing Thunder ───────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_Masa_PaidStunsEnemyAtBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto cost = card_registry.get(kMasa)->optionalAdditionalCost();
    EXPECT_TRUE(cost.valid);
    EXPECT_EQ(cost.energy, 0);
    EXPECT_EQ(cost.power, 1);
    EXPECT_EQ(cost.power_domain, Domain::Order);

    auto masa = addUnit(P1, kMasa, 0, 0);
    auto in_base = addUnit(P2, kInvalidId, 2);        // not at a battlefield
    auto at_bf = addUnit(P2, kInvalidId, 2, 1);

    fireTriggerAs(kMasa, P1, masa, TriggerType::WhenYouPlayMe, exec);
    EXPECT_FALSE(state.getObject(at_bf).is_stunned) << "additional cost not paid";

    state.getObject(masa).card_counters[cost.paid_flag] = 1;
    fireTriggerAs(kMasa, P1, masa, TriggerType::WhenYouPlayMe, exec);
    EXPECT_TRUE(state.getObject(at_bf).is_stunned);
    EXPECT_FALSE(state.getObject(in_base).is_stunned);
    EXPECT_FALSE(state.getObject(masa).is_stunned);
}

// ── 908 Reluctant Leader ─────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_ReluctantLeader_PlusTwoWhenAnotherUnitPlayed) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kReluctantLeader);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenYouPlayAUnit));
    EXPECT_FALSE(c->firesOn(TriggerType::WhenYouPlayMe)) << "\"another unit\"";

    auto leader = addUnit(P1, kReluctantLeader, 0, 0);
    const int before = state.getObject(leader).current_might;
    fireTriggerAs(kReluctantLeader, P1, leader, TriggerType::WhenYouPlayAUnit, exec);
    EXPECT_EQ(state.getObject(leader).temp_might_bonus, 2) << "this turn only";
    EXPECT_EQ(state.getObject(leader).current_might, before + 2);
}

// ── 909 Solari Sunhawk ───────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_SolariSunhawk_EmpoweredMightAndDeflect) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kSolariSunhawk);
    EXPECT_EQ(c->activatedAbilities().at(c->empowerAbilityIndex()).cost.energy, 2);
    auto hawk = addUnit(P1, kSolariSunhawk, 0, 0);

    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(hawk)), 0);
    EXPECT_FALSE(hasAuraKeyword(state.getObject(hawk), Keyword::Deflect, 2));

    clearAuras();
    exec.empower(hawk);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(hawk)), 1);
    EXPECT_TRUE(hasAuraKeyword(state.getObject(hawk), Keyword::Deflect, 2));
}

// ── 911 Escaped Grayback ─────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_EscapedGrayback_EmpowerCostKillsAFriendlyUnit) {
    const Card* c = card_registry.get(kEscapedGrayback);
    auto abs = c->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_TRUE(abs[0].is_empower);
    EXPECT_TRUE(abs[0].cost.kill_friendly_unit);
    EXPECT_EQ(abs[0].targets.count, 1) << "the unit to kill is the ability's target";
    EXPECT_EQ(c->empowerAbilityIndex(), 0);

    auto grayback = addUnit(P1, kEscapedGrayback, 0, 0);
    auto friendly = addUnit(P1, kInvalidId, 1);
    auto enemy = addUnit(P2, kInvalidId, 1, 0);
    auto legal = c->enumerateLegalTargets(state, P1, 0);
    EXPECT_NE(std::find(legal.begin(), legal.end(), friendly), legal.end());
    EXPECT_NE(std::find(legal.begin(), legal.end(), grayback), legal.end());
    EXPECT_EQ(std::find(legal.begin(), legal.end(), enemy), legal.end());
}

TEST_F(VendettaCTest, Vendetta_EscapedGrayback_EmpoweredPlusTwo) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kEscapedGrayback);
    auto grayback = addUnit(P1, kEscapedGrayback, 0, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(grayback)), 0);
    clearAuras();
    exec.empower(grayback);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(grayback)), 2);
}

// ── 912 Hungry Wolf ──────────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_HungryWolf_OnlyAfterChoosingAnEnemyUnit) {
    const Card* c = card_registry.get(kHungryWolf);
    auto abs = c->activatedAbilities();
    ASSERT_EQ(abs.size(), 1u);
    EXPECT_EQ(abs[0].cost.power, 1);
    EXPECT_EQ(abs[0].cost.power_domain, Domain::Order);
    EXPECT_EQ(abs[0].cost.energy, 0);
    EXPECT_FALSE(abs[0].cost.exhaust);

    EXPECT_FALSE(c->canActivateAbility(state, P1));
    state.player(P1).chose_enemy_unit_this_turn = true;
    EXPECT_TRUE(c->canActivateAbility(state, P1));
    EXPECT_FALSE(c->canActivateAbility(state, P2));
}

TEST_F(VendettaCTest, Vendetta_HungryWolf_EnabledByChoosingAnEnemyUnitWithAnAbility) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* wolf_card = card_registry.get(kHungryWolf);
    auto kennen = addUnit(P1, kKennenBalance, 0, 0);
    auto ally = addUnit(P1, kInvalidId, 3, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    for (int i = 0; i < 4; ++i) addRune(P1, Domain::Order);

    // Choosing a friendly unit does not count.
    driveResumableTrigger(kKennenBalance, P1, kennen, answer(1, ally), exec,
                          TriggerType::WhenIAttack);
    EXPECT_FALSE(wolf_card->canActivateAbility(state, P1));

    // Choosing an enemy unit (Card::pickTarget) does.
    driveResumableTrigger(kKennenBalance, P1, kennen, answer(1, enemy), exec,
                          TriggerType::WhenIAttack);
    EXPECT_TRUE(wolf_card->canActivateAbility(state, P1));
    EXPECT_FALSE(wolf_card->canActivateAbility(state, P2));
}

TEST_F(VendettaCTest, Vendetta_HungryWolf_ReadiesAndBuffsOncePerTurn) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    Card* c = card_registry.get(kHungryWolf);
    auto wolf = addUnit(P1, kHungryWolf, 0, 0);
    auto other_wolf = addUnit(P1, kHungryWolf, 0, 0);
    state.getObject(wolf).is_exhausted = true;
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, wolf, 0));

    CardContext ctx{state, events, exec, P1, wolf};
    state.getObject(wolf).card_counters["__ability_used_turn_0"] = state.turn.turn_number + 1;   // as GameEngine does on activation
    c->onActivate(ctx, 0, {});
    EXPECT_FALSE(state.getObject(wolf).is_exhausted);
    EXPECT_EQ(state.getObject(wolf).temp_might_bonus, 1);
    EXPECT_FALSE(c->canActivateAbilityAt(state, P1, wolf, 0)) << "only once each turn";
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, other_wolf, 0)) << "per Hungry Wolf";

    state.turn.turn_number++;
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, wolf, 0)) << "usable again next turn";
}

// ── 915 Noxian Emissary ──────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_NoxianEmissary_DeathknellOnlyWhileEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kNoxianEmissary);
    auto ab = c->activatedAbilities().at(c->empowerAbilityIndex());
    EXPECT_EQ(ab.cost.energy, 1);
    EXPECT_EQ(ab.cost.power, 1);
    EXPECT_EQ(ab.cost.power_domain, Domain::Order);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIDie));

    // Dies without ever being Empowered: no Recruits.
    auto plain = addUnit(P1, kNoxianEmissary, 0, 0);
    exec.killObject(plain);
    fireTriggerAs(kNoxianEmissary, P1, plain, TriggerType::WhenIDie, exec);
    EXPECT_EQ(countUnitsNamed(P1, "Recruit"), 0);

    // Empowered on the board, then dies. The engine clears the status once the
    // object is off the board; the Deathknell reads the kill path's
    // last-known stamp (card_counters["__died_empowered"]).
    auto emp = addUnit(P1, kNoxianEmissary, 0, 0);
    exec.empower(emp);
    exec.killObject(emp);
    EXPECT_GT(state.getObject(emp).card_counters["__died_empowered"], 0);
    state.getObject(emp).empowered = 0;               // what recalculateAuras does
    fireTriggerAs(kNoxianEmissary, P1, emp, TriggerType::WhenIDie, exec);
    EXPECT_EQ(countUnitsNamed(P1, "Recruit"), 2);
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Recruit") continue;
        EXPECT_TRUE(obj.isAtBase());
        EXPECT_EQ(obj.base_might, 1);
    }
}

TEST_F(VendettaCTest, Vendetta_NoxianEmissary_DisempoweredBeforeDyingGetsNothing) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto emissary = addUnit(P1, kNoxianEmissary, 0, 0);
    exec.empower(emissary);
    exec.disempower(emissary);
    exec.killObject(emissary);                        // stamps __died_empowered = 0
    fireTriggerAs(kNoxianEmissary, P1, emissary, TriggerType::WhenIDie, exec);
    EXPECT_EQ(countUnitsNamed(P1, "Recruit"), 0);
}

// ── 916 Sacred Protector ─────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_SacredProtector_DealsCombatDamageOnlyWithExactlyOneOther) {
    const Card* c = card_registry.get(kSacredProtector);
    auto protector = addUnit(P1, kSacredProtector, 0, 0);
    addUnit(P2, kInvalidId, 2, 0);                    // enemy: doesn't count

    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(suppressesCombatDamage(state.getObject(protector))) << "alone";

    clearAuras();
    addUnit(P1, kInvalidId, 2, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(suppressesCombatDamage(state.getObject(protector))) << "exactly one other";

    clearAuras();
    addUnit(P1, kInvalidId, 2, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(suppressesCombatDamage(state.getObject(protector))) << "two others";
}

TEST_F(VendettaCTest, Vendetta_SacredProtector_SuppressedInBase) {
    const Card* c = card_registry.get(kSacredProtector);
    auto protector = addUnit(P1, kSacredProtector);   // base
    addUnit(P1, kInvalidId, 2);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(suppressesCombatDamage(state.getObject(protector)));
}

// ── 917 Aurok General ────────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_AurokGeneral_BuffsFriendlyEmpoweredUnits) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kAurokGeneral);
    auto ab = c->activatedAbilities().at(c->empowerAbilityIndex());
    EXPECT_EQ(ab.cost.energy, 3);
    EXPECT_EQ(ab.cost.power, 1);
    EXPECT_EQ(ab.cost.power_domain, Domain::Order);

    auto general = addUnit(P1, kAurokGeneral, 0, 0);
    auto emp_ally = addUnit(P1, kPunchingPoro, 0, 1);
    auto plain_ally = addUnit(P1, kInvalidId, 2, 0);
    auto emp_enemy = addUnit(P2, kPunchingPoro, 0, 0);
    exec.empower(emp_ally);
    exec.empower(emp_enemy);

    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(emp_ally)), 0) << "the General is not Empowered";

    clearAuras();
    exec.empower(general);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(general)), 2) << "including me";
    EXPECT_EQ(auraMightSum(state.getObject(emp_ally)), 2);
    EXPECT_EQ(auraMightSum(state.getObject(plain_ally)), 0);
    EXPECT_EQ(auraMightSum(state.getObject(emp_enemy)), 0);
}

// ── 919 Fallen Feline ────────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_FallenFeline_NamedSpellLockedWhileAtBattlefield) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kFallenFeline);
    auto feline = addUnit(P1, kFallenFeline, 0, 0);

    // "Name a spell": any spell, whether or not the opponent has shown one.
    size_t offered = 0;
    bool unit_offered = false;
    driveResumableTrigger(kFallenFeline, P1, feline,
        [&](const std::vector<Intent>& legal) {
            offered = legal.size();
            for (const auto& i : legal)
                if (i.chosen_card_def == kPunchingPoro) unit_offered = true;
            for (const auto& i : legal)
                if (i.chosen_card_def == kOnslaught) return i;
            return legal.front();
        }, exec, TriggerType::WhenYouPlayMe);
    EXPECT_GT(offered, 100u);
    EXPECT_FALSE(unit_offered) << "not a spell: can't be named";
    EXPECT_EQ(state.getObject(feline).string_state["named_spell"], "Onslaught");

    c->applyPassiveAura(state, P1);
    auto& locked = state.player(P2).locked_spell_names;
    ASSERT_EQ(locked.size(), 1u);
    EXPECT_EQ(locked[0], "Onslaught");
    EXPECT_TRUE(state.player(P1).locked_spell_names.empty()) << "only opponents are locked";
}

TEST_F(VendettaCTest, Vendetta_FallenFeline_NoLockInBaseOrUnnamed) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kFallenFeline);
    auto feline = addUnit(P1, kFallenFeline);         // base
    driveResumableTrigger(kFallenFeline, P1, feline,
        [&](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (i.chosen_card_def == kOnslaught) return i;
            return legal.front();
        }, exec, TriggerType::WhenYouPlayMe);
    ASSERT_EQ(state.getObject(feline).string_state["named_spell"], "Onslaught");
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P2).locked_spell_names.empty()) << "not at a battlefield";

    auto unnamed = addUnit(P1, kFallenFeline, 0, 1);  // at a battlefield, nothing named
    clearAuras();
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P2).locked_spell_names.empty());
    EXPECT_EQ(state.getObject(unnamed).string_state.count("named_spell"), 0u);
}

// ── 921 Kayle, Justified ─────────────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_KayleJustified_EmpowersUpToThreeTimes) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kKayle);
    EXPECT_EQ(c->maxEmpower(), 3);
    const int idx = c->empowerAbilityIndex();
    ASSERT_GE(idx, 0);
    EXPECT_EQ(c->activatedAbilities()[idx].cost.energy, 3);

    auto kayle = addUnit(P1, kKayle, 0, 0);
    EXPECT_TRUE(exec.empower(kayle));
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, kayle, idx)) << "may Empower while Empowered";
    EXPECT_TRUE(exec.empower(kayle));
    EXPECT_TRUE(exec.empower(kayle));
    EXPECT_FALSE(exec.empower(kayle)) << "capped at three";
    EXPECT_EQ(state.getObject(kayle).empowered, 3);
    // CR 827.1.c.5: an object that may be Empowered multiple times ignores
    // "Play only if not Empowered" — the ability can be activated even at
    // its maximum, where it does nothing.
    EXPECT_TRUE(c->canActivateAbilityAt(state, P1, kayle, idx));
}

TEST_F(VendettaCTest, Vendetta_KayleJustified_MightPerEmpowerAndThreeStackKeywords) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kKayle);
    auto kayle = addUnit(P1, kKayle, 0, 0);

    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(kayle)), 0);

    clearAuras();
    exec.empower(kayle);
    exec.empower(kayle);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(kayle)), 4);
    EXPECT_FALSE(hasAuraKeyword(state.getObject(kayle), Keyword::Deflect, 3));
    EXPECT_FALSE(hasAuraKeyword(state.getObject(kayle), Keyword::Ganking, 0));

    clearAuras();
    exec.empower(kayle);
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(kayle)), 6);
    EXPECT_TRUE(hasAuraKeyword(state.getObject(kayle), Keyword::Deflect, 3));
    EXPECT_TRUE(hasAuraKeyword(state.getObject(kayle), Keyword::Ganking, 0));
}

// ── 922 Kennen, Keeper of Balance ────────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_KennenKeeperOfBalance_PaysTwoToStun) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kKennenBalance);
    EXPECT_TRUE(c->firesOn(TriggerType::WhenYouPlayMe));
    EXPECT_TRUE(c->firesOn(TriggerType::WhenIAttack));

    auto kennen = addUnit(P1, kKennenBalance, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    for (int i = 0; i < 3; ++i) addRune(P1, Domain::Order);

    driveResumableTrigger(kKennenBalance, P1, kennen, answer(1, enemy), exec,
                          TriggerType::WhenIAttack);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
    EXPECT_EQ(readyRuneCount(P1), 1) << "paid exactly [2], once";
    EXPECT_FALSE(state.getObject(kennen).is_stunned);

    // Floating Energy is spent before runes are exhausted.
    auto second = addUnit(P2, kInvalidId, 3, 0);
    state.getObject(enemy).is_stunned = false;
    state.player(P1).rune_pool.energy = 1;
    driveResumableTrigger(kKennenBalance, P1, kennen, answer(1, second), exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_TRUE(state.getObject(second).is_stunned);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(readyRuneCount(P1), 0);
}

TEST_F(VendettaCTest, Vendetta_KennenKeeperOfBalance_DeclineOrCantPay) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto kennen = addUnit(P1, kKennenBalance, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    addRune(P1, Domain::Order);                        // only [1] available

    driveResumableTrigger(kKennenBalance, P1, kennen, answer(1, enemy), exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(state.getObject(enemy).is_stunned) << "can't pay [2]";
    EXPECT_EQ(readyRuneCount(P1), 1);

    addRune(P1, Domain::Order);
    driveResumableTrigger(kKennenBalance, P1, kennen, answer(0, enemy), exec,
                          TriggerType::WhenYouPlayMe);
    EXPECT_FALSE(state.getObject(enemy).is_stunned) << "declined";
    EXPECT_EQ(readyRuneCount(P1), 2);
}

TEST_F(VendettaCTest, Vendetta_KennenKeeperOfBalance_PlusTwoWithStunnedEnemyHere) {
    const Card* c = card_registry.get(kKennenBalance);
    auto kennen = addUnit(P1, kKennenBalance, 0, 0);
    auto here = addUnit(P2, kInvalidId, 3, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 3, 1);
    auto friendly = addUnit(P1, kInvalidId, 3, 0);

    state.getObject(elsewhere).is_stunned = true;
    state.getObject(friendly).is_stunned = true;
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(kennen)), 0) << "no stunned ENEMY unit HERE";

    clearAuras();
    state.getObject(here).is_stunned = true;
    c->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMightSum(state.getObject(kennen)), 2);
}

// ── 923 Ambessa, Respected and Feared ────────────────────────────────────

TEST_F(VendettaCTest, Vendetta_Ambessa_EmpoweredAssault) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    const Card* c = card_registry.get(kAmbessa);
    auto ab = c->activatedAbilities().at(c->empowerAbilityIndex());
    EXPECT_EQ(ab.cost.energy, 1);
    EXPECT_EQ(ab.cost.power, 2);
    EXPECT_EQ(ab.cost.power_domain, Domain::Order);

    auto ambessa = addUnit(P1, kAmbessa, 0, 0);
    c->applyPassiveAura(state, P1);
    EXPECT_FALSE(hasAuraKeyword(state.getObject(ambessa), Keyword::Assault, 2));

    clearAuras();
    exec.empower(ambessa);
    state.getObject(ambessa).combat_designation = CombatDesignation::Attacker;
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(hasAuraKeyword(state.getObject(ambessa), Keyword::Assault, 2));
    // The +2 [M] while attacking is the engine's aura fold of keyword_value
    // (CR 807) — AurasCostsTest.AuraKeywordHelper_*; the effect carries none.
    EXPECT_EQ(auraMightSum(state.getObject(ambessa)), 0);
}

TEST_F(VendettaCTest, Vendetta_Ambessa_AttackKillsWeakerEnemyHereOnlyWhileEmpowered) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    auto ambessa = addUnit(P1, kAmbessa, 0, 0);       // 5 [M]
    auto equal = addUnit(P2, kInvalidId, 5, 0);       // not LESS Might
    auto weaker_elsewhere = addUnit(P2, kInvalidId, 1, 1);
    auto weaker_friend = addUnit(P1, kInvalidId, 1, 0);
    auto weaker = addUnit(P2, kInvalidId, 4, 0);

    // On the engine path: "[Empowered][>]" is the trigger condition
    // (Card::triggerCondition, asked by TriggerManager::queueTrigger), so for
    // an Ambessa who is not Empowered no ability goes on the chain.
    EnginePathAgent plain(EnginePathAgent::script({}));
    fireTriggerThroughEngine(ambessa, TriggerType::WhenIAttack, &plain);
    EXPECT_TRUE(plain.prompts.empty()) << "not Empowered: the ability does not trigger";
    EXPECT_TRUE(state.getObject(weaker).location.has_value()) << "not Empowered: no trigger";

    exec.empower(ambessa);
    EnginePathAgent empowered(EnginePathAgent::script({static_cast<int>(weaker.raw())}));
    fireTriggerThroughEngine(ambessa, TriggerType::WhenIAttack, &empowered);
    ASSERT_EQ(empowered.prompts.size(), 1u);
    EXPECT_EQ(empowered.prompts[0].size(), 1u)
        << "only the enemy unit here with less Might than her is offered";
    EXPECT_FALSE(state.getObject(weaker).location.has_value());
    EXPECT_TRUE(state.getObject(equal).location.has_value());
    EXPECT_TRUE(state.getObject(weaker_elsewhere).location.has_value());
    EXPECT_TRUE(state.getObject(weaker_friend).location.has_value());
}

// ── 925 Shen, Leader of the Kinkou Order ─────────────────────────────────

TEST_F(VendettaCTest, Vendetta_Shen_HoldScoresWithExactlyOneOtherFriendlyHere) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    EXPECT_TRUE(card_db.get(kShen).keywords.has(Keyword::Shield));
    auto shen = addUnit(P1, kShen, 0, 0);
    addUnit(P2, kInvalidId, 2, 0);                    // enemy: doesn't count
    addUnit(P1, kInvalidId, 2, 1);                    // friendly, but elsewhere

    fireTriggerAs(kShen, P1, shen, TriggerType::WhenIHold, exec);
    EXPECT_EQ(state.player(P1).score, 0) << "alone here";

    addUnit(P1, kInvalidId, 2, 0);
    fireTriggerAs(kShen, P1, shen, TriggerType::WhenIHold, exec);
    EXPECT_EQ(state.player(P1).score, 1);

    addUnit(P1, kInvalidId, 2, 0);
    fireTriggerAs(kShen, P1, shen, TriggerType::WhenIHold, exec);
    EXPECT_EQ(state.player(P1).score, 1) << "two others is not exactly one";
}

}  // namespace
}  // namespace test_vendetta_C
}  // namespace riftbound::test
