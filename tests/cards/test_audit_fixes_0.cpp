/// @file test_audit_fixes_0.cpp
/// Per-card unit tests for batch-0 audit-fix overrides implemented in
/// src/cards/manual/audit_fixes_0.cpp. Each card's distinct clauses plus
/// negative/boundary cases are exercised. The card behavior MUST match the
/// implementation in that file (verified API by API).
///
///   8   Get Excited!         — discard 1, deal its Energy cost to a unit
///   116 Thousand-Tailed W.   — on-play AoE -3M (min 1) to enemy units
///   331 Sentinel Adept       — on-play optional free equip (agent picks gear)
///   412 The Zero Drive       — equip / Deathknell banish-record / replay
///   467 Vex, Cheerless       — combat cost modifiers (friendly -1 min1 / enemy +1)
///   527 Ornn's Forge (BF)    — controller-gated non-token cost reduction
///   603 Allay, Eager Admirer — Deflect aura to your OTHER units here
///   617 Vex, Mocking         — WhenYouStun: move self to stunned unit's BF
///   652 LeBlanc              — shell only (documented gap)
///   703 Evelynn, Entrancing   — on-play pull enemy to MY battlefield
///   749 Bashful Bloom (Legend)— [4][E]: play 3M Sprite token (Temporary)

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kGetExcited     = 8;
constexpr CardDefId kThousandTailed = 116;
constexpr CardDefId kSentinelAdept  = 331;
constexpr CardDefId kZeroDrive      = 412;
constexpr CardDefId kVexCheerless   = 467;
constexpr CardDefId kOrnnsForge     = 527;
constexpr CardDefId kAllayEager     = 603;
constexpr CardDefId kVexMocking     = 617;
constexpr CardDefId kLeblanc        = 652;
constexpr CardDefId kEvelynn        = 703;
constexpr CardDefId kBashfulBloom   = 749;

// Reference cards used as Get Excited! discard fodder (known Energy costs).
constexpr CardDefId kSealOfRage     = 40;   // energy 0
constexpr CardDefId kCleave         = 4;    // energy 1
constexpr CardDefId kFlameChompers  = 6;    // energy 3 (per registry)
constexpr CardDefId kLongSword      = 345;  // [Equipment] tag, +2M, no effect

class AuditFix0Test : public CardTestFixture {
protected:
    // Build a gear object on board (optionally attached). Sets `tags` and
    // `might_bonus` from the def so Equipment detection + might math work.
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
            obj.tags = def.tags;
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

    // Drive a resumable onResolve WITH targets (the base helper passes empty
    // targets). Mirrors CardTestFixture::driveResumable but keeps `targets`.
    void driveResolveWithTargets(CardDefId def_id, PlayerId controller,
                                 GameObjectId source,
                                 const std::vector<GameObjectId>& targets,
                                 std::function<Intent(const std::vector<Intent>&)> picker,
                                 EffectExecutor& exec) {
        ChainItem ri;
        ri.id = state.chain.allocateId();
        ri.controller = controller;
        ri.is_spell = true;
        ri.resume_point = 0;
        ri.source = source;
        ri.card_def_id = def_id;
        ri.targets = targets;
        state.chain.resuming = ri;

        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};

        card->onResolve(ctx, targets);
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            Intent picked = picker ? picker(pending.legal)
                                   : (pending.legal.empty() ? Intent{} : pending.legal.front());
            exec.recordChoice(picked);
            card->onResolve(ctx, targets);
        }
        state.chain.resuming.reset();
    }

    // Picker that selects a MakeChoice whose chosen_objects[0] == want.
    static std::function<Intent(const std::vector<Intent>&)>
    pickObject(GameObjectId want) {
        return [want](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == want) return i;
            return legal.empty() ? Intent{} : legal.front();
        };
    }

    // Picker for confirmOptional "yes" (chosen_value == 1).
    static Intent yes(const std::vector<Intent>& legal) {
        for (auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.empty() ? Intent{} : legal.back();
    }
    // Picker for confirmOptional "no" (chosen_value == 0).
    static Intent no(const std::vector<Intent>& legal) {
        for (auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        return legal.empty() ? Intent{} : legal.front();
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [8] Get Excited! — discard 1, deal its Energy cost to a unit at a BF.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, GetExcited_DealsDiscardedCardEnergyCost) {
    auto target = addUnit(P1, kInvalidId, /*might=*/9, /*at_bf=*/0);
    auto fodder = addToHand(P1, kFlameChompers);  // energy 3
    auto src    = addToHand(P1, kGetExcited);

    EffectExecutor exec(state, events, card_db);
    // Discard `fodder` (3 energy) → 3 damage to target.
    driveResolveWithTargets(kGetExcited, P1, src, {target}, pickObject(fodder), exec);

    EXPECT_EQ(state.getObject(target).damage_marked, 3)
        << "should deal the discarded card's printed Energy cost (3), not 1";
    EXPECT_TRUE(inTrash(P1, fodder)) << "discarded card goes to trash";
}

TEST_F(AuditFix0Test, GetExcited_OneEnergyCardDealsOne) {
    auto target = addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);
    auto fodder = addToHand(P1, kCleave);  // energy 1
    auto src    = addToHand(P1, kGetExcited);

    EffectExecutor exec(state, events, card_db);
    driveResolveWithTargets(kGetExcited, P1, src, {target}, pickObject(fodder), exec);

    EXPECT_EQ(state.getObject(target).damage_marked, 1);
}

TEST_F(AuditFix0Test, GetExcited_ZeroEnergyCardDealsNoDamage) {
    auto target = addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);
    auto fodder = addToHand(P1, kSealOfRage);  // energy 0
    auto src    = addToHand(P1, kGetExcited);

    EffectExecutor exec(state, events, card_db);
    driveResolveWithTargets(kGetExcited, P1, src, {target}, pickObject(fodder), exec);

    // dmg <= 0 short-circuits: no damage, but the card is still discarded.
    EXPECT_EQ(state.getObject(target).damage_marked, 0);
    EXPECT_TRUE(inTrash(P1, fodder));
}

TEST_F(AuditFix0Test, GetExcited_EmptyHandFizzles) {
    auto target = addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);
    auto src    = addToHand(P1, kGetExcited);
    // Remove the spell from hand so hand is empty when discardOne runs.
    auto& h = state.player(P1).hand;
    h.erase(std::remove(h.begin(), h.end(), src), h.end());

    EffectExecutor exec(state, events, card_db);
    driveResolveWithTargets(kGetExcited, P1, src, {target}, nullptr, exec);

    EXPECT_EQ(state.getObject(target).damage_marked, 0)
        << "no card to discard → no damage";
}

TEST_F(AuditFix0Test, GetExcited_TargetRequirements) {
    Card* c = card_registry.get(kGetExcited);
    ASSERT_NE(c, nullptr);
    auto req = c->getTargetRequirements();
    EXPECT_EQ(req.count, 1);
    EXPECT_TRUE(req.must_be_unit);
    EXPECT_TRUE(req.must_be_at_battlefield);
}

// ═══════════════════════════════════════════════════════════════════════════
// [116] Thousand-Tailed Watcher — on play, enemy units -3M (min 1) this turn.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, ThousandTailed_TriggerType) {
    Card* c = card_registry.get(kThousandTailed);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayMe);
}

TEST_F(AuditFix0Test, ThousandTailed_DebuffsAllEnemyUnits) {
    auto self = addUnit(P1, kThousandTailed, /*might=*/7, /*at_bf=*/0);
    auto e1 = addUnit(P2, kInvalidId, /*might=*/6, /*at_bf=*/0);
    auto e2 = addUnit(P2, kInvalidId, /*might=*/5, /*at_bf=*/1);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kThousandTailed, P1, self, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(state.getObject(e1).current_might, 3) << "6 - 3 = 3";
    EXPECT_EQ(state.getObject(e2).current_might, 2) << "5 - 3 = 2";
}

TEST_F(AuditFix0Test, ThousandTailed_FloorAtOne) {
    auto self = addUnit(P1, kThousandTailed, /*might=*/7, /*at_bf=*/0);
    auto weak = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/0);  // 2-3 = -1 → 1

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kThousandTailed, P1, self, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(state.getObject(weak).current_might, 1)
        << "must floor at minimum 1 [M]";
}

TEST_F(AuditFix0Test, ThousandTailed_DoesNotTouchFriendlies) {
    auto self     = addUnit(P1, kThousandTailed, /*might=*/7, /*at_bf=*/0);
    auto friendly = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto enemy    = addUnit(P2, kInvalidId, /*might=*/4, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kThousandTailed, P1, self, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(state.getObject(friendly).current_might, 4) << "friendly untouched";
    EXPECT_EQ(state.getObject(enemy).current_might, 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// [331] Sentinel Adept — Weaponmaster: on play, may equip an Equipment to me
// for free (agent picks WHICH equipment).
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, SentinelAdept_TriggerType) {
    Card* c = card_registry.get(kSentinelAdept);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayMe);
}

TEST_F(AuditFix0Test, SentinelAdept_EquipsChosenGearForFree) {
    auto self = addUnit(P1, kSentinelAdept, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kLongSword, /*at_bf=*/0);  // +2M, [Equipment]

    EffectExecutor exec(state, events, card_db);
    int dirk_bonus = state.getObject(gear).might_bonus;
    ASSERT_GT(dirk_bonus, 0);

    driveResumableTrigger(kSentinelAdept, P1, self,
        [&](const std::vector<Intent>& legal) -> Intent {
            // First prompt is the "may" yes/no, second is the gear pick.
            for (auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;  // yes
            for (auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == gear) return i;
            return legal.empty() ? Intent{} : legal.back();
        }, exec, TriggerType::WhenYouPlayMe);

    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(self));
    EXPECT_EQ(state.getObject(self).attachment_might_bonus, dirk_bonus);
    EXPECT_EQ(state.getObject(self).current_might, 3 + dirk_bonus)
        << "free equip — no runes paid, might increased by gear bonus";
    EXPECT_EQ(exhaustedRuneCount(P1), 0) << "free equip pays nothing";
}

TEST_F(AuditFix0Test, SentinelAdept_DeclineLeavesGearUnattached) {
    auto self = addUnit(P1, kSentinelAdept, /*might=*/3, /*at_bf=*/0);
    auto gear = addGear(P1, kLongSword, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kSentinelAdept, P1, self,
        &AuditFix0Test::no, exec, TriggerType::WhenYouPlayMe);

    EXPECT_FALSE(state.getObject(gear).attached_to.has_value())
        << "declining the 'may' leaves gear unattached";
    EXPECT_EQ(state.getObject(self).attachment_might_bonus, 0);
}

TEST_F(AuditFix0Test, SentinelAdept_NoEquipmentNoOp) {
    auto self = addUnit(P1, kSentinelAdept, /*might=*/3, /*at_bf=*/0);
    // Non-equipment gear (no "Equipment" tag) should not be offered.
    auto seal = addGear(P1, kSealOfRage, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kSentinelAdept, P1, self,
        &AuditFix0Test::yes, exec, TriggerType::WhenYouPlayMe);

    EXPECT_FALSE(state.getObject(seal).attached_to.has_value())
        << "non-Equipment gear is not eligible";
}

// ═══════════════════════════════════════════════════════════════════════════
// [412] The Zero Drive — Equip / Deathknell record / replay.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, ZeroDrive_EquipPaysAndAttaches) {
    Card* c = card_registry.get(kZeroDrive);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasEquipAbility());

    auto unit = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto gear = addGear(P1, kZeroDrive, /*at_bf=*/0);
    addRune(P1, Domain::Mind);  // energy
    addRune(P1, Domain::Mind);  // power (recycled)

    int gear_bonus = state.getObject(gear).might_bonus;  // +2 per registry
    int runes_before = readyRuneCount(P1) + exhaustedRuneCount(P1);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = c->onEquip(ctx, unit);

    EXPECT_TRUE(ok);
    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(unit));
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, gear_bonus);
    // One rune was recycled (power) out of base; energy exhausted a ready rune.
    int runes_after = readyRuneCount(P1) + exhaustedRuneCount(P1);
    EXPECT_LT(runes_after, runes_before) << "equip consumed at least one rune from base";
    EXPECT_EQ(state.player(P1).rune_deck.size(), 1u) << "one rune recycled for [B] power";
}

TEST_F(AuditFix0Test, ZeroDrive_EquipFailsWhenCantAfford) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto gear = addGear(P1, kZeroDrive, /*at_bf=*/0);
    // No Mind runes at all — both the energy and the domain-rune check fail.

    Card* c = card_registry.get(kZeroDrive);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = c->onEquip(ctx, unit);

    EXPECT_FALSE(ok) << "no runes → can't pay";
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value())
        << "failed equip must not half-apply";
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, 0);
}

TEST_F(AuditFix0Test, ZeroDrive_DeathknellBanishesAndRecords) {
    Card* c = card_registry.get(kZeroDrive);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasReplacementEffect());

    auto unit = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto gear = addGear(P1, kZeroDrive, /*at_bf=*/0);
    // Attach manually (so we test replacement, not equip).
    {
        auto& g = state.getObject(gear);
        auto& u = state.getObject(unit);
        g.attached_to = unit;
        u.attachments.push_back(gear);
        u.attachment_might_bonus += g.might_bonus;
        u.recomputeMight();
    }

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, gear};
    bool consumed = c->applyReplacement(ctx, unit);

    EXPECT_TRUE(consumed) << "replacement consumes the death of the bearer";
    EXPECT_EQ(state.getObject(unit).zone, ZoneType::Banishment)
        << "bearer banished instead of trashed";
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value())
        << "gear detaches and stays on board unattached";
    auto& tracked = state.getObject(gear).tracked_objects;
    EXPECT_NE(std::find(tracked.begin(), tracked.end(), unit), tracked.end())
        << "banished unit recorded for replay";
}

TEST_F(AuditFix0Test, ZeroDrive_ReplacementIgnoresOtherUnits) {
    auto bearer = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto other  = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto gear   = addGear(P1, kZeroDrive, /*at_bf=*/0);
    {
        auto& g = state.getObject(gear);
        g.attached_to = bearer;
        state.getObject(bearer).attachments.push_back(gear);
    }

    Card* c = card_registry.get(kZeroDrive);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, gear};
    bool consumed = c->applyReplacement(ctx, other);  // a DIFFERENT unit dies

    EXPECT_FALSE(consumed) << "only intercepts the attached bearer's death";
    EXPECT_NE(state.getObject(other).zone, ZoneType::Banishment);
}

TEST_F(AuditFix0Test, ZeroDrive_ReplayLegalityAndActivate) {
    Card* c = card_registry.get(kZeroDrive);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasActivatedAbility());
    EXPECT_EQ(c->getActivationCost().energy, 3);

    // Unattached gear with a recorded banished unit → replay is legal.
    auto gear = addGear(P1, kZeroDrive, /*at_bf=*/0);
    auto banished = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/-1);
    // Move the banished unit into banishment, recorded by the gear.
    {
        auto& u = state.getObject(banished);
        u.zone = ZoneType::Banishment;
        u.location = std::nullopt;
        state.player(P1).banishment.push_back(banished);
        state.getObject(gear).tracked_objects.push_back(banished);
    }

    EXPECT_TRUE(c->hasLegalTargets(state, P1))
        << "unattached + has recorded unit → replay legal";

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    CardContext ctx{state, events, exec, P1, gear};
    c->onActivate(ctx, {});

    EXPECT_NE(state.getObject(banished).zone, ZoneType::Banishment)
        << "recorded unit replayed (back on board)";
    EXPECT_EQ(state.getObject(gear).zone, ZoneType::Banishment)
        << "'Banish this' cost — gear banished after activating";
}

TEST_F(AuditFix0Test, ZeroDrive_ReplayIllegalWhileAttached) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto gear = addGear(P1, kZeroDrive, /*at_bf=*/0);
    {
        auto& g = state.getObject(gear);
        g.attached_to = unit;
        state.getObject(unit).attachments.push_back(gear);
        g.tracked_objects.push_back(addUnit(P1, kInvalidId, 1, -1));
    }
    Card* c = card_registry.get(kZeroDrive);
    EXPECT_FALSE(c->hasLegalTargets(state, P1))
        << "(Use only if unattached.) — attached → illegal";
}

TEST_F(AuditFix0Test, ZeroDrive_ReplayIllegalWithNoRecord) {
    addGear(P1, kZeroDrive, /*at_bf=*/0);  // unattached but nothing recorded
    Card* c = card_registry.get(kZeroDrive);
    EXPECT_FALSE(c->hasLegalTargets(state, P1))
        << "nothing banished with this → nothing to replay";
}

// ═══════════════════════════════════════════════════════════════════════════
// [467] Vex, Cheerless — combat cost modifiers.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, VexCheerless_PushesTwoCombatModifiers) {
    Card* c = card_registry.get(kVexCheerless);
    ASSERT_NE(c, nullptr);

    c->applyPassiveAura(state, P1);
    auto& mods = state.player(P1).cost_modifiers;
    ASSERT_EQ(mods.size(), 2u);

    // Friendly reduction: -1 energy, min 1, friendly-only, combat-active.
    const auto& fr = mods[0];
    EXPECT_EQ(fr.energy_reduction, 1);
    EXPECT_EQ(fr.min_cost, 1) << "the gap fix: 'to a minimum of [1]' floor";
    EXPECT_TRUE(fr.affects_friendly_only);
    EXPECT_TRUE(fr.combat_active_only);
    EXPECT_FALSE(fr.this_turn_only);

    // Enemy increase: +1 energy, enemy-only, combat-active.
    const auto& en = mods[1];
    EXPECT_EQ(en.energy_increase, 1);
    EXPECT_TRUE(en.affects_enemy_only);
    EXPECT_TRUE(en.combat_active_only);
}

// ═══════════════════════════════════════════════════════════════════════════
// [527] Ornn's Forge — controller-gated non-token cost reduction.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, OrnnsForge_AppliesReductionToController) {
    // Back the first battlefield with an Ornn's Forge card object.
    auto bf_card = state.createObject();
    {
        auto& o = state.getObject(bf_card);
        o.card_def_id = kOrnnsForge;
        o.card_type = CardType::Battlefield;
        o.name = "Ornn's Forge";
    }
    state.battlefields[0].card_object_id = bf_card;
    state.battlefields[0].controller = P1;

    Card* c = card_registry.get(kOrnnsForge);
    ASSERT_NE(c, nullptr);
    c->applyPassiveAura(state, P1);

    auto& mods = state.player(P1).cost_modifiers;
    ASSERT_EQ(mods.size(), 1u);
    EXPECT_EQ(mods[0].energy_reduction, 1);
    EXPECT_TRUE(mods[0].affects_non_token_only);
    EXPECT_TRUE(mods[0].affects_friendly_only);
    EXPECT_TRUE(mods[0].gear_only)
        << "printed text: reduces gear, not arbitrary non-token cards";
    EXPECT_TRUE(mods[0].first_gear_per_turn)
        << "printed text: only the FIRST gear played each turn";
    EXPECT_EQ(mods[0].source, bf_card);
}

TEST_F(AuditFix0Test, OrnnsForge_NoReductionWhenUncontrolled) {
    auto bf_card = state.createObject();
    {
        auto& o = state.getObject(bf_card);
        o.card_def_id = kOrnnsForge;
        o.card_type = CardType::Battlefield;
        o.name = "Ornn's Forge";
    }
    state.battlefields[0].card_object_id = bf_card;
    // controller left unset (nullopt) → "While you control this battlefield"
    // is not satisfied.

    Card* c = card_registry.get(kOrnnsForge);
    c->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.player(P1).cost_modifiers.empty())
        << "no reduction unless a player controls the BF";
}

// ═══════════════════════════════════════════════════════════════════════════
// [603] Allay, Eager Admirer — Deflect aura to YOUR OTHER units HERE.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, AllayEager_GrantsDeflectToFriendlyUnitsHere) {
    auto allay   = addUnit(P1, kAllayEager, /*might=*/3, /*at_bf=*/0);
    auto ally    = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);

    Card* c = card_registry.get(kAllayEager);
    ASSERT_NE(c, nullptr);
    c->applyPassiveAura(state, P1);

    auto& fx = state.getObject(ally).aura_effects;
    bool granted = false;
    for (auto& ae : fx)
        if (ae.keyword == Keyword::Deflect && ae.source == allay) granted = true;
    EXPECT_TRUE(granted) << "friendly unit at my BF gains Deflect";

    // Self is excluded ("OTHER").
    bool self_buffed = false;
    for (auto& ae : state.getObject(allay).aura_effects)
        if (ae.keyword == Keyword::Deflect) self_buffed = true;
    EXPECT_FALSE(self_buffed) << "Allay does not grant the aura to itself";
}

TEST_F(AuditFix0Test, AllayEager_DoesNotGrantToEnemiesOrOtherBF) {
    auto allay  = addUnit(P1, kAllayEager, /*might=*/3, /*at_bf=*/0);
    auto enemy  = addUnit(P2, kInvalidId, /*might=*/2, /*at_bf=*/0);  // same BF, enemy
    auto faraway = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/1); // friendly, other BF

    Card* c = card_registry.get(kAllayEager);
    c->applyPassiveAura(state, P1);

    auto hasDeflect = [&](GameObjectId id) {
        for (auto& ae : state.getObject(id).aura_effects)
            if (ae.keyword == Keyword::Deflect) return true;
        return false;
    };
    EXPECT_FALSE(hasDeflect(enemy)) << "enemy units don't get the aura";
    EXPECT_FALSE(hasDeflect(faraway)) << "friendly at a DIFFERENT BF doesn't get it";
    (void)allay;
}

TEST_F(AuditFix0Test, AllayEager_NoAuraWhenAtBase) {
    auto allay = addUnit(P1, kAllayEager, /*might=*/3, /*at_bf=*/-1);  // base
    auto ally  = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/-1);

    Card* c = card_registry.get(kAllayEager);
    c->applyPassiveAura(state, P1);

    bool granted = false;
    for (auto& ae : state.getObject(ally).aura_effects)
        if (ae.keyword == Keyword::Deflect) granted = true;
    EXPECT_FALSE(granted) << "aura only active 'While I'm at a battlefield'";
    (void)allay;
}

// ═══════════════════════════════════════════════════════════════════════════
// [617] Vex, Mocking — WhenYouStun: move self to stunned unit's battlefield.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, VexMocking_TriggerType) {
    Card* c = card_registry.get(kVexMocking);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouStun);
}

TEST_F(AuditFix0Test, VexMocking_MovesToStunnedUnitsBattlefield) {
    // Vex starts at BF 0; the stunned enemy is at BF 1.
    auto vex   = addUnit(P1, kVexMocking, /*might=*/5, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/1);
    state.getObject(enemy).is_stunned = true;  // already stunned
    // TriggerManager captures the stunned unit id into card_counters.
    state.getObject(vex).card_counters["__stunned_unit_id"] =
        static_cast<int>(enemy);

    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kVexMocking, P1, vex, &AuditFix0Test::yes, exec,
                          TriggerType::WhenYouStun);

    auto bf = state.getObject(vex).battlefieldId();
    ASSERT_TRUE(bf.has_value());
    EXPECT_EQ(*bf, 1u) << "Vex moves to the stunned unit's battlefield";
}

TEST_F(AuditFix0Test, VexMocking_DeclineLeavesVexInPlace) {
    auto vex   = addUnit(P1, kVexMocking, /*might=*/5, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/1);
    state.getObject(enemy).is_stunned = true;
    state.getObject(vex).card_counters["__stunned_unit_id"] =
        static_cast<int>(enemy);

    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kVexMocking, P1, vex, &AuditFix0Test::no, exec,
                          TriggerType::WhenYouStun);

    auto bf = state.getObject(vex).battlefieldId();
    ASSERT_TRUE(bf.has_value());
    EXPECT_EQ(*bf, 0u) << "declining keeps Vex at its original BF";
}

TEST_F(AuditFix0Test, VexMocking_NoOpWhenAlreadyAtSameBF) {
    auto vex   = addUnit(P1, kVexMocking, /*might=*/5, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);  // same BF as Vex
    state.getObject(enemy).is_stunned = true;
    state.getObject(vex).card_counters["__stunned_unit_id"] =
        static_cast<int>(enemy);

    EffectExecutor exec(state, events, card_db);
    // still_legal returns false (already there) → no prompt; yes-picker is a no-op.
    driveResumableTrigger(kVexMocking, P1, vex, &AuditFix0Test::yes, exec,
                          TriggerType::WhenYouStun);

    auto bf = state.getObject(vex).battlefieldId();
    ASSERT_TRUE(bf.has_value());
    EXPECT_EQ(*bf, 0u) << "nothing to move — already at that battlefield";
}

TEST_F(AuditFix0Test, VexMocking_NoOpWithoutCapturedStun) {
    auto vex = addUnit(P1, kVexMocking, /*might=*/5, /*at_bf=*/0);
    // No __stunned_unit_id counter set.
    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kVexMocking, P1, vex, &AuditFix0Test::yes, exec,
                          TriggerType::WhenYouStun);
    EXPECT_EQ(*state.getObject(vex).battlefieldId(), 0u);
}

// ═══════════════════════════════════════════════════════════════════════════
// [652] LeBlanc, Everywhere at Once — documented gap (shell only).
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, LeBlanc_RegisteredAsUnitNoTrigger) {
    Card* c = card_registry.get(kLeblanc);
    ASSERT_NE(c, nullptr);
    // The "Temporary effects don't trigger" passive is a documented engine
    // gap; the shell preserves identity with no trigger / no aura.
    // TODO: 'Your [Temporary] effects at my battlefield don't trigger' is
    // unimplemented (no engine hook to suppress triggers at a location).
    EXPECT_EQ(c->triggerType(), TriggerType::None);
    EXPECT_FALSE(c->hasActivatedAbility());
    EXPECT_FALSE(c->hasReplacementEffect());
}

// ═══════════════════════════════════════════════════════════════════════════
// [703] Evelynn, Entrancing — on play, pull an enemy at a different location
// to MY battlefield.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, Evelynn_TriggerType) {
    Card* c = card_registry.get(kEvelynn);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayMe);
}

TEST_F(AuditFix0Test, Evelynn_PullsEnemyToMyBattlefield) {
    auto eve   = addUnit(P1, kEvelynn, /*might=*/2, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/1);  // different BF

    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kEvelynn, P1, eve,
        [&](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;  // yes
            for (auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == enemy) return i;
            return legal.empty() ? Intent{} : legal.back();
        }, exec, TriggerType::WhenYouPlayMe);

    auto bf = state.getObject(enemy).battlefieldId();
    ASSERT_TRUE(bf.has_value());
    EXPECT_EQ(*bf, 0u) << "enemy pulled to Evelynn's battlefield (not its own base)";
}

TEST_F(AuditFix0Test, Evelynn_DeclineLeavesEnemyInPlace) {
    auto eve   = addUnit(P1, kEvelynn, /*might=*/2, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/1);

    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kEvelynn, P1, eve, &AuditFix0Test::no, exec,
                          TriggerType::WhenYouPlayMe);

    EXPECT_EQ(*state.getObject(enemy).battlefieldId(), 1u)
        << "declining leaves the enemy where it was";
}

TEST_F(AuditFix0Test, Evelynn_NoOpWhenEvelynnAtBase) {
    auto eve   = addUnit(P1, kEvelynn, /*might=*/2, /*at_bf=*/-1);  // base, no BF
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/1);

    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kEvelynn, P1, eve, &AuditFix0Test::yes, exec,
                          TriggerType::WhenYouPlayMe);

    EXPECT_EQ(*state.getObject(enemy).battlefieldId(), 1u)
        << "Evelynn must be at a battlefield to have a destination";
}

TEST_F(AuditFix0Test, Evelynn_SameBFEnemyNotEligible) {
    auto eve   = addUnit(P1, kEvelynn, /*might=*/2, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);  // SAME BF

    EffectExecutor exec(state, events, card_db);
    driveResumableTrigger(kEvelynn, P1, eve, &AuditFix0Test::yes, exec,
                          TriggerType::WhenYouPlayMe);

    // No "different location" enemy exists → no prompt, no move.
    EXPECT_EQ(*state.getObject(enemy).battlefieldId(), 0u);
}

// ═══════════════════════════════════════════════════════════════════════════
// [749] Bashful Bloom (Legend) — [4][E]: play a 3M Sprite token w/ Temporary.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix0Test, BashfulBloom_ActivationCost) {
    Card* c = card_registry.get(kBashfulBloom);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasActivatedAbility());
    auto cost = c->getActivationCost();
    EXPECT_TRUE(cost.exhaust) << "[E] exhaust cost added by the fix";
    EXPECT_EQ(cost.energy, 4) << "flat [4] (per-Temporary reduction is a documented gap)";
    // TODO: 'costs [1] less for each friendly unit with [Temporary]' is
    // unimplemented (no state-aware activation-cost hook).
}

TEST_F(AuditFix0Test, BashfulBloom_CreatesReadyTemporarySprite) {
    auto legend = addUnit(P1, kBashfulBloom, /*might=*/0, /*at_bf=*/-1);

    int before = static_cast<int>(state.objects.size());
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, legend};
    Card* c = card_registry.get(kBashfulBloom);
    c->onActivate(ctx, {});

    // Find the created Sprite token.
    GameObjectId sprite = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (obj.name == "Sprite" && obj.super_type == SuperType::Token) sprite = id;
    }
    ASSERT_NE(sprite, kInvalidId) << "a Sprite token must be created";
    auto& s = state.getObject(sprite);
    EXPECT_EQ(s.current_might, 3) << "3 [M]";
    EXPECT_TRUE(s.hasKeyword(Keyword::Temporary)) << "[Temporary]";
    EXPECT_FALSE(s.is_exhausted) << "enters ready";
    EXPECT_EQ(s.controller, P1);
    EXPECT_GT(static_cast<int>(state.objects.size()), before);
}

}  // namespace
