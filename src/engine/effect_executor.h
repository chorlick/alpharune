#pragma once
/// @file effect_executor.h
/// Atomic game operation helpers used by Card objects.
///
/// Card objects call these methods to modify game state (deal damage,
/// draw cards, bounce units, etc.). This is a utility library, not a
/// dispatch mechanism — Card objects are the dispatch layer.

#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"

#include <functional>
#include <random>
#include <vector>

namespace riftbound {

class EffectExecutor {
public:
    EffectExecutor(GameState& state, EventBus& events, const CardDB& card_db);

    /// Set the RNG for effects that need randomness (reveal, recycle).
    void setRng(std::mt19937_64* rng) { rng_ = rng; }

    /// Set agent query callback for effects that require player choice (discard).
    using AgentChoiceQuery = std::function<Intent(PlayerId, const std::vector<Intent>&)>;
    void setAgentQuery(AgentChoiceQuery query) { agent_query_ = std::move(query); }

    // ── Atomic game operations ──
    void dealDamage(GameObjectId target, int amount, GameObjectId source);
    void killObject(GameObjectId target);
    void drawCards(PlayerId player, int count);
    void bounceToHand(GameObjectId target);
    void giveTemporaryMight(GameObjectId target, int amount);
    void giveTemporaryKeyword(GameObjectId target, Keyword kw, int value);
    void buffUnit(GameObjectId target);
    void readyObject(GameObjectId target);
    void moveToBase(GameObjectId target);
    void stunUnit(GameObjectId target);
    void discardCards(PlayerId player, int count);
    /// Make a specific player discard (for opponent targeting: "they discard 1").
    void opponentDiscards(PlayerId opponent, int count);
    void recycleCards(PlayerId player, const std::vector<GameObjectId>& cards);
    void banishObject(GameObjectId target);
    void healObject(GameObjectId target);
    void exhaustObject(GameObjectId target);
    void channelRunes(PlayerId player, int count, bool enter_exhausted = false);

    // ── Token creation ──
    /// Create a token game object and place it on the board.
    GameObjectId createToken(PlayerId controller, CardType type,
                             const std::string& name, int might,
                             const std::vector<std::string>& tags,
                             KeywordSet keywords, LocationId location,
                             bool enter_ready = false);

    // ── Copy effects ──
    /// Make a token become a copy of another unit (base traits only, not buffs/auras).
    void copyUnit(GameObjectId token, GameObjectId source);

    // ── Composite operations ──
    std::pair<GameObjectId, std::vector<GameObjectId>>
        revealUntil(PlayerId player, CardType condition);
    void playIgnoringCost(PlayerId player, GameObjectId card);

    /// Predict N: look at top N cards, agent chooses which to recycle (put back on bottom).
    void predict(PlayerId player, int count);

    /// Reveal top N and let agent choose: draw matching cards or recycle them.
    /// Returns list of cards the agent chose to draw/play.
    std::vector<GameObjectId> revealAndChoose(PlayerId player, int count);

private:
    GameState& state_;
    EventBus& events_;
    const CardDB& card_db_;
    std::mt19937_64* rng_ = nullptr;
    AgentChoiceQuery agent_query_;
};

} // namespace riftbound
