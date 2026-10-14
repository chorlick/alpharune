/// Testing phase, batch 4b: generators, payments, log audience, turn structure
/// and the questions of plans/10 (Batch 4b, B3-B8, G1-G25, H1-H3). One test
/// (or a few) per finding, through the engine path; the finding id is in each
/// test's name. Detail of every finding: docs/testing-findings-decks-2026-10.md
/// (D..), docs/testing-findings-games19-2026-10.md section 4 (G..),
/// plans/reports/batch-1.md section 11 (B..), plans/reports/batch-4a.md
/// section 9 (H..).

#include <gtest/gtest.h>

#include "card_test_fixture.h"
#include "cards/card_helpers.h"
#include "core/board_state.h"
#include "rules/deck_validator.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test::batch_4b {

constexpr CardDefId kImmortalPhoenix = 37;
constexpr CardDefId kVoidSeeker = 24;
constexpr CardDefId kTimeWarp = 122;
constexpr CardDefId kShipyardSkulker = 175;
constexpr CardDefId kTideturner = 199;
constexpr CardDefId kHiddenBlade = 213;
constexpr CardDefId kFacebreaker = 220;
constexpr CardDefId kFortifiedPosition = 274;
constexpr CardDefId kVilemawsLair = 290;
constexpr CardDefId kLonelyPoro359 = 359;
constexpr CardDefId kSwitcheroo = 466;
constexpr CardDefId kGuards = 475;
constexpr CardDefId kRockfallPath = 530;
constexpr CardDefId kBrush = 561;
constexpr CardDefId kRengarTrophyHunter = 682;
constexpr CardDefId kGreenFather = 753;
constexpr CardDefId kTornadoWarrior = 886;
constexpr CardDefId kKennenKeeperOfBalance = 922;
constexpr CardDefId kDefenderOfTomorrow = 936;
constexpr CardDefId kZhonyasHourglass = 77;
constexpr CardDefId kSunDisc = 21;

class Batch4bTest : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    PlayerState& ps(PlayerId p) { return state.player(p); }
    static BattlefieldId bfId(int bf) { return BattlefieldId::fromRaw(bf); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }

    GameObjectId battlefieldCard(int bf, CardDefId def_id, std::optional<PlayerId> controller = P1) {
        auto id = state.createObject();
        auto& b = o(id);
        const auto& def = card_db.get(def_id);
        b.card_def_id = def_id;
        b.card_type = CardType::Battlefield;
        b.name = def.name;
        b.owner = b.controller = controller.value_or(PlayerId::None);
        b.zone = ZoneType::BattlefieldZone;
        auto& slot = state.battlefields[bf];
        slot.card_object_id = id;
        slot.controller = controller;
        if (auto* bfc = dynamic_cast<const BattlefieldCard*>(card_registry.get(def_id))) {
            slot.blocks_move_to_base = bfc->blocksMoveToBase();
            slot.blocks_unit_play = bfc->blocksUnitPlay();
        }
        return id;
    }
    GameObjectId gear(PlayerId p, CardDefId def_id, bool exhausted = false) {
        auto id = state.createObject();
        auto& g = o(id);
        const auto& def = card_db.get(def_id);
        g.owner = g.controller = p;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        g.super_type = def.super_type;
        g.name = def.name;
        g.tags = def.tags;
        g.domains = def.domains;
        g.keywords = def.keywords;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{p};
        g.is_exhausted = exhausted;
        return id;
    }
    GameObjectId legendCard(PlayerId p, CardDefId def_id) {
        auto id = state.createObject();
        auto& l = o(id);
        const auto& def = card_db.get(def_id);
        l.owner = l.controller = p;
        l.card_def_id = def_id;
        l.card_type = CardType::Legend;
        l.name = def.name;
        l.tags = def.tags;
        l.domains = def.domains;
        l.keywords = def.keywords;
        l.zone = ZoneType::LegendZone;
        ps(p).legend_zone = id;
        return id;
    }
    /// Put `card` (in its owner's hand) facedown at battlefield `bf`, hidden
    /// since last turn.
    void hideAt(GameObjectId card, int bf) {
        auto& hand = ps(o(card).owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        o(card).zone = ZoneType::FacedownZone;
        o(card).is_hidden = true;
        o(card).hidden_at = bfId(bf);
        o(card).hidden_on_turn = state.turn.turn_number - 1;
        state.battlefields[bf].facedown.push_back(card);
    }
    void fund(PlayerId p, int energy, int power = 0) {
        ps(p).rune_pool.energy += energy;
        ps(p).rune_pool.universal_power += power;
    }
    void run(const Intent& intent, AgentInterface* p1 = nullptr, AgentInterface* p2 = nullptr) {
        withEngineDefaultTurn([&](GameEngine& e) { e.testHook_executeIntent(intent); settleLikeTheEngine(e); },
                              p1, p2, intent.player);
    }
    Intent playIntent(GameObjectId card) {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = o(card).controller;
        play.card = card;
        return play;
    }
    /// `player`'s Main Phase options in a Neutral Open State of their turn.
    std::vector<Intent> mainPhaseOptions(PlayerId player) {
        state.turn.phase = TurnPhase::MainPhase;
        state.turn.turn_player = player;
        state.turn.ns_state = NeutralShowdownState::Neutral;
        state.turn.oc_state = OpenClosedState::Open;
        state.turn.priority_holder = player;
        std::vector<Intent> out;
        withEngine([&](GameEngine& e) { out = e.generateLegalActions(); }, nullptr, nullptr, player);
        return out;
    }
    /// `player`'s options at chain priority (a Closed State on `turn_player`'s turn).
    std::vector<Intent> closedStateOptions(PlayerId player, PlayerId turn_player) {
        state.turn.phase = TurnPhase::MainPhase;
        state.turn.turn_player = turn_player;
        state.turn.ns_state = NeutralShowdownState::Neutral;
        state.turn.oc_state = OpenClosedState::Closed;
        state.turn.priority_holder = player;
        std::vector<Intent> out;
        withEngine([&](GameEngine& e) { out = e.generateLegalActions(); }, nullptr, nullptr,
                   turn_player);
        return out;
    }
    static std::vector<Intent> playsOf(const std::vector<Intent>& legal, GameObjectId card) {
        std::vector<Intent> out;
        for (const auto& i : legal)
            if (i.card == card && (i.type == IntentType::PlayCard ||
                                   i.type == IntentType::PlayActionCard ||
                                   i.type == IntentType::PlayReaction))
                out.push_back(i);
        return out;
    }
    static bool offersPlayTo(const std::vector<Intent>& legal, GameObjectId card, int bf) {
        for (const auto& i : playsOf(legal, card))
            if (i.play_location.has_value() &&
                *i.play_location == LocationId{BattlefieldLocation{bfId(bf)}})
                return true;
        return false;
    }
    /// Every trace line while `body` runs.
    std::vector<std::string> traced(const std::function<void()>& body) {
        std::vector<std::string> lines;
        auto conn = events.on_log.connect([&](const LogEvent& e) { lines.push_back(e.message); });
        body();
        conn.disconnect();
        return lines;
    }
    static bool anyLineHas(const std::vector<std::string>& lines, const std::string& text) {
        return std::any_of(lines.begin(), lines.end(),
                           [&](const std::string& l) { return l.find(text) != std::string::npos; });
    }
};

// ─── G1  1v1: the player going first draws in their first Draw Phase ───────
// CR 487.7 / 488.7 (1v1 Duel / Match): "The player going second channels an
// extra Rune ..." — nothing more. The no-draw sentence is 489.7 / 490.7 /
// 491.7 (FFA3, FFA4, 2v2). (BurnOutScoringTest.FirstPlayerFirstTurn_* pins the
// draw itself; this one plays the first turn's phases.)
TEST_F(Batch4bTest, G1_TheFirstPlayersFirstTurnHasADrawAndTwoRunes) {
    state.turn.turn_player = P1;
    state.turn.starting_player = P1;
    state.turn.turn_number = 0;
    ps(P1).is_first_turn = ps(P2).is_first_turn = true;
    for (int i = 0; i < 3; ++i) addToDeck(P1, kShipyardSkulker);
    const int hand = handSize(P1);
    withEngine([&](GameEngine& e) { e.testHook_drawPhase(); });
    EXPECT_EQ(handSize(P1), hand + 1);
}

// ─── G2  Time Warp on the opponent's turn ──────────────────────────────────
// CR 735: the Additional Turn "is inserted into the turn queue after the
// current turn"; 737: it "does not change the Turn Order ... After that turn
// is completed ... the queue will proceed with its previously queued turns."
TEST_F(Batch4bTest, G2_TimeWarpOnTheOpponentsTurnAddsATurnBeforeTheOwnersOwn) {
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P2;
    state.turn.starting_player = P1;
    state.turn.turn_number = 5;
    auto warp = addToHand(P1, kTimeWarp);
    playThroughEngine(warp);
    std::vector<PlayerId> order;
    std::vector<bool> additional;
    withEngine([&](GameEngine& e) {
        for (int i = 0; i < 4; ++i) {
            e.testHook_nextTurn();
            order.push_back(e.mutableState().turn.turn_player);
            additional.push_back(e.mutableState().turn.is_additional_turn);
        }
    }, nullptr, nullptr, P2);
    EXPECT_EQ(order, (std::vector<PlayerId>{P1, P1, P2, P1}))
        << "P2's turn, P1's Additional Turn, P1's own turn, then the order goes on";
    EXPECT_EQ(additional, (std::vector<bool>{true, false, false, false}));
}

// CR 738, second example: each Additional Turn is inserted right after the
// current turn, so the one created last is taken first.
TEST_F(Batch4bTest, G2_AdditionalTurnsMadeInOneTurnAreTakenNewestFirst) {
    state.turn.phase = TurnPhase::MainPhase;
    state.turn.turn_player = P1;
    state.turn.turn_number = 2;
    std::vector<PlayerId> order;
    withEngine([&](GameEngine& e) {
        auto& s = e.mutableState();
        // P2's Time Warp resolves first, then P1's (on P1's turn).
        s.queueAdditionalTurn(P2);
        s.queueAdditionalTurn(P1);
        for (int i = 0; i < 4; ++i) {
            e.testHook_nextTurn();
            order.push_back(s.turn.turn_player);
        }
    });
    EXPECT_EQ(order, (std::vector<PlayerId>{P1, P2, P2, P1}))
        << "[> A > A* > B* > B > A]";
}

// ─── G4  Setup: the battlefield is chosen (pinned; a question to the project) ─
// CR 487.5 (Duel): "Each player randomly selects one (1) of their three (3)
// Battlefields." CR 488.5 (Match): "Each player selects one ...". An engine game
// asks each player (the Match reading); see plans/reports/batch-4b.md,
// "Questions for the user".
TEST_F(Batch4bTest, G4_Pin_EachPlayerChoosesTheirBattlefield) {
    DeckSubmission d1, d2;
    d1.battlefields = {kVilemawsLair, kRockfallPath, kFortifiedPosition};
    d2.battlefields = {kRockfallPath, kFortifiedPosition, kVilemawsLair};
    state.battlefields.clear();
    int asked = 0;
    PickByPredicateAgent pick([&](int, const std::vector<Intent>& legal) {
        ++asked;
        EXPECT_EQ(legal.size(), 3u);
        return legal[2];
    });
    withEngine([&](GameEngine& e) { e.testHook_setupBattlefields(d1, d2); }, &pick, &pick);
    EXPECT_EQ(asked, 2) << "one decision per player";
    ASSERT_EQ(state.battlefields.size(), 2u);
    EXPECT_EQ(o(state.battlefields[0].card_object_id).card_def_id, kFortifiedPosition);
    EXPECT_EQ(o(state.battlefields[1].card_object_id).card_def_id, kVilemawsLair);
}

// ─── G18  Defender of Tomorrow 936: "Ready a gear." ─────────────────────────
// No "exhausted" in the text: a ready gear is a valid choice (CR 355.9).
TEST_F(Batch4bTest, G18_DefenderOfTomorrow_AReadyGearCanBeChosen) {
    auto legend = legendCard(P1, kDefenderOfTomorrow);
    auto ready_gear = gear(P1, kSunDisc, /*exhausted=*/false);
    fund(P1, 1);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = legend;
    EnginePathAgent agent(EnginePathAgent::choosing(ready_gear));
    run(act, &agent);
    EXPECT_TRUE(agent.wasOffered(ready_gear));
    EXPECT_TRUE(o(legend).is_exhausted) << "the ability was activated";
}

// ─── G21  Kennen, Keeper of Balance from facedown (closed by batch 4a) ──────
// CR 811.1.d.3: his play effect chooses among units at the battlefield he was
// played to — not a unit in the enemy base.
TEST_F(Batch4bTest, G21_KennenFromFacedown_AUnitInTheEnemyBaseIsNotOffered) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    addUnit(P1, kShipyardSkulker, 3, 0);
    auto there = addUnit(P2, kShipyardSkulker, 2, 0);
    auto in_base = addUnit(P2, kShipyardSkulker, 2);   // P2's base
    auto kennen = addToHand(P1, kKennenKeeperOfBalance);
    hideAt(kennen, 0);
    fund(P1, 2);
    EnginePathAgent agent(EnginePathAgent::accepting());
    Intent play = playIntent(kennen);
    play.type = IntentType::PlayReaction;
    run(play, &agent);
    EXPECT_FALSE(agent.wasOffered(in_base));
    EXPECT_FALSE(o(in_base).is_stunned);
    EXPECT_TRUE(agent.wasOffered(there));
}

// ─── G22  A facedown unit is not played where units can't be played ────────
// CR 811.1.d.1: "A hidden permanent must be played to that battlefield."
// Rockfall Path: "Units can't be played here." "Can't" wins (CR 054.1).
TEST_F(Batch4bTest, G22_AHiddenUnitAtRockfallPathIsNotOffered) {
    state.turn.turn_number = 3;
    battlefieldCard(0, kRockfallPath);
    addUnit(P1, kShipyardSkulker, 3, 0);
    auto warrior = addToHand(P1, kTornadoWarrior);
    hideAt(warrior, 0);
    // The same card at an ordinary battlefield is offered.
    state.battlefields[1].controller = P1;
    addUnit(P1, kShipyardSkulker, 3, 1);
    auto tide = addToHand(P1, kTideturner);
    hideAt(tide, 1);
    const auto legal = closedStateOptions(P1, P2);
    EXPECT_TRUE(playsOf(legal, warrior).empty()) << "no unit is played at Rockfall Path";
    EXPECT_FALSE(playsOf(legal, tide).empty());
}

// ─── G23  A Brush over Vilemaw's Lair (closed by batch 4a) ──────────────────
// The Brush has its own two abilities (CR 187.8); "Units can't move from here
// to base." was the Lair's (judge question J3 asks whether 439.1's
// "inheriting all effects or statuses" carries it over; this is the reading).
TEST_F(Batch4bTest, G23_ABrushOverVilemawsLairLetsUnitsMoveToBase) {
    auto father = legendCard(P1, kGreenFather);
    auto lair = battlefieldCard(0, kVilemawsLair);
    auto unit = addUnit(P1, kShipyardSkulker, 3, 0);
    ASSERT_TRUE(state.battlefields[0].blocks_move_to_base);
    AcceptingEnginePathAgent yes;
    fireTriggerThroughEngine(father, TriggerType::WhenIConquerOrHold, &yes, lair);
    ASSERT_NE(state.battlefields[0].card_object_id, lair);
    o(unit).is_exhausted = false;
    bool to_base = false;
    for (const auto& i : mainPhaseOptions(P1))
        if (i.type == IntentType::StandardMove && !i.units_to_move.empty() &&
            i.units_to_move.front() == unit && i.move_destination.has_value() &&
            std::holds_alternative<BaseLocation>(*i.move_destination))
            to_base = true;
    EXPECT_TRUE(to_base);
}

// ─── G24  Green Father on a battlefield that is already a Brush ─────────────
// CR 439.6: a token can be replaced; 439.7.b.1: the card the first token
// replaced is still the one to swap back to.
TEST_F(Batch4bTest, G24_GreenFather_ReplacesABrush_TheOriginalStaysTheOneToSwapBackTo) {
    auto father = legendCard(P1, kGreenFather);
    auto lair = battlefieldCard(0, kVilemawsLair);
    addUnit(P1, kShipyardSkulker, 3, 0);
    AcceptingEnginePathAgent yes;
    fireTriggerThroughEngine(father, TriggerType::WhenIConquerOrHold, &yes, lair);
    const GameObjectId brush1 = state.battlefields[0].card_object_id;
    ASSERT_NE(brush1, lair);
    o(father).is_exhausted = false;
    AcceptingEnginePathAgent yes2;
    fireTriggerThroughEngine(father, TriggerType::WhenIConquerOrHold, &yes2, brush1);
    EXPECT_FALSE(yes2.prompts.empty()) << "the \"may\" is offered";
    const GameObjectId brush2 = state.battlefields[0].card_object_id;
    EXPECT_NE(brush2, brush1) << "a new Brush";
    EXPECT_TRUE(o(father).is_exhausted);
    EXPECT_EQ(o(brush2).card_def_id, kBrush);
    EXPECT_FALSE(onBoard(brush1)) << "the replaced token ceased to exist (CR 439.6)";
    // Swap back: the Lair, not the first Brush.
    o(brush2).controller = P1;
    AcceptingEnginePathAgent yes3;
    fireTriggerThroughEngine(brush2, TriggerType::WhenYouScoreHere, &yes3);
    EXPECT_EQ(state.battlefields[0].card_object_id, lair);
    EXPECT_TRUE(state.battlefields[0].blocks_move_to_base);
}

// ─── H2  healObject acts only on an object in play ──────────────────────────
TEST_F(Batch4bTest, H2_HealingAnObjectThatHasLeftTheBoardAnnouncesNothing) {
    auto card = addToHand(P1, kShipyardSkulker);
    int healed = 0;
    auto conn = events.on_object_state_changed.connect([&](const ObjectStateChangedEvent& e) {
        if (e.what_changed == "healed") ++healed;
    });
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.healObject(card); });
    conn.disconnect();
    EXPECT_EQ(healed, 0);
}

// ─── B3  Immortal Phoenix 37: "play me from your trash" ─────────────────────
// The ability works from the trash (CR 383.2.c.1 and its Phoenix example); a
// Phoenix on the board does not trigger.
TEST_F(Batch4bTest, B3_ImmortalPhoenixOnTheBoardDoesNotTrigger) {
    addUnit(P1, kImmortalPhoenix, 0, 0);
    auto victim = addUnit(P2, kShipyardSkulker, 2, 0);
    auto seeker = addToHand(P1, kVoidSeeker);
    fund(P1, 2, 2);   // the spell, and [1][R] if anything asked for it
    EnginePathAgent agent(EnginePathAgent::choosing(victim));
    std::vector<std::string> lines;
    lines = traced([&] { playThroughEngine(seeker, &agent); });
    ASSERT_FALSE(onBoard(victim));
    EXPECT_FALSE(anyLineHas(lines, "TRIGGER: Immortal Phoenix"));
}

TEST_F(Batch4bTest, B3_ImmortalPhoenixInTheTrashTriggers) {
    auto phoenix = addToHand(P1, kImmortalPhoenix);
    ps(P1).hand.clear();
    o(phoenix).zone = ZoneType::Trash;
    ps(P1).trash.push_back(phoenix);
    auto victim = addUnit(P2, kShipyardSkulker, 2, 0);
    auto seeker = addToHand(P1, kVoidSeeker);
    fund(P1, 0, 0);
    std::vector<std::string> lines;
    EnginePathAgent agent(EnginePathAgent::choosing(victim, EnginePathAgent::May::No));
    lines = traced([&] { playThroughEngine(seeker, &agent); });
    ASSERT_FALSE(onBoard(victim));
    EXPECT_TRUE(anyLineHas(lines, "TRIGGER: Immortal Phoenix"));
}

// ─── D25  A facedown card in its controller's own Neutral Open State ────────
// CR 811.6: a hidden card "gains Reaction the turn after it is hidden", and
// Reaction "is not restricted to Closed States or Showdowns. This permission is
// inclusive of all other timings" (813.2) — its controller's Main Phase too.
TEST_F(Batch4bTest, D25_AFacedownCardIsOfferedInTheMainPhase) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    addUnit(P1, kShipyardSkulker, 3, 0);
    auto guards = addToHand(P1, kGuards);
    hideAt(guards, 0);
    const auto legal = mainPhaseOptions(P1);
    EXPECT_FALSE(playsOf(legal, guards).empty());
}

// The play offered there is executed (a facedown card, from the Main Phase).
TEST_F(Batch4bTest, D25_AFacedownCardPlayedFromTheMainPhaseIsPlayed) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    addUnit(P1, kShipyardSkulker, 3, 0);
    auto warrior = addToHand(P1, kTornadoWarrior);
    hideAt(warrior, 0);
    const auto plays = playsOf(mainPhaseOptions(P1), warrior);
    ASSERT_EQ(plays.size(), 1u);
    run(plays.front());
    EXPECT_TRUE(onBoard(warrior));
    EXPECT_EQ(o(warrior).battlefieldId(), std::optional<BattlefieldId>{bfId(0)});
}

// ─── D37  Rengar, Trophy Hunter: [Ambush]'s location at ordinary timing ─────
// CR 822.1.b: "I may be played to a battlefield where you control Units" —
// a location permission, separate from the [Reaction] it also grants; 822.1.d's
// example widens it for Rengar to "a battlefield where there are enemy units".
TEST_F(Batch4bTest, D37_RengarCanBePlayedToAnEnemyBattlefieldInTheMainPhase) {
    addUnit(P2, kShipyardSkulker, 3, 1);
    state.battlefields[1].controller = P2;
    auto rengar = addToHand(P1, kRengarTrophyHunter);
    fund(P1, 5, 1);
    EXPECT_TRUE(offersPlayTo(mainPhaseOptions(P1), rengar, 1));
}

TEST_F(Batch4bTest, D37_AnAmbushUnitCanBePlayedWhereYouHaveUnitsInTheMainPhase) {
    // A battlefield where P1 has a unit but which P1 does not control.
    addUnit(P1, kShipyardSkulker, 3, 1);
    auto rengar = addToHand(P1, kRengarTrophyHunter);
    fund(P1, 5, 1);
    EXPECT_TRUE(offersPlayTo(mainPhaseOptions(P1), rengar, 1));
    EXPECT_FALSE(offersPlayTo(mainPhaseOptions(P1), rengar, 0)) << "nobody there";
}

// ─── D63  Rengar in the Champion Zone as an [Ambush] play ───────────────────
// The Chosen Champion is played from its zone like a card from hand (CR
// 420.1.a); [Ambush] gives it [Reaction] to a battlefield where you have units.
TEST_F(Batch4bTest, D63_RengarInTheChampionZoneIsOfferedAsAnAmbushPlay) {
    addUnit(P1, kShipyardSkulker, 3, 0);
    state.battlefields[0].controller = P1;
    auto rengar = addToHand(P1, kRengarTrophyHunter);
    ps(P1).hand.clear();
    o(rengar).zone = ZoneType::ChampionZone;
    ps(P1).champion_zone = rengar;
    fund(P1, 5, 1);
    EXPECT_TRUE(offersPlayTo(closedStateOptions(P1, P2), rengar, 0));
}

// ─── D74  Facebreaker / Switcheroo from facedown ─────────────────────────────
// CR 811.1.d / 811.1.d.2: the choices are made among units at the battlefield
// it was hidden at, and it cannot be played without them.
TEST_F(Batch4bTest, D74_FacebreakerFromFacedownWithoutAPairThereIsNotOffered) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    addUnit(P1, kShipyardSkulker, 3, 0);
    addUnit(P1, kShipyardSkulker, 3, 0);
    addUnit(P2, kShipyardSkulker, 3, 1);      // the enemy is elsewhere
    addUnit(P1, kShipyardSkulker, 3, 1);
    auto fb = addToHand(P1, kFacebreaker);
    hideAt(fb, 0);
    EXPECT_TRUE(playsOf(closedStateOptions(P1, P2), fb).empty());
}

TEST_F(Batch4bTest, D74_FacebreakerFromFacedownIsOneOptionAndChoosesThere) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    auto mine = addUnit(P1, kShipyardSkulker, 3, 0);
    addUnit(P1, kShipyardSkulker, 3, 0);
    auto theirs = addUnit(P2, kShipyardSkulker, 3, 0);
    addUnit(P2, kShipyardSkulker, 3, 0);
    auto mine_elsewhere = addUnit(P1, kShipyardSkulker, 3, 1);
    auto theirs_elsewhere = addUnit(P2, kShipyardSkulker, 3, 1);
    auto fb = addToHand(P1, kFacebreaker);
    hideAt(fb, 0);
    const auto plays = playsOf(closedStateOptions(P1, P2), fb);
    ASSERT_EQ(plays.size(), 1u) << "one play; the pair is asked";
    int n = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        const GameObjectId want = n++ == 0 ? mine : theirs;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        return legal.front();
    });
    run(plays.front(), &agent);
    EXPECT_FALSE(agent.wasOffered(mine_elsewhere));
    EXPECT_FALSE(agent.wasOffered(theirs_elsewhere));
    EXPECT_TRUE(o(mine).is_stunned);
    EXPECT_TRUE(o(theirs).is_stunned);
}

TEST_F(Batch4bTest, D74_SwitcherooFromFacedownChoosesOnlyThere) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    auto a = addUnit(P1, kShipyardSkulker, 2, 0);
    auto b = addUnit(P2, kShipyardSkulker, 5, 0);
    auto elsewhere1 = addUnit(P1, kShipyardSkulker, 1, 1);
    auto elsewhere2 = addUnit(P2, kShipyardSkulker, 7, 1);
    auto sw = addToHand(P1, kSwitcheroo);
    hideAt(sw, 0);
    const auto plays = playsOf(closedStateOptions(P1, P2), sw);
    ASSERT_EQ(plays.size(), 1u);
    int n = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        const GameObjectId want = n++ == 0 ? a : b;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        return legal.front();
    });
    run(plays.front(), &agent);
    EXPECT_FALSE(agent.wasOffered(elsewhere1));
    EXPECT_FALSE(agent.wasOffered(elsewhere2));
    EXPECT_EQ(o(a).current_might, 5);
    EXPECT_EQ(o(b).current_might, 2);
}

TEST_F(Batch4bTest, D74_SwitcherooFromFacedownWithOneUnitThereIsNotOffered) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    addUnit(P1, kShipyardSkulker, 2, 0);
    addUnit(P1, kShipyardSkulker, 1, 1);
    addUnit(P2, kShipyardSkulker, 7, 1);
    auto sw = addToHand(P1, kSwitcheroo);
    hideAt(sw, 0);
    EXPECT_TRUE(playsOf(closedStateOptions(P1, P2), sw).empty());
}

// ─── D80  Lonely Poro: the two printings read "died alone" the same way ─────
// QUESTION (docs/judge-questions-2026-10.md): the engine's reading is that a
// friendly unit that dies at the same time was "here" (CR 383.2.c.3, 359.3.e.13:
// the last-known information of units that die together is taken before the
// first leaves). Lonely Poro 777 already reads it so; 359 read the board as
// its ability resolved.
TEST_F(Batch4bTest, D80_LonelyPoro359_UnitsThatDieTogetherDidNotDieAlone) {
    for (int i = 0; i < 4; ++i) addToDeck(P1, kShipyardSkulker);
    auto poro = addUnit(P1, kLonelyPoro359, 1, 0);
    auto other = addUnit(P1, kShipyardSkulker, 1, 0);
    o(poro).damage_marked = 3;
    o(other).damage_marked = 3;
    const int before = handSize(P1);
    withEngine([&](GameEngine& e) { e.testHook_processLethalDamage(); e.testHook_runChain(); });
    EXPECT_EQ(handSize(P1) - before, 0);
}

TEST_F(Batch4bTest, D80_LonelyPoro359_AUnitThatArrivesLaterDoesNotChangeIt) {
    for (int i = 0; i < 4; ++i) addToDeck(P1, kShipyardSkulker);
    auto poro = addUnit(P1, kLonelyPoro359, 1, 0);
    o(poro).damage_marked = 3;
    const int before = handSize(P1);
    withEngine([&](GameEngine& e) {
        e.testHook_processLethalDamage();
        // Before the Deathknell resolves, a friendly unit arrives there.
        auto& s = e.mutableState();
        auto id = s.createObject();
        auto& u = s.getObject(id);
        u.owner = u.controller = P1;
        u.card_type = CardType::Unit;
        u.name = "Late";
        u.base_might = u.current_might = 1;
        u.zone = ZoneType::BattlefieldZone;
        u.location = BattlefieldLocation{bfId(0)};
        e.testHook_runChain();
    });
    EXPECT_EQ(handSize(P1) - before, 1) << "it died alone";
}

}  // namespace riftbound::test::batch_4b
