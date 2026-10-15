/// Hidden order must not reach a player (batch 5b items T2 and L2; trainer
/// plan section 0, item 11: "leaks poison the model").
///
/// What a player is shown of a decision, as this file compares it:
///   - for the decider, its options in action-id order: each option's action
///     id (openspiel::actionIds), what the decider sees of it
///     (openspiel::visibleDescription: objects by whose they are, where they
///     are, their card only if the decider knows it, never an id) and its
///     display label;
///   - for each player, its info-set id (openspiel::infoSetIdFor, which is
///     RiftboundState::InfoSetId and ObservationString) and its observation
///     (ml::extractStateFeatures, RiftboundState::ObservationTensor);
///   - for each player, the log lines it may read (LogEvent::visibleTo).
///
/// Three ways of making two positions a player cannot tell apart, each at
/// every decision of real games (deck lists, [Hidden] decks among them, and
/// random constructed decks; `random` and `random-player` choices, made by
/// action id so that the same choice is made in both positions):
///   1. Variants of one position: the opponent's hidden cards swapped or
///      replaced, the decider's own deck reordered, its own hand reordered.
///   2. The same game under other object ids (GameEngine::setIdLabelSalt).
///   3. Games against an opponent who never acts, whose deck is in another
///      order or holds other cards: everything up to the first card of theirs
///      that the player gets to see.
/// Each test also checks that the positions really differ.

#include <gtest/gtest.h>

#include "agents/determinizer.h"
#include "agents/random_player_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "engine/knowledge_tracker.h"
#include "ml/feature_extractor.h"
#include "openspiel/action_vocab.h"
#include "rules/deck_validator.h"
#include "rules/random_deck.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace riftbound::test::hidden_order_leak {

std::string deckDir() {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/";
}

/// How many games: the suite's default, or RIFTBOUND_LEAK_GAMES for a long run.
int gamesOr(int dflt) {
    if (const char* e = std::getenv("RIFTBOUND_LEAK_GAMES")) return std::max(1, std::atoi(e));
    return dflt;
}

/// The first game: 0, or RIFTBOUND_LEAK_FROM (to replay one game of a long run).
int firstGame() {
    if (const char* e = std::getenv("RIFTBOUND_LEAK_FROM")) return std::max(0, std::atoi(e));
    return 0;
}

/// What `who` is shown at a decision.
struct Shown {
    std::vector<int> ids;                          // decider: action ids, ascending
    std::vector<std::vector<int64_t>> described;   // decider: per id
    std::vector<std::string> labels;               // decider: per id
    uint64_t info_set = 0;
    std::vector<float> observation;

    bool operator==(const Shown&) const = default;
};

/// The decider's options in action-id order (indices into `legal`).
std::vector<size_t> byActionId(const GameState& s, const std::vector<Intent>& legal) {
    const std::vector<int> ids = openspiel::actionIds(legal, s);
    std::vector<size_t> order(legal.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](size_t a, size_t b) { return ids[a] < ids[b]; });
    return order;
}

Shown shownTo(const GameState& s, const std::vector<Intent>& legal, PlayerId decider,
              PlayerId who, const CardDB& db) {
    Shown out;
    if (who == decider) {
        const std::vector<int> ids = openspiel::actionIds(legal, s);
        for (size_t i : byActionId(s, legal)) {
            out.ids.push_back(ids[i]);
            out.described.push_back(openspiel::visibleDescription(legal[i], s));
            out.labels.push_back(legal[i].choice_label);
        }
    }
    out.info_set = openspiel::infoSetIdFor(s, legal, decider, who, db);
    ml::extractStateFeatures(s, who, db, out.observation);
    return out;
}

/// The first thing that differs between two showings, for a failure message.
std::string firstDifference(const Shown& a, const Shown& b) {
    if (a.ids != b.ids) {
        std::ostringstream o;
        o << "action ids differ (" << a.ids.size() << " vs " << b.ids.size() << " options)";
        for (size_t i = 0; i < std::min(a.ids.size(), b.ids.size()); ++i)
            if (a.ids[i] != b.ids[i]) {
                o << ": #" << i << " " << a.ids[i] << " vs " << b.ids[i] << " (slot "
                  << openspiel::actionIdSlot(a.ids[i]) << " vs "
                  << openspiel::actionIdSlot(b.ids[i]) << ")";
                break;
            }
        return o.str();
    }
    if (a.described != b.described) {
        for (size_t i = 0; i < a.described.size(); ++i)
            if (a.described[i] != b.described[i])
            {
                std::string da, db;
                for (auto v : a.described[i]) da += std::to_string(v) + ",";
                for (auto v : b.described[i]) db += std::to_string(v) + ",";
                return "the visible description of option #" + std::to_string(i) +
                       " (action id " + std::to_string(a.ids[i]) + ") differs: '" +
                       a.labels[i] + "' [" + da + "] vs '" + b.labels[i] + "' [" + db + "]";
            }
    }
    if (a.labels != b.labels) {
        for (size_t i = 0; i < a.labels.size(); ++i)
            if (a.labels[i] != b.labels[i])
                return "label differs: '" + a.labels[i] + "' vs '" + b.labels[i] + "'";
    }
    if (a.observation != b.observation) {
        for (size_t i = 0; i < std::min(a.observation.size(), b.observation.size()); ++i)
            if (a.observation[i] != b.observation[i])
                return "observation differs at feature " + std::to_string(i) + ": " +
                       std::to_string(a.observation[i]) + " vs " +
                       std::to_string(b.observation[i]);
        return "observation length differs";
    }
    if (a.info_set != b.info_set) return "info-set id differs";
    return "";
}

/// A choice made by action id: the same in two positions that show the same
/// ids. `pass_p` < 0: uniform over every option (`random`); otherwise
/// `random-player` (a pass with probability pass_p while anything else is legal).
size_t chooseByActionId(const GameState& s, const std::vector<Intent>& legal,
                        std::mt19937_64& rng, double pass_p) {
    const std::vector<size_t> order = byActionId(s, legal);
    if (pass_p < 0) return order[rng() % order.size()];
    std::vector<size_t> act, pass;
    for (size_t i : order) (isPassOption(legal, i) ? pass : act).push_back(i);
    const double u = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    if (act.empty() || (!pass.empty() && u < pass_p)) {
        const auto& from = pass.empty() ? act : pass;
        return from[rng() % from.size()];
    }
    return act[rng() % act.size()];
}

/// A log line as compared: object ids printed in it are names, not
/// information ("id=N" made "id=?"), and a prompt line's list of options
/// ("TGT_PROMPT: ... [a/b/c]") is in the engine's order of its options, which
/// is the order of StepResult::legal and may follow object ids (the order a
/// player is shown is the action-id order): the names are compared as a set.
std::string normalised(const std::string& line) {
    static const std::regex id_number("id=[0-9]+");
    std::string out = std::regex_replace(line, id_number, "id=?");
    if (out.find("PROMPT") == std::string::npos) return out;
    // The list is the last bracket before " (agent ...": a prompt's own
    // text may hold brackets ("+2 [M]", "[Assault 2]").
    const auto close = out.rfind("] (");
    const auto open = close == std::string::npos ? close : out.rfind('[', close);
    if (open == std::string::npos) return out;
    std::vector<std::string> names;
    std::stringstream items(out.substr(open + 1, close - open - 1));
    for (std::string n; std::getline(items, n, '/');) names.push_back(n);
    std::sort(names.begin(), names.end());
    std::string joined;
    for (const auto& n : names) joined += (joined.empty() ? "" : "/") + n;
    return out.substr(0, open + 1) + joined + out.substr(close);
}

/// Log lines `who` may read, collected between decisions.
struct LogTap {
    std::vector<std::string> lines[2];
    boost::signals2::scoped_connection conn;
    void attach(EventBus& bus) {
        conn = bus.on_log.connect([this](const LogEvent& e) {
            for (int p = 0; p < 2; ++p)
                if (e.visibleTo(p == 0 ? PlayerId::Player1 : PlayerId::Player2))
                    lines[p].push_back(normalised(e.message));
        });
    }
    void clear() { lines[0].clear(); lines[1].clear(); }
};

class HiddenOrderLeak : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
        builder_ = new RandomDeckBuilder(*db_);
        for (const auto& entry : std::filesystem::directory_iterator(deckDir()))
            if (entry.path().extension() == ".txt") lists_.push_back(entry.path().string());
        std::sort(lists_.begin(), lists_.end());
    }
    static void TearDownTestSuite() {
        delete builder_;
        delete db_;
        delete registry_;
    }

    static DeckSubmission list(size_t k) {
        return DeckValidator::loadFromDeckList(lists_[k % lists_.size()], *db_);
    }
    /// Deck pair of game `g`: two deck lists for even g, two random decks for odd g.
    static std::pair<DeckSubmission, DeckSubmission> decks(int g) {
        if (g % 2 == 0) {
            const size_t a = static_cast<size_t>(g / 2) * 7 % lists_.size();
            const size_t b = (a + 1 + static_cast<size_t>(g / 2) % (lists_.size() - 1)) %
                             lists_.size();
            return {list(a), list(b)};
        }
        return {builder_->build(1000 + g), builder_->build(5000 + g)};
    }
    static std::string deckName(int g) {
        if (g % 2) return "random decks " + std::to_string(1000 + g) + "/" + std::to_string(5000 + g);
        const size_t a = static_cast<size_t>(g / 2) * 7 % lists_.size();
        const size_t b = (a + 1 + static_cast<size_t>(g / 2) % (lists_.size() - 1)) % lists_.size();
        return std::filesystem::path(lists_[a]).stem().string() + " v " +
               std::filesystem::path(lists_[b]).stem().string();
    }

    static CardRegistry* registry_;
    static CardDB* db_;
    static RandomDeckBuilder* builder_;
    static std::vector<std::string> lists_;
};

CardRegistry* HiddenOrderLeak::registry_ = nullptr;
CardDB* HiddenOrderLeak::db_ = nullptr;
RandomDeckBuilder* HiddenOrderLeak::builder_ = nullptr;
std::vector<std::string> HiddenOrderLeak::lists_;

// ─── 1. Variants of one position ────────────────────────────────────────────

struct VariantTally {
    long decisions = 0, variants = 0, changed_cards = 0, hand_orders = 0, ids_named = 0;
    long prompts = 0, chain_item_prompts = 0, add_payment_prompts = 0;
    std::vector<std::string> problems;
    void problem(const std::string& s) { if (problems.size() < 20) problems.push_back(s); }
};

/// A copy of `truth` that `observer` cannot tell from it: the opponent's
/// unknown hand card swapped with an unknown deck card, one of their unknown
/// deck cards and one rune replaced, an unknown facedown card swapped with a
/// [Hidden] hand card, two of the observer's own deck cards (at slots it does
/// not know) swapped, and the observer's hand in another order. Returns how
/// many cards changed identity.
long makeVariant(const GameState& truth, GameState& variant, PlayerId observer,
                 const CardDB& db, std::mt19937_64& rng, bool* hand_reordered) {
    const knowledge::GameKnowledgeView view(truth, observer);
    auto unknown = [&](GameObjectId id) { return !view.about(id).identity_known; };
    long changed = 0;
    auto swapCards = [&](GameObjectId a, GameObjectId b) {
        const CardDef& da = db.get(variant.getObject(a).card_def_id);
        const CardDef& dbb = db.get(variant.getObject(b).card_def_id);
        becomeCard(variant.getObject(a), dbb);
        becomeCard(variant.getObject(b), da);
        changed += 2;
    };
    const PlayerId opp = opponent(observer);
    const auto& op = truth.player(opp);
    std::vector<GameObjectId> hand, deck;
    for (auto id : op.hand) if (unknown(id)) hand.push_back(id);
    for (auto id : op.main_deck) if (unknown(id)) deck.push_back(id);
    // Their hand: swap two unknown hand cards of different cards (their hand
    // order), and one with a deck card.
    for (size_t i = 0; i + 1 < hand.size(); ++i)
        if (truth.getObject(hand[i]).card_def_id != truth.getObject(hand.back()).card_def_id) {
            swapCards(hand[i], hand.back());
            break;
        }
    [&] {
        for (auto h : hand)
            for (auto d : deck)
                if (variant.getObject(h).card_def_id != variant.getObject(d).card_def_id) {
                    swapCards(h, d);
                    return;
                }
    }();
    if (!deck.empty()) {
        std::vector<CardDefId> others;
        for (const auto& [id, def] : db.all())
            if ((def.card_type == CardType::Spell || def.card_type == CardType::Unit) &&
                !def.isTokenCard())
                others.push_back(id);
        std::sort(others.begin(), others.end());
        becomeCard(variant.getObject(deck[rng() % deck.size()]),
                   db.get(others[rng() % others.size()]));
        ++changed;
    }
    if (!op.rune_deck.empty()) {
        std::vector<CardDefId> runes;
        for (const auto& [id, def] : db.all())
            if (def.card_type == CardType::Rune && !def.isTokenCard()) runes.push_back(id);
        std::sort(runes.begin(), runes.end());
        becomeCard(variant.getObject(op.rune_deck[rng() % op.rune_deck.size()]),
                   db.get(runes[rng() % runes.size()]));
        ++changed;
    }
    for (const auto& bf : truth.battlefields)
        for (auto id : bf.facedown) {
            if (truth.getObject(id).owner != opp || !unknown(id)) continue;
            for (auto h : hand)
                if (db.get(variant.getObject(h).card_def_id).keywords.has(Keyword::Hidden) &&
                    variant.getObject(h).card_def_id != variant.getObject(id).card_def_id) {
                    swapCards(h, id);
                    break;
                }
        }
    // Own deck order: two own deck cards at slots the observer does not know.
    const auto& own = truth.player(observer).main_deck;
    for (size_t i = 0; i + 1 < own.size(); ++i) {
        const GameObjectId a = own[i], b = own[own.size() - 1 - i];
        if (a == b) break;
        if (view.about(a).deck_position == DeckPosition::Unknown &&
            view.about(b).deck_position == DeckPosition::Unknown &&
            truth.getObject(a).card_def_id != truth.getObject(b).card_def_id) {
            swapCards(a, b);
            break;
        }
    }
    // Own hand order.
    auto& my_hand = variant.player(observer).hand;
    *hand_reordered = false;
    if (my_hand.size() > 1) {
        std::reverse(my_hand.begin(), my_hand.end());
        *hand_reordered = true;
    }
    return changed;
}

void checkVariants(const GameState& state, const StepResult& step, const CardDB& db,
                   uint64_t seed, const std::string& where, VariantTally& t) {
    ++t.decisions;
    const PlayerId decider = step.perspective;
    if (decider == PlayerId::None) return;
    // Census of prompt kinds (batch 3's chain-item and [Add]-payment prompts).
    if (!step.legal.empty() && step.legal.front().type == IntentType::MakeChoice) {
        ++t.prompts;
        bool chain_item = false, add_payment = false;
        for (const Intent& i : step.legal) {
            if (i.choice_label.rfind("[Add] for this payment", 0) == 0) add_payment = true;
            if (i.chosen_value && !i.chosen_objects.empty())
                for (const auto& ci : state.chain.items)
                    if (static_cast<int64_t>(ci.id.raw()) == *i.chosen_value &&
                        ci.source == i.chosen_objects.front())
                        chain_item = true;
        }
        t.chain_item_prompts += chain_item;
        t.add_payment_prompts += add_payment;
    }
    for (const Intent& i : step.legal) {
        auto named = [&](GameObjectId id) {
            return id != kInvalidId && state.objectExists(id) &&
                   !knowledge::identityVisible(state, id, decider);
        };
        bool hidden = named(i.card) || named(i.ability_source);
        for (auto id : i.chosen_objects) hidden = hidden || named(id);
        for (auto id : i.targets) hidden = hidden || named(id);
        // An option that offers the decider one of its own cards it was not
        // shown: the decider cannot tell such options apart (key 0, and the
        // card's look was not recorded). An opponent's hidden card is fine.
        auto own_unseen = [&](GameObjectId id) {
            return named(id) && state.getObject(id).owner == decider;
        };
        bool mine = own_unseen(i.card) || own_unseen(i.ability_source);
        for (auto id : i.chosen_objects) mine = mine || own_unseen(id);
        for (auto id : i.targets) mine = mine || own_unseen(id);
        if (mine)
            t.problem(where + ": an option offers " + toString(decider) +
                      " a card of its own it was not shown (" + i.describe() + ")");
        if (hidden) {
            ++t.ids_named;
            if (std::getenv("RIFTBOUND_LEAK_VERBOSE"))
            {
                std::cout << "  hidden object named: " << where << ": " << i.describe()
                          << " '" << i.choice_label << "'";
                for (auto id : i.chosen_objects)
                    if (state.objectExists(id))
                        std::cout << " [" << state.getObject(id).name.get() << " zone "
                                  << static_cast<int>(state.getObject(id).zone.get()) << " owner "
                                  << toString(state.getObject(id).owner) << "]";
                if (!state.chain.items.empty() && state.objectExists(state.chain.items.back().source))
                    std::cout << " chain top: " << state.getObject(state.chain.items.back().source).name.get();
                if (state.chain.resuming && state.objectExists(state.chain.resuming->source))
                    std::cout << " resolving: " << state.getObject(state.chain.resuming->source).name.get();
                std::cout << "\n";
            }
        }
    }
    GameState variant = state;
    std::mt19937_64 rng(seed);
    bool reordered = false;
    const long changed = makeVariant(state, variant, decider, db, rng, &reordered);
    if (changed == 0 && !reordered) return;
    ++t.variants;
    t.changed_cards += changed;
    t.hand_orders += reordered;
    // Control: the variant is another position.
    bool differs = reordered;
    for (const auto& [id, obj] : state.objects)
        if (variant.getObject(id).card_def_id != obj.card_def_id) differs = true;
    if (!differs) t.problem(where + ": the variant is the same position");
    const Shown a = shownTo(state, step.legal, decider, decider, db);
    const Shown b = shownTo(variant, step.legal, decider, decider, db);
    if (!(a == b)) t.problem(where + ": " + firstDifference(a, b));
}

TEST_F(HiddenOrderLeak, PositionsTheDeciderCannotTellApartShowItTheSame) {
    VariantTally t;
    const int games = gamesOr(60);
    for (int g = firstGame(); g < firstGame() + games; ++g) {
        const auto [d1, d2] = decks(g);
        EventBus bus;
        GameEngine engine(*db_, bus, *registry_);
        const uint64_t seed = 20261014 + static_cast<uint64_t>(g);
        std::mt19937_64 rng(seed * 31 + 7);
        const double pass_p = g % 4 < 2 ? -1.0 : 0.2;   // random, random-player
        engine.beginGame(d1, d2, seed);
        for (int n = 0; n < 4000 && !engine.isStepDone(); ++n) {
            const StepResult step = engine.currentStep();
            checkVariants(engine.state(), step, *db_, seed * 1000 + n,
                          deckName(g) + " seed " + std::to_string(seed) + " decision " +
                              std::to_string(n),
                          t);
            engine.applyChoice(static_cast<int>(
                chooseByActionId(engine.state(), step.legal, rng, pass_p)));
        }
    }
    for (const auto& p : t.problems) ADD_FAILURE() << p;
    EXPECT_GT(t.variants, t.decisions / 2);
    EXPECT_GT(t.hand_orders, 0);
    std::cout << "[ hidden order, variants: " << games << " games, " << t.decisions
              << " decisions, " << t.variants << " variant positions (" << t.changed_cards
              << " hidden cards changed, " << t.hand_orders << " own hands reordered); "
              << t.ids_named << " options naming an object the decider cannot see; " << t.prompts
              << " prompts, " << t.chain_item_prompts << " choosing a chain item, "
              << t.add_payment_prompts << " choosing an [Add] for a payment ]\n";
}

// ─── 2. The same game under other ids ───────────────────────────────────────
//
// Two engines, one seed, two id label salts: the same deal, other numbers.
// While the two games are the same game up to which number each card has
// (`stateKey`), every decision must show both players the same thing, and
// both games take the option with the same action id. The engine handles
// some simultaneous things in the object table's order (several units dealt
// damage at once, cards put into a trash or a deck together, which of two
// identical runes pays): there the two games may become different games.
// That is not something a player is shown (the ids are random), so the
// comparison of that pair stops there and is counted.

/// The game state as text with no object id in it: each object by its card
/// and state, each zone by the cards in it. Two states with equal keys are
/// the same game up to which number each card has (objects nobody can tell
/// apart, such as two ready runes of one card, may have traded numbers).
std::string stateKey(const GameState& s) {
    auto card = [&](GameObjectId o) {
        return s.objectExists(o) ? std::to_string(s.getObject(o).card_def_id.raw())
                                 : std::string("?");
    };
    auto cards = [&](const std::vector<GameObjectId>& v) {
        std::string out;
        for (auto o : v) out += card(o) + ",";
        return out;
    };
    std::vector<std::string> objs;
    for (const auto& [oid, o] : s.objects) {
        std::ostringstream x;
        x << o.card_def_id.raw() << ':' << static_cast<int>(o.zone.get()) << ':'
          << static_cast<int>(o.controller) << ':' << static_cast<int>(o.owner) << ':'
          << o.current_might << ':' << o.damage_marked << ':' << o.is_exhausted << ':'
          << o.is_stunned << ':' << o.buff_count << ':'
          << (o.attached_to ? card(*o.attached_to) : std::string("-"));
        if (o.location.has_value()) {
            if (const auto* bf = std::get_if<BattlefieldLocation>(&*o.location))
                x << "@bf" << bf->id.raw();
            else
                x << "@base" << static_cast<int>(std::get<BaseLocation>(*o.location).player);
        }
        objs.push_back(x.str());
    }
    std::sort(objs.begin(), objs.end());
    std::ostringstream out;
    for (const auto& t : objs) out << t << ';';
    for (const auto& ps : s.players)
        out << "|P" << ps.score << " h" << cards(ps.hand) << " d" << cards(ps.main_deck)
            << " r" << cards(ps.rune_deck) << " t" << cards(ps.trash) << " b"
            << cards(ps.banishment);
    for (const auto& bf : s.battlefields)
        out << "|bf" << bf.id.raw() << ' ' << (bf.controller ? static_cast<int>(*bf.controller) : -1)
            << " f" << cards(bf.facedown);
    for (const auto& ci : s.chain.items)
        out << "|c" << card(ci.source) << ':' << ci.card_def_id.raw() << ':'
            << static_cast<int>(ci.controller) << ':' << static_cast<int>(ci.status);
    return out.str();
}

/// A log line with every number made '#': ids, chain item numbers, and the
/// running totals of things done at once in the table's order ("-> 3M").
std::string withoutNumbers(const std::string& line) {
    static const std::regex digits("[0-9]+");
    std::string l = line;
    if (l.compare(0, 3, "[D:") == 0 && l.find("] ") != std::string::npos)
        l = l.substr(l.find("] ") + 2);
    return std::regex_replace(l, digits, "#");
}

TEST_F(HiddenOrderLeak, TheSameGameUnderOtherObjectIdsLooksTheSameToBothPlayers) {
    const int games = gamesOr(60);
    long decisions = 0, renamed = 0, log_lines = 0, diverged = 0, finished = 0;
    std::vector<std::string> problems;
    auto problem = [&](const std::string& s) { if (problems.size() < 20) problems.push_back(s); };
    for (int g = firstGame(); g < firstGame() + games; ++g) {
        const auto [d1, d2] = decks(g);
        const uint64_t seed = 777000 + static_cast<uint64_t>(g);
        EventBus bus_a, bus_b;
        LogTap tap_a, tap_b;
        tap_a.attach(bus_a);
        tap_b.attach(bus_b);
        GameEngine a(*db_, bus_a, *registry_), b(*db_, bus_b, *registry_);
        a.setIdLabelSalt(0);
        b.setIdLabelSalt(1 + static_cast<uint64_t>(g));
        a.beginGame(d1, d2, seed);
        b.beginGame(d1, d2, seed);
        // Control: the decks hold the same cards in the same order under
        // other numbers.
        bool other_ids = false;
        for (PlayerId p : {PlayerId::Player1, PlayerId::Player2})
            other_ids = other_ids || a.state().player(p).main_deck != b.state().player(p).main_deck;
        if (other_ids) ++renamed;
        std::mt19937_64 rng_a(seed * 13 + 5), rng_b(seed * 13 + 5);
        const double pass_p = g % 4 < 2 ? -1.0 : 0.2;
        const std::string where = deckName(g) + " seed " + std::to_string(seed);
        for (int n = 0; n < 4000; ++n) {
            if (stateKey(a.state()) != stateKey(b.state())) {
                if (std::getenv("RIFTBOUND_LEAK_VERBOSE")) {
                    const std::string ka = stateKey(a.state()), kb = stateKey(b.state());
                    size_t k = 0;
                    while (k < ka.size() && k < kb.size() && ka[k] == kb[k]) ++k;
                    const size_t from = ka.rfind(';', k) == std::string::npos ? 0 : ka.rfind(';', k);
                    std::cout << "  diverged: " << where << " decision " << n << ": "
                              << ka.substr(from, 120) << " | " << kb.substr(from, 120) << "\n";
                }
                ++diverged;   // handled in the table's order: another game now
                break;
            }
            bool ok = true;
            for (int p = 0; p < 2; ++p) {
                std::vector<std::string> la, lb;
                for (const auto& l : tap_a.lines[p]) la.push_back(withoutNumbers(l));
                for (const auto& l : tap_b.lines[p]) lb.push_back(withoutNumbers(l));
                std::sort(la.begin(), la.end());
                std::sort(lb.begin(), lb.end());
                log_lines += static_cast<long>(la.size());
                if (la != lb) {
                    size_t k = 0;
                    while (k < std::min(la.size(), lb.size()) && la[k] == lb[k]) ++k;
                    problem(where + " decision " + std::to_string(n) + ": the log Player" +
                            std::to_string(p + 1) + " reads differs: '" +
                            (k < la.size() ? la[k] : std::string("<end>")) + "' vs '" +
                            (k < lb.size() ? lb[k] : std::string("<end>")) + "'");
                    ok = false;
                }
            }
            tap_a.clear();
            tap_b.clear();
            if (!ok) break;
            if (a.isStepDone() || b.isStepDone()) {
                if (a.isStepDone() != b.isStepDone())
                    problem(where + ": one game ended, the other did not");
                else
                    ++finished;
                break;
            }
            const StepResult sa = a.currentStep(), sb = b.currentStep();
            ++decisions;
            if (sa.perspective != sb.perspective) {
                problem(where + ": another player decides at decision " + std::to_string(n));
                break;
            }
            for (PlayerId who : {PlayerId::Player1, PlayerId::Player2}) {
                const Shown va = shownTo(a.state(), sa.legal, sa.perspective, who, *db_);
                const Shown vb = shownTo(b.state(), sb.legal, sb.perspective, who, *db_);
                if (!(va == vb)) {
                    if (std::getenv("RIFTBOUND_LEAK_VERBOSE")) {
                        for (const GameEngine* e : {&a, &b}) {
                            const auto& st = e->state();
                            std::cout << "  chain:";
                            for (const auto& ci : st.chain.items)
                                std::cout << " [" << (st.objectExists(ci.source) ? st.getObject(ci.source).name.get() : std::string("?"))
                                          << " zone " << (st.objectExists(ci.source) ? static_cast<int>(st.getObject(ci.source).zone.get()) : -1)
                                          << " ctl " << toString(ci.controller) << " st " << static_cast<int>(ci.status)
                                          << " batch " << ci.trigger_batch << " src " << ci.source.raw()
                                          << " subj " << ci.triggering_subject.raw() << "]";
                            std::cout << "\n  objects of those cards:";
                            for (const auto& ci : st.chain.items)
                                for (const auto& [oid, o] : st.objects)
                                    if (st.objectExists(ci.source) && o.card_def_id == st.getObject(ci.source).card_def_id)
                                        std::cout << " " << o.name.get() << "#" << oid.raw() << " z" << static_cast<int>(o.zone.get());
                            std::cout << "\n";
                            const auto& lg = e->currentStep().legal;
                            const auto ids = openspiel::actionIds(lg, st);
                            std::cout << "  -- game " << (e == &a ? "A" : "B") << "\n";
                            for (size_t i : byActionId(st, lg)) {
                                std::cout << "   " << ids[i] << " " << lg[i].describe() << " '"
                                          << lg[i].choice_label << "'";
                                for (auto o : lg[i].chosen_objects)
                                    if (st.objectExists(o)) std::cout << " obj:" << st.getObject(o).name.get();
                                std::cout << "\n";
                            }
                        }
                    }
                    problem(where + " decision " + std::to_string(n) + " (shown to " +
                            toString(who) + "): " + firstDifference(va, vb));
                    ok = false;
                }
            }
            if (!ok) break;
            const size_t ia = chooseByActionId(a.state(), sa.legal, rng_a, pass_p);
            const size_t ib = chooseByActionId(b.state(), sb.legal, rng_b, pass_p);
            // One action id: the decider sees the two options as the same
            // (their descriptions were compared above). They may name
            // different numbers for cards nobody can tell apart.
            a.applyChoice(static_cast<int>(ia));
            b.applyChoice(static_cast<int>(ib));
        }
    }
    for (const auto& p : problems) ADD_FAILURE() << p;
    EXPECT_EQ(renamed, games) << "the salt did not change the ids";
    EXPECT_GT(decisions, 50L * games) << "too few decisions compared";
    std::cout << "[ hidden order, relabelled: " << games << " game pairs (" << finished
              << " compared to the end, " << diverged
              << " became other games by the table's order), " << decisions
              << " decisions compared for both players, " << log_lines
              << " log lines compared ]\n";
}

// ─── 3. An opponent who never acts ──────────────────────────────────────────

/// Player 2 passes wherever it can and otherwise takes its first option by
/// action id; Player 1 plays `random` or `random-player` by action id. Its
/// deck is `base` in one game and `other` in the other. Compared until a card
/// that differs between the two games could be known to Player 1.
TEST_F(HiddenOrderLeak, AnOpponentWhoNeverActsCannotBeToldFromAnother) {
    const int games = gamesOr(40);
    long decisions = 0, pairs = 0, ended_by_sight = 0, differing_hands = 0, log_lines = 0;
    std::vector<std::string> problems;
    auto problem = [&](const std::string& s) { if (problems.size() < 20) problems.push_back(s); };
    for (int g = firstGame(); g < firstGame() + games; ++g) {
        const auto [mine, base] = decks(g);
        DeckSubmission other = base;
        std::mt19937_64 deck_rng(9100 + static_cast<uint64_t>(g));
        if (g % 2 == 0) {
            // The same cards, listed in another order (another deal).
            std::shuffle(other.main_deck.begin(), other.main_deck.end(), deck_rng);
        } else {
            // Other cards of the legend's pool for a third of the deck.
            const auto* pool = builder_->legendCards(base.legend);
            if (!pool || pool->main.empty()) continue;
            for (size_t k = 0; k < other.main_deck.size(); k += 3)
                other.main_deck[k] = pool->main[deck_rng() % pool->main.size()];
        }
        ++pairs;
        const uint64_t seed = 515000 + static_cast<uint64_t>(g);
        EventBus bus_a, bus_b;
        LogTap tap_a, tap_b;
        tap_a.attach(bus_a);
        tap_b.attach(bus_b);
        GameEngine a(*db_, bus_a, *registry_), b(*db_, bus_b, *registry_);
        a.beginGame(mine, base, seed);
        b.beginGame(mine, other, seed);
        std::mt19937_64 rng_a(seed * 17 + 3), rng_b(seed * 17 + 3);
        const double pass_p = g % 4 < 2 ? -1.0 : 0.2;
        const std::string where = deckName(g) + " seed " + std::to_string(seed);
        {
            // Control: Player 2's deck (dealt, before the opening hand) is
            // another deck in the other game, card by card in the first seven.
            const auto& da = a.state().player(PlayerId::Player2).main_deck;
            const auto& db = b.state().player(PlayerId::Player2).main_deck;
            bool differ = false;
            for (size_t k = 0; k < 7 && k < da.size() && k < db.size(); ++k) {
                const auto ia = da[da.size() - 1 - k], ib = db[db.size() - 1 - k];
                if (a.state().getObject(ia).card_def_id != b.state().getObject(ib).card_def_id)
                    differ = true;
            }
            if (differ) ++differing_hands;
        }
        // A card of Player 2's that is not the same card in both games and
        // that Player 1 could know in either.
        auto seen = [&]() {
            for (const GameEngine* e : {&a, &b}) {
                const GameEngine* f = e == &a ? &b : &a;
                for (const auto& [id, obj] : e->state().objects) {
                    if (obj.owner != PlayerId::Player2) continue;
                    if (f->state().objectExists(id) &&
                        f->state().getObject(id).card_def_id == obj.card_def_id)
                        continue;
                    if (knowledge::identityVisible(e->state(), id, PlayerId::Player1)) return true;
                }
            }
            return false;
        };
        for (int n = 0; n < 4000; ++n) {
            if (seen()) { ++ended_by_sight; break; }
            std::vector<std::string> la = tap_a.lines[0], lb = tap_b.lines[0];
            tap_a.clear();
            tap_b.clear();
            log_lines += static_cast<long>(la.size());
            if (la != lb) {
                size_t k = 0;
                while (k < std::min(la.size(), lb.size()) && la[k] == lb[k]) ++k;
                problem(where + " decision " + std::to_string(n) +
                        ": the log Player1 reads differs: '" +
                        (k < la.size() ? la[k] : std::string("<end>")) + "' vs '" +
                        (k < lb.size() ? lb[k] : std::string("<end>")) + "'");
                break;
            }
            if (a.isStepDone() || b.isStepDone()) {
                if (a.isStepDone() != b.isStepDone())
                    problem(where + ": one game ended, the other did not");
                break;
            }
            const StepResult sa = a.currentStep(), sb = b.currentStep();
            if (sa.perspective != sb.perspective) {
                problem(where + ": another player decides at decision " + std::to_string(n));
                break;
            }
            ++decisions;
            const Shown va = shownTo(a.state(), sa.legal, sa.perspective, PlayerId::Player1, *db_);
            const Shown vb = shownTo(b.state(), sb.legal, sb.perspective, PlayerId::Player1, *db_);
            if (!(va == vb)) {
                problem(where + " decision " + std::to_string(n) + ": " + firstDifference(va, vb));
                break;
            }
            auto pick = [&](GameEngine& e, const StepResult& s, std::mt19937_64& rng) {
                if (s.perspective == PlayerId::Player1)
                    return chooseByActionId(e.state(), s.legal, rng, pass_p);
                const std::vector<size_t> order = byActionId(e.state(), s.legal);
                for (size_t i : order)
                    if (isPassOption(s.legal, i)) return i;
                return order.front();
            };
            a.applyChoice(static_cast<int>(pick(a, sa, rng_a)));
            b.applyChoice(static_cast<int>(pick(b, sb, rng_b)));
        }
    }
    for (const auto& p : problems) ADD_FAILURE() << p;
    EXPECT_GT(differing_hands, pairs / 2) << "the two opponents rarely hold other cards";
    std::cout << "[ hidden order, passive opponent: " << pairs << " game pairs ("
              << differing_hands << " with another top of deck), " << decisions
              << " decisions compared, " << log_lines << " log lines; " << ended_by_sight
              << " stopped when a differing card became visible ]\n";
}

}  // namespace riftbound::test::hidden_order_leak
