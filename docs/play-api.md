# riftbound — Web UI API

When the `riftbound` binary runs with any seat set to `human` (or with
`--web on`), it exposes a Boost.Beast HTTP + WebSocket server on
`127.0.0.1:8080` so a human can drive their seat from a browser. Same
binary, same flags as the CLI / batch modes — `--web` controls whether
the server starts.

This document is the wire-protocol reference for that server. The
browser frontend embedded in the binary (`src/io/play_index_html.h`) is
the reference consumer, but any tool can speak this protocol.

---

## CLI

```text
riftbound
    --deck1 PATH         # P1 deck JSON (required)
    --deck2 PATH         # P2 deck JSON (required)
    --agent1 SPEC        # random | human | mcts:sims=N | ismcts:sims=N
    --agent2 SPEC        # random | human | mcts:sims=N | ismcts:sims=N
    --web auto|on|off    # auto (default): ON iff any seat is human
    --port N             # default: 8080
    --render-html auto|on|off  # auto: ON iff any seat is human
    --replay-dir PATH    # default: ./replays/<timestamp>/replay.html
    --debug / --trace    # stream engine events into the UI log panel
    --games N            # > 1 runs via BatchRunner; no human seats allowed
    --threads N          # parallel workers for batch mode
    --seed N             # 0 = random, otherwise deterministic
    --registry PATH      # deprecated/ignored (card data is compiled into the engine)
    --show-hand          # show hand contents in render (debug aid)
```

**Auto modes.** When at least one seat is `human`, `--web` and
`--render-html` both auto-enable, and `--debug` + `--trace` are
implied. The HTML replay lands at
`<replay-dir>/YYYYMMDD-HHMMSS/replay.html` with trace + debug logging
piped into it.

---

## Connection

| Path  | Method | Behaviour                                              |
|-------|--------|--------------------------------------------------------|
| `/`   | GET    | Serves the embedded single-page UI                     |
| `/ws` | GET    | WebSocket upgrade — bidirectional JSON message channel |

There is no auth. The default bind is loopback only. Do not expose the
port to the network — `god_mode` edits below let any connected client
mutate the game state arbitrarily.

---

## Server → Client messages

Every WS frame is a single JSON object. The `type` field discriminates.

### `decision`

Pushed whenever a `HumanAgent` (the seat tied to either Player1 or
Player2) is parked in `selectAction` waiting for input.

```json
{
  "type": "decision",
  "decision_id": 7,
  "player": "P1",
  "render_text": "...big ASCII board state from StateRenderer...",
  "god_mode": true,
  "legal": [
    {"index": 0, "label": "End Turn",                "type": "EndTurn"},
    {"index": 1, "label": "Play Miss Fortune → BF1", "type": "PlayCard", "card_object_id": 42}
  ],
  "zones": {
    "P1": {
      "hand":       [{"id": 9, "name": "Miss Fortune", "exhausted": false, "damage": 0, "might": 3}],
      "main_deck":  [...],
      "rune_deck":  [...],
      "trash":      [...],
      "banishment": [...],
      "score": 0,
      "energy": 2,
      "power_total": 1
    },
    "P2": { /* mirror */ }
  },
  "turn": {
    "phase": 7,                       // TurnPhase int (see src/core/types.h)
    "turn_player": "P1",
    "turn_number": 3
  },
  "game_over": false
}
```

* `decision_id` is monotonic per `HumanAgent` and bumps on every new
  decision. Echo it back in `choose` to guard against stale clicks
  (server ignores mismatch silently — see note in next section).
* `legal[].index` is the index into the engine's legal-action vector
  for the decision currently active. Always send this exact integer
  back; legality is engine-determined.
* `legal[].label` is rendered by `StateRenderer::renderChosenAction` —
  human-readable, e.g. "Play Miss Fortune → Battlefield 1" rather
  than an opaque enum.
* `legal[].type` is the action type stem ("PlayCard", "EndTurn", etc.)
  for client-side grouping.
* `zones.*` is included on every frame; the UI consumes it to populate
  the god-mode side panel.
* `render_text` is the ASCII board rendering identical to the HTML
  replay's main panel.
* `prompt` (optional) — present for mid-resolution choices (discard,
  modal "choose one", variable-X, target picks). It's the human-readable
  question captured from the card's `*_PROMPT` trace line; the UI shows
  it as a header above the option buttons. For these `MakeChoice`
  decisions, `legal[].label` carries a meaningful option name (`Yes`,
  `No`, a mode label, an `X` value) rather than a bare index.

### `state`

Same shape as `decision` but `type = "state"` and the `legal` array is
empty. Sent when:

* A client connects (so the UI immediately shows current state without
  waiting for the next decision).
* A god-mode edit completes (re-render so the UI reflects the change).
* A human decision is consumed and the engine is now busy resolving /
  the AI opponent is thinking. This lets the UI clear the just-clicked
  action buttons and show an "engine is processing…" state instead of
  leaving stale, re-clickable buttons.
* The game terminates (the final frame has `game_over: true`).

### `edit_ok` / `edit_err`

Sent in reply to any `edit_*` message from the client:

```json
{"type": "edit_ok"}
{"type": "edit_err", "msg": "object not found: 137"}
```

A `state` frame is broadcast immediately afterwards so the UI updates
even if the edit reply is dropped.

### `err`

Generic protocol error (unknown message type, malformed JSON, etc.):

```json
{"type": "err", "msg": "unknown message type: foo"}
```

---

## Client → Server messages

### `choose` — pick a legal action

```json
{"type": "choose", "player": "P1", "index": 1}
```

* `player` should match the `player` of the most recent `decision`
  frame.
* `index` is the engine's legal-action index (echoed from `legal[].index`).

If `player` doesn't have a human seat, the server returns
`edit_err`. Out-of-range indices fall back to index 0 inside the
engine's `HumanAgent::selectAction` — preferable to crashing.

Stale clicks (older `decision_id`) are *not* explicitly rejected —
the engine is parked for *one* choice at a time. If the user clicks
twice for the same decision, only the first wakes the engine; the
second is overwritten in the pending-choice slot and never observed.

### `request_state` — ask for an out-of-band state push

```json
{"type": "request_state"}
```

Triggers a `state` frame. Useful for clients that connect mid-game.

### `edit_*` — god-mode mutations  *(gated)*

All `edit_*` messages require `god_mode_enabled = true` on the server.
Gating rule: enabled iff at least one seat is `human`. In pure-AI
spectator mode every `edit_*` is rejected with
`{"type": "edit_err", "msg": "god mode disabled (no human seat)"}`.

Edits bypass engine validation. They can leave the game in a state
the engine cannot reach via normal play (e.g., a card in two zones,
phase out of sequence). Use them deliberately for testing,
reproduction setups, or "I changed my mind" rewinds.

#### `edit_move` — move a card between zones

```json
{
  "type": "edit_move",
  "object_id": 42,
  "to_player": "P1",
  "to_zone":   "Hand",
  "to_top":    true
}
```

`to_zone` is one of `MainDeck`, `RuneDeck`, `Hand`, `Trash`,
`Banishment`, `ChampionZone`, `LegendZone`, `BattlefieldZone`,
`FacedownZone`, `Base`, `Chain` (see `ZoneType` in
`src/core/types.h`). For `BattlefieldZone`, add a `battlefield_id`
field; the server routes through `moveObjectToBattlefield`.

#### `edit_object` — set an object's field

```json
{"type": "edit_object", "object_id": 42, "field": "exhausted", "value": true}
{"type": "edit_object", "object_id": 42, "field": "damage",    "value": 2}
{"type": "edit_object", "object_id": 42, "field": "might",     "value": 5}
```

#### `edit_player` — set a player field

```json
{"type": "edit_player", "player": "P1", "field": "score",  "value": 4}
{"type": "edit_player", "player": "P1", "field": "energy", "value": 3}
{"type": "edit_player", "player": "P1", "field": "power",  "domain_idx": -1, "value": 2}
```

For `power`, `domain_idx` is the `Domain` enum value, or `-1` to set
the universal power bucket.

#### `edit_phase` — force a turn-state change

```json
{"type": "edit_phase", "field": "phase",       "value": 7}
{"type": "edit_phase", "field": "turn_player", "value": "P2"}
```

`phase` value is the `TurnPhase` integer (see `src/core/types.h`).
**Risky**: phase jumps bypass the cleanup / scoring / trigger sequence
the engine normally runs at phase transitions. Use only for
debugging.

#### `edit_reorder_deck` — replace the deck ordering

```json
{
  "type": "edit_reorder_deck",
  "player": "P1",
  "order":  [42, 17, 9, 3, ...]
}
```

`order` must be a permutation of the player's current `main_deck` — same
multiset of `GameObjectId`s, just reordered. Mismatches return
`edit_err`. Top of deck = end of array.

---

## Threading & safety

The binary runs three concurrent threads:

| Thread       | Responsibility                                                    |
|--------------|-------------------------------------------------------------------|
| Game thread  | Drives `GameEngine::runGame`. Blocks at each decision inside the parked `HumanAgent::selectAction`. |
| IO thread    | `asio::io_context::run` — handles HTTP accept + WS read/write.    |
| Watcher      | Polls `HumanAgent::atDecision()` and broadcasts `decision` frames when a new decision arrives. |

The state mutex (`PlayServer::stateMutex`) serialises god-mode edits
with snapshot reads. The engine thread does **not** take this lock —
it mutates `GameState` from its own thread, but edits only arrive
while the engine is parked in `selectAction` (waiting for the human),
which is exactly when the engine is *not* touching state. The
structural guarantee removes the need for engine-side locking.

The `AgentInterface` contract has no edit surface. `RandomAgent` and
any future model-based agent cannot reach `StateEditor`. God-mode is
a property of the UI binary, not the engine.

---

## Examples

### Pure spectator (both seats AI)

```bash
./build/riftbound --agent1 random --agent2 random \
    --deck1 decks/miss_fortune_test.txt --deck2 decks/miss_fortune_test.txt
```

The UI shows the game running. `god_mode_enabled = false`; edit
messages are rejected. No auto-replay (no human seat).

### Human vs random (default)

```bash
./build/riftbound \
    --deck1 decks/miss_fortune_test.txt --deck2 decks/miss_fortune_test.txt
```

Equivalent to `--agent1 human --agent2 random`. Open
`http://127.0.0.1:8080` — buttons for legal P1 actions appear when the
engine is waiting on P1. P2's actions resolve automatically. Replay
auto-writes to `./replays/<timestamp>/replay.html`.

### Hot-seat (both human, one browser, take turns)

```bash
./build/riftbound --agent1 human --agent2 human \
    --deck1 ... --deck2 ...
```

The UI's status line shows which player is currently to act. Each new
`decision` frame carries the correct `player` field; click the button
when it's your turn.

### Programmatic client (websocat)

```bash
websocat -t ws://127.0.0.1:8080/ws
> {"type":"request_state"}
< {"type":"state",...}
> {"type":"choose","player":"P1","index":0}
< {"type":"decision",...}    # next decision frame for whoever acts next
```
