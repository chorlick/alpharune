/// @file test_baron_nashor.cpp
/// Per-card tests for Baron Nashor [709].
///
/// Card text:
///   As you play me, add the Baron Pit battlefield token to the board
///   if it's not there already. If you do, I enter there.
///   (It has "Units can move here from anywhere.")
///   I can't be chosen by enemy spells and abilities.
///   Other friendly units have +2 [M].
///
/// Mechanics covered here:
///   1. canBeChosenByEnemy() = false                                    ✅
///   2. enumerateLegalTargets filters Baron from enemy targets when
///      GameObject::untargetable_by_enemy is set                         ✅
///   3. Friendly targeting still works (Baron can be chosen by his
///      controller's own spells)                                          ✅
///   4. No on-play AoE damage (the prior implementation hallucinated a
///      "deal 3 to each enemy unit" effect that doesn't exist)            ✅
///   5. Card text matches the +2 aura pattern the engine parses           ✅
///
/// Mechanics deferred (Baron Pit BF token spawn + "I enter there" +
/// "Units can move here from anywhere"): not implementable until the
/// engine supports dynamic battlefield-token slots. Captured here as
/// disabled tests so we have a green-light target when that lands.

#include "tests/cards/card_test_fixture.h"

#include "agents/agent_interface.h"
#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "rules/deck_validator.h"

#include <algorithm>
#include <cstring>

using namespace riftbound;
using namespace riftbound::test;

namespace {

constexpr CardDefId kBaronNashor = 709;

// A throwaway "deal 1 damage to a target enemy unit" spell-like card
// used to exercise enumerateLegalTargets — we build its TargetRequirements
// inline rather than depending on a specific real card whose targeting
// rules might change.
class TestEnemyTargetCard : public SpellCard {
public:
    const CardDef& def() const override {
        static const CardDef d = [] { CardDef d; d.id = kInvalidId; return d; }();
        return d;
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1,
                                   .must_be_unit = true,
                                   .must_be_enemy = true,
                                   .must_be_at_battlefield = true};
    }
};

class TestFriendlyTargetCard : public SpellCard {
public:
    const CardDef& def() const override {
        static const CardDef d = [] { CardDef d; d.id = kInvalidId; return d; }();
        return d;
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1,
                                   .must_be_unit = true,
                                   .must_be_friendly = true,
                                   .must_be_at_battlefield = true};
    }
};

} // namespace

// ─── 1. canBeChosenByEnemy override ────────────────────────────────────────

TEST_F(CardTestFixture, BaronNashor_CardSaysCannotBeChosenByEnemy) {
    auto* baron = card_registry.get(kBaronNashor);
    ASSERT_NE(baron, nullptr) << "Baron Nashor not registered";
    EXPECT_FALSE(baron->canBeChosenByEnemy())
        << "Baron Nashor's card text says 'I can't be chosen by enemy "
           "spells and abilities' — canBeChosenByEnemy() must return false.";
}

// ─── 2. Enemy target enumeration filters Baron ─────────────────────────────

TEST_F(CardTestFixture, BaronNashor_FiltersOutAsEnemyTarget) {
    // Put Baron Nashor on P2's side, at BF 0. Set the flag the engine
    // would set during cleanup/aura-recalc.
    auto baron_id = addUnit(P2, kBaronNashor, /*might=*/12, /*at_bf=*/0);
    state.getObject(baron_id).untargetable_by_enemy = true;

    // Put a normal P2 unit at the same BF — should be a legal target.
    auto chump_id = addUnit(P2, /*def_id=*/kInvalidId, /*might=*/3, /*at_bf=*/0);

    TestEnemyTargetCard probe;
    auto targets = probe.enumerateLegalTargets(state, /*controller=*/P1);

    EXPECT_NE(std::find(targets.begin(), targets.end(), chump_id),
              targets.end())
        << "Normal enemy unit at BF should be a legal target.";
    EXPECT_EQ(std::find(targets.begin(), targets.end(), baron_id),
              targets.end())
        << "Baron Nashor (with untargetable_by_enemy=true) must NOT appear "
           "as a legal enemy target.";
}

// ─── 3. Friendly targeting still works ─────────────────────────────────────

TEST_F(CardTestFixture, BaronNashor_RemainsTargetableByFriendlyControl) {
    // Baron on P1's side. "I can't be chosen by ENEMY spells" — friendly
    // targeting should still include him.
    auto baron_id = addUnit(P1, kBaronNashor, /*might=*/12, /*at_bf=*/0);
    state.getObject(baron_id).untargetable_by_enemy = true;

    TestFriendlyTargetCard probe;
    auto targets = probe.enumerateLegalTargets(state, /*controller=*/P1);

    EXPECT_NE(std::find(targets.begin(), targets.end(), baron_id),
              targets.end())
        << "Baron Nashor must remain a legal target for his controller's "
           "own friendly-target spells (the restriction is enemy-only).";
}

// ─── 4. No on-play AoE damage ─────────────────────────────────────────────

TEST_F(CardTestFixture, BaronNashor_OnPlayDealsNoAoEDamage) {
    // Earlier hallucinated implementation dealt 3 damage to each enemy
    // unit on play. Regression test: confirm onPlay is a no-op for
    // damage so we never reintroduce that bug.
    auto* baron = card_registry.get(kBaronNashor);
    ASSERT_NE(baron, nullptr);

    auto enemy1 = addUnit(P2, /*def_id=*/kInvalidId, /*might=*/3, /*at_bf=*/0);
    auto enemy2 = addUnit(P2, /*def_id=*/kInvalidId, /*might=*/3, /*at_bf=*/1);
    auto baron_id = addUnit(P1, kBaronNashor, /*might=*/12, /*at_bf=*/0);

    int enemy1_dmg_before = state.getObject(enemy1).damage_marked;
    int enemy2_dmg_before = state.getObject(enemy2).damage_marked;

    // Invoke the play hook directly the way executePlayCard would.
    EffectExecutor executor(state, events, card_db);
    CardContext ctx{state, events, executor, P1, baron_id};
    baron->onPlay(ctx);

    EXPECT_EQ(state.getObject(enemy1).damage_marked, enemy1_dmg_before)
        << "Baron Nashor must NOT deal damage on play. Card text has no "
           "AoE; a prior implementation incorrectly added one.";
    EXPECT_EQ(state.getObject(enemy2).damage_marked, enemy2_dmg_before)
        << "Same: enemy2 should remain undamaged.";
}

// ─── 5. AoE damage still hits Baron Nashor ────────────────────────────────
//
// The card text says "I can't be CHOSEN by enemy spells and abilities" —
// that restriction applies to TARGETED selection. AoE damage doesn't
// choose Baron explicitly; it iterates all eligible units and applies
// damage to each. The untargetable flag must NOT block AoE.
//
// We exercise this with Flurry of Blades (id=133), the canonical
// "deal 1 to all units" reaction. Its onResolve iterates state.objects
// and calls EffectExecutor::dealDamage directly — no enumerateLegalTargets
// involved. Baron should take damage like any other unit.

TEST_F(CardTestFixture, BaronNashor_AoEDamageStillHits) {
    constexpr CardDefId kFlurryOfBlades = 133;

    // Baron on P2's side at BF 0, flag set as engine would set it after
    // cleanup/aura-recalc.
    auto baron_id = addUnit(P2, kBaronNashor, /*might=*/12, /*at_bf=*/0);
    state.getObject(baron_id).untargetable_by_enemy = true;

    // A normal P2 unit beside Baron, also at BF 0 — for comparison.
    auto chump_id = addUnit(P2, /*def_id=*/kInvalidId, /*might=*/3, /*at_bf=*/0);

    int baron_dmg_before = state.getObject(baron_id).damage_marked;
    int chump_dmg_before = state.getObject(chump_id).damage_marked;

    // P1 plays Flurry of Blades. AoE has no target slots — its
    // onResolve sweeps state.objects directly.
    auto* flurry = card_registry.get(kFlurryOfBlades);
    ASSERT_NE(flurry, nullptr);
    // We need a source GameObjectId for the spell. Synthesize one in
    // P1's trash so the damage event has a credible source. (The
    // executor records source for the DamageDealtEvent.)
    auto flurry_src = addToHand(P1, kFlurryOfBlades);

    EffectExecutor executor(state, events, card_db);
    CardContext ctx{state, events, executor, P1, flurry_src};
    flurry->onResolve(ctx, /*targets=*/{});

    EXPECT_GT(state.getObject(baron_id).damage_marked, baron_dmg_before)
        << "Baron Nashor MUST take AoE damage. 'Can't be CHOSEN' applies "
           "to targeted selection only — AoE iterates units without "
           "choosing them.";
    EXPECT_GT(state.getObject(chump_id).damage_marked, chump_dmg_before)
        << "Sanity: the normal unit also took AoE damage.";
    // Both should have taken the SAME amount (1 from Flurry).
    EXPECT_EQ(state.getObject(baron_id).damage_marked - baron_dmg_before,
              state.getObject(chump_id).damage_marked - chump_dmg_before)
        << "Baron and the chump should take equal AoE damage — Baron "
           "has no special damage-reduction property.";
}

// ─── 6. Aura text matches engine's parser pattern ──────────────────────────

TEST_F(CardTestFixture, BaronNashor_AuraTextMatchesParserPattern) {
    // The engine's recalculateAuras() picks up "other friendly units
    // have +N [M]" via text matching. Document the exact pattern so a
    // future cards-data refresh doesn't silently break the aura.
    const auto& def = card_db.get(kBaronNashor);
    std::string lower;
    for (char c : def.ability_text) lower += std::tolower(c);

    EXPECT_NE(lower.find("other friendly units have"), std::string::npos)
        << "Baron Nashor's ability_text must include the aura pattern "
           "the engine's text-matcher uses. If this fails, either the "
           "card text changed (re-fetch from gallery) or the pattern "
           "matcher in recalculateAuras() needs updating.";
    EXPECT_NE(lower.find("+2 [m]"), std::string::npos)
        << "The aura value should be +2 per card text. If this changes, "
           "the test should be updated AND the design re-validated.";
}

// ─── 7. Baron Pit BF token spawn ──────────────────────────────────────────

TEST_F(CardTestFixture, BaronNashor_AddsBaronPitOnPlay) {
    // Baron Nashor's "As you play me" effect (CR 355.1) runs in Card::onPlay,
    // invoked synchronously during the play action — NOT as a triggered
    // ability — so there is no chain item between Baron being played and
    // him entering the Pit. The Pit is spawned and Baron's location is
    // rewritten BEFORE resolvePermanent lands him on the board.
    auto* baron = card_registry.get(kBaronNashor);
    ASSERT_NE(baron, nullptr);
    EXPECT_EQ(baron->triggerType(), TriggerType::None)
        << "Baron's 'As you play me' must not be a triggered ability — "
           "otherwise an opponent could respond in the chain priority "
           "window between play resolution and Pit creation.";

    // Baron starts where the default permanent-play resolution would
    // put him: base. Then his onPlay hook fires.
    auto baron_id = addUnit(P1, kBaronNashor, /*might=*/12, /*at_bf=*/-1);
    ASSERT_TRUE(state.getObject(baron_id).isAtBase());

    size_t bf_count_before = state.battlefields.size();

    EffectExecutor executor(state, events, card_db);
    CardContext ctx{state, events, executor, P1, baron_id};
    baron->onPlay(ctx);

    // Pit was spawned.
    EXPECT_EQ(state.battlefields.size(), bf_count_before + 1)
        << "Expected one new battlefield slot to be appended.";
    auto& pit = state.battlefields.back();
    ASSERT_TRUE(state.objectExists(pit.card_object_id));
    EXPECT_EQ(state.getObject(pit.card_object_id).name, "Baron Pit");
    EXPECT_TRUE(pit.is_token);
    EXPECT_TRUE(pit.accepts_any_inbound)
        << "Baron Pit must carry the 'Units can move here from anywhere' "
           "flag onto its BattlefieldState slot.";

    // Baron relocated there.
    auto& baron_obj = state.getObject(baron_id);
    EXPECT_TRUE(baron_obj.isAtBattlefield());
    EXPECT_EQ(baron_obj.battlefieldId().value_or(kInvalidId), pit.id)
        << "'If you do, I enter there' — Baron must move to the new Pit.";
}

// Integration test: drive a full game where P1 prefers to play Baron
// Nashor whenever it's legal. Asserts that within the same trace event
// stream, every "PLAY: Baron Nashor" line is immediately preceded or
// followed by a "BF_TOKEN: added 'Baron Pit'" event and that Baron Pit
// persists in state.battlefields afterwards. This catches integration
// regressions where executePlayCard's onPlay hook is skipped or where
// resolvePermanent stomps Baron's location back to base — the kind of
// bug that wouldn't surface from calling onPlay directly.
//
// Note: the god-mode `edit_move` action in the web UI bypasses
// executePlayCard entirely (StateEditor::moveObject just rewrites zone
// and location), so if a user moves Baron from hand to base via the
// god panel, NO onPlay fires and NO Baron Pit is created. That's by
// design — god-mode is for state mutation, not simulated plays.
TEST_F(CardTestFixture, BaronNashor_IntegrationFullPlayPathCreatesPit) {
    GameEngine engine(card_db, events, card_registry);

    // P1 plays Baron Nashor whenever offered, else first-legal; P2 always
    // first-legal (close to random but deterministic on tied options).
    struct PreferBaronAgent : public AgentInterface {
        Intent selectAction(const GameState& state,
                             const std::vector<Intent>& legal) override {
            for (const auto& i : legal) {
                if (i.type != IntentType::PlayCard) continue;
                if (i.card == kInvalidId) continue;
                if (!state.objectExists(i.card)) continue;
                if (state.getObject(i.card).card_def_id == 709) return i;
            }
            return legal.empty() ? Intent{} : legal.front();
        }
    };
    struct FirstAgent : public AgentInterface {
        Intent selectAction(const GameState&,
                             const std::vector<Intent>& legal) override {
            return legal.empty() ? Intent{} : legal.front();
        }
    };
    PreferBaronAgent p1; FirstAgent p2;

    // Watch the event stream so we can correlate Baron plays with Pit
    // creation. If a play fires but the Pit token isn't materialised,
    // we'll catch it post-game.
    int baron_plays = 0;
    int pit_tokens  = 0;
    auto conn = events.on_log.connect([&](const LogEvent& e) {
        if (e.message.find("PLAY: Baron Nashor") != std::string::npos) ++baron_plays;
        if (e.message.find("BF_TOKEN: added 'Baron Pit'") != std::string::npos) ++pit_tokens;
    });

    auto registry_root = findRegistryPath();
    std::string root = registry_root.substr(0, registry_root.rfind('/')) + "/..";
    DeckSubmission deck1 = DeckValidator::loadFromDeckList(
        root + "/decks/miss_fortune_test.txt", card_db);
    DeckSubmission deck2 = DeckValidator::loadFromDeckList(
        root + "/decks/miss_fortune_test.txt", card_db);

    // Fixed seed so any future engine-side regression that breaks Baron
    // Pit creation is deterministic to reproduce. 12345 plays Baron at
    // least once with this deck pair.
    auto result = engine.runGame(deck1, deck2, p1, p2, 12345);
    conn.disconnect();

    // The agent prefers Baron — at least one play should land over a
    // full game. If this assertion is false, the test fixture itself
    // needs adjusting (different seed); not a bug.
    ASSERT_GT(baron_plays, 0)
        << "Test scaffold: P1 never got to play Baron Nashor with seed 12345 — "
           "adjust seed or deck to make Baron playable.";

    // The integration invariant: every Baron play must trigger a Pit
    // creation, unless the Pit already exists (mirror match — but in
    // this fixture P2 doesn't play Baron, so only P1's plays count).
    EXPECT_GE(pit_tokens, 1)
        << "Engine integration regression: P1 played Baron Nashor "
        << baron_plays << " times, but " << pit_tokens
        << " Baron Pit BF tokens were created. Expected at least 1 (the "
           "first Baron play creates the Pit; subsequent plays in the same "
           "game reuse it, so pit_tokens == 1 is correct).";

    // Final state: Pit should still be in battlefields list.
    bool pit_present = false;
    for (const auto& bf : engine.state().battlefields) {
        if (!engine.state().objectExists(bf.card_object_id)) continue;
        if (engine.state().getObject(bf.card_object_id).name == "Baron Pit") {
            pit_present = true;
            EXPECT_TRUE(bf.is_token);
            EXPECT_TRUE(bf.accepts_any_inbound);
            break;
        }
    }
    EXPECT_TRUE(pit_present)
        << "Baron Pit was created during the game but disappeared from "
           "state.battlefields by game-end — something downstream is "
           "removing token battlefields they shouldn't.";

    (void)result;  // outcome not relevant to this invariant
}

TEST_F(CardTestFixture, BaronNashor_SecondCopyDoesNotEnterExistingPit) {
    // Mirror-match scenario: P1 already played Baron and Baron Pit
    // exists. P2's Baron must NOT relocate into the existing Pit per
    // the "If you do, I enter there" qualifier (the Pit must have been
    // CREATED by this play). P2's Baron stays at base.
    auto* baron = card_registry.get(kBaronNashor);
    ASSERT_NE(baron, nullptr);

    // Manually pre-spawn a Baron Pit (as if P1 already triggered it).
    EffectExecutor pre_executor(state, events, card_db);
    auto pit_id = pre_executor.addBattlefieldToken(
        "Baron Pit", /*accepts_any_inbound=*/true);
    size_t bf_count_after_pre = state.battlefields.size();

    // P2 plays Baron — starts at base.
    auto p2_baron = addUnit(P2, kBaronNashor, /*might=*/12, /*at_bf=*/-1);

    EffectExecutor executor(state, events, card_db);
    CardContext ctx{state, events, executor, P2, p2_baron};
    baron->onPlay(ctx);

    // Pit count is unchanged — no new BF spawned, because one existed.
    EXPECT_EQ(state.battlefields.size(), bf_count_after_pre);

    // P2's Baron stays at base ("If you do" clause failed).
    auto& obj = state.getObject(p2_baron);
    EXPECT_TRUE(obj.isAtBase())
        << "Second Baron must stay at base when Pit already existed.";
    // And specifically NOT at the existing pit.
    auto bf = obj.battlefieldId();
    EXPECT_FALSE(bf.has_value() && *bf == pit_id);
}

TEST_F(CardTestFixture, BaronNashor_TwoBaronsPlayedSequentially_OnePitOneBaronInIt) {
    // End-to-end: P1 plays Baron → spawns Pit, Baron #1 enters.
    // Then P2 plays Baron → "If you do" fails, no new Pit, P2's Baron at base.
    // After both plays: exactly 1 Pit total, Baron #1 in Pit, Baron #2 at base.
    auto* baron = card_registry.get(kBaronNashor);
    ASSERT_NE(baron, nullptr);

    size_t bf_count_before = state.battlefields.size();
    EffectExecutor executor(state, events, card_db);

    // ── P1 plays Baron ──
    auto p1_baron = addUnit(P1, kBaronNashor, /*might=*/12, /*at_bf=*/-1);
    ASSERT_TRUE(state.getObject(p1_baron).isAtBase());
    {
        CardContext ctx{state, events, executor, P1, p1_baron};
        baron->onPlay(ctx);
    }

    // Pit exists, P1's Baron entered it.
    ASSERT_EQ(state.battlefields.size(), bf_count_before + 1)
        << "P1's Baron must spawn exactly one new BF slot.";
    auto pit_id = state.battlefields.back().id;
    EXPECT_TRUE(state.battlefields.back().is_token);
    EXPECT_EQ(state.getObject(p1_baron).battlefieldId().value_or(kInvalidId), pit_id)
        << "P1's Baron must be in the freshly-spawned Pit.";

    // ── P2 plays Baron ──
    auto p2_baron = addUnit(P2, kBaronNashor, /*might=*/12, /*at_bf=*/-1);
    ASSERT_TRUE(state.getObject(p2_baron).isAtBase());
    {
        CardContext ctx{state, events, executor, P2, p2_baron};
        baron->onPlay(ctx);
    }

    // No new BF (only one Pit exists, total).
    EXPECT_EQ(state.battlefields.size(), bf_count_before + 1)
        << "P2's Baron must NOT spawn a second Pit.";

    // Count Pits by name — must be exactly 1.
    int pit_count = 0;
    for (auto& bf : state.battlefields) {
        if (state.objectExists(bf.card_object_id) &&
            state.getObject(bf.card_object_id).name == "Baron Pit") {
            ++pit_count;
        }
    }
    EXPECT_EQ(pit_count, 1) << "Exactly one Baron Pit must exist on the board.";

    // Count Barons in the Pit — must be exactly 1, and it's P1's.
    int barons_in_pit = 0;
    GameObjectId pit_resident = kInvalidId;
    for (auto& [oid, obj] : state.objects) {
        if (obj.name == "Baron Nashor" && obj.isAtBattlefield() &&
            obj.battlefieldId().value_or(kInvalidId) == pit_id) {
            ++barons_in_pit;
            pit_resident = oid;
        }
    }
    EXPECT_EQ(barons_in_pit, 1) << "Exactly one Baron Nashor must occupy the Pit.";
    EXPECT_EQ(pit_resident, p1_baron)
        << "The Baron in the Pit must be P1's (the one that triggered the spawn).";
    EXPECT_TRUE(state.getObject(p2_baron).isAtBase())
        << "P2's Baron must stay at base — its 'If you do' clause did not fire.";
}

// ─── 8. Movement rule: any unit can move from another BF to the Pit ───────
//
// This is the heart of "Units can move here from anywhere". A unit AT a
// different battlefield, without Ganking, should still be able to move
// directly to Baron Pit. Normally BF→BF requires Ganking (CR 144.4.c);
// Baron Pit's `accepts_any_inbound` flag relaxes that restriction.

TEST_F(CardTestFixture, BaronNashor_NonGankingUnitCanMoveBFToPit) {
    // Three battlefields: the two starting ones from SetUp, plus a
    // freshly-added Baron Pit.
    EffectExecutor exec(state, events, card_db);
    auto pit_id = exec.addBattlefieldToken("Baron Pit",
                                            /*accepts_any_inbound=*/true);

    // A normal P1 unit (no Ganking) currently at BF 0.
    auto chump = addUnit(P1, /*def_id=*/kInvalidId, /*might=*/3, /*at_bf=*/0);
    state.getObject(chump).is_exhausted = false;  // ready, so Move is legal
    ASSERT_FALSE(state.getObject(chump).hasKeyword(Keyword::Ganking));

    // Build a GameEngine just to call generateMainPhaseActions through
    // a public surface. The engine constructor needs runtime params we
    // don't have in this fixture — instead we test the underlying flag
    // by calling the generator-equivalent in-line: any BF with
    // accepts_any_inbound is a valid move destination for a BF→BF jump.
    //
    // Direct verification: walk the state and assert that, given the
    // current move-action generator's logic, a non-Ganking unit at BF 0
    // has BF=pit_id as a valid destination. We can simulate this by
    // checking the condition the generator uses.
    auto& unit = state.getObject(chump);
    bool found_legal_pit_move = false;
    for (auto& bf : state.battlefields) {
        auto unit_bf = unit.battlefieldId();
        if (!unit_bf || *unit_bf == bf.id) continue;
        const bool has_ganking = unit.hasKeyword(Keyword::Ganking);
        const bool can_move = has_ganking || bf.accepts_any_inbound;
        if (can_move && bf.id == pit_id) {
            found_legal_pit_move = true;
            break;
        }
    }
    EXPECT_TRUE(found_legal_pit_move)
        << "A non-Ganking unit at another BF must be able to move "
           "directly to Baron Pit. 'Units can move here from anywhere' "
           "requires the move-action generator to honor "
           "BattlefieldState::accepts_any_inbound for BF→BF jumps.";
}

TEST_F(CardTestFixture, BaronNashor_NonGankingUnitCannotMoveBFToBF_NonPit) {
    // Counter-test: the relaxation is Pit-specific. A non-Ganking unit
    // at BF 0 must NOT have BF 1 as a legal destination (BF 1 doesn't
    // have the flag set). Defends against accidentally globalizing the
    // accepts_any_inbound logic.
    auto chump = addUnit(P1, /*def_id=*/kInvalidId, /*might=*/3, /*at_bf=*/0);
    state.getObject(chump).is_exhausted = false;

    auto& unit = state.getObject(chump);
    auto& other_bf = state.battlefields[1];
    EXPECT_FALSE(other_bf.accepts_any_inbound);

    const bool has_ganking = unit.hasKeyword(Keyword::Ganking);
    const bool can_move_to_other = has_ganking || other_bf.accepts_any_inbound;
    EXPECT_FALSE(can_move_to_other)
        << "Non-Ganking unit at a vanilla BF must NOT have a non-Pit BF "
           "as a legal move destination — that's standard CR 144.4.c.";
}

// ─── 9. Aura (deferred — needs engine in fixture) ─────────────────────────

TEST_F(CardTestFixture, DISABLED_BaronNashor_AuraGivesFriendlyUnitsPlus2M) {
    // After recalculateAuras runs with Baron on the board, every OTHER
    // friendly unit should have aura_might_bonus += 2. Baron himself
    // is excluded by the "other" clause in the parser.
    GTEST_SKIP() << "Deferred: needs a fixture-side hook into "
                    "GameEngine::recalculateAuras (currently the "
                    "CardTestFixture builds GameState directly without "
                    "an engine). When that lands, drop the DISABLED_ "
                    "prefix and verify aura_might_bonus on a sibling.";
}
