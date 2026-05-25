// Per-card unit tests for audit-fix batch 3 (src/cards/manual/audit_fixes_3.cpp).
// Cards: 45 Defy, 209 Cull the Weak, 366 Emperor's Divide, 440 Jax Unrelenting,
//        486 Glasc Mixologist, 534 Treasure Hoard, 609 Mosstomper,
//        642 Hwei Brooding Painter, 675 Master Yi, 735 Sacrifice,
//        765 Dusk Rose Lab.
//
// Fixture class: AuditFix3Test.

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

// Card def IDs used in these tests (verified against cards/registry.json).
constexpr CardDefId kDefy          = 45;   // spell, energy 1
constexpr CardDefId kCullTheWeak   = 209;  // spell
constexpr CardDefId kEmperorsDivide= 366;  // spell
constexpr CardDefId kJax           = 440;  // unit (Weaponmaster)
constexpr CardDefId kGlasc         = 486;  // unit (Deathknell)
constexpr CardDefId kTreasureHoard = 534;  // battlefield
constexpr CardDefId kMosstomper    = 609;  // unit
constexpr CardDefId kHwei          = 642;  // unit
constexpr CardDefId kMasterYi      = 675;  // unit
constexpr CardDefId kSacrifice     = 735;  // spell
constexpr CardDefId kDuskRoseLab   = 765;  // battlefield

// Cheap spell (energy 4 — counterable by Defy) and a costly one (energy > 4).
constexpr CardDefId kDisintegrate  = 5;    // spell, energy 4
constexpr CardDefId kThermoBeam    = 22;   // spell, energy 5
// Units: cheap (<=3 cost) and big (>3 cost) for Glasc Mixologist.
constexpr CardDefId kLegionRearguard = 10; // unit, energy 2
constexpr CardDefId kMagmaWurm       = 11; // unit, energy 8

class AuditFix3Test : public CardTestFixture {
protected:
    // Gear with explicit tags (Jax checks the "Equipment" tag).
    GameObjectId addEquipment(PlayerId owner, int might_bonus, int at_bf = -1,
                              bool is_equipment = true) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_type = CardType::Gear;
        obj.name = "TestGear";
        obj.might_bonus = might_bonus;
        if (is_equipment) obj.tags.push_back("Equipment");
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    // Add a card to a player's trash by def id.
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.card_type = def.card_type;
            obj.super_type = def.super_type;
            obj.keywords = def.keywords;
            obj.domains = def.domains;
        }
        obj.zone = ZoneType::Trash;
        state.player(owner).trash.push_back(id);
        return id;
    }

    // Add a rune into the player's rune_deck (channelRunes pulls from here).
    GameObjectId addRuneToRuneDeck(PlayerId owner, Domain domain) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_type = CardType::Rune;
        obj.name = "RuneDeckRune";
        obj.domains = {domain};
        obj.zone = ZoneType::RuneDeck;
        state.player(owner).rune_deck.push_back(id);
        return id;
    }

    // Picker that answers "yes" to confirmOptional (chosen_value==1) and the
    // first target offered for pickTarget (chosen_objects).
    static Intent yesAndFirstTarget(const std::vector<Intent>& legal) {
        for (auto& i : legal) {
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        }
        return legal.empty() ? Intent{} : legal.front();
    }

    // Picker that answers "no" to confirmOptional (chosen_value==0).
    static Intent noChoice(const std::vector<Intent>& legal) {
        for (auto& i : legal) {
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        }
        return legal.empty() ? Intent{} : legal.front();
    }

    // Drive a resumable spell onResolve while passing explicit play-time
    // targets on each re-entry (the base fixture's driveResumable always
    // passes {} — Emperor's Divide reads targets[0] on case 0).
    void driveResumableWithTargets(
            CardDefId def_id, PlayerId controller, GameObjectId source,
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
        state.chain.resuming = ri;

        Card* card = card_registry.get(def_id);
        ASSERT_NE(card, nullptr);
        CardContext ctx{state, events, exec, controller, source};

        card->onResolve(ctx, targets);
        while (exec.hasPendingChoice()) {
            auto pending = exec.consumePendingChoice();
            Intent picked = picker
                ? picker(pending.legal)
                : (pending.legal.empty() ? Intent{} : pending.legal.front());
            exec.recordChoice(picked);
            card->onResolve(ctx, targets);
        }
        state.chain.resuming.reset();
    }
};

// ─── [45] Defy — counter a spell costing <= [4], no un-printed draw ──────────

TEST_F(AuditFix3Test, Defy_CountersCheapSpell) {
    EffectExecutor exec(state, events, card_db);
    // Peek-and-pop model: by the time Defy resolves, FEPR has already popped
    // Defy itself, so the spell to counter is the chain's top item. Push ONLY
    // the target spell; the Defy source is a separate (off-chain) object.
    auto target = pushSpellOnChain(P2, kDisintegrate);  // 4-cost: counterable
    auto defy = addToHand(P1, kDefy);

    int hand_before = handSize(P1);
    invokeOnResolve(defy, kDefy, P1, {}, exec);

    // The 4-cost spell is countered (moved to its owner's trash, off chain).
    EXPECT_TRUE(inTrash(P2, target));
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Trash);
    EXPECT_TRUE(state.chain.items.empty());
    // No un-printed draw (the fixed Defy no longer draws). Defy is still in
    // hand here because invokeOnResolve doesn't move the source.
    EXPECT_EQ(handSize(P1), hand_before);
}

TEST_F(AuditFix3Test, Defy_CannotCounterExpensiveSpell) {
    EffectExecutor exec(state, events, card_db);
    auto target = pushSpellOnChain(P2, kThermoBeam);  // energy 5 > 4
    auto defy = addToHand(P1, kDefy);

    invokeOnResolve(defy, kDefy, P1, {}, exec);

    // Cost gate fails: spell NOT countered, stays on chain.
    EXPECT_FALSE(inTrash(P2, target));
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Chain);
    EXPECT_EQ(state.chain.items.size(), 1u);
}

TEST_F(AuditFix3Test, Defy_NoLegalTargetWhenChainEmpty) {
    // hasLegalTargets is false with no spell on the chain.
    EXPECT_FALSE(card_registry.get(kDefy)->hasLegalTargets(state, P1));
    // With a spell present it becomes legal.
    pushSpellOnChain(P2, kDisintegrate);
    EXPECT_TRUE(card_registry.get(kDefy)->hasLegalTargets(state, P1));
}

// ─── [209] Cull the Weak — each player kills one of their units ──────────────

TEST_F(AuditFix3Test, CullTheWeak_KillsOneEachSide) {
    EffectExecutor exec(state, events, card_db);
    auto p1_unit = addUnit(P1, kInvalidId, 3, 0);
    auto p2_unit = addUnit(P2, kInvalidId, 3, 0);
    auto src = pushSpellOnChain(P1, kCullTheWeak);

    invokeOnResolve(src, kCullTheWeak, P1, {}, exec);

    EXPECT_TRUE(inTrash(P1, p1_unit));
    EXPECT_TRUE(inTrash(P2, p2_unit));
}

TEST_F(AuditFix3Test, CullTheWeak_OnlyOwnUnitDiesIfOpponentEmpty) {
    EffectExecutor exec(state, events, card_db);
    auto p1_unit = addUnit(P1, kInvalidId, 3, 0);
    auto src = pushSpellOnChain(P1, kCullTheWeak);

    invokeOnResolve(src, kCullTheWeak, P1, {}, exec);

    EXPECT_TRUE(inTrash(P1, p1_unit));
    EXPECT_EQ(trashSize(P2), 0);
}

TEST_F(AuditFix3Test, CullTheWeak_NoUnitsNoOp) {
    EffectExecutor exec(state, events, card_db);
    auto src = pushSpellOnChain(P1, kCullTheWeak);
    invokeOnResolve(src, kCullTheWeak, P1, {}, exec);
    EXPECT_EQ(trashSize(P1), 0);
    EXPECT_EQ(trashSize(P2), 0);
}

// ─── [366] Emperor's Divide — move chosen friendly units at a BF to base ─────

TEST_F(AuditFix3Test, EmperorsDivide_MovesChosenUnitToBase) {
    EffectExecutor exec(state, events, card_db);
    BattlefieldId bf = 0;
    auto u1 = addUnit(P1, kInvalidId, 2, static_cast<int>(bf));
    auto u2 = addUnit(P1, kInvalidId, 2, static_cast<int>(bf));
    auto src = pushSpellOnChain(P1, kEmperorsDivide);

    // Picker: always pick the first offered unit; the choice list shrinks
    // each pass as units move off the BF, until only "done" remains.
    driveResumableWithTargets(kEmperorsDivide, P1, src, {u1},
        [&](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal) {
                if (!i.chosen_objects.empty()) return i;
            }
            return legal.empty() ? Intent{} : legal.front();
        },
        exec);

    // Both friendly units moved to base.
    EXPECT_TRUE(state.getObject(u1).isAtBase());
    EXPECT_TRUE(state.getObject(u2).isAtBase());
}

TEST_F(AuditFix3Test, EmperorsDivide_StopEarlyLeavesUnitAtBF) {
    EffectExecutor exec(state, events, card_db);
    BattlefieldId bf = 0;
    auto u1 = addUnit(P1, kInvalidId, 2, static_cast<int>(bf));
    auto u2 = addUnit(P1, kInvalidId, 2, static_cast<int>(bf));
    auto src = pushSpellOnChain(P1, kEmperorsDivide);

    // Pick "done" (empty chosen_objects) on the FIRST prompt — no unit moves.
    driveResumableWithTargets(kEmperorsDivide, P1, src, {u1},
        [&](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal) {
                if (i.chosen_objects.empty()) return i;  // "done"
            }
            return legal.empty() ? Intent{} : legal.front();
        },
        exec);

    EXPECT_TRUE(state.getObject(u1).isAtBattlefield());
    EXPECT_TRUE(state.getObject(u2).isAtBattlefield());
}

TEST_F(AuditFix3Test, EmperorsDivide_DoesNotMoveEnemyUnits) {
    EffectExecutor exec(state, events, card_db);
    BattlefieldId bf = 0;
    auto friendly = addUnit(P1, kInvalidId, 2, static_cast<int>(bf));
    auto enemy = addUnit(P2, kInvalidId, 2, static_cast<int>(bf));
    auto src = pushSpellOnChain(P1, kEmperorsDivide);

    driveResumableWithTargets(kEmperorsDivide, P1, src, {friendly},
        [&](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal) {
                if (!i.chosen_objects.empty()) return i;
            }
            return legal.empty() ? Intent{} : legal.front();
        },
        exec);

    EXPECT_TRUE(state.getObject(friendly).isAtBase());
    // Enemy unit must remain at the battlefield.
    EXPECT_TRUE(state.getObject(enemy).isAtBattlefield());
}

// ─── [440] Jax, Unrelenting — Weaponmaster equip + pay [1] draw 1 rider ──────

TEST_F(AuditFix3Test, Jax_EquipsEquipmentAndPaysForDraw) {
    EffectExecutor exec(state, events, card_db);
    auto jax = addUnit(P1, kJax, 3, 0);
    auto gear = addEquipment(P1, /*might_bonus=*/2, /*at_bf=*/0);
    state.player(P1).rune_pool.energy = 1;
    addToDeck(P1, kInvalidId);  // something to draw

    int hand_before = handSize(P1);
    // Jax's onTrigger dereferences chain.resuming, so we must drive it
    // through the resume loop (sets resuming). Answer "yes" to the rider.
    driveResumableTrigger(kJax, P1, jax, &AuditFix3Test::yesAndFirstTarget, exec,
                          TriggerType::WhenYouPlayMe);

    // Gear attached to Jax, might bonus applied.
    EXPECT_EQ(state.getObject(gear).attached_to.value_or(kInvalidId), jax);
    EXPECT_EQ(state.getObject(jax).attachment_might_bonus, 2);
    // Paid [1], drew 1.
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(handSize(P1), hand_before + 1);
}

TEST_F(AuditFix3Test, Jax_NoEquipmentNoRider) {
    EffectExecutor exec(state, events, card_db);
    auto jax = addUnit(P1, kJax, 3, 0);
    state.player(P1).rune_pool.energy = 1;
    addToDeck(P1, kInvalidId);

    int hand_before = handSize(P1);
    driveResumableTrigger(kJax, P1, jax, &AuditFix3Test::yesAndFirstTarget, exec,
                          TriggerType::WhenYouPlayMe);

    // No gear attached → rider not offered → energy untouched, no draw.
    EXPECT_EQ(state.getObject(jax).attachments.size(), 0u);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
    EXPECT_EQ(handSize(P1), hand_before);
}

TEST_F(AuditFix3Test, Jax_DeclineRiderKeepsEnergy) {
    EffectExecutor exec(state, events, card_db);
    auto jax = addUnit(P1, kJax, 3, 0);
    auto gear = addEquipment(P1, /*might_bonus=*/2, /*at_bf=*/0);
    state.player(P1).rune_pool.energy = 1;
    addToDeck(P1, kInvalidId);

    int hand_before = handSize(P1);
    // Drive through the resume loop and DECLINE the pay-1 rider.
    driveResumableTrigger(kJax, P1, jax, &AuditFix3Test::noChoice, exec,
                          TriggerType::WhenYouPlayMe);

    // Equip still happened (mandatory Weaponmaster step), rider declined.
    EXPECT_EQ(state.getObject(gear).attached_to.value_or(kInvalidId), jax);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
    EXPECT_EQ(handSize(P1), hand_before);
}

TEST_F(AuditFix3Test, Jax_RiderNotOfferedWhenNoEnergy) {
    EffectExecutor exec(state, events, card_db);
    auto jax = addUnit(P1, kJax, 3, 0);
    auto gear = addEquipment(P1, /*might_bonus=*/2, /*at_bf=*/0);
    state.player(P1).rune_pool.energy = 0;  // can't pay [1]
    addToDeck(P1, kInvalidId);

    int hand_before = handSize(P1);
    driveResumableTrigger(kJax, P1, jax, &AuditFix3Test::yesAndFirstTarget, exec,
                          TriggerType::WhenYouPlayMe);

    EXPECT_EQ(state.getObject(gear).attached_to.value_or(kInvalidId), jax);
    EXPECT_EQ(handSize(P1), hand_before);  // no draw — couldn't afford
}

// ─── [486] Glasc Mixologist — Deathknell play <=3-cost unit from trash free ──

TEST_F(AuditFix3Test, Glasc_PlaysCheapUnitFromTrash) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto glasc = addUnit(P1, kGlasc, 5, 0);
    auto cheap = addToTrash(P1, kLegionRearguard);  // energy 2 <= 3

    fireTriggerAs(kGlasc, P1, glasc, TriggerType::WhenIDie, exec);

    // Unit removed from trash and put onto board (no longer in trash).
    EXPECT_FALSE(inTrash(P1, cheap));
    EXPECT_NE(state.getObject(cheap).zone, ZoneType::Trash);
}

TEST_F(AuditFix3Test, Glasc_CannotPlayExpensiveUnit) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto glasc = addUnit(P1, kGlasc, 5, 0);
    auto big = addToTrash(P1, kMagmaWurm);  // energy 8 > 3 — ineligible

    fireTriggerAs(kGlasc, P1, glasc, TriggerType::WhenIDie, exec);

    // No eligible unit — confirmOptional precondition fails, big stays.
    EXPECT_TRUE(inTrash(P1, big));
}

TEST_F(AuditFix3Test, Glasc_EmptyTrashNoOp) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto glasc = addUnit(P1, kGlasc, 5, 0);
    // No crash, nothing to do.
    fireTriggerAs(kGlasc, P1, glasc, TriggerType::WhenIDie, exec);
    EXPECT_EQ(trashSize(P1), 0);
}

TEST_F(AuditFix3Test, Glasc_DeclineLeavesUnitInTrash) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto glasc = addUnit(P1, kGlasc, 5, 0);
    auto cheap = addToTrash(P1, kLegionRearguard);

    // Drive resume loop, decline the optional play.
    driveResumableTrigger(kGlasc, P1, glasc, &AuditFix3Test::noChoice, exec,
                          TriggerType::WhenIDie);

    EXPECT_TRUE(inTrash(P1, cheap));
}

// ─── [534] Treasure Hoard — conquer here: pay [1] for Gold gear token (exh) ──

TEST_F(AuditFix3Test, TreasureHoard_PaysForGoldToken) {
    EffectExecutor exec(state, events, card_db);
    // Bind this battlefield card to BF 0.
    auto bf_card = state.createObject();
    state.getObject(bf_card).card_def_id = kTreasureHoard;
    state.getObject(bf_card).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = bf_card;
    state.player(P1).rune_pool.energy = 1;

    fireTriggerAs(kTreasureHoard, P1, bf_card, TriggerType::WhenYouConquerHere, exec);

    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);  // paid [1]
    // A new exhausted Gold gear token exists at BF 0.
    int gold = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isGear() && obj.name == "Gold") {
            ++gold;
            EXPECT_TRUE(obj.is_exhausted) << "Gold token must enter exhausted";
            EXPECT_EQ(obj.battlefieldId().value_or(kInvalidId), 0u);
        }
    }
    EXPECT_EQ(gold, 1);
}

TEST_F(AuditFix3Test, TreasureHoard_NoEnergyNoToken) {
    EffectExecutor exec(state, events, card_db);
    auto bf_card = state.createObject();
    state.getObject(bf_card).card_def_id = kTreasureHoard;
    state.getObject(bf_card).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = bf_card;
    state.player(P1).rune_pool.energy = 0;  // can't pay

    fireTriggerAs(kTreasureHoard, P1, bf_card, TriggerType::WhenYouConquerHere, exec);

    int gold = 0;
    for (auto& [id, obj] : state.objects) if (obj.isGear() && obj.name == "Gold") ++gold;
    EXPECT_EQ(gold, 0);
}

TEST_F(AuditFix3Test, TreasureHoard_DeclineNoToken) {
    EffectExecutor exec(state, events, card_db);
    auto bf_card = state.createObject();
    state.getObject(bf_card).card_def_id = kTreasureHoard;
    state.getObject(bf_card).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = bf_card;
    state.player(P1).rune_pool.energy = 1;

    driveResumableTrigger(kTreasureHoard, P1, bf_card, &AuditFix3Test::noChoice,
                          exec, TriggerType::WhenYouConquerHere);

    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);  // not paid
    int gold = 0;
    for (auto& [id, obj] : state.objects) if (obj.isGear() && obj.name == "Gold") ++gold;
    EXPECT_EQ(gold, 0);
}

// ─── [609] Mosstomper — Hunt 2 (XP) + Level-3 static +1M & Deflect ───────────

TEST_F(AuditFix3Test, Mosstomper_Hunt2GainsXP) {
    EffectExecutor exec(state, events, card_db);
    auto moss = addUnit(P1, kMosstomper, 3, 0);
    setXp(P1, 0);
    fireTriggerAs(kMosstomper, P1, moss, TriggerType::WhenIConquerOrHold, exec);
    EXPECT_EQ(state.player(P1).xp, 2);
}

TEST_F(AuditFix3Test, Mosstomper_Level3StaticBuffAtThreshold) {
    EffectExecutor exec(state, events, card_db);
    auto moss = addUnit(P1, kMosstomper, 3, 0);
    setXp(P1, 3);

    card_registry.get(kMosstomper)->applyPassiveAura(state, P1);

    auto& obj = state.getObject(moss);
    int might_bonus = 0;
    bool has_deflect = false;
    for (auto& ae : obj.aura_effects) {
        might_bonus += ae.might_bonus;
        if (ae.keyword == Keyword::Deflect) has_deflect = true;
    }
    EXPECT_EQ(might_bonus, 1);
    EXPECT_TRUE(has_deflect);
}

TEST_F(AuditFix3Test, Mosstomper_NoBuffBelowLevel3) {
    EffectExecutor exec(state, events, card_db);
    auto moss = addUnit(P1, kMosstomper, 3, 0);
    setXp(P1, 2);  // below threshold

    card_registry.get(kMosstomper)->applyPassiveAura(state, P1);

    EXPECT_TRUE(state.getObject(moss).aura_effects.empty());
}

// ─── [642] Hwei — WhenIMove: draw 1, discard 1, branch on discarded type ─────

TEST_F(AuditFix3Test, Hwei_DiscardSpellDrawsExtra) {
    EffectExecutor exec(state, events, card_db);
    auto hwei = addUnit(P1, kHwei, 5, 0);
    // Two deck cards: one for the mandatory initial draw, one for the
    // Spell-branch bonus draw (lets us observe the branch fired).
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto spell_in_hand = addToHand(P1, kDisintegrate);  // card_type Spell

    int hand_before = handSize(P1);  // = 1 (just the spell)
    // Picker: discard the spell.
    driveResumableTrigger(kHwei, P1, hwei,
        [&](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal) {
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == spell_in_hand)
                    return i;
            }
            return legal.empty() ? Intent{} : legal.front();
        },
        exec, TriggerType::WhenIMove);

    EXPECT_TRUE(inTrash(P1, spell_in_hand));  // discarded
    // draw 1 (initial) - 1 (discard) + 1 (Spell branch) => net +1 over start.
    EXPECT_EQ(handSize(P1), hand_before + 1);
    EXPECT_EQ(deckSize(P1), 0);  // both draws consumed the deck
}

TEST_F(AuditFix3Test, Hwei_DiscardGearReadiesRunes) {
    EffectExecutor exec(state, events, card_db);
    auto hwei = addUnit(P1, kHwei, 5, 0);
    addToDeck(P1, kInvalidId);
    // Gear card in hand to discard. Build a plain Gear-typed object in hand.
    auto gear_in_hand = state.createObject();
    {
        auto& o = state.getObject(gear_in_hand);
        o.owner = P1; o.controller = P1;
        o.card_type = CardType::Gear;
        o.name = "HandGear";
        o.zone = ZoneType::Hand;
        state.player(P1).hand.push_back(gear_in_hand);
    }
    // Two exhausted runes to ready.
    auto r1 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);

    driveResumableTrigger(kHwei, P1, hwei,
        [&](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal) {
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == gear_in_hand)
                    return i;
            }
            return legal.empty() ? Intent{} : legal.front();
        },
        exec, TriggerType::WhenIMove);

    EXPECT_FALSE(state.getObject(r1).is_exhausted);
    EXPECT_FALSE(state.getObject(r2).is_exhausted);
}

TEST_F(AuditFix3Test, Hwei_DiscardUnitGivesPlus3Might) {
    EffectExecutor exec(state, events, card_db);
    auto hwei = addUnit(P1, kHwei, 5, 0);
    addToDeck(P1, kInvalidId);
    auto unit_in_hand = addToHand(P1, kLegionRearguard);  // card_type Unit

    int might_before = state.getObject(hwei).current_might;

    driveResumableTrigger(kHwei, P1, hwei,
        [&](const std::vector<Intent>& legal) -> Intent {
            for (auto& i : legal) {
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == unit_in_hand)
                    return i;
            }
            return legal.empty() ? Intent{} : legal.front();
        },
        exec, TriggerType::WhenIMove);

    EXPECT_TRUE(inTrash(P1, unit_in_hand));
    EXPECT_EQ(state.getObject(hwei).current_might, might_before + 3);
}

// ─── [675] Master Yi — Hunt 2 (XP) + Level-6 static Deflect & Ganking ────────

TEST_F(AuditFix3Test, MasterYi_Hunt2GainsXP) {
    EffectExecutor exec(state, events, card_db);
    auto yi = addUnit(P1, kMasterYi, 4, 0);
    setXp(P1, 0);
    fireTriggerAs(kMasterYi, P1, yi, TriggerType::WhenIConquerOrHold, exec);
    EXPECT_EQ(state.player(P1).xp, 2);
}

TEST_F(AuditFix3Test, MasterYi_Level6StaticKeywords) {
    EffectExecutor exec(state, events, card_db);
    auto yi = addUnit(P1, kMasterYi, 4, 0);
    setXp(P1, 6);

    card_registry.get(kMasterYi)->applyPassiveAura(state, P1);

    bool deflect = false, ganking = false;
    for (auto& ae : state.getObject(yi).aura_effects) {
        if (ae.keyword == Keyword::Deflect) deflect = true;
        if (ae.keyword == Keyword::Ganking) ganking = true;
    }
    EXPECT_TRUE(deflect);
    EXPECT_TRUE(ganking);
}

TEST_F(AuditFix3Test, MasterYi_NoKeywordsBelowLevel6) {
    EffectExecutor exec(state, events, card_db);
    auto yi = addUnit(P1, kMasterYi, 4, 0);
    setXp(P1, 5);  // below threshold

    card_registry.get(kMasterYi)->applyPassiveAura(state, P1);

    EXPECT_TRUE(state.getObject(yi).aura_effects.empty());
}

// ─── [735] Sacrifice — kill a friendly Mighty (5+M), draw 2, channel 1 exh ───

TEST_F(AuditFix3Test, Sacrifice_KillsMightyDrawsChannels) {
    EffectExecutor exec(state, events, card_db);
    auto mighty = addUnit(P1, kInvalidId, /*might=*/5, 0);  // 5 M = Mighty
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto rune = addRuneToRuneDeck(P1, Domain::Fury);
    auto src = pushSpellOnChain(P1, kSacrifice);

    int hand_before = handSize(P1);
    // Direct invoke: pickTarget (no resuming) defaults to first legal target.
    invokeOnResolve(src, kSacrifice, P1, {}, exec);

    EXPECT_TRUE(inTrash(P1, mighty));            // additional cost paid
    EXPECT_EQ(handSize(P1), hand_before + 2);    // draw 2
    // Channeled rune entered base exhausted.
    auto& r = state.getObject(rune);
    EXPECT_EQ(r.zone, ZoneType::Base);
    EXPECT_TRUE(r.is_exhausted);
}

TEST_F(AuditFix3Test, Sacrifice_NoMightyMeansNoLegalTargets) {
    auto card = card_registry.get(kSacrifice);
    // Only a non-Mighty (4 M) friendly unit present.
    addUnit(P1, kInvalidId, /*might=*/4, 0);
    EXPECT_FALSE(card->hasLegalTargets(state, P1));
    EXPECT_TRUE(card->enumerateLegalTargets(state, P1).empty());
}

TEST_F(AuditFix3Test, Sacrifice_EnumeratesOnlyFriendlyMighty) {
    auto card = card_registry.get(kSacrifice);
    auto friendly_mighty = addUnit(P1, kInvalidId, /*might=*/5, 0);
    addUnit(P1, kInvalidId, /*might=*/4, 0);     // friendly but not Mighty
    addUnit(P2, kInvalidId, /*might=*/6, 0);     // enemy Mighty — excluded

    auto legal = card->enumerateLegalTargets(state, P1);
    ASSERT_EQ(legal.size(), 1u);
    EXPECT_EQ(legal[0], friendly_mighty);
    EXPECT_TRUE(card->hasLegalTargets(state, P1));
}

TEST_F(AuditFix3Test, Sacrifice_FizzlesWhenMightyVanishes) {
    EffectExecutor exec(state, events, card_db);
    // No Mighty unit at resolve → pickTarget returns invalid → no upside.
    addToDeck(P1, kInvalidId);
    auto src = pushSpellOnChain(P1, kSacrifice);

    int hand_before = handSize(P1);
    invokeOnResolve(src, kSacrifice, P1, {}, exec);

    EXPECT_EQ(handSize(P1), hand_before);  // no draw — cost unpaid
}

// ─── [765] Dusk Rose Lab — beginning: may kill a unit here to draw 1 ─────────

TEST_F(AuditFix3Test, DuskRoseLab_KillsUnitHereDrawsOne) {
    EffectExecutor exec(state, events, card_db);
    auto bf_card = state.createObject();
    state.getObject(bf_card).card_def_id = kDuskRoseLab;
    state.getObject(bf_card).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = bf_card;
    auto unit = addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);

    int hand_before = handSize(P1);
    // fireTriggerAs → confirmOptional defaults YES, pickTarget picks first.
    fireTriggerAs(kDuskRoseLab, P1, bf_card, TriggerType::AtStartOfBeginning, exec);

    EXPECT_TRUE(inTrash(P1, unit));            // killed
    EXPECT_EQ(handSize(P1), hand_before + 1);  // drew 1
}

TEST_F(AuditFix3Test, DuskRoseLab_NoUnitHereNoOp) {
    EffectExecutor exec(state, events, card_db);
    auto bf_card = state.createObject();
    state.getObject(bf_card).card_def_id = kDuskRoseLab;
    state.getObject(bf_card).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = bf_card;
    // A friendly unit, but at the OTHER battlefield (not here).
    addUnit(P1, kInvalidId, 2, /*at_bf=*/1);
    addToDeck(P1, kInvalidId);

    int hand_before = handSize(P1);
    fireTriggerAs(kDuskRoseLab, P1, bf_card, TriggerType::AtStartOfBeginning, exec);

    EXPECT_EQ(trashSize(P1), 0);
    EXPECT_EQ(handSize(P1), hand_before);  // no draw
}

TEST_F(AuditFix3Test, DuskRoseLab_DeclineKeepsUnit) {
    EffectExecutor exec(state, events, card_db);
    auto bf_card = state.createObject();
    state.getObject(bf_card).card_def_id = kDuskRoseLab;
    state.getObject(bf_card).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = bf_card;
    auto unit = addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);

    int hand_before = handSize(P1);
    driveResumableTrigger(kDuskRoseLab, P1, bf_card, &AuditFix3Test::noChoice,
                          exec, TriggerType::AtStartOfBeginning);

    EXPECT_FALSE(inTrash(P1, unit));           // not killed
    EXPECT_EQ(handSize(P1), hand_before);      // no draw
}

TEST_F(AuditFix3Test, DuskRoseLab_OnlyKillsOwnUnitHere) {
    EffectExecutor exec(state, events, card_db);
    auto bf_card = state.createObject();
    state.getObject(bf_card).card_def_id = kDuskRoseLab;
    state.getObject(bf_card).card_type = CardType::Battlefield;
    state.battlefields[0].card_object_id = bf_card;
    auto own = addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, 2, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);

    fireTriggerAs(kDuskRoseLab, P1, bf_card, TriggerType::AtStartOfBeginning, exec);

    EXPECT_TRUE(inTrash(P1, own));
    EXPECT_FALSE(inTrash(P2, enemy));  // enemy untouched
}

} // namespace
