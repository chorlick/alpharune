/// @file test_review_fix_c0.cpp
/// Regression tests for the 2026-10 card review, stream c0: every battlefield
/// card and gear 17..387 (plans/cards/batch-00.md and the plan items that
/// point into that range). Each test drives the real engine path — a trigger
/// queued through TriggerManager and resolved off the chain, an ability
/// activated through executeIntent, an aura through cleanup — and names the
/// card it pins.

#include "card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace test_review_fix_c0 {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kIronBallista = 17;
constexpr CardDefId kSunDisc = 21;
constexpr CardDefId kUnlicensedArmory = 23;
constexpr CardDefId kMaskOfForesight = 60;
constexpr CardDefId kSpiritsRefuge = 63;
constexpr CardDefId kSolariShrine = 72;
constexpr CardDefId kGarbageGrabber = 99;
constexpr CardDefId kMushroomPouch = 101;
constexpr CardDefId kArenaBar = 124;
constexpr CardDefId kPiratesHaven = 143;
constexpr CardDefId kMistfall = 152;
constexpr CardDefId kPrimalStrength = 154;     // spell, [4][O]: "Give a unit +7 [M] this turn."
constexpr CardDefId kPackOfWonders = 181;
constexpr CardDefId kScrapheap = 182;
constexpr CardDefId kTheSyren = 184;
constexpr CardDefId kTreasureTrove = 186;
constexpr CardDefId kForgeOfTheFuture = 212;
constexpr CardDefId kBaitedHook = 242;
constexpr CardDefId kRecruit = 266;            // unit, 1 [M]
constexpr CardDefId kBackAlleyBar = 272;
constexpr CardDefId kFortifiedPosition = 274;
constexpr CardDefId kHallowedTomb = 276;
constexpr CardDefId kMonasteryOfHirana = 277;
constexpr CardDefId kNavoriFightingPit = 278;
constexpr CardDefId kSigilOfTheStorm = 282;
constexpr CardDefId kTargonsPeak = 284;
constexpr CardDefId kCandlelitSanctum = 286;
constexpr CardDefId kDreamingTree = 287;
constexpr CardDefId kTrifarianWarCamp = 289;
constexpr CardDefId kVoidGate = 291;
constexpr CardDefId kGold = 326;
constexpr CardDefId kRecurveBow = 339;
constexpr CardDefId kAssemblyRig = 342;
constexpr CardDefId kLongSword = 345;
constexpr CardDefId kDoransShield = 356;
constexpr CardDefId kBrutalizer = 365;
constexpr CardDefId kPoroSnax = 369;
constexpr CardDefId kPurifier = 502;           // legend
constexpr CardDefId kHeartOfDarkIce = 375;
constexpr CardDefId kForgeOfTheFluft = 522;
constexpr CardDefId kOrnnsForge = 527;
constexpr CardDefId kSunkenTemple = 532;
constexpr CardDefId kVeiledTemple = 535;
constexpr CardDefId kBaronPit = 558;
constexpr CardDefId kBrush = 561;
constexpr CardDefId kAltarOfBlood = 762;
constexpr CardDefId kAmateurRecital = 763;
constexpr CardDefId kBlackFlameAltar = 764;
constexpr CardDefId kDuskRoseLab = 765;
constexpr CardDefId kForbiddingWaste = 766;
constexpr CardDefId kGardensOfBecoming = 769;
constexpr CardDefId kStarSpring = 771;
constexpr CardDefId kTheAcademy = 772;
constexpr CardDefId kValleyOfIdols = 774;
constexpr CardDefId kKinkouTemple = 946;
constexpr CardDefId kRisenAltar = 950;

class C0Test : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }

    /// Put `def_id` in battlefield slot `bf`. The card object's controller is
    /// who the fixture's trigger helper fires its abilities for.
    GameObjectId battlefield(int bf, CardDefId def_id,
                             std::optional<PlayerId> controller = P1) {
        auto id = state.createObject();
        auto& b = o(id);
        const auto& def = card_db.get(def_id);
        b.card_def_id = def_id;
        b.card_type = CardType::Battlefield;
        b.name = def.name;
        b.owner = b.controller = controller.value_or(PlayerId::None);
        b.zone = ZoneType::BattlefieldZone;
        state.battlefields[bf].card_object_id = id;
        state.battlefields[bf].controller = controller;
        return id;
    }
    /// A gear from its def, ready in its controller's base.
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
        g.deflect_value = def.deflect_value;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{p};
        return id;
    }
    /// A gear attached to `unit`, the way the engine leaves it.
    GameObjectId attached(CardDefId def_id, GameObjectId unit) {
        auto g = gear(o(unit).controller, def_id);
        o(g).attached_to = unit;
        o(g).location = o(unit).location;
        o(g).zone = o(unit).zone;
        o(unit).attachments.push_back(g);
        o(unit).attachment_might_bonus += o(g).might_bonus;
        o(unit).recomputeMight();
        return g;
    }
    GameObjectId legend(PlayerId p) {
        auto id = state.createObject();
        auto& l = o(id);
        l.owner = l.controller = p;
        l.card_def_id = kPurifier;       // any legend: abilities need a card behind them
        l.card_type = CardType::Legend;
        l.name = "TestLegend";
        l.zone = ZoneType::LegendZone;
        state.player(p).legend_zone = id;
        return id;
    }
    GameObjectId trashCard(PlayerId p, CardDefId def_id) {
        auto id = addToHand(p, def_id);
        state.player(p).hand.pop_back();
        o(id).zone = ZoneType::Trash;
        state.player(p).trash.push_back(id);
        return id;
    }
    void cleanup() {
        withEngine([](GameEngine& e) { e.testHook_cleanup(); });
    }
    void combatAt(int bf, PlayerId attacker) {
        auto& b = state.battlefields[bf];
        b.combat_in_progress = true;
        b.attacker = attacker;
        b.defender = opponent(attacker);
    }
    /// Activate an ability of `source` through the engine (cost, chain,
    /// finalization, resolution).
    void activate(GameObjectId source, AgentInterface* agent = nullptr,
                  std::vector<GameObjectId> targets = {}, CardDefId granted = kInvalidId) {
        Intent act;
        act.type = IntentType::ActivateAbility;
        act.player = o(source).controller;
        act.ability_source = source;
        act.targets = std::move(targets);
        act.granted_ability_def = granted;
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(act); settleLikeTheEngine(e); }, agent, nullptr,
                   act.player);
    }
    /// Play a spell from hand at `target`, cost paid from the floating pool
    /// the test set up.
    void playAt(GameObjectId spell, GameObjectId target, AgentInterface* agent = nullptr) {
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = o(spell).controller;
        play.card = spell;
        play.targets = {target};
        withEngine([&](GameEngine& e) { e.testHook_executeIntent(play); settleLikeTheEngine(e); }, agent, nullptr,
                   play.player);
    }
    /// Queue `source`'s trigger and run the chain. `probe` is handed the live
    /// state the first time the opponent gets priority — the ability has been
    /// finalized (choices made, costs paid) and has not resolved yet.
    void fireAndProbe(GameObjectId source, TriggerType trigger, AgentInterface* p1_agent,
                      const std::function<void(GameState&)>& probe,
                      GameObjectId subject = kInvalidId) {
        GameState* live = nullptr;
        bool probed = false;
        PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
            if (!probed && live) { probed = true; probe(*live); }
            for (const auto& i : legal)
                if (i.type == IntentType::PassPriority) return i;
            return legal.front();
        });
        withEngine([&](GameEngine& e) {
            live = &e.mutableState();
            e.testHook_executor().queueTriggeredAbility(source, P1, trigger, subject);
            e.testHook_runChain();
            live = nullptr;
        }, p1_agent, &p2);
        EXPECT_TRUE(probed) << "the opponent never got priority";
    }
    int might(GameObjectId id) { return o(id).current_might; }
    int aurasFrom(GameObjectId unit, GameObjectId source) {
        int n = 0;
        for (const auto& ae : o(unit).aura_effects) if (ae.source == source) ++n;
        return n;
    }
    static EnginePathAgent::Picker yesThen(GameObjectId id) { return EnginePathAgent::choosing(id); }
    static EnginePathAgent::Picker sayingNo() {
        return [](const std::vector<Intent>& legal) {
            for (const auto& i : legal)
                if (i.chosen_value.has_value() && *i.chosen_value == 0) return i;
            return legal.front();
        };
    }
};

// ═══ Battlefields ═══════════════════════════════════════════════════════════

TEST_F(C0Test, BackAlleyBar_OnlyAUnitStillOnTheBoardGetsTheMight) {
    auto bar = battlefield(0, kBackAlleyBar);
    auto moved = addUnit(P1, kInvalidId, 2);          // now in base, having left the Bar
    fireTriggerThroughEngine(bar, TriggerType::WhenAUnitMovesFromHere, nullptr, moved);
    EXPECT_EQ(o(moved).temp_might_bonus, 1);

    auto dead = addUnit(P1, kInvalidId, 2);
    o(dead).location = std::nullopt;                  // died after moving
    o(dead).zone = ZoneType::Trash;
    fireTriggerThroughEngine(bar, TriggerType::WhenAUnitMovesFromHere, nullptr, dead);
    EXPECT_EQ(o(dead).temp_might_bonus, 0) << "a card in the trash is not given Might";
}

TEST_F(C0Test, FortifiedPosition_OffersEveryUnit) {
    // "choose a unit": not only the defender's own (batch-08).
    auto pos = battlefield(0, kFortifiedPosition);
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    auto theirs = addUnit(P2, kInvalidId, 2, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(theirs));
    fireTriggerThroughEngine(pos, TriggerType::WhenYouDefendHere, &agent);
    EXPECT_TRUE(agent.wasOffered(mine));
    EXPECT_TRUE(agent.wasOffered(theirs));
    EXPECT_TRUE(o(theirs).keywords.has(Keyword::Shield));
    EXPECT_FALSE(o(mine).keywords.has(Keyword::Shield));
}

TEST_F(C0Test, HallowedTomb_ReturnsTheChosenChampionAndCanBeDeclined) {
    auto tomb = battlefield(0, kHallowedTomb);
    // A Main Deck copy of the champion sits in the trash ahead of the Chosen
    // Champion itself: only the latter may come back.
    auto copy = trashCard(P1, kInvalidId);
    o(copy).card_type = CardType::Unit;
    o(copy).super_type = SuperType::Champion;
    auto chosen = trashCard(P1, kInvalidId);
    o(chosen).card_type = CardType::Unit;
    o(chosen).super_type = SuperType::Champion;
    state.player(P1).chosen_champion = chosen;

    EnginePathAgent no(sayingNo());
    fireTriggerThroughEngine(tomb, TriggerType::WhenYouHoldHere, &no);
    ASSERT_EQ(no.prompts.size(), 1u) << "\"you may\"";
    EXPECT_EQ(state.player(P1).champion_zone, kInvalidId);
    EXPECT_TRUE(inTrash(P1, chosen));

    EnginePathAgent yes(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(tomb, TriggerType::WhenYouHoldHere, &yes);
    EXPECT_EQ(state.player(P1).champion_zone, chosen);
    EXPECT_FALSE(inTrash(P1, chosen));
    EXPECT_TRUE(inTrash(P1, copy)) << "not just any champion unit";
    EXPECT_EQ(o(chosen).zone, ZoneType::ChampionZone);

    // Zone occupied: nothing to offer.
    EnginePathAgent again;
    fireTriggerThroughEngine(tomb, TriggerType::WhenYouHoldHere, &again);
    EXPECT_TRUE(again.prompts.empty());
}

TEST_F(C0Test, MonasteryOfHirana_SpendsTheBuffThePlayerPicks) {
    auto monastery = battlefield(0, kMonasteryOfHirana);
    addToDeck(P1, kRecruit);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    auto b = addUnit(P1, kInvalidId, 2, 0);
    o(a).buff_count = o(b).buff_count = 1;
    EnginePathAgent agent(EnginePathAgent::choosing(b, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(monastery, TriggerType::WhenYouConquerHere, &agent);
    EXPECT_TRUE(agent.wasOffered(a));
    EXPECT_EQ(o(b).buff_count, 0);
    EXPECT_EQ(o(a).buff_count, 1);
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(C0Test, MonasteryOfHirana_TheBuffIsSpentBeforeAnyoneCanRespond) {
    // CR 383.3: the cost of a "may [cost] to [effect]" is paid as the ability
    // is finalized, so it is gone while the ability still waits on the chain.
    auto monastery = battlefield(0, kMonasteryOfHirana);
    addToDeck(P1, kRecruit);
    auto a = addUnit(P1, kInvalidId, 2, 0);
    o(a).buff_count = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireAndProbe(monastery, TriggerType::WhenYouConquerHere, &agent, [&](GameState& st) {
        EXPECT_EQ(st.getObject(a).buff_count, 0) << "paid at finalization";
        EXPECT_TRUE(st.player(P1).hand.empty()) << "not yet resolved";
    });
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_EQ(o(a).buff_count, 0) << "and not spent a second time at resolution";
}

TEST_F(C0Test, NavoriFightingPit_BuffsTheUnitHereThePlayerPicks) {
    auto pit = battlefield(0, kNavoriFightingPit);
    auto here = addUnit(P1, kInvalidId, 2, 0);
    auto also_here = addUnit(P2, kInvalidId, 2, 0);
    auto elsewhere = addUnit(P1, kInvalidId, 2, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(also_here));
    fireTriggerThroughEngine(pit, TriggerType::WhenYouHoldHere, &agent);
    EXPECT_TRUE(agent.wasOffered(here));
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "\"a unit here\"";
    EXPECT_EQ(o(also_here).buff_count, 1);
    EXPECT_EQ(o(here).buff_count, 0);
}

TEST_F(C0Test, SigilOfTheStorm_ThePlayerPicksTheRune) {
    auto sigil = battlefield(0, kSigilOfTheStorm);
    auto fury = addRune(P1, Domain::Fury);
    auto calm = addRune(P1, Domain::Calm);
    EnginePathAgent agent(EnginePathAgent::choosing(calm));
    fireTriggerThroughEngine(sigil, TriggerType::WhenYouConquerHere, &agent);
    EXPECT_TRUE(agent.wasOffered(fury));
    EXPECT_EQ(o(calm).zone, ZoneType::RuneDeck);
    EXPECT_TRUE(o(fury).location.has_value());
}

TEST_F(C0Test, TargonsPeak_ThePlayerPicksWhichTwoRunesReady) {
    auto peak = battlefield(0, kTargonsPeak);
    auto r1 = addRune(P1, Domain::Fury, /*exhausted=*/true);
    auto r2 = addRune(P1, Domain::Calm, /*exhausted=*/true);
    auto r3 = addRune(P1, Domain::Mind, /*exhausted=*/true);
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(r3), static_cast<int>(r2)}));
    fireTriggerThroughEngine(peak, TriggerType::AtEndOfTurn, &agent);
    EXPECT_EQ(agent.prompts.size(), 2u);
    EXPECT_TRUE(o(r1).is_exhausted);
    EXPECT_FALSE(o(r2).is_exhausted);
    EXPECT_FALSE(o(r3).is_exhausted);
}

TEST_F(C0Test, CandlelitSanctum_KeptCardsGoBackInTheOrderThePlayerChooses) {
    // "Put those you don't [recycle] back in any order."
    auto sanctum = battlefield(0, kCandlelitSanctum);
    addToDeck(P1, kRecruit);
    auto second = addToDeck(P1, kRecruit);
    auto top = addToDeck(P1, kRecruit);

    // Keep both, take the default order: they go back as they were (they used
    // to come back swapped).
    EnginePathAgent keep_as_is([](const std::vector<Intent>& legal) {
        for (const auto& i : legal) if (i.chosen_objects.empty()) return i;   // "keep"
        return legal.front();
    });
    fireTriggerThroughEngine(sanctum, TriggerType::WhenYouConquerHere, &keep_as_is);
    EXPECT_EQ(keep_as_is.prompts.size(), 3u) << "keep/recycle twice, then the order";
    ASSERT_EQ(deckSize(P1), 3);
    EXPECT_EQ(state.player(P1).main_deck.back(), top);

    // Keep both and put the second card on top.
    EnginePathAgent reorder([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal) if (i.chosen_objects.empty()) return i;   // "keep"
        for (const auto& i : legal)
            if (i.chosen_objects.front() == second) return i;                 // the order
        return legal.front();
    });
    fireTriggerThroughEngine(sanctum, TriggerType::WhenYouConquerHere, &reorder);
    ASSERT_EQ(deckSize(P1), 3);
    EXPECT_EQ(state.player(P1).main_deck.back(), second);
    EXPECT_EQ(state.player(P1).main_deck[1], top);
}

TEST_F(C0Test, DreamingTree_OnlyASpellsChoiceDraws) {
    battlefield(0, kDreamingTree);
    addToDeck(P1, kRecruit);
    addToDeck(P1, kRecruit);
    auto unit = addUnit(P1, kInvalidId, 2, 0);

    // An ability chooses the unit: no draw ("with a spell").
    auto heart = gear(P1, kHeartOfDarkIce);
    EnginePathAgent pick(EnginePathAgent::choosing(unit));
    activate(heart, &pick);
    ASSERT_EQ(o(unit).temp_might_bonus, 3) << "Heart of Dark Ice chose the unit";
    EXPECT_EQ(handSize(P1), 0);

    // A spell chooses it: its player draws 1.
    auto spell = addToHand(P1, kPrimalStrength);
    state.player(P1).rune_pool.energy = 4;
    state.player(P1).rune_pool.universal_power = 1;
    playAt(spell, unit);
    ASSERT_EQ(o(unit).temp_might_bonus, 10);
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(C0Test, SameBattlefieldTwice_EachCopyAppliesOnce) {
    // A battlefield's hook runs once per slot holding the card; when both
    // players bring the same battlefield every effect used to be doubled.
    struct Row { CardDefId def; int want_might; };
    for (const Row& row : {Row{kTrifarianWarCamp, 3}, Row{kKinkouTemple, 3}}) {
        SCOPED_TRACE(card_db.get(row.def).name);
        auto b0 = battlefield(0, row.def);
        auto b1 = battlefield(1, row.def, P2);
        auto u0 = addUnit(P1, kInvalidId, 2, 0);
        auto u1 = addUnit(P2, kInvalidId, 2, 1);
        o(u0).keywords.set(Keyword::Tank);
        o(u1).keywords.set(Keyword::Tank);
        cleanup();
        EXPECT_EQ(might(u0), row.want_might);
        EXPECT_EQ(might(u1), row.want_might);
        EXPECT_EQ(aurasFrom(u0, b0), 1);
        EXPECT_EQ(aurasFrom(u0, b1), 0);
        EXPECT_EQ(aurasFrom(u1, b1), 1);
        o(u0).location = std::nullopt;   // out of the way of the next row
        o(u1).location = std::nullopt;
    }
}

TEST_F(C0Test, SameBattlefieldTwice_VoidGateWasteAltarAndGardens) {
    {
        battlefield(0, kVoidGate);
        battlefield(1, kVoidGate, P2);
        auto u = addUnit(P1, kInvalidId, 2, 1);
        cleanup();
        EXPECT_EQ(o(u).aura_bonus_damage_taken, 1);
        o(u).location = std::nullopt;
    }
    {
        battlefield(0, kForbiddingWaste);
        battlefield(1, kForbiddingWaste, P2);
        auto u = addUnit(P2, kInvalidId, 5, 1);
        o(u).combat_designation = CombatDesignation::Defender;
        auto att = addUnit(P1, kInvalidId, 2, 1);
        o(att).combat_designation = CombatDesignation::Attacker;
        combatAt(1, P1);
        cleanup();
        o(att).location = std::nullopt;
        state.battlefields[1].combat_in_progress = false;
        EXPECT_EQ(might(u), 3) << "-2 once";
        o(u).location = std::nullopt;
    }
    {
        auto b0 = battlefield(0, kBlackFlameAltar);
        battlefield(1, kBlackFlameAltar, P2);
        auto u = addUnit(P1, kInvalidId, 2, 0);
        o(u).keywords.set(Keyword::Temporary);
        o(u).combat_designation = CombatDesignation::Defender;
        auto att = addUnit(P2, kInvalidId, 2, 0);
        o(att).combat_designation = CombatDesignation::Attacker;
        combatAt(0, P2);
        cleanup();
        o(att).location = std::nullopt;
        state.battlefields[0].combat_in_progress = false;
        EXPECT_EQ(aurasFrom(u, b0), 1);
        EXPECT_EQ(might(u), 3) << "[Shield]: +1 while defending, once";
        o(u).location = std::nullopt;
    }
    {
        battlefield(0, kGardensOfBecoming);
        battlefield(1, kGardensOfBecoming, P2);
        auto u0 = addUnit(P1, kInvalidId, 2, 0);
        auto u1 = addUnit(P2, kInvalidId, 2, 1);
        cleanup();
        EXPECT_EQ(o(u0).granted_abilities.size(), 1u);
        EXPECT_EQ(o(u1).granted_abilities.size(), 1u) << "the second Gardens grants too";
    }
}

TEST_F(C0Test, SameBattlefieldTwice_ControllerScopedDiscounts) {
    {
        battlefield(0, kRisenAltar);
        battlefield(1, kRisenAltar, P2);
        addUnit(P1, kInvalidId, 2, 0);   // holds control through cleanup
        addUnit(P2, kInvalidId, 2, 1);
        cleanup();
        EXPECT_EQ(state.battlefields[0].empower_cost_reduction[0], 1);
        EXPECT_EQ(state.battlefields[0].empower_cost_reduction[1], 0) << "\"your units here\"";
        EXPECT_EQ(state.battlefields[1].empower_cost_reduction[1], 1);
        EXPECT_EQ(state.battlefields[1].empower_cost_reduction[0], 0);
    }
    {
        battlefield(0, kOrnnsForge);
        battlefield(1, kOrnnsForge, P2);
        for (int i = 0; i < 4; ++i) cleanup();
        EXPECT_EQ(state.player(P1).cost_modifiers.size(), 1u);
        EXPECT_EQ(state.player(P2).cost_modifiers.size(), 1u);
    }
}

TEST_F(C0Test, KinkouTemple_TankFromAnEquipmentCounts) {
    battlefield(0, kKinkouTemple);
    auto unit = addUnit(P1, kInvalidId, 2, 0);
    attached(kDoransShield, unit);                  // +1 [M], [Tank]
    cleanup();
    EXPECT_TRUE(o(unit).hasKeyword(Keyword::Tank));
    EXPECT_EQ(might(unit), 4) << "2 + Doran's Shield 1 + Kinkou Temple 1";
}

TEST_F(C0Test, ForgeOfTheFluft_MovesAnAttachedEquipment) {
    battlefield(0, kForgeOfTheFluft);
    addUnit(P1, kInvalidId, 1, 0);                  // holds the battlefield
    auto lg = legend(P1);
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    auto sword = attached(kLongSword, a);           // the only Equipment, already attached
    cleanup();
    ASSERT_EQ(o(lg).granted_abilities.size(), 1u);
    ASSERT_TRUE(card_registry.get(kForgeOfTheFluft)->hasLegalTargets(state, P1));

    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(sword), static_cast<int>(b)}));
    activate(lg, &agent, {}, kForgeOfTheFluft);
    EXPECT_TRUE(o(lg).is_exhausted);
    ASSERT_TRUE(o(sword).attached_to.has_value());
    EXPECT_EQ(*o(sword).attached_to, b);
    EXPECT_TRUE(o(a).attachments.empty());
    EXPECT_FALSE(agent.wasOffered(a)) << "not onto the unit it is already on";
}

TEST_F(C0Test, ForgeOfTheFluft_NotOfferedWithNothingToAttach) {
    battlefield(0, kForgeOfTheFluft);
    addUnit(P1, kInvalidId, 2);
    EXPECT_FALSE(card_registry.get(kForgeOfTheFluft)->hasLegalTargets(state, P1));
}

TEST_F(C0Test, SunkenTemple_NeedsAMightyUnitToTriggerNotToResolve) {
    auto temple = battlefield(0, kSunkenTemple);
    addToDeck(P1, kRecruit);
    addToDeck(P1, kRecruit);
    state.player(P1).rune_pool.energy = 2;

    // No Mighty unit: the ability does not trigger at all.
    auto small = addUnit(P1, kInvalidId, 4, 0);
    EnginePathAgent none;
    fireTriggerThroughEngine(temple, TriggerType::WhenYouConquerHere, &none);
    EXPECT_TRUE(none.prompts.empty());
    EXPECT_EQ(handSize(P1), 0);

    // Mighty: pay [1] at finalization; losing the unit before the ability
    // resolves does not cost the draw (plan 07 item 7.8).
    o(small).base_might = o(small).current_might = 5;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    fireAndProbe(temple, TriggerType::WhenYouConquerHere, &yes, [&](GameState& st) {
        EXPECT_EQ(st.player(P1).rune_pool.energy, 1) << "[1] paid at finalization";
        auto& u = st.getObject(small);     // the Mighty unit is lost in response
        u.location = std::nullopt;
        u.zone = ZoneType::Trash;
    });
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(C0Test, VeiledTemple_ReadiesAGearAndMayDetachIt) {
    auto temple = battlefield(0, kVeiledTemple);
    auto unit = addUnit(P1, kInvalidId, 2);
    auto sword = attached(kLongSword, unit);
    o(sword).is_exhausted = true;

    EnginePathAgent no(sayingNo());
    fireTriggerThroughEngine(temple, TriggerType::WhenYouConquerHere, &no);
    EXPECT_TRUE(o(sword).is_exhausted) << "declined";

    // yes; the gear (the only one, taken without a prompt or picked); detach.
    EnginePathAgent yes([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects.front() == sword) return i;
        for (const auto& i : legal)
            if (i.chosen_value.has_value() && *i.chosen_value == 1) return i;   // yes / "Detach it"
        return legal.front();
    });
    fireTriggerThroughEngine(temple, TriggerType::WhenYouConquerHere, &yes);
    EXPECT_GE(yes.prompts.size(), 2u) << "the may, then detach-or-not";
    EXPECT_FALSE(o(sword).is_exhausted);
    EXPECT_FALSE(o(sword).attached_to.has_value());
    EXPECT_TRUE(o(unit).attachments.empty());
}

TEST_F(C0Test, AmateurRecital_MovesTheChosenUnitToItsBase) {
    auto recital = battlefield(0, kAmateurRecital);
    auto enemy = addUnit(P2, kInvalidId, 2, 1);
    auto stuck = addUnit(P1, kInvalidId, 2, 0);
    state.battlefields[0].blocks_move_to_base = true;    // as Vilemaw's Lair sets it
    EnginePathAgent agent(EnginePathAgent::choosing(enemy, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(recital, TriggerType::WhenYouHoldHere, &agent);
    EXPECT_FALSE(agent.wasOffered(stuck)) << "can't move from there to base";
    EXPECT_TRUE(o(enemy).isAtBase());
    EXPECT_TRUE(o(stuck).isAtBattlefield());

    auto other = addUnit(P2, kInvalidId, 2, 1);
    EnginePathAgent no(sayingNo());
    fireTriggerThroughEngine(recital, TriggerType::WhenYouHoldHere, &no);
    EXPECT_TRUE(o(other).isAtBattlefield()) << "\"you may\"";
}

TEST_F(C0Test, DuskRoseLab_KillsThePickedUnitAsTheCost) {
    auto lab = battlefield(0, kDuskRoseLab);
    addToDeck(P1, kRecruit);
    auto keep = addUnit(P1, kInvalidId, 2, 0);
    auto kill = addUnit(P1, kInvalidId, 2, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(kill, EnginePathAgent::May::Yes));
    fireAndProbe(lab, TriggerType::AtStartOfBeginning, &agent, [&](GameState& st) {
        EXPECT_FALSE(st.getObject(kill).location.has_value())
            << "the unit is killed as the ability is finalized";
        EXPECT_TRUE(st.player(P1).hand.empty());
    });
    EXPECT_TRUE(agent.wasOffered(keep));
    EXPECT_TRUE(o(keep).location.has_value());
    EXPECT_TRUE(inTrash(P1, kill));
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(C0Test, StarSpring_MovesAnotherUnitNotTheOnePlayed) {
    auto spring = battlefield(0, kStarSpring);
    // The played unit has the LOWER id: the old newest-id guess took `other`
    // for the played unit and sent `played` home.
    auto played = addUnit(P1, kRecruit, 0, 0);
    auto other = addUnit(P1, kRecruit, 0, 0);
    EnginePathAgent agent(EnginePathAgent::choosing(other, EnginePathAgent::May::Yes));
    fireTriggerThroughEngine(spring, TriggerType::WhenYouPlayAUnit, &agent, played);
    EXPECT_FALSE(agent.wasOffered(played)) << "\"another unit\"";
    EXPECT_TRUE(o(other).isAtBase());
    EXPECT_TRUE(o(played).isAtBattlefield());

    // "The first time ... each turn": a second unit played here does nothing.
    auto second = addUnit(P1, kRecruit, 0, 0);
    EnginePathAgent again;
    fireTriggerThroughEngine(spring, TriggerType::WhenYouPlayAUnit, &again, second);
    EXPECT_TRUE(again.prompts.empty());
}

TEST_F(C0Test, StarSpring_DecliningStillUsesUpTheFirstTime) {
    auto spring = battlefield(0, kStarSpring);
    auto played = addUnit(P1, kRecruit, 0, 0);
    auto other = addUnit(P1, kRecruit, 0, 0);
    EnginePathAgent no(sayingNo());
    fireTriggerThroughEngine(spring, TriggerType::WhenYouPlayAUnit, &no, played);
    EXPECT_TRUE(o(other).isAtBattlefield());
    auto second = addUnit(P1, kRecruit, 0, 0);
    EnginePathAgent again;
    fireTriggerThroughEngine(spring, TriggerType::WhenYouPlayAUnit, &again, second);
    EXPECT_TRUE(again.prompts.empty());
    // The other player's first play here this turn is their own "first time".
    battlefield(0, kStarSpring, P2);
    state.battlefields[0].card_object_id = spring;
    o(spring).controller = P2;
    auto theirs = addUnit(P2, kRecruit, 0, 0);
    addUnit(P2, kRecruit, 0, 0);
    EnginePathAgent p2;
    fireTriggerThroughEngine(spring, TriggerType::WhenYouPlayAUnit, nullptr, theirs, &p2);
    EXPECT_FALSE(p2.prompts.empty());
}

TEST_F(C0Test, ValleyOfIdols_BuffsTheUnitThatWasPlayed) {
    auto valley = battlefield(0, kValleyOfIdols);
    auto played = addUnit(P1, kRecruit, 0, 0);
    auto newer = addUnit(P1, kRecruit, 0, 0);       // higher id, not the one played
    state.player(P1).rune_pool.energy = 1;
    EnginePathAgent agent(EnginePathAgent::script({1}));
    fireTriggerThroughEngine(valley, TriggerType::WhenYouPlayAUnit, &agent, played);
    EXPECT_EQ(o(played).buff_count, 1);
    EXPECT_EQ(o(newer).buff_count, 0);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
}

TEST_F(C0Test, ValleyOfIdols_ARealPlayHandsTheTriggerItsUnit) {
    battlefield(0, kValleyOfIdols);
    auto bystander = addUnit(P1, kRecruit, 0, 0);
    auto card = addToHand(P1, kRecruit);
    state.player(P1).rune_pool.energy = 1;          // the [1] for the buff
    EnginePathAgent agent(EnginePathAgent::script({1}));
    playThroughEngine(card, &agent, LocationId{BattlefieldLocation{0}});
    ASSERT_TRUE(o(card).isAtBattlefield());
    EXPECT_EQ(o(card).buff_count, 1);
    EXPECT_EQ(o(bystander).buff_count, 0);
}

TEST_F(C0Test, TheAcademy_GrantedRepeatCostsTheWholeBaseCost) {
    // "[Repeat] equal to its base cost": Primal Strength is [4][O], so the
    // Repeat is [4][O], not [4].
    auto unit = addUnit(P1, kInvalidId, 2);
    {
        state.player(P1).grant_repeat_base_to_next_spell = true;
        auto spell = addToHand(P1, kPrimalStrength);
        auto& pool = state.player(P1).rune_pool;
        pool.energy = 8;
        pool.power[static_cast<int>(Domain::Body)] = 1;   // the base cost's [O] only
        // No prompt is expected (asserted below). The one entry is the answer
        // a wrongly offered Repeat would get: yes, so the failure shows in the
        // Might as well; any prompt after it fails the strict script.
        EnginePathAgent agent(EnginePathAgent::script({1}));
        playAt(spell, unit, &agent);
        EXPECT_TRUE(agent.prompts.empty()) << "the Repeat's [O] can't be paid: not offered";
        EXPECT_EQ(o(unit).temp_might_bonus, 7);
        EXPECT_FALSE(state.player(P1).grant_repeat_base_to_next_spell) << "used up by this spell";
    }
    {
        o(unit).temp_might_bonus = 0;
        state.player(P1).grant_repeat_base_to_next_spell = true;
        auto spell = addToHand(P1, kPrimalStrength);
        auto& pool = state.player(P1).rune_pool;
        pool.energy = 8;
        pool.power[static_cast<int>(Domain::Body)] = 2;
        EnginePathAgent agent(EnginePathAgent::script({
            1,                        // pay the granted Repeat ([4][O])? yes
            static_cast<int>(unit),   // the additional execution's target: the same unit
                                      // (the only unit on the board)
        }));
        playAt(spell, unit, &agent);
        // The Repeat yes/no, then the additional execution's own target: its
        // choices are made as the spell is finalized and need not be the first
        // execution's (CR 820.2, 820.2.a; plans/09 item 9.T4 — it reused the
        // play-time target before).
        EXPECT_EQ(agent.prompts.size(), 2u);
        EXPECT_EQ(o(unit).temp_might_bonus, 14);
        EXPECT_EQ(state.player(P1).rune_pool.energy, 0);
        EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Body)], 0);
    }
}

TEST_F(C0Test, AltarOfBlood_IsAskedForASpellKillDuringCombatToo) {
    // batch-00: EffectExecutor::killObject never asked the battlefield's card.
    battlefield(0, kAltarOfBlood);
    state.battlefields[0].combat_in_progress = true;
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    state.player(P1).rune_pool.universal_power = 3;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &yes);
    EXPECT_TRUE(o(unit).isAtBase()) << "recalled instead of dying";
    EXPECT_TRUE(o(unit).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 0);
}

TEST_F(C0Test, TokenReferenceCardsAreTokens) {
    for (CardDefId id : {kGold, kBaronPit, kBrush})
        EXPECT_EQ(card_db.get(id).super_type, SuperType::Token) << card_db.get(id).name;
}

// ═══ Gear ═══════════════════════════════════════════════════════════════════

TEST_F(C0Test, IronBallista_EntersExhausted) {
    auto ballista = addToHand(P1, kIronBallista);
    playThroughEngine(ballista);
    ASSERT_TRUE(o(ballista).location.has_value());
    EXPECT_TRUE(o(ballista).is_exhausted);
}

TEST_F(C0Test, SunDisc_NeedsAnotherCardPlayedThisTurn) {
    auto disc = gear(P1, kSunDisc);
    auto unit = addUnit(P1, kRecruit, 0);
    o(unit).is_exhausted = true;

    // No card played this turn: [Legion] is not met, nothing is armed.
    activate(disc);
    fireTriggerThroughEngine(disc, TriggerType::WhenYouPlayAUnit, nullptr, unit);
    EXPECT_TRUE(o(unit).is_exhausted);

    // A card was played this turn: armed, and the next unit played — a real
    // play from hand, which is what hands the trigger its subject — is readied.
    o(disc).is_exhausted = false;
    state.player(P1).cards_played_this_turn = 1;
    activate(disc);
    auto played = addToHand(P1, kRecruit);
    playThroughEngine(played);
    ASSERT_TRUE(o(played).location.has_value());
    EXPECT_FALSE(o(played).is_exhausted) << "\"enters ready\"";
    EXPECT_TRUE(o(unit).is_exhausted) << "only the unit that was played";
}

TEST_F(C0Test, SunDisc_DoesNotCountItselfOnTheTurnItIsPlayed) {
    auto disc = addToHand(P1, kSunDisc);
    playThroughEngine(disc);
    ASSERT_EQ(state.player(P1).cards_played_this_turn, 1);
    auto unit = addUnit(P1, kRecruit, 0);
    o(unit).is_exhausted = true;
    activate(disc);
    fireTriggerThroughEngine(disc, TriggerType::WhenYouPlayAUnit, nullptr, unit);
    EXPECT_TRUE(o(unit).is_exhausted) << "CR 812.1.c: a card OTHER than the one with Legion";
}

TEST_F(C0Test, SunDisc_ReadiesThePlayedUnitAndIsUsedUpByIt) {
    auto disc = gear(P1, kSunDisc);
    state.player(P1).cards_played_this_turn = 1;
    auto bystander = addUnit(P1, kRecruit, 0);
    o(bystander).is_exhausted = true;
    auto played = addUnit(P1, kRecruit, 0);             // the unit just played: exhausted
    o(played).is_exhausted = true;
    activate(disc);
    EnginePathAgent agent;
    fireTriggerThroughEngine(disc, TriggerType::WhenYouPlayAUnit, &agent, played);
    EXPECT_TRUE(agent.prompts.empty()) << "nothing to choose: \"the next unit you play\"";
    EXPECT_FALSE(o(played).is_exhausted) << "the played unit is readied";
    EXPECT_TRUE(o(bystander).is_exhausted) << "not some other exhausted unit";

    // The arm is gone: the following unit is not readied (batch-08).
    auto later = addUnit(P1, kRecruit, 0);
    o(later).is_exhausted = true;
    fireTriggerThroughEngine(disc, TriggerType::WhenYouPlayAUnit, nullptr, later);
    EXPECT_TRUE(o(later).is_exhausted);
}

TEST_F(C0Test, UnlicensedArmory_SavingTheUnitCostsPowerAndIsOptional) {
    auto armory = gear(P1, kUnlicensedArmory);
    addToHand(P1, kRecruit);                            // "Discard 1"
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    EnginePathAgent pick(EnginePathAgent::choosing(unit));
    activate(armory, &pick);
    ASSERT_TRUE(o(armory).is_exhausted);

    // Would die with no Power to pay: it dies.
    {
        // 9.B4: no agent was passed (a may is declined by default) and the
        // mark was copied from card_counters, which the card no longer uses:
        // the unit died whether or not the may was offered.
        ASSERT_FALSE(o(unit).pending_death_replacements.empty()) << "the ability marked the unit";
        auto doomed = addUnit(P1, kInvalidId, 3, 0);
        o(doomed).pending_death_replacements = o(unit).pending_death_replacements;   // marked the same way
        EnginePathAgent broke(EnginePathAgent::script({1}));
        resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(doomed); },
                             &broke);
        EXPECT_TRUE(broke.prompts.empty()) << "no [C] to pay: not offered";
        EXPECT_FALSE(o(doomed).location.has_value()) << "no [C], no rescue";
    }
    // With a Fury Power and a yes: healed, exhausted, recalled; the Power is spent.
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    o(unit).damage_marked = 2;
    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &yes);
    EXPECT_FALSE(yes.prompts.empty()) << "with the [C] available the may IS offered";
    EXPECT_TRUE(o(unit).isAtBase());
    EXPECT_TRUE(o(unit).is_exhausted);
    EXPECT_EQ(o(unit).damage_marked, 0);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 0);

    // "The next time": the second death is not replaced.
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &yes);
    EXPECT_FALSE(o(unit).location.has_value());
}

TEST_F(C0Test, UnlicensedArmory_CanBeDeclined) {
    auto armory = gear(P1, kUnlicensedArmory);
    addToHand(P1, kRecruit);
    auto unit = addUnit(P1, kInvalidId, 3, 0);
    EnginePathAgent pick(EnginePathAgent::choosing(unit));
    activate(armory, &pick);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    EnginePathAgent no(sayingNo());
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(unit); }, &no);
    EXPECT_FALSE(o(unit).location.has_value());
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)], 1);
}

TEST_F(C0Test, MaskOfForesight_IgnoresAUnitThatLeftTheBoard) {
    auto mask = gear(P1, kMaskOfForesight);
    auto gone = addUnit(P1, kInvalidId, 2);
    o(gone).location = std::nullopt;
    o(gone).zone = ZoneType::Hand;
    fireTriggerThroughEngine(mask, TriggerType::WhenAUnitAttacksOrDefendsAlone, nullptr, gone);
    EXPECT_EQ(o(gone).temp_might_bonus, 0);

    // 9.B10: `== 0` alone also held for a Mask that does nothing. The same
    // trigger for a unit still on the board gives it the +1 [M].
    auto lone = addUnit(P1, kInvalidId, 2, 0);
    fireTriggerThroughEngine(mask, TriggerType::WhenAUnitAttacksOrDefendsAlone, nullptr, lone);
    EXPECT_EQ(o(lone).temp_might_bonus, 1) << "+1 [M] this turn to the lone unit";
    EXPECT_EQ(o(gone).temp_might_bonus, 0);
}

TEST_F(C0Test, SpiritsRefuge_BuffsAChosenUnitWhenPlayed) {
    auto a = addUnit(P1, kInvalidId, 2);
    auto b = addUnit(P1, kInvalidId, 2);
    auto refuge = addToHand(P1, kSpiritsRefuge);
    EnginePathAgent agent(EnginePathAgent::choosing(b));
    playThroughEngine(refuge, &agent);
    EXPECT_TRUE(agent.wasOffered(a));
    EXPECT_EQ(o(b).buff_count, 1);
    EXPECT_EQ(o(a).buff_count, 0);
    EXPECT_FALSE(o(refuge).hasKeyword(Keyword::Deflect)) << "the gear itself has no [Deflect]";
    EXPECT_EQ(card_db.get(kSpiritsRefuge).deflect_value, 0);
}

TEST_F(C0Test, SpiritsRefuge_DeflectOnlyIfTheUnitHadNone) {
    gear(P1, kSpiritsRefuge);
    gear(P1, kSpiritsRefuge);                           // a second Refuge adds nothing
    auto plain = addUnit(P1, kInvalidId, 2);
    auto printed = addUnit(P1, kInvalidId, 2);
    auto unbuffed = addUnit(P1, kInvalidId, 2);
    o(plain).buff_count = o(printed).buff_count = 1;
    o(printed).keywords.set(Keyword::Deflect);
    o(printed).deflect_value = 1;
    int cost_plain = 0, cost_printed = 0, cost_unbuffed = 0;
    withEngine([&](GameEngine& e) {
        e.testHook_cleanup();
        cost_plain = e.testHook_executor().deflectCostFor(P2, plain);
        cost_printed = e.testHook_executor().deflectCostFor(P2, printed);
        cost_unbuffed = e.testHook_executor().deflectCostFor(P2, unbuffed);
    });
    EXPECT_EQ(cost_plain, 1);
    EXPECT_EQ(cost_printed, 1) << "\"if they didn't already\"";
    EXPECT_EQ(cost_unbuffed, 0);
}

TEST_F(C0Test, SolariShrine_OnlyAStunnedEnemyUnitsDeathTriggers) {
    auto shrine = gear(P1, kSolariShrine);
    addToDeck(P1, kRecruit);
    addToDeck(P1, kRecruit);
    auto plain = addUnit(P2, kInvalidId, 2, 0);
    auto stunned = addUnit(P2, kInvalidId, 2, 0);
    o(stunned).is_stunned = true;
    // "When YOU kill": P1's damage is what is lethal. Each unit is marked
    // just before it is killed: a lethally damaged unit does not outlive the
    // cleanup that follows the other one's death.
    auto markedByP1 = [&](GameObjectId id) {
        o(id).damage_marked = 2;
        o(id).damage_marked_by[0] = 2;
    };

    markedByP1(plain);
    EnginePathAgent a1(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(plain); }, &a1);
    EXPECT_TRUE(a1.prompts.empty());
    EXPECT_EQ(handSize(P1), 0);
    ASSERT_TRUE(o(stunned).location.has_value()) << "the stunned unit is still on the board";

    markedByP1(stunned);
    EnginePathAgent a2(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(stunned); }, &a2);
    EXPECT_EQ(a2.prompts.size(), 1u);
    EXPECT_EQ(handSize(P1), 1);
    EXPECT_TRUE(o(shrine).is_exhausted);
}

TEST_F(C0Test, GarbageGrabber_RecyclesThePickedCardsAsACost) {
    auto grabber = gear(P1, kGarbageGrabber);
    state.player(P1).rune_pool.energy = 1;
    auto top = addToDeck(P1, kRecruit);
    auto t1 = trashCard(P1, kRecruit);
    auto t2 = trashCard(P1, kRecruit);
    auto t3 = trashCard(P1, kRecruit);
    auto t4 = trashCard(P1, kRecruit);
    EnginePathAgent agent(EnginePathAgent::script(
        {static_cast<int>(t4), static_cast<int>(t2), static_cast<int>(t3)}));
    activate(grabber, &agent);
    EXPECT_EQ(agent.prompts.size(), 3u);
    EXPECT_TRUE(inTrash(P1, t1)) << "the one not picked stays";
    EXPECT_TRUE(inDeck(P1, t2));
    EXPECT_TRUE(inDeck(P1, t3));
    EXPECT_TRUE(inDeck(P1, t4));
    EXPECT_TRUE(inHand(P1, top));
}

TEST_F(C0Test, GarbageGrabber_TheRecycleHappensAtActivation) {
    auto grabber = gear(P1, kGarbageGrabber);
    state.player(P1).rune_pool.energy = 1;
    addToDeck(P1, kRecruit);
    for (int i = 0; i < 3; ++i) trashCard(P1, kRecruit);
    Intent act;
    act.type = IntentType::ActivateAbility;
    act.player = P1;
    act.ability_source = grabber;
    // P2 gets priority while the ability is on the chain: by then the cost
    // must have been paid.
    int trash_when_p2_could_respond = -1;
    GameState* live = nullptr;
    PickByPredicateAgent p2([&](int, const std::vector<Intent>& legal) {
        if (trash_when_p2_could_respond < 0 && live)
            trash_when_p2_could_respond = static_cast<int>(live->player(P1).trash.size());
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        return legal.front();
    });
    withEngine([&](GameEngine& e) {
        live = &e.mutableState();
        e.testHook_executeIntent(act); settleLikeTheEngine(e);
        live = nullptr;
    }, nullptr, &p2);
    EXPECT_EQ(trash_when_p2_could_respond, 0);
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(C0Test, MushroomPouch_SeesACardHiddenAtABattlefield) {
    auto pouch = gear(P1, kMushroomPouch);
    addToDeck(P1, kRecruit);
    addToDeck(P1, kRecruit);
    fireTriggerThroughEngine(pouch, TriggerType::AtStartOfBeginning);
    EXPECT_EQ(handSize(P1), 0) << "no facedown card";

    // A hidden card as the engine stores it: facedown zone, no board location.
    auto hidden = addToHand(P1, kRecruit);
    state.player(P1).hand.pop_back();
    o(hidden).zone = ZoneType::FacedownZone;
    o(hidden).is_hidden = true;
    o(hidden).hidden_at = BattlefieldId::fromRaw(0);
    state.battlefields[0].facedown.push_back(hidden);
    fireTriggerThroughEngine(pouch, TriggerType::AtStartOfBeginning);
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(C0Test, ArenaBar_OnlyAnExhaustedFriendlyUnit) {
    auto ready = addUnit(P1, kInvalidId, 2);
    auto tired = addUnit(P1, kInvalidId, 2);
    o(tired).is_exhausted = true;
    const auto legal = card_registry.get(kArenaBar)->enumerateLegalTargets(state, P1, 0, kInvalidId);
    EXPECT_EQ(legal, std::vector<GameObjectId>{tired});

    auto bar = gear(P1, kArenaBar);
    activate(bar, nullptr, {tired});
    EXPECT_EQ(o(tired).buff_count, 1);

    // A target that is ready by the time the ability resolves is not buffed.
    o(bar).is_exhausted = false;
    activate(bar, nullptr, {ready});
    EXPECT_EQ(o(ready).buff_count, 0);
}

TEST_F(C0Test, PiratesHaven_ReadyingARuneDoesNotTrigger) {
    gear(P1, kPiratesHaven);
    auto rune = addRune(P1, Domain::Body, /*exhausted=*/true);
    auto unit = addUnit(P1, kInvalidId, 2);
    o(unit).is_exhausted = true;
    size_t chain_after_rune = 99;
    resolveThroughEngine([&](EffectExecutor& exec, GameState& st) {
        exec.readyObject(rune);
        chain_after_rune = st.chain.items.size();
        exec.readyObject(unit);
    });
    EXPECT_EQ(chain_after_rune, 0u) << "\"a friendly unit\"";
    EXPECT_EQ(o(unit).temp_might_bonus, 1);
    EXPECT_EQ(o(rune).temp_might_bonus, 0);
}

TEST_F(C0Test, Mistfall_NotOfferedWithoutThePowerAndPaysFromThePool) {
    auto mistfall = gear(P1, kMistfall);
    auto unit = addUnit(P1, kInvalidId, 2);
    o(unit).is_exhausted = true;

    EnginePathAgent broke(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.buffUnit(unit); }, &broke);
    EXPECT_TRUE(broke.prompts.empty()) << "no [O] to pay: not offered";
    EXPECT_TRUE(o(unit).is_exhausted);

    o(unit).buff_count = 0;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Body)] = 1;   // floating [O]
    EnginePathAgent yes(EnginePathAgent::script({1}));
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.buffUnit(unit); }, &yes);
    EXPECT_EQ(yes.prompts.size(), 1u);
    EXPECT_FALSE(o(unit).is_exhausted);
    EXPECT_TRUE(o(mistfall).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Body)], 0);
}

TEST_F(C0Test, PackOfWonders_NeverItselfAndCanTakeAFacedownCard) {
    auto pack = gear(P1, kPackOfWonders);
    const Card* card = card_registry.get(kPackOfWonders);
    EXPECT_TRUE(card->enumerateLegalTargets(state, P1, 0, pack).empty())
        << "the Pack alone: \"another\" leaves nothing";

    auto hidden = addToHand(P1, kRecruit);
    state.player(P1).hand.pop_back();
    o(hidden).zone = ZoneType::FacedownZone;
    o(hidden).is_hidden = true;
    o(hidden).hidden_at = BattlefieldId::fromRaw(1);
    state.battlefields[1].facedown.push_back(hidden);
    EXPECT_EQ(card->enumerateLegalTargets(state, P1, 0, pack), std::vector<GameObjectId>{hidden});

    EnginePathAgent agent(EnginePathAgent::choosing(hidden));
    activate(pack, &agent);
    EXPECT_TRUE(inHand(P1, hidden));
    EXPECT_FALSE(o(hidden).is_hidden);
    EXPECT_TRUE(state.battlefields[1].facedown.empty());
    EXPECT_TRUE(o(pack).location.has_value());
}

TEST_F(C0Test, Scrapheap_DrawsWhenDiscardedAndWhenKilled) {
    addToDeck(P1, kRecruit);
    addToDeck(P1, kRecruit);
    auto in_hand = addToHand(P1, kScrapheap);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.applyDiscard(P1, in_hand); });
    EXPECT_TRUE(inTrash(P1, in_hand));
    EXPECT_EQ(handSize(P1), 1) << "discarded -> draw 1";

    auto on_board = gear(P1, kScrapheap);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.killObject(on_board); });
    EXPECT_EQ(handSize(P1), 2) << "killed -> draw 1";
}

TEST_F(C0Test, TheSyren_NotFromABattlefieldUnitsCantLeaveForBase) {
    auto syren = gear(P1, kTheSyren);
    state.player(P1).rune_pool.energy = 2;
    auto stuck = addUnit(P1, kInvalidId, 2, 0);
    auto free_unit = addUnit(P1, kInvalidId, 2, 1);
    state.battlefields[0].blocks_move_to_base = true;   // as Vilemaw's Lair sets it
    const auto legal = card_registry.get(kTheSyren)->enumerateLegalTargets(state, P1, 0, syren);
    EXPECT_EQ(legal, std::vector<GameObjectId>{free_unit});

    activate(syren, nullptr, {stuck});                  // a stale / hand-built intent
    EXPECT_TRUE(o(stuck).isAtBattlefield());
    o(syren).is_exhausted = false;
    activate(syren, nullptr, {free_unit});
    EXPECT_TRUE(o(free_unit).isAtBase());
}

TEST_F(C0Test, TreasureTrove_TriggersHoweverItLeavesTheBoard) {
    for (int i = 0; i < 4; ++i) addToDeck(P1, kRecruit);
    for (int i = 0; i < 4; ++i) {
        auto r = addRune(P1, Domain::Chaos);
        o(r).location = std::nullopt;
        o(r).zone = ZoneType::RuneDeck;
        state.player(P1).rune_deck.push_back(r);
    }
    auto runesInBase = [&]() {
        int n = 0;
        for (auto& [id, r] : state.objects)
            if (r.isRune() && r.controller == P1 && r.location.has_value()) ++n;
        return n;
    };
    // Returned to hand.
    auto bounced = gear(P1, kTreasureTrove);
    resolveThroughEngine([&](EffectExecutor& exec, GameState&) { exec.bounceToHand(bounced); });
    EXPECT_EQ(handSize(P1), 2) << "the Trove itself, and the card drawn";
    EXPECT_EQ(runesInBase(), 1);

    // Killed by its own ability: once, not once for the kill and once for leaving.
    auto killed = gear(P1, kTreasureTrove);
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Chaos)] = 1;
    activate(killed);
    EXPECT_TRUE(inTrash(P1, killed));
    EXPECT_EQ(handSize(P1), 3);
    EXPECT_EQ(runesInBase(), 2);
}

TEST_F(C0Test, ForgeOfTheFuture_PlaysARecruitWhenPlayed) {
    auto forge = addToHand(P1, kForgeOfTheFuture);
    playThroughEngine(forge);
    int recruits = 0;
    for (auto& [id, u] : state.objects)
        if (u.isUnit() && u.controller == P1 && u.name == "Recruit" && u.isAtBase()) {
            ++recruits;
            EXPECT_EQ(u.current_might, 1);
        }
    EXPECT_EQ(recruits, 1);
    EXPECT_TRUE(o(forge).location.has_value()) << "it does not kill anything";
}

TEST_F(C0Test, ForgeOfTheFuture_KillThisRecyclesUpToFourFromTrashes) {
    auto forge = gear(P1, kForgeOfTheFuture);
    auto mine = trashCard(P1, kRecruit);
    auto theirs = trashCard(P2, kRecruit);
    auto kept = trashCard(P2, kRecruit);
    // Two picks, then "None".
    EnginePathAgent agent(EnginePathAgent::script({static_cast<int>(mine), static_cast<int>(theirs), 0}));
    activate(forge, &agent);
    EXPECT_TRUE(inTrash(P1, forge)) << "\"Kill this\" is the cost";
    EXPECT_TRUE(inDeck(P1, mine));
    EXPECT_TRUE(inDeck(P2, theirs)) << "to its owner's deck";
    EXPECT_TRUE(inTrash(P2, kept)) << "\"up to\"";
}

TEST_F(C0Test, BaitedHook_PaysPowerStaysOnBoardAndPlaysThePickedUnit) {
    auto hook = gear(P1, kBaitedHook);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;
    auto victim = addUnit(P1, kInvalidId, 1);
    // Top five (top = back): a 2 [M]-or-less unit is eligible.
    auto d1 = addToDeck(P1, kPrimalStrength);
    auto d2 = addToDeck(P1, kPrimalStrength);
    auto eligible = addToDeck(P1, kRecruit);
    o(eligible).base_might = 1;
    auto d4 = addToDeck(P1, kPrimalStrength);
    auto d5 = addToDeck(P1, kPrimalStrength);
    auto below = addToDeck(P1, kPrimalStrength);    // will be 6th from the top: untouched
    state.player(P1).main_deck = {below, d1, d2, eligible, d4, d5};

    EnginePathAgent agent(EnginePathAgent::choosing(eligible));
    activate(hook, &agent, {victim});

    EXPECT_TRUE(o(hook).location.has_value()) << "the cost is [1][C] + exhaust, not \"recycle this\"";
    EXPECT_TRUE(o(hook).is_exhausted);
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Order)], 0) << "[C] = Order";
    EXPECT_TRUE(inTrash(P1, victim));
    EXPECT_TRUE(o(eligible).location.has_value()) << "the picked unit is played (plan 03 item 3.6)";
    EXPECT_EQ(state.player(P1).cards_banished_this_turn, 1) << "banished on the way";
    ASSERT_EQ(state.player(P1).main_deck.size(), 5u);
    EXPECT_EQ(state.player(P1).main_deck.back(), below) << "the other four went to the bottom";
    for (auto id : {d1, d2, d4, d5}) EXPECT_TRUE(inDeck(P1, id));
}

TEST_F(C0Test, BaitedHook_PlayingAUnitIsOptional) {
    auto hook = gear(P1, kBaitedHook);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Order)] = 1;
    auto victim = addUnit(P1, kInvalidId, 1);
    auto eligible = addToDeck(P1, kRecruit);
    o(eligible).base_might = 1;
    EnginePathAgent none(sayingNo());
    activate(hook, &none, {victim});
    ASSERT_EQ(none.prompts.size(), 1u);
    EXPECT_TRUE(inDeck(P1, eligible)) << "\"You may\": recycled with the rest";
    EXPECT_TRUE(inTrash(P1, victim));
}

TEST_F(C0Test, Gold_AddsTheLegendsBonusEnergy) {
    auto lg = legend(P1);
    auto gold = gear(P1, kGold);
    o(gold).is_token = true;
    activate(gold);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 1);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 0);

    o(lg).card_counters["__gold_add_bonus_energy"] = 1;   // as a legend's aura publishes it
    auto gold2 = gear(P1, kGold);
    o(gold2).is_token = true;
    activate(gold2);
    EXPECT_EQ(state.player(P1).rune_pool.universal_power, 2);
    EXPECT_EQ(state.player(P1).rune_pool.energy, 1);
}

TEST_F(C0Test, RecurveBow_ItsControllerPicksTheEnemyUnitHere) {
    auto bearer = addUnit(P1, kInvalidId, 2, 0);
    auto bow = attached(kRecurveBow, bearer);
    auto e1 = addUnit(P2, kInvalidId, 5, 0);
    auto e2 = addUnit(P2, kInvalidId, 5, 0);
    auto elsewhere = addUnit(P2, kInvalidId, 5, 1);
    EnginePathAgent agent(EnginePathAgent::choosing(e2));
    // Combat starts at battlefield 0 with P1 attacking: the bearer attacks.
    resolveThroughEngine([&](EffectExecutor&, GameState&) {
        events.emit(CombatStartedEvent{BattlefieldId::fromRaw(0), P1, P2});
    }, &agent);
    EXPECT_TRUE(o(bow).attached_to.has_value());
    EXPECT_TRUE(agent.wasOffered(e1));
    EXPECT_FALSE(agent.wasOffered(elsewhere)) << "\"here\"";
    EXPECT_EQ(o(e2).damage_marked, 2);
    EXPECT_EQ(o(e1).damage_marked, 0);
}

TEST_F(C0Test, AssemblyRig_RecyclesThePickedUnitAsACost) {
    auto rig = gear(P1, kAssemblyRig);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    auto u1 = trashCard(P1, kRecruit);
    auto u2 = trashCard(P1, kRecruit);
    EnginePathAgent agent(EnginePathAgent::choosing(u1));
    activate(rig, &agent);
    EXPECT_TRUE(agent.wasOffered(u2));
    EXPECT_TRUE(inDeck(P1, u1));
    EXPECT_TRUE(inTrash(P1, u2));
    int mechs = 0;
    for (auto& [id, u] : state.objects)
        if (u.isUnit() && u.name == "Mech" && u.location.has_value()) ++mechs;
    EXPECT_EQ(mechs, 1);
}

TEST_F(C0Test, Brutalizer_BonusAppliesHoweverItWasAttached) {
    auto unit = addUnit(P1, kInvalidId, 2);
    auto brutalizer = gear(P1, kBrutalizer);
    // Attached for free, the way Rell / Jax / Forge of the Fluft do it.
    {
        EffectExecutor exec(state, events, card_db, &card_registry);
        CardContext ctx{state, events, exec, P1, brutalizer};
        attachFree(ctx, brutalizer, unit);
    }
    cleanup();
    EXPECT_EQ(might(unit), 5) << "2 + 1 (Equipment) + 2 (attached this turn)";
    cleanup();
    EXPECT_EQ(might(unit), 5) << "still this turn, still once";
    state.turn.turn_number += 1;
    cleanup();
    EXPECT_EQ(might(unit), 3) << "not attached this turn any more";
}

TEST_F(C0Test, PoroSnax_CostsCalmPowerAndIsKilledNotRecycled) {
    auto snax = gear(P1, kPoroSnax);
    auto drawn = addToDeck(P1, kRecruit);
    state.player(P1).rune_pool.energy = 1;
    state.player(P1).rune_pool.power[static_cast<int>(Domain::Calm)] = 1;
    activate(snax);
    EXPECT_TRUE(inTrash(P1, snax)) << "\"Kill this\"";
    EXPECT_FALSE(inDeck(P1, snax));
    EXPECT_TRUE(inHand(P1, drawn));
    EXPECT_EQ(state.player(P1).rune_pool.power[static_cast<int>(Domain::Calm)], 0) << "[G]";

    // Without the Calm Power the ability cannot be activated.
    auto snax2 = gear(P1, kPoroSnax);
    addToDeck(P1, kRecruit);
    state.player(P1).rune_pool.energy = 1;
    activate(snax2);
    EXPECT_TRUE(o(snax2).location.has_value());
    EXPECT_EQ(handSize(P1), 1);
}

TEST_F(C0Test, HeartOfDarkIce_IsOfferedAndGivesTheChosenUnitMight) {
    auto heart = gear(P1, kHeartOfDarkIce);
    auto mine = addUnit(P1, kInvalidId, 2);
    auto theirs = addUnit(P2, kInvalidId, 2);
    bool offered = false;
    withEngine([&](GameEngine& e) {
        for (const auto& i : e.generateLegalActions())
            if (i.type == IntentType::ActivateAbility && i.ability_source == heart) offered = true;
    });
    EXPECT_TRUE(offered) << "batch-00: the ability was never generated";
    EnginePathAgent agent(EnginePathAgent::choosing(theirs));
    activate(heart, &agent);
    EXPECT_TRUE(agent.wasOffered(mine));
    EXPECT_EQ(o(theirs).temp_might_bonus, 3) << "\"a unit\": either player's";
}

}  // namespace
}  // namespace test_review_fix_c0
}  // namespace riftbound::test
