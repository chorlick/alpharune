/// @file test_review_fix_ledger.cpp
/// Tests from the 2026-10 engine review, stage 3, stream sweep-verify-ledger:
///   - plans/08 item 8.8: a def carries the keywords the CARD has, not the
///     ones its text gives to something else or that belong to one of its
///     abilities;
///   - plans/09 item 9.C4: Blitzcrank 67, Mischievous Marai 562 and Shadow 752
///     print no permission to be played to another player's battlefield.
/// The behavioural tests drive a real GameEngine (generators, canAfford, the
/// aura pass); the data tests read the defs the card classes return.

#include "card_test_fixture.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_ledger {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kCleave = 4;
constexpr CardDefId kSealOfRage = 40;
constexpr CardDefId kBlitzcrankImpassive = 67;
constexpr CardDefId kShenKinkou = 241;
constexpr CardDefId kLongSword = 345;
constexpr CardDefId kInferna = 560;
constexpr CardDefId kMischievousMarai = 562;
constexpr CardDefId kGoldToken = 564;
constexpr CardDefId kSumpworksMap = 647;
constexpr CardDefId kShadow = 752;
constexpr CardDefId kMysticVortex = 947;

class LedgerTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    EnginePathAgent a1_, a2_;

    GameState& s() { return engine_->mutableState(); }

    /// An engine in P1's main phase, Neutral Open, two battlefields, a blank
    /// legend each.
    void makeEngine() {
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& st = s();
        st.mode = ModeOfPlay{};
        st.players[0].id = P1;
        st.players[1].id = P2;
        for (int i = 0; i < 2; ++i) {
            BattlefieldState bf;
            bf.id = static_cast<int>(st.battlefields.size());
            st.battlefields.push_back(bf);
        }
        engine_->testHook_setAgents(&a1_, &a2_);
        st.turn.turn_player = P1;
        st.turn.turn_number = 3;
        st.turn.phase = TurnPhase::MainPhase;
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Open;
        for (PlayerId p : {P1, P2}) {
            auto id = st.createObject();
            auto& lo = st.getObject(id);
            lo.owner = lo.controller = p;
            lo.card_type = CardType::Legend;
            lo.zone = ZoneType::LegendZone;
            st.player(p).legend_zone = id;
        }
    }

    /// A card object built the way GameEngine::instantiateCard builds one.
    GameObjectId makeCard(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        const auto& def = card_db.get(def_id);
        c.card_def_id = def_id;
        c.owner = c.controller = p;
        c.name = def.name;
        c.card_type = def.card_type;
        c.super_type = def.super_type;
        c.tags = def.tags;
        c.domains = def.domains;
        c.base_might = c.current_might = def.might;
        c.keywords = def.keywords;
        c.assault_value = def.assault_value;
        c.shield_value = def.shield_value;
        c.deflect_value = def.deflect_value;
        c.might_bonus = def.might_bonus;
        return id;
    }
    GameObjectId inHand(PlayerId p, CardDefId def_id) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId onBoard(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = makeCard(p, def_id);
        auto& c = s().getObject(id);
        if (at_bf >= 0) {
            c.zone = ZoneType::BattlefieldZone;
            c.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            c.zone = ZoneType::Base;
            c.location = BaseLocation{p};
        }
        return id;
    }
    /// A plain 2 [M] unit with no card behind it.
    GameObjectId plainUnit(PlayerId p, int at_bf) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        c.owner = c.controller = p;
        c.card_type = CardType::Unit;
        c.name = "Plain Unit";
        c.base_might = c.current_might = 2;
        c.zone = ZoneType::BattlefieldZone;
        c.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        return id;
    }
    void setBattlefieldCard(int bf, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        c.card_def_id = def_id;
        c.card_type = CardType::Battlefield;
        c.name = card_db.get(def_id).name;
        c.owner = c.controller = PlayerId::None;
        s().battlefields[bf].card_object_id = id;
    }
    void enterShowdown(PlayerId focus, int at_bf) {
        s().turn.ns_state = NeutralShowdownState::Showdown;
        s().turn.oc_state = OpenClosedState::Open;
        s().turn.focus_holder = focus;
        s().turn.priority_holder = focus;
        s().battlefields[at_bf].showdown_in_progress = true;
    }
    /// A spell of `def_id` on the chain for `p`; the state is Closed and
    /// `holder` has priority.
    void spellOnChain(PlayerId p, CardDefId def_id, PlayerId holder) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::Chain;
        ChainItem item;
        item.id = s().chain.next_id++;
        item.source = id;
        item.controller = p;
        item.card_def_id = def_id;
        item.is_spell = true;
        item.status = ChainItemStatus::Finalized;
        s().chain.items.push_back(item);
        s().turn.oc_state = OpenClosedState::Closed;
        s().turn.priority_holder = holder;
    }
    void pool(PlayerId p, int energy, int any_power) {
        s().player(p).rune_pool.energy = energy;
        s().player(p).rune_pool.universal_power = any_power;
    }
    static std::vector<Intent> playsOf(const std::vector<Intent>& legal, GameObjectId card) {
        std::vector<Intent> out;
        for (const auto& i : legal)
            if ((i.type == IntentType::PlayCard || i.type == IntentType::PlayActionCard ||
                 i.type == IntentType::PlayReaction) && i.card == card) out.push_back(i);
        return out;
    }
    static bool playsTo(const std::vector<Intent>& plays, LocationId loc) {
        return std::any_of(plays.begin(), plays.end(), [&](const Intent& i) {
            return i.play_location.has_value() && *i.play_location == loc; });
    }
    static std::vector<Intent> activationsOf(const std::vector<Intent>& legal, GameObjectId src) {
        std::vector<Intent> out;
        for (const auto& i : legal)
            if ((i.type == IntentType::ActivateAbility ||
                 i.type == IntentType::ActivateActionAbility ||
                 i.type == IntentType::ActivateReactionAbility) && i.ability_source == src)
                out.push_back(i);
        return out;
    }
    bool defHas(CardDefId id, Keyword k) const { return card_db.get(id).keywords.has(k); }
};

// ═══ 08 item 8.8: keywords on the def ═══════════════════════════════════════

// CR 813.1.a / 813.5: [Reaction] on a card and [Reaction] on one of its
// abilities are different characteristics. Keyword::Reaction on a permanent's
// def says the CARD has it: it prints [Reaction] itself (Shen, Kinkou; Rengar,
// Pouncing; Janna, Savior; Sumpworks Map — the cards whose class answers
// hasReactionTiming()), or it has [Quick-Draw] (CR 819.1.b: "Cards with
// Quick-Draw have Reaction inherently"). A Seal's "[E]: [Reaction] — [Add]"
// is an ability's timing (ActivatedAbility::is_reaction), and an [Ambush]
// unit has [Reaction] only while it is being played to a battlefield where
// its controller has units (CR 822.1.b, 813.4), which the engine reads from
// Keyword::Ambush.
TEST_F(LedgerTest, Defs_APermanentHasReactionOnlyWhenTheCardItselfDoes) {
    int reaction_permanents = 0;
    for (CardDefId id : card_registry.classDefIds()) {
        const CardDef& def = card_db.get(id);
        if (def.card_type == CardType::Spell) continue;
        const Card* card = card_registry.get(id);
        ASSERT_NE(card, nullptr);
        const bool card_has_it = card->hasReactionTiming() ||
                                 def.keywords.has(Keyword::QuickDraw);
        EXPECT_EQ(def.keywords.has(Keyword::Reaction), card_has_it)
            << id << " " << def.name << ": " << def.ability_text;
        if (def.keywords.has(Keyword::Reaction)) ++reaction_permanents;
    }
    EXPECT_GE(reaction_permanents, 8) << "241, 348, 376, 647 and the four Quick-Draw gear";
    EXPECT_TRUE(defHas(kShenKinkou, Keyword::Reaction));
    EXPECT_TRUE(defHas(kSumpworksMap, Keyword::Reaction));
    EXPECT_TRUE(defHas(kLongSword, Keyword::Reaction)) << "CR 819.1.b";
    EXPECT_FALSE(defHas(kSealOfRage, Keyword::Reaction));
    EXPECT_FALSE(defHas(kGoldToken, Keyword::Reaction));
    EXPECT_FALSE(defHas(kInferna, Keyword::Reaction));
    EXPECT_TRUE(defHas(kInferna, Keyword::Ambush));
}

// CR 806.1.d: "[Action]" on a spell, "[Action][>]" on an ability. No permanent
// prints [Action] for itself; the six that carried Keyword::Action have an
// [Action] ABILITY (Malzahar 113, Azir 373, Ezreal 404, Forgotten Signpost
// 607, Divining Shells 723, Shadow 752).
TEST_F(LedgerTest, Defs_NoPermanentHasActionForAnAbility) {
    for (CardDefId id : card_registry.classDefIds()) {
        const CardDef& def = card_db.get(id);
        if (def.card_type == CardType::Spell) continue;
        EXPECT_FALSE(def.keywords.has(Keyword::Action))
            << id << " " << def.name << ": " << def.ability_text;
    }
}

// A keyword the text hands to another object ("Give a unit [Assault 3]",
// "Play a Sprite unit token with [Temporary]", "Your Mechs have [Shield]",
// "Units here have [Ganking]") is not a keyword of the card.
TEST_F(LedgerTest, Defs_AKeywordTheTextGivesToSomethingElseIsNotOnTheCard) {
    struct Row { CardDefId id; Keyword kw; const char* why; };
    const Row rows[] = {
        {4,   Keyword::Assault,      "Cleave: Give a unit [Assault 3]"},
        {57,  Keyword::Shield,       "Block: Give a unit [Shield 3] and [Tank]"},
        {57,  Keyword::Tank,         "Block: Give a unit [Shield 3] and [Tank]"},
        {69,  Keyword::Temporary,    "Last Stand: Give it [Temporary]"},
        {94,  Keyword::Temporary,    "Sprite Call: a Sprite token with [Temporary]"},
        {180, Keyword::Temporary,    "Fading Memories: Give a unit or a gear [Temporary]"},
        {325, Keyword::Assault,      "Blood Rush: Give a unit [Assault 2]"},
        {572, Keyword::Assault,      "Vault Breaker: Give a unit [Assault 2] and [Ganking]"},
        {572, Keyword::Ganking,      "Vault Breaker: Give a unit [Assault 2] and [Ganking]"},
        {579, Keyword::Assault,      "Square Up: Give a unit [Assault 4]"},
        {606, Keyword::Deflect,      "Flurry of Feathers: Bird tokens with [Deflect]"},
        {631, Keyword::Temporary,    "Sprite Burst: Sprite tokens with [Temporary]"},
        {632, Keyword::Temporary,    "Turn to Dust: Give a gear [Temporary]"},
        {645, Keyword::Temporary,    "Smoke and Mirrors: If at least one of them has [Temporary]"},
        {727, Keyword::Temporary,    "Shadow's Call: Give it [Temporary]"},
        {757, Keyword::Temporary,    "Mirror Image: Give it [Temporary]"},
        {259, Keyword::Hidden,       "Swift Scout: hide a card with [Hidden]"},
        {262, Keyword::Ganking,      "Bounty Hunter: Give a unit [Ganking]"},
        {502, Keyword::Assault,      "Purifier: Your Equipment each give [Assault]"},
        {512, Keyword::Weaponmaster, "Emperor of the Sands: Your Sand Soldiers have [Weaponmaster]"},
        {551, Keyword::Shield,       "Mechanized Menace: Your Mechs have [Shield]"},
        {749, Keyword::Temporary,    "Bashful Bloom: a Sprite token with [Temporary]"},
        {756, Keyword::Temporary,    "Deceiver: Give it [Temporary]"},
        {274, Keyword::Shield,       "Fortified Position: It gains [Shield 2]"},
        {292, Keyword::Ganking,      "Windswept Hillock: Units here have [Ganking]"},
        {525, Keyword::Repeat,       "Marai Spire: friendly [Repeat] costs cost [1] less"},
        {764, Keyword::Shield,       "Black Flame Altar: Units here with [Temporary] have [Shield]"},
        {764, Keyword::Temporary,    "Black Flame Altar: Units here with [Temporary] have [Shield]"},
        {772, Keyword::Repeat,       "The Academy: give your next spell [Repeat]"},
        {773, Keyword::Deflect,      "Trapping Grounds: a Bird token with [Deflect]"},
        {401, Keyword::Repeat,       "Temporal Portal: Give the next spell [Repeat]"},
        {544, Keyword::Equip,        "Yone, Blademaster: [Equip] is in the Weaponmaster reminder"},
        {746, Keyword::Reaction,     "Bloodharbor Ripper: [Reaction] is in the Gold reminder"},
    };
    for (const auto& r : rows) {
        const CardDef& def = card_db.get(r.id);
        EXPECT_FALSE(def.keywords.has(r.kw))
            << r.id << " " << def.name << " carries " << toString(r.kw) << " — " << r.why;
    }
    // The printed number of a keyword the card does not have is not its own
    // either (GameEngine::instantiateCard copies it onto the card's object).
    for (CardDefId id : {4, 325, 572, 579, 502})
        EXPECT_EQ(card_db.get(id).assault_value, 0) << id << " " << card_db.get(id).name;
    for (CardDefId id : {57, 551, 274, 764})
        EXPECT_EQ(card_db.get(id).shield_value, 0) << id << " " << card_db.get(id).name;
    for (CardDefId id : {606, 773})
        EXPECT_EQ(card_db.get(id).deflect_value, 0) << id << " " << card_db.get(id).name;

    // What these cards do print for themselves stays.
    EXPECT_TRUE(defHas(4, Keyword::Action));
    EXPECT_TRUE(defHas(57, Keyword::Hidden));
    EXPECT_TRUE(defHas(325, Keyword::Repeat));
    EXPECT_TRUE(defHas(579, Keyword::Repeat));
    EXPECT_TRUE(defHas(606, Keyword::Reaction));
    EXPECT_TRUE(defHas(645, Keyword::Hidden));
    EXPECT_TRUE(defHas(544, Keyword::Weaponmaster));
}

// Every keyword, every registered card (plans/09 item 9.N1 asked whether the
// 8.8 checks would have caught Yone, Blademaster 544's `Equip`): a keyword on
// a def must be printed in the card's text OUTSIDE reminder text. "(When you
// play me, you may [Equip] one of your Equipment to me ...)" and "(You may
// play me as a [Reaction] ...)" are reminders of another keyword. The one
// keyword a reminder really confers is [Reaction] on [Quick-Draw] gear
// (CR 819.1.b). This does not see a keyword the rules text hands to something
// else ("Give a unit [Assault 3]"): those are the rows of the test above.
TEST_F(LedgerTest, Defs_AKeywordPrintedOnlyInReminderTextIsNotOnTheCard) {
    auto lower = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    auto withoutReminders = [](const std::string& text) {
        std::string out;
        int depth = 0;
        for (char c : text) {
            if (c == '(') { ++depth; continue; }
            if (c == ')') { if (depth > 0) --depth; continue; }
            if (depth == 0) out.push_back(c);
        }
        return out;
    };
    int keywords_checked = 0;
    for (CardDefId id : card_registry.classDefIds()) {
        const CardDef& def = card_db.get(id);
        const std::string rules = lower(withoutReminders(def.ability_text + "\n" + def.effect_text));
        for (int k = 0; k < static_cast<int>(Keyword::Count); ++k) {
            const Keyword kw = static_cast<Keyword>(k);
            if (!def.keywords.has(kw)) continue;
            if (kw == Keyword::Reaction && def.keywords.has(Keyword::QuickDraw)) continue;
            ++keywords_checked;
            EXPECT_NE(rules.find(lower(std::string(toString(kw)))), std::string::npos)
                << id << " " << def.name << " carries " << toString(kw)
                << ", which its text prints only in reminder text or not at all: "
                << def.ability_text;
        }
    }
    EXPECT_GT(keywords_checked, 500);
    EXPECT_FALSE(defHas(544, Keyword::Equip)) << "Yone, Blademaster: [Equip] is in the "
                                                 "[Weaponmaster] reminder (9.N1)";
}

// The reader named in 8.8: GameEngine::extraAnyPowerCost. Mystic Vortex 947:
// "During showdowns here, cards with [Reaction] cost [A] more to play.
// (Hidden cards have [Reaction].)" A Seal is not a card with [Reaction]: its
// ability is.
TEST_F(LedgerTest, MysticVortex_DoesNotTaxACardWhoseAbilityIsAReaction) {
    makeEngine();
    setBattlefieldCard(0, kMysticVortex);
    plainUnit(P1, 0);
    plainUnit(P2, 0);
    enterShowdown(P1, 0);
    engine_->testHook_recalculateAuras();
    ASSERT_EQ(s().battlefields[0].reaction_tax_power, 1) << "the Vortex published its tax";

    auto seal = inHand(P1, kSealOfRage);          // [A] of Fury... printed cost: 1 Power
    pool(P1, 0, 1);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, seal))
        << "Seal of Rage costs its printed Power; the Vortex taxes [Reaction] cards only";
}

TEST_F(LedgerTest, MysticVortex_StillTaxesReactionQuickDrawAndAmbushCards) {
    makeEngine();
    setBattlefieldCard(0, kMysticVortex);
    plainUnit(P1, 0);
    plainUnit(P2, 0);
    enterShowdown(P1, 0);
    engine_->testHook_recalculateAuras();
    ASSERT_EQ(s().battlefields[0].reaction_tax_power, 1);

    // Long Sword: [Quick-Draw] "(This has [Reaction]...)", CR 819.1.b. [2] + 1 Power.
    auto sword = inHand(P1, kLongSword);
    pool(P1, 2, 1);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, sword)) << "one [A] short of the taxed cost";
    pool(P1, 2, 2);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, sword));

    // Inferna: [Ambush] — [Reaction] while played to a battlefield where you
    // have units (CR 822.1.b), the only way she is played in a showdown. [2].
    auto inferna = inHand(P1, kInferna);
    pool(P1, 2, 0);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, inferna));
    pool(P1, 2, 1);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, inferna));

    // No showdown, no tax.
    s().battlefields[0].showdown_in_progress = false;
    s().turn.ns_state = NeutralShowdownState::Neutral;
    engine_->testHook_recalculateAuras();
    pool(P1, 2, 0);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, inferna));
}

// Nothing that worked through the wrong keyword may break: a [Reaction]
// ability is offered from the ability descriptor (ActivatedAbility::is_reaction),
// not from Keyword::Reaction on the def.
TEST_F(LedgerTest, ReactionAbility_OfASealIsOfferedInAClosedStateWithoutTheKeywordOnTheDef) {
    makeEngine();
    auto seal = onBoard(P1, kSealOfRage);
    auto seal_in_hand = inHand(P1, kSealOfRage);
    pool(P1, 5, 5);
    spellOnChain(P2, kCleave, P1);
    const auto legal = engine_->generateLegalActions();
    const auto acts = activationsOf(legal, seal);
    ASSERT_EQ(acts.size(), 1u) << "[E]: [Reaction] — [Add] [R]";
    EXPECT_EQ(acts.front().type, IntentType::ActivateReactionAbility);
    EXPECT_TRUE(playsOf(legal, seal_in_hand).empty())
        << "the Seal itself is not a [Reaction] card: it cannot be played in a Closed State";
}

TEST_F(LedgerTest, ReactionAbility_OfASealIsOfferedInAShowdown) {
    makeEngine();
    auto seal = onBoard(P1, kSealOfRage);
    plainUnit(P1, 0);
    plainUnit(P2, 0);
    enterShowdown(P1, 0);
    const auto acts = activationsOf(engine_->generateLegalActions(), seal);
    EXPECT_EQ(acts.size(), 1u) << "CR 813.1.b: [Reaction] includes every [Action] permission";
}

// [Ambush] grants its timing from Keyword::Ambush (CR 822.1.b): a battlefield
// where the player has units, in a Closed State and in a showdown — and
// nowhere else at that timing (CR 813.4: she has [Reaction] only while being
// played there).
TEST_F(LedgerTest, Ambush_StillGrantsReactionTimingToABattlefieldWithYourUnits) {
    makeEngine();
    plainUnit(P1, 0);
    auto inferna = inHand(P1, kInferna);
    ASSERT_FALSE(s().getObject(inferna).keywords.has(Keyword::Reaction));
    pool(P1, 5, 5);
    spellOnChain(P2, kCleave, P1);
    auto plays = playsOf(engine_->generateLegalActions(), inferna);
    EXPECT_TRUE(playsTo(plays, LocationId{BattlefieldLocation{0}}));
    EXPECT_FALSE(playsTo(plays, LocationId{BattlefieldLocation{1}})) << "no unit of hers there";
    EXPECT_FALSE(playsTo(plays, LocationId{BaseLocation{P1}}))
        << "to her base she is not a [Reaction] card";
    for (const auto& p : plays) EXPECT_EQ(p.type, IntentType::PlayReaction);
}

TEST_F(LedgerTest, Ambush_StillGrantsItsTimingInAShowdown) {
    makeEngine();
    plainUnit(P1, 0);
    plainUnit(P2, 0);
    auto inferna = inHand(P1, kInferna);
    pool(P1, 5, 5);
    enterShowdown(P1, 0);
    const auto plays = playsOf(engine_->generateLegalActions(), inferna);
    EXPECT_TRUE(playsTo(plays, LocationId{BattlefieldLocation{0}}));
    EXPECT_FALSE(playsTo(plays, LocationId{BaseLocation{P1}}));
}

// A permanent that IS a [Reaction] card keeps its timing (Shen, Kinkou 241).
TEST_F(LedgerTest, ReactionUnit_IsStillOfferedInAClosedState) {
    makeEngine();
    auto shen = inHand(P1, kShenKinkou);
    pool(P1, 20, 20);
    spellOnChain(P2, kCleave, P1);
    const auto plays = playsOf(engine_->generateLegalActions(), shen);
    EXPECT_TRUE(playsTo(plays, LocationId{BaseLocation{P1}}));
}

// ═══ 09 item 9.C4: no play-anywhere permission on 67, 562, 752 ══════════════

// Printed texts (def().ability_text; the same wording in cards/raw/gallery_raw.json
// ogn-067-298, unl-003-219, unl-194-219):
//   Blitzcrank, Impassive: "When you play me to a battlefield, you may move an
//     enemy unit to here."                                — a trigger condition
//   Mischievous Marai: "When you play me to a battlefield, deal 2 to an enemy
//     unit here."                                         — a trigger condition
//   Shadow: "If you play me to a battlefield, I enter ready." — a condition
// None says "You may play me to ..."; a unit is played to its controller's
// base or a battlefield they control (CR 355.2 with 813.3.a's restatement).
// The engine's old text match on "play me to a battlefield" let all three be
// played to any battlefield.
TEST_F(LedgerTest, Pin_9C4_BlitzcrankMaraiAndShadowArePlayedOnlyWhereAnyUnitIs) {
    makeEngine();
    s().battlefields[0].controller = P2;          // enemy-controlled and occupied
    plainUnit(P2, 0);
    // battlefield 1: open (nobody controls it, no units)
    pool(P1, 30, 30);
    for (CardDefId id : {kBlitzcrankImpassive, kMischievousMarai, kShadow}) {
        const Card* card = card_registry.get(id);
        ASSERT_NE(card, nullptr);
        EXPECT_EQ(card->extraPlayLocations(), 0u) << card_db.get(id).name;
        EXPECT_FALSE(card->restrictsPlayLocations()) << card_db.get(id).name;
        EXPECT_EQ(card_db.get(id).ability_text.find("You may play me"), std::string::npos)
            << card_db.get(id).name;
        auto in_hand = inHand(P1, id);
        const auto plays = playsOf(engine_->generateLegalActions(), in_hand);
        EXPECT_TRUE(playsTo(plays, LocationId{BaseLocation{P1}})) << card_db.get(id).name;
        EXPECT_FALSE(playsTo(plays, LocationId{BattlefieldLocation{0}}))
            << card_db.get(id).name << ": an enemy battlefield";
        EXPECT_FALSE(playsTo(plays, LocationId{BattlefieldLocation{1}}))
            << card_db.get(id).name << ": an open battlefield";
    }
}

// ...and each is still played to a battlefield its controller controls, which
// is what its "to a battlefield" text is about.
TEST_F(LedgerTest, Pin_9C4_EachIsStillPlayableToABattlefieldItsControllerControls) {
    makeEngine();
    s().battlefields[1].controller = P1;
    plainUnit(P1, 1);
    pool(P1, 30, 30);
    for (CardDefId id : {kBlitzcrankImpassive, kMischievousMarai, kShadow}) {
        auto in_hand = inHand(P1, id);
        const auto plays = playsOf(engine_->generateLegalActions(), in_hand);
        EXPECT_TRUE(playsTo(plays, LocationId{BattlefieldLocation{1}})) << card_db.get(id).name;
    }
}

}  // namespace
}  // namespace test_review_fix_ledger
}  // namespace riftbound::test
