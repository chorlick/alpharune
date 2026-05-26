/// @file test_legal_targets_gate.cpp
/// Verifies the universal Card::hasLegalTargets() gate (CR 700.x):
/// counter spells with no valid chain target are NOT offered as legal
/// moves. Default impl preserves the historical "needs a board target
/// but none exists" behavior for normal cards.
///
/// Pure unit-level: pokes Card directly with a synthesized GameState,
/// no engine, no chain manager, no agents. The engine wire-up is
/// covered indirectly by the existing miss-fortune mirror smoke run.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/game_state.h"

#include <gtest/gtest.h>

using namespace riftbound;

namespace {

constexpr CardDefId kHardBargainId = 457;
constexpr CardDefId kWindWallId    = 64;
constexpr CardDefId kRepulseId     = 668;
constexpr CardDefId kNotSoFastId   = 368;
constexpr CardDefId kDefyId        = 45;   // counter a spell costing no more than [4]
constexpr CardDefId kLunarBoonId   = 687;  // cost 3 spell (legal Defy target)
constexpr CardDefId kAbandonId     = 693;  // counter + predict (always playable)
constexpr CardDefId kLullabyId     = 750;  // counter + lockout (lockout is scoped
                                           // to countered spell's controller, so
                                           // it's a pure counter for legality)

class LegalTargetsFixture : public ::testing::Test {
protected:
    CardDB card_db;
    CardRegistry card_registry;
    GameState state;

    void SetUp() override {
        card_registry.loadAll();
        card_db.buildFromClasses(card_registry);

        state.players[0].id = PlayerId::Player1;
        state.players[1].id = PlayerId::Player2;
        BattlefieldState bf0; bf0.id = 0; state.battlefields.push_back(bf0);
    }

    /// Push a fake spell ChainItem onto the chain, optionally targeting a
    /// friendly id. Returns the spell's GameObjectId.
    GameObjectId pushSpellOnChain(PlayerId controller,
                                   const std::vector<GameObjectId>& targets = {},
                                   CardDefId def_id = kInvalidId) {
        auto src = state.createObject();
        auto& obj = state.getObject(src);
        obj.owner = controller;
        obj.controller = controller;
        obj.card_type = CardType::Spell;
        obj.zone = ZoneType::Chain;
        obj.card_def_id = def_id;

        ChainItem item;
        item.id = state.chain.allocateId();
        item.source = src;
        item.controller = controller;
        item.is_spell = true;
        item.card_def_id = def_id;
        item.targets = targets;
        state.chain.items.push_back(item);
        return src;
    }

    /// Create a friendly unit at the given battlefield (for Repulse / NSF
    /// target-shape tests).
    GameObjectId addFriendlyUnit(PlayerId controller) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = controller;
        obj.controller = controller;
        obj.card_type = CardType::Unit;
        obj.zone = ZoneType::BattlefieldZone;
        obj.location = BattlefieldLocation{0};
        return id;
    }
};

// ─── Pure counters: not playable when chain is empty ────────────────────────

TEST_F(LegalTargetsFixture, HardBargainBlockedWhenChainEmpty) {
    Card* card = card_registry.get(kHardBargainId);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, HardBargainAllowedWhenSpellOnChain) {
    pushSpellOnChain(PlayerId::Player2);
    Card* card = card_registry.get(kHardBargainId);
    EXPECT_TRUE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, WindWallBlockedWhenChainEmpty) {
    Card* card = card_registry.get(kWindWallId);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, WindWallAllowedWhenSpellOnChain) {
    pushSpellOnChain(PlayerId::Player2);
    Card* card = card_registry.get(kWindWallId);
    EXPECT_TRUE(card->hasLegalTargets(state, PlayerId::Player1));
}

// ─── Filtered counters: require enemy spell targeting friendly ──────────────

TEST_F(LegalTargetsFixture, RepulseBlockedWhenChainEmpty) {
    Card* card = card_registry.get(kRepulseId);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, RepulseBlockedWhenChainTargetsNothing) {
    pushSpellOnChain(PlayerId::Player2, /*targets=*/{});
    Card* card = card_registry.get(kRepulseId);
    EXPECT_FALSE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, RepulseBlockedWhenChainTargetsEnemy) {
    auto enemy_unit = state.createObject();
    auto& obj = state.getObject(enemy_unit);
    obj.controller = PlayerId::Player2;  // not friendly to P1
    obj.card_type = CardType::Unit;
    obj.location = BattlefieldLocation{0};
    pushSpellOnChain(PlayerId::Player2, {enemy_unit});
    Card* card = card_registry.get(kRepulseId);
    EXPECT_FALSE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, RepulseAllowedWhenEnemySpellTargetsFriendlyUnit) {
    auto friendly = addFriendlyUnit(PlayerId::Player1);
    pushSpellOnChain(PlayerId::Player2, {friendly});
    Card* card = card_registry.get(kRepulseId);
    EXPECT_TRUE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, NotSoFastBlockedWhenChainEmpty) {
    Card* card = card_registry.get(kNotSoFastId);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, NotSoFastAllowedWhenEnemySpellTargetsFriendlyUnit) {
    auto friendly = addFriendlyUnit(PlayerId::Player1);
    pushSpellOnChain(PlayerId::Player2, {friendly});
    Card* card = card_registry.get(kNotSoFastId);
    EXPECT_TRUE(card->hasLegalTargets(state, PlayerId::Player1));
}

// ─── Counters with secondary effects keep being playable ─────────────────────

// Per CR 355.9.a.2 + 355.10 — "Counter a spell" requires a target
// on the Chain. Side effects (draw 1 for Defy, predict 1 for Abandon)
// don't satisfy the target requirement. Earlier "always playable"
// design was reversed after user CR audit.

TEST_F(LegalTargetsFixture, DefyBlockedWhenChainEmpty) {
    Card* card = card_registry.get(kDefyId);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->hasLegalTargets(state, PlayerId::Player1))
        << "Per CR, Defy needs a spell on chain — draw side effect "
           "doesn't satisfy the target requirement.";
}

TEST_F(LegalTargetsFixture, DefyAllowedWhenSpellOnChain) {
    // Defy is legal only vs a spell costing no more than [4]. Lunar Boon
    // (cost 3) is a valid counter target.
    pushSpellOnChain(PlayerId::Player2, /*targets=*/{}, kLunarBoonId);
    Card* card = card_registry.get(kDefyId);
    EXPECT_TRUE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, AbandonBlockedWhenChainEmpty) {
    Card* card = card_registry.get(kAbandonId);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->hasLegalTargets(state, PlayerId::Player1))
        << "Per CR, Abandon needs a spell on chain — predict side "
           "effect doesn't satisfy the target requirement.";
}

TEST_F(LegalTargetsFixture, AbandonAllowedWhenSpellOnChain) {
    pushSpellOnChain(PlayerId::Player2);
    Card* card = card_registry.get(kAbandonId);
    EXPECT_TRUE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, LiltingLullabyBlockedWhenChainEmpty) {
    // Lullaby's "Its controller can't play spells this turn" lockout is
    // scoped to the controller of the countered spell. With no counter
    // target the lockout has no owner — the whole spell silently no-ops.
    // So Lullaby IS a pure counter for legality purposes.
    Card* card = card_registry.get(kLullabyId);
    ASSERT_NE(card, nullptr);
    EXPECT_FALSE(card->hasLegalTargets(state, PlayerId::Player1));
}

TEST_F(LegalTargetsFixture, LiltingLullabyAllowedWhenSpellOnChain) {
    pushSpellOnChain(PlayerId::Player2);
    Card* card = card_registry.get(kLullabyId);
    EXPECT_TRUE(card->hasLegalTargets(state, PlayerId::Player1));
}

}  // namespace
