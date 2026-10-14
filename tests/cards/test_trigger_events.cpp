/// @file test_trigger_events.cpp
/// Verifies the phase-2 trigger events dispatch end-to-end: emit the event
/// (or perform the engine action that emits it), and assert the watching
/// card's ability is enqueued on the chain with the right firing trigger.
///
///   WhenIAmReadied              — Irelia, Fervent (380)
///   WhenYouHideACard            — Katarina, Reckless (585)
///   WhenYouPlayFromFacedown     — Katarina, Reckless (585)
///   WhenAnEnemyUnitDies         — Pyke, Returned (707)
///   WhenAUnitReturnsToHandHere  — Ripper's Bay (770, battlefield)
///   WhenAShowdownBeginsHere     — Diana, Lunari (641)
///   WhenOpponentPlaysAUnit      — Vex, Apathetic (712)
///   WhenIRevealedFromTop        — Nocturne, Horrifying (194)

#include "tests/cards/card_test_fixture.h"
#include "engine/chain_manager.h"
#include "engine/trigger_manager.h"

using namespace riftbound;
using namespace riftbound::test;

namespace {

class TriggerEventTest : public CardTestFixture {
protected:
    // Put a battlefield card object on battlefield `bf` (for BF-card triggers).
    GameObjectId installBattlefieldCard(BattlefieldId bf, CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_def_id = def_id;
        obj.card_type = CardType::Battlefield;
        const auto& def = card_db.get(def_id);
        obj.name = def.name;
        state.battlefields[bf].card_object_id = id;
        return id;
    }
    // Number of ability items currently on the chain.
    size_t chainSize() { return state.chain.items.size(); }
};

// ── WhenIAmReadied: Irelia gets enqueued when readied (exhausted->ready) ──
TEST_F(TriggerEventTest, ReadyAUnit_FiresOnReadiedUnit) {
    auto irelia = addUnit(P1, 380, /*might=*/4, /*at_bf=*/0);
    state.getObject(irelia).is_exhausted = true;
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    EffectExecutor exec(state, events, card_db);
    size_t before = chainSize();
    exec.readyObject(irelia);                 // emits UnitReadiedEvent
    EXPECT_GT(chainSize(), before) << "Irelia's WhenIAmReadied ability should enqueue";
}

TEST_F(TriggerEventTest, ReadyAUnit_NoFireWhenAlreadyReady) {
    auto irelia = addUnit(P1, 380, 4, 0);
    state.getObject(irelia).is_exhausted = false;   // already ready
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    EffectExecutor exec(state, events, card_db);
    size_t before = chainSize();
    exec.readyObject(irelia);
    EXPECT_EQ(chainSize(), before) << "re-readying a ready unit must not fire";
}

// ── WhenYouHideACard / WhenYouPlayFromFacedown: Katarina enqueues ──
TEST_F(TriggerEventTest, HideACard_FiresKatarina) {
    auto kat = addUnit(P1, 585, 3, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(CardHiddenEvent{kat, P1});
    EXPECT_GT(chainSize(), before);
}

TEST_F(TriggerEventTest, PlayFromFacedown_FiresKatarina) {
    auto kat = addUnit(P1, 585, 3, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(PlayedFromFacedownEvent{kat, P1});
    EXPECT_GT(chainSize(), before);
}

// ── WhenAnEnemyUnitDies: Pyke (P1, at a BF) enqueues when a P2 unit dies ──
TEST_F(TriggerEventTest, EnemyUnitDies_FiresPyke) {
    addUnit(P1, 707, 5, /*at_bf=*/0);   // at a battlefield
    auto enemy = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(UnitDiedEvent{enemy, P2, LocationId{}, 0});          // an ENEMY (of P1) died
    EXPECT_GT(chainSize(), before) << "Pyke's WhenAnEnemyUnitDies should enqueue";
}

TEST_F(TriggerEventTest, FriendlyUnitDies_DoesNotFirePyke) {
    addUnit(P1, 707, 5, 0);
    auto friendly = addUnit(P1, kInvalidId, 3, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(UnitDiedEvent{friendly, P1, LocationId{}, 0});        // friendly death, not enemy
    EXPECT_EQ(chainSize(), before);
}

// ── WhenAUnitReturnsToHandHere: Ripper's Bay BF card enqueues ──
TEST_F(TriggerEventTest, UnitReturnsToHandHere_FiresRippersBay) {
    auto unit = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    installBattlefieldCard(BattlefieldId::fromRaw(0), 770);                  // Ripper's Bay on BF 0
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(UnitReturnedToHandEvent{unit, P2, /*from_battlefield=*/BattlefieldId::fromRaw(0)});
    EXPECT_GT(chainSize(), before);
}

// ── WhenAShowdownBeginsHere: Diana (at the BF) enqueues ──
TEST_F(TriggerEventTest, ShowdownBeginsHere_FiresDiana) {
    addUnit(P1, 641, 4, /*at_bf=*/1);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(ShowdownStartedEvent{/*battlefield=*/BattlefieldId::fromRaw(1), /*is_combat=*/true});
    EXPECT_GT(chainSize(), before);
}

TEST_F(TriggerEventTest, ShowdownBeginsElsewhere_DoesNotFireDiana) {
    addUnit(P1, 641, 4, /*at_bf=*/1);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(ShowdownStartedEvent{/*battlefield=*/BattlefieldId::fromRaw(0), true});  // a different BF
    EXPECT_EQ(chainSize(), before);
}

// ── WhenOpponentPlaysAUnit: Vex (P1, at a BF) enqueues on a P2 unit play ──
TEST_F(TriggerEventTest, OpponentPlaysAUnit_FiresVex) {
    auto vex = addUnit(P1, 712, 4, /*at_bf=*/0);
    auto played = addUnit(P2, kInvalidId, 3, /*at_bf=*/0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(CardPlayedEvent{played, P2, CardType::Unit, /*count=*/1, /*energy=*/0});
    EXPECT_GT(chainSize(), before);
    // The played unit is the trigger's subject.
    EXPECT_EQ(state.chain.items.back().triggering_subject, played);
    (void)vex;
}

TEST_F(TriggerEventTest, OwnPlayDoesNotFireVex) {
    addUnit(P1, 712, 4, 0);
    auto played = addUnit(P1, kInvalidId, 3, 0);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(CardPlayedEvent{played, P1, CardType::Unit, 1, 0});  // P1's own play
    EXPECT_EQ(chainSize(), before);
}

// ── Nocturne revealed from the Main Deck: a replacement, not a trigger ──
// (Testing phase D71, by design: "As you look at or reveal me ..." is applied
// with the reveal, inside the revealing effect — EffectExecutor::
// seenOnTopOfDeck — and puts nothing on the chain. Old: a chain item.)
TEST_F(TriggerEventTest, RevealedFromTop_PutsNothingOnTheChainForNocturne) {
    auto noct = addToDeck(P1, 194);                  // in P1's main deck
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(CardRevealedEvent{noct, state.getObject(noct).card_def_id, P1,
                                  /*all=*/false, P1, ZoneType::MainDeck});
    EXPECT_EQ(chainSize(), before);
}

TEST_F(TriggerEventTest, RevealedFromHand_DoesNotFireNocturne) {
    auto noct = addToHand(P1, 194);
    ChainManager cm(state, events, card_db);
    TriggerManager tm(state, events, card_db, cm, card_registry);
    tm.subscribe();
    size_t before = chainSize();
    events.emit(CardRevealedEvent{noct, state.getObject(noct).card_def_id, P1,
                                  false, P1, ZoneType::Hand});       // not deck-top
    EXPECT_EQ(chainSize(), before);
}

}  // namespace
