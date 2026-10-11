/// @file test_review_fix_auras_costs.cpp
/// Regression tests for the 2026-10 engine review: the aura pass
/// (GameEngine::recalculateAuras) and the cost-payment paths. Every test goes
/// through the engine — cleanup for auras, the play / hide / lethal-damage
/// entry points for costs — and names the plan item it pins.

#include "cards/card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"

#include <gtest/gtest.h>

namespace riftbound::test {
namespace test_review_fix_auras_costs {  // file-unique: test sources are unity-built
namespace {

constexpr CardDefId kCaptainFarron = 15;
constexpr CardDefId kRagingSoul = 19;
constexpr CardDefId kDravenShowboat = 28;
constexpr CardDefId kRagingFirebrand = 31;
constexpr CardDefId kTaricProtector = 74;
constexpr CardDefId kCaitlynPatrolling = 68;
constexpr CardDefId kLeonaZealot = 79;
constexpr CardDefId kHeimerdinger = 111;
constexpr CardDefId kEagerApprentice = 84;
constexpr CardDefId kDrMundoExpert = 109;
constexpr CardDefId kHeraldOfScales = 140;
constexpr CardDefId kFioraVictorious = 232;
constexpr CardDefId kKarthusEternal = 236;
constexpr CardDefId kDarius243 = 243;
constexpr CardDefId kTrifarianWarCamp = 289;
constexpr CardDefId kAnnieFiery = 299;
constexpr CardDefId kIncinerate = 301;         // spell, [2], Fury
constexpr CardDefId kRecruit = 266;            // unit, costs [0]
constexpr CardDefId kGarenCommander = 311;
constexpr CardDefId kSerratedDirk = 332;
constexpr CardDefId kRumbleHotheaded = 349;
constexpr CardDefId kClothArmor = 387;
constexpr CardDefId kForecaster = 388;
constexpr CardDefId kRumbleScrapper = 411;
constexpr CardDefId kAncientWarmonger = 452;
constexpr CardDefId kVexCheerless = 467;
constexpr CardDefId kDangerZone = 501;         // spell, [1][R/B], [Repeat] [1][A]
constexpr CardDefId kPurifier = 502;
constexpr CardDefId kOrnnsForge = 527;
constexpr CardDefId kDarius547 = 547;
constexpr CardDefId kMechanizedMenace = 551;
constexpr CardDefId kLotusTrap = 575;          // spell with [Hidden]
constexpr CardDefId kAllay = 603;
constexpr CardDefId kSoulShepherd = 639;
constexpr CardDefId kElderDragon = 680;
constexpr CardDefId kBaronNashor = 709;
constexpr CardDefId kAltarOfBlood = 762;
constexpr CardDefId kBlackFlameAltar = 764;
constexpr CardDefId kVaultsOfHelia = 775;
constexpr CardDefId kWujuMaster = 784;
constexpr CardDefId kShadowFiend = 801;
constexpr CardDefId kRageAmplifier = 805;
constexpr CardDefId kSereneAscetic = 817;

/// Passes priority when it can; answers every yes/no the way `say_yes` says;
/// otherwise takes the first option. Counts the yes/no questions it was asked.
class AnswerAgent : public AgentInterface {
public:
    bool say_yes = false;
    int yes_no_asked = 0;
    Intent selectAction(const GameState&, const std::vector<Intent>& legal) override {
        for (const auto& i : legal)
            if (i.type == IntentType::PassPriority) return i;
        if (legal.size() == 2 && legal[0].type == IntentType::MakeChoice &&
            legal[0].chosen_value.has_value() && legal[1].chosen_value.has_value()) {
            ++yes_no_asked;
            for (const auto& i : legal)
                if (i.chosen_value.value_or(-1) == (say_yes ? 1 : 0)) return i;
        }
        return legal.empty() ? Intent{} : legal.front();
    }
};

class AurasCostsTest : public CardTestFixture {
protected:
    std::unique_ptr<GameEngine> engine_;
    AnswerAgent a1_, a2_;
    GameState& s() { return engine_->mutableState(); }
    GameObject& o(GameObjectId id) { return s().getObject(id); }

    void SetUp() override {
        CardTestFixture::SetUp();
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
        st.turn.phase = TurnPhase::MainPhase;
        st.turn.ns_state = NeutralShowdownState::Neutral;
        st.turn.oc_state = OpenClosedState::Open;
        legend(P1);
        legend(P2);
    }

    /// An object built from its def the way GameEngine::instantiateCard does.
    GameObjectId fromDef(PlayerId p, CardDefId def_id) {
        auto id = s().createObject();
        auto& c = o(id);
        const auto& def = card_db.get(def_id);
        c.owner = c.controller = p;
        c.card_def_id = def_id;
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
    void place(GameObjectId id, int at_bf) {
        auto& c = o(id);
        if (at_bf >= 0) {
            c.zone = ZoneType::BattlefieldZone;
            c.location = BattlefieldLocation{static_cast<BattlefieldId>(at_bf)};
        } else {
            c.zone = ZoneType::Base;
            c.location = BaseLocation{c.controller};
        }
    }
    /// A card's unit / gear on the board (at_bf < 0: its controller's base).
    GameObjectId onBoard(PlayerId p, CardDefId def_id, int at_bf = -1) {
        auto id = fromDef(p, def_id);
        place(id, at_bf);
        return id;
    }
    /// A vanilla unit with no card behind it.
    GameObjectId vanilla(PlayerId p, int might, int at_bf = -1) {
        auto id = s().createObject();
        auto& u = o(id);
        u.owner = u.controller = p;
        u.card_type = CardType::Unit;
        u.name = "TestUnit";
        u.base_might = u.current_might = might;
        place(id, at_bf);
        return id;
    }
    GameObjectId legend(PlayerId p, CardDefId def_id = kInvalidId) {
        auto id = def_id != kInvalidId ? fromDef(p, def_id) : s().createObject();
        auto& lo = o(id);
        lo.owner = lo.controller = p;
        lo.card_type = CardType::Legend;
        lo.zone = ZoneType::LegendZone;
        s().player(p).legend_zone = id;
        return id;
    }
    GameObjectId inHand(PlayerId p, CardDefId def_id) {
        auto id = fromDef(p, def_id);
        o(id).zone = ZoneType::Hand;
        s().player(p).hand.push_back(id);
        return id;
    }
    GameObjectId rune(PlayerId p, Domain d, bool exhausted = false) {
        auto id = s().createObject();
        auto& r = o(id);
        r.owner = r.controller = p;
        r.card_type = CardType::Rune;
        r.name = "Rune";
        r.domains = {d};
        r.zone = ZoneType::Base;
        r.location = BaseLocation{p};
        r.is_exhausted = exhausted;
        return id;
    }
    /// Make battlefield `bf` the card `def_id`.
    GameObjectId battlefieldCard(int bf, CardDefId def_id) {
        auto id = fromDef(PlayerId::None, def_id);
        o(id).zone = ZoneType::BattlefieldZone;
        s().battlefields[bf].card_object_id = id;
        return id;
    }
    GameObjectId attach(CardDefId gear_def, GameObjectId unit) {
        auto g = fromDef(o(unit).controller, gear_def);
        o(g).zone = o(unit).zone;
        o(g).location = o(unit).location;
        o(g).attached_to = unit;
        o(unit).attachments.push_back(g);
        return g;
    }
    /// A combat at `bf`: `attacker` attacking, the other player defending.
    void combatAt(int bf, PlayerId attacker) {
        auto& b = s().battlefields[bf];
        b.combat_in_progress = true;
        b.attacker = attacker;
        b.defender = opponent(attacker);
    }
    void cleanup() { engine_->testHook_cleanup(); }
    int might(GameObjectId id) { return o(id).current_might; }
    int runesInBase(PlayerId p) {
        int n = 0;
        for (auto& [id, r] : s().objects)
            if (r.isRune() && r.controller == p && r.location.has_value()) ++n;
        return n;
    }
    bool inRuneDeck(PlayerId p, GameObjectId rune_id) {
        const auto& d = s().player(p).rune_deck;
        return std::find(d.begin(), d.end(), rune_id) != d.end() &&
               o(rune_id).zone == ZoneType::RuneDeck;
    }
};

// ─── 2.1 / 8.1: an aura is the card's own hook, applied once ────────────────

TEST_F(AurasCostsTest, GarenCommander_OnlyOtherUnitsAtHisBattlefield) {
    // "Other friendly units have +1 [M] here." The text parser also read it as
    // "every other friendly unit anywhere": +2 here, +1 everywhere else.
    auto garen = onBoard(P1, kGarenCommander, 0);
    auto here = vanilla(P1, 2, 0);
    auto elsewhere = vanilla(P1, 2, 1);
    auto at_base = vanilla(P1, 2);
    cleanup();
    EXPECT_EQ(might(here), 3);
    EXPECT_EQ(might(elsewhere), 2);
    EXPECT_EQ(might(at_base), 2);
    EXPECT_EQ(might(garen), 5) << "\"other\"";
}

TEST_F(AurasCostsTest, GarenCommander_InBaseGivesNothing) {
    onBoard(P1, kGarenCommander);
    auto a = vanilla(P1, 2, 0);
    auto b = vanilla(P1, 2);
    cleanup();
    EXPECT_EQ(might(a), 2);
    EXPECT_EQ(might(b), 2);
}

TEST_F(AurasCostsTest, DariusExecutioner_BothPrintings_PlusOneHereOnly) {
    // 243 / 547 had no hook of their own: the parser's "anywhere" reading was
    // all they had. One printing per player, each at its own battlefield.
    auto d243 = onBoard(P1, kDarius243, 0);
    auto here1 = vanilla(P1, 2, 0);
    auto base1 = vanilla(P1, 2);
    auto d547 = onBoard(P2, kDarius547, 1);
    auto here2 = vanilla(P2, 2, 1);
    auto base2 = vanilla(P2, 2);
    cleanup();
    EXPECT_EQ(might(here1), 3);
    EXPECT_EQ(might(base1), 2);
    EXPECT_EQ(might(d243), 6);
    EXPECT_EQ(might(here2), 3);
    EXPECT_EQ(might(base2), 2);
    EXPECT_EQ(might(d547), 6);
}

TEST_F(AurasCostsTest, WujuMaster_LevelSixGate) {
    // "[Level 6][>] Your units have +1 [M]." The parser ignored the level.
    legend(P1, kWujuMaster);
    auto u = vanilla(P1, 2);
    cleanup();
    EXPECT_EQ(might(u), 2) << "below Level 6";
    s().player(P1).xp = 6;
    cleanup();
    EXPECT_EQ(might(u), 3) << "+1 at Level 6, once";
}

TEST_F(AurasCostsTest, RageAmplifier_PlusOneOrPlusTwoEmpowered) {
    auto amp = onBoard(P1, kRageAmplifier);
    auto u = vanilla(P1, 2);
    cleanup();
    EXPECT_EQ(might(u), 3);
    o(amp).empowered = 1;
    cleanup();
    EXPECT_EQ(might(u), 4) << "+2 instead, not +3";
}

TEST_F(AurasCostsTest, CaptainFarron_AssaultForOtherFriendlyUnitsHereOnly) {
    // "units here have" is a substring of "other friendly units here have":
    // the parser also gave [Assault] to Farron and to the enemy units there.
    auto farron = onBoard(P1, kCaptainFarron, 0);
    auto ally = vanilla(P1, 2, 0);
    auto enemy = vanilla(P2, 2, 0);
    combatAt(0, P1);
    cleanup();
    EXPECT_TRUE(o(ally).hasKeyword(Keyword::Assault));
    EXPECT_EQ(might(ally), 3) << "+1 [M] while an attacker";
    EXPECT_FALSE(o(enemy).hasKeyword(Keyword::Assault));
    EXPECT_FALSE(o(farron).hasKeyword(Keyword::Assault)) << "\"other\"";
    EXPECT_EQ(might(farron), 5);
}

TEST_F(AurasCostsTest, LeonaZealot_OnlyStunnedEnemiesAtHerBattlefield) {
    // "Stunned enemy units here have -8 [M], to a minimum of 1 [M]." Read as
    // "units here have -8", it hit every unit there, Leona included.
    auto leona = onBoard(P1, kLeonaZealot, 0);
    auto ally = vanilla(P1, 4, 0);
    auto stunned = vanilla(P2, 6, 0);
    auto unstunned = vanilla(P2, 6, 0);
    auto stunned_elsewhere = vanilla(P2, 6, 1);
    o(stunned).is_stunned = true;
    o(stunned_elsewhere).is_stunned = true;
    cleanup();
    EXPECT_EQ(might(stunned), 1) << "-8, to a minimum of 1";
    EXPECT_EQ(might(unstunned), 6);
    EXPECT_EQ(might(stunned_elsewhere), 6);
    EXPECT_EQ(might(ally), 4);
    EXPECT_EQ(might(leona), 6);
}

TEST_F(AurasCostsTest, BaronNashor_OtherFriendlyUnitsPlusTwo) {
    auto baron = onBoard(P1, kBaronNashor, 0);
    auto ally = vanilla(P1, 2);
    auto enemy = vanilla(P2, 2, 1);
    cleanup();
    EXPECT_EQ(might(ally), 4);
    EXPECT_EQ(might(baron), 12);
    EXPECT_EQ(might(enemy), 2);
}

TEST_F(AurasCostsTest, DravenShowboat_MightIncreasedByPoints) {
    auto draven = onBoard(P1, kDravenShowboat);
    cleanup();
    EXPECT_EQ(might(draven), 3);
    s().player(P1).score = 4;
    cleanup();
    EXPECT_EQ(might(draven), 7);
}

TEST_F(AurasCostsTest, Allay_DeflectForYourOtherUnitsHereNotEnemies) {
    auto allay = onBoard(P1, kAllay, 0);
    auto ally = vanilla(P1, 2, 0);
    auto enemy = vanilla(P2, 2, 0);
    cleanup();
    EXPECT_TRUE(o(ally).hasKeyword(Keyword::Deflect));
    EXPECT_EQ(engine_->testHook_executor().deflectCostFor(P2, ally), 1)
        << "one instance, not two";
    EXPECT_FALSE(o(enemy).hasKeyword(Keyword::Deflect));
    (void)allay;
}

TEST_F(AurasCostsTest, TrifarianWarCamp_PlusOneOnce) {
    battlefieldCard(0, kTrifarianWarCamp);
    auto here = vanilla(P1, 2, 0);
    auto elsewhere = vanilla(P1, 2, 1);
    cleanup();
    EXPECT_EQ(might(here), 3);
    EXPECT_EQ(might(elsewhere), 2);
}

TEST_F(AurasCostsTest, FioraVictorious_KeywordsOnlyWhileMighty) {
    auto fiora = onBoard(P1, kFioraVictorious);
    cleanup();
    EXPECT_FALSE(o(fiora).hasKeyword(Keyword::Ganking)) << "4 [M]: not Mighty";
    EXPECT_FALSE(o(fiora).hasKeyword(Keyword::Shield));
    o(fiora).buff_count = 1;
    o(fiora).recomputeMight();
    cleanup();
    EXPECT_TRUE(o(fiora).hasKeyword(Keyword::Ganking));
    EXPECT_TRUE(o(fiora).hasKeyword(Keyword::Deflect));
    EXPECT_TRUE(o(fiora).hasKeyword(Keyword::Shield));
}

// ─── 2.2: copies of an aura card stack once each ────────────────────────────

TEST_F(AurasCostsTest, TwoGarens_EachGivesPlusOne) {
    // Called once per instance while looping over every instance, two Garens
    // gave four +1 effects.
    auto g1 = onBoard(P1, kGarenCommander, 0);
    auto g2 = onBoard(P1, kGarenCommander, 0);
    auto ally = vanilla(P1, 2, 0);
    cleanup();
    EXPECT_EQ(might(ally), 4);
    EXPECT_EQ(might(g1), 6) << "+1 from the other Garen";
    EXPECT_EQ(might(g2), 6);
}

TEST_F(AurasCostsTest, TwoDrMundos_EachCountsTheTrashOnce) {
    auto m1 = onBoard(P1, kDrMundoExpert);
    auto m2 = onBoard(P1, kDrMundoExpert);
    for (int i = 0; i < 5; ++i) {
        auto c = s().createObject();
        o(c).owner = o(c).controller = P1;
        o(c).zone = ZoneType::Trash;
        s().player(P1).trash.push_back(c);
    }
    cleanup();
    EXPECT_EQ(might(m1), 6 + 5);
    EXPECT_EQ(might(m2), 6 + 5);
}

TEST_F(AurasCostsTest, PerCopyEffectsStillStack) {
    // One call per (card, controller) must not collapse effects that are
    // meant to stack per copy.
    onBoard(P1, kSoulShepherd);
    onBoard(P1, kSoulShepherd);
    onBoard(P1, kKarthusEternal);
    onBoard(P1, kKarthusEternal);
    onBoard(P1, kAnnieFiery);
    onBoard(P1, kAnnieFiery);
    onBoard(P1, kRumbleScrapper);
    auto scrapper2 = onBoard(P1, kRumbleScrapper);
    auto token = vanilla(P1, 1);
    o(token).super_type = SuperType::Token;
    cleanup();
    EXPECT_EQ(might(token), 3) << "two Soul Shepherds: +2";
    EXPECT_EQ(s().player(P1).deathknell_double_count, 2) << "two Karthus";
    EXPECT_EQ(s().player(P1).bonus_damage_dealt, 2) << "two Annies";
    EXPECT_EQ(might(scrapper2), 4 + 2) << "two Rumbles: each Mech +2";
}

TEST_F(AurasCostsTest, EachPlayersCopyAppliesForItsOwnController) {
    auto g1 = onBoard(P1, kGarenCommander, 0);
    auto g2 = onBoard(P2, kGarenCommander, 1);
    auto a1 = vanilla(P1, 2, 0);
    auto a2 = vanilla(P2, 2, 1);
    cleanup();
    EXPECT_EQ(might(a1), 3);
    EXPECT_EQ(might(a2), 3);
    EXPECT_EQ(might(g1), 5);
    EXPECT_EQ(might(g2), 5);
}

// ─── 8.3: aura-granted [Assault N] / [Shield N] are worth their Might ───────

TEST_F(AurasCostsTest, EquippedAssaultAndShield_AddMightInCombat) {
    auto attacker = vanilla(P1, 3, 0);
    auto defender = vanilla(P2, 3, 0);
    attach(kSerratedDirk, attacker);     // [Assault 2]
    attach(kClothArmor, defender);       // [Shield 2]
    cleanup();
    EXPECT_EQ(might(attacker), 3) << "no combat: Assault is not on";
    EXPECT_EQ(might(defender), 3);
    combatAt(0, P1);
    cleanup();
    EXPECT_EQ(might(attacker), 5) << "Assault 2, counted once";
    EXPECT_EQ(might(defender), 5) << "Shield 2, counted once";
}

TEST_F(AurasCostsTest, AuraShield_TaricAndMechanizedMenace) {
    legend(P2, kMechanizedMenace);       // "Your Mechs have [Shield]."
    onBoard(P2, kTaricProtector, 0);     // "Other friendly units here have [Shield]."
    auto mech = vanilla(P2, 2, 0);
    o(mech).tags = {"Mech"};
    auto plain = vanilla(P2, 2, 0);
    vanilla(P1, 2, 0);
    combatAt(0, P1);
    cleanup();
    EXPECT_EQ(might(plain), 3) << "Taric's Shield";
    EXPECT_EQ(might(mech), 4) << "Taric's Shield + the legend's (CR 814: they sum)";
}

TEST_F(AurasCostsTest, AuraAssault_RumbleFarronWarmongerRagingSoul) {
    auto rumble = onBoard(P1, kRumbleHotheaded, 0);   // "Your Mechs each have [Assault]."
    auto mech = vanilla(P1, 2, 0);
    o(mech).tags = {"Mech"};
    auto warmonger = onBoard(P1, kAncientWarmonger, 0);  // Assault = enemy units here
    auto soul = onBoard(P1, kRagingSoul, 0);          // Assault while you've discarded
    s().player(P1).has_discarded_this_turn = true;
    vanilla(P2, 2, 0);
    vanilla(P2, 2, 0);
    combatAt(0, P1);
    cleanup();
    EXPECT_EQ(might(mech), 3);
    EXPECT_EQ(might(rumble), 5) << "Rumble is a Mech: Assault 1, not 2";
    EXPECT_EQ(might(warmonger), 4 + 2) << "two enemy units here: Assault 2";
    EXPECT_EQ(might(soul), 4 + 1) << "Assault 1, counted once";
}

TEST_F(AurasCostsTest, AuraAssaultShield_PurifierAndBlackFlameAltar) {
    legend(P1, kPurifier);                  // "Your Equipment each give [Assault]."
    battlefieldCard(0, kBlackFlameAltar);   // "Units here with [Temporary] have [Shield]."
    auto attacker = vanilla(P1, 3, 0);
    attach(kClothArmor, attacker);          // an Equipment (its own Shield is off: attacking)
    auto temp = vanilla(P2, 3, 0);
    o(temp).keywords.set(Keyword::Temporary);
    auto plain = vanilla(P2, 3, 0);
    combatAt(0, P1);
    cleanup();
    EXPECT_EQ(might(attacker), 4) << "Assault 1 from the legend, through the Equipment";
    EXPECT_EQ(might(temp), 4) << "Shield 1 from the battlefield";
    EXPECT_EQ(might(plain), 3);
}

TEST_F(AurasCostsTest, AuraKeywordHelper_AssaultCountedOnce) {
    // Shadow Fiend: "[Empowered][>] I have [Assault 3]." (card_helpers auraKeyword)
    auto fiend = onBoard(P1, kShadowFiend, 0);
    o(fiend).empowered = 1;
    vanilla(P2, 2, 0);
    combatAt(0, P1);
    cleanup();
    EXPECT_EQ(might(fiend), 2 + 3);
}

TEST_F(AurasCostsTest, AuraKeywordHelper_ShieldCountedOnce) {
    // Serene Ascetic: "[Empowered][>] I have [Deflect] and [Shield 3]."
    auto ascetic = onBoard(P2, kSereneAscetic, 0);
    o(ascetic).empowered = 1;
    vanilla(P1, 2, 0);
    combatAt(0, P1);
    cleanup();
    EXPECT_EQ(might(ascetic), card_db.get(kSereneAscetic).might + 3);
    o(ascetic).empowered = 0;
    cleanup();
    EXPECT_EQ(might(ascetic), card_db.get(kSereneAscetic).might);
}

// ─── 8.4: a keyword granted by an aura counts when the unit is played ───────

class AuraVisionTest : public AurasCostsTest {
protected:
    GameObjectId top_spell_ = kInvalidId;
    /// P1 plays a Mech with no printed Vision; a spell is on top of the deck.
    void playAMech() {
        auto bottom = fromDef(P1, kSoulShepherd);
        o(bottom).zone = ZoneType::MainDeck;
        s().player(P1).main_deck.push_back(bottom);
        top_spell_ = fromDef(P1, kIncinerate);
        o(top_spell_).zone = ZoneType::MainDeck;
        s().player(P1).main_deck.push_back(top_spell_);
        auto mech = inHand(P1, kRumbleScrapper);        // [5][B], Mech
        s().player(P1).rune_pool.energy = 5;
        s().player(P1).rune_pool.power[static_cast<int>(Domain::Mind)] = 1;
        Intent play;
        play.type = IntentType::PlayCard;
        play.player = P1;
        play.card = mech;
        engine_->testHook_executeIntent(play);
        ASSERT_TRUE(o(mech).location.has_value());
    }
};

TEST_F(AuraVisionTest, Forecaster_MechPlayedLooksAtTheTopCard) {
    // "Your Mechs have [Vision]." Vision was read from the played card's own
    // keywords, before any aura could reach it: Forecaster did nothing.
    onBoard(P1, kForecaster);
    cleanup();
    playAMech();
    EXPECT_EQ(s().player(P1).main_deck.front(), top_spell_)
        << "Vision: the spell on top was recycled to the bottom";
}

TEST_F(AuraVisionTest, NoVisionSource_TopCardStays) {
    playAMech();
    EXPECT_EQ(s().player(P1).main_deck.back(), top_spell_);
}

// ─── 8.2: aura cost modifiers are rebuilt, not accumulated ──────────────────

TEST_F(AurasCostsTest, OrnnsForge_OneModifierHoweverManyCleanups) {
    battlefieldCard(0, kOrnnsForge);
    s().battlefields[0].controller = P1;
    auto holder = vanilla(P1, 2, 0);
    for (int i = 0; i < 6; ++i) cleanup();
    ASSERT_EQ(s().player(P1).cost_modifiers.size(), 1u);
    EXPECT_TRUE(s().player(P1).cost_modifiers[0].from_aura);

    // Control is lost: the discount goes with it.
    o(holder).location = BaseLocation{P1};
    o(holder).zone = ZoneType::Base;
    cleanup();
    EXPECT_FALSE(s().battlefields[0].controller.has_value());
    EXPECT_TRUE(s().player(P1).cost_modifiers.empty());
}

TEST_F(AurasCostsTest, EffectModifiersSurviveTheAuraPass) {
    PlayerState::CostModifier m;        // "the next spell you play this turn costs [5] less"
    m.energy_reduction = 5;
    m.next_spell_only = true;
    s().player(P1).cost_modifiers.push_back(m);
    for (int i = 0; i < 3; ++i) cleanup();
    ASSERT_EQ(s().player(P1).cost_modifiers.size(), 1u);
    EXPECT_FALSE(s().player(P1).cost_modifiers[0].from_aura);
}

TEST_F(AurasCostsTest, HeraldOfScales_DragonsOnly_NoStacking) {
    // "Your Dragons' Energy costs are reduced by [2], to a minimum of [1]."
    onBoard(P1, kHeraldOfScales);
    auto dragon = inHand(P1, kRagingFirebrand);     // Dragon, [6][R]
    auto not_dragon = inHand(P1, kAnnieFiery);      // [5][R]
    for (int i = 0; i < 5; ++i) cleanup();
    for (int i = 0; i < 3; ++i) rune(P1, Domain::Fury);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, dragon)) << "[4][R] after one -2, not free";
    EXPECT_FALSE(engine_->testHook_canAfford(P1, not_dragon));
    rune(P1, Domain::Fury);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, dragon)) << "4 runes pay [4][R]";
    EXPECT_FALSE(engine_->testHook_canAfford(P1, not_dragon)) << "not a Dragon: still [5][R]";
}

TEST_F(AurasCostsTest, EagerApprentice_SpellsOnly) {
    // "While I'm at a battlefield, the Energy costs for spells you play is
    // reduced by [1], to a minimum of [1]."
    auto apprentice = onBoard(P1, kEagerApprentice);
    auto spell = inHand(P1, kIncinerate);           // [2]
    auto unit = inHand(P1, kSoulShepherd);          // [5]
    rune(P1, Domain::Fury);
    for (int i = 0; i < 4; ++i) cleanup();
    EXPECT_FALSE(engine_->testHook_canAfford(P1, spell)) << "in base: no discount";
    place(apprentice, 0);
    for (int i = 0; i < 4; ++i) cleanup();
    EXPECT_TRUE(engine_->testHook_canAfford(P1, spell)) << "[2] -> [1]";
    for (int i = 0; i < 3; ++i) rune(P1, Domain::Mind);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, unit)) << "units are not discounted";
}

TEST_F(AurasCostsTest, VexCheerless_OnlyWhileSheIsInCombat) {
    auto vex = onBoard(P1, kVexCheerless, 0);
    vanilla(P2, 2, 0);
    auto mine = inHand(P1, kIncinerate);            // [2]
    auto theirs = inHand(P2, kIncinerate);
    rune(P1, Domain::Fury);
    rune(P2, Domain::Fury);
    rune(P2, Domain::Fury);
    for (int i = 0; i < 4; ++i) cleanup();
    EXPECT_FALSE(engine_->testHook_canAfford(P1, mine)) << "not in combat";
    EXPECT_TRUE(engine_->testHook_canAfford(P2, theirs));
    combatAt(0, P2);
    for (int i = 0; i < 4; ++i) cleanup();
    ASSERT_EQ(o(vex).combat_designation, CombatDesignation::Defender);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, mine)) << "friendly spells cost [1] less";
    EXPECT_FALSE(engine_->testHook_canAfford(P2, theirs))
        << "enemy spells cost [1][A] more: two runes can't pay [3][A]";
    EXPECT_TRUE(s().player(P1).cost_modifiers.empty()) << "no permanent modifiers";
}

TEST_F(AurasCostsTest, VaultsOfHelia_ModifierIsUnitsOnly) {
    // "When you hold here, your non-token units cost [1] more to play this turn."
    PlayerState::CostModifier m;
    m.energy_increase = 1;
    m.units_only = true;
    s().player(P1).cost_modifiers.push_back(m);
    auto spell = inHand(P1, kIncinerate);           // [2]
    auto unit = inHand(P1, kEagerApprentice);       // [3]
    rune(P1, Domain::Mind);
    rune(P1, Domain::Mind);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, spell)) << "a spell is not taxed";
    rune(P1, Domain::Mind);
    EXPECT_FALSE(engine_->testHook_canAfford(P1, unit)) << "a unit costs [4]";
    // And the card sets the scope.
    const auto vaults = battlefieldCard(1, kVaultsOfHelia);
    s().player(P1).cost_modifiers.clear();
    CardContext ctx{s(), events, engine_->testHook_executor(), P1, vaults};
    card_registry.get(kVaultsOfHelia)->onTrigger(ctx, {});
    ASSERT_EQ(s().player(P1).cost_modifiers.size(), 1u);
    EXPECT_TRUE(s().player(P1).cost_modifiers[0].units_only);
}

// ─── 5.9: one cost computation for the gate and the payment ─────────────────

TEST_F(AurasCostsTest, OneShotModifier_KeptUntilACardItAppliesTo) {
    // The "next spell" discount was consumed by any play that went through
    // the payment path, even one it did not apply to.
    PlayerState::CostModifier m;
    m.energy_reduction = 5;
    m.next_spell_only = true;
    m.combat_active_only = true;        // does not apply: no combat
    s().player(P1).cost_modifiers.push_back(m);
    s().player(P1).rune_pool.energy = 10;
    auto spell = inHand(P1, kIncinerate);
    EXPECT_TRUE(engine_->testHook_payCardCost(P1, spell));
    EXPECT_EQ(s().player(P1).rune_pool.energy, 8) << "paid in full";
    EXPECT_EQ(s().player(P1).cost_modifiers.size(), 1u) << "and the one-shot is still there";

    combatAt(0, P1);
    auto spell2 = inHand(P1, kIncinerate);
    EXPECT_TRUE(engine_->testHook_payCardCost(P1, spell2));
    EXPECT_EQ(s().player(P1).rune_pool.energy, 8) << "discounted to [0]";
    EXPECT_TRUE(s().player(P1).cost_modifiers.empty()) << "consumed by the spell it applied to";
}

TEST_F(AurasCostsTest, EnemyOneShotModifier_ConsumedFromTheOpponentsList) {
    PlayerState::CostModifier m;        // P2's effect: "the next enemy spell costs [2] more"
    m.energy_increase = 2;
    m.next_spell_only = true;
    m.affects_enemy_only = true;
    s().player(P2).cost_modifiers.push_back(m);
    s().player(P1).rune_pool.energy = 10;
    auto spell = inHand(P1, kIncinerate);
    EXPECT_TRUE(engine_->testHook_payCardCost(P1, spell));
    EXPECT_EQ(s().player(P1).rune_pool.energy, 6);
    EXPECT_TRUE(s().player(P2).cost_modifiers.empty());
}

TEST_F(AurasCostsTest, PowerDiscount_HonoursTheSameFiltersAsEnergy) {
    // powerCostDelta skipped first_gear_per_turn / non-token / combat filters.
    PlayerState::CostModifier m;
    m.power_reduction = 1;
    m.combat_active_only = true;
    s().player(P1).cost_modifiers.push_back(m);
    auto card = inHand(P1, kAnnieFiery);            // [5][R]
    s().player(P1).rune_pool.energy = 5;
    EXPECT_FALSE(engine_->testHook_canAfford(P1, card)) << "no combat: the [R] is still owed";
    combatAt(0, P1);
    EXPECT_TRUE(engine_->testHook_canAfford(P1, card));
}

TEST_F(AurasCostsTest, MinimumCost_LimitsTheReductionOnly) {
    // "reduced by [1], to a minimum of [1]"
    PlayerState::CostModifier m;
    m.energy_reduction = 1;
    m.min_cost = 1;
    s().player(P1).cost_modifiers.push_back(m);
    PlayerState::CostModifier big = m;
    big.energy_reduction = 5;
    s().player(P1).cost_modifiers.push_back(big);
    auto spell = inHand(P1, kIncinerate);           // [2] - 6, floor [1]
    EXPECT_FALSE(engine_->testHook_canAfford(P1, spell)) << "the floor of [1] holds";
    s().player(P1).rune_pool.energy = 1;
    EXPECT_TRUE(engine_->testHook_canAfford(P1, spell));
    // A card that already costs less than the minimum is not made dearer.
    s().player(P1).rune_pool.energy = 0;
    auto free_card = inHand(P1, kRecruit);          // [0]
    EXPECT_TRUE(engine_->testHook_canAfford(P1, free_card)) << "[0] stays [0]";
}

TEST_F(AurasCostsTest, GrantedAbility_HonoursTheGrantingCardsGates) {
    // Heimerdinger, Inventor has the [E] abilities of friendly units. Caitlyn,
    // Patrolling's is "Use this ability only while I'm at a battlefield" and
    // needs a unit at a battlefield to choose: granted abilities skipped both.
    auto heimer = onBoard(P1, kHeimerdinger);
    auto caitlyn = onBoard(P1, kCaitlynPatrolling);
    auto offered = [&]() {
        cleanup();
        for (const auto& i : engine_->generateLegalActions())
            if (i.type == IntentType::ActivateAbility && i.ability_source == heimer &&
                i.granted_ability_def == kCaitlynPatrolling) return true;
        return false;
    };
    EXPECT_FALSE(offered()) << "Caitlyn's own gate and target requirement apply";
    s().battlefields[0].controller = P1;
    place(caitlyn, 0);
    EXPECT_TRUE(offered());
}

// ─── 2.3 / 5.10: Repeat is paid through the shared additional-cost path ─────

TEST_F(AurasCostsTest, Repeat_PowerComesFromRecyclingTheRuneToTheRuneDeck) {
    // Danger Zone: [Repeat] [1][A]. One ready rune pays the tranche — exhaust
    // it for the [1], recycle it for the [A]. payRepeatCost used to refuse a
    // ready rune (so the paid-for repeat never happened) and, when it did
    // recycle, put the rune in the MAIN deck.
    a1_.say_yes = true;
    auto target = vanilla(P1, 2);
    auto spell = inHand(P1, kDangerZone);
    s().player(P1).rune_pool.energy = 1;                                   // base [1]
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;    // base [R]
    auto r = rune(P1, Domain::Calm);

    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    play.targets = {target};
    engine_->testHook_executeIntent(play);

    EXPECT_EQ(a1_.yes_no_asked, 1) << "offered once: the gate and the payment agree";
    EXPECT_TRUE(inRuneDeck(P1, r));
    EXPECT_FALSE(o(r).is_exhausted);
    const auto& deck = s().player(P1).main_deck;
    EXPECT_EQ(std::find(deck.begin(), deck.end(), r), deck.end()) << "not in the Main Deck";
    EXPECT_EQ(s().player(P1).power_spent_this_turn, 1);
    EXPECT_EQ(o(target).temp_might_bonus, 2) << "the effect ran twice";
}

TEST_F(AurasCostsTest, Repeat_FloatingResourcesPayTheTranche) {
    a1_.say_yes = true;
    auto target = vanilla(P1, 2);
    auto spell = inHand(P1, kDangerZone);
    s().player(P1).rune_pool.energy = 2;
    s().player(P1).rune_pool.power[static_cast<int>(Domain::Fury)] = 1;
    s().player(P1).rune_pool.universal_power = 1;

    Intent play;
    play.type = IntentType::PlayCard;
    play.player = P1;
    play.card = spell;
    play.targets = {target};
    engine_->testHook_executeIntent(play);

    EXPECT_EQ(s().player(P1).rune_pool.energy, 0);
    EXPECT_EQ(s().player(P1).rune_pool.universal_power, 0);
    EXPECT_EQ(o(target).temp_might_bonus, 2);
}

// ─── 2.25: Hide pays [A] through the shared path ────────────────────────────

TEST_F(AurasCostsTest, Hide_OfferedAndPaidWithAnExhaustedRune) {
    s().battlefields[0].controller = P1;
    vanilla(P1, 2, 0);
    auto card = inHand(P1, kLotusTrap);
    auto r = rune(P1, Domain::Fury, /*exhausted=*/true);

    const auto legal = engine_->generateLegalActions();
    const Intent* hide = nullptr;
    for (const auto& i : legal)
        if (i.type == IntentType::HideCard && i.card == card) hide = &i;
    ASSERT_NE(hide, nullptr) << "an exhausted rune can be recycled for the [A]";

    engine_->testHook_executeIntent(*hide);
    EXPECT_TRUE(o(card).is_hidden);
    EXPECT_TRUE(inRuneDeck(P1, r));
    EXPECT_FALSE(o(r).is_exhausted) << "a recycled rune is not exhausted in the deck";
    EXPECT_EQ(s().player(P1).power_spent_this_turn, 1);
}

TEST_F(AurasCostsTest, Hide_FloatingPowerIsSpentBeforeARune) {
    s().battlefields[0].controller = P1;
    vanilla(P1, 2, 0);
    auto card = inHand(P1, kLotusTrap);
    s().player(P1).rune_pool.universal_power = 1;

    const auto legal = engine_->generateLegalActions();
    const Intent* hide = nullptr;
    for (const auto& i : legal)
        if (i.type == IntentType::HideCard && i.card == card) hide = &i;
    ASSERT_NE(hide, nullptr) << "floating Power pays the [A] with no rune at all";

    auto keep = rune(P1, Domain::Fury);
    engine_->testHook_executeIntent(*hide);
    EXPECT_TRUE(o(card).is_hidden);
    EXPECT_EQ(s().player(P1).rune_pool.universal_power, 0);
    EXPECT_TRUE(o(keep).location.has_value()) << "the rune stays in base";
}

TEST_F(AurasCostsTest, Hide_NotOfferedWithoutAnyPower) {
    s().battlefields[0].controller = P1;
    vanilla(P1, 2, 0);
    auto card = inHand(P1, kLotusTrap);
    for (const auto& i : engine_->generateLegalActions())
        EXPECT_FALSE(i.type == IntentType::HideCard && i.card == card);
    // Executed anyway (a stale intent), the unpaid Hide does not happen.
    Intent hide;
    hide.type = IntentType::HideCard;
    hide.player = P1;
    hide.card = card;
    hide.chosen_battlefield = 0;
    engine_->testHook_executeIntent(hide);
    EXPECT_FALSE(o(card).is_hidden);
    EXPECT_EQ(o(card).zone, ZoneType::Hand);
}

// ─── 2.26 / 5.4: Altar of Blood is the battlefield card's own replacement ───

class AltarOfBloodTest : public AurasCostsTest {
protected:
    GameObjectId dying_ = kInvalidId;
    void SetUp() override {
        AurasCostsTest::SetUp();
        battlefieldCard(0, kAltarOfBlood);
        dying_ = vanilla(P1, 2, 0);
        vanilla(P2, 5, 0);
        o(dying_).damage_marked = 5;
    }
};

TEST_F(AltarOfBloodTest, PaysThreePowerFromExhaustedRunesAndRecalls) {
    // The old block wanted three READY runes and moved them by hand.
    combatAt(0, P2);
    GameObjectId runes[3];
    for (auto& r : runes) r = rune(P1, Domain::Calm, /*exhausted=*/true);
    a1_.say_yes = true;

    engine_->testHook_processLethalDamage();

    EXPECT_EQ(a1_.yes_no_asked, 1);
    ASSERT_TRUE(o(dying_).location.has_value()) << "it did not die";
    EXPECT_TRUE(o(dying_).isAtBase()) << "recalled";
    EXPECT_TRUE(o(dying_).is_exhausted);
    EXPECT_EQ(o(dying_).damage_marked, 0) << "healed";
    for (auto r : runes) EXPECT_TRUE(inRuneDeck(P1, r));
    EXPECT_EQ(s().player(P1).power_spent_this_turn, 3);
    EXPECT_EQ(o(dying_).moves_this_turn, 0) << "a recall is not a move (CR 458)";
}

TEST_F(AltarOfBloodTest, FloatingPowerCounts) {
    combatAt(0, P2);
    s().player(P1).rune_pool.universal_power = 3;
    a1_.say_yes = true;
    engine_->testHook_processLethalDamage();
    EXPECT_TRUE(o(dying_).isAtBase());
    EXPECT_EQ(s().player(P1).rune_pool.universal_power, 0);
}

TEST_F(AltarOfBloodTest, DecliningLetsTheUnitDie) {
    combatAt(0, P2);
    for (int i = 0; i < 3; ++i) rune(P1, Domain::Calm);
    a1_.say_yes = false;
    engine_->testHook_processLethalDamage();
    EXPECT_EQ(a1_.yes_no_asked, 1);
    EXPECT_FALSE(o(dying_).location.has_value());
    EXPECT_EQ(runesInBase(P1), 3) << "nothing was paid";
}

TEST_F(AltarOfBloodTest, NotOfferedWhenItCannotBePaid) {
    a1_.say_yes = true;
    combatAt(0, P2);
    rune(P1, Domain::Calm);
    rune(P1, Domain::Calm);                 // only [A][A]
    engine_->testHook_processLethalDamage();
    EXPECT_EQ(a1_.yes_no_asked, 0);
    EXPECT_FALSE(o(dying_).location.has_value());
}

TEST_F(AltarOfBloodTest, NotOfferedOutsideCombat) {
    a1_.say_yes = true;
    for (int i = 0; i < 3; ++i) rune(P1, Domain::Calm);
    engine_->testHook_processLethalDamage();
    EXPECT_EQ(a1_.yes_no_asked, 0) << "\"during combat\"";
    EXPECT_FALSE(o(dying_).location.has_value());
}

TEST_F(AltarOfBloodTest, OnlyUnitsAtTheAltar) {
    combatAt(1, P2);
    auto elsewhere = vanilla(P1, 2, 1);
    vanilla(P2, 5, 1);
    o(elsewhere).damage_marked = 5;
    o(dying_).damage_marked = 0;
    for (int i = 0; i < 3; ++i) rune(P1, Domain::Calm);
    a1_.say_yes = true;
    engine_->testHook_processLethalDamage();
    EXPECT_EQ(a1_.yes_no_asked, 0);
    EXPECT_FALSE(o(elsewhere).location.has_value());
}

// ─── 5.4: Elder Dragon's rule is the card's, not a text match ───────────────

TEST_F(AurasCostsTest, ElderDragon_AnyOfYourDamageKills) {
    onBoard(P1, kElderDragon);
    auto enemy = vanilla(P2, 6, 0);
    auto own = vanilla(P1, 6, 0);
    o(enemy).damage_marked = 1;
    o(enemy).damage_marked_by[playerIndex(P1)] = 1;
    o(own).damage_marked = 1;
    o(own).damage_marked_by[playerIndex(P2)] = 1;
    engine_->testHook_processLethalDamage();
    EXPECT_FALSE(o(enemy).location.has_value()) << "1 damage from the Dragon's controller";
    EXPECT_TRUE(o(own).location.has_value()) << "the opponent has no such rule";
}

TEST_F(AurasCostsTest, WithoutElderDragon_NonLethalDamageDoesNotKill) {
    auto enemy = vanilla(P2, 6, 0);
    o(enemy).damage_marked = 1;
    o(enemy).damage_marked_by[playerIndex(P1)] = 1;
    engine_->testHook_processLethalDamage();
    EXPECT_TRUE(o(enemy).location.has_value());
}

}  // namespace
}  // namespace test_review_fix_auras_costs
}  // namespace riftbound::test
