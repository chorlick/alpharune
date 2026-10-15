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
#include <random>
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
constexpr CardDefId kPoutyPoro = 13;        // [Deflect]
constexpr CardDefId kLongSword = 345;
constexpr CardDefId kStarCrossed = 690;
constexpr CardDefId kSwitcheroo = 466;
constexpr CardDefId kFacebreaker = 220;
constexpr CardDefId kDefiantDance = 511;
constexpr CardDefId kStrikeDown = 429;
constexpr CardDefId kPublicExecution = 941;
constexpr CardDefId kRideTheWind = 173;
constexpr CardDefId kCharm = 43;
constexpr CardDefId kVoidAssault = 758;
constexpr CardDefId kShurikenFlip = 927;
constexpr CardDefId kThrillOfTheHunt = 745;
constexpr CardDefId kRelentlessPursuit = 503;
constexpr CardDefId kShowstopper = 265;
constexpr CardDefId kDragonsRage = 255;
constexpr CardDefId kStormbringer = 248;
constexpr CardDefId kDecreeOfInsight = 848;
constexpr CardDefId kPitRookie = 136;       // a Body unit
constexpr CardDefId kAzirSovereign = 497;
constexpr CardDefId kTornadoWarrior = 886;
constexpr CardDefId kEvelynnEntrancing = 703;
constexpr CardDefId kKennenStorm = 900;
constexpr CardDefId kZedFromTheShadows = 810;
constexpr CardDefId kEmperorsDivide = 366;
constexpr CardDefId kGuerillaWarfare = 260;
constexpr CardDefId kSteraksGage = 379;
constexpr CardDefId kHeartOfTheTempest = 942;
constexpr CardDefId kDefenderOfTomorrow = 936;
constexpr CardDefId kMatriarchOfWar = 940;
constexpr CardDefId kBountyHunter = 262;
constexpr CardDefId kDiviningShells = 723;
constexpr CardDefId kSwiftScout = 259;
constexpr CardDefId kForgeOfTheFuture = 212;
constexpr CardDefId kKennenKeeper = 922;
constexpr CardDefId kSinisterPoro = 699;
constexpr CardDefId kIcevaleArcher = 627;
constexpr CardDefId kTeemoStrategist = 121;
constexpr CardDefId kSealOfDiscord = 204;
constexpr CardDefId kDaughterOfTheVoid = 294;
constexpr CardDefId kScornOfTheMoon = 786;
constexpr CardDefId kFireBelow = 506;
constexpr CardDefId kGold564 = 564;
constexpr CardDefId kPlatewyrmEgg = 862;
constexpr CardDefId kLuxCrownguard = 312;
constexpr CardDefId kDragonsoulSage = 655;
constexpr CardDefId kEnergyConduit = 98;
constexpr CardDefId kHoneyfruit = 611;
constexpr CardDefId kDianaLunari = 641;
constexpr CardDefId kUndertitan = 495;
constexpr CardDefId kJhinMurderousArtist = 584;
constexpr CardDefId kWildClaw = 876;
constexpr CardDefId kZaunPunk = 481;
constexpr CardDefId kSacrifice = 735;
constexpr CardDefId kTheBoss = 264;
constexpr CardDefId kHeedlessResurrection = 704;
constexpr CardDefId kFizzTrickster = 461;
constexpr CardDefId kPunchFirst = 419;
constexpr CardDefId kSmokeScreen = 93;
constexpr CardDefId kSmite = 568;
constexpr CardDefId kVoidRush = 505;

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
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = ps(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        o(id).zone = ZoneType::Trash;
        ps(owner).trash.push_back(id);
        return id;
    }
    GameObjectId addGear(PlayerId owner, CardDefId def_id, bool at_base = true) {
        auto id = state.createObject();
        auto& g = o(id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.card_def_id = def_id;
        const auto& d = card_db.get(def_id);
        g.name = d.name;
        g.tags = d.tags;
        g.keywords = d.keywords;
        g.might_bonus = d.might_bonus;
        if (at_base) {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{owner};
        }
        return id;
    }
    GameObjectId gearOn(PlayerId owner, CardDefId def, GameObjectId unit) {
        auto id = state.createObject();
        auto& g = o(id);
        const auto& d = card_db.get(def);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.card_def_id = def;
        g.name = d.name;
        g.tags = d.tags;
        g.keywords = d.keywords;
        g.zone = o(unit).zone.get();
        g.location = o(unit).location.get();
        g.attached_to = unit;
        o(unit).attachments.push_back(id);
        return id;
    }
    /// Whether P1 is offered a play of `card` now (main phase, nothing on the chain).
    bool offered(GameObjectId card) {
        bool out = false;
        withEngine([&](GameEngine& e) {
            e.testHook_cleanup();
            for (const auto& i : e.generateLegalActions())
                if (i.card == card) out = true;
        });
        return out;
    }
    GameObjectId legendOf(PlayerId p, CardDefId def) {
        auto id = state.createObject();
        auto& l = o(id);
        const auto& d = card_db.get(def);
        l.owner = p;
        l.controller = p;
        l.card_def_id = def;
        l.name = d.name;
        l.card_type = d.card_type;
        l.zone = ZoneType::LegendZone;
        ps(p).legend_zone = id;
        return id;
    }
    /// Activate ability `index` of `source` the way a game does.
    void activate(GameObjectId source, AgentInterface* p1, AgentInterface* p2 = nullptr,
                  int index = 0, std::vector<GameObjectId> targets = {}) {
        Intent i;
        i.type = IntentType::ActivateAbility;
        i.player = o(source).controller;
        i.ability_source = source;
        i.ability_index = index;
        i.targets = std::move(targets);
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(i); settleLikeTheEngine(e); },
                   p1, p2, i.player);
    }
    /// Whether P1 is offered an activation of `source` now.
    bool activationOffered(GameObjectId source) {
        bool out = false;
        withEngine([&](GameEngine& e) {
            e.testHook_cleanup();
            for (const auto& i : e.generateLegalActions())
                if (i.ability_source == source &&
                    (i.type == IntentType::ActivateAbility ||
                     i.type == IntentType::ActivateActionAbility ||
                     i.type == IntentType::ActivateReactionAbility)) out = true;
        });
        return out;
    }
    bool onChain(GameObjectId src) {
        for (const auto& ci : state.chain.items)
            if (ci.source == src) return true;
        return false;
    }
};

/// P2's agent: passes, and counts the priority / Focus decisions it is given.
class CountsPriority : public EnginePathAgent {
public:
    int priority = 0;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) {
                ++priority;
                break;
            }
        return EnginePathAgent::selectAction(st, legal);
    }
};

/// P1's agent: answers prompts with `pick` and notes, for each prompt, how many
/// priority decisions P2 had been given before it, and whether anything was
/// on the chain then.
class NotesWhen : public EnginePathAgent {
public:
    const CountsPriority* other = nullptr;
    std::vector<int> other_priority_at;
    std::vector<size_t> chain_size_at;
    using EnginePathAgent::EnginePathAgent;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        const bool is_choice = !legal.empty() && std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return i.type == IntentType::MakeChoice; });
        if (is_choice && !isTriggerOrderPrompt(legal) && !isBeginPrompt(legal)) {
            other_priority_at.push_back(other ? other->priority : 0);
            chain_size_at.push_back(st.chain.items.size());
        }
        return EnginePathAgent::selectAction(st, legal);
    }
    /// The prompt (index) whose options include a value labelled with `text`.
    int promptWith(const std::string& text) const {
        for (size_t k = 0; k < prompts.size(); ++k)
            for (const auto& i : prompts[k])
                if (i.choice_label.find(text) != std::string::npos) return static_cast<int>(k);
        return -1;
    }
};

/// P1's agent that records, at its first prompt, what `look` reads.
class LooksAtFirstPrompt : public EnginePathAgent {
public:
    std::function<void(const GameState&)> look;
    bool looked = false;
    using EnginePathAgent::EnginePathAgent;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        const bool is_choice = !legal.empty() && std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return i.type == IntentType::MakeChoice; });
        if (is_choice && !looked && !isTriggerOrderPrompt(legal) && look) {
            looked = true;
            look(st);
        }
        return EnginePathAgent::selectAction(st, legal);
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

// ─── D8 / D17 / F7 / G20: offered only with a legal choice for every target ──
// (CR 355.8 "valid choices must be made for all targets"; 355.16.)

TEST_F(Batch3Test, D8_StarCrossedNeedsAFriendlyAndAnEnemyUnit) {
    auto card = addToHand(P1, kStarCrossed);
    fund(P1, 9, 9);
    addUnit(P1, kInvalidId, 2, 0);
    EXPECT_FALSE(offered(card)) << "no enemy unit";
    addUnit(P2, kInvalidId, 2, 1);
    EXPECT_TRUE(offered(card));
}

TEST_F(Batch3Test, D8_StarCrossedWithNoFriendlyUnitIsNotOffered) {
    auto card = addToHand(P1, kStarCrossed);
    fund(P1, 9, 9);
    addUnit(P2, kInvalidId, 2, 1);
    EXPECT_FALSE(offered(card));
}

TEST_F(Batch3Test, D8_SwitcherooNeedsTwoUnitsAtOneBattlefield) {
    auto card = addToHand(P1, kSwitcheroo);
    fund(P1, 9, 9);
    addUnit(P1, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 3, 1);
    EXPECT_FALSE(offered(card)) << "one unit at each battlefield";
    addUnit(P2, kInvalidId, 4, 1);
    EXPECT_TRUE(offered(card));
}

TEST_F(Batch3Test, D8_SwitcherooOffersAsItsFirstUnitOnlyOneWithAPartner) {
    auto card = addToHand(P1, kSwitcheroo);
    fund(P1, 9, 9);
    auto alone = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 3, 1);
    auto c = addUnit(P2, kInvalidId, 5, 1);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(b.raw()),
                                                  static_cast<int>(c.raw()), EnginePathAgent::kRest}));
    playThroughEngine(card, &agent);
    ASSERT_FALSE(agent.prompts.empty());
    EXPECT_FALSE(agent.wasOffered(alone)) << "it has no other unit at its battlefield (CR 355.16)";
    EXPECT_EQ(o(b).current_might, 5);
    EXPECT_EQ(o(c).current_might, 3);
}

TEST_F(Batch3Test, D8_DefiantDanceNeedsTwoUnits) {
    auto card = addToHand(P1, kDefiantDance);
    fund(P1, 9, 9);
    addUnit(P1, kInvalidId, 2, 0);
    EXPECT_FALSE(offered(card)) << "\"another unit\"";
    addUnit(P2, kInvalidId, 2, 0);
    EXPECT_TRUE(offered(card));
}

// Fuzz finding (seeds 5507695139391896804 16390680603618962191 235069363,
// random-player): a pair spell that declares no target requirements was
// offered on its cost alone, and its pass found every pair too dear with the
// enemy's Deflect. Defiant Dance [1][Calm/Chaos]: a friendly unit and an enemy
// [Deflect] unit, Energy and Power for the spell but not for the Deflect.
TEST_F(Batch3Test, D8_DefiantDanceIsNotOfferedWhenEveryPairCarriesADeflectItCannotPay) {
    auto card = addToHand(P1, kDefiantDance);
    fund(P1, 1, 1);
    addUnit(P1, kInvalidId, 2, 0);
    addUnit(P2, kPoutyPoro, 0, 0);
    EXPECT_FALSE(offered(card)) << "the only pair chooses the [Deflect] unit (CR 355.8, 809)";
    fund(P1, 0, 1);
    EXPECT_TRUE(offered(card)) << "with the Deflect's [A] it is payable";
}

TEST_F(Batch3Test, D8_FacebreakerNeedsAFriendlyAndAnEnemyUnitAtOneBattlefield) {
    auto card = addToHand(P1, kFacebreaker);
    fund(P1, 9, 9);
    addUnit(P1, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2, 1);
    EXPECT_FALSE(offered(card));
    addUnit(P2, kInvalidId, 2, 0);
    EXPECT_TRUE(offered(card));
}

TEST_F(Batch3Test, F7_StrikeDownNeedsAnEnemyUnit) {
    auto card = addToHand(P1, kStrikeDown);
    fund(P1, 9, 9);
    auto mine = addUnit(P1, kInvalidId, 3, 0);
    gearOn(P1, kLongSword, mine);
    EXPECT_FALSE(offered(card)) << "an equipped friendly unit and no enemy unit";
    addUnit(P2, kInvalidId, 2, 1);
    EXPECT_TRUE(offered(card));
}

TEST_F(Batch3Test, G20_PublicExecutionNeedsAnEnemyWithLessMight) {
    auto card = addToHand(P1, kPublicExecution);
    fund(P1, 9, 9);
    addUnit(P1, kInvalidId, 6, 0);
    addUnit(P2, kInvalidId, 6, 1);
    EXPECT_FALSE(offered(card)) << "the enemy has as much Might as the strongest friendly unit";
    addUnit(P2, kInvalidId, 5, 1);
    EXPECT_TRUE(offered(card));
}

TEST_F(Batch3Test, G20_PublicExecutionOffersOnlyAFriendlyUnitThatHasAWeakerEnemy) {
    auto card = addToHand(P1, kPublicExecution);
    fund(P1, 9, 9);
    auto weak = addUnit(P1, kInvalidId, 2, 0);
    auto strong = addUnit(P1, kInvalidId, 6, 0);
    auto victim = addUnit(P2, kInvalidId, 4, 1);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(strong.raw()),
                                                  static_cast<int>(victim.raw()), EnginePathAgent::kRest}));
    playThroughEngine(card, &agent);
    EXPECT_FALSE(agent.wasOffered(weak)) << "no enemy has less Might than it (CR 355.16)";
    EXPECT_FALSE(onBoard(victim));
}

TEST_F(Batch3Test, D79_SwitcherooChargesDeflectOnlyForTheDeflectUnitChosen) {
    auto card = addToHand(P1, kSwitcheroo);
    fund(P1, 2, 2);
    for (int i = 0; i < 2; ++i) addRune(P1, Domain::Chaos);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto poro = addUnit(P2, kPoutyPoro, 0, 0);
    auto other = addUnit(P2, kInvalidId, 4, 0);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(mine.raw()),
                                                  static_cast<int>(other.raw()), EnginePathAgent::kRest}));
    playThroughEngine(card, &agent, std::nullopt, nullptr, /*fund=*/false);
    EXPECT_EQ(readyRuneCount(P1), 2) << "no Deflect paid: the Deflect unit was not chosen";
    EXPECT_TRUE(agent.wasOffered(poro));
}

TEST_F(Batch3Test, D79_SwitcherooChoosingTheDeflectUnitPaysItOnce) {
    auto card = addToHand(P1, kSwitcheroo);
    fund(P1, 2, 2);
    for (int i = 0; i < 3; ++i) addRune(P1, Domain::Chaos);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto poro = addUnit(P2, kPoutyPoro, 0, 0);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(mine.raw()),
                                                  static_cast<int>(poro.raw()), EnginePathAgent::kRest}));
    playThroughEngine(card, &agent, std::nullopt, nullptr, /*fund=*/false);
    int on_board = 0;
    for (auto& [id, ob] : state.objects) if (ob.isRune() && ob.location.has_value()) ++on_board;
    EXPECT_EQ(on_board, 2) << "one rune recycled for the one Deflect";
}

// ─── D3 / D16 / D17 / D34: a spell's choices are made as it is played ───────
// (CR 355.4: "choose a valid Location as the Move Destination for each Move";
// 355.5 objects; both in step 2, before the cost and before anyone responds.)

TEST_F(Batch3Test, D3_RideTheWindChoosesItsDestinationBeforeAnyoneResponds) {
    auto card = addToHand(P1, kRideTheWind);
    auto unit = addUnit(P1, kInvalidId, 2);
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(unit.raw()), 2, EnginePathAgent::kRest}));
    me.other = &them;
    playThroughEngine(card, &me, std::nullopt, &them);
    const int k = me.promptWith("Battlefield #1");
    ASSERT_GE(k, 0) << "a destination was asked";
    EXPECT_EQ(me.other_priority_at[k], 0) << "the destination is chosen before P2 can respond";
    EXPECT_EQ(me.chain_size_at[k], 0u) << "...as the spell is played, before its cost";
    EXPECT_EQ(o(unit).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(Batch3Test, D3_CharmChoosesItsDestinationAsItIsPlayed) {
    auto card = addToHand(P1, kCharm);
    auto enemy = addUnit(P2, kInvalidId, 2, 0);
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(enemy.raw()), 0, EnginePathAgent::kRest}));
    me.other = &them;
    playThroughEngine(card, &me, std::nullopt, &them);
    const int k = me.promptWith("Base");
    ASSERT_GE(k, 0);
    EXPECT_EQ(me.other_priority_at[k], 0);
    EXPECT_TRUE(o(enemy).isAtBase());
}

TEST_F(Batch3Test, D3_VoidAssaultChoosesTheDestinationsBeforeAnyoneResponds) {
    auto card = addToHand(P1, kVoidAssault);
    auto mine = addUnit(P1, kInvalidId, 2);
    auto theirs = addUnit(P2, kInvalidId, 2);
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({EnginePathAgent::kRest}));
    me.other = &them;
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = card;
    play.targets = {mine, theirs};
    fund(P1, 5, 5);
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); settleLikeTheEngine(e); },
               &me, &them, P1);
    const int k = me.promptWith("yours:");
    ASSERT_GE(k, 0);
    EXPECT_EQ(me.other_priority_at[k], 0);
}

TEST_F(Batch3Test, D3_ShowstopperDragonsRageStormbringerChooseBeforeAnyoneResponds) {
    for (CardDefId def : {kShowstopper, kDragonsRage, kStormbringer}) {
        SCOPED_TRACE(card_db.get(def).name);
        state = GameState{};
        state.players[0].id = P1;
        state.players[1].id = P2;
        addBattlefield();
        addBattlefield();
        auto card = addToHand(P1, def);
        addUnit(P1, kInvalidId, 3);           // in base
        addUnit(P2, kInvalidId, 2, 0);
        addUnit(P2, kInvalidId, 2, 1);
        CountsPriority them;
        NotesWhen me(EnginePathAgent::script({EnginePathAgent::kRest}));
        me.other = &them;
        playThroughEngine(card, &me, std::nullopt, &them);
        const int k = me.promptWith("Battlefield #");
        ASSERT_GE(k, 0) << "a destination / battlefield was asked";
        EXPECT_EQ(me.other_priority_at[k], 0);
    }
}

TEST_F(Batch3Test, D3_RelentlessPursuitChoosesBeforeAnyoneResponds) {
    auto card = addToHand(P1, kRelentlessPursuit);
    auto unit = addUnit(P1, kInvalidId, 3);
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(unit.raw()), EnginePathAgent::kRest}));
    me.other = &them;
    playThroughEngine(card, &me, std::nullopt, &them);
    const int k = me.promptWith("Battlefield #");
    ASSERT_GE(k, 0);
    EXPECT_EQ(me.other_priority_at[k], 0);
}

TEST_F(Batch3Test, D3_Pin_ThrillOfTheHuntChoosesTheBattlefieldAsItPlaysTheUnit) {
    // NOT A DEFECT: the battlefield is where the owner plays the unit, a
    // permission of that play (CR 355.10.b: "Play a unit ... to a battlefield
    // doesn't target a battlefield"), chosen as the play happens.
    auto card = addToHand(P1, kThrillOfTheHunt);
    auto unit = addUnit(P1, kPoutyPoro, 0);
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(unit.raw()), EnginePathAgent::kRest}));
    me.other = &them;
    playThroughEngine(card, &me, std::nullopt, &them);
    int k = -1;
    for (size_t i = 0; i < me.prompts.size(); ++i)
        if (me.prompts[i].size() == 2 && me.prompts[i][0].chosen_value.has_value()) k = static_cast<int>(i);
    ASSERT_GE(k, 0) << "a battlefield was asked";
    EXPECT_GT(me.other_priority_at[k], 0) << "asked as the spell resolves";
}

TEST_F(Batch3Test, D16_ShurikenFlipChoosesEverythingAsItIsPlayed) {
    auto card = addToHand(P1, kShurikenFlip);
    auto mine = addUnit(P1, kInvalidId, 2);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(enemy.raw()), static_cast<int>(mine.raw()),
                                          2, EnginePathAgent::kRest}));
    me.other = &them;
    playThroughEngine(card, &me, std::nullopt, &them);
    ASSERT_GE(me.other_priority_at.size(), 3u);
    for (size_t k = 0; k < 3; ++k) EXPECT_EQ(me.other_priority_at[k], 0) << "prompt " << k;
    EXPECT_EQ(o(enemy).damage_marked, 2);
    EXPECT_EQ(o(mine).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(Batch3Test, D17_ShurikenFlipIsNotOfferedWithNoFriendlyUnitToMove) {
    auto card = addToHand(P1, kShurikenFlip);
    fund(P1, 5, 5);
    addUnit(P2, kInvalidId, 3, 0);
    EXPECT_FALSE(offered(card));
    addUnit(P1, kInvalidId, 2);
    EXPECT_TRUE(offered(card));
}

TEST_F(Batch3Test, D34_DecreeOfInsightChoosesItsTargetBeforeItsCost) {
    auto card = addToHand(P1, kDecreeOfInsight);
    auto rookie = addUnit(P2, kPitRookie, 0, 0);
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(rookie.raw()), EnginePathAgent::kRest}));
    me.other = &them;
    playThroughEngine(card, &me, std::nullopt, &them);
    ASSERT_FALSE(me.chain_size_at.empty());
    EXPECT_EQ(me.chain_size_at[0], 0u) << "the target is chosen before the spell is on the chain (CR 355.5)";
}

// ─── D22 / D14 / D24 / D54 / D78 / B5 / G17 / D67: a triggered ability's and
// a spell's choices are made as it is finalized (CR 383.3, 403.1-403.2, 355.5)

TEST_F(Batch3Test, D22_AWeaponmasterTokensMayAndEquipmentAreChosenBeforeAnyoneResponds) {
    auto sword = addGear(P1, kLongSword, true);
    (void)sword;
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    me.other = &them;
    resolveThroughEngine([&](EffectExecutor& ex, GameState&) {
        KeywordSet kw;
        kw.set(Keyword::Weaponmaster);
        ex.createToken(P1, CardType::Unit, "Sand Soldier", 2, {"Sand Soldier"}, kw,
                       LocationId{BaseLocation{P1}});
    }, &me, &them, P1);
    ASSERT_GE(me.other_priority_at.size(), 2u) << "the may and the Equipment";
    EXPECT_EQ(me.other_priority_at[0], 0);
    EXPECT_EQ(me.other_priority_at[1], 0);
}

TEST_F(Batch3Test, D14_QuickDrawIsATriggeredAbilityOnTheChain) {
    // CR 819.1.a "Quick-Draw is a Triggered Ability keyword"; 819.1.d "When you
    // play this, attach it to a Unit you control": the unit is chosen as the
    // ability is finalized; the opponent may respond before it attaches.
    auto unit = addUnit(P1, kInvalidId, 2);
    auto gage = addToHand(P1, kSteraksGage);
    bool unattached_at_priority = false;
    class Watch : public CountsPriority {
    public:
        GameObjectId gear = kInvalidId;
        bool* seen = nullptr;
        Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
            if (!st.chain.items.empty() && st.objectExists(gear) &&
                st.getObject(gear).location.has_value() &&
                !st.getObject(gear).attached_to.has_value())
                *seen = true;
            return CountsPriority::selectAction(st, legal);
        }
    } them;
    them.gear = gage;
    them.seen = &unattached_at_priority;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(unit.raw()), EnginePathAgent::kRest}));
    me.other = &them;
    playThroughEngine(gage, &me, std::nullopt, &them);
    EXPECT_TRUE(unattached_at_priority) << "the attach waits on the chain";
    ASSERT_FALSE(me.other_priority_at.empty());
    EXPECT_EQ(me.other_priority_at[0], 0) << "the unit is chosen before the opponent responds";
    EXPECT_EQ(o(gage).attached_to, std::optional<GameObjectId>{unit});
}

TEST_F(Batch3Test, D24_AzirChoosesTheTokensBeforeAnyoneResponds) {
    auto azir = addUnit(P1, kAzirSovereign, 0, 0);
    addUnit(P2, kInvalidId, 2, 0);
    combatInProgress(0, P1);
    auto token = addUnit(P1, kInvalidId, 1);
    o(token).super_type = SuperType::Token;
    o(token).card_type = CardType::Unit;
    o(token).name = "Sand Soldier";
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(token.raw()), 0, EnginePathAgent::kRest}));
    me.other = &them;
    fireTriggerThroughEngine(azir, TriggerType::WhenIAttack, &me, kInvalidId, &them);
    ASSERT_FALSE(me.other_priority_at.empty());
    EXPECT_EQ(me.other_priority_at[0], 0);
}

TEST_F(Batch3Test, D54_TornadoWarriorChoosesBeforeAnyoneResponds) {
    auto tw = addUnit(P1, kTornadoWarrior, 0, 0);
    o(tw).card_counters["__played_from_facedown"] = 1;
    o(tw).card_counters["__facedown_play_turn"] = state.turn.turn_number;
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({1, static_cast<int>(tw.raw()), EnginePathAgent::kRest}));
    me.other = &them;
    fireTriggerThroughEngine(tw, TriggerType::WhenYouPlayMe, &me, kInvalidId, &them);
    ASSERT_GE(me.other_priority_at.size(), 2u);
    EXPECT_EQ(me.other_priority_at[0], 0);
    EXPECT_EQ(me.other_priority_at[1], 0);
    EXPECT_EQ(o(tw).empowered, 1);
}

TEST_F(Batch3Test, D54_EvelynnChoosesBeforeAnyoneResponds) {
    auto ev = addUnit(P1, kEvelynnEntrancing, 0, 0);
    auto enemy = addUnit(P2, kInvalidId, 2, 1);
    o(ev).card_counters["__played_from_facedown"] = 1;
    o(ev).card_counters["__facedown_play_turn"] = state.turn.turn_number;
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({1, static_cast<int>(enemy.raw()), EnginePathAgent::kRest}));
    me.other = &them;
    fireTriggerThroughEngine(ev, TriggerType::WhenYouPlayMe, &me, kInvalidId, &them);
    ASSERT_GE(me.other_priority_at.size(), 2u);
    EXPECT_EQ(me.other_priority_at[0], 0);
    EXPECT_EQ(me.other_priority_at[1], 0);
    EXPECT_EQ(o(enemy).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

TEST_F(Batch3Test, D78_KennenChoosesTheSpellBeforeAnyoneResponds) {
    auto kennen = addUnit(P1, kKennenStorm, 0, 0);
    auto spell = addToTrash(P1, kDredgeUp);
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(spell.raw()), EnginePathAgent::kRest}));
    me.other = &them;
    fireTriggerThroughEngine(kennen, TriggerType::WhenIConquer, &me, kInvalidId, &them);
    ASSERT_FALSE(me.other_priority_at.empty());
    EXPECT_EQ(me.other_priority_at[0], 0);
}

TEST_F(Batch3Test, B5_ZedChoosesWhereTheShadowCloneGoesBeforeAnyoneResponds) {
    auto zed = addUnit(P1, kZedFromTheShadows, 0, 0);
    state.battlefields[0].controller = P1;
    o(zed).card_counters["zed_discard_paid"] = 1;
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({EnginePathAgent::kRest}));
    me.other = &them;
    fireTriggerThroughEngine(zed, TriggerType::WhenYouPlayMe, &me, kInvalidId, &them);
    ASSERT_FALSE(me.other_priority_at.empty()) << "base or the battlefield P1 controls";
    EXPECT_EQ(me.other_priority_at[0], 0);
}

TEST_F(Batch3Test, G17_EmperorsDivideChoosesTheUnitsAsItIsPlayed) {
    auto card = addToHand(P1, kEmperorsDivide);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 3, 0);
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(b.raw()), 0, EnginePathAgent::kRest}));
    me.other = &them;
    playThroughEngine(card, &me, std::nullopt, &them);
    ASSERT_FALSE(me.other_priority_at.empty());
    EXPECT_EQ(me.other_priority_at[0], 0) << "the units are chosen before anyone responds";
    EXPECT_TRUE(o(b).isAtBase());
    EXPECT_FALSE(o(a).isAtBase()) << "only the unit chosen moves";
}

TEST_F(Batch3Test, D67_GuerillaWarfareChoosesTheCardsAsItIsPlayed) {
    auto card = addToHand(P1, kGuerillaWarfare);
    auto hidden = addToTrash(P1, kEmperorsDivide);   // a card with [Hidden]
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(hidden.raw()), EnginePathAgent::kRest}));
    me.other = &them;
    playThroughEngine(card, &me, std::nullopt, &them);
    ASSERT_FALSE(me.other_priority_at.empty());
    EXPECT_EQ(me.other_priority_at[0], 0) << "the trash is public: the cards are targets (CR 355.10.a.1)";
    EXPECT_TRUE(inHand(P1, hidden));
}

// ─── G15 / D38 / D15 / G19 / D67 / D41: an activated ability is put on the
// chain, makes its choices, then pays (CR 402.1, 403.2, 404.2, 405.1) ────────

TEST_F(Batch3Test, G15_HeartOfTheTempestChoosesBeforeItDisempowersAndExhausts) {
    auto heart = legendOf(P1, kHeartOfTheTempest);
    o(heart).empowered = 1;
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    bool exhausted = true;
    int empowered = 0;
    LooksAtFirstPrompt me(EnginePathAgent::script({static_cast<int>(unit.raw()), EnginePathAgent::kRest}));
    me.look = [&](const GameState& st) {
        exhausted = st.getObject(heart).is_exhausted;
        empowered = st.getObject(heart).empowered;
    };
    activate(heart, &me);
    ASSERT_TRUE(me.looked);
    EXPECT_FALSE(exhausted) << "the target is chosen before the cost is paid";
    EXPECT_EQ(empowered, 1);
    EXPECT_TRUE(o(heart).is_exhausted);
}

TEST_F(Batch3Test, G15_BountyHunterChoosesBeforeItExhausts) {
    auto bh = legendOf(P1, kBountyHunter);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    bool exhausted = true;
    LooksAtFirstPrompt me(EnginePathAgent::script({static_cast<int>(unit.raw()), EnginePathAgent::kRest}));
    me.look = [&](const GameState& st) { exhausted = st.getObject(bh).is_exhausted; };
    activate(bh, &me);
    ASSERT_TRUE(me.looked);
    EXPECT_FALSE(exhausted);
}

TEST_F(Batch3Test, G15_MatriarchOfWarChoosesBeforeItPays) {
    auto m = legendOf(P1, kMatriarchOfWar);
    o(m).empowered = 1;
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    o(unit).is_exhausted = true;
    fund(P1, 0, 1);
    int power = -1;
    LooksAtFirstPrompt me(EnginePathAgent::script({static_cast<int>(unit.raw()), EnginePathAgent::kRest}));
    me.look = [&](const GameState& st) { power = st.player(P1).rune_pool.universal_power; };
    activate(m, &me);
    ASSERT_TRUE(me.looked);
    EXPECT_EQ(power, 1) << "the [A] is paid after the unit is chosen";
    EXPECT_FALSE(o(unit).is_exhausted);
}

TEST_F(Batch3Test, G15_DefenderOfTomorrowChoosesBeforeItPays) {
    auto d = legendOf(P1, kDefenderOfTomorrow);
    auto gear = addGear(P1, kLongSword);
    o(gear).is_exhausted = true;
    fund(P1, 1, 0);
    int energy = -1;
    LooksAtFirstPrompt me(EnginePathAgent::script({static_cast<int>(gear.raw()), EnginePathAgent::kRest}));
    me.look = [&](const GameState& st) { energy = st.player(P1).rune_pool.energy; };
    activate(d, &me);
    ASSERT_TRUE(me.looked);
    EXPECT_EQ(energy, 1) << "the [1] is paid after the gear is chosen";
    EXPECT_FALSE(o(gear).is_exhausted);
}

TEST_F(Batch3Test, G19_DefenderOfTomorrowIsNotOfferedWithNoGear) {
    auto d = legendOf(P1, kDefenderOfTomorrow);
    fund(P1, 3, 0);
    EXPECT_FALSE(activationOffered(d)) << "CR 403.3: no legal choice, not legal to activate";
    addGear(P1, kLongSword);
    EXPECT_TRUE(activationOffered(d));
}

TEST_F(Batch3Test, D67_SwiftScoutIsNotOfferedWithNoTeemoAndChoosesAtActivation) {
    auto scout = legendOf(P1, kSwiftScout);
    fund(P1, 3, 0);
    EXPECT_FALSE(activationOffered(scout)) << "no Teemo unit to put in hand";
    auto teemo = addUnit(P1, kTeemoStrategist, 0, 0);
    ASSERT_TRUE(activationOffered(scout));
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(teemo.raw()), EnginePathAgent::kRest}));
    me.other = &them;
    activate(scout, &me, &them);
    ASSERT_FALSE(me.other_priority_at.empty());
    EXPECT_EQ(me.other_priority_at[0], 0) << "the unit is chosen as it is activated";
    EXPECT_TRUE(inHand(P1, teemo));
}

TEST_F(Batch3Test, D38_DiviningShellsIsKilledAsItIsActivated) {
    auto shells = addGear(P1, kDiviningShells);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    class Watch : public CountsPriority {
    public:
        GameObjectId gear = kInvalidId;
        bool gear_on_board_at_priority = false;
        Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
            if (!st.chain.items.empty() && st.getObject(gear).location.has_value())
                gear_on_board_at_priority = true;
            return CountsPriority::selectAction(st, legal);
        }
    } them;
    them.gear = shells;
    EnginePathAgent me(EnginePathAgent::script({static_cast<int>(unit.raw()), EnginePathAgent::kRest}));
    activate(shells, &me, &them);
    EXPECT_GT(them.priority, 0);
    EXPECT_FALSE(them.gear_on_board_at_priority) << "\"Kill this\" is a cost, paid as it is activated";
    EXPECT_EQ(o(unit).current_might, 4);
}

TEST_F(Batch3Test, D41_ForgeOfTheFutureChoosesItsCardsAsItIsActivated) {
    auto forge = addGear(P1, kForgeOfTheFuture);
    auto card = addToTrash(P2, kDredgeUp);
    CountsPriority them;
    NotesWhen me(EnginePathAgent::script({static_cast<int>(card.raw()), 0, EnginePathAgent::kRest}));
    me.other = &them;
    activate(forge, &me, &them);
    ASSERT_FALSE(me.other_priority_at.empty());
    EXPECT_EQ(me.other_priority_at[0], 0) << "the cards are targets (CR 355.10.a.1)";
    EXPECT_TRUE(inDeck(P2, card));
}

TEST_F(Batch3Test, D15_EquipIsAnActivatedAbilityOnTheChain) {
    // CR 377.3 "Activated Abilities use the chain"; 818.1.b.1 "Equip's choice
    // is a Target": the opponent may respond before the gear attaches.
    auto sword = addGear(P1, kLongSword);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    fund(P1, 0, 1);
    class Watch : public CountsPriority {
    public:
        GameObjectId gear = kInvalidId;
        bool unattached_at_priority = false;
        Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
            if (!st.chain.items.empty() && !st.getObject(gear).attached_to.has_value())
                unattached_at_priority = true;
            return CountsPriority::selectAction(st, legal);
        }
    } them;
    them.gear = sword;
    activate(sword, nullptr, &them, 0, {unit});
    EXPECT_TRUE(them.unattached_at_priority);
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>{unit});
}

TEST_F(Batch3Test, D15_AnEquipWhoseUnitLeftAttachesNothing) {
    auto sword = addGear(P1, kLongSword);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    fund(P1, 0, 1);
    class Bounce : public EnginePathAgent {
    public:
        GameObjectId unit = kInvalidId;
        GameEngine* engine = nullptr;
        bool done = false;
        Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
            if (!done && !st.chain.items.empty() && engine) {
                done = true;
                engine->testHook_executor().bounceToHand(unit);
            }
            return EnginePathAgent::selectAction(st, legal);
        }
    } them;
    them.unit = unit;
    Intent i;
    i.type = IntentType::ActivateAbility;
    i.player = P1;
    i.ability_source = sword;
    i.targets = {unit};
    withEngine([&](GameEngine& e) {
        them.engine = &e;
        e.testHook_executeIntent(i);
        settleLikeTheEngine(e);
    }, nullptr, &them, P1);
    EXPECT_TRUE(them.done);
    EXPECT_FALSE(o(sword).attached_to.has_value()) << "a mistarget: nothing to attach to";
}

// ─── F5: "you may pay [N] to <choose a unit>" — the Deflect and the [N]
// together ────────────────────────────────────────────────────────────────

TEST_F(Batch3Test, F5_SinisterPoroPaysTheDeflectAndItsOneFromOneReadyRune) {
    // One ready rune: exhausted for [1] and recycled for the [A] of Deflect.
    // The Deflect used to be paid by recycling the rune unexhausted, and the
    // [1] could then not be paid (an engine error).
    auto poro = addUnit(P1, kSinisterPoro, 0, 0);
    auto deflector = addUnit(P2, kPoutyPoro, 0, 0);
    addRune(P1, Domain::Body);
    combatInProgress(0, P1);
    EnginePathAgent me(EnginePathAgent::script({1, static_cast<int>(deflector.raw()), EnginePathAgent::kRest}));
    EXPECT_NO_THROW(fireTriggerThroughEngine(poro, TriggerType::WhenIAttack, &me));
    EXPECT_TRUE(o(deflector).isAtBase()) << "moved to its base";
}

TEST_F(Batch3Test, F5_KennenAndIcevaleOfferOnlyAUnitTheyCanPayFor) {
    // Not enough for the [N] and the Deflect: the Deflect unit is not offered;
    // the other one is.
    for (CardDefId def : {kKennenKeeper, kIcevaleArcher}) {
        SCOPED_TRACE(card_db.get(def).name);
        state = GameState{};
        state.players[0].id = P1;
        state.players[1].id = P2;
        addBattlefield();
        addBattlefield();
        auto me_unit = addUnit(P1, def, 0, 0);
        auto deflector = addUnit(P2, kPoutyPoro, 0, 0);
        auto plain = addUnit(P2, kInvalidId, 3, 0);
        const int n = def == kKennenKeeper ? 2 : 1;
        fund(P1, n, 0);   // exactly the [N]; nothing for a Deflect
        combatInProgress(0, P1);
        EnginePathAgent me(EnginePathAgent::script({1, static_cast<int>(plain.raw()), EnginePathAgent::kRest}));
        const TriggerType t = def == kKennenKeeper ? TriggerType::WhenYouPlayMe : TriggerType::WhenIAttack;
        EXPECT_NO_THROW(fireTriggerThroughEngine(me_unit, t, &me));
        EXPECT_FALSE(me.wasOffered(deflector));
        EXPECT_TRUE(me.wasOffered(plain));
    }
}

// ─── D2 / D30 / D31 / D64: an ability that [Add]s resolves as it is played
// (CR 401.2, 430.2, 430.2.a, 430.3, 445.2.c) ────────────────────────────────

TEST_F(Batch3Test, D2_SealOfDiscordAddsAtOnceWithNoChainItem) {
    auto seal = addGear(P1, kSealOfDiscord);
    CountsPriority them;
    bool chain_empty_after = false;
    int power_after = -1;
    Intent i;
    i.type = IntentType::ActivateAbility;
    i.player = P1;
    i.ability_source = seal;
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(i);
        chain_empty_after = e.mutableState().chain.items.empty();
        power_after = e.mutableState().player(P1).rune_pool.totalPower();
    }, nullptr, &them, P1);
    EXPECT_TRUE(chain_empty_after) << "no chain item (CR 430.2)";
    EXPECT_EQ(power_after, 1) << "the Power is there at once";
    EXPECT_EQ(them.priority, 0) << "nobody gets priority for it (430.2.a)";
    EXPECT_EQ(state.turn.oc_state, OpenClosedState::Open);
}

TEST_F(Batch3Test, D2_G27_EveryAddAbilityUsesNoChainItem) {
    for (CardDefId def : {kSealOfDiscord, kDaughterOfTheVoid, kScornOfTheMoon, kFireBelow,
                          kGold564, kPlatewyrmEgg, kLuxCrownguard, kDragonsoulSage,
                          kEnergyConduit, kHoneyfruit}) {
        SCOPED_TRACE(card_db.get(def).name);
        state = GameState{};
        state.players[0].id = P1;
        state.players[1].id = P2;
        addBattlefield();
        addBattlefield();
        const auto& d = card_db.get(def);
        GameObjectId src;
        if (d.card_type == CardType::Legend) src = legendOf(P1, def);
        else if (d.card_type == CardType::Unit) src = addUnit(P1, def, 0);
        else src = addGear(P1, def);
        o(src).is_exhausted = false;
        CountsPriority them;
        bool empty = false;
        Intent i;
        i.type = IntentType::ActivateAbility;
        i.player = P1;
        i.ability_source = src;
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(i);
            empty = e.mutableState().chain.items.empty();
        }, nullptr, &them, P1);
        EXPECT_TRUE(empty);
        EXPECT_EQ(them.priority, 0);
        EXPECT_TRUE(o(src).is_exhausted || !o(src).location.has_value()) << "it was activated";
    }
}

TEST_F(Batch3Test, D31_ScornOfTheMoonActivatedWithFocusKeepsFocus) {
    auto scorn = legendOf(P1, kScornOfTheMoon);
    addUnit(P1, kInvalidId, 2, 0);
    addUnit(P2, kInvalidId, 2, 0);
    state.battlefields[0].showdown_in_progress = true;
    state.turn.ns_state = NeutralShowdownState::Showdown;
    std::optional<PlayerId> next;
    Intent i;
    i.type = IntentType::ActivateReactionAbility;
    i.player = P1;
    i.ability_source = scorn;
    withEngine([&](GameEngine& e) {
        int n = 0;
        next = e.testHook_resolveShowdownDecision(BattlefieldId::fromRaw(0), i, P1, n);
    });
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(*next, P1) << "Focus does not pass from an [Add] (CR 430.2.a)";
    EXPECT_EQ(ps(P1).rune_pool.showdown_energy, 1);
}

TEST_F(Batch3Test, D30_DianaPaysHerOneWithScornOfTheMoon) {
    auto scorn = legendOf(P1, kScornOfTheMoon);
    auto diana = addUnit(P1, kDianaLunari, 0, 0);
    addUnit(P2, kInvalidId, 2, 0);
    addToDeck(P1, kDredgeUp);
    addToDeck(P1, kDredgeUp);
    state.battlefields[0].showdown_in_progress = true;
    state.turn.ns_state = NeutralShowdownState::Showdown;
    ASSERT_EQ(readyRuneCount(P1), 0);
    EnginePathAgent me(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    fireTriggerThroughEngine(diana, TriggerType::WhenAShowdownBeginsHere, &me);
    ASSERT_FALSE(me.prompts.empty()) << "the [1] is payable: Scorn of the Moon adds it as it is paid";
    EXPECT_TRUE(o(scorn).is_exhausted) << "Scorn was activated to pay (CR 430.3, 445.2.c)";
    EXPECT_EQ(ps(P1).rune_pool.showdown_energy, 0) << "and its Energy spent";
}

TEST_F(Batch3Test, D64_UndertitansAddLandsInsideTheRevealingEffect) {
    auto titan = addToDeck(P1, kUndertitan);
    int energy_in_effect = -1;
    bool chain_empty_in_effect = false;
    resolveThroughEngine([&](EffectExecutor&, GameState& st) {
        events.emit(CardRevealedEvent{titan, kUndertitan, P1, true, PlayerId::None,
                                      ZoneType::MainDeck});
        energy_in_effect = st.player(P1).rune_pool.energy;
        chain_empty_in_effect = st.chain.items.empty();
    }, nullptr, nullptr, P1);
    EXPECT_EQ(energy_in_effect, 2) << "the [2] is there while the revealing effect goes on";
    EXPECT_TRUE(chain_empty_in_effect);
}

TEST_F(Batch3Test, D2_JhinsMoveAddsAtOnce) {
    auto jhin = addUnit(P1, kJhinMurderousArtist, 0, 0);
    int energy = -1, power = -1;
    bool empty = false;
    resolveThroughEngine([&](EffectExecutor& ex, GameState& st) {
        ex.moveToBase(jhin);
        energy = st.player(P1).rune_pool.energy;
        power = st.player(P1).rune_pool.totalPower();
        empty = st.chain.items.empty();
    }, nullptr, nullptr, P1);
    EXPECT_EQ(energy, 1);
    EXPECT_EQ(power, 1);
    EXPECT_TRUE(empty);
}

// ─── B2 / H3: "from face down" is one test, of this play's stamp ─────────────

TEST_F(Batch3Test, B2_H3_PlayedFromFacedownThenFromHandTheSameTurnDoesNotTrigger) {
    // A card played from face down, returned to hand and played again from
    // hand the same turn: the stamp of the first play is dropped as the
    // second begins (Card::forgetPlayPayments), so none of the three "from
    // face down" cards triggers on it (testing phase B2 / H3).
    for (CardDefId def : {kTornadoWarrior, kEvelynnEntrancing}) {
        SCOPED_TRACE(card_db.get(def).name);
        state = GameState{};
        state.players[0].id = P1;
        state.players[1].id = P2;
        addBattlefield();
        addBattlefield();
        state.turn.phase = TurnPhase::MainPhase;
        state.turn.turn_player = P1;
        addUnit(P2, kInvalidId, 2, 1);
        auto card = addToHand(P1, def);
        o(card).card_counters["__played_from_facedown"] = 1;   // the earlier play
        o(card).card_counters["__facedown_play_turn"] = state.turn.turn_number;
        EXPECT_TRUE(playedFromFacedown(state, card));
        EnginePathAgent me(EnginePathAgent::script({EnginePathAgent::kRest}));
        playThroughEngine(card, &me);
        EXPECT_TRUE(me.prompts.empty()) << "nothing triggered: this play is from hand";
        EXPECT_FALSE(playedFromFacedown(state, card));
    }
}

TEST_F(Batch3Test, D8_Sweep_NoOfferedSpellIsPlayedWithoutARequiredTarget) {
    // Every spell that chooses as it is played (play-time targets, pairs, a
    // finalize pass): on 60 random boards, whenever it is offered, playing it
    // with the first option at every prompt never meets a required choice
    // with nothing to choose (CR 355.8; the gates of testing phase D8 / D17 /
    // F7 / G20 against the cards' own pick lists).
    std::vector<CardDefId> spells;
    for (const auto& [id, def] : card_db.all()) {
        const Card* c = card_registry.get(id);
        if (!c || def.card_type != CardType::Spell || id.raw() >= 1000) continue;
        if (c->needsPlayTimeTarget() || c->needsPlayTimeTargetPair() ||
            (c->choicesAtFinalization() & Card::kFinalizeResolve))
            spells.push_back(id);
    }
    std::sort(spells.begin(), spells.end());
    ASSERT_GE(spells.size(), 40u);
    std::mt19937 rng(20261014);
    std::vector<std::string> misses;
    int plays = 0;
    for (int board = 0; board < 60; ++board) {
        const unsigned seed = rng();
        for (CardDefId id : spells) {
            state = GameState{};
            state.players[0].id = P1;
            state.players[1].id = P2;
            addBattlefield();
            addBattlefield();
            std::mt19937 b(seed);
            for (PlayerId p : {P1, P2}) {
                const int n = static_cast<int>(b() % 4);
                for (int k = 0; k < n; ++k) {
                    const int where = static_cast<int>(b() % 3) - 1;   // base, bf0, bf1
                    const bool poro = b() % 4 == 0;
                    auto u = addUnit(p, poro ? kPoutyPoro : kInvalidId,
                                     1 + static_cast<int>(b() % 6), where);
                    if (b() % 4 == 0) gearOn(p, kLongSword, u);
                }
                for (int k = 0; k < 4; ++k) addToDeck(p, kDredgeUp);
            }
            for (int k = 0; k < 2; ++k) addToTrash(P1, kDredgeUp);
            auto card = addToHand(P1, id);
            fund(P1, 12, 12);
            bool offered_now = false;
            Intent play;
            withEngine([&](GameEngine& e) {
                e.testHook_cleanup();
                for (const auto& i : e.generateLegalActions())
                    if (i.card == card) { offered_now = true; play = i; break; }
            });
            if (!offered_now) continue;
            ++plays;
            // A required target with nothing to choose, in the spell's own
            // choices (a "may" not offered, or a reflexive trigger's own
            // target, is not this).
            std::vector<std::string> lines;
            bool in_spell_pass = false;
            auto conn = events.on_log.connect([&](const LogEvent& e) {
                const auto& m = e.message;
                if (m.find("FINALIZE-CHOICES:") != std::string::npos)
                    in_spell_pass = m.find("spell=1") != std::string::npos;
                else if (m.find("RESOLVE:") != std::string::npos)
                    in_spell_pass = false;
                if (in_spell_pass && (m.find("TGT_NOT_OFFERED") != std::string::npos ||
                                      m.find("TGT_PAIR_NOT_OFFERED") != std::string::npos))
                    lines.push_back(m);
            });
            EnginePathAgent first(EnginePathAgent::first());
            try {
                withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); }, &first);
            } catch (const std::exception& ex) {
                misses.push_back(card_db.get(id).name + ": threw " + ex.what());
            }
            conn.disconnect();
            for (const auto& l : lines) misses.push_back(card_db.get(id).name + ": " + l);
        }
    }
    EXPECT_GT(plays, 500);
    std::string all;
    for (const auto& m : misses) all += "\n  " + m;
    EXPECT_TRUE(misses.empty()) << misses.size() << " plays met a required choice with nothing to choose:" << all;
}

// ─── K1: an effect play that pays keeps what that play paid ─────────────────
// Wild Claw plays Zaun Punk from the top of the deck paying its cost; Zaun
// Punk's optional additional cost (kill a friendly gear) is paid as part of
// that play (CR 356.2.b), so "if you paid the additional cost" holds.
TEST_F(Batch3Test, K1_ZaunPunkPlayedByWildClawRemembersItsPaidAdditionalCost) {
    auto mine = addGear(P1, kLongSword);
    auto theirs = addGear(P2, kLongSword);
    auto punk = addToDeck(P1, kZaunPunk);
    auto claw = addToHand(P1, kWildClaw);
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(punk.raw()), 1, static_cast<int>(mine.raw()),
         static_cast<int>(theirs.raw()), 0}));
    playThroughEngine(claw, &agent);
    ASSERT_TRUE(onBoard(punk)) << "Wild Claw played it";
    EXPECT_FALSE(onBoard(mine)) << "the additional cost was paid";
    EXPECT_FALSE(onBoard(theirs)) << "\"if you paid the additional cost, kill a gear\"";
}

// ─── F6: a spell's "kill a friendly unit" is paid with its Energy and Power ──
// (CR 357.2 "In addition, pay any non-standard Cost ... in any order"; 357.2.a
// a replaced cost is still paid.) Fuzz finding: the unit was killed as the
// choice was made, The Boss's replacement took the one rune, and the spell was
// left unpayable (engine error "made choices it cannot be paid for with").
TEST_F(Batch3Test, F6_SacrificeWithTheBossPaysTheEnergyFirstAndTheReplacementFromWhatIsLeft) {
    legendOf(P1, kTheBoss);
    addRune(P1, Domain::Order);
    deckOf(P1, 4);
    auto big = addUnit(P1, kInvalidId, 5, 0);
    o(big).buff_count = 1;
    auto card = addToHand(P1, kSacrifice);
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(big.raw()), EnginePathAgent::kFirst, 1, EnginePathAgent::kRest}));
    playThroughEngine(card, &agent, std::nullopt, nullptr, /*fund=*/false);
    // The unit; the rune exhausted for the Energy; then The Boss's "may" —
    // asked once the Energy is paid, and payable by recycling that rune.
    ASSERT_GE(agent.prompts.size(), 3u);
    EXPECT_TRUE(onBoard(big)) << "The Boss replaced the death";
    EXPECT_TRUE(o(big).isAtBase()) << "healed, exhausted and recalled";
    EXPECT_EQ(ps(P1).hand.size(), 2u) << "the replaced cost is still paid: draw 2 (CR 357.2.a)";
}

TEST_F(Batch3Test, F6_HeedlessResurrectionKillsItsUnitAfterTheRunesArePaid) {
    for (int i = 0; i < 3; ++i) addRune(P1, Domain::Chaos);
    auto victim = addUnit(P1, kInvalidId, 3, 0);
    auto card = addToHand(P1, kHeedlessResurrection);
    // Reads the game it is asked in (the live state, not the fixture's).
    struct Notes : EnginePathAgent {
        GameObjectId victim;
        std::vector<bool> victim_on_board_at_rune_prompt;
        Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
            for (const auto& i : legal)
                if (i.type == IntentType::MakeChoice && !i.chosen_objects.empty() &&
                    st.objectExists(i.chosen_objects[0]) &&
                    st.getObject(i.chosen_objects[0]).isRune()) {
                    victim_on_board_at_rune_prompt.push_back(
                        st.getObject(victim).location.has_value());
                    return i;
                }
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects[0] == victim) return i;
            return EnginePathAgent::selectAction(st, legal);
        }
    } agent;
    agent.victim = victim;
    playThroughEngine(card, &agent, std::nullopt, nullptr, /*fund=*/false);
    ASSERT_FALSE(agent.victim_on_board_at_rune_prompt.empty()) << "the runes were asked for";
    for (bool b : agent.victim_on_board_at_rune_prompt)
        EXPECT_TRUE(b) << "the unit is killed with the cost, after the Energy and Power (CR 357.2)";
    EXPECT_FALSE(onBoard(victim)) << "and it was killed";
}

// ─── G16: an effect play that pays makes the spell's choices first ──────────
// Fizz plays Punch First ("Give a unit +5 [M] this turn", [1] + 2 Body) from
// the trash, ignoring its Energy. Its target is chosen before the Power is
// paid (CR 355 -> 357), and a unit whose Deflect cannot be paid with that
// Power is not offered (CR 356.2.a.2, 809).
TEST_F(Batch3Test, G16_FizzAsksPunchFirstsTargetBeforeItsPowerAndOffersOnlyPayableOnes) {
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto poro = addUnit(P2, kPoutyPoro, 0, 0);
    for (int i = 0; i < 2; ++i) addRune(P1, Domain::Body);
    auto punch = addToTrash(P1, kPunchFirst);
    auto fizz = addToHand(P1, kFizzTrickster);
    fund(P1, 3, 1);   // Fizz itself
    struct Notes : EnginePathAgent {
        GameObjectId punch, mine, poro;
        std::vector<std::string> order;   // "target" / "rune", as asked
        bool poro_offered = false;
        Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
            const bool is_choice = !legal.empty() && legal.front().type == IntentType::MakeChoice;
            if (is_choice) {
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && i.chosen_objects[0] == poro) poro_offered = true;
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && st.objectExists(i.chosen_objects[0]) &&
                        st.getObject(i.chosen_objects[0]).isRune()) {
                        order.push_back("rune");
                        return i;
                    }
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && i.chosen_objects[0] == mine) {
                        order.push_back("target");
                        return i;
                    }
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && i.chosen_objects[0] == punch) return i;
                for (const auto& i : legal)
                    if (i.chosen_value == 1) return i;   // Fizz's "may": yes
            }
            return EnginePathAgent::selectAction(st, legal);
        }
    } agent;
    agent.punch = punch;
    agent.mine = mine;
    agent.poro = poro;
    playThroughEngine(fizz, &agent, std::nullopt, nullptr, /*fund=*/false);
    ASSERT_FALSE(agent.order.empty());
    EXPECT_EQ(agent.order.front(), "target") << "the target before the Power (CR 355 -> 357)";
    EXPECT_FALSE(agent.poro_offered) << "its Deflect can't be paid with Punch First's Power";
    EXPECT_EQ(o(mine).current_might, 7) << "+5 [M]";
}

// ─── D32 / D57 (B9): a spell an effect plays, whose one target a play intent
// would name, is asked for it — before its cost when the effect pays ─────────
TEST_F(Batch3Test, D32_FizzPlaysSmokeScreenWithATargetChosenBeforeItsPower) {
    auto theirs = addUnit(P2, kInvalidId, 6, 0);
    addRune(P1, Domain::Mind);
    auto smoke = addToTrash(P1, kSmokeScreen);
    auto fizz = addToHand(P1, kFizzTrickster);
    fund(P1, 3, 1);   // Fizz itself
    struct Notes : EnginePathAgent {
        GameObjectId smoke, theirs;
        std::vector<std::string> order;
        Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
            if (!legal.empty() && legal.front().type == IntentType::MakeChoice) {
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && st.objectExists(i.chosen_objects[0]) &&
                        st.getObject(i.chosen_objects[0]).isRune()) {
                        order.push_back("rune");
                        return i;
                    }
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && i.chosen_objects[0] == theirs) {
                        order.push_back("target");
                        return i;
                    }
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && i.chosen_objects[0] == smoke) return i;
                for (const auto& i : legal)
                    if (i.chosen_value == 1) return i;
            }
            return EnginePathAgent::selectAction(st, legal);
        }
    } agent;
    agent.smoke = smoke;
    agent.theirs = theirs;
    playThroughEngine(fizz, &agent, std::nullopt, nullptr, /*fund=*/false);
    ASSERT_FALSE(agent.order.empty());
    EXPECT_EQ(agent.order.front(), "target");
    EXPECT_EQ(o(theirs).current_might, 2) << "-4 [M]: it resolved with its target";
}

TEST_F(Batch3Test, D57_VoidRushPlaysSmiteAtTheUnitItChooses) {
    auto victim = addUnit(P2, kInvalidId, 3, 0);
    addRune(P1, Domain::Fury);
    auto smite = addToDeck(P1, kSmite);
    addToDeck(P1, kDredgeUp);
    auto rush = addToHand(P1, kVoidRush);
    fund(P1, 2, 1);   // Void Rush itself
    EnginePathAgent agent(EnginePathAgent::script(
        {1, static_cast<int>(smite.raw()), static_cast<int>(victim.raw()), EnginePathAgent::kRest}));
    playThroughEngine(rush, &agent, std::nullopt, nullptr, /*fund=*/false);
    EXPECT_FALSE(onBoard(victim)) << "Deal 3: Smite resolved with its target";
}

TEST_F(Batch3Test, D57_VoidRushDoesNotOfferSmiteWithNoUnitAtABattlefield) {
    addUnit(P2, kInvalidId, 3);   // at its base
    addRune(P1, Domain::Fury);
    auto smite = addToDeck(P1, kSmite);
    addToDeck(P1, kDredgeUp);
    auto rush = addToHand(P1, kVoidRush);
    fund(P1, 2, 1);
    EnginePathAgent agent(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    playThroughEngine(rush, &agent, std::nullopt, nullptr, /*fund=*/false);
    EXPECT_FALSE(agent.wasOffered(smite)) << "no legal target, so no play (CR 355.8)";
    EXPECT_TRUE(inHand(P1, smite)) << "drawn, as a card not banished";
}

}  // namespace riftbound::test::batch_3
