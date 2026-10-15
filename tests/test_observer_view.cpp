/// What a player may know, and the facts the engine derives, as per-observer
/// queries (testing phase T4, T6; engine/observer_view.h).
///
/// T4, the reverse of the leak test (trainer plan section 2, "the
/// completeness test"): two positions that differ in a fact the observer may
/// know are told apart by `ObserverView::fingerprint()`. At every decision of
/// real games each kind of knowable fact is changed in turn, for both
/// players, and the fingerprint must change. (The leak direction — positions
/// the observer cannot tell apart give the same fingerprint — is in
/// test_hidden_order_leak.cpp.)
///
/// T6: each derived fact agrees with what the engine uses for its own rules
/// at every decision: the lethal rule (EffectExecutor::damageKills), Might and
/// keywords, the payment predicates (resource::canPay), the timing window
/// against the options offered, combat designations against the combat in
/// progress, and the opponent's legal card pool against their true deck.

#include <gtest/gtest.h>

#include "agents/determinizer.h"
#include "agents/random_player_agent.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "engine/game_engine.h"
#include "engine/knowledge_tracker.h"
#include "engine/observer_view.h"
#include "engine/resource_payment.h"
#include "openspiel/action_vocab.h"
#include "rules/deck_model.h"
#include "rules/deck_validator.h"
#include "rules/random_deck.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace riftbound::test::observer_view {

std::string deckDir() {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    return std::string(root ? root : "..") + "/decks/";
}

int gamesOr(int dflt) {
    if (const char* e = std::getenv("RIFTBOUND_OBSERVER_GAMES")) return std::max(1, std::atoi(e));
    return dflt;
}

class ObserverViewGames : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        registry_ = new CardRegistry();
        registry_->loadAll();
        db_ = new CardDB();
        db_->buildFromClasses(*registry_);
        builder_ = new RandomDeckBuilder(*db_);
        model_ = new DeckModel(*db_);
        for (const auto& entry : std::filesystem::directory_iterator(deckDir()))
            if (entry.path().extension() == ".txt") lists_.push_back(entry.path().string());
        std::sort(lists_.begin(), lists_.end());
        for (const auto& [id, d] : db_->all())
            if ((d.card_type == CardType::Unit || d.card_type == CardType::Spell) && !d.isTokenCard())
                others_.push_back(id);
        std::sort(others_.begin(), others_.end());
    }
    static void TearDownTestSuite() {
        delete model_;
        delete builder_;
        delete db_;
        delete registry_;
    }
    static std::pair<DeckSubmission, DeckSubmission> decks(int g) {
        if (g % 2 == 0) {
            const size_t a = static_cast<size_t>(g / 2) * 3 % lists_.size();
            const size_t b = (a + 2 + static_cast<size_t>(g / 2) % (lists_.size() - 2)) %
                             lists_.size();
            return {DeckValidator::loadFromDeckList(lists_[a], *db_),
                    DeckValidator::loadFromDeckList(lists_[b], *db_)};
        }
        return {builder_->build(9100 + g), builder_->build(9900 + g)};
    }
    /// A card other than `not_this` (for changing a card's identity).
    static const CardDef& another(CardDefId not_this, size_t salt) {
        for (size_t k = 0; k < others_.size(); ++k) {
            const CardDefId c = others_[(salt + k) % others_.size()];
            if (c != not_this && db_->get(c).name != db_->get(not_this).name) return db_->get(c);
        }
        return db_->get(not_this);
    }
    /// Plays a game by action id (random or random-player) and calls `at`
    /// at every decision.
    template <typename At>
    static void play(int g, uint64_t seed, At&& at) {
        const auto [d1, d2] = decks(g);
        EventBus bus;
        GameEngine engine(*db_, bus, *registry_);
        std::mt19937_64 rng(seed * 3 + 11);
        const double pass_p = g % 4 < 2 ? -1.0 : 0.2;
        engine.beginGame(d1, d2, seed);
        for (int n = 0; n < 5000 && !engine.isStepDone(); ++n) {
            const StepResult step = engine.currentStep();
            at(engine, step, n);
            const std::vector<int> ids = openspiel::actionIds(step.legal, engine.state());
            std::vector<size_t> order(step.legal.size());
            for (size_t i = 0; i < order.size(); ++i) order[i] = i;
            std::stable_sort(order.begin(), order.end(),
                             [&](size_t a, size_t b) { return ids[a] < ids[b]; });
            size_t pick;
            if (pass_p < 0) {
                pick = order[rng() % order.size()];
            } else {
                std::vector<size_t> act, pass;
                for (size_t i : order) (isPassOption(step.legal, i) ? pass : act).push_back(i);
                const double u = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
                const auto& from = act.empty() || (!pass.empty() && u < pass_p) ? (pass.empty() ? act : pass) : act;
                pick = from[rng() % from.size()];
            }
            engine.applyChoice(static_cast<int>(pick));
        }
    }

    static CardRegistry* registry_;
    static CardDB* db_;
    static RandomDeckBuilder* builder_;
    static DeckModel* model_;
    static std::vector<std::string> lists_;
    static std::vector<CardDefId> others_;
};

CardRegistry* ObserverViewGames::registry_ = nullptr;
CardDB* ObserverViewGames::db_ = nullptr;
RandomDeckBuilder* ObserverViewGames::builder_ = nullptr;
DeckModel* ObserverViewGames::model_ = nullptr;
std::vector<std::string> ObserverViewGames::lists_;
std::vector<CardDefId> ObserverViewGames::others_;

// ─── T4: every knowable fact is told apart ──────────────────────────────────

TEST_F(ObserverViewGames, TwoPositionsThatDifferInAFactThePlayerMayKnowAreToldApart) {
    const int games = gamesOr(8);
    std::map<std::string, long> tried;
    std::vector<std::string> problems;
    auto problem = [&](const std::string& s) { if (problems.size() < 25) problems.push_back(s); };
    long decisions = 0;
    for (int g = 0; g < games; ++g) {
        play(g, 31000 + static_cast<uint64_t>(g), [&](GameEngine& engine, const StepResult&, int n) {
            ++decisions;
            const GameState& truth = engine.state();
            for (PlayerId who : {PlayerId::Player1, PlayerId::Player2}) {
                const PlayerId opp = opponent(who);
                const std::vector<int64_t> base =
                    ObserverView(truth, who, *db_, registry_).fingerprint();
                const knowledge::GameKnowledgeView view(truth, who);
                auto check = [&](const std::string& fact, const std::function<bool(GameState&)>& edit) {
                    GameState v = truth;
                    if (!edit(v)) return;
                    ++tried[fact];
                    if (ObserverView(v, who, *db_, registry_).fingerprint() == base)
                        problem("game " + std::to_string(g) + " decision " + std::to_string(n) +
                                " (" + toString(who) + "): " + fact + " changed, not told apart");
                };
                const size_t salt = static_cast<size_t>(n);
                check("a card in the opponent's trash", [&](GameState& v) {
                    const auto& t = v.player(opp).trash;
                    if (t.empty()) return false;
                    becomeCard(v.getObject(t.front()), another(v.getObject(t.front()).card_def_id, salt));
                    return true;
                });
                check("the order of a trash", [&](GameState& v) {
                    auto& t = v.player(who).trash;
                    for (size_t i = 0; i + 1 < t.size(); ++i)
                        if (v.getObject(t[i]).card_def_id != v.getObject(t[i + 1]).card_def_id) {
                            std::swap(t[i], t[i + 1]);
                            return true;
                        }
                    return false;
                });
                check("a banished card", [&](GameState& v) {
                    for (PlayerId p : {who, opp}) {
                        const auto& b = v.player(p).banishment;
                        if (b.empty()) continue;
                        becomeCard(v.getObject(b.back()), another(v.getObject(b.back()).card_def_id, salt));
                        return true;
                    }
                    return false;
                });
                check("a card in its own hand", [&](GameState& v) {
                    const auto& h = v.player(who).hand;
                    if (h.empty()) return false;
                    becomeCard(v.getObject(h.front()), another(v.getObject(h.front()).card_def_id, salt));
                    return true;
                });
                check("a card of the opponent's hand it knows", [&](GameState& v) {
                    for (auto id : v.player(opp).hand)
                        if (view.about(id).identity_known) {
                            becomeCard(v.getObject(id), another(v.getObject(id).card_def_id, salt));
                            return true;
                        }
                    return false;
                });
                check("a card it knows on top of its deck", [&](GameState& v) {
                    const auto top = view.knownDeckTop(who);
                    if (top.empty()) return false;
                    becomeCard(v.getObject(top.front().object),
                               another(top.front().card_def_id, salt));
                    return true;
                });
                check("a rune it knows at the bottom of a Rune Deck", [&](GameState& v) {
                    for (PlayerId p : {who, opp}) {
                        const auto bottom = view.knownRuneDeckBottom(p);
                        if (bottom.empty()) continue;
                        // Another rune card.
                        for (const auto& [id, d] : db_->all())
                            if (d.card_type == CardType::Rune && !d.isTokenCard() &&
                                id != bottom.front().card_def_id) {
                                becomeCard(v.getObject(bottom.front().object), d);
                                return true;
                            }
                    }
                    return false;
                });
                check("damage on a unit", [&](GameState& v) {
                    for (const auto& [id, o] : v.objects.onBoard())
                        if (o.isUnit()) { v.getObject(id).damage_marked += 1; return true; }
                    return false;
                });
                check("a unit's Might", [&](GameState& v) {
                    for (const auto& [id, o] : v.objects.onBoard())
                        if (o.isUnit()) { v.getObject(id).current_might += 1; return true; }
                    return false;
                });
                check("a keyword a unit has", [&](GameState& v) {
                    for (const auto& [id, o] : v.objects.onBoard())
                        if (o.isUnit() && !o.hasKeyword(Keyword::Tank)) {
                            v.getObject(id).aura_keywords.set(Keyword::Tank);
                            return true;
                        }
                    return false;
                });
                check("the opponent's floating Energy", [&](GameState& v) {
                    v.player(opp).rune_pool.energy += 1;
                    return true;
                });
                check("a score", [&](GameState& v) {
                    v.player(opp).score += 1;
                    return true;
                });
                check("a chain item's target", [&](GameState& v) {
                    for (auto& ci : v.chain.items) {
                        if (ci.chosen_objects.empty() && ci.targets.empty()) continue;
                        auto& slot = ci.targets.empty() ? ci.chosen_objects.front() : ci.targets.front();
                        for (const auto& [id, o] : v.objects.onBoard())
                            if (o.isUnit() && id != slot &&
                                openspiel::visibleObjectDescription(v, id, who) !=
                                    openspiel::visibleObjectDescription(v, slot, who)) {
                                slot = id;
                                return true;
                            }
                    }
                    return false;
                });
                check("who controls a chain item", [&](GameState& v) {
                    if (v.chain.items.empty()) return false;
                    auto& c = v.chain.items.back().controller;
                    c = opponent(c);
                    return true;
                });
                check("who holds priority", [&](GameState& v) {
                    auto& h = v.turn.priority_holder;
                    h = h.has_value() ? std::optional<PlayerId>{opponent(*h)}
                                      : std::optional<PlayerId>{who};
                    return true;
                });
                check("a battlefield's combat", [&](GameState& v) {
                    if (v.battlefields.empty()) return false;
                    auto& bf = v.battlefields.front();
                    bf.combat_staged = !bf.combat_staged;
                    return true;
                });
            }
        });
    }
    for (const auto& p : problems) ADD_FAILURE() << p;
    std::cout << "[ observer view, completeness: " << games << " games, " << decisions
              << " decisions; facts changed:";
    for (const auto& [f, n] : tried) std::cout << " " << f << "=" << n;
    std::cout << " ]\n";
    for (const char* f : {"a card in the opponent's trash", "the order of a trash", "a banished card",
                          "a card in its own hand", "a rune it knows at the bottom of a Rune Deck",
                          "damage on a unit", "a keyword a unit has", "a chain item's target",
                          "who controls a chain item"})
        EXPECT_GT(tried[f], 0) << f << " was never changed";
}

// ─── T6: the derived facts are the engine's ──────────────────────────────────

TEST_F(ObserverViewGames, EachDerivedFactIsWhatTheEngineUsesForItsRules) {
    const int games = gamesOr(24);
    std::vector<std::string> problems;
    auto problem = [&](const std::string& s) { if (problems.size() < 25) problems.push_back(s); };
    long decisions = 0, units = 0, lethal = 0, combats = 0, pools = 0, designated = 0;
    constexpr SpendFor kPurposes[4] = {SpendFor::Other, SpendFor::Unit, SpendFor::Spell, SpendFor::Gear};
    for (int g = 0; g < games; ++g) {
        play(g, 47000 + static_cast<uint64_t>(g), [&](GameEngine& engine, const StepResult& step, int n) {
            ++decisions;
            const GameState& s = engine.state();
            const std::string where = "game " + std::to_string(g) + " decision " + std::to_string(n);
            const PlayerId decider = step.perspective;
            const ObserverView view(s, decider, *db_, registry_);
            // Might, keywords, lethal: the engine's.
            for (const auto& f : view.board()) {
                const GameObject& o = s.getObject(f.id);
                if (!o.isUnit()) continue;
                ++units;
                if (f.might != o.current_might) problem(where + ": Might differs");
                for (uint32_t k = 0; k < static_cast<uint32_t>(Keyword::Count); ++k)
                    if (((f.keywords >> k) & 1u) != (o.hasKeyword(static_cast<Keyword>(k)) ? 1u : 0u))
                        problem(where + ": a keyword differs");
                if (f.lethal != engine.testHook_executor().damageKills(f.id))
                    problem(where + ": lethal differs from EffectExecutor::damageKills");
                lethal += f.lethal;
                // A designation exactly while its battlefield's combat is on (CR 323.2).
                const auto bf = o.battlefieldId();
                const bool in_combat = bf.has_value() && s.battlefields[*bf].combat_in_progress;
                if ((f.designation != CombatDesignation::None) != in_combat)
                    problem(where + ": a designation outside a combat, or none in one");
                if (in_combat) {
                    ++designated;
                    const auto& b = s.battlefields[*bf];
                    const bool attacking = b.attacker.has_value() && o.controller == *b.attacker;
                    if ((f.designation == CombatDesignation::Attacker) != attacking)
                        problem(where + ": the designation is not the controller's side");
                }
            }
            for (const auto& b : view.battlefields())
                if (b.combat_in_progress) {
                    ++combats;
                    if (!b.attacker || !b.defender) problem(where + ": a combat with no sides");
                }
            // Resources: the most each payment predicate accepts.
            for (PlayerId p : {PlayerId::Player1, PlayerId::Player2}) {
                const ResourceFacts r = view.resources(p);
                for (int k = 0; k < 4; ++k) {
                    if (!resource::canPay(s, p, r.energy[k], 0, Domain::Count, kPurposes[k]) ||
                        resource::canPay(s, p, r.energy[k] + 1, 0, Domain::Count, kPurposes[k]))
                        problem(where + ": spendable Energy is not the payment's bound");
                    if (!resource::canPay(s, p, 0, r.power_any[k], Domain::Count, kPurposes[k]) ||
                        resource::canPay(s, p, 0, r.power_any[k] + 1, Domain::Count, kPurposes[k]))
                        problem(where + ": spendable Power is not the payment's bound");
                }
            }
            // The timing window against what is offered.
            const TimingFacts t = view.timing();
            auto offers = [&](IntentType ty) {
                return std::any_of(step.legal.begin(), step.legal.end(),
                                   [ty](const Intent& i) { return i.type == ty; });
            };
            if (offers(IntentType::EndTurn) &&
                !(t.ns == NeutralShowdownState::Neutral && t.oc == OpenClosedState::Open &&
                  t.turn_player == decider))
                problem(where + ": End Turn offered outside the turn player's Neutral Open State");
            if (offers(IntentType::PassFocus) &&
                !(t.ns == NeutralShowdownState::Showdown && t.focus_holder == decider))
                problem(where + ": Focus offered to who does not hold it in a showdown");
            if (offers(IntentType::PassPriority) && t.priority_holder != std::optional<PlayerId>{decider})
                problem(where + ": priority offered to who does not hold it");
            // The opponent's legal pool covers their true unseen cards.
            if (n % 5 == 0) {
                ++pools;
                const PlayerId opp = opponent(decider);
                const auto pool = view.opponentCardPool(*model_);
                std::map<std::string, int> room;
                std::map<CardDefId, bool> listed;
                for (const auto& c : pool) {
                    listed[c.card] = true;
                    room[db_->get(c.card).name] = c.copies;
                }
                std::map<std::string, int> unseen;
                for (const auto& [id, o] : s.objects) {
                    if (o.owner != opp || o.is_token || o.card_def_id == kInvalidId) continue;
                    const CardDef& d = db_->get(o.card_def_id);
                    if (!isMainDeckType(d)) continue;
                    if (view.knowledge().about(id).identity_known) continue;
                    if (!listed.count(o.card_def_id))
                        problem(where + ": an unseen card of theirs (" + d.name + ") is not in their pool");
                    ++unseen[d.name];
                }
                for (const auto& [name, k] : unseen)
                    if (room.count(name) && room[name] >= 0 && k > room[name])
                        problem(where + ": more unseen copies of " + name + " than the pool allows");
            }
        });
    }
    for (const auto& p : problems) ADD_FAILURE() << p;
    std::cout << "[ observer view, derived facts: " << games << " games, " << decisions
              << " decisions, " << units << " units (" << lethal << " lethal, " << designated
              << " in combat), " << combats << " combats, " << pools << " pools checked ]\n";
    EXPECT_GT(designated, 0);
}

}  // namespace riftbound::test::observer_view
