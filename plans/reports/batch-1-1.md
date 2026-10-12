# batch-1 — interim report 1: verdicts and mechanism design (before the large changes)

Branch `fix/batch-1` from master `4a4cac4`. Nothing is changed yet. Baseline built on lane B
(`-j3`, `CCACHE_NAMESPACE=batch-1`, `-DRIFTBOUND_TEST_OPT_LEVEL=0`).

Baseline smoke (`fiora` vs `yi_rebuild`, 200 games, seed 20261010, 6 threads): **P1 138 / P2 62 / 0 draws /
17 turns / 295 decisions**, rc 0. The baseline binary is kept aside for before/after runs.

Replays: the cited games of D1, D4, D6, D7, D10, D19 were re-run and show the quoted lines exactly. The
findings document's citations were checked mechanically by its author; the remaining verdicts below rest on
those citations, the code and the rule quoted.

## 1. Verdicts

All twenty are defects. None needs a judge. Two places where the rule is narrower than the finding are noted.

| id | verdict | rule the behaviour breaks | where in the code |
|---|---|---|---|
| F1 | DEFECT (card + engine) | card: CR 350.2 "Tokens are not cards" — "when you play a card" does not see a token. Engine: CLAUDE.md "engine bounds throw" | Viktor 117 registers `WhenYouPlayAUnit` / `AGear`, which `TokenCreatedEvent` also drives. `ChainManager::processFEPR` -> `abandonChain` warns and plays on |
| F2 | DEFECT | finalize contract (`Card::finalizingChoices`): a return before the marker that is not a helper's "nothing to decide" | `AlphaStrike::onResolve`: `if (targets.empty() ...) return;` first |
| D1 | DEFECT | CR 383.4.a.2 (play effects go on the chain "after the Permanent ... is finalized and enters the board"); 355.1.a (the choice to pay an optional additional cost is made in step 2, before the cost is paid) | `executePlayCard` / `executePlayFromHidden` emit `CardPlayedEvent` before `addPermanent`; Accelerate is asked in `resolvePermanent`; `maybePayOptionalAdditionalCost` runs after `payCardCost` |
| D4 | DEFECT | CR 403.1.a / 383.3.a.2 (declined "may": removed, "considered to have not triggered"); 405.2 / 383.3.b.1 (cost not paid: removed, never finalized); **403.4** ("not enough options to make legal choices for a Triggered Ability ... remove it from the Chain now") | `stepFinalize` keeps every item whatever its finalize pass stored |
| D6 | DEFECT | CR 331.1 / 338.1.a.1; and an offered option that is dropped | `oc_state` is set Open with an item on the chain (`runShowdown`, `advanceShowdown`, `mainPhase`), so `generateSpellActions` takes its `isNeutralOpen()` branch at chain priority; `stepExecuteAndPass` has no branch for `PlayCard` and loops |
| D7 | DEFECT | CR 334.2, 335.1 (Focus only with "no pending Chain Items"), 336 | triggers queued by `cleanup()`, by `ShowdownStartedEvent`, by a conquer are not run before the next Focus decision / the damage step |
| D10 | DEFECT | CR 319.5 / 319.6 + 323.6 (no Open-State condition, unlike 323.5) | no cleanup inside `processFEPR` |
| D19 | DEFECT | CR 383.2.a.1 (Sona example: "will only be placed on the chain if she is located at a battlefield when the Condition is fulfilled. If she is removed in reaction ... it will still resolve") | Astral Heron tests `isAtBattlefield()` in `onTrigger` |
| D27 | DEFECT | CR 383.2.a.1, 383.3.e.1 ("already been performed that many times, it does not trigger"); Edge of Night: "from face down" is part of the event | Pyke, Returned / Edge of Night test in `onTrigger` |
| D29 | DEFECT | CR 344.1, 466.2.b ("start of showdown effects happen simultaneously") | `ShowdownStartedEvent` only in `runShowdown` |
| D33 | DEFECT (both halves) | CR 383.2.a.1 and its Sona example | Vex, Apathetic tests "at a battlefield" in `onTrigger` only |
| D35 | DEFECT | CR 709 with 319.3-319.5 (a cleanup after each chain step), 317.2 (Expiration Step is a cleanup with insertions), 335 (no phase is left with pending items) | "became_mighty" is announced by `GameEngine::cleanup`, which runs only at top level; triggers it queues wait for the next `runChain` |
| D36 | DEFECT | CR 709; 319.4 (cleanup after an item is finalized: the cost paid there is seen) | same; the dip at finalization and the rise at resolution fall between two cleanups |
| D39 | DEFECT | CR 354 (step 1 puts the card on the chain) before 357 (costs); 359.3.b | `executePlaySpell` pays additional costs before `addSpell`, so the trigger is the older item |
| D40 | DEFECT | CR 383.3.d, 383.3.d.1; 466.2.d.1 for a combat | no ordering anywhere; order is `TriggerManager`'s walk |
| D44 | DEFECT | CR 319.5 + 323.4 | no cleanup inside `processFEPR` |
| D51 | DEFECT | CR 383.2.a.1, 383.4.e.2.b / f.2.b | Kha'Zix, Evelynn, Tornado Warrior test in `onTrigger` |
| D53 | DEFECT | CR 816.1 ("Temporary is a Triggered Ability keyword"), 383.3.d | `beginningStep` kills directly |
| D68 | DEFECT | CR 383.4.e / .f ("gains the designation for the first time during a combat"), 466.2.c.3.a, 323.2.a | attack / defend triggers fire only from `CombatStartedEvent` |
| D77 | DEFECT | CR 383.1 ("the [Nth] time" is the condition), 383.2.a.1 | Yasuo counts moves in `onTrigger`; Fiora, Peerless tests "one on one" there |

Narrower than the finding:

- **D4, "nothing to choose".** The rule (403.4) removes an ability when a *required* choice has no legal
  option. An ability whose effect simply does nothing ("deal 2 to all enemy units here" with none there)
  stays, is finalized and gives priority (358.3.a). So removal is tied to what the choice helpers stored, not
  to "the resolution would do nothing".
- **D36, inside one resolution.** A unit that goes below 5 and back to 5+ inside a single resolution (no
  cleanup can run there, CR 321) is still not seen. The findings' cases (cost at finalization, effect at
  resolution) are covered. Exact edge detection would have to live in `recomputeMight`, where the aura pass
  (which zeroes and re-adds bonuses) produces false edges. I will pin this limit with a test and say so at the
  code site, unless you want the exact version.

Not raised to a judge, decided from the text: reader 2's question 4 (Guards!: Weaponmaster of the token against
the reflexive "pay [C] to ready it"). They are not simultaneous — the token is played, then "Then do this"
creates the reflexive item (CR 389.2 "creates them all in order") — so 383.3.d gives no choice; the present
order stays and gets a test.

## 2. Mechanisms

Seven, as you listed, plus the two fuzz items. Header edits (`game_state.h`: three `ChainItem` fields, three
`ChainState` counters; `chain_manager.h`; `card.h`: one bit) go in one rebuild.

### M-A  One loop for "handle outstanding tasks, then FEPR" (D44, D10, D35, D36, D7, D6; part of D68)

`ChainManager::processFEPR` becomes the HOT-FEPR loop of CR 334:

```
loop:
    handle_tasks()            // GameEngine::cleanup, injected  (CR 319.3-319.5, 334)
    stop if the game is over or the chain is empty
    oc_state = Closed         // CR 331.1: a chain exists
    if a pending item exists: order simultaneous triggers (M-E), finalize the OLDEST one, loop
    priority round; an item added -> loop
    resolve the newest item, loop
```

- One item is finalized per pass, so a cleanup follows each finalization (319.4: a cost paid there — a buff
  spent, a unit killed — is seen before anything else) and each resolution (319.5). Lethal damage kills, the
  recall of unattached Equipment, Might recomputation and "became Mighty" all happen there, and what they
  trigger is pending for the next pass.
- The loop ends only when a cleanup leaves the chain empty, so "the chain is settled" and "the cleanup is
  done" are one condition. `runChain()` therefore replaces every `if (chainExists()) runChain(); cleanup();`
  pair. It is called: after an intent, at the end of every phase function (Awaken, Beginning, Scoring per hold
  as today, Channel, Draw, Ending), in the Expiration Step after its body (317.2: the body is the special
  cleanup's insertion; 317.2.f loops), after `ShowdownStartedEvent`, after each showdown action, after the
  combat showdown closes and before damage, and where `combatResolutionStep` drains today.
- Nested calls do nothing: inside a card played in response (as today) and while an item is resolving
  (CR 321, 354.3).
- A win is found by the cleanup after the item that scored the point, not when the chain has emptied.
- D6 at the root: the state is Closed whenever the chain exists, so the generator's Closed branch is taken.
  In `stepExecuteAndPass` an intent type with no branch throws `std::logic_error`. The existing "a
  PlayReaction that added no item is dropped and not offered again" path: I will count how often it is taken
  over the pairing run first; if never, it throws too; if it is taken, each cause is a generator defect and I
  report them (they would be batch 3's D8 kind) rather than make 2,000 games fail on another batch's item.

Cost: a cleanup per chain step. A random game is 1.4 ms today; I will report games/s before and after.

### M-B  Removal at finalization (D4)

After a triggered ability's finalize pass (a `kFinalizeTrigger` card, or a reflexive trigger), the item leaves
the chain — no `ChainItemFinalizedEvent`, no priority, trace line `REMOVED: ...` — when either

1. `may_answer == 0`: the leading "may" was declined, not offered (cost not payable) or invalidated
   (403.1.a, 405.2); or
2. a helper recorded that a **required** choice had no legal option (403.4): `pickTarget` / `pickTargetPair` /
   `selectObject` with an empty list, `pickMode` with no legal mode, `splitDamageAmong` without
   `may_choose_none`. New bit in `ChainItem::finalize_checked`, set next to `nothingToDecide`.

Not removed: a spell (359.3.e.1: it resolves), an activated ability (its cost is paid), a mistargeted ability
(it was finalized with a legal target), an ability whose optional choice chose nothing.

Guard against a card that uses `confirmOptional` for a "may" that is not the leading one, or `pickTarget` for
something that is not a required choice: the registry sweep (`SweepCostsTest.CardContract_...`) gets a second
pass that, for every case the engine would remove, resolves the item instead on a copy and compares the
state. A card whose removed ability would have changed anything is listed and read by hand; the expected list
is in the test.

If the removed item was the only one, the chain is empty after the pass: no Closed State, nobody is asked.

### M-C  Conditions at the trigger event (D19, D27, D33, D51, D77)

`Card::triggerCondition` already exists and `fireTrigger` already applies it. The named cards move their
printed condition there and stop re-testing it as they resolve (the Sona example). A script over all card
files lists 73 cards with an intervening condition in their text ("When ..., if ...", "while I'm ...", "the
Nth time", "once each turn", "from face down", "on an opponent's turn", "one on one", "alone") and no
`triggerCondition`; each is read and either converted or shown to test it in the right place already (a
condition inside a helper's `still_legal` ends in M-B's removal, which is the same outcome). "Once each turn"
abilities: not triggered when already performed this turn, nor while an instance from the same source is
still on the chain (383.3.e.1); a helper in `card_helpers.h`.

`GameObject::moves_this_turn` already counts moves as they happen: Yasuo's "third time" reads it.

### M-D  Late joiners of a combat (D68)

`processContestedBattlefields` is where CR 323.2.a gives a designation to a unit that arrived during a
combat. It announces it (`ObjectStateChangedEvent "joined_combat"`); `TriggerManager` fires that unit's
WhenIAttack / WhenIDefend / WhenIAttackOrDefend (own, equipped, and "an enemy unit attacks a battlefield you
control"), once per unit per combat (383.4.e.2.a: a mark on the unit, set for everyone present at
`CombatStartedEvent`, cleared at `CombatEndedEvent`). With M-A the cleanup runs right after the unit enters, so
the trigger is pending before anyone has priority again.

### M-E  Order of simultaneous triggers (D40, part of D53)

- "Simultaneous" needs an identity. `TriggerManager` stamps each triggered item with a batch number: one per
  event it handles, or one held open across events that the rules call simultaneous — the units of a death
  batch, the units one cleanup finds newly Mighty, combat start with showdown start (466.2.b), the units that
  join a combat in one cleanup. Reflexive triggers are not stamped (389.2: created in order).
- When the oldest pending item belongs to a batch that has not been ordered: the turn player's items go first
  (lowest), then the other player's (383.3.d.1); in a combat-start batch the attacker's go first (466.2.d.1).
  A player with two or more items in the batch that are not the same ability is asked, one prompt per position
  but the last: `MakeChoice`, `chosen_value` = index, label "<source>: <trigger> — on the chain next (resolves
  after the ones chosen later)". Identical items (the same ability twice, e.g. a doubled Deathknell) ask
  nothing. First option = today's order.
- Existing action-id mechanism only (`MakeChoice` value slots); no vocabulary change. The test fixture's
  pickers take the first option of an order prompt unless a test scripts it, and count them.

### M-F  A permanent's play (D1, D39)

- Order in `executePlayCard`: choices (location is in the intent; Accelerate and the card's optional additional
  cost are asked here, each only if the whole — base cost plus the additional one — is payable) -> total cost ->
  pay -> the permanent's item goes on the chain **below** anything its payment triggered (D39's rule, same code
  as for spells) -> it is finalized and enters (ready if Accelerate was paid, 805.2.b) -> **then**
  `CardPlayedEvent` and "played not from hand": the play abilities are queued with the unit on the board.
  `executePlayFromHidden` likewise. Effect plays (`EffectExecutor::playIgnoringCost` and friends) already
  announce after the permanent has entered and are not touched; Accelerate for them is D48 / D58 (batch 4).
- `executePlaySpell` / `finishPlaySpell`: the spell's item is placed below the items that were queued since
  the play began (D39).

### M-G  [Temporary] as a keyword triggered ability (D53)

`TriggerManager::onPhaseChanged(BeginningStep)` queues, for each of the turn player's permanents with
[Temporary] (once per permanent, 816.2; an attached Equipment's printed instance is Inactive, a granted one is
not — today's gear rule, moved), a chain item with `keyword_ability = Temporary`, in the same batch as the
"At the start of your Beginning Phase" abilities, so M-E lets the controller order them (kill a Reflection for
Dusk Rose Lab first). `keywordAbilityCard(Temporary)` kills the source if it is still on the board. The loop in
`beginningStep` goes; its comment ("each kill is its own game action") stays true and moves to the ability.
Side effect on another batch's item: LeBlanc, Everywhere at Once already suppresses triggers of [Temporary]
units at her battlefield through `temporaryTriggerSuppressed`, which the keyword path consults — so the kill
itself is now what does not trigger there (first half of D55). The second half (the unit's other abilities are
suppressed too) is batch 4's; I leave the box.

### F1, F2

- F1: Viktor fires on a spell (resolved), a unit card and a gear card, not on a token
  (`triggerCondition`: the subject is not a token) and only on an opponent's turn (condition, not resolution).
  `processFEPR`'s bound throws `std::logic_error` naming the top item; `abandonChain` goes. Other bounds that
  end quietly instead of throwing, to be listed with a recommendation each (not all are errors — `kMaxTurns`
  is a draw by design): `kMainPhaseMaxActions` 500, `kMaxShowdownActions` 100, `kMaxPriorityPasses` 10,
  `kMaxExpirationPasses` 8, `kMaxCleanupChains` 16, `kMaxCleanupPasses` 20, `kMaxCostOnlyActivations` 512.
- F2: Alpha Strike takes its friendly unit from the play intent; with none (effect play, new choices) it picks
  it through `pickTarget` inside the pass. The registry sweep plays every finalize-pass spell also through
  `EffectExecutor::playIgnoringCost` and after `makeNewChoicesFor`; every card that trips is fixed.

## 3. What will move

- Decisions per game go **down** where declined / unpayable / target-less triggers stop opening priority
  rounds (2,249 such rounds in the 720 read games) and where conditions stop putting empty items on the
  chain; **up** by the order prompts (M-E) and by priority rounds for triggers that used to be skipped
  (Diana in combat, late joiners, [Temporary]).
- Timing: deaths, recalls and "becomes Mighty" move earlier (between chain items); play abilities move after
  the unit has entered; a win is declared as soon as the point is scored.
- I will report the smoke after each mechanism, in this order: M-A, M-B, M-C, M-D + D29, M-F, M-G, M-E.

## 4. Questions for you (I continue on the stated assumptions meanwhile)

1. D36 inside a single resolution: pin the limit (assumed), or exact edge detection?
2. M-A "dropped play" path: throw only if the pairing run never takes it (assumed)?
3. M-B applies the strict reading of 403.4: an ability with a required choice and no option is removed even
   when it has other instructions ("attach it to a unit here" with no unit, but also a hypothetical "stun a
   unit, then draw 1"). The sweep lists every registered card this changes the outcome for.
