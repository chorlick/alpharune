/// Testing phase, batch 4a: the card-level findings of plans/10 (Batch 4).
/// One test (or a few) per finding, through the engine path; the finding id
/// is in each test's name. Detail of every finding: docs/testing-findings-
/// decks-2026-10.md (D..) and docs/testing-findings-fuzz-2026-10.md (F.., A..).

#include <gtest/gtest.h>

#include "agents/random_player_agent.h"
#include "card_test_fixture.h"
#include "cards/card_helpers.h"
#include "core/board_state.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test::batch_4a {

constexpr CardDefId kNoxusHopeful = 12;
constexpr CardDefId kFallingStar = 29;
constexpr CardDefId kZhonyasHourglass = 77;
constexpr CardDefId kSpriteCall = 94;
constexpr CardDefId kStupefy = 95;
constexpr CardDefId kWatchfulSentry = 96;
constexpr CardDefId kThousandTailedWatcher = 116;
constexpr CardDefId kShipyardSkulker = 175;
constexpr CardDefId kFadingMemories = 180;
constexpr CardDefId kFortifiedPosition = 274;
constexpr CardDefId kVilemawsLair = 290;
constexpr CardDefId kHeraldOfTheArcane = 298;
constexpr CardDefId kRekSaiBreacher = 352;
constexpr CardDefId kEmperorsDivide = 366;
constexpr CardDefId kRocketBarrage = 400;
constexpr CardDefId kBellowsBreath = 402;
constexpr CardDefId kWindsinger = 459;
constexpr CardDefId kHonestBroker = 476;
constexpr CardDefId kMinefield = 526;
constexpr CardDefId kRockfallPath = 530;
constexpr CardDefId kBrush = 561;
constexpr CardDefId kViHotheaded = 592;
constexpr CardDefId kLeBlancEverywhereAtOnce = 652;
constexpr CardDefId kDeterminedSentry = 673;
constexpr CardDefId kLoyalPoro = 718;
constexpr CardDefId kLeBlancFragmented = 734;
constexpr CardDefId kShardOfUndoing = 736;
constexpr CardDefId kThrillOfTheHunt = 745;
constexpr CardDefId kGreenFather = 753;
constexpr CardDefId kGardensOfBecoming = 769;
constexpr CardDefId kStarSpring = 771;
constexpr CardDefId kPendulumBlade = 798;
constexpr CardDefId kAstralHeron = 831;
constexpr CardDefId kTemporalBreach = 853;
constexpr CardDefId kMelNewlyAwakened = 856;
constexpr CardDefId kMasterOfShadows = 930;

class Batch4aTest : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    PlayerState& ps(PlayerId p) { return state.player(p); }
    static BattlefieldId bfId(int bf) { return BattlefieldId::fromRaw(bf); }
    std::optional<BattlefieldId> bfOf(GameObjectId id) { return o(id).battlefieldId(); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }
    bool inBanishment(PlayerId p, GameObjectId id) {
        const auto& b = ps(p).banishment;
        return o(id).zone == ZoneType::Banishment && std::find(b.begin(), b.end(), id) != b.end();
    }

    /// A battlefield card of `def_id` in slot `bf`, as GameEngine sets one up
    /// (zone BattlefieldZone, the slot's static flags from the card).
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
    GameObjectId gear(PlayerId p, CardDefId def_id) {
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
        g.might_bonus = def.might_bonus;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{p};
        return id;
    }
    GameObjectId attached(CardDefId def_id, GameObjectId unit) {
        auto g = gear(o(unit).controller, def_id);
        o(g).attached_to = unit;
        o(g).location = o(unit).location;
        o(g).zone = o(unit).zone;
        o(g).is_rules_text_inactive = true;
        o(unit).attachments.push_back(g);
        o(unit).attachment_might_bonus += o(g).might_bonus;
        o(unit).recomputeMight();
        return g;
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
    void cleanup() {
        withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    }
    /// Execute `intent` as its player's main-phase decision and settle.
    void run(const Intent& intent, AgentInterface* p1 = nullptr, AgentInterface* p2 = nullptr) {
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(intent); settleLikeTheEngine(e); },
                   p1, p2, intent.player);
    }
    Intent playIntent(GameObjectId card, std::vector<GameObjectId> targets = {}) {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = o(card).controller;
        play.card = card;
        play.targets = std::move(targets);
        return play;
    }
    void fund(PlayerId p, int energy, int power = 0) {
        ps(p).rune_pool.energy += energy;
        ps(p).rune_pool.universal_power += power;
    }
    void fundFor(GameObjectId card) {
        const auto& def = card_db.get(o(card).card_def_id);
        fund(o(card).controller, def.energy_cost, def.power_cost);
    }
    /// Execute `intent` (P1's); the first time P2 gets priority — the item is
    /// on the chain, finalized, not resolved — `probe` runs on the live engine.
    void runAndProbe(const Intent& intent, AgentInterface* p1_agent,
                     const std::function<void(GameEngine&)>& probe) {
        GameEngine* live = nullptr;
        bool probed = false;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (!probed && live) { probed = true; probe(*live); }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            live = &e;
            e.testHook_executeIntent(intent);
            settleLikeTheEngine(e);
            live = nullptr;
        }, p1_agent, &p2, intent.player);
        EXPECT_TRUE(probed) << "the opponent never got priority";
    }
    /// Prefer, in order: the first of `objects` a prompt offers; else the
    /// option with value `value` that names no object; else the first option.
    static EnginePathAgent::Picker prefer(std::vector<GameObjectId> objects, int value = -1) {
        return [objects = std::move(objects), value](const std::vector<Intent>& legal) {
            for (auto want : objects)
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
            if (value >= 0)
                for (const auto& i : legal)
                    if (i.chosen_objects.empty() && i.chosen_value == std::optional<int>{value})
                        return i;
            return legal.front();
        };
    }
    GameObjectId tokenNamed(const std::string& name) {
        for (const auto& [id, obj] : state.objects)
            if (obj.name == name && obj.isToken() && obj.location.has_value()) return id;
        return kInvalidId;
    }
    int cardsNamedOnBoard(const std::string& name) {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.name == name && obj.location.has_value()) ++n;
        return n;
    }
};

// ─── D18  Astral Heron 831 ──────────────────────────────────────────────────
// Erratum (errata/vendetta-errata.txt): "... the next card you play THIS TURN
// costs [2][A][A] less." A discount nobody used ends with the turn.
TEST_F(Batch4aTest, D18_AstralHeron_TheUnusedDiscountEndsWithTheTurn) {
    auto heron = addUnit(P1, kAstralHeron, 0, 0);
    playThroughEngine(addToHand(P1, kLoyalPoro));
    auto discounts = [&]() {
        int n = 0;
        for (const auto& m : ps(P1).cost_modifiers) if (m.source == heron) ++n;
        return n;
    };
    ASSERT_EQ(discounts(), 1) << "the first card of the turn";
    // What the engine does to each player as the next turn starts.
    ps(P1).resetTurnTracking();
    EXPECT_EQ(discounts(), 0) << "\"the next card you play this turn\"";
    EXPECT_NE(card_db.get(kAstralHeron).ability_text.find("the next card you play this turn"),
              std::string::npos) << "the def carries the errata text";
}

// ─── D20  Falling Star 29 ───────────────────────────────────────────────────
// "Deal 3 to a unit. / Deal 3 to a unit." Nothing dies between the two
// instructions: lethal damage is found by the cleanup after the spell (CR
// 321.1, 323.4). Both halves on a 2 [M] unit whose controller has a Zhonya's
// Hourglass: one death, replaced once — the unit is healed and recalled.
TEST_F(Batch4aTest, D20_FallingStar_BothHalvesOnOneUnitAreOneDeath) {
    auto victim = addUnit(P2, kShipyardSkulker, 2, 0);
    auto hourglass = gear(P2, kZhonyasHourglass);
    auto spell = addToHand(P1, kFallingStar);
    EnginePathAgent agent(prefer({victim}));
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(onBoard(victim)) << "the one death was replaced by the Hourglass";
    EXPECT_TRUE(o(victim).isAtBase()) << "recalled";
    EXPECT_EQ(o(victim).damage_marked, 0) << "healed";
    EXPECT_FALSE(onBoard(hourglass)) << "killed instead";
}

TEST_F(Batch4aTest, D20_FallingStar_TwoUnitsBothDie) {
    auto a = addUnit(P2, kShipyardSkulker, 2, 0);
    auto b = addUnit(P2, kShipyardSkulker, 3, 0);
    auto spell = addToHand(P1, kFallingStar);
    int asked = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        const GameObjectId want = asked++ == 0 ? a : b;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(inTrash(P2, a));
    EXPECT_TRUE(inTrash(P2, b));
}

// ─── D45  Bellows Breath 402 ────────────────────────────────────────────────
// "[Repeat] [1][B] / Deal 1 to up to three units at the same location." The
// Repeat execution makes choices of its own (CR 820.2.a): any legal set —
// none, one, two or three units that share a location — whatever the first
// execution chose.
class BellowsBreathTest : public Batch4aTest {
protected:
    struct Prompt {
        std::vector<GameObjectId> objects;
        bool may_stop = false;
    };
    std::vector<Prompt> seen;
    /// Answers the object prompts from `script` (kInvalidId = "no more"),
    /// pays every Repeat, and records what each object prompt offered.
    EnginePathAgent::Picker scripted(std::vector<GameObjectId> script) {
        auto next = std::make_shared<size_t>(0);
        return [this, script = std::move(script), next](const std::vector<Intent>& legal) {
            Prompt p;
            const Intent* stop = nullptr;
            const Intent* yes = nullptr;
            for (const auto& i : legal) {
                if (!i.chosen_objects.empty()) p.objects.push_back(i.chosen_objects.front());
                else if (i.chosen_value == std::optional<int>{0}) stop = &i;
                else if (i.chosen_value == std::optional<int>{1}) yes = &i;
            }
            if (p.objects.empty()) return yes ? *yes : legal.front();   // "pay the Repeat?"
            p.may_stop = stop != nullptr;
            seen.push_back(p);
            const GameObjectId want = *next < script.size() ? script[(*next)++] : kInvalidId;
            if (want != kInvalidId)
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
            if (stop) return *stop;
            ADD_FAILURE() << "the script wanted a unit this prompt does not offer";
            return legal.front();
        };
    }
    static bool offers(const Prompt& p, GameObjectId id) {
        return std::find(p.objects.begin(), p.objects.end(), id) != p.objects.end();
    }
};

TEST_F(BellowsBreathTest, D45_TheRepeatChoosesItsOwnUnitsAtOneLocation) {
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto b = addUnit(P2, kInvalidId, 3, 0);
    auto c = addUnit(P2, kInvalidId, 3, 1);
    auto d = addUnit(P2, kInvalidId, 3);            // in P2's base
    auto spell = addToHand(P1, kBellowsBreath);
    fund(P1, 1 + 1, 1 + 1);                          // the spell and one Repeat
    // First execution: the two at BF 0 (then nothing is left to add there, so
    // no third prompt). The Repeat: the one at BF 1.
    EnginePathAgent agent(scripted({a, b, c, kInvalidId}));
    run(playIntent(spell), &agent);

    EXPECT_EQ(o(a).damage_marked, 1);
    EXPECT_EQ(o(b).damage_marked, 1);
    EXPECT_EQ(o(c).damage_marked, 1) << "the Repeat's own choice";
    EXPECT_EQ(o(d).damage_marked, 0);

    ASSERT_GE(seen.size(), 3u);
    // First execution: any unit; after `a`, only units where `a` is.
    EXPECT_TRUE(offers(seen[0], a) && offers(seen[0], c) && offers(seen[0], d));
    EXPECT_TRUE(seen[0].may_stop) << "\"up to three\" includes none (CR 355.13)";
    EXPECT_TRUE(offers(seen[1], b));
    EXPECT_FALSE(offers(seen[1], c)) << "\"at the same location\"";
    EXPECT_FALSE(offers(seen[1], d));
    // The Repeat starts afresh: every unit again, not "that many again".
    const Prompt& repeat_first = seen[2];
    EXPECT_TRUE(offers(repeat_first, a) && offers(repeat_first, c) && offers(repeat_first, d));
    EXPECT_TRUE(repeat_first.may_stop);
    for (size_t k = 3; k < seen.size(); ++k) {
        EXPECT_FALSE(offers(seen[k], a)) << "the Repeat chose a unit at BF 1";
        EXPECT_FALSE(offers(seen[k], d));
    }
}

TEST_F(BellowsBreathTest, D45_TheRepeatIsAskedWhenTheFirstExecutionChoseNothing) {
    auto a = addUnit(P2, kInvalidId, 3, 0);
    auto spell = addToHand(P1, kBellowsBreath);
    fund(P1, 1 + 1, 1 + 1);
    EnginePathAgent agent(scripted({kInvalidId, a, kInvalidId}));
    run(playIntent(spell), &agent);
    EXPECT_EQ(o(a).damage_marked, 1) << "chosen by the Repeat only";
}

TEST_F(BellowsBreathTest, D45_ThreeAtOneLocationAndNoFourth) {
    std::vector<GameObjectId> u;
    for (int i = 0; i < 4; ++i) u.push_back(addUnit(P2, kInvalidId, 3, 0));
    auto spell = addToHand(P1, kBellowsBreath);
    fundFor(spell);
    EnginePathAgent agent(scripted({u[0], u[1], u[2], u[3]}));
    run(playIntent(spell), &agent);
    EXPECT_EQ(seen.size(), 3u) << "\"up to three\"";
    EXPECT_EQ(o(u[0]).damage_marked + o(u[1]).damage_marked + o(u[2]).damage_marked, 3);
    EXPECT_EQ(o(u[3]).damage_marked, 0);
}

// ─── D47 / D62  where an effect may play a unit ─────────────────────────────
// Rockfall Path 530: "Units can't be played here." A "can't" is not overcome
// by an instruction to play there (CR 054.1; Vendetta FAQ: Temporal Breach on
// a unit in a base that is not its owner's — "it isn't possible to play the
// unit ... the unit will remain in Banishment").
TEST_F(Batch4aTest, D47_TemporalBreach_AUnitAtRockfallPathStaysBanished) {
    battlefieldCard(1, kRockfallPath);
    auto unit = addUnit(P1, kLoyalPoro, 0, 1);
    auto spell = addToHand(P1, kTemporalBreach);
    EnginePathAgent agent(prefer({unit}));
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(onBoard(unit)) << "\"Units can't be played here.\"";
    EXPECT_TRUE(inBanishment(P1, unit));
}

TEST_F(Batch4aTest, D47_TemporalBreach_AUnitInAnotherPlayersBaseStaysBanished) {
    // The FAQ's own case: P1 controls P2's unit and it is in P1's base.
    auto unit = addUnit(P2, kLoyalPoro);
    o(unit).controller = P1;
    o(unit).location = BaseLocation{P1};
    auto spell = addToHand(P1, kTemporalBreach);
    EnginePathAgent agent(prefer({unit}));
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(onBoard(unit)) << "its owner cannot play it to a base that is not theirs";
    EXPECT_TRUE(inBanishment(P2, unit));
}

TEST_F(Batch4aTest, D47_TemporalBreach_ElsewhereTheUnitComesBackWhereItWas) {
    auto unit = addUnit(P2, kLoyalPoro, 0, 0);
    o(unit).damage_marked = 1;
    auto spell = addToHand(P1, kTemporalBreach);
    EnginePathAgent agent(prefer({unit}));
    playThroughEngine(spell, &agent);
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{bfId(0)});
    EXPECT_EQ(o(unit).controller, P2);
    EXPECT_EQ(o(unit).damage_marked, 0) << "a new object";
}

TEST_F(Batch4aTest, D47_PlayIgnoringCost_RefusesALocationUnitsCannotBePlayedTo) {
    battlefieldCard(1, kRockfallPath);
    auto unit = addToHand(P1, kLoyalPoro);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.playIgnoringCost(P1, unit, LocationId{BattlefieldLocation{bfId(1)}});
    });
    EXPECT_TRUE(inHand(P1, unit)) << "not played to Rockfall Path: it stays where it was";
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.playIgnoringCost(P1, unit, LocationId{BaseLocation{P2}});
    });
    EXPECT_TRUE(inHand(P1, unit)) << "not played to another player's base";
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.playIgnoringCost(P1, unit, LocationId{BattlefieldLocation{bfId(0)}});
    });
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{bfId(0)});
}

TEST_F(Batch4aTest, D62_ThrillOfTheHunt_RockfallPathIsNotOffered) {
    battlefieldCard(1, kRockfallPath);
    auto unit = addUnit(P1, kLoyalPoro);
    auto spell = addToHand(P1, kThrillOfTheHunt);
    // Would take "Rockfall Path" (mode 1) if it were offered.
    std::vector<std::string> labels;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal) labels.push_back(i.choice_label);
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == unit) return i;
        for (const auto& i : legal)
            if (i.chosen_objects.empty() && i.chosen_value == std::optional<int>{1} &&
                i.choice_label.find("Rockfall") != std::string::npos) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    for (const auto& l : labels)
        EXPECT_EQ(l.find("Rockfall Path"), std::string::npos) << "offered: " << l;
    EXPECT_EQ(bfOf(unit), std::optional<BattlefieldId>{bfId(0)}) << "the only battlefield it can be played to";
}

TEST_F(Batch4aTest, D62_ThrillOfTheHunt_WithNoBattlefieldToPlayToTheUnitStaysBanished) {
    battlefieldCard(0, kRockfallPath);
    battlefieldCard(1, kRockfallPath);
    auto unit = addUnit(P1, kLoyalPoro);
    auto spell = addToHand(P1, kThrillOfTheHunt);
    EnginePathAgent agent(prefer({unit}));
    playThroughEngine(spell, &agent);
    EXPECT_FALSE(onBoard(unit));
    EXPECT_TRUE(inBanishment(P1, unit)) << "\"to any battlefield\": not to its base";
}

// ─── D52 / F9  Star Spring 771 ──────────────────────────────────────────────
// "The first time a player plays a non-token unit here each turn, they may
// move another unit they control here to its base."
TEST_F(Batch4aTest, D52_StarSpring_TheSameCardPlayedHereAgainDoesNotTriggerAgain) {
    battlefieldCard(0, kStarSpring);
    addUnit(P1, kShipyardSkulker, 0, 0);                       // "another unit ... here"
    auto card = addToHand(P1, kShipyardSkulker);
    EnginePathAgent first;
    playThroughEngine(card, &first, LocationId{BattlefieldLocation{bfId(0)}});
    ASSERT_EQ(first.prompts.size(), 1u) << "the first time: \"may move another unit\"";
    // Returned to hand (Gust) and played here again: the same object id.
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.bounceToHand(card); });
    ASSERT_TRUE(inHand(P1, card));
    EnginePathAgent second;
    playThroughEngine(card, &second, LocationId{BattlefieldLocation{bfId(0)}});
    EXPECT_TRUE(second.prompts.empty()) << "not the first time this turn";
}

TEST_F(Batch4aTest, F9_StarSpring_TwoUnitsPlayedHereByOneEffectTriggerItOnce) {
    battlefieldCard(0, kStarSpring);
    addUnit(P1, kShipyardSkulker, 0, 0);
    auto a = addToHand(P1, kShipyardSkulker);
    auto b = addToHand(P1, kShipyardSkulker);
    EnginePathAgent agent;
    const LocationId here{BattlefieldLocation{bfId(0)}};
    // Both plays happen inside one resolution (Thrill of the Hunt with its
    // Repeat), so both are announced before either ability is finalized.
    EXPECT_NO_THROW(resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.playIgnoringCost(P1, a, here);
        exec.playIgnoringCost(P1, b, here);
    }, &agent));
    EXPECT_EQ(agent.prompts.size(), 1u) << "one \"first time\", one may";
}

TEST_F(Batch4aTest, D52_StarSpring_NextTurnIsAFirstTimeAgain) {
    battlefieldCard(0, kStarSpring);
    addUnit(P1, kShipyardSkulker, 0, 0);
    EnginePathAgent first;
    playThroughEngine(addToHand(P1, kShipyardSkulker), &first,
                      LocationId{BattlefieldLocation{bfId(0)}});
    ASSERT_EQ(first.prompts.size(), 1u);
    state.turn.turn_number += 2;
    ps(P1).resetTurnTracking();
    EnginePathAgent next_turn;
    playThroughEngine(addToHand(P1, kShipyardSkulker), &next_turn,
                      LocationId{BattlefieldLocation{bfId(0)}});
    EXPECT_EQ(next_turn.prompts.size(), 1u);
}

// ─── D55 (second half)  LeBlanc, Everywhere at Once 652 ─────────────────────
// "Your [Temporary] effects at my battlefield don't trigger." Only the
// [Temporary] ability is stopped: a [Temporary] unit that dies there some
// other way still has its [Deathknell].
TEST_F(Batch4aTest, D55_LeBlanc_ATemporaryUnitsDeathknellStillTriggersAtHerBattlefield) {
    addUnit(P1, kLeBlancEverywhereAtOnce, 0, 0);
    auto sentry = addUnit(P1, kWatchfulSentry, 0, 0);       // "[Deathknell] — Draw 1."
    o(sentry).keywords.set(Keyword::Temporary);
    o(sentry).granted_keywords.set(Keyword::Temporary);
    addToDeck(P1, kLoyalPoro);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(sentry); });
    EXPECT_EQ(handSize(P1), 1) << "its [Deathknell] is not a [Temporary] effect";
}

// ─── D56  "your Beginning Phase" ────────────────────────────────────────────
// CR 315.1 / 315.2: the Awaken Phase is not part of the Beginning Phase (the
// Beginning Step and the Scoring Step).
class BeginningPhaseTest : public Batch4aTest {
protected:
    void phase(TurnPhase ph, PlayerId turn_player = P1) {
        state.turn.phase = ph;
        state.turn.turn_player = turn_player;
    }
    void kill(GameObjectId unit, AgentInterface* p2 = nullptr) {
        resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); },
                             nullptr, p2);
    }
};

TEST_F(BeginningPhaseTest, D56_LeBlancFragmented_DrawsOneInTheAwakenPhase) {
    auto leblanc = addUnit(P1, kLeBlancFragmented);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kLoyalPoro);
    phase(TurnPhase::AwakenPhase);
    kill(leblanc);
    EXPECT_EQ(handSize(P1), 1) << "the Awaken Phase is not her controller's Beginning Phase";
}

TEST_F(BeginningPhaseTest, D56_LeBlancFragmented_DrawsTwoInTheBeginningAndScoringSteps) {
    for (TurnPhase ph : {TurnPhase::BeginningStep, TurnPhase::ScoringStep}) {
        ps(P1).hand.clear();
        auto leblanc = addUnit(P1, kLeBlancFragmented);
        for (int i = 0; i < 4; ++i) addToDeck(P1, kLoyalPoro);
        phase(ph);
        kill(leblanc);
        EXPECT_EQ(handSize(P1), 2);
    }
    // The opponent's Beginning Phase is not hers.
    ps(P1).hand.clear();
    auto leblanc = addUnit(P1, kLeBlancFragmented);
    phase(TurnPhase::BeginningStep, P2);
    kill(leblanc);
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(BeginningPhaseTest, D56_ShardOfUndoingAndTheDiedInBeginningFlagIgnoreTheAwakenPhase) {
    gear(P1, kShardOfUndoing);
    auto mine = addUnit(P1, kShipyardSkulker);
    auto theirs = addUnit(P2, kShipyardSkulker);
    phase(TurnPhase::AwakenPhase);
    kill(mine);
    EXPECT_TRUE(onBoard(theirs)) << "\"during your Beginning Phase\"";
    EXPECT_FALSE(ps(P1).unit_died_in_beginning_this_turn);

    auto mine2 = addUnit(P1, kShipyardSkulker);
    phase(TurnPhase::BeginningStep);
    kill(mine2);
    EXPECT_FALSE(onBoard(theirs)) << "each opponent must kill one of their units";
    EXPECT_TRUE(ps(P1).unit_died_in_beginning_this_turn);
}

// ─── D59  Rek'Sai, Breacher 352 ─────────────────────────────────────────────
// "Friendly units played from anywhere other than a player's hand have
// [Accelerate]." The grant belongs to that one play: a later play of the same
// card from hand, with no Rek'Sai on the board, is not offered Accelerate.
TEST_F(Batch4aTest, D59_RekSaiBreacher_TheGrantDoesNotOutliveThePlayItWasMadeFor) {
    auto reksai = addUnit(P1, kRekSaiBreacher);
    auto broker = addToHand(P1, kHonestBroker);
    // In banishment, then played by an effect that pays its cost (Void Burrower).
    ps(P1).hand.clear();
    o(broker).zone = ZoneType::Banishment;
    ps(P1).banishment.push_back(broker);
    fund(P1, 2);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        EXPECT_TRUE(exec.playPayingCost(P1, broker, LocationId{BaseLocation{P1}}));
    });
    ASSERT_TRUE(onBoard(broker));
    EXPECT_EQ(o(broker).card_counters.count("__granted_accelerate"), 0u)
        << "nothing of that play is left on the card";

    // Back in hand, Rek'Sai gone, enough for the unit and for [1][Order].
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.bounceToHand(broker);
        exec.killObject(reksai);
    });
    ASSERT_TRUE(inHand(P1, broker));
    fund(P1, 2 + 1, 1);
    EnginePathAgent agent;
    playThroughEngine(broker, &agent, std::nullopt, nullptr, /*fund=*/false);
    EXPECT_TRUE(agent.prompts.empty()) << "no Accelerate offer: Honest Broker has no [Accelerate]";
    EXPECT_TRUE(o(broker).is_exhausted);
}

// ─── D65  Mel, Newly Awakened 856 ───────────────────────────────────────────
// "[Empowered][>] ... If a spell or ability you control would give -[M] to a
// unit it chooses, it gives an additional -1 [M]." Only to a unit the spell or
// ability chose.
TEST_F(Batch4aTest, D65_Mel_NoExtraMinusMightForUnitsThatWereNotChosen) {
    auto mel = addUnit(P1, kMelNewlyAwakened);
    o(mel).empowered = 1;
    auto enemy = addUnit(P2, kInvalidId, 6, 0);
    cleanup();
    // "When you play me, give enemy units -3 [M] this turn": nothing chosen.
    auto watcher = addToHand(P1, kThousandTailedWatcher);
    playThroughEngine(watcher);
    EXPECT_EQ(o(enemy).current_might, 3) << "-3, with no additional -1";
}

TEST_F(Batch4aTest, D65_Mel_AChosenUnitGetsTheAdditionalMinusOne) {
    auto mel = addUnit(P1, kMelNewlyAwakened);
    o(mel).empowered = 1;
    auto enemy = addUnit(P2, kInvalidId, 6, 0);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kLoyalPoro);
    cleanup();
    auto stupefy = addToHand(P1, kStupefy);          // "Give a unit -1 [M] this turn"
    fundFor(stupefy);
    EnginePathAgent agent(prefer({enemy}));
    run(playIntent(stupefy, {enemy}), &agent);
    EXPECT_EQ(o(enemy).current_might, 4) << "-1 and an additional -1";
}

// ─── D66  Herald of the Arcane 298 ──────────────────────────────────────────
// "[1], [E]: Play a 1 [M] Recruit unit token." No location is printed: the
// base or a battlefield its controller controls (CR 355.2.a; a token is
// played, CR 350.2).
TEST_F(Batch4aTest, D66_HeraldOfTheArcane_TheRecruitsLocationIsChosen) {
    auto herald = legendCard(P1, kHeraldOfTheArcane);
    state.battlefields[0].controller = P1;
    addUnit(P1, kShipyardSkulker, 0, 0);
    fund(P1, 1);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = herald;
    // Takes the battlefield (value 1) when a location is asked.
    EnginePathAgent agent(prefer({}, 1));
    run(act, &agent);
    ASSERT_FALSE(agent.prompts.empty()) << "where the Recruit is played";
    const GameObjectId recruit = tokenNamed("Recruit");
    ASSERT_NE(recruit, kInvalidId);
    EXPECT_EQ(bfOf(recruit), std::optional<BattlefieldId>{bfId(0)});
    EXPECT_TRUE(o(herald).is_exhausted);
}

TEST_F(Batch4aTest, D66_HeraldOfTheArcane_WithNoBattlefieldControlledItGoesToTheBaseUnasked) {
    auto herald = legendCard(P1, kHeraldOfTheArcane);
    fund(P1, 1);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = herald;
    EnginePathAgent agent;
    run(act, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    const GameObjectId recruit = tokenNamed("Recruit");
    ASSERT_NE(recruit, kInvalidId);
    EXPECT_TRUE(o(recruit).isAtBase());
}

// ─── D69 / D70  a hidden card's choices are bound to its battlefield ────────
// CR 811.1.d.5: a unit a hidden spell plays is played at that battlefield.
TEST_F(Batch4aTest, D69_SpriteCall_FromFacedownTheSpriteIsPlayedAtItsBattlefield) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    addUnit(P1, kShipyardSkulker, 0, 0);
    addUnit(P1, kShipyardSkulker, 0, 1);
    auto spell = addToHand(P1, kSpriteCall);
    hideAt(spell, 1);
    EnginePathAgent agent;                      // would take the first option: the base
    run(playIntent(spell), &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "nothing to choose: the battlefield it was hidden at";
    const GameObjectId sprite = tokenNamed("Sprite");
    ASSERT_NE(sprite, kInvalidId);
    EXPECT_EQ(bfOf(sprite), std::optional<BattlefieldId>{bfId(1)});
}

TEST_F(Batch4aTest, D69_SpriteCall_FromHandTheLocationIsChosen) {
    state.battlefields[0].controller = P1;
    addUnit(P1, kShipyardSkulker, 0, 0);
    auto spell = addToHand(P1, kSpriteCall);
    EnginePathAgent agent(prefer({}, 1));
    playThroughEngine(spell, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u);
    EXPECT_EQ(agent.prompts[0].size(), 2u) << "base or the battlefield controlled";
    const GameObjectId sprite = tokenNamed("Sprite");
    ASSERT_NE(sprite, kInvalidId);
    EXPECT_EQ(bfOf(sprite), std::optional<BattlefieldId>{bfId(0)});
}

TEST_F(Batch4aTest, D69_SpriteCall_HiddenWhereUnitsCannotBePlayedPlaysNoSprite) {
    state.turn.turn_number = 3;
    battlefieldCard(1, kRockfallPath);
    addUnit(P1, kShipyardSkulker, 0, 1);
    auto spell = addToHand(P1, kSpriteCall);
    hideAt(spell, 1);
    run(playIntent(spell));
    EXPECT_EQ(tokenNamed("Sprite"), kInvalidId)
        << "must be played there (CR 811.1.d.5), can't be played there (Rockfall Path)";
}

// CR 811.1.d.3: a play effect of a hidden permanent chooses its targets among
// the options at the battlefield it was played to.
TEST_F(Batch4aTest, D70_Windsinger_FromFacedownOnlyUnitsAtItsBattlefieldAreOffered) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    auto here = addUnit(P1, kShipyardSkulker, 2, 0);
    auto elsewhere = addUnit(P2, kShipyardSkulker, 2, 1);
    auto windsinger = addToHand(P1, kWindsinger);
    hideAt(windsinger, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(here, EnginePathAgent::May::Yes));
    run(playIntent(windsinger), &agent);
    EXPECT_TRUE(agent.wasOffered(here));
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "at another battlefield";
    EXPECT_TRUE(inHand(P1, here));
    EXPECT_TRUE(onBoard(elsewhere));
}

TEST_F(Batch4aTest, D70_Windsinger_FromFacedownWithNothingAtItsBattlefieldTheMayIsNotAsked) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    addUnit(P1, kShipyardSkulker, 6, 0);                    // here, but more than 3 [M]
    auto elsewhere = addUnit(P2, kShipyardSkulker, 2, 1);
    auto windsinger = addToHand(P1, kWindsinger);
    hideAt(windsinger, 0);
    EnginePathAgent agent(EnginePathAgent::accepting());
    run(playIntent(windsinger), &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_TRUE(onBoard(elsewhere));
}

TEST_F(Batch4aTest, D70_Windsinger_FromHandAnyBattlefieldsUnitMayBeChosen) {
    auto elsewhere = addUnit(P2, kShipyardSkulker, 2, 1);
    auto windsinger = addToHand(P1, kWindsinger);
    EnginePathAgent agent(EnginePathAgent::choosing(elsewhere, EnginePathAgent::May::Yes));
    playThroughEngine(windsinger, &agent);
    EXPECT_TRUE(inHand(P2, elsewhere));
}

// ─── D73  Fortified Position 274 ────────────────────────────────────────────
// "When you defend here, choose a unit. It gains [Shield 2] this combat."
TEST_F(Batch4aTest, D73_FortifiedPosition_TheShieldEndsWithTheCombat) {
    auto fort = battlefieldCard(0, kFortifiedPosition);
    auto defender = addUnit(P1, kShipyardSkulker, 3, 0);
    auto attacker = addUnit(P2, kShipyardSkulker, 3, 0);
    auto startCombat = [&]() {
        auto& bf = state.battlefields[0];
        bf.combat_in_progress = true;
        bf.attacker = P2;
        bf.defender = P1;
        o(defender).combat_designation = CombatDesignation::Defender;
        o(attacker).combat_designation = CombatDesignation::Attacker;
    };
    auto endCombat = [&]() {
        auto& bf = state.battlefields[0];
        bf.combat_in_progress = false;
        bf.attacker = std::nullopt;
        bf.defender = std::nullopt;
        o(defender).combat_designation = CombatDesignation::None;
        o(attacker).combat_designation = CombatDesignation::None;
    };
    startCombat();
    EnginePathAgent agent(EnginePathAgent::choosing(defender));
    fireTriggerThroughEngine(fort, TriggerType::WhenYouDefendHere, &agent);
    EXPECT_TRUE(o(defender).hasKeyword(Keyword::Shield));
    EXPECT_EQ(o(defender).current_might, 5) << "[Shield 2] while it is a defender";

    endCombat();
    cleanup();
    EXPECT_FALSE(o(defender).hasKeyword(Keyword::Shield)) << "\"this combat\"";
    EXPECT_EQ(o(defender).current_might, 3);

    // A second combat here the same turn: the first combat's Shield is gone.
    startCombat();
    cleanup();
    EXPECT_EQ(o(defender).current_might, 3);
    EXPECT_FALSE(o(defender).hasKeyword(Keyword::Shield));
}

// ─── D75  Master of Shadows 930 ─────────────────────────────────────────────
// "When you banish a card you own, empower me." CR 412.4: an ability that
// triggers when "you" do something triggers on a game action you are
// responsible for; the player who performs it is (412.1).
TEST_F(Batch4aTest, D75_MasterOfShadows_AnOpponentsBanishDoesNotEmpowerIt) {
    auto master = legendCard(P2, kMasterOfShadows);
    auto theirs = addUnit(P2, kLoyalPoro, 0, 0);
    auto breach = addToHand(P1, kTemporalBreach);    // P1 banishes P2's unit
    EnginePathAgent agent(prefer({theirs}));
    playThroughEngine(breach, &agent);
    EXPECT_EQ(o(master).empowered, 0) << "P1 performed the banish";
}

TEST_F(Batch4aTest, D75_MasterOfShadows_ItsControllersOwnBanishEmpowersIt) {
    auto master = legendCard(P2, kMasterOfShadows);
    auto theirs = addUnit(P2, kLoyalPoro, 0, 0);
    auto breach = addToHand(P2, kTemporalBreach);
    EnginePathAgent agent(prefer({theirs}));
    playThroughEngine(breach, nullptr, std::nullopt, &agent);
    EXPECT_EQ(o(master).empowered, 1);
}

// ─── D81  Minefield 526 ─────────────────────────────────────────────────────
// "When you conquer here, put the top 2 cards of your Main Deck into your
// trash." Moving cards from the top of the Main Deck to the trash is burning
// (CR 441.1); with fewer cards than that the player burns out and completes
// it (CR 432.1.b, 441.4).
TEST_F(Batch4aTest, D81_Minefield_BurnsOutWhenTheDeckHasFewerThanTwoCards) {
    auto minefield = battlefieldCard(0, kMinefield);
    addUnit(P1, kShipyardSkulker, 0, 0);
    addToDeck(P1, kLoyalPoro);                         // one card left
    for (int i = 0; i < 3; ++i) {
        auto c = addToHand(P1, kLoyalPoro);
        ps(P1).hand.pop_back();
        o(c).zone = ZoneType::Trash;
        ps(P1).trash.push_back(c);
    }
    fireTriggerThroughEngine(minefield, TriggerType::WhenYouConquerHere);
    EXPECT_EQ(ps(P2).score, 1) << "Burn Out: an opponent gains 1 point (CR 432.2.c)";
    EXPECT_EQ(deckSize(P1) + trashSize(P1), 4);
    EXPECT_EQ(trashSize(P1), 1) << "the second card, moved after the Burn Out";
    EXPECT_EQ(ps(P1).cards_burned_this_turn, 2);
}

TEST_F(Batch4aTest, D81_Minefield_MovesTwoCardsOtherwise) {
    auto minefield = battlefieldCard(0, kMinefield);
    addUnit(P1, kShipyardSkulker, 0, 0);
    for (int i = 0; i < 5; ++i) addToDeck(P1, kLoyalPoro);
    fireTriggerThroughEngine(minefield, TriggerType::WhenYouConquerHere);
    EXPECT_EQ(deckSize(P1), 3);
    EXPECT_EQ(trashSize(P1), 2);
    EXPECT_EQ(ps(P2).score, 0);
}

// ─── F3  board state on an object that has left the board ───────────────────
TEST_F(Batch4aTest, F3_ViHotheaded_HerAbilityDoesNothingOnceSheHasLeft) {
    auto vi = addUnit(P1, kViHotheaded);
    fund(P1, 2, 1);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = vi;
    runAndProbe(act, nullptr, [&](GameEngine& e) { e.testHook_executor().killObject(vi); });
    ASSERT_FALSE(onBoard(vi));
    EXPECT_EQ(o(vi).temp_might_bonus, 0);
    EXPECT_FALSE(carriesBoardState(o(vi)));
}

TEST_F(Batch4aTest, F3_PendulumBlade_NoMightForABearerThatHasLeft) {
    auto bearer = addUnit(P1, kShipyardSkulker);
    attached(kPendulumBlade, bearer);
    GameEngine* live = nullptr;
    bool killed = false;
    PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
        if (!killed && live) { killed = true; live->testHook_executor().killObject(bearer); }
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        live = &e;
        e.testHook_executor().moveToBattlefield(bearer, bfId(0));   // "When I move to a battlefield"
        settleLikeTheEngine(e);
        live = nullptr;
    }, nullptr, &p2);
    ASSERT_TRUE(killed) << "the ability went on the chain and the opponent got priority";
    ASSERT_FALSE(onBoard(bearer));
    EXPECT_EQ(o(bearer).temp_might_bonus, 0);
    EXPECT_FALSE(carriesBoardState(o(bearer)));
}

TEST_F(Batch4aTest, F3_FadingMemories_ATargetThatHasLeftGainsNothing) {
    auto target = addUnit(P2, kShipyardSkulker, 0, 0);
    auto spell = addToHand(P1, kFadingMemories);
    fundFor(spell);
    EnginePathAgent agent(prefer({target}));
    runAndProbe(playIntent(spell), &agent,
                [&](GameEngine& e) { e.testHook_executor().bounceToHand(target); });
    ASSERT_TRUE(inHand(P2, target));
    EXPECT_FALSE(o(target).keywords.has(Keyword::Temporary));
    EXPECT_FALSE(carriesBoardState(o(target)));
}

TEST_F(Batch4aTest, F3_FadingMemories_AUnitStillAtABattlefieldGainsTemporary) {
    auto target = addUnit(P2, kShipyardSkulker, 0, 0);
    auto spell = addToHand(P1, kFadingMemories);
    EnginePathAgent agent(prefer({target}));
    playThroughEngine(spell, &agent);
    EXPECT_TRUE(o(target).hasKeyword(Keyword::Temporary));
}

TEST_F(Batch4aTest, F3_GreenFather_TheReplacedBattlefieldLeavesAsANewObject) {
    auto father = legendCard(P1, kGreenFather);
    auto gardens = battlefieldCard(0, kGardensOfBecoming);
    addUnit(P1, kShipyardSkulker, 0, 0);
    o(gardens).is_exhausted = true;                    // board state of its own
    o(gardens).temp_might_bonus = 2;
    AcceptingEnginePathAgent yes;
    fireTriggerThroughEngine(father, TriggerType::WhenIConquerOrHold, &yes, gardens);
    ASSERT_EQ(o(gardens).zone, ZoneType::Banishment) << "replaced (CR 438)";
    EXPECT_FALSE(carriesBoardState(o(gardens)));
    const GameObjectId brush = state.battlefields[0].card_object_id;
    ASSERT_NE(brush, gardens);
    EXPECT_EQ(o(brush).name, "Brush");

    // Swapped back when scored: the Brush token leaves the same way.
    o(brush).is_exhausted = true;
    o(brush).controller = P1;
    fireTriggerThroughEngine(brush, TriggerType::WhenYouScoreHere, &yes);
    EXPECT_EQ(state.battlefields[0].card_object_id, gardens);
    EXPECT_EQ(o(gardens).zone, ZoneType::BattlefieldZone);
    EXPECT_FALSE(inBanishment(P1, gardens));
    EXPECT_FALSE(carriesBoardState(o(brush)));
}

TEST_F(Batch4aTest, F3_GreenFather_ABrushThatReplacedRockfallPathTakesUnits) {
    // The slot's "Units can't be played here." is Rockfall Path's text, not
    // the slot's: Brush has none, and the text is back when it is swapped back.
    auto father = legendCard(P1, kGreenFather);
    auto rockfall = battlefieldCard(0, kRockfallPath);
    addUnit(P1, kShipyardSkulker, 0, 0);
    ASSERT_TRUE(state.battlefields[0].blocks_unit_play);
    AcceptingEnginePathAgent yes;
    fireTriggerThroughEngine(father, TriggerType::WhenIConquerOrHold, &yes, rockfall);
    ASSERT_NE(state.battlefields[0].card_object_id, rockfall);
    EXPECT_FALSE(state.battlefields[0].blocks_unit_play);
    const GameObjectId brush = state.battlefields[0].card_object_id;
    o(brush).controller = P1;
    fireTriggerThroughEngine(brush, TriggerType::WhenYouScoreHere, &yes);
    ASSERT_EQ(state.battlefields[0].card_object_id, rockfall);
    EXPECT_TRUE(state.battlefields[0].blocks_unit_play);
}

TEST_F(Batch4aTest, F3_Executor_BoardStateIsNotWrittenToAnObjectThatIsNotInPlay) {
    auto card = addToHand(P1, kShipyardSkulker);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) {
        exec.giveTemporaryMight(card, 2);
        exec.giveTemporaryKeyword(card, Keyword::Shield, 2);
        exec.exhaustObject(card);
        exec.takeControl(card, P2, /*until_end_of_turn=*/true);
    });
    EXPECT_FALSE(carriesBoardState(o(card)));
    EXPECT_EQ(o(card).controller, P1);
    EXPECT_EQ(o(card).current_might, 3);
}

// ─── F4  Emperor's Divide 366 ───────────────────────────────────────────────
// "Move any number of friendly units at a battlefield to their base." A unit
// that can't move to base is not one of them.
TEST_F(Batch4aTest, F4_EmperorsDivide_AUnitThatCannotMoveToBaseIsNotOffered) {
    auto sentry = addUnit(P1, kDeterminedSentry, 0, 0);     // "I can't move to base."
    o(sentry).cant_move_to_base = true;                     // set by the card as it is played
    auto other = addUnit(P1, kShipyardSkulker, 0, 0);
    cleanup();
    auto spell = addToHand(P1, kEmperorsDivide);
    fundFor(spell);
    // Never says "move no more units" while a unit is on offer.
    int prompts = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        ++prompts;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty()) return i;
        return legal.front();
    });
    EXPECT_NO_THROW(run(playIntent(spell, {other}), &agent));
    EXPECT_FALSE(agent.wasOffered(sentry));
    EXPECT_TRUE(o(other).isAtBase());
    EXPECT_EQ(bfOf(sentry), std::optional<BattlefieldId>{bfId(0)});
    EXPECT_LE(prompts, 2);
}

TEST_F(Batch4aTest, F4_EmperorsDivide_AtVilemawsLairNothingIsOffered) {
    battlefieldCard(0, kVilemawsLair);                      // "Units can't move from here to base."
    auto a = addUnit(P1, kShipyardSkulker, 0, 0);
    auto spell = addToHand(P1, kEmperorsDivide);
    fundFor(spell);
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty()) return i;
        return legal.front();
    });
    EXPECT_NO_THROW(run(playIntent(spell, {a}), &agent));
    EXPECT_FALSE(agent.wasOffered(a));
    EXPECT_EQ(bfOf(a), std::optional<BattlefieldId>{bfId(0)});
}

// ─── A1  random-player: a decline is told from a first mode ─────────────────
TEST_F(Batch4aTest, A1_RandomPlayer_ForcedPlayTakesTheFirstOfTwoModes) {
    // Rocket Barrage: "Choose one — Deal 4 to a unit in a base. / Kill a
    // gear." Two modes, values 0 and 1: neither is a pass.
    auto unit = addUnit(P2, kInvalidId, 9);
    gear(P2, kZhonyasHourglass);
    const GameState base = state;
    int first_mode = 0, second_mode = 0;
    for (uint64_t seed = 1; seed <= 40; ++seed) {
        state = base;
        auto spell = addToHand(P1, kRocketBarrage);
        RandomPlayerAgent forced(seed, /*pass_probability=*/0.0);
        playThroughEngine(spell, &forced);
        (o(unit).damage_marked > 0 ? first_mode : second_mode)++;
    }
    EXPECT_GT(first_mode, 0) << "mode 0 is a choice, not a decline";
    EXPECT_GT(second_mode, 0);
}

TEST_F(Batch4aTest, A1_RandomPlayer_ForcedPlayNeverDeclinesAMay) {
    // Windsinger: "you may return another unit ..." — "No" is the pass.
    auto small = addUnit(P2, kShipyardSkulker, 2, 0);
    const GameState base = state;
    for (uint64_t seed = 1; seed <= 20; ++seed) {
        state = base;
        auto windsinger = addToHand(P1, kWindsinger);
        RandomPlayerAgent forced(seed, /*pass_probability=*/0.0);
        playThroughEngine(windsinger, &forced);
        EXPECT_TRUE(inHand(P2, small)) << "seed " << seed;
    }
}

}  // namespace riftbound::test::batch_4a
