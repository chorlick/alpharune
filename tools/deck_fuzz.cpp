/// @file deck_fuzz.cpp
/// Whole-pool fuzz: random agents play random constructed-legal decks (or the
/// deck lists of a directory) and every game that fails is recorded with what
/// is needed to play it again alone.
///
///   deck_fuzz --decks N --games-per-pair M --seed S --threads T [--json out.json]
///       N decks by seed (RandomDeckBuilder); pair j is deck[j % N] against
///       deck[(j % N + 1 + j / N) % N]; `--pairs P` pairs (default N), M games each.
///   deck_fuzz --deck-dir decks --games-per-pair M --seed S --threads T
///       every *.txt list of the directory against every one (ordered, mirrors included).
///   deck_fuzz --replay <deckA> <deckB> <game seed> [--trace] [--tail 30] [--show-decks]
///       one game alone. A deck is a deck seed (a number) or a deck list path.
///   deck_fuzz --deck-stats N --seed S
///       builds N decks and prints how the card pool is covered; plays nothing.
///
/// A game is identified by (deck A, deck B, game seed) and by nothing else:
/// not by the thread count, not by its position in a batch.
///
/// The result (`--json`): totals, every failure grouped by its exception text
/// with numbers and card names normalised (count, cards named, the examples
/// with the fewest decisions and their command lines), the games that hit the
/// turn cap or ended without a winner or a reason, the longest 0.1% of games,
/// and per-card coverage. Exit status: 0 no failure, 1 failures, 2 usage,
/// 3 a game did not come back (hang; the result is written first).

#include "cards/card_registry.h"
#include "core/card_db.h"
#include "rules/deck_validator.h"
#include "rules/random_deck.h"
#include "tools/fuzz/fuzz_game.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>

using namespace riftbound;
using nlohmann::json;

namespace {

uint64_t mix64(uint64_t a, uint64_t b) {
    uint64_t z = a * 0x9E3779B97F4A7C15ull + b + 0x632BE59BD9B4E019ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// ─── What a crash was playing ───────────────────────────────────────────────
// Written before each game; a fatal signal prints it. Nothing else can say
// which game a segmentation fault or an abort happened in.
thread_local char t_current_game[200] = "";

void onFatalSignal(int sig) {
    const char head[] = "\ndeck_fuzz: fatal signal in game: ";
    (void)!write(2, head, sizeof(head) - 1);
    (void)!write(2, t_current_game, std::strlen(t_current_game));
    (void)!write(2, "\n", 1);
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

// ─── Decks ──────────────────────────────────────────────────────────────────

struct DeckRef {
    std::string spec;       // a deck seed in decimal, or a deck list path
    DeckSubmission deck;
};

bool isNumber(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(),
                                     [](unsigned char c) { return std::isdigit(c) != 0; });
}

DeckRef loadDeck(const std::string& spec, const CardDB& db, const RandomDeckBuilder& builder) {
    DeckRef r;
    r.spec = spec;
    r.deck = isNumber(spec) ? builder.build(std::stoull(spec))
                            : DeckValidator::loadFromDeckList(spec, db);
    return r;
}

std::string deckSummary(const DeckSubmission& d, const CardDB& db) {
    return db.get(d.legend).name + " / " + db.get(d.chosen_champion).name;
}

// ─── Normalising a failure's text ───────────────────────────────────────────

/// Card names, longest first, for finding them in an exception text.
std::vector<std::string> cardNamesLongestFirst(const CardDB& db) {
    std::set<std::string> names;
    for (const auto& [id, def] : db.all())
        if (def.name.size() >= 3) names.insert(def.name);
    std::vector<std::string> v(names.begin(), names.end());
    std::sort(v.begin(), v.end(), [](const std::string& a, const std::string& b) {
        return a.size() != b.size() ? a.size() > b.size() : a < b;
    });
    return v;
}

bool wordChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }

/// Replaces every card name by <card> (whole words only) and every run of
/// digits by N; the names found are added to `cards`.
std::string normalise(std::string text, const std::vector<std::string>& names,
                      std::set<std::string>* cards) {
    for (const std::string& name : names) {
        std::size_t at = 0;
        while ((at = text.find(name, at)) != std::string::npos) {
            const bool left = at == 0 || !wordChar(text[at - 1]);
            const std::size_t end = at + name.size();
            const bool right = end >= text.size() || !wordChar(text[end]);
            if (left && right) {
                if (cards) cards->insert(name);
                text.replace(at, name.size(), "<card>");
                at += 6;
            } else {
                at = end;
            }
        }
    }
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(text[i]))) {
            if (out.empty() || out.back() != 'N') out += 'N';
            while (i + 1 < text.size() && std::isdigit(static_cast<unsigned char>(text[i + 1]))) ++i;
        } else {
            out += text[i];
        }
    }
    return out;
}

// ─── A batch ────────────────────────────────────────────────────────────────

struct Failure {
    std::string deck_a, deck_b;
    uint64_t game_seed = 0;
    std::string termination, exception;
    int decisions = 0, turns = 0;
    std::string legends;
    long game = 0;          // index in the batch
};

struct GameRow {            // one per game, for the length statistics
    uint32_t decisions = 0;
    uint16_t turns = 0;
    uint16_t longest_chain = 0;
    uint8_t flags = 0;      // see kFlag*
    uint8_t winner = 0;     // 0 none, 1 P1, 2 P2 (for --rows)
};
constexpr uint8_t kFlagFailed = 1, kFlagMaxTurns = 2, kFlagNoWinner = 4, kFlagNoReason = 8,
                  kFlagDone = 16;

struct Options {
    int decks = 0;
    long pairs = 0;
    int games_per_pair = 1;
    uint64_t seed = 1;
    int threads = 1;
    std::string json_path;
    std::string rows_path;        // --rows: one line per game, for comparing two builds
    std::string deck_dir;
    long max_decisions = 20000;
    double max_seconds = 120;
    int max_chain = 500;
    double memory_limit_gb = 4;
    int coverage_every = 1;       // 0 = none, k = every k-th game
    int hang_seconds = 300;
    int examples = 10;
    int long_chain = 200;         // a chain of this many items is flagged
    std::string agents[2] = {"random", "random"};   // random | random-player[:pass=P]
    MatchFormat match_format = MatchFormat::BestOf1;  // --game-mode duel | match
    std::string guide_path;       // a previous result: build decks toward what it covered least
    double guide_boost = 8;
    std::string known_path;       // known failure signatures: "ID|text" per line
    bool quiet = false;
    // --replay
    std::vector<std::string> replay;
    bool trace = false;
    int tail = 0;
    bool show_decks = false;
    // --deck-stats
    int deck_stats = 0;
};

/// Known failure signatures: one "ID|text" per line ('#' starts a comment). A
/// failure whose "<termination>: <exception>" contains the text is that known
/// failure ("a && b": contains both); the first line that matches wins.
std::vector<std::pair<std::string, std::string>> loadKnown(const std::string& path) {
    std::vector<std::pair<std::string, std::string>> known;
    if (path.empty()) return known;
    std::ifstream in(path);
    if (!in) { std::cerr << "cannot read " << path << "\n"; std::exit(2); }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const auto bar = line.find('|');
        if (bar == std::string::npos || bar + 1 >= line.size()) continue;
        known.emplace_back(line.substr(0, bar), line.substr(bar + 1));
    }
    return known;
}

/// Weights for RandomDeckBuilder from a previous result's coverage: a card is
/// picked up to (1 + boost) times as often as a well-covered one, the less it
/// was exercised the more. "Exercised" is the smallest of the counts that
/// apply to the card: games in play; games it triggered in (if it has a
/// triggered ability); games it was activated in (if it has an activated one);
/// games something of it resolved in (a spell, or a card with an ability).
/// A card at or over the median of those gets weight 1.
std::vector<double> guideWeights(const std::string& path, double boost) {
    std::ifstream in(path);
    if (!in) { std::cerr << "cannot read " << path << "\n"; std::exit(2); }
    const json prev = json::parse(in);
    std::vector<std::pair<std::size_t, double>> scores;
    for (const auto& c : prev.at("coverage").at("cards")) {
        if (c.at("banned").get<bool>() || c.at("token_card").get<bool>()) continue;
        double s = c.at("games_in_play").get<double>();
        const bool trig = c.at("has_triggered_ability").get<bool>();
        const bool act = c.at("has_activated_ability").get<bool>();
        if (trig) s = std::min(s, c.at("games_triggered").get<double>());
        if (act) s = std::min(s, c.at("games_activated").get<double>());
        if (trig || act || c.at("type").get<std::string>() == "Spell")
            s = std::min(s, c.at("games_resolved").get<double>());
        scores.emplace_back(c.at("id").get<std::size_t>(), s);
    }
    std::vector<double> sorted;
    for (const auto& [id, sc] : scores) sorted.push_back(sc);
    std::sort(sorted.begin(), sorted.end());
    const double median = sorted.empty() ? 1.0 : std::max(1.0, sorted[sorted.size() / 2]);
    std::vector<double> weight(fuzz::kCardSlots, 1.0);
    for (const auto& [id, sc] : scores)
        if (id < weight.size()) weight[id] = 1.0 + boost * (1.0 - std::min(1.0, sc / median));
    return weight;
}

int usage() {
    std::cerr <<
        "usage:\n"
        "  deck_fuzz --decks N [--pairs P] --games-per-pair M --seed S --threads T [--json out.json]\n"
        "  deck_fuzz --deck-dir DIR --games-per-pair M --seed S --threads T [--json out.json]\n"
        "  deck_fuzz --replay <deckA> <deckB> <game seed> [--trace] [--tail N] [--show-decks]\n"
        "  deck_fuzz --deck-stats N --seed S [--json out.json]\n"
        "options: --max-decisions N (20000)  --max-seconds N per game (120, 0 = none)\n"
        "         --max-chain N: stop a game whose chain had N items without emptying (500, 0 = none)\n"
        "         --memory-limit-gb G for the process (4, 0 = none)\n"
        "         --coverage K (1: every game, 0: none, K: every K-th)\n"
        "         --hang-seconds N (300)  --examples N (10 per failure group)  --quiet\n"
        "         --long-chain N (200): flag a game with N chain items in one chain\n"
        "         --agent1 / --agent2 / --agents random | random-player[:pass=P]\n"
        "         --game-mode duel | match (duel: setup battlefields drawn at random, CR 487.5;\n"
        "             match: each seat chooses, CR 488.5)\n"
        "         --guide prev.json [--guide-boost B (8)]: weight deck building toward the cards\n"
        "             the previous result exercised least (a seed then means a deck only with\n"
        "             the same guide file: the replay command names it)\n"
        "         --rows FILE: one line per game (index, decks, game seed, winner, decisions,\n"
        "             turns, longest chain, flags), to compare two builds game by game\n"
        "         --known FILE: lines of ID|text; failures containing the text are reported as\n"
        "             known, apart from the new ones. Exit status 1 only for new failures.\n";
    return 2;
}

std::string g_replay_extra;   // the flags a replay needs to be the same game

std::string replayCommand(const std::string& a, const std::string& b, uint64_t seed) {
    return "RIFTBOUND_ROOT=. ./build/deck_fuzz --replay " + a + " " + b + " " +
           std::to_string(seed) + g_replay_extra + " --trace --tail 30";
}

json cardRow(const CardDef& def) {
    return {{"id", def.id.raw()}, {"name", def.name}, {"type", toString(def.card_type)}};
}

/// --deck-stats and the "decks" block of a batch: how the pool is covered.
json deckCoverage(const std::vector<DeckSubmission>& decks, const CardDB& db,
                  const RandomDeckBuilder& builder) {
    std::vector<uint32_t> in_decks(fuzz::kCardSlots, 0);
    std::unordered_set<std::string> distinct;
    std::map<std::string, int> legends;
    for (const auto& d : decks) {
        std::set<CardDefId> ids(d.main_deck.begin(), d.main_deck.end());
        ids.insert(d.legend);
        ids.insert(d.chosen_champion);
        ids.insert(d.rune_deck.begin(), d.rune_deck.end());
        ids.insert(d.battlefields.begin(), d.battlefields.end());
        for (CardDefId id : ids) ++in_decks[id.raw()];
        std::string key = std::to_string(d.legend.raw()) + "/" +
                          std::to_string(d.chosen_champion.raw()) + "/";
        auto sorted = [](std::vector<CardDefId> v) { std::sort(v.begin(), v.end()); return v; };
        for (CardDefId id : sorted(d.main_deck)) key += std::to_string(id.raw()) + ",";
        key += "/";
        for (CardDefId id : sorted(d.rune_deck)) key += std::to_string(id.raw()) + ",";
        key += "/";
        for (CardDefId id : sorted(d.battlefields)) key += std::to_string(id.raw()) + ",";
        distinct.insert(std::move(key));
        ++legends[db.get(d.legend).name];
    }
    json never = json::array();
    std::vector<uint32_t> counts;
    int appeared = 0;
    for (CardDefId id : builder.reachable()) {
        const uint32_t n = in_decks[id.raw()];
        if (n == 0) never.push_back(cardRow(db.get(id)));
        else { ++appeared; counts.push_back(n); }
    }
    std::sort(counts.begin(), counts.end());
    json unreachable = json::array();
    for (const auto& [id, why] : builder.unreachable()) {
        json row = cardRow(db.get(id));
        row["why"] = why;
        unreachable.push_back(row);
    }
    json j = {
        {"decks", decks.size()},
        {"distinct_decks", distinct.size()},
        {"legends_used", legends.size()},
        {"legends_available", builder.legendCount()},
        {"registered_cards", db.size()},
        {"reachable_cards", builder.reachable().size()},
        {"reachable_cards_in_at_least_one_deck", appeared},
        {"reachable_cards_in_no_deck", never},
        {"unreachable_cards", unreachable},
    };
    if (!counts.empty()) {
        j["decks_per_card"] = {{"min", counts.front()},
                               {"median", counts[counts.size() / 2]},
                               {"max", counts.back()}};
    }
    return j;
}

int runReplay(const Options& o, const CardDB& db, const CardRegistry& registry,
              const RandomDeckBuilder& builder) {
    const DeckRef a = loadDeck(o.replay[0], db, builder);
    const DeckRef b = loadDeck(o.replay[1], db, builder);
    const uint64_t seed = std::stoull(o.replay[2]);
    if (o.show_decks) {
        std::cout << "── deck A (" << a.spec << ") ──\n" << builder.toDeckList(a.deck)
                  << "── deck B (" << b.spec << ") ──\n" << builder.toDeckList(b.deck);
    }
    fuzz::GameOptions opt;
    opt.max_decisions = o.max_decisions;
    opt.max_seconds = o.max_seconds;
            opt.max_chain = o.max_chain;
            opt.agents[0] = o.agents[0];
            opt.agents[1] = o.agents[1];
            opt.match_format = o.match_format;
    // --trace --tail N: the last N lines once the game has ended. --trace
    // alone: every line as it comes, so a game that never ends shows too.
    opt.log_tail = o.trace && o.tail > 0 ? o.tail : 0;
    if (o.trace && o.tail <= 0) opt.log_stream = &std::cout;
    const fuzz::GameOutcome r = fuzz::playGame(db, registry, a.deck, b.deck, seed, opt);
    for (const auto& line : r.log) std::cout << "  " << line << "\n";
    std::cout << "deck A " << a.spec << " (" << deckSummary(a.deck, db) << ")\n"
              << "deck B " << b.spec << " (" << deckSummary(b.deck, db) << ")\n"
              << "game seed " << r.game_seed << ": " << r.termination;
    if (!r.exception.empty()) std::cout << ": " << r.exception;
    std::cout << "\nwinner " << toString(r.winner) << ", score " << r.scores[0] << "-"
              << r.scores[1] << ", turn " << r.turns << ", " << r.decisions << " decisions\n";
    return r.failed ? 1 : 0;
}

} // namespace

int main(int argc, char* argv[]) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << flag << " needs a value\n"; std::exit(usage()); }
            return argv[++i];
        };
        if (flag == "--decks") o.decks = std::stoi(value());
        else if (flag == "--pairs") o.pairs = std::stol(value());
        else if (flag == "--games-per-pair") o.games_per_pair = std::stoi(value());
        else if (flag == "--seed") o.seed = std::stoull(value());
        else if (flag == "--threads") o.threads = std::stoi(value());
        else if (flag == "--json") o.json_path = value();
        else if (flag == "--rows") o.rows_path = value();
        else if (flag == "--deck-dir") o.deck_dir = value();
        else if (flag == "--max-decisions") o.max_decisions = std::stol(value());
        else if (flag == "--max-seconds") o.max_seconds = std::stod(value());
        else if (flag == "--max-chain") o.max_chain = std::stoi(value());
        else if (flag == "--memory-limit-gb") o.memory_limit_gb = std::stod(value());
        else if (flag == "--coverage") o.coverage_every = std::stoi(value());
        else if (flag == "--hang-seconds") o.hang_seconds = std::stoi(value());
        else if (flag == "--examples") o.examples = std::stoi(value());
        else if (flag == "--long-chain") o.long_chain = std::stoi(value());
        else if (flag == "--agent1") o.agents[0] = value();
        else if (flag == "--agent2") o.agents[1] = value();
        else if (flag == "--agents") o.agents[0] = o.agents[1] = value();
        else if (flag == "--game-mode") {
            try { o.match_format = matchFormatFromString(value()); }
            catch (const std::exception& e) { std::cerr << e.what() << "\n"; return usage(); }
        }
        else if (flag == "--guide") o.guide_path = value();
        else if (flag == "--guide-boost") o.guide_boost = std::stod(value());
        else if (flag == "--known") o.known_path = value();
        else if (flag == "--quiet") o.quiet = true;
        else if (flag == "--trace") o.trace = true;
        else if (flag == "--tail") o.tail = std::stoi(value());
        else if (flag == "--show-decks") o.show_decks = true;
        else if (flag == "--deck-stats") o.deck_stats = std::stoi(value());
        else if (flag == "--replay") {
            for (int k = 0; k < 3; ++k) o.replay.push_back(value());
        } else {
            std::cerr << "Unknown argument " << flag << "\n";
            return usage();
        }
    }
    if (o.threads < 1 || o.games_per_pair < 1) return usage();

    // A game that grows without bound (a trigger that feeds itself) must end
    // as a recorded failure (std::bad_alloc), not take the machine with it.
    if (o.memory_limit_gb > 0) {
        const rlim_t bytes = static_cast<rlim_t>(o.memory_limit_gb * 1024.0 * 1024.0 * 1024.0);
        const rlimit lim{bytes, bytes};
        if (setrlimit(RLIMIT_AS, &lim) != 0) std::cerr << "warning: no memory limit set\n";
    }

    CardRegistry registry;
    registry.loadAll();
    CardDB db;
    db.buildFromClasses(registry);
    RandomDeckBuilder builder(db);
    const DeckValidator validator(db);
    if (!o.guide_path.empty()) {
        builder.setWeights(guideWeights(o.guide_path, o.guide_boost));
        g_replay_extra += " --guide " + o.guide_path;
        if (o.guide_boost != 8) g_replay_extra += " --guide-boost " + std::to_string(o.guide_boost);
    }
    for (int seat = 0; seat < 2; ++seat) {
        // Refuse a bad spec now, not in the first game.
        try { fuzz::FuzzAgent probe(o.agents[seat], 1, 1); }
        catch (const std::exception& e) { std::cerr << e.what() << "\n"; return 2; }
        if (o.agents[seat] != "random")
            g_replay_extra += " --agent" + std::to_string(seat + 1) + " " + o.agents[seat];
    }
    if (o.max_chain != 500) g_replay_extra += " --max-chain " + std::to_string(o.max_chain);
    if (o.match_format != MatchFormat::BestOf1)
        g_replay_extra += std::string(" --game-mode ") + toString(o.match_format);
    const auto known = loadKnown(o.known_path);

    if (!o.replay.empty()) return runReplay(o, db, registry, builder);

    // ── The decks ──
    std::vector<DeckRef> decks;
    json invalid_decks = json::array();
    const auto t_decks = std::chrono::steady_clock::now();
    if (!o.deck_dir.empty()) {
        std::vector<std::string> paths;
        for (const auto& e : std::filesystem::directory_iterator(o.deck_dir))
            if (e.path().extension() == ".txt") paths.push_back(e.path().string());
        std::sort(paths.begin(), paths.end());
        for (const auto& p : paths) decks.push_back(loadDeck(p, db, builder));
    } else {
        const int n = o.deck_stats > 0 ? o.deck_stats : o.decks;
        if (n < 1) return usage();
        decks.reserve(n);
        for (int i = 0; i < n; ++i) {
            DeckRef r;
            r.spec = std::to_string(mix64(o.seed, static_cast<uint64_t>(i)));
            r.deck = builder.build(std::stoull(r.spec));
            decks.push_back(std::move(r));
        }
    }
    const double deck_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t_decks).count();
    // DeckValidator is the judge. A seed deck it rejects is reported and not
    // played (the builder is wrong); a deck list it rejects is reported and
    // played all the same (it is what somebody plays).
    {
        std::vector<DeckRef> kept;
        for (auto& d : decks) {
            const ValidationResult v = validator.validate(d.deck);
            if (!v.is_legal) invalid_decks.push_back({{"deck", d.spec}, {"errors", v.errors}});
            if (v.is_legal || !o.deck_dir.empty()) kept.push_back(std::move(d));
        }
        decks = std::move(kept);
    }
    if (decks.empty()) { std::cerr << "no legal deck\n"; return 2; }

    std::vector<DeckSubmission> deck_lists;
    for (const auto& d : decks) deck_lists.push_back(d.deck);
    json decks_json = deckCoverage(deck_lists, db, builder);
    decks_json["rejected_by_the_validator"] = invalid_decks;
    decks_json["seconds_to_build"] = deck_seconds;

    if (o.deck_stats > 0) {
        std::cout << "decks " << decks_json["decks"] << ", distinct "
                  << decks_json["distinct_decks"] << ", rejected by the validator "
                  << invalid_decks.size() << "\n"
                  << "legends used " << decks_json["legends_used"] << " of "
                  << decks_json["legends_available"] << "\n"
                  << "registered cards " << decks_json["registered_cards"] << ", reachable "
                  << decks_json["reachable_cards"] << ", in at least one deck "
                  << decks_json["reachable_cards_in_at_least_one_deck"] << "\n";
        if (decks_json.contains("decks_per_card"))
            std::cout << "decks per card: " << decks_json["decks_per_card"].dump() << "\n";
        std::cout << "built in " << deck_seconds << " s\n";
        for (const auto& row : decks_json["unreachable_cards"])
            std::cout << "  unreachable: " << row["id"] << " " << row["name"].get<std::string>()
                      << " (" << row["why"].get<std::string>() << ")\n";
        for (const auto& row : decks_json["reachable_cards_in_no_deck"])
            std::cout << "  reachable, in no deck: " << row["id"] << " "
                      << row["name"].get<std::string>() << "\n";
        if (!o.json_path.empty()) std::ofstream(o.json_path) << decks_json.dump(1) << "\n";
        return invalid_decks.empty() ? 0 : 1;
    }

    // ── The pairs ──
    const long n = static_cast<long>(decks.size());
    const bool all_pairs = !o.deck_dir.empty();
    const long pairs = all_pairs ? n * n : (o.pairs > 0 ? o.pairs : n);
    auto pairDecks = [&](long j) -> std::pair<long, long> {
        if (all_pairs) return {j / n, j % n};
        const long a = j % n;
        return {a, (a + 1 + j / n) % n};
    };
    const long total = pairs * o.games_per_pair;
    auto gameSeed = [&](long game) {
        const long pair = game / o.games_per_pair;
        const long g = game % o.games_per_pair;
        return fuzz::engineSeed(mix64(o.seed ^ 0x5EEDull, static_cast<uint64_t>(pair)) +
                                static_cast<uint64_t>(g));
    };

    std::signal(SIGSEGV, onFatalSignal);
    std::signal(SIGABRT, onFatalSignal);
    std::signal(SIGFPE, onFatalSignal);

    // ── Play ──
    std::vector<GameRow> rows(static_cast<std::size_t>(total));
    std::vector<fuzz::CardCoverage> coverage(static_cast<std::size_t>(o.threads));
    std::vector<Failure> failures;
    std::map<std::string, long> reasons;
    std::mutex mu;
    std::atomic<long> next{0}, done{0};
    std::atomic<long> p1{0}, p2{0};
    struct Slot { std::atomic<long> game{-1}; std::atomic<int64_t> since{0}; };
    std::vector<Slot> slots(static_cast<std::size_t>(o.threads));
    auto nowMs = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch()).count();
    };
    const std::vector<std::string> names = cardNamesLongestFirst(db);

    auto fillRow = [&](long game, const fuzz::GameOutcome& r) {
        GameRow& row = rows[static_cast<std::size_t>(game)];
        row.decisions = static_cast<uint32_t>(r.decisions);
        row.turns = static_cast<uint16_t>(r.turns);
        row.longest_chain = static_cast<uint16_t>(std::min(r.longest_chain, 65535));
        row.flags = kFlagDone;
        if (r.failed) row.flags |= kFlagFailed;
        else {
            if (r.termination == "Max turns reached") row.flags |= kFlagMaxTurns;
            if (r.winner == PlayerId::None) row.flags |= kFlagNoWinner;
            if (r.termination.empty()) row.flags |= kFlagNoReason;
        }
        if (r.winner == PlayerId::Player1) ++p1;
        else if (r.winner == PlayerId::Player2) ++p2;
        row.winner = r.winner == PlayerId::Player1 ? 1 : r.winner == PlayerId::Player2 ? 2 : 0;
    };
    auto reasonKey = [&](const fuzz::GameOutcome& r) {
        return r.failed ? r.termination : normalise(r.termination, {}, nullptr);
    };

    const auto t0 = std::chrono::steady_clock::now();
    auto worker = [&](int w) {
        std::map<std::string, long> my_reasons;
        for (;;) {
            const long game = next.fetch_add(1);
            if (game >= total) break;
            const auto [ia, ib] = pairDecks(game / o.games_per_pair);
            const DeckRef& a = decks[static_cast<std::size_t>(ia)];
            const DeckRef& b = decks[static_cast<std::size_t>(ib)];
            const uint64_t seed = gameSeed(game);
            std::snprintf(t_current_game, sizeof(t_current_game), "--replay %s %s %llu",
                          a.spec.c_str(), b.spec.c_str(),
                          static_cast<unsigned long long>(seed));
            slots[w].since.store(nowMs());
            slots[w].game.store(game);

            fuzz::GameOptions opt;
            opt.max_decisions = o.max_decisions;
            opt.max_seconds = o.max_seconds;
            opt.max_chain = o.max_chain;
            opt.agents[0] = o.agents[0];
            opt.agents[1] = o.agents[1];
            opt.match_format = o.match_format;
            if (o.coverage_every > 0 && game % o.coverage_every == 0) opt.coverage = &coverage[w];
            const fuzz::GameOutcome r = fuzz::playGame(db, registry, a.deck, b.deck, seed, opt);
            slots[w].game.store(-1);

            fillRow(game, r);
            ++my_reasons[reasonKey(r)];
            if (r.failed) {
                Failure f;
                f.deck_a = a.spec;
                f.deck_b = b.spec;
                f.game_seed = seed;
                f.termination = r.termination;
                f.exception = r.exception;
                f.decisions = r.decisions;
                f.turns = r.turns;
                f.legends = deckSummary(a.deck, db) + " vs " + deckSummary(b.deck, db);
                f.game = game;
                std::lock_guard<std::mutex> lock(mu);
                failures.push_back(std::move(f));
            }
            ++done;
        }
        std::lock_guard<std::mutex> lock(mu);
        for (const auto& [k, v] : my_reasons) reasons[k] += v;
    };

    std::vector<std::thread> threads;
    for (int w = 0; w < o.threads; ++w) threads.emplace_back(worker, w);

    // The main thread watches: progress, and a game that does not come back.
    json hung = json::array();
    auto last_print = t0;
    while (done.load() < total && hung.empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const auto now = std::chrono::steady_clock::now();
        for (int w = 0; w < o.threads; ++w) {
            const long game = slots[w].game.load();
            if (game < 0) continue;
            if (nowMs() - slots[w].since.load() < int64_t{1000} * o.hang_seconds) continue;
            const auto [ia, ib] = pairDecks(game / o.games_per_pair);
            hung.push_back({{"deck_a", decks[ia].spec}, {"deck_b", decks[ib].spec},
                            {"game_seed", gameSeed(game)},
                            {"command", replayCommand(decks[ia].spec, decks[ib].spec,
                                                      gameSeed(game))}});
        }
        if (!o.quiet && std::chrono::duration<double>(now - last_print).count() >= 5.0) {
            last_print = now;
            const double el = std::chrono::duration<double>(now - t0).count();
            std::cerr << "\r  " << done.load() << "/" << total << " games, "
                      << static_cast<long>(done.load() / el) << " games/s, failures so far "
                      << [&] { std::lock_guard<std::mutex> l(mu); return failures.size(); }()
                      << "    " << std::flush;
        }
    }
    const bool is_hung = !hung.empty();
    if (!is_hung)
        for (auto& t : threads) t.join();
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (!o.quiet) std::cerr << "\n";

    // A game that runs out of memory can make an allocation fail in a game
    // another thread is playing. Every game that ended in std::bad_alloc is
    // therefore played again alone; what it does alone is its result.
    long bad_alloc_only_in_company = 0;
    if (!is_hung) {
        std::vector<Failure> kept;
        for (auto& f : failures) {
            if (f.exception.find("bad_alloc") == std::string::npos) {
                kept.push_back(std::move(f));
                continue;
            }
            const auto [ia, ib] = pairDecks(f.game / o.games_per_pair);
            fuzz::GameOptions opt;
            opt.max_decisions = o.max_decisions;
            opt.max_seconds = o.max_seconds;
            opt.max_chain = o.max_chain;
            opt.agents[0] = o.agents[0];
            opt.agents[1] = o.agents[1];
            opt.match_format = o.match_format;
            const fuzz::GameOutcome r =
                fuzz::playGame(db, registry, decks[ia].deck, decks[ib].deck, f.game_seed, opt);
            --reasons["engine_error"];
            ++reasons[reasonKey(r)];
            fillRow(f.game, r);
            if (!r.failed) { ++bad_alloc_only_in_company; continue; }
            f.termination = r.termination;
            f.exception = r.exception;
            f.decisions = r.decisions;
            f.turns = r.turns;
            kept.push_back(std::move(f));
        }
        failures = std::move(kept);
        if (reasons["engine_error"] == 0) reasons.erase("engine_error");
    }

    // ── The result ──
    std::unique_lock<std::mutex> final_lock(mu);   // workers may still run if a game hung
    // --rows: game index, the replay arguments, winner, decisions, turns,
    // longest chain, flags — one line per game, so two builds can be compared
    // game by game (a game is a function of its seeds).
    if (!o.rows_path.empty()) {
        std::ofstream out(o.rows_path);
        for (long g = 0; g < total; ++g) {
            const GameRow& r = rows[static_cast<std::size_t>(g)];
            const auto [ia, ib] = pairDecks(g / o.games_per_pair);
            out << g << '\t' << decks[static_cast<std::size_t>(ia)].spec << '\t'
                << decks[static_cast<std::size_t>(ib)].spec << '\t' << gameSeed(g) << '\t'
                << int(r.winner) << '\t' << r.decisions << '\t' << r.turns << '\t'
                << r.longest_chain << '\t' << int(r.flags) << '\n';
        }
    }
    long completed = 0, n_failed = 0, n_max_turns = 0, n_no_winner = 0, n_no_reason = 0;
    uint64_t sum_decisions = 0, sum_turns = 0;
    std::vector<uint32_t> lengths;
    lengths.reserve(rows.size());
    for (const auto& r : rows) {
        if (!(r.flags & kFlagDone)) continue;
        ++completed;
        sum_decisions += r.decisions;
        sum_turns += r.turns;
        if (r.flags & kFlagFailed) { ++n_failed; continue; }
        lengths.push_back(r.decisions);
        if (r.flags & kFlagMaxTurns) ++n_max_turns;
        if (r.flags & kFlagNoWinner) ++n_no_winner;
        if (r.flags & kFlagNoReason) ++n_no_reason;
    }
    auto gameJson = [&](long game) {
        const auto [ia, ib] = pairDecks(game / o.games_per_pair);
        const GameRow& r = rows[static_cast<std::size_t>(game)];
        return json{{"deck_a", decks[ia].spec}, {"deck_b", decks[ib].spec},
                    {"game_seed", gameSeed(game)}, {"decisions", r.decisions},
                    {"turns", r.turns}, {"longest_chain", r.longest_chain},
                    {"legends", deckSummary(decks[ia].deck, db) + " vs " +
                                    deckSummary(decks[ib].deck, db)},
                    {"command", replayCommand(decks[ia].spec, decks[ib].spec, gameSeed(game))}};
    };

    // Flagged games (not failures): listed, at most 200 of each.
    json flagged = {{"max_turns", json::array()}, {"no_winner", json::array()},
                    {"no_reason", json::array()}, {"long_chain", json::array()}};
    long n_long_chain = 0;
    for (long g = 0; g < total; ++g) {
        const uint8_t f = rows[static_cast<std::size_t>(g)].flags;
        if (!(f & kFlagDone)) continue;
        // Failed games too: a chain that ran away is often what failed it.
        if (rows[static_cast<std::size_t>(g)].longest_chain >= o.long_chain) {
            ++n_long_chain;
            if (flagged["long_chain"].size() < 200) flagged["long_chain"].push_back(gameJson(g));
        }
        if (f & kFlagFailed) continue;
        if ((f & kFlagMaxTurns) && flagged["max_turns"].size() < 200)
            flagged["max_turns"].push_back(gameJson(g));
        if ((f & kFlagNoWinner) && !(f & kFlagMaxTurns) && flagged["no_winner"].size() < 200)
            flagged["no_winner"].push_back(gameJson(g));
        if ((f & kFlagNoReason) && flagged["no_reason"].size() < 200)
            flagged["no_reason"].push_back(gameJson(g));
    }

    // The longest 0.1% of the games that ended (by decisions).
    json length = json::object();
    if (!lengths.empty()) {
        std::sort(lengths.begin(), lengths.end());
        auto q = [&](double p) {
            return lengths[std::min(lengths.size() - 1,
                                    static_cast<std::size_t>(p * static_cast<double>(lengths.size())))];
        };
        const uint32_t cut = q(0.999);
        std::vector<long> longest;
        for (long g = 0; g < total; ++g) {
            const GameRow& r = rows[static_cast<std::size_t>(g)];
            if ((r.flags & kFlagDone) && !(r.flags & kFlagFailed) && r.decisions >= cut)
                longest.push_back(g);
        }
        std::sort(longest.begin(), longest.end(), [&](long x, long y) {
            return rows[static_cast<std::size_t>(x)].decisions >
                   rows[static_cast<std::size_t>(y)].decisions;
        });
        json list = json::array();
        for (std::size_t i = 0; i < longest.size() && i < 100; ++i)
            list.push_back(gameJson(longest[i]));
        length = {{"decisions_mean", static_cast<double>(sum_decisions) / static_cast<double>(completed)},
                  {"decisions_median", q(0.5)}, {"decisions_p99", q(0.99)},
                  {"decisions_p999", cut}, {"decisions_max", lengths.back()},
                  {"turns_mean", static_cast<double>(sum_turns) / static_cast<double>(completed)},
                  {"games_at_or_over_p999", longest.size()},
                  {"longest", list}};
    }

    // Failures, grouped by their normalised text, most frequent first.
    struct Group { long count = 0; std::map<std::string, long> cards; std::vector<const Failure*> ex;
                   std::string known;
                 };
    std::map<std::string, Group> groups;
    long n_new_failures = 0, n_known_failures = 0, n_new_groups = 0;
    for (const auto& f : failures) {
        std::set<std::string> cards;
        const std::string raw = f.termination + ": " + f.exception;
        std::string known_id;
        for (const auto& [id, text] : known) {
            // "a && b": every part must occur.
            bool all = true;
            for (std::size_t at = 0; all && at <= text.size();) {
                std::size_t end = text.find(" && ", at);
                if (end == std::string::npos) end = text.size();
                if (raw.find(text.substr(at, end - at)) == std::string::npos) all = false;
                at = end + 4;
            }
            if (all) { known_id = id; break; }
        }
        // A known cause is one group whatever else the text says; the rest
        // group by their normalised text.
        const std::string key = known_id.empty()
            ? f.termination + ": " + normalise(f.exception, names, &cards)
            : "known " + known_id + ": " + f.termination + ": " +
                  normalise(f.exception, names, &cards);
        Group& g = groups[key];
        g.known = known_id;
        (known_id.empty() ? n_new_failures : n_known_failures) += 1;
        ++g.count;
        for (const auto& c : cards) ++g.cards[c];
        g.ex.push_back(&f);
    }
    std::vector<std::pair<std::string, Group*>> ordered;
    for (auto& [k, g] : groups) ordered.emplace_back(k, &g);
    std::sort(ordered.begin(), ordered.end(), [](const auto& x, const auto& y) {
        return x.second->count != y.second->count ? x.second->count > y.second->count
                                                  : x.first < y.first;
    });
    json groups_json = json::array();
    for (auto& [key, g] : ordered) {
        std::sort(g->ex.begin(), g->ex.end(), [](const Failure* x, const Failure* y) {
            if (x->decisions != y->decisions) return x->decisions < y->decisions;
            if (x->game_seed != y->game_seed) return x->game_seed < y->game_seed;
            return x->deck_a < y->deck_a;
        });
        json ex = json::array();
        for (std::size_t i = 0; i < g->ex.size() && static_cast<int>(i) < o.examples; ++i) {
            const Failure& f = *g->ex[i];
            ex.push_back({{"deck_a", f.deck_a}, {"deck_b", f.deck_b}, {"game_seed", f.game_seed},
                          {"decisions", f.decisions}, {"turns", f.turns},
                          {"legends", f.legends}, {"exception", f.exception},
                          {"command", replayCommand(f.deck_a, f.deck_b, f.game_seed)}});
        }
        if (g->known.empty()) ++n_new_groups;
        groups_json.push_back({{"key", key}, {"count", g->count},
                               {"known", g->known.empty() ? json(nullptr) : json(g->known)},
                               {"cards_named", g->cards}, {"fewest_decisions_first", ex}});
        // One more example per card named, so each card of a group that
        // covers several has a reproduction of its own.
        json per_card = json::object();
        for (const auto& [card, n] : g->cards) {
            for (const Failure* f : g->ex) {
                std::set<std::string> in;
                normalise(f->exception, names, &in);
                if (!in.count(card)) continue;
                per_card[card] = {{"count", n}, {"decisions", f->decisions}, {"turns", f->turns},
                                  {"legends", f->legends}, {"exception", f->exception},
                                  {"command", replayCommand(f->deck_a, f->deck_b, f->game_seed)}};
                break;
            }
        }
        groups_json.back()["fewest_decisions_per_card"] = per_card;
    }

    // Coverage.
    fuzz::CardCoverage cov;
    for (const auto& c : coverage) cov.add(c);
    json cards_json = json::array();
    std::array<long, fuzz::kCoverageKinds> zero{};
    long cards_in_deck = 0;
    if (cov.games_counted > 0) {
        for (CardDefId id : registry.classDefIds()) {
            const CardDef& def = db.get(id);
            const Card* card = registry.get(id);
            const std::size_t i = id.raw();
            json row = cardRow(def);
            row["super_type"] = toString(def.super_type);
            row["banned"] = def.banned;
            row["token_card"] = def.isTokenCard();
            // TriggerType::Activated marks an activated ability, not a trigger.
            bool triggers = card && (card->equippedTriggerType() != TriggerType::None ||
                                     card->hasReflexiveTrigger());
            if (card)
                for (TriggerType t : card->triggerTypes())
                    if (t != TriggerType::None && t != TriggerType::Activated) triggers = true;
            row["has_triggered_ability"] = triggers;
            row["has_activated_ability"] = card && !card->abilities().empty();
            for (int k = 0; k < fuzz::kCoverageKinds; ++k) {
                row[std::string("games_") + fuzz::coverageKindName(k)] = cov.games[k][i];
                row[std::string("events_") + fuzz::coverageKindName(k)] = cov.events[k][i];
                if (cov.games[k][i] == 0) ++zero[k];
            }
            if (cov.games[fuzz::kInDeck][i] > 0) ++cards_in_deck;
            cards_json.push_back(std::move(row));
        }
    }

    json reasons_json = json::object();
    for (const auto& [k, v] : reasons) reasons_json[k] = v;

    json out = {
        {"config", {{"decks", decks.size()}, {"deck_dir", o.deck_dir}, {"pairs", pairs},
                    {"games_per_pair", o.games_per_pair}, {"seed", o.seed},
                    {"threads", o.threads}, {"max_decisions", o.max_decisions},
                    {"max_seconds", o.max_seconds}, {"max_chain", o.max_chain},
                    {"agent1", o.agents[0]}, {"agent2", o.agents[1]},
                    {"guide", o.guide_path}, {"guide_boost", o.guide_boost},
                    {"known", o.known_path}, {"replay_flags", g_replay_extra}, {"memory_limit_gb", o.memory_limit_gb},
                    {"coverage_every", o.coverage_every},
#ifdef RIFTBOUND_CHECK_BOARD_INDEX
                    {"checked_build", true},
#else
                    {"checked_build", false},
#endif
                   }},
        {"totals", {{"games", total}, {"completed", completed}, {"failures", n_failed},
                    {"failure_groups", groups.size()},
                    {"new_failures", n_new_failures}, {"new_failure_groups", n_new_groups},
                    {"known_failures", n_known_failures},
                    {"p1_wins", p1.load()}, {"p2_wins", p2.load()},
                    {"max_turns", n_max_turns}, {"no_winner", n_no_winner},
                    {"no_reason", n_no_reason}, {"hung", hung.size()},
                    {"long_chain", n_long_chain},
                    {"bad_alloc_only_beside_another_game", bad_alloc_only_in_company},
                    {"decisions", sum_decisions}, {"seconds", seconds},
                    {"games_per_second", static_cast<double>(completed) / seconds},
                    {"games_per_second_per_thread",
                     static_cast<double>(completed) / seconds / o.threads}}},
        {"termination_reasons", reasons_json},
        {"failure_groups", groups_json},
        {"flagged", flagged},
        {"hung", hung},
        {"length", length},
        {"decks", decks_json},
        {"coverage", {{"games_counted", cov.games_counted},
                      {"registered_cards", registry.size()},
                      {"cards_in_a_deck", cards_in_deck},
                      {"cards_never_in_play", zero[fuzz::kInPlay]},
                      {"cards_never_triggered", zero[fuzz::kTriggered]},
                      {"cards_never_activated", zero[fuzz::kActivated]},
                      {"cards_never_resolved", zero[fuzz::kResolved]},
                      {"cards", cards_json}}},
    };
    if (!o.json_path.empty()) std::ofstream(o.json_path) << out.dump(1) << "\n";

    std::cout << std::fixed << std::setprecision(1)
              << "games " << completed << " of " << total << " on " << decks.size()
              << " decks (" << decks_json["distinct_decks"] << " distinct), " << pairs
              << " pairs x " << o.games_per_pair << "\n"
              << "P1 wins " << p1.load() << ", P2 wins " << p2.load() << ", failures "
              << n_failed << " in " << groups.size() << " groups (" << n_new_failures
              << " new in " << n_new_groups << " groups, " << n_known_failures
              << " known), max turns " << n_max_turns
              << ", no winner " << n_no_winner << ", no reason " << n_no_reason
              << ", chain of " << o.long_chain << "+ items " << n_long_chain << ", hung "
              << hung.size() << "\n"
              << seconds << " s, " << static_cast<double>(completed) / seconds << " games/s, "
              << static_cast<double>(completed) / seconds / o.threads << " per thread\n";
    if (!length.empty())
        std::cout << "decisions per game: mean " << length["decisions_mean"].get<double>()
                  << ", median " << length["decisions_median"] << ", p99 "
                  << length["decisions_p99"] << ", p99.9 " << length["decisions_p999"]
                  << ", max " << length["decisions_max"] << "\n";
    for (const auto& g : groups_json) {
        std::cout << std::setw(7) << g["count"].get<long>() << "  "
                  << (g["known"].is_null() ? "NEW  " : "")
                  << g["key"].get<std::string>().substr(0, 300) << "\n";
        if (!g["fewest_decisions_first"].empty())
            std::cout << "         " << g["fewest_decisions_first"][0]["command"].get<std::string>()
                      << "   (" << g["fewest_decisions_first"][0]["decisions"] << " decisions)\n";
    }
    for (const auto& h : hung)
        std::cout << "HUNG: " << h["command"].get<std::string>() << "\n";
    std::cout.flush();
    if (is_hung) std::_Exit(3);
    return n_new_failures > 0 ? 1 : 0;
}
