/// @file test_review_fix_verify_engine.cpp
/// Regression tests for the 2026-10 engine review, stage 3, stream
/// sweep-verify-engine: the engine checks what its caller hands it (plans/09
/// items 9.G4, 9.G5, 9.I6, 9.C3), the card-facing engine gaps (9.A13, 9.A19,
/// 9.A21, 9.A22, 9.D3, 9.G1, 9.G8, 9.G12, 9.C5) and the dead-code remainder of
/// plans/05 items 5.13 / 5.14.
/// Every test drives a real GameEngine: decisions are asked through the
/// engine's agents, triggers go through TriggerManager and the chain.

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

// ─── Test-only cards (ids in the reserved 956..1100 range) ──────────────────

constexpr CardDefId kTVanilla = 1080;        // unit, free, 2 [M]
constexpr CardDefId kTTargetReader = 1081;   // unit: trigger that records targets.size()
constexpr CardDefId kTGearReader = 1082;     // Equipment: equipped trigger, same
constexpr CardDefId kTSplitSpell = 1083;     // spell: deal 20 split among enemy units
constexpr CardDefId kTManySpell = 1084;      // spell: selectAnyNumber of friendly units
constexpr CardDefId kTAbilityUnit = 1085;    // unit with "[E]: draw 1"

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
        auto picked = selectAnyNumber(ctx, "Test Many", mine);
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
    EXPECT_THROW(fireTriggerThroughEngine(dragon, TriggerType::WhenYouPlayMe, &liar),
                 std::invalid_argument);
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
        EXPECT_THROW(e.testHook_runCombat(0), std::invalid_argument);
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
    EnginePathAgent agent(EnginePathAgent::choosing(kill));
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
    EXPECT_EQ(std::get<BattlefieldLocation>(where.locations[1]).id, 1);
    EXPECT_EQ(tokenPlayDests(state, P1), (std::vector<int>{0, 2}));
    EXPECT_EQ(tokenPlayLocations(state, P1).size(), 2u);
    // A hidden card's battlefield (CR 811.1.d.5) where units can't be played:
    // nowhere at all.
    EXPECT_EQ(playLocations(state, P1, BattlefieldId{0}).count(), 0);
    EXPECT_EQ(playLocations(state, P1, BattlefieldId{1}).count(), 1);
    EXPECT_EQ(playLocations(state, P1, std::nullopt, /*include_base=*/false).count(), 1);

    // "Units can only be played to base" (Mageseeker Warden): no battlefield.
    state.battlefields[0].blocks_unit_play = false;
    ps(P1).units_play_base_only = true;
    EXPECT_EQ(playLocations(state, P1).count(), 1);
    EXPECT_EQ(tokenPlayDests(state, P1), (std::vector<int>{0}));
    EXPECT_EQ(playLocations(state, P1, BattlefieldId{1}).count(), 0);
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
        EXPECT_EQ(unitsAt(0), 1);
        EXPECT_EQ(unitsAt(1), 2);
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
        const int before = unitsAt(1);
        playThroughEngine(arise, &agent);
        ASSERT_GE(agent.prompts.size(), 2u);
        EXPECT_EQ(agent.prompts[0].size(), 2u) << "base and battlefield 1 only";
        EXPECT_EQ(agent.prompts[1].size(), 2u);
        EXPECT_EQ(unitsAt(0), 1);
        EXPECT_EQ(unitsAt(1), before + 2);
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
    o(fan).card_counters["__defend_attacker_id"] = static_cast<int>(attacker);
    o(fan).banish_if_would_die_this_turn = true;
    state.battlefields[0].combat_in_progress = true;
    state.battlefields[0].attacker = P2;
    state.battlefields[0].defender = P1;

    AcceptingEnginePathAgent agent;
    fireTriggerThroughEngine(fan, TriggerType::WhenIDefend, &agent);
    ASSERT_FALSE(agent.prompts.empty()) << "the may was offered";
    EXPECT_FALSE(onBoard(fan));
    EXPECT_EQ(o(fan).zone, ZoneType::Banishment);
    EXPECT_EQ(o(attacker).battlefieldId(), std::optional<BattlefieldId>(0))
        << "the cost was not paid: the attacker stays";
}

// Control: a Fan that does die moves the attacker.
TEST_F(VerifyEngineTest, OverzealousFan_DyingMovesTheAttacker) {
    state.battlefields[0].controller = P1;
    auto fan = addUnit(P1, kOverzealousFan, 0, 0);
    auto attacker = addVanilla(P2, 3, 0);
    o(fan).combat_designation = CombatDesignation::Defender;
    o(attacker).combat_designation = CombatDesignation::Attacker;
    o(fan).card_counters["__defend_attacker_id"] = static_cast<int>(attacker);
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
        for (GameObjectId id = 10; id < 50; ++id) ids.push_back(id);
        if (!ascending) std::reverse(ids.begin(), ids.end());
        for (auto id : ids) {
            GameObject obj;
            obj.id = id;
            obj.owner = obj.controller = (id % 2 == 0) ? P1 : P2;
            obj.card_type = (id % 5 == 0) ? CardType::Gear : CardType::Unit;
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

// splitDamageAmong: 20 damage is 20 picks of 1, so any split can be expressed
// (the old cap made each pick worth 2 above 12 damage).
TEST_F(VerifyEngineTest, SplitDamage_AboveTwelveIsStillAssignedOnePointAtATime) {
    auto a = addVanilla(P2, 30);
    auto b = addVanilla(P2, 30);
    auto spell = addToHand(P1, kTSplitSpell);
    int n = 0;
    EnginePathAgent agent([&](const std::vector<Intent>& legal) {
        // 13 to a, then 7 to b: an odd split.
        const GameObjectId want = n++ < 13 ? a : b;
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == want) return i;
        return legal.front();
    });
    playThroughEngine(spell, &agent);
    EXPECT_EQ(static_cast<int>(agent.prompts.size()), 20);
    EXPECT_EQ(o(a).damage_marked, 13);
    EXPECT_EQ(o(b).damage_marked, 7);
}

// selectAnyNumber: more than the old ten picks.
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
        e.testHook_runCombat(0);
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
        e.testHook_executor().replaceBattlefieldWithToken(0, "Brush", P1, kBrush);
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

}  // namespace
}  // namespace test_review_fix_verify_engine
}  // namespace riftbound::test
