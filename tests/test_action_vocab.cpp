/// Round-trip + structural tests for the dense action vocabulary.
///
/// Replaces the Phase B bit-packed 52-bit encoding with a fixed dense
/// [0, kVocabSize) slot vocabulary suitable for AlphaZero-style policy
/// heads.
///
/// Key design properties asserted here:
///   - All slots fit in [0, kVocabSize).
///   - Each verb bucket lives in a disjoint slot range.
///   - Distinct primary keys within a bucket produce distinct slots.
///   - Round-trip (encode → decode within a one-element legal list)
///     returns the original intent.
///   - Verb collapsing is intentional: PlayCard / PlayReaction /
///     PlayActionCard with the same card_def_id ALIAS to one slot.
///     Decode picks whichever variant is in the legal list.

#include <gtest/gtest.h>

#include "core/game_object.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "openspiel/action_vocab.h"

#include <vector>

using namespace riftbound;
using namespace riftbound::openspiel;

namespace {

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

void expectRoundTrip(const Intent& intent, const GameState& state) {
    int id = encodeAction(intent, state);
    ASSERT_GE(id, 0);
    ASSERT_LT(id, kVocabSize);
    std::vector<Intent> legal = {intent};
    const Intent* decoded = decodeAction(id, legal, state);
    ASSERT_NE(decoded, nullptr) << "decode failed for type "
                                  << static_cast<int>(intent.type);
    EXPECT_EQ(encodeAction(*decoded, state), id);
}

} // namespace

// ── Per-verb round-trip ─────────────────────────────────────────────────────

TEST(ActionVocab, EndTurn) {
    MiniState ms;
    expectRoundTrip(Intent::endTurn(PlayerId::Player1), ms.state);
}

TEST(ActionVocab, PassPriority) {
    MiniState ms;
    expectRoundTrip(Intent::passPriority(PlayerId::Player1), ms.state);
}

TEST(ActionVocab, PassFocus) {
    MiniState ms;
    expectRoundTrip(Intent::passFocus(PlayerId::Player1), ms.state);
}

TEST(ActionVocab, Concede) {
    MiniState ms;
    expectRoundTrip(Intent::concede(PlayerId::Player1), ms.state);
}

TEST(ActionVocab, PlayFirstDecision) {
    MiniState ms;
    expectRoundTrip(Intent::playFirst(PlayerId::Player1, true), ms.state);
    expectRoundTrip(Intent::playFirst(PlayerId::Player1, false), ms.state);
    EXPECT_NE(encodeAction(Intent::playFirst(PlayerId::Player1, true), ms.state),
              encodeAction(Intent::playFirst(PlayerId::Player1, false), ms.state));
}

TEST(ActionVocab, MulliganDecision) {
    MiniState ms;
    auto c1 = ms.addObject(/*def_id=*/700);
    auto c2 = ms.addObject(/*def_id=*/701);
    expectRoundTrip(Intent::mulligan(PlayerId::Player1, {}), ms.state);
    expectRoundTrip(Intent::mulligan(PlayerId::Player1, {c1}), ms.state);
    expectRoundTrip(Intent::mulligan(PlayerId::Player1, {c1, c2}), ms.state);
}

TEST(ActionVocab, ChooseBattlefield) {
    MiniState ms;
    auto bf = ms.addBattlefield();
    expectRoundTrip(Intent::chooseBattlefield(PlayerId::Player1, bf), ms.state);
}

TEST(ActionVocab, AssignCombatDamage) {
    MiniState ms;
    auto t1 = ms.addObject(/*def_id=*/620);
    auto t2 = ms.addObject(/*def_id=*/621);
    Intent i = Intent::assignCombatDamage(PlayerId::Player1, {
        DamageAssignment{t1, 3},
        DamageAssignment{t2, 2},
    });
    expectRoundTrip(i, ms.state);
}

TEST(ActionVocab, StandardMove) {
    MiniState ms;
    auto u1 = ms.addObject(/*def_id=*/400);
    auto u2 = ms.addObject(/*def_id=*/401);
    auto bf = ms.addBattlefield();
    Intent i = Intent::standardMove(PlayerId::Player1, {u1, u2},
                                     BattlefieldLocation{bf});
    expectRoundTrip(i, ms.state);
}

TEST(ActionVocab, PlayCard) {
    MiniState ms;
    auto card = ms.addObject(/*def_id=*/100);
    Intent i;
    i.type = IntentType::PlayCard;
    i.player = PlayerId::Player1;
    i.card = card;
    expectRoundTrip(i, ms.state);
}

TEST(ActionVocab, HideCard) {
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

TEST(ActionVocab, ActivateAbility) {
    MiniState ms;
    auto src = ms.addObject(/*def_id=*/500);
    auto tgt = ms.addObject(/*def_id=*/501);
    Intent i;
    i.type = IntentType::ActivateAbility;
    i.player = PlayerId::Player1;
    i.ability_source = src;
    i.ability = 7;
    i.targets = {tgt};
    expectRoundTrip(i, ms.state);
}

TEST(ActionVocab, MakeChoice) {
    MiniState ms;
    auto obj = ms.addObject(/*def_id=*/811);
    Intent i;
    i.type = IntentType::MakeChoice;
    i.player = PlayerId::Player1;
    i.chosen_objects = {obj};
    i.chosen_value = 4;
    expectRoundTrip(i, ms.state);
}

TEST(ActionVocab, MakeChoiceNoObject) {
    MiniState ms;
    Intent i;
    i.type = IntentType::MakeChoice;
    i.player = PlayerId::Player1;
    // chosen_value is intentionally unset here to exercise the "no
    // object, no int answer" → slot 0 path (the legacy decline / skip
    // encoding used by various card-pick MakeChoice intents that don't
    // carry an answer when the prompt is empty).
    expectRoundTrip(i, ms.state);
}

// ── Regression: confirmOptional yes/no must encode to distinct slots ───────
//
// Before kNumIntChoices was added to the MakeChoice arity, Card::
// confirmOptional encoded "yes" as chosen_objects={kYesSentinel=0}.
// defIdOf(0) returned 0, which collided with the "empty chosen_objects"
// → slot 0 encoding for "no". RiftboundState::LegalActions dedupes by
// encoded slot, so OpenSpiel only ever saw the FIRST intent — the no.
// Random / MCTS / AlphaZero agents could never pick "yes" through the
// vocab, causing 100% MAY_DECLINED on every Virtuoso prompt (and every
// other "may"-trigger card built on confirmOptional).
//
// This test fixes the encoding by going through Intent::chosen_value
// instead. Each int-coded answer (0=no, 1=yes) gets its own slot in
// the kNumIntChoices reserved range. If anyone re-introduces the
// sentinel-via-chosen_objects pattern this test will fail.
TEST(ActionVocab, ConfirmOptionalYesAndNoSlotsAreDistinct) {
    MiniState ms;
    Intent no_choice;
    no_choice.type = IntentType::MakeChoice;
    no_choice.player = PlayerId::Player1;
    no_choice.chosen_value = 0;
    Intent yes_choice;
    yes_choice.type = IntentType::MakeChoice;
    yes_choice.player = PlayerId::Player1;
    yes_choice.chosen_value = 1;

    int no_slot  = encodeAction(no_choice, ms.state);
    int yes_slot = encodeAction(yes_choice, ms.state);
    EXPECT_GE(no_slot, 0);
    EXPECT_GE(yes_slot, 0);
    EXPECT_NE(no_slot, yes_slot)
        << "confirmOptional no/yes collided on slot " << no_slot
        << " — OpenSpiel agents will never see the yes branch.";

    // And neither should collide with the legacy slot-0 (empty MakeChoice)
    // encoding used by other card paths.
    Intent empty_choice;
    empty_choice.type = IntentType::MakeChoice;
    empty_choice.player = PlayerId::Player1;
    int empty_slot = encodeAction(empty_choice, ms.state);
    EXPECT_NE(no_slot,  empty_slot);
    EXPECT_NE(yes_slot, empty_slot);
}

// ── Regression: pickMode mode indices must encode to distinct slots ────────
//
// Same class of bug as the confirmOptional case above, but for modal
// spells (Curtain Call, Rocket Barrage). Mode indices were stuffed into
// chosen_objects as (m+1) fake GameObjectIds. Modes 0, 1, 2, 3 mapped
// to chosen_objects={1}, {2}, {3}, {4} — which the encoder treated as
// "card with def_id=1" (= Bounty Hunter) / def_id=2 (= MF Captain) etc.
// The slots aliased to whatever real cards happened to share those def
// ids, and the policy head could not learn meaningful mode preferences.
TEST(ActionVocab, PickModeIndicesEncodeToDistinctSlots) {
    MiniState ms;
    std::vector<int> seen;
    for (int m = 0; m < 4; ++m) {
        Intent i;
        i.type = IntentType::MakeChoice;
        i.player = PlayerId::Player1;
        i.chosen_value = m;
        int slot = encodeAction(i, ms.state);
        EXPECT_GE(slot, 0);
        for (int prior : seen) {
            EXPECT_NE(slot, prior)
                << "pickMode index " << m << " collided with prior slot " << prior;
        }
        seen.push_back(slot);
    }
}

// ── Regression: int-coded slots don't alias card-keyed slots ───────────────
//
// chosen_value-based answers must live in a slot range disjoint from the
// card_def_id-based range, so a "yes" answer never accidentally encodes
// to "the slot for card_def_id=X" (which would let one agent decision
// silently mean two different things at different decision points).
TEST(ActionVocab, IntCodedMakeChoiceDisjointFromCardKeyedMakeChoice) {
    MiniState ms;
    auto obj = ms.addObject(/*def_id=*/200);

    Intent card_pick;
    card_pick.type = IntentType::MakeChoice;
    card_pick.player = PlayerId::Player1;
    card_pick.chosen_objects = {obj};
    int card_slot = encodeAction(card_pick, ms.state);

    for (int v = 0; v < 16; ++v) {
        Intent int_pick;
        int_pick.type = IntentType::MakeChoice;
        int_pick.player = PlayerId::Player1;
        int_pick.chosen_value = v;
        int int_slot = encodeAction(int_pick, ms.state);
        EXPECT_NE(int_slot, card_slot)
            << "chosen_value=" << v << " collides with card-keyed slot for def_id=200";
    }
}

TEST(ActionVocab, TriggerResponses) {
    MiniState ms;
    auto src = ms.addObject(/*def_id=*/600);
    for (IntentType type : {IntentType::PlaceOptionalTrigger,
                            IntentType::DeclineOptionalTrigger,
                            IntentType::PayTriggeredCost,
                            IntentType::DeclineTriggeredCost}) {
        Intent i;
        i.type = type;
        i.player = PlayerId::Player1;
        i.ability_source = src;
        expectRoundTrip(i, ms.state);
    }
}

// ── Structural properties ──────────────────────────────────────────────────

TEST(ActionVocab, SlotsInRange) {
    MiniState ms;
    auto card = ms.addObject(/*def_id=*/787);  // highest registered id
    Intent i;
    i.type = IntentType::PlayCard;
    i.player = PlayerId::Player1;
    i.card = card;
    int id = encodeAction(i, ms.state);
    EXPECT_GE(id, 0);
    EXPECT_LT(id, kVocabSize);
}

TEST(ActionVocab, DistinctCardsDistinctSlots) {
    MiniState ms;
    auto c1 = ms.addObject(/*def_id=*/100);
    auto c2 = ms.addObject(/*def_id=*/200);
    Intent a, b;
    a.type = b.type = IntentType::PlayCard;
    a.player = b.player = PlayerId::Player1;
    a.card = c1;
    b.card = c2;
    EXPECT_NE(encodeAction(a, ms.state), encodeAction(b, ms.state));
}

TEST(ActionVocab, VerbBucketsAreDisjoint) {
    MiniState ms;
    auto card = ms.addObject(/*def_id=*/100);

    Intent play;
    play.type = IntentType::PlayCard;
    play.player = PlayerId::Player1;
    play.card = card;

    Intent hide;
    hide.type = IntentType::HideCard;
    hide.player = PlayerId::Player1;
    hide.card = card;

    Intent activate;
    activate.type = IntentType::ActivateAbility;
    activate.player = PlayerId::Player1;
    activate.ability_source = card;

    int play_slot     = encodeAction(play, ms.state);
    int hide_slot     = encodeAction(hide, ms.state);
    int activate_slot = encodeAction(activate, ms.state);

    EXPECT_NE(play_slot, hide_slot);
    EXPECT_NE(play_slot, activate_slot);
    EXPECT_NE(hide_slot, activate_slot);
}

TEST(ActionVocab, CollapsingPlayVariantsAliasIntentionally) {
    // PlayCard / PlayReaction / PlayActionCard with same card_def_id are
    // semantically the same decision once the engine has decided the
    // appropriate context. The dense vocab collapses them. Decode picks
    // whichever variant appears in the legal-action list.
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

    EXPECT_EQ(encodeAction(play, ms.state), encodeAction(reaction, ms.state));
}

TEST(ActionVocab, VocabSizeIsReasonable) {
    // Should be ~7000 (≈ 9 buckets × 787 cards + small singletons).
    // Sanity: not insanely small (< 1000) and not insanely large (> 100k).
    EXPECT_GT(kVocabSize, 1000);
    EXPECT_LT(kVocabSize, 100000);
}
