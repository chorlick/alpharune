# riftbound — Web UI API

When the `riftbound` binary runs a single game with any seat set to
`human` (or with `--web on`), it exposes a Boost.Beast HTTP + WebSocket
server on `127.0.0.1:8080` so a human can drive their seat from a
browser. Same binary, same flags as the CLI / batch modes — `--web`
controls whether the server starts.

This document is the wire-protocol reference for that server. The
browser frontend embedded in the binary (`src/io/play_index_html.cpp`)
is the reference consumer, but any tool can speak this protocol.

---

## CLI

```text
riftbound
    --deck1 PATH         # P1 deck list, .txt (required)
    --deck2 PATH         # P2 deck list, .txt (required)
    --agent1 SPEC        # random (default) | human | mcts:sims=N | ismcts:sims=N
    --agent2 SPEC        # random (default) | human | mcts:sims=N | ismcts:sims=N
    --web auto|on|off    # auto (default): ON iff any seat is human
    --port N             # default: 8080
    --bind ADDR          # default: 127.0.0.1 (loopback only)
    --god-mode auto|on|off     # auto: ON iff any seat is human
    --render-html auto|on|off  # auto: ON iff any seat is human
    --replay-dir PATH    # default: ./replays (a <timestamp>/ directory per run)
    --debug / --trace    # print engine events to stdout
    --games N            # > 1 runs via BatchRunner; no human seats, no web UI
    --threads N          # parallel workers for batch mode
    --seed N             # 0 = random, otherwise deterministic
    --wild               # allow banned cards in decks
    --registry PATH      # deprecated/ignored (card data is compiled into the engine)
    --show-hand          # name facedown cards in renders; show every hand in the UI
```

The defaults are random vs random, one game, no web UI. A value other
than `auto`, `on` or `off` for `--web`, `--god-mode` or `--render-html`
is an error, as is `--web on` with `--games` above 1.

**Auto modes.** When at least one seat is `human`, `--web`,
`--god-mode` and `--render-html` all auto-enable, and `--debug` +
`--trace` are implied (streamed to the UI's log panel and the replay,
not to stdout). The HTML replay lands at
`<replay-dir>/YYYYMMDD-HHMMSS/replay.html`.

---

## Connection

| Path  | Method | Behaviour                                              |
|-------|--------|--------------------------------------------------------|
| `/`   | GET    | Serves the embedded single-page UI                     |
| `/ws` | GET    | WebSocket upgrade — bidirectional JSON message channel |

Every request is checked before it is served:

* **Host.** The `Host` header must name the server by IP literal or as
  `localhost` (any port). This is what stops a DNS-rebinding page.
* **Origin** (WebSocket upgrade only). If the request has an `Origin`
  header it must be `http://<Host>` — the page this server served.
  Another site's page open in the same browser cannot connect.
  Non-browser clients send no `Origin` and are accepted.
* **Access token.** When the server is bound to a non-loopback address
  (`--bind 0.0.0.0`), it generates a random token and prints the URL as
  `http://<addr>:<port>/?token=<token>`. Both `GET /` and the upgrade
  (`/ws?token=<token>`) are refused with 403 without it. On loopback
  there is no token.

Requests that fail a check get `403 Forbidden`.

The token is the only authentication, and it travels in the URL over
plain HTTP. Anyone who has it can play the game and, with god mode on,
edit its state. Bind to something other than loopback only on a network
you trust.

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
  "engine_busy": false,
  "legal": [
    {"index": 0, "label": "End Turn",                "type": "EndTurn"},
    {"index": 1, "label": "Play Miss Fortune → BF1", "type": "PlayCard", "card_object_id": 42}
  ],
  "zones": {
    "P1": {
      "hand":       [{"id": 9, "name": "Miss Fortune", "exhausted": false, "damage": 0,
                      "might": 3, "controller": "P1", "owner": "P1"}],
      "main_deck":  [...],
      "rune_deck":  [...],
      "trash":      [...],
      "banishment": [...],
      "base":       [...],
      "hand_count": 1,
      "main_deck_count": 33,
      "rune_deck_count": 10,
      "hidden": [],
      "score": 0,
      "energy": 2,
      "power_total": 1
    },
    "P2": { /* mirror */ }
  },
  "battlefields": [
    {"id": 0, "name": "Vilemaw's Lair (P1's)", "controller": "none",
     "contributed_by": "P1", "is_token": false, "units": [ /* objects, as in zones */ ]}
  ],
  "turn": {
    "phase": 7,                       // TurnPhase int (see src/core/types.h)
    "phase_name": "Main",
    "turn_player": "P1",
    "turn_number": 3,
    "p1_score": 0,
    "p2_score": 0,
    "victory_score": 8
  },
  "game_over": false
}
```

* `decision_id` is monotonic per `HumanAgent` and bumps on every new
  decision. Echo it back in `choose`: the server applies a `choose`
  only to the decision it names.
* `legal[].index` is the index into the engine's legal-action vector
  for the decision currently active. Always send this exact integer
  back; legality is engine-determined.
* `legal[].label` is rendered by `StateRenderer::renderChosenAction` —
  human-readable, e.g. "Play Miss Fortune → Battlefield 1" rather
  than an opaque enum.
* `legal[].type` is the action type stem ("PlayCard", "EndTurn", etc.)
  for client-side grouping.
* `zones.*` is included on every frame; the UI consumes it to populate
  the god-mode side panel. `base` lists the objects at that player's
  base; objects at a battlefield are under `battlefields[].units`. Each
  object carries its `controller` and `owner`.
* **Hidden information.** With god mode on, with no human seat, or with
  `--show-hand`, a frame carries everything. Otherwise it carries only
  what the human seat may see: that seat's own hand (in hot-seat play,
  the hand of whoever is deciding), and for the rest a size. A withheld
  zone is an empty array and is named in `hidden`
  (`"hand"`, `"main_deck"`, `"rune_deck"`); `hand_count`,
  `main_deck_count` and `rune_deck_count` are always present.
  `render_text` follows the same rule (`Hand:  (4 cards, hidden)`).
  The `log` stream is withheld entirely in that case: no `log`
  message is sent to any client. Engine log lines name cards as they
  are drawn, mulliganed, hidden or looked at, and carry no marker of
  who may see them, so none are forwarded. (The HTML replay on disk
  still has the full trace.)
* `battlefields[].name` is disambiguated when both players brought the
  same battlefield. `controller` / `contributed_by` are `"P1"`, `"P2"`
  or `"none"`.
* `render_text` is the ASCII board rendering identical to the HTML
  replay's main panel.
* `engine_busy` is true when the engine is running rather than waiting
  (see `state` below).
* `winner` (`"P1"` / `"P2"` / `"none"`) and `game_over_reason` are
  present when `game_over` is true.
* `prompt` (optional) — present for mid-resolution choices (discard,
  modal "choose one", variable-X, target picks). It's the human-readable
  question captured from the card's `*_PROMPT` trace line; the UI shows
  it as a header above the option buttons. It is on every frame of the
  decision it was asked for and on no other. For these `MakeChoice`
  decisions, `legal[].label` carries a meaningful option name (`Yes`,
  `No`, a mode label, an `X` value) rather than a bare index.
* `spectator` — present in spectator mode; see the example at the end.

### `state`

Same shape as `decision` but `type = "state"` and the `legal` array is
empty. Sent when:

* A client connects (so the UI immediately shows current state without
  waiting for the next decision).
* A human decision is consumed and the engine is now busy resolving /
  the AI opponent is thinking. This lets the UI clear the just-clicked
  action buttons and show an "engine is processing…" state instead of
  leaving stale, re-clickable buttons.
* An AI seat starts choosing (the board as it stands at that moment).
* The game terminates (the final frame has `game_over: true`; it is
  written to every client before the server shuts down).

While the engine is running (`engine_busy: true`) the server does not
read the game state: the board in such a frame is the most recent
snapshot, taken the last time the engine waited or an AI seat began to
think. It can therefore be one action behind until the next frame.

### `log`

One engine log line, streamed as it is emitted:

```json
{"type": "log", "level": "TRC", "msg": "PHASE: MainPhase"}
```

`level` is `TRC`, `DBG`, `INF` or `WRN`.

Sent only when frames carry everything (god mode on, no human seat, or
`--show-hand`). With hidden information withheld — a human seat and
`--god-mode off` — the server sends no `log` messages at all; see
"Hidden information" above.

### `edit_ok` / `edit_err`

Sent in reply to any `edit_*` message from the client:

```json
{"type": "edit_ok", "legal_recomputed": true}
{"type": "edit_err", "msg": "object not found: 137"}
```

`legal_recomputed` says whether the pending decision's legal actions
were rebuilt from the edited state (see "God-mode edits" below).

A `decision` / `state` frame is broadcast immediately afterwards so the
UI updates even if the edit reply is dropped.

### `err`

Any other refusal — a `choose` that was not applied, an unknown message
type, malformed JSON, a field of the wrong type:

```json
{"type": "err", "msg": "unknown message type: foo"}
{"type": "err", "msg": "stale choose: decision 6 is no longer pending (now 7)"}
```

No message a client sends can stop the server; the worst outcome is an
`err` reply.

---

## Client → Server messages

### `choose` — pick a legal action

```json
{"type": "choose", "player": "P1", "index": 1, "decision_id": 7}
```

* `player` and `decision_id` are those of the `decision` frame being
  answered.
* `index` is the engine's legal-action index (echoed from `legal[].index`).

A `choose` is applied only if `player` is a human seat that is waiting
on exactly decision `decision_id` and `index` is one of its legal
actions. Anything else — a missing or old `decision_id`, an index out
of range, a second click on a decision already answered, a click while
the engine is busy — is refused with an `err` reply and changes
nothing; the current frame is sent again so the client can re-render.

### `request_state` — ask for an out-of-band state push

```json
{"type": "request_state"}
```

Triggers a `state` frame. Useful for clients that connect mid-game.

### `edit_*` — god-mode mutations  *(gated)*

All `edit_*` messages require `god_mode_enabled = true` on the server:
by default when at least one seat is `human`, overridden either way by
`--god-mode on|off`. Otherwise every `edit_*` is rejected with
`{"type": "edit_err", "msg": "god mode disabled"}`.

Edits are applied only while the engine is waiting — at a human
decision, at the spectator gate, or after the game. While it is running
they are refused with `edit_err` ("engine is busy…"); send the edit
again when the next `decision` frame arrives.

Edits bypass engine validation. They can leave the game in a state
the engine cannot reach via normal play (e.g., a phase out of
sequence). Use them deliberately for testing, reproduction setups, or
"I changed my mind" rewinds.

An edit made while a human seat is at a decision whose options depend
on the state alone — its **main-phase decision**, a **showdown** (focus)
decision or a **chain priority** decision — rebuilds that decision's
legal actions from the edited state. If the list changed, `edit_ok`
carries `"legal_recomputed": true` and the `decision` frame that follows
has a new `decision_id` and the new list; a `choose` sent for the old id
is refused as stale. (The engine checks the answer to these three
decisions against the rebuilt list, so the old list must not stay
clickable.) At any other decision (mulligan, combat damage, a cost
payment, a card's prompt), when the list did not change, and at the
spectator gate (where the AI seat has already chosen), the list is the
one computed before the edit and `edit_ok` carries
`"legal_recomputed": false`: at those other decisions an action in it
may no longer make sense.

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
`Banishment`, `ChampionZone`, `LegendZone`, `Base`, `BattlefieldZone`.
For `BattlefieldZone`, add a `battlefield_id` field; the server routes
through `moveObjectToBattlefield` and `to_player` is ignored. Any other
`to_zone` is an error — including `Chain` and `FacedownZone`, which
need more than an object id to be consistent. A card that is on the
chain cannot be moved until it resolves.

The object leaves whatever held it (zone list, a battlefield's facedown
list, the unit it was attached to). Moving to `Base` gives `to_player`
control of it; moving it off the board makes `to_player` its owner too.
The UI sends the object's own `controller` / `owner`. Gear attached to
a unit follows it around the board and is left unattached at its
controller's base when the unit leaves the board.

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

`phase` value is the `TurnPhase` integer (see `src/core/types.h`);
values outside the enum are refused.
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
| Game thread  | Drives `GameEngine::runGame`. Blocks at each decision inside the parked `HumanAgent::selectAction` (or, spectating, in `PlayServer::awaitSpectatorStep`). |
| IO thread    | `asio::io_context::run` — handles HTTP accept + WS read/write.    |
| Watcher      | Polls for a human seat reaching a new decision and broadcasts its `decision` frame. |

The engine thread mutates `GameState` without taking a lock. The
server therefore touches `GameState` — to build a frame or to apply a
god-mode edit — only while the engine is *parked*, and holds the state
mutex (`PlayServer::stateMutex`) for as long as it does:

* at a human decision, the engine is woken only by a `choose`, which
  the server applies under the same mutex;
* at the spectator gate, the engine thread takes the mutex before it
  moves on;
* after the game, `PlayServer::notifyGameFinished` says the engine
  thread is done.

When none of these holds, frames are built from the last snapshot and
edits are refused. The engine thread itself publishes a fresh snapshot
when an AI seat is about to think (`PlayServer::publishEngineState`).

The `AgentInterface` contract has no edit surface. `RandomAgent` and
any future model-based agent cannot reach `StateEditor`. God-mode is
a property of the UI binary, not the engine.

---

## Examples

### Pure spectator (both seats AI)

```bash
./build/riftbound --agent1 random --agent2 random --web on \
    --deck1 decks/fiora.txt --deck2 decks/yi_rebuild.txt
```

`--web on` is needed: without a human seat the UI defaults to off.
The engine parks after every AI decision: `decision` frames carry a
`spectator` object (`waiting`, `autoplay`, `delay_ms`, and `chosen` — the
index into `legal` the agent picked; the board is still pre-resolution).
The browser advances with **Step** (Space / →) or toggles **Play** (P):

```json
{"type": "spectator_step"}
{"type": "spectator_autoplay", "on": true, "delay_ms": 600}
```

God mode is off unless `--god-mode on` is passed; frames carry every
hand. No replay unless `--render-html on`.

### Human vs random

```bash
./build/riftbound --agent1 human \
    --deck1 decks/fiora.txt --deck2 decks/yi_rebuild.txt
```

(`--agent2` defaults to `random`.) Open `http://127.0.0.1:8080` —
buttons for legal P1 actions appear when the engine is waiting on P1.
P2's actions resolve automatically. Replay auto-writes to
`./replays/<timestamp>/replay.html`. Add `--god-mode off` to play
without seeing P2's hand.

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
< {"type":"decision","decision_id":3,"player":"P1",...}
> {"type":"choose","player":"P1","index":0,"decision_id":3}
< {"type":"state",...}       # engine busy
< {"type":"decision",...}    # next decision frame for whoever acts next
```
