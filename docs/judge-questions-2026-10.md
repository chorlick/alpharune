# Rules questions for judge review (2026-10)

Questions that came up while implementing cards against the 2026-10-09 Core Rules. Each gives the card text, the situation, the question, and the rules we found on either side. Rule numbers are from the Core Rules; "FAQ" is the Vendetta Rules FAQ and Clarifications.

---

## 1. Does "ready up to N runes" include an opponent's runes?

**Cards**

- Dark Child - Starter: "At the end of your turn, ready up to 2 runes."
- Targon's Peak: "When you conquer here, ready up to 2 runes at the end of this turn."
- Gentle Gemdragon: "When you play me or another Dragon, ready up to 2 runes."
- Curator of the Sands: "...you may exhaust me to ready up to 2 runes."
- Hwei, Brooding Painter: "Gear — Ready up to 2 runes."
- Aphelios, Exalted: "Ready 2 runes."
- For contrast, Sona, Harmonious: "...ready up to 4 **friendly** runes."

**Situation.** My opponent has exhausted runes. My Dark Child's end-of-turn ability resolves.

**Question.** May I choose my opponent's exhausted runes to ready (for example, by mistake or in a multiplayer deal), or does "runes" with no qualifier mean only my own?

**Rules found**

- For "any rune on the board": 133.3.a (a characteristic refers to any Game Object with it on the board unless a zone is specified), 355.9.a.1 ("unit", "gear" and "rune" refer to objects on the Board unless specified otherwise), 416.3.b (players may ready Game Objects on the board when instructed). Sona prints "friendly", which suggests the others are not limited.
- For "only your own": no rule found; this would be a convention that an unqualified "runes" means the controller's.

---

## 2. Does Void Hatchling apply to a reveal from the Rune Deck?

**Cards**

- Void Hatchling: "If you would reveal cards from a deck, look at the top card first. You may recycle it. Then reveal those cards."
- Twisted Fate, Gambler: "When I attack, reveal the top rune of your rune deck, then recycle it. Do one of the following based on its domain: ..."

**Situation.** I control Void Hatchling. Twisted Fate attacks and his ability resolves.

**Question.** Is the Rune Deck "a deck" for Void Hatchling? If yes, I look at the top rune first and may recycle it before the reveal, which lets me steer which of Twisted Fate's three effects I get.

**Rules found**

- For yes: 425.3 ("Reveal cards from [zone]"), 108.5 (the Rune Deck is a zone that holds a deck), 417.5.a (cards are recycled to the Rune Deck). Void Hatchling says "a deck", not "your Main Deck".
- For no: every other card that reveals from a deck names the Main Deck, and the rules' own examples of "deck" in card text mean the Main Deck.

---

## 3. Guards! revealed from hidden at a battlefield where units can't be played

**Cards**

- Guards!: "[Hidden] Play a 2 [M] Sand Soldier unit token. Then do this: You may pay [C] to ready it."
- Rockfall Path (battlefield): "Units can't be played here."

**Situation.** I hid Guards! at Rockfall Path. I play it from facedown.

**Question.** Where does the Sand Soldier go?

- (a) Nowhere. 811.1.d.5 says the unit must be played at that battlefield; Rockfall Path says it can't be; "can't" beats "can" (054.1), so no token is played. (Can Guards! be played from facedown here at all?)
- (b) To my base, because the mandatory location cannot be met and the token falls back to a legal one.

**Rules found**

- 811.1.d.5: "If a hidden spell or a play effect of a hidden permanent causes you to play a unit, you must choose to play that unit at that battlefield."
- 054.1: cards that forbid supersede cards that permit.
- FAQ, Perched Grimwyrm / Dragon Roost / Rockfall Path entry: a restriction ("can't") is not overcome by a requirement or permission.

**Follow-up.** If the answer is (a) and no token is played, does "Then do this: You may pay [C] to ready it" still happen (is there an "it")?

---

## 4. Disposal Order: may I choose the "up to 3" mode with nothing to choose?

**Card**

- Disposal Order: "[Reaction] Choose one — Choose up to 3 cards from opponents' trashes. Their owners recycle them. / Draw 1."

**Situation.** My opponent's trash is empty. I play Disposal Order.

**Question.** May I pick the first mode and choose zero cards (the spell then does nothing), or is that mode unavailable when there is nothing to choose, so I must pick "Draw 1"?

**Rules found**

- For "may pick it": 355.13 ("up to" allows choosing zero, and the spell can be played without any targets).
- For "may not": no rule found; this would be a principle that a mode with no possible effect can't be selected.

---

## 5. Optional effects that would do nothing

### 5a. Emperor's Divide from facedown with no friendly unit there

**Card**

- Emperor's Divide: "[Hidden] [Action] Move any number of friendly units at a battlefield to their base."

**Situation.** I hid Emperor's Divide at a battlefield. I now have no units there (I do have units at the other battlefield).

**Question.** Can I play it from facedown?

- (a) No. 811.1.d: "A card cannot be played from facedown if it is a spell with no valid targets under these restrictions", and 811.1.d.2's example says all units chosen must be at the battlefield it was played from. There are none.
- (b) Yes, choosing zero units. 355.13: "any number" includes zero, and the spell can be played without any targets.

Which rule wins when both apply?

### 5b. Curator of the Sands: exhaust to ready nothing

**Card**

- Curator of the Sands (legend): "When you play a unit, gear, or activated ability with Energy cost [7] or more, you may exhaust me to ready up to 2 runes."

**Situation.** The ability triggers while none of my runes are exhausted.

**Question.** May I still choose to exhaust Curator (paying the cost and readying zero runes), or is the "may" unavailable when the effect would do nothing?

**Rules found**

- For "may exhaust": 355.13 ("up to" includes zero); a cost may be paid for an effect that does nothing.
- For "may not": no rule found.

---

## 6. A triggered ability with a required choice it cannot make, and an instruction that needs no choice

**Card**

- Teemo, Strategist: "When I defend, choose an enemy unit here and reveal the top 5 cards of your Main Deck. Deal 1 to that unit for each card with [Hidden] revealed this way, then recycle the revealed cards."

**Situation.** Teemo defends. Every attacking unit "can't be chosen by enemy spells and abilities" (or the attackers have left by the time the ability would be finalized), so there is no enemy unit here that he can choose.

**Question.** Is the whole ability removed from the chain — no reveal, no recycle — or is it finalized without a target, so that the top 5 cards are still revealed and recycled and only the damage is skipped?

**Rules found**

- For "removed, nothing happens": 403.4, "If there are not enough options to make legal choices for a Triggered Ability that has been put on the chain, remove it from the Chain now. It ceases to be a Pending Item but never becomes a Finalized Chain Item."; 355.8, "In order to put a spell or ability on the chain, valid choices must be made for all targets."
- For "reveal and recycle anyway": 359.3.e.5 to 359.3.e.7 — an instruction whose target is illegal is ignored and the rest is followed — but those rules speak of a target that became illegal after the item was finalized (a mistarget), not of one that could never be chosen. The reveal reorders the deck and shows both players five cards, so the answer changes the game.

**What the engine does.** The strict reading of 403.4: the ability is removed as it would be finalized and nothing of it happens (`C4Test.TeemoStrategist_NoUnitToChoose_TheAbilityLeavesTheChainAndNothingIsRevealed`). Teemo, Strategist is the only registered card for which the two readings differ (`SweepCostsTest.Removal_NoRegisteredTriggerLosesAnEffectExceptTheListedOnes` checks every opted-in trigger).

---

## 7. A gear played from facedown: is it recalled before its "when you play this" ability attaches it?

**Cards**

- Edge of Night: "[Hidden] When you play this from face down, attach it to a unit you control (here). [Equip] [C]"
- Zhonya's Hourglass (any non-Equipment gear with [Hidden]) for the general case.

**Situation.** I play Edge of Night from facedown at a battlefield where I control a unit. It enters the board at that battlefield, unattached; its play ability is put on the chain as a pending item.

**Question.** A Cleanup follows the gear entering (319.6) and its task 5 recalls "Unattached non-Unit Gear located at Battlefields" (323.6), with no Open-State condition (323.5 has one, 323.6 does not). Is the gear recalled to my base before the play ability is finalized? If so, does "attach it to a unit you control (here)" still attach it to a unit at the battlefield it was hidden at ("here" being where it was played), or does the ability find no unit "here" and leave the chain (403.4)?

**Rules found**

- For "recalled first, then attached to a unit at the battlefield it was played at": 319.6, 323.6 as quoted; 383.4.a.2 (play effects go on the chain after the permanent has entered); the card would do nothing otherwise.
- For "not recalled while its own ability is pending": 323.6 "unless an effect would indicate otherwise" — the pending attach could be read as such an effect.

**What the engine does.** The gear is recalled by the Cleanup; the ability chooses among my units at the battlefield it was played from facedown at, and attaches it there (`ReviewFixC1Test.EdgeOfNight_TheControllerChoosesTheUnitHere`).

---

## 8. Who gains Focus when one player's effect moves the other player's unit into a showdown

**Card**

- Charm: "Move an enemy unit."

**Situation.** On my turn I play Charm and move my opponent's unit from their base to a battlefield nobody controls. No other unit is there. The battlefield becomes Contested and a (non-combat) showdown opens at the next Cleanup.

**Question.** Who gains Focus as the showdown begins: my opponent, whose unit applied Contested, or me, whose spell caused the move?

**Rules found**

- For "the opponent": 345, "As a Showdown begins, the player who applied Contested status to the Battlefield gains Focus."; 190.3.a.1, "Units moving to or being played to a battlefield apply Contested status if that Battlefield is not already Contested and that Unit's controller does not already control that battlefield." — the unit applies it, and the player who applied it would be its controller; 323.10.a speaks of "their controller applies Contested status".
- For "me": the move is my action and my effect; nothing in 345 says the player who applied Contested is the controller of the unit rather than the player who moved it.
- For a combat the rules answer it: 466.2.c.1 / 466.2.c.1.a, the Attacker ("the player whose unit(s) applied the Contested status") gains Focus.

**What the engine does.** Focus goes first to the controller of the unit that applied Contested — my opponent in this situation (`Batch2Combat.G3_NonCombatShowdown_TheControllerOfTheUnitThatAppliedContestedHasFocusFirst_J2`). In a combat it goes to the Attacker (`Batch2Combat.G3_InACombatTheAttackerHasFocusFirst_AlsoOnTheOtherPlayersTurn`). Before batch 2 it always went to the turn player (finding G3).

---

## 9. Battlefield abilities at the start of a Beginning Phase: who controls them, and "your" with no controller

**Cards**

- Frozen Fortress: "At the start of each player's Beginning Phase, deal 1 to each unit here."
- Dusk Rose Lab: "At the start of your Beginning Phase, you may kill a unit you control here to draw 1."

**Situation.** (a) Player B controls Frozen Fortress; player A's Beginning Phase starts. (b) Nobody controls Dusk Rose Lab when a player's Beginning Phase starts.

**Question.** (a) Who controls Frozen Fortress's ability — B, the battlefield's controller, or A, because "each player's Beginning Phase" "indicates another player"? It decides who puts it on the chain and has priority first. (b) Does Dusk Rose Lab's ability trigger at all while the battlefield is uncontrolled?

**Rules found**

- (a) For B: 190.6.a, "While a Battlefield is Controlled, its Controller controls its Abilities unless that ability indicates another player does." The ability names a time, not a player who controls it. For A: the same sentence, if naming whose phase it is counts as indicating a player.
- (b) For "no trigger": 190.6.d, "'You' in a battlefield's abilities refers to the battlefield's Controller ... If the battlefield has no Controller, 'you' refers to no one, and all such instructions are ignored." "Your Beginning Phase" is then nobody's. For "it triggers, controlled by the turn player": 190.6.b, an uncontrolled battlefield's abilities are the Turn Player's, who "is treated as their Controller if any game rule or effect requires one".

**What the engine does.** (a) The battlefield's controller controls it, the turn player while it is uncontrolled (`Batch1b.G10_FrozenFortress_TheControllerOfTheBattlefieldControlsItsAbilityOnTheOtherPlayersTurn`, `.G10_FrozenFortress_UncontrolledTheTurnPlayerPutsItOnTheChain`). (b) Dusk Rose Lab triggers only in its controller's Beginning Phase; uncontrolled, it does not trigger (`Batch1b.G10_DuskRoseLab_UncontrolledDoesNotTrigger_J5`). Before batch 1b the turn player controlled every such ability (finding G10).

---

## 10. A "when you play a spell" unit that enters during that spell's resolution

**Cards**

- Ravenbloom Student: "When you play a spell, give me +1 [M] this turn."
- Temporal Breach: "Banish a unit, then its owner plays it to the same location, ignoring its cost."

**Situation.** I play Temporal Breach on my own Ravenbloom Student. It is banished and played back during the resolution: a new object (124.1).

**Question.** Does the played-back Student trigger for that Temporal Breach?

**Rules found**

- For "yes": 420.4.a, "Any such triggered abilities trigger when the act of playing the card has been completed by the resolution of the card." When Temporal Breach has resolved, the new Student is on the board and is a card that says "When you play a spell".
- For "no": the new object did not exist when Temporal Breach was played; the play it would trigger on began before it existed.

**What the engine does.** The played-back Student triggers and gets +1 [M] (`Batch1b.G26_RavenbloomStudentPlayedBackByTemporalBreachTriggersOnIt_J7`).

---

## 11. Deathgrip with only one friendly unit

**Card**

- Deathgrip: "[Reaction] Kill a friendly unit. If you do, give +[M] equal to its Might to another friendly unit this turn. Draw 1."

**Situation.** I control exactly one unit. I play Deathgrip.

**Question.** Can Deathgrip be played (kill that unit, no unit to give the Might to, draw 1), or must a valid "another friendly unit" exist to put it on the chain?

**Rules found**

- For "it can be played": the second instruction is conditional ("If you do, ..."); 359.3.e.5 — an instruction that cannot be followed is skipped and the rest of the spell is followed.
- For "it cannot": 355.8, "In order to put a spell or ability on the chain, valid choices must be made for all targets"; 355.5 / 355.10 — "another friendly unit" is chosen by its controller, so it is a target.

**What the engine does.** Deathgrip is playable on a single friendly unit: it kills it and draws 1 (`C3Test.Deathgrip_IsPlayableOnASingleFriendlyUnit`; testing phase D28).

---

## 12. Lonely Poro: did it die alone when every other friendly unit there died with it?

**Card**

- Lonely Poro (two printings): "[Deathknell] — If I died alone, draw 1. (I'm alone if there are no other friendly units here.)"

**Situation.** Lonely Poro and the only other friendly unit at its battlefield take lethal combat damage and die in the same Cleanup.

**Question.** Did Lonely Poro die alone?

**Rules found**

- For "no, it was not alone": the deaths are simultaneous (323.4, 370.1.a.2); a Deathknell is processed just before its source changes zones (383.2.c.3), when the other unit was still there; an effect "can look back" at an object's characteristics before it changed zones (359.3.e.13).
- For "yes, it was alone": the ability resolves after both have died, and nothing friendly is "here" then; the reminder text says "I'm alone if there are no other friendly units here" in the present tense.

**What the engine does.** Not alone: no card is drawn. "Here" is read from the moment Lonely Poro died, from last-known information taken for all units that die together before the first leaves, so a friendly unit that arrives before the ability resolves changes nothing either (`SweepTriggersTest.LonelyPoro_UnitsThatDieTogetherDidNotDieAlone` for 777, `Batch4bTest.D80_LonelyPoro359_UnitsThatDieTogetherDidNotDieAlone` for 359; testing phase D80 — the 359 printing used to read the board as the ability resolved).
