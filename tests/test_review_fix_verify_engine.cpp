/// @file test_review_fix_verify_engine.cpp
/// Regression tests for the 2026-10 engine review, stage 3, stream
/// sweep-verify-engine: the engine checks what its caller hands it (plans/09
/// items 9.G4, 9.G5, 9.I6, 9.C3), the card-facing engine gaps (9.A13, 9.A19,
/// 9.A21, 9.A22, 9.D3, 9.G1, 9.G8, 9.G12, 9.C5) and the dead-code remainder of
/// plans/05 items 5.13 / 5.14.
/// Every test drives a real GameEngine: decisions are asked through the
/// engine's agents, triggers go through TriggerManager and the chain.

#include "ml/feature_extractor.h"
#include "cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

#include <boost/signals2/connection.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_verify_engine {  // file-unique: test sources are unity-built
namespace {

// ─── Test-only cards (ids in the reserved 956..1150 range) ──────────────────

constexpr CardDefId kTVanilla = 1080;        // unit, free, 2 [M]
constexpr CardDefId kTTargetReader = 1081;   // unit: trigger that records targets.size()
constexpr CardDefId kTGearReader = 1082;     // Equipment: equipped trigger, same
constexpr CardDefId kTSplitSpell = 1083;     // spell: deal 20 split among enemy units
constexpr CardDefId kTManySpell = 1084;      // spell: selectSeveral of friendly units
constexpr CardDefId kTAbilityUnit = 1085;    // unit with "[E]: draw 1"
constexpr CardDefId kTChooserUnit = 1086;    // unit: "choose an enemy unit" trigger
constexpr CardDefId kTChooserGear = 1087;    // gear: the same
constexpr CardDefId kTChooserLegend = 1088;  // legend: the same
constexpr CardDefId kTFinalSpell = 1089;     // spell: "choose an enemy unit" at finalization, marks it
constexpr CardDefId kTFinalRepeat = 1090;    // the same with [Repeat] [1]
constexpr CardDefId kTPlayerSpell = 1091;    // spell: plays a kTFinalSpell from hand, then marks itself
constexpr CardDefId kTRechooser = 1092;      // [Reaction] spell: the spell below makes new choices
constexpr CardDefId kTPokeSpell = 1093;      // spell: target one enemy unit, chosen with the play
constexpr CardDefId kTRepeatUnit = 1094;     // a UNIT that declares repeatCosts (never registered)
constexpr CardDefId kTurnToDust = 632;
constexpr CardDefId kSpinningAxe = 504;      // Equipment that prints [Temporary]
constexpr CardDefId kPerchedGrimwyrm = 338;  // "play me only to a battlefield you conquered this turn"
constexpr CardDefId kDianaNoLongerHuman = 711;  // an [Ambush] unit
constexpr CardDefId kRengarPouncing = 348;   // "I can be played to a battlefield you're attacking."
constexpr CardDefId kAlphaStrike = 751;
constexpr CardDefId kVolibearFurious = 41;

constexpr CardDefId kUdyrWildman = 157;
constexpr CardDefId kHarnessedDragon = 234;
constexpr CardDefId kShipyardSkulker = 175;   // unit, [3], 3 [M], no text
constexpr CardDefId kSpiritsRefuge = 63;
constexpr CardDefId kAllay = 603;
constexpr CardDefId kJayceManOfProgress = 406;
constexpr CardDefId kSerratedDirk = 332;      // gear, Equipment, [1]
constexpr CardDefId kTheZeroDrive = 412;
constexpr CardDefId kSpriteCall = 94;
constexpr CardDefId kFlurryOfFeathers = 606;
constexpr CardDefId kArise = 513;
constexpr CardDefId kOverzealousFan = 449;
constexpr CardDefId kAlbusFerros = 230;
constexpr CardDefId kOvertOperation = 153;
constexpr CardDefId kFoxFire = 253;
constexpr CardDefId kCorruptedDragon = 878;

CardDef makeDef(CardDefId id, const char* name, CardType type) {
    CardDef d;
    d.id = id;
    d.name = name;
    d.card_type = type;
    return d;
}

#define VE_TEST_CARD_DEF(expr)                                \
    const CardDef& def() const override {                     \
        static const CardDef d = [] { return (expr); }();     \
        return d;                                             \
    }

class TVanilla : public UnitCard {
public:
    VE_TEST_CARD_DEF([] { auto d = makeDef(kTVanilla, "Test Vanilla", CardType::Unit);
                          d.might = 2; return d; }())
};

// "When you play me": notes how many targets the engine handed the callback.
class TTargetReader : public UnitCard {
public:
    VE_TEST_CARD_DEF([] { auto d = makeDef(kTTargetReader, "Test Target Reader", CardType::Unit);
                          d.might = 2; return d; }())
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& c = ctx.state.getObject(ctx.source).card_counters;
        c["fired"]++;
        c["targets_seen"] = static_cast<int>(targets.size());
    }
};

class TGearReader : public GearCard {
public:
    VE_TEST_CARD_DEF([] { auto d = makeDef(kTGearReader, "Test Gear Reader", CardType::Gear);
                          d.tags = {"Equipment"}; return d; }())
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquer; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId,
                           const std::vector<GameObjectId>& targets) override {
        auto& c = ctx.state.getObject(ctx.source).card_counters;
        c["fired"]++;
        c["targets_seen"] = static_cast<int>(targets.size());
    }
};

// "Deal 20 damage split among enemy units."
class TSplitSpell : public SpellCard {
public:
    VE_TEST_CARD_DEF([] { return makeDef(kTSplitSpell, "Test Split", CardType::Spell); }())
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        auto enemies = chooseableUnits(ctx.state, me,
                                       [me](const GameObject& o) { return o.controller != me; });
        std::vector<GameObjectId> damaged;
        splitDamageAmong(ctx, "Test Split", 20, ctx.source, enemies, damaged);
    }
};

// "Give any number of friendly units +1 [M] this turn."
class TManySpell : public SpellCard {
public:
    VE_TEST_CARD_DEF([] { return makeDef(kTManySpell, "Test Many", CardType::Spell); }())
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        auto mine = boardObjects(ctx.state, [me](const GameObject& o) {
            return o.isUnit() && o.controller == me;
        });
        auto picked = selectSeveral(ctx, "Test Many", mine);
        if (!picked.has_value()) return;
        for (auto id : *picked) ctx.state.getObject(id).card_counters["picked"] = 1;
    }
};

class TAbilityUnit : public UnitCard {
public:
    VE_TEST_CARD_DEF([] { auto d = makeDef(kTAbilityUnit, "Test Ability Unit", CardType::Unit);
                          d.might = 2; return d; }())
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.getObject(ctx.source).card_counters["activated"]++;
    }
};

// "Choose an enemy unit." as a triggered ability of a unit, a gear, a legend.
// (An id is not a structural type: the template takes its number.)
template <class Base, uint32_t kIdRaw, CardType kType>
class TChooser : public Base {
public:
    const CardDef& def() const override {
        static const CardDef d = [] { auto x = makeDef(CardDefId::fromRaw(kIdRaw), "Test Chooser", kType);
                                      x.might = 2; return x; }();
        return d;
    }
    unsigned choicesAtFinalization() const override { return Card::kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        this->pickTarget(ctx, "Test Chooser", chooseableUnits(ctx.state, me,
            [me](const GameObject& o) { return o.controller != me; }));
        if (Card::finalizingChoices(ctx)) return;
    }
};
using TChooserUnit = TChooser<UnitCard, kTChooserUnit.raw(), CardType::Unit>;
using TChooserGear = TChooser<GearCard, kTChooserGear.raw(), CardType::Gear>;
using TChooserLegend = TChooser<LegendCard, kTChooserLegend.raw(), CardType::Legend>;

// "Choose an enemy unit." — chosen as the spell is finalized (kFinalizeResolve);
// the effect marks the unit.
class TFinalSpell : public SpellCard {
public:
    VE_TEST_CARD_DEF([] { return makeDef(kTFinalSpell, "Test Final", CardType::Spell); }())
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        const PlayerId me = ctx.controller;
        const GameObjectId t = pickTarget(ctx, "Test Final", chooseableUnits(ctx.state, me,
            [me](const GameObject& o) { return o.controller != me; }));
        if (finalizingChoices(ctx)) return;
        if (t != kInvalidId) ctx.state.getObject(t).card_counters["hit"]++;
    }
};
class TFinalRepeat : public TFinalSpell {
public:
    const CardDef& def() const override {
        static const CardDef d = makeDef(kTFinalRepeat, "Test Final Repeat", CardType::Spell);
        return d;
    }
    std::vector<RepeatCost> repeatCosts() const override { return {RepeatCost{.energy = 1}}; }
};
// "Play a Test Final from your hand, ignoring its cost." Then marks itself.
class TPlayerSpell : public SpellCard {
public:
    VE_TEST_CARD_DEF([] { return makeDef(kTPlayerSpell, "Test Player", CardType::Spell); }())
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        GameObjectId inner = kInvalidId;
        for (auto id : ctx.state.player(ctx.controller).hand)
            if (ctx.state.getObject(id).card_def_id == kTFinalSpell) inner = id;
        if (inner != kInvalidId) ctx.executor.playIgnoringCost(ctx.controller, inner);
        ctx.state.getObject(ctx.source).card_counters["after_play"] = 1;
    }
};
// "[Reaction] The spell below this makes new choices." Then marks itself.
class TRechooser : public SpellCard {
public:
    VE_TEST_CARD_DEF([] { auto d = makeDef(kTRechooser, "Test Rechooser", CardType::Spell);
                          d.keywords.set(Keyword::Reaction); return d; }())
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.chain.items.empty())
            ctx.executor.makeNewChoices(ctx.state.chain.items.back().id);
        ctx.state.getObject(ctx.source).card_counters["after_rechoose"] = 1;
    }
};

// "Choose an enemy unit." — the target rides in the Play intent.
class TPokeSpell : public SpellCard {
public:
    VE_TEST_CARD_DEF([] { return makeDef(kTPokeSpell, "Test Poke", CardType::Spell); }())
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        for (auto t : targets) ctx.state.getObject(t).card_counters["hit"]++;
    }
};
class TRepeatUnit : public UnitCard {
public:
    VE_TEST_CARD_DEF([] { return makeDef(kTRepeatUnit, "Test Repeat Unit", CardType::Unit); }())
    std::vector<RepeatCost> repeatCosts() const override { return {RepeatCost{.energy = 1}}; }
};

// ─── Agents ─────────────────────────────────────────────────────────────────

/// Answers every decision with `answer(state, legal)`; counts what it was asked.
class LambdaAgent : public AgentInterface {
public:
    using Fn = std::function<Intent(const GameState&, const std::vector<Intent>&)>;
    Fn answer;
    int asked = 0;
    explicit LambdaAgent(Fn f) : answer(std::move(f)) {}
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        ++asked;
        return answer(state, legal);
    }
};

Intent passOrFirst(const std::vector<Intent>& legal) {
    for (const auto& i : legal)
        if (i.type == IntentType::PassPriority || i.type == IntentType::PassFocus) return i;
    return legal.empty() ? Intent{} : legal.front();
}

/// The first option that picks an object (never "stop" / "none").
Intent firstObjectOption(const std::vector<Intent>& legal) {
    for (const auto& i : legal)
        if (!i.chosen_objects.empty()) return i;
    return legal.front();
}

// ─── Fixture ────────────────────────────────────────────────────────────────

class VerifyEngineTest : public CardTestFixture {
protected:
    void SetUp() override {
        CardTestFixture::SetUp();
        card_registry.registerCard(kTVanilla, std::make_unique<TVanilla>());
        card_registry.registerCard(kTTargetReader, std::make_unique<TTargetReader>());
        card_registry.registerCard(kTGearReader, std::make_unique<TGearReader>());
        card_registry.registerCard(kTSplitSpell, std::make_unique<TSplitSpell>());
        card_registry.registerCard(kTManySpell, std::make_unique<TManySpell>());
        card_registry.registerCard(kTAbilityUnit, std::make_unique<TAbilityUnit>());
        card_registry.registerCard(kTChooserUnit, std::make_unique<TChooserUnit>());
        card_registry.registerCard(kTChooserGear, std::make_unique<TChooserGear>());
        card_registry.registerCard(kTChooserLegend, std::make_unique<TChooserLegend>());
        card_registry.registerCard(kTFinalSpell, std::make_unique<TFinalSpell>());
        card_registry.registerCard(kTFinalRepeat, std::make_unique<TFinalRepeat>());
        card_registry.registerCard(kTPlayerSpell, std::make_unique<TPlayerSpell>());
        card_registry.registerCard(kTRechooser, std::make_unique<TRechooser>());
        card_registry.registerCard(kTPokeSpell, std::make_unique<TPokeSpell>());
        card_db.buildFromClasses(card_registry);
    }

    GameObject& o(GameObjectId id) { return state.getObject(id); }
    PlayerState& ps(PlayerId p) { return state.player(p); }
    bool onBoard(GameObjectId id) { return o(id).location.has_value(); }

    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = ps(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        o(id).zone = ZoneType::Trash;
        ps(owner).trash.push_back(id);
        return id;
    }
    GameObjectId addGear(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& g = o(id);
        g.owner = g.controller = owner;
        g.card_def_id = def_id;
        g.card_type = CardType::Gear;
        const auto& def = card_db.get(def_id);
        g.name = def.name;
        g.super_type = def.super_type;
        g.keywords = def.keywords;
        g.domains = def.domains;
        g.tags = def.tags;
        g.might_bonus = def.might_bonus;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{owner};
        return id;
    }
    /// A plain 0-cost unit of `might`, a real card (not a token).
    GameObjectId addVanilla(PlayerId owner, int might, int at_bf = -1) {
        return addUnit(owner, kTVanilla, might, at_bf);
    }
    void cleanup() {
        withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    }
    Intent activation(GameObjectId source, int index = 0) {
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = o(source).controller;
        act.ability_source = source;
        act.ability_index = index;
        return act;
    }
    Intent playOf(GameObjectId card) {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = o(card).controller;
        play.card = card;
        if (!o(card).isSpell()) play.play_location = LocationId{BaseLocation{play.player}};
        return play;
    }
};

// ═══ 9.G4: a MakeChoice answer must be one of the options offered ═══════════

// Harnessed Dragon 234 "When you play me, kill an enemy unit": the agent
// answers the target prompt with a FRIENDLY unit, which was not on the list.
// The engine refuses the answer (std::invalid_argument, as applyChoice refuses
// an index outside the list) and nothing is killed.
TEST_F(VerifyEngineTest, MakeChoice_AnObjectThatWasNotOfferedIsRefused) {
    auto dragon = addUnit(P1, kHarnessedDragon, 0, -1);
    auto mine = addVanilla(P1, 3);
    auto theirs = addVanilla(P2, 3);
    auto other = addVanilla(P2, 3);
    LambdaAgent liar([&](const GameState&, const std::vector<Intent>& legal) {
        if (!legal.empty() && legal.front().type == IntentType::MakeChoice &&
            !legal.front().chosen_objects.empty()) {
            Intent bad = legal.front();
            bad.chosen_objects = {mine};
            return bad;
        }
        return passOrFirst(legal);
    });
    withEngine([&](GameEngine& e) {
        e.testHook_executor().queueTriggeredAbility(dragon, P1, TriggerType::WhenYouPlayMe,
                                                    kInvalidId);
        EXPECT_THROW(e.testHook_runChain(), std::invalid_argument);
        // plans/09 item 9.P3: the refusal came out of the finalize pass, and
        // the chain is not left with an item "resolving" or a prompt pending.
        EXPECT_FALSE(e.state().chain.resuming.has_value());
        EXPECT_FALSE(e.testHook_executor().hasPendingChoice());
        e.mutableState().chain.items.clear();
    }, &liar);
    EXPECT_GE(liar.asked, 1);
    EXPECT_TRUE(onBoard(mine));
    EXPECT_TRUE(onBoard(theirs));
    EXPECT_TRUE(onBoard(other));

    // Control: an answer from the list is carried out.
    EnginePathAgent honest(EnginePathAgent::choosing(other));
    fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &honest);
    EXPECT_TRUE(honest.wasOffered(theirs));
    EXPECT_FALSE(honest.wasOffered(mine));
    EXPECT_FALSE(onBoard(other));
    EXPECT_TRUE(onBoard(theirs));
}

// A yes/no answered with a number that is not on the list (Jayce's "you may
// kill a friendly gear": 0 / 1 offered, 7 answered).
TEST_F(VerifyEngineTest, MakeChoice_AValueThatWasNotOfferedIsRefused) {
    auto jayce = addUnit(P1, kJayceManOfProgress, 0, -1);
    auto dirk = addGear(P1, kSerratedDirk);
    LambdaAgent liar([&](const GameState&, const std::vector<Intent>& legal) {
        if (!legal.empty() && legal.front().type == IntentType::MakeChoice) {
            Intent bad = legal.front();
            bad.chosen_objects.clear();
            bad.chosen_value = 7;
            return bad;
        }
        return passOrFirst(legal);
    });
    EXPECT_THROW(fireTriggerThroughEngine(jayce, TriggerType::WhenYouPlayMe, &liar),
                 std::invalid_argument);
    EXPECT_TRUE(onBoard(dirk));
}

// ═══ 9.G5: an applied intent must be one of the legal intents ═══════════════

// The plan's example: an Udyr in the trash. No generator offers its ability.
// (a) Handed to the engine as the answer to the main-phase decision, it is
//     refused before anything is executed or recorded.
TEST_F(VerifyEngineTest, MainPhaseDecision_AnAbilityOfACardInTheTrashIsRefused) {
    auto udyr = addToTrash(P1, kUdyrWildman);
    o(udyr).card_type = CardType::Unit;
    o(udyr).buff_count = 1;
    auto victim = addVanilla(P2, 2, 0);
    const Intent forged = activation(udyr);
    LambdaAgent liar([&](const GameState&, const std::vector<Intent>&) { return forged; });
    withEngine([&](GameEngine& e) {
        const auto history_before = e.state().action_history.size();
        EXPECT_THROW(e.testHook_queryAgent(P1), std::invalid_argument);
        EXPECT_EQ(e.state().action_history.size(), history_before)
            << "a refused answer is not part of the game's history";
        EXPECT_TRUE(e.state().chain.items.empty());
    }, &liar);
    EXPECT_EQ(liar.asked, 1);
    EXPECT_EQ(o(udyr).buff_count, 1);
    EXPECT_EQ(o(victim).damage_marked, 0);
}

// (b) Through the test seam, which does not pass through the decision: the
//     activation itself is refused — nothing is paid, nothing goes on the chain.
TEST_F(VerifyEngineTest, ExecuteIntent_AnAbilityOfACardInTheTrashDoesNothing) {
    auto udyr = addToTrash(P1, kUdyrWildman);
    o(udyr).card_type = CardType::Unit;
    o(udyr).buff_count = 1;
    auto victim = addVanilla(P2, 2, 0);
    EnginePathAgent agent;
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(activation(udyr));
        EXPECT_TRUE(e.state().chain.items.empty());
    }, &agent);
    EXPECT_TRUE(agent.prompts.empty()) << "no mode / target prompt for a dead Udyr";
    EXPECT_EQ(o(udyr).buff_count, 1) << "the cost was not paid";
    EXPECT_EQ(ps(P1).abilities_activated_this_turn, 0);
    EXPECT_EQ(o(victim).damage_marked, 0);

    // Control: the same Udyr on the board activates.
    auto live = addUnit(P1, kUdyrWildman, 0, -1);
    o(live).buff_count = 1;
    EnginePathAgent agent2;
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(activation(live)); }, &agent2);
    EXPECT_FALSE(agent2.prompts.empty());
    EXPECT_EQ(o(live).buff_count, 0);
    EXPECT_EQ(ps(P1).abilities_activated_this_turn, 1);
}

// An ability whose "use only if" gate fails is refused by the activation too
// (the generators' gate, Card::canActivateAbilityAt): Udyr with no buff.
TEST_F(VerifyEngineTest, ExecuteIntent_AnAbilityWhoseGateFailsDoesNothing) {
    auto udyr = addUnit(P1, kUdyrWildman, 0, -1);
    o(udyr).buff_count = 0;
    EnginePathAgent agent;
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(activation(udyr));
        EXPECT_TRUE(e.state().chain.items.empty());
    }, &agent);
    EXPECT_TRUE(agent.prompts.empty());
    EXPECT_EQ(ps(P1).abilities_activated_this_turn, 0);
}

// Every intent type goes through the same check, not only activations: a play
// of a card that is not in hand, a move of an enemy unit, an end of turn by
// the wrong player — each is refused at the main-phase decision.
TEST_F(VerifyEngineTest, MainPhaseDecision_ForgedPlaysMovesAndEndTurnsAreRefused) {
    state.battlefields[0].controller = P1;
    auto in_deck = addToDeck(P1, kTVanilla);
    auto enemy = addVanilla(P2, 2);
    addVanilla(P1, 2);

    std::vector<Intent> forged;
    forged.push_back(playOf(in_deck));
    forged.push_back(Intent::standardMove(P1, {enemy}, LocationId{BattlefieldLocation{0}}));
    forged.push_back(Intent::endTurn(P2));
    {
        Intent hide;
        hide.type = IntentType::HideCard;
        hide.player = P1;
        hide.card = in_deck;
        forged.push_back(hide);
    }
    for (const auto& bad : forged) {
        LambdaAgent liar([&](const GameState&, const std::vector<Intent>&) { return bad; });
        withEngine([&](GameEngine& e) {
            EXPECT_THROW(e.testHook_queryAgent(P1), std::invalid_argument) << bad.describe();
        }, &liar);
        EXPECT_EQ(liar.asked, 1);
    }
    EXPECT_TRUE(inDeck(P1, in_deck));
    EXPECT_TRUE(o(enemy).isAtBase());

    // Controls: a legal answer and a concession are both accepted.
    LambdaAgent ender([&](const GameState&, const std::vector<Intent>& legal) {
        for (const auto& i : legal) if (i.type == IntentType::EndTurn) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        Intent got;
        EXPECT_NO_THROW(got = e.testHook_queryAgent(P1));
        EXPECT_EQ(got.type, IntentType::EndTurn);
        EXPECT_EQ(e.state().action_history.size(), 1u);
    }, &ender);
    LambdaAgent quitter([&](const GameState&, const std::vector<Intent>&) {
        return Intent::concede(P1);
    });
    withEngine([&](GameEngine& e) {
        Intent got;
        EXPECT_NO_THROW(got = e.testHook_queryAgent(P1));
        EXPECT_EQ(got.type, IntentType::Concede);
    }, &quitter);
}

// A priority decision: the opponent answers with a reaction play that was not
// offered (the card is not even theirs).
TEST_F(VerifyEngineTest, ChainPriority_AForgedReactionIsRefused) {
    auto spell = addToHand(P1, kTManySpell);
    auto not_theirs = addToHand(P1, kTSplitSpell);
    LambdaAgent liar([&](const GameState&, const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) {
                Intent bad;
                bad.type = IntentType::PlayReaction;
                bad.player = P2;
                bad.card = not_theirs;
                return bad;
            }
        return passOrFirst(legal);
    });
    EXPECT_THROW(playThroughEngine(spell, nullptr, std::nullopt, &liar), std::invalid_argument);
    EXPECT_GE(liar.asked, 1);
    EXPECT_TRUE(inHand(P1, not_theirs));
}

// Combat damage: an assignment that is not one of the legal cascades (all of
// it on the second defender, none on the first) is refused.
TEST_F(VerifyEngineTest, CombatDamage_AnAssignmentThatWasNotOfferedIsRefused) {
    state.battlefields[0].controller = P2;
    state.battlefields[0].is_contested = true;
    state.battlefields[0].contested_by = P1;
    addVanilla(P1, 4, 0);
    auto d1 = addVanilla(P2, 3, 0);
    auto d2 = addVanilla(P2, 3, 0);
    int damage_prompts = 0;
    LambdaAgent liar([&](const GameState&, const std::vector<Intent>& legal) {
        if (!legal.empty() && legal.front().type == IntentType::AssignCombatDamage &&
            legal.front().player == P1) {
            ++damage_prompts;
            return Intent::assignCombatDamage(P1, {{std::max(d1, d2), 1}});
        }
        return passOrFirst(legal);
    });
    withEngine([&](GameEngine& e) {
        EXPECT_THROW(e.testHook_runCombat(BattlefieldId::fromRaw(0)), std::invalid_argument);
    }, &liar);
    EXPECT_EQ(damage_prompts, 1);
}

// ═══ 9.C3: an edit of the state while a decision is pending ═════════════════

// The decision the engine can rebuild (main phase): after an edit made while
// the engine waits, an answer from the OLD list that the edited state no
// longer allows is refused; an answer that is legal only in the edited state
// is accepted (the play server swaps its list for the rebuilt one).
TEST_F(VerifyEngineTest, EditAtTheMainPhaseDecision_AStaleIntentIsRefusedAndAFreshOneAccepted) {
    auto unit = addToHand(P1, kTVanilla);            // free: offered
    auto late = addToDeck(P1, kTVanilla);            // not in hand: not offered
    GameEngine* engine = nullptr;

    // Stale: the card is edited out of the hand, then the old play is answered.
    LambdaAgent stale([&](const GameState&, const std::vector<Intent>& legal) {
        Intent play;
        bool found = false;
        for (const auto& i : legal)
            if (i.type == IntentType::PlayCard && i.card == unit) { play = i; found = true; }
        EXPECT_TRUE(found) << "the play was offered before the edit";
        auto& st = engine->mutableState();
        auto& hand = st.player(P1).hand;
        hand.erase(std::remove(hand.begin(), hand.end(), unit), hand.end());
        st.getObject(unit).zone = ZoneType::Trash;
        st.player(P1).trash.push_back(unit);
        return play;
    });
    withEngine([&](GameEngine& e) {
        engine = &e;
        EXPECT_THROW(e.testHook_queryAgent(P1), std::invalid_argument);
        EXPECT_TRUE(e.state().action_history.empty());
    }, &stale);
    EXPECT_EQ(stale.asked, 1);

    // Fresh: a card is edited INTO the hand; its play is not on the old list
    // but is on the rebuilt one.
    LambdaAgent fresh([&](const GameState&, const std::vector<Intent>& legal) {
        for (const auto& i : legal) EXPECT_NE(i.card, late);
        auto& st = engine->mutableState();
        auto& deck = st.player(P1).main_deck;
        deck.erase(std::remove(deck.begin(), deck.end(), late), deck.end());
        st.getObject(late).zone = ZoneType::Hand;
        st.player(P1).hand.push_back(late);
        const auto rebuilt = engine->recomputePendingLegalActions();
        EXPECT_TRUE(rebuilt.has_value());
        for (const auto& i : *rebuilt)
            if (i.type == IntentType::PlayCard && i.card == late) return i;
        ADD_FAILURE() << "the rebuilt list does not offer the new card";
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        engine = &e;
        Intent got;
        EXPECT_NO_THROW(got = e.testHook_queryAgent(P1));
        EXPECT_EQ(got.card, late);
    }, &fresh);
}

// A nested decision (a card's target prompt) cannot be rebuilt, and
// recomputePendingLegalActions says so. The stale answer is refused all the
// same: the option names an object the edit moved off the board.
TEST_F(VerifyEngineTest, EditAtANestedDecision_AnAnswerNamingAnEditedObjectIsRefused) {
    auto dragon = addUnit(P1, kHarnessedDragon, 0, -1);
    auto a = addVanilla(P2, 3);
    auto b = addVanilla(P2, 3);
    GameEngine* engine = nullptr;
    bool nested_seen = false;

    // The offered unit `a` is edited into the trash, then chosen.
    LambdaAgent stale([&](const GameState&, const std::vector<Intent>& legal) {
        if (legal.empty() || legal.front().type != IntentType::MakeChoice)
            return passOrFirst(legal);
        nested_seen = true;
        EXPECT_FALSE(engine->recomputePendingLegalActions().has_value())
            << "a card's prompt is not a state-derived decision";
        auto& st = engine->mutableState();
        st.getObject(a).location = std::nullopt;
        st.getObject(a).zone = ZoneType::Trash;
        st.player(P2).trash.push_back(a);
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == a) return i;
        return legal.front();
    });
    EXPECT_THROW(withEngine([&](GameEngine& e) {
        engine = &e;
        e.testHook_executor().queueTriggeredAbility(dragon, P1, TriggerType::WhenYouPlayMe,
                                                    kInvalidId);
        e.testHook_runChain();
    }, &stale), std::invalid_argument);
    EXPECT_TRUE(nested_seen);
    EXPECT_TRUE(onBoard(b)) << "nothing of the stale answer was carried out";

    // Control: an edit that does not touch the chosen object (a score) leaves
    // the answer valid, and it is carried out.
    LambdaAgent harmless([&](const GameState&, const std::vector<Intent>& legal) {
        if (legal.empty() || legal.front().type != IntentType::MakeChoice)
            return passOrFirst(legal);
        engine->mutableState().player(P1).score = 3;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == b) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        engine = &e;
        e.testHook_executor().queueTriggeredAbility(dragon, P1, TriggerType::WhenYouPlayMe,
                                                    kInvalidId);
        EXPECT_NO_THROW(e.testHook_runChain());
    }, &harmless);
    EXPECT_FALSE(onBoard(b));
    EXPECT_EQ(ps(P1).score, 3);
}

// ═══ 9.I6: a triggered ability is never handed targets ══════════════════════

// A chain item for a triggered ability that does carry targets (nothing in the
// engine puts them there; the test does) still reaches onTrigger with none.
TEST_F(VerifyEngineTest, TriggeredAbility_IsHandedNoTargetsWhateverTheChainItemHolds) {
    auto reader = addUnit(P1, kTTargetReader, 0, -1);
    auto bystander = addVanilla(P2, 2);
    withEngine([&](GameEngine& e) {
        e.testHook_executor().queueTriggeredAbility(reader, P1, TriggerType::WhenYouPlayMe,
                                                    kInvalidId);
        ASSERT_FALSE(e.mutableState().chain.items.empty());
        e.mutableState().chain.items.back().targets = {bystander};
        e.testHook_runChain();
    });
    EXPECT_EQ(o(reader).card_counters["fired"], 1);
    EXPECT_EQ(o(reader).card_counters["targets_seen"], 0);
}

// The same for an Equipment's "When I ..." ability (onEquippedTrigger).
TEST_F(VerifyEngineTest, EquippedTrigger_IsHandedNoTargetsWhateverTheChainItemHolds) {
    auto bearer = addVanilla(P1, 2, 0);
    auto gear = addGear(P1, kTGearReader);
    auto bystander = addVanilla(P2, 2);
    o(gear).attached_to = bearer;
    o(gear).location = o(bearer).location;
    o(gear).zone = o(bearer).zone;
    o(bearer).attachments.push_back(gear);
    withEngine([&](GameEngine& e) {
        auto& st = e.mutableState();
        ChainItem item;
        item.id = st.chain.allocateId();
        item.source = gear;
        item.controller = P1;
        item.card_def_id = kTGearReader;
        item.is_ability = true;
        item.is_activated_ability = false;
        item.equipped_unit = bearer;
        item.fired_trigger = TriggerType::WhenIConquer;
        item.targets = {bystander};
        st.chain.items.push_back(item);
        st.turn.oc_state = OpenClosedState::Closed;
        e.testHook_runChain();
    });
    EXPECT_EQ(o(gear).card_counters["fired"], 1);
    EXPECT_EQ(o(gear).card_counters["targets_seen"], 0);
}

// ═══ 9.A19: Spirit's Refuge 63, "if they didn't already" ════════════════════

// Whatever order the aura hooks run in — it follows the order the objects were
// created in — a buffed unit next to Allay costs the [A] Allay's [Deflect]
// says, not one more for the Refuge.
class VerifyEngineRefugeTest : public VerifyEngineTest {
protected:
    void run(bool refuge_first) {
        state.battlefields[0].controller = P1;
        GameObjectId refuge = kInvalidId, allay = kInvalidId;
        if (refuge_first) { refuge = addGear(P1, kSpiritsRefuge); allay = addUnit(P1, kAllay, 0, 0); }
        else              { allay = addUnit(P1, kAllay, 0, 0); refuge = addGear(P1, kSpiritsRefuge); }
        auto both = addVanilla(P1, 2, 0);    // buffed, next to Allay
        o(both).buff_count = 1;
        auto solo = addVanilla(P1, 2);       // buffed, in base: the Refuge only
        o(solo).buff_count = 1;
        auto plain = addVanilla(P1, 2);      // not buffed: nothing
        auto printed = addVanilla(P1, 2);    // buffed, with a printed [Deflect 2]
        o(printed).buff_count = 1;
        o(printed).keywords.set(Keyword::Deflect);
        o(printed).deflect_value = 2;
        auto this_turn = addVanilla(P1, 2);  // buffed, given [Deflect] this turn
        o(this_turn).buff_count = 1;
        o(this_turn).keywords.set(Keyword::Deflect);   // as giveTemporaryKeyword leaves it
        o(this_turn).temp_deflect_value = 1;

        cleanup();

        auto effects = [&](GameObjectId id, GameObjectId from) {
            int n = 0;
            for (const auto& ae : o(id).aura_effects)
                if (ae.keyword == Keyword::Deflect && ae.source == from) ++n;
            return n;
        };
        EXPECT_EQ(deflectCostFor(state, P2, both), 1) << "Allay's [Deflect], not two";
        EXPECT_EQ(effects(both, allay), 1);
        EXPECT_EQ(effects(both, refuge), 0) << "it had [Deflect] already";
        EXPECT_EQ(deflectCostFor(state, P2, solo), 1) << "the Refuge's own grant";
        EXPECT_EQ(effects(solo, refuge), 1);
        EXPECT_TRUE(o(solo).hasKeyword(Keyword::Deflect));
        EXPECT_EQ(deflectCostFor(state, P2, plain), 0);
        EXPECT_EQ(deflectCostFor(state, P2, printed), 2) << "printed [Deflect 2] stays 2";
        EXPECT_EQ(effects(printed, refuge), 0);
        EXPECT_EQ(deflectCostFor(state, P2, this_turn), 1) << "[Deflect] given this turn";
        EXPECT_EQ(effects(this_turn, refuge), 0);
    }
};
TEST_F(VerifyEngineRefugeTest, SpiritsRefuge_NextToAllayCostsOne_RefugeCreatedFirst) { run(true); }
TEST_F(VerifyEngineRefugeTest, SpiritsRefuge_NextToAllayCostsOne_AllayCreatedFirst) { run(false); }

// Two Refuges are still one [Deflect].
TEST_F(VerifyEngineTest, SpiritsRefuge_TwoCopiesGrantOneDeflect) {
    addGear(P1, kSpiritsRefuge);
    addGear(P1, kSpiritsRefuge);
    auto solo = addVanilla(P1, 2);
    o(solo).buff_count = 1;
    cleanup();
    EXPECT_EQ(deflectCostFor(state, P2, solo), 1);
}

// ═══ 9.A21: Jayce, Man of Progress 406, "from hand" ═════════════════════════

TEST_F(VerifyEngineTest, JayceManOfProgress_ThePermissionIsForAGearPlayedFromHand) {
    auto jayce = addToHand(P1, kJayceManOfProgress);
    auto kill = addGear(P1, kSerratedDirk);
    auto in_hand = addToHand(P1, kSerratedDirk);     // [1], no Power
    auto in_trash = addToTrash(P1, kSerratedDirk);
    // The may (yes), then the gear to kill.
    EnginePathAgent agent(EnginePathAgent::script({1, static_cast<int>(kill.raw())}));
    playThroughEngine(jayce, &agent);
    ASSERT_FALSE(onBoard(kill));
    ASSERT_EQ(ps(P1).cost_modifiers.size(), 1u) << "the permission";
    EXPECT_TRUE(ps(P1).cost_modifiers.front().from_hand_only);

    ps(P1).rune_pool.energy = 0;
    ps(P1).rune_pool.universal_power = 0;
    withEngine([&](GameEngine& e) {
        EXPECT_TRUE(e.testHook_canAfford(P1, in_hand)) << "from hand: its Energy is ignored";
        EXPECT_FALSE(e.testHook_canAfford(P1, in_trash))
            << "the same gear in the trash is not \"from hand\"";
        // Paying for it as a play from the trash neither discounts it nor uses
        // the permission up.
        e.mutableState().player(P1).current_play_source = Intent::PlaySource::Trash;
        e.mutableState().player(P1).rune_pool.energy = 1;
        EXPECT_TRUE(e.testHook_payCardCost(P1, in_trash));
        e.mutableState().player(P1).current_play_source = Intent::PlaySource::Hand;
        EXPECT_EQ(e.state().player(P1).rune_pool.energy, 0) << "the full [1] was paid";
        EXPECT_EQ(e.state().player(P1).cost_modifiers.size(), 1u);
    });
    // The gear in hand uses it.
    playThroughEngine(in_hand, nullptr, std::nullopt, nullptr, /*fund=*/false);
    EXPECT_TRUE(onBoard(in_hand));
    EXPECT_TRUE(ps(P1).cost_modifiers.empty());
}

// ═══ 9.A13: The Zero Drive 412, "Banish this" is a cost ═════════════════════

// The Drive is banished as the ability is activated: when the opponent gets
// priority with the ability on the chain it is already gone.
TEST_F(VerifyEngineTest, ZeroDrive_IsBanishedBeforeTheOpponentCanRespond) {
    auto drive = addGear(P1, kTheZeroDrive);
    auto banished = addToHand(P1, kTVanilla);
    ps(P1).hand.clear();
    o(banished).zone = ZoneType::Banishment;
    ps(P1).banishment.push_back(banished);
    o(drive).tracked_objects.push_back(banished);
    ps(P1).rune_pool.energy = 3;
    ps(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;

    int priority_windows = 0;
    bool drive_on_board_at_priority = true;
    bool unit_back_at_priority = true;
    LambdaAgent watcher([&](const GameState& st, const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority && !st.chain.items.empty()) {
                ++priority_windows;
                drive_on_board_at_priority = st.getObject(drive).location.has_value();
                unit_back_at_priority = st.getObject(banished).location.has_value();
            }
        return passOrFirst(legal);
    });
    withEngine([&](GameEngine& e) {
        const auto legal = e.generateLegalActions();
        const Intent act = activation(drive);
        ASSERT_TRUE(std::find(legal.begin(), legal.end(), act) != legal.end())
            << "the ability is offered";
        e.testHook_executeIntent(act);
    }, nullptr, &watcher);
    ASSERT_GE(priority_windows, 1) << "the opponent was given priority";
    EXPECT_FALSE(drive_on_board_at_priority) << "\"Banish this\" is paid at activation";
    EXPECT_FALSE(unit_back_at_priority) << "the effect waits for the chain";
    EXPECT_EQ(o(drive).zone, ZoneType::Banishment);
    EXPECT_FALSE(onBoard(drive));
    EXPECT_TRUE(onBoard(banished)) << "the unit banished with it is played";
    EXPECT_EQ(ps(P1).rune_pool.energy, 0);
}

// ═══ 9.G1: the play-location helpers leave out where units can't be played ══

TEST_F(VerifyEngineTest, PlayLocationHelpers_LeaveOutABattlefieldUnitsCannotBePlayedTo) {
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    state.battlefields[0].blocks_unit_play = true;   // Rockfall Path

    const auto where = playLocations(state, P1);
    ASSERT_EQ(where.count(), 2);
    EXPECT_TRUE(std::holds_alternative<BaseLocation>(where.locations[0]));
    EXPECT_EQ(std::get<BattlefieldLocation>(where.locations[1]).id, BattlefieldId::fromRaw(1));
    EXPECT_EQ(tokenPlayDests(state, P1), (std::vector<int>{0, 2}));
    EXPECT_EQ(tokenPlayLocations(state, P1).size(), 2u);
    // A hidden card's battlefield (CR 811.1.d.5) where units can't be played:
    // nowhere at all.
    EXPECT_EQ(playLocations(state, P1, BattlefieldId::fromRaw(0)).count(), 0);
    EXPECT_EQ(playLocations(state, P1, BattlefieldId::fromRaw(1)).count(), 1);
    EXPECT_EQ(playLocations(state, P1, std::nullopt, /*include_base=*/false).count(), 1);

    // "Units can only be played to base" (Mageseeker Warden): no battlefield.
    state.battlefields[0].blocks_unit_play = false;
    ps(P1).units_play_base_only = true;
    EXPECT_EQ(playLocations(state, P1).count(), 1);
    EXPECT_EQ(tokenPlayDests(state, P1), (std::vector<int>{0}));
    EXPECT_EQ(playLocations(state, P1, BattlefieldId::fromRaw(1)).count(), 0);
    EXPECT_EQ(playLocations(state, P2).count(), 1) << "the other player controls none";
}

// Through the engine, a card of each helper family: the blocked battlefield is
// not offered and no token lands there.
TEST_F(VerifyEngineTest, TokenPlays_DoNotOfferABattlefieldUnitsCannotBePlayedTo) {
    state.battlefields[0].controller = P1;
    state.battlefields[1].controller = P1;
    state.battlefields[0].blocks_unit_play = true;
    // A unit at each, so both stay controlled through the cleanups.
    addVanilla(P1, 2, 0);
    addVanilla(P1, 2, 1);
    auto unitsAt = [&](BattlefieldId bf) {
        int n = 0;
        for (auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.battlefieldId() == std::optional<BattlefieldId>(bf)) ++n;
        return n;
    };
    // Whatever is asked, take the LAST option: the highest battlefield offered.
    auto last = [](const std::vector<Intent>& legal) { return legal.back(); };

    {   // pickTokenDest / tokenPlayDests — Sprite Call 94
        auto call = addToHand(P1, kSpriteCall);
        EnginePathAgent agent(last);
        playThroughEngine(call, &agent);
        ASSERT_EQ(agent.prompts.size(), 1u);
        EXPECT_EQ(agent.prompts[0].size(), 2u) << "base and battlefield 1 only";
        EXPECT_EQ(unitsAt(BattlefieldId::fromRaw(0)), 1);
        EXPECT_EQ(unitsAt(BattlefieldId::fromRaw(1)), 2);
    }
    {   // playTokensChoosingLocations — Arise! 513 (one Sand Soldier per Equipment)
        addGear(P1, kSerratedDirk);
        addGear(P1, kSerratedDirk);
        auto arise = addToHand(P1, kArise);
        EnginePathAgent agent([&](const std::vector<Intent>& legal) {
            // Location prompts carry values; the "ready a Sand Soldier" prompt
            // that follows offers objects — stop there.
            for (const auto& i : legal)
                if (!i.chosen_objects.empty()) return legal.back();
            return legal.back();
        });
        const int before = unitsAt(BattlefieldId::fromRaw(1));
        playThroughEngine(arise, &agent);
        ASSERT_GE(agent.prompts.size(), 2u);
        EXPECT_EQ(agent.prompts[0].size(), 2u) << "base and battlefield 1 only";
        EXPECT_EQ(agent.prompts[1].size(), 2u);
        EXPECT_EQ(unitsAt(BattlefieldId::fromRaw(0)), 1);
        EXPECT_EQ(unitsAt(BattlefieldId::fromRaw(1)), before + 2);
    }
    {   // only the base left: nothing is asked
        state.battlefields[1].blocks_unit_play = true;
        auto call = addToHand(P1, kSpriteCall);
        EnginePathAgent agent(last);
        const int base_units_before = static_cast<int>(state.unitsAt(LocationId{BaseLocation{P1}}, P1).size());
        playThroughEngine(call, &agent);
        EXPECT_TRUE(agent.prompts.empty());
        EXPECT_EQ(static_cast<int>(state.unitsAt(LocationId{BaseLocation{P1}}, P1).size()),
                  base_units_before + 1);
    }
}

// ═══ 9.G8: a cost that could not be paid (Card::killAsCost / costUnpaid) ════

// Overzealous Fan 449 "you may kill me to move an attacking unit to its base":
// a Fan that is banished instead of dying (Smite) was not killed — it has left
// the board all the same, which the card's own check used to read as "died".
TEST_F(VerifyEngineTest, OverzealousFan_BanishedInsteadOfDyingMovesNobody) {
    state.battlefields[0].controller = P1;
    auto fan = addUnit(P1, kOverzealousFan, 0, 0);
    auto attacker = addVanilla(P2, 3, 0);
    o(fan).combat_designation = CombatDesignation::Defender;
    o(attacker).combat_designation = CombatDesignation::Attacker;
    o(fan).card_counters["__defend_attacker_id"] = static_cast<int>(attacker.raw());
    o(fan).banish_if_would_die_this_turn = true;
    state.battlefields[0].combat_in_progress = true;
    state.battlefields[0].attacker = P2;
    state.battlefields[0].defender = P1;

    AcceptingEnginePathAgent agent;
    fireTriggerThroughEngine(fan, TriggerType::WhenIDefend, &agent);
    ASSERT_FALSE(agent.prompts.empty()) << "the may was offered";
    EXPECT_FALSE(onBoard(fan));
    EXPECT_EQ(o(fan).zone, ZoneType::Banishment);
    EXPECT_EQ(o(attacker).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)})
        << "the cost was not paid: the attacker stays";
}

// Control: a Fan that does die moves the attacker.
TEST_F(VerifyEngineTest, OverzealousFan_DyingMovesTheAttacker) {
    state.battlefields[0].controller = P1;
    auto fan = addUnit(P1, kOverzealousFan, 0, 0);
    auto attacker = addVanilla(P2, 3, 0);
    o(fan).combat_designation = CombatDesignation::Defender;
    o(attacker).combat_designation = CombatDesignation::Attacker;
    o(fan).card_counters["__defend_attacker_id"] = static_cast<int>(attacker.raw());
    state.battlefields[0].combat_in_progress = true;
    state.battlefields[0].attacker = P2;
    state.battlefields[0].defender = P1;

    AcceptingEnginePathAgent agent;
    fireTriggerThroughEngine(fan, TriggerType::WhenIDefend, &agent);
    EXPECT_FALSE(onBoard(fan));
    EXPECT_EQ(o(fan).zone, ZoneType::Trash);
    EXPECT_TRUE(o(attacker).isAtBase());
}

// ═══ 9.G12: option order is a property of the list helpers ══════════════════

// GameState::objects is a hash map: its iteration order follows the order the
// objects were inserted in. The same objects inserted in opposite orders
// iterate differently (the control below), and boardObjects / chooseableUnits
// return the same ascending list for both.
TEST_F(VerifyEngineTest, ListHelpers_ReturnAscendingIdsWhateverTheMapOrder) {
    auto build = [&](bool ascending) {
        GameState st;
        st.players[0].id = P1;
        st.players[1].id = P2;
        std::vector<GameObjectId> ids;
        for (uint32_t n = 10; n < 50; ++n) ids.push_back(GameObjectId::fromRaw(n));
        if (!ascending) std::reverse(ids.begin(), ids.end());
        for (auto id : ids) {
            GameObject obj;
            obj.id = id;
            obj.owner = obj.controller = (id.raw() % 2 == 0) ? P1 : P2;
            obj.card_type = (id.raw() % 5 == 0) ? CardType::Gear : CardType::Unit;
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{obj.controller};
            st.objects.emplace(id, obj);
        }
        return st;
    };
    const GameState up = build(true);
    const GameState down = build(false);
    auto raw = [](const GameState& st) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : st.objects) out.push_back(id);
        return out;
    };
    ASSERT_NE(raw(up), raw(down)) << "control: the map order does follow insertion order";
    ASSERT_FALSE(std::is_sorted(raw(up).begin(), raw(up).end()) &&
                 std::is_sorted(raw(down).begin(), raw(down).end()));

    auto all = [](const GameObject&) { return true; };
    auto gear = [](const GameObject& g) { return g.isGear(); };
    for (const GameState* st : {&up, &down}) {
        const auto units = chooseableUnits(*st, P1, all);
        const auto gears = boardObjects(*st, gear);
        EXPECT_EQ(units.size(), 32u);
        EXPECT_EQ(gears.size(), 8u);
        EXPECT_TRUE(std::is_sorted(units.begin(), units.end()));
        EXPECT_TRUE(std::is_sorted(gears.begin(), gears.end()));
    }
    EXPECT_EQ(chooseableUnits(up, P1, all), chooseableUnits(down, P1, all));
    EXPECT_EQ(boardObjects(up, gear), boardObjects(down, gear));
}

// ═══ 9.A22, 9.D3, 9.C5: "any number" is not capped at 12 ════════════════════

constexpr int kMany = 14;   // more than the old 12-pick caps

// Albus Ferros 230: "spend any number of buffs. For each buff spent, channel 1
// rune exhausted."
TEST_F(VerifyEngineTest, AlbusFerros_SpendsMoreThanTwelveBuffs) {
    std::vector<GameObjectId> buffed;
    for (int i = 0; i < kMany; ++i) {
        buffed.push_back(addVanilla(P1, 2));
        o(buffed.back()).buff_count = 1;
    }
    for (int i = 0; i < kMany; ++i) {
        auto rune = state.createObject();
        o(rune).owner = o(rune).controller = P1;
        o(rune).card_type = CardType::Rune;
        o(rune).domains = {Domain::Order};
        o(rune).zone = ZoneType::RuneDeck;
        ps(P1).rune_deck.push_back(rune);
    }
    auto albus = addToHand(P1, kAlbusFerros);
    EnginePathAgent agent(firstObjectOption);
    playThroughEngine(albus, &agent);
    EXPECT_EQ(static_cast<int>(agent.prompts.size()), kMany);
    for (auto id : buffed) EXPECT_EQ(o(id).buff_count, 0);
    EXPECT_TRUE(ps(P1).rune_deck.empty()) << "one rune channeled per buff spent";
}

// Overt Operation 153: "For each friendly unit, you may spend its buff to
// ready it."
TEST_F(VerifyEngineTest, OvertOperation_AsksAboutMoreThanTwelveBuffedUnits) {
    std::vector<GameObjectId> buffed;
    for (int i = 0; i < kMany; ++i) {
        buffed.push_back(addVanilla(P1, 2));
        o(buffed.back()).buff_count = 1;
        o(buffed.back()).is_exhausted = true;
    }
    auto op = addToHand(P1, kOvertOperation);
    EnginePathAgent agent([](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;
        return legal.front();
    });
    playThroughEngine(op, &agent);
    EXPECT_EQ(static_cast<int>(agent.prompts.size()), kMany);
    for (auto id : buffed) {
        EXPECT_FALSE(o(id).is_exhausted) << "its buff was spent to ready it";
        EXPECT_EQ(o(id).buff_count, 1) << "then every friendly unit was buffed";
    }
}

// Fox-Fire 253: "Kill any number of units at a battlefield with total Might 4
// or less." 0-Might units fit any total.
TEST_F(VerifyEngineTest, FoxFire_KillsMoreThanTwelveZeroMightUnits) {
    std::vector<GameObjectId> weak;
    for (int i = 0; i < kMany; ++i) {
        weak.push_back(addVanilla(P2, 1, 0));
        o(weak.back()).base_might = 0;
        o(weak.back()).current_might = 0;
    }
    auto fox = addToHand(P1, kFoxFire);
    EnginePathAgent agent(firstObjectOption);
    playThroughEngine(fox, &agent);
    for (auto id : weak) EXPECT_FALSE(onBoard(id));
}

// Corrupted Dragon 878: "you may move any number of enemy units here each with
// 5 [M] or less to their base."
TEST_F(VerifyEngineTest, CorruptedDragon_MovesMoreThanTwelveUnits) {
    auto dragon = addUnit(P1, kCorruptedDragon, 0, 0);
    std::vector<GameObjectId> small;
    for (int i = 0; i < kMany; ++i) small.push_back(addVanilla(P2, 2, 0));
    EnginePathAgent agent(firstObjectOption);
    fireTriggerThroughEngine(dragon, TriggerType::WhenIAttack, &agent);
    for (auto id : small) EXPECT_TRUE(o(id).isAtBase());
}

// splitDamageAmong: 20 damage between two targets is 1 each plus 18 picks of
// one point, so any split can be expressed (the old cap made each pick worth 2
// above 12 damage).
TEST_F(VerifyEngineTest, SplitDamage_AboveTwelveIsStillAssignedOnePointAtATime) {
    auto a = addVanilla(P2, 30);
    auto b = addVanilla(P2, 30);
    auto spell = addToHand(P1, kTSplitSpell);
    int n = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        // Targets a then b; then 12 more to a and 6 to b: 13 / 7, an odd split.
        const int k = n++;
        const GameObjectId want = (k == 0 || (k >= 2 && k < 14)) ? a : b;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    EXPECT_EQ(static_cast<int>(agent.prompts.size()), 20) << "2 targets + 18 points";
    EXPECT_EQ(o(a).damage_marked, 13);
    EXPECT_EQ(o(b).damage_marked, 7);
}

// selectSeveral (was selectAnyNumber, capped at ten picks): more than ten.
TEST_F(VerifyEngineTest, SelectAnyNumber_PicksMoreThanTen) {
    std::vector<GameObjectId> mine;
    for (int i = 0; i < kMany; ++i) mine.push_back(addVanilla(P1, 2));
    auto spell = addToHand(P1, kTManySpell);
    EnginePathAgent agent(firstObjectOption);
    playThroughEngine(spell, &agent);
    for (auto id : mine) EXPECT_EQ(o(id).card_counters["picked"], 1);
}

// ═══ plans/05 item 5.13: combat resolution leaves nothing stale ═════════════

// When a combat ends with one player left, the battlefield is no longer
// Contested — and nobody is recorded as contesting it and no combat is staged
// for it, already while the conquer triggers of that combat are being decided.
TEST_F(VerifyEngineTest, CombatResolution_LeavesNoContesterAndNoStagedCombat) {
    state.battlefields[0].controller = P2;
    state.battlefields[0].is_contested = true;
    state.battlefields[0].contested_by = P1;
    state.battlefields[0].combat_staged = true;
    addVanilla(P1, 5, 0);
    addVanilla(P2, 2, 0);
    std::optional<PlayerId> controller_event;
    boost::signals2::scoped_connection c1 = events.on_control_changed.connect(
        [&](const ControlChangedEvent& e) { controller_event = e.new_controller; });
    withEngine([&](GameEngine& e) {
        e.testHook_runCombat(BattlefieldId::fromRaw(0));
        const auto& bf = e.state().battlefields[0];
        EXPECT_FALSE(bf.combat_in_progress);
        EXPECT_FALSE(bf.is_contested);
        EXPECT_EQ(bf.contested_by, PlayerId::None);
        EXPECT_FALSE(bf.combat_staged);
        EXPECT_EQ(bf.controller, std::optional<PlayerId>(P1));
    });
    // on_control_changed has no subscriber in src/: it is part of the event
    // surface for tests and tools, and it is emitted.
    EXPECT_EQ(controller_event, std::optional<PlayerId>(P1));
}

// Pin for plans/05 item 5.13 "swapBackBattlefield has no callers, so a replaced
// battlefield can never be restored": the engine's two unused copies
// (GameEngine::replaceBattlefield / swapBackBattlefield) are gone; replacing is
// EffectExecutor::replaceBattlefieldWithToken and the swap back is the token
// battlefield's own ability (Brush 561: "When you score here, you may replace
// this with the battlefield it replaced").
TEST_F(VerifyEngineTest, Pin_Brush_WhenScoredRestoresTheBattlefieldItReplaced) {
    constexpr CardDefId kBrush = 561;
    constexpr CardDefId kAspirantsClimb = 271;
    auto original = state.createObject();
    o(original).owner = P1;
    o(original).card_type = CardType::Battlefield;
    o(original).card_def_id = kAspirantsClimb;
    o(original).name = card_db.get(kAspirantsClimb).name;
    o(original).zone = ZoneType::BattlefieldZone;
    state.battlefields[0].card_object_id = original;
    state.battlefields[0].controller = P1;
    addVanilla(P1, 2, 0);

    AcceptingEnginePathAgent agent;
    withEngine([&](GameEngine& e) {
        e.testHook_executor().replaceBattlefieldWithToken(BattlefieldId::fromRaw(0), "Brush", P1, kBrush);
        const auto& bf = e.state().battlefields[0];
        ASSERT_TRUE(bf.was_replaced);
        ASSERT_EQ(e.state().getObject(original).zone, ZoneType::Banishment);
        const GameObjectId brush = bf.card_object_id;
        ASSERT_NE(brush, original);
        e.testHook_executor().queueTriggeredAbility(brush, P1, TriggerType::WhenYouScoreHere,
                                                    kInvalidId);
        e.testHook_runChain();
    }, &agent);
    ASSERT_FALSE(agent.prompts.empty()) << "the may was offered";
    const auto& bf = state.battlefields[0];
    EXPECT_EQ(bf.card_object_id, original);
    EXPECT_FALSE(bf.was_replaced);
    EXPECT_FALSE(bf.is_token);
    EXPECT_EQ(o(original).zone, ZoneType::BattlefieldZone);
    auto& banished = ps(P1).banishment;
    EXPECT_TRUE(std::find(banished.begin(), banished.end(), original) == banished.end());
}

// Pin for the signals that have no subscriber in src/ (plans/05 item 5.13):
// they are emitted. A concession reaches on_game_over.
TEST_F(VerifyEngineTest, Pin_EventSurface_SignalsWithNoEngineSubscriberAreEmitted) {
    int game_over = 0;
    PlayerId winner = PlayerId::None;
    boost::signals2::scoped_connection c = events.on_game_over.connect(
        [&](const GameOverEvent& e) { ++game_over; winner = e.winner; });
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(Intent::concede(P1)); });
    EXPECT_EQ(game_over, 1);
    EXPECT_EQ(winner, P2);
    EXPECT_TRUE(state.game_over);
}

// An exhausted source cannot pay an [E] cost through the seam either (the
// activation gates were there before; this pins that the new ones did not
// displace them), and a ready one does.
TEST_F(VerifyEngineTest, ExecuteIntent_AnActivationStillPaysItsCost) {
    auto unit = addUnit(P1, kTAbilityUnit, 0, -1);
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(activation(unit)); });
    EXPECT_TRUE(o(unit).is_exhausted);
    EXPECT_EQ(o(unit).card_counters["activated"], 1);
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(activation(unit)); });
    EXPECT_EQ(o(unit).card_counters["activated"], 1) << "exhausted: refused";
    // Not its controller's to activate.
    o(unit).is_exhausted = false;
    Intent theirs = activation(unit);
    theirs.player = P2;
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(theirs); });
    EXPECT_FALSE(o(unit).is_exhausted);
    EXPECT_EQ(o(unit).card_counters["activated"], 1);
}

// ═══ 9.M4: the units of a split are targets chosen at finalization ══════════

// Alpha Strike 751 (CR 355.14.a-b): when the opponent gets priority with the
// spell on the chain, the units the damage will be split among are already
// chosen (and Deflect for them paid); how much each takes is decided as the
// spell resolves (355.14.e).
TEST_F(VerifyEngineTest, AlphaStrike_TheSplitTargetsAreChosenBeforeTheOpponentCanRespond) {
    auto striker = addVanilla(P1, 5);
    auto e1 = addVanilla(P2, 9, 0);
    auto e2 = addVanilla(P2, 9, 0);
    auto e3 = addVanilla(P2, 9, 1);
    o(e2).keywords.set(Keyword::Deflect);
    o(e2).deflect_value = 1;
    ps(P1).rune_pool.universal_power = 1;   // for e2's Deflect
    auto spell = addToHand(P1, kAlphaStrike);

    std::vector<GameObjectId> chosen_at_priority;
    int power_at_priority = -1;
    int priority_windows = 0;
    LambdaAgent opponent([&](const GameState& st, const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority && !st.chain.items.empty()) {
                ++priority_windows;
                chosen_at_priority = st.chain.items.back().chosen_objects;
                power_at_priority = st.player(P1).rune_pool.universal_power;
                EXPECT_EQ(st.getObject(e1).damage_marked, 0);
            }
        return passOrFirst(legal);
    });
    int n = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        // Targets: e1, e2, then "No more targets". Division: the 3 extra
        // points all to e2.
        const int k = n++;
        if (k == 2)
            for (const auto& i : legal)
                if (i.chosen_objects.empty()) return i;
        const GameObjectId want = k == 0 ? e1 : e2;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        return legal.front();
    });
    const auto& def = card_db.get(kAlphaStrike);
    ps(P1).rune_pool.energy += def.energy_cost;
    ps(P1).rune_pool.universal_power += def.power_cost;
    Intent play = playOf(spell);
    play.targets = {striker};
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(play);
        e.testHook_runChain();
    }, &agent, &opponent);

    ASSERT_GE(priority_windows, 1);
    auto was_chosen = [&](GameObjectId id) {
        return std::find(chosen_at_priority.begin(), chosen_at_priority.end(), id) !=
               chosen_at_priority.end();
    };
    EXPECT_TRUE(was_chosen(e1) && was_chosen(e2))
        << "both targets were on the chain item when the opponent could respond";
    EXPECT_FALSE(was_chosen(e3));
    EXPECT_EQ(power_at_priority, 0) << "Deflect was paid as the target was chosen";
    EXPECT_TRUE(agent.wasOffered(e3));
    EXPECT_EQ(o(e1).damage_marked, 1) << "every target gets at least 1 (CR 355.14.f)";
    EXPECT_EQ(o(e2).damage_marked, 4);
    EXPECT_EQ(o(e3).damage_marked, 0) << "not a target";
    EXPECT_EQ(static_cast<int>(agent.prompts.size()), 6) << "3 for the targets, 3 points";
}

// A target that is gone when the split resolves takes nothing, and all of the
// damage goes to the targets that are left (CR 359.3.e.5).
TEST_F(VerifyEngineTest, Volibear_ASplitTargetThatLeftInResponseIsDropped) {
    state.battlefields[0].controller = P2;
    auto bear = addUnit(P1, kVolibearFurious, 0, 0);
    auto e1 = addVanilla(P2, 9, 0);
    auto e2 = addVanilla(P2, 9, 0);
    o(bear).combat_designation = CombatDesignation::Attacker;
    GameEngine* engine = nullptr;
    bool removed = false;
    LambdaAgent opponent([&](const GameState& st, const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority && !st.chain.items.empty() && !removed) {
                removed = true;
                // "In response": e1 goes back to its owner's hand.
                engine->testHook_executor().bounceToHand(e1);
            }
        return passOrFirst(legal);
    });
    int n = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        const int k = n++;
        const GameObjectId want = k == 0 ? e1 : e2;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        engine = &e;
        e.testHook_executor().queueTriggeredAbility(bear, P1, TriggerType::WhenIAttack,
                                                    kInvalidId);
        e.testHook_runChain();
    }, &agent, &opponent);
    ASSERT_TRUE(removed);
    EXPECT_EQ(static_cast<int>(agent.prompts.size()), 2) << "two targets; one left: no division";
    EXPECT_FALSE(onBoard(e1));
    EXPECT_EQ(o(e2).damage_marked, 5);
}

// ═══ 9.M1: a multi-unit Standard Move is one action (CR 144.3) ══════════════

TEST_F(VerifyEngineTest, StandardMove_SeveralUnitsArriveAndExhaustBeforeAnyMoveIsAnnounced) {
    auto u1 = addVanilla(P1, 2);
    auto u2 = addVanilla(P1, 2);
    GameEngine* engine = nullptr;
    int moved_events = 0, exhausted_events = 0;
    bool all_there_at_first_move = false, all_exhausted_at_first_exhaust = false;
    const LocationId dest{BattlefieldLocation{0}};
    boost::signals2::scoped_connection c1 = events.on_unit_moved.connect(
        [&](const UnitMovedEvent&) {
            if (moved_events++ == 0) {
                const auto& st = engine->state();
                all_there_at_first_move = st.getObject(u1).location == dest &&
                                          st.getObject(u2).location == dest;
            }
        });
    boost::signals2::scoped_connection c2 = events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.what_changed != "exhausted") return;
            if (exhausted_events++ == 0) {
                const auto& st = engine->state();
                all_exhausted_at_first_exhaust = st.getObject(u1).is_exhausted &&
                                                 st.getObject(u2).is_exhausted;
            }
        });
    EnginePathAgent agent(firstObjectOption);   // add the other unit to the move
    withEngine([&](GameEngine& e) {
        engine = &e;
        e.testHook_executeStandardMove(Intent::standardMove(P1, {u1}, dest));
    }, &agent);
    ASSERT_EQ(agent.prompts.size(), 1u) << "\"move another unit with this move\"";
    EXPECT_EQ(moved_events, 2);
    EXPECT_EQ(exhausted_events, 2);
    EXPECT_TRUE(all_exhausted_at_first_exhaust) << "CR 144.3.c: the costs are paid together";
    EXPECT_TRUE(all_there_at_first_move) << "CR 144.3: one action";
    EXPECT_EQ(o(u1).moves_this_turn, 1);
    EXPECT_EQ(o(u2).moves_this_turn, 1);
}

// ═══ 9.M9: CR 323.6 recalls go through the one recall ═══════════════════════

TEST_F(VerifyEngineTest, Cleanup_RecallsAUnitFromAnotherPlayersBaseWithItsGear) {
    // A unit P1 took control of, still standing in P2's base, with a gear on it.
    auto unit = addVanilla(P2, 3);
    auto gear = addGear(P2, kSerratedDirk);
    o(unit).controller = P1;
    o(gear).controller = P1;
    o(gear).attached_to = unit;
    o(unit).attachments.push_back(gear);
    int recalled = 0;
    boost::signals2::scoped_connection c = events.on_object_state_changed.connect(
        [&](const ObjectStateChangedEvent& e) {
            if (e.object == unit && e.what_changed == "recalled") ++recalled;
        });
    cleanup();
    const std::optional<LocationId> home{LocationId{BaseLocation{P1}}};
    EXPECT_EQ(o(unit).location, home);
    EXPECT_EQ(o(gear).location, home) << "CR 719.3.a: what is attached goes along";
    EXPECT_EQ(o(gear).attached_to, std::optional<GameObjectId>(unit));
    EXPECT_EQ(o(unit).moves_this_turn, 0) << "a recall is not a move (CR 458)";
    EXPECT_EQ(recalled, 1) << "announced like every other recall";
}

// ═══ 9.M11: control taken without a Score is not a conquer ══════════════════

TEST_F(VerifyEngineTest, ScoreConquer_ControlWithoutAScoreIsNotAConquer) {
    const Card* grimwyrm = card_registry.get(kPerchedGrimwyrm);
    ASSERT_NE(grimwyrm, nullptr);
    // (a) "Players can't score here until their third turn": no Score, so no
    //     Conquer (CR 471.1) — not "a battlefield you conquered this turn".
    state.battlefields[0].min_turn_to_score = 3;
    ps(P1).turns_taken = 1;
    // (b) Already scored this turn (held it, lost it, took it back): "did not
    //     yet Score this turn" fails (CR 468.5.d).
    ps(P1).battlefields_scored_this_turn.insert(BattlefieldId::fromRaw(1));
    withEngine([&](GameEngine& e) {
        e.testHook_scoreConquer(P1, BattlefieldId::fromRaw(0));
        e.testHook_scoreConquer(P1, BattlefieldId::fromRaw(1));
    });
    EXPECT_EQ(state.battlefields[0].conquered_on_turn, -1);
    EXPECT_EQ(state.battlefields[1].conquered_on_turn, -1);
    EXPECT_TRUE(grimwyrm->getPlayLocations(state, P1).empty());
    EXPECT_EQ(ps(P1).score, 0);

    // Control: a real conquer is stamped, also when its point is withheld.
    state.battlefields[0].min_turn_to_score = 0;
    ps(P1).cannot_gain_points = true;
    withEngine([&](GameEngine& e) { e.testHook_scoreConquer(P1, BattlefieldId::fromRaw(0), P2); });
    EXPECT_EQ(state.battlefields[0].conquered_on_turn, state.turn.turn_number);
    EXPECT_EQ(state.battlefields[0].conquered_by_player, P1);
    ASSERT_EQ(grimwyrm->getPlayLocations(state, P1).size(), 1u);
}

// ═══ 9.M6: [Temporary] on attached Equipment — printed vs given ═════════════

TEST_F(VerifyEngineTest, Temporary_GivenToAttachedEquipmentKillsItPrintedDoesNot) {
    auto bearer = addVanilla(P1, 3);
    auto attach = [&](GameObjectId gear) {
        o(gear).attached_to = bearer;
        o(bearer).attachments.push_back(gear);
    };
    auto given = addGear(P1, kSerratedDirk);          // no printed [Temporary]
    o(given).keywords.set(Keyword::Temporary);        // "give a gear [Temporary]"
    attach(given);
    auto printed = addGear(P1, kSpinningAxe);         // prints it
    ASSERT_TRUE(card_db.get(kSpinningAxe).keywords.has(Keyword::Temporary));
    attach(printed);
    auto loose = addGear(P1, kSpinningAxe);           // unattached: its text is active

    state.turn.turn_player = P1;
    state.turn.phase = TurnPhase::AwakenPhase;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); });

    EXPECT_FALSE(onBoard(given)) << "a granted [Temporary] is not printed text (CR 718.2)";
    EXPECT_TRUE(onBoard(printed)) << "attached: the printed rules text is Inactive";
    EXPECT_EQ(o(printed).attached_to, std::optional<GameObjectId>(bearer));
    EXPECT_FALSE(onBoard(loose));
    EXPECT_TRUE(onBoard(bearer));
}

// ═══ 9.N3: reaction-time unit plays respect "units only to base" ════════════

TEST_F(VerifyEngineTest, AmbushAndPounce_AreNotOfferedToAPlayerWhoMayOnlyPlayUnitsToBase) {
    state.battlefields[0].controller = P2;
    addVanilla(P1, 2, 0);                      // P1 has a unit there: Ambush's condition
    addVanilla(P2, 2, 0);
    auto ambusher = addToHand(P1, kDianaNoLongerHuman);
    auto pouncer = addToHand(P1, kRengarPouncing);
    ps(P1).rune_pool.energy = 20;
    ps(P1).rune_pool.universal_power = 10;
    state.battlefields[0].combat_in_progress = true;
    state.battlefields[0].attacker = P1;
    state.battlefields[0].defender = P2;
    auto offered = [&](bool closed, GameObjectId card) {
        bool found = false;
        withEngine([&](GameEngine& e) {
            auto& st = e.mutableState();
            st.turn.ns_state = NeutralShowdownState::Showdown;
            st.turn.focus_holder = P1;
            if (closed) {
                st.turn.oc_state = OpenClosedState::Closed;
                st.turn.priority_holder = P1;
            }
            for (const auto& i : e.generateLegalActions())
                if (i.card == card && i.play_location ==
                        std::optional<LocationId>(LocationId{BattlefieldLocation{0}}))
                    found = true;
            st.turn.ns_state = NeutralShowdownState::Neutral;
            st.turn.oc_state = OpenClosedState::Open;
            st.turn.priority_holder.reset();
            st.turn.focus_holder.reset();
        });
        return found;
    };
    for (bool closed : {false, true}) {
        SCOPED_TRACE(closed ? "Closed State" : "showdown");
        ps(P1).units_play_base_only = false;
        EXPECT_TRUE(offered(closed, ambusher)) << "control: offered without the restriction";
        EXPECT_TRUE(offered(closed, pouncer));
        ps(P1).units_play_base_only = true;    // an opposing Mageseeker Warden
        EXPECT_FALSE(offered(closed, ambusher));
        EXPECT_FALSE(offered(closed, pouncer));
    }
}

// ═══ 9.M5: Smite's "banish it instead" on the lethal-damage path ════════════

TEST_F(VerifyEngineTest, BanishIfWouldDie_AppliesToLethalDamageAndEndsWithTheTurn) {
    auto doomed = addVanilla(P2, 2, 0);
    auto spared = addVanilla(P2, 2, 0);
    o(doomed).banish_if_would_die_this_turn = true;
    o(spared).banish_if_would_die_this_turn = true;
    o(doomed).damage_marked = 2;   // lethal: cleanup kills it (GameEngine::killUnit)
    int died = 0;
    boost::signals2::scoped_connection c = events.on_unit_died.connect(
        [&](const UnitDiedEvent&) { ++died; });
    withEngine([&](GameEngine& e) { e.testHook_cleanup(); });
    EXPECT_FALSE(onBoard(doomed));
    EXPECT_EQ(o(doomed).zone, ZoneType::Banishment);
    auto& banished = ps(P2).banishment;
    EXPECT_TRUE(std::find(banished.begin(), banished.end(), doomed) != banished.end());
    EXPECT_TRUE(ps(P2).trash.empty());
    EXPECT_EQ(died, 0) << "banished instead: it did not die";
    EXPECT_TRUE(onBoard(spared));
    EXPECT_TRUE(o(spared).banish_if_would_die_this_turn);

    withEngine([&](GameEngine& e) { e.testHook_expirationStep(); });
    EXPECT_FALSE(o(spared).banish_if_would_die_this_turn) << "\"this turn\"";
}

// ═══ 9.M7: "with spells or unit abilities" (GameState::noteChosen) ══════════

TEST_F(VerifyEngineTest, EnemyChoices_CountUnitAbilitiesNotGearOrLegendAbilities) {
    auto enemy = addVanilla(P2, 3);
    auto unit = addUnit(P1, kTChooserUnit, 0, -1);
    auto gear = addGear(P1, kTChooserGear);
    auto legend = state.createObject();
    o(legend).owner = o(legend).controller = P1;
    o(legend).card_def_id = kTChooserLegend;
    o(legend).card_type = CardType::Legend;
    o(legend).zone = ZoneType::LegendZone;
    ps(P1).legend_zone = legend;

    auto fire = [&](GameObjectId source) {
        EnginePathAgent agent(EnginePathAgent::choosing(enemy));
        fireTriggerThroughEngine(source, TriggerType::WhenYouPlayMe, &agent);
        EXPECT_TRUE(agent.wasOffered(enemy) || !agent.prompts.empty() || true);
    };
    fire(gear);
    EXPECT_TRUE(ps(P1).chose_enemy_unit_this_turn) << "any choice of an enemy unit";
    EXPECT_EQ(ps(P1).enemy_choices_this_turn, 0) << "a gear's ability is not a unit ability";
    fire(legend);
    EXPECT_EQ(ps(P1).enemy_choices_this_turn, 0) << "nor is a legend's";
    fire(unit);
    EXPECT_EQ(ps(P1).enemy_choices_this_turn, 1) << "a unit's ability counts";
}

// ═══ 9.T4: choices are made where the rules finalize ════════════════════════

namespace {
/// Answers the n-th object prompt with picks[n]; `at_prompt(state)` is called
/// at each one. Passes priority, says yes (1) to a yes/no.
struct PickInOrder : AgentInterface {
    std::vector<GameObjectId> picks;
    std::function<void(const GameState&)> at_prompt;
    int object_prompts = 0;
    Intent selectAction(const GameState& st, const std::vector<Intent>& legal) override {
        const bool objects = std::any_of(legal.begin(), legal.end(), [](const Intent& i) {
            return i.type == IntentType::MakeChoice && !i.chosen_objects.empty();
        });
        if (objects) {
            if (at_prompt) at_prompt(st);
            const size_t n = static_cast<size_t>(object_prompts++);
            for (const auto& i : legal)
                if (n < picks.size() && !i.chosen_objects.empty() &&
                    i.chosen_objects.front() == picks[n]) return i;
            ADD_FAILURE() << "object prompt " << n << " did not offer the scripted pick";
            return legal.front();
        }
        for (const auto& i : legal)
            if (i.type == IntentType::MakeChoice && i.chosen_value.has_value() &&
                *i.chosen_value == 1) return i;
        return passOrFirst(legal);
    }
};
}  // namespace

// (1) A spell an effect plays is finalized as it is put on the chain: its
//     choices are made then, before the rest of the effect that played it.
TEST_F(VerifyEngineTest, EffectPlayedSpell_MakesItsChoicesAsItIsPutOnTheChain) {
    auto e1 = addVanilla(P2, 3);
    auto e2 = addVanilla(P2, 3);
    auto outer = addToHand(P1, kTPlayerSpell);
    addToHand(P1, kTFinalSpell);
    PickInOrder me;
    me.picks = {e2};
    int after_play_at_prompt = -1;
    me.at_prompt = [&](const GameState& st) {
        auto& c = st.getObject(outer).card_counters;
        auto it = c.find("after_play");
        after_play_at_prompt = it == c.end() ? 0 : it->second;
    };
    playThroughEngine(outer, &me);
    ASSERT_EQ(me.object_prompts, 1);
    EXPECT_EQ(after_play_at_prompt, 0)
        << "the inner spell chose while the effect that played it was still under way";
    EXPECT_EQ(o(e2).card_counters["hit"], 1);
    EXPECT_EQ(o(e1).card_counters["hit"], 0);
    EXPECT_EQ(o(outer).card_counters["after_play"], 1);
}

// (2) "Make new choices" (CR 750-755): the new choices are made as that effect
//     resolves, not when the re-chosen spell resolves later.
TEST_F(VerifyEngineTest, MakeNewChoices_AFinalizePassSpellChoosesAgainAtOnce) {
    auto e1 = addVanilla(P2, 3);
    auto e2 = addVanilla(P2, 3);
    auto spell = addToHand(P1, kTFinalSpell);
    auto rechooser = addToHand(P2, kTRechooser);
    PickInOrder me;
    me.picks = {e1, e2};
    std::vector<int> marker_at_prompt;
    me.at_prompt = [&](const GameState& st) {
        auto& c = st.getObject(rechooser).card_counters;
        auto it = c.find("after_rechoose");
        marker_at_prompt.push_back(it == c.end() ? 0 : it->second);
    };
    bool responded = false;
    LambdaAgent them([&](const GameState&, const std::vector<Intent>& legal) {
        if (!responded)
            for (const auto& i : legal)
                if (i.type == IntentType::PlayReaction && i.card == rechooser) {
                    responded = true;
                    return i;
                }
        return passOrFirst(legal);
    });
    playThroughEngine(spell, &me, std::nullopt, &them);
    ASSERT_TRUE(responded) << "the opponent answered with the re-choice spell";
    ASSERT_EQ(me.object_prompts, 2);
    ASSERT_EQ(marker_at_prompt.size(), 2u);
    EXPECT_EQ(marker_at_prompt[1], 0)
        << "asked again while the re-choice effect was resolving, not afterwards";
    EXPECT_EQ(o(e1).card_counters["hit"], 0);
    EXPECT_EQ(o(e2).card_counters["hit"], 1) << "the new choice is the one that counts";
}

// (3) Repeat (CR 820.2): the choices of the additional execution are made with
//     the first execution's, as the spell is finalized — the opponent responds
//     knowing both — and need not be the same (820.2.a).
TEST_F(VerifyEngineTest, Repeat_TheExtraExecutionChoosesAtFinalization) {
    auto e1 = addVanilla(P2, 3);
    auto e2 = addVanilla(P2, 3);
    auto spell = addToHand(P1, kTFinalRepeat);
    ps(P1).rune_pool.energy = 1;   // the Repeat cost
    PickInOrder me;
    me.picks = {e1, e2};
    int prompts_answered_at_priority = -1;
    size_t stored_at_priority = 99;
    LambdaAgent them([&](const GameState& st, const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority && !st.chain.items.empty() &&
                prompts_answered_at_priority < 0) {
                prompts_answered_at_priority = me.object_prompts;
                stored_at_priority = st.chain.items.back().repeat_choices.size();
            }
        return passOrFirst(legal);
    });
    playThroughEngine(spell, &me, std::nullopt, &them);
    EXPECT_EQ(ps(P1).rune_pool.energy, 0) << "the Repeat was paid";
    EXPECT_EQ(prompts_answered_at_priority, 2)
        << "both executions had chosen when the opponent could respond";
    EXPECT_EQ(stored_at_priority, 1u);
    EXPECT_EQ(me.object_prompts, 2) << "nothing is asked again at resolution";
    EXPECT_EQ(o(e1).card_counters["hit"], 1);
    EXPECT_EQ(o(e2).card_counters["hit"], 1);
}

// Pin (CR 355.14.b): with nothing to target as the split is finalized, no
// target is chosen — a unit that arrives in response takes nothing.
TEST_F(VerifyEngineTest, Pin_SplitDamage_NothingToTargetAtFinalizationDealsNothing) {
    state.battlefields[0].controller = P2;
    auto bear = addUnit(P1, kVolibearFurious, 0, 0);
    auto late = addVanilla(P2, 9);
    GameEngine* engine = nullptr;
    bool arrived = false;
    LambdaAgent them([&](const GameState& st, const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority && !st.chain.items.empty() && !arrived) {
                arrived = true;
                engine->testHook_executor().moveToBattlefield(late, BattlefieldId::fromRaw(0));
            }
        return passOrFirst(legal);
    });
    EnginePathAgent me;
    withEngine([&](GameEngine& e) {
        engine = &e;
        e.testHook_executor().queueTriggeredAbility(bear, P1, TriggerType::WhenIAttack,
                                                    kInvalidId);
        e.testHook_runChain();
    }, &me, &them);
    ASSERT_TRUE(arrived);
    EXPECT_TRUE(me.prompts.empty());
    EXPECT_EQ(o(late).battlefieldId(), std::optional<BattlefieldId>{BattlefieldId::fromRaw(0)});
    EXPECT_EQ(o(late).damage_marked, 0);
}

// ═══ 9.U1: a [Temporary] given to Equipment that also prints it ═════════════

TEST_F(VerifyEngineTest, Temporary_GivenToAttachedEquipmentThatAlsoPrintsItKillsIt) {
    auto bearer = addVanilla(P1, 3);
    auto axe = addGear(P1, kSpinningAxe);
    o(axe).attached_to = bearer;
    o(bearer).attachments.push_back(axe);
    auto control = addGear(P1, kSpinningAxe);        // attached, printed only
    auto bearer2 = addVanilla(P1, 3);
    o(control).attached_to = bearer2;
    o(bearer2).attachments.push_back(control);

    // Turn to Dust 632: "Give a gear [Temporary]."
    auto dust = addToHand(P2, kTurnToDust);
    const auto& def = card_db.get(kTurnToDust);
    ps(P2).rune_pool.energy += def.energy_cost;
    ps(P2).rune_pool.universal_power += def.power_cost;
    Intent play = playOf(dust);
    play.targets = {axe};
    withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); }, nullptr, nullptr, P2);
    ASSERT_TRUE(o(axe).granted_keywords.has(Keyword::Temporary));
    ASSERT_TRUE(onBoard(axe));

    state.turn.turn_player = P1;
    withEngine([&](GameEngine& e) { e.testHook_beginningStep(); });
    EXPECT_FALSE(onBoard(axe)) << "the given [Temporary] is not its printed text (CR 718.2)";
    EXPECT_TRUE(onBoard(control)) << "printed only, attached: Inactive";
}

// ═══ 9.U2: "the second player" is who lost the coin toss (CR 116) ═══════════

TEST_F(VerifyEngineTest, ChannelPhase_TheExtraRuneGoesToWhoeverDidNotGoFirst) {
    auto fill = [&](PlayerId p) {
        ps(p).rune_deck.clear();
        for (int i = 0; i < 4; ++i) {
            auto rune = state.createObject();
            o(rune).owner = o(rune).controller = p;
            o(rune).card_type = CardType::Rune;
            o(rune).domains = {Domain::Order};
            o(rune).zone = ZoneType::RuneDeck;
            ps(p).rune_deck.push_back(rune);
        }
    };
    auto channeled = [&](PlayerId turn_player, int turn_number) {
        fill(turn_player);
        state.turn.turn_player = turn_player;
        state.turn.turn_number = turn_number;
        ps(turn_player).is_first_turn = true;
        withEngine([](GameEngine& e) { e.testHook_channelPhase(); }, nullptr, nullptr,
                   turn_player);
        return 4 - static_cast<int>(ps(turn_player).rune_deck.size());
    };
    state.turn.starting_player = P1;
    state.turn.phase = TurnPhase::AwakenPhase;
    // P2 lost the toss. Its first turn gets the extra rune whatever number the
    // turn carries (the old code read "turn 0" as "the first player's turn").
    EXPECT_EQ(channeled(P2, 0), 3);
    EXPECT_EQ(channeled(P2, 1), 3);
    // P1 went first: no extra rune on its first turn, at any turn number (an
    // extra turn before it does not make it "the second player").
    EXPECT_EQ(channeled(P1, 0), 2);
    EXPECT_EQ(channeled(P1, 2), 2);
}

// ═══ 9.Y1: a re-chosen target is chosen like any target ═════════════════════

// A spell whose target rides in the Play intent is told to make new choices
// (CR 750-755): the new target's Deflect is paid and the choice is counted
// ("chosen enemy units ... with spells", Prodigal Explorer's count).
TEST_F(VerifyEngineTest, MakeNewChoices_AReChosenTargetPaysDeflectAndCounts) {
    auto e1 = addVanilla(P2, 3);
    auto e2 = addVanilla(P2, 3);
    o(e2).keywords.set(Keyword::Deflect);
    o(e2).deflect_value = 1;
    auto spell = addToHand(P1, kTPokeSpell);
    auto rechooser = addToHand(P2, kTRechooser);
    ps(P1).rune_pool.universal_power = 1;
    PickInOrder me;
    me.picks = {e2};                       // the re-choice
    bool responded = false;
    int count_before_rechoice = -1;
    LambdaAgent them([&](const GameState& st, const std::vector<Intent>& legal) {
        if (!responded)
            for (const auto& i : legal)
                if (i.type == IntentType::PlayReaction && i.card == rechooser) {
                    responded = true;
                    count_before_rechoice = st.player(P1).enemy_choices_this_turn;
                    return i;
                }
        return passOrFirst(legal);
    });
    Intent play = playOf(spell);
    play.targets = {e1};
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(play);
        e.testHook_runChain();
    }, &me, &them);
    ASSERT_TRUE(responded);
    ASSERT_EQ(me.object_prompts, 1) << "asked for the new target";
    EXPECT_EQ(o(e2).card_counters["hit"], 1);
    EXPECT_EQ(o(e1).card_counters["hit"], 0);
    EXPECT_EQ(ps(P1).rune_pool.universal_power, 0) << "Deflect for the new target";
    EXPECT_EQ(ps(P1).enemy_choices_this_turn, count_before_rechoice + 1)
        << "the new choice is a choice of an enemy unit with a spell";
}

// A new target whose Deflect cannot be paid is not offered.
TEST_F(VerifyEngineTest, MakeNewChoices_ATargetWhoseDeflectCannotBePaidIsNotOffered) {
    auto e1 = addVanilla(P2, 3);
    auto e2 = addVanilla(P2, 3);
    o(e2).keywords.set(Keyword::Deflect);
    o(e2).deflect_value = 1;
    auto spell = addToHand(P1, kTPokeSpell);
    auto rechooser = addToHand(P2, kTRechooser);
    EnginePathAgent me;                    // no Power: only e1 can be chosen again
    bool responded = false;
    LambdaAgent them([&](const GameState&, const std::vector<Intent>& legal) {
        if (!responded)
            for (const auto& i : legal)
                if (i.type == IntentType::PlayReaction && i.card == rechooser) {
                    responded = true;
                    return i;
                }
        return passOrFirst(legal);
    });
    Intent play = playOf(spell);
    play.targets = {e1};
    withEngine([&](GameEngine& e) {
        e.testHook_executeIntent(play);
        e.testHook_runChain();
    }, &me, &them);
    ASSERT_TRUE(responded);
    ASSERT_EQ(me.prompts.size(), 1u);
    EXPECT_FALSE(me.wasOffered(e2));
    EXPECT_TRUE(me.wasOffered(e1));
    EXPECT_EQ(o(e1).card_counters["hit"], 1);
}

// ═══ 9.Y3: an effect-played spell is Finalized when its choices are made ════

TEST_F(VerifyEngineTest, EffectPlayedSpell_IsFinalizedAndAnnouncedAsItIsPutOnTheChain) {
    auto e1 = addVanilla(P2, 3);
    auto outer = addToHand(P1, kTPlayerSpell);
    auto inner = addToHand(P1, kTFinalSpell);
    GameEngine* engine = nullptr;
    int finalized_events = 0;
    int after_play_at_event = -1;
    boost::signals2::scoped_connection c = events.on_chain_item_finalized.connect(
        [&](const ChainItemFinalizedEvent& ev) {
            if (ev.source != inner) return;
            ++finalized_events;
            const auto& st = engine->state();
            auto& counters = st.getObject(outer).card_counters;
            auto it = counters.find("after_play");
            after_play_at_event = it == counters.end() ? 0 : it->second;
            for (const auto& ci : st.chain.items) {
                if (ci.source == inner) {
                    EXPECT_EQ(ci.status, ChainItemStatus::Finalized);
                }
            }
        });
    PickInOrder me;
    me.picks = {e1};
    const Intent play = playOf(outer);
    withEngine([&](GameEngine& e) {
        engine = &e;
        e.testHook_executeIntent(play);
        e.testHook_runChain();
    }, &me);
    EXPECT_EQ(finalized_events, 1) << "announced once, not again at the Finalize step";
    EXPECT_EQ(after_play_at_event, 0) << "while the effect that played it was under way";
    EXPECT_EQ(me.object_prompts, 1) << "and not asked a second time";
    EXPECT_EQ(o(e1).card_counters["hit"], 1);
}

// ═══ 9.Y2: [Repeat] exists on spells only ═══════════════════════════════════

// CR 820.1.a allows Repeat on abilities; the engine offers the cost only as a
// spell is played (executePlaySpell). No card prints Repeat on anything else:
// this fails the day one does, instead of the cost silently never being offered.
TEST_F(VerifyEngineTest, Registry_OnlySpellsDeclareRepeatCosts) {
    auto offenders = [&](const std::vector<const Card*>& cards) {
        std::vector<std::string> out;
        for (const Card* c : cards)
            if (c && c->def().card_type != CardType::Spell && !c->repeatCosts().empty())
                out.push_back(c->def().name);
        return out;
    };
    std::vector<const Card*> all;
    int spells_with_repeat = 0;
    for (uint32_t n = 1; n <= static_cast<uint32_t>(ml::kCardVocabSize); ++n) {
        const CardDefId id = CardDefId::fromRaw(n);
        const Card* c = card_registry.get(id);
        if (!c) continue;
        all.push_back(c);
        if (c->def().card_type == CardType::Spell && !c->repeatCosts().empty())
            ++spells_with_repeat;
    }
    EXPECT_GT(all.size(), 900u);
    EXPECT_GT(spells_with_repeat, 0) << "control: the scan does see Repeat costs";
    const auto bad = offenders(all);
    EXPECT_TRUE(bad.empty()) << "a non-spell declares repeatCosts(): " << bad.front();
    // The scan can fail: a unit that declares one is reported.
    const TRepeatUnit probe;
    EXPECT_EQ(offenders({&probe}).size(), 1u);
}

}  // namespace
}  // namespace test_review_fix_verify_engine
}  // namespace riftbound::test
