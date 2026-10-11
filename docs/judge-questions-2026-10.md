# Rules questions for judge review (2026-10)

Five questions that came up while implementing cards against the 2026-10-09 Core Rules. Each gives the card text, the situation, the question, and the rules we found on either side. Rule numbers are from the Core Rules; "FAQ" is the Vendetta Rules FAQ and Clarifications.

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

## 6. An unplayed hidden unit or gear when its controller loses the battlefield

**Card (example)**

- Zhonya's Hourglass (gear): "[Hidden] If a friendly unit would die, kill this instead. Heal that unit, exhaust it, and recall it."

**Situation.** My Hourglass is facedown at a battlefield. An enemy spell takes or moves away my only unit there. I do **not** play the Hourglass in response. Nothing dies. I lose control of the battlefield in the next cleanup.

**Question.** Where does the facedown Hourglass go?

- (a) To my trash, like any hidden card.
- (b) To my base, because it is a gear (or a unit); only hidden spells are trashed. If (b): is it revealed, or does it stay facedown in base?

**Rules found**

- For (a): 323.6.a (cleanup step 5): "Remove all Hidden cards from all Battlefields that are not controlled by the same player and place them in their owner's Trash." 107.3.d: cards in the Facedown Zone of a battlefield whose control is lost "are removed during the next Cleanup." Neither separates spells from units or gear.
- For (b): no rule found. (A gear that was played faceup at a battlefield is recalled to base by 149.3 / 323.6; that is a different case.)
