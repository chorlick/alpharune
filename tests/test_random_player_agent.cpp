/// The `random-player` agent (agents/random_player_agent.h): uniform over the
/// legal actions that are not a pass; passes with probability P when it could
/// act (default 0.2, `pass=0` never) and whenever a pass is all there is.

#include <gtest/gtest.h>

#include "agents/random_player_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "rules/random_deck.h"
#include "tools/fuzz/fuzz_game.h"

#include <map>
#include <string>
#include <vector>

namespace riftbound::test::random_player_agent {

Intent ofType(IntentType t) {
    Intent i;
    i.type = t;
    i.player = PlayerId::Player1;
    return i;
}
Intent value(int v) {
    Intent i = ofType(IntentType::MakeChoice);
    i.chosen_value = v;
    return i;
}
/// The "No" of a yes/no, as Card::confirmOptional builds it (Intent::declines).
Intent no() {
    Intent i = value(0);
    i.declines = true;
    return i;
}
Intent object(uint32_t id) {
    Intent i = ofType(IntentType::MakeChoice);
    i.chosen_objects = {GameObjectId::fromRaw(id)};
    return i;
}

/// How often each option is chosen in `n` asks.
std::vector<int> tally(RandomPlayerAgent& agent, const std::vector<Intent>& legal, int n) {
    GameState state;
    std::vector<int> count(legal.size(), 0);
    for (int k = 0; k < n; ++k) {
        const Intent chosen = agent.selectAction(state, legal);
        for (std::size_t i = 0; i < legal.size(); ++i)
            if (legal[i] == chosen) { ++count[i]; break; }
    }
    return count;
}

TEST(RandomPlayerAgent, WhatIsAPass) {
    // The passes of the turn structure, and declines by type.
    for (IntentType t : {IntentType::PassPriority, IntentType::PassFocus, IntentType::EndTurn,
                         IntentType::DeclineOptionalTrigger, IntentType::DeclineTriggeredCost}) {
        const std::vector<Intent> legal = {ofType(t), ofType(IntentType::PlayCard)};
        EXPECT_TRUE(isPassOption(legal, 0)) << toString(t);
        EXPECT_FALSE(isPassOption(legal, 1));
    }
    // A mulligan that sets nothing aside keeps the hand.
    {
        Intent keep = ofType(IntentType::MulliganDecision);
        Intent swap = keep;
        swap.cards_to_mulligan = {GameObjectId::fromRaw(3)};
        const std::vector<Intent> legal = {keep, swap};
        EXPECT_TRUE(isPassOption(legal, 0));
        EXPECT_FALSE(isPassOption(legal, 1));
    }
    // No / yes: the asker marks the decline (Intent::declines).
    {
        const std::vector<Intent> legal = {no(), value(1)};
        EXPECT_TRUE(isPassOption(legal, 0));
        EXPECT_FALSE(isPassOption(legal, 1));
    }
    // A1: two modes, an X of 0 or 1, "cost [1] less / [A] less": the values
    // 0 and 1 with no decline among them. Neither is a pass. (It was: a
    // prompt of exactly the values 0 and 1 was taken for a no / yes.)
    {
        const std::vector<Intent> legal = {value(0), value(1)};
        EXPECT_FALSE(isPassOption(legal, 0));
        EXPECT_FALSE(isPassOption(legal, 1));
    }
    // "None" / "No more targets" / "Move no more units" / "Keep it": the bare
    // value 0 beside options that name an object, wherever it stands.
    {
        const std::vector<Intent> legal = {object(4), value(0), object(5)};
        EXPECT_FALSE(isPassOption(legal, 0));
        EXPECT_TRUE(isPassOption(legal, 1));
        EXPECT_FALSE(isPassOption(legal, 2));
    }
    // A named card counts as naming something.
    {
        Intent named = ofType(IntentType::MakeChoice);
        named.chosen_card_def = CardDefId::fromRaw(7);
        const std::vector<Intent> legal = {value(0), named};
        EXPECT_TRUE(isPassOption(legal, 0));
        EXPECT_FALSE(isPassOption(legal, 1));
    }
    // Three modes, or an X of 0..3: value 0 is a choice like the others.
    {
        const std::vector<Intent> legal = {value(0), value(1), value(2)};
        for (std::size_t i = 0; i < legal.size(); ++i) EXPECT_FALSE(isPassOption(legal, i)) << i;
    }
    // Objects only (a rune to exhaust, a target): nothing is a pass.
    {
        const std::vector<Intent> legal = {object(1), object(2)};
        EXPECT_FALSE(isPassOption(legal, 0));
        EXPECT_FALSE(isPassOption(legal, 1));
    }
    // Other decisions have no pass.
    for (IntentType t : {IntentType::PlayCard, IntentType::StandardMove,
                         IntentType::ActivateAbility, IntentType::AssignCombatDamage,
                         IntentType::ChooseBattlefield, IntentType::PlayFirstDecision,
                         IntentType::HideCard}) {
        const std::vector<Intent> legal = {ofType(t), ofType(IntentType::EndTurn)};
        EXPECT_FALSE(isPassOption(legal, 0)) << toString(t);
    }
}

TEST(RandomPlayerAgent, WithPassZeroItNeverPassesWhileAnotherOptionIsLegal) {
    RandomPlayerAgent agent(11, 0.0);
    Intent play_a = ofType(IntentType::PlayCard);
    play_a.card = GameObjectId::fromRaw(1);
    Intent play_b = play_a;
    play_b.card = GameObjectId::fromRaw(2);
    const std::vector<int> main_phase =
        tally(agent, {ofType(IntentType::EndTurn), play_a, play_b}, 2000);
    EXPECT_EQ(main_phase[0], 0);
    EXPECT_GT(main_phase[1], 800);   // uniform over the two plays
    EXPECT_GT(main_phase[2], 800);

    EXPECT_EQ(tally(agent, {no(), value(1)}, 500)[0], 0);
    // A1: the first of two modes is taken as often as the second.
    for (int n : tally(agent, {value(0), value(1)}, 2000)) EXPECT_GT(n, 800);
    EXPECT_EQ(tally(agent, {object(4), value(0), object(5)}, 500)[1], 0);
    // Three values: no pass among them, so all three are taken.
    for (int n : tally(agent, {value(0), value(1), value(2)}, 3000)) EXPECT_GT(n, 800);
}

TEST(RandomPlayerAgent, ItPassesWhenAPassIsAllThereIs) {
    for (double pass : {0.0, 0.2, 1.0}) {
        RandomPlayerAgent agent(5, pass);
        GameState state;
        for (IntentType t : {IntentType::PassPriority, IntentType::PassFocus, IntentType::EndTurn})
            EXPECT_EQ(agent.selectAction(state, {ofType(t)}).type, t);
        // Two passes and nothing else: one of them.
        const std::vector<int> n =
            tally(agent, {ofType(IntentType::PassPriority), ofType(IntentType::EndTurn)}, 400);
        EXPECT_GT(n[0], 100);
        EXPECT_GT(n[1], 100);
    }
    // No option at all: concede, as the random agent does.
    RandomPlayerAgent agent(5);
    GameState state;
    EXPECT_EQ(agent.selectAction(state, {}).type, IntentType::Concede);
}

TEST(RandomPlayerAgent, TheDefaultPassesOneTimeInFive) {
    EXPECT_DOUBLE_EQ(kDefaultPassProbability, 0.2);
    const std::vector<Intent> legal = {ofType(IntentType::EndTurn), ofType(IntentType::PlayCard)};
    RandomPlayerAgent by_default(21);
    const int passed = tally(by_default, legal, 20000)[0];
    EXPECT_GT(passed, 3700);   // 4000 expected; 3 sigma is about 170
    EXPECT_LT(passed, 4300);

    RandomPlayerAgent half(21, 0.5);
    const int half_passed = tally(half, legal, 20000)[0];
    EXPECT_GT(half_passed, 9500);
    EXPECT_LT(half_passed, 10500);

    RandomPlayerAgent always(21, 1.0);
    EXPECT_EQ(tally(always, legal, 500)[0], 500);
}

TEST(RandomPlayerAgent, TheSpec) {
    double pass = -1;
    EXPECT_TRUE(isRandomPlayerSpec("random-player", &pass));
    EXPECT_DOUBLE_EQ(pass, 0.2);
    EXPECT_TRUE(isRandomPlayerSpec("random-player:pass=0", &pass));
    EXPECT_DOUBLE_EQ(pass, 0.0);
    EXPECT_TRUE(isRandomPlayerSpec("random-player:pass=0.35", &pass));
    EXPECT_DOUBLE_EQ(pass, 0.35);
    EXPECT_FALSE(isRandomPlayerSpec("random"));
    EXPECT_FALSE(isRandomPlayerSpec("random-players"));
    EXPECT_THROW(isRandomPlayerSpec("random-player:pass=2"), std::runtime_error);
    EXPECT_THROW(isRandomPlayerSpec("random-player:pass=abc"), std::runtime_error);
    EXPECT_THROW(isRandomPlayerSpec("random-player:sims=3"), std::runtime_error);
    EXPECT_THROW(RandomPlayerAgent(1, 1.5), std::invalid_argument);
}

TEST(RandomPlayerAgent, TheSameSeedMakesTheSameChoices) {
    const std::vector<Intent> legal = {ofType(IntentType::EndTurn), object(1), object(2),
                                       object(3), value(0)};
    GameState state;
    for (double pass : {0.0, 0.2}) {
        RandomPlayerAgent a(99, pass), b(99, pass), c(100, pass);
        bool differs = false;
        for (int k = 0; k < 300; ++k) {
            const Intent x = a.selectAction(state, legal);
            EXPECT_TRUE(x == b.selectAction(state, legal));
            if (!(x == c.selectAction(state, legal))) differs = true;
        }
        EXPECT_TRUE(differs);
    }
}

/// An agent that forwards to a RandomPlayerAgent and checks, at every real
/// decision of a real game, that a pass is chosen only when it had to be.
class Watching : public AgentInterface {
public:
    Watching(uint64_t seed, double pass, long* decisions, long* avoidable_passes)
        : inner_(seed, pass), decisions_(decisions), avoidable_(avoidable_passes) {}
    Intent selectAction(const GameState& state, const std::vector<Intent>& legal) override {
        const Intent chosen = inner_.selectAction(state, legal);
        ++*decisions_;
        bool can_act = false, chose_pass = false;
        for (std::size_t i = 0; i < legal.size(); ++i) {
            const bool pass = isPassOption(legal, i);
            if (!pass) can_act = true;
            if (legal[i] == chosen && pass) chose_pass = true;
        }
        if (can_act && chose_pass) ++*avoidable_;
        return chosen;
    }

private:
    RandomPlayerAgent inner_;
    long* decisions_;
    long* avoidable_;
};

// Whole games, real engine: with pass=0 the seat never ends its turn while
// anything else is legal, and the games still end (costs run out, units
// exhaust). The same seed plays the same game.
TEST(RandomPlayerAgent, GamesOfForcedPlayEndAndRepeat) {
    CardRegistry registry;
    registry.loadAll();
    CardDB db;
    db.buildFromClasses(registry);
    const RandomDeckBuilder builder(db);

    long decisions = 0, avoidable = 0;
    for (int g = 0; g < 12; ++g) {
        const DeckSubmission a = builder.build(4100 + g), b = builder.build(4200 + g);
        const uint64_t seed = 900 + g;
        GameResult first;
        for (int run = 0; run < 2; ++run) {
            EventBus events;
            GameEngine engine(db, events, registry);
            Watching p1(seed * 2, 0.0, &decisions, &avoidable);
            Watching p2(seed * 2 + 1, 0.0, &decisions, &avoidable);
            const GameResult r = engine.runGame(a, b, p1, p2, seed);
            EXPECT_NE(r.winner, PlayerId::None) << "game " << g;
            EXPECT_LT(r.total_turns, 200) << "game " << g;
            if (run == 0) { first = r; continue; }
            EXPECT_EQ(r.total_decisions, first.total_decisions) << "game " << g;
            EXPECT_EQ(r.winner, first.winner) << "game " << g;
            EXPECT_EQ(r.total_turns, first.total_turns) << "game " << g;
        }
    }
    EXPECT_GT(decisions, 2000);
    EXPECT_EQ(avoidable, 0);
}

// The fuzz game loop seats it by spec, and it is not the random agent's game.
TEST(RandomPlayerAgent, TheFuzzSeatsItBySpec) {
    CardRegistry registry;
    registry.loadAll();
    CardDB db;
    db.buildFromClasses(registry);
    const RandomDeckBuilder builder(db);
    const DeckSubmission a = builder.build(4100), b = builder.build(4200);

    const fuzz::GameOutcome random = fuzz::playGame(db, registry, a, b, 31);
    fuzz::GameOptions opt;
    opt.agents[0] = opt.agents[1] = "random-player";
    const fuzz::GameOutcome player = fuzz::playGame(db, registry, a, b, 31, opt);
    const fuzz::GameOutcome again = fuzz::playGame(db, registry, a, b, 31, opt);
    EXPECT_FALSE(player.failed) << player.termination << " " << player.exception;
    EXPECT_EQ(player.decisions, again.decisions);
    EXPECT_EQ(player.winner, again.winner);
    EXPECT_NE(player.decisions, random.decisions);

    opt.agents[1] = "no-such-agent";
    EXPECT_THROW(fuzz::playGame(db, registry, a, b, 31, opt), std::invalid_argument);
}

} // namespace riftbound::test::random_player_agent
