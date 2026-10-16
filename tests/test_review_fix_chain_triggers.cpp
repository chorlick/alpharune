/// @file test_review_fix_chain_triggers.cpp
/// Regression tests for the chain / trigger findings of the 2026-10 engine
/// review (plans/02-engine-rules.md items 2.4, 2.5, 2.10, 2.11, 2.13, 2.16,
/// 2.17, 2.32; plans/07 items 7.1, 7.4; plans/08 items 8.5, 8.9, 8.11, 8.15).
///
/// Wherever a real card shows the defect the test plays it through the real
/// GameEngine (TriggerManager + ChainManager + finalization). The ChainManager
/// safety bounds and the Deflect / owner bookkeeping have no card that reaches
/// them on demand, so those drive ChainManager::processFEPR directly.

#include "cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "ml/feature_extractor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace riftbound::test {
namespace test_review_fix_chain_triggers {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kFrigidTouch   = 389;  // [Repeat] [2]; "Give a unit -2 [M] this turn."
constexpr CardDefId kCalledShot    = 443;  // [Repeat] [P]; look at top 2, draw one, recycle the other
constexpr CardDefId kPunchFirst    = 419;  // "Give a unit +5 [M] this turn."
constexpr CardDefId kDredgeUp      = 836;  // [2] "Draw 1."
constexpr CardDefId kFizzTrickster = 461;  // plays a spell from trash, "Then recycle it."
constexpr CardDefId kBattleMistress = 517; // legend: "When one or more enemy units die, ready me."
constexpr CardDefId kLastRites     = 471;  // Equipment: "When I conquer or hold, you may play a unit from your trash."
constexpr CardDefId kSacredShears  = 493;  // Equipment: "[Deathknell] — Draw 1."
constexpr CardDefId kSpriteFountain = 640; // gear, [Temporary]; "[Deathknell][>] Repeat this gear's play effect."
constexpr CardDefId kScrapheap     = 182;  // gear: "When this is played, discarded, or killed, draw 1."
constexpr CardDefId kJaeMedarda    = 463;  // "When you choose me with a spell, draw 1."
constexpr CardDefId kIreliaFervent = 380;  // "When you choose or ready me, give me +1 [M] this turn."
constexpr CardDefId kStubShowdownBattlefield = 1900;  // unused id (reserved range), see below

/// Answers every decision by kind: "you may" / Repeat prompts with `say_yes`,
/// object choices with `want` in order (then the first option), anything else
/// by passing priority when it can.
class ScriptAgent : public AgentInterface {
public:
    bool say_yes = true;
    std::vector<GameObjectId> want;   // consumed front to back, one per object choice
    int value_asked = 0;
    int object_asked = 0;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        if (legal.empty()) return Intent{};
        const bool choice = legal.front().type == IntentType::MakeChoice;
        if (choice && legal.front().chosen_value.has_value()) {
            ++value_asked;
            for (const auto& i : legal)
                if (i.chosen_value == (say_yes ? 1 : 0)) return i;
            return legal.front();
        }
        if (choice && !legal.front().chosen_objects.empty()) {
            const size_t n = static_cast<size_t>(object_asked++);
            if (n < want.size())
                for (const auto& i : legal)
                    if (i.chosen_objects.front() == want[n]) return i;
            return legal.front();
        }
        for (const auto& i : legal) if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    }
};

/// A battlefield that triggers when a showdown begins there. No printed
/// battlefield does today (item 2.16 is latent), so the dispatch is exercised
/// with this stand-in, registered under an id no card uses.
class StubShowdownBattlefield : public BattlefieldCard {
public:
    static inline std::vector<PlayerId> resolved_for;
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenAShowdownBeginsHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        resolved_for.push_back(ctx.controller);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = kStubShowdownBattlefield;
        d.name = "Stub Showdown Battlefield";
        d.card_type = CardType::Battlefield;
        return d;
    }();
};

class ChainTriggerFixTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    ScriptAgent p1_, p2_;
    GameState& s() { return engine_->mutableState(); }

    void makeEngine(int n_bfs = 2) {
        engine_ = std::make_unique<GameEngine>(card_db, events, card_registry);
        auto& st = s();
        st.mode = ModeOfPlay{};
        st.players[0].id = P1;
        st.players[1].id = P2;
        for (int i = 0; i < n_bfs; ++i) {
            BattlefieldState bf;
            bf.id = BattlefieldId::fromRaw(st.battlefields.size());
            st.battlefields.push_back(bf);
        }
        engine_->testHook_setAgents(&p1_, &p2_);
        st.turn.turn_player = P1;
        st.turn.phase = TurnPhase::MainPhase;
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Open;
        setLegend(P1);
        setLegend(P2);
    }

    GameObjectId setLegend(PlayerId p, CardDefId def_id = kInvalidId) {
        auto id = s().createObject();
        auto& lo = s().getObject(id);
        lo.owner = lo.controller = p;
        lo.card_type = CardType::Legend;
        lo.card_def_id = def_id;
        if (def_id != kInvalidId) lo.name = card_db.get(def_id).name;
        lo.zone = ZoneType::LegendZone;
        s().player(p).legend_zone = id;
        return id;
    }

    /// An object of card `def_id` with the def's type, keywords and domains,
    /// in no zone yet.
    GameObjectId makeCard(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = s().getObject(id);
        const auto& def = card_db.get(def_id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
        c.card_type = def.card_type;
        c.super_type = def.super_type;
        c.name = def.name;
        c.domains = def.domains;
        c.keywords = def.keywords;
        c.tags = def.tags;
        c.base_might = c.current_might = def.might;
        return id;
    }
    GameObjectId cardInHand(PlayerId p, CardDefId def_id) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId cardInDeck(PlayerId p, CardDefId def_id) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::MainDeck;
        s().player(p).main_deck.push_back(id);
        return id;
    }
    GameObjectId cardInTrash(PlayerId p, CardDefId def_id) {
        auto id = makeCard(p, def_id);
        s().getObject(id).zone = ZoneType::Trash;
        s().player(p).trash.push_back(id);
        return id;
    }
    GameObjectId onBoardCard(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = makeCard(p, def_id);
        auto& c = s().getObject(id);
        if (at_bf >= 0) {
            c.zone = ZoneType::BattlefieldZone;
            c.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            c.zone = ZoneType::Base;
            c.location = BaseLocation{p};
        }
        return id;
    }
    GameObjectId plainUnit(PlayerId p, int might, int at_bf = -1) {
        auto id = s().createObject();
        auto& u = s().getObject(id);
        u.owner = u.controller = p;
        u.card_type = CardType::Unit;
        u.name = "TestUnit";
        u.base_might = u.current_might = might;
        if (at_bf >= 0) {
            u.zone = ZoneType::BattlefieldZone;
            u.location = BattlefieldLocation{BattlefieldId::fromRaw(at_bf)};
        } else {
            u.zone = ZoneType::Base;
            u.location = BaseLocation{p};
        }
        return id;
    }
    GameObjectId rune(PlayerId p, Domain d, bool exhausted = false) {
        auto id = s().createObject();
        auto& r = s().getObject(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = "Rune";
        r.domains = {d};
        r.zone = ZoneType::Base;
        r.location = BaseLocation{p};
        r.is_exhausted = exhausted;
        return id;
    }
    /// Attach `gear` to `unit` the way EffectExecutor::attachGear leaves them.
    void attach(GameObjectId gear, GameObjectId unit) {
        auto& g = s().getObject(gear);
        auto& u = s().getObject(unit);
        g.attached_to = unit;
        g.is_rules_text_inactive = true;   // CR 718.2
        g.location = u.location;
        g.zone = u.zone;
        u.attachments.push_back(gear);
    }
    void playFromHand(GameObjectId card, bool to_base = false) {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = card;
        if (to_base) play.play_location = LocationId{BaseLocation{P1}};
        engine_->testHook_executeIntent(play);
    }
    /// Resolves whatever triggered abilities are waiting on the chain: the
    /// Ending Step is the engine's "run the chain if there is one" seam.
    void drainChain() { engine_->testHook_endingStep(); }

    static int countOf(const std::vector<GameObjectId>& v, GameObjectId id) {
        return static_cast<int>(std::count(v.begin(), v.end(), id));
    }
    int exhaustedRunes(PlayerId p) {
        int n = 0;
        for (auto& [id, o] : s().objects)
            if (o.isRune() && o.controller == p && o.location.has_value() && o.is_exhausted) ++n;
        return n;
    }
};

// ─── 2.4 / 7.4 / 8.9 — a repeated effect that asks for a choice ─────────────

TEST_F(ChainTriggerFixTest, Repeat_RepeatedEffectGetsItsOwnChoiceAndHappens) {
    // Frigid Touch with its Repeat paid: the effect happens twice, and the
    // second execution may choose a different unit (CR 820.2.a).
    makeEngine();
    auto first  = plainUnit(P2, 5);
    auto second = plainUnit(P2, 5);
    p1_.want = {first, second};
    s().player(P1).rune_pool.energy = 2;                 // the base cost
    rune(P1, Domain::Mind); rune(P1, Domain::Mind);      // the Repeat [2]
    auto spell = cardInHand(P1, kFrigidTouch);

    playFromHand(spell);

    EXPECT_EQ(p1_.value_asked, 1) << "asked once whether to pay Repeat";
    // One target per execution, and the two runes of the Repeat's [2]: since
    // batch 8 (U11) the Repeat is paid in the spell's own cost payment, where
    // the payer names every rune (forced picks included), as for a card's cost.
    EXPECT_EQ(p1_.object_asked, 4) << "one target per execution, one prompt per rune";
    EXPECT_EQ(s().getObject(first).temp_might_bonus, -2);
    EXPECT_EQ(s().getObject(second).temp_might_bonus, -2) << "the repeated effect happened";
    EXPECT_FALSE(engine_->testHook_executor().hasPendingChoice()) << "no choice left behind";
    EXPECT_FALSE(s().chain.exists());
    EXPECT_EQ(countOf(s().player(P1).trash, spell), 1);
}

TEST_F(ChainTriggerFixTest, Repeat_HandRolledChoiceCardFinishesItsRepeat) {
    // Called Shot drives requestChoice itself: "Look at the top 2 cards of
    // your Main Deck. Draw one and recycle the other." Twice with Repeat.
    makeEngine();
    for (int i = 0; i < 4; ++i) cardInDeck(P1, kDredgeUp);
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;   // base cost [P]
    rune(P1, Domain::Chaos, /*exhausted=*/true);                            // Repeat [P]
    auto spell = cardInHand(P1, kCalledShot);

    playFromHand(spell);

    ASSERT_EQ(p1_.value_asked, 1) << "the Repeat cost was offered";
    // Each execution asks which card to draw, and the Repeat's [P] rune is
    // named in the spell's own cost payment (batch 8, U11).
    EXPECT_EQ(p1_.object_asked, 3) << "each execution asks which card to draw; one rune";
    EXPECT_EQ(s().player(P1).hand.size(), 2u) << "drew one card per execution";
    EXPECT_EQ(s().player(P1).main_deck.size(), 2u);
    EXPECT_FALSE(engine_->testHook_executor().hasPendingChoice())
        << "a stale request would be answered by the next item to resolve";
    EXPECT_FALSE(engine_->testHook_executor().hasRecordedChoice());
}

TEST_F(ChainTriggerFixTest, Repeat_CostCanBePaidOnlyOnce) {
    // CR 820.1.c.3: each Repeat cost can be paid only a single time.
    makeEngine();
    auto victim = plainUnit(P2, 9);
    s().player(P1).rune_pool.energy = 2;
    for (int i = 0; i < 6; ++i) rune(P1, Domain::Mind);   // enough for three payments
    auto spell = cardInHand(P1, kFrigidTouch);

    playFromHand(spell);

    EXPECT_EQ(p1_.value_asked, 1) << "the Repeat cost is offered once";
    EXPECT_EQ(exhaustedRunes(P1), 2) << "and paid once";
    EXPECT_EQ(s().getObject(victim).temp_might_bonus, -4) << "two executions, not four";
}

// ─── 2.5 — "Then recycle it" ────────────────────────────────────────────────

TEST_F(ChainTriggerFixTest, RecycleOnLeaveChain_SpellEndsUpOnlyInTheDeck) {
    // Fizz, Trickster: "you may play a spell from your trash ... Then recycle
    // it." The spell leaves the chain for the deck and is in no other zone.
    makeEngine();
    cardInDeck(P1, kPunchFirst);
    cardInDeck(P1, kPunchFirst);
    auto spell = cardInTrash(P1, kDredgeUp);
    s().player(P1).rune_pool.energy = 3;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;
    auto fizz = cardInHand(P1, kFizzTrickster);

    playFromHand(fizz, /*to_base=*/true);

    ASSERT_EQ(s().player(P1).hand.size(), 1u) << "Dredge Up resolved (drew 1)";
    EXPECT_EQ(countOf(s().player(P1).main_deck, spell), 1);
    EXPECT_EQ(countOf(s().player(P1).trash, spell), 0) << "never filed in the trash as well";
    EXPECT_EQ(s().getObject(spell).zone, ZoneType::MainDeck);
}

// ─── 2.32 / 8.16 — where a resolved spell goes (ChainManager) ───────────────

class ChainManagerFixTest : public CardTestFixture {
protected:
    struct Run {
        int chain_emptied = 0;
        int resolve_calls = 0;
    };
    /// processFEPR with both players passing and `on_resolve` as the spell /
    /// ability resolution callback.
    Run run(ChainManager& cm, const std::function<void(const ChainItem&)>& on_resolve) {
        Run r;
        auto conn = events.on_chain_emptied.connect(
            [&r](const ChainEmptiedEvent&) { ++r.chain_emptied; });
        cm.processFEPR(
            [](PlayerId, const std::vector<Intent>& legal) {
                return legal.empty() ? Intent{} : legal.front();
            },
            [](const ChainItem&) {},
            [&](const ChainItem& item) { ++r.resolve_calls; on_resolve(item); },
            nullptr);
        conn.disconnect();
        return r;
    }
    GameObjectId spellObject(PlayerId owner) {
        auto id = state.createObject();
        auto& o = state.getObject(id);
        o.owner = o.controller = owner;
        o.card_type = CardType::Spell;
        o.name = "TestSpell";
        return id;
    }
};

TEST_F(ChainManagerFixTest, ResolvedSpell_GoesToItsOwnersTrash) {
    // A card always goes to its owner's trash, whoever controlled the spell.
    EffectExecutor exec(state, events, card_db, &card_registry);
    ChainManager cm(state, events, card_db);
    cm.setEffectExecutor(&exec);
    auto spell = spellObject(P2);
    cm.addSpell(spell, /*controller=*/P1, {});

    run(cm, [](const ChainItem&) {});

    EXPECT_TRUE(inTrash(P2, spell));
    EXPECT_FALSE(inTrash(P1, spell));
}

TEST_F(ChainManagerFixTest, SpellThatBanishesItself_IsNotAlsoTrashed) {
    // "Banish this." as part of the effect (Arcane Shift, Time Warp): the
    // spell is in banishment when it finishes resolving and stays only there.
    EffectExecutor exec(state, events, card_db, &card_registry);
    ChainManager cm(state, events, card_db);
    cm.setEffectExecutor(&exec);
    auto spell = spellObject(P1);
    cm.addSpell(spell, P1, {});

    run(cm, [&](const ChainItem& item) { exec.banishObject(item.source); });

    EXPECT_EQ(state.getObject(spell).zone, ZoneType::Banishment);
    EXPECT_FALSE(inTrash(P1, spell));
    const auto& bz = state.player(P1).banishment;
    EXPECT_EQ(std::count(bz.begin(), bz.end(), spell), 1);
}

// ─── 2.10 — safety bounds ───────────────────────────────────────────────────

TEST_F(ChainManagerFixTest, SafetyCap_ChainThatNeverEmptiesIsAnError) {
    // Every resolution puts another ability on the chain. The loop's bound is
    // an engine error naming the newest item (CLAUDE.md "Engine bounds
    // throw"; finding F1): it used to drop the chain with a warning and let
    // the game go on, which hid a card that triggered on its own token.
    EffectExecutor exec(state, events, card_db, &card_registry);
    ChainManager cm(state, events, card_db);
    cm.setEffectExecutor(&exec);
    auto source = addUnit(P1, kInvalidId);
    auto spell = spellObject(P1);
    cm.addSpell(spell, P1, {});
    cm.addAbility(source, P1, kInvalidId);

    int resolve_calls = 0;
    try {
        run(cm, [&](const ChainItem& item) {
            ++resolve_calls;
            if (item.is_ability) cm.addAbility(source, P1, kInvalidId);
        });
        ADD_FAILURE() << "a chain that never empties ended quietly";
    } catch (const std::logic_error& ex) {
        const std::string what = ex.what();
        EXPECT_NE(what.find("did not empty"), std::string::npos) << what;
        EXPECT_NE(what.find(state.getObject(source).name.get()), std::string::npos)
            << "the newest item is named: " << what;
    }
    EXPECT_GT(resolve_calls, 100);
    EXPECT_TRUE(state.chain.exists()) << "nothing was dropped behind the game's back";
}

TEST_F(ChainManagerFixTest, SafetyCap_ACardThatNeverStopsAskingIsAnError) {
    // A card that asks for a choice on every re-entry never finishes. That is
    // an engine error, reported as one (std::logic_error) — the resolution is
    // not quietly abandoned with the card half-resolved.
    EffectExecutor exec(state, events, card_db, &card_registry);
    ChainManager cm(state, events, card_db);
    cm.setEffectExecutor(&exec);
    auto spell = spellObject(P1);
    cm.addSpell(spell, P1, {});

    int calls = 0;
    EXPECT_THROW(run(cm, [&](const ChainItem&) {
        ++calls;
        exec.takeChoice();
        Intent a; a.type = IntentType::MakeChoice; a.player = P1; a.chosen_value = 0;
        exec.requestChoice(P1, {a}, "again");
    }), std::logic_error);
    EXPECT_GT(calls, 64) << "a card may ask many questions before the bound";
}

// ─── 2.11 — Deflect is a cost of choosing ───────────────────────────────────

TEST_F(ChainManagerFixTest, Deflect_ChoiceThatCannotBePaidForIsNotMade) {
    // CR 809: choosing an opponent's [Deflect] unit costs [A]. With nothing
    // left to pay it (the spell's own cost used the last rune) the unit is
    // not chosen — it is not chosen for free.
    EffectExecutor exec(state, events, card_db, &card_registry);
    ChainManager cm(state, events, card_db);
    cm.setEffectExecutor(&exec);
    auto deflector = addUnit(P2, kInvalidId, 3, 0);
    state.getObject(deflector).keywords.set(Keyword::Deflect);
    state.getObject(deflector).deflect_value = 1;
    auto plain = addUnit(P2, kInvalidId, 3, 0);
    auto spell = spellObject(P1);
    ASSERT_EQ(exec.payableAnyPower(P1), 0);

    cm.addSpell(spell, P1, {deflector, plain});

    ASSERT_EQ(state.chain.items.size(), 1u);
    EXPECT_EQ(state.chain.items.back().targets, std::vector<GameObjectId>{plain});
    EXPECT_EQ(state.chain.items.back().chosen_objects, std::vector<GameObjectId>{plain});
}

TEST_F(ChainManagerFixTest, Deflect_PaidChoiceIsKept) {
    EffectExecutor exec(state, events, card_db, &card_registry);
    ChainManager cm(state, events, card_db);
    cm.setEffectExecutor(&exec);
    auto deflector = addUnit(P2, kInvalidId, 3, 0);
    state.getObject(deflector).keywords.set(Keyword::Deflect);
    state.getObject(deflector).deflect_value = 1;
    state.player(P1).rune_pool.universal_power = 1;
    auto source = addUnit(P1, kInvalidId);

    cm.addAbility(source, P1, kInvalidId, {deflector}, /*is_activated=*/true);

    ASSERT_EQ(state.chain.items.size(), 1u);
    EXPECT_EQ(state.chain.items.back().targets, std::vector<GameObjectId>{deflector});
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0) << "the [A] was paid";
}

// ─── 2.13 — legends watch unit deaths ───────────────────────────────────────

TEST_F(ChainTriggerFixTest, Legend_EnemyUnitDeathReachesTheLegend) {
    // Battle Mistress: "When one or more enemy units die, ready me."
    makeEngine();
    auto legend = setLegend(P1, kBattleMistress);
    s().getObject(legend).is_exhausted = true;
    auto enemy = plainUnit(P2, 2, 0);
    s().getObject(enemy).damage_marked = 2;      // lethal

    engine_->testHook_processLethalDamage();
    drainChain();

    ASSERT_FALSE(s().getObject(enemy).location.has_value());
    EXPECT_FALSE(s().getObject(legend).is_exhausted);
}

TEST_F(ChainTriggerFixTest, Legend_OwnUnitDeathDoesNotReadyBattleMistress) {
    makeEngine();
    auto legend = setLegend(P1, kBattleMistress);
    s().getObject(legend).is_exhausted = true;
    auto mine = plainUnit(P1, 2, 0);
    s().getObject(mine).damage_marked = 2;

    engine_->testHook_processLethalDamage();
    drainChain();

    EXPECT_TRUE(s().getObject(legend).is_exhausted);
}

// ─── 2.16 — a battlefield's showdown trigger has a controller ───────────────

TEST_F(ChainTriggerFixTest, ShowdownBattlefieldTrigger_IsControlledByARealPlayer) {
    // CR 190.6.a/b: the battlefield's controller controls its abilities; the
    // turn player does while it is uncontrolled. Never PlayerId::None.
    card_registry.registerCard(kStubShowdownBattlefield,
                               std::make_unique<StubShowdownBattlefield>());
    StubShowdownBattlefield::resolved_for.clear();
    makeEngine();
    for (auto& bf : s().battlefields) {
        auto id = s().createObject();
        auto& o = s().getObject(id);
        o.card_type = CardType::Battlefield;
        o.card_def_id = kStubShowdownBattlefield;
        o.name = "Stub Showdown Battlefield";
        bf.card_object_id = id;
    }
    s().battlefields[1].controller = P2;

    events.emit(ShowdownStartedEvent{s().battlefields[0].id, false});   // uncontrolled
    ASSERT_EQ(s().chain.items.size(), 1u);
    EXPECT_EQ(s().chain.items.back().controller, P1) << "the turn player";
    events.emit(ShowdownStartedEvent{s().battlefields[1].id, false});   // P2's
    ASSERT_EQ(s().chain.items.size(), 2u);
    EXPECT_EQ(s().chain.items.back().controller, P2) << "its controller";

    drainChain();
    EXPECT_EQ(StubShowdownBattlefield::resolved_for, (std::vector<PlayerId>{P2, P1}));
}

// ─── 2.17 / 8.15 — an Equipment's "When I ..." ability uses the chain ───────

class EquippedTriggerChainTest : public ChainTriggerFixTest {
protected:
    GameObjectId bearer_ = kInvalidId, gear_ = kInvalidId;
    /// P1 holds battlefield 0 with a unit wearing Last Rites.
    void setUpHold() {
        makeEngine();
        s().battlefields[0].controller = P1;
        bearer_ = plainUnit(P1, 3, 0);
        gear_ = onBoardCard(P1, kLastRites);
        attach(gear_, bearer_);
    }
    /// Floating Energy and [A] to cover the cost of a unit played by an effect
    /// that does not waive it (Last Rites: "You still pay its costs.").
    void fundPlay(PlayerId p) {
        s().player(p).rune_pool.energy += 10;
        s().player(p).rune_pool.universal_power += 4;
    }
};

TEST_F(EquippedTriggerChainTest, IsNotRunInsideEventDispatch) {
    // The ability goes on the chain when the unit holds; nothing has happened
    // yet when the event returns.
    setUpHold();
    auto in_trash = cardInTrash(P1, kJaeMedarda);

    engine_->testHook_scoreHold(P1, s().battlefields[0].id);

    ASSERT_EQ(s().chain.items.size(), 1u);
    const auto& item = s().chain.items.back();
    EXPECT_EQ(item.source, gear_);
    EXPECT_EQ(item.equipped_unit, bearer_);
    EXPECT_EQ(item.fired_trigger, TriggerType::WhenIConquerOrHold);
    EXPECT_TRUE(item.is_ability);
    EXPECT_FALSE(item.is_activated_ability);
    EXPECT_EQ(countOf(s().player(P1).trash, in_trash), 1) << "not resolved yet";
}

TEST_F(EquippedTriggerChainTest, YouMayIsARealChoice_Declined) {
    // Last Rites: "you MAY play a unit from your trash." Outside the chain
    // the helpers could only answer yes.
    setUpHold();
    auto in_trash = cardInTrash(P1, kJaeMedarda);
    fundPlay(P1);   // "(You still pay its costs.)": only offered if it can be paid for
    p1_.say_yes = false;

    engine_->testHook_scoringStep();

    EXPECT_EQ(p1_.value_asked, 1);
    EXPECT_EQ(countOf(s().player(P1).trash, in_trash), 1);
    EXPECT_FALSE(s().getObject(in_trash).location.has_value());
}

TEST_F(EquippedTriggerChainTest, TheControllerChoosesWhichUnit) {
    setUpHold();
    auto first  = cardInTrash(P1, kJaeMedarda);
    auto second = cardInTrash(P1, kIreliaFervent);
    fundPlay(P1);   // "(You still pay its costs.)"
    p1_.want = {second};

    engine_->testHook_scoringStep();

    EXPECT_EQ(p1_.value_asked, 2);   // the may, then where to play it (CR 355.2.a)
    EXPECT_EQ(p1_.object_asked, 1);
    EXPECT_TRUE(s().getObject(second).location.has_value()) << "the chosen unit was played";
    EXPECT_FALSE(s().getObject(first).location.has_value()) << "not simply the first in the trash";
}

TEST_F(EquippedTriggerChainTest, DeathknellFromEquipmentResolvesFromTheChain) {
    // Sacred Shears: "[Deathknell] — Draw 1." on the unit that wore it.
    makeEngine();
    cardInDeck(P1, kPunchFirst);
    auto bearer = plainUnit(P1, 2, 0);
    auto shears = onBoardCard(P1, kSacredShears);
    attach(shears, bearer);
    s().getObject(bearer).damage_marked = 2;

    engine_->testHook_processLethalDamage();
    EXPECT_EQ(s().player(P1).hand.size(), 0u) << "queued, not run during the death event";
    drainChain();

    EXPECT_EQ(s().player(P1).hand.size(), 1u);
}

// ─── 8.5 — a killed gear's own "when this dies" ─────────────────────────────

TEST_F(ChainTriggerFixTest, GearDeath_TemporaryGearGetsItsDeathknell) {
    // Sprite Fountain: [Temporary]; "[Deathknell][>] Repeat this gear's play
    // effect" (play a ready 3 [M] Sprite unit token).
    makeEngine();
    auto fountain = onBoardCard(P1, kSpriteFountain);
    ASSERT_TRUE(s().getObject(fountain).keywords.has(Keyword::Temporary));

    engine_->testHook_beginningStep();   // kills [Temporary] permanents, then runs the chain

    ASSERT_EQ(s().getObject(fountain).zone, ZoneType::Trash);
    int sprites = 0;
    for (auto& [id, o] : s().objects)
        if (o.isUnit() && o.name == "Sprite" && o.location.has_value()) ++sprites;
    EXPECT_EQ(sprites, 1);
}

TEST_F(ChainTriggerFixTest, GearDeath_KilledGearTriggers) {
    // Scrapheap: "When this is played, discarded, or killed, draw 1."
    makeEngine();
    cardInDeck(P1, kPunchFirst);
    auto heap = onBoardCard(P1, kScrapheap);

    engine_->testHook_executor().killObject(heap);
    drainChain();

    EXPECT_EQ(s().player(P1).hand.size(), 1u);
}

TEST_F(ChainTriggerFixTest, GearDeath_ALeavingGearThatWasNotKilledDoesNotTrigger) {
    makeEngine();
    cardInDeck(P1, kPunchFirst);
    auto heap = onBoardCard(P1, kScrapheap);

    engine_->testHook_executor().bounceToHand(heap);
    drainChain();

    EXPECT_EQ(countOf(s().player(P1).hand, heap), 1);
    EXPECT_EQ(s().player(P1).hand.size(), 1u) << "returned to hand is not killed: no draw";
}

// ─── 8.11 — the chosen unit sees its own choice ─────────────────────────────

TEST_F(ChainTriggerFixTest, ChosenUnit_JaeMedardaDrawsWhenChosenWithASpell) {
    makeEngine();
    cardInDeck(P1, kDredgeUp);
    auto jae = onBoardCard(P1, kJaeMedarda);
    p1_.want = {jae};
    s().player(P1).rune_pool.energy = 1;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 2;
    auto spell = cardInHand(P1, kPunchFirst);

    playFromHand(spell);

    EXPECT_EQ(s().getObject(jae).temp_might_bonus, 5);
    EXPECT_EQ(s().player(P1).hand.size(), 1u) << "When you choose me with a spell, draw 1.";
}

TEST_F(ChainTriggerFixTest, ChosenUnit_IreliaTriggersOnHerOwnChoice) {
    // "When you choose or ready me, give me +1 [M] this turn." Driven from the
    // choice announcement itself rather than a played spell: until item 2.6
    // is fixed a play-time-target spell announces its choice twice.
    makeEngine();
    auto irelia = onBoardCard(P1, kIreliaFervent);

    events.emit(ObjectStateChangedEvent{irelia, "chosen_by_controller"});

    ASSERT_EQ(s().chain.items.size(), 1u);
    EXPECT_EQ(s().chain.items.back().source, irelia);
    EXPECT_EQ(s().chain.items.back().triggering_subject, irelia);
    drainChain();
    EXPECT_EQ(s().getObject(irelia).temp_might_bonus, 1);
}

// ─── 7.1 — triggered abilities choose their targets themselves ──────────────
//
// The engine never passes targets to Card::onTrigger (plans/07 item 7.1): a
// triggered ability picks them with Card::pickTarget / pickTargetPair and
// opts into the finalize pass with kFinalizeTrigger. A card that declares
// target requirements for a trigger without opting in is waiting for targets
// that never arrive, so its effect silently does nothing.
//
// `kKnownOffenders` is the list from the review (7.1 and 7.2), still to be
// fixed card by card. It may only shrink: fix a card, then delete its id
// here. (Forge of the Future 212 is on the review's list too but declares no
// requirements, so this check cannot see it.)
//
// 9.B13: this first test is only a proxy (requirements declared + no opt-in).
// A card that reads `targets` without declaring requirements, and every
// onEquippedTrigger, passes it; the source scan below is the real check.

TEST(TargetlessTriggerAudit, NoNewCardWaitsForEngineSuppliedTriggerTargets) {
    static const std::set<CardDefId> kKnownOffenders = {
    };
    // Cards that declare requirements their trigger never reads; nothing
    // waits on the engine. (Empty: Void Burrower 553 dropped its dead
    // getTargetRequirements().)
    static const std::set<CardDefId> kRequirementsUnused = {};
    CardRegistry registry;
    registry.loadAll();
    std::vector<CardDefId> unexpected;
    std::vector<CardDefId> fixed;
    for (uint32_t n = 1; n <= static_cast<uint32_t>(ml::kCardVocabSize); ++n) {
        const CardDefId id = CardDefId::fromRaw(n);
        const Card* card = registry.get(id);
        if (!card) continue;
        const bool is_spell = card->def().card_type == CardType::Spell;
        // TriggerType::Activated marks an activated ability, whose targets the
        // engine does supply.
        bool has_trigger = false;
        for (auto t : card->triggerTypes())
            if (t != TriggerType::None && t != TriggerType::Activated) has_trigger = true;
        const bool waits = !is_spell && has_trigger && !kRequirementsUnused.count(id) &&
                           card->getTargetRequirements().count > 0 &&
                           !(card->choicesAtFinalization() & Card::kFinalizeTrigger);
        if (waits && !kKnownOffenders.count(id)) unexpected.push_back(id);
        if (!waits && kKnownOffenders.count(id)) fixed.push_back(id);
    }
    std::string list;
    for (auto id : unexpected) list += toString(id) + " ";
    EXPECT_TRUE(unexpected.empty())
        << "cards with a trigger and target requirements but no kFinalizeTrigger: " << list
        << "\nUse pickTarget + choicesAtFinalization() (see plans/07-cards-second-pass.md, 7.1).";
    std::string done;
    for (auto id : fixed) done += toString(id) + " ";
    if (!fixed.empty())
        std::cout << "[ note ] fixed; remove from kKnownOffenders: " << done << "\n";
}

// ─── 9.B13 — no card reads the `targets` of a trigger (source scan) ─────────
//
// "Does this card read the vector it is handed" cannot be asked of a compiled
// card, and the engine always hands a trigger an empty one, so the check is
// made on the source: every onTrigger / onEquippedTrigger DEFINITION under
// src/cards/<dir>/ whose vector parameter has a name must not use that name
// in its body, except to forward it to a base class's callback
// (`WeaponmasterUnit::onTrigger(ctx, targets)`).
//
// Known blind spots of the scan (kept simple on purpose):
//   - it is text, not a C++ parser: a callback produced by a macro, or one
//     whose parameter list or body hides behind a macro, is not seen;
//   - a card that forwards to a base class is trusted; the base class's own
//     definition is scanned only if it lives under src/cards/<dir>/;
//   - a lambda parameter or local in the body with the same name as the
//     vector parameter is reported as a read (a false positive: rename it);
//   - `(void)targets;` counts as a read: leave the parameter unnamed instead;
//   - a digit separator directly before a character literal (1'000'x') would
//     confuse the literal skipper; no card file has one.
namespace trigger_source_audit {

bool isIdent(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }
bool isSpace(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

/// `src` with comments, string literals (raw ones included) and character
/// literals replaced by spaces, newlines kept: the same offsets and line
/// numbers, and no brace, paren or name left in text that is not code.
std::string blankCommentsAndLiterals(const std::string& src) {
    std::string out = src;
    const size_t n = src.size();
    size_t i = 0;
    while (i < n) {
        const char c = src[i];
        const bool after_word = i > 0 && isIdent(src[i - 1]);
        size_t j = i + 1;
        if (c == '/' && i + 1 < n && src[i + 1] == '/') {
            j = src.find('\n', i);
            if (j == std::string::npos) j = n;
        } else if (c == '/' && i + 1 < n && src[i + 1] == '*') {
            j = src.find("*/", i + 2);
            j = (j == std::string::npos) ? n : j + 2;
        } else if (c == 'R' && i + 1 < n && src[i + 1] == '"' && !after_word) {
            const size_t open = src.find('(', i + 2);
            if (open == std::string::npos) break;
            const std::string close = ")" + src.substr(i + 2, open - (i + 2)) + "\"";
            j = src.find(close, open + 1);
            j = (j == std::string::npos) ? n : j + close.size();
        } else if (c == '"' || (c == '\'' && !after_word)) {
            j = i + 1;
            while (j < n && src[j] != c && src[j] != '\n') {
                if (src[j] == '\\') ++j;
                ++j;
            }
            j = std::min(j + 1, n);
        } else {
            ++i;
            continue;
        }
        for (size_t k = i; k < j && k < n; ++k)
            if (out[k] != '\n') out[k] = ' ';
        i = j;
    }
    return out;
}

size_t skipSpace(const std::string& s, size_t i) {
    while (i < s.size() && isSpace(s[i])) ++i;
    return i;
}

/// Position of the `close` that matches the `open` at `i`; npos if unmatched.
size_t matchingClose(const std::string& s, size_t i, char open, char close) {
    int depth = 0;
    for (; i < s.size(); ++i) {
        if (s[i] == open) ++depth;
        else if (s[i] == close && --depth == 0) return i;
    }
    return std::string::npos;
}

/// The identifier that ends just before `i` (spaces skipped); `begin` gets
/// where it starts. Empty if what is there is not an identifier.
std::string wordBefore(const std::string& s, size_t i, size_t* begin = nullptr) {
    while (i > 0 && isSpace(s[i - 1])) --i;
    const size_t end = i;
    while (i > 0 && isIdent(s[i - 1])) --i;
    if (begin) *begin = i;
    return s.substr(i, end - i);
}

/// Scan one source file. Returns the number of onTrigger / onEquippedTrigger
/// definitions seen and appends one "file:line: ..." entry to `findings` per
/// read of a named vector parameter.
int auditSource(const std::string& file, const std::string& raw,
                std::vector<std::string>& findings) {
    const std::string s = blankCommentsAndLiterals(raw);
    static const char* const kCallbacks[] = {"onTrigger", "onEquippedTrigger"};
    int definitions = 0;
    for (const std::string fn : kCallbacks) {
        for (size_t at = s.find(fn); at != std::string::npos; at = s.find(fn, at + fn.size())) {
            if (at > 0 && isIdent(s[at - 1])) continue;
            const size_t lp = skipSpace(s, at + fn.size());
            if (lp >= s.size() || s[lp] != '(') continue;
            const size_t rp = matchingClose(s, lp, '(', ')');
            if (rp == std::string::npos) continue;
            // A definition: the parameter list is followed by `{`, with only
            // specifiers in between. Anything else is a call or a declaration.
            size_t body = skipSpace(s, rp + 1);
            for (;;) {
                size_t j = body;
                while (j < s.size() && isIdent(s[j])) ++j;
                const std::string word = s.substr(body, j - body);
                if (word != "const" && word != "override" && word != "final" &&
                    word != "noexcept") break;
                body = skipSpace(s, j);
            }
            if (body >= s.size() || s[body] != '{') continue;
            ++definitions;

            // The vector parameter's name: the identifier its declaration ends
            // with. None for `const std::vector<GameObjectId>&` and for a
            // `/*commented*/` name (comments are blanked).
            std::string name;
            int depth = 0;
            size_t start = lp + 1;
            for (size_t q = lp + 1; q <= rp; ++q) {
                const char ch = s[q];
                if (ch == '<' || ch == '(' || ch == '{') ++depth;
                else if ((ch == '>' || ch == ')' || ch == '}') && q != rp) --depth;
                if (!((ch == ',' && depth == 0) || q == rp)) continue;
                const std::string param = s.substr(start, q - start);
                start = q + 1;
                if (param.find("vector") == std::string::npos) continue;
                const std::string word = wordBefore(param, param.size());
                if (!word.empty() && word != "const" &&
                    std::isdigit(static_cast<unsigned char>(word[0])) == 0) name = word;
            }
            if (name.empty()) continue;   // unnamed: it cannot be read

            size_t end = matchingClose(s, body, '{', '}');
            if (end == std::string::npos) end = s.size();
            std::vector<size_t> parens;   // open parens enclosing the scan position
            for (size_t q = body; q < end; ++q) {
                if (s[q] == '(') parens.push_back(q);
                else if (s[q] == ')' && !parens.empty()) parens.pop_back();
                if (s.compare(q, name.size(), name) != 0 || isIdent(s[q - 1]) ||
                    (q + name.size() < s.size() && isIdent(s[q + name.size()]))) continue;
                // Allowed: the name as a whole argument of `Base::onTrigger(...)`
                // / `Base::onEquippedTrigger(...)`.
                bool forwarded = false;
                if (!parens.empty()) {
                    size_t callee_begin = 0;
                    const std::string callee = wordBefore(s, parens.back(), &callee_begin);
                    size_t b = callee_begin;
                    while (b > 0 && isSpace(s[b - 1])) --b;
                    const bool base_call = b >= 2 && s[b - 2] == ':' && s[b - 1] == ':';
                    size_t before = q;
                    while (before > 0 && isSpace(s[before - 1])) --before;
                    const size_t after = skipSpace(s, q + name.size());
                    const bool whole_argument =
                        before > 0 && (s[before - 1] == '(' || s[before - 1] == ',') &&
                        after < s.size() && (s[after] == ')' || s[after] == ',');
                    forwarded = base_call && whole_argument &&
                                (callee == "onTrigger" || callee == "onEquippedTrigger");
                }
                if (!forwarded) {
                    const auto line = 1 + std::count(
                        s.begin(), s.begin() + static_cast<std::ptrdiff_t>(q), '\n');
                    findings.push_back(file + ":" + std::to_string(line) + ": " + fn +
                                       " reads `" + name + "`");
                }
                q += name.size() - 1;
            }
        }
    }
    return definitions;
}

}  // namespace trigger_source_audit

// The scan itself can fail: it flags a reader (of either callback, under any
// parameter name) and lets the three legitimate shapes through.
TEST(TargetlessTriggerAudit, TheSourceScanFlagsAReaderAndAllowsAForward) {
    using trigger_source_audit::auditSource;
    auto findingsIn = [](const std::string& src) {
        std::vector<std::string> out;
        EXPECT_EQ(auditSource("sample.cpp", src, out), 1) << "one definition in the sample";
        return out;
    };
    const std::string head = "struct A : UnitCard {\n    const char* t = R\"RB(a // b { ( )RB\";\n";
    // Reads the vector: both uses are reported, with their lines.
    auto reader = findingsIn(head +
        "    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {\n"
        "        if (!targets.empty()) use(ctx, targets[0]);\n"
        "    }\n};\n");
    ASSERT_EQ(reader.size(), 2u);
    EXPECT_EQ(reader[0], "sample.cpp:4: onTrigger reads `targets`");
    // An Equipment's callback, the parameter on its own line under another name.
    EXPECT_EQ(findingsIn(head +
        "    void onEquippedTrigger(CardContext& ctx, GameObjectId unit,\n"
        "                           const std::vector<GameObjectId>& chosen) override {\n"
        "        for (auto id : chosen) hit(ctx, unit, id);\n"
        "    }\n};\n").size(), 1u);
    // Handing it to anything but a base class's callback is a read.
    EXPECT_EQ(findingsIn(head +
        "    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {\n"
        "        helper(ctx, targets);\n"
        "    }\n};\n").size(), 1u);
    // Forwarded to the base class: fine.
    EXPECT_TRUE(findingsIn(head +
        "    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {\n"
        "        if (ctx.firing_trigger == TriggerType::WhenIAttack) return;\n"
        "        WeaponmasterUnit::onTrigger(ctx, targets);\n"
        "    }\n};\n").empty());
    // Unnamed, or named only in a comment: nothing to read, whatever the body
    // calls its own variables.
    EXPECT_TRUE(findingsIn(head +
        "    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {\n"
        "        std::vector<GameObjectId> targets;   // targets are never passed\n"
        "        log(\"targets\"); use(ctx, targets);\n"
        "    }\n};\n").empty());
    EXPECT_TRUE(findingsIn(head +
        "    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {\n"
        "        Base::onTrigger(ctx, {});\n"
        "    }\n};\n").empty());
}

TEST(TargetlessTriggerAudit, NoCardReadsTheTargetsItsTriggerIsHanded) {
    namespace fs = std::filesystem;
    const char* root = std::getenv("RIFTBOUND_ROOT");
    if (!root) {
        GTEST_SKIP() << "RIFTBOUND_ROOT is not set: the card sources (src/cards) cannot be "
                        "located, so the source audit did not run";
    }
    const fs::path cards = fs::path(root) / "src" / "cards";
    ASSERT_TRUE(fs::is_directory(cards))
        << cards.string() << " is not a directory: RIFTBOUND_ROOT must be the checkout root";

    std::vector<fs::path> files;
    for (const auto& dir : fs::directory_iterator(cards)) {
        if (!dir.is_directory()) continue;
        for (const auto& entry : fs::directory_iterator(dir.path())) {
            const auto ext = entry.path().extension();
            if (entry.is_regular_file() && (ext == ".cpp" || ext == ".h"))
                files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    int definitions = 0;
    std::vector<std::string> findings;
    for (const auto& path : files) {
        std::ifstream in(path, std::ios::binary);
        ASSERT_TRUE(in.good()) << "cannot read " << path.string();
        const std::string raw((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
        definitions += trigger_source_audit::auditSource(
            fs::relative(path, fs::path(root)).generic_string(), raw, findings);
    }
    // 438 when this was written; far fewer means the scan stopped recognising
    // definitions and would pass without having looked.
    EXPECT_GE(definitions, 400) << "onTrigger / onEquippedTrigger definitions found in "
                                << files.size() << " files";
    std::string list;
    for (const auto& f : findings) list += "\n  " + f;
    EXPECT_TRUE(findings.empty())
        << "the engine never passes targets to a trigger (the vector is always empty); "
           "these callbacks read it:" << list
        << "\nChoose with pickTarget / pickTargetPair under kFinalizeTrigger and leave the "
           "parameter unnamed (plans/07-cards-second-pass.md, 7.1).";
}

}  // namespace
}  // namespace test_review_fix_chain_triggers
}  // namespace riftbound::test
