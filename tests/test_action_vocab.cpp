/// Structural tests for the action vocabulary (openspiel/action_vocab.h).
///
/// An action id is a slot (what the action is made of) plus a twin number
/// (which of the legal intents with that description it is). Asserted here on
/// hand-built intents and legal lists:
///   - every slot fits in [0, kNumSlots) and every verb has a disjoint range;
///   - each sub-problem of plans/01 item 1.12 (mulligan, Standard Move,
///     "my X" / "their X", an object with no card id, the alternative-cost
///     play, the ability index, Equip) gets distinct slots;
///   - intents that do share a slot get distinct ids, in a canonical order;
///   - a history token replays to the intent it was made from.
/// The property over real legal lists is tests/test_action_ids.cpp.

#include <gtest/gtest.h>

#include "core/game_object.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "openspiel/action_vocab.h"

#include <set>
#include <vector>

using namespace riftbound;
using namespace riftbound::openspiel;

namespace {

struct MiniState {
    GameState state;

    GameObjectId addObject(CardDefId def_id, PlayerId owner = PlayerId::Player1) {
        const GameObjectId id = GameObjectId::fromRaw(next_id_++);
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
        bf.id = BattlefieldId::fromRaw(next_bf_++);
        state.battlefields.push_back(bf);
        return state.battlefields.back().id;
    }

private:
    uint32_t next_id_ = 1;
    uint32_t next_bf_ = 0;
};

void expectRoundTrip(const Intent& intent, const GameState& state) {
    int id = actionSlot(intent, state);
    ASSERT_GE(id, 0);
    ASSERT_LT(id, kNumSlots);
    // Alone in its legal list an intent's id is its slot.
    std::vector<Intent> legal = {intent};
    EXPECT_EQ(actionIds(legal, state), std::vector<int>{id});
    EXPECT_EQ(actionIdOf(intent, legal, state), id);
    const Intent* decoded = decodeAction(id, legal, state);
    ASSERT_NE(decoded, nullptr) << "decode failed for type "
                                  << static_cast<int>(intent.type);
    EXPECT_EQ(actionSlot(*decoded, state), id);
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
    EXPECT_NE(actionSlot(Intent::playFirst(PlayerId::Player1, true), ms.state),
              actionSlot(Intent::playFirst(PlayerId::Player1, false), ms.state));
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
    expectRoundTrip(Intent::chooseBattlefield(PlayerId::Player1,
                                              BattlefieldPoolIndex::fromRaw(1)), ms.state);
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
    i.ability = AbilityId::fromRaw(7);
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
    // No object, no value, no named card: GameEngine::askAgent refuses a
    // prompt with such an option, so no legal list holds one. It still has a
    // slot (the "another value" one), not another answer's.
    expectRoundTrip(i, ms.state);
    EXPECT_EQ(actionSlot(i, ms.state),
              verbOffset(ActionVerb::ChooseValue) + kNumIntChoices);
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

    int no_slot  = actionSlot(no_choice, ms.state);
    int yes_slot = actionSlot(yes_choice, ms.state);
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
    int empty_slot = actionSlot(empty_choice, ms.state);
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
        int slot = actionSlot(i, ms.state);
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
    int card_slot = actionSlot(card_pick, ms.state);

    for (int v = 0; v < 16; ++v) {
        Intent int_pick;
        int_pick.type = IntentType::MakeChoice;
        int_pick.player = PlayerId::Player1;
        int_pick.chosen_value = v;
        int int_slot = actionSlot(int_pick, ms.state);
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
    auto card = ms.addObject(/*def_id=*/kNumCardDefIds);  // top of the id space
    Intent i;
    i.type = IntentType::PlayCard;
    i.player = PlayerId::Player1;
    i.card = card;
    int id = actionSlot(i, ms.state);
    EXPECT_GE(id, 0);
    EXPECT_LT(id, kNumSlots);
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
    EXPECT_NE(actionSlot(a, ms.state), actionSlot(b, ms.state));
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

    int play_slot     = actionSlot(play, ms.state);
    int hide_slot     = actionSlot(hide, ms.state);
    int activate_slot = actionSlot(activate, ms.state);

    EXPECT_NE(play_slot, hide_slot);
    EXPECT_NE(play_slot, activate_slot);
    EXPECT_NE(hide_slot, activate_slot);
}

TEST(ActionVocab, PlayTimingVariantsShareASlot) {
    // PlayCard / PlayReaction / PlayActionCard of one card are the same
    // description: the timing is part of the state, not of the choice. (A
    // legal list that held two of them for one card object would still give
    // them two ids: they are different intents.)
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

    EXPECT_EQ(actionSlot(play, ms.state), actionSlot(reaction, ms.state));
}

TEST(ActionVocab, SizesArePinned) {
    // The numbers a consumer sizes things by. A change here is a break of
    // every recorded action id: list it in docs/checkpoint-breaking-*.md.
    EXPECT_EQ(kCardKeys, 2049);
    EXPECT_EQ(kNumSlots, 127124);
    EXPECT_EQ(kTwinPlanes, 1024);
    EXPECT_EQ(kNumActions, 130174976);
    EXPECT_EQ(verbOffset(ActionVerb::EndTurn), 1);
    EXPECT_EQ(verbOffset(ActionVerb::MulliganDecision), 7);
    EXPECT_EQ(verbOffset(ActionVerb::StandardMove), 69);
    EXPECT_EQ(verbOffset(ActionVerb::Play), 16461);
    EXPECT_EQ(verbOffset(ActionVerb::HideCard), 82029);
    EXPECT_EQ(verbOffset(ActionVerb::ActivateAbility), 98421);
    EXPECT_EQ(verbOffset(ActionVerb::ChooseOwnObject), 110715);
    EXPECT_EQ(verbOffset(ActionVerb::ChooseValue), 116862);
    for (int v = 0; v < static_cast<int>(ActionVerb::Count); ++v) {
        const auto verb = static_cast<ActionVerb>(v);
        EXPECT_EQ(slotVerb(verbOffset(verb)), verb);
        EXPECT_EQ(slotVerb(verbOffset(verb) + verbArity(verb) - 1), verb);
    }
    EXPECT_EQ(actionIdSlot(makeActionId(1234, 7)), 1234);
    EXPECT_EQ(actionIdTwin(makeActionId(1234, 7)), 7);
    EXPECT_EQ(makeActionId(1234, 0), 1234) << "the first intent of a slot keeps the slot as its id";
}

// ── plans/01 item 1.12, one test per sub-problem ────────────────────────────

namespace {

/// A hand of four for Player1 (cards 700, 701, 700, 702), a battlefield, and
/// a few units on both sides.
struct VocabTable : MiniState {
    GameObjectId h[4];
    BattlefieldId bf0, bf1;
    VocabTable() {
        const CardDefId defs[4] = {700, 701, 700, 702};
        for (int i = 0; i < 4; ++i) {
            h[i] = addObject(defs[i]);
            state.player(PlayerId::Player1).hand.push_back(h[i]);
        }
        bf0 = addBattlefield();
        bf1 = addBattlefield();
    }
};

void expectAllIdsDistinct(const std::vector<Intent>& legal, const GameState& state) {
    const std::vector<int> ids = actionIds(legal, state);
    for (size_t i = 0; i < legal.size(); ++i) {
        ASSERT_GE(ids[i], 0);
        ASSERT_LT(ids[i], kNumActions);
        for (size_t j = i + 1; j < legal.size(); ++j)
            EXPECT_NE(ids[i], ids[j]) << "legal[" << i << "] and legal[" << j << "]";
        EXPECT_EQ(decodeAction(ids[i], legal, state), &legal[i]);
        EXPECT_EQ(actionIdOf(legal[i], legal, state), ids[i]);
    }
}

} // namespace

TEST(ActionVocab, MulliganSlotsSayWhichCards) {
    VocabTable t;
    // The generator's list for a hand of four: keep, 4 singles, 6 pairs.
    std::vector<Intent> legal = {Intent::mulligan(PlayerId::Player1, {})};
    for (int i = 0; i < 4; ++i) legal.push_back(Intent::mulligan(PlayerId::Player1, {t.h[i]}));
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
            legal.push_back(Intent::mulligan(PlayerId::Player1, {t.h[i], t.h[j]}));
    std::set<int> slots;
    for (const auto& i : legal) {
        const int slot = actionSlot(i, t.state);
        EXPECT_EQ(slotVerb(slot), ActionVerb::MulliganDecision);
        slots.insert(slot);
    }
    EXPECT_EQ(slots.size(), legal.size()) << "11 choices, 11 slots: no twin number is needed";
    const int base = verbOffset(ActionVerb::MulliganDecision);
    EXPECT_EQ(actionSlot(legal[0], t.state), base);                 // keep
    EXPECT_EQ(actionSlot(legal[1], t.state), base + 1);             // hand position 0
    EXPECT_EQ(actionSlot(legal[4], t.state), base + 4);             // hand position 3
    EXPECT_EQ(actionSlot(legal[5], t.state), base + 9);             // positions (0, 1)
    EXPECT_EQ(actionSlot(legal[10], t.state), base + 9 + 7 + 6 + 0) // positions (2, 3)
        << "pairs are numbered row by row over 8 hand positions";
    // Two copies of one card in hand are two choices (it was "how many").
    EXPECT_NE(actionSlot(legal[1], t.state), actionSlot(legal[3], t.state));
    expectAllIdsDistinct(legal, t.state);
    // The slot is the unordered pair.
    EXPECT_EQ(actionSlot(Intent::mulligan(PlayerId::Player1, {t.h[2], t.h[0]}), t.state),
              actionSlot(Intent::mulligan(PlayerId::Player1, {t.h[0], t.h[2]}), t.state));
}

TEST(ActionVocab, StandardMoveSlotsSayWhichUnitAndWhereTo) {
    VocabTable t;
    auto a = t.addObject(400), b = t.addObject(401), a2 = t.addObject(400);
    const std::vector<Intent> legal = {
        Intent::standardMove(PlayerId::Player1, {a}, BattlefieldLocation{t.bf0}),
        Intent::standardMove(PlayerId::Player1, {a}, BattlefieldLocation{t.bf1}),
        Intent::standardMove(PlayerId::Player1, {b}, BattlefieldLocation{t.bf0}),
        Intent::standardMove(PlayerId::Player1, {b}, BaseLocation{PlayerId::Player1}),
        Intent::standardMove(PlayerId::Player1, {a2}, BattlefieldLocation{t.bf0}),
    };
    // Unit and destination are in the slot (it was the destination alone)...
    EXPECT_NE(actionSlot(legal[0], t.state), actionSlot(legal[1], t.state));
    EXPECT_NE(actionSlot(legal[0], t.state), actionSlot(legal[2], t.state));
    EXPECT_NE(actionSlot(legal[2], t.state), actionSlot(legal[3], t.state));
    EXPECT_EQ(actionSlot(legal[0], t.state),
              verbOffset(ActionVerb::StandardMove) + 400 * kPlaceKeys + 1);
    // ...and a second unit of the same card going the same way is a twin.
    EXPECT_EQ(actionSlot(legal[0], t.state), actionSlot(legal[4], t.state));
    const auto ids = actionIds(legal, t.state);
    EXPECT_EQ(ids[0], actionSlot(legal[0], t.state)) << "the lower object id is twin 0";
    EXPECT_EQ(ids[4], makeActionId(actionSlot(legal[0], t.state), 1));
    expectAllIdsDistinct(legal, t.state);
}

TEST(ActionVocab, APromptTellsMyCardFromTheirs) {
    // The mirror match: both players have a unit of card 300.
    VocabTable t;
    auto mine = t.addObject(300, PlayerId::Player1);
    auto theirs = t.addObject(300, PlayerId::Player2);
    auto mine2 = t.addObject(300, PlayerId::Player1);
    auto pick = [](GameObjectId o) {
        Intent i; i.type = IntentType::MakeChoice; i.player = PlayerId::Player1;
        i.chosen_objects = {o};
        return i;
    };
    EXPECT_EQ(actionSlot(pick(mine), t.state), verbOffset(ActionVerb::ChooseOwnObject) + 300);
    EXPECT_EQ(actionSlot(pick(theirs), t.state),
              verbOffset(ActionVerb::ChooseOpposingObject) + 300);
    // Asked of Player2, the same two objects swap ranges.
    Intent as_p2 = pick(mine);
    as_p2.player = PlayerId::Player2;
    EXPECT_EQ(actionSlot(as_p2, t.state), verbOffset(ActionVerb::ChooseOpposingObject) + 300);
    expectAllIdsDistinct({pick(mine), pick(theirs), pick(mine2)}, t.state);
}

TEST(ActionVocab, AnObjectWithNoCardIdHasAKeyOfItsOwn) {
    // A token with no card class has card_def_id kInvalidId. It used to fall
    // back to the first slot of the verb's range: in a prompt that was the
    // slot of "no object" (skip / decline), for an activation the slot of
    // card 1's first ability, for a play the slot of card 1.
    VocabTable t;
    auto token = t.addObject(kInvalidId);
    auto token2 = t.addObject(kInvalidId);
    auto card1 = t.addObject(1);
    Intent pick; pick.type = IntentType::MakeChoice; pick.player = PlayerId::Player1;
    pick.chosen_objects = {token};
    Intent pick2 = pick; pick2.chosen_objects = {token2};
    Intent skip; skip.type = IntentType::MakeChoice; skip.player = PlayerId::Player1;
    skip.chosen_value = 0;
    Intent pick_card1 = pick; pick_card1.chosen_objects = {card1};
    EXPECT_EQ(actionSlot(pick, t.state), verbOffset(ActionVerb::ChooseOwnObject) + 0);
    EXPECT_NE(actionSlot(pick, t.state), actionSlot(skip, t.state));
    EXPECT_NE(actionSlot(pick, t.state), actionSlot(pick_card1, t.state));
    expectAllIdsDistinct({skip, pick, pick2, pick_card1}, t.state);

    Intent act; act.type = IntentType::ActivateAbility; act.player = PlayerId::Player1;
    act.ability_source = token;
    Intent act_card1 = act; act_card1.ability_source = card1;
    EXPECT_NE(actionSlot(act, t.state), actionSlot(act_card1, t.state));
    Intent move_token = Intent::standardMove(PlayerId::Player1, {token}, BattlefieldLocation{t.bf0});
    Intent move_card1 = Intent::standardMove(PlayerId::Player1, {card1}, BattlefieldLocation{t.bf0});
    EXPECT_NE(actionSlot(move_token, t.state), actionSlot(move_card1, t.state));
}

TEST(ActionVocab, ThePlayOfACardSaysWhereAndHow) {
    VocabTable t;
    auto champ = t.addObject(250);
    Intent play; play.type = IntentType::PlayCard; play.player = PlayerId::Player1;
    play.card = champ;
    play.play_source = Intent::PlaySource::ChampionZone;
    play.play_location = BaseLocation{PlayerId::Player1};
    Intent alt = play; alt.use_alt_play_cost = true;         // Jhin, Meticulous Killer
    Intent at_bf = play; at_bf.play_location = BattlefieldLocation{t.bf1};
    Intent paid = at_bf; paid.pay_location_cost = true;      // Dragon Roost
    Intent replay = play; replay.play_source = Intent::PlaySource::Trash;
    const int base = verbOffset(ActionVerb::Play) + 250 * kPlaceKeys * 4;
    EXPECT_EQ(actionSlot(play, t.state), base + 0);
    EXPECT_EQ(actionSlot(alt, t.state), base + 1) << "the alternative-cost play is its own slot";
    EXPECT_EQ(actionSlot(at_bf, t.state), base + 2 * 4 + 0);
    EXPECT_EQ(actionSlot(paid, t.state), base + 2 * 4 + 2);
    EXPECT_EQ(actionSlot(replay, t.state), base + 3);
    // From hand and from the Champion Zone are both the normal play.
    Intent from_hand = play; from_hand.play_source = Intent::PlaySource::Hand;
    EXPECT_EQ(actionSlot(from_hand, t.state), actionSlot(play, t.state));
    expectAllIdsDistinct({play, alt, at_bf, paid, replay}, t.state);
}

TEST(ActionVocab, AnAbilityIndexIsNeverOverwritten) {
    VocabTable t;
    auto gear = t.addObject(500);
    t.state.getObject(gear).card_type = CardType::Gear;
    auto u1 = t.addObject(501), u2 = t.addObject(505), u3 = t.addObject(501);
    Intent own0; own0.type = IntentType::ActivateAbility; own0.player = PlayerId::Player1;
    own0.ability_source = gear;
    Intent own1 = own0; own1.ability_index = 1;
    Intent own3 = own0; own3.ability_index = 3;
    Intent equip1 = own0; equip1.targets = {u1};
    Intent equip2 = own0; equip2.targets = {u2};   // 505 % 4 == 501 % 4: the old hash bucket
    Intent equip3 = own0; equip3.targets = {u3};
    Intent granted = own0; granted.granted_ability_def = 506;
    const int base = verbOffset(ActionVerb::ActivateAbility) + 500 * kMaxAbilitiesPerCard;
    EXPECT_EQ(actionSlot(own0, t.state), base + 0);
    EXPECT_EQ(actionSlot(own1, t.state), base + 1);
    EXPECT_EQ(actionSlot(own3, t.state), base + 3);
    // Equip is a range of its own: a target never lands on an ability index
    // (unit 501 used to make "equip" the slot of ability 1).
    EXPECT_EQ(actionSlot(equip1, t.state), verbOffset(ActionVerb::ActivateAtTarget) + 500);
    EXPECT_EQ(actionSlot(equip2, t.state), actionSlot(equip1, t.state));
    EXPECT_EQ(actionSlot(granted, t.state), verbOffset(ActionVerb::ActivateGranted) + 506);
    expectAllIdsDistinct({own0, own1, own3, equip1, equip2, equip3, granted}, t.state);
    // The three timing types of one ability are one description.
    Intent reaction = own1; reaction.type = IntentType::ActivateReactionAbility;
    EXPECT_EQ(actionSlot(reaction, t.state), actionSlot(own1, t.state));
}

TEST(ActionVocab, PromptOptionsThatDifferInAnyFieldGetDistinctIds) {
    // Options that differ only in a field the slot does not read (the old
    // Party Favors 71 options differed only in `ability_index`) are twins,
    // not one action.
    VocabTable t;
    Intent a; a.type = IntentType::MakeChoice; a.player = PlayerId::Player1;
    a.chosen_value = 0;
    Intent b = a; b.ability_index = 1;
    Intent c = a; c.chosen_value = 40;      // past the 16 value slots
    Intent d = a; d.chosen_value = 41;
    Intent e = a; e.chosen_value = -2;
    EXPECT_EQ(actionSlot(a, t.state), actionSlot(b, t.state));
    EXPECT_EQ(actionSlot(c, t.state), actionSlot(d, t.state));
    expectAllIdsDistinct({a, b, c, d, e}, t.state);
}

TEST(ActionVocab, CombatDamageSlotIsTheNumberOfUnitsHit) {
    VocabTable t;
    auto x = t.addObject(620), y = t.addObject(621);
    const Intent focus = Intent::assignCombatDamage(PlayerId::Player1,
        {DamageAssignment{x, 5}, DamageAssignment{y, 0}});
    const Intent spread = Intent::assignCombatDamage(PlayerId::Player1,
        {DamageAssignment{x, 3}, DamageAssignment{y, 2}});
    const Intent spread2 = Intent::assignCombatDamage(PlayerId::Player1,
        {DamageAssignment{x, 2}, DamageAssignment{y, 3}});
    EXPECT_EQ(actionSlot(focus, t.state), verbOffset(ActionVerb::AssignCombatDamage) + 1);
    EXPECT_EQ(actionSlot(spread, t.state), verbOffset(ActionVerb::AssignCombatDamage) + 2);
    EXPECT_EQ(verbArity(ActionVerb::AssignCombatDamage), 16);
    expectAllIdsDistinct({focus, spread, spread2}, t.state);
}

// ── Twins: ids within one slot ──────────────────────────────────────────────

TEST(ActionVocab, TwinsAreNumberedInACanonicalOrderWhateverTheListOrder) {
    VocabTable t;
    Intent play; play.type = IntentType::PlayCard; play.player = PlayerId::Player1;
    std::vector<Intent> legal;
    std::vector<GameObjectId> targets;
    for (int i = 0; i < 6; ++i) targets.push_back(t.addObject(CardDefId::fromRaw(800 + i)));
    play.card = t.h[1];
    for (auto target : targets) {          // one spell, offered once per target
        Intent p = play;
        p.targets = {target};
        legal.push_back(p);
    }
    legal.push_back(Intent::endTurn(PlayerId::Player1));
    const int slot = actionSlot(legal[0], t.state);
    const auto ids = actionIds(legal, t.state);
    for (int i = 0; i < 6; ++i) EXPECT_EQ(ids[i], makeActionId(slot, i));
    EXPECT_EQ(ids[6], verbOffset(ActionVerb::EndTurn));

    // Any order of the list, and an option listed twice: the same ids.
    std::vector<Intent> other = {legal[4], legal[6], legal[1], legal[1], legal[5],
                                 legal[0], legal[3], legal[2]};
    const auto other_ids = actionIds(other, t.state);
    const int want[] = {4, -1, 1, 1, 5, 0, 3, 2};
    for (size_t k = 0; k < other.size(); ++k) {
        if (want[k] < 0) EXPECT_EQ(other_ids[k], verbOffset(ActionVerb::EndTurn));
        else EXPECT_EQ(other_ids[k], makeActionId(slot, want[k])) << k;
    }
    EXPECT_EQ(decodeAction(makeActionId(slot, 3), other, t.state), &other[6]);
    EXPECT_EQ(decodeAction(makeActionId(slot, 6), other, t.state), nullptr);
}

TEST(ActionVocab, CompareIntentsIsATotalOrderThatAgreesWithEquality) {
    VocabTable t;
    std::vector<Intent> v;
    Intent m; m.type = IntentType::MakeChoice; m.player = PlayerId::Player1;
    v.push_back(m);
    { Intent i = m; i.chosen_value = 0; v.push_back(i); }
    { Intent i = m; i.chosen_value = 1; v.push_back(i); }
    { Intent i = m; i.chosen_objects = {t.h[0]}; v.push_back(i); }
    { Intent i = m; i.chosen_objects = {t.h[0], t.h[1]}; v.push_back(i); }
    { Intent i = m; i.chosen_card_def = 235; v.push_back(i); }
    { Intent i = m; i.ability_index = 2; v.push_back(i); }
    { Intent i = m; i.player = PlayerId::Player2; v.push_back(i); }
    { Intent i = m; i.granted_ability_def = 9; v.push_back(i); }
    { Intent i = m; i.battlefield_pool_index = BattlefieldPoolIndex::fromRaw(0); v.push_back(i); }
    { Intent i = m; i.chosen_battlefield = t.bf0; v.push_back(i); }
    { Intent i = m; i.play_location = BaseLocation{PlayerId::Player1}; v.push_back(i); }
    { Intent i = m; i.play_location = BattlefieldLocation{t.bf0}; v.push_back(i); }
    { Intent i = m; i.use_alt_play_cost = true; v.push_back(i); }
    { Intent i = m; i.pay_location_cost = true; v.push_back(i); }
    { Intent i = m; i.choose_first = false; v.push_back(i); }
    { Intent i = m; i.play_source = Intent::PlaySource::Trash; v.push_back(i); }
    { Intent i = m; i.damage_assignments = {DamageAssignment{t.h[0], 1}}; v.push_back(i); }
    { Intent i = m; i.sideboard_swaps = {{t.h[0], t.h[1]}}; v.push_back(i); }
    { Intent i = m; i.cards_to_mulligan = {t.h[2]}; v.push_back(i); }
    { Intent i = m; i.units_to_move = {t.h[2]}; v.push_back(i); }
    { Intent i = m; i.move_destination = BaseLocation{PlayerId::Player2}; v.push_back(i); }
    { Intent i = m; i.card = t.h[3]; v.push_back(i); }
    { Intent i = m; i.ability_source = t.h[3]; v.push_back(i); }
    { Intent i = m; i.ability = AbilityId::fromRaw(3); v.push_back(i); }
    { Intent i = m; i.targets = {t.h[3]}; v.push_back(i); }
    v.push_back(Intent::endTurn(PlayerId::Player1));
    for (size_t a = 0; a < v.size(); ++a) {
        for (size_t b = 0; b < v.size(); ++b) {
            const int ab = compareIntents(v[a], v[b]);
            EXPECT_EQ(ab == 0, v[a] == v[b]) << a << " " << b;
            EXPECT_EQ(ab < 0, compareIntents(v[b], v[a]) > 0) << a << " " << b;
            for (size_t c = 0; c < v.size(); ++c) {
                if (ab < 0 && compareIntents(v[b], v[c]) < 0) {
                    EXPECT_LT(compareIntents(v[a], v[c]), 0) << a << " " << b << " " << c;
                }
            }
        }
    }
    // The display label is not part of an intent's identity.
    Intent labelled = v[1];
    labelled.choice_label = "No";
    EXPECT_EQ(compareIntents(labelled, v[1]), 0);
}

TEST(ActionVocab, PastTheTwinPlanesAnIntentHasNoId) {
    VocabTable t;
    std::vector<Intent> legal;
    Intent pick; pick.type = IntentType::MakeChoice; pick.player = PlayerId::Player1;
    for (int i = 0; i < kTwinPlanes + 3; ++i) {
        pick.chosen_objects = {t.addObject(kInvalidId)};   // tokens: one slot
        legal.push_back(pick);
    }
    const auto ids = actionIds(legal, t.state);
    const int slot = actionSlot(legal[0], t.state);
    EXPECT_EQ(ids[kTwinPlanes - 1], makeActionId(slot, kTwinPlanes - 1));
    EXPECT_LT(ids[kTwinPlanes - 1], kNumActions);
    for (int i = kTwinPlanes; i < kTwinPlanes + 3; ++i) {
        EXPECT_EQ(ids[i], kNoActionId);
        EXPECT_EQ(actionIdOf(legal[i], legal, t.state), kNoActionId);
    }
    // The engine still records and replays such a choice: a history token
    // does not hold the twin number.
    const int64_t token = historyToken(legal[kTwinPlanes + 1], t.state);
    EXPECT_EQ(historyTokenSlot(token), slot);
    EXPECT_EQ(findHistoryToken(token, legal, t.state), kTwinPlanes + 1);
}

// ── History tokens ──────────────────────────────────────────────────────────

TEST(ActionVocab, AHistoryTokenFindsItsIntentAmongTwins) {
    VocabTable t;
    auto c1 = t.h[0], c2 = t.h[1], c3 = t.h[2];   // c3: second copy of c1's card
    Intent play1; play1.type = IntentType::PlayCard;
    play1.player = PlayerId::Player1; play1.card = c1;
    Intent play3 = play1; play3.card = c3;               // other copy, same slot
    Intent play1_at_bf = play1;
    play1_at_bf.play_location = LocationId{BattlefieldLocation{t.bf0}};
    Intent alt = play1; alt.use_alt_play_cost = true;
    Intent target1; target1.type = IntentType::MakeChoice;
    target1.player = PlayerId::Player1; target1.chosen_objects = {c1};
    Intent target3 = target1; target3.chosen_objects = {c3};

    const std::vector<Intent> legal = {
        Intent::mulligan(PlayerId::Player1, {}),
        Intent::mulligan(PlayerId::Player1, {c1}),
        Intent::mulligan(PlayerId::Player1, {c2}),
        Intent::standardMove(PlayerId::Player1, {c1}, BattlefieldLocation{t.bf0}),
        Intent::standardMove(PlayerId::Player1, {c1, c2}, BattlefieldLocation{t.bf0}),
        play1, play3, play1_at_bf, alt, target1, target3,
    };
    expectAllIdsDistinct(legal, t.state);
    // Twins share a slot...
    EXPECT_EQ(actionSlot(play1, t.state), actionSlot(play3, t.state));
    EXPECT_EQ(actionSlot(target1, t.state), actionSlot(target3, t.state));
    EXPECT_EQ(actionSlot(legal[3], t.state), actionSlot(legal[4], t.state));
    // ...and every intent of the list still has its own token, which finds it.
    for (size_t i = 0; i < legal.size(); ++i) {
        const int64_t token = historyToken(legal[i], t.state);
        EXPECT_GE(token, 0);
        EXPECT_EQ(historyTokenSlot(token), actionSlot(legal[i], t.state));
        EXPECT_EQ(findHistoryToken(token, legal, t.state), static_cast<int>(i))
            << "legal[" << i << "]";
    }
    // A token for something not on offer matches nothing.
    EXPECT_EQ(findHistoryToken(
                  historyToken(Intent::endTurn(PlayerId::Player1), t.state),
                  legal, t.state),
              -1);
}

TEST(ActionVocab, HistoryTokenIgnoresTheDisplayLabel) {
    MiniState ms;
    Intent yes; yes.type = IntentType::MakeChoice;
    yes.player = PlayerId::Player1; yes.chosen_value = 1;
    Intent labelled = yes; labelled.choice_label = "Yes";
    EXPECT_EQ(historyToken(yes, ms.state), historyToken(labelled, ms.state));
    Intent no = yes; no.chosen_value = 0;
    EXPECT_NE(historyToken(yes, ms.state), historyToken(no, ms.state));
    // "no value" and "value 0" are different answers.
    Intent none = yes; none.chosen_value.reset();
    EXPECT_NE(historyToken(none, ms.state), historyToken(no, ms.state));
}
