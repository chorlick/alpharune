/// Round-trip tests for the OpenSpiel bit-packed action encoding.
///
/// For each of the 14 main IntentTypes we construct an example Intent
/// referencing GameObjects in a minimal hand-built GameState, encode it
/// to an int64 ActionID, decode within a single-element legal list, and
/// assert that decode → encode equals the original encoding (proving the
/// encoding is structurally stable across encode/decode round-trips).
///
/// Phase B requirement: action IDs must remain valid across Clone(), so
/// the encoding cannot depend on volatile GameObjectIds. The encoding
/// uses CardDefIds (from registry.json) which are immutable.

#include <gtest/gtest.h>

#include "core/game_object.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "openspiel/action_encoding.h"

#include <vector>

using namespace riftbound;
using namespace riftbound::openspiel;

namespace {

/// Hand-build a GameState with enough scaffolding to look up CardDefIds
/// from a handful of GameObjectIds. Each created object has a distinct,
/// non-kInvalidId CardDefId so the encoding ends up non-trivial.
struct MiniState {
    GameState state;

    GameObjectId addObject(CardDefId def_id, PlayerId owner = PlayerId::Player1) {
        GameObjectId id = next_id_++;
        state.objects[id] = GameObject{};
        auto& o = state.objects[id];
        o.id = id;
        o.card_def_id = def_id;
        o.owner = owner;
        o.controller = owner;
        return id;
    }

    BattlefieldId addBattlefield() {
        BattlefieldState bf;
        bf.id = next_bf_++;
        state.battlefields.push_back(bf);
        return state.battlefields.back().id;
    }

private:
    GameObjectId next_id_ = 1;
    BattlefieldId next_bf_ = 0;
};

/// Round-trip helper: encode an intent, decode it within a 1-element
/// legal-action list, assert the decoded intent has the same encoding.
void expectRoundTrip(const Intent& intent, const GameState& state) {
    int64_t id = encodeAction(intent, state);
    std::vector<Intent> legal = {intent};
    const Intent* decoded = decodeAction(id, legal, state);
    ASSERT_NE(decoded, nullptr) << "decode failed for type "
                                  << static_cast<int>(intent.type);
    EXPECT_EQ(encodeAction(*decoded, state), id);
}

} // namespace

// 1. EndTurn (type-only)
TEST(ActionEncoding, EndTurn) {
    MiniState ms;
    expectRoundTrip(Intent::endTurn(PlayerId::Player1), ms.state);
}

// 2. PassPriority (type-only)
TEST(ActionEncoding, PassPriority) {
    MiniState ms;
    expectRoundTrip(Intent::passPriority(PlayerId::Player1), ms.state);
}

// 3. PassFocus (type-only)
TEST(ActionEncoding, PassFocus) {
    MiniState ms;
    expectRoundTrip(Intent::passFocus(PlayerId::Player1), ms.state);
}

// 4. PlayCard (card + targets + dest_bf)
TEST(ActionEncoding, PlayCard) {
    MiniState ms;
    auto card  = ms.addObject(/*def_id=*/100);
    auto tgt1  = ms.addObject(/*def_id=*/200);
    auto tgt2  = ms.addObject(/*def_id=*/300);
    auto bf    = ms.addBattlefield();

    Intent i;
    i.type = IntentType::PlayCard;
    i.player = PlayerId::Player1;
    i.card = card;
    i.targets = {tgt1, tgt2};
    i.play_location = BattlefieldLocation{bf};
    expectRoundTrip(i, ms.state);
}

// 5. PlayActionCard (same shape as PlayCard but different type bit)
TEST(ActionEncoding, PlayActionCard) {
    MiniState ms;
    auto card = ms.addObject(/*def_id=*/101);

    Intent i;
    i.type = IntentType::PlayActionCard;
    i.player = PlayerId::Player1;
    i.card = card;
    expectRoundTrip(i, ms.state);
}

// 6. PlayReaction (same shape; reaction-phase)
TEST(ActionEncoding, PlayReaction) {
    MiniState ms;
    auto card = ms.addObject(/*def_id=*/102);
    auto tgt  = ms.addObject(/*def_id=*/250);

    Intent i;
    i.type = IntentType::PlayReaction;
    i.player = PlayerId::Player1;
    i.card = card;
    i.targets = {tgt};
    expectRoundTrip(i, ms.state);
}

// 7. StandardMove (multi-unit + dest_bf)
TEST(ActionEncoding, StandardMove) {
    MiniState ms;
    auto u1 = ms.addObject(/*def_id=*/400);
    auto u2 = ms.addObject(/*def_id=*/401);
    auto bf = ms.addBattlefield();

    Intent i = Intent::standardMove(PlayerId::Player1, {u1, u2},
                                     BattlefieldLocation{bf});
    expectRoundTrip(i, ms.state);
}

// 8. ActivateAbility (ability_source + ability_id + targets)
TEST(ActionEncoding, ActivateAbility) {
    MiniState ms;
    auto src  = ms.addObject(/*def_id=*/500);
    auto tgt  = ms.addObject(/*def_id=*/501);

    Intent i;
    i.type = IntentType::ActivateAbility;
    i.player = PlayerId::Player1;
    i.ability_source = src;
    i.ability = 7;
    i.targets = {tgt};
    expectRoundTrip(i, ms.state);
}

// 9. ActivateActionAbility (action-phase ability)
TEST(ActionEncoding, ActivateActionAbility) {
    MiniState ms;
    auto src = ms.addObject(/*def_id=*/502);

    Intent i;
    i.type = IntentType::ActivateActionAbility;
    i.player = PlayerId::Player1;
    i.ability_source = src;
    i.ability = 1;
    expectRoundTrip(i, ms.state);
}

// 10. MulliganDecision (cards + count)
TEST(ActionEncoding, MulliganDecision) {
    MiniState ms;
    auto c1 = ms.addObject(/*def_id=*/700);
    auto c2 = ms.addObject(/*def_id=*/701);

    Intent i = Intent::mulligan(PlayerId::Player1, {c1, c2});
    expectRoundTrip(i, ms.state);
}

// 11. AssignCombatDamage (damage assignments)
TEST(ActionEncoding, AssignCombatDamage) {
    MiniState ms;
    auto t1 = ms.addObject(/*def_id=*/620);
    auto t2 = ms.addObject(/*def_id=*/621);

    Intent i = Intent::assignCombatDamage(PlayerId::Player1, {
        DamageAssignment{t1, 3},
        DamageAssignment{t2, 2},
    });
    expectRoundTrip(i, ms.state);
}

// 12. HideCard (card + location)
TEST(ActionEncoding, HideCard) {
    MiniState ms;
    auto card = ms.addObject(/*def_id=*/345);
    auto bf   = ms.addBattlefield();

    Intent i;
    i.type = IntentType::HideCard;
    i.player = PlayerId::Player1;
    i.card = card;
    i.play_location = BattlefieldLocation{bf};
    expectRoundTrip(i, ms.state);
}

// 13. MakeChoice (chosen_objects + value)
TEST(ActionEncoding, MakeChoice) {
    MiniState ms;
    auto obj = ms.addObject(/*def_id=*/811);

    Intent i;
    i.type = IntentType::MakeChoice;
    i.player = PlayerId::Player1;
    i.chosen_objects = {obj};
    i.chosen_value = 4;
    expectRoundTrip(i, ms.state);
}

// 14. ChooseBattlefield
TEST(ActionEncoding, ChooseBattlefield) {
    MiniState ms;
    auto bf = ms.addBattlefield();
    Intent i = Intent::chooseBattlefield(PlayerId::Player1, bf);
    expectRoundTrip(i, ms.state);
}

// Bonus: distinct intents produce distinct encodings.
TEST(ActionEncoding, DistinctIntentsDistinctEncoding) {
    MiniState ms;
    auto c1 = ms.addObject(/*def_id=*/100);
    auto c2 = ms.addObject(/*def_id=*/200);

    Intent a;
    a.type = IntentType::PlayCard;
    a.player = PlayerId::Player1;
    a.card = c1;

    Intent b;
    b.type = IntentType::PlayCard;
    b.player = PlayerId::Player1;
    b.card = c2;

    EXPECT_NE(encodeAction(a, ms.state), encodeAction(b, ms.state));
}

// Bonus: type bit alone differentiates intents that otherwise share fields.
TEST(ActionEncoding, TypeBitDifferentiates) {
    MiniState ms;
    auto card = ms.addObject(/*def_id=*/100);

    Intent play;
    play.type = IntentType::PlayCard;
    play.player = PlayerId::Player1;
    play.card = card;

    Intent reaction;
    reaction.type = IntentType::PlayReaction;
    reaction.player = PlayerId::Player1;
    reaction.card = card;

    EXPECT_NE(encodeAction(play, ms.state), encodeAction(reaction, ms.state));
}

// Bonus: ActionID stays in 52 bits (fits int64).
TEST(ActionEncoding, IdInRange) {
    MiniState ms;
    auto card = ms.addObject(/*def_id=*/787);
    auto bf   = ms.addBattlefield();

    Intent i;
    i.type = IntentType::PlayCard;
    i.player = PlayerId::Player1;
    i.card = card;
    i.play_location = BattlefieldLocation{bf};
    int64_t id = encodeAction(i, ms.state);
    EXPECT_GE(id, 0);
    EXPECT_LT(id, kMaxActionId);
}
