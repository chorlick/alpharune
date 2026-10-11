#pragma once
/// @file state_editor.h
/// God-mode state mutation primitives for the human-play web UI.
///
/// These operations bypass engine rule validation entirely. They are
/// surfaced to the user through the `riftbound_play` web UI (see
/// docs/play-api.md), and are NEVER available to any AgentInterface
/// implementation — the AgentInterface contract only sees `selectAction`,
/// which receives engine-generated legal actions.
///
/// Usage: caller must hold an external mutex covering GameState mutation
/// for the lifetime of any edit operation. The Beast WebSocket server in
/// `play_server.cpp` takes that lock; the engine releases its hold on
/// GameState at decision boundaries (it's idle inside HumanAgent
/// `selectAction`), so contention is structural-zero in normal flow.
///
/// All methods return EditResult — `ok=true` plus an updated state, or
/// `ok=false` with a descriptive `error` message that the UI surfaces
/// back to the user. The editor never throws or asserts on invalid input;
/// it returns errors. This makes UI mistakes recoverable without crashing
/// an active game.

#include "core/game_state.h"
#include "core/types.h"

#include <optional>
#include <string>
#include <vector>

namespace riftbound {

struct EditResult {
    bool ok = false;
    std::string error;

    static EditResult success() { return {true, ""}; }
    static EditResult fail(std::string msg) { return {false, std::move(msg)}; }
};

/// Stateless utility — every method takes a GameState& and mutates it.
/// The caller is responsible for locking.
class StateEditor {
public:
    // ── Zone moves ────────────────────────────────────────────────────────

    /// Move an object from its current zone to a target zone on a target
    /// player. Updates GameObject.zone, GameObject.location, and the
    /// player's zone vector membership. For deck moves, `to_top=true`
    /// inserts at the top (back of vector), `false` at the bottom.
    ///
    /// The object leaves whatever held it: a player zone vector, a
    /// battlefield's facedown list, or the unit it was attached to. Moving
    /// to `Base` changes the controller only (attached gear travels along);
    /// moving off the board makes `target_player` the owner as well, and
    /// gear attached to the moved unit is left unattached at its
    /// controller's base.
    ///
    /// Refused: `BattlefieldZone` (use moveObjectToBattlefield), `Chain` and
    /// `FacedownZone` as destinations, and any object currently on the
    /// chain.
    EditResult moveObject(GameState& state,
                          GameObjectId object_id,
                          PlayerId target_player,
                          ZoneType target_zone,
                          bool to_top = true);

    /// Move an object onto a specific battlefield. Owner/controller stay
    /// the same. Sets zone=BattlefieldZone and location=BattlefieldLocation.
    EditResult moveObjectToBattlefield(GameState& state,
                                       GameObjectId object_id,
                                       BattlefieldId battlefield_id);

    // ── Object field edits ────────────────────────────────────────────────

    /// Set GameObject.is_exhausted. Bypasses all cost/timing rules.
    EditResult setObjectExhausted(GameState& state,
                                  GameObjectId object_id,
                                  bool exhausted);

    /// Set GameObject.damage_marked. Negative values become 0.
    EditResult setObjectDamage(GameState& state,
                               GameObjectId object_id,
                               int damage);

    /// Set GameObject.current_might directly. Bypasses might layers; will
    /// be overwritten by next recomputeMight() in normal engine flow.
    EditResult setObjectMight(GameState& state,
                              GameObjectId object_id,
                              int might);

    // ── Player field edits ────────────────────────────────────────────────

    EditResult setPlayerScore(GameState& state,
                              PlayerId player,
                              int score);

    /// Set rune-pool energy (the single shared energy bucket).
    EditResult setPlayerEnergy(GameState& state,
                               PlayerId player,
                               int amount);

    /// Set rune-pool power for a specific domain.
    /// `domain_idx` indexes into RunePool.power[].
    /// Passing `domain_idx == -1` sets RunePool.universal_power (the
    /// [A] / any-domain bucket) instead.
    EditResult setPlayerPower(GameState& state,
                              PlayerId player,
                              int domain_idx,
                              int amount);

    // ── Turn structure edits ──────────────────────────────────────────────

    /// Jump to an arbitrary TurnPhase. RISKY — phase transitions normally
    /// run a sequence of cleanup + scoring + trigger steps. Bypassing
    /// those can leave the engine in an inconsistent state. Intended for
    /// debugging only. `phase` is the TurnPhase integer; values outside the
    /// enum are refused.
    EditResult setPhase(GameState& state, int phase);

    /// Change which player has turn priority.
    EditResult setTurnPlayer(GameState& state, PlayerId player);

    // ── Deck ordering ─────────────────────────────────────────────────────

    /// Replace the player's main deck with a specific ordering. The
    /// provided vector must contain the same set of GameObjectIds
    /// currently in the deck — same multiset, just reordered.
    EditResult reorderMainDeck(GameState& state,
                               PlayerId player,
                               const std::vector<GameObjectId>& new_order);

private:
    /// Remove object_id from whatever container currently lists it: a
    /// player zone vector, the champion / legend slot, or a battlefield's
    /// facedown list. Returns true if removed (false = not listed anywhere,
    /// which is expected for on-board objects since they are found by
    /// their location rather than through a container).
    bool removeFromZoneVector(GameState& state, GameObjectId object_id);

    /// True while the object is a spell on the chain.
    static bool isOnChain(const GameState& state, GameObjectId object_id);

    /// If `gear_id` is attached to a unit, take it off that unit.
    static void detachGear(GameState& state, GameObjectId gear_id);

    /// Unattach everything attached to `host_id`, leaving each piece of
    /// gear at its controller's base.
    static void dropAttachments(GameState& state, GameObjectId host_id);

    /// Insert object_id into the appropriate player zone vector for
    /// (target_player, target_zone). No-op for zones that aren't backed
    /// by a player vector (BattlefieldZone, Chain, etc.).
    void insertIntoZoneVector(GameState& state,
                              GameObjectId object_id,
                              PlayerId target_player,
                              ZoneType target_zone,
                              bool to_top);
};

} // namespace riftbound
