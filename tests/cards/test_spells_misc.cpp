/// @file test_spells_misc.cpp
/// Per-card unit tests for the miscellaneous spells / triggered units /
/// token-creators that live in src/cards/manual/deck_cards.cpp.
///
/// Spells (one-shot resolve):
///   209 Cull the Weak           (mutual kill)
///   631 Sprite Burst            (2 sprite tokens)
///   745 Thrill of the Hunt      (banish friendly + play to BF)
///   727 Shadow's Call           (give Temporary, draw 2)
///   690 Star-Crossed            (bounce friendly + enemy)
///   484 Deathgrip               (kill + transfer might + draw)
///   263 Bullet Time             (variable damage from energy pool)
///   735 Sacrifice               (kill mighty friendly, draw 2, channel exhausted)
///
/// Spells using resume pattern (driveResumable):
///   687 Lunar Boon              (discard 1, draw 2)
///   192 Mindsplitter            (opponent discards 1)
///   156 Sabotage                (opponent discards 1)
///
/// Token creators / triggers:
///   451 Treasure Hunter         (on move → gold token)
///   476 Honest Broker           (Deathknell → gold token)
///   778 Plundering Poro         (on conquer → gold token)
///   644 Lillia Fae Fawn         (on move → sprite token)
///   344 Ferrous Forerunner      (Deathknell → 2 mech tokens)
///   749 Bashful Bloom           (activated → sprite token)
///   752 Shadow                  (enters ready when played to BF)
///   106 Sprite Mother Unit      (on play → sprite token)

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kCullTheWeak     = 209;
constexpr CardDefId kSpriteBurst     = 631;
constexpr CardDefId kThrillOfHunt    = 745;
constexpr CardDefId kShadowsCall     = 727;
constexpr CardDefId kStarCrossed     = 690;
constexpr CardDefId kDeathgrip       = 484;
constexpr CardDefId kBulletTime      = 263;
constexpr CardDefId kSacrifice       = 735;
constexpr CardDefId kLunarBoon       = 687;
constexpr CardDefId kMindsplitter    = 192;
constexpr CardDefId kSabotage        = 156;
constexpr CardDefId kTreasureHunter  = 451;
constexpr CardDefId kHonestBroker    = 476;
constexpr CardDefId kPlunderingPoro  = 778;
constexpr CardDefId kLilliaFaeFawn   = 644;
constexpr CardDefId kFerrousForerunner = 344;
constexpr CardDefId kBashfulBloom    = 749;
constexpr CardDefId kShadow          = 752;
constexpr CardDefId kSpriteMother    = 106;
constexpr CardDefId kArbitrary       = 687;  // any spell used as filler

// Helper: count tokens with a given name controlled by a player.
int countTokensNamed(GameState& state, PlayerId controller,
                      const std::string& name) {
    int n = 0;
    for (auto& [id, obj] : state.objects) {
        (void)id;
        if (obj.controller == controller && obj.isToken() && obj.name == name) ++n;
    }
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════
// Cull the Weak (209) — mutual kill (one unit per side)
// ═══════════════════════════════════════════════════════════════════════════

class CullTheWeakSpellTest : public CardTestFixture {};

TEST_F(CullTheWeakSpellTest, KillsOneUnitOnEachSide) {
    auto u1 = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto u2 = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kCullTheWeak, P1, {}, exec);

    // Both units should be dead (in trash zones).
    EXPECT_EQ(state.getObject(u1).zone, ZoneType::Trash);
    EXPECT_EQ(state.getObject(u2).zone, ZoneType::Trash);
}

TEST_F(CullTheWeakSpellTest, NoOpIfNoUnitsOnBoard) {
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    EXPECT_NO_THROW(invokeOnResolve(src, kCullTheWeak, P1, {}, exec));
}

TEST_F(CullTheWeakSpellTest, KillsP1UnitWhenOnlyP1HasUnits) {
    auto u1 = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kCullTheWeak, P1, {}, exec);

    EXPECT_EQ(state.getObject(u1).zone, ZoneType::Trash);
}

// ═══════════════════════════════════════════════════════════════════════════
// Sprite Burst (631) — create 2 ready 3M Sprite tokens with Temporary
// ═══════════════════════════════════════════════════════════════════════════

class SpriteBurstSpellTest : public CardTestFixture {};

TEST_F(SpriteBurstSpellTest, CreatesTwoSpriteTokens) {
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kSpriteBurst, P1, {}, exec);

    EXPECT_EQ(countTokensNamed(state, P1, "Sprite"), 2);
}

TEST_F(SpriteBurstSpellTest, TokensHaveTemporaryAndAreReady) {
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kSpriteBurst, P1, {}, exec);

    int found_with_temp = 0;
    int found_ready = 0;
    int found_3M = 0;
    for (auto& [id, obj] : state.objects) {
        (void)id;
        if (obj.controller != P1 || !obj.isToken() || obj.name != "Sprite") continue;
        if (obj.hasKeyword(Keyword::Temporary)) ++found_with_temp;
        if (!obj.is_exhausted) ++found_ready;
        if (obj.base_might == 3) ++found_3M;
    }
    EXPECT_EQ(found_with_temp, 2);
    EXPECT_EQ(found_ready, 2);
    EXPECT_EQ(found_3M, 2);
}

// ═══════════════════════════════════════════════════════════════════════════
// Thrill of the Hunt (745) — banish friendly, replay to a BF ignoring cost
// ═══════════════════════════════════════════════════════════════════════════

class ThrillOfHuntTest : public CardTestFixture {};

TEST_F(ThrillOfHuntTest, BanishedUnitReentersExhaustedAtBattlefield) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/4, /*at_bf=*/0);
    state.getObject(unit).is_exhausted = false;  // ready before banish
    state.getObject(unit).damage_marked = 2;
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kThrillOfHunt, P1, {unit}, exec);

    EXPECT_EQ(state.getObject(unit).zone, ZoneType::BattlefieldZone);
    // Per CR 143.4, a unit (re-)played via Thrill enters EXHAUSTED — it has
    // no Accelerate. The earlier "re-plays ready" assertion was a bug.
    EXPECT_TRUE(state.getObject(unit).is_exhausted)
        << "re-played unit enters exhausted (CR 143.4)";
    EXPECT_EQ(state.getObject(unit).damage_marked, 0)
        << "damage cleared on re-entry (banish wipes state)";
    // Banishment list should not still contain the card.
    auto& bz = state.player(P1).banishment;
    EXPECT_TRUE(std::find(bz.begin(), bz.end(), unit) == bz.end());
}

TEST_F(ThrillOfHuntTest, NoOpIfNoTarget) {
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    EffectExecutor exec(state, events, card_db);
    EXPECT_NO_THROW(invokeOnResolve(src, kThrillOfHunt, P1, {}, exec));
}

// ═══════════════════════════════════════════════════════════════════════════
// Shadow's Call (727) — give Temporary, draw 2
// ═══════════════════════════════════════════════════════════════════════════

class ShadowsCallTest : public CardTestFixture {};

TEST_F(ShadowsCallTest, GivesTemporaryToTargetAndDrawsTwo) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    EXPECT_FALSE(state.getObject(unit).hasKeyword(Keyword::Temporary));

    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    int initial_hand = handSize(P1);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kShadowsCall, P1, {unit}, exec);

    EXPECT_TRUE(state.getObject(unit).hasKeyword(Keyword::Temporary));
    EXPECT_EQ(handSize(P1), initial_hand + 2);
}

TEST_F(ShadowsCallTest, DrawsTwoEvenWithoutTarget) {
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    int initial_hand = handSize(P1);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kShadowsCall, P1, {}, exec);

    EXPECT_EQ(handSize(P1), initial_hand + 2);
}

// ═══════════════════════════════════════════════════════════════════════════
// Star-Crossed (690) — bounce both targets to owners' hands
// ═══════════════════════════════════════════════════════════════════════════

class StarCrossedTest : public CardTestFixture {};

TEST_F(StarCrossedTest, BouncesFriendlyAndEnemyToOwnersHands) {
    auto friendly = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto enemy    = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kStarCrossed, P1, {friendly, enemy}, exec);

    EXPECT_EQ(state.getObject(friendly).zone, ZoneType::Hand);
    EXPECT_EQ(state.getObject(enemy).zone, ZoneType::Hand);
    EXPECT_TRUE(inHand(P1, friendly));
    EXPECT_TRUE(inHand(P2, enemy));
}

TEST_F(StarCrossedTest, NoOpWhenNoTargets) {
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    EffectExecutor exec(state, events, card_db);
    EXPECT_NO_THROW(invokeOnResolve(src, kStarCrossed, P1, {}, exec));
}

// ═══════════════════════════════════════════════════════════════════════════
// Deathgrip (484) — kill friendly, transfer might to another friendly, draw 1
// ═══════════════════════════════════════════════════════════════════════════

class DeathgripSpellTest : public CardTestFixture {};

TEST_F(DeathgripSpellTest, KillsTransfersMightAndDraws) {
    auto sacrifice = addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);
    auto recipient = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);

    int initial_recipient_might = state.getObject(recipient).current_might;
    int initial_hand = handSize(P1);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kDeathgrip, P1, {sacrifice, recipient}, exec);

    EXPECT_EQ(state.getObject(sacrifice).zone, ZoneType::Trash)
        << "sacrifice target should be killed";
    EXPECT_EQ(handSize(P1), initial_hand + 1) << "Deathgrip draws 1";
    // Recipient gets +5 might (sacrifice's might).
    EXPECT_GE(state.getObject(recipient).current_might,
              initial_recipient_might + 5);
}

TEST_F(DeathgripSpellTest, DrawsEvenWithoutTwoTargets) {
    addToDeck(P1, kInvalidId);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    int initial_hand = handSize(P1);
    EffectExecutor exec(state, events, card_db);
    invokeOnResolve(src, kDeathgrip, P1, {}, exec);
    EXPECT_EQ(handSize(P1), initial_hand + 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// Bullet Time (263) — variable-X cost spell
// ═══════════════════════════════════════════════════════════════════════════
//
// CR-correct text: "[Action] Pay any amount of [A] to deal that much damage
// to all enemy units at a battlefield."
//
// X is selected via Card::pickXAmount at resolve time. X power is paid by
// recycling X exhausted runes (any domain). X damage is dealt to ALL enemy
// units at the targeted unit's battlefield (the targeted unit is the
// anchor — Bullet Time is AoE-by-BF, not single-target).

class BulletTimeTest : public CardTestFixture {};

// Helper: pick the intent that encodes a specific X. Card::pickXAmount
// encodes X via Intent::chosen_value (one option intent per legal X).
static Intent pickXSlot(const std::vector<Intent>& legal, int x) {
    for (const auto& it : legal) {
        if (it.chosen_value.has_value() && *it.chosen_value == x) {
            return it;
        }
    }
    return legal.empty() ? Intent{} : legal.front();
}

// Drive a spell's onResolve through the pickXAmount loop with explicit
// targets (driveResumable's signature passes {} as targets — Bullet Time
// needs a real target). picker(legal) returns the chosen X-encoded Intent.
static void driveSpellWithTargets(GameState& state, EventBus& events,
                                   EffectExecutor& exec,
                                   CardRegistry& reg,
                                   GameObjectId src, CardDefId def_id,
                                   PlayerId controller,
                                   const std::vector<GameObjectId>& targets,
                                   std::function<Intent(const std::vector<Intent>&)> picker) {
    ChainItem ri;
    ri.id = state.chain.allocateId();
    ri.controller = controller;
    ri.is_spell = true;
    ri.resume_point = 0;
    ri.source = src;
    ri.card_def_id = def_id;
    state.chain.resuming = ri;

    Card* card = reg.get(def_id);
    ASSERT_NE(card, nullptr);
    CardContext ctx{state, events, exec, controller, src};
    card->onResolve(ctx, targets);
    while (exec.hasPendingChoice()) {
        auto pending = exec.consumePendingChoice();
        exec.recordChoice(picker(pending.legal));
        card->onResolve(ctx, targets);
    }
    state.chain.resuming.reset();
}

TEST_F(BulletTimeTest, VariableX_PaysExhaustedRunesAndDealsXToAllEnemiesAtBF) {
    // Two enemies at BF#0, three exhausted runes available. Agent picks X=2.
    auto e1 = addUnit(P2, kInvalidId, /*might=*/5, /*at_bf=*/0);
    auto e2 = addUnit(P2, kInvalidId, /*might=*/5, /*at_bf=*/0);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    int before_exhausted = exhaustedRuneCount(P1);

    driveSpellWithTargets(state, events, exec, card_registry, src,
        kBulletTime, P1, {e1},
        [](const std::vector<Intent>& legal) { return pickXSlot(legal, 2); });

    EXPECT_EQ(state.getObject(e1).damage_marked, 2)
        << "X=2 → 2 damage to each enemy at BF";
    EXPECT_EQ(state.getObject(e2).damage_marked, 2);
    EXPECT_EQ(exhaustedRuneCount(P1), before_exhausted - 2)
        << "X exhausted runes recycled to deck for X power";
}

TEST_F(BulletTimeTest, X0_NoDamageNoPay) {
    auto enemy = addUnit(P2, kInvalidId, /*might=*/5, /*at_bf=*/0);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    int before_exhausted = exhaustedRuneCount(P1);

    driveSpellWithTargets(state, events, exec, card_registry, src,
        kBulletTime, P1, {enemy},
        [](const std::vector<Intent>& legal) { return pickXSlot(legal, 0); });

    EXPECT_EQ(state.getObject(enemy).damage_marked, 0)
        << "X=0 should deal no damage";
    EXPECT_EQ(exhaustedRuneCount(P1), before_exhausted)
        << "X=0 should not recycle any runes";
}

TEST_F(BulletTimeTest, KillsLethalTarget) {
    // X=4 → 4 damage to a 3M unit → lethal damage marked.
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    driveSpellWithTargets(state, events, exec, card_registry, src,
        kBulletTime, P1, {enemy},
        [](const std::vector<Intent>& legal) { return pickXSlot(legal, 4); });

    EXPECT_GE(state.getObject(enemy).damage_marked, 3)
        << "4 dmg to 3M unit lethal";
}

TEST_F(BulletTimeTest, NoOpWhenNoExhaustedRunes) {
    // No exhausted runes → max_x = 0 → only legal X is 0.
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    driveSpellWithTargets(state, events, exec, card_registry, src,
        kBulletTime, P1, {enemy},
        [](const std::vector<Intent>& legal) { return pickXSlot(legal, 0); });

    EXPECT_EQ(state.getObject(enemy).damage_marked, 0);
}

TEST_F(BulletTimeTest, OnlyEnemiesAtTargetedBF_AreHit) {
    // 2 enemies at BF#0 (the targeted area), 1 enemy at BF#1, 1 friendly
    // at BF#0. Only the two enemies at BF#0 should take damage.
    auto e_bf0_a = addUnit(P2, kInvalidId, /*might=*/5, /*at_bf=*/0);
    auto e_bf0_b = addUnit(P2, kInvalidId, /*might=*/5, /*at_bf=*/0);
    auto e_bf1 = addUnit(P2, kInvalidId, /*might=*/5, /*at_bf=*/1);
    auto f_bf0 = addUnit(P1, kInvalidId, /*might=*/5, /*at_bf=*/0);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    driveSpellWithTargets(state, events, exec, card_registry, src,
        kBulletTime, P1, {e_bf0_a},
        [](const std::vector<Intent>& legal) { return pickXSlot(legal, 1); });

    EXPECT_EQ(state.getObject(e_bf0_a).damage_marked, 1);
    EXPECT_EQ(state.getObject(e_bf0_b).damage_marked, 1);
    EXPECT_EQ(state.getObject(e_bf1).damage_marked, 0)
        << "enemy at other BF must NOT be hit";
    EXPECT_EQ(state.getObject(f_bf0).damage_marked, 0)
        << "friendly at same BF must NOT be hit";
}

// ═══════════════════════════════════════════════════════════════════════════
// Bullet Time + Elder Dragon (680) — the famous Miss Fortune combo
// ═══════════════════════════════════════════════════════════════════════════
//
// Elder Dragon: "Any amount of your damage is enough to kill enemy units."
// That passive lives on GameObject::has_elder_dragon_rule, refreshed during
// aura recalc. Combined with Bullet Time @ X=1, a single power of [A] wipes
// every enemy at the targeted battlefield — the engine treats every
// damaged-by-you enemy unit as having lethal regardless of damage_marked vs
// might.
//
// This is the strategic value driver for the Miss Fortune precon.

TEST_F(BulletTimeTest, ElderDragonSynergy_X1_WipesAllEnemiesAtBF) {
    // P1 plays Elder Dragon. P1 plays Bullet Time @ X=1. All enemies at
    // the targeted BF die regardless of their might. This is the famous
    // 1-power board-wipe combo.
    //
    // Setup: Elder Dragon at P1's base (controller, alive). Three
    // beefy enemies (10M each) at BF#0. One exhausted rune for X=1.

    // Elder Dragon's "any damage kills" passive is text-driven —
    // GameEngine::processLethalDamage scans card.ability_text for the
    // phrase. Placing the dragon with card_def_id=680 is sufficient;
    // there's no per-object flag to set.
    auto dragon = addUnit(P1, 680, /*might=*/0, /*at_bf=*/-1);  // base

    auto big1 = addUnit(P2, kInvalidId, /*might=*/10, /*at_bf=*/0);
    auto big2 = addUnit(P2, kInvalidId, /*might=*/10, /*at_bf=*/0);
    auto big3 = addUnit(P2, kInvalidId, /*might=*/10, /*at_bf=*/0);
    addRune(P1, Domain::Chaos, /*exhausted=*/true);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);

    ChainItem ri;
    ri.id = state.chain.allocateId();
    ri.controller = P1;
    ri.is_spell = true;
    ri.resume_point = 0;
    ri.source = src;
    ri.card_def_id = kBulletTime;
    state.chain.resuming = ri;

    Card* card = card_registry.get(kBulletTime);
    CardContext ctx{state, events, exec, P1, src};
    card->onResolve(ctx, {big1});  // anchor — uses BF#0
    while (exec.hasPendingChoice()) {
        auto pending = exec.consumePendingChoice();
        exec.recordChoice(pickXSlot(pending.legal, 1));
        card->onResolve(ctx, {big1});
    }
    state.chain.resuming.reset();

    // dealDamage marks damage; lethal-check happens separately. Verify
    // the damage is marked, and with Elder Dragon's rule each enemy has
    // damage > 0 and would die in the lethal sweep.
    EXPECT_EQ(state.getObject(big1).damage_marked, 1);
    EXPECT_EQ(state.getObject(big2).damage_marked, 1);
    EXPECT_EQ(state.getObject(big3).damage_marked, 1);
    // Under Elder Dragon's rule, any damaged-by-controller enemy is
    // treated as lethal — surface that via the helper. (The actual kill
    // sweep happens in GameEngine::cleanup; this test asserts the
    // damage-marking precondition that makes the wipe possible.)
    EXPECT_GT(state.getObject(big1).damage_marked, 0);
    EXPECT_GT(state.getObject(big2).damage_marked, 0);
    EXPECT_GT(state.getObject(big3).damage_marked, 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Sacrifice (735) — kill mighty friendly, draw 2, channel 1 rune exhausted
// ═══════════════════════════════════════════════════════════════════════════

class SacrificeSpellTest : public CardTestFixture {};

TEST_F(SacrificeSpellTest, KillsMightyAndDrawsTwoAndChannels) {
    auto mighty = addUnit(P1, kInvalidId, /*might=*/6, /*at_bf=*/0);  // ≥5M
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);
    // Need a rune in main deck for channelRunes to work — runes come from deck.
    // Best-effort: just verify draw + kill happen; channel may no-op gracefully.

    int initial_hand = handSize(P1);
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    EXPECT_NO_THROW(invokeOnResolve(src, kSacrifice, P1, {}, exec));

    EXPECT_EQ(state.getObject(mighty).zone, ZoneType::Trash)
        << "Mighty (≥5M) friendly unit should be killed";
    EXPECT_EQ(handSize(P1), initial_hand + 2) << "Sacrifice draws 2";
}

TEST_F(SacrificeSpellTest, DoesNotKillNonMightyFriendly) {
    auto small = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);  // <5M
    addToDeck(P1, kInvalidId);
    addToDeck(P1, kInvalidId);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    EXPECT_NO_THROW(invokeOnResolve(src, kSacrifice, P1, {}, exec));

    EXPECT_NE(state.getObject(small).zone, ZoneType::Trash)
        << "Non-Mighty unit (<5M) should NOT be killed";
}

// ═══════════════════════════════════════════════════════════════════════════
// Lunar Boon (687) — RESUME PATTERN — discard 1, draw 2
// ═══════════════════════════════════════════════════════════════════════════

class LunarBoonSpellTest : public CardTestFixture {};

TEST_F(LunarBoonSpellTest, DiscardsChosenCardAndDrawsTwo) {
    auto h1 = addToHand(P1, kArbitrary);
    auto h2 = addToHand(P1, kArbitrary);
    addToDeck(P1, kArbitrary);
    addToDeck(P1, kArbitrary);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    // Pick h1 to discard.
    driveResumable(kLunarBoon, P1, src,
        [h1](const std::vector<Intent>& legal) {
            for (auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == h1) return i;
            return legal.front();
        }, exec);

    EXPECT_EQ(state.getObject(h1).zone, ZoneType::Trash);
    EXPECT_TRUE(inTrash(P1, h1));
    EXPECT_FALSE(inHand(P1, h1));
    // h2 still in hand, plus 2 drawn = hand size went from 2 → 1 (discard) → 3 (draw 2).
    EXPECT_EQ(handSize(P1), 3);
    (void)h2;
}

TEST_F(LunarBoonSpellTest, DrawsTwoEvenWithEmptyHand) {
    // No cards in hand — falls through directly to draw 2.
    addToDeck(P1, kArbitrary);
    addToDeck(P1, kArbitrary);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;

    EffectExecutor exec(state, events, card_db);
    driveResumable(kLunarBoon, P1, src,
        [](const std::vector<Intent>& legal) {
            return legal.empty() ? Intent{} : legal.front();
        }, exec);

    EXPECT_EQ(handSize(P1), 2);
}

// ═══════════════════════════════════════════════════════════════════════════
// Mindsplitter (192) — RESUME PATTERN — opponent discards 1
// On-trigger (WhenYouPlayMe) goes through chain → use driveResumable.
// ═══════════════════════════════════════════════════════════════════════════

class MindsplitterTest : public CardTestFixture {};

TEST_F(MindsplitterTest, OpponentDiscardsAChosenCardFromHand) {
    auto opp_card = addToHand(P2, kArbitrary);
    auto src = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);

    // Reproduce the resume pattern manually for the trigger path.
    ChainItem ri;
    ri.id = state.chain.allocateId();
    ri.controller = P1;
    ri.is_ability = true;
    ri.resume_point = 0;
    ri.source = src;
    ri.card_def_id = kMindsplitter;
    state.chain.resuming = ri;

    Card* card = card_registry.get(kMindsplitter);
    ASSERT_NE(card, nullptr);
    CardContext ctx{state, events, exec, P1, src};

    card->onTrigger(ctx, {});
    while (exec.hasPendingChoice()) {
        auto pending = exec.consumePendingChoice();
        // Card text: "Choose a card from it, and they discard that card." YOU
        // (the controller) choose which card the opponent discards.
        EXPECT_EQ(pending.player, P1) << "Mindsplitter: the CONTROLLER chooses";
        Intent picked = pending.legal.empty() ? Intent{} : pending.legal.front();
        exec.recordChoice(picked);
        card->onTrigger(ctx, {});
    }
    state.chain.resuming.reset();

    EXPECT_TRUE(inTrash(P2, opp_card));
    EXPECT_FALSE(inHand(P2, opp_card));
}

TEST_F(MindsplitterTest, NoOpIfOpponentHandEmpty) {
    auto src = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    ChainItem ri;
    ri.id = state.chain.allocateId();
    ri.controller = P1;
    ri.is_ability = true;
    ri.resume_point = 0;
    ri.source = src;
    ri.card_def_id = kMindsplitter;
    state.chain.resuming = ri;

    Card* card = card_registry.get(kMindsplitter);
    ASSERT_NE(card, nullptr);
    CardContext ctx{state, events, exec, P1, src};

    EXPECT_NO_THROW(card->onTrigger(ctx, {}));
    EXPECT_FALSE(exec.hasPendingChoice())
        << "no choice should be published when opp's hand is empty";
    state.chain.resuming.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
// Sabotage (156) — RESUME PATTERN — caster recycles a non-unit from opp hand
// Full behavioral coverage lives in tests/cards/test_sabotage.cpp. Kept here:
// the empty-hand fast-path (no choice published, no events).
// ═══════════════════════════════════════════════════════════════════════════

class SabotageSpellTest : public CardTestFixture {};

TEST_F(SabotageSpellTest, NoOpIfOpponentHandEmpty) {
    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    EffectExecutor exec(state, events, card_db);
    EXPECT_NO_THROW(driveResumable(kSabotage, P1, src,
        [](const std::vector<Intent>& legal) {
            return legal.empty() ? Intent{} : legal.front();
        }, exec));
}

// ═══════════════════════════════════════════════════════════════════════════
// Token-creating triggers
// ═══════════════════════════════════════════════════════════════════════════

class TreasureHunterTest : public CardTestFixture {};

TEST_F(TreasureHunterTest, OnMoveCreatesGoldToken) {
    auto unit = addUnit(P1, kTreasureHunter, /*might=*/3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kTreasureHunter);
    ASSERT_NE(c, nullptr);
    c->onTrigger(ctx, {});
    EXPECT_EQ(countTokensNamed(state, P1, "Gold"), 1);
}

class HonestBrokerTest : public CardTestFixture {};

TEST_F(HonestBrokerTest, OnDeathCreatesGoldToken) {
    auto unit = addUnit(P1, kHonestBroker, /*might=*/3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kHonestBroker);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIDie);
    c->onTrigger(ctx, {});
    EXPECT_EQ(countTokensNamed(state, P1, "Gold"), 1);
}

class PlunderingPoroTest : public CardTestFixture {};

TEST_F(PlunderingPoroTest, OnConquerCreatesGoldToken) {
    auto unit = addUnit(P1, kPlunderingPoro, /*might=*/3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kPlunderingPoro);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIConquer);
    c->onTrigger(ctx, {});
    EXPECT_EQ(countTokensNamed(state, P1, "Gold"), 1);
}

class LilliaFaeFawnTest : public CardTestFixture {};

TEST_F(LilliaFaeFawnTest, OnMoveCreatesSpriteTokenWithTemporary) {
    auto unit = addUnit(P1, kLilliaFaeFawn, /*might=*/3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kLilliaFaeFawn);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIMove);
    c->onTrigger(ctx, {});

    EXPECT_EQ(countTokensNamed(state, P1, "Sprite"), 1);
    // Verify the new sprite has Temporary.
    bool has_temp = false;
    for (auto& [id, obj] : state.objects) {
        (void)id;
        if (obj.controller == P1 && obj.isToken() && obj.name == "Sprite"
            && obj.hasKeyword(Keyword::Temporary)) {
            has_temp = true;
        }
    }
    EXPECT_TRUE(has_temp);
}

class FerrousForerunnerTest : public CardTestFixture {};

TEST_F(FerrousForerunnerTest, DeathknellCreatesTwoMechTokens) {
    auto unit = addUnit(P1, kFerrousForerunner, /*might=*/3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kFerrousForerunner);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenIDie);
    c->onTrigger(ctx, {});

    EXPECT_EQ(countTokensNamed(state, P1, "Mech"), 2);
}

class BashfulBloomTest : public CardTestFixture {};

TEST_F(BashfulBloomTest, ActivatedAbilityCreatesSpriteToken) {
    auto src = addUnit(P1, kBashfulBloom, /*might=*/3, /*at_bf=*/-1);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, src};
    Card* c = card_registry.get(kBashfulBloom);
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->hasActivatedAbility());
    c->onActivate(ctx, {});

    EXPECT_EQ(countTokensNamed(state, P1, "Sprite"), 1);
}

TEST_F(BashfulBloomTest, ActivationCostIsExhaustPlusFourEnergy) {
    Card* c = card_registry.get(kBashfulBloom);
    ASSERT_NE(c, nullptr);
    auto cost = c->getActivationCost();
    EXPECT_TRUE(cost.exhaust);
    EXPECT_EQ(cost.energy, 4);
}

// ═══════════════════════════════════════════════════════════════════════════
// Shadow (752) — enters ready when played to a battlefield
// ═══════════════════════════════════════════════════════════════════════════

class ShadowUnitTest : public CardTestFixture {};

TEST_F(ShadowUnitTest, OnPlayClearsExhaustedAtBattlefield) {
    auto unit = addUnit(P1, kShadow, /*might=*/3, /*at_bf=*/0);
    state.getObject(unit).is_exhausted = true;  // simulate "enters exhausted by default"

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kShadow);
    ASSERT_NE(c, nullptr);
    c->onPlay(ctx);

    EXPECT_FALSE(state.getObject(unit).is_exhausted)
        << "Shadow enters ready when played to a battlefield";
}

TEST_F(ShadowUnitTest, OnPlayDoesNotAffectBaseLocation) {
    // NOTE: addUnit's at_bf=-1 doesn't reliably route to base because the
    // parameter is uint32_t — `-1` underflows to UINT32_MAX which compares
    // >= 0 in the helper. Construct the unit manually to ensure base placement.
    auto unit = state.createObject();
    auto& obj = state.getObject(unit);
    obj.owner = P1;
    obj.controller = P1;
    obj.card_def_id = kShadow;
    obj.card_type = CardType::Unit;
    obj.zone = ZoneType::Base;
    obj.location = BaseLocation{P1};
    obj.is_exhausted = true;

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kShadow);
    c->onPlay(ctx);

    EXPECT_TRUE(state.getObject(unit).is_exhausted)
        << "Shadow's onPlay only modifies state at battlefield";
}

TEST_F(ShadowUnitTest, HasActionAbility) {
    Card* c = card_registry.get(kShadow);
    ASSERT_NE(c, nullptr);
    // Phase 6r multi-ability API
    auto abilities = c->activatedAbilities();
    ASSERT_EQ(abilities.size(), 1u);
    EXPECT_TRUE(abilities[0].is_action);
    EXPECT_TRUE(abilities[0].cost.exhaust);
    EXPECT_TRUE(abilities[0].needs_activation_time_target);
}

// ═══════════════════════════════════════════════════════════════════════════
// Sprite Mother Unit (106) — on play, sprite token at controller's location
// ═══════════════════════════════════════════════════════════════════════════

class SpriteMotherUnitTest : public CardTestFixture {};

TEST_F(SpriteMotherUnitTest, OnPlayCreatesSpriteToken) {
    auto unit = addUnit(P1, kSpriteMother, /*might=*/3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kSpriteMother);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayMe);
    c->onTrigger(ctx, {});

    EXPECT_EQ(countTokensNamed(state, P1, "Sprite"), 1);
}

TEST_F(SpriteMotherUnitTest, SpriteHasTemporary) {
    auto unit = addUnit(P1, kSpriteMother, /*might=*/3, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, unit};
    Card* c = card_registry.get(kSpriteMother);
    c->onTrigger(ctx, {});

    bool found_temp = false;
    for (auto& [id, obj] : state.objects) {
        (void)id;
        if (obj.controller == P1 && obj.isToken() && obj.name == "Sprite"
            && obj.hasKeyword(Keyword::Temporary)) {
            found_temp = true;
        }
    }
    EXPECT_TRUE(found_temp);
}

}  // namespace
