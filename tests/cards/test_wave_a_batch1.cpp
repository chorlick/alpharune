/// @file test_wave_a_batch1.cpp
/// Per-card tests for Wave A batch 1. Each fully-implemented card gets at
/// least one TEST_F asserting a concrete game-state change for its clauses.
/// Escalated cards (Aspirant's Climb, Noxus Saboteur, Perched Grimwyrm,
/// Simian Ancestor, Prize of Progress) are intentionally NOT tested here.

#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"
#include "tests/cards/card_test_fixture.h"

#include <algorithm>

namespace riftbound::test {

namespace {
constexpr CardDefId kWizenedElder      = 65;
constexpr CardDefId kBilgewaterBully   = 125;
constexpr CardDefId kVanguardAttendant = 314;
constexpr CardDefId kBandleSoldier     = 713;
constexpr CardDefId kRevna             = 565;
constexpr CardDefId kPetriciteMonument = 426;
constexpr CardDefId kVoidBurrower      = 553;
constexpr CardDefId kRockfallPath      = 530;
constexpr CardDefId kSneakyDeckhand    = 176;
constexpr CardDefId kJaullFish         = 425;
}  // namespace

class WaveABatch1Test : public CardTestFixture {
protected:
    // Place a gear on the board (at a battlefield, or controller's base).
    GameObjectId addGear(PlayerId owner, CardDefId def_id, int at_bf = -1) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_def_id = def_id;
        obj.card_type = CardType::Gear;
        if (def_id != kInvalidId) {
            const auto& def = card_db.get(def_id);
            obj.name = def.name;
            obj.keywords = def.keywords;
            obj.domains = def.domains;
            obj.tags = def.tags;
        }
        if (at_bf >= 0) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    // Sum might_bonus across a unit's aura_effects.
    int auraMight(GameObjectId id) {
        int m = 0;
        for (auto& ae : state.getObject(id).aura_effects) m += ae.might_bonus;
        return m;
    }
    bool auraHasKeyword(GameObjectId id, Keyword kw) {
        for (auto& ae : state.getObject(id).aura_effects)
            if (ae.keyword == kw) return true;
        return false;
    }
};

// ─── [65] Wizened Elder — "While I'm buffed, I have an additional +1 [M]." ──

TEST_F(WaveABatch1Test, WizenedElder_BuffedGainsExtraMight) {
    EffectExecutor exec(state, events, card_db);
    auto elder = addUnit(P1, kWizenedElder, 4, 0);
    state.getObject(elder).buff_count = 1;  // buffed

    card_registry.get(kWizenedElder)->applyPassiveAura(state, P1);
    EXPECT_EQ(auraMight(elder), 1) << "buffed -> +1 [M] aura";
}

TEST_F(WaveABatch1Test, WizenedElder_UnbuffedNoExtraMight) {
    EffectExecutor exec(state, events, card_db);
    auto elder = addUnit(P1, kWizenedElder, 4, 0);
    state.getObject(elder).buff_count = 0;  // not buffed

    card_registry.get(kWizenedElder)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.getObject(elder).aura_effects.empty())
        << "not buffed -> no aura";
}

// ─── [125] Bilgewater Bully — "While I'm buffed, I have [Ganking]." ─────────

TEST_F(WaveABatch1Test, BilgewaterBully_NotGankingByDefault) {
    // The printed def must NOT grant Ganking unconditionally.
    const auto& def = card_db.get(kBilgewaterBully);
    EXPECT_FALSE(def.keywords.has(Keyword::Ganking))
        << "Ganking is conditional (while buffed), not a printed keyword";
}

TEST_F(WaveABatch1Test, BilgewaterBully_BuffedGainsGanking) {
    EffectExecutor exec(state, events, card_db);
    auto bully = addUnit(P1, kBilgewaterBully, 6, 0);
    state.getObject(bully).buff_count = 1;

    card_registry.get(kBilgewaterBully)->applyPassiveAura(state, P1);
    EXPECT_TRUE(auraHasKeyword(bully, Keyword::Ganking));
}

TEST_F(WaveABatch1Test, BilgewaterBully_UnbuffedNoGanking) {
    EffectExecutor exec(state, events, card_db);
    auto bully = addUnit(P1, kBilgewaterBully, 6, 0);
    state.getObject(bully).buff_count = 0;

    card_registry.get(kBilgewaterBully)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.getObject(bully).aura_effects.empty());
}

// ─── [314] Vanguard Attendant — "I enter ready." ───────────────────────────

TEST_F(WaveABatch1Test, VanguardAttendant_EntersReady) {
    auto* card = card_registry.get(kVanguardAttendant);
    EXPECT_TRUE(card->entersReadyOnPlay());
    // State-aware overload falls through to the unconditional one.
    EXPECT_TRUE(card->entersReadyOnPlay(state, P1));
}

// ─── [713] Bandle Soldier — "[Level 3] I enter ready." ─────────────────────

TEST_F(WaveABatch1Test, BandleSoldier_EntersReadyAtLevel3) {
    auto* card = card_registry.get(kBandleSoldier);
    setXp(P1, 3);
    EXPECT_TRUE(card->entersReadyOnPlay(state, P1)) << "3+ XP -> enter ready";
}

TEST_F(WaveABatch1Test, BandleSoldier_NotReadyBelowLevel3) {
    auto* card = card_registry.get(kBandleSoldier);
    setXp(P1, 2);
    EXPECT_FALSE(card->entersReadyOnPlay(state, P1)) << "below 3 XP -> exhausted";
}

// ─── [565] Revna — Ganking + "When you play a spell, if you spent [4]+, ready me." ─

TEST_F(WaveABatch1Test, Revna_HasGankingKeyword) {
    const auto& def = card_db.get(kRevna);
    EXPECT_TRUE(def.keywords.has(Keyword::Ganking));
}

TEST_F(WaveABatch1Test, Revna_ReadiesWhenSpent4Plus) {
    EffectExecutor exec(state, events, card_db);
    auto revna = addUnit(P1, kRevna, 7, 0);
    state.getObject(revna).is_exhausted = true;
    state.player(P1).last_spell_energy_spent = 5;  // spent 4+

    fireTriggerAs(kRevna, P1, revna, TriggerType::WhenYouPlayASpell, exec);
    EXPECT_FALSE(state.getObject(revna).is_exhausted) << "spent 4+ -> ready me";
}

TEST_F(WaveABatch1Test, Revna_DoesNotReadyBelow4) {
    EffectExecutor exec(state, events, card_db);
    auto revna = addUnit(P1, kRevna, 7, 0);
    state.getObject(revna).is_exhausted = true;
    state.player(P1).last_spell_energy_spent = 3;  // below 4

    fireTriggerAs(kRevna, P1, revna, TriggerType::WhenYouPlayASpell, exec);
    EXPECT_TRUE(state.getObject(revna).is_exhausted) << "below 4 -> stays exhausted";
}

// ─── [426] Petricite Monument — "Friendly units have [Deflect]." ───────────

TEST_F(WaveABatch1Test, PetriciteMonument_GrantsDeflectToFriendlyUnits) {
    EffectExecutor exec(state, events, card_db);
    auto gear = addGear(P1, kPetriciteMonument, /*at_bf=*/-1);  // on board (base)
    auto friendly = addUnit(P1, kInvalidId, 3, 0);
    auto enemy = addUnit(P2, kInvalidId, 3, 0);
    (void)gear;

    card_registry.get(kPetriciteMonument)->applyPassiveAura(state, P1);
    EXPECT_TRUE(auraHasKeyword(friendly, Keyword::Deflect)) << "friendly gets Deflect";
    EXPECT_FALSE(auraHasKeyword(enemy, Keyword::Deflect)) << "enemy unaffected";
}

TEST_F(WaveABatch1Test, PetriciteMonument_NoEffectWhenOffBoard) {
    EffectExecutor exec(state, events, card_db);
    // Gear not on board: create it with no location.
    auto id = state.createObject();
    auto& g = state.getObject(id);
    g.owner = P1; g.controller = P1; g.card_def_id = kPetriciteMonument;
    g.card_type = CardType::Gear; g.zone = ZoneType::Hand;  // not on board
    auto friendly = addUnit(P1, kInvalidId, 3, 0);

    card_registry.get(kPetriciteMonument)->applyPassiveAura(state, P1);
    EXPECT_TRUE(state.getObject(friendly).aura_effects.empty());
}

// ─── [530] Rockfall Path — "Units can't be played here." (engine-handled) ──

TEST_F(WaveABatch1Test, RockfallPath_TextDrivesBlocksUnitPlay) {
    // The engine lowercases ability_text and matches "can't be played here".
    std::string t = card_db.get(kRockfallPath).ability_text;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);
    EXPECT_NE(t.find("can't be played here"), std::string::npos)
        << "engine substring match drives BattlefieldState::blocks_unit_play";
}

// ─── [176] Sneaky Deckhand — "You may play me to an open battlefield." ─────

TEST_F(WaveABatch1Test, SneakyDeckhand_TextDrivesOpenBfPlay) {
    EXPECT_NE(card_db.get(kSneakyDeckhand).ability_text.find(
                  "play me to an open battlefield"),
              std::string::npos)
        << "engine substring match enables play-to-open-battlefield";
}

// ─── [425] Jaull-Fish — "I cost [2] less for each of your [Mighty] units." ──

TEST_F(WaveABatch1Test, JaullFish_CostReductionPerMightyUnit) {
    auto* card = card_registry.get(kJaullFish);
    // No mighty units yet.
    EXPECT_EQ(card->selfCostReduction(state, P1), 0);

    // Two Mighty units (might >= 5) and one non-mighty.
    addUnit(P1, kInvalidId, 5, 0);
    addUnit(P1, kInvalidId, 6, 0);
    addUnit(P1, kInvalidId, 4, 0);   // not mighty
    addUnit(P2, kInvalidId, 7, 0);   // enemy, ignored

    EXPECT_EQ(card->selfCostReduction(state, P1), 4) << "2 mighty * 2 = 4";
}

// ─── [553] Void Burrower — WhenIConquer optional reveal/banish/play/recycle ─

TEST_F(WaveABatch1Test, VoidBurrower_ConquerBanishAndPlayTop) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto legend = addUnit(P1, kVoidBurrower, 0, -1);
    state.getObject(legend).card_type = CardType::Legend;
    state.getObject(legend).is_exhausted = false;

    // Top 2 cards: top (back) is a unit we will banish+play; second recycled.
    auto recycled = addToDeck(P1, kInvalidId);                 // bottom of top-2
    auto top = addUnit(P1, kVanguardAttendant, 0, -1);          // make a real unit obj
    // Move `top` into the deck (top = back) so it's the first revealed.
    state.getObject(top).zone = ZoneType::MainDeck;
    state.getObject(top).location = std::nullopt;
    state.player(P1).main_deck.push_back(top);

    int deck_before = deckSize(P1);

    // Agent: first picks "yes" to exhaust (confirmOptional publishes [no, yes]),
    // then picks the slot that banishes `top` (the slot whose chosen_objects
    // contains top).
    PickByPredicateAgent agent([&](int call, const std::vector<Intent>& legal) -> Intent {
        // confirmOptional yes/no: pick the "yes" (label starts with "optional:").
        for (auto& i : legal)
            if (!i.chosen_objects.empty() && i.chosen_objects[0] == top) return i;
        // For the yes/no confirm, legal[1] is "yes".
        return legal.size() > 1 ? legal[1] : legal.front();
    });

    driveResumableTrigger(kVoidBurrower, P1, legend,
                          [&](const std::vector<Intent>& legal) {
                              return agent.selectAction(state, legal);
                          },
                          exec, TriggerType::WhenIConquer);

    EXPECT_TRUE(state.getObject(legend).is_exhausted) << "exhausted as cost";
    // `top` was banished then played -> now on board (not in deck).
    EXPECT_FALSE(inDeck(P1, top));
    EXPECT_TRUE(state.getObject(top).location.has_value()) << "played to board";
    // `recycled` returned to the deck (recycle the rest).
    EXPECT_TRUE(inDeck(P1, recycled));
    (void)deck_before;
}

TEST_F(WaveABatch1Test, VoidBurrower_DeclineDoesNothing) {
    EffectExecutor exec(state, events, card_db);
    exec.setCardRegistry(&card_registry);
    auto legend = addUnit(P1, kVoidBurrower, 0, -1);
    state.getObject(legend).card_type = CardType::Legend;
    state.getObject(legend).is_exhausted = false;
    auto a = addToDeck(P1, kInvalidId);
    auto b = addToDeck(P1, kInvalidId);
    int deck_before = deckSize(P1);

    // Decline the optional: confirmOptional publishes [no, yes]; pick legal[0].
    driveResumableTrigger(kVoidBurrower, P1, legend,
                          [&](const std::vector<Intent>& legal) {
                              return legal.empty() ? Intent{} : legal.front();
                          },
                          exec, TriggerType::WhenIConquer);

    EXPECT_FALSE(state.getObject(legend).is_exhausted) << "declined -> not exhausted";
    EXPECT_EQ(deckSize(P1), deck_before) << "deck untouched";
    EXPECT_TRUE(inDeck(P1, a));
    EXPECT_TRUE(inDeck(P1, b));
}

}  // namespace riftbound::test
