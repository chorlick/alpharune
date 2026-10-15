/// Worlds for determinized search (agents/determinizer.h, batch 5 item T3).
///
/// Two properties, checked at every decision of real games (repository decks,
/// decks with [Hidden] cards, and random constructed decks):
///   - a world never contradicts what its player knows (WorldSampler::check:
///     public zones, own hand, known slots, own deck composition, legality of
///     what was drawn, the world's knowledge tracker);
///   - a world is a function of what its player knows and of the RNG only:
///     two positions that differ only in cards the player cannot see give the
///     same world for the same seed.

#include <gtest/gtest.h>

#include "agents/determinizer.h"
#include "agents/random_player_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/knowledge_view.h"
#include "engine/game_engine.h"
#include "engine/knowledge_tracker.h"
#include "rules/deck_validator.h"
#include "rules/random_deck.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace riftbound::test::determinizer {

std::string deckFile(const char* name) {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/" + name;
}

/// Everything about a world that a search could read, as text.
std::string fingerprint(const GameState& s) {
    std::vector<GameObjectId> ids;
    for (const auto& [id, obj] : s.objects) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    std::ostringstream out;
    for (auto id : ids) {
        const GameObject& o = s.getObject(id);
        out << o.id.raw() << ':' << o.card_def_id.raw() << ':' << o.name.get() << ':'
            << static_cast<int>(o.zone.get()) << ':' << o.location.has_value() << ':'
            << static_cast<int>(o.controller) << ':' << o.current_might << ':'
            << o.damage_marked << ':' << o.is_hidden << ':' << o.card_counters.empty() << ':'
            << o.last_known.valid << ';';
    }
    for (const auto& ps : s.players) {
        out << '|' << ps.battlefield_pool.size() << ',' << ps.observed_cards.size();
    }
    for (int p = 0; p < 2; ++p) {
        for (const auto& [id, f] : s.knowledge.facts[p]) {
            out << '#' << p << ':' << id.raw() << ':' << static_cast<int>(f.zone) << ':'
                << f.zone_confirmed << ':' << f.held << ':' << static_cast<int>(f.end) << ':'
                << f.anchor << ':' << f.group;
        }
    }
    return out.str();
}

class DeterminizerWorlds : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
        sampler_ = new WorldSampler(*db_);
    }
    static void TearDownTestSuite() {
        delete sampler_;
        delete db_;
        delete registry_;
    }

    struct Tally {
        long decisions = 0, worlds = 0, leak_checks = 0, swaps = 0;
        long opp_hand_redrawn = 0, drawn = 0, placed = 0, kept = 0;
        std::vector<std::string> problems;
    };

    /// A position the player `observer` cannot tell from `truth`: hidden
    /// cards it does not know swapped or replaced. Returns false if there was
    /// nothing to change.
    static bool indistinguishable(const GameState& truth, GameState& variant,
                                  PlayerId observer, std::mt19937_64& rng, long* swaps) {
        const knowledge::GameKnowledgeView view(truth, observer);
        auto unknown = [&](GameObjectId id) { return !view.about(id).identity_known; };
        auto ownUnknownSlot = [&](GameObjectId id) {
            return view.about(id).deck_position == DeckPosition::Unknown;
        };
        auto swapCards = [&](GameObjectId a, GameObjectId b) {
            const CardDef& da = db_->get(variant.getObject(a).card_def_id);
            const CardDef& dbb = db_->get(variant.getObject(b).card_def_id);
            becomeCard(variant.getObject(a), dbb);
            becomeCard(variant.getObject(b), da);
            ++*swaps;
        };
        bool changed = false;
        const PlayerId opp = opponent(observer);
        const auto& op = truth.player(opp);
        // An opponent's hand card for one of their deck cards.
        std::vector<GameObjectId> hand, deck;
        for (auto id : op.hand) if (unknown(id)) hand.push_back(id);
        for (auto id : op.main_deck) if (unknown(id)) deck.push_back(id);
        [&] {
            for (auto h : hand) {
                for (auto d : deck) {
                    if (truth.getObject(h).card_def_id != truth.getObject(d).card_def_id) {
                        swapCards(h, d);
                        changed = true;
                        return;
                    }
                }
            }
        }();
        // One of their deck cards replaced by another card altogether.
        if (!deck.empty()) {
            std::vector<CardDefId> others;
            for (const auto& [id, def] : db_->all())
                if (def.card_type == CardType::Spell && !def.isTokenCard()) others.push_back(id);
            std::sort(others.begin(), others.end());
            const GameObjectId d = deck[rng() % deck.size()];
            becomeCard(variant.getObject(d), db_->get(others[rng() % others.size()]));
            changed = true;
            ++*swaps;
        }
        // A rune of their rune deck the observer does not know replaced (a
        // recycled rune is known at the bottom: batch 5d T4).
        std::vector<GameObjectId> unknown_runes;
        for (auto id : op.rune_deck) if (unknown(id)) unknown_runes.push_back(id);
        if (!unknown_runes.empty()) {
            std::vector<CardDefId> runes;
            for (const auto& [id, def] : db_->all())
                if (def.card_type == CardType::Rune && !def.isTokenCard()) runes.push_back(id);
            std::sort(runes.begin(), runes.end());
            becomeCard(variant.getObject(unknown_runes[rng() % unknown_runes.size()]),
                       db_->get(runes[rng() % runes.size()]));
            changed = true;
            ++*swaps;
        }
        // Two of the observer's own deck cards, both at slots it does not know.
        const auto& own = truth.player(observer).main_deck;
        for (size_t i = 0; i + 1 < own.size() && !own.empty(); ++i) {
            const GameObjectId a = own[i], b = own[own.size() - 1 - i];
            if (a == b) break;
            if (ownUnknownSlot(a) && ownUnknownSlot(b) &&
                truth.getObject(a).card_def_id != truth.getObject(b).card_def_id) {
                swapCards(a, b);
                changed = true;
                break;
            }
        }
        // Their facedown cards the observer has not seen, for another [Hidden] card.
        for (const auto& bf : truth.battlefields) {
            for (auto id : bf.facedown) {
                if (truth.getObject(id).owner != opp || !unknown(id)) continue;
                for (auto h : hand) {
                    if (db_->get(truth.getObject(h).card_def_id).keywords.has(Keyword::Hidden) &&
                        truth.getObject(h).card_def_id != truth.getObject(id).card_def_id) {
                        swapCards(h, id);
                        changed = true;
                        break;
                    }
                }
            }
        }
        return changed;
    }

    static void checkDecision(const GameState& state, const StepResult& step, uint64_t seed,
                              const std::string& where, Tally& t) {
        ++t.decisions;
        for (PlayerId observer : {PlayerId::Player1, PlayerId::Player2}) {
            GameState world = state;
            std::mt19937_64 rng(seed);
            const WorldDraw draw = sampler_->sample(world, observer, rng);
            ++t.worlds;
            t.drawn += draw.drawn;
            t.placed += draw.placed;
            t.kept += draw.kept;
            for (const auto& p : sampler_->check(state, world, observer)) {
                if (t.problems.size() < 20)
                    t.problems.push_back(where + " (" + toString(observer) + "): " + p);
            }
            const auto& opp_hand = state.player(opponent(observer)).hand;
            for (auto id : opp_hand)
                if (world.getObject(id).card_def_id != state.getObject(id).card_def_id) {
                    ++t.opp_hand_redrawn;
                    break;
                }
        }
        // The decider's world does not depend on what the decider cannot see.
        const PlayerId decider = step.perspective;
        if (decider == PlayerId::None) return;
        GameState variant = state;
        std::mt19937_64 vrng(seed ^ 0x9E3779B97F4A7C15ull);
        if (!indistinguishable(state, variant, decider, vrng, &t.swaps)) return;
        // The fingerprint sees the change (else the comparison below proves nothing).
        if (fingerprint(state) == fingerprint(variant) && t.problems.size() < 20)
            t.problems.push_back(where + ": the variant is not a different position");
        GameState a = state, b = variant;
        std::mt19937_64 ra(seed), rb(seed);
        sampler_->sample(a, decider, ra);
        sampler_->sample(b, decider, rb);
        ++t.leak_checks;
        if (fingerprint(a) != fingerprint(b) && t.problems.size() < 20)
            t.problems.push_back(where + ": the world depends on cards " +
                                 toString(decider) + " cannot see");
    }

    static void playGame(const DeckSubmission& d1, const DeckSubmission& d2, uint64_t seed,
                         const std::string& label, Tally& t) {
        EventBus bus;
        GameEngine engine(*db_, bus, *registry_);
        RandomPlayerAgent picker(seed * 7919 + 1);
        engine.beginGame(d1, d2, seed);
        for (int n = 0; n < 4000 && !engine.isStepDone(); ++n) {
            const StepResult step = engine.currentStep();
            checkDecision(engine.state(), step, seed * 1000 + n,
                          label + " seed " + std::to_string(seed) + " decision " +
                              std::to_string(n),
                          t);
            const Intent chosen = picker.selectAction(engine.state(), step.legal);
            const auto it = std::find(step.legal.begin(), step.legal.end(), chosen);
            engine.applyChoice(static_cast<int>(it - step.legal.begin()));
        }
    }

    static CardRegistry* registry_;
    static CardDB* db_;
    static WorldSampler* sampler_;
};

CardRegistry* DeterminizerWorlds::registry_ = nullptr;
CardDB* DeterminizerWorlds::db_ = nullptr;
WorldSampler* DeterminizerWorlds::sampler_ = nullptr;

TEST_F(DeterminizerWorlds, EveryWorldAgreesWithWhatItsPlayerKnowsAndIgnoresWhatItCannotSee) {
    Tally t;
    const std::vector<std::pair<const char*, const char*>> pairs = {
        {"yi_rebuild.txt", "fiora.txt"}, {"leblanc.txt", "azir.txt"},
        {"teemo.txt", "vi.txt"}, {"renata.txt", "reksai.txt"}};
    uint64_t seed = 20261014;
    for (const auto& [a, b] : pairs) {
        const DeckSubmission d1 = DeckValidator::loadFromDeckList(deckFile(a), *db_);
        const DeckSubmission d2 = DeckValidator::loadFromDeckList(deckFile(b), *db_);
        playGame(d1, d2, ++seed, std::string(a) + " v " + b, t);
    }
    RandomDeckBuilder builder(*db_);
    for (uint64_t k = 1; k <= 4; ++k) {
        playGame(builder.build(k), builder.build(k + 100), ++seed,
                 "random decks " + std::to_string(k), t);
    }
    for (const auto& p : t.problems) ADD_FAILURE() << p;
    EXPECT_GT(t.decisions, 600);
    EXPECT_GT(t.leak_checks, 500);
    EXPECT_GT(t.drawn, 0);
    EXPECT_GT(t.opp_hand_redrawn, t.worlds / 4) << "the opponent's hand is rarely redrawn";
    std::cout << "[ determinizer: " << t.decisions << " decisions, " << t.worlds
              << " worlds checked, " << t.leak_checks << " leak checks (" << t.swaps
              << " hidden cards changed); drawn " << t.drawn << ", placed " << t.placed
              << ", kept " << t.kept << " ]\n";
    RecordProperty("decisions", static_cast<int>(t.decisions));
    RecordProperty("worlds", static_cast<int>(t.worlds));
    RecordProperty("leak_checks", static_cast<int>(t.leak_checks));
    RecordProperty("placed", static_cast<int>(t.placed));
}

/// The deck model: an unseen card comes from the legend's legal pool, never
/// from the true decklist, and within the copy limits.
TEST_F(DeterminizerWorlds, TheOpponentsUnseenCardsComeFromTheirLegendsPoolNotTheirDecklist) {
    const DeckSubmission yi = DeckValidator::loadFromDeckList(deckFile("yi_rebuild.txt"), *db_);
    const DeckSubmission fiora = DeckValidator::loadFromDeckList(deckFile("fiora.txt"), *db_);
    EventBus bus;
    GameEngine engine(*db_, bus, *registry_);
    engine.beginGame(yi, fiora, 77);
    std::set<CardDefId> in_list(fiora.main_deck.begin(), fiora.main_deck.end());
    long outside = 0, total = 0;
    for (uint64_t k = 0; k < 50; ++k) {
        GameState world = engine.state();
        std::mt19937_64 rng(k);
        sampler_->sample(world, PlayerId::Player1, rng);
        EXPECT_TRUE(sampler_->check(engine.state(), world, PlayerId::Player1).empty());
        for (auto id : world.player(PlayerId::Player2).main_deck) {
            ++total;
            if (!in_list.count(world.getObject(id).card_def_id)) ++outside;
        }
        // The observer's own hand is never touched.
        for (auto id : world.player(PlayerId::Player1).hand)
            EXPECT_EQ(world.getObject(id).card_def_id, engine.state().getObject(id).card_def_id);
    }
    EXPECT_GT(outside, total / 2) << "the drawn deck looks like the true decklist";
}

}  // namespace riftbound::test::determinizer
