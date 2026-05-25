/// @file test_counter_spells.cpp
/// Per-card unit tests for the seven counter spells:
///   45  Defy            (counter + draw 1 → always playable)
///   64  Wind Wall       (pure counter)
///   368 Not So Fast     (counter spell/ability targeting friendly unit/gear)
///   457 Hard Bargain    (counter unless controller pays 2)
///   668 Repulse         (counter spell/ability targeting friendly unit)
///   693 Abandon         (counter to hand + predict 1)
///   750 Lilting Lullaby (counter + opp can't play spells this turn)
///
/// Each card covers four axes:
///   1. Legality — when does hasLegalTargets() return true/false?
///   2. Targeting — what target shapes does it accept?
///   3. Resolution — what does onResolve do to game state?
///   4. Chain interaction — does it pop / preserve / re-route correctly?
///
/// Tests use the CardTestFixture from card_test_fixture.h.

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kDefy        = 45;
constexpr CardDefId kWindWall    = 64;
constexpr CardDefId kNotSoFast   = 368;
constexpr CardDefId kHardBargain = 457;
constexpr CardDefId kRepulse     = 668;
constexpr CardDefId kAbandon     = 693;
constexpr CardDefId kLullaby     = 750;
constexpr CardDefId kLunarBoon   = 687;  // arbitrary "spell on chain"

// ═══════════════════════════════════════════════════════════════════════════
// Wind Wall (64) — pure counter
// ═══════════════════════════════════════════════════════════════════════════

class WindWallTest : public CardTestFixture {};

TEST_F(WindWallTest, NotPlayableWhenChainEmpty) {
    Card* c = card_registry.get(kWindWall);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
}

TEST_F(WindWallTest, PlayableWhenSpellOnChain) {
    pushSpellOnChain(P2, kLunarBoon);
    Card* c = card_registry.get(kWindWall);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

TEST_F(WindWallTest, CountersTopSpellToTrash) {
    auto target = pushSpellOnChain(P2, kLunarBoon);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kWindWall, P1, {}, exec);

    EXPECT_TRUE(state.chain.items.empty()) << "target spell should be popped";
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Trash);
    EXPECT_TRUE(inTrash(P2, target));
}

TEST_F(WindWallTest, NoOpsWhenChainEmpty) {
    auto src = state.createObject();
    EffectExecutor exec(state, events, card_db);
    EXPECT_NO_THROW(invokeOnResolve(src, kWindWall, P1, {}, exec));
    EXPECT_TRUE(state.chain.items.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// Hard Bargain (457) — counter unless controller pays 2
// ═══════════════════════════════════════════════════════════════════════════

class HardBargainTest : public CardTestFixture {};

TEST_F(HardBargainTest, NotPlayableWhenChainEmpty) {
    Card* c = card_registry.get(kHardBargain);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
}

TEST_F(HardBargainTest, PlayableWhenSpellOnChain) {
    pushSpellOnChain(P2, kLunarBoon);
    Card* c = card_registry.get(kHardBargain);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

TEST_F(HardBargainTest, NotPlayableWhenChainTopIsAbilityNotSpell) {
    // Hard Bargain requires a SPELL on the chain. Activated abilities go
    // on the chain too but with is_spell=false — Hard Bargain shouldn't
    // be offered against them.
    ChainItem ability_item;
    ability_item.id = state.chain.allocateId();
    ability_item.controller = P2;
    ability_item.is_spell = false;
    ability_item.is_activated_ability = true;
    state.chain.items.push_back(ability_item);

    Card* c = card_registry.get(kHardBargain);
    EXPECT_FALSE(c->hasLegalTargets(state, P1))
        << "Hard Bargain text says 'Counter a spell' — abilities don't qualify";
}

TEST_F(HardBargainTest, AutoCountersWhenTargetCantAfford) {
    // Target controller has only 1 ready rune — can't afford 2E rescue.
    auto target = pushSpellOnChain(P2, kLunarBoon);
    addRune(P2, Domain::Chaos);  // only 1 ready

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    // No agent invocation needed — case 0 auto-counters and returns.
    driveResumable(kHardBargain, P1, src,
        [](const std::vector<Intent>& legal) { return legal.front(); }, exec);

    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Trash);
}

TEST_F(HardBargainTest, CountersWhenTargetDeclinesToPay) {
    auto target = pushSpellOnChain(P2, kLunarBoon);
    auto r1 = addRune(P2, Domain::Chaos);
    auto r2 = addRune(P2, Domain::Chaos);
    (void)r1; (void)r2;

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    // Pick the "decline" Intent (chosen_objects empty).
    driveResumable(kHardBargain, P1, src,
        [](const std::vector<Intent>& legal) {
            for (auto& i : legal)
                if (i.chosen_objects.empty()) return i;
            return legal.front();
        }, exec);

    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Trash);
}

TEST_F(HardBargainTest, SavesSpellWhenTargetPays) {
    auto target = pushSpellOnChain(P2, kLunarBoon);
    auto r1 = addRune(P2, Domain::Chaos);
    auto r2 = addRune(P2, Domain::Chaos);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    // Pick the "pay" Intent (chosen_objects = [target]).
    driveResumable(kHardBargain, P1, src,
        [target](const std::vector<Intent>& legal) {
            for (auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == target)
                    return i;
            return legal.front();
        }, exec);

    // Spell stays on chain — Hard Bargain didn't pop it.
    EXPECT_EQ(state.chain.items.size(), 1u) << "spell should be saved";
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Chain);
    // Both runes were exhausted.
    EXPECT_TRUE(state.getObject(r1).is_exhausted);
    EXPECT_TRUE(state.getObject(r2).is_exhausted);
}

// ═══════════════════════════════════════════════════════════════════════════
// Repulse (668) — counter enemy spell targeting friendly UNIT
// ═══════════════════════════════════════════════════════════════════════════

class RepulseTest : public CardTestFixture {};

TEST_F(RepulseTest, NotPlayableWhenChainEmpty) {
    Card* c = card_registry.get(kRepulse);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
}

TEST_F(RepulseTest, NotPlayableWhenChainTargetsNothing) {
    pushSpellOnChain(P2, kLunarBoon, /*targets=*/{});
    Card* c = card_registry.get(kRepulse);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
}

TEST_F(RepulseTest, NotPlayableWhenChainTargetsEnemy) {
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    pushSpellOnChain(P2, kLunarBoon, {enemy});
    Card* c = card_registry.get(kRepulse);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
}

TEST_F(RepulseTest, NotPlayableWhenChainOwnerIsSelf) {
    // A friendly spell targeting friendly unit — Repulse can't counter own
    // spells.
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    pushSpellOnChain(P1, kLunarBoon, {unit});
    Card* c = card_registry.get(kRepulse);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
}

TEST_F(RepulseTest, PlayableWhenEnemySpellTargetsFriendlyUnit) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    pushSpellOnChain(P2, kLunarBoon, {unit});
    Card* c = card_registry.get(kRepulse);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

TEST_F(RepulseTest, CountersOnResolve) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto target = pushSpellOnChain(P2, kLunarBoon, {unit});
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kRepulse, P1, {}, exec);

    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Trash);
}

// ═══════════════════════════════════════════════════════════════════════════
// Not So Fast (368) — same as Repulse but also covers GEAR
// ═══════════════════════════════════════════════════════════════════════════

class NotSoFastTest : public CardTestFixture {};

TEST_F(NotSoFastTest, NotPlayableWhenChainEmpty) {
    Card* c = card_registry.get(kNotSoFast);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
}

TEST_F(NotSoFastTest, PlayableForFriendlyUnit) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    pushSpellOnChain(P2, kLunarBoon, {unit});
    Card* c = card_registry.get(kNotSoFast);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

TEST_F(NotSoFastTest, PlayableForFriendlyGear) {
    // Synthesize a friendly gear on board.
    auto gid = state.createObject();
    auto& g = state.getObject(gid);
    g.owner = P1;
    g.controller = P1;
    g.card_type = CardType::Gear;
    g.zone = ZoneType::BattlefieldZone;
    g.location = BattlefieldLocation{0};
    pushSpellOnChain(P2, kLunarBoon, {gid});
    Card* c = card_registry.get(kNotSoFast);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

TEST_F(NotSoFastTest, NotPlayableForFriendlySpellOnChain) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    pushSpellOnChain(P1, kLunarBoon, {unit});
    Card* c = card_registry.get(kNotSoFast);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
}

// ═══════════════════════════════════════════════════════════════════════════
// Lilting Lullaby (750) — counter + lockout on countered spell's controller
// ═══════════════════════════════════════════════════════════════════════════

class LullabyTest : public CardTestFixture {};

TEST_F(LullabyTest, NotPlayableWhenChainEmpty) {
    Card* c = card_registry.get(kLullaby);
    EXPECT_FALSE(c->hasLegalTargets(state, P1));
}

TEST_F(LullabyTest, PlayableWhenSpellOnChain) {
    pushSpellOnChain(P2, kLunarBoon);
    Card* c = card_registry.get(kLullaby);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

TEST_F(LullabyTest, CountersAndAppliesLockoutToTargetController) {
    auto target = pushSpellOnChain(P2, kLunarBoon);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EXPECT_FALSE(state.player(P2).cant_play_spells_this_turn);

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kLullaby, P1, {}, exec);

    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Trash);
    EXPECT_TRUE(state.player(P2).cant_play_spells_this_turn)
        << "lockout should land on countered spell's controller (P2)";
    EXPECT_FALSE(state.player(P1).cant_play_spells_this_turn);
}

// ═══════════════════════════════════════════════════════════════════════════
// Defy (45) — counter + draw 1
// Per CR 355.9.a.2 + 355.10: "Counter a spell" requires a chain target;
// the draw-1 side effect doesn't satisfy that requirement.
// ═══════════════════════════════════════════════════════════════════════════

class DefyTest : public CardTestFixture {};

TEST_F(DefyTest, NotPlayableWhenChainEmpty) {
    Card* c = card_registry.get(kDefy);
    EXPECT_FALSE(c->hasLegalTargets(state, P1))
        << "Per CR, 'Counter a spell' is a target requirement; "
           "draw-1 side effect doesn't make Defy playable on its own.";
}

TEST_F(DefyTest, PlayableWhenSpellOnChain) {
    pushSpellOnChain(P2, kLunarBoon);
    Card* c = card_registry.get(kDefy);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

TEST_F(DefyTest, Counters) {
    // Defy's text is only "Counter a spell that costs no more than [4]".
    // The previously-asserted "+1 draw" was an un-printed bug, now removed.
    auto target = pushSpellOnChain(P2, kLunarBoon);  // cost 3 <= 4: counterable
    auto deck_card = addToDeck(P1, kInvalidId);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    int initial_hand = handSize(P1);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kDefy, P1, {}, exec);

    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Trash);
    EXPECT_EQ(handSize(P1), initial_hand) << "Defy no longer draws";
    EXPECT_FALSE(inHand(P1, deck_card)) << "no card drawn";
}

TEST_F(DefyTest, DoesNothingHarmfulWithEmptyChain) {
    // With no spell on the chain there is nothing to counter, and Defy no
    // longer has a draw side effect — resolving it should be a harmless no-op.
    auto deck_card = addToDeck(P1, kInvalidId);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    int initial_hand = handSize(P1);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kDefy, P1, {}, exec);

    EXPECT_EQ(handSize(P1), initial_hand) << "no draw on empty chain";
    EXPECT_FALSE(inHand(P1, deck_card));
    EXPECT_TRUE(state.chain.items.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// Abandon (693) — counter to hand + predict 1
// Per CR 355.9.a.2 + 355.10: "Counter a spell" requires a chain target;
// the predict-1 side effect doesn't satisfy that requirement.
// ═══════════════════════════════════════════════════════════════════════════

class AbandonTest : public CardTestFixture {};

TEST_F(AbandonTest, NotPlayableWhenChainEmpty) {
    Card* c = card_registry.get(kAbandon);
    EXPECT_FALSE(c->hasLegalTargets(state, P1))
        << "Per CR, 'Counter a spell' is a target requirement; "
           "predict-1 side effect doesn't make Abandon playable on its own.";
}

TEST_F(AbandonTest, PlayableWhenSpellOnChain) {
    pushSpellOnChain(P2, kLunarBoon);
    Card* c = card_registry.get(kAbandon);
    EXPECT_TRUE(c->hasLegalTargets(state, P1));
}

TEST_F(AbandonTest, ReturnsCounteredSpellToHand) {
    auto target = pushSpellOnChain(P2, kLunarBoon);
    auto deck_top = addToDeck(P1, kInvalidId);  // for predict
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    // Predict picker: pick "keep" (chosen_objects empty).
    driveResumable(kAbandon, P1, src,
        [](const std::vector<Intent>& legal) {
            for (auto& i : legal)
                if (i.chosen_objects.empty()) return i;
            return legal.front();
        }, exec);

    EXPECT_TRUE(state.chain.items.empty());
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Hand);
    EXPECT_TRUE(inHand(P2, target)) << "Abandon returns to OWNER's hand";
    (void)deck_top;
}

}  // namespace
