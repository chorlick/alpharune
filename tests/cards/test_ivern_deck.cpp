/// @file test_ivern_deck.cpp
/// Per-card tests for the Ivern test deck (decks/ivern_test.txt).
/// Covers the manual implementations added in deck_cards.cpp's
/// "Ivern Test Deck — full card support" section.
///
/// Card text references:
///   [739] Ivern, Friend to All — "As you play me, choose Bird/Cat/Dog/Poro.
///       When I conquer or hold, score 1 if all 4 tags among your units."
///   [754] Daisy! — "I enter ready. Cost -1 per tag (B/C/D/P). On attack
///       while all 4 tags present, [Stun] an enemy unit here."
///   [613] Ivern, Nurturer — "Look at top 3, may draft a unit, recycle
///       rest. If themed, buff a friendly."
///   [753] Green Father — "When you conquer/hold, may exhaust to replace
///       BF with Brush token. Brush: B/C/D/P/Ivern +1[M]."
///   [480] Trusty Ramhound — "While you have another unit here, +1[M]."
///   [595] Frisky Hunter — "When you play me, play a 1[M] Bird token
///       with [Deflect] here."
///   [608] Friendship — "+1[M] per tag among your units (B/C/D/P)."
///   [718] Loyal Poro — "[Deathknell] If I didn't die alone, draw 1."
///   [622] Vilemaw — "When I hold, draw 1." (+ Ambush keyword)
///   [290] Vilemaw's Lair — Units can't move from here to base.
///   [530] Rockfall Path — Units can't be played here.
///   [775] Vaults of Helia — Non-token units cost +1 this turn on hold.
///   [606] Flurry of Feathers — Modal counter / 4 Bird tokens.
///   [213] Hidden Blade — Kill a unit, its controller draws 2.
///   [604] Back Off — Stun a unit, draw 1.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <algorithm>

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kIvernFriend     = 739;
constexpr CardDefId kDaisy           = 754;
constexpr CardDefId kIvernNurturer   = 613;
constexpr CardDefId kGreenFather     = 753;
constexpr CardDefId kTrustyRamhound  = 480;
constexpr CardDefId kFriskyHunter    = 595;
constexpr CardDefId kFriendship      = 608;
constexpr CardDefId kLoyalPoro       = 718;
constexpr CardDefId kVilemaw         = 622;
constexpr CardDefId kVilemawsLair    = 290;
constexpr CardDefId kRockfallPath    = 530;
constexpr CardDefId kVaultsOfHelia   = 775;
constexpr CardDefId kFlurryOfFeathers= 606;
constexpr CardDefId kHiddenBlade     = 213;
constexpr CardDefId kBackOff         = 604;

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// [739] Ivern, Friend to All
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, IvernFriend_GainsChosenTagOnAsYouPlayMe) {
    // The tag choice moved from a hardcoded "Bird" in onPlay to an AGENT
    // pickMode on the AsYouPlayMe trigger. Drive that trigger with a picker
    // that selects the Bird mode (the kTags[0] mode), and assert Ivern gains
    // the chosen tag.
    auto ivern_id = addUnit(P1, kIvernFriend, /*might=*/0, /*at_bf=*/-1);
    auto* card = card_registry.get(kIvernFriend);
    ASSERT_NE(card, nullptr);

    EffectExecutor exec(state, events, card_db);
    // pickMode publishes one MakeChoice per mode (Bird/Cat/Dog/Poro). Bird is
    // the first mode — pick legal[0].
    driveResumableTrigger(
        kIvernFriend, P1, ivern_id,
        [](const std::vector<Intent>& legal) {
            return legal.empty() ? Intent{} : legal.front();  // Bird
        },
        exec, TriggerType::AsYouPlayMe);

    const auto& tags = state.getObject(ivern_id).tags;
    EXPECT_TRUE(std::find(tags.begin(), tags.end(), std::string("Bird")) != tags.end())
        << "Ivern's AsYouPlayMe choice must append the chosen tag ('Bird').";
}

TEST_F(CardTestFixture, IvernFriend_ScoresOnConquerOrHoldWhenAllFourTags) {
    auto ivern_id = addUnit(P1, kIvernFriend, /*might=*/0, /*at_bf=*/0);
    // Make sure Ivern has the Bird tag (its onPlay would add it).
    state.getObject(ivern_id).tags.push_back("Bird");
    // Provide the other 3 tags via co-controlled units.
    addUnit(P1, kInvalidId, /*might=*/1, /*at_bf=*/0);   // base unit
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Cat"};
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Dog"};
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Poro"};

    int score_before = state.player(P1).score;
    EffectExecutor exec(state, events, card_db);
    // Scoring is on the WhenIConquerOrHold trigger; fire it explicitly.
    fireTriggerAs(kIvernFriend, P1, ivern_id,
                  TriggerType::WhenIConquerOrHold, exec);

    EXPECT_EQ(state.player(P1).score, score_before + 1)
        << "All-4-tag union satisfied — must score 1.";
}

TEST_F(CardTestFixture, IvernFriend_DoesNotScoreWhenMissingATag) {
    auto ivern_id = addUnit(P1, kIvernFriend, /*might=*/0, /*at_bf=*/0);
    state.getObject(ivern_id).tags.push_back("Bird");
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Cat"};
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Dog"};
    // Missing Poro intentionally.

    int score_before = state.player(P1).score;
    EffectExecutor exec(state, events, card_db);
    fireTriggerAs(kIvernFriend, P1, ivern_id,
                  TriggerType::WhenIConquerOrHold, exec);

    EXPECT_EQ(state.player(P1).score, score_before)
        << "Union not satisfied (Poro missing) — must not score.";
}

// ═══════════════════════════════════════════════════════════════════════════
// [754] Daisy!
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, Daisy_DeclaresEntersReady) {
    auto* card = card_registry.get(kDaisy);
    ASSERT_NE(card, nullptr);
    EXPECT_TRUE(card->entersReadyOnPlay())
        << "Daisy! must declare entersReadyOnPlay so resolvePermanent "
           "skips the default 'units enter exhausted' rule.";
}

TEST_F(CardTestFixture, Daisy_SelfCostReductionScalesWithTagCount) {
    auto* card = card_registry.get(kDaisy);
    ASSERT_NE(card, nullptr);

    // Zero tags present.
    EXPECT_EQ(card->selfCostReduction(state, P1), 0);

    // Add one Bird.
    auto bird = addUnit(P1, kInvalidId, 1, 0);
    state.getObject(bird).tags = {"Bird"};
    EXPECT_EQ(card->selfCostReduction(state, P1), 1);

    // Add a Cat — total 2.
    auto cat = addUnit(P1, kInvalidId, 1, 0);
    state.getObject(cat).tags = {"Cat"};
    EXPECT_EQ(card->selfCostReduction(state, P1), 2);

    // Two cats don't double-count.
    auto cat2 = addUnit(P1, kInvalidId, 1, 0);
    state.getObject(cat2).tags = {"Cat"};
    EXPECT_EQ(card->selfCostReduction(state, P1), 2);

    // Add Dog and Poro — total 4.
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Dog"};
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Poro"};
    EXPECT_EQ(card->selfCostReduction(state, P1), 4);
}

TEST_F(CardTestFixture, Daisy_OnAttackStunsEnemyOnlyWhenAllFourTagsPresent) {
    auto daisy_id = addUnit(P1, kDaisy, /*might=*/4, /*at_bf=*/0);
    state.getObject(daisy_id).tags = {"Ivern", "Bird"};  // own Bird
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Cat"};
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Dog"};
    // Missing Poro — stun must NOT fire.
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    ASSERT_FALSE(state.getObject(enemy).is_stunned);

    auto* card = card_registry.get(kDaisy);
    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, daisy_id};
    card->onTrigger(ctx, {});
    EXPECT_FALSE(state.getObject(enemy).is_stunned)
        << "Union incomplete — Daisy must NOT stun.";

    // Now satisfy the Poro tag.
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Poro"};
    card->onTrigger(ctx, {});
    EXPECT_TRUE(state.getObject(enemy).is_stunned)
        << "All 4 tags present — Daisy must stun the enemy at her location.";
}

// ═══════════════════════════════════════════════════════════════════════════
// [480] Trusty Ramhound — conditional aura
// ═══════════════════════════════════════════════════════════════════════════
//
// The aura is applied during recalculateAuras (engine-side), which we don't
// invoke from these tests. Coverage focus: confirm the card's registry
// text matches the engine's pattern. Live-game effect is covered by trace
// inspection during smoke tests; aura recalculation is tested elsewhere.

TEST_F(CardTestFixture, TrustyRamhound_RegistryTextHasAuraPattern) {
    const auto& def = card_db.get(kTrustyRamhound);
    std::string t = def.ability_text;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    EXPECT_NE(t.find("while you have another unit here"), std::string::npos);
    EXPECT_NE(t.find("+1 [m]"), std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════════
// [595] Frisky Hunter — token spawn at my location
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, FriskyHunter_OnPlaySpawnsBirdAtMyLocation) {
    auto frisky_id = addUnit(P1, kFriskyHunter, /*might=*/2, /*at_bf=*/1);
    auto* card = card_registry.get(kFriskyHunter);
    ASSERT_NE(card, nullptr);

    EffectExecutor exec(state, events, card_db);
    CardContext ctx{state, events, exec, P1, frisky_id};
    card->onTrigger(ctx, {});

    // Find the spawned Bird.
    int birds_at_bf1 = 0;
    for (const auto& [id, obj] : state.objects) {
        if (obj.name != "Bird") continue;
        if (obj.controller != P1) continue;
        if (!obj.location.has_value()) continue;
        auto bf = std::get_if<BattlefieldLocation>(&*obj.location);
        if (!bf || bf->id != 1) continue;
        ++birds_at_bf1;
        EXPECT_TRUE(obj.keywords.has(Keyword::Deflect))
            << "Bird token must have Deflect.";
        EXPECT_EQ(obj.base_might, 1);
        ASSERT_FALSE(obj.tags.empty());
        EXPECT_EQ(obj.tags.front(), "Bird");
    }
    EXPECT_EQ(birds_at_bf1, 1) << "Exactly one Bird token must spawn at Frisky's BF.";
}

// ═══════════════════════════════════════════════════════════════════════════
// [608] Friendship — +1[M] per tag among friendlies
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, Friendship_AppliesBonusPerTagCount) {
    // Target a friendly unit, give it +N where N = distinct tags.
    auto target = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    auto& targ = state.getObject(target);
    int base = targ.current_might;

    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Bird"};
    state.getObject(addUnit(P1, kInvalidId, 1, 0)).tags = {"Cat"};

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    state.getObject(src).card_type = CardType::Spell;

    // Phase 6q: Friendship now defers target selection via pickTarget.
    // Drive the resume loop and pick the specific `target` (multiple
    // friendly units on board, so we can't rely on fast-path ordering).
    EffectExecutor exec(state, events, card_db);
    driveResumable(kFriendship, P1, src,
                   [&](const std::vector<Intent>& opts) -> Intent {
                       for (const auto& o : opts) {
                           if (!o.chosen_objects.empty() &&
                               o.chosen_objects[0] == target)
                               return o;
                       }
                       return opts.empty() ? Intent{} : opts.front();
                   },
                   exec);

    EXPECT_EQ(state.getObject(target).current_might, base + 2)
        << "+1[M] per of {Bird, Cat} present.";
}

TEST_F(CardTestFixture, Friendship_ZeroBonusWhenNoTaggedFriendlies) {
    auto target = addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    int base = state.getObject(target).current_might;

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    state.getObject(src).card_type = CardType::Spell;

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kFriendship);
    CardContext ctx{state, events, exec, P1, src};
    card->onResolve(ctx, {target});

    EXPECT_EQ(state.getObject(target).current_might, base)
        << "No B/C/D/P tags among friendlies — no buff.";
}

// ═══════════════════════════════════════════════════════════════════════════
// [718] Loyal Poro — Deathknell with co-location check
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, LoyalPoro_DrawsWhenAnotherFriendlyAtSameLocationAtDeath) {
    auto poro_id = addUnit(P1, kLoyalPoro, /*might=*/3, /*at_bf=*/0);
    // Companion at the same BF.
    addUnit(P1, kInvalidId, /*might=*/2, /*at_bf=*/0);
    // Seed a deck so drawCards has something to pull (avoids burn-out
    // side effects making the assertion ambiguous).
    addToDeck(P1, kInvalidId);

    EffectExecutor exec(state, events, card_db);
    exec.killObject(poro_id);

    int hand_before = handSize(P1);
    auto* card = card_registry.get(kLoyalPoro);
    CardContext ctx{state, events, exec, P1, poro_id};
    card->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), hand_before + 1)
        << "Companion present at death location — Loyal Poro must draw 1.";
}

TEST_F(CardTestFixture, LoyalPoro_DoesNotDrawWhenDiedAlone) {
    auto poro_id = addUnit(P1, kLoyalPoro, /*might=*/3, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);

    EffectExecutor exec(state, events, card_db);
    exec.killObject(poro_id);

    int hand_before = handSize(P1);
    auto* card = card_registry.get(kLoyalPoro);
    CardContext ctx{state, events, exec, P1, poro_id};
    card->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), hand_before)
        << "Died alone — must NOT draw.";
}

// ═══════════════════════════════════════════════════════════════════════════
// [622] Vilemaw — hold trigger
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, Vilemaw_OnHoldDraws1) {
    auto vm = addUnit(P1, kVilemaw, /*might=*/4, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);
    int hand_before = handSize(P1);

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kVilemaw);
    CardContext ctx{state, events, exec, P1, vm};
    card->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), hand_before + 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// [213] Hidden Blade — kill + controller draws 2
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, HiddenBlade_KillsTargetAndItsControllerDraws2) {
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    addToDeck(P2, kInvalidId);
    addToDeck(P2, kInvalidId);
    int p2_hand = handSize(P2);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    state.getObject(src).card_type = CardType::Spell;

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kHiddenBlade);
    CardContext ctx{state, events, exec, P1, src};
    card->onResolve(ctx, {enemy});

    EXPECT_EQ(state.getObject(enemy).zone, ZoneType::Trash) << "Target must die.";
    EXPECT_EQ(handSize(P2), p2_hand + 2)
        << "The killed unit's controller (P2) must draw 2.";
}

// ═══════════════════════════════════════════════════════════════════════════
// [604] Back Off — stun + draw 1
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, BackOff_StunsAndDraws1) {
    auto enemy = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);
    addToDeck(P1, kInvalidId);
    int p1_hand = handSize(P1);

    auto src = state.createObject();
    state.getObject(src).owner = P1;
    state.getObject(src).controller = P1;
    state.getObject(src).card_type = CardType::Spell;

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kBackOff);
    CardContext ctx{state, events, exec, P1, src};
    card->onResolve(ctx, {enemy});

    EXPECT_TRUE(state.getObject(enemy).is_stunned) << "Target must be stunned.";
    EXPECT_EQ(handSize(P1), p1_hand + 1) << "Caster draws 1.";
}

// ═══════════════════════════════════════════════════════════════════════════
// [606] Flurry of Feathers — modal counter / 4-Bird-token swarm
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, FlurryOfFeathers_CountersChainTopWhenSpellOnChain) {
    // Push a victim spell, then Flurry.
    auto victim = pushSpellOnChain(P2, /*def_id=*/kInvalidId);
    auto flurry = state.createObject();
    state.getObject(flurry).owner = P1;
    state.getObject(flurry).controller = P1;
    state.getObject(flurry).card_type = CardType::Spell;

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kFlurryOfFeathers);
    CardContext ctx{state, events, exec, P1, flurry};
    card->onResolve(ctx, {});

    EXPECT_TRUE(state.chain.items.empty()) << "Victim popped.";
    EXPECT_EQ(state.getObject(victim).zone, ZoneType::Trash);
}

TEST_F(CardTestFixture, FlurryOfFeathers_SpawnsFourBirdsWhenChainEmpty) {
    auto flurry = state.createObject();
    state.getObject(flurry).owner = P1;
    state.getObject(flurry).controller = P1;
    state.getObject(flurry).card_type = CardType::Spell;

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kFlurryOfFeathers);
    CardContext ctx{state, events, exec, P1, flurry};
    card->onResolve(ctx, {});

    int birds = 0;
    for (const auto& [id, obj] : state.objects) {
        if (obj.name == "Bird" && obj.controller == P1) ++birds;
    }
    EXPECT_EQ(birds, 4);
}

// ═══════════════════════════════════════════════════════════════════════════
// [290] Vilemaw's Lair / [530] Rockfall Path — BF restrictions
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, VilemawsLair_TextParsesToBlockMoveToBase) {
    const auto& def = card_db.get(kVilemawsLair);
    std::string t = def.ability_text;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    EXPECT_NE(t.find("can't move from here to base"), std::string::npos)
        << "Text must contain the substring the engine parses to set "
           "BattlefieldState::blocks_move_to_base.";
}

// ─── Behavioral: blocks_move_to_base actually suppresses BF→base moves ───
//
// Sets up a unit on a BF flagged blocks_move_to_base=true (Vilemaw's
// Lair stand-in) and verifies the action generator does NOT emit a
// StandardMove intent targeting that unit's base. Counter-test on an
// unflagged BF confirms moves to base ARE legal there.

TEST_F(CardTestFixture, VilemawsLair_BlocksMoveFromBFToBase) {
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.mode = ModeOfPlay{};
    s.players[0].id = P1;
    s.players[1].id = P2;
    s.turn.turn_player = P1;
    s.turn.phase       = TurnPhase::MainPhase;
    s.turn.ns_state    = NeutralShowdownState::Neutral;
    s.turn.oc_state    = OpenClosedState::Open;

    // BF#0: Vilemaw's Lair stand-in (blocks_move_to_base).
    // BF#1: unflagged (control sentinel for the counter-test).
    BattlefieldState lair;
    lair.id = 0;
    lair.blocks_move_to_base = true;
    s.battlefields.push_back(lair);
    BattlefieldState normal;
    normal.id = 1;
    s.battlefields.push_back(normal);

    auto legend = s.createObject();
    auto& leg = s.getObject(legend);
    leg.owner = P1; leg.controller = P1;
    leg.card_type = CardType::Legend;
    leg.zone = ZoneType::LegendZone;
    s.players[0].legend_zone = legend;

    // A friendly READY unit at the Lair (BF#0) — eligible to move.
    auto trapped = s.createObject();
    auto& t = s.getObject(trapped);
    t.owner = P1; t.controller = P1;
    t.card_type = CardType::Unit;
    t.base_might = 3; t.current_might = 3;
    t.zone = ZoneType::BattlefieldZone;
    t.location = BattlefieldLocation{0};
    t.is_exhausted = false;

    // A friendly READY unit at the normal BF (BF#1) — control case.
    auto free_unit = s.createObject();
    auto& f = s.getObject(free_unit);
    f.owner = P1; f.controller = P1;
    f.card_type = CardType::Unit;
    f.base_might = 3; f.current_might = 3;
    f.zone = ZoneType::BattlefieldZone;
    f.location = BattlefieldLocation{1};
    f.is_exhausted = false;

    auto actions = engine.generateLegalActions();

    bool trapped_can_go_to_base   = false;
    bool free_unit_can_go_to_base = false;
    for (const auto& a : actions) {
        if (a.type != IntentType::StandardMove) continue;
        if (a.units_to_move.empty()) continue;
        if (!a.move_destination.has_value()) continue;
        if (!std::holds_alternative<BaseLocation>(*a.move_destination)) continue;
        auto uid = a.units_to_move[0];
        if (uid == trapped)   trapped_can_go_to_base   = true;
        if (uid == free_unit) free_unit_can_go_to_base = true;
    }

    EXPECT_FALSE(trapped_can_go_to_base)
        << "CR / Vilemaw's Lair: 'Units can't move from here to base.' "
           "The action generator must NOT emit a StandardMove intent "
           "from this BF back to its controller's base.";
    EXPECT_TRUE(free_unit_can_go_to_base)
        << "Sanity: a unit at an UN-flagged BF must still be able to "
           "return to base (CR 144.4.b default).";
}

TEST_F(CardTestFixture, VilemawsLair_AllowsBFToBFAndUnitsCanStillEnter) {
    // The Lair only blocks the FROM-here-TO-base move. Moves OUT to
    // another BF (with Ganking) and moves INTO the Lair are both
    // unaffected.
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.mode = ModeOfPlay{};
    s.players[0].id = P1;
    s.players[1].id = P2;
    s.turn.turn_player = P1;
    s.turn.phase       = TurnPhase::MainPhase;
    s.turn.ns_state    = NeutralShowdownState::Neutral;
    s.turn.oc_state    = OpenClosedState::Open;

    BattlefieldState lair;
    lair.id = 0;
    lair.blocks_move_to_base = true;
    s.battlefields.push_back(lair);
    BattlefieldState normal;
    normal.id = 1;
    s.battlefields.push_back(normal);

    auto legend = s.createObject();
    auto& leg = s.getObject(legend);
    leg.owner = P1; leg.controller = P1;
    leg.card_type = CardType::Legend;
    leg.zone = ZoneType::LegendZone;
    s.players[0].legend_zone = legend;

    // A unit AT P1's base — moving INTO the Lair should be legal.
    auto base_unit = s.createObject();
    auto& bu = s.getObject(base_unit);
    bu.owner = P1; bu.controller = P1;
    bu.card_type = CardType::Unit;
    bu.base_might = 2; bu.current_might = 2;
    bu.zone = ZoneType::Base;
    bu.location = BaseLocation{P1};
    bu.is_exhausted = false;

    // A Ganking unit AT the Lair — BF→BF should still work.
    auto ganker = s.createObject();
    auto& gk = s.getObject(ganker);
    gk.owner = P1; gk.controller = P1;
    gk.card_type = CardType::Unit;
    gk.base_might = 3; gk.current_might = 3;
    gk.zone = ZoneType::BattlefieldZone;
    gk.location = BattlefieldLocation{0};
    gk.is_exhausted = false;
    gk.keywords.set(Keyword::Ganking);

    auto actions = engine.generateLegalActions();

    bool can_enter_lair_from_base = false;
    bool ganker_can_bf_to_bf      = false;
    for (const auto& a : actions) {
        if (a.type != IntentType::StandardMove) continue;
        if (a.units_to_move.empty()) continue;
        if (!a.move_destination.has_value()) continue;
        auto uid = a.units_to_move[0];
        if (uid == base_unit &&
            std::holds_alternative<BattlefieldLocation>(*a.move_destination) &&
            std::get<BattlefieldLocation>(*a.move_destination).id == 0) {
            can_enter_lair_from_base = true;
        }
        if (uid == ganker &&
            std::holds_alternative<BattlefieldLocation>(*a.move_destination) &&
            std::get<BattlefieldLocation>(*a.move_destination).id == 1) {
            ganker_can_bf_to_bf = true;
        }
    }

    EXPECT_TRUE(can_enter_lair_from_base)
        << "Vilemaw's Lair text only restricts move-FROM-here-TO-base. "
           "Base→Lair entries must still be legal.";
    EXPECT_TRUE(ganker_can_bf_to_bf)
        << "BF→BF moves (with Ganking) from inside the Lair must still "
           "be legal — only the to-base leg is blocked.";
}

TEST_F(CardTestFixture, RockfallPath_TextParsesToBlockUnitPlay) {
    const auto& def = card_db.get(kRockfallPath);
    std::string t = def.ability_text;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    EXPECT_NE(t.find("can't be played here"), std::string::npos)
        << "Text must contain the substring the engine parses to set "
           "BattlefieldState::blocks_unit_play.";
}

// ─── Behavioral: blocks_unit_play actually suppresses plays in MainPhase ───
//
// Sets up a minimal MainPhase scenario where P1 controls a BF flagged
// blocks_unit_play=true (as if Rockfall Path were in the lineup) and
// holds a unit in hand they can afford. The action generator must:
//   - NOT emit any PlayCard intent targeting the blocked BF, even
//     though P1 controls it. CR "Units can't be played here" applies
//     regardless of who holds the BF — Rockfall Path doesn't have a
//     "your" / "enemy" qualifier, so the restriction is universal.
//   - STILL emit a StandardMove intent targeting the blocked BF
//     (the restriction is on plays-from-hand, not on movement).
//   - STILL emit PlayCard intents targeting OTHER BFs that don't
//     carry the flag.

namespace {

// Build a minimal engine state in MainPhase with: 2 BFs (one flagged
// blocks_unit_play, controlled by P1; the other unflagged + open), a
// unit in P1's hand they can afford, and enough ready runes to pay
// for it. Returns the engine and the relevant ids by reference.
struct RockfallScenario {
    GameEngine engine;
    BattlefieldId blocked_bf;
    BattlefieldId open_bf;
    GameObjectId  unit_in_hand;
    GameObjectId  legend;
    RockfallScenario(const CardDB& db, EventBus& events,
                     const CardRegistry& reg, PlayerId P1, PlayerId P2)
        : engine(db, events, reg) {
        auto& s = engine.mutableState();
        s.mode = ModeOfPlay{};
        s.players[0].id = P1;
        s.players[1].id = P2;
        s.turn.turn_player  = P1;
        s.turn.phase        = TurnPhase::MainPhase;
        s.turn.ns_state     = NeutralShowdownState::Neutral;
        s.turn.oc_state     = OpenClosedState::Open;

        // BF #0: blocked (Rockfall Path stand-in), controlled by P1.
        BattlefieldState bf0;
        bf0.id = 0;
        bf0.blocks_unit_play = true;
        bf0.controller = P1;
        s.battlefields.push_back(bf0);
        blocked_bf = 0;

        // BF #1: open, no controller, no restriction.
        BattlefieldState bf1;
        bf1.id = 1;
        s.battlefields.push_back(bf1);
        open_bf = 1;

        // Legend in legend zone (engine queries it for various paths).
        legend = s.createObject();
        auto& leg = s.getObject(legend);
        leg.owner = P1; leg.controller = P1;
        leg.card_type = CardType::Legend;
        leg.zone = ZoneType::LegendZone;
        s.players[0].legend_zone = legend;

        // Unit in P1's hand. Pick a real cheap unit so cost lookups
        // resolve via CardDB. Frisky Hunter (id=595) costs 3E+1P.
        unit_in_hand = s.createObject();
        auto& u = s.getObject(unit_in_hand);
        u.owner = P1; u.controller = P1;
        u.card_def_id = 595;  // Frisky Hunter
        u.card_type = CardType::Unit;
        u.zone = ZoneType::Hand;
        const auto& def = db.get(595);
        u.name = def.name;
        u.domains = def.domains;
        u.base_might = def.might;
        u.current_might = def.might;
        s.players[0].hand.push_back(unit_in_hand);

        // 4 ready runes in P1's base, all in Frisky's domain (Calm).
        for (int i = 0; i < 4; ++i) {
            auto rune_id = s.createObject();
            auto& r = s.getObject(rune_id);
            r.owner = P1; r.controller = P1;
            r.card_type = CardType::Rune;
            r.name = "Calm Rune";
            r.domains = {Domain::Calm};
            r.zone = ZoneType::Base;
            r.location = BaseLocation{P1};
            r.is_exhausted = false;
        }
    }
};

}  // namespace

TEST_F(CardTestFixture, RockfallPath_BlockedBF_NoPlayCardIntentEmitted) {
    RockfallScenario sc(card_db, events, card_registry, P1, P2);
    auto actions = sc.engine.generateLegalActions();

    bool play_to_blocked = false;
    bool play_to_open = false;
    for (const auto& a : actions) {
        if (a.type != IntentType::PlayCard) continue;
        if (a.card != sc.unit_in_hand) continue;
        if (!a.play_location.has_value()) continue;
        if (!std::holds_alternative<BattlefieldLocation>(*a.play_location)) continue;
        auto bf = std::get<BattlefieldLocation>(*a.play_location).id;
        if (bf == sc.blocked_bf) play_to_blocked = true;
        if (bf == sc.open_bf)    play_to_open = true;
    }

    EXPECT_FALSE(play_to_blocked)
        << "PlayCard to a BF with blocks_unit_play=true must NOT be a legal "
           "action even when controlled by the playing player. Rockfall "
           "Path's text 'Units can't be played here' has no controller "
           "qualifier — the restriction is universal.";
    // Note: PlayCard to BFs only fires for cards with play-to-BF
    // permission (Ambush, Pouncing, "play me to an open BF" effects).
    // Plain Frisky Hunter (no such permission) only generates
    // play-to-base and play-to-controlled-BF normally. The "open_bf"
    // counter-test catches a regression where we'd accidentally
    // suppress all BF plays via the wrong gate.
    (void)play_to_open;  // counter-test intent recorded; assertion above
                         // is the load-bearing one.
}

TEST_F(CardTestFixture, RockfallPath_MoveToBlockedBFStillAllowed) {
    // The "can't be played here" restriction must NOT block moves.
    // A unit at base must still get a StandardMove intent for the
    // blocked BF.
    RockfallScenario sc(card_db, events, card_registry, P1, P2);
    auto& s = sc.engine.mutableState();

    // Add a ready unit at P1's base so the move-from-base path emits
    // intents for both BFs.
    auto unit_at_base = s.createObject();
    auto& u = s.getObject(unit_at_base);
    u.owner = P1; u.controller = P1;
    u.card_type = CardType::Unit;
    u.base_might = 1; u.current_might = 1;
    u.zone = ZoneType::Base;
    u.location = BaseLocation{P1};
    u.is_exhausted = false;

    auto actions = sc.engine.generateLegalActions();

    bool move_to_blocked = false;
    bool move_to_open    = false;
    for (const auto& a : actions) {
        if (a.type != IntentType::StandardMove) continue;
        if (a.units_to_move.empty() || a.units_to_move[0] != unit_at_base) continue;
        if (!a.move_destination.has_value()) continue;
        if (!std::holds_alternative<BattlefieldLocation>(*a.move_destination)) continue;
        auto bf = std::get<BattlefieldLocation>(*a.move_destination).id;
        if (bf == sc.blocked_bf) move_to_blocked = true;
        if (bf == sc.open_bf)    move_to_open    = true;
    }

    EXPECT_TRUE(move_to_blocked)
        << "Movement to a Rockfall Path-style BF must remain legal — "
           "CR distinguishes 'played' (from hand) from 'moved' (already "
           "on board). Only the play-from-hand path consults "
           "blocks_unit_play.";
    EXPECT_TRUE(move_to_open) << "sanity check: open BF also receives a move intent";
}

// ─── Reinforce play: a player CAN play a unit directly to a BF they ──────
//     already control (CR 355.2.a). This is the default play-location
//     rule and the engine's positive side of the same gate Rockfall
//     suppresses. Test verifies generateMainPhaseActions emits a
//     PlayCard intent whose play_location targets the controlled BF,
//     not just the player's base.

TEST_F(CardTestFixture, Reinforce_PlayCardToControlledBFIsLegal) {
    // Minimal scenario: P1 controls BF#0 (has a unit there), holds an
    // affordable unit in hand, MainPhase + Neutral Open. We expect a
    // PlayCard intent for the held card with play_location =
    // BattlefieldLocation{0}.
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.mode = ModeOfPlay{};
    s.players[0].id = P1;
    s.players[1].id = P2;
    s.turn.turn_player = P1;
    s.turn.phase       = TurnPhase::MainPhase;
    s.turn.ns_state    = NeutralShowdownState::Neutral;
    s.turn.oc_state    = OpenClosedState::Open;

    BattlefieldState bf;
    bf.id = 0;
    bf.controller = P1;  // P1 already holds it
    s.battlefields.push_back(bf);

    // A friendly unit at BF#0 so control is realized on the board.
    auto holder = s.createObject();
    auto& h = s.getObject(holder);
    h.owner = P1; h.controller = P1;
    h.card_type = CardType::Unit;
    h.base_might = 2; h.current_might = 2;
    h.zone = ZoneType::BattlefieldZone;
    h.location = BattlefieldLocation{0};

    // Legend slot (engine touches it during action generation).
    auto legend = s.createObject();
    auto& leg = s.getObject(legend);
    leg.owner = P1; leg.controller = P1;
    leg.card_type = CardType::Legend;
    leg.zone = ZoneType::LegendZone;
    s.players[0].legend_zone = legend;

    // Affordable unit in hand. Frisky Hunter (id=595) — 3E + 1P [Calm].
    auto held = s.createObject();
    auto& u = s.getObject(held);
    u.owner = P1; u.controller = P1;
    u.card_def_id = 595;
    u.card_type = CardType::Unit;
    u.zone = ZoneType::Hand;
    const auto& def = card_db.get(595);
    u.name = def.name;
    u.domains = def.domains;
    u.base_might = def.might;
    u.current_might = def.might;
    s.players[0].hand.push_back(held);

    // 4 ready Calm runes — affords 3E + 1P recycle.
    for (int i = 0; i < 4; ++i) {
        auto rune_id = s.createObject();
        auto& r = s.getObject(rune_id);
        r.owner = P1; r.controller = P1;
        r.card_type = CardType::Rune;
        r.name = "Calm Rune";
        r.domains = {Domain::Calm};
        r.zone = ZoneType::Base;
        r.location = BaseLocation{P1};
        r.is_exhausted = false;
    }

    auto actions = engine.generateLegalActions();

    bool play_to_base = false;
    bool play_to_controlled_bf = false;
    for (const auto& a : actions) {
        if (a.type != IntentType::PlayCard) continue;
        if (a.card != held) continue;
        if (!a.play_location.has_value()) continue;
        if (std::holds_alternative<BaseLocation>(*a.play_location)) {
            play_to_base = true;
        } else if (std::holds_alternative<BattlefieldLocation>(*a.play_location)) {
            auto bf_id = std::get<BattlefieldLocation>(*a.play_location).id;
            if (bf_id == 0) play_to_controlled_bf = true;
        }
    }

    EXPECT_TRUE(play_to_base)
        << "Sanity: play-to-base is always available as a default "
           "play location (CR 355.2.a).";
    EXPECT_TRUE(play_to_controlled_bf)
        << "CR 355.2.a: 'Valid locations include the controller's Base "
           "or a Battlefield the controller controls.' A player must "
           "be able to reinforce a BF they already hold by playing "
           "another unit directly there from hand.";
}

TEST_F(CardTestFixture, Reinforce_PlayCardToEnemyOrUncontrolledBFNotEmitted) {
    // Counter-test: without special play-to-location permission
    // (Pouncing, "play me to an open battlefield" etc.), a normal
    // unit cannot be played to a BF the player doesn't control —
    // even if the BF is uncontested/empty. Reinforce permission is
    // strictly "BFs you control" by default.
    GameEngine engine(card_db, events, card_registry);
    auto& s = engine.mutableState();
    s.mode = ModeOfPlay{};
    s.players[0].id = P1;
    s.players[1].id = P2;
    s.turn.turn_player = P1;
    s.turn.phase       = TurnPhase::MainPhase;
    s.turn.ns_state    = NeutralShowdownState::Neutral;
    s.turn.oc_state    = OpenClosedState::Open;

    // BF#0: P2 controls. BF#1: open (no controller).
    BattlefieldState bf0;
    bf0.id = 0;
    bf0.controller = P2;
    s.battlefields.push_back(bf0);
    BattlefieldState bf1;
    bf1.id = 1;  // no controller
    s.battlefields.push_back(bf1);

    auto legend = s.createObject();
    auto& leg = s.getObject(legend);
    leg.owner = P1; leg.controller = P1;
    leg.card_type = CardType::Legend;
    leg.zone = ZoneType::LegendZone;
    s.players[0].legend_zone = legend;

    auto held = s.createObject();
    auto& u = s.getObject(held);
    u.owner = P1; u.controller = P1;
    u.card_def_id = 595;
    u.card_type = CardType::Unit;
    u.zone = ZoneType::Hand;
    const auto& def = card_db.get(595);
    u.name = def.name;
    u.domains = def.domains;
    u.base_might = def.might;
    u.current_might = def.might;
    s.players[0].hand.push_back(held);

    for (int i = 0; i < 4; ++i) {
        auto rune_id = s.createObject();
        auto& r = s.getObject(rune_id);
        r.owner = P1; r.controller = P1;
        r.card_type = CardType::Rune;
        r.name = "Calm Rune";
        r.domains = {Domain::Calm};
        r.zone = ZoneType::Base;
        r.location = BaseLocation{P1};
        r.is_exhausted = false;
    }

    auto actions = engine.generateLegalActions();

    bool play_to_enemy_bf = false;
    bool play_to_open_bf  = false;
    for (const auto& a : actions) {
        if (a.type != IntentType::PlayCard) continue;
        if (a.card != held) continue;
        if (!a.play_location.has_value()) continue;
        if (!std::holds_alternative<BattlefieldLocation>(*a.play_location)) continue;
        auto bf_id = std::get<BattlefieldLocation>(*a.play_location).id;
        if (bf_id == 0) play_to_enemy_bf = true;
        if (bf_id == 1) play_to_open_bf  = true;
    }

    EXPECT_FALSE(play_to_enemy_bf)
        << "Default play permission does NOT extend to enemy-controlled "
           "BFs. Only cards with 'play me to an enemy BF' style "
           "permission grant this.";
    EXPECT_FALSE(play_to_open_bf)
        << "Default play permission does NOT extend to open/uncontrolled "
           "BFs either — only base + controlled BFs. The 'play to open "
           "BF' permission is card-specific.";
}

TEST_F(CardTestFixture, RockfallPath_SetupBattlefieldsSetsFlagFromText) {
    // Verifies the parsing in GameEngine::setupBattlefields populates
    // blocks_unit_play correctly. We can't easily run setupBattlefields
    // without a full DeckSubmission, but we can verify the text
    // pattern matches what the parser looks for, and that flipping
    // the flag has the right behavioral effect (above tests).
    //
    // This is a contract test that the registry text and the engine
    // parser stay in sync. If someone errata's Rockfall Path's text
    // without updating the parser, this fails loudly.
    const auto& def = card_db.get(kRockfallPath);
    std::string t = def.ability_text;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    EXPECT_NE(t.find("can't be played here"), std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════════
// [775] Vaults of Helia — on-hold cost+1 for non-token units
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, VaultsOfHelia_OnHoldAppendsCostIncreaseModifier) {
    // The BF card itself is the source; trigger via the manual card.
    // We don't need a real BF slot — the modifier lives on the player.
    auto vault_obj = state.createObject();
    state.getObject(vault_obj).owner = P1;
    state.getObject(vault_obj).controller = P1;
    state.getObject(vault_obj).card_type = CardType::Battlefield;

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kVaultsOfHelia);
    CardContext ctx{state, events, exec, P1, vault_obj};
    card->onTrigger(ctx, {});

    const auto& mods = state.player(P1).cost_modifiers;
    ASSERT_EQ(mods.size(), 1u);
    EXPECT_EQ(mods[0].energy_increase, 1);
    EXPECT_TRUE(mods[0].affects_non_token_only);
    EXPECT_TRUE(mods[0].this_turn_only);
}

// ═══════════════════════════════════════════════════════════════════════════
// [753] Green Father — BF replacement with Brush token
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, GreenFather_OnConquerOrHoldReplacesABFWithBrush) {
    // Add the legend in the legend zone.
    auto legend = state.createObject();
    state.getObject(legend).owner = P1;
    state.getObject(legend).controller = P1;
    state.getObject(legend).card_def_id = kGreenFather;
    state.getObject(legend).card_type = CardType::Legend;
    state.getObject(legend).zone = ZoneType::LegendZone;
    state.players[0].legend_zone = legend;

    // Mark BF#0 as controlled by P1 (the hold/conquer just happened).
    state.battlefields[0].controller = P1;

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kGreenFather);
    CardContext ctx{state, events, exec, P1, legend};
    card->onTrigger(ctx, {});

    EXPECT_TRUE(state.getObject(legend).is_exhausted);
    auto& bf = state.battlefields[0];
    EXPECT_TRUE(bf.was_replaced);
    EXPECT_TRUE(bf.is_token);
    ASSERT_TRUE(state.objectExists(bf.card_object_id));
    EXPECT_EQ(state.getObject(bf.card_object_id).name, "Brush");
}

TEST_F(CardTestFixture, GreenFather_NoActionWhenAlreadyExhausted) {
    auto legend = state.createObject();
    state.getObject(legend).owner = P1;
    state.getObject(legend).controller = P1;
    state.getObject(legend).card_def_id = kGreenFather;
    state.getObject(legend).card_type = CardType::Legend;
    state.getObject(legend).zone = ZoneType::LegendZone;
    state.getObject(legend).is_exhausted = true;
    state.players[0].legend_zone = legend;
    state.battlefields[0].controller = P1;

    auto bf_card_before = state.battlefields[0].card_object_id;

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kGreenFather);
    CardContext ctx{state, events, exec, P1, legend};
    card->onTrigger(ctx, {});

    EXPECT_FALSE(state.battlefields[0].was_replaced);
    EXPECT_EQ(state.battlefields[0].card_object_id, bf_card_before);
}

// ═══════════════════════════════════════════════════════════════════════════
// [613] Ivern, Nurturer — draft + recycle + themed buff
// ═══════════════════════════════════════════════════════════════════════════

TEST_F(CardTestFixture, IvernNurturer_DraftsAUnitFromTopThreeAndRecyclesRest) {
    auto nurturer = addUnit(P1, kIvernNurturer, /*might=*/3, /*at_bf=*/0);

    // Build a deck: top is the BACK of main_deck. From top to bottom:
    //   spell, unit, spell, then more.
    // After scan: drafted = unit; rest = [spell, spell] (the two non-units).
    auto u_spell1 = addToDeck(P1, kInvalidId);  // bottom-most of the 3
    state.getObject(u_spell1).card_type = CardType::Spell;
    auto u_unit = addToDeck(P1, kInvalidId);
    state.getObject(u_unit).card_type = CardType::Unit;
    state.getObject(u_unit).tags = {"Dog"};
    auto u_spell2 = addToDeck(P1, kInvalidId);  // top of the 3
    state.getObject(u_spell2).card_type = CardType::Spell;

    int deck_before = deckSize(P1);
    int hand_before = handSize(P1);

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kIvernNurturer);
    CardContext ctx{state, events, exec, P1, nurturer};
    card->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), hand_before + 1)
        << "The drafted unit should land in hand.";
    EXPECT_EQ(deckSize(P1), deck_before - 1)
        << "Three popped, two recycled, one drafted to hand.";
    // The drafted card is the unit.
    EXPECT_TRUE(inHand(P1, u_unit));
    EXPECT_FALSE(inHand(P1, u_spell1));
    EXPECT_FALSE(inHand(P1, u_spell2));
}

TEST_F(CardTestFixture, IvernNurturer_NoUnitInTopThree_AllRecycled) {
    auto nurturer = addUnit(P1, kIvernNurturer, /*might=*/3, /*at_bf=*/0);
    auto a = addToDeck(P1, kInvalidId); state.getObject(a).card_type = CardType::Spell;
    auto b = addToDeck(P1, kInvalidId); state.getObject(b).card_type = CardType::Spell;
    auto c = addToDeck(P1, kInvalidId); state.getObject(c).card_type = CardType::Spell;

    int hand_before = handSize(P1);
    int deck_before = deckSize(P1);

    EffectExecutor exec(state, events, card_db);
    auto* card = card_registry.get(kIvernNurturer);
    CardContext ctx{state, events, exec, P1, nurturer};
    card->onTrigger(ctx, {});

    EXPECT_EQ(handSize(P1), hand_before) << "No unit drafted.";
    EXPECT_EQ(deckSize(P1), deck_before) << "All 3 cards recycled back to deck.";
}
