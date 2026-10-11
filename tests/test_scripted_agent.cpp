/// Tests for ScriptedAgent — the fixed policy the supervised sanity check
/// imitates. Its header promises: most expensive play first, then the move
/// onto the weakest-held battlefield, then abilities, then prompts, then
/// pass; always a legal action; never a concede while anything else is legal.

#include <gtest/gtest.h>

#include "agents/scripted_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "rules/deck_validator.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

using namespace riftbound;

namespace {

class ScriptedAgentTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
    }
    static void TearDownTestSuite() { delete db_; delete registry_; }

    GameObjectId addCard(GameState& s, const char* name, PlayerId owner) {
        const CardDef* def = db_->findByName(name);
        EXPECT_NE(def, nullptr) << name;
        GameObjectId id = s.createObject();
        auto& o = s.getObject(id);
        o.card_def_id = def ? def->id : kInvalidId;
        o.owner = owner;
        o.controller = owner;
        return id;
    }

    static Intent play(PlayerId p, GameObjectId card) {
        Intent i; i.type = IntentType::PlayCard; i.player = p; i.card = card;
        return i;
    }

    static CardRegistry* registry_;
    static CardDB* db_;
};

CardRegistry* ScriptedAgentTest::registry_ = nullptr;
CardDB* ScriptedAgentTest::db_ = nullptr;

TEST_F(ScriptedAgentTest, PlaysTheMostExpensiveCardBeforeAnythingElse) {
    GameState s;
    const auto cheap = addCard(s, "Lonely Poro", PlayerId::Player1);
    const auto dear  = addCard(s, "Baron Nashor", PlayerId::Player1);
    ASSERT_LT(db_->get(s.getObject(cheap).card_def_id).energy_cost,
              db_->get(s.getObject(dear).card_def_id).energy_cost);

    const std::vector<Intent> legal = {
        Intent::endTurn(PlayerId::Player1),
        play(PlayerId::Player1, cheap),
        play(PlayerId::Player1, dear),
        Intent::standardMove(PlayerId::Player1, {cheap}, BattlefieldLocation{0}),
    };
    ScriptedAgent agent(*db_);
    EXPECT_EQ(agent.selectIndex(s, legal), 2);
    EXPECT_EQ(agent.selectAction(s, legal), legal[2]);
}

TEST_F(ScriptedAgentTest, MovesToTheWeakestHeldBattlefieldWithTheMostUnits) {
    GameState s;
    BattlefieldState bf0; bf0.id = 0; s.battlefields.push_back(bf0);
    BattlefieldState bf1; bf1.id = 1; s.battlefields.push_back(bf1);
    const auto mine_a = addCard(s, "Lonely Poro", PlayerId::Player1);
    const auto mine_b = addCard(s, "Lonely Poro", PlayerId::Player1);
    // The opponent holds battlefield 0 with 5 Might and battlefield 1 with 2.
    auto place = [&](BattlefieldId bf, int might) {
        const auto id = addCard(s, "Lonely Poro", PlayerId::Player2);
        auto& o = s.getObject(id);
        o.card_type = CardType::Unit;
        o.zone = ZoneType::BattlefieldZone;
        o.location = BattlefieldLocation{bf};
        o.current_might = might;
    };
    place(0, 5);
    place(1, 2);

    const std::vector<Intent> legal = {
        Intent::endTurn(PlayerId::Player1),
        Intent::standardMove(PlayerId::Player1, {mine_a, mine_b}, BattlefieldLocation{0}),
        Intent::standardMove(PlayerId::Player1, {mine_a}, BattlefieldLocation{1}),
        Intent::standardMove(PlayerId::Player1, {mine_a, mine_b}, BattlefieldLocation{1}),
        Intent::standardMove(PlayerId::Player1, {mine_a}, BaseLocation{PlayerId::Player1}),
    };
    ScriptedAgent agent(*db_);
    EXPECT_EQ(agent.selectIndex(s, legal), 3);
}

TEST_F(ScriptedAgentTest, AnswersPromptsAndNeverConcedesWhileItCanPass) {
    GameState s;
    ScriptedAgent agent(*db_);

    Intent no;  no.type = IntentType::MakeChoice;  no.player = PlayerId::Player1;
    no.chosen_value = 0;
    Intent yes = no; yes.chosen_value = 1;
    EXPECT_EQ(agent.selectIndex(s, {no, yes}), 1) << "yes over no";

    const auto card = addCard(s, "Lonely Poro", PlayerId::Player1);
    EXPECT_EQ(agent.selectIndex(s, {Intent::mulligan(PlayerId::Player1, {card}),
                                    Intent::mulligan(PlayerId::Player1, {})}),
              1) << "keeps the opening hand";

    EXPECT_EQ(agent.selectIndex(s, {Intent::concede(PlayerId::Player1),
                                    Intent::passPriority(PlayerId::Player1)}),
              1);
    EXPECT_EQ(agent.selectIndex(s, {}), -1);
    EXPECT_EQ(agent.selectAction(s, {}).type, IntentType::Concede);
}

// Over whole games, from both seats: always one of the offered actions, the
// same one selectIndex names, and never a concede.
TEST_F(ScriptedAgentTest, AlwaysReturnsALegalActionInRealGames) {
    struct Checked : AgentInterface {
        explicit Checked(const CardDB& db) : agent(db) {}
        Intent selectAction(const GameState& state,
                            const std::vector<Intent>& legal) override {
            Intent chosen = agent.selectAction(state, legal);
            ++decisions;
            if (legal.empty()) return chosen;
            const int idx = agent.selectIndex(state, legal);
            if (idx < 0 || idx >= static_cast<int>(legal.size()) ||
                !(legal[idx] == chosen)) ++illegal;
            if (chosen.type == IntentType::Concede) ++concedes;
            return chosen;
        }
        ScriptedAgent agent;
        int decisions = 0, illegal = 0, concedes = 0;
    };

    const char* root = std::getenv("RIFTBOUND_ROOT");
    const std::string decks = std::string(root ? root : "..") + "/decks/";
    const auto deck1 = DeckValidator::loadFromDeckList(decks + "yi_rebuild.txt", *db_);
    const auto deck2 = DeckValidator::loadFromDeckList(decks + "fiora.txt", *db_);

    for (uint64_t seed : {101u, 102u}) {
        EventBus bus;
        GameEngine engine(*db_, bus, *registry_);
        Checked p1(*db_), p2(*db_);
        const auto result = engine.runGame(deck1, deck2, p1, p2, seed);
        EXPECT_GT(result.total_decisions, 20);
        for (const Checked* a : {&p1, &p2}) {
            EXPECT_GT(a->decisions, 10);
            EXPECT_EQ(a->illegal, 0);
            EXPECT_EQ(a->concedes, 0);
        }
    }
}

} // namespace
