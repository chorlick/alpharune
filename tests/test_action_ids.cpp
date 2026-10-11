/// Action ids over real legal lists (plans/01 item 1.12).
///
/// The property: over every legal list the engine produces, two distinct legal
/// intents never get the same action id, and every legal intent can be found
/// again from its id. Checked by walking every decision of random games: the
/// two decks of the repository in all four pairings (both mirror matches), and
/// decks built from the whole card pool, one per legend, so that the prompts
/// of every registered card can come up.
///
/// The same walk checks what the search agents rely on: an intent's id does
/// not depend on the order the generator listed the options in, nor on
/// anything the deciding player cannot know.

#include <gtest/gtest.h>

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "engine/knowledge_tracker.h"
#include "openspiel/action_vocab.h"
#include "rules/deck_validator.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace riftbound::test::action_ids {

std::string deckFile(const char* name) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/" + name;
}

/// What a decision asks, for the census.
std::string decisionKind(const std::vector<Intent>& legal) {
    auto has = [&](IntentType t) {
        return std::any_of(legal.begin(), legal.end(),
                           [t](const Intent& i) { return i.type == t; });
    };
    if (has(IntentType::EndTurn)) return "main phase";
    if (has(IntentType::PassPriority)) return "chain priority";
    if (has(IntentType::PassFocus)) return "showdown focus";
    if (has(IntentType::MulliganDecision)) return "mulligan";
    if (has(IntentType::ChooseBattlefield)) return "battlefield setup";
    if (has(IntentType::AssignCombatDamage)) return "combat damage";
    if (has(IntentType::PlayFirstDecision)) return "play first";
    if (has(IntentType::MakeChoice)) {
        bool values = false, objects = false, named = false;
        for (const auto& i : legal) {
            if (i.chosen_value.has_value()) values = true;
            if (!i.chosen_objects.empty()) objects = true;
            if (i.chosen_card_def != kInvalidId) named = true;
        }
        if (named) return "prompt: name a card";
        if (objects && values) return "prompt: objects or a value";
        if (objects) return "prompt: objects";
        return "prompt: values";
    }
    return "other";
}

/// One deck per legend, built from the whole pool: the legend, a champion
/// unit with its tag, 39 main-deck cards of the legend's domains (seeded
/// pick, at most two copies), 12 runes of those domains and three
/// battlefields. Not tournament-legal on purpose (signature cards of other
/// champions are in): the engine plays what it is given.
std::vector<DeckSubmission> poolDecks(const CardDB& db) {
    std::vector<const CardDef*> defs;
    for (const auto& [id, def] : db.all()) defs.push_back(&def);
    std::sort(defs.begin(), defs.end(),
              [](const CardDef* a, const CardDef* b) { return a->id < b->id; });
    std::vector<const CardDef*> legends, battlefields, runes;
    for (const CardDef* d : defs) {
        if (d->super_type == SuperType::Token) continue;
        if (d->card_type == CardType::Legend) legends.push_back(d);
        if (d->card_type == CardType::Battlefield) battlefields.push_back(d);
        if (d->card_type == CardType::Rune) runes.push_back(d);
    }
    auto within = [](const CardDef& card, const CardDef& legend) {
        for (Domain d : card.domains)
            if (std::find(legend.domains.begin(), legend.domains.end(), d) ==
                legend.domains.end())
                return false;
        return true;
    };
    std::vector<DeckSubmission> decks;
    for (size_t li = 0; li < legends.size(); ++li) {
        const CardDef& legend = *legends[li];
        if (legend.domains.empty()) continue;
        std::mt19937_64 rng(1000 + legend.id.raw());
        DeckSubmission deck;
        deck.legend = legend.id;
        std::vector<const CardDef*> pool;
        for (const CardDef* d : defs) {
            if (d->super_type == SuperType::Token) continue;
            if (d->card_type != CardType::Unit && d->card_type != CardType::Spell &&
                d->card_type != CardType::Gear) continue;
            if (!within(*d, legend)) continue;
            if (d->card_type == CardType::Unit && d->super_type == SuperType::Champion &&
                deck.chosen_champion == kInvalidId &&
                (legend.tags.empty() || d->hasTag(legend.tags.front())))
                deck.chosen_champion = d->id;
            pool.push_back(d);
        }
        if (pool.size() < 20) continue;
        if (deck.chosen_champion == kInvalidId) {
            for (const CardDef* d : pool)
                if (d->card_type == CardType::Unit && d->super_type == SuperType::Champion) {
                    deck.chosen_champion = d->id;
                    break;
                }
        }
        if (deck.chosen_champion == kInvalidId) continue;
        std::shuffle(pool.begin(), pool.end(), rng);
        // Two copies of the first ten (so duplicates of one card meet), one of
        // the rest.
        for (size_t i = 0; deck.main_deck.size() < 39; ++i) {
            const CardDef* d = pool[i % pool.size()];
            deck.main_deck.push_back(d->id);
            if (i < 10 && deck.main_deck.size() < 39) deck.main_deck.push_back(d->id);
        }
        std::vector<const CardDef*> my_runes;
        for (const CardDef* r : runes)
            if (within(*r, legend) && !r->domains.empty()) my_runes.push_back(r);
        if (my_runes.empty()) continue;
        for (int i = 0; i < 12; ++i)
            deck.rune_deck.push_back(my_runes[i % my_runes.size()]->id);
        for (int i = 0; i < 3; ++i)
            deck.battlefields.push_back(
                battlefields[(li * 3 + i) % battlefields.size()]->id);
        decks.push_back(std::move(deck));
    }
    return decks;
}

struct Census {
    long games = 0, decisions = 0, intents = 0;
    long colliding_decisions = 0, lost_intents = 0;
    std::map<std::string, std::pair<long, long>> by_kind;   // decisions, colliding
    std::map<std::string, std::pair<long, long>> by_type;   // intents, lost
    long not_found_again = 0, out_of_range = 0;
    long order_dependent = 0, hidden_dependent = 0;
    int most_twins = 0;      // most intents on one id (1 = no collision)
    int highest_twin = 0;    // highest twin number any intent got
    long twin_intents = 0;   // intents that are not the first of their slot
    long without_id = 0;
    // Intents whose keyed object has no card id (a token without a card
    // class), per verb: they used to land on the first slot of the verb's
    // range, which is another card's — or, in a prompt, "no object".
    std::map<std::string, long> no_card_id;
    std::string first;
    std::vector<std::string> engine_failures;
};

class ActionIdsWalk : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
    }
    static void TearDownTestSuite() { delete db_; delete registry_; }

    /// Everything checked at one decision.
    static void check(const GameState& state, const StepResult& step, Census& c,
                      const std::string& where, std::mt19937_64& rng) {
        const auto& legal = step.legal;
        const std::vector<int> ids = openspiel::actionIds(legal, state);
        ASSERT_EQ(ids.size(), legal.size());
        ++c.decisions;
        auto& kind = c.by_kind[decisionKind(legal)];
        ++kind.first;

        // Distinct intents (the engine may list an option twice).
        std::vector<size_t> distinct;
        for (size_t i = 0; i < legal.size(); ++i) {
            bool seen = false;
            for (size_t d : distinct) {
                if (legal[d] == legal[i]) {
                    seen = true;
                    EXPECT_EQ(ids[d], ids[i]) << where << ": one intent, two ids";
                }
            }
            if (!seen) distinct.push_back(i);
        }
        std::map<int, int> uses;
        int lost = 0;
        for (size_t i : distinct) {
            auto& type = c.by_type[toString(legal[i].type)];
            ++type.first;
            ++c.intents;
            if (ids[i] == openspiel::kNoActionId) ++c.without_id;
            else if (ids[i] < 0 || ids[i] >= openspiel::kNumActions) ++c.out_of_range;
            else {
                c.highest_twin = std::max(c.highest_twin, openspiel::actionIdTwin(ids[i]));
                if (openspiel::actionIdTwin(ids[i]) > 0) ++c.twin_intents;
                // The slot part of the id is the intent's own description.
                if (openspiel::actionIdSlot(ids[i]) != openspiel::actionSlot(legal[i], state))
                    ++c.out_of_range;
            }
            if (openspiel::actionIdOf(legal[i], legal, state) != ids[i]) ++c.not_found_again;
            {
                const Intent& it = legal[i];
                GameObjectId keyed = kInvalidId;
                if (it.type == IntentType::MakeChoice && !it.chosen_objects.empty())
                    keyed = it.chosen_objects.front();
                else if (it.type == IntentType::StandardMove && !it.units_to_move.empty())
                    keyed = it.units_to_move.front();
                else if (it.ability_source != kInvalidId) keyed = it.ability_source;
                else if (it.card != kInvalidId) keyed = it.card;
                if (keyed != kInvalidId && state.objectExists(keyed) &&
                    state.getObject(keyed).card_def_id == kInvalidId)
                    ++c.no_card_id[toString(it.type)];
            }
            if (uses[ids[i]]++ > 0) {
                ++lost;
                ++type.second;
                if (c.first.empty())
                    c.first = where + ": " + legal[i].describe() + " shares id " +
                              std::to_string(ids[i]) + " with another legal intent";
            }
            // Selectable: the id leads back to this intent.
            const Intent* found = openspiel::decodeAction(ids[i], legal, state);
            if (found == nullptr || !(*found == legal[i])) ++c.not_found_again;
        }
        for (const auto& [id, n] : uses) c.most_twins = std::max(c.most_twins, n);
        c.lost_intents += lost;
        if (lost > 0) {
            ++c.colliding_decisions;
            ++kind.second;
        }

        // The id of an intent does not depend on where the generator put it.
        std::vector<size_t> order(legal.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::shuffle(order.begin(), order.end(), rng);
        std::vector<Intent> shuffled;
        for (size_t i : order) shuffled.push_back(legal[i]);
        const std::vector<int> shuffled_ids = openspiel::actionIds(shuffled, state);
        for (size_t k = 0; k < order.size(); ++k)
            if (shuffled_ids[k] != ids[order[k]]) { ++c.order_dependent; break; }

        // ... nor on anything the deciding player cannot know: re-deal the
        // opponent's hand, deck and facedown cards the player has not been
        // shown (the tracker's record, and the cards this very prompt puts in
        // front of the player), and the order of both decks.
        const PlayerId me = step.perspective;
        const PlayerId opp = opponent(me);
        GameState redealt = state;
        const knowledge::GameKnowledgeView knows(state, me);
        std::set<GameObjectId> offered;
        for (const auto& item : state.chain.items) {   // announced: public
            offered.insert(item.source);
            offered.insert(item.targets.begin(), item.targets.end());
        }
        for (const auto& i : legal) {
            offered.insert(i.chosen_objects.begin(), i.chosen_objects.end());
            offered.insert(i.targets.begin(), i.targets.end());
        }
        auto& ops = redealt.player(opp);
        std::vector<GameObjectId> hidden;
        auto consider = [&](GameObjectId id) {
            if (!knows.about(id).identity_known && !offered.count(id)) hidden.push_back(id);
        };
        for (auto id : ops.hand) consider(id);
        for (auto id : ops.main_deck) consider(id);
        for (const auto& bf : redealt.battlefields)
            for (auto id : bf.facedown)
                if (redealt.getObject(id).controller == opp) consider(id);
        std::vector<CardDefId> defs;
        for (auto id : hidden) defs.push_back(redealt.getObject(id).card_def_id);
        std::shuffle(defs.begin(), defs.end(), rng);
        for (size_t i = 0; i < hidden.size(); ++i)
            redealt.getObject(hidden[i]).card_def_id = defs[i];
        std::shuffle(ops.main_deck.begin(), ops.main_deck.end(), rng);
        std::shuffle(redealt.player(me).main_deck.begin(),
                     redealt.player(me).main_deck.end(), rng);
        if (openspiel::actionIds(legal, redealt) != ids) ++c.hidden_dependent;
    }

    static void playGame(const DeckSubmission& d1, const DeckSubmission& d2,
                         uint64_t seed, const std::string& label, Census& c) {
        EventBus bus;
        GameEngine engine(*db_, bus, *registry_);
        std::mt19937_64 rng(seed * 7919);
        try {
            engine.beginGame(d1, d2, seed);
            for (int n = 0; n < 6000 && !engine.isStepDone(); ++n) {
                const auto& step = engine.currentStep();
                check(engine.state(), step, c,
                      label + " seed " + std::to_string(seed) + " decision " +
                          std::to_string(n), rng);
                if (::testing::Test::HasFatalFailure()) return;
                std::uniform_int_distribution<size_t> pick(0, step.legal.size() - 1);
                engine.applyChoice(static_cast<int>(pick(rng)));
            }
            ++c.games;
        } catch (const std::exception& e) {
            c.engine_failures.push_back(label + " seed " + std::to_string(seed) + ": " +
                                        e.what());
        }
    }

    static void report(const char* title, const Census& c) {
        std::cout << "[ " << title << ": " << c.games << " games, " << c.decisions
                  << " decisions, " << c.intents << " distinct legal intents; "
                  << c.colliding_decisions << " decisions with a collision, "
                  << c.lost_intents << " intents sharing an id with an earlier one; "
                  << "most intents on one id: " << c.most_twins << "; " << c.twin_intents
                  << " intents are told apart by a twin number, the highest is "
                  << c.highest_twin << " ]\n";
        for (const auto& [kind, n] : c.by_kind)
            if (n.second > 0)
                std::cout << "[   decision kind " << kind << ": " << n.second << " of "
                          << n.first << " collide ]\n";
        for (const auto& [type, n] : c.by_type)
            if (n.second > 0)
                std::cout << "[   intent type " << type << ": " << n.second << " of "
                          << n.first << " share an id ]\n";
        for (const auto& [type, n] : c.no_card_id)
            std::cout << "[   " << type << " intents keyed by an object with no card id: "
                      << n << " ]\n";
        for (const auto& f : c.engine_failures) std::cout << "[   ENGINE FAILURE " << f << " ]\n";
    }

    static void expectClean(const Census& c) {
        EXPECT_EQ(c.lost_intents, 0) << "first: " << c.first;
        EXPECT_EQ(c.colliding_decisions, 0);
        EXPECT_EQ(c.not_found_again, 0) << "intents that their own id does not lead back to";
        EXPECT_EQ(c.out_of_range, 0);
        EXPECT_EQ(c.without_id, 0) << "intents past the twin planes";
        EXPECT_LT(c.highest_twin, openspiel::kTwinPlanes / 8)
            << "the walk comes close to the bound: raise kTwinPlanes";
        EXPECT_EQ(c.order_dependent, 0) << "decisions whose ids depend on the generator's order";
        EXPECT_EQ(c.hidden_dependent, 0) << "decisions whose ids depend on hidden cards";
        EXPECT_TRUE(c.engine_failures.empty());
    }

    static CardRegistry* registry_;
    static CardDB* db_;
};

CardRegistry* ActionIdsWalk::registry_ = nullptr;
CardDB* ActionIdsWalk::db_ = nullptr;

TEST_F(ActionIdsWalk, NoTwoLegalIntentsShareAnId_RepositoryDecks) {
    const DeckSubmission fiora = DeckValidator::loadFromDeckList(deckFile("fiora.txt"), *db_);
    const DeckSubmission yi = DeckValidator::loadFromDeckList(deckFile("yi_rebuild.txt"), *db_);
    const DeckSubmission* decks[2] = {&fiora, &yi};
    const char* names[2] = {"fiora", "yi_rebuild"};
    Census c;
    for (int a = 0; a < 2; ++a)
        for (int b = 0; b < 2; ++b)
            for (uint64_t g = 1; g <= 12; ++g)
                playGame(*decks[a], *decks[b], 100 * (2 * a + b) + g,
                         std::string(names[a]) + " vs " + names[b], c);
    report("repository decks, all four pairings", c);
    EXPECT_EQ(c.games, 48);
    EXPECT_GT(c.decisions, 10000);
    expectClean(c);
}

TEST_F(ActionIdsWalk, NoTwoLegalIntentsShareAnId_DecksFromTheWholePool) {
    const std::vector<DeckSubmission> decks = poolDecks(*db_);
    ASSERT_GT(decks.size(), 20u);
    std::set<CardDefId> cards;
    for (const auto& d : decks) cards.insert(d.main_deck.begin(), d.main_deck.end());
    Census c;
    for (size_t i = 0; i < decks.size(); ++i) {
        const std::string label = "pool deck " + std::to_string(i) + " (legend " +
                                  db_->get(decks[i].legend).name + ")";
        // The mirror match, and the next legend's deck.
        playGame(decks[i], decks[i], 5000 + i, label + " mirror", c);
        playGame(decks[i], decks[(i + 1) % decks.size()], 7000 + i, label + " vs the next", c);
    }
    report("pool decks", c);
    std::cout << "[   " << decks.size() << " decks, " << cards.size()
              << " different main-deck cards ]\n";
    EXPECT_GT(cards.size(), 600u) << "the pool decks cover the registry";
    EXPECT_GT(c.decisions, 20000);
    // Reach of "an object with no card id" (1.12): real legal lists do offer
    // them, in prompts and as units to move.
    EXPECT_GT(c.no_card_id["MakeChoice"], 0);
    EXPECT_GT(c.no_card_id["StandardMove"], 0);
    expectClean(c);
}

} // namespace riftbound::test::action_ids
