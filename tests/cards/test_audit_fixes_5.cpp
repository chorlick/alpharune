// Per-card unit tests for audit-fix batch 5 (src/cards/manual/audit_fixes_5.cpp).
//
// Cards: 48 Meditation, 277 Monastery of Hirana, 382 Svellsongur,
//        460 Edge of Night, 506 Fire Below the Mountain, 544 Yone Blademaster,
//        613 Ivern Nurturer, 644 Lillia Fae Fawn, 682 Rengar Trophy Hunter,
//        739 Ivern Friend to All, 769 Gardens of Becoming.

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

class AuditFix5Test : public CardTestFixture {
protected:
    // Card ids for this batch.
    static constexpr CardDefId kMeditation = 48;
    static constexpr CardDefId kMonastery = 277;
    static constexpr CardDefId kSvellsongur = 382;
    static constexpr CardDefId kEdgeOfNight = 460;
    static constexpr CardDefId kFireBelowMtn = 506;
    static constexpr CardDefId kYone = 544;
    static constexpr CardDefId kIvernNurturer = 613;
    static constexpr CardDefId kLillia = 644;
    static constexpr CardDefId kRengar = 682;
    static constexpr CardDefId kIvernFriend = 739;
    static constexpr CardDefId kGardens = 769;

    // Add a gear object on the board (at controller's base by default), with
    // the given might_bonus and optional Equipment tag.
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int might_bonus = 0,
                         bool equipment_tag = true) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.domains = def.domains;
            obj.might_bonus = def.might_bonus;
        }
        obj.might_bonus = might_bonus;
        if (equipment_tag) obj.tags.push_back("Equipment");
        obj.zone = ZoneType::Base;
        obj.location = BaseLocation{owner};
        return id;
    }

    // Picker that always returns the "yes" (last) intent for confirmOptional,
    // which publishes [no, yes].
    static Intent pickYes(const std::vector<Intent>& legal) {
        return legal.empty() ? Intent{} : legal.back();
    }
    // Picker that always returns the "no" (first) intent.
    static Intent pickNo(const std::vector<Intent>& legal) {
        return legal.empty() ? Intent{} : legal.front();
    }
    // Find a token unit by name controlled by a player.
    GameObjectId findTokenByName(PlayerId owner, const std::string& name) {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_type == CardType::Unit && obj.controller == owner &&
                obj.name == name) {
                return id;
            }
        }
        return kInvalidId;
    }
};

// ─── [48] Meditation ─────────────────────────────────────────────────────────

// Default (non-chain) path: a ready friendly unit exists -> confirmOptional
// returns YES -> exhaust it and draw 2.
TEST_F(AuditFix5Test, Meditation_ExhaustFriendlyDrawsTwo) {
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 3);          // ready friendly at base
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto spell = addToHand(P1, kMeditation);

    int hand_before = handSize(P1);
    invokeOnResolve(spell, kMeditation, P1, {}, exec);

    EXPECT_TRUE(state.getObject(unit).is_exhausted)
        << "should exhaust the friendly unit on the yes branch";
    // Drew 2 cards from deck into hand.
    EXPECT_EQ(handSize(P1), hand_before + 2);
}

// No ready friendly unit -> precondition fails -> confirmOptional returns 0 ->
// draw 1.
TEST_F(AuditFix5Test, Meditation_NoFriendlyDrawsOne) {
    EffectExecutor exec(state, events, card_db);
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto spell = addToHand(P1, kMeditation);

    int hand_before = handSize(P1);
    invokeOnResolve(spell, kMeditation, P1, {}, exec);

    EXPECT_EQ(handSize(P1), hand_before + 1)
        << "no friendly to exhaust -> draws only 1";
}

// An exhausted friendly unit does NOT count as a legal exhaust target -> draws 1.
TEST_F(AuditFix5Test, Meditation_ExhaustedFriendlyIgnored) {
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 3);
    state.getObject(unit).is_exhausted = true;
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    auto spell = addToHand(P1, kMeditation);

    int hand_before = handSize(P1);
    invokeOnResolve(spell, kMeditation, P1, {}, exec);

    EXPECT_EQ(handSize(P1), hand_before + 1);
    EXPECT_TRUE(state.getObject(unit).is_exhausted);
}

// Chain-driven "no" branch: agent declines -> draws 1, friendly stays ready.
TEST_F(AuditFix5Test, Meditation_DeclineDrawsOne) {
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 3);          // legal target exists
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);

    int hand_before = handSize(P1);
    driveResumable(kMeditation, P1, /*source=*/unit, &AuditFix5Test::pickNo, exec);

    EXPECT_EQ(handSize(P1), hand_before + 1) << "declined -> draw 1";
    EXPECT_FALSE(state.getObject(unit).is_exhausted)
        << "declining must not exhaust the unit";
}

// ─── [277] Monastery of Hirana ───────────────────────────────────────────────

// WhenYouConquerHere + yes branch: spend a buff (decrement buff_count) and
// draw 1.
TEST_F(AuditFix5Test, Monastery_SpendBuffDrawsOne) {
    EffectExecutor exec(state, events, card_db);
    auto bf_src = addUnit(P1, kInvalidId, 1);  // use as the BF's source obj
    auto unit = addUnit(P1, kInvalidId, 3);
    state.getObject(unit).buff_count = 2;
    addToDeck(P1, kInvalidId);

    int hand_before = handSize(P1);
    driveResumableTrigger(kMonastery, P1, bf_src, &AuditFix5Test::pickYes, exec,
                          TriggerType::WhenYouConquerHere);

    EXPECT_EQ(state.getObject(unit).buff_count, 1) << "one buff spent";
    EXPECT_EQ(handSize(P1), hand_before + 1) << "drew 1";
}

// Decline (no branch): no buff spent, no draw.
TEST_F(AuditFix5Test, Monastery_DeclineNoDraw) {
    EffectExecutor exec(state, events, card_db);
    auto bf_src = addUnit(P1, kInvalidId, 1);
    auto unit = addUnit(P1, kInvalidId, 3);
    state.getObject(unit).buff_count = 1;
    addToDeck(P1, kInvalidId);

    int hand_before = handSize(P1);
    driveResumableTrigger(kMonastery, P1, bf_src, &AuditFix5Test::pickNo, exec,
                          TriggerType::WhenYouConquerHere);

    EXPECT_EQ(state.getObject(unit).buff_count, 1) << "no buff spent on decline";
    EXPECT_EQ(handSize(P1), hand_before) << "no draw on decline";
}

// No buffed unit -> precondition fails -> no draw even though it fired.
TEST_F(AuditFix5Test, Monastery_NoBuffedUnitNoDraw) {
    EffectExecutor exec(state, events, card_db);
    auto bf_src = addUnit(P1, kInvalidId, 1);
    addUnit(P1, kInvalidId, 3);  // buff_count == 0
    addToDeck(P1, kInvalidId);

    int hand_before = handSize(P1);
    driveResumableTrigger(kMonastery, P1, bf_src, &AuditFix5Test::pickYes, exec,
                          TriggerType::WhenYouConquerHere);

    EXPECT_EQ(handSize(P1), hand_before) << "no buffed unit -> nothing happens";
}

// ─── [382] Svellsongur ───────────────────────────────────────────────────────

// Equip succeeds when 1 ready rune + a Calm-domain rune are present.
// might_bonus is 0 so attachment_might_bonus stays 0.
TEST_F(AuditFix5Test, Svellsongur_EquipSucceeds) {
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 4);
    auto gear = addGear(P1, kSvellsongur, /*might_bonus=*/0);
    // Need 1 ready rune for energy and a Calm rune for power. One Calm rune
    // can serve as the energy exhaust AND the recycle (energy_cost=1).
    addRune(P1, Domain::Calm);
    addRune(P1, Domain::Calm);

    Card* card = card_registry.get(kSvellsongur);
    ASSERT_NE(card, nullptr);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = card->onEquip(ctx, unit);

    EXPECT_TRUE(ok);
    EXPECT_EQ(state.getObject(gear).attached_to, unit);
    auto& u = state.getObject(unit);
    EXPECT_EQ(u.attachment_might_bonus, 0) << "Svellsongur grants no might bonus";
    EXPECT_EQ(std::count(u.attachments.begin(), u.attachments.end(), gear), 1);
}

// Equip fails (no Calm-domain rune available) and does NOT attach.
TEST_F(AuditFix5Test, Svellsongur_EquipFailsNoDomainRune) {
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 4);
    auto gear = addGear(P1, kSvellsongur, 0);
    // Wrong-domain runes: ready but not Calm.
    addRune(P1, Domain::Fury);
    addRune(P1, Domain::Fury);

    Card* card = card_registry.get(kSvellsongur);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = card->onEquip(ctx, unit);

    EXPECT_FALSE(ok);
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
    EXPECT_TRUE(state.getObject(unit).attachments.empty());
}

// Equip fails when not enough ready runes for the energy cost.
TEST_F(AuditFix5Test, Svellsongur_EquipFailsInsufficientEnergy) {
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 4);
    auto gear = addGear(P1, kSvellsongur, 0);
    // A single exhausted Calm rune: domain present but 0 ready -> energy fail.
    addRune(P1, Domain::Calm, /*exhausted=*/true);

    Card* card = card_registry.get(kSvellsongur);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = card->onEquip(ctx, unit);

    EXPECT_FALSE(ok);
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
}

// ─── [460] Edge of Night ─────────────────────────────────────────────────────

// Equip ability: [C] cost (Chaos power, 0 energy) attaches and grants +2 might.
TEST_F(AuditFix5Test, EdgeOfNight_EquipGrantsMight) {
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 3);
    auto gear = addGear(P1, kEdgeOfNight, /*might_bonus=*/2);
    addRune(P1, Domain::Chaos);  // Chaos power for [C]

    Card* card = card_registry.get(kEdgeOfNight);
    ASSERT_NE(card, nullptr);
    CardContext ctx{state, events, exec, P1, gear};
    bool ok = card->onEquip(ctx, unit);

    EXPECT_TRUE(ok);
    EXPECT_EQ(state.getObject(gear).attached_to, unit);
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, 2);
}

// Equip fails without a Chaos rune.
TEST_F(AuditFix5Test, EdgeOfNight_EquipFailsNoChaos) {
    EffectExecutor exec(state, events, card_db);
    auto unit = addUnit(P1, kInvalidId, 3);
    auto gear = addGear(P1, kEdgeOfNight, 2);
    addRune(P1, Domain::Calm);

    Card* card = card_registry.get(kEdgeOfNight);
    CardContext ctx{state, events, exec, P1, gear};
    EXPECT_FALSE(card->onEquip(ctx, unit));
    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
}

// Play-from-facedown: WhenYouPlayThis on an unattached gear sitting at a BF
// auto-attaches it to a friendly unit at the same battlefield.
TEST_F(AuditFix5Test, EdgeOfNight_PlayFromFacedownAutoAttaches) {
    EffectExecutor exec(state, events, card_db);
    BattlefieldId bf = 0;
    auto unit = addUnit(P1, kInvalidId, 3, /*at_bf=*/static_cast<int>(bf));
    // Gear unattached, at the same battlefield as the friendly unit.
    auto gear = addGear(P1, kEdgeOfNight, 2);
    {
        auto& g = state.getObject(gear);
        g.zone = ZoneType::BattlefieldZone;
        g.location = BattlefieldLocation{bf};
    }

    fireTriggerAs(kEdgeOfNight, P1, gear, TriggerType::WhenYouPlayThis, exec);

    EXPECT_EQ(state.getObject(gear).attached_to, unit)
        << "facedown play attaches to friendly unit 'here'";
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, 2);
}

// Guard: an ALREADY-ATTACHED gear (the normal Equip-play case) makes the
// WhenYouPlayThis trigger a no-op (does not re-attach or double-buff).
TEST_F(AuditFix5Test, EdgeOfNight_AlreadyAttachedTriggerIsNoop) {
    EffectExecutor exec(state, events, card_db);
    BattlefieldId bf = 0;
    auto unit = addUnit(P1, kInvalidId, 3, static_cast<int>(bf));
    auto other = addUnit(P1, kInvalidId, 3, static_cast<int>(bf));
    auto gear = addGear(P1, kEdgeOfNight, 2);
    {
        auto& g = state.getObject(gear);
        g.zone = ZoneType::BattlefieldZone;
        g.location = BattlefieldLocation{bf};
        g.attached_to = unit;  // pretend already attached
    }
    state.getObject(unit).attachments.push_back(gear);
    state.getObject(unit).attachment_might_bonus = 2;

    fireTriggerAs(kEdgeOfNight, P1, gear, TriggerType::WhenYouPlayThis, exec);

    EXPECT_EQ(state.getObject(gear).attached_to, unit) << "stays on original unit";
    EXPECT_TRUE(state.getObject(other).attachments.empty())
        << "should not attach to the other unit";
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, 2)
        << "no double-buff";
}

// Guard: gear at base (not at a battlefield) -> trigger is a no-op.
TEST_F(AuditFix5Test, EdgeOfNight_AtBaseTriggerIsNoop) {
    EffectExecutor exec(state, events, card_db);
    addUnit(P1, kInvalidId, 3);  // friendly unit at base
    auto gear = addGear(P1, kEdgeOfNight, 2);  // gear at base, unattached

    fireTriggerAs(kEdgeOfNight, P1, gear, TriggerType::WhenYouPlayThis, exec);

    EXPECT_FALSE(state.getObject(gear).attached_to.has_value())
        << "gear not at a battlefield -> no auto-attach";
}

// ─── [506] Fire Below the Mountain ───────────────────────────────────────────

// Activated ability metadata: exhaust cost + reaction timing.
TEST_F(AuditFix5Test, FireBelowMtn_AbilityMetadata) {
    Card* card = card_registry.get(kFireBelowMtn);
    ASSERT_NE(card, nullptr);
    EXPECT_TRUE(card->hasActivatedAbility());
    EXPECT_TRUE(card->isReactionAbility());
    EXPECT_TRUE(card->getActivationCost().exhaust);
}

// onActivate adds one floating [A] (universal power) to the controller's pool.
TEST_F(AuditFix5Test, FireBelowMtn_ActivateAddsUniversalPower) {
    EffectExecutor exec(state, events, card_db);
    auto src = addUnit(P1, kInvalidId, 1);  // stand-in source for the legend
    int before = state.player(P1).rune_pool.universal_power;

    Card* card = card_registry.get(kFireBelowMtn);
    CardContext ctx{state, events, exec, P1, src};
    card->onActivate(ctx, {});

    EXPECT_EQ(state.player(P1).rune_pool.universal_power, before + 1);
    // NOTE: the "use only to play gear / use gear abilities" earmark is NOT
    // engine-enforced (documented limitation) — the [A] is spendable anywhere.
}

// ─── [544] Yone, Blademaster ─────────────────────────────────────────────────

// WhenIConquer: deal damage equal to my Might to an enemy unit in a base.
// Lethal damage kills the target.
TEST_F(AuditFix5Test, Yone_ConquerDealsMightDamageAndKills) {
    EffectExecutor exec(state, events, card_db);
    auto yone = addUnit(P1, kYone, /*might=*/5);  // current_might == 5
    auto enemy = addUnit(P2, kInvalidId, 4);      // at P2's base, 4 might

    fireTriggerAs(kYone, P1, yone, TriggerType::WhenIConquer, exec);

    // killObject routes the unit to Trash (it stays in the objects map), so
    // assert the zone rather than objectExists.
    EXPECT_EQ(state.getObject(enemy).zone, ZoneType::Trash)
        << "5 damage to a 4-might enemy is lethal -> killed (to trash)";
}

// WhenIConquer with a tougher enemy: damaged but survives (no kill).
TEST_F(AuditFix5Test, Yone_ConquerDamagesNonLethal) {
    EffectExecutor exec(state, events, card_db);
    auto yone = addUnit(P1, kYone, /*might=*/5);
    auto enemy = addUnit(P2, kInvalidId, 9);

    fireTriggerAs(kYone, P1, yone, TriggerType::WhenIConquer, exec);

    ASSERT_TRUE(state.objectExists(enemy));
    EXPECT_EQ(state.getObject(enemy).damage_marked, 5);
}

// WhenIConquer with no enemy unit in a base -> no-op.
TEST_F(AuditFix5Test, Yone_ConquerNoEnemyNoop) {
    EffectExecutor exec(state, events, card_db);
    auto yone = addUnit(P1, kYone, 5);
    // Only an enemy unit at a battlefield (not in a base).
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);

    fireTriggerAs(kYone, P1, yone, TriggerType::WhenIConquer, exec);

    ASSERT_TRUE(state.objectExists(enemy));
    EXPECT_EQ(state.getObject(enemy).damage_marked, 0)
        << "only enemy units in a base are targeted";
}

// WhenYouPlayMe (Weaponmaster): free-equip a friendly Equipment gear onto Yone.
TEST_F(AuditFix5Test, Yone_WeaponmasterFreeEquip) {
    EffectExecutor exec(state, events, card_db);
    auto yone = addUnit(P1, kYone, 5);
    auto gear = addGear(P1, kInvalidId, /*might_bonus=*/3, /*equipment_tag=*/true);

    fireTriggerAs(kYone, P1, yone, TriggerType::WhenYouPlayMe, exec);

    EXPECT_EQ(state.getObject(gear).attached_to, yone);
    EXPECT_EQ(state.getObject(yone).attachment_might_bonus, 3);
}

// Weaponmaster ignores non-Equipment gear (nothing to equip).
TEST_F(AuditFix5Test, Yone_WeaponmasterNoEquipmentNoop) {
    EffectExecutor exec(state, events, card_db);
    auto yone = addUnit(P1, kYone, 5);
    auto gear = addGear(P1, kInvalidId, 3, /*equipment_tag=*/false);

    fireTriggerAs(kYone, P1, yone, TriggerType::WhenYouPlayMe, exec);

    EXPECT_FALSE(state.getObject(gear).attached_to.has_value());
    EXPECT_EQ(state.getObject(yone).attachment_might_bonus, 0);
}

// ─── [613] Ivern, Nurturer ───────────────────────────────────────────────────

// WhenYouPlayMe, default yes branch: draft the first unit from top 3 to hand,
// recycle the rest to the bottom of the deck.
TEST_F(AuditFix5Test, IvernNurturer_DraftsUnitOnPlay) {
    EffectExecutor exec(state, events, card_db);
    auto ivern = addUnit(P1, kIvernNurturer, 4);
    // Build a deck; top (back) three matter. Use real unit id 544 (Yone) as
    // the unit to draft and non-unit-ish ids around it.
    auto bottom = addToDeck(P1, kInvalidId);  // stays in deck
    auto rest_a = addToDeck(P1, kMeditation); // spell (non-unit) -> recycled
    auto unit_card = addToDeck(P1, kYone);    // unit -> drafted (this is back/top)

    int hand_before = handSize(P1);
    driveResumableTrigger(kIvernNurturer, P1, ivern, &AuditFix5Test::pickYes, exec,
                          TriggerType::WhenYouPlayMe);

    EXPECT_TRUE(inHand(P1, unit_card)) << "the unit should be drafted to hand";
    EXPECT_EQ(handSize(P1), hand_before + 1);
    // The non-unit peeked card was recycled (still in deck, not hand).
    EXPECT_FALSE(inHand(P1, rest_a));
    EXPECT_TRUE(inDeck(P1, rest_a));
    // The 4th card (never peeked) is still in the deck.
    EXPECT_TRUE(inDeck(P1, bottom));
}

// Decline branch: reveal nothing, all 3 recycled, hand unchanged.
TEST_F(AuditFix5Test, IvernNurturer_DeclineRecyclesAll) {
    EffectExecutor exec(state, events, card_db);
    auto ivern = addUnit(P1, kIvernNurturer, 4);
    auto a = addToDeck(P1, kMeditation);
    auto b = addToDeck(P1, kYone);
    auto c = addToDeck(P1, kYone);

    int hand_before = handSize(P1);
    int deck_before = deckSize(P1);
    driveResumableTrigger(kIvernNurturer, P1, ivern, &AuditFix5Test::pickNo, exec,
                          TriggerType::WhenIHold);

    EXPECT_EQ(handSize(P1), hand_before) << "declined -> nothing drafted";
    EXPECT_EQ(deckSize(P1), deck_before) << "all peeked cards recycled back";
    EXPECT_FALSE(inHand(P1, a));
    EXPECT_FALSE(inHand(P1, b));
    EXPECT_FALSE(inHand(P1, c));
}

// Themed draft: drafting a unit with a Bird/Cat/Dog/Poro tag buffs a friendly
// unit (+1 buff_count). Rengar (682) has the "Cat" tag.
TEST_F(AuditFix5Test, IvernNurturer_ThemedDraftBuffs) {
    EffectExecutor exec(state, events, card_db);
    auto ivern = addUnit(P1, kIvernNurturer, 4);  // friendly unit to receive buff
    addToDeck(P1, kRengar);  // top: a Cat-tagged unit -> themed

    int buff_before = state.getObject(ivern).buff_count;
    driveResumableTrigger(kIvernNurturer, P1, ivern, &AuditFix5Test::pickYes, exec,
                          TriggerType::WhenYouPlayMe);

    EXPECT_EQ(state.getObject(ivern).buff_count, buff_before + 1)
        << "themed (Cat) draft buffs a friendly unit";
}

// Empty deck -> no-op (no crash, nothing drafted).
TEST_F(AuditFix5Test, IvernNurturer_EmptyDeckNoop) {
    EffectExecutor exec(state, events, card_db);
    auto ivern = addUnit(P1, kIvernNurturer, 4);

    int hand_before = handSize(P1);
    driveResumableTrigger(kIvernNurturer, P1, ivern, &AuditFix5Test::pickYes, exec,
                          TriggerType::WhenYouPlayMe);

    EXPECT_EQ(handSize(P1), hand_before);
}

// ─── [644] Lillia, Fae Fawn ──────────────────────────────────────────────────

// WhenIMove: creates a 3-Might Sprite token with [Temporary] at the unit's
// current (post-move) location.
TEST_F(AuditFix5Test, Lillia_MoveCreatesSpriteToken) {
    EffectExecutor exec(state, events, card_db);
    BattlefieldId bf = 1;
    auto lillia = addUnit(P1, kLillia, 3, /*at_bf=*/static_cast<int>(bf));

    fireTriggerAs(kLillia, P1, lillia, TriggerType::WhenIMove, exec);

    auto token = findTokenByName(P1, "Sprite");
    ASSERT_NE(token, kInvalidId) << "a Sprite token should be created";
    auto& t = state.getObject(token);
    EXPECT_EQ(t.base_might, 3);
    EXPECT_TRUE(t.hasKeyword(Keyword::Temporary));
    // Token spawns "there" — same battlefield Lillia is now at.
    ASSERT_TRUE(t.location.has_value());
    EXPECT_TRUE(std::holds_alternative<BattlefieldLocation>(*t.location));
    EXPECT_EQ(std::get<BattlefieldLocation>(*t.location).id, bf);
}

// Fallback: if the source has no location, the token spawns at the
// controller's base.
TEST_F(AuditFix5Test, Lillia_NoLocationFallsBackToBase) {
    EffectExecutor exec(state, events, card_db);
    auto lillia = addUnit(P1, kLillia, 3);  // at base
    state.getObject(lillia).location.reset();

    fireTriggerAs(kLillia, P1, lillia, TriggerType::WhenIMove, exec);

    auto token = findTokenByName(P1, "Sprite");
    ASSERT_NE(token, kInvalidId);
    auto& t = state.getObject(token);
    ASSERT_TRUE(t.location.has_value());
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*t.location));
}

// ─── [682] Rengar, Trophy Hunter ─────────────────────────────────────────────

// The Ambush-to-enemy-BF extension is an unimplemented engine gap. Verify the
// card resolves to a defined Unit identity (Cat-tagged champion).
// TODO: Ambush-to-enemy-only-battlefield extension is NOT implemented
// (requires a new engine hook in generateClosedStateActions). No behavior to
// assert beyond identity.
TEST_F(AuditFix5Test, Rengar_RegisteredIdentity) {
    Card* card = card_registry.get(kRengar);
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->cardDefId(), kRengar);
    const auto& def = card_db.get(kRengar);
    EXPECT_EQ(def.card_type, CardType::Unit);
    // Cat tag is present in the registry def.
    EXPECT_NE(std::find(def.tags.begin(), def.tags.end(), "Cat"), def.tags.end());
}

// ─── [739] Ivern, Friend to All ──────────────────────────────────────────────

// AsYouPlayMe default (non-chain) path: pickMode falls back to mode 0 = "Bird";
// the unit gains the Bird tag.
TEST_F(AuditFix5Test, IvernFriend_DefaultTagBird) {
    EffectExecutor exec(state, events, card_db);
    auto ivern = addUnit(P1, kIvernFriend, 6);

    // Non-chain fireTriggerAs -> pickMode returns legal.front() == Bird.
    fireTriggerAs(kIvernFriend, P1, ivern, TriggerType::AsYouPlayMe, exec);

    auto& tags = state.getObject(ivern).tags;
    EXPECT_NE(std::find(tags.begin(), tags.end(), "Bird"), tags.end())
        << "default tag choice is Bird";
}

// AsYouPlayMe chain path: agent picks a non-default mode (Dog) via pickMode.
TEST_F(AuditFix5Test, IvernFriend_AgentChoosesTag) {
    EffectExecutor exec(state, events, card_db);
    auto ivern = addUnit(P1, kIvernFriend, 6);

    // pickMode publishes [Bird, Cat, Dog, Poro] with chosen_value 0..3.
    // Pick the "Dog" intent (chosen_value == 2).
    auto pickDog = [](const std::vector<Intent>& legal) -> Intent {
        for (const auto& i : legal) {
            if (i.chosen_value.has_value() && *i.chosen_value == 2) return i;
        }
        return legal.empty() ? Intent{} : legal.front();
    };
    driveResumableTrigger(kIvernFriend, P1, ivern, pickDog, exec,
                          TriggerType::AsYouPlayMe);

    auto& tags = state.getObject(ivern).tags;
    EXPECT_NE(std::find(tags.begin(), tags.end(), "Dog"), tags.end())
        << "agent-chosen tag is Dog";
    EXPECT_EQ(std::find(tags.begin(), tags.end(), "Bird"), tags.end())
        << "Bird not added when agent picks Dog";
}

// WhenIConquerOrHold: score 1 only when friendly units collectively cover all
// four tags (Bird, Cat, Dog, Poro).
TEST_F(AuditFix5Test, IvernFriend_ScoresWhenAllFourTags) {
    EffectExecutor exec(state, events, card_db);
    auto ivern = addUnit(P1, kIvernFriend, 6);
    // Spread the four tags across friendly units.
    state.getObject(ivern).tags.push_back("Bird");
    auto u2 = addUnit(P1, kInvalidId, 1);
    state.getObject(u2).tags.push_back("Cat");
    auto u3 = addUnit(P1, kInvalidId, 1);
    state.getObject(u3).tags.push_back("Dog");
    auto u4 = addUnit(P1, kInvalidId, 1);
    state.getObject(u4).tags.push_back("Poro");

    setScore(P1, 0);
    fireTriggerAs(kIvernFriend, P1, ivern, TriggerType::WhenIConquerOrHold, exec);

    EXPECT_EQ(state.player(P1).score, 1) << "all four tags present -> score 1";
}

// WhenIConquerOrHold: missing a tag -> no score.
TEST_F(AuditFix5Test, IvernFriend_NoScoreMissingTag) {
    EffectExecutor exec(state, events, card_db);
    auto ivern = addUnit(P1, kIvernFriend, 6);
    state.getObject(ivern).tags.push_back("Bird");
    auto u2 = addUnit(P1, kInvalidId, 1);
    state.getObject(u2).tags.push_back("Cat");
    auto u3 = addUnit(P1, kInvalidId, 1);
    state.getObject(u3).tags.push_back("Dog");
    // No Poro.

    setScore(P1, 3);
    fireTriggerAs(kIvernFriend, P1, ivern, TriggerType::WhenIConquerOrHold, exec);

    EXPECT_EQ(state.player(P1).score, 3) << "missing Poro -> no score";
}

// Enemy tags don't count toward the union (friendly-only scan).
TEST_F(AuditFix5Test, IvernFriend_EnemyTagsDontCount) {
    EffectExecutor exec(state, events, card_db);
    auto ivern = addUnit(P1, kIvernFriend, 6);
    state.getObject(ivern).tags.push_back("Bird");
    auto f2 = addUnit(P1, kInvalidId, 1);
    state.getObject(f2).tags.push_back("Cat");
    // Enemy units cover Dog + Poro — must NOT count.
    auto e1 = addUnit(P2, kInvalidId, 1);
    state.getObject(e1).tags.push_back("Dog");
    auto e2 = addUnit(P2, kInvalidId, 1);
    state.getObject(e2).tags.push_back("Poro");

    setScore(P1, 0);
    fireTriggerAs(kIvernFriend, P1, ivern, TriggerType::WhenIConquerOrHold, exec);

    EXPECT_EQ(state.player(P1).score, 0)
        << "enemy-controlled tags must not satisfy the union";
}

// ─── [769] Gardens of Becoming ───────────────────────────────────────────────

// The granted "[E]: Gain 1 XP" ability is an unimplemented engine gap (battle-
// field auras cannot inject activated abilities onto units). Verify identity.
// TODO: granted-activated-ability aura is NOT implemented (needs a new engine
// hook). No behavior to assert beyond identity.
TEST_F(AuditFix5Test, Gardens_RegisteredIdentity) {
    Card* card = card_registry.get(kGardens);
    ASSERT_NE(card, nullptr);
    EXPECT_EQ(card->cardDefId(), kGardens);
    EXPECT_EQ(card_db.get(kGardens).card_type, CardType::Battlefield);
}

} // namespace
