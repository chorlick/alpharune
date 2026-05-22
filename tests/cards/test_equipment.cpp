/// @file test_equipment.cpp
/// Per-card unit tests for the 33 manually-implemented equipment gear cards
/// in src/cards/manual/equip_cards.cpp and the 9 weaponmaster units in
/// src/cards/manual/weaponmaster_cards.cpp.
///
/// Coverage strategy — breadth over depth:
///   1. Simple equip — hasEquipAbility() true, onEquip() succeeds, target
///      unit gains attachment_might_bonus equal to gear.might_bonus.
///   2. Equipped triggers — set up unit + equip, fire onEquippedTrigger,
///      verify the trigger effect on game state.
///   3. Granted keywords — assert equippedKeywords()/equippedAssault()
///      /equippedShield()/equippedDeflect() return expected values.
///   4. Weaponmaster — on-play trigger detaches an Equipment from the board
///      and reattaches it to self for free.
///
/// Tests use the CardTestFixture from card_test_fixture.h.

#include "tests/cards/card_test_fixture.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

// ─── Equipment IDs (keep in sync with equip_cards.cpp registration) ────────

constexpr CardDefId kSerratedDirk        = 332;  // [R] Assault 2 keyword
constexpr CardDefId kRecurveBow          = 339;  // [R] WhenIAttackOrDefend deal 2
constexpr CardDefId kLongSword           = 345;  // [R] +2M, no effect
constexpr CardDefId kDoransShield        = 356;  // [G] Tank keyword
constexpr CardDefId kClothArmor          = 387;  // [B] Shield 2 keyword
constexpr CardDefId kDoransBlade         = 417;  // [O] +2M, no effect
constexpr CardDefId kHexdrinker          = 424;  // [O] Deflect keyword
constexpr CardDefId kWarmogsArmor        = 430;  // [O] WhenIConquer buff
constexpr CardDefId kTrinityForce        = 437;  // [O] WhenIHold +1 score
constexpr CardDefId kBoneshiver          = 439;  // [1][O] WhenIConquer channel exhausted
constexpr CardDefId kDoransRing          = 445;  // [P] WhenIConquer discard 1 draw 1
constexpr CardDefId kBootsOfSwiftness    = 454;  // [P] Ganking keyword
constexpr CardDefId kCull                = 455;  // [P] WhenIConquer create Gold
constexpr CardDefId kLastRites           = 471;  // [P] + recycle 2 from trash
constexpr CardDefId kEyeOfTheHerald      = 474;  // [Y] WhenIMove create Recruit
constexpr CardDefId kBFSword             = 482;  // [Y] +3M, no effect
constexpr CardDefId kSacredShears        = 493;  // [Y] WhenIDie draw 1
constexpr CardDefId kSpinningAxe         = 504;  // [A] +3M
constexpr CardDefId kForgefireCape       = 507;  // [A] WhenIAttackOrDefend AoE 2
constexpr CardDefId kHuntersMachete      = 658;  // [O] WhenIConquerOrHold +1 XP
constexpr CardDefId kSkyfallAreion       = 353;  // [1][R] +2M
constexpr CardDefId kBrutalizer          = 365;  // [G] +1M
constexpr CardDefId kGuardianAngel       = 374;  // [G] +1M
constexpr CardDefId kSteraksGage         = 379;  // [G] +3M
constexpr CardDefId kSvellsongur         = 382;  // [1][G] +0M
constexpr CardDefId kExperimentalHexplate= 396;  // [B] +1M
constexpr CardDefId kWorldAtlas          = 408;  // [B] WhenIHold create 2 Gold
constexpr CardDefId kZeroDrive           = 412;  // [1][B] +2M
constexpr CardDefId kEdgeOfNight         = 460;  // [P] +2M
constexpr CardDefId kBladeOfRuinedKing   = 498;  // [Y] +4M
constexpr CardDefId kRabadonsDeathcrown  = 508;  // [A] +3M
constexpr CardDefId kShurelyasRequiem    = 509;  // [A] +2M
constexpr CardDefId kBlightedBattleaxe   = 581;  // [1][R] +4M
constexpr CardDefId kSoulSword           = 601;  // [G] +1M
constexpr CardDefId kShepherdsHeirloom   = 720;  // [Y] +2M
constexpr CardDefId kHextechGauntlets    = 748;  // [3][A] +3M

// ─── Weaponmaster unit IDs ─────────────────────────────────────────────────

constexpr CardDefId kArmedAssailant      = 324;
constexpr CardDefId kSentinelAdept       = 331;
constexpr CardDefId kOrnnForgeGod        = 407;
constexpr CardDefId kCombatChef          = 414;
constexpr CardDefId kVeteranPoro         = 421;
constexpr CardDefId kLucianMerciless     = 435;
constexpr CardDefId kJaxUnrelenting      = 440;
constexpr CardDefId kMasterBingwen       = 448;
constexpr CardDefId kYoneBlademaster     = 544;

// ═══════════════════════════════════════════════════════════════════════════
// Equipment fixture — extends CardTestFixture with equip-specific helpers.
// ═══════════════════════════════════════════════════════════════════════════

class EquipmentTest : public CardTestFixture {
protected:
    /// Spawn a gear card on a battlefield, ready to equip.
    /// Sets `might_bonus` from CardDef so attachment_might_bonus updates correctly,
    /// and sets `tags` so Weaponmaster equipment-detection can find it.
    GameObjectId addGearOnBoard(PlayerId owner, CardDefId def_id,
                                  BattlefieldId at_bf = -1) {
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
            obj.domains = def.domains;
            obj.tags = def.tags;            // contains "Equipment" for Equipment gear
            obj.might_bonus = def.might_bonus;
        } else {
            obj.name = "TestGear";
        }
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{at_bf};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    /// Wrapper: equip `gear` to `unit`, return success flag.
    bool runEquip(GameObjectId gear, GameObjectId unit, EffectExecutor& exec) {
        Card* c = card_registry.get(state.getObject(gear).card_def_id);
        if (!c) return false;
        CardContext ctx{state, events, exec, state.getObject(gear).controller, gear};
        return c->onEquip(ctx, unit);
    }

    /// Add enough ready runes of `domain` (and a few generic) to pay any
    /// reasonable equip cost. The standardEquip helper exhausts ready runes
    /// for energy and recycles a matching-domain rune for power.
    void giveRunesForEquip(PlayerId owner, Domain domain, int extra_energy = 0) {
        // Always have at least one matching-domain rune for the recycle cost.
        addRune(owner, domain);
        addRune(owner, domain);
        // Extra ready runes for [N] energy portion.
        for (int i = 0; i < extra_energy; ++i) addRune(owner, domain);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// hasEquipAbility — every manually-implemented gear must declare the ability.
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(EquipmentTest, AllManualGearReportsHasEquipAbility) {
    for (auto def_id : {
            kSerratedDirk, kRecurveBow, kLongSword, kDoransShield,
            kClothArmor, kDoransBlade, kHexdrinker, kWarmogsArmor,
            kTrinityForce, kBoneshiver, kDoransRing, kBootsOfSwiftness,
            kCull, kLastRites, kEyeOfTheHerald, kBFSword, kSacredShears,
            kSpinningAxe, kForgefireCape, kHuntersMachete, kSkyfallAreion,
            kBrutalizer, kGuardianAngel, kSteraksGage, kSvellsongur,
            kExperimentalHexplate, kWorldAtlas, kZeroDrive, kEdgeOfNight,
            kBladeOfRuinedKing, kRabadonsDeathcrown, kShurelyasRequiem,
            kBlightedBattleaxe, kSoulSword, kShepherdsHeirloom,
            kHextechGauntlets,
        }) {
        Card* c = card_registry.get(def_id);
        ASSERT_NE(c, nullptr) << "card " << def_id << " not registered";
        EXPECT_TRUE(c->hasEquipAbility())
            << "card " << def_id << " (" << card_db.get(def_id).name
            << ") should report hasEquipAbility() == true";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Standard equip path — onEquip succeeds, attachment_might_bonus increases.
// One test per simple-equip gear (parameterized via a loop with a helper).
// ═══════════════════════════════════════════════════════════════════════════

namespace {
struct SimpleEquipCase {
    CardDefId gear;
    Domain    domain;
    int       extra_energy;  // [N] in the cost — beyond the recycled rune
};
}  // namespace

class SimpleEquipTest : public EquipmentTest {
protected:
    void runSimpleEquip(const SimpleEquipCase& c) {
        auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
        auto gear = addGearOnBoard(P1, c.gear, /*at_bf=*/0);
        giveRunesForEquip(P1, c.domain, c.extra_energy);

        int initial_bonus  = state.getObject(unit).attachment_might_bonus;
        int gear_bonus     = state.getObject(gear).might_bonus;

        EffectExecutor exec(state, events, card_db);
        EXPECT_TRUE(runEquip(gear, unit, exec))
            << "equip should succeed for card " << c.gear
            << " (" << card_db.get(c.gear).name << ")";

        EXPECT_EQ(state.getObject(unit).attachment_might_bonus,
                   initial_bonus + gear_bonus)
            << "attachment_might_bonus should grow by gear's might_bonus";
        EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(unit));
        // Gear should have moved to the unit's location.
        EXPECT_EQ(state.getObject(gear).zone, ZoneType::BattlefieldZone);
    }
};

TEST_F(SimpleEquipTest, SerratedDirk)        { runSimpleEquip({kSerratedDirk,   Domain::Fury, 0}); }
TEST_F(SimpleEquipTest, RecurveBow)          { runSimpleEquip({kRecurveBow,     Domain::Fury, 0}); }
TEST_F(SimpleEquipTest, LongSword)           { runSimpleEquip({kLongSword,      Domain::Fury, 0}); }
TEST_F(SimpleEquipTest, DoransShield)        { runSimpleEquip({kDoransShield,   Domain::Calm, 0}); }
TEST_F(SimpleEquipTest, ClothArmor)          { runSimpleEquip({kClothArmor,     Domain::Mind, 0}); }
TEST_F(SimpleEquipTest, DoransBlade)         { runSimpleEquip({kDoransBlade,    Domain::Body, 0}); }
TEST_F(SimpleEquipTest, Hexdrinker)          { runSimpleEquip({kHexdrinker,     Domain::Body, 0}); }
TEST_F(SimpleEquipTest, WarmogsArmor)        { runSimpleEquip({kWarmogsArmor,   Domain::Body, 0}); }
TEST_F(SimpleEquipTest, TrinityForce)        { runSimpleEquip({kTrinityForce,   Domain::Body, 0}); }
TEST_F(SimpleEquipTest, Boneshiver)          { runSimpleEquip({kBoneshiver,     Domain::Body, 1}); }
TEST_F(SimpleEquipTest, DoransRing)          { runSimpleEquip({kDoransRing,     Domain::Chaos, 0}); }
TEST_F(SimpleEquipTest, BootsOfSwiftness)    { runSimpleEquip({kBootsOfSwiftness, Domain::Chaos, 0}); }
TEST_F(SimpleEquipTest, Cull)                { runSimpleEquip({kCull,           Domain::Chaos, 0}); }
TEST_F(SimpleEquipTest, EyeOfTheHerald)      { runSimpleEquip({kEyeOfTheHerald, Domain::Order, 0}); }
TEST_F(SimpleEquipTest, BFSword)             { runSimpleEquip({kBFSword,        Domain::Order, 0}); }
TEST_F(SimpleEquipTest, SacredShears)        { runSimpleEquip({kSacredShears,   Domain::Order, 0}); }
TEST_F(SimpleEquipTest, HuntersMachete)      { runSimpleEquip({kHuntersMachete, Domain::Body, 0}); }
TEST_F(SimpleEquipTest, SkyfallAreion)       { runSimpleEquip({kSkyfallAreion,  Domain::Fury, 1}); }
TEST_F(SimpleEquipTest, Brutalizer)          { runSimpleEquip({kBrutalizer,     Domain::Calm, 0}); }
TEST_F(SimpleEquipTest, GuardianAngel)       { runSimpleEquip({kGuardianAngel,  Domain::Calm, 0}); }
TEST_F(SimpleEquipTest, SteraksGage)         { runSimpleEquip({kSteraksGage,    Domain::Calm, 0}); }
TEST_F(SimpleEquipTest, Svellsongur)         { runSimpleEquip({kSvellsongur,    Domain::Calm, 1}); }
TEST_F(SimpleEquipTest, ExperimentalHexplate){ runSimpleEquip({kExperimentalHexplate, Domain::Mind, 0}); }
TEST_F(SimpleEquipTest, WorldAtlas)          { runSimpleEquip({kWorldAtlas,     Domain::Mind, 0}); }
TEST_F(SimpleEquipTest, ZeroDrive)           { runSimpleEquip({kZeroDrive,      Domain::Mind, 1}); }
TEST_F(SimpleEquipTest, EdgeOfNight)         { runSimpleEquip({kEdgeOfNight,    Domain::Chaos, 0}); }
TEST_F(SimpleEquipTest, BladeOfRuinedKing)   { runSimpleEquip({kBladeOfRuinedKing, Domain::Order, 0}); }
TEST_F(SimpleEquipTest, BlightedBattleaxe)   { runSimpleEquip({kBlightedBattleaxe, Domain::Fury, 1}); }
TEST_F(SimpleEquipTest, SoulSword)           { runSimpleEquip({kSoulSword,      Domain::Calm, 0}); }
TEST_F(SimpleEquipTest, ShepherdsHeirloom)   { runSimpleEquip({kShepherdsHeirloom, Domain::Order, 0}); }

// ═══════════════════════════════════════════════════════════════════════════
// UniversalEquipGear ([Equip] [A] — recycle ANY rune, not domain-specific)
// ═══════════════════════════════════════════════════════════════════════════

class UniversalEquipTest : public EquipmentTest {
protected:
    /// Universal equip pulls any ready rune; the domain we add doesn't matter.
    void runUniversalEquip(CardDefId gear_id, int energy_cost) {
        auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
        auto gear = addGearOnBoard(P1, gear_id, /*at_bf=*/0);
        // Need 1 rune to recycle + extra ready runes for energy_cost.
        addRune(P1, Domain::Fury);
        for (int i = 0; i < energy_cost; ++i) addRune(P1, Domain::Fury);

        int gear_bonus = state.getObject(gear).might_bonus;
        EffectExecutor exec(state, events, card_db);
        EXPECT_TRUE(runEquip(gear, unit, exec));
        EXPECT_EQ(state.getObject(unit).attachment_might_bonus, gear_bonus);
        EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(unit));
    }
};

TEST_F(UniversalEquipTest, SpinningAxe)         { runUniversalEquip(kSpinningAxe, 0); }
TEST_F(UniversalEquipTest, ForgefireCape)       { runUniversalEquip(kForgefireCape, 0); }
TEST_F(UniversalEquipTest, RabadonsDeathcrown)  { runUniversalEquip(kRabadonsDeathcrown, 0); }
TEST_F(UniversalEquipTest, ShurelyasRequiem)    { runUniversalEquip(kShurelyasRequiem, 0); }
TEST_F(UniversalEquipTest, HextechGauntlets)    { runUniversalEquip(kHextechGauntlets, 3); }

// ═══════════════════════════════════════════════════════════════════════════
// Granted keywords — equippedKeywords() / equippedAssault/Shield/Deflect.
// ═══════════════════════════════════════════════════════════════════════════

class GrantedKeywordsTest : public EquipmentTest {};

TEST_F(GrantedKeywordsTest, SerratedDirkGrantsAssault2) {
    Card* c = card_registry.get(kSerratedDirk);
    EXPECT_TRUE(c->equippedKeywords().has(Keyword::Assault));
    EXPECT_EQ(c->equippedAssault(), 2);
}

TEST_F(GrantedKeywordsTest, ClothArmorGrantsShield2) {
    Card* c = card_registry.get(kClothArmor);
    EXPECT_TRUE(c->equippedKeywords().has(Keyword::Shield));
    EXPECT_EQ(c->equippedShield(), 2);
}

TEST_F(GrantedKeywordsTest, DoransShieldGrantsTank) {
    Card* c = card_registry.get(kDoransShield);
    EXPECT_TRUE(c->equippedKeywords().has(Keyword::Tank));
}

TEST_F(GrantedKeywordsTest, HexdrinkerGrantsDeflect) {
    Card* c = card_registry.get(kHexdrinker);
    EXPECT_TRUE(c->equippedKeywords().has(Keyword::Deflect));
}

TEST_F(GrantedKeywordsTest, BootsOfSwiftnessGrantsGanking) {
    Card* c = card_registry.get(kBootsOfSwiftness);
    EXPECT_TRUE(c->equippedKeywords().has(Keyword::Ganking));
}

TEST_F(GrantedKeywordsTest, BFSwordGrantsNoKeywords) {
    // Vanilla +M gear should grant no keywords.
    Card* c = card_registry.get(kBFSword);
    EXPECT_EQ(c->equippedKeywords().bits, 0u);
    EXPECT_EQ(c->equippedAssault(), 0);
    EXPECT_EQ(c->equippedShield(), 0);
    EXPECT_EQ(c->equippedDeflect(), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// Equipped triggers — onEquippedTrigger fires the gear's effect.
// ═══════════════════════════════════════════════════════════════════════════

class EquippedTriggerTest : public EquipmentTest {};

TEST_F(EquippedTriggerTest, TrinityForceScoresOnHold) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kTrinityForce, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Body);
    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, unit, exec));

    Card* c = card_registry.get(kTrinityForce);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIHold);

    int initial_score = state.player(P1).score;
    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, unit, {});
    EXPECT_EQ(state.player(P1).score, initial_score + 1)
        << "Trinity Force should score 1 point on hold";
}

TEST_F(EquippedTriggerTest, WarmogsArmorBuffsOnConquer) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kWarmogsArmor, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Body);
    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, unit, exec));

    Card* c = card_registry.get(kWarmogsArmor);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIConquer);

    int initial_buffs = state.getObject(unit).buff_count;
    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, unit, {});
    EXPECT_GT(state.getObject(unit).buff_count, initial_buffs)
        << "Warmog's Armor should buff the unit on conquer";
}

TEST_F(EquippedTriggerTest, DoransRingDiscardThenDraw) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kDoransRing, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Chaos);
    auto deck_card = addToDeck(P1, kInvalidId);
    auto hand_card = addToHand(P1, kInvalidId);

    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, unit, exec));

    Card* c = card_registry.get(kDoransRing);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIConquer);

    int initial_deck = deckSize(P1);
    int initial_hand = handSize(P1);
    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, unit, {});

    // discard 1 (-1 hand) then draw 1 (-1 deck, +1 hand) → net hand: same;
    // net deck: -1.
    EXPECT_EQ(deckSize(P1), initial_deck - 1) << "draw consumes a deck card";
    EXPECT_EQ(handSize(P1), initial_hand)     << "discard 1 + draw 1 = no net hand change";
    EXPECT_TRUE(inHand(P1, deck_card));        // drawn
    (void)hand_card;
}

TEST_F(EquippedTriggerTest, SacredShearsDeathknellDraws1) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kSacredShears, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Order);
    auto deck_card = addToDeck(P1, kInvalidId);

    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, unit, exec));

    Card* c = card_registry.get(kSacredShears);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIDie);

    int initial_hand = handSize(P1);
    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, unit, {});
    EXPECT_EQ(handSize(P1), initial_hand + 1) << "Sacred Shears Deathknell draws 1";
    EXPECT_TRUE(inHand(P1, deck_card));
}

TEST_F(EquippedTriggerTest, HuntersMacheteGrantsXpOnConquerOrHold) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kHuntersMachete, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Body);
    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, unit, exec));

    Card* c = card_registry.get(kHuntersMachete);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIConquerOrHold);

    int initial_xp = state.player(P1).xp;
    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, unit, {});
    EXPECT_EQ(state.player(P1).xp, initial_xp + 1) << "Hunter's Machete grants +1 XP";
}

TEST_F(EquippedTriggerTest, RecurveBowDealsDamageToEnemyHere) {
    auto attacker = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto enemy    = addUnit(P2, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto gear     = addGearOnBoard(P1, kRecurveBow, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Fury);
    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, attacker, exec));

    Card* c = card_registry.get(kRecurveBow);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIAttackOrDefend);

    int initial_damage = state.getObject(enemy).damage_marked;
    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, attacker, {});
    EXPECT_EQ(state.getObject(enemy).damage_marked, initial_damage + 2)
        << "Recurve Bow should deal 2 to an enemy unit at the BF";
}

TEST_F(EquippedTriggerTest, ForgefireCapeDealsAoeToAllEnemiesHere) {
    auto attacker = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto enemy_a  = addUnit(P2, kInvalidId, /*might=*/4, /*at_bf=*/0);
    auto enemy_b  = addUnit(P2, kInvalidId, /*might=*/5, /*at_bf=*/0);
    auto enemy_other_bf = addUnit(P2, kInvalidId, /*might=*/4, /*at_bf=*/1);
    auto gear     = addGearOnBoard(P1, kForgefireCape, /*at_bf=*/0);
    addRune(P1, Domain::Fury);  // universal — any rune

    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, attacker, exec));

    Card* c = card_registry.get(kForgefireCape);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIAttackOrDefend);

    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, attacker, {});
    EXPECT_EQ(state.getObject(enemy_a).damage_marked, 2);
    EXPECT_EQ(state.getObject(enemy_b).damage_marked, 2);
    EXPECT_EQ(state.getObject(enemy_other_bf).damage_marked, 0)
        << "AoE should not reach units at other battlefields";
}

TEST_F(EquippedTriggerTest, CullCreatesGoldOnConquer) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kCull, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Chaos);
    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, unit, exec));

    Card* c = card_registry.get(kCull);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIConquer);

    int initial_gear_count = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isGear() && obj.controller == P1) ++initial_gear_count;
    }

    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, unit, {});

    int after_gear_count = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isGear() && obj.controller == P1) ++after_gear_count;
    }
    EXPECT_EQ(after_gear_count, initial_gear_count + 1)
        << "Cull should create one Gold gear token on conquer";
}

TEST_F(EquippedTriggerTest, WorldAtlasCreatesTwoGoldOnHold) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kWorldAtlas, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Mind);
    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, unit, exec));

    Card* c = card_registry.get(kWorldAtlas);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIHold);

    int initial_gear_count = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isGear() && obj.controller == P1) ++initial_gear_count;
    }

    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, unit, {});

    int after_gear_count = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isGear() && obj.controller == P1) ++after_gear_count;
    }
    EXPECT_EQ(after_gear_count, initial_gear_count + 2)
        << "World Atlas should create two Gold gear tokens on hold";
}

TEST_F(EquippedTriggerTest, EyeOfTheHeraldCreatesRecruitOnMove) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kEyeOfTheHerald, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Order);
    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, unit, exec));

    Card* c = card_registry.get(kEyeOfTheHerald);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIMove);

    int initial_unit_count = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isUnit() && obj.controller == P1) ++initial_unit_count;
    }

    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, unit, {});

    int after_unit_count = 0;
    for (auto& [id, obj] : state.objects) {
        if (obj.isUnit() && obj.controller == P1) ++after_unit_count;
    }
    EXPECT_EQ(after_unit_count, initial_unit_count + 1)
        << "Eye of the Herald should create a Recruit unit token on move";
}

TEST_F(EquippedTriggerTest, BoneshiverChannelsExhaustedRuneOnConquer) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kBoneshiver, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Body, /*extra_energy=*/1);
    // Stock the rune deck so channelRunes has something to channel.
    auto rune_in_deck = addRune(P1, Domain::Body);
    state.getObject(rune_in_deck).zone = ZoneType::RuneDeck;
    state.getObject(rune_in_deck).location = std::nullopt;
    state.player(P1).rune_deck.push_back(rune_in_deck);

    EffectExecutor exec(state, events, card_db);
    ASSERT_TRUE(runEquip(gear, unit, exec));

    Card* c = card_registry.get(kBoneshiver);
    EXPECT_EQ(c->equippedTriggerType(), TriggerType::WhenIConquer);

    size_t initial_deck_size = state.player(P1).rune_deck.size();
    CardContext ctx{state, events, exec, P1, gear};
    c->onEquippedTrigger(ctx, unit, {});
    // channelRunes should have removed one rune from the rune_deck.
    EXPECT_LT(state.player(P1).rune_deck.size(), initial_deck_size)
        << "Boneshiver should channel 1 rune (deck shrinks)";
}

// ═══════════════════════════════════════════════════════════════════════════
// Last Rites — additional cost (recycle 2 from trash) gates the equip.
// ═══════════════════════════════════════════════════════════════════════════

class LastRitesTest : public EquipmentTest {};

TEST_F(LastRitesTest, FailsWhenTrashLacksTwoCards) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kLastRites, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Chaos);
    // Trash has 0 cards — additional cost can't be paid.
    EffectExecutor exec(state, events, card_db);
    EXPECT_FALSE(runEquip(gear, unit, exec))
        << "Last Rites equip should fail when trash has < 2 cards";
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, 0);
}

TEST_F(LastRitesTest, SucceedsAndRecyclesTwoFromTrash) {
    auto unit = addUnit(P1, kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto gear = addGearOnBoard(P1, kLastRites, /*at_bf=*/0);
    giveRunesForEquip(P1, Domain::Chaos);

    // Stock trash with 3 cards — equip recycles 2.
    auto t1 = state.createObject();
    state.getObject(t1).owner = P1; state.getObject(t1).controller = P1;
    state.getObject(t1).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(t1);
    auto t2 = state.createObject();
    state.getObject(t2).owner = P1; state.getObject(t2).controller = P1;
    state.getObject(t2).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(t2);
    auto t3 = state.createObject();
    state.getObject(t3).owner = P1; state.getObject(t3).controller = P1;
    state.getObject(t3).zone = ZoneType::Trash;
    state.player(P1).trash.push_back(t3);

    EffectExecutor exec(state, events, card_db);
    EXPECT_TRUE(runEquip(gear, unit, exec));
    EXPECT_EQ(trashSize(P1), 1) << "Last Rites should recycle exactly 2 from trash";
    int gear_bonus = state.getObject(gear).might_bonus;
    EXPECT_EQ(state.getObject(unit).attachment_might_bonus, gear_bonus);
}

// ═══════════════════════════════════════════════════════════════════════════
// Weaponmaster — on-play attaches an Equipment from the board to self.
// ═══════════════════════════════════════════════════════════════════════════

class WeaponmasterTest : public CardTestFixture {
protected:
    /// Spawn a generic Equipment gear on a battlefield with `Equipment` tag.
    /// Used by Weaponmaster trigger to find a target.
    GameObjectId addEquipmentGear(PlayerId owner, CardDefId def_id,
                                    BattlefieldId at_bf) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.tags = def.tags;
            obj.might_bonus = def.might_bonus;
        } else {
            obj.name = "TestGear";
            obj.tags = {"Equipment"};
        }
        obj.zone = ZoneType::BattlefieldZone;
        obj.location = BattlefieldLocation{at_bf};
        return id;
    }

    /// Drive a Weaponmaster's WhenYouPlayMe trigger.
    void fireWeaponmasterTrigger(GameObjectId weapon_master,
                                  CardDefId def_id,
                                  EffectExecutor& exec) {
        Card* c = card_registry.get(def_id);
        ASSERT_NE(c, nullptr);
        EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayMe);
        CardContext ctx{state, events, exec, P1, weapon_master};
        c->onTrigger(ctx, {});
    }
};

TEST_F(WeaponmasterTest, AllWeaponmasterUnitsRegistered) {
    for (auto def_id : {
            kArmedAssailant, kSentinelAdept, kOrnnForgeGod, kCombatChef,
            kVeteranPoro, kLucianMerciless, kJaxUnrelenting, kMasterBingwen,
            kYoneBlademaster,
        }) {
        Card* c = card_registry.get(def_id);
        ASSERT_NE(c, nullptr) << "weaponmaster " << def_id << " not registered";
        EXPECT_EQ(c->triggerType(), TriggerType::WhenYouPlayMe)
            << "weaponmaster " << def_id << " should trigger on play";
    }
}

TEST_F(WeaponmasterTest, ArmedAssailantEquipsAvailableEquipment) {
    auto wm   = addUnit(P1, kArmedAssailant, /*might=*/0, /*at_bf=*/0);
    auto gear = addEquipmentGear(P1, kLongSword, /*at_bf=*/0);
    int gear_bonus = state.getObject(gear).might_bonus;

    EffectExecutor exec(state, events, card_db);
    fireWeaponmasterTrigger(wm, kArmedAssailant, exec);

    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(wm))
        << "weaponmaster should attach the available Equipment to itself";
    EXPECT_EQ(state.getObject(wm).attachment_might_bonus, gear_bonus);
}

TEST_F(WeaponmasterTest, OrnnDetachesGearFromOtherUnitToSelf) {
    // Existing unit with an Equipment already attached — Ornn steals it.
    auto holder = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto gear   = addEquipmentGear(P1, kBFSword, /*at_bf=*/0);
    int gear_bonus = state.getObject(gear).might_bonus;
    state.getObject(gear).attached_to = holder;
    state.getObject(holder).attachments.push_back(gear);
    state.getObject(holder).attachment_might_bonus += gear_bonus;
    state.getObject(holder).recomputeMight();

    auto ornn = addUnit(P1, kOrnnForgeGod, /*might=*/0, /*at_bf=*/0);

    EffectExecutor exec(state, events, card_db);
    fireWeaponmasterTrigger(ornn, kOrnnForgeGod, exec);

    EXPECT_EQ(state.getObject(gear).attached_to, std::optional<GameObjectId>(ornn))
        << "Ornn should pull the gear off the previous holder";
    EXPECT_EQ(state.getObject(ornn).attachment_might_bonus, gear_bonus);
    EXPECT_EQ(state.getObject(holder).attachment_might_bonus, 0)
        << "previous holder loses the gear's might bonus";
    auto& holder_attachments = state.getObject(holder).attachments;
    EXPECT_EQ(std::find(holder_attachments.begin(), holder_attachments.end(), gear),
              holder_attachments.end())
        << "previous holder's attachments list no longer contains gear";
}

TEST_F(WeaponmasterTest, NoEquipmentAvailableNoOps) {
    auto wm = addUnit(P1, kSentinelAdept, /*might=*/0, /*at_bf=*/0);
    EffectExecutor exec(state, events, card_db);
    int initial_bonus = state.getObject(wm).attachment_might_bonus;
    fireWeaponmasterTrigger(wm, kSentinelAdept, exec);
    EXPECT_EQ(state.getObject(wm).attachment_might_bonus, initial_bonus)
        << "weaponmaster trigger with no Equipment available should be a no-op";
}

TEST_F(WeaponmasterTest, IgnoresNonEquipmentGear) {
    // A gear with no Equipment tag should not be attached — Weaponmaster
    // text restricts to Equipment.
    auto wm = addUnit(P1, kCombatChef, /*might=*/0, /*at_bf=*/0);
    // Hand-built gear with no Equipment tag.
    auto fake = state.createObject();
    auto& g = state.getObject(fake);
    g.owner = P1; g.controller = P1;
    g.card_type = CardType::Gear;
    g.name = "NotEquipment";
    g.tags = {"Mystery"};
    g.zone = ZoneType::BattlefieldZone;
    g.location = BattlefieldLocation{0};
    g.might_bonus = 5;

    EffectExecutor exec(state, events, card_db);
    fireWeaponmasterTrigger(wm, kCombatChef, exec);

    EXPECT_FALSE(state.getObject(fake).attached_to.has_value())
        << "Weaponmaster should ignore gear without 'Equipment' tag";
    EXPECT_EQ(state.getObject(wm).attachment_might_bonus, 0);
}

}  // namespace
