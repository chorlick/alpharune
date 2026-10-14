/// Testing phase, batch 3: targets, finalization and what goes on the chain —
/// spells without targets, choices at resolution, counters, [Add], Equip and
/// Quick-Draw, payments (plans/10, Batch 3 and the items tagged "(batch 3)").
/// One test (or a few) per finding, through the engine path; the finding id is
/// in each test's name. Detail: docs/testing-findings-decks-2026-10.md (D..),
/// docs/testing-findings-fuzz-2026-10.md (F..), docs/testing-findings-games19-
/// 2026-10.md section 4 (G..), plans/reports/batch-1.md / -1b.md / -4b.md (B, J, K).

#include <gtest/gtest.h>

#include "card_test_fixture.h"
#include "cards/card_helpers.h"
#include "core/board_state.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace riftbound::test::batch_3 {

constexpr CardDefId kDefy = 45;
constexpr CardDefId kWindWall = 64;
constexpr CardDefId kMysticReversal = 80;
constexpr CardDefId kNotSoFast = 368;
constexpr CardDefId kHardBargain = 457;
constexpr CardDefId kRiposte = 520;
constexpr CardDefId kFlurryOfFeathers = 606;
constexpr CardDefId kRepulse = 668;
constexpr CardDefId kAbandon = 693;
constexpr CardDefId kLiltingLullaby = 750;
constexpr CardDefId kCrumblingSands = 826;
constexpr CardDefId kDredgeUp = 836;        // "Draw 1." [2]
constexpr CardDefId kRebuttal = 939;
constexpr CardDefId kPremonition = 409;     // "Draw 3."

/// The chain's newest item is an ability (a spell lies under it).
inline bool topIsAbility(const GameState& s) {
    return !s.chain.items.empty() && s.chain.items.back().is_ability;
}
/// Two spells are on the chain.
inline bool twoSpells(const GameState& s) {
    int n = 0;
    for (const auto& ci : s.chain.items) n += ci.is_spell ? 1 : 0;
    return n >= 2;
}

class Batch3Test : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    PlayerState& ps(PlayerId p) { return state.player(p); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }
    void fund(PlayerId p, int energy, int power) {
        ps(p).rune_pool.energy += energy;
        ps(p).rune_pool.universal_power += power;
    }
    void deckOf(PlayerId p, int n) {
        for (int i = 0; i < n; ++i) addToDeck(p, kDredgeUp);
    }
    /// A finalized spell of `controller` on the chain (the card on the chain,
    /// as a played spell is).
    GameObjectId spellOnChain(PlayerId controller, CardDefId def,
                              const std::vector<GameObjectId>& targets = {}) {
        const auto src = pushSpellOnChain(controller, def, targets);
        state.chain.items.back().status = ChainItemStatus::Finalized;
        o(src).owner = controller;
        return src;
    }
    /// A finalized triggered ability of `controller` on top of the chain,
    /// sourced by a unit with no card (it does nothing as it resolves).
    ChainItemId abilityOnChain(PlayerId controller, GameObjectId source,
                               const std::vector<GameObjectId>& chosen = {}) {
        ChainItem item;
        item.id = state.chain.allocateId();
        item.status = ChainItemStatus::Finalized;
        item.source = source;
        item.controller = controller;
        item.is_ability = true;
        item.chosen_objects = chosen;
        state.chain.items.push_back(item);
        return item.id;
    }
    bool onChain(GameObjectId src) {
        for (const auto& ci : state.chain.items)
            if (ci.source == src) return true;
        return false;
    }
};

/// An agent that plays `card` (as a reaction or a play) the first time it is
/// offered, answers prompts as an EnginePathAgent with `pick`, and otherwise
/// passes. `offered` says whether the play was ever offered.
class PlaysCard : public EnginePathAgent {
public:
    GameObjectId card = kInvalidId;
    bool offered = false;
    bool played = false;
    /// The play is counted as offered, and taken, only while this holds.
    std::function<bool(const GameState&)> when;
    PlaysCard(GameObjectId c, Picker p = nullptr) : EnginePathAgent(std::move(p)), card(c) {}
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        if (!when || when(state))
          for (const auto& i : legal)
            if (i.card == card && i.type != IntentType::MakeChoice &&
                (i.type == IntentType::PlayReaction || i.type == IntentType::PlayCard ||
                 i.type == IntentType::PlayActionCard)) {
                offered = true;
                if (!played) { played = true; return i; }
            }
        return EnginePathAgent::selectAction(state, legal);
    }
};

// ─── D21: a counter chooses the spell it counters (CR 355.9.a.2, 355.5) ─────
// Lower spell: Dredge Up (draw 1); upper spell: Premonition (draw 3) — what P2
// draws says which one resolved.

TEST_F(Batch3Test, D21_DefyIsOfferedAgainstASpellUnderAnAbility) {
    deckOf(P2, 5);
    auto unit = addUnit(P2, kInvalidId, 2, 0);
    auto spell = spellOnChain(P2, kDredgeUp);
    abilityOnChain(P2, unit);
    auto defy = addToHand(P1, kDefy);
    fund(P1, 5, 5);
    PlaysCard me(defy, EnginePathAgent::script({static_cast<int>(spell.raw()), EnginePathAgent::kRest}));
    me.when = topIsAbility;
    withEngine([&](GameEngine& e) { e.testHook_runChain(); }, &me, nullptr, P2);
    EXPECT_TRUE(me.offered) << "Defy can counter the spell under the ability (CR 355.9.a.2)";
    EXPECT_TRUE(inTrash(P2, spell));
    EXPECT_EQ(handSize(P2), 0) << "the spell was countered, not resolved";
}

TEST_F(Batch3Test, D21_WindWallCountersTheSpellItChoseNotTheTopOne) {
    deckOf(P2, 5);
    auto lower = spellOnChain(P2, kDredgeUp);
    auto upper = spellOnChain(P2, kPremonition);
    auto wall = addToHand(P1, kWindWall);
    fund(P1, 5, 5);
    PlaysCard me(wall, EnginePathAgent::script({static_cast<int>(lower.raw()), EnginePathAgent::kRest}));
    me.when = twoSpells;
    withEngine([&](GameEngine& e) { e.testHook_runChain(); }, &me, nullptr, P2);
    ASSERT_TRUE(me.played);
    EXPECT_EQ(handSize(P2), 3) << "the upper spell (draw 3) resolved; the lower one was countered";
    EXPECT_TRUE(inTrash(P2, lower));
    EXPECT_TRUE(inTrash(P2, upper));
}

TEST_F(Batch3Test, D21_TheCounterAsksWhichSpellAsItIsPlayed) {
    deckOf(P2, 5);
    auto lower = spellOnChain(P2, kDredgeUp);
    auto upper = spellOnChain(P2, kPremonition);
    auto wall = addToHand(P1, kWindWall);
    fund(P1, 5, 5);
    PlaysCard me(wall, EnginePathAgent::script({static_cast<int>(upper.raw()), EnginePathAgent::kRest}));
    me.when = twoSpells;
    withEngine([&](GameEngine& e) { e.testHook_runChain(); }, &me, nullptr, P2);
    bool asked_both = false;
    for (const auto& prompt : me.prompts) {
        std::set<GameObjectId> named;
        for (const auto& i : prompt)
            if (!i.chosen_objects.empty()) named.insert(i.chosen_objects.front());
        if (named == std::set<GameObjectId>{lower, upper}) asked_both = true;
    }
    EXPECT_TRUE(asked_both) << "both spells are on offer";
    EXPECT_EQ(handSize(P2), 1) << "the upper one was chosen and countered; the lower drew 1";
}

TEST_F(Batch3Test, D21_Pin_ACounteredCounterLeavesItsSpellOnTheChain) {
    // P2's spell A; P1's Wind Wall choosing A; P2's Wind Wall choosing P1's.
    deckOf(P2, 5);
    auto a = spellOnChain(P2, kDredgeUp);
    auto mine = addToHand(P1, kWindWall);
    auto theirs = addToHand(P2, kWindWall);
    fund(P1, 5, 5);
    fund(P2, 5, 5);
    PlaysCard me(mine, EnginePathAgent::script({static_cast<int>(a.raw()), EnginePathAgent::kRest}));
    PlaysCard them(theirs, EnginePathAgent::script({static_cast<int>(mine.raw()), EnginePathAgent::kRest}));
    them.when = [](const GameState& s) {
        return !s.chain.items.empty() && s.chain.items.back().controller == PlayerId::Player1;
    };
    withEngine([&](GameEngine& e) { e.testHook_runChain(); }, &me, &them, P2);
    ASSERT_TRUE(me.played);
    ASSERT_TRUE(them.played);
    EXPECT_TRUE(inTrash(P1, mine)) << "P1's Wind Wall was countered";
    EXPECT_EQ(handSize(P2), 1) << "A resolved (one draw)";
}

TEST_F(Batch3Test, D21_ACounterWhoseSpellLeftTheChainCountersNothing) {
    // P2's spells A (draw 1, lower) and B (draw 3, upper). P1 plays Defy
    // choosing A, then Wind Wall choosing A: Wind Wall counters A first; Defy
    // then mistargets and does not take B instead.
    deckOf(P2, 5);
    auto a = spellOnChain(P2, kDredgeUp);
    auto b = spellOnChain(P2, kPremonition);
    auto defy = addToHand(P1, kDefy);
    auto wall = addToHand(P1, kWindWall);
    fund(P1, 10, 10);
    class TwoPlays : public EnginePathAgent {
    public:
        std::vector<GameObjectId> cards;
        size_t next = 0;
        using EnginePathAgent::EnginePathAgent;
        Intent selectAction(const GameState& s, const std::vector<Intent>& legal) override {
            if (next < cards.size())
                for (const auto& i : legal)
                    if (i.card == cards[next] && i.type == IntentType::PlayReaction) {
                        ++next;
                        return i;
                    }
            return EnginePathAgent::selectAction(s, legal);
        }
    } me(EnginePathAgent::script(
        {static_cast<int>(a.raw()), static_cast<int>(a.raw()), EnginePathAgent::kRest}));
    me.cards = {defy, wall};
    withEngine([&](GameEngine& e) { e.testHook_runChain(); }, &me, nullptr, P2);
    ASSERT_EQ(me.next, 2u);
    EXPECT_TRUE(inTrash(P2, a));
    EXPECT_EQ(handSize(P2), 3) << "B resolved: Defy did not take it";
    EXPECT_TRUE(inTrash(P2, b));
}

TEST_F(Batch3Test, D21_NotSoFastIsOfferedAgainstAnAbilityUnderASpell) {
    // An enemy ability choosing P1's unit, with an enemy spell above it.
    deckOf(P2, 5);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto theirs = addUnit(P2, kInvalidId, 2, 0);
    abilityOnChain(P2, theirs, {mine});
    spellOnChain(P2, kDredgeUp);
    auto nsf = addToHand(P1, kNotSoFast);
    fund(P1, 5, 5);
    PlaysCard me(nsf, EnginePathAgent::script({EnginePathAgent::kRest}));
    me.when = [](const GameState& s) { return !s.chain.items.empty() && s.chain.items.back().is_spell; };
    withEngine([&](GameEngine& e) { e.testHook_runChain(); }, &me, nullptr, P2);
    EXPECT_TRUE(me.offered) << "the ability under the spell chooses a friendly unit";
}

TEST_F(Batch3Test, D21_RiposteChoosesTheSpellAndGetsItsCost) {
    deckOf(P2, 5);
    auto lower = spellOnChain(P2, kDredgeUp);   // Energy 2
    auto unit = addUnit(P2, kInvalidId, 2, 0);
    abilityOnChain(P2, unit);
    auto mine = addUnit(P1, kInvalidId, 3, 1);
    auto riposte = addToHand(P1, kRiposte);
    fund(P1, 5, 5);
    PlaysCard me(riposte, EnginePathAgent::script(
        {static_cast<int>(mine.raw()), static_cast<int>(lower.raw()), EnginePathAgent::kRest}));
    me.when = topIsAbility;
    withEngine([&](GameEngine& e) { e.testHook_runChain(); }, &me, nullptr, P2);
    ASSERT_TRUE(me.offered);
    EXPECT_TRUE(inTrash(P2, lower));
    EXPECT_EQ(handSize(P2), 0);
    EXPECT_EQ(o(mine).current_might, 5) << "+2 [M]: the countered spell's Energy cost";
}

TEST_F(Batch3Test, D21_HardBargainAsksWhichSpell) {
    deckOf(P2, 5);
    auto lower = spellOnChain(P2, kDredgeUp);
    spellOnChain(P2, kPremonition);
    auto hb = addToHand(P1, kHardBargain);
    fund(P1, 5, 5);
    // P2 has no Energy to pay [2]: the chosen spell is countered. (No Repeat.)
    PlaysCard me(hb, EnginePathAgent::script({static_cast<int>(lower.raw()), 0, EnginePathAgent::kRest}));
    me.when = twoSpells;
    withEngine([&](GameEngine& e) { e.testHook_runChain(); }, &me, nullptr, P2);
    ASSERT_TRUE(me.played);
    EXPECT_TRUE(inTrash(P2, lower));
    EXPECT_EQ(handSize(P2), 3) << "the upper one resolved";
}

TEST_F(Batch3Test, D21_TheOtherASpellCardsAreOfferedAgainstASpellUnderAnAbility) {
    for (CardDefId def : {kAbandon, kRebuttal, kLiltingLullaby, kCrumblingSands,
                          kFlurryOfFeathers, kMysticReversal, kWindWall, kRepulse, kHardBargain}) {
        SCOPED_TRACE(card_db.get(def).name);
        state = GameState{};
        state.players[0].id = P1;
        state.players[1].id = P2;
        addBattlefield();
        addBattlefield();
        deckOf(P2, 5);
        deckOf(P1, 5);
        ps(P2).cards_played_this_turn = 2;   // Crumbling Sands: "another spell this turn"
        auto mine = addUnit(P1, kInvalidId, 2, 0);
        spellOnChain(P2, kDredgeUp, def == kRepulse ? std::vector<GameObjectId>{mine}
                                                    : std::vector<GameObjectId>{});
        if (def == kRepulse) state.chain.items.back().chosen_objects = {mine};
        auto unit = addUnit(P2, kInvalidId, 2, 0);
        abilityOnChain(P2, unit);
        auto card = addToHand(P1, def);
        fund(P1, 10, 10);
        PlaysCard me(card, EnginePathAgent::script({EnginePathAgent::kRest}));
        me.when = topIsAbility;
        withEngine([&](GameEngine& e) { e.testHook_runChain(); }, &me, nullptr, P2);
        EXPECT_TRUE(me.offered);
    }
}

}  // namespace riftbound::test::batch_3
