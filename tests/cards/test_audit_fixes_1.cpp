// Per-card unit tests for audit-fix batch 1 (src/cards/manual/audit_fixes_1.cpp).
// Cards: 27, 133, 355, 431, 470, 529, 605, 622, 656, 712, 756.
//
// Each test verifies the actual implemented behavior in audit_fixes_1.cpp.
// Where the implementation documents an approximation/gap, the test exercises
// the approximation and a // TODO: notes the unimplemented clause.

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

// Card ids under test.
constexpr CardDefId kDarius      = 27;
constexpr CardDefId kFlurry      = 133;
constexpr CardDefId kDisarmRake  = 355;
constexpr CardDefId kAkshan      = 431;
constexpr CardDefId kEzreal      = 470;
constexpr CardDefId kRavenbloom  = 529;
constexpr CardDefId kPromoter    = 605;
constexpr CardDefId kVilemaw     = 622;
constexpr CardDefId kGemhand     = 656;
constexpr CardDefId kVex         = 712;
constexpr CardDefId kDeceiver    = 756;

// Some known def ids for filler cards.
constexpr CardDefId kSpellCleave = 4;    // a spell
constexpr CardDefId kUnitScorch  = 1;    // a unit
constexpr CardDefId kGearBallista = 17;  // a gear

class AuditFix1Test : public CardTestFixture {
protected:
    // Add a gear on the board controlled/owned by `owner`. Optionally attach
    // to a unit and tag as Equipment.
    GameObjectId addGearOnBoard(PlayerId owner, CardDefId def_id = kGearBallista,
                                int at_bf = -1, bool equipment = false) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        if (def_id != kInvalidId) obj.name = card_db.get(def_id).name;
        if (equipment) obj.tags.push_back("Equipment");
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    // A picker that says "yes" to a confirmOptional prompt (chosen_value==1)
    // and picks the first object-bearing intent for a pickTarget prompt.
    static Intent yesAndFirstTarget(const std::vector<Intent>& legal) {
        // confirmOptional publishes intents carrying chosen_value (no=0, yes=1).
        for (const auto& i : legal) {
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        }
        // pickTarget publishes intents carrying chosen_objects.
        for (const auto& i : legal) {
            if (!i.chosen_objects.empty()) return i;
        }
        return legal.empty() ? Intent{} : legal.front();
    }

    static Intent noPicker(const std::vector<Intent>& legal) {
        // For confirmOptional: return the "no" (chosen_value==0) option.
        for (const auto& i : legal) {
            if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
        }
        return legal.empty() ? Intent{} : legal.front();
    }
};

// ════════════════════════════════════════════════════════════════════════════
// [27] Darius, Trifarian
// "When you play your second card in a turn, give me +2 [M] this turn and
//  ready me." Fires on unit/spell/gear plays; gated on count == 2.
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, Darius_SecondCardBuffsAndReadies) {
    auto darius = addUnit(P1, kDarius, /*might=*/5, /*at_bf=*/0);
    state.getObject(darius).is_exhausted = true;        // entered exhausted
    state.player(P1).cards_played_this_turn = 2;        // this is the 2nd play
    EffectExecutor exec(state, events, card_db);

    fireTriggerAs(kDarius, P1, darius, TriggerType::WhenYouPlayAUnit, exec);

    EXPECT_EQ(state.getObject(darius).current_might, 7);   // 5 + 2
    EXPECT_FALSE(state.getObject(darius).is_exhausted);     // readied
}

TEST_F(AuditFix1Test, Darius_FirstCardDoesNothing) {
    auto darius = addUnit(P1, kDarius, 5, 0);
    state.getObject(darius).is_exhausted = true;
    state.player(P1).cards_played_this_turn = 1;           // only the first play
    EffectExecutor exec(state, events, card_db);

    fireTriggerAs(kDarius, P1, darius, TriggerType::WhenYouPlayAUnit, exec);

    EXPECT_EQ(state.getObject(darius).current_might, 5);    // no buff
    EXPECT_TRUE(state.getObject(darius).is_exhausted);      // not readied
}

TEST_F(AuditFix1Test, Darius_ThirdCardDoesNothing) {
    auto darius = addUnit(P1, kDarius, 5, 0);
    state.getObject(darius).is_exhausted = true;
    state.player(P1).cards_played_this_turn = 3;           // third play
    EffectExecutor exec(state, events, card_db);

    fireTriggerAs(kDarius, P1, darius, TriggerType::WhenYouPlayASpell, exec);

    EXPECT_EQ(state.getObject(darius).current_might, 5);
    EXPECT_TRUE(state.getObject(darius).is_exhausted);
}

TEST_F(AuditFix1Test, Darius_FiresOnSpellAndGearTriggers) {
    // Verifies the FIX: "second card" counts any type, not just units.
    Card* c = card_registry.get(kDarius);
    ASSERT_NE(c, nullptr);
    auto types = c->triggerTypes();
    auto has = [&](TriggerType t) {
        return std::find(types.begin(), types.end(), t) != types.end();
    };
    EXPECT_TRUE(has(TriggerType::WhenYouPlayAUnit));
    EXPECT_TRUE(has(TriggerType::WhenYouPlayASpell));
    EXPECT_TRUE(has(TriggerType::WhenYouPlayAGear));

    // Fire via the gear-play trigger as the 2nd card -> still buffs.
    auto darius = addUnit(P1, kDarius, 5, 0);
    state.getObject(darius).is_exhausted = true;
    state.player(P1).cards_played_this_turn = 2;
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kDarius, P1, darius, TriggerType::WhenYouPlayAGear, exec);
    EXPECT_EQ(state.getObject(darius).current_might, 7);
    EXPECT_FALSE(state.getObject(darius).is_exhausted);
}

// ════════════════════════════════════════════════════════════════════════════
// [133] Flurry of Blades
// "[Reaction] Deal 1 to all units at battlefields." (NOT base units.)
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, Flurry_DamagesAllUnitsAtBattlefields) {
    auto u1 = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto u2 = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/1);
    EffectExecutor exec(state, events, card_db);

    auto spell = pushSpellOnChain(P1, kFlurry);
    invokeOnResolve(spell, kFlurry, P1, {}, exec);

    EXPECT_EQ(state.getObject(u1).damage_marked, 1);
    EXPECT_EQ(state.getObject(u2).damage_marked, 1);
}

TEST_F(AuditFix1Test, Flurry_DoesNotDamageBaseUnits) {
    auto bf_unit = addUnit(P1, kInvalidId, 2, /*at_bf=*/0);
    auto base_unit = addUnit(P1, kInvalidId, 2, /*at_bf=*/-1);  // in base
    EffectExecutor exec(state, events, card_db);

    auto spell = pushSpellOnChain(P1, kFlurry);
    invokeOnResolve(spell, kFlurry, P1, {}, exec);

    EXPECT_EQ(state.getObject(bf_unit).damage_marked, 1);
    EXPECT_EQ(state.getObject(base_unit).damage_marked, 0);  // base untouched
}

TEST_F(AuditFix1Test, Flurry_KillsLethallyDamagedUnit) {
    auto frail = addUnit(P1, kInvalidId, /*might=*/1, /*at_bf=*/0);  // dies to 1
    auto tough = addUnit(P2, kInvalidId, /*might=*/5, /*at_bf=*/0);  // survives
    EffectExecutor exec(state, events, card_db);

    auto spell = pushSpellOnChain(P1, kFlurry);
    invokeOnResolve(spell, kFlurry, P1, {}, exec);

    // 1-might unit took lethal damage and was killed (removed from board).
    EXPECT_FALSE(state.getObject(frail).location.has_value());
    EXPECT_TRUE(state.getObject(tough).location.has_value());
    EXPECT_EQ(state.getObject(tough).damage_marked, 1);
}

TEST_F(AuditFix1Test, Flurry_NoUnitsNoOp) {
    EffectExecutor exec(state, events, card_db);
    auto spell = pushSpellOnChain(P1, kFlurry);
    // No units on board; should not crash / no-op.
    invokeOnResolve(spell, kFlurry, P1, {}, exec);
    SUCCEED();
}

// ════════════════════════════════════════════════════════════════════════════
// [355] Disarming Rake
// "When you play me, you may kill a gear." confirmOptional + pickTarget.
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, DisarmingRake_YesKillsChosenGear) {
    auto rake = addUnit(P1, kDisarmRake, 2, 0);
    auto enemy_gear = addGearOnBoard(P2, kGearBallista, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);

    driveResumableTrigger(kDisarmRake, P1, rake,
                          /*picker=*/&AuditFix1Test::yesAndFirstTarget, exec);

    // Gear killed -> off the board.
    EXPECT_FALSE(state.getObject(enemy_gear).location.has_value());
}

TEST_F(AuditFix1Test, DisarmingRake_DeclineLeavesGear) {
    auto rake = addUnit(P1, kDisarmRake, 2, 0);
    auto gear = addGearOnBoard(P2, kGearBallista, 0);
    EffectExecutor exec(state, events, card_db);

    driveResumableTrigger(kDisarmRake, P1, rake,
                          /*picker=*/&AuditFix1Test::noPicker, exec);

    EXPECT_TRUE(state.getObject(gear).location.has_value());  // still on board
}

TEST_F(AuditFix1Test, DisarmingRake_NoGearNoOp) {
    auto rake = addUnit(P1, kDisarmRake, 2, 0);
    EffectExecutor exec(state, events, card_db);

    // No gear exists; confirmOptional's still_legal() is false -> skip.
    // (No choice published, no crash.)
    driveResumableTrigger(kDisarmRake, P1, rake,
                          /*picker=*/&AuditFix1Test::yesAndFirstTarget, exec);
    SUCCEED();
}

TEST_F(AuditFix1Test, DisarmingRake_CanKillOwnGear) {
    // Implementation collects ALL gears on board (no enemy-only filter).
    auto rake = addUnit(P1, kDisarmRake, 2, 0);
    auto own_gear = addGearOnBoard(P1, kGearBallista, 0);
    EffectExecutor exec(state, events, card_db);

    driveResumableTrigger(kDisarmRake, P1, rake,
                          /*picker=*/&AuditFix1Test::yesAndFirstTarget, exec);

    EXPECT_FALSE(state.getObject(own_gear).location.has_value());
}

// ════════════════════════════════════════════════════════════════════════════
// [431] Akshan, Mischievous
// "...move an enemy gear to your base... If it's an Equipment, attach it to me."
// Approximation: always steals on play (no additional-cost gate), takes
// permanent control.
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, Akshan_StealsEnemyGearToBase) {
    auto akshan = addUnit(P1, kAkshan, 4, 0);
    auto enemy_gear = addGearOnBoard(P2, kGearBallista, /*at_bf=*/0,
                                     /*equipment=*/false);
    // Simulate paying the optional [O][O] additional cost (set at play time by
    // maybePayOptionalAdditionalCost) — the steal is gated on it.
    state.getObject(akshan).card_counters["__akshan_paid"] = 1;
    EffectExecutor exec(state, events, card_db);

    fireTriggerAs(kAkshan, P1, akshan, TriggerType::WhenYouPlayMe, exec);

    auto& g = state.getObject(enemy_gear);
    EXPECT_EQ(g.controller, P1);                            // control taken
    EXPECT_FALSE(g.control_reverts_eot);                    // permanent
    ASSERT_TRUE(g.location.has_value());
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*g.location));  // moved to base
    // Non-equipment: NOT attached to Akshan.
    EXPECT_FALSE(g.attached_to.has_value());
}

TEST_F(AuditFix1Test, Akshan_AttachesStolenEquipment) {
    auto akshan = addUnit(P1, kAkshan, 4, 0);
    auto enemy_equip = addGearOnBoard(P2, kGearBallista, /*at_bf=*/0,
                                      /*equipment=*/true);
    state.getObject(enemy_equip).might_bonus = 2;
    state.getObject(akshan).card_counters["__akshan_paid"] = 1;  // paid the additional cost
    EffectExecutor exec(state, events, card_db);

    fireTriggerAs(kAkshan, P1, akshan, TriggerType::WhenYouPlayMe, exec);

    auto& g = state.getObject(enemy_equip);
    auto& self = state.getObject(akshan);
    EXPECT_EQ(g.controller, P1);
    ASSERT_TRUE(g.attached_to.has_value());
    EXPECT_EQ(*g.attached_to, akshan);
    EXPECT_NE(std::find(self.attachments.begin(), self.attachments.end(),
                        enemy_equip),
              self.attachments.end());
    EXPECT_EQ(self.attachment_might_bonus, 2);             // bonus applied
    EXPECT_EQ(self.current_might, 6);                       // 4 + 2
}

TEST_F(AuditFix1Test, Akshan_NoEnemyGearNoOp) {
    auto akshan = addUnit(P1, kAkshan, 4, 0);
    // Only a FRIENDLY gear exists -> not stolen.
    auto friendly = addGearOnBoard(P1, kGearBallista, 0);
    state.getObject(akshan).card_counters["__akshan_paid"] = 1;  // even if paid
    EffectExecutor exec(state, events, card_db);

    fireTriggerAs(kAkshan, P1, akshan, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(state.getObject(friendly).controller, P1);   // unchanged
    EXPECT_FALSE(state.getObject(friendly).attached_to.has_value());
    EXPECT_EQ(state.getObject(akshan).current_might, 4);
    // TODO: "until I leave the board" control reversion is not modeled.
}

TEST_F(AuditFix1Test, Akshan_NoStealWhenAdditionalCostNotPaid) {
    auto akshan = addUnit(P1, kAkshan, 4, 0);
    auto enemy_gear = addGearOnBoard(P2, kGearBallista, /*at_bf=*/0,
                                     /*equipment=*/false);
    // No __akshan_paid flag -> additional cost not paid -> no steal.
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kAkshan, P1, akshan, TriggerType::WhenYouPlayMe, exec);
    EXPECT_EQ(state.getObject(enemy_gear).controller, P2);  // NOT stolen
}

// ════════════════════════════════════════════════════════════════════════════
// [470] Ezreal, Prodigy
// "When you play me, discard 1, then draw 2." (resumable discard)
// Second clause (optional additional costs cost less) NOT implemented.
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, Ezreal_DiscardOneDrawTwo) {
    auto ezreal = addUnit(P1, kEzreal, 3, 0);
    auto h1 = addToHand(P1, kSpellCleave);   // discard candidate
    // Stock the deck so draw 2 has cards.
    auto d1 = addToDeck(P1, kUnitScorch);
    auto d2 = addToDeck(P1, kUnitScorch);

    EffectExecutor exec(state, events, card_db);
    EXPECT_EQ(handSize(P1), 1);

    driveResumableTrigger(kEzreal, P1, ezreal,
        /*picker=*/[](const std::vector<Intent>& legal) {
            // Discard prompt publishes MakeChoice intents with chosen_objects.
            return legal.empty() ? Intent{} : legal.front();
        }, exec);

    // Discarded the 1 hand card, drew 2 -> hand now has 2.
    EXPECT_TRUE(inTrash(P1, h1));
    EXPECT_EQ(handSize(P1), 2);
    EXPECT_TRUE(inHand(P1, d1));
    EXPECT_TRUE(inHand(P1, d2));
}

TEST_F(AuditFix1Test, Ezreal_EmptyHandStillDrawsTwo) {
    auto ezreal = addUnit(P1, kEzreal, 3, 0);
    auto d1 = addToDeck(P1, kUnitScorch);
    auto d2 = addToDeck(P1, kUnitScorch);
    EffectExecutor exec(state, events, card_db);
    EXPECT_EQ(handSize(P1), 0);

    // Empty hand -> case 0 draws 2 immediately, no choice published.
    driveResumableTrigger(kEzreal, P1, ezreal, nullptr, exec);

    EXPECT_EQ(handSize(P1), 2);
    EXPECT_TRUE(inHand(P1, d1));
    EXPECT_TRUE(inHand(P1, d2));
    // TODO: "Optional additional costs you pay cost [1]/[A] less" not modeled.
}

// ════════════════════════════════════════════════════════════════════════════
// [529] Ravenbloom Conservatory (battlefield)
// "When you defend here, reveal top of deck. Spell -> hand; else recycle."
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, Ravenbloom_SpellGoesToHand) {
    auto bf_card = addUnit(P1, kRavenbloom, 0, 0);  // proxy source object
    // Top of deck (back of vector) is a spell.
    addToDeck(P1, kUnitScorch);
    auto top_spell = addToDeck(P1, kSpellCleave);
    EffectExecutor exec(state, events, card_db);
    int before = deckSize(P1);

    fireTriggerAs(kRavenbloom, P1, bf_card, TriggerType::WhenYouDefendHere, exec);

    EXPECT_TRUE(inHand(P1, top_spell));
    EXPECT_FALSE(inDeck(P1, top_spell));
    EXPECT_EQ(deckSize(P1), before - 1);
}

TEST_F(AuditFix1Test, Ravenbloom_NonSpellRecycledToBottom) {
    auto bf_card = addUnit(P1, kRavenbloom, 0, 0);
    // bottom = front; top = back. Put a unit on top.
    auto bottom_card = addToDeck(P1, kSpellCleave);  // front (bottom)
    auto top_unit = addToDeck(P1, kUnitScorch);      // back (top)
    EffectExecutor exec(state, events, card_db);
    int before = deckSize(P1);

    fireTriggerAs(kRavenbloom, P1, bf_card, TriggerType::WhenYouDefendHere, exec);

    // Not put in hand; recycled to bottom of deck (still in deck).
    EXPECT_FALSE(inHand(P1, top_unit));
    EXPECT_TRUE(inDeck(P1, top_unit));
    EXPECT_EQ(deckSize(P1), before);                       // count unchanged
    // Recycled card lands at the BOTTOM (front of vector, index 0).
    EXPECT_EQ(state.player(P1).main_deck.front(), top_unit);
    (void)bottom_card;
}

TEST_F(AuditFix1Test, Ravenbloom_EmptyDeckNoOp) {
    auto bf_card = addUnit(P1, kRavenbloom, 0, 0);
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kRavenbloom, P1, bf_card, TriggerType::WhenYouDefendHere, exec);
    EXPECT_EQ(deckSize(P1), 0);
    EXPECT_EQ(handSize(P1), 0);
}

// ════════════════════════════════════════════════════════════════════════════
// [605] Enthusiastic Promoter
// "[Backline] When I hold, [Buff] all units here. (Give +1 [M] if it doesn't
//  have one.)" Applies to ALL units at the BF; skips already-buffed.
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, Promoter_BuffsUnbuffedUnitsHere) {
    auto promoter = addUnit(P1, kPromoter, 2, /*at_bf=*/0);
    auto ally = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);

    fireTriggerAs(kPromoter, P1, promoter, TriggerType::WhenIHold, exec);

    // Impl buffs ALL units here (both players) that have buff_count == 0.
    EXPECT_EQ(state.getObject(ally).current_might, 4);     // 3 + 1
    EXPECT_EQ(state.getObject(enemy).current_might, 4);    // 3 + 1 (both players)
    EXPECT_EQ(state.getObject(promoter).current_might, 3); // 2 + 1 (itself)
}

TEST_F(AuditFix1Test, Promoter_SkipsAlreadyBuffedUnit) {
    auto promoter = addUnit(P1, kPromoter, 2, 0);
    auto buffed = addUnit(P1, kInvalidId, 3, 0);
    state.getObject(buffed).buff_count = 1;                // already has a buff
    state.getObject(buffed).recomputeMight();              // -> 4
    EffectExecutor exec(state, events, card_db);

    fireTriggerAs(kPromoter, P1, promoter, TriggerType::WhenIHold, exec);

    // "if it doesn't have one" -> already-buffed unit is skipped (stays 4).
    EXPECT_EQ(state.getObject(buffed).current_might, 4);
    EXPECT_EQ(state.getObject(buffed).buff_count, 1);      // unchanged
}

TEST_F(AuditFix1Test, Promoter_DoesNotBuffOtherBattlefield) {
    auto promoter = addUnit(P1, kPromoter, 2, /*at_bf=*/0);
    auto elsewhere = addUnit(P1, kInvalidId, 3, /*at_bf=*/1);  // different BF
    EffectExecutor exec(state, events, card_db);

    fireTriggerAs(kPromoter, P1, promoter, TriggerType::WhenIHold, exec);

    EXPECT_EQ(state.getObject(elsewhere).current_might, 3);   // not buffed
}

// ════════════════════════════════════════════════════════════════════════════
// [622] Vilemaw
// "[Ambush] ... When I hold, draw 1." (Static "enemy units deal no combat
//  damage" clause NOT implemented.)
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, Vilemaw_HoldDrawsOne) {
    auto vilemaw = addUnit(P1, kVilemaw, 8, 0);
    auto d1 = addToDeck(P1, kUnitScorch);
    EffectExecutor exec(state, events, card_db);
    EXPECT_EQ(handSize(P1), 0);

    fireTriggerAs(kVilemaw, P1, vilemaw, TriggerType::WhenIHold, exec);

    EXPECT_EQ(handSize(P1), 1);
    EXPECT_TRUE(inHand(P1, d1));
    // TODO: "Enemy units here with less Might don't deal combat damage" is a
    // documented gap — no per-unit "deals no combat damage" flag exists.
}

TEST_F(AuditFix1Test, Vilemaw_HoldEmptyDeckDrawsNothing) {
    auto vilemaw = addUnit(P1, kVilemaw, 8, 0);
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kVilemaw, P1, vilemaw, TriggerType::WhenIHold, exec);
    EXPECT_EQ(handSize(P1), 0);
}

// ════════════════════════════════════════════════════════════════════════════
// [656] Gemhand Hunter
// "[Hunt] (conquer/hold -> gain 1 XP). [Level 6] I have +1 [M]." (passive)
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, Gemhand_HuntGainsXpOnConquerOrHold) {
    auto gem = addUnit(P1, kGemhand, 2, 0);
    setXp(P1, 3);
    EffectExecutor exec(state, events, card_db);

    fireTriggerAs(kGemhand, P1, gem, TriggerType::WhenIConquerOrHold, exec);

    EXPECT_EQ(state.player(P1).xp, 4);   // gained 1 XP
}

TEST_F(AuditFix1Test, Gemhand_Level6PassiveGrantsMightAtThreshold) {
    auto gem = addUnit(P1, kGemhand, 2, 0);
    setXp(P1, 6);                        // at threshold
    state.getObject(gem).aura_effects.clear();
    Card* c = card_registry.get(kGemhand);
    ASSERT_NE(c, nullptr);

    c->applyPassiveAura(state, P1);

    int bonus = 0;
    for (auto& ae : state.getObject(gem).aura_effects) bonus += ae.might_bonus;
    EXPECT_EQ(bonus, 1);                 // +1 M aura granted
}

TEST_F(AuditFix1Test, Gemhand_Level6PassiveNotGrantedBelowThreshold) {
    auto gem = addUnit(P1, kGemhand, 2, 0);
    setXp(P1, 5);                        // below threshold
    state.getObject(gem).aura_effects.clear();
    Card* c = card_registry.get(kGemhand);
    ASSERT_NE(c, nullptr);

    c->applyPassiveAura(state, P1);

    EXPECT_TRUE(state.getObject(gem).aura_effects.empty());  // no aura
}

TEST_F(AuditFix1Test, Gemhand_PassiveOnlyAffectsControllerCopies) {
    auto mine = addUnit(P1, kGemhand, 2, 0);
    auto theirs = addUnit(P2, kGemhand, 2, 0);
    setXp(P1, 6);
    setXp(P2, 0);
    state.getObject(mine).aura_effects.clear();
    state.getObject(theirs).aura_effects.clear();
    Card* c = card_registry.get(kGemhand);
    ASSERT_NE(c, nullptr);

    c->applyPassiveAura(state, P1);

    EXPECT_FALSE(state.getObject(mine).aura_effects.empty());
    EXPECT_TRUE(state.getObject(theirs).aura_effects.empty());  // P2 copy not buffed
}

// ════════════════════════════════════════════════════════════════════════════
// [712] Vex, Apathetic
// Deflect engine-handled; "opponent plays a unit -> stun" NOT implemented.
// Override is a no-op stub. Verify it registers and the registry keyword
// (Deflect) is intact.
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, Vex_FiresOnOpponentPlaysAUnit) {
    Card* c = card_registry.get(kVex);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->cardDefId(), kVex);
    // The "when an opponent plays a unit while I'm at a battlefield, stun it"
    // clause now has an engine trigger (WhenOpponentPlaysAUnit). [Deflect]
    // remains engine-handled. (The "can't move / no combat damage this turn"
    // rider still needs per-unit flags — documented gap.)
    EXPECT_TRUE(c->firesOn(TriggerType::WhenOpponentPlaysAUnit));
}

TEST_F(AuditFix1Test, Vex_DeflectKeywordFromRegistry) {
    // Deflect is engine-handled from the card def's keyword set.
    const auto& def = card_db.get(kVex);
    EXPECT_TRUE(def.keywords.has(Keyword::Deflect));
}

// ════════════════════════════════════════════════════════════════════════════
// [756] Deceiver (legend)
// "When you conquer or hold, you may discard 1 and exhaust me to play a ready
//  Reflection token there. It becomes a copy of another unit there. Give it
//  [Temporary]."
// ════════════════════════════════════════════════════════════════════════════

TEST_F(AuditFix1Test, Deceiver_CreatesCopyTokenWithTemporary) {
    // A legend object as the source.
    auto legend = addUnit(P1, kDeceiver, 0, /*at_bf=*/-1);
    state.getObject(legend).card_type = CardType::Legend;
    state.getObject(legend).is_exhausted = false;
    // P1 controls BF 0 and has a unit there to copy.
    state.battlefields[0].controller = P1;
    auto model = addUnit(P1, kUnitScorch, /*might=*/5, /*at_bf=*/0);
    // A card to discard.
    auto discard_me = addToHand(P1, kSpellCleave);

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);  // copyUnit needs the registry/db

    int objs_before = state.objects.size();

    driveResumableTrigger(kDeceiver, P1, legend,
        /*picker=*/[](const std::vector<Intent>& legal) {
            return legal.empty() ? Intent{} : legal.front();  // discard first
        }, exec, TriggerType::WhenIConquerOrHold);

    // Legend exhausted (cost paid).
    EXPECT_TRUE(state.getObject(legend).is_exhausted);
    // Discarded a hand card.
    EXPECT_TRUE(inTrash(P1, discard_me));
    // A new token object was created.
    EXPECT_GT((int)state.objects.size(), objs_before);

    // Find the token: a Token unit at BF 0 that is NOT the model.
    GameObjectId token = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (id == model) continue;
        if (obj.super_type != SuperType::Token) continue;
        if (!obj.isUnit()) continue;
        if (obj.battlefieldId() == 0u) { token = id; break; }
    }
    ASSERT_NE(token, kInvalidId);
    auto& tk = state.getObject(token);
    // Became a copy of the model (Blazing Scorcher: 5M).
    EXPECT_EQ(tk.card_def_id, kUnitScorch);
    EXPECT_EQ(tk.base_might, card_db.get(kUnitScorch).might);
    // Keeps [Temporary] after the copy overwrote keywords.
    EXPECT_TRUE(tk.keywords.has(Keyword::Temporary));
    // Token stays super_type Token.
    EXPECT_EQ(tk.super_type, SuperType::Token);
}

TEST_F(AuditFix1Test, Deceiver_NoOpWhenLegendExhausted) {
    auto legend = addUnit(P1, kDeceiver, 0, -1);
    state.getObject(legend).card_type = CardType::Legend;
    state.getObject(legend).is_exhausted = true;           // can't pay exhaust
    addToHand(P1, kSpellCleave);
    state.battlefields[0].controller = P1;

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    int objs_before = state.objects.size();

    driveResumableTrigger(kDeceiver, P1, legend, nullptr, exec,
                          TriggerType::WhenIConquerOrHold);

    // No discard, no token created.
    EXPECT_EQ((int)state.objects.size(), objs_before);
    EXPECT_EQ(handSize(P1), 1);                            // hand untouched
}

TEST_F(AuditFix1Test, Deceiver_NoOpWhenHandEmpty) {
    auto legend = addUnit(P1, kDeceiver, 0, -1);
    state.getObject(legend).card_type = CardType::Legend;
    state.getObject(legend).is_exhausted = false;
    state.battlefields[0].controller = P1;
    // Empty hand -> can't pay discard cost.

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    int objs_before = state.objects.size();

    driveResumableTrigger(kDeceiver, P1, legend, nullptr, exec,
                          TriggerType::WhenIConquerOrHold);

    EXPECT_FALSE(state.getObject(legend).is_exhausted);    // cost not paid
    EXPECT_EQ((int)state.objects.size(), objs_before);     // no token
}

TEST_F(AuditFix1Test, Deceiver_NoUnitToCopyStillMakesTemporaryToken) {
    // Controls a BF but has NO other unit there -> token created, not copied,
    // still gets [Temporary].
    auto legend = addUnit(P1, kDeceiver, 0, -1);
    state.getObject(legend).card_type = CardType::Legend;
    state.getObject(legend).is_exhausted = false;
    state.battlefields[0].controller = P1;
    auto discard_me = addToHand(P1, kSpellCleave);

    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);

    driveResumableTrigger(kDeceiver, P1, legend,
        /*picker=*/[](const std::vector<Intent>& legal) {
            return legal.empty() ? Intent{} : legal.front();
        }, exec, TriggerType::WhenIConquerOrHold);

    EXPECT_TRUE(inTrash(P1, discard_me));
    // Token exists at BF 0, still a Reflection token with Temporary.
    GameObjectId token = kInvalidId;
    for (auto& [id, obj] : state.objects) {
        if (obj.super_type != SuperType::Token || !obj.isUnit()) continue;
        if (obj.battlefieldId() == 0u) { token = id; break; }
    }
    ASSERT_NE(token, kInvalidId);
    auto& tk = state.getObject(token);
    EXPECT_EQ(tk.name, "Reflection");           // not copied (no model)
    EXPECT_TRUE(tk.keywords.has(Keyword::Temporary));
}

} // namespace
