/// Testing phase, batch 8 (plans/remaining/10-testing-phase.md items U1-U21
/// found by round 2, V1-G4, S1 and the stopping rule's coverage; report
/// plans/reports/batch-8.md).

#include <gtest/gtest.h>

#include "batch_m1_agents.h"
#include "card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/prompt_description.h"

#include <algorithm>
#include <random>
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
constexpr CardDefId kJhinMeticulous = 651;     // "If you've spent [4] or more to play a spell this turn, you may play me for [B]."
constexpr CardDefId kPykeDockside = 590;       // [Hidden]; "You may pay [R] as an additional cost to play me. When you play me, if you paid ..., ready me and give me +2 [M] this turn."
constexpr CardDefId kPiercingLight = 346;      // "[Repeat] [2][R] Deal 2 to a unit at a battlefield, then deal 2 to up to one other unit."
constexpr CardDefId kViktorLeader = 246;       // "When another non-Recruit unit you control dies, play a 1 [M] Recruit unit token into your base."
constexpr CardDefId kTheRuination = 742;       // "Kill all units."
constexpr CardDefId kRekSaiBreacher = 352;     // "Friendly units played from anywhere other than a player's hand have [Accelerate]."
constexpr CardDefId kDeathMark = 931;          // [2][P]... prints "[Flow] [1][A][A]"
constexpr CardDefId kSacrifice = 735;          // "As an additional cost to play this, kill a friendly [Mighty] unit. Draw 2 ..."
constexpr CardDefId kNamiHeadstrong = 614;     // "You may pay [G] as an additional cost ... When you play me, if you paid ..., [Stun] an enemy unit."
constexpr CardDefId kFizzTrickster = 461;      // "When you play me, you may play a spell from your trash ..."
constexpr CardDefId kWagesOfPain = 393;        // "Deal 3 to a unit at a battlefield. Play a Gold gear token exhausted."
constexpr CardDefId kVoidreaver = 787;         // legend: "Spend 1 XP, [E]: [Buff] a unit." ...
constexpr CardDefId kStarSpring = 771;         // battlefield: "... they may move another unit they control here to its base."
constexpr CardDefId kArise = 513;              // "... Then do this: Ready up to two of them." (a reflexive trigger)
constexpr CardDefId kSerratedDirk = 332;       // gear, Equipment
constexpr CardDefId kDredgeUp = 836;           // "Draw 1." [2]; [Flow] [2]
constexpr CardDefId kThermoBeam = 22;          // "Kill all gear."
constexpr CardDefId kScrapheap = 182;          // gear: "When this is played, discarded, or killed, draw 1."
constexpr CardDefId kSpriteFountain = 640;     // gear: "[Deathknell][>] Repeat this gear's play effect."
constexpr CardDefId kReckonersArena = 281;     // "When you hold here, activate the conquer effects of units here."
constexpr CardDefId kSettBrawler = 164;        // "When I'm played and when I conquer, buff me."
constexpr CardDefId kLucianMerciless = 435;    // "The first time I conquer each turn, ready me."
constexpr CardDefId kKennenStorm = 900;        // "When you play me, [Burn 2]."
constexpr CardDefId kHeartOfTheTempest = 942;  // legend: "When you play a card from anywhere other than your hand, empower me."

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
    /// Execute `play` as the main-phase loop does, then settle.
    void playThroughEngineIntent(const Intent& play, AgentInterface* p1 = nullptr,
                                 AgentInterface* p2 = nullptr) {
        withEngine([&](GameEngine& e) {
            e.testHook_executeIntent(play);
            settleLikeTheEngine(e);
        }, p1, p2);
    }
    void hideAt(GameObjectId card, int bf) {
        auto& hand = state.player(o(card).owner).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), card), hand.end());
        o(card).zone = ZoneType::FacedownZone;
        o(card).is_hidden = true;
        o(card).hidden_at = BattlefieldId::fromRaw(bf);
        o(card).hidden_on_turn = state.turn.turn_number - 1;
        state.battlefields[bf].facedown.push_back(card);
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


// ═══ U4: two abilities triggered by one play are ordered together ══════════
//
// CR 383.3.d: "If more than one Triggered Ability is Triggered
// simultaneously, then the player that controls the Abilities selects the
// order to place them on the Chain." A play is one event: Kennen's "When you
// play me" and Heart of the Tempest's "When you play a card from anywhere
// other than your hand" trigger together (round 2 g03, SNAP 136-137).

TEST_F(Batch8Test, U4_APlayFromTheChampionZoneAsksTheOrderOfItsTwoPlayAbilities) {
    addLegend(P1, kHeartOfTheTempest);
    const auto kennen = addToHand(P1, kKennenStorm);
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), kennen), h.end());
        o(kennen).zone = ZoneType::ChampionZone;
        state.player(P1).champion_zone = kennen;
        state.player(P1).chosen_champion = kennen;
    }
    for (int i = 0; i < 6; ++i) addToDeck(P1, kPoutyPoro);
    state.player(P1).rune_pool.energy += 3;
    state.player(P1).rune_pool.universal_power += 1;
    EnginePathAgent p1, p2;
    withEngine([&](GameEngine& e) {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = kennen;
        play.play_location = LocationId{BaseLocation{P1}};
        e.testHook_executeIntent(play);
        settleLikeTheEngine(e);
    }, &p1, &p2);
    ASSERT_TRUE(o(kennen).location.has_value());
    EXPECT_EQ(p1.order_prompts.size(), 1u) << "P1 orders the two abilities of one play";
}

TEST_F(Batch8Test, U4_AnEffectPlayAsksTheOrderOfItsTwoPlayAbilities) {
    addLegend(P1, kHeartOfTheTempest);
    const auto kennen = addToHand(P1, kKennenStorm);
    for (int i = 0; i < 6; ++i) addToDeck(P1, kPoutyPoro);
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), kennen), h.end());
        o(kennen).zone = ZoneType::Trash;
        state.player(P1).trash.push_back(kennen);
    }
    EnginePathAgent p1, p2;
    withEngine([&](GameEngine& e) {
        e.testHook_executor().playIgnoringCost(P1, kennen);
        settleLikeTheEngine(e);
    }, &p1, &p2);
    ASSERT_TRUE(o(kennen).location.has_value());
    EXPECT_EQ(p1.order_prompts.size(), 1u);
}


// ═══ U3: a modal spell offered a mode whose target cannot be chosen ════════
//
// Curtain Call / Rocket Barrage built their mode mask from the units they may
// choose by text (chooseableUnits), while the target prompt offers only the
// units that can be chosen now (Card::offerableChoices: a [Deflect] unit only
// if its [A] can be paid, and before the cost only what leaves the spell
// payable). With 10 floating Energy and no Power, an enemy Rengar, Unseen
// ([Deflect]) at a battlefield made "Deal 2 to a unit at a battlefield" a
// mode with no target: executePlaySpell threw (round 2, the only fuzz
// failure; CR 355.8). The sweep plays every spell that chooses as it is
// played, on random boards with [Deflect] units, funded with Energy and Power
// and with Energy only, answering its first prompt with EACH of its options
// (so every mode is tried) and every later prompt with the first.
TEST_F(Batch8Test, U3_Sweep_EveryOptionOfASpellsFirstChoiceCanBeCompleted) {
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
    std::mt19937 rng(20261015);
    std::vector<std::string> misses;
    int plays = 0;
    for (int board = 0; board < 24; ++board) {
        const unsigned seed = rng();
        for (int power = 0; power < 3; ++power)
        for (CardDefId id : spells) {
            for (int option = 0; option < 8; ++option) {
                state = GameState{};
                state.players[0].id = P1;
                state.players[1].id = P2;
                addBattlefield();
                addBattlefield();
                std::mt19937 b(seed);
                for (PlayerId p : {P1, P2}) {
                    const int n = 1 + static_cast<int>(b() % 3);
                    for (int k = 0; k < n; ++k) {
                        const int where = static_cast<int>(b() % 3) - 1;   // base, bf0, bf1
                        auto u = addUnit(p, b() % 2 == 0 ? kPoutyPoro : kInvalidId,
                                         1 + static_cast<int>(b() % 6), where);
                        (void)u;
                    }
                    if (b() % 2 == 0) addGear(p, kLongSword);
                    for (int k = 0; k < 4; ++k) addToDeck(p, kPoutyPoro);
                }
                auto card = addToHand(P1, id);
                state.player(P1).rune_pool.energy += 12;
                // Energy only; exactly the spell's own Power (a Deflect then
                // cannot be paid on top); plenty of both.
                state.player(P1).rune_pool.universal_power +=
                    power == 0 ? 0 : power == 1 ? card_db.get(id).power_cost : 12;
                Intent play;
                bool offered = false;
                withEngine([&](GameEngine& e) {
                    e.testHook_cleanup();
                    for (const auto& i : e.generateLegalActions())
                        if (i.card == card) { offered = true; play = i; break; }
                });
                if (!offered) break;
                int first_size = 0;
                bool first_seen = false;
                EnginePathAgent agent([&](const std::vector<Intent>& legal) {
                    if (!first_seen) {
                        first_seen = true;
                        first_size = static_cast<int>(legal.size());
                        if (option < first_size) return legal[static_cast<size_t>(option)];
                    }
                    return legal.front();
                });
                ++plays;
                try {
                    withEngine([&](GameEngine& e) {
                        e.testHook_executeIntent(play);
                        settleLikeTheEngine(e);
                    }, &agent);
                } catch (const std::exception& ex) {
                    misses.push_back(card_db.get(id).name + " (option " + std::to_string(option) +
                                     (power == 0 ? ", Energy only" : power == 1
                                          ? ", its own Power only" : ", Energy and Power") +
                                     "): " + ex.what());
                }
                if (!first_seen || option + 1 >= first_size) break;
            }
        }
    }
    EXPECT_GT(plays, 2000);
    std::sort(misses.begin(), misses.end());
    misses.erase(std::unique(misses.begin(), misses.end()), misses.end());
    std::string all;
    for (const auto& m : misses) all += "\n  " + m;
    EXPECT_TRUE(misses.empty()) << misses.size() << " plays could not be completed:" << all;
}


// ═══ U6: Jhin, Meticulous Killer's "play me for [B]" ═══════════════════════
//
// "If you've spent [4] or more to play a spell this turn, you may play me for
// [B]." No zone is named, so it is a way to pay for any play of the card: from
// the hand too, and to every location the unit may be played to (CR 355.2).
// It was offered only from the Champion Zone and only to base (round 2 g13).

TEST_F(Batch8Test, U6_JhinsAlternativeCostIsOfferedFromTheHandToEveryLocation) {
    const auto jhin = addToHand(P1, kJhinMeticulous);
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 1, 0);
    state.player(P1).max_spell_spent_this_turn = 4;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;   // [B], nothing else
    std::vector<Intent> alt;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.card == jhin && i.type == IntentType::PlayCard) {
                EXPECT_TRUE(i.use_alt_play_cost) << "the printed [4] cannot be paid";
                alt.push_back(i);
            }
    });
    ASSERT_EQ(alt.size(), 2u) << "base and the battlefield P1 controls";
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(*alt[0].play_location));
    EXPECT_TRUE(std::holds_alternative<BattlefieldLocation>(*alt[1].play_location));
    playThroughEngineIntent(alt[1]);
    EXPECT_EQ(o(jhin).battlefieldId(), std::optional<BattlefieldId>(BattlefieldId::fromRaw(0)));
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)], 0) << "paid [B]";
}

TEST_F(Batch8Test, U6_NotOfferedBeforeFourWasSpentOnASpell) {
    const auto jhin = addToHand(P1, kJhinMeticulous);
    state.player(P1).max_spell_spent_this_turn = 3;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions()) EXPECT_NE(i.card, jhin);
    });
}

// Dragon Roost's pay-to-play-here offer is judged with the printed cost
// (GameEngine::generateMainPhaseActions); no card with an alternative cost
// is a Dragon, so none is withheld it.
TEST_F(Batch8Test, U6_NoCardWithAnAlternativeCostIsADragon) {
    GameState probe;
    probe.player(P1).max_spell_spent_this_turn = 99;
    int with_alt = 0;
    for (const auto& [id, def] : card_db.all()) {
        const Card* c = card_registry.get(id);
        if (!c || !c->alternativePlayCost(probe, P1).valid) continue;
        ++with_alt;
        EXPECT_EQ(std::find(def.tags.begin(), def.tags.end(), "Dragon"), def.tags.end()) << def.name;
    }
    EXPECT_GE(with_alt, 1);
}


// ═══ U7: an optional additional cost on a play from face down ══════════════
//
// A play from face down ignores the base cost (CR 811.1.b); an Optional
// Additional Cost is still offered (CR 356.1.b and its example). Pyke,
// Dockside Butcher's [R] was never asked (round 2 g15, SNAP 64).

TEST_F(Batch8Test, U7_PykePlayedFromFaceDownIsOfferedItsAdditionalCost) {
    state.turn.turn_number = 3;
    state.battlefields[0].controller = P1;
    addUnit(P1, kInvalidId, 2, 0);
    const auto pyke = addToHand(P1, kPykeDockside);
    hideAt(pyke, 0);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;   // [R]
    Intent play;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.card == pyke) { play = i; break; }
    });
    ASSERT_EQ(play.card, pyke) << "the hidden play is offered";
    EnginePathAgent p1(EnginePathAgent::script({1})), p2;   // pay [R]
    playThroughEngineIntent(play, &p1, &p2);
    ASSERT_EQ(o(pyke).battlefieldId(), std::optional<BattlefieldId>(BattlefieldId::fromRaw(0)));
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 0) << "[R] paid";
    EXPECT_FALSE(o(pyke).is_exhausted) << "ready me";
    EXPECT_EQ(o(pyke).current_might, 4) << "+2 [M] this turn";
}

TEST_F(Batch8Test, U7_NoHiddenCardPrintsAMandatoryAdditionalCost) {
    // executePlayFromHidden offers the optional additional cost only: a
    // mandatory one ("As an additional cost to play me, ...") would need the
    // hidden-play gate too. None exists.
    GameState probe;
    probe.players[0].id = P1;
    probe.players[1].id = P2;
    for (const auto& [id, def] : card_db.all()) {
        if (!def.keywords.has(Keyword::Hidden)) continue;
        const Card* c = card_registry.get(id);
        ASSERT_NE(c, nullptr);
        GameState st = probe;
        const auto obj = st.createObject();
        st.getObject(obj).card_def_id = id;
        st.getObject(obj).owner = st.getObject(obj).controller = P1;
        const auto addl = c->additionalPlayCost(st, card_db, P1, obj);
        EXPECT_FALSE(addl.exists && addl.mandatory) << def.name;
    }
}


// ═══ U8 / U9: Piercing Light's two different targets ═══════════════════════
//
// "Deal 2 to a unit at a battlefield, then deal 2 to up to one other unit."
// U8: a [Repeat] execution chooses again following the card's requirements
// (CR 820.2, 820.2.a; 355.13 "up to"): it went through the generic re-choice,
// which offered base units for the first unit and no "none" for the second
// (round 2 g15, SNAP 197-198). U9: played by an effect it was offered "No
// target" for its required first unit (CR 355.8; g15 SNAP 316).

namespace {
/// Answers Piercing Light's prompts: the first unit `first`, the other one
/// none; pays any [Repeat]; checks that no unit in a base is offered as the
/// first, and that "none" is offered for the other one and never for the first.
class PiercingAgent : public EnginePathAgent {
public:
    GameObjectId first = kInvalidId;
    const GameState* st = nullptr;
    int first_prompts = 0, other_prompts = 0, repeats = 0;
    std::vector<std::string> wrong;
    std::vector<int> energy_at_prompt;   // P1's floating Energy at each card prompt
    std::string question;                // the asking code's words (Prompt::question)
    PromptKind kind = PromptKind::Unspecified;
    Intent decide(const GameState& state, const std::vector<Intent>& legal,
                  const Prompt& prompt) override {
        question = prompt.question;
        kind = prompt.kind;
        return selectAction(state, legal);
    }
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        const bool is_choice = !legal.empty() && std::all_of(legal.begin(), legal.end(),
            [](const Intent& i) { return i.type == IntentType::MakeChoice; });
        if (!is_choice) return EnginePathAgent::selectAction(state, legal);
        energy_at_prompt.push_back(state.player(PlayerId::Player1).rune_pool.energy);
        bool any_object = false, none = false;
        for (const auto& i : legal) {
            if (!i.chosen_objects.empty()) any_object = true;
            else if (i.chosen_value.value_or(-1) == 0) none = true;
        }
        const std::string label = question;
        if (kind == PromptKind::Repeat) {
            ++repeats;
            for (const auto& i : legal) if (i.chosen_value.value_or(0) == 1) return i;
        }
        if (any_object && label.find("at a battlefield") != std::string::npos) {
            ++first_prompts;
            if (none) wrong.push_back("\"none\" offered for the first unit");
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && !state.getObject(i.chosen_objects.front()).isAtBattlefield())
                    wrong.push_back("a unit in a base offered as the first unit");
            for (const auto& i : legal)
                if (!i.chosen_objects.empty() && i.chosen_objects.front() == first) return i;
        }
        if (any_object && label.find("other unit") != std::string::npos) {
            ++other_prompts;
            if (!none) wrong.push_back("no \"none\" for the other unit");
            for (const auto& i : legal) if (i.chosen_objects.empty()) return i;
        }
        return legal.front();
    }
};
}  // namespace

TEST_F(Batch8Test, U8_PiercingLightsRepeatChoosesByTheCardsOwnRequirements) {
    const auto at_bf = addUnit(P2, kInvalidId, 5, 0);
    const auto in_base = addUnit(P2, kInvalidId, 5);
    const auto spell = addToHand(P1, kPiercingLight);
    state.player(P1).rune_pool.energy += 2;   // the spell's [2]... and the Repeat's [2][R]
    state.player(P1).rune_pool.energy += 2;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 2;
    PiercingAgent p1;
    p1.first = at_bf;
    playThroughEngine(spell, &p1, std::nullopt, nullptr, /*fund=*/false);
    for (const auto& w : p1.wrong) ADD_FAILURE() << w;
    EXPECT_EQ(p1.repeats, 1) << "the Repeat was offered and paid";
    EXPECT_EQ(p1.first_prompts, 2) << "the first unit is asked for each execution";
    EXPECT_EQ(p1.other_prompts, 2);
    EXPECT_EQ(o(at_bf).damage_marked, 4) << "both executions dealt 2 to it";
    EXPECT_EQ(o(in_base).damage_marked, 0);
}

TEST_F(Batch8Test, U9_PiercingLightPlayedByAnEffectIsNotOfferedNoTargetForItsFirstUnit) {
    const auto at_bf = addUnit(P2, kInvalidId, 5, 0);
    addUnit(P2, kInvalidId, 5);
    const auto spell = addToHand(P1, kPiercingLight);
    state.player(P1).rune_pool.energy += 2;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    PiercingAgent p1;
    p1.first = at_bf;
    withEngine([&](GameEngine& e) {
        EXPECT_TRUE(e.testHook_executor().playPayingCost(P1, spell));
        settleLikeTheEngine(e);
    }, &p1);
    for (const auto& w : p1.wrong) ADD_FAILURE() << w;
    EXPECT_EQ(p1.first_prompts, 1);
    EXPECT_EQ(o(at_bf).damage_marked, 2);
}


// ═══ U11: [Repeat] is a choice of the Make Relevant Choices step ═══════════
//
// CR 355.1.a: "This includes the choice of whether or not to pay an Optional
// Additional Cost"; CR 820.2: an extra execution's choices are made "at the
// usual time during the Make Relevant Choices step". Both before any cost
// (CR 356 / 357) — they came after the base cost (round 2 g03 Hard Bargain,
// g08 / g11 Blood Rush, g19 Bellows Breath).
TEST_F(Batch8Test, U11_TheRepeatAndItsChoicesComeBeforeAnyCostIsPaid) {
    const auto at_bf = addUnit(P2, kInvalidId, 5, 0);
    addUnit(P2, kInvalidId, 5);   // a choice for "up to one other unit" too
    const auto spell = addToHand(P1, kPiercingLight);
    state.player(P1).rune_pool.energy += 4;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 2;
    PiercingAgent p1;
    p1.first = at_bf;
    playThroughEngine(spell, &p1, std::nullopt, nullptr, /*fund=*/false);
    // first unit, other unit, Repeat, first unit again, other unit again
    ASSERT_EQ(p1.energy_at_prompt.size(), 5u);
    for (size_t k = 0; k < p1.energy_at_prompt.size(); ++k)
        EXPECT_EQ(p1.energy_at_prompt[k], 4) << "prompt " << k << ": nothing paid yet";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0) << "then [2] and the Repeat's [2] paid";
    EXPECT_EQ(o(at_bf).damage_marked, 4);
}

TEST_F(Batch8Test, U11_TheRepeatIsNotOfferedWhenItAndTheBaseCostCannotBothBePaid) {
    addUnit(P2, kInvalidId, 5, 0);
    const auto spell = addToHand(P1, kPiercingLight);
    state.player(P1).rune_pool.energy += 3;   // [2] + [2] would be 4
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 2;
    PiercingAgent p1;
    p1.first = kInvalidId;
    playThroughEngine(spell, &p1, std::nullopt, nullptr, /*fund=*/false);
    EXPECT_EQ(p1.repeats, 0) << "not offered";
}

// U10: an effect play that pays is a play: its [Repeat] is offered (CR
// 820.1.d "as you play this"; round 2 g15 Fizz, Trickster).
TEST_F(Batch8Test, U10_AnEffectPlayThatPaysIsOfferedTheRepeat) {
    const auto at_bf = addUnit(P2, kInvalidId, 5, 0);
    const auto spell = addToHand(P1, kPiercingLight);
    state.player(P1).rune_pool.energy += 4;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 2;
    PiercingAgent p1;
    p1.first = at_bf;
    withEngine([&](GameEngine& e) {
        EXPECT_TRUE(e.testHook_executor().playPayingCost(P1, spell));
        settleLikeTheEngine(e);
    }, &p1);
    for (const auto& w : p1.wrong) ADD_FAILURE() << w;
    EXPECT_EQ(p1.repeats, 1);
    EXPECT_EQ(o(at_bf).damage_marked, 4);
}


// ═══ X1 (found by batch 8): a death batch's triggers followed the kill order ═
//
// The Ruination ("Kill all units") kills its batch one after another in
// object-table order. Viktor, Leader, dying in the same batch, put his "when
// another unit dies" ability on the chain when a friendly unit happened to
// be killed before him, and endDeathBatch took it back (CR 383.2.c.2) — with
// a TRIGGER line in the log only in that order: the relabelled-games test
// (HiddenOrderLeak) told two numberings of one game apart. A batch member
// that will die in the batch does not trigger on another member's death at
// all (TriggerManager::fireTrigger), whichever is killed first.
TEST_F(Batch8Test, X1_AUnitDyingInTheSameBatchNeverTriggersOnAnotherMembersDeath) {
    for (int viktor_first = 0; viktor_first < 2; ++viktor_first) {
        state = GameState{};
        state.players[0].id = P1;
        state.players[1].id = P2;
        addBattlefield();
        addBattlefield();
        GameObjectId viktor, other;
        if (viktor_first) {
            viktor = addUnit(P1, kViktorLeader, 0, 0);
            other = addUnit(P1, kInvalidId, 2, 0);
        } else {
            other = addUnit(P1, kInvalidId, 2, 0);
            viktor = addUnit(P1, kViktorLeader, 0, 0);
        }
        const auto spell = addToHand(P2, kTheRuination);
        std::vector<std::string> lines;
        auto conn = events.on_log.connect([&](const LogEvent& e) {
            if (e.message.find("Viktor, Leader") != std::string::npos) lines.push_back(e.message);
        });
        state.turn.turn_player = P2;
        playThroughEngine(spell);
        conn.disconnect();
        EXPECT_FALSE(o(viktor).location.has_value());
        EXPECT_FALSE(o(other).location.has_value());
        for (const auto& l : lines)
            EXPECT_EQ(l.find("fires ability"), std::string::npos) << l;
        int recruits = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.name == "Recruit" && obj.location.has_value()) ++recruits;
        EXPECT_EQ(recruits, 0) << "Viktor died with the unit he watched (CR 383.2.c.2)";
    }
}

// The control: Viktor outside the batch sees the unit die.
TEST_F(Batch8Test, X1_AUnitOutsideTheBatchStillTriggers) {
    addUnit(P1, kViktorLeader, 0, 0);
    const auto other = addUnit(P1, kInvalidId, 2, 0);
    withEngine([&](GameEngine& e) {
        {
            DeathBatch together(e.testHook_executor(), {other});
            e.testHook_executor().killObject(other);
        }
        settleLikeTheEngine(e);
    });
    int recruits = 0;
    for (const auto& [id, obj] : state.objects)
        if (obj.name == "Recruit" && obj.location.has_value()) ++recruits;
    EXPECT_EQ(recruits, 1);
}


// ═══ U12: a unit token played by an effect and a granted [Accelerate] ══════
//
// Rek'Sai, Breacher: "Friendly units played from anywhere other than a
// player's hand have [Accelerate]." A token is played (CR 185.2.a), not from
// a hand, so it has [Accelerate] and may pay [1] and [A] (it has no Domain,
// CR 805.1.b.2) to enter ready (round 2 g10: Up from the Deep's Tentacles).

TEST_F(Batch8Test, U12_AUnitTokenPlayedWithRekSaiOnTheBoardIsOfferedAccelerate) {
    addUnit(P1, kRekSaiBreacher, 0);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.universal_power = 1;
    GameObjectId token = kInvalidId;
    EnginePathAgent p1(EnginePathAgent::script({1}));   // pay [Accelerate]
    withEngine([&](GameEngine& e) {
        token = e.testHook_executor().createToken(P1, CardType::Unit, "Tentacle", 1, {},
                                                  KeywordSet{}, LocationId{BaseLocation{P1}},
                                                  /*enter_ready=*/false);
        settleLikeTheEngine(e);
    }, &p1);
    EXPECT_EQ(p1.prompts.size(), 1u) << "asked once";
    EXPECT_FALSE(o(token).is_exhausted) << "entered ready";
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(Batch8Test, U12_WithoutTheGrantOrTheResourcesNothingIsAsked) {
    for (int rek_sai = 0; rek_sai < 2; ++rek_sai) {
        state = GameState{};
        state.players[0].id = P1;
        state.players[1].id = P2;
        addBattlefield();
        addBattlefield();
        if (rek_sai) addUnit(P1, kRekSaiBreacher, 0);   // but nothing to pay with
        else {
            state.player(P1).rune_pool.energy = 1;
            state.player(P1).rune_pool.universal_power = 1;
        }
        GameObjectId token = kInvalidId;
        EnginePathAgent p1(EnginePathAgent::script({}));
        withEngine([&](GameEngine& e) {
            token = e.testHook_executor().createToken(P1, CardType::Unit, "Tentacle", 1, {},
                                                      KeywordSet{}, LocationId{BaseLocation{P1}},
                                                      false);
            settleLikeTheEngine(e);
        }, &p1);
        EXPECT_TRUE(p1.prompts.empty());
        EXPECT_TRUE(o(token).is_exhausted);
    }
}

// ═══ U13: a spell that prints [Flow] and is given [Flow] ═══════════════════
//
// CR 829.1.c.3: "If a card has multiple instances of the Flow keyword with
// different costs, its controller may choose". Kennen, Storm of Shuriken
// gives a spell in the trash [Flow] equal to its cost; a spell that prints
// its own [Flow] ignored the grant (round 2, Flow focus: Lightning Rush).

TEST_F(Batch8Test, U13_BothFlowCostsAreOfferedAndEachIsPaidAsItSays) {
    const auto mark = addToHand(P1, kDeathMark);
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), mark), h.end());
        o(mark).zone = ZoneType::Trash;
        state.player(P1).trash.push_back(mark);
        o(mark).card_counters["__flow_granted_turn"] = state.turn.turn_number;
    }
    for (int i = 0; i < 5; ++i) addToDeck(P1, kPoutyPoro);
    // [2] and a Chaos Power: the granted Flow ([2][P], its cost) is payable,
    // the printed one ([1][A][A]) is not.
    state.player(P1).rune_pool.energy = 2;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;
    std::vector<Intent> plays;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        for (const auto& i : e.generateLegalActions())
            if (i.card == mark) plays.push_back(i);
    });
    ASSERT_EQ(plays.size(), 1u);
    EXPECT_TRUE(plays[0].use_alt_play_cost) << "the granted Flow, the card's alternative";
    // With [1] and two Power of any Domain as well, both are offered.
    state.player(P1).rune_pool.energy = 3;
    state.player(P1).rune_pool.universal_power = 2;
    plays.clear();
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        for (const auto& i : e.generateLegalActions())
            if (i.card == mark) plays.push_back(i);
    });
    ASSERT_EQ(plays.size(), 2u);
    EXPECT_NE(plays[0].use_alt_play_cost, plays[1].use_alt_play_cost);
    // Play it for the granted cost: [2] and the Chaos Power are spent, the
    // [A] are not.
    const Intent granted = plays[0].use_alt_play_cost ? plays[0] : plays[1];
    playThroughEngineIntent(granted);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 2);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)], 0);
    EXPECT_EQ(o(mark).zone.get(), ZoneType::Banishment) << "a Flow play banishes it";
}


// ═══ U14: an additional cost's object is not a target ══════════════════════
//
// CR 355.10.c: "As an additional cost to play me, kill a friendly unit"
// doesn't target anything. Sacrifice asked its unit as a Target (pickTarget):
// the prompt kind, a Deflect gate and "chosen" counts (round 2 g01, g08).
TEST_F(Batch8Test, U14_SacrificesUnitIsAskedAsACostNotATarget) {
    const auto a = addUnit(P1, kInvalidId, 5, 0);
    const auto b = addUnit(P1, kInvalidId, 6, 0);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kPoutyPoro);
    const auto spell = addToHand(P1, kSacrifice);
    std::vector<PromptKind> kinds;
    class Kinds : public EnginePathAgent {
    public:
        std::vector<PromptKind>* out = nullptr;
        GameObjectId want = kInvalidId;
        Intent decide(const GameState& st, const std::vector<Intent>& legal,
                      const Prompt& prompt) override {
            if (!legal.empty() && legal.front().type == IntentType::MakeChoice) {
                out->push_back(prompt.kind);
                for (const auto& i : legal)
                    if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
            }
            return selectAction(st, legal);
        }
    } agent;
    agent.out = &kinds;
    agent.want = b;
    playThroughEngine(spell, &agent);
    ASSERT_FALSE(kinds.empty());
    EXPECT_EQ(kinds.front(), PromptKind::ChooseObject) << "a cost, not a Target";
    EXPECT_TRUE(o(a).location.has_value());
    EXPECT_FALSE(o(b).location.has_value()) << "the unit named paid the cost";
    EXPECT_EQ(state.player(P1).hand.size(), 2u) << "draw 2";
}

// ═══ U15: Nami, Headstrong's condition is the trigger's ════════════════════
//
// "When you play me, if you paid the additional cost, [Stun] an enemy unit."
// CR 383.2.a.1: judged as the event happens. Unpaid, the ability does not
// trigger (it triggered and was removed with "no legal targets", round 2 g18).
TEST_F(Batch8Test, U15_NamiPlayedWithoutPayingDoesNotTrigger) {
    addUnit(P2, kInvalidId, 3, 0);
    for (int paid = 0; paid < 2; ++paid) {
        const auto nami = addToHand(P1, kNamiHeadstrong);
        if (paid) state.player(P1).rune_pool.power[static_cast<int>(Domain::Calm)] = 1;
        std::vector<std::string> lines;
        auto conn = events.on_log.connect([&](const LogEvent& e) {
            if (e.message.find("TRIGGER: Nami") != std::string::npos) lines.push_back(e.message);
        });
        EnginePathAgent p1(EnginePathAgent::script({paid, EnginePathAgent::kRest}));
        playThroughEngine(nami, &p1);
        conn.disconnect();
        EXPECT_EQ(lines.size(), paid ? 1u : 0u) << (paid ? "paid: it triggers" : "unpaid: no trigger");
    }
}


// ═══ U16: what describePrompt says a prompt is about ══════════════════════
//
// What a player (or the trainer) is shown of a prompt (testing phase T1):
// round 2 found a spell played by an effect described as "its triggered
// ability", an activated ability with a target and a reflexive trigger as
// "its play", and a battlefield named by the player who brought it rather
// than its controller (CR 190.6).

namespace {
/// Records describePrompt's sentence for every card prompt, as the decider
/// is shown it; answers with the EnginePathAgent picker.
class Describing : public EnginePathAgent {
public:
    using EnginePathAgent::EnginePathAgent;
    const CardDB* db = nullptr;
    std::vector<std::string> texts;
    Intent decide(const GameState& st, const std::vector<Intent>& legal,
                  const Prompt& prompt) override {
        if (!legal.empty() && legal.front().type == IntentType::MakeChoice)
            texts.push_back(describePrompt(st, prompt, legal, prompt.decider, *db).text);
        return selectAction(st, legal);
    }
};
bool anyContains(const std::vector<std::string>& texts, const std::string& what) {
    return std::any_of(texts.begin(), texts.end(),
                       [&](const std::string& t) { return t.find(what) != std::string::npos; });
}
std::string joined(const std::vector<std::string>& texts) {
    std::string out;
    for (const auto& t : texts) out += "\n  " + t;
    return out;
}
}  // namespace

TEST_F(Batch8Test, U16_ASpellPlayedByAnEffectIsDescribedAsTheSpell) {
    addUnit(P2, kInvalidId, 5, 0);
    const auto wages = addToHand(P1, kWagesOfPain);
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), wages), h.end());
        o(wages).zone = ZoneType::Trash;
        state.player(P1).trash.push_back(wages);
    }
    const auto fizz = addUnit(P1, kFizzTrickster, 0);
    state.player(P1).rune_pool.universal_power = 3;
    Describing p1(EnginePathAgent::accepting());
    p1.db = &card_db;
    fireTriggerThroughEngine(fizz, TriggerType::WhenYouPlayMe, &p1);
    EXPECT_TRUE(anyContains(p1.texts, "Wages of Pain in the trash, its spell") ||
                anyContains(p1.texts, "Wages of Pain on the chain, its spell"))
        << joined(p1.texts);
    EXPECT_FALSE(anyContains(p1.texts, "Wages of Pain in the trash, its triggered ability"))
        << joined(p1.texts);
}

TEST_F(Batch8Test, U16_AnActivatedAbilitysTargetIsDescribedAsTheActivatedAbility) {
    const auto legend = addLegend(P1, kVoidreaver);
    addUnit(P1, kInvalidId, 3, 0);
    addUnit(P1, kInvalidId, 2, 0);
    setXp(P1, 3);
    Intent act;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.type == IntentType::ActivateAbility && i.ability_source == legend &&
                i.ability_index == 0) { act = i; break; }
    });
    ASSERT_EQ(act.ability_source, legend);
    Describing p1;
    p1.db = &card_db;
    playThroughEngineIntent(act, &p1);
    ASSERT_FALSE(p1.texts.empty());
    EXPECT_TRUE(anyContains(p1.texts, "its activated ability 1")) << joined(p1.texts);
    EXPECT_FALSE(anyContains(p1.texts, "its play")) << joined(p1.texts);
}

TEST_F(Batch8Test, U16_ABattlefieldIsNamedByItsController) {
    const auto spring = placeBattlefield(0, kStarSpring, P2);   // P2 brought it
    state.battlefields[0].controller = P1;                       // P1 holds it
    addUnit(P1, kInvalidId, 2, 0);
    Prompt pr;
    pr.kind = PromptKind::May;
    pr.decider = P1;
    pr.source = spring;
    Intent no, yes;
    no.type = yes.type = IntentType::MakeChoice;
    no.player = yes.player = P1;
    no.chosen_value = 0;
    yes.chosen_value = 1;
    no.declines = true;
    const auto d = describePrompt(state, pr, {no, yes}, P1, card_db);
    EXPECT_NE(d.text.find("your Star Spring"), std::string::npos) << d.text;
    EXPECT_EQ(d.asker, 1) << "the asker is P1, its controller";
    const auto seen_by_p2 = describePrompt(state, pr, {no, yes}, P2, card_db);
    EXPECT_NE(seen_by_p2.text.find("the opponent's Star Spring"), std::string::npos) << seen_by_p2.text;
}


TEST_F(Batch8Test, U16_AReflexiveTriggerIsDescribedAsATriggeredAbility) {
    addGear(P1, kSerratedDirk);
    addGear(P1, kSerratedDirk);
    const auto spell = addToHand(P1, kArise);
    Describing p1(EnginePathAgent::first());
    p1.db = &card_db;
    playThroughEngine(spell, &p1);
    EXPECT_TRUE(anyContains(p1.texts, "Arise! on the chain, its triggered ability") ||
                anyContains(p1.texts, "Arise! in the trash, its triggered ability") ||
                anyContains(p1.texts, "Arise!, its triggered ability"))
        << joined(p1.texts);
    EXPECT_FALSE(anyContains(p1.texts, "its play")) << joined(p1.texts);
}


// ═══ U18: a play from the trash pays its cost like a card's cost ═══════════
//
// CR 829.1.c.1: a Flow cost is the spell's cost. A hand play asks the payer
// for every rune, forced picks included, in prompts about the card; a Flow
// play asked nothing with one kind of rune left and its prompts named no card
// (round 2, Flow focus: 26 of 26 Up from the Deep plays).
TEST_F(Batch8Test, U18_AFlowPlayAsksEveryRuneLikeAHandPlayAndNamesTheCard) {
    const auto dredge = addToHand(P1, kDredgeUp);
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), dredge), h.end());
        o(dredge).zone = ZoneType::Trash;
        state.player(P1).trash.push_back(dredge);
    }
    for (int i = 0; i < 3; ++i) addToDeck(P1, kPoutyPoro);
    addRune(P1, Domain::Mind);
    addRune(P1, Domain::Mind);   // one kind of rune: every pick is forced
    Intent play;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        for (const auto& i : e.generateLegalActions())
            if (i.card == dredge && i.play_source == Intent::PlaySource::Trash) { play = i; break; }
    });
    ASSERT_EQ(play.card, dredge) << "the Flow play is offered";
    class Runes : public EnginePathAgent {
    public:
        GameObjectId card = kInvalidId;
        int asked = 0, about_card = 0;
        Intent decide(const GameState& st, const std::vector<Intent>& legal,
                      const Prompt& prompt) override {
            if (prompt.kind == PromptKind::RuneForEnergy || prompt.kind == PromptKind::RuneForPower) {
                ++asked;
                if (prompt.source == card) ++about_card;
            }
            return selectAction(st, legal);
        }
    } p1;
    p1.card = dredge;
    playThroughEngineIntent(play, &p1);
    EXPECT_EQ(p1.asked, 2) << "two runes exhausted for [2], each asked";
    EXPECT_EQ(p1.about_card, 2) << "the prompts are about Dredge Up";
    EXPECT_EQ(o(dredge).zone.get(), ZoneType::Banishment);
}


// ═══ U19: log lines that said what had not happened ═══════════════════════
TEST_F(Batch8Test, U19_FizzSaysTheRecycleWhenItHappens) {
    addUnit(P2, kInvalidId, 5, 0);
    const auto wages = addToHand(P1, kWagesOfPain);
    {
        auto& h = state.player(P1).hand;
        h.erase(std::remove(h.begin(), h.end(), wages), h.end());
        o(wages).zone = ZoneType::Trash;
        state.player(P1).trash.push_back(wages);
    }
    const auto fizz = addUnit(P1, kFizzTrickster, 0);
    state.player(P1).rune_pool.universal_power = 3;
    std::vector<std::string> lines;
    auto conn = events.on_log.connect([&](const LogEvent& e) { lines.push_back(e.message); });
    EnginePathAgent p1(EnginePathAgent::accepting());
    fireTriggerThroughEngine(fizz, TriggerType::WhenYouPlayMe, &p1);
    conn.disconnect();
    int resolved_at = -1, recycled_at = -1;
    for (int k = 0; k < static_cast<int>(lines.size()); ++k) {
        EXPECT_EQ(lines[k].find("then recycled it"), std::string::npos) << lines[k];
        if (lines[k].find("RESOLVE: Wages of Pain") != std::string::npos) resolved_at = k;
        if (lines[k].find("RECYCLE: Wages of Pain") != std::string::npos) recycled_at = k;
    }
    ASSERT_GE(resolved_at, 0);
    EXPECT_GT(recycled_at, resolved_at) << "the recycle is written as it happens";
    EXPECT_TRUE(inDeck(P1, wages));
}

TEST_F(Batch8Test, U19_AMightChangeStatesNoMightThatTheCleanupHasNotSettled) {
    const auto unit = addUnit(P1, kInvalidId, 3, 0);
    std::vector<std::string> lines;
    auto conn = events.on_log.connect([&](const LogEvent& e) { lines.push_back(e.message); });
    withEngine([&](GameEngine& e) { e.testHook_executor().giveTemporaryMight(unit, -2); });
    conn.disconnect();
    bool seen = false;
    for (const auto& l : lines)
        if (l.find("EFFECT: give") != std::string::npos) {
            seen = true;
            EXPECT_EQ(l.find("->"), std::string::npos) << l;
        }
    EXPECT_TRUE(seen);
}

TEST_F(Batch8Test, U19_NoLegalTargetAtFinalizationSaysSo) {
    // Nami paid, no enemy unit: her required target is missing as the
    // ability is finalized.
    const auto nami = addToHand(P1, kNamiHeadstrong);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Calm)] = 1;
    std::vector<std::string> lines;
    auto conn = events.on_log.connect([&](const LogEvent& e) {
        if (e.message.find("TGT_NOT_OFFERED") != std::string::npos) lines.push_back(e.message);
    });
    EnginePathAgent p1(EnginePathAgent::script({1, EnginePathAgent::kRest}));
    playThroughEngine(nami, &p1);
    conn.disconnect();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_NE(lines[0].find("as it is finalized"), std::string::npos) << lines[0];
}

// X3 (found by batch 8): a countered spell Fizz played keeps no "recycle me"
// mark in the trash, where a later play of it would have been recycled.
TEST_F(Batch8Test, X3_ACounteredSpellLeavesItsRecycleMarkBehind) {
    const auto wages = addToHand(P1, kWagesOfPain);
    o(wages).card_counters["__recycle_on_leave_chain"] = 1;
    withEngine([&](GameEngine& e) {
        CardContext ctx{e.mutableState(), events, e.testHook_executor(), P2, kInvalidId};
        disposeCounteredCard(ctx, wages);
    });
    EXPECT_TRUE(inTrash(P1, wages));
    EXPECT_EQ(o(wages).card_counters.count("__recycle_on_leave_chain"), 0u);
}

// The same class as U14 / R13: an object picked for a cost, or a card picked
// from a hand, is not a target (CR 355.10.a, 355.10.c).
TEST_F(Batch8Test, U14_TheOtherCostAndHandPicksAreNotTargets) {
    // Heedless Resurrection: "As an additional cost to play this, kill a
    // friendly unit."
    {
        const CardDefId kHeedless = 704;
        const auto a = addUnit(P1, kInvalidId, 2, 0);
        const auto b = addUnit(P1, kInvalidId, 3, 0);
        const auto spell = addToHand(P1, kHeedless);
        class Kinds : public EnginePathAgent {
        public:
            std::vector<PromptKind> kinds;
            Intent decide(const GameState& st, const std::vector<Intent>& legal,
                          const Prompt& prompt) override {
                if (!legal.empty() && legal.front().type == IntentType::MakeChoice)
                    kinds.push_back(prompt.kind);
                return selectAction(st, legal);
            }
        } p1;
        playThroughEngine(spell, &p1);
        ASSERT_FALSE(p1.kinds.empty());
        EXPECT_EQ(p1.kinds.front(), PromptKind::ChooseObject);
        EXPECT_NE(o(a).location.has_value(), o(b).location.has_value()) << "one paid the cost";
    }
}


// ═══ S1: Thermo Beam's gear die together ═══════════════════════════════════
//
// "Kill all gear." They die at the same time (CR 370.1.a.2): one death batch,
// their death abilities one simultaneous batch, ordered by their controller
// (CR 383.3.d). They were killed one after another, each its own batch.
TEST_F(Batch8Test, S1_ThermoBeamsGearDieTogetherAndTheirAbilitiesAreOrderedTogether) {
    addGear(P1, kScrapheap);
    addGear(P1, kSpriteFountain);
    for (int i = 0; i < 4; ++i) addToDeck(P1, kPoutyPoro);
    const auto spell = addToHand(P1, kThermoBeam);
    EnginePathAgent p1, p2;
    playThroughEngine(spell, &p1, std::nullopt, &p2);
    EXPECT_EQ(p1.order_prompts.size(), 1u)
        << "Scrapheap's and Sprite Fountain's death abilities: one batch, P1 orders them";
}


// ═══ Q3: Reckoner's Arena's activated conquer abilities are simultaneous ════
//
// CR 383.4.g.1: each conquer ability "is placed on the chain as if it had just
// triggered" — all of them by the one instruction, so they are simultaneous
// triggered abilities their controller orders (CR 383.3.d). They were queued
// one by one with no trigger batch and no order prompt.
TEST_F(Batch8Test, Q3_ReckonersArenaAsksTheOrderOfTheConquerAbilitiesItActivates) {
    const auto arena = placeBattlefield(0, kReckonersArena);
    state.battlefields[0].controller = P1;
    const auto sett = addUnit(P1, kSettBrawler, 0, 0);
    const auto lucian = addUnit(P1, kLucianMerciless, 0, 0);
    o(lucian).is_exhausted = true;
    EnginePathAgent p1, p2;
    withEngine([&](GameEngine& e) {
        e.testHook_executor().queueTriggeredAbility(arena, P1, TriggerType::WhenYouHoldHere,
                                                    kInvalidId);
        settleLikeTheEngine(e);
    }, &p1, &p2);
    EXPECT_EQ(p1.order_prompts.size(), 1u) << "P1 orders Sett's and Lucian's conquer abilities";
    EXPECT_EQ(o(sett).buff_count, 1);
    EXPECT_FALSE(o(lucian).is_exhausted);
}


// X2 (found by batch 8): Altar of Memories' "put a card from your hand ..."
// was asked as a Target; a card picked from a hand is not one (CR 355.10.a),
// the class batch 6 (R13) fixed in ten other cards.
TEST_F(Batch8Test, X2_AltarOfMemoriesPicksTheHandCardAsACardNotATarget) {
    const CardDefId kAltarOfMemories = 490;
    const auto altar = addGear(P1, kAltarOfMemories);
    const auto dead = addUnit(P1, kInvalidId, 2, 0);
    addToHand(P1, kPoutyPoro);
    addToHand(P1, kPoutyPoro);
    for (int i = 0; i < 3; ++i) addToDeck(P1, kPoutyPoro);
    class Kinds : public EnginePathAgent {
    public:
        using EnginePathAgent::EnginePathAgent;
        std::vector<PromptKind> kinds;
        Intent decide(const GameState& st, const std::vector<Intent>& legal,
                      const Prompt& prompt) override {
            if (!legal.empty() && legal.front().type == IntentType::MakeChoice)
                kinds.push_back(prompt.kind);
            return selectAction(st, legal);
        }
    } p1(EnginePathAgent::accepting());
    fireTriggerThroughEngine(altar, TriggerType::WhenAFriendlyUnitDies, &p1, dead);
    EXPECT_NE(std::find(p1.kinds.begin(), p1.kinds.end(), PromptKind::ChooseObject), p1.kinds.end());
    EXPECT_EQ(std::find(p1.kinds.begin(), p1.kinds.end(), PromptKind::Target), p1.kinds.end());
}

}  // namespace riftbound::test::batch_8
