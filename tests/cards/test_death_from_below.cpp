/// @file test_death_from_below.cpp
/// Tests for [747] Death from Below and the trash-replay grant machinery.
///
/// "Kill a unit at a battlefield. Then, if it had 3 [M] or less, do this:
///  You may play this from your trash for [A]."
///
/// Two layers are covered:
///   1. onResolve pushes a PlayerState::TrashReplayGrant only when the killed
///      unit had ≤3 might, with the [A] override cost (0 energy, 1 any-domain
///      power).
///   2. The action generator (GameEngine::generateTrashReplayActions) offers
///      the spell from trash with play_source=Trash when a grant exists and the
///      override cost is affordable — and does NOT offer it otherwise.

#include "tests/cards/card_test_fixture.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/events.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <algorithm>
#include <gtest/gtest.h>

namespace riftbound::test {
namespace {

constexpr CardDefId kDeathFromBelow = 747;

// ── Layer 1: onResolve grants ─────────────────────────────────────────────

TEST_F(CardTestFixture, DeathFromBelow_GrantsTrashReplayWhenMightLow) {
    EffectExecutor exec(state, events, card_db, &card_registry);

    // Spell object (its own id is what the grant references).
    auto spell = addToHand(P1, kDeathFromBelow);
    // A small enemy unit at a battlefield — the kill target.
    auto victim = addUnit(P2, kInvalidId, /*might=*/3, /*at_bf=*/0);

    invokeOnResolve(spell, kDeathFromBelow, P1, {victim}, exec);

    auto& grants = state.player(P1).trash_replay_grants;
    ASSERT_EQ(grants.size(), 1u) << "≤3 might victim must grant a trash-replay";
    EXPECT_EQ(grants[0].card, spell);
    EXPECT_EQ(grants[0].energy, 0);
    EXPECT_EQ(grants[0].power, 1);
    EXPECT_TRUE(grants[0].any_domain) << "[A] = one power of any domain";
}

TEST_F(CardTestFixture, DeathFromBelow_NoGrantWhenMightHigh) {
    EffectExecutor exec(state, events, card_db, &card_registry);

    auto spell  = addToHand(P1, kDeathFromBelow);
    auto victim = addUnit(P2, kInvalidId, /*might=*/4, /*at_bf=*/0);

    invokeOnResolve(spell, kDeathFromBelow, P1, {victim}, exec);

    EXPECT_TRUE(state.player(P1).trash_replay_grants.empty())
        << "4 might victim is above the '3 [M] or less' threshold";
}

// ── Layer 2: action generation ────────────────────────────────────────────

namespace {
// Counts Play intents that replay `card` straight out of trash.
int trashReplayActionsFor(GameEngine& engine, GameObjectId card) {
    int n = 0;
    for (const auto& a : engine.generateLegalActions()) {
        if (a.play_source != Intent::PlaySource::Trash) continue;
        if (a.card == card) ++n;
    }
    return n;
}

// Build an engine in P1's Neutral-Open main phase with Death from Below sitting
// in P1's trash and a legal kill target on the board.
GameObjectId setupTrashedSpell(GameEngine& engine, CardDB& card_db) {
    auto& s = engine.mutableState();
    s.mode = ModeOfPlay{};
    s.players[0].id = PlayerId::Player1;
    s.players[1].id = PlayerId::Player2;
    s.turn.turn_player = PlayerId::Player1;
    s.turn.phase    = TurnPhase::MainPhase;
    s.turn.ns_state = NeutralShowdownState::Neutral;
    s.turn.oc_state = OpenClosedState::Open;
    s.battlefields.push_back(BattlefieldState{});
    s.battlefields.back().id = 0;

    // Legend zones (mirror other GameEngine action-gen tests).
    for (int p = 0; p < 2; ++p) {
        auto leg = s.createObject();
        auto& lo = s.getObject(leg);
        lo.owner = s.players[p].id;
        lo.controller = s.players[p].id;
        lo.card_type = CardType::Legend;
        lo.zone = ZoneType::LegendZone;
        s.players[p].legend_zone = leg;
    }

    // Death from Below in P1's trash.
    auto spell = s.createObject();
    auto& so = s.getObject(spell);
    const auto& def = card_db.get(kDeathFromBelow);
    so.owner = PlayerId::Player1; so.controller = PlayerId::Player1;
    so.card_def_id = kDeathFromBelow;
    so.card_type = def.card_type;
    so.name = def.name;
    so.keywords = def.keywords;
    so.domains = def.domains;
    so.zone = ZoneType::Trash;
    s.players[0].trash.push_back(spell);

    // A legal kill target (unit at a battlefield) so hasLegalTargets() holds.
    auto victim = s.createObject();
    auto& vo = s.getObject(victim);
    vo.owner = PlayerId::Player2; vo.controller = PlayerId::Player2;
    vo.card_type = CardType::Unit;
    vo.base_might = 2; vo.current_might = 2;
    vo.zone = ZoneType::BattlefieldZone;
    vo.location = BattlefieldLocation{0};
    return spell;
}

// Add a ready rune of `domain` to a player's base.
void addReadyRune(GameState& s, PlayerId p, Domain domain) {
    auto r = s.createObject();
    auto& ro = s.getObject(r);
    ro.owner = p; ro.controller = p;
    ro.card_type = CardType::Rune;
    ro.domains = {domain};
    ro.zone = ZoneType::Base;
    ro.location = BaseLocation{p};
    ro.is_exhausted = false;
}
}  // namespace

TEST_F(CardTestFixture, DeathFromBelow_ActionGenOffersTrashReplayWithGrant) {
    GameEngine engine(card_db, events, card_registry);
    auto spell = setupTrashedSpell(engine, card_db);
    auto& s = engine.mutableState();

    // One ready Fury rune covers the [A] override (recycle for 1 power).
    addReadyRune(s, PlayerId::Player1, Domain::Fury);

    // No grant yet → not offered, even though the spell is in trash.
    EXPECT_EQ(trashReplayActionsFor(engine, spell), 0)
        << "a trashed spell is not replayable without an explicit grant";

    // Grant it.
    PlayerState::TrashReplayGrant g;
    g.card = spell; g.energy = 0; g.power = 1; g.any_domain = true;
    s.players[0].trash_replay_grants.push_back(g);

    EXPECT_GE(trashReplayActionsFor(engine, spell), 1)
        << "with the grant + an affordable [A], the trash-replay is offered";
}

TEST_F(CardTestFixture, DeathFromBelow_TrashReplayNotOfferedWhenUnaffordable) {
    GameEngine engine(card_db, events, card_registry);
    auto spell = setupTrashedSpell(engine, card_db);
    auto& s = engine.mutableState();

    // Grant present, but NO runes — [A] cannot be paid.
    PlayerState::TrashReplayGrant g;
    g.card = spell; g.energy = 0; g.power = 1; g.any_domain = true;
    s.players[0].trash_replay_grants.push_back(g);

    EXPECT_EQ(trashReplayActionsFor(engine, spell), 0)
        << "the override cost [A] must be affordable for the replay to appear";
}

}  // namespace
}  // namespace riftbound::test
