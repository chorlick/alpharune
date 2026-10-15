/// @file test_batch1_trigger_conditions.cpp
/// Fix batch 1, findings D19 / D27 / D33 / D51 / D77: a conditional statement
/// printed immediately after a trigger condition is part of the trigger
/// condition (CR 383.2.a.1), "the Nth time ..." is the trigger condition
/// itself (CR 383.1), a "once each turn" ability that was performed this turn
/// does not trigger (CR 383.3.e.1), and an attack / defend trigger whose extra
/// requirement is false when the unit gains the designation does not trigger
/// (CR 383.4.e.2.b / 383.4.f.2.b).
///
/// So for every card below:
///   1. the condition is false when the event happens -> NOTHING is put on the
///      chain (no chain item id is allocated: no empty item, no priority
///      window);
///   2. it is true -> the ability goes on the chain and its effect happens;
///   3. (where cheap) it is true at the event and false by the time the
///      ability resolves -> the effect STILL happens ("If she is removed in
///      reaction to the triggered ability, it will still resolve").
///
/// Every test drives the real engine path: the ability is queued through
/// TriggerManager (which is what asks Card::triggerCondition), finalized and
/// resolved by a GameEngine, and the agent answers the choices.

#include "tests/cards/card_test_fixture.h"
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

namespace riftbound::test {
namespace batch1_trigger_conditions {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kClockworkKeeper = 44;
constexpr CardDefId kPoroHerder = 61;
constexpr CardDefId kWraithOfEchoes = 118;
constexpr CardDefId kVolibearImposing = 158;
constexpr CardDefId kYasuoWindrider205 = 205;
constexpr CardDefId kTheGrandPlaza = 288;
constexpr CardDefId kIncinerate = 301;        // a spell
constexpr CardDefId kBlastCorpsCadet = 336;
constexpr CardDefId kChemtechCask = 386;
constexpr CardDefId kFrostcoatCub = 390;
constexpr CardDefId kDropboarder = 395;
constexpr CardDefId kSeaMonkey = 420;
constexpr CardDefId kAkshanMischievous = 431;
constexpr CardDefId kFioraPeerless = 432;
constexpr CardDefId kLucianMerciless = 435;
constexpr CardDefId kEdgeOfNight = 460;
constexpr CardDefId kDravenAudacious = 469;
constexpr CardDefId kYasuoWindrider546 = 546;
constexpr CardDefId kRevna = 565;
constexpr CardDefId kYetiBrawler = 580;
constexpr CardDefId kPykeDocksideButcher = 590;
constexpr CardDefId kVexMocking = 617;
constexpr CardDefId kGutterPalace = 650;
constexpr CardDefId kPoppyParagon = 678;
constexpr CardDefId kEvelynnEntrancing = 703;
constexpr CardDefId kKhaZixMutatingHorror = 705;
constexpr CardDefId kPykeReturned = 707;
constexpr CardDefId kVexApathetic = 712;
constexpr CardDefId kLoyalPoro = 718;         // deck / hand filler
constexpr CardDefId kForgottenLibrary = 767;
constexpr CardDefId kVirtuoso = 782;
constexpr CardDefId kEclipseDragon = 803;
constexpr CardDefId kRenektonRageFueled = 806;
constexpr CardDefId kZedFromTheShadows = 810;
constexpr CardDefId kAffectionatePoro = 811;
constexpr CardDefId kShenScourgeOfShadows = 829;
constexpr CardDefId kAstralHeron = 831;
constexpr CardDefId kPatchedPorobot = 845;
constexpr CardDefId kNasusGuardianOfKnowledge = 850;
constexpr CardDefId kSwainVisionary = 852;
constexpr CardDefId kTornadoWarrior = 886;
constexpr CardDefId kGustMonk = 888;
constexpr CardDefId kMasaCrashingThunder = 907;
constexpr CardDefId kShenLeaderOfTheKinkouOrder = 925;
constexpr CardDefId kProtectiveSands = 949;

using May = EnginePathAgent::May;

}  // namespace

class Batch1TriggerConditions : public CardTestFixture {
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

    /// True if a trace line containing `text` was logged (what a card's effect
    /// announces when it happens).
    bool logged(const std::string& text) const {
        return std::any_of(log_.begin(), log_.end(), [&](const std::string& line) {
            return line.find(text) != std::string::npos;
        });
    }

    /// The event happens: `source`'s ability for `trigger` is offered to
    /// TriggerManager (which asks Card::triggerCondition), then the engine
    /// finalizes and resolves whatever went on the chain. Returns whether an
    /// ability was put on the chain at all (every chain item allocates an id).
    /// `before_resolution` runs on the ENGINE's state after the event and
    /// before anything resolves — "in reaction to the triggered ability".
    using Edit = std::function<void(GameState&)>;
    bool fire(GameObjectId source, TriggerType trigger, GameObjectId subject = kInvalidId,
              AgentInterface* agent = nullptr, const Edit& before_resolution = nullptr,
              PlayerId controller = PlayerId::None) {
        if (controller == PlayerId::None) controller = o(source).controller;
        bool queued = false;
        withEngine([&](GameEngine& e) {
            const ChainItemId before = e.mutableState().chain.next_id;
            e.testHook_executor().queueTriggeredAbility(source, controller, trigger, subject);
            queued = e.mutableState().chain.next_id != before;
            if (before_resolution) before_resolution(e.mutableState());
            settleLikeTheEngine(e);
        }, controller == P1 ? agent : nullptr, controller == P2 ? agent : nullptr);
        return queued;
    }

    /// It is `p`'s Main Phase (withEngine leaves a phase that is set alone).
    void turnOf(PlayerId p) {
        state.turn.phase = TurnPhase::MainPhase;
        state.turn.turn_player = p;
    }

    GameObjectId vanilla(PlayerId p, int at_bf = -1, int might = 2) {
        return addUnit(p, kInvalidId, might, at_bf);
    }
    /// A gear `p` controls, in their base or at a battlefield.
    GameObjectId gearOf(PlayerId p, CardDefId def = kInvalidId, int at_bf = -1) {
        auto id = state.createObject();
        auto& g = o(id);
        g.owner = p;
        g.controller = p;
        g.card_def_id = def;
        g.card_type = CardType::Gear;
        g.name = "TestGear";
        if (def != kInvalidId) {
            const auto& d = card_db.get(def);
            g.name = d.name;
            g.tags = d.tags;
            g.keywords = d.keywords;
            g.might_bonus = d.might_bonus;
        }
        if (at_bf >= 0) {
            g.zone = ZoneType::BattlefieldZone;
            g.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{p};
        }
        return id;
    }
    /// The battlefield card of slot `slot`.
    GameObjectId battlefieldCard(int slot, CardDefId def) {
        auto id = state.createObject();
        auto& b = o(id);
        b.card_def_id = def;
        b.card_type = CardType::Battlefield;
        b.name = card_db.get(def).name;
        state.battlefields[BattlefieldId::fromRaw(slot)].card_object_id = id;
        return id;
    }
    /// `p`'s legend.
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
    /// A spell of `p` that has resolved (it is in the trash), stamped the way
    /// TriggerManager::onCardPlayed stamps a played card: what was spent on
    /// it and which play of the turn it was.
    GameObjectId spellPlayed(PlayerId p, int energy_spent, int ordinal = 1) {
        auto id = addToHand(p, kIncinerate);
        auto& h = ps(p).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        o(id).zone = ZoneType::Trash;
        ps(p).trash.push_back(id);
        o(id).card_counters["__energy_spent_on_play"] = energy_spent;
        o(id).card_counters["__play_ordinal"] = ordinal;
        o(id).card_counters["__play_ordinal_turn"] = state.turn.turn_number + 1;
        return id;
    }
    /// `n` cards on top of `p`'s Main Deck, so a draw is a draw.
    void stock(PlayerId p, int n = 3) {
        for (int i = 0; i < n; ++i) addToDeck(p, kLoyalPoro);
    }
    void runes(PlayerId p, int n) {
        for (int i = 0; i < n; ++i) addRune(p, Domain::Fury);
    }
    int goldTokens(PlayerId p) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.isGear() && obj.controller == p && obj.location.has_value() &&
                obj.name == "Gold") ++n;
        return n;
    }
    int unitsNamed(PlayerId p, const std::string& name) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == p && obj.location.has_value() &&
                obj.name == name) ++n;
        return n;
    }
    /// Put a unit of the ENGINE's state into its controller's base.
    static void toBase(GameState& s, GameObjectId id) {
        auto& u = s.getObject(id);
        u.zone = ZoneType::Base;
        u.location = BaseLocation{u.controller};
    }
    std::optional<BattlefieldId> bfOf(GameObjectId id) { return o(id).battlefieldId(); }
};

// ═══ D19 ════════════════════════════════════════════════════════════════════
// Astral Heron: "When you play your first card each turn, if I'm at a
// battlefield, your next card costs [2][A][A] less."

TEST_F(Batch1TriggerConditions, AstralHeron_AtBaseDoesNotTrigger) {
    auto heron = addUnit(P1, kAstralHeron, 0, -1);
    auto first = spellPlayed(P1, 0, 1);
    EXPECT_FALSE(fire(heron, TriggerType::WhenYouPlayACard, first));
    EXPECT_TRUE(ps(P1).cost_modifiers.empty());
}

TEST_F(Batch1TriggerConditions, AstralHeron_AtABattlefieldDiscountsTheNextCard) {
    auto heron = addUnit(P1, kAstralHeron, 0, 0);
    auto first = spellPlayed(P1, 0, 1);
    EXPECT_TRUE(fire(heron, TriggerType::WhenYouPlayACard, first));
    ASSERT_EQ(ps(P1).cost_modifiers.size(), 1u);
    EXPECT_EQ(ps(P1).cost_modifiers[0].energy_reduction, 2);
    EXPECT_EQ(ps(P1).cost_modifiers[0].power_reduction, 2);
}

TEST_F(Batch1TriggerConditions, AstralHeron_StillResolvesAfterLeavingTheBattlefield) {
    auto heron = addUnit(P1, kAstralHeron, 0, 0);
    auto first = spellPlayed(P1, 0, 1);
    EXPECT_TRUE(fire(heron, TriggerType::WhenYouPlayACard, first, nullptr,
                     [&](GameState& s) { toBase(s, heron); }));
    EXPECT_EQ(ps(P1).cost_modifiers.size(), 1u)
        << "the condition was true when the card was played (CR 383.2.a.1)";
}

// ═══ D27 ════════════════════════════════════════════════════════════════════
// Pyke, Returned: "Once each turn, when an enemy unit dies while I'm at a
// battlefield, play a Gold gear token exhausted."

TEST_F(Batch1TriggerConditions, PykeReturned_AtBaseDoesNotTrigger) {
    auto pyke = addUnit(P1, kPykeReturned, 0, -1);
    auto dead = vanilla(P2, 0);
    EXPECT_FALSE(fire(pyke, TriggerType::WhenAnEnemyUnitDies, dead));
    EXPECT_EQ(goldTokens(P1), 0);
}

TEST_F(Batch1TriggerConditions, PykeReturned_AtABattlefieldPlaysGoldOnceEachTurn) {
    auto pyke = addUnit(P1, kPykeReturned, 0, 0);
    auto dead = vanilla(P2, 1);
    EXPECT_TRUE(fire(pyke, TriggerType::WhenAnEnemyUnitDies, dead));
    EXPECT_EQ(goldTokens(P1), 1);
    // A second enemy death this turn: already performed, so it does not
    // trigger (CR 383.3.e.1) — nothing goes on the chain.
    EXPECT_FALSE(fire(pyke, TriggerType::WhenAnEnemyUnitDies, dead));
    EXPECT_EQ(goldTokens(P1), 1);
    // Next turn it triggers again.
    state.turn.turn_number += 1;
    EXPECT_TRUE(fire(pyke, TriggerType::WhenAnEnemyUnitDies, dead));
    EXPECT_EQ(goldTokens(P1), 2);
}

TEST_F(Batch1TriggerConditions, PykeReturned_TwoDeathsBeforeItResolvesTriggerOnce) {
    auto pyke = addUnit(P1, kPykeReturned, 0, 0);
    auto dead = vanilla(P2, 1);
    int queued = 0;
    withEngine([&](GameEngine& e) {
        const auto before = e.mutableState().chain.next_id.raw();
        e.testHook_executor().queueTriggeredAbility(pyke, P1, TriggerType::WhenAnEnemyUnitDies, dead);
        e.testHook_executor().queueTriggeredAbility(pyke, P1, TriggerType::WhenAnEnemyUnitDies, dead);
        queued = static_cast<int>(e.mutableState().chain.next_id.raw() - before);
        settleLikeTheEngine(e);
    });
    EXPECT_EQ(queued, 1) << "the first one is still on the chain";
    EXPECT_EQ(goldTokens(P1), 1);
}

TEST_F(Batch1TriggerConditions, PykeReturned_StillResolvesAfterLeavingTheBattlefield) {
    auto pyke = addUnit(P1, kPykeReturned, 0, 0);
    auto dead = vanilla(P2, 1);
    EXPECT_TRUE(fire(pyke, TriggerType::WhenAnEnemyUnitDies, dead, nullptr,
                     [&](GameState& s) { toBase(s, pyke); }));
    EXPECT_EQ(goldTokens(P1), 1);
}

// ═══ D33 ════════════════════════════════════════════════════════════════════
// Edge of Night: "When you play this from face down, attach it to a unit you
// control (here)."

TEST_F(Batch1TriggerConditions, EdgeOfNight_PlayedFromHandDoesNotTrigger) {
    auto edge = gearOf(P1, kEdgeOfNight, 0);
    auto unit = vanilla(P1, 0);
    EXPECT_FALSE(fire(edge, TriggerType::WhenYouPlayThis));
    EXPECT_FALSE(o(edge).attached_to.has_value());
    (void)unit;
}

TEST_F(Batch1TriggerConditions, EdgeOfNight_PlayedFromFaceDownAttachesToAUnitHere) {
    auto edge = gearOf(P1, kEdgeOfNight, 0);
    auto unit = vanilla(P1, 0);
    o(edge).card_counters["__played_from_facedown"] = 1;   // GameEngine::executePlayFromHidden
    o(edge).card_counters["__facedown_play_turn"] = state.turn.turn_number;
    o(edge).card_counters["__facedown_play_bf"] =
        static_cast<int>(state.battlefields[0].id.raw()) + 1;   // "here"
    EXPECT_TRUE(fire(edge, TriggerType::WhenYouPlayThis));
    EXPECT_TRUE(o(edge).attached_to == std::optional<GameObjectId>{unit});
    // The mark covers that play only: the next play of the card drops it
    // (Card::forgetPlayPayments; testing phase B2 — the card used to clear it
    // itself as it resolved).
    Card::forgetPlayPayments(state, card_registry.get(kEdgeOfNight), P1, edge);
    EXPECT_FALSE(triggersNow(kEdgeOfNight, P1, edge, TriggerType::WhenYouPlayThis));
}

TEST_F(Batch1TriggerConditions, EdgeOfNight_AMarkFromAnEarlierTurnIsNotThisPlay) {
    auto edge = gearOf(P1, kEdgeOfNight, 0);
    vanilla(P1, 0);
    state.turn.turn_number = 5;
    o(edge).card_counters["__played_from_facedown"] = 1;
    o(edge).card_counters["__facedown_play_turn"] = 3;
    EXPECT_FALSE(fire(edge, TriggerType::WhenYouPlayThis));
    EXPECT_FALSE(o(edge).attached_to.has_value());
}

// Vex, Apathetic: "When an opponent plays a unit while I'm at a battlefield,
// [Stun] it. They can't move it this turn."

TEST_F(Batch1TriggerConditions, VexApathetic_AtBaseDoesNotTrigger) {
    auto vex = addUnit(P2, kVexApathetic, 0, -1);
    auto played = vanilla(P1);
    EXPECT_FALSE(fire(vex, TriggerType::WhenOpponentPlaysAUnit, played));
    EXPECT_FALSE(o(played).is_stunned);
}

TEST_F(Batch1TriggerConditions, VexApathetic_AtABattlefieldStunsThePlayedUnit) {
    auto vex = addUnit(P2, kVexApathetic, 0, 0);
    auto played = vanilla(P1);
    EXPECT_TRUE(fire(vex, TriggerType::WhenOpponentPlaysAUnit, played));
    EXPECT_TRUE(o(played).is_stunned);
    EXPECT_TRUE(o(played).cant_move_this_turn);
}

TEST_F(Batch1TriggerConditions, VexApathetic_StillStunsAfterLeavingTheBattlefield) {
    auto vex = addUnit(P2, kVexApathetic, 0, 0);
    auto played = vanilla(P1);
    EXPECT_TRUE(fire(vex, TriggerType::WhenOpponentPlaysAUnit, played, nullptr,
                     [&](GameState& s) { toBase(s, vex); }));
    EXPECT_TRUE(o(played).is_stunned)
        << "she was at a battlefield when the unit was played (CR 383.2.a.1)";
}

// Evelynn, Entrancing: "When you play me from face down on your turn, you may
// move an enemy unit at a different location to my battlefield."

TEST_F(Batch1TriggerConditions, Evelynn_FromHandOrOnAnOpponentsTurnDoesNotTrigger) {
    auto eve = addUnit(P1, kEvelynnEntrancing, 0, 0);
    auto enemy = vanilla(P2, 1);
    turnOf(P1);
    EXPECT_FALSE(fire(eve, TriggerType::WhenYouPlayMe)) << "played from hand";

    o(eve).card_counters["__played_from_facedown"] = 1;
    o(eve).card_counters["__facedown_play_turn"] = state.turn.turn_number;
    turnOf(P2);
    EXPECT_FALSE(fire(eve, TriggerType::WhenYouPlayMe)) << "from face down, an opponent's turn";
    EXPECT_EQ(bfOf(enemy), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

TEST_F(Batch1TriggerConditions, Evelynn_FromFaceDownOnYourTurnMovesAnEnemyUnit) {
    auto eve = addUnit(P1, kEvelynnEntrancing, 0, 0);
    auto enemy = vanilla(P2, 1);
    turnOf(P1);
    o(eve).card_counters["__played_from_facedown"] = 1;
    o(eve).card_counters["__facedown_play_turn"] = state.turn.turn_number;
    EnginePathAgent agent(EnginePathAgent::choosing(enemy, May::Yes));
    EXPECT_TRUE(fire(eve, TriggerType::WhenYouPlayMe, kInvalidId, &agent));
    EXPECT_EQ(bfOf(enemy), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
}

// Tornado Warrior: "When you play me from face down, you may empower
// something here. Disempower it at end of turn."

TEST_F(Batch1TriggerConditions, TornadoWarrior_PlayedFromHandDoesNotTrigger) {
    auto warrior = addUnit(P1, kTornadoWarrior, 0, 0);
    auto ally = vanilla(P1, 0);
    EXPECT_FALSE(fire(warrior, TriggerType::WhenYouPlayMe));
    EXPECT_FALSE(o(ally).isEmpowered());
}

TEST_F(Batch1TriggerConditions, TornadoWarrior_PlayedFromFaceDownEmpowers) {
    auto warrior = addUnit(P1, kTornadoWarrior, 0, 0);
    auto ally = vanilla(P1, 0);
    o(warrior).card_counters["__played_from_facedown"] = 1;
    o(warrior).card_counters["__facedown_play_turn"] = state.turn.turn_number;
    EnginePathAgent agent(EnginePathAgent::choosing(ally, May::Yes));
    EXPECT_TRUE(fire(warrior, TriggerType::WhenYouPlayMe, kInvalidId, &agent));
    EXPECT_TRUE(o(ally).isEmpowered());
}

// ═══ D51 ════════════════════════════════════════════════════════════════════
// Kha'Zix, Mutating Horror: "When I attack or defend, if an enemy unit is
// alone here, give me +2 [M] this turn and gain 2 XP."

TEST_F(Batch1TriggerConditions, KhaZix_TwoEnemyUnitsHereDoesNotTrigger) {
    auto kha = addUnit(P1, kKhaZixMutatingHorror, 0, 0);
    vanilla(P2, 0);
    vanilla(P2, 0);
    EXPECT_FALSE(fire(kha, TriggerType::WhenIAttackOrDefend));
    EXPECT_EQ(o(kha).temp_might_bonus, 0);
    EXPECT_EQ(ps(P1).xp, 0);
}

TEST_F(Batch1TriggerConditions, KhaZix_AnEnemyUnitAloneHereGivesMightAndXp) {
    auto kha = addUnit(P1, kKhaZixMutatingHorror, 0, 0);
    vanilla(P2, 0);
    EXPECT_TRUE(fire(kha, TriggerType::WhenIAttackOrDefend));
    EXPECT_EQ(o(kha).temp_might_bonus, 2);
    EXPECT_EQ(ps(P1).xp, 2);
}

TEST_F(Batch1TriggerConditions, KhaZix_StillResolvesWhenASecondEnemyArrives) {
    auto kha = addUnit(P1, kKhaZixMutatingHorror, 0, 0);
    vanilla(P2, 0);
    auto late = vanilla(P2, -1);
    EXPECT_TRUE(fire(kha, TriggerType::WhenIAttackOrDefend, kInvalidId, nullptr,
                     [&](GameState& s) {
                         auto& u = s.getObject(late);
                         u.zone = ZoneType::BattlefieldZone;
                         u.location = BattlefieldLocation{BattlefieldId::fromRaw(0)};
                     }));
    EXPECT_EQ(o(kha).temp_might_bonus, 2);
    EXPECT_EQ(ps(P1).xp, 2);
}

// The ability still resolves when Kha'Zix has left the board since it
// triggered — the XP is gained — but "give me +2 [M]" has no unit to give it
// to: nothing is written to the card in the trash (the checked build throws
// on board state carried by an off-board object; found by the pairing run).
TEST_F(Batch1TriggerConditions, KhaZix_DiedBeforeItResolves_GainsTheXpAndWritesNothingToTheCard) {
    auto kha = addUnit(P1, kKhaZixMutatingHorror, 0, 0);
    vanilla(P2, 0);
    bool queued = false;
    withEngine([&](GameEngine& e) {
        const ChainItemId before = e.mutableState().chain.next_id;
        e.testHook_executor().queueTriggeredAbility(kha, P1, TriggerType::WhenIAttackOrDefend,
                                                    kInvalidId);
        queued = e.mutableState().chain.next_id != before;
        e.testHook_executor().killObject(kha);        // in response
        settleLikeTheEngine(e);
    });
    ASSERT_TRUE(queued);
    EXPECT_FALSE(o(kha).location.has_value());
    EXPECT_EQ(o(kha).temp_might_bonus, 0);
    EXPECT_EQ(ps(P1).xp, 2);
}

// Fiora, Peerless: "When I attack or defend one on one, double my Might this
// combat."

TEST_F(Batch1TriggerConditions, FioraPeerless_TwoAttackersIsNotOneOnOne) {
    auto fiora = addUnit(P1, kFioraPeerless, 0, 0);
    auto ally = vanilla(P1, 0);
    auto enemy = vanilla(P2, 0);
    o(fiora).combat_designation = CombatDesignation::Attacker;
    o(ally).combat_designation = CombatDesignation::Attacker;
    o(enemy).combat_designation = CombatDesignation::Defender;
    EXPECT_FALSE(fire(fiora, TriggerType::WhenIAttackOrDefend));
    EXPECT_FALSE(logged("FIORA PEERLESS: one on one"));
}

TEST_F(Batch1TriggerConditions, FioraPeerless_NoDefenderIsNotOneOnOne) {
    auto fiora = addUnit(P1, kFioraPeerless, 0, 0);
    o(fiora).combat_designation = CombatDesignation::Attacker;
    EXPECT_FALSE(fire(fiora, TriggerType::WhenIAttackOrDefend));
}

TEST_F(Batch1TriggerConditions, FioraPeerless_OneOnOneDoublesHerMight) {
    auto fiora = addUnit(P1, kFioraPeerless, 0, 0);
    auto enemy = vanilla(P2, 0);
    o(fiora).combat_designation = CombatDesignation::Attacker;
    o(enemy).combat_designation = CombatDesignation::Defender;
    EXPECT_TRUE(fire(fiora, TriggerType::WhenIAttackOrDefend));
    EXPECT_TRUE(logged("FIORA PEERLESS: one on one"));
}

TEST_F(Batch1TriggerConditions, FioraPeerless_DefendingOneOnOneTriggersToo) {
    auto fiora = addUnit(P1, kFioraPeerless, 0, 0);
    auto enemy = vanilla(P2, 0);
    o(fiora).combat_designation = CombatDesignation::Defender;
    o(enemy).combat_designation = CombatDesignation::Attacker;
    EXPECT_TRUE(fire(fiora, TriggerType::WhenIAttackOrDefend));
}

// ═══ D77 ════════════════════════════════════════════════════════════════════
// Yasuo, Windrider (205 and 546): "The third time I move in a turn, you score
// 1 point."

TEST_F(Batch1TriggerConditions, YasuoWindrider_OnlyTheThirdMoveTriggers) {
    for (CardDefId def : {kYasuoWindrider205, kYasuoWindrider546}) {
        SCOPED_TRACE(toString(def));
        ps(P1).score = 0;
        auto yasuo = addUnit(P1, def, 0, 0);
        // GameObject::moves_this_turn is bumped before the move is announced.
        o(yasuo).moves_this_turn = 1;
        EXPECT_FALSE(fire(yasuo, TriggerType::WhenIMove));
        o(yasuo).moves_this_turn = 2;
        EXPECT_FALSE(fire(yasuo, TriggerType::WhenIMove));
        EXPECT_EQ(ps(P1).score, 0);
        o(yasuo).moves_this_turn = 3;
        EXPECT_TRUE(fire(yasuo, TriggerType::WhenIMove));
        EXPECT_EQ(ps(P1).score, 1);
        o(yasuo).moves_this_turn = 4;
        EXPECT_FALSE(fire(yasuo, TriggerType::WhenIMove));
        EXPECT_EQ(ps(P1).score, 1);
    }
}

// ═══ The same defect class, found by the sweep ══════════════════════════════

// The Grand Plaza: "When you hold here, if you have 7+ units here, you win
// the game."

TEST_F(Batch1TriggerConditions, GrandPlaza_SixUnitsDoesNotTrigger) {
    auto plaza = battlefieldCard(0, kTheGrandPlaza);
    for (int i = 0; i < 6; ++i) vanilla(P1, 0);
    EXPECT_FALSE(fire(plaza, TriggerType::WhenYouHoldHere, kInvalidId, nullptr, nullptr, P1));
    EXPECT_FALSE(state.game_over);
}

TEST_F(Batch1TriggerConditions, GrandPlaza_SevenUnitsWinsEvenIfOneLeavesInResponse) {
    auto plaza = battlefieldCard(0, kTheGrandPlaza);
    GameObjectId last = kInvalidId;
    for (int i = 0; i < 7; ++i) last = vanilla(P1, 0);
    EXPECT_TRUE(fire(plaza, TriggerType::WhenYouHoldHere, kInvalidId, nullptr,
                     [&](GameState& s) { toBase(s, last); }, P1));
    EXPECT_TRUE(state.game_over);
    EXPECT_TRUE(state.winner == P1);
}

// Forgotten Library: "While you control this battlefield, when you play a
// spell, if you spent [4] or more, [Predict]."

TEST_F(Batch1TriggerConditions, ForgottenLibrary_ACheapSpellDoesNotTrigger) {
    auto library = battlefieldCard(0, kForgottenLibrary);
    vanilla(P1, 0);
    state.battlefields[BattlefieldId::fromRaw(0)].controller = P1;
    stock(P1);
    auto spell = spellPlayed(P1, 3);
    EXPECT_FALSE(fire(library, TriggerType::WhenYouPlayASpell, spell, nullptr, nullptr, P1));
    EXPECT_FALSE(logged("FORGOTTEN LIBRARY: predict"));
}

TEST_F(Batch1TriggerConditions, ForgottenLibrary_NotControllingItDoesNotTrigger) {
    auto library = battlefieldCard(0, kForgottenLibrary);
    vanilla(P2, 0);
    state.battlefields[BattlefieldId::fromRaw(0)].controller = P2;
    stock(P1);
    auto spell = spellPlayed(P1, 5);
    EXPECT_FALSE(fire(library, TriggerType::WhenYouPlayASpell, spell, nullptr, nullptr, P1));
}

TEST_F(Batch1TriggerConditions, ForgottenLibrary_AFourEnergySpellPredicts) {
    auto library = battlefieldCard(0, kForgottenLibrary);
    vanilla(P1, 0);
    state.battlefields[BattlefieldId::fromRaw(0)].controller = P1;
    stock(P1);
    auto spell = spellPlayed(P1, 4);
    EXPECT_TRUE(fire(library, TriggerType::WhenYouPlayASpell, spell, nullptr, nullptr, P1));
    EXPECT_TRUE(logged("FORGOTTEN LIBRARY: predict"));
}

// Protective Sands: "When you conquer here, if you control 4 or fewer runes,
// you may pay [1] to draw 1."

TEST_F(Batch1TriggerConditions, ProtectiveSands_FiveRunesDoesNotTrigger) {
    auto sands = battlefieldCard(0, kProtectiveSands);
    runes(P1, 5);
    stock(P1);
    ps(P1).rune_pool.energy = 1;
    AcceptingEnginePathAgent agent;
    EXPECT_FALSE(fire(sands, TriggerType::WhenYouConquerHere, kInvalidId, &agent, nullptr, P1));
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(handSize(P1), 0);
}

TEST_F(Batch1TriggerConditions, ProtectiveSands_FourRunesOffersTheDraw) {
    auto sands = battlefieldCard(0, kProtectiveSands);
    runes(P1, 4);
    stock(P1);
    ps(P1).rune_pool.energy = 1;
    AcceptingEnginePathAgent agent;
    EXPECT_TRUE(fire(sands, TriggerType::WhenYouConquerHere, kInvalidId, &agent, nullptr, P1));
    EXPECT_EQ(handSize(P1), 1);
}

// Chemtech Cask: "When you play a spell on an opponent's turn, you may exhaust
// me to play a Gold gear token exhausted."

TEST_F(Batch1TriggerConditions, ChemtechCask_OnYourOwnTurnDoesNotTrigger) {
    auto cask = gearOf(P1, kChemtechCask);
    turnOf(P1);
    AcceptingEnginePathAgent agent;
    EXPECT_FALSE(fire(cask, TriggerType::WhenYouPlayASpell, spellPlayed(P1, 1), &agent));
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(goldTokens(P1), 0);
}

TEST_F(Batch1TriggerConditions, ChemtechCask_OnAnOpponentsTurnOffersTheGold) {
    auto cask = gearOf(P1, kChemtechCask);
    turnOf(P2);
    AcceptingEnginePathAgent agent;
    EXPECT_TRUE(fire(cask, TriggerType::WhenYouPlayASpell, spellPlayed(P1, 1), &agent));
    EXPECT_TRUE(o(cask).is_exhausted);
    EXPECT_EQ(goldTokens(P1), 1);
}

// Gutter Palace: "At the start of your Beginning Phase, if you have exactly 4
// cards in hand and exactly 4 units at battlefields, you win the game."

TEST_F(Batch1TriggerConditions, GutterPalace_ThreeCardsInHandDoesNotTrigger) {
    auto palace = gearOf(P1, kGutterPalace);
    for (int i = 0; i < 3; ++i) addToHand(P1, kLoyalPoro);
    for (int i = 0; i < 4; ++i) vanilla(P1, i % 2);
    EXPECT_FALSE(fire(palace, TriggerType::AtStartOfBeginning));
    EXPECT_FALSE(state.game_over);
}

TEST_F(Batch1TriggerConditions, GutterPalace_FourAndFourWins) {
    auto palace = gearOf(P1, kGutterPalace);
    for (int i = 0; i < 4; ++i) addToHand(P1, kLoyalPoro);
    for (int i = 0; i < 4; ++i) vanilla(P1, i % 2);
    EXPECT_TRUE(fire(palace, TriggerType::AtStartOfBeginning));
    EXPECT_TRUE(state.game_over);
    EXPECT_TRUE(state.winner == P1);
}

// Virtuoso: "When you play a spell, if you spent [4] or more, you may banish
// it. Then, if there are four spells banished with me, ..."

TEST_F(Batch1TriggerConditions, Virtuoso_ACheapSpellDoesNotTrigger) {
    auto virtuoso = legendOf(P1, kVirtuoso);
    auto spell = spellPlayed(P1, 3);
    AcceptingEnginePathAgent agent;
    EXPECT_FALSE(fire(virtuoso, TriggerType::WhenYouPlayASpell, spell, &agent));
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(inTrash(P1, spell));
}

TEST_F(Batch1TriggerConditions, Virtuoso_AFourEnergySpellMayBeBanished) {
    auto virtuoso = legendOf(P1, kVirtuoso);
    auto spell = spellPlayed(P1, 4);
    AcceptingEnginePathAgent agent;
    EXPECT_TRUE(fire(virtuoso, TriggerType::WhenYouPlayASpell, spell, &agent));
    EXPECT_FALSE(inTrash(P1, spell));
    EXPECT_TRUE(o(spell).zone == ZoneType::Banishment);
}

// Clockwork Keeper: "When you play me, if you paid the additional cost,
// draw 1."

TEST_F(Batch1TriggerConditions, ClockworkKeeper_UnpaidDoesNotTriggerPaidDraws) {
    auto keeper = addUnit(P1, kClockworkKeeper);
    stock(P1);
    EXPECT_FALSE(fire(keeper, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(handSize(P1), 0);
    o(keeper).card_counters["__clockwork_paid"] = 1;
    EXPECT_TRUE(fire(keeper, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(handSize(P1), 1);
}

// Poro Herder: "When you play me, if you control a Poro, buff me and draw 1."

TEST_F(Batch1TriggerConditions, PoroHerder_WithoutAPoroDoesNotTrigger) {
    auto herder = addUnit(P1, kPoroHerder);
    vanilla(P1);
    stock(P1);
    EXPECT_FALSE(fire(herder, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(handSize(P1), 0);
    EXPECT_EQ(o(herder).buff_count, 0);
}

TEST_F(Batch1TriggerConditions, PoroHerder_WithAPoroBuffsAndDrawsEvenIfThePoroIsGoneByThen) {
    auto herder = addUnit(P1, kPoroHerder);
    auto poro = vanilla(P1);
    o(poro).tags = std::vector<std::string>{"Poro"};
    stock(P1);
    EXPECT_TRUE(fire(herder, TriggerType::WhenYouPlayMe, kInvalidId, nullptr,
                     [&](GameState& s) { s.getObject(poro).tags = std::vector<std::string>{"Cat"}; }));
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(o(herder).buff_count, 1);
}

// Wraith of Echoes: "The first time a friendly unit dies each turn, draw 1."

TEST_F(Batch1TriggerConditions, WraithOfEchoes_OnlyTheFirstDeathOfTheTurnTriggers) {
    auto wraith = addUnit(P1, kWraithOfEchoes);
    auto dead = vanilla(P1);
    stock(P1);
    // The trigger is fired by hand: set the turn's count of friendly deaths
    // the death would have set (testing phase B6: the condition is the turn's
    // first friendly death, PlayerState::unit_deaths_this_turn — it used to be
    // the card's own "performed" mark).
    state.player(P1).unit_deaths_this_turn = 1;
    EXPECT_TRUE(fire(wraith, TriggerType::WhenAFriendlyUnitDies, dead));
    EXPECT_EQ(handSize(P1), 1);
    state.player(P1).unit_deaths_this_turn = 2;
    EXPECT_FALSE(fire(wraith, TriggerType::WhenAFriendlyUnitDies, dead));
    EXPECT_EQ(handSize(P1), 1);
}

// Volibear, Imposing: "When an opponent moves to a battlefield other than
// mine, draw 1."

TEST_F(Batch1TriggerConditions, VolibearImposing_AMoveToMyBattlefieldDoesNotTrigger) {
    auto voli = addUnit(P1, kVolibearImposing, 0, 0);
    auto mover = vanilla(P2, 0);
    stock(P1);
    EXPECT_FALSE(fire(voli, TriggerType::WhenAnOpponentMovesToBattlefield, mover));
    EXPECT_EQ(handSize(P1), 0);
}

TEST_F(Batch1TriggerConditions, VolibearImposing_AMoveToAnotherBattlefieldDraws) {
    auto voli = addUnit(P1, kVolibearImposing, 0, 0);
    auto mover = vanilla(P2, 1);
    stock(P1);
    EXPECT_TRUE(fire(voli, TriggerType::WhenAnOpponentMovesToBattlefield, mover));
    EXPECT_EQ(handSize(P1), 1);
}

// Blast Corps Cadet: "When you play me, if you paid the additional cost, deal
// 2 to a unit at a battlefield."

TEST_F(Batch1TriggerConditions, BlastCorpsCadet_UnpaidDoesNotTriggerPaidDealsTwo) {
    auto cadet = addUnit(P1, kBlastCorpsCadet);
    auto enemy = vanilla(P2, 0, 5);
    EnginePathAgent none;
    EXPECT_FALSE(fire(cadet, TriggerType::WhenYouPlayMe, kInvalidId, &none));
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_EQ(o(enemy).damage_marked, 0);

    o(cadet).card_counters["__blastcorps_paid"] = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    EXPECT_TRUE(fire(cadet, TriggerType::WhenYouPlayMe, kInvalidId, &agent));
    EXPECT_EQ(o(enemy).damage_marked, 2);
}

// Frostcoat Cub: "When you play me, if you paid the additional cost, give a
// unit -2 [M] this turn."

TEST_F(Batch1TriggerConditions, FrostcoatCub_UnpaidDoesNotTriggerPaidShrinksAUnit) {
    auto cub = addUnit(P1, kFrostcoatCub);
    auto enemy = vanilla(P2, 0, 5);
    EXPECT_FALSE(fire(cub, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(o(enemy).temp_might_bonus, 0);

    o(cub).card_counters["__frostcoat_paid"] = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    EXPECT_TRUE(fire(cub, TriggerType::WhenYouPlayMe, kInvalidId, &agent));
    EXPECT_EQ(o(enemy).temp_might_bonus, -2);
}

// Dropboarder: "When you play me, if you control two or more gear, ready me."

TEST_F(Batch1TriggerConditions, Dropboarder_OneGearDoesNotTriggerTwoReadyHim) {
    auto boarder = addUnit(P1, kDropboarder);
    o(boarder).is_exhausted = true;
    gearOf(P1);
    EXPECT_FALSE(fire(boarder, TriggerType::WhenYouPlayMe));
    EXPECT_TRUE(o(boarder).is_exhausted);
    gearOf(P1);
    EXPECT_TRUE(fire(boarder, TriggerType::WhenYouPlayMe));
    EXPECT_FALSE(o(boarder).is_exhausted);
}

// Sea Monkey: "When you play me, if you paid the additional cost, buff me."

TEST_F(Batch1TriggerConditions, SeaMonkey_UnpaidDoesNotTriggerPaidBuffs) {
    auto monkey = addUnit(P1, kSeaMonkey);
    EXPECT_FALSE(fire(monkey, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(o(monkey).buff_count, 0);
    o(monkey).card_counters["__sea_monkey_paid"] = 1;
    EXPECT_TRUE(fire(monkey, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(o(monkey).buff_count, 1);
}

// Akshan, Mischievous: "When you play me, if you paid the additional cost,
// move an enemy gear to your base. You control it until I leave the board."

TEST_F(Batch1TriggerConditions, Akshan_UnpaidTheGearAbilityDoesNotTrigger) {
    auto akshan = addUnit(P1, kAkshanMischievous);
    auto loot = gearOf(P2);
    EnginePathAgent none;
    EXPECT_FALSE(fire(akshan, TriggerType::AsYouPlayMe, kInvalidId, &none));
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_EQ(o(loot).controller, P2);
}

TEST_F(Batch1TriggerConditions, Akshan_PaidTakesAnEnemyGear) {
    auto akshan = addUnit(P1, kAkshanMischievous);
    auto loot = gearOf(P2);
    o(akshan).card_counters["__akshan_paid"] = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(loot));
    EXPECT_TRUE(fire(akshan, TriggerType::AsYouPlayMe, kInvalidId, &agent));
    EXPECT_EQ(o(loot).controller, P1);
}

// Lucian, Merciless: "The first time I conquer each turn, ready me."

TEST_F(Batch1TriggerConditions, LucianMerciless_OnlyTheFirstConquerOfTheTurnTriggers) {
    auto lucian = addUnit(P1, kLucianMerciless, 0, 0);
    o(lucian).is_exhausted = true;
    EXPECT_TRUE(fire(lucian, TriggerType::WhenIConquer));
    EXPECT_FALSE(o(lucian).is_exhausted);
    o(lucian).is_exhausted = true;
    EXPECT_FALSE(fire(lucian, TriggerType::WhenIConquer));
    EXPECT_TRUE(o(lucian).is_exhausted);
}

// Draven, Audacious: "The first time I win a combat each turn, you score 1
// point. When I die in combat, choose an opponent. They score 1 point."

TEST_F(Batch1TriggerConditions, DravenAudacious_OnlyTheFirstWinOfTheTurnTriggers) {
    auto draven = addUnit(P1, kDravenAudacious, 0, 0);
    EXPECT_TRUE(fire(draven, TriggerType::WhenIWinCombat));
    EXPECT_EQ(ps(P1).score, 1);
    EXPECT_FALSE(fire(draven, TriggerType::WhenIWinCombat));
    EXPECT_EQ(ps(P1).score, 1);
}

TEST_F(Batch1TriggerConditions, DravenAudacious_DyingOutsideCombatDoesNotTrigger) {
    auto draven = addUnit(P1, kDravenAudacious, 0, 0);
    o(draven).last_known.valid = true;          // the snapshot of a death outside combat
    o(draven).last_known.in_combat = false;
    o(draven).last_known.combat_designation = CombatDesignation::None;
    o(draven).location = std::nullopt;
    o(draven).zone = ZoneType::Trash;
    ps(P1).trash.push_back(draven);
    EXPECT_FALSE(fire(draven, TriggerType::WhenIDie, draven));
    EXPECT_EQ(ps(P2).score, 0);
}

TEST_F(Batch1TriggerConditions, DravenAudacious_DyingInCombatGivesAnOpponentAPoint) {
    auto draven = addUnit(P1, kDravenAudacious, 0, 0);
    o(draven).last_known.valid = true;
    o(draven).last_known.in_combat = true;
    o(draven).last_known.combat_designation = CombatDesignation::Attacker;
    o(draven).location = std::nullopt;
    o(draven).zone = ZoneType::Trash;
    ps(P1).trash.push_back(draven);
    EXPECT_TRUE(fire(draven, TriggerType::WhenIDie, draven));
    EXPECT_EQ(ps(P2).score, 1);
}

// Revna the Lorekeeper: "When you play a spell, if you spent [4] or more,
// ready me."

TEST_F(Batch1TriggerConditions, Revna_ACheapSpellDoesNotTriggerADearOneReadiesHer) {
    auto revna = addUnit(P1, kRevna, 0, 0);
    o(revna).is_exhausted = true;
    ps(P1).last_spell_energy_spent = 6;   // another, dearer spell: not the one played
    EXPECT_FALSE(fire(revna, TriggerType::WhenYouPlayASpell, spellPlayed(P1, 3)));
    EXPECT_TRUE(o(revna).is_exhausted);
    EXPECT_TRUE(fire(revna, TriggerType::WhenYouPlayASpell, spellPlayed(P1, 4)));
    EXPECT_FALSE(o(revna).is_exhausted);
}

// Yeti Brawler: "When I conquer, if you assigned 3 or more excess damage, play
// two Gold gear tokens exhausted."

TEST_F(Batch1TriggerConditions, YetiBrawler_TwoExcessDoesNotTriggerThreePlaysTwoGold) {
    auto yeti = addUnit(P1, kYetiBrawler, 0, 0);
    ps(P1).last_combat_bf = BattlefieldId::fromRaw(0);
    ps(P1).excess_combat_damage = 2;
    EXPECT_FALSE(fire(yeti, TriggerType::WhenIConquer));
    EXPECT_EQ(goldTokens(P1), 0);
    ps(P1).excess_combat_damage = 3;
    EXPECT_TRUE(fire(yeti, TriggerType::WhenIConquer));
    EXPECT_EQ(goldTokens(P1), 2);
}

// Pyke, Dockside Butcher: "When you play me, if you paid the additional cost,
// ready me and give me +2 [M] this turn."

TEST_F(Batch1TriggerConditions, PykeDocksideButcher_UnpaidDoesNotTriggerPaidReadies) {
    auto pyke = addUnit(P1, kPykeDocksideButcher, 0, 0);
    o(pyke).is_exhausted = true;
    EXPECT_FALSE(fire(pyke, TriggerType::WhenYouPlayMe));
    EXPECT_TRUE(o(pyke).is_exhausted);
    o(pyke).card_counters["__pyke_db_paid"] = 1;
    EXPECT_TRUE(fire(pyke, TriggerType::WhenYouPlayMe));
    EXPECT_FALSE(o(pyke).is_exhausted);
    EXPECT_EQ(o(pyke).temp_might_bonus, 2);
}

// Vex, Mocking: "When you [Stun] an enemy unit at a battlefield, you may move
// me to that battlefield."

TEST_F(Batch1TriggerConditions, VexMocking_StunningAUnitAtABaseDoesNotTrigger) {
    auto vex = addUnit(P1, kVexMocking);
    auto stunned = vanilla(P2, -1);
    AcceptingEnginePathAgent agent;
    EXPECT_FALSE(fire(vex, TriggerType::WhenYouStun, stunned, &agent));
    EXPECT_TRUE(agent.prompts.empty());
}

TEST_F(Batch1TriggerConditions, VexMocking_StunningAUnitAtABattlefieldOffersTheMove) {
    auto vex = addUnit(P1, kVexMocking);
    auto stunned = vanilla(P2, 1);
    AcceptingEnginePathAgent agent;
    EXPECT_TRUE(fire(vex, TriggerType::WhenYouStun, stunned, &agent));
    EXPECT_EQ(bfOf(vex), std::optional<BattlefieldId>{BattlefieldId::fromRaw(1)});
}

// Poppy, Paragon: "When you play me, if an opponent's score is within 3 points
// of the Victory Score, ready me and gain 3 XP."

TEST_F(Batch1TriggerConditions, PoppyParagon_AFarOpponentDoesNotTriggerANearOneDoes) {
    auto poppy = addUnit(P1, kPoppyParagon);
    o(poppy).is_exhausted = true;
    const int victory = state.effectiveVictoryScore();
    setScore(P2, victory - 4);
    EXPECT_FALSE(fire(poppy, TriggerType::WhenYouPlayMe));
    EXPECT_TRUE(o(poppy).is_exhausted);
    EXPECT_EQ(ps(P1).xp, 0);
    setScore(P2, victory - 3);
    EXPECT_TRUE(fire(poppy, TriggerType::WhenYouPlayMe));
    EXPECT_FALSE(o(poppy).is_exhausted);
    EXPECT_EQ(ps(P1).xp, 3);
}

// Eclipse Dragon: "When I move, if you control 4 or fewer runes, draw 1."

TEST_F(Batch1TriggerConditions, EclipseDragon_FiveRunesDoesNotTrigger) {
    auto dragon = addUnit(P1, kEclipseDragon, 0, 0);
    runes(P1, 5);
    stock(P1);
    EXPECT_FALSE(fire(dragon, TriggerType::WhenIMove));
    EXPECT_EQ(handSize(P1), 0);
}

TEST_F(Batch1TriggerConditions, EclipseDragon_FourRunesDrawsEvenIfAFifthArrivesInResponse) {
    auto dragon = addUnit(P1, kEclipseDragon, 0, 0);
    runes(P1, 4);
    auto fifth = addRune(P2, Domain::Fury);   // P1's by the time the ability resolves
    stock(P1);
    EXPECT_TRUE(fire(dragon, TriggerType::WhenIMove, kInvalidId, nullptr,
                     [&](GameState& s) {
                         auto& r = s.getObject(fifth);
                         r.owner = P1;
                         r.controller = P1;
                         r.location = BaseLocation{P1};
                     }));
    EXPECT_EQ(handSize(P1), 1);
}

// Renekton, Rage-Fueled: "When I attack, if you control 4 or fewer runes, deal
// 2 to all enemy units here."

TEST_F(Batch1TriggerConditions, Renekton_FiveRunesDoesNotTriggerFourDealsTwo) {
    auto renekton = addUnit(P1, kRenektonRageFueled, 0, 0);
    auto enemy = vanilla(P2, 0, 6);
    runes(P1, 4);
    auto fifth = addRune(P1, Domain::Fury);
    EXPECT_FALSE(fire(renekton, TriggerType::WhenIAttack));
    EXPECT_EQ(o(enemy).damage_marked, 0);
    o(fifth).controller = P2;
    o(fifth).owner = P2;
    o(fifth).location = BaseLocation{P2};
    EXPECT_TRUE(fire(renekton, TriggerType::WhenIAttack));
    EXPECT_EQ(o(enemy).damage_marked, 2);
}

// Zed, From the Shadows: "When you play me, if you paid the additional cost,
// play a 0 [M] Shadow Clone unit token."

TEST_F(Batch1TriggerConditions, Zed_UnpaidDoesNotTriggerPaidPlaysAShadowClone) {
    auto zed = addUnit(P1, kZedFromTheShadows);
    EXPECT_FALSE(fire(zed, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(unitsNamed(P1, "Shadow Clone"), 0);
    o(zed).card_counters["zed_discard_paid"] = 1;
    EXPECT_TRUE(fire(zed, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(unitsNamed(P1, "Shadow Clone"), 1);
}

// Affectionate Poro: "When a combat that I was in ends, if I haven't been
// dealt damage this turn, draw 1."

TEST_F(Batch1TriggerConditions, AffectionatePoro_DamagedThisTurnDoesNotTrigger) {
    auto poro = addUnit(P1, kAffectionatePoro, 0, 0);
    o(poro).damage_taken_this_turn = 1;
    stock(P1);
    EXPECT_FALSE(fire(poro, TriggerType::WhenACombatIWasInEnds));
    EXPECT_EQ(handSize(P1), 0);
}

TEST_F(Batch1TriggerConditions, AffectionatePoro_UndamagedDrawsEvenIfDamagedInResponse) {
    auto poro = addUnit(P1, kAffectionatePoro, 0, 0);
    stock(P1);
    EXPECT_TRUE(fire(poro, TriggerType::WhenACombatIWasInEnds, kInvalidId, nullptr,
                     [&](GameState& s) { s.getObject(poro).damage_taken_this_turn = 1; }));
    EXPECT_EQ(handSize(P1), 1);
}

// Shen, Scourge of Shadows: "When I hold, if there is exactly one other unit
// you control here, draw 1."

TEST_F(Batch1TriggerConditions, ShenScourge_AloneOrWithTwoOthersDoesNotTrigger) {
    auto shen = addUnit(P1, kShenScourgeOfShadows, 0, 0);
    stock(P1);
    EXPECT_FALSE(fire(shen, TriggerType::WhenIHold)) << "alone";
    vanilla(P1, 0);
    vanilla(P1, 0);
    EXPECT_FALSE(fire(shen, TriggerType::WhenIHold)) << "two others";
    EXPECT_EQ(handSize(P1), 0);
}

TEST_F(Batch1TriggerConditions, ShenScourge_ExactlyOneOtherDraws) {
    auto shen = addUnit(P1, kShenScourgeOfShadows, 0, 0);
    vanilla(P1, 0);
    stock(P1);
    EXPECT_TRUE(fire(shen, TriggerType::WhenIHold));
    EXPECT_EQ(handSize(P1), 1);
}

// Shen, Leader of the Kinkou Order: "When I hold, if there is exactly one
// other unit you control here, you score 1 point."

TEST_F(Batch1TriggerConditions, ShenLeader_AloneDoesNotTrigger) {
    auto shen = addUnit(P1, kShenLeaderOfTheKinkouOrder, 0, 0);
    EXPECT_FALSE(fire(shen, TriggerType::WhenIHold));
    EXPECT_EQ(ps(P1).score, 0);
}

TEST_F(Batch1TriggerConditions, ShenLeader_ExactlyOneOtherScoresEvenIfItLeavesInResponse) {
    auto shen = addUnit(P1, kShenLeaderOfTheKinkouOrder, 0, 0);
    auto other = vanilla(P1, 0);
    EXPECT_TRUE(fire(shen, TriggerType::WhenIHold, kInvalidId, nullptr,
                     [&](GameState& s) { toBase(s, other); }));
    EXPECT_EQ(ps(P1).score, 1);
}

// Patched Porobot: "When you play me, if you control 3 or more other gear,
// draw 1."

TEST_F(Batch1TriggerConditions, PatchedPorobot_TwoGearDoesNotTriggerThreeDraws) {
    auto porobot = addUnit(P1, kPatchedPorobot);
    stock(P1);
    gearOf(P1);
    gearOf(P1);
    EXPECT_FALSE(fire(porobot, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(handSize(P1), 0);
    gearOf(P1);
    EXPECT_TRUE(fire(porobot, TriggerType::WhenYouPlayMe));
    EXPECT_EQ(handSize(P1), 1);
}

// Nasus, Guardian of Knowledge: "Once each turn, when an enemy unit here dies,
// channel 1 rune exhausted."

TEST_F(Batch1TriggerConditions, NasusGuardian_OnceEachTurn) {
    auto nasus = addUnit(P1, kNasusGuardianOfKnowledge, 0, 0);
    auto dead = vanilla(P2, 0);
    EXPECT_TRUE(fire(nasus, TriggerType::WhenAnEnemyUnitDiesHere, dead));
    EXPECT_TRUE(logged("NASUS, GUARDIAN OF KNOWLEDGE: enemy unit died here"));
    log_.clear();
    EXPECT_FALSE(fire(nasus, TriggerType::WhenAnEnemyUnitDiesHere, dead));
    EXPECT_FALSE(logged("NASUS, GUARDIAN OF KNOWLEDGE: enemy unit died here"));
    state.turn.turn_number += 1;
    EXPECT_TRUE(fire(nasus, TriggerType::WhenAnEnemyUnitDiesHere, dead));
}

// Swain, Visionary: "When I conquer, if you've played a non-token unit, a
// non-token gear, and a spell this turn, you score 1 point."

TEST_F(Batch1TriggerConditions, SwainVisionary_WithoutAllThreeDoesNotTrigger) {
    auto swain = addUnit(P1, kSwainVisionary, 0, 0);
    ps(P1).played_nontoken_unit_this_turn = true;
    ps(P1).played_spell_this_turn = true;      // no gear
    EXPECT_FALSE(fire(swain, TriggerType::WhenIConquer));
    EXPECT_EQ(ps(P1).score, 0);
}

TEST_F(Batch1TriggerConditions, SwainVisionary_WithAllThreeScores) {
    auto swain = addUnit(P1, kSwainVisionary, 0, 0);
    ps(P1).played_nontoken_unit_this_turn = true;
    ps(P1).played_nontoken_gear_this_turn = true;
    ps(P1).played_spell_this_turn = true;
    EXPECT_TRUE(fire(swain, TriggerType::WhenIConquer));
    EXPECT_EQ(ps(P1).score, 1);
}

// Gust Monk: "When you play me, if you paid the additional cost, banish a card
// from any trash to give a unit [Assault 2] this turn."

TEST_F(Batch1TriggerConditions, GustMonk_UnpaidDoesNotTriggerPaidBanishesACard) {
    auto monk = addUnit(P1, kGustMonk, 0, 0);
    auto fodder = spellPlayed(P2, 0);          // a card in a trash
    EnginePathAgent none;
    EXPECT_FALSE(fire(monk, TriggerType::WhenYouPlayMe, kInvalidId, &none));
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_TRUE(inTrash(P2, fodder));

    o(monk).card_counters["__gust_monk_paid"] = 1;
    EXPECT_TRUE(fire(monk, TriggerType::WhenYouPlayMe));
    EXPECT_FALSE(inTrash(P2, fodder));
    EXPECT_TRUE(o(fodder).zone == ZoneType::Banishment);
}

// Masa, Crashing Thunder: "When you play me, if you paid the additional cost,
// [Stun] an enemy unit at a battlefield."

TEST_F(Batch1TriggerConditions, Masa_UnpaidDoesNotTriggerPaidStuns) {
    auto masa = addUnit(P1, kMasaCrashingThunder);
    auto enemy = vanilla(P2, 0);
    EnginePathAgent none;
    EXPECT_FALSE(fire(masa, TriggerType::WhenYouPlayMe, kInvalidId, &none));
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_FALSE(o(enemy).is_stunned);

    o(masa).card_counters["__masa_paid"] = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(enemy));
    EXPECT_TRUE(fire(masa, TriggerType::WhenYouPlayMe, kInvalidId, &agent));
    EXPECT_TRUE(o(enemy).is_stunned);
}

}  // namespace batch1_trigger_conditions
}  // namespace riftbound::test
