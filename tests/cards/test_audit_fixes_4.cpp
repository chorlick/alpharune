/// @file test_audit_fixes_4.cpp
/// Per-card tests for audit-fix batch 4 (src/cards/manual/audit_fixes_4.cpp).
///
///   46  En Garde            (spell)   — +1M, +1M more if only friendly unit there
///   220 Facebreaker         (spell)   — stun friendly + enemy at SAME battlefield
///   381 Ornn, Blacksmith    (unit)    — look-4, draft a gear, recycle rest (play/hold)
///   444 Corrupt Enforcer    (unit)    — move:discard1 + win-combat:draw1
///   498 Blade of Ruined King(gear)    — equip cost [Y] + kill a friendly unit
///   543 Sett, Brawler       (unit)    — played/conquer buff; spend buff +4M
///   612 Iascylla            (unit)    — hold -> schedule next-main-phase enemy pull
///   643 Keeper of Masks     (unit)    — two Reflection tokens copy me
///   676 Nidalee, Cat Form   (unit)    — win-combat draw 1
///   737 Tactical Retreat    (spell)   — heal + exhaust + recall friendly unit
///   766 Forbidding Waste    (battlefield) — defending-alone -2M aura

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kEnGarde         = 46;
constexpr CardDefId kFacebreaker     = 220;
constexpr CardDefId kOrnn            = 381;
constexpr CardDefId kCorruptEnforcer = 444;
constexpr CardDefId kBladeRuinedKing = 498;
constexpr CardDefId kSett            = 543;
constexpr CardDefId kIascylla        = 612;
constexpr CardDefId kKeeperOfMasks   = 643;
constexpr CardDefId kNidaleeCat      = 676;
constexpr CardDefId kTacticalRetreat = 737;
constexpr CardDefId kForbiddingWaste = 766;

class AuditFix4Test : public CardTestFixture {
protected:
    // Build a gear object on board (mirrors test_missing_card_fixes::addGear).
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.super_type = def.super_type;
            obj.keywords = def.keywords;
            obj.might_bonus = def.might_bonus;
        }
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    void fireTrigger(CardDefId def_id, GameObjectId source, PlayerId controller,
                     EffectExecutor& exec) {
        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};
        card->onTrigger(ctx, {});
    }
};

// ─── 46 En Garde ────────────────────────────────────────────────────────────

TEST_F(AuditFix4Test, EnGarde_OnlyUnitThere_GetsTwoMight) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    int before = state.getObject(unit).current_might;
    invokeOnResolve(/*source=*/unit, kEnGarde, P1, {unit}, exec);
    // +1 base, +1 because it's the only friendly unit at BF0.
    EXPECT_EQ(state.getObject(unit).current_might, before + 2);
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 2);
}

TEST_F(AuditFix4Test, EnGarde_NotAlone_GetsOnlyOneMight) {
    auto unit  = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);  // second friendly here
    EffectExecutor exec(state, events, card_db);
    int before = state.getObject(unit).current_might;
    invokeOnResolve(unit, kEnGarde, P1, {unit}, exec);
    EXPECT_EQ(state.getObject(unit).current_might, before + 1)
        << "no bonus +1 when another friendly unit shares the battlefield";
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 1);
}

TEST_F(AuditFix4Test, EnGarde_EnemyUnitHereDoesNotCount) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);  // enemy doesn't count
    EffectExecutor exec(state, events, card_db);
    int before = state.getObject(unit).current_might;
    invokeOnResolve(unit, kEnGarde, P1, {unit}, exec);
    EXPECT_EQ(state.getObject(unit).current_might, before + 2)
        << "enemy unit sharing the BF still leaves P1 'alone there'";
}

TEST_F(AuditFix4Test, EnGarde_NoTargetIsNoOp) {
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    invokeOnResolve(unit, kEnGarde, P1, {}, exec);  // empty targets
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 0);
}

// ─── 220 Facebreaker ────────────────────────────────────────────────────────

TEST_F(AuditFix4Test, Facebreaker_StunsBothAtSameBattlefield) {
    auto friendly = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto enemy    = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(/*source=*/friendly, kFacebreaker, P1, {}, exec);
    EXPECT_TRUE(state.getObject(friendly).is_stunned);
    EXPECT_TRUE(state.getObject(enemy).is_stunned);
}

TEST_F(AuditFix4Test, Facebreaker_NoPairWhenOnDifferentBattlefields) {
    auto friendly = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    auto enemy    = addUnit(P2, kInvalidId, 3, /*at_bf=*/1);  // different BF
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(friendly, kFacebreaker, P1, {}, exec);
    EXPECT_FALSE(state.getObject(friendly).is_stunned)
        << "no same-battlefield friendly+enemy pair -> no stun";
    EXPECT_FALSE(state.getObject(enemy).is_stunned);
}

TEST_F(AuditFix4Test, Facebreaker_HasLegalTargetsRequiresSharedBattlefield) {
    Card* c = card_registry.get(kFacebreaker);
    ASSERT_NE(c, nullptr);
    addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    addUnit(P2, kInvalidId, 3, /*at_bf=*/1);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
    // Now put the enemy at BF0 too -> legal.
    addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

TEST_F(AuditFix4Test, Facebreaker_NeedsPlayTimeTargetPair) {
    Card* c = card_registry.get(kFacebreaker);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->needsPlayTimeTargetPair());
}

// ─── 381 Ornn, Blacksmith ───────────────────────────────────────────────────

TEST_F(AuditFix4Test, Ornn_FiresOnPlayAndHold) {
    Card* c = card_registry.get(kOrnn);
    ASSERT_NE(c, nullptr);
    auto tt = c->triggerTypes();
    EXPECT_NE(std::find(tt.begin(), tt.end(), TriggerType::WhenYouPlayMe), tt.end());
    EXPECT_NE(std::find(tt.begin(), tt.end(), TriggerType::WhenIHold), tt.end());
}

TEST_F(AuditFix4Test, Ornn_DraftsGearFromTop4AndRecyclesRest) {
    auto ornn = addUnit(P1, kOrnn, /*might=*/5, /*at_bf=*/0);
    // Top of deck = back of vector. Put a gear among the top 4 plus 3 non-gear.
    auto nong1 = addToDeck(P1, kInvalidId);            // deeper
    auto nong2 = addToDeck(P1, kInvalidId);
    auto gear  = addToDeck(P1, kBladeRuinedKing);      // a gear (def 498)
    auto nong3 = addToDeck(P1, kInvalidId);            // top
    (void)nong1; (void)nong2; (void)nong3;
    int hand_before = handSize(P1);

    EffectExecutor exec(state, events, card_db);
    fireTrigger(kOrnn, ornn, P1, exec);

    EXPECT_EQ(handSize(P1), hand_before + 1) << "gear drafted into hand";
    EXPECT_TRUE(inHand(P1, gear));
    EXPECT_EQ(state.getObject(gear).zone, ZoneType::Hand);
    // The 3 non-gear cards were recycled to the bottom of the deck (not drawn).
    EXPECT_TRUE(inDeck(P1, nong1));
    EXPECT_TRUE(inDeck(P1, nong2));
    EXPECT_TRUE(inDeck(P1, nong3));
    EXPECT_FALSE(inHand(P1, nong3));
}

TEST_F(AuditFix4Test, Ornn_NoGearAmongTop4_DrawsNothingRecyclesAll) {
    auto ornn = addUnit(P1, kOrnn, 5, 0);
    auto a = addToDeck(P1, kInvalidId);
    auto b = addToDeck(P1, kInvalidId);
    int hand_before = handSize(P1);

    EffectExecutor exec(state, events, card_db);
    fireTrigger(kOrnn, ornn, P1, exec);

    EXPECT_EQ(handSize(P1), hand_before) << "no gear revealed -> draw nothing";
    EXPECT_TRUE(inDeck(P1, a));
    EXPECT_TRUE(inDeck(P1, b));
}

TEST_F(AuditFix4Test, Ornn_EmptyDeckIsNoOp) {
    auto ornn = addUnit(P1, kOrnn, 5, 0);
    EffectExecutor exec(state, events, card_db);
    int hand_before = handSize(P1);
    fireTrigger(kOrnn, ornn, P1, exec);
    EXPECT_EQ(handSize(P1), hand_before);
}

// ─── 444 Corrupt Enforcer ───────────────────────────────────────────────────

TEST_F(AuditFix4Test, CorruptEnforcer_DeclaresBothTriggers) {
    Card* c = card_registry.get(kCorruptEnforcer);
    ASSERT_NE(c, nullptr);
    auto tt = c->triggerTypes();
    EXPECT_NE(std::find(tt.begin(), tt.end(), TriggerType::WhenIMoveToFB), tt.end());
    EXPECT_NE(std::find(tt.begin(), tt.end(), TriggerType::WhenIWinCombat), tt.end());
}

TEST_F(AuditFix4Test, CorruptEnforcer_WinCombatDrawsOne) {
    auto enf = addUnit(P1, kCorruptEnforcer, /*might=*/4, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);  // something to draw
    int hand_before = handSize(P1);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kCorruptEnforcer, P1, enf, TriggerType::WhenIWinCombat, exec);
    EXPECT_EQ(handSize(P1), hand_before + 1);
}

TEST_F(AuditFix4Test, CorruptEnforcer_MoveToFBDiscardsOne) {
    auto enf = addUnit(P1, kCorruptEnforcer, /*might=*/4, /*at_bf=*/0);
    auto card = addToHand(P1, kInvalidId);
    int hand_before = handSize(P1);

    EffectExecutor exec(state, events, card_db);
    // Direct (non-resuming) fire -> discardThenAct falls back to discardCards.
    fireTriggerAs(kCorruptEnforcer, P1, enf, TriggerType::WhenIMoveToFB, exec);
    EXPECT_EQ(handSize(P1), hand_before - 1) << "moved to BF -> discard 1";
    EXPECT_TRUE(inTrash(P1, card));
}

// ─── 498 Blade of the Ruined King ───────────────────────────────────────────

TEST_F(AuditFix4Test, BladeRuinedKing_HasEquipAbility) {
    Card* c = card_registry.get(kBladeRuinedKing);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasEquipAbility());
}

TEST_F(AuditFix4Test, BladeRuinedKing_EquipKillsFriendlyAndRecyclesOrderRune) {
    auto bearer = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto victim = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);  // killable friendly
    auto rune   = addRune(P1, Domain::Order);
    auto gear   = addGear(P1, kBladeRuinedKing, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    Card* c = card_registry.get(kBladeRuinedKing);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = c->onEquip(ctx, bearer);

    EXPECT_TRUE(ok);
    EXPECT_EQ(state.getObject(gear).attached_to, bearer);
    EXPECT_EQ(state.getObject(bearer).attachment_might_bonus, 4)
        << "Blade grants +4 might_bonus";
    // Victim killed -> routed to trash.
    EXPECT_EQ(state.getObject(victim).zone, ZoneType::Trash)
        << "a friendly unit was killed to pay the cost";
    // Order rune was recycled out of the base (not ready anymore).
    EXPECT_EQ(state.getObject(rune).zone, ZoneType::RuneDeck);
}

TEST_F(AuditFix4Test, BladeRuinedKing_FailsWithoutOrderRune) {
    auto bearer = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);
    addUnit(P1, kInvalidId, 2, /*at_bf=*/0);     // a killable friendly exists
    addRune(P1, Domain::Chaos);                   // wrong domain — not [Y]
    auto gear = addGear(P1, kBladeRuinedKing, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    Card* c = card_registry.get(kBladeRuinedKing);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = c->onEquip(ctx, bearer);
    EXPECT_FALSE(ok) << "no Order rune -> can't pay [Y] -> equip fails";
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
    EXPECT_EQ(state.getObject(bearer).attachment_might_bonus, 0);
}

TEST_F(AuditFix4Test, BladeRuinedKing_FailsWithNoOtherFriendlyUnitToKill) {
    auto bearer = addUnit(P1, kInvalidId, 3, /*at_bf=*/0);  // only friendly unit
    addRune(P1, Domain::Order);
    auto gear = addGear(P1, kBladeRuinedKing, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    Card* c = card_registry.get(kBladeRuinedKing);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = c->onEquip(ctx, bearer);
    EXPECT_FALSE(ok) << "no friendly unit to kill (can't kill the equip target) -> fails";
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
}

// ─── 543 Sett, Brawler ──────────────────────────────────────────────────────

TEST_F(AuditFix4Test, Sett_FiresOnPlayAndConquer_NotHold) {
    Card* c = card_registry.get(kSett);
    ASSERT_NE(c, nullptr);
    auto tt = c->triggerTypes();
    EXPECT_NE(std::find(tt.begin(), tt.end(), TriggerType::WhenYouPlayMe), tt.end());
    EXPECT_NE(std::find(tt.begin(), tt.end(), TriggerType::WhenIConquer), tt.end());
    EXPECT_EQ(std::find(tt.begin(), tt.end(), TriggerType::WhenIHold), tt.end())
        << "Sett must NOT fire on hold (old impl over-fired)";
    EXPECT_EQ(std::find(tt.begin(), tt.end(), TriggerType::WhenIConquerOrHold),
              tt.end());
}

TEST_F(AuditFix4Test, Sett_GainsBuffWhenUnbuffed_OnlyOnce) {
    auto sett = addUnit(P1, kSett, /*might=*/4, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    EXPECT_EQ(state.getObject(sett).buff_count, 0);

    fireTriggerAs(kSett, P1, sett, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(state.getObject(sett).buff_count, 1) << "unbuffed -> gains a buff";

    // Already has a buff -> conquer trigger no-ops (stays at 1).
    fireTriggerAs(kSett, P1, sett, TriggerType::WhenIConquer, exec);
    EXPECT_EQ(state.getObject(sett).buff_count, 1)
        << "buffed already -> no new buff";
}

TEST_F(AuditFix4Test, Sett_SpendBuffGivesPlus4Might) {
    auto sett = addUnit(P1, kSett, /*might=*/4, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    Card* c = card_registry.get(kSett);
    EXPECT_TRUE(c->hasActivatedAbility());

    // Give a buff first.
    fireTriggerAs(kSett, P1, sett, TriggerType::WhenYouPlayMe, exec);
    ASSERT_EQ(state.getObject(sett).buff_count, 1);
    int might_before = state.getObject(sett).current_might;  // 4 + 1 buff = 5

    CardContext ctx{state, events, exec, P1, sett};
    c->onActivate(ctx, {});
    // Observable might: -1 (buff consumed via buff_count--) then +4 = +3 net.
    EXPECT_EQ(state.getObject(sett).current_might, might_before - 1 + 4);
    // The permanent buff is properly spent now that temp might (the +4) lives
    // in temp_might_bonus, not buff_count — so buff_count returns to 0 and the
    // ability can't be re-activated until Sett gains another buff.
    EXPECT_EQ(state.getObject(sett).buff_count, 0);
}

TEST_F(AuditFix4Test, Sett_ActivateWithoutBuffIsNoOp) {
    auto sett = addUnit(P1, kSett, /*might=*/4, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    Card* c = card_registry.get(kSett);
    int might_before = state.getObject(sett).current_might;
    CardContext ctx{state, events, exec, P1, sett};
    c->onActivate(ctx, {});
    EXPECT_EQ(state.getObject(sett).buff_count, 0);
    EXPECT_EQ(state.getObject(sett).current_might, might_before)
        << "no buff to spend -> no +4M";
}

// ─── 612 Iascylla ───────────────────────────────────────────────────────────

TEST_F(AuditFix4Test, Iascylla_HoldSchedulesDelayedAbility) {
    auto ias = addUnit(P1, kIascylla, /*might=*/6, /*at_bf=*/0);
    addUnit(P2, kInvalidId, 3, /*at_bf=*/1);  // an enemy exists somewhere

    EffectExecutor exec(state, events, card_db);
    EXPECT_TRUE(state.delayed_abilities.empty());
    fireTriggerAs(kIascylla, P1, ias, TriggerType::WhenIHold, exec);

    ASSERT_EQ(state.delayed_abilities.size(), 1u)
        << "hold schedules a deferred next-Main-Phase pull (does NOT move now)";
    const auto& da = state.delayed_abilities.front();
    EXPECT_EQ(da.trigger, TriggerType::AtStartOfMain);
    EXPECT_EQ(da.source, ias);
    EXPECT_EQ(da.controller, P1);
    EXPECT_EQ(da.expires_on_turn, -1);
    // The triggering battlefield was stashed.
    EXPECT_EQ(state.getObject(ias).card_counters["__iascylla_pull_bf"], 0);
}

TEST_F(AuditFix4Test, Iascylla_HoldDoesNotMoveEnemyImmediately) {
    auto ias   = addUnit(P1, kIascylla, 6, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/1);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kIascylla, P1, ias, TriggerType::WhenIHold, exec);
    EXPECT_EQ(state.getObject(enemy).battlefieldId(), std::optional<BattlefieldId>(1))
        << "enemy stays put until the deferred fire at next Main Phase";
}

TEST_F(AuditFix4Test, Iascylla_DeferredFirePullsEnemyToBattlefield) {
    auto ias   = addUnit(P1, kIascylla, 6, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/1);

    EffectExecutor exec(state, events, card_db);
    // Step 1: hold schedules + stashes the BF.
    fireTriggerAs(kIascylla, P1, ias, TriggerType::WhenIHold, exec);
    ASSERT_EQ(state.getObject(ias).card_counters["__iascylla_pull_bf"], 0);

    // Step 2: deferred fire (firing_trigger != WhenIHold). We use a direct
    // (non-chain) fire: confirmOptional defaults to "yes" and pickTarget
    // defaults to the first legal enemy, so the pull completes in one pass.
    // NOTE: see bug report — the deferred fire does NOT survive a real
    // suspend/resume cycle because it erases the stashed BF before the
    // resumable confirmOptional/pickTarget; only the single-pass path works.
    fireTriggerAs(kIascylla, P1, ias, TriggerType::AtStartOfMain, exec);

    EXPECT_EQ(state.getObject(enemy).battlefieldId(), std::optional<BattlefieldId>(0))
        << "deferred fire pulls the enemy to Iascylla's battlefield";
    // The stash is consumed.
    EXPECT_EQ(state.getObject(ias).card_counters.count("__iascylla_pull_bf"), 0u);
}

TEST_F(AuditFix4Test, Iascylla_DeferredFireNoOpWithNoEnemy) {
    auto ias = addUnit(P1, kIascylla, 6, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kIascylla, P1, ias, TriggerType::WhenIHold, exec);
    // No enemy units anywhere on a battlefield -> deferred fire does nothing.
    fireTriggerAs(kIascylla, P1, ias, TriggerType::AtStartOfMain, exec);
    SUCCEED();  // no crash; nothing to move
}

// ─── 643 Keeper of Masks ────────────────────────────────────────────────────

TEST_F(AuditFix4Test, KeeperOfMasks_FiresOnPlay) {
    Card* c = card_registry.get(kKeeperOfMasks);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayMe);
}

TEST_F(AuditFix4Test, KeeperOfMasks_CreatesTwoReflectionCopies) {
    auto keeper = addUnit(P1, kKeeperOfMasks, /*might=*/1, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);

    size_t before = state.objects.size();
    fireTrigger(kKeeperOfMasks, keeper, P1, exec);
    EXPECT_EQ(state.objects.size(), before + 2) << "two Reflection tokens created";

    int copies = 0;
    for (auto& [id, obj] : state.objects) {
        if (id == keeper) continue;
        if (obj.super_type != SuperType::Token) continue;
        ++copies;
        // copyUnit inherits Keeper's card_def_id and printed traits.
        EXPECT_EQ(obj.card_def_id, kKeeperOfMasks);
        EXPECT_EQ(obj.controller, P1);
        EXPECT_EQ(obj.base_might, state.getObject(keeper).base_might);
        // Spawned "here" = Keeper's battlefield (BF0).
        EXPECT_EQ(obj.battlefieldId(), std::optional<BattlefieldId>(0));
    }
    EXPECT_EQ(copies, 2);
}

// ─── 676 Nidalee, Cat Form ──────────────────────────────────────────────────

TEST_F(AuditFix4Test, NidaleeCat_WinCombatTriggerType) {
    Card* c = card_registry.get(kNidaleeCat);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIWinCombat);
}

TEST_F(AuditFix4Test, NidaleeCat_WinCombatDrawsOne) {
    auto nid = addUnit(P1, kNidaleeCat, /*might=*/4, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);
    int hand_before = handSize(P1);
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kNidaleeCat, P1, nid, TriggerType::WhenIWinCombat, exec);
    EXPECT_EQ(handSize(P1), hand_before + 1);
}

// ─── 737 Tactical Retreat ───────────────────────────────────────────────────

TEST_F(AuditFix4Test, TacticalRetreat_HealsExhaustsAndRecalls) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    state.getObject(unit).damage_marked = 3;
    state.getObject(unit).is_exhausted = false;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(/*source=*/unit, kTacticalRetreat, P1, {unit}, exec);

    EXPECT_EQ(state.getObject(unit).damage_marked, 0) << "healed";
    EXPECT_TRUE(state.getObject(unit).is_exhausted) << "exhausted";
    EXPECT_EQ(state.getObject(unit).zone, ZoneType::Base) << "recalled to base";
    EXPECT_TRUE(state.getObject(unit).isAtBase());
    // TODO: the real card installs a "next time it would DIE this turn"
    // deferred replacement. This implementation applies heal/exhaust/recall
    // immediately as a documented engine-limitation approximation.
}

TEST_F(AuditFix4Test, TacticalRetreat_NoTargetIsNoOp) {
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 4, 0);
    state.getObject(unit).damage_marked = 2;
    invokeOnResolve(unit, kTacticalRetreat, P1, {}, exec);
    EXPECT_EQ(state.getObject(unit).damage_marked, 2) << "unchanged with no target";
}

// ─── 766 Forbidding Waste ───────────────────────────────────────────────────

// Helper: install the Forbidding Waste battlefield card object on a BF slot.
namespace {
GameObjectId installBF(GameState& state, BattlefieldId bf, CardDefId def_id) {
    auto id = state.createObject();
    auto& obj = state.getObject(id);
    obj.card_def_id = def_id;
    obj.card_type = CardType::Battlefield;
    state.battlefields[bf].card_object_id = id;
    return id;
}
}  // namespace

TEST_F(AuditFix4Test, ForbiddingWaste_DefendingAloneGetsMinus2) {
    installBF(state, /*bf=*/0, kForbiddingWaste);
    auto defender = addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);
    state.getObject(defender).combat_designation = CombatDesignation::Defender;

    Card* c = card_registry.get(kForbiddingWaste);
    ASSERT_NE(c, nullptr);
    c->applyPassiveAura(state, PlayerId::None);

    int sum = 0;
    for (auto& ae : state.getObject(defender).aura_effects) sum += ae.might_bonus;
    EXPECT_EQ(sum, -2) << "a lone defender here takes -2 M";
}

TEST_F(AuditFix4Test, ForbiddingWaste_NotAloneNoDebuff) {
    installBF(state, /*bf=*/0, kForbiddingWaste);
    auto defender = addUnit(P1, kInvalidId, 5, /*at_bf=*/0);
    addUnit(P1, kInvalidId, 5, /*at_bf=*/0);  // a second friendly here -> not alone
    state.getObject(defender).combat_designation = CombatDesignation::Defender;

    Card* c = card_registry.get(kForbiddingWaste);
    c->applyPassiveAura(state, PlayerId::None);

    int sum = 0;
    for (auto& ae : state.getObject(defender).aura_effects) sum += ae.might_bonus;
    EXPECT_EQ(sum, 0) << "not alone (another friendly unit here) -> no debuff";
}

TEST_F(AuditFix4Test, ForbiddingWaste_NonDefenderNoDebuff) {
    installBF(state, /*bf=*/0, kForbiddingWaste);
    auto attacker = addUnit(P1, kInvalidId, 5, /*at_bf=*/0);
    state.getObject(attacker).combat_designation = CombatDesignation::Attacker;

    Card* c = card_registry.get(kForbiddingWaste);
    c->applyPassiveAura(state, PlayerId::None);

    int sum = 0;
    for (auto& ae : state.getObject(attacker).aura_effects) sum += ae.might_bonus;
    EXPECT_EQ(sum, 0) << "only DEFENDING units get the -2 (attacker untouched)";
}

TEST_F(AuditFix4Test, ForbiddingWaste_OtherBattlefieldUnaffected) {
    installBF(state, /*bf=*/0, kForbiddingWaste);
    auto elsewhere = addUnit(P1, kInvalidId, 5, /*at_bf=*/1);  // different BF
    state.getObject(elsewhere).combat_designation = CombatDesignation::Defender;

    Card* c = card_registry.get(kForbiddingWaste);
    c->applyPassiveAura(state, PlayerId::None);

    int sum = 0;
    for (auto& ae : state.getObject(elsewhere).aura_effects) sum += ae.might_bonus;
    EXPECT_EQ(sum, 0) << "Forbidding Waste only affects units at its own BF";
}

}  // namespace
