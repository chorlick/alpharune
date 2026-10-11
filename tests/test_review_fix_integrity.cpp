// Regression tests for the 2026-10 engine review, stream "integrity"
// (plans/05-integrity.md: 5.1, 5.2, 5.3, 5.6, 5.7, 5.8, 5.11).

#include <gtest/gtest.h>

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "rules/deck_validator.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace riftbound::test::review_fix_integrity {

// Ids outside the real card table and the feature-extractor vocabulary; these
// cards only ever reach a CardRegistry / CardDB, never a game.
constexpr CardDefId kTYordleSignature = 999101;
constexpr CardDefId kTTwoTagLegend = 999102;
constexpr CardDefId kTWrongKey = 999103;

class TYordleSignature : public SpellCard {
public:
    const CardDef& def() const override {
        static const CardDef d = [] {
            CardDef d;
            d.id = kTYordleSignature;
            d.name = "Test Yordle Signature";
            d.card_type = CardType::Spell;
            d.super_type = SuperType::Signature;
            d.tags = {"Yordle"};
            return d;
        }();
        return d;
    }
};

// A legend printing two tags whose author did not say which is the champion tag.
class TTwoTagLegend : public LegendCard {
public:
    const CardDef& def() const override {
        static const CardDef d = [] {
            CardDef d;
            d.id = kTTwoTagLegend;
            d.name = "Test Two-Tag Legend";
            d.card_type = CardType::Legend;
            d.tags = {"Yordle", "Kennen"};
            return d;
        }();
        return d;
    }
};

class IntegrityFixTest : public ::testing::Test {
protected:
    CardRegistry registry;
    CardDB db;

    void SetUp() override {
        registry.loadAll();
        registry.registerCard(kTYordleSignature, std::make_unique<TYordleSignature>());
        registry.registerCard(kTTwoTagLegend, std::make_unique<TTwoTagLegend>());
        db.buildFromClasses(registry);
    }

    CardDefId idOf(const char* name) {
        const CardDef* c = db.findByName(name);
        EXPECT_NE(c, nullptr) << name;
        return c ? c->id : 0;
    }

    /// Heart of the Tempest (942): tags {Yordle, Kennen}, champion tag Kennen.
    DeckSubmission kennenDeck(const char* champion) {
        DeckSubmission deck;
        deck.legend = 942;
        deck.chosen_champion = idOf(champion);
        return deck;
    }

    static bool hasError(const ValidationResult& r, const std::string& needle) {
        for (const auto& e : r.errors)
            if (e.find(needle) != std::string::npos) return true;
        return false;
    }

    static std::string errorsOf(const ValidationResult& r) {
        std::string out;
        for (const auto& e : r.errors) out += "\n  " + e;
        return out;
    }
};

// ─── 5.1 champion tag ────────────────────────────────────────────────────────

TEST_F(IntegrityFixTest, LegendChampionTagIsAuthoredForTwoTagLegend) {
    ASSERT_EQ(db.get(942).name, "Heart of the Tempest");
    EXPECT_EQ(db.get(942).legendChampionTag(), "Kennen");
}

TEST_F(IntegrityFixTest, EveryLegendHasOneChampionTagCarriedByAChampionUnit) {
    for (const auto& [id, def] : db.all()) {
        if (def.card_type != CardType::Legend || id == kTTwoTagLegend) continue;
        const std::string& tag = def.legendChampionTag();
        EXPECT_FALSE(tag.empty()) << def.name << " (" << id << ") prints "
                                  << def.tags.size() << " tags and names no champion_tag";
        if (tag.empty()) continue;
        EXPECT_TRUE(def.hasTag(tag)) << def.name;
        bool has_champion = false;
        for (const auto& [cid, c] : db.all())
            if (c.super_type == SuperType::Champion && c.hasTag(tag)) has_champion = true;
        EXPECT_TRUE(has_champion) << def.name << ": no champion unit is tagged " << tag;
    }
}

// CR 103.3.a.2: sharing the Yordle tag with the legend is not enough.
TEST_F(IntegrityFixTest, ChosenChampionSharingOnlyANonChampionTagIsRejected) {
    for (const char* name : {"Teemo, Strategist", "Vex, Apathetic", "Fizz, Trickster",
                             "Poppy, Paragon"}) {
        auto result = DeckValidator(db).validate(kennenDeck(name));
        EXPECT_TRUE(hasError(result, "doesn't share a champion tag")) << name;
    }
}

TEST_F(IntegrityFixTest, ChosenChampionWithTheLegendsChampionTagIsAccepted) {
    for (const char* name : {"Kennen, Storm of Shuriken", "Kennen, Keeper of Balance"}) {
        auto result = DeckValidator(db).validate(kennenDeck(name));
        EXPECT_FALSE(hasError(result, "champion tag")) << name << errorsOf(result);
    }
}

// CR 103.3.d.2: Signature cards need the legend's champion tag too.
TEST_F(IntegrityFixTest, SignatureSharingOnlyANonChampionTagIsRejected) {
    auto deck = kennenDeck("Kennen, Storm of Shuriken");
    deck.main_deck.push_back(kTYordleSignature);
    deck.main_deck.push_back(idOf("Lightning Rush"));  // a real Kennen Signature
    auto result = DeckValidator(db).validate(deck);
    EXPECT_TRUE(hasError(result, "'Test Yordle Signature' doesn't match")) << errorsOf(result);
    EXPECT_FALSE(hasError(result, "'Lightning Rush' doesn't match")) << errorsOf(result);
}

TEST_F(IntegrityFixTest, LegendWithNoIdentifiableChampionTagIsAnErrorNotAGuess) {
    DeckSubmission deck;
    deck.legend = kTTwoTagLegend;
    deck.chosen_champion = idOf("Teemo, Strategist");
    auto result = DeckValidator(db).validate(deck);
    EXPECT_TRUE(hasError(result, "has no single champion tag")) << errorsOf(result);
}

// ─── 5.2 card types ──────────────────────────────────────────────────────────

TEST_F(IntegrityFixTest, MainDeckRejectsCardsThatAreNotUnitsGearOrSpells) {
    struct Case { CardDefId id; const char* what; };
    const Case cases[] = {
        {326, "token card: Gold"},                              // token gear, no Token supertype
        {idOf("Recruit"), "token card: Recruit"},               // SuperType::Token
        {idOf("Order Rune"), "Rune card: Order Rune"},
        {idOf("Windswept Hillock"), "Battlefield card: Windswept Hillock"},
        {idOf("Deceiver"), "Legend card: Deceiver"},
    };
    for (const auto& c : cases) {
        auto deck = kennenDeck("Kennen, Storm of Shuriken");
        deck.main_deck.push_back(c.id);
        auto result = DeckValidator(db).validate(deck);
        EXPECT_TRUE(hasError(result, std::string("Main deck contains ") + c.what))
            << c.what << errorsOf(result);

        // Tournament Rules 601.1.c.2: a sideboard holds only valid Main Deck cards.
        auto side = kennenDeck("Kennen, Storm of Shuriken");
        side.sideboard.push_back(c.id);
        result = DeckValidator(db).validate(side);
        EXPECT_TRUE(hasError(result, std::string("Sideboard contains ") + c.what))
            << c.what << errorsOf(result);
    }
}

TEST_F(IntegrityFixTest, MainDeckAcceptsUnitsGearAndSpells) {
    auto deck = kennenDeck("Kennen, Storm of Shuriken");
    for (const auto& [id, def] : db.all()) {
        if (def.isTokenCard()) continue;
        if (def.card_type == CardType::Unit || def.card_type == CardType::Spell ||
            def.card_type == CardType::Gear)
            deck.main_deck.push_back(id);
    }
    ASSERT_GT(deck.main_deck.size(), 500u);
    auto result = DeckValidator(db).validate(deck);
    EXPECT_FALSE(hasError(result, "Main deck contains")) << errorsOf(result);
}

TEST_F(IntegrityFixTest, TokenBattlefieldIsNotADeckBattlefield) {
    ASSERT_EQ(db.get(558).name, "Baron Pit");
    auto deck = kennenDeck("Kennen, Storm of Shuriken");
    deck.battlefields = {558, idOf("Windswept Hillock"), idOf("Dusk Rose Lab")};
    auto result = DeckValidator(db).validate(deck);
    EXPECT_TRUE(hasError(result, "token battlefield: Baron Pit")) << errorsOf(result);
    EXPECT_FALSE(hasError(result, "Windswept Hillock")) << errorsOf(result);
}

// ─── 5.3 deck list parse errors ──────────────────────────────────────────────

TEST_F(IntegrityFixTest, DeckListLineWithOnlyACountThrowsRuntimeError) {
    auto path = std::filesystem::temp_directory_path() /
                "riftbound_review_fix_integrity_count_only.txt";
    {
        std::ofstream f(path);
        f << "Legend:\n1 Deceiver\n\nChampion:\n1 LeBlanc, Fragmented\n\nMainDeck:\n3\n";
    }
    try {
        DeckValidator::loadFromDeckList(path.string(), db);
        ADD_FAILURE() << "expected a parse failure";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("line 8"), std::string::npos) << e.what();
        EXPECT_NE(std::string(e.what()).find("missing card name"), std::string::npos) << e.what();
    } catch (const std::exception& e) {
        ADD_FAILURE() << "wrong exception type: " << e.what();
    }
    std::filesystem::remove(path);
}

TEST_F(IntegrityFixTest, DeckListNamesKeepInnerSpacesAndTrailingBlanks) {
    auto path = std::filesystem::temp_directory_path() /
                "riftbound_review_fix_integrity_names.txt";
    {
        std::ofstream f(path);
        f << "Legend:\n1 Deceiver\nChampion:\n1   LeBlanc, Fragmented  \r\n"
             "MainDeck:\n2 Cull the Weak\t\n";
    }
    auto deck = DeckValidator::loadFromDeckList(path.string(), db);
    std::filesystem::remove(path);
    EXPECT_EQ(deck.legend, idOf("Deceiver"));
    EXPECT_EQ(deck.chosen_champion, idOf("LeBlanc, Fragmented"));
    ASSERT_EQ(deck.main_deck.size(), 2u);
    EXPECT_EQ(deck.main_deck[0], idOf("Cull the Weak"));
}

// ─── 5.6 invalid ids are defined behaviour in every build type ───────────────

TEST(IntegrityFixIds, GetObjectThrowsOnUnknownId) {
    GameState state;
    GameObjectId id = state.createObject();
    EXPECT_NO_THROW(state.getObject(id));
    EXPECT_THROW(state.getObject(id + 1), std::logic_error);
    EXPECT_THROW(state.getObject(kInvalidId), std::logic_error);
    const GameState& cstate = state;
    EXPECT_THROW(cstate.getObject(kInvalidId), std::logic_error);
}

TEST(IntegrityFixIds, PlayerNoneIsNotIndexable) {
    EXPECT_EQ(playerIndex(PlayerId::Player1), 0);
    EXPECT_EQ(playerIndex(PlayerId::Player2), 1);
    EXPECT_THROW(playerIndex(PlayerId::None), std::logic_error);
    GameState state;
    EXPECT_THROW(state.player(PlayerId::None), std::logic_error);
    EXPECT_NO_THROW(state.player(PlayerId::Player2));
}

TEST(IntegrityFixIds, OpponentOfNoneIsNone) {
    EXPECT_EQ(opponent(PlayerId::Player1), PlayerId::Player2);
    EXPECT_EQ(opponent(PlayerId::Player2), PlayerId::Player1);
    EXPECT_EQ(opponent(PlayerId::None), PlayerId::None);
}

// ─── 5.7 Intent equality ─────────────────────────────────────────────────────

TEST(IntegrityFixIntent, EqualityDistinguishesAltCostAndGrantedAbilityTwins) {
    Intent play;
    play.type = IntentType::PlayCard;
    play.player = PlayerId::Player1;
    play.card = 7;
    Intent alt = play;
    EXPECT_TRUE(play == alt);
    alt.use_alt_play_cost = true;   // Jhin: same card, alternative cost
    EXPECT_FALSE(play == alt);

    Intent own;
    own.type = IntentType::ActivateAbility;
    own.player = PlayerId::Player1;
    own.ability_source = 9;
    Intent granted = own;
    EXPECT_TRUE(own == granted);
    granted.granted_ability_def = 506;  // same source, aura-granted ability
    EXPECT_FALSE(own == granted);

    // The display label stays out of the comparison.
    Intent labelled = play;
    labelled.choice_label = "Yes";
    EXPECT_TRUE(play == labelled);
}

// ─── 5.8 BattlefieldState::unitsControlledBy ─────────────────────────────────

TEST(IntegrityFixBattlefield, UnitsControlledByFindsUnitsAtTheBattlefield) {
    GameState state;
    BattlefieldState bf;
    bf.id = 0;
    BattlefieldState other;
    other.id = 1;

    auto addUnit = [&](PlayerId controller, std::optional<LocationId> loc, ZoneType zone) {
        GameObjectId id = state.createObject();
        auto& obj = state.getObject(id);
        obj.card_type = CardType::Unit;
        obj.controller = controller;
        obj.owner = controller;
        obj.zone = zone;
        obj.location = loc;
        return id;
    };
    GameObjectId here1 = addUnit(PlayerId::Player1, BattlefieldLocation{0}, ZoneType::BattlefieldZone);
    GameObjectId here2 = addUnit(PlayerId::Player1, BattlefieldLocation{0}, ZoneType::BattlefieldZone);
    GameObjectId enemy = addUnit(PlayerId::Player2, BattlefieldLocation{0}, ZoneType::BattlefieldZone);
    addUnit(PlayerId::Player1, BattlefieldLocation{1}, ZoneType::BattlefieldZone);
    addUnit(PlayerId::Player1, BaseLocation{PlayerId::Player1}, ZoneType::Base);
    GameObjectId gear = addUnit(PlayerId::Player1, BattlefieldLocation{0}, ZoneType::BattlefieldZone);
    state.getObject(gear).card_type = CardType::Gear;

    auto mine = bf.unitsControlledBy(PlayerId::Player1, state.objects);
    std::sort(mine.begin(), mine.end());
    EXPECT_EQ(mine, (std::vector<GameObjectId>{here1, here2}));
    EXPECT_EQ(bf.unitsControlledBy(PlayerId::Player2, state.objects),
              (std::vector<GameObjectId>{enemy}));
    EXPECT_TRUE(other.unitsControlledBy(PlayerId::Player2, state.objects).empty());
    // Agrees with hasUnitsFrom.
    EXPECT_TRUE(bf.hasUnitsFrom(PlayerId::Player1, state.objects));
    EXPECT_FALSE(other.hasUnitsFrom(PlayerId::Player2, state.objects));
}

// ─── 5.11 registration guards ────────────────────────────────────────────────

TEST(IntegrityFixRegistry, DuplicateIdIsRejected) {
    CardRegistry reg;
    reg.registerCard(kTYordleSignature, std::make_unique<TYordleSignature>());
    EXPECT_THROW(reg.registerCard(kTYordleSignature, std::make_unique<TYordleSignature>()),
                 std::logic_error);
    EXPECT_EQ(reg.size(), 1u);
    ASSERT_NE(reg.get(kTYordleSignature), nullptr);
}

TEST(IntegrityFixRegistry, KeyMustEqualTheCardsOwnDefId) {
    CardRegistry reg;
    EXPECT_THROW(reg.registerCard(kTWrongKey, std::make_unique<TYordleSignature>()),
                 std::logic_error);
    EXPECT_THROW(reg.registerCard(kTWrongKey, nullptr), std::logic_error);
    EXPECT_EQ(reg.size(), 0u);
}

TEST(IntegrityFixRegistry, EveryShippedCardRegistersOnceUnderItsOwnId) {
    CardRegistry reg;
    ASSERT_NO_THROW(reg.loadAll());
    CardDB db;
    db.buildFromClasses(reg);
    EXPECT_EQ(db.size(), reg.size());
    for (CardDefId id : reg.classDefIds()) EXPECT_EQ(db.get(id).id, id);
}

}  // namespace riftbound::test::review_fix_integrity
