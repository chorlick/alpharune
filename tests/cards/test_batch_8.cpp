/// Testing phase, batch 8 (plans/remaining/10-testing-phase.md items U1-U21
/// found by round 2, V1-G4, S1 and the stopping rule's coverage; report
/// plans/reports/batch-8.md).

#include <gtest/gtest.h>

#include "batch_m1_agents.h"
#include "card_test_fixture.h"
#include "cards/card_helpers.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test::batch_8 {

using batch_m1::raw;

constexpr CardDefId kLongSword = 345;          // gear, Equipment, "[Equip] [R]"
constexpr CardDefId kAzirSovereign = 497;
constexpr CardDefId kSeatOfPower = 531;        // "When you conquer here, draw 1 for each other battlefield you control"
constexpr CardDefId kBrush = 561;              // "When you score here, you may replace this with the battlefield it replaced."
constexpr CardDefId kGreenFather = 753;        // "When you conquer or hold, you may exhaust me to replace that battlefield with a Brush"
constexpr CardDefId kPoutyPoro = 13;

/// EnginePathAgent that counts priority windows and the prompts it is asked.
class Counting : public EnginePathAgent {
public:
    using EnginePathAgent::EnginePathAgent;
    int priority = 0, prompts_seen = 0;
    std::vector<std::string> labels;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        bool is_priority = false;
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) { is_priority = true; break; }
        if (is_priority) ++priority;
        else if (!legal.empty() && legal.front().type == IntentType::MakeChoice) {
            ++prompts_seen;
            labels.push_back(legal.back().choice_label);
        }
        return EnginePathAgent::selectAction(st, legal);
    }
};

class Batch8Test : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& g = o(id);
        const auto& d = card_db.get(def_id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.card_def_id = def_id;
        g.name = d.name;
        g.tags = d.tags;
        g.keywords = d.keywords;
        if (at_bf >= 0) {
            g.zone = ZoneType::BattlefieldZone;
            g.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            g.zone = ZoneType::Base;
            g.location = BaseLocation{owner};
        }
        return id;
    }
    GameObjectId addToken(PlayerId owner, int might, int at_bf = -1) {
        auto id = addUnit(owner, kInvalidId, might, at_bf);
        o(id).is_token = true;
        o(id).name = "Sand Soldier";
        return id;
    }
    /// Put battlefield card `def` in slot `bf`, owned by `owner`.
    GameObjectId placeBattlefield(int bf, CardDefId def, PlayerId owner = P1) {
        auto id = state.createObject();
        auto& b = o(id);
        b.owner = b.controller = owner;
        b.card_def_id = def;
        b.name = card_db.get(def).name;
        b.card_type = CardType::Battlefield;
        b.zone = ZoneType::BattlefieldZone;
        state.battlefields[bf].card_object_id = id;
        return id;
    }
    GameObjectId addLegend(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& l = o(id);
        const auto& d = card_db.get(def_id);
        l.owner = l.controller = owner;
        l.card_def_id = def_id;
        l.name = d.name;
        l.card_type = d.card_type;
        l.domains = d.domains;
        l.zone = ZoneType::LegendZone;
        state.player(owner).legend_zone = id;
        return id;
    }
    int finalizedDuring(const std::function<void()>& body) {
        int n = 0;
        auto conn = events.on_chain_item_finalized.connect(
            [&](const ChainItemFinalizedEvent&) { ++n; });
        body();
        conn.disconnect();
        return n;
    }
};

// ═══ U1 (REGRESSION of D4): [Weaponmaster] with no Equipment ═══════════════
//
// CR 403.1.a / 403.4 / 405.2: a triggered ability whose leading "may" is not
// offered leaves the chain as it would be finalized: no priority window, no
// resolution. WeaponmasterUnit never asked confirmOptional when there was no
// Equipment, so ChainItem::may_answer stayed -1 and the item stayed (round 2
// g09 Lucian, Merciless; g11 Riven, Shattered).

TEST_F(Batch8Test, U1_EveryWeaponmasterUnitWithNoEquipmentLeavesTheChainUnfinalized) {
    const GameState blank = state;
    std::vector<std::string> stayed;
    int swept = 0;
    for (const auto& [id, def] : card_db.all()) {
        if (def.card_type != CardType::Unit || !def.keywords.has(Keyword::Weaponmaster)) continue;
        const Card* card = card_registry.get(id);
        if (!card || !card->firesOn(TriggerType::WhenYouPlayMe)) continue;
        state = blank;
        const auto unit = addUnit(P1, id, 0);
        addGear(P2, kLongSword);   // an Equipment, but not the controller's
        Counting p1, p2;
        const int finalized = finalizedDuring([&] {
            fireTriggerThroughEngine(unit, TriggerType::WhenYouPlayMe, &p1, kInvalidId, &p2);
        });
        ++swept;
        if (finalized != 0 || p1.priority + p2.priority != 0 || p1.prompts_seen != 0)
            stayed.push_back(def.name + " (" + toString(id) + "): finalized " +
                             std::to_string(finalized) + ", priority windows " +
                             std::to_string(p1.priority + p2.priority));
    }
    state = blank;
    std::string all;
    for (const auto& s : stayed) all += "\n  " + s;
    EXPECT_TRUE(stayed.empty()) << stayed.size() << " [Weaponmaster] abilities stayed:" << all;
    EXPECT_GE(swept, 8) << "every registered [Weaponmaster] unit";
}

TEST_F(Batch8Test, U1_AWeaponmasterUnitWithEquipmentIsStillAsked) {
    // The control: with an Equipment of its own the "may" is offered,
    // accepted, and the ability is finalized and resolves.
    const CardDefId lucian = 435;
    const auto unit = addUnit(P1, lucian, 0);
    const auto sword = addGear(P1, kLongSword);
    Counting p1(EnginePathAgent::accepting()), p2;
    const int finalized = finalizedDuring([&] {
        fireTriggerThroughEngine(unit, TriggerType::WhenYouPlayMe, &p1, kInvalidId, &p2);
    });
    EXPECT_EQ(finalized, 1);
    EXPECT_EQ(o(sword).attached_to, std::optional<GameObjectId>(unit));
}

// ═══ U2 (REGRESSION of D4): Azir, Sovereign with no token to move ══════════
//
// "When I attack, you may move any number of your token units to this
// battlefield." The leading "may" (CR 383.3.e.2.b): with no token that can
// move it is not offered, and declined it is not performed; either way the
// ability leaves the chain unfinalized (CR 403.1.a, 405.2).

TEST_F(Batch8Test, U2_AzirWithNoTokenToMoveLeavesTheChainUnfinalized) {
    const auto azir = addUnit(P1, kAzirSovereign, 0, 0);
    addToken(P1, 1, 0);   // already here: nothing to move
    Counting p1, p2;
    const int finalized = finalizedDuring([&] {
        fireTriggerThroughEngine(azir, TriggerType::WhenIAttack, &p1, kInvalidId, &p2);
    });
    EXPECT_EQ(finalized, 0);
    EXPECT_EQ(p1.priority + p2.priority, 0);
    EXPECT_EQ(p1.prompts_seen, 0) << "nothing to decide: no prompt";
}

TEST_F(Batch8Test, U2_AzirsDeclinedMayLeavesTheChainUnfinalized) {
    const auto azir = addUnit(P1, kAzirSovereign, 0, 0);
    const auto token = addToken(P1, 1);
    Counting p1(EnginePathAgent::script({0})), p2;   // the "may": no
    const int finalized = finalizedDuring([&] {
        fireTriggerThroughEngine(azir, TriggerType::WhenIAttack, &p1, kInvalidId, &p2);
    });
    EXPECT_EQ(finalized, 0);
    EXPECT_EQ(p1.priority + p2.priority, 0);
    EXPECT_FALSE(o(token).isAtBattlefield());
}

TEST_F(Batch8Test, U2_AzirsAcceptedMayMovesTheChosenTokens) {
    const auto azir = addUnit(P1, kAzirSovereign, 0, 0);
    const auto token = addToken(P1, 1);
    Counting p1(EnginePathAgent::script({1, raw(token)})), p2;
    const int finalized = finalizedDuring([&] {
        fireTriggerThroughEngine(azir, TriggerType::WhenIAttack, &p1, kInvalidId, &p2);
    });
    EXPECT_EQ(finalized, 1);
    EXPECT_EQ(o(token).battlefieldId(), std::optional<BattlefieldId>(BattlefieldId::fromRaw(0)));
}


// ═══ U5: a replaced battlefield keeps its "here" (CR 439.1.a, 439.7.a) ══════
//
// "The replacing token is treated as the same Game Object as the card or
// token it replaced for the purposes of Game Effects that target or reference
// that game object" (CR 439.1.a, whose own example is Green Father resolving
// before the conquered battlefield's ability). The battlefield cards found
// their slot by `card_object_id == ctx.source`, which a replacement changes.

TEST_F(Batch8Test, U5_SeatOfPowerReplacedBeforeItsAbilityResolvesStillSkipsHere) {
    const auto seat = placeBattlefield(0, kSeatOfPower);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 2, 0);
    for (int i = 0; i < 5; ++i) addToDeck(P1, kPoutyPoro);
    const size_t hand_before = state.player(P1).hand.size();
    withEngine([&](GameEngine& e) {
        e.testHook_executor().queueTriggeredAbility(seat, P1, TriggerType::WhenYouConquerHere,
                                                    kInvalidId);
        // Green Father's ability resolves first and replaces it (round 2 g07).
        e.testHook_executor().replaceBattlefieldWithToken(BattlefieldId::fromRaw(0), "Brush", P1,
                                                          kBrush);
        settleLikeTheEngine(e);
    });
    EXPECT_TRUE(state.battlefields[0].is_token);
    EXPECT_EQ(state.player(P1).hand.size(), hand_before)
        << "no OTHER battlefield is controlled: draw 0 (it drew 1 for its own slot)";
}

TEST_F(Batch8Test, U5_BrushsAcceptedSwapBackWorksAfterTheBrushWasReplacedAgain) {
    const auto peak = placeBattlefield(0, kSeatOfPower);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 2, 0);
    const auto green = addLegend(P1, kGreenFather);
    GameObjectId brush_a = kInvalidId;
    EnginePathAgent p1(EnginePathAgent::script({1, 1}));   // Brush's may, Green Father's may
    withEngine([&](GameEngine& e) {
        auto& ex = e.testHook_executor();
        ex.replaceBattlefieldWithToken(BattlefieldId::fromRaw(0), "Brush", P1, kBrush);
        brush_a = e.mutableState().battlefields[0].card_object_id;
        // Brush A is scored (its swap-back), and Green Father's conquer
        // ability about Brush A goes above it: it resolves first, replacing
        // Brush A with Brush B (round 2 g04, D:054-059).
        ex.queueTriggeredAbility(brush_a, P1, TriggerType::WhenYouScoreHere, kInvalidId);
        ex.queueTriggeredAbility(green, P1, TriggerType::WhenIConquerOrHold, brush_a);
        settleLikeTheEngine(e);
    }, &p1);
    EXPECT_TRUE(o(green).is_exhausted) << "Green Father replaced Brush A";
    EXPECT_EQ(state.battlefields[0].card_object_id, peak)
        << "Brush A's accepted swap-back puts back the battlefield it replaced (CR 439.7.b.1)";
    EXPECT_FALSE(state.battlefields[0].is_token);
}

TEST_F(Batch8Test, U5_GreenFatherAfterASwapBackReplacesThatBattlefield) {
    const auto peak = placeBattlefield(0, kSeatOfPower);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 2, 0);
    const auto green = addLegend(P1, kGreenFather);
    EnginePathAgent p1(EnginePathAgent::script({1, 1}));   // Green Father's may, Brush's may
    withEngine([&](GameEngine& e) {
        auto& ex = e.testHook_executor();
        ex.replaceBattlefieldWithToken(BattlefieldId::fromRaw(0), "Brush", P1, kBrush);
        const auto brush = e.mutableState().battlefields[0].card_object_id;
        // P1 conquers the Brush: Green Father's ability (about the Brush) and,
        // above it, the Brush's swap-back (round 2 g04, D:088-090).
        ex.queueTriggeredAbility(green, P1, TriggerType::WhenIConquerOrHold, brush);
        ex.queueTriggeredAbility(brush, P1, TriggerType::WhenYouScoreHere, kInvalidId);
        settleLikeTheEngine(e);
    }, &p1);
    EXPECT_TRUE(o(green).is_exhausted);
    EXPECT_TRUE(state.battlefields[0].is_token)
        << "\"that battlefield\" is the card swapped back in its place: it is replaced again";
    EXPECT_NE(state.battlefields[0].card_object_id, peak);
}

}  // namespace riftbound::test::batch_8
