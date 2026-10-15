#pragma once
/// @file determinizer.h
/// Sampling a world consistent with what one player knows, for determinized
/// (information-set) search: `IsMctsAgent` searches each simulation in a world
/// drawn here instead of in the real game, so the search never reads a card
/// its player cannot see.
///
/// ── What is kept, what is drawn ──
///
/// A world is the true position with every hidden fact the observer cannot
/// know replaced by a draw from the sampler's RNG. **No object moves**: every
/// id stays in its slot (hand position, deck position, facedown pile, rune
/// deck position); only which card an id IS changes. Everything that refers
/// to an id therefore stays valid — the engine's suspended call stack, chain
/// items, both players' knowledge-tracker facts (which are positional) — and a
/// world can be drawn at any kind of decision, mid-chain included.
///
/// Read through `KnowledgeView` (knowledge_tracker.h) for the observer:
///   - Kept exactly: every public zone; the observer's hand and facedown
///     cards; any card the observer knows at an exact place (a revealed hand
///     card, a known deck slot, a facedown card it looked at, a card held by
///     its own look).
///   - Shuffled among their slots: members of a known unordered group (cards
///     recycled together, a mulligan's set-aside cards); the observer's other
///     Main Deck cards (composition known, order not); the observer's rune
///     deck (rune decks are not tracked: the observer then knows less, never
///     more).
///   - Placed at random: an opponent's card the observer knows by identity
///     but not by place (seen, then shuffled in: a random slot of its deck;
///     "maybe still in hand": a random hidden slot of its owner's).
///   - Drawn from the deck model: every other card of the opponent in a hand,
///     Main Deck, facedown pile or held by the opponent's look, and the
///     opponent's rune deck.
/// Every slot that is rewritten becomes a fresh instance of its new card
/// (`becomeCard`): nothing of the card that was there survives on it. The
/// draws depend only on the observer's knowledge and the RNG, never on the
/// true identities of the cards being replaced (`DeterminizerLeak*` tests).
///
/// ── The deck model (the opponent's unseen cards) ──
///
/// The opponent's decklist is not known. An unseen card is drawn uniformly
/// from the cards a constructed deck around the opponent's legend may hold
/// (`RandomDeckBuilder::legendCards`: the legend's Domain Identity, Signature
/// cards with its champion tag, no banned card, no token card), within the
/// copy limits counted over every card of theirs the observer has seen (3 of
/// a name, a [Unique] card 1, unlimited cards exempt, at most 3 Signature
/// cards): a random completion of a legal deck. A facedown card is drawn from
/// the [Hidden] cards of that pool (only those can be hidden). An unseen rune:
/// a Domain of the legend's, then a printing of it, as RandomDeckBuilder does.
/// Taking "the true deck's unseen cards, shuffled" instead would hand the
/// search the opponent's decklist.

#include "core/card_db.h"
#include "core/game_object.h"
#include "core/game_state.h"
#include "rules/deck_model.h"
#include "rules/random_deck.h"

#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace riftbound {

/// Rebuild `obj` as a fresh instance of `def` (what GameEngine::instantiateCard
/// makes), keeping its id, owner, controller and where it is (zone, location,
/// facedown status). Everything else — memory, last-known information,
/// statuses — is reset.
void becomeCard(GameObject& obj, const CardDef& def);

/// What one world draw did (for tests and diagnostics).
struct WorldDraw {
    int kept = 0;          // hidden slots kept as they are (the observer knows them)
    int shuffled = 0;      // slots refilled from a known multiset (own deck, groups, own runes)
    int placed = 0;        // known-but-unplaced cards put on a random slot
    int drawn = 0;         // opponent cards drawn from the deck model
    int runes_drawn = 0;   // opponent runes drawn
};

class WorldSampler {
public:
    explicit WorldSampler(const CardDB& db);

    /// Rewrite `world` — the true position — so that every fact `observer`
    /// cannot know is drawn from `rng` (see the file comment). The engine's
    /// RNG stream is not part of GameState: a caller that will play the world
    /// reseeds it too (GameEngine::editHiddenInformation).
    WorldDraw sample(GameState& world, PlayerId observer, std::mt19937_64& rng) const;

    /// Everything a world drawn for `observer` from `truth` must agree on,
    /// one line per disagreement (empty = consistent): the same objects in the
    /// same places; public cards, the observer's hand and facedown cards and
    /// every card the observer knows at an exact place unchanged; known groups
    /// and the observer's deck and rune deck holding the same cards; the
    /// opponent's cards the observer knows by identity present among their
    /// hidden cards; the opponent's drawn cards legal under the deck model
    /// (Domain Identity, copy limits, [Hidden] when facedown); and the
    /// world's knowledge tracker consistent (knowledge::check).
    std::vector<std::string> check(const GameState& truth, const GameState& world,
                                   PlayerId observer) const;

    /// The deck model it draws from (rules/deck_model.h), shared with the
    /// opponent's card pool an observer is shown (ObserverView, T6).
    const DeckModel& model() const { return model_; }

private:
    using LegendPool = DeckModel::LegendPool;
    const LegendPool& poolFor(CardDefId legend) const { return model_.poolFor(legend); }
    const CardDef* def(CardDefId id) const { return model_.def(id); }

    const CardDB& db_;
    DeckModel model_;
};

} // namespace riftbound
