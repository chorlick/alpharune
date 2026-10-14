// Testing phase, batch 1b: trigger findings from 19 read ISMCTS games (G5, G6,
// G9, G10, G11, G12, G13, G26) and two combat-flow items found by batch 2 (I1,
// I2). Outcomes in plans/completed/10-testing-phase.md; the cited traces in
// docs/testing-findings-games19-2026-10.md section 4.
//
// Every test drives the engine path. The cited games ran on a binary from
// before batch 1 and no longer replay to the same decisions, so each situation
// is rebuilt by hand.

#include "cards/card_test_fixture.h"

#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <boost/signals2/connection.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test::batch1b {

constexpr PlayerId P1 = PlayerId::Player1;
constexpr PlayerId P2 = PlayerId::Player2;

// Real cards.
constexpr CardDefId kForgefireCape = 507;      // Equipment: "When I attack or defend, deal 2 to all enemy units here."
constexpr CardDefId kSoaringScout = 216;       // "[Deathknell] — Channel 1 rune exhausted."
constexpr CardDefId kFrozenFortress = 768;     // "At the start of each player's Beginning Phase, deal 1 to each unit here."
constexpr CardDefId kDuskRoseLab = 765;        // "At the start of your Beginning Phase, you may kill a unit you control here to draw 1."
constexpr CardDefId kObeliskOfPower = 279;     // "At the start of each player's first Beginning Phase, that player channels 1 rune."
constexpr CardDefId kTheArenasGreatest = 285;  // "At the start of each player's first Beginning Phase, that player gains 1 point."
constexpr CardDefId kHeartOfTheTempest = 942;  // legend: "When you play a card from anywhere other than your hand, empower me."
constexpr CardDefId kFioraPeerless = 432;      // a champion unit (Champion Zone play)
constexpr CardDefId kUpFromTheDeep = 887;      // [Flow] [3] spell
constexpr CardDefId kDefy = 45;                // [Reaction] counter: a spell that costs no more than [4] and [A]
constexpr CardDefId kTornadoWarrior = 886;     // "When you play me from face down, you may empower something here. Disempower it at end of turn."
constexpr CardDefId kSanction = 822;           // "Empower a unit. Disempower it at end of turn. / Disempower ... Empower it at end of turn."
constexpr CardDefId kRavenbloomStudent = 103;  // "When you play a spell, give me +1 [M] this turn."
constexpr CardDefId kTemporalBreach = 853;     // "Banish a unit, then its owner plays it to the same location, ignoring its cost."
constexpr CardDefId kCuratorOfTheSands = 932;  // legend: "When you play a unit, gear, or activated ability with Energy cost [7] or more, you may exhaust me to ready up to 2 runes."
constexpr CardDefId kLadyOfLuminosity = 319;   // legend: "When you play a spell that costs [5] or more, draw 1."
constexpr CardDefId kLuxIlluminated = 304;     // "When you play a spell that costs [5] or more, give me +3 [M] this turn."
constexpr CardDefId kYordleExplorer = 422;     // "When you play a card with Power cost [A][A] or more, draw 1."
constexpr CardDefId kBlitzcrankImpassive = 67; // "When you play me to a battlefield, you may move an enemy unit to here."
constexpr CardDefId kVanguardCaptain = 218;    // "[Legion] — When you play me, play two 1 [M] Recruit unit tokens here."
constexpr CardDefId kTrifarianGloryseeker = 217; // "[Legion] — When you play me, buff me."
constexpr CardDefId kDangerousDuo = 16;        // "[Legion] — When you play me, give a unit +2 [M] this turn."
constexpr CardDefId kScrapyardChampion = 20;   // "[Legion] — When you play me, discard 2, then draw 2."
constexpr CardDefId kDariusExecutioner243 = 243; // "[Legion] — When you play me, ready me."
constexpr CardDefId kDariusExecutioner547 = 547;
constexpr CardDefId kWujuApprentice = 602;     // "[Level 6][>] When you play me, draw 1."
constexpr CardDefId kNasusAscended = 833;      // "[Empowered][>] When I conquer, you score 1 point."
constexpr CardDefId kCovertInformant = 844;    // "[Empowered][>] When I move, draw 1."
constexpr CardDefId kBaccaiWitherclaw = 865;   // "[Empowered][>][>>][Deathknell][>] Channel 2 runes exhausted."
constexpr CardDefId kNoxianEmissary = 915;     // "[Empowered][>][>>][Deathknell][>] Play two 1 [M] Recruit unit tokens to your base."
constexpr CardDefId kIncinerate = 301;         // a spell (cost [1]-ish) for "a spell was played"
constexpr CardDefId kLoyalPoro = 718;          // deck / hand filler
constexpr CardDefId kSivirAmbitious = 441;     // "When I conquer after an attack, if you assigned 5 or more excess damage ..."
constexpr CardDefId kTryndamereBarbarian = 34; // "When I conquer after an attack, if you assigned 5 or more excess damage to enemy units, you score 1 point."
constexpr CardDefId kYetiBrawler = 580;        // "When I conquer, if you assigned 3 or more excess damage, play two Gold gear tokens exhausted."
constexpr CardDefId kFortifiedPosition = 274;  // "When you defend here, choose a unit. It gains [Shield 2] this combat."

using May = EnginePathAgent::May;

/// EnginePathAgent that shows every decision to a callback first and lets the
/// test answer it (`act` returns an Intent to take instead of the default).
class Watcher1b : public EnginePathAgent {
public:
    using EnginePathAgent::EnginePathAgent;
    std::function<void(const std::vector<Intent>&)> see;
    std::function<std::optional<Intent>(const std::vector<Intent>&)> act;
    int priority = 0, focus = 0;
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        if (has(legal, IntentType::PassPriority)) ++priority;
        if (has(legal, IntentType::PassFocus)) ++focus;
        if (see) see(legal);
        if (act)
            if (auto chosen = act(legal)) return *chosen;
        return EnginePathAgent::selectAction(state, legal);
    }
    static bool has(const std::vector<Intent>& legal, IntentType t) {
        return std::any_of(legal.begin(), legal.end(), [t](const Intent& i) { return i.type == t; });
    }
    static const Intent* playOf(const std::vector<Intent>& legal, GameObjectId card) {
        for (const auto& i : legal)
            if (i.card == card && (i.type == IntentType::PlayCard || i.type == IntentType::PlayActionCard ||
                                   i.type == IntentType::PlayReaction))
                return &i;
        return nullptr;
    }
};

class Batch1b : public CardTestFixture {
protected:
    std::vector<std::string> log_;
    boost::signals2::scoped_connection log_connection_;

    void SetUp() override {
        CardTestFixture::SetUp();
        log_connection_ = events.on_log.connect(
            [this](const LogEvent& e) { log_.push_back(e.message); });
    }

    GameObject& o(GameObjectId id) { return state.getObject(id); }
    PlayerState& ps(PlayerId p) { return state.player(p); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }

    bool logged(const std::string& text) const {
        return std::any_of(log_.begin(), log_.end(), [&](const std::string& line) {
            return line.find(text) != std::string::npos;
        });
    }
    int loggedCount(const std::string& text) const {
        return static_cast<int>(std::count_if(log_.begin(), log_.end(), [&](const std::string& l) {
            return l.find(text) != std::string::npos;
        }));
    }

    GameObjectId named(GameObjectId id, const std::string& name) {
        o(id).name = name;
        return id;
    }
    GameObjectId vanilla(PlayerId p, int at_bf = -1, int might = 2) {
        return addUnit(p, kInvalidId, might, at_bf);
    }
    GameObjectId legendOf(PlayerId p, CardDefId def) {
        auto id = state.createObject();
        auto& l = o(id);
        const auto& d = card_db.get(def);
        l.owner = l.controller = p;
        l.card_def_id = def;
        l.name = d.name;
        l.card_type = d.card_type;
        l.domains = d.domains;
        l.zone = ZoneType::LegendZone;
        ps(p).legend_zone = id;
        return id;
    }
    GameObjectId battlefieldCard(int slot, CardDefId def) {
        auto id = state.createObject();
        auto& b = o(id);
        b.card_def_id = def;
        b.card_type = CardType::Battlefield;
        b.name = card_db.get(def).name;
        b.owner = b.controller = P1;   // the player who brought it (a game sets this)
        state.battlefields[slot].card_object_id = id;
        return id;
    }
    GameObjectId gearOf(PlayerId p, CardDefId def, int at_bf = -1) {
        auto id = state.createObject();
        auto& g = o(id);
        g.owner = g.controller = p;
        g.card_def_id = def;
        g.card_type = CardType::Gear;
        const auto& d = card_db.get(def);
        g.name = d.name;
        g.tags = d.tags;
        g.keywords = d.keywords;
        g.might_bonus = d.might_bonus;
        if (at_bf >= 0) {
            g.zone = ZoneType::BattlefieldZone;
            g.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{p};
        }
        return id;
    }
    /// A card of `p`'s in their trash.
    GameObjectId inTrashOf(PlayerId p, CardDefId def) {
        auto id = addToHand(p, def);
        auto& h = ps(p).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        o(id).zone = ZoneType::Trash;
        ps(p).trash.push_back(id);
        return id;
    }
    /// A rune card on top of `p`'s Rune Deck.
    GameObjectId runeInDeck(PlayerId p, Domain d = Domain::Fury) {
        auto id = state.createObject();
        auto& r = o(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = std::string(toString(d)) + " Rune";
        r.domains = {d};
        r.zone = ZoneType::RuneDeck;
        ps(p).rune_deck.push_back(id);
        return id;
    }
    void stock(PlayerId p, int n = 3) {
        for (int i = 0; i < n; ++i) addToDeck(p, kLoyalPoro);
    }
    int runesOnBoard(PlayerId p) {
        int n = 0;
        for (auto& [id, obj] : state.objects)
            if (obj.isRune() && obj.controller == p && obj.location.has_value()) ++n;
        return n;
    }
    int unitsNamed(PlayerId p, const std::string& name) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == p && obj.location.has_value() && obj.name == name) ++n;
        return n;
    }
    int goldTokens(PlayerId p) {
        int n = 0;
        for (auto& [id, obj] : state.objects)
            if (obj.isGear() && obj.isToken() && obj.location.has_value() && obj.controller == p) ++n;
        return n;
    }

    /// The event happens: `source`'s ability for `trigger` is offered to
    /// TriggerManager (which asks the card's trigger condition), then the
    /// engine finalizes and resolves whatever went on the chain. Returns
    /// whether an ability was put on the chain at all (every chain item
    /// allocates an id). `before_resolution` runs on the engine's state after
    /// the event and before anything resolves.
    using Edit = std::function<void(GameState&)>;
    bool fire(GameObjectId source, TriggerType trigger, GameObjectId subject = kInvalidId,
              AgentInterface* agent = nullptr, const Edit& before_resolution = nullptr) {
        const PlayerId controller = o(source).controller;
        bool queued = false;
        withEngine([&](GameEngine& e) {
            const ChainItemId before = e.mutableState().chain.next_id;
            e.testHook_executor().queueTriggeredAbility(source, controller, trigger, subject);
            queued = e.mutableState().chain.next_id != before;
            if (before_resolution) before_resolution(e.mutableState());
            settleLikeTheEngine(e);
        }, controller == P1 ? agent : nullptr, controller == P2 ? agent : nullptr, controller);
        return queued;
    }

    /// A combat is staged at `bf`: `attacker`'s units applied Contested to a
    /// battlefield the other player controls.
    void stageCombat(int bf, PlayerId attacker) {
        auto& b = state.battlefields[bf];
        b.controller = opponent(attacker);
        b.is_contested = true;
        b.contested_by = attacker;
        b.combat_staged = true;
    }
    /// A (non-combat) showdown is staged at `bf`: `by`'s units applied
    /// Contested to a battlefield nobody controls.
    void stageShowdown(int bf, PlayerId by) {
        auto& b = state.battlefields[bf];
        b.controller = std::nullopt;
        b.is_contested = true;
        b.contested_by = by;
        b.showdown_staged = true;
    }
    GameEngine* live = nullptr;
    void runCombat(int bf, PlayerId attacker, AgentInterface* p1 = nullptr,
                   AgentInterface* p2 = nullptr) {
        withEngine([&](GameEngine& e) {
            live = &e;
            e.testHook_runCombat(BattlefieldId::fromRaw(bf));
            live = nullptr;
        }, p1, p2, attacker);
    }
    const GameObject& liveObject(GameObjectId id) const { return live->state().getObject(id); }
    bool liveOnBoard(GameObjectId id) const { return liveObject(id).location.has_value(); }
};

// ═══ G5 ═════════════════════════════════════════════════════════════════════
// g01 D:246-254: Forgefire Cape's "When I attack or defend, deal 2 to all enemy
// units here" resolved; the 2 [M] Tideturner it killed was still on the board
// at P2's Focus decision and Charm moved it. CR 319.5: a Cleanup follows "After
// a Chain Item is removed from the Chain for any reason"; CR 323.4 kills units
// with lethal damage.

TEST_F(Batch1b, G5_ForgefireCape_AUnitItKillsIsGoneBeforeTheFirstFocusDecision) {
    auto aphelios = named(vanilla(P2, 0, 4), "Aphelios");
    auto cape = gearOf(P2, kForgefireCape, 0);
    resolveThroughEngine([&](EffectExecutor& x, GameState&) { x.attachGear(cape, aphelios); },
                         nullptr, nullptr, P2);
    ASSERT_EQ(o(cape).attached_to, std::optional<GameObjectId>{aphelios});
    auto tideturner = named(vanilla(P1, 0, 2), "Tideturner");
    auto adaptatron = named(vanilla(P1, 0, 4), "Adaptatron");
    stageCombat(0, P2);

    Watcher1b p1, p2;
    std::vector<bool> tideturner_at_focus;
    auto look = [&](const std::vector<Intent>& legal) {
        if (Watcher1b::has(legal, IntentType::PassFocus))
            tideturner_at_focus.push_back(liveOnBoard(tideturner));
    };
    p1.see = look;
    p2.see = look;
    runCombat(0, P2, &p1, &p2);
    ASSERT_FALSE(tideturner_at_focus.empty()) << "the combat has a showdown: Adaptatron survives";
    for (bool there : tideturner_at_focus) EXPECT_FALSE(there);
    EXPECT_FALSE(onBoard(tideturner));
    EXPECT_TRUE(inTrash(P1, tideturner));
    (void)adaptatron;
}

// ═══ G6 ═════════════════════════════════════════════════════════════════════
// g13 D:061-064: Soaring Scout, the last unit at Sunken Temple, died with its
// Deathknell queued; the battlefield kept its controller through the Main
// Phase decision after the Deathknell resolved. CR 323.5: "Players lose control
// of any controlled Battlefields without their Units occupying them if the turn
// is in an Open State and there is no Showdown or Combat ongoing there."

TEST_F(Batch1b, G6_ABattlefieldWhoseLastUnitDiedWithADeathknellIsUncontrolledAtTheNextDecision) {
    auto scout = addUnit(P1, kSoaringScout, 0, 0);
    runeInDeck(P1);
    state.battlefields[0].controller = P1;
    std::optional<PlayerId> at_decision = P1;
    bool asked = false;
    Watcher1b p1;
    withEngine([&](GameEngine& e) {
        e.testHook_executor().killObject(scout);
        e.testHook_runChain();
        // The turn player's next Main Phase decision.
        at_decision = e.state().battlefields[0].controller;
        e.testHook_advanceMainPhase();
        asked = true;
    }, &p1);
    EXPECT_TRUE(asked);
    EXPECT_FALSE(onBoard(scout));
    EXPECT_EQ(runesOnBoard(P1), 1) << "the Deathknell resolved";
    EXPECT_EQ(at_decision, std::nullopt);
    EXPECT_EQ(state.battlefields[0].controller, std::nullopt);
}

// ═══ G10 ════════════════════════════════════════════════════════════════════
// CR 190.6.a: "While a Battlefield is Controlled, its Controller controls its
// Abilities unless that ability indicates another player does." CR 190.6.b:
// uncontrolled, the Turn Player takes responsibility. CR 190.6.d: "'You' in a
// battlefield's abilities refers to the battlefield's Controller ... If the
// battlefield has no Controller, 'you' refers to no one, and all such
// instructions are ignored."

TEST_F(Batch1b, G10_FrozenFortress_TheControllerOfTheBattlefieldControlsItsAbilityOnTheOtherPlayersTurn) {
    battlefieldCard(0, kFrozenFortress);
    state.battlefields[0].controller = P2;
    auto p2_unit = vanilla(P2, 0, 3);
    state.turn.phase = TurnPhase::ChannelPhase;
    state.turn.turn_player = P1;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, nullptr, nullptr, P1);
    EXPECT_TRUE(logged("TRIGGER: Frozen Fortress (P2)"));
    EXPECT_FALSE(logged("TRIGGER: Frozen Fortress (P1)"));
    EXPECT_EQ(o(p2_unit).damage_marked, 1);
}

TEST_F(Batch1b, G10_FrozenFortress_UncontrolledTheTurnPlayerPutsItOnTheChain) {
    battlefieldCard(0, kFrozenFortress);
    auto p1_unit = vanilla(P1, 0, 3);
    state.battlefields[0].controller = std::nullopt;
    state.turn.phase = TurnPhase::ChannelPhase;
    state.turn.turn_player = P1;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, nullptr, nullptr, P1);
    EXPECT_TRUE(logged("TRIGGER: Frozen Fortress (P1)"));
    EXPECT_EQ(o(p1_unit).damage_marked, 1);
}

TEST_F(Batch1b, G10_DuskRoseLab_DoesNotTriggerInTheBeginningPhaseOfAPlayerWhoDoesNotControlIt) {
    battlefieldCard(0, kDuskRoseLab);
    state.battlefields[0].controller = P1;
    vanilla(P1, 0, 3);
    state.turn.phase = TurnPhase::ChannelPhase;
    state.turn.turn_player = P2;
    Watcher1b p1, p2;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, &p1, &p2, P2);
    EXPECT_FALSE(logged("TRIGGER: Dusk Rose Lab"));
    EXPECT_EQ(p1.priority + p2.priority, 0) << "no item, no priority window";
}

// J5 (b), pinned: with no controller "your Beginning Phase" is nobody's.
TEST_F(Batch1b, G10_DuskRoseLab_UncontrolledDoesNotTrigger_J5) {
    battlefieldCard(0, kDuskRoseLab);
    state.battlefields[0].controller = std::nullopt;
    state.turn.phase = TurnPhase::ChannelPhase;
    state.turn.turn_player = P1;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, nullptr, nullptr, P1);
    EXPECT_FALSE(logged("TRIGGER: Dusk Rose Lab"));
}

TEST_F(Batch1b, G10_DuskRoseLab_OnItsControllersTurnTheKillAndTheDrawAreOffered) {
    battlefieldCard(0, kDuskRoseLab);
    state.battlefields[0].controller = P1;
    auto mine = vanilla(P1, 0, 3);
    vanilla(P1, 0, 3);   // the battlefield stays controlled
    stock(P1);
    state.turn.phase = TurnPhase::ChannelPhase;
    state.turn.turn_player = P1;
    EnginePathAgent p1(EnginePathAgent::choosing(mine, May::Yes));
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, &p1, nullptr, P1);
    EXPECT_TRUE(logged("TRIGGER: Dusk Rose Lab (P1)"));
    EXPECT_FALSE(onBoard(mine));
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(Batch1b, G10_ObeliskOfPower_ThatPlayerChannelsWhileTheOtherPlayerControlsIt) {
    battlefieldCard(0, kObeliskOfPower);
    state.battlefields[0].controller = P1;
    vanilla(P1, 0, 3);
    runeInDeck(P2);
    ps(P2).turns_taken = 1;   // P2's first turn
    state.turn.phase = TurnPhase::ChannelPhase;
    state.turn.turn_player = P2;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, nullptr, nullptr, P2);
    EXPECT_TRUE(logged("TRIGGER: Obelisk of Power (P1)"));
    EXPECT_EQ(runesOnBoard(P2), 1);
    EXPECT_EQ(runesOnBoard(P1), 0);
}

// "first Beginning Phase" is part of the trigger condition: a later one puts
// nothing on the chain (G9's root, CR 383.2.a.1).
TEST_F(Batch1b, G10_ObeliskOfPower_ALaterBeginningPhaseDoesNotTrigger) {
    battlefieldCard(0, kObeliskOfPower);
    runeInDeck(P1);
    ps(P1).turns_taken = 2;
    state.turn.phase = TurnPhase::ChannelPhase;
    state.turn.turn_player = P1;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, nullptr, nullptr, P1);
    EXPECT_FALSE(logged("TRIGGER: Obelisk of Power"));
    EXPECT_EQ(runesOnBoard(P1), 0);
}

TEST_F(Batch1b, G10_TheArenasGreatest_ThatPlayerGainsThePointWhileTheOtherPlayerControlsIt) {
    battlefieldCard(0, kTheArenasGreatest);
    state.battlefields[0].controller = P1;
    vanilla(P1, 0, 3);
    ps(P2).turns_taken = 1;
    state.turn.phase = TurnPhase::ChannelPhase;
    state.turn.turn_player = P2;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, nullptr, nullptr, P2);
    EXPECT_TRUE(logged("TRIGGER: The Arena's Greatest (P1)"));
    EXPECT_EQ(ps(P2).score, 1);
    EXPECT_EQ(ps(P1).score, 0);
}

TEST_F(Batch1b, G10_TheArenasGreatest_ALaterBeginningPhaseDoesNotTrigger) {
    battlefieldCard(0, kTheArenasGreatest);
    ps(P1).turns_taken = 3;
    state.turn.phase = TurnPhase::ChannelPhase;
    state.turn.turn_player = P1;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); }, nullptr, nullptr, P1);
    EXPECT_FALSE(logged("TRIGGER: The Arena's Greatest"));
    EXPECT_EQ(ps(P1).score, 0);
}

// ═══ G11 ════════════════════════════════════════════════════════════════════
// Heart of the Tempest: "When you play a card from anywhere other than your
// hand, empower me." CR 420.1.a: "By default, a player can only Play cards from
// their hand or their Chosen Champion zone." CR 420.3: "Game effects may result
// in cards being Played as part of their resolution."

TEST_F(Batch1b, G11_HeartOfTheTempest_APlayFromTheChampionZoneEmpowersIt) {
    auto heart = legendOf(P1, kHeartOfTheTempest);
    auto champ = addToHand(P1, kFioraPeerless);
    {
        auto& h = ps(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), champ), h.end());
        o(champ).zone = ZoneType::ChampionZone;
        ps(P1).champion_zone = champ;
    }
    ps(P1).rune_pool.energy = 10;
    ps(P1).rune_pool.universal_power = 5;
    bool played = false;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.card == champ && !played) {
                e.testHook_executeIntent(i);
                played = true;
            }
        settleLikeTheEngine(e);
    });
    ASSERT_TRUE(played);
    ASSERT_TRUE(onBoard(champ));
    EXPECT_TRUE(o(heart).isEmpowered());
}

TEST_F(Batch1b, G11_HeartOfTheTempest_AUnitAnEffectPlaysFromTheTrashEmpowersIt) {
    auto heart = legendOf(P1, kHeartOfTheTempest);
    auto unit = inTrashOf(P1, kLoyalPoro);
    resolveThroughEngine([&](EffectExecutor& x, GameState&) {
        x.playIgnoringCost(P1, unit, LocationId{BaseLocation{P1}});
    });
    ASSERT_TRUE(onBoard(unit));
    EXPECT_TRUE(o(heart).isEmpowered());
}

TEST_F(Batch1b, G11_HeartOfTheTempest_AUnitAnEffectPlaysFromTheHandDoesNot) {
    auto heart = legendOf(P1, kHeartOfTheTempest);
    auto unit = addToHand(P1, kLoyalPoro);
    resolveThroughEngine([&](EffectExecutor& x, GameState&) {
        x.playIgnoringCost(P1, unit, LocationId{BaseLocation{P1}});
    });
    ASSERT_TRUE(onBoard(unit));
    EXPECT_FALSE(o(heart).isEmpowered());
}

TEST_F(Batch1b, G11_HeartOfTheTempest_APlayFromTheHandDoesNot) {
    auto heart = legendOf(P1, kHeartOfTheTempest);
    auto unit = addToHand(P1, kLoyalPoro);
    playThroughEngine(unit);
    ASSERT_TRUE(onBoard(unit));
    EXPECT_FALSE(o(heart).isEmpowered());
}

// ═══ G12 ════════════════════════════════════════════════════════════════════
// CR 420.4.a: "Any such triggered abilities trigger when the act of playing the
// card has been completed by the resolution of the card." 420.4.a.1: "If a game
// effect prevents the resolution of the card—for example, because the card was
// countered—abilities that trigger on playing cards will not trigger."

TEST_F(Batch1b, G12_HeartOfTheTempest_AFlowSpellEmpowersItOnlyOnceItHasResolved) {
    auto heart = legendOf(P1, kHeartOfTheTempest);
    auto flow = inTrashOf(P1, kUpFromTheDeep);
    ps(P1).rune_pool.energy = 10;
    ps(P1).rune_pool.universal_power = 5;
    bool played = false;
    // The chain at the first priority decision after the play.
    std::optional<std::vector<GameObjectId>> first_window;
    Watcher1b p1, p2;
    auto look = [&](const std::vector<Intent>& legal) {
        if (first_window || !live || !Watcher1b::has(legal, IntentType::PassPriority)) return;
        std::vector<GameObjectId> sources;
        for (const auto& ci : live->state().chain.items) sources.push_back(ci.source);
        first_window = sources;
    };
    p1.see = look;
    p2.see = look;
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_cleanup();   // the standing Flow permission is rebuilt by the aura pass
        for (const auto& i : e.generateLegalActions())
            if (i.card == flow && !played) {
                e.testHook_executeIntent(i);
                played = true;
            }
        settleLikeTheEngine(e);
        live = nullptr;
    }, &p1, &p2);
    ASSERT_TRUE(played);
    ASSERT_TRUE(first_window.has_value());
    EXPECT_EQ(*first_window, std::vector<GameObjectId>{flow})
        << "only the spell is on the chain as it is played";
    EXPECT_TRUE(o(heart).isEmpowered());
}

TEST_F(Batch1b, G12_HeartOfTheTempest_ACounteredFlowSpellDoesNotEmpowerIt) {
    auto heart = legendOf(P1, kHeartOfTheTempest);
    auto flow = inTrashOf(P1, kUpFromTheDeep);
    ps(P1).rune_pool.energy = 10;
    ps(P1).rune_pool.universal_power = 5;
    auto defy = addToHand(P2, kDefy);
    ps(P2).rune_pool.energy = 5;
    ps(P2).rune_pool.universal_power = 5;
    Watcher1b p1, p2;
    bool countered = false;
    p2.act = [&](const std::vector<Intent>& legal) -> std::optional<Intent> {
        if (countered) return std::nullopt;
        if (const Intent* i = Watcher1b::playOf(legal, defy)) {
            countered = true;
            return *i;
        }
        return std::nullopt;
    };
    bool played = false;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        for (const auto& i : e.generateLegalActions())
            if (i.card == flow && !played) {
                e.testHook_executeIntent(i);
                played = true;
            }
        settleLikeTheEngine(e);
    }, &p1, &p2);
    ASSERT_TRUE(played);
    ASSERT_TRUE(countered) << "Defy was offered at priority and played";
    EXPECT_FALSE(o(heart).isEmpowered());
}

TEST_F(Batch1b, G12_HeartOfTheTempest_ASpellAnEffectPlaysFromTheTrashEmpowersItAsItResolves) {
    auto heart = legendOf(P1, kHeartOfTheTempest);
    auto spell = inTrashOf(P1, kUpFromTheDeep);
    bool heart_item_with_spell = false;
    withEngine([&](GameEngine& e) {
        e.testHook_executor().playIgnoringCost(P1, spell, std::nullopt);
        for (const auto& ci : e.state().chain.items)
            if (ci.source == heart) heart_item_with_spell = true;
        settleLikeTheEngine(e);
    });
    EXPECT_FALSE(heart_item_with_spell);
    EXPECT_TRUE(o(heart).isEmpowered());
}

// ═══ G13 ════════════════════════════════════════════════════════════════════
// "Disempower it at end of turn": a delayed triggered ability (CR 391.2:
// "Delayed Triggers ... can be recognized by describing a specific time of the
// turn"), performed in the Ending Step (CR 317.1.a: "At the end of this turn
// and at the end of turn Game Effects take place.") — on the chain, with
// priority, not a flag flipped in the Expiration Step.

TEST_F(Batch1b, G13_TornadoWarrior_TheDisempowerIsAnAbilityOnTheChainInTheEndingStep) {
    auto warrior = addUnit(P1, kTornadoWarrior, 0, 0);
    auto ally = vanilla(P1, 0);
    o(warrior).card_counters["__played_from_facedown"] = 1;
    o(warrior).card_counters["__facedown_play_turn"] = state.turn.turn_number;
    EnginePathAgent agent(EnginePathAgent::choosing(ally, May::Yes));
    ASSERT_TRUE(fire(warrior, TriggerType::WhenYouPlayMe, kInvalidId, &agent));
    ASSERT_TRUE(o(ally).isEmpowered());

    Watcher1b p1, p2;
    bool empowered_at_ending_priority = false;
    p1.see = [&](const std::vector<Intent>& legal) {
        if (Watcher1b::has(legal, IntentType::PassPriority) && live)
            empowered_at_ending_priority = liveObject(ally).isEmpowered();
    };
    bool empowered_after_ending = true;
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_endingStep();
        empowered_after_ending = e.state().getObject(ally).isEmpowered();
        e.testHook_expirationStep();
        live = nullptr;
    }, &p1, &p2);
    EXPECT_GE(p1.priority, 1) << "the delayed ability gave priority in the Ending Step";
    EXPECT_TRUE(empowered_at_ending_priority) << "pending: not disempowered yet";
    EXPECT_FALSE(empowered_after_ending);
    EXPECT_FALSE(o(ally).isEmpowered());
}

TEST_F(Batch1b, G13_Sanction_EmpowerThenDisempowerAtEndOfTurnIsAnAbilityInTheEndingStep) {
    auto unit = vanilla(P1, 0);
    auto sanction = addToHand(P1, kSanction);
    EnginePathAgent agent(EnginePathAgent::script({0, EnginePathAgent::kFirst}));
    playThroughEngine(sanction, &agent);
    ASSERT_TRUE(o(unit).isEmpowered());
    Watcher1b p1, p2;
    bool empowered_after_ending = true;
    withEngine([&](GameEngine& e) {
        e.testHook_endingStep();
        empowered_after_ending = e.state().getObject(unit).isEmpowered();
    }, &p1, &p2);
    EXPECT_GE(p1.priority, 1);
    EXPECT_FALSE(empowered_after_ending);
}

TEST_F(Batch1b, G13_Sanction_DisempowerThenEmpowerAtEndOfTurn) {
    auto unit = vanilla(P1, 0);
    o(unit).empowered = 1;
    auto sanction = addToHand(P1, kSanction);
    EnginePathAgent agent(EnginePathAgent::script({1, EnginePathAgent::kFirst}));
    playThroughEngine(sanction, &agent);
    ASSERT_FALSE(o(unit).isEmpowered());
    bool empowered_after_ending = false;
    withEngine([&](GameEngine& e) {
        e.testHook_endingStep();
        empowered_after_ending = e.state().getObject(unit).isEmpowered();
    });
    EXPECT_TRUE(empowered_after_ending);
}

// The unit left the board before the end of the turn: it is a new object
// (CR 124.1) and the delayed ability's window ended with the old one (CR
// 391.5.a). Played again, it is not disempowered.
TEST_F(Batch1b, G13_TornadoWarrior_AUnitThatLeftAndCameBackIsNotTouched) {
    auto warrior = addUnit(P1, kTornadoWarrior, 0, 0);
    auto ally = vanilla(P1, 0);
    o(warrior).card_counters["__played_from_facedown"] = 1;
    o(warrior).card_counters["__facedown_play_turn"] = state.turn.turn_number;
    EnginePathAgent agent(EnginePathAgent::choosing(ally, May::Yes));
    ASSERT_TRUE(fire(warrior, TriggerType::WhenYouPlayMe, kInvalidId, &agent));
    ASSERT_TRUE(o(ally).isEmpowered());
    resolveThroughEngine([&](EffectExecutor& x, GameState& s) {
        x.bounceToHand(ally);
        // Empowered again by something else, once back on the board.
        x.playIgnoringCost(P1, ally, LocationId{BaseLocation{P1}});
        x.empower(ally);
        (void)s;
    });
    ASSERT_TRUE(o(ally).isEmpowered());
    withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    EXPECT_TRUE(o(ally).isEmpowered());
}

// ═══ G26 (QUESTION, J7) ═════════════════════════════════════════════════════
// Temporal Breach on my own Ravenbloom Student: the Student is banished and
// played back during the spell's resolution (a new object, CR 124.1). The
// engine triggers the played-back Student for that Temporal Breach (CR 420.4.a:
// such abilities trigger when the play is completed by the resolution; the new
// Student is on the board then). Pinned; judge question 10.

TEST_F(Batch1b, G26_RavenbloomStudentPlayedBackByTemporalBreachTriggersOnIt_J7) {
    auto student = addUnit(P1, kRavenbloomStudent, 0, 0);
    const int printed = o(student).current_might;
    auto breach = addToHand(P1, kTemporalBreach);
    EnginePathAgent agent(EnginePathAgent::choosing(student));
    playThroughEngine(breach, &agent);
    ASSERT_TRUE(onBoard(student));
    EXPECT_EQ(o(student).temp_might_bonus, 1);
    EXPECT_EQ(o(student).current_might, printed + 1);
}


// ═══ G9 ═════════════════════════════════════════════════════════════════════
// CR 383.2.a.1: "Any additional conditional statement immediately after the
// Condition must be true in order for the Condition to be fulfilled. Such a
// conditional statement is part of the Trigger Condition and not the Effect."
// A qualifier of the event ("a spell that costs [5] or more", "to a
// battlefield") is the condition itself (CR 383.1). CR 727.1.c.1: "Triggered
// Abilities of Dependent Keywords must be Active for their trigger to be
// evaluated" — [Legion] (812.1.c), [Level] (824.1.c), [Empowered].
// For each card: condition false -> nothing on the chain (no item id is
// allocated: no empty item, no priority window); true -> the effect; where a
// condition can change, true at the event and false at resolution -> the effect
// still happens.

constexpr CardDefId kDownwell = 468;      // a spell, [8]
constexpr CardDefId kFallingStar = 29;    // a spell, [2][A][A]

class Batch1bG9 : public Batch1b {
protected:
    /// A spell of `p` that has been played and resolved (it is in the trash).
    GameObjectId spellPlayed(PlayerId p, CardDefId def) { return inTrashOf(p, def); }
};

TEST_F(Batch1bG9, CuratorOfTheSands_AUnitThatCostsLessThan7DoesNotTrigger) {
    auto curator = legendOf(P1, kCuratorOfTheSands);
    auto played = addUnit(P1, kLoyalPoro);
    addRune(P1, Domain::Calm, /*exhausted=*/true);
    AcceptingEnginePathAgent agent;
    EXPECT_FALSE(fire(curator, TriggerType::WhenYouPlayACard, played, &agent));
    EXPECT_TRUE(agent.prompts.empty());
}

TEST_F(Batch1bG9, CuratorOfTheSands_ASpellDoesNotTrigger) {
    auto curator = legendOf(P1, kCuratorOfTheSands);
    auto spell = spellPlayed(P1, kDownwell);   // [8], but a spell
    EXPECT_FALSE(fire(curator, TriggerType::WhenYouPlayACard, spell));
}

TEST_F(Batch1bG9, CuratorOfTheSands_AUnitThatCosts7OrMoreTriggers) {
    auto curator = legendOf(P1, kCuratorOfTheSands);
    auto played = addUnit(P1, kNasusAscended);   // [8]
    auto rune = addRune(P1, Domain::Calm, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::choosing(rune, May::Yes));
    EXPECT_TRUE(fire(curator, TriggerType::WhenYouPlayACard, played, &agent));
    EXPECT_FALSE(o(rune).is_exhausted);
    EXPECT_TRUE(o(curator).is_exhausted);
}

TEST_F(Batch1bG9, CuratorOfTheSands_AnActivatedAbilityIsJudgedByItsEnergyCost) {
    auto curator = legendOf(P1, kCuratorOfTheSands);
    auto source = gearOf(P1, 671);   // any permanent with an ability
    o(source).card_counters["__ability_energy_cost"] = 3;
    EXPECT_FALSE(fire(curator, TriggerType::WhenYouActivateAnAbility, source));
    o(source).card_counters["__ability_energy_cost"] = 7;
    EXPECT_TRUE(fire(curator, TriggerType::WhenYouActivateAnAbility, source));
}

TEST_F(Batch1bG9, LadyOfLuminosity_ASpellThatCostsLessThan5DoesNotTrigger) {
    auto lady = legendOf(P1, kLadyOfLuminosity);
    stock(P1);
    EXPECT_FALSE(fire(lady, TriggerType::WhenYouPlayASpell, spellPlayed(P1, kIncinerate)));
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_TRUE(fire(lady, TriggerType::WhenYouPlayASpell, spellPlayed(P1, kDownwell)));
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(Batch1bG9, LuxIlluminated_ASpellThatCostsLessThan5DoesNotTrigger) {
    auto lux = addUnit(P1, kLuxIlluminated, 0, 0);
    EXPECT_FALSE(fire(lux, TriggerType::WhenYouPlayASpell, spellPlayed(P1, kIncinerate)));
    EXPECT_EQ(o(lux).temp_might_bonus, 0);
    EXPECT_TRUE(fire(lux, TriggerType::WhenYouPlayASpell, spellPlayed(P1, kDownwell)));
    EXPECT_EQ(o(lux).temp_might_bonus, 3);
}

TEST_F(Batch1bG9, YordleExplorer_ACardWithLessThanTwoPowerDoesNotTrigger) {
    auto yordle = addUnit(P1, kYordleExplorer, 0, 0);
    stock(P1);
    EXPECT_FALSE(fire(yordle, TriggerType::WhenYouPlayACard, spellPlayed(P1, kIncinerate)));
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_TRUE(fire(yordle, TriggerType::WhenYouPlayACard, spellPlayed(P1, kFallingStar)));
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(Batch1bG9, BlitzcrankImpassive_PlayedToTheBaseDoesNotTrigger) {
    auto blitz = addUnit(P1, kBlitzcrankImpassive);   // in the base
    vanilla(P2, 1);
    AcceptingEnginePathAgent agent;
    EXPECT_FALSE(fire(blitz, TriggerType::WhenYouPlayMe, kInvalidId, &agent));
    EXPECT_TRUE(agent.prompts.empty());
}

TEST_F(Batch1bG9, BlitzcrankImpassive_PlayedToABattlefieldPullsAnEnemyUnit) {
    auto blitz = addUnit(P1, kBlitzcrankImpassive, 0, 0);
    auto enemy = vanilla(P2, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(enemy, May::Yes));
    EXPECT_TRUE(fire(blitz, TriggerType::WhenYouPlayMe, kInvalidId, &agent));
    EXPECT_EQ(o(enemy).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

TEST_F(Batch1bG9, VanguardCaptain_LegionOffDoesNotTrigger_LegionOnPlaysTwoRecruits) {
    auto captain = addUnit(P1, kVanguardCaptain, 0, 0);
    ps(P1).cards_played_this_turn = 1;   // only the Captain
    EXPECT_FALSE(fire(captain, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(unitsNamed(P1, "Recruit"), 0);
    ps(P1).cards_played_this_turn = 2;
    EXPECT_TRUE(fire(captain, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(unitsNamed(P1, "Recruit"), 2);
}

TEST_F(Batch1bG9, TrifarianGloryseeker_LegionOffDoesNotTrigger_LegionOnBuffs) {
    auto unit = addUnit(P1, kTrifarianGloryseeker, 0, 0);
    ps(P1).cards_played_this_turn = 1;
    EXPECT_FALSE(fire(unit, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(o(unit).buff_count, 0);
    ps(P1).cards_played_this_turn = 2;
    EXPECT_TRUE(fire(unit, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(o(unit).buff_count, 1);
}

TEST_F(Batch1bG9, DangerousDuo_LegionOffDoesNotTrigger_LegionOnGivesPlus2) {
    auto duo = addUnit(P1, kDangerousDuo, 0, 0);
    auto ally = vanilla(P1, 0);
    ps(P1).cards_played_this_turn = 1;
    EnginePathAgent none(EnginePathAgent::choosing(ally));
    EXPECT_FALSE(fire(duo, TriggerType::WhenYouPlayMe, kInvalidId, &none));
    EXPECT_EQ(o(ally).temp_might_bonus, 0);
    ps(P1).cards_played_this_turn = 2;
    EnginePathAgent agent(EnginePathAgent::choosing(ally));
    EXPECT_TRUE(fire(duo, TriggerType::WhenYouPlayMe, kInvalidId, &agent));
    EXPECT_EQ(o(ally).temp_might_bonus, 2);
}

TEST_F(Batch1bG9, ScrapyardChampion_LegionOffDoesNotTrigger_LegionOnDiscardsAndDraws) {
    auto champ = addUnit(P1, kScrapyardChampion, 0, 0);
    addToHand(P1, kLoyalPoro);
    addToHand(P1, kLoyalPoro);
    stock(P1, 4);
    ps(P1).cards_played_this_turn = 1;
    EXPECT_FALSE(fire(champ, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(trashSize(P1), 0);
    ps(P1).cards_played_this_turn = 2;
    EXPECT_TRUE(fire(champ, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(trashSize(P1), 2);
    EXPECT_EQ(handSize(P1), 2);
}

TEST_F(Batch1bG9, DariusExecutioner_BothPrintings_LegionOffDoesNotTrigger_LegionOnReadies) {
    for (CardDefId def : {kDariusExecutioner243, kDariusExecutioner547}) {
        auto darius = addUnit(P1, def, 0, 0);
        o(darius).is_exhausted = true;
        ps(P1).cards_played_this_turn = 1;
        EXPECT_FALSE(fire(darius, TriggerType::WhenYouPlayMe)) << def.raw();
        EXPECT_TRUE(o(darius).is_exhausted);
        ps(P1).cards_played_this_turn = 2;
        EXPECT_TRUE(fire(darius, TriggerType::WhenYouPlayMe)) << def.raw();
        EXPECT_FALSE(o(darius).is_exhausted);
    }
}

TEST_F(Batch1bG9, WujuApprentice_Level6OffDoesNotTrigger_OnDrawsEvenIfTheXpIsSpentInResponse) {
    auto wuju = addUnit(P1, kWujuApprentice, 0, 0);
    stock(P1);
    setXp(P1, 5);
    EXPECT_FALSE(fire(wuju, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(handSize(P1), 0);
    setXp(P1, 6);
    EXPECT_TRUE(fire(wuju, TriggerType::WhenYouPlayMe, kInvalidId, nullptr,
                     [](GameState& s) { s.player(PlayerId::Player1).xp = 0; }));
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(Batch1bG9, NasusAscended_NotEmpoweredDoesNotTrigger_EmpoweredScoresEvenIfDisempoweredInResponse) {
    auto nasus = addUnit(P1, kNasusAscended, 0, 0);
    EXPECT_FALSE(fire(nasus, TriggerType::WhenIConquer));
    EXPECT_EQ(ps(P1).score, 0);
    o(nasus).empowered = 1;
    EXPECT_TRUE(fire(nasus, TriggerType::WhenIConquer, kInvalidId, nullptr,
                     [nasus](GameState& s) { s.getObject(nasus).empowered = 0; }));
    EXPECT_EQ(ps(P1).score, 1);
}

TEST_F(Batch1bG9, CovertInformant_NotEmpoweredDoesNotTrigger_EmpoweredDraws) {
    auto informant = addUnit(P1, kCovertInformant, 0, 0);
    stock(P1);
    EXPECT_FALSE(fire(informant, TriggerType::WhenIMove));
    EXPECT_EQ(handSize(P1), 0);
    o(informant).empowered = 1;
    EXPECT_TRUE(fire(informant, TriggerType::WhenIMove, kInvalidId, nullptr,
                     [informant](GameState& s) { s.getObject(informant).empowered = 0; }));
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(Batch1bG9, BaccaiWitherclaw_DiedNotEmpoweredDoesNotTrigger_DiedEmpoweredChannels) {
    auto baccai = addUnit(P1, kBaccaiWitherclaw, 0, 0);
    runeInDeck(P1);
    runeInDeck(P1);
    resolveThroughEngine([&](EffectExecutor& x, GameState&) { x.killObject(baccai); });
    EXPECT_FALSE(logged("TRIGGER: Baccai"));
    EXPECT_EQ(runesOnBoard(P1), 0);

    auto again = addUnit(P1, kBaccaiWitherclaw, 0, 0);
    o(again).empowered = 1;
    resolveThroughEngine([&](EffectExecutor& x, GameState&) { x.killObject(again); });
    EXPECT_TRUE(logged("TRIGGER: Baccai"));
    EXPECT_EQ(runesOnBoard(P1), 2);
}

TEST_F(Batch1bG9, NoxianEmissary_DiedNotEmpoweredDoesNotTrigger_DiedEmpoweredPlaysTwoRecruits) {
    auto emissary = addUnit(P1, kNoxianEmissary, 0, 0);
    resolveThroughEngine([&](EffectExecutor& x, GameState&) { x.killObject(emissary); });
    EXPECT_FALSE(logged("TRIGGER: Noxian Emissary"));
    EXPECT_EQ(unitsNamed(P1, "Recruit"), 0);

    auto again = addUnit(P1, kNoxianEmissary, 0, 0);
    o(again).empowered = 1;
    resolveThroughEngine([&](EffectExecutor& x, GameState&) { x.killObject(again); });
    EXPECT_TRUE(logged("TRIGGER: Noxian Emissary"));
    EXPECT_EQ(unitsNamed(P1, "Recruit"), 2);
}


// ═══ I1, I2: combat flow ═══════════════════════════════════════════════════

// Test-only cards (ids in the spare range, never in a set's).
constexpr CardDefId kTProbeA = 1980;
constexpr CardDefId kTProbeB = 1981;

/// A unit whose abilities call `probe` as they resolve, with the trigger that
/// fired. [Assault 2] so that "while I'm an attacker" shows in its Might.
class TProbe : public UnitCard {
public:
    using Probe = std::function<void(CardContext&, TriggerType)>;
    TProbe(CardDefId id, std::string name, std::vector<TriggerType> triggers, Probe probe)
        : triggers_(std::move(triggers)), probe_(std::move(probe)) {
        def_.id = id;
        def_.name = std::move(name);
        def_.card_type = CardType::Unit;
        def_.might = 3;
        def_.energy_cost = 1;
        def_.domains = {Domain::Fury};
        def_.keywords.set(Keyword::Assault);
        def_.assault_value = 2;
    }
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override { return triggers_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        probe_(ctx, ctx.firing_trigger);
    }
private:
    CardDef def_;
    std::vector<TriggerType> triggers_;
    Probe probe_;
};

struct ProbeSeen {
    TriggerType trigger;
    CombatDesignation designation;
    int might;
    bool combat_in_progress;
    NeutralShowdownState ns;
};

class Batch1bCombat : public Batch1b {
protected:
    std::vector<ProbeSeen> seen;
    std::vector<std::string> order;
    void SetUp() override {
        Batch1b::SetUp();
        using T = TriggerType;
        auto probe = [this](CardContext& ctx, TriggerType t) {
            const auto& self = ctx.state.getObject(ctx.source);
            bool going = false;
            for (const auto& bf : ctx.state.battlefields)
                if (bf.combat_in_progress) going = true;
            seen.push_back({t, self.combat_designation, self.current_might, going,
                            ctx.state.turn.ns_state});
            order.push_back(t == T::WhenIConquer ? "conquer"
                            : t == T::WhenIWinCombat ? "win"
                            : t == T::WhenACombatIWasInEnds ? "ended"
                            : t == T::WhenIAttack ? "attack" : "?");
        };
        card_registry.registerCard(kTProbeA, std::make_unique<TProbe>(kTProbeA, "ProbeA",
            std::vector<T>{T::WhenIConquer, T::WhenIWinCombat, T::WhenACombatIWasInEnds}, probe));
        card_registry.registerCard(kTProbeB, std::make_unique<TProbe>(kTProbeB, "ProbeB",
            std::vector<T>{T::WhenIConquer}, probe));
        card_db.buildFromClasses(card_registry);
    }
    /// The order in which combats and showdowns begin.
    std::vector<std::string> begun;
    boost::signals2::scoped_connection c1_, c2_;
    void recordBeginnings() {
        c1_ = events.on_combat_started.connect([this](const CombatStartedEvent& e) {
            begun.push_back("combat@" + toString(e.battlefield));
        });
        c2_ = events.on_showdown_started.connect([this](const ShowdownStartedEvent& e) {
            if (!e.is_combat_showdown) begun.push_back("showdown@" + toString(e.battlefield));
        });
    }
    static bool isBeginPrompt(const std::vector<Intent>& legal) {
        return !legal.empty() && std::all_of(legal.begin(), legal.end(), [](const Intent& i) {
            return i.type == IntentType::MakeChoice &&
                   (i.choice_label.rfind("Combat begins at", 0) == 0 ||
                    i.choice_label.rfind("Showdown begins at", 0) == 0);
        });
    }
    /// The option of a begin prompt that names battlefield `bf`.
    static std::optional<Intent> beginAt(const std::vector<Intent>& legal, int bf) {
        for (const auto& i : legal)
            if (i.chosen_value == bf) return i;
        return std::nullopt;
    }
};

// CR 323.12: "If the current state is a Neutral Open State and one or more
// Combats are Staged at Battlefields, the Turn Player chooses one of those
// Battlefields. Combat begins there."
TEST_F(Batch1bCombat, I1_TwoStagedCombats_TheTurnPlayerChoosesWhereTheFirstBegins) {
    battlefieldCard(0, kFrozenFortress);
    battlefieldCard(1, kDuskRoseLab);
    vanilla(P1, 0, 5);
    vanilla(P2, 0, 1);
    vanilla(P1, 1, 5);
    vanilla(P2, 1, 1);
    stageCombat(0, P1);
    stageCombat(1, P1);
    recordBeginnings();
    Watcher1b p1, p2;
    int asked_p1 = 0, asked_p2 = 0;
    std::vector<std::vector<Intent>> prompts;
    p1.act = [&](const std::vector<Intent>& legal) -> std::optional<Intent> {
        if (!isBeginPrompt(legal)) return std::nullopt;
        ++asked_p1;
        prompts.push_back(legal);
        return beginAt(legal, 1);
    };
    p2.see = [&](const std::vector<Intent>& legal) { if (isBeginPrompt(legal)) ++asked_p2; };
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); }, &p1, &p2);
    EXPECT_EQ(asked_p1, 1) << "asked once: after the first, one combat is left";
    EXPECT_EQ(asked_p2, 0);
    ASSERT_EQ(prompts.size(), 1u);
    ASSERT_EQ(prompts[0].size(), 2u);
    // Each option names the battlefield: its number and its card.
    EXPECT_EQ(prompts[0][0].chosen_objects,
              std::vector<GameObjectId>{state.battlefields[0].card_object_id});
    EXPECT_EQ(begun, (std::vector<std::string>{"combat@1", "combat@0"}));
}

// On the turn player's turn, even when the other player is the attacker in
// both combats (the defender's units moved in by an effect): the Turn Player
// chooses.
TEST_F(Batch1bCombat, I1_TheTurnPlayerChoosesAlsoWhenTheOtherPlayerAttacks) {
    vanilla(P2, 0, 5);
    vanilla(P1, 0, 1);
    vanilla(P2, 1, 5);
    vanilla(P1, 1, 1);
    stageCombat(0, P2);
    stageCombat(1, P2);
    Watcher1b p1, p2;
    int asked_p1 = 0, asked_p2 = 0;
    p1.see = [&](const std::vector<Intent>& legal) { if (isBeginPrompt(legal)) ++asked_p1; };
    p2.see = [&](const std::vector<Intent>& legal) { if (isBeginPrompt(legal)) ++asked_p2; };
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); }, &p1, &p2);
    EXPECT_EQ(asked_p1, 1);
    EXPECT_EQ(asked_p2, 0);
}

// CR 323.11 comes before 323.12 in the Cleanup: a staged showdown begins
// before a staged combat, wherever they are.
TEST_F(Batch1bCombat, I1_AStagedShowdownBeginsBeforeAStagedCombat) {
    vanilla(P1, 0, 5);
    vanilla(P2, 0, 1);
    vanilla(P1, 1, 3);
    stageCombat(0, P1);
    stageShowdown(1, P1);
    recordBeginnings();
    Watcher1b p1;
    int asked = 0;
    p1.see = [&](const std::vector<Intent>& legal) { if (isBeginPrompt(legal)) ++asked; };
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); }, &p1);
    EXPECT_EQ(asked, 0) << "one candidate at each step: nothing to choose";
    EXPECT_EQ(begun, (std::vector<std::string>{"showdown@1", "combat@0"}));
}

TEST_F(Batch1bCombat, I1_TwoStagedShowdowns_TheTurnPlayerChooses) {
    vanilla(P1, 0, 3);
    vanilla(P1, 1, 3);
    stageShowdown(0, P1);
    stageShowdown(1, P1);
    recordBeginnings();
    Watcher1b p1;
    p1.act = [&](const std::vector<Intent>& legal) -> std::optional<Intent> {
        if (!isBeginPrompt(legal)) return std::nullopt;
        return beginAt(legal, 1);
    };
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); }, &p1);
    EXPECT_EQ(begun, (std::vector<std::string>{"showdown@1", "showdown@0"}));
    EXPECT_EQ(state.battlefields[0].controller, std::optional<PlayerId>{P1});
    EXPECT_EQ(state.battlefields[1].controller, std::optional<PlayerId>{P1});
}

TEST_F(Batch1bCombat, I1_OneStagedCombatAsksNothing) {
    vanilla(P1, 0, 5);
    vanilla(P2, 0, 1);
    stageCombat(0, P1);
    Watcher1b p1;
    int asked = 0;
    p1.see = [&](const std::vector<Intent>& legal) { if (isBeginPrompt(legal)) ++asked; };
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); }, &p1);
    EXPECT_EQ(asked, 0);
    EXPECT_EQ(state.battlefields[0].controller, std::optional<PlayerId>{P1});
}

// CR 468.5-468.7: control is established (468.5), what that queued resolves
// (468.6), and only then the combat ends: "Remove Attacker and Defender
// Designation from all Units and Players" (468.7.a), "At the end of Combat or
// Combat ends effects take place" (468.7.b), "All 'this combat' effects expire
// simultaneously" (468.7.c).
TEST_F(Batch1bCombat, I2_AConquerAbilityResolvesWhileTheCombatIsStillOn) {
    auto probe = addUnit(P1, kTProbeB, 0, 0);   // 3 [M], [Assault 2]
    vanilla(P2, 0, 1);
    stageCombat(0, P1);
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); });
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0].trigger, TriggerType::WhenIConquer);
    EXPECT_EQ(seen[0].designation, CombatDesignation::Attacker);
    EXPECT_EQ(seen[0].might, 5) << "3 [M] + [Assault 2] while an attacker";
    EXPECT_TRUE(seen[0].combat_in_progress);
    EXPECT_EQ(seen[0].ns, NeutralShowdownState::Showdown) << "CR 308.1: a combat is a Showdown State";
    // After the combat: no designation, the Assault gone, a Neutral State.
    EXPECT_EQ(o(probe).combat_designation, CombatDesignation::None);
    EXPECT_EQ(o(probe).current_might, 3);
    EXPECT_FALSE(state.battlefields[0].combat_in_progress);
    EXPECT_EQ(state.turn.ns_state, NeutralShowdownState::Neutral);
}

// "When I win a combat" at the result (468.3), resolved before control is
// established (468.4); the conquer (468.5) resolved before the combat ends
// (468.6); "When a combat that I was in ends" at the end (468.7.b).
TEST_F(Batch1bCombat, I2_WinThenConquerThenTheEndOfTheCombat) {
    addUnit(P1, kTProbeA, 0, 0);
    vanilla(P2, 0, 1);
    stageCombat(0, P1);
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); });
    EXPECT_EQ(order, (std::vector<std::string>{"win", "conquer", "ended"}));
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_TRUE(seen[0].combat_in_progress);
    EXPECT_TRUE(seen[1].combat_in_progress);
    EXPECT_FALSE(seen[2].combat_in_progress) << "468.7.b comes after 468.7.a";
    EXPECT_EQ(seen[2].designation, CombatDesignation::None);
}

// No Result (468.3.d: the attackers were recalled in the Combat Cleanup):
// nobody establishes control, and the combat still ends (468.7) — "When a
// combat that I was in ends" after the result, with the designation gone.
// (The other No Result, both players still here, stages another combat:
// SweepFlowTest.Combat_NoResultWithBothPlayersPresentStagesAnotherCombat.)
TEST_F(Batch1bCombat, I2_ANoResultCombatStillEnds) {
    auto probe = addUnit(P1, kTProbeA, 0, 0);
    auto wall = vanilla(P2, 0, 9);
    o(probe).is_stunned = true;   // neither deals combat damage: a tie
    o(wall).is_stunned = true;
    stageCombat(0, P1);
    std::vector<std::string> flow;
    boost::signals2::scoped_connection a = events.on_combat_started.connect(
        [&](const CombatStartedEvent&) { flow.push_back("start"); });
    boost::signals2::scoped_connection b = events.on_combat_ended.connect(
        [&](const CombatEndedEvent&) { flow.push_back("end"); });
    withEngine([&](GameEngine& e) { e.testHook_runCombat(BattlefieldId::fromRaw(0)); });
    EXPECT_EQ(flow, (std::vector<std::string>{"start", "end"}));
    EXPECT_EQ(order, (std::vector<std::string>{"ended"}));
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_FALSE(seen[0].combat_in_progress);
    EXPECT_TRUE(o(probe).isAtBase()) << "recalled";
    EXPECT_EQ(state.battlefields[0].controller, std::optional<PlayerId>{P2});
    EXPECT_FALSE(state.battlefields[0].combat_in_progress);
}

// The cards that read the combat at a conquer: Tryndamere, Barbarian; Sivir,
// Ambitious; Yeti Brawler (B4 pins the last two with two combats staged).
TEST_F(Batch1bCombat, I2_TryndamereBarbarian_ConquersAfterAnAttackWithFiveExcessAndScores) {
    addUnit(P1, kTryndamereBarbarian, 0, 0);
    vanilla(P2, 0, 1);
    stageCombat(0, P1);
    const int before = ps(P1).score;
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); });
    ASSERT_EQ(state.battlefields[0].controller, std::optional<PlayerId>{P1});
    EXPECT_GE(ps(P1).excess_combat_damage, 5);
    // The conquer point and Tryndamere's.
    EXPECT_EQ(ps(P1).score, before + 2);
}

TEST_F(Batch1bCombat, I2_YetiBrawler_ConquerWithThreeExcessPlaysTwoGold) {
    addUnit(P1, kYetiBrawler, 0, 0);
    vanilla(P2, 0, 1);
    stageCombat(0, P1);
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); });
    EXPECT_EQ(goldTokens(P1), 2);
}


// G9, Equipment: the effect text's condition is the trigger condition too.
constexpr CardDefId kHextechGauntlets = 748;   // "When I conquer, if you assigned 3 or more excess damage, draw 1."
constexpr CardDefId kBlightedBattleaxe = 581;  // "At the end of your turn, if I didn't conquer this turn, unattach this and deal 4 to me."
constexpr CardDefId kTrappingGrounds = 773;    // "When you conquer here, if you assigned 3 or more excess damage, play a 1 [M] Bird unit token with [Deflect]."
constexpr CardDefId kPiltoverEnforcer = 783;   // legend: "When you conquer, if you assigned 3 or more excess damage, you may exhaust me to ready a unit."

TEST_F(Batch1bCombat, G9_HextechGauntlets_AConquerWithLessThanThreeExcessPutsNothingOnTheChain) {
    auto bearer = vanilla(P1, 0, 1);   // 1 + the Gauntlets' 3 = 4 [M]
    auto gauntlets = gearOf(P1, kHextechGauntlets, 0);
    resolveThroughEngine([&](EffectExecutor& x, GameState&) { x.attachGear(gauntlets, bearer); });
    ASSERT_EQ(o(gauntlets).attached_to, std::optional<GameObjectId>{bearer});
    vanilla(P2, 0, 2);   // 2 excess
    stock(P1);
    stageCombat(0, P1);
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); });
    ASSERT_EQ(state.battlefields[0].controller, std::optional<PlayerId>{P1});
    EXPECT_FALSE(logged("EQUIP_TRIGGER: Hextech Gauntlets"));
    EXPECT_EQ(handSize(P1), 0);
}

TEST_F(Batch1bCombat, G9_HextechGauntlets_AConquerWithThreeExcessDraws) {
    auto bearer = vanilla(P1, 0, 1);   // 1 + the Gauntlets' 3 = 4 [M]
    auto gauntlets = gearOf(P1, kHextechGauntlets, 0);
    resolveThroughEngine([&](EffectExecutor& x, GameState&) { x.attachGear(gauntlets, bearer); });
    vanilla(P2, 0, 1);   // 3 excess
    stock(P1);
    stageCombat(0, P1);
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); });
    EXPECT_TRUE(logged("EQUIP_TRIGGER: Hextech Gauntlets"));
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(Batch1bCombat, G9_BlightedBattleaxe_ABearerThatConqueredPutsNothingOnTheChain) {
    auto bearer = vanilla(P1, 0, 6);
    auto axe = gearOf(P1, kBlightedBattleaxe, 0);
    resolveThroughEngine([&](EffectExecutor& x, GameState&) { x.attachGear(axe, bearer); });
    o(bearer).card_counters["__conquered_turn"] = state.turn.turn_number;
    withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    EXPECT_FALSE(logged("EQUIP_TRIGGER: Blighted Battleaxe"));
    EXPECT_EQ(o(bearer).damage_marked, 0);
    EXPECT_EQ(o(axe).attached_to, std::optional<GameObjectId>{bearer});
}

TEST_F(Batch1bCombat, G9_BlightedBattleaxe_ABearerThatDidNotConquerTakes4) {
    auto bearer = vanilla(P1, 0, 6);
    auto axe = gearOf(P1, kBlightedBattleaxe, 0);
    resolveThroughEngine([&](EffectExecutor& x, GameState&) { x.attachGear(axe, bearer); });
    withEngine([&](GameEngine& e) { e.testHook_endingStep(); });
    EXPECT_TRUE(logged("EQUIP_TRIGGER: Blighted Battleaxe"));
    EXPECT_EQ(o(bearer).damage_marked, 4);
    EXPECT_FALSE(o(axe).attached_to.has_value());
}

// I2: the other cards that read the combat at a conquer.
TEST_F(Batch1bCombat, I2_TrappingGrounds_AConquerWithThreeExcessPlaysABird) {
    battlefieldCard(0, kTrappingGrounds);
    vanilla(P1, 0, 4);
    vanilla(P2, 0, 1);   // 3 excess
    stageCombat(0, P1);
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); });
    ASSERT_EQ(state.battlefields[0].controller, std::optional<PlayerId>{P1});
    EXPECT_EQ(unitsNamed(P1, "Bird"), 1);
}

TEST_F(Batch1bCombat, I2_PiltoverEnforcer_AConquerWithThreeExcessOffersTheReady) {
    battlefieldCard(0, kFrozenFortress);   // the conquer's subject is the battlefield's card
    auto enforcer = legendOf(P1, kPiltoverEnforcer);
    auto tired = vanilla(P1, -1, 2);
    o(tired).is_exhausted = true;
    vanilla(P1, 0, 4);
    vanilla(P2, 0, 1);   // 3 excess
    stageCombat(0, P1);
    EnginePathAgent p1(EnginePathAgent::choosing(tired, May::Yes));
    withEngine([&](GameEngine& e) { e.testHook_advanceMainPhase(); }, &p1);
    EXPECT_TRUE(o(enforcer).is_exhausted);
    EXPECT_FALSE(o(tired).is_exhausted);
}

}  // namespace riftbound::test::batch1b
