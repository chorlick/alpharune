# Card gap audit — master summary

Per-clause audit of all 787 cards vs printed ability_text (2026-05-26).
**325 of 787 cards (~41%) have a real gap** — far more than the 23 cards
carrying an explicit gap comment. (The old audit_deck_cards.py 'FULL' metric
only checked 'has a non-empty body', so it badly over-reported completeness.)

Categories: full-miss (whole effect absent), partial (a clause missing),
approx (documented approximation), bug (implemented but WRONG behavior).

## Range 001-100

Notes on judging:
- Engine has a generic `ability_text`-based aura parser (`GameEngine::recalculateAuras`,
  game_engine.cpp ~3576) that covers "other friendly units (here) have …",
  "your units have …", "units here have …", "stunned enemy units here have …",
  "your mechs each have …", plus conditional self-auras ("if you've discarded …",
  "while I'm buffed …", "my might is increased by your points", "while I'm [Mighty] …").
  Cards whose only effect is matched there are treated as IMPLEMENTED even when the
  per-card .cpp is vanilla (e.g. 15 Captain Farron, 19 Raging Soul, 28 Draven, 65
  Wizened Elder, 74 Taric aura, 79 Leona stun-aura, 100 Gemcraft Seer).
- `EffectExecutor::giveTemporaryMight(target, amount, minimum=0)` only enforces a
  floor when `minimum` is passed. Cards that say "to a minimum of 1 [M]" but call it
  without `minimum=1` fail to clamp.

| id | name | type | missing/partial clause | severity |
|----|------|------|------------------------|----------|
| 2 | Brazen Buccaneer | unit | "discard 1 as additional cost, then reduce my cost by [2]" — vanilla, no impl | full-miss |
| 5 | Disintegrate | spell | "If this kills it, draw 1" — draws unconditionally (comment `// Condition: IfKills` but no guard) | partial |
| 6 | Flame Chompers | unit | "When you discard me, you may pay [R] to play me" — vanilla, no impl | full-miss |
| 11 | Magma Wurm | unit | "Other friendly units enter ready" — vanilla; engine only consults the entering card's own entersReadyOnPlay | full-miss |
| 17 | Iron Ballista | gear | "This enters exhausted" + "[E]: Deal 2 to a unit at a battlefield" — vanilla, no activated ability | full-miss |
| 21 | Sun Disc | gear | "[E]: next unit you play this turn enters ready" — activated ability declared but no onActivate body (base is no-op) | full-miss |
| 23 | Unlicensed Armory | gear | "Discard 1, [E]: choose a friendly unit; next death this turn pay [C] to heal/exhaust/recall" — vanilla, no impl | full-miss |
| 25 | Blind Fury | spell | reveal top / banish / play ignoring cost / recycle rest — onResolve is empty | full-miss |
| 31 | Raging Firebrand | unit | "next spell you play this turn costs [5] less" — triggerType set but no onTrigger | full-miss |
| 32 | Ravenborn Tome | gear | "[E]: next spell you play this turn deals 1 Bonus Damage" — activated declared but no onActivate body | full-miss |
| 33 | Shakedown | spell | opponent "have you draw 2 instead" opt-out unmodeled; always deals 6 (documented) | approx |
| 34 | Tryndamere, Barbarian | unit | "When I conquer, if you assigned 5+ excess damage, score 1 point" — vanilla, no impl | full-miss |
| 35 | Vayne, Hunter | unit | "If an opponent controls a battlefield, I enter ready" + "When I conquer, pay [1] to return me to hand" — vanilla, no impl | full-miss |
| 36 | Vi, Destructive | unit | "Recycle 1 from your trash: Give me +1 [M] this turn" — target reqs only, no activated ability | full-miss |
| 37 | Immortal Phoenix | unit | "When you kill a unit with a spell, pay [1][R] to play me from your trash" — vanilla, no impl | full-miss |
| 41 | Volibear, Furious | unit | "deal 5 split among enemy units here" — code deals 5 to ctx.source (self) and kills itself; wrong target | partial |
| 44 | Clockwork Keeper | unit | "pay [C] additional cost; if paid, draw 1" — vanilla, no impl | full-miss |
| 45 | Defy | spell | counter cost gate ignores "no more than [A]" additional-power cap (only energy ≤4 checked) | approx |
| 47 | Find Your Center | spell | missing "channel 1 rune exhausted" and the "costs [2] less if opp within 3 of Victory" reduction; only draws 1 | partial |
| 53 | Stand United | spell | missing "Buffs give an additional +1 [M] to friendly units this turn"; only buffs target | partial |
| 57 | Block | spell | missing [Tank] this turn; only grants [Shield 3] | partial |
| 59 | Eclipse Herald | unit | "When you stun an enemy unit, ready me and give me +1 [M]" — vanilla, no impl | full-miss |
| 60 | Mask of Foresight | gear | "When a friendly unit attacks or defends alone, give it +1 [M]" — vanilla, no impl | full-miss |
| 61 | Poro Herder | unit | ignores "if you control a Poro" gate and "buff me"; always draws 1, never buffs | partial |
| 62 | Reinforce | spell | only banishes target; missing look-top-5 / play reducing cost by [5] / recycle rest | full-miss |
| 63 | Spirit's Refuge | gear | missing aura "Friendly buffed units have [Deflect]" (aura parser doesn't match this phrasing); only the on-play buff works | partial |
| 68 | Caitlyn, Patrolling | unit | "I must be assigned combat damage last" + "[E]: deal damage = my Might to a unit" — vanilla, no impl | full-miss |
| 70 | Mageseeker Warden | unit | "opponents can only play units to their base" + "spells/abilities can't ready enemy units and gear" — vanilla, no impl | full-miss |
| 71 | Party Favors | spell | entire "each player chooses Cards or Runes; draw/channel accordingly" — vanilla, no impl | full-miss |
| 72 | Solari Shrine | gear | "When you kill a stunned enemy unit, you may exhaust this to draw 1" — vanilla, no impl | full-miss |
| 76 | Yasuo, Remorseful | unit | "deal damage equal to my Might" — code deals hardcoded 1 instead of current_might | partial |
| 79 | Leona, Zealot | unit | missing "If an opponent's score is within 3 of Victory, I enter ready" (no entersReadyOnPlay); stun-aura is handled | partial |
| 80 | Mystic Reversal | spell | "Gain control of a spell. You may make new choices for it" — vanilla, no impl | full-miss |
| 84 | Eager Apprentice | unit | cost reduction applies to ALL friendly plays, not just spells (documented over-application) | approx |
| 90 | Orb of Regret | gear | "to a minimum of 1 [M]" not enforced — calls giveTemporaryMight(-1) without minimum | partial |
| 93 | Smoke Screen | spell | "to a minimum of 1 [M]" not enforced — calls giveTemporaryMight(-4) without minimum | partial |
| 94 | Sprite Call | spell | "Play a ready 3 [M] Sprite token with [Temporary]" — vanilla, no impl | full-miss |
| 97 | Blastcone Fae | unit | "to a minimum of 1 [M]" not enforced — calls giveTemporaryMight(-2) without minimum | partial |
| 98 | Energy Conduit | gear | "[E]: [Add] [1]" — activated ability declared but no onActivate body (cf. Seal of Rage/Focus which call addFloatingPower) | full-miss |
| 99 | Garbage Grabber | gear | "Recycle 3 from your trash, [1], [E]: Draw 1" — target reqs only, no activated ability | full-miss |

## Range 101-200

| id | name | type | missing/partial clause | severity (full-miss / partial / approx) |
|----|------|------|------------------------|------------------------------------------|
| 101 | Mushroom Pouch | gear | "if you control a facedown card at a battlefield, draw 1" — triggerType declared but no onTrigger; conditional draw not implemented | full-miss |
| 102 | Portal Rescue | spell | banishes the unit but never replays it to owner's base ignoring cost | partial |
| 104 | Retreat | spell | bounces unit but missing "Its owner channels 1 rune exhausted" | partial |
| 107 | Ava Achiever | unit | "When I attack, you may pay [C] to play a [Hidden] card ignoring cost..." — triggerType only, no onTrigger | full-miss |
| 108 | Convergent Mutation | spell | "increase its Might to the Might of another friendly unit" — onResolve empty, only targeting set | full-miss |
| 109 | Dr. Mundo, Expert | unit | Might += cards in trash (no aura/dynamic might); + start-of-Beginning "recycle 3 from trash" — both missing | full-miss |
| 111 | Heimerdinger, Inventor | unit | "I have all [E] abilities of all friendly legends, units, and gear" — not implemented (stub) | full-miss |
| 112 | Kai'Sa, Evolutionary | unit | conquer: play spell from trash without Energy cost then recycle — triggerType only, no onTrigger | full-miss |
| 113 | Malzahar, Fanatic | unit | "Kill a friendly unit/gear, [E]: Add [A][A]" — no hasActivatedAbility/onActivate; target reqs alone do nothing | full-miss |
| 115 | Promising Future | spell | look-top-5/banish/recycle/replay clause — stub, no onResolve | full-miss |
| 117 | Viktor, Innovator | unit | "When you play a card on opponent's turn, play a 1 [M] Recruit token" — stub | full-miss |
| 118 | Wraith of Echoes | unit | "first time a friendly unit dies each turn, draw 1" — stub, no trigger | full-miss |
| 119 | Ahri, Inquisitive | unit | -2 [M] applied but missing "to a minimum of 1 [M]" clamp | partial |
| 129 | Confront | spell | readies the spell object itself; does NOT make units-you-play-this-turn enter ready | partial |
| 141 | Kinkou Monk | unit | buffs only ONE unit; text is "buff up to two other friendly units" | partial |
| 143 | Pirate's Haven | gear | "When you ready a friendly unit, give it +1 [M] this turn" — stub | full-miss |
| 144 | Spoils of War | spell | missing conditional "If an enemy unit has died this turn, this costs [2] less" | partial |
| 146 | Wallop | spell | missing "you may spend a buff as additional cost; if you do, ignore this spell's cost" | partial |
| 147 | Wildclaw Shaman | unit | only readies; missing "you may spend a buff to buff me and ready me" (buff + spend-buff gate) | partial |
| 149 | Carnivorous Snapvine | unit | onTrigger empty — mutual "deal damage equal to Mights" not implemented (only targeting set) | full-miss |
| 150 | Kraken Hunter | unit | "spend any number of buffs to reduce cost by [O] each" — not implemented (Accelerate/Assault engine-handled) | full-miss |
| 151 | Lee Sin, Centered | unit | "Other buffed friendly units at my battlefield have +2 [M]" — aura not matched by engine ("buffed"/"at my battlefield" not a recognized pattern) | full-miss |
| 152 | Mistfall | gear | "When you buff a friendly unit, you may pay [O] and exhaust this to ready it" — target reqs only, no onTrigger/onActivate | full-miss |
| 157 | Udyr, Wildman | unit | "Spend my buff: choose one not chosen this turn — deal 2 / stun / ready me / give Ganking" — target reqs only, no modal onActivate | full-miss |
| 158 | Volibear, Imposing | unit | "When an opponent moves to a battlefield other than mine, draw 1" — missing (Shield/Tank engine-handled) | partial |
| 159 | Warwick, Hunter | unit | "I enter ready" + "When I attack, kill all damaged enemy units here" — stub | full-miss |
| 161 | Deadbloom Predator | unit | "You may play me to an occupied enemy battlefield" — no getPlayLocations override | full-miss |
| 164 | Sett, Brawler | unit | "When played and when I conquer, buff me" + "Spend my buff: +4 [M] this turn" — stub | full-miss |
| 165 | Cemetery Attendant | unit | "return a unit from your trash to your hand" — default target enumeration is on-board only; targets friendly board units, not trash | full-miss |
| 167 | Ember Monk | unit | "When you play a card from [Hidden], give me +2 [M] this turn" — stub | full-miss |
| 170 | Morbid Return | spell | "return a unit from your trash to your hand" — default enumeration is on-board only; bounces a board unit, not trash | full-miss |
| 174 | Sai Scout | unit | "You may play me to an open battlefield" — no getPlayLocations override ([Vision] engine-handled) | full-miss |
| 176 | Sneaky Deckhand | unit | "You may play me to an open battlefield" — no getPlayLocations override (stub) | full-miss |
| 177 | Stealthy Pursuer | unit | "When a friendly unit moves from my location, I may be moved with it" — stub | full-miss |
| 179 | Acceptable Losses | spell | "Each player kills one of their gear" — stub, no onResolve | full-miss |
| 182 | Scrapheap | gear | "When this is played, discarded, or killed, draw 1" — stub, no trigger | full-miss |
| 186 | Treasure Trove | gear | modeled on WhenIDie only; "When this leaves the board" does not fire on bounce-to-hand departures (documented PARTIAL) | partial |
| 187 | Whirlwind | spell | "each player may return a unit to its owner's hand" — stub | full-miss |
| 189 | Kayn, Unleashed | unit | "If I have moved twice this turn, I don't take damage" — not implemented (Ganking engine-handled) | full-miss |
| 192 | Mindsplitter | unit | text: controller "chooses a card" for opponent to discard; impl lets the OPPONENT pick which card to discard (wrong chooser) | partial |
| 193 | Miss Fortune, Buccaneer | unit | "play me to open battlefield" + "Friendly units may be played to open battlefields" — stub, no getPlayLocations | full-miss |
| 195 | Rhasa the Sunderer | unit | "I cost [1] less for each card in your trash" — stub, no cost modifier | full-miss |
| 196 | Soulgorger | unit | "play a unit from your trash, ignoring its Energy cost" — triggerType declared, no onTrigger | full-miss |
| 198 | The Harrowing | spell | "Play a unit from your trash, ignoring its Energy cost" — stub, no onResolve | full-miss |
| 200 | Twisted Fate, Gambler | unit | reveal-top-rune modal (R/B/Y domain payoffs) — onTrigger body empty | full-miss |

## Range 201-300

Audited every `src/cards/<type>/<NNNN>_*.cpp` for card_id in [201,300] against `ability_text` in
`cards/registry.json`. Engine-handled keywords (Tank/Deflect/Ambush/Accelerate/Hidden/Shield/
Assault/Ganking/Backline/Legion/Vision/Unique/Quick-Draw/Repeat/Temporary/Equip, Action/Reaction
timing) are NOT counted as gaps. Battlefield Hold/Conquer/Defend/AtStart/AtEnd triggers ARE
dispatched by the engine (`fireBattlefieldTriggers` via `bf.card_object_id`), so most BF gaps below
are unimplemented clauses, not dispatch failures; static/passive-aura BF clauses with no trigger and
no `applyPassiveAura` are flagged as passive-not-implemented.

| id | name | type | missing/partial clause | severity |
|----|------|------|------------------------|----------|
| 201 | Invert Timelines | spell | Entire effect missing — no onResolve. "Each player discards their hand, then draws 4" not implemented. | high |
| 202 | Jinx, Rebel | unit | Entire ability missing — no trigger. "When you discard 1+ cards, ready me and give me +1 [M] this turn." | high |
| 203 | Possession | spell | onResolve empty. "Take control of it and recall it" not implemented (only targets). | high |
| 205 | Yasuo, Windrider | unit | "The third time I move in a turn, you score 1 point" not implemented (no move-counter trigger). | high |
| 207 | Call to Glory | spell | "spend a buff to ignore this spell's cost" optional additional-cost / cost-ignore unmodeled (documented). +3 [M] works. | low |
| 208 | Cruel Patron | unit | "As an additional cost to play me, kill a friendly unit" not implemented (no optionalAdditionalCost / cost hook). | high |
| 209 | Cull the Weak | spell | Opponent's own-unit choice not plumbed — kills first available enemy unit (documented approximation). | low |
| 211 | Faithful Manufactor | unit | onTrigger empty (only triggerType set). "Play a 1 [M] Recruit token here" not implemented. | high |
| 218 | Vanguard Captain | unit | No onTrigger. "[Legion] play two 1 [M] Recruit tokens here" not implemented. | high |
| 221 | Imperial Decree | spell | Only kills already-damaged units; "when any unit takes damage THIS TURN, kill it" future-damage delayed effect unmodeled (documented). | medium |
| 222 | Noxian Drummer | unit | onTrigger empty (only WhenIMoveToFB set). "Play a 1 [M] Recruit token here" not implemented. | high |
| 225 | Solari Chief | unit | onTrigger empty. "If stunned, kill it; otherwise stun it" not implemented (only targets enemy). | high |
| 223 | Peak Guardian | unit | Buffs self only; "if at a battlefield, buff all OTHER friendly units there" second clause not implemented. | medium |
| 226 | Spectral Matron | unit | "no more than [A]" (available-power) cap on trash unit not checked — only the [3] energy cap applied. | low |
| 227 | Symbol of the Solari | gear | Entire effect missing — empty class. "If a combat you attack ends in a tie, recall ALL units instead" not implemented. | high |
| 228 | Vanguard Helm | gear | Entire effect missing — empty class. "When a buffed friendly unit dies, buff another friendly unit." | high |
| 230 | Albus Ferros | unit | onTrigger empty. "Spend any number of buffs; channel 1 rune exhausted per buff" not implemented. | high |
| 231 | Commander Ledros | unit | "As you play me, kill any number of friendly units; reduce cost by [Y] each" cost-reduction not implemented (Deflect/Ganking keywords OK). | high |
| 235 | Karma, Channeler | unit | "When you recycle 1+ cards to Main Deck, buff a friendly unit" not implemented (Vision is engine keyword). | high |
| 237 | King's Edict | spell | Only kills 1 chosen target; the multi-player "each other player chooses a unit you don't control, kill those" voting effect not implemented. | high |
| 239 | Machine Evangel | unit | onTrigger empty (WhenIDie set). "[Deathknell] play three 1 [M] Recruit tokens into base" not implemented. | high |
| 240 | Sett, Kingpin | unit | "+1 [M] for each buffed friendly unit at my battlefield" not implemented (no applyPassiveAura; Tank is engine keyword). | high |
| 242 | Baited Hook | gear | Entire [E] ability missing — only TargetRequirements, no hasActivatedAbility/onActivate. Kill+dig+banish+play not implemented. | high |
| 244 | Divine Judgment | spell | onResolve empty. "Each player keeps 2 units/2 gear/2 runes/2 hand cards, recycle the rest" not implemented. | high |
| 246 | Viktor, Leader | unit | Entire ability missing — no trigger. "When another non-Recruit unit you control dies, play a 1 [M] Recruit token into base." | high |
| 248 | Stormbringer | spell | Battlefield choice anchored to first enemy-occupied BF (agent BF preference not surfaced); core damage+move works (documented). | low |
| 250 | Super Mega Death Rocket! | spell | "When you conquer, you may discard 1 to return this from trash to hand" recursion clause not implemented (deal 5 works). | medium |
| 251 | Noxian Guillotine | spell | Non-Legion branch wrong: should set "kill next time it takes damage this turn" (delayed). Code kills immediately in BOTH branches (killObject twice). | high |
| 252 | Nine-Tailed Fox | legend | Entire effect missing (documented engine gap). "When an enemy unit attacks a BF you control, give it -1 [M] (min 1)." | high |
| 253 | Fox-Fire | spell | Only kills 1 unit; "kill ANY NUMBER of units at a battlefield with total Might 4 or less" multi-kill not implemented. | high |
| 255 | Dragon's Rage | spell | Only moves the enemy unit; "choose another enemy unit at its destination; they deal damage = Might to each other" not implemented. | high |
| 259 | Swift Scout | legend | Both abilities missing — empty class. "[1] hide alt cost" and "[1],[E] return a Teemo unit to hand" not implemented. | high |
| 261 | Siphon Power | spell | Only gives chosen unit +1 [M]; should give ALL friendly units at chosen BF +1 and all enemy units there -1 (min 1). | high |
| 263 | Bullet Time | spell | Battlefield/target preference simplified to chosen unit's BF (documented); pay-X AoE damage otherwise works. | low |
| 265 | Showstopper | spell | Direction inverted: text moves a base unit TO a battlefield; code calls moveToBase (buff OK, move wrong). | high |
| 270 | Altar to Unity | battlefield | onTrigger empty (WhenYouHoldHere set). "Play a 1 [M] Recruit token in base" not implemented. | high |
| 271 | Aspirant's Climb | battlefield | "Increase points needed to win by 1" passive not implemented (no trigger, no aura hook). | medium |
| 273 | Bandle Tree | battlefield | "You may hide an additional card here" passive not implemented. | medium |
| 274 | Fortified Position | battlefield | onTrigger empty (WhenYouDefendHere set). "Chosen unit gains [Shield 2] this combat" not implemented. | high |
| 276 | Hallowed Tomb | battlefield | Implemented as bounce-friendly-unit-to-hand; printed effect is "return Chosen Champion from trash to Champion Zone if empty" — wrong effect. | high |
| 279 | Obelisk of Power | battlefield | "At each player's first Beginning Phase, that player channels 1 rune" not implemented (no trigger wired). | medium |
| 281 | Reckoner's Arena | battlefield | onTrigger empty (WhenYouHoldHere set). "Activate the conquer effects of units here" not implemented. | high |
| 282 | Sigil of the Storm | battlefield | onTrigger empty (WhenYouConquerHere set). "You must recycle one of your runes" not implemented. | high |
| 286 | The Candlelit Sanctum | battlefield | onTrigger empty (WhenYouConquerHere set). Top-2 scry/recycle not implemented. | high |
| 287 | The Dreaming Tree | battlefield | "When a player first targets a friendly unit here with a spell each turn, draw 1" passive/trigger not implemented. | high |
| 288 | The Grand Plaza | battlefield | onTrigger empty (WhenYouHoldHere set). "If 7+ units here, you win the game" not implemented. | high |
| 289 | Trifarian War Camp | battlefield | "Units here have +1 [M]" static aura not implemented (no applyPassiveAura). | high |
| 290 | Vilemaw's Lair | battlefield | "Units can't move from here to base" movement restriction not implemented. | high |
| 291 | Void Gate | battlefield | "Spells and abilities deal 1 Bonus Damage to units here" not implemented. | high |
| 292 | Windswept Hillock | battlefield | "Units here have [Ganking]" static keyword aura not implemented. | high |
| 294 | Daughter of the Void | legend | onActivate missing — declares activated/[Add] ability but adds no power. "[Add] [A], spells only" not implemented. | high |
| 296 | Hand of Noxus | legend | onActivate missing — declares activated/[Add] ability but adds no power. "[Legion] [Add] [1]" not implemented. | high |
| 299 | Annie, Fiery | unit | "Your spells and abilities deal 1 Bonus Damage" static effect not implemented (empty class, no aura/damage hook). | high |
| 300 | Firestorm | spell | Over-applies: hits ALL enemy units at ALL battlefields. Text is "all enemy units at A battlefield" (single chosen BF). | medium |

Notes on cards verified CORRECT / fully implemented (not gaps): 204 Seal of Discord, 206 Back to Back,
210/215 (Assault keyword), 212 Forge of the Future, 213 Hidden Blade, 214 Order Rune (empty),
216 Soaring Scout, 217 Trifarian Gloryseeker, 219 Vanguard Sergeant (empty), 220 Facebreaker,
223 Peak Guardian (note: buffs only self, not "all other friendly units here" — see below),
224 Salvage, 229 Vengeance, 232 Fiora Victorious, 233 Grand Strategem, 234 Harnessed Dragon,
236 Karthus Eternal, 238 Leona Determined, 241 Shen Kinkou (keywords), 243 Darius Executioner,
245 Seal of Unity, 247 Icathian Rain, 249 Loose Cannon, 254 Blind Monk, 256 Unforgiven,
257 Last Breath, 258 Zenith Blade, 260 Guerilla Warfare (hide-ignoring-cost clause is a known
engine gap but minor), 262 Bounty Hunter, 264 The Boss, 266-268 Recruits (empty), 269 Sprite
(Temporary keyword), 275 Grove, 276 Hallowed Tomb, 277 Monastery of Hirana, 278 Navori,
279 Obelisk (note below), 280 Reaver's Row, 283 Startipped Peak, 284 Targon's Peak,
285 The Arena's Greatest, 293 Zaun Warrens, 295 Relentless Storm, 297 Radiant Dawn,
298 Herald of the Arcane.

All gaps are in the table above. Several are correctness MISMATCHES (effect implemented but wrong),
not just omissions — notably 251 (kills immediately in both branches), 265 (move direction inverted),
276 (wrong effect entirely), 300 (over-applies to all BFs). These are the highest-risk because they
silently do something the card doesn't say.

## Range 301-400

| id | name | type | missing/partial clause | severity |
|----|------|------|------------------------|----------|
| 302 | Yi, Meditative | unit | Stub. "While you have 8+ runes, I have +4 [M]" conditional static not implemented (no applyPassiveAura; not matched by engine aura text-patterns). | high |
| 304 | Lux, Illuminated | unit | Stub. "When you play a spell that costs [5]+, give me +3 [M] this turn" trigger not implemented (no triggerType/onTrigger). | high |
| 306 | Gentlemen's Duel | spell | Only gives +3 [M]. Missing entire second clause: choose an enemy unit; both units deal damage equal to their Mights to each other. | high |
| 311 | Garen, Commander | unit | "Other friendly units have +1 [M] here." Engine text-match applies it as all-friendly-everywhere (matches "other friendly units have" before the "here" qualifier) — scope too broad; should be here-only. | medium |
| 312 | Lux, Crownguard | unit | Declares activated [E] ability but no onActivate — "[Add] [2], use only to play spells" produces no resource (Add cards must call addFloating* themselves). | high |
| 318 | Highlander | spell | onResolve empty. Entire effect (next-time-it-would-die: heal/exhaust/recall replacement) is a no-op. | high |
| 327 | Bushwhack | spell | Only readies the spell itself (`readyObject(ctx.source)`). Should make friendly units enter ready this turn and play a Gold gear token exhausted. | high |
| 333 | Void Drone | unit | Stub. "I cost [2] less to play from anywhere other than your hand" not implemented (no selfCostReduction). | medium |
| 335 | Battering Ram | unit | Stub. "I cost [1] less for each card you've played this turn, min [1]" not implemented (no selfCostReduction). | medium |
| 336 | Blast Corps Cadet | unit | Stub. Optional additional cost [1][R] + conditional "if paid, deal 2 to a unit at a battlefield" not implemented. | high |
| 337 | Minotaur Reckoner | unit | Stub. Global static "Units can't move to base" not implemented. | high |
| 338 | Perched Grimwyrm | unit | Stub. Play restriction "only to a battlefield you conquered this turn" not enforced. | medium |
| 341 | Void Hatchling | unit | Stub. Reveal-replacement ("look at top card first, may recycle, then reveal") not implemented. | medium |
| 342 | Assembly Rig | gear | Stub. Activated ability "[1][R], Recycle a unit from trash, [E]: play a 3[M] Mech token to base" not implemented (no hasActivatedAbility/onActivate). | high |
| 343 | Draven, Vanquisher | unit | Stub. Both abilities missing: "when I win combat, play Gold token"; "when I attack/defend, may pay [R] for +2 [M]". | high |
| 346 | Piercing Light | spell | Second hit not restricted to a different unit ("up to one OTHER unit") — same target could be picked twice. | low |
| 347 | Rell, Magnetic | unit | Declares WhenIAttack trigger but no onTrigger — "when I attack, may play an Equipment ≤[2] ignoring cost, then attach to me" not implemented. | high |
| 349 | Rumble, Hotheaded | unit | Conquer recur plays the Mech free; documented approximation — the printed "reduce Energy cost by Might of recycled unit" cost reduction is not modeled. | low |
| 350 | Dunebreaker | unit | Stub. Both clauses missing: conditional enter-ready (≤2 cards in hand) and "when I hold, draw 2". | high |
| 351 | Lucian, Gunslinger | unit | Deals hardcoded 1, but text is "damage equal to my [Assault]" — does not scale with buffed/equipped Assault value. | medium |
| 352 | Rek'Sai, Breacher | unit | Stub. Aura "Friendly units played from anywhere other than hand have [Accelerate]" not implemented. | medium |
| 358 | Guardian of the Passage | unit | Contradictory target req (must_be_unit && must_be_gear) and uses bounceToHand on a board object; should return a unit or gear from trash to hand (optional "may"). | high |
| 359 | Lonely Poro | unit | Declares WhenIDie but no onTrigger — "[Deathknell] if died alone, draw 1" not implemented. | high |
| 362 | Royal Entourage | unit | Targets a unit and only readies; text is "ready OR exhaust a legend" — wrong target type (legend) and missing exhaust mode. | high |
| 364 | Apprentice Smith | unit | onTrigger empty. "When I move, reveal top of deck; if gear draw it, else recycle" is a no-op. | high |
| 367 | Legion Quartermaster | unit | Stub. Additional play cost "return a friendly gear to its owner's hand" not implemented. | high |
| 372 | Aphelios, Exalted | unit | Only declares target req; no trigger/effect. Modal "when you attach Equipment to me, choose one (not chosen this turn): ready 2 runes / channel 1 exhausted / buff a friendly unit" not implemented. | high |
| 376 | Janna, Savior | unit | Heals one friendly target then moves the SAME unit. Text: heal ALL your units here, then move up to one ENEMY unit from here to base. Both clauses wrong. | high |
| 377 | Jax, Unmatched | unit | Stub (Deflect engine-handled). Aura "Your Equipment everywhere have [Quick-Draw]" not implemented. | medium |
| 378 | Needlessly Large Yordle | unit | Stub (Shield/Tank engine-handled). Cost reduction "[2][G] less for each point scored from holding this turn" not implemented. | medium |
| 383 | Tianna Crownguard | unit | Stub (Deflect engine-handled). Static "while I'm at a battlefield, opponents can't gain points" not implemented. | high |
| 386 | Chemtech Cask | gear | Stub. "When you play a spell on an opponent's turn, may exhaust me to play a Gold gear token exhausted" not implemented. | high |
| 388 | Forecaster | unit | Stub. Aura "Your Mechs have [Vision]" not implemented (no applyPassiveAura; engine text-match only handles "your mechs EACH have", not "your mechs have"). | medium |
| 390 | Frostcoat Cub | unit | Stub. Optional additional cost [B] + conditional "if paid, give a unit -2 [M] this turn" not implemented. | high |
| 391 | Gearhead | unit | Stub (Accelerate engine-handled). "Each Equipment attached to me gives double its base Might bonus" not implemented. | high |
| 392 | Plundering Poro | unit | Declares WhenIConquer but no onTrigger — "play a Gold gear token exhausted" not implemented. | high |
| 397 | Pickpocket | unit | Kills any gear unconditionally; missing the "≤[1] Energy cost" filter and the "if you do, play a Gold gear token exhausted" follow-up. | high |
| 398 | Prize of Progress | unit | Stub. "When you use an activated ability of a gear, give me +1 [M] this turn" trigger not implemented. | high |

## Range 401-500

Severity: **high** = effect entirely absent / wrong direction; **med** = a clause missing or scope-reduced (e.g. buffs 1 of N); **low** = minor scope/approximation.

| id | name | type | missing/partial clause | severity |
|----|------|------|------------------------|----------|
| 404 | Ezreal, Dashing | unit | Deals fixed 1, not "damage equal to my Might"; "I don't deal combat damage" and "[B]: [Action] — Move me to your base" both absent | high |
| 405 | Hextech Anomaly | gear | Text adds Energy per [A] paid; code instead recycles runes (power) and grants Energy — converts power→energy, opposite resource direction | high |
| 410 | Renata Glasc, Mastermind | unit | Empty stub: "[1][B]: Draw 1", "[4][B][B][B][B],[E]: Score 1 point", and "only while at a battlefield" gate all absent | high |
| 411 | Rumble, Scrapper | unit | Empty stub: "Your Mechs have +1 [M]" aura and "When I hold, play a 3[M] Mech token to base" absent | high |
| 415 | Dauntless Vanguard | unit | "You may play me to an occupied enemy battlefield" placement permission not implemented | med |
| 420 | Sea Monkey | unit | Empty stub: optional [1] additional cost + "if you paid, buff me" absent | med |
| 422 | Yordle Explorer | unit | Empty stub: "When you play a card with Power cost [A][A]+, draw 1" trigger absent | high |
| 423 | Fae Dragon | unit | Buffs only 1 target (text: "up to four friendly units"); "When you spend a buff, play a Gold gear token" absent | high |
| 425 | Jaull-Fish | unit | Empty stub: "costs [2] less per friendly [Mighty] unit" cost reduction absent ([Accelerate] is engine-handled) | med |
| 429 | Strike Down | spell | Empty onResolve: no Might-damage dealt, no Equipment detach — entire effect absent | high |
| 432 | Fiora, Peerless | unit | Has WhenIAttackOrDefend trigger but no onTrigger body: "double my Might this combat (1-on-1)" absent | high |
| 433 | Here to Help | spell | Empty stub: "play a unit from hand to a battlefield you control, reducing cost by [3]" absent | high |
| 434 | Kato the Arm | unit | Gives fixed +1 [M] only; should grant "my keywords and +[M] equal to my Might"; no keyword copy, no Might scaling | high |
| 436 | Marching Orders | spell | Empty onResolve: mutual "deal damage equal to their Mights to each other" absent (needs friendly+enemy pair) | high |
| 438 | Ancient Henge | gear | Activated ability has no onActivate: "Pay Energy to [Add] that much [A]" power conversion absent | high |
| 441 | Sivir, Ambitious | unit | Empty stub: "on conquer, if 5+ excess damage assigned, deal that much to an enemy unit" absent | high |
| 442 | Black Market Broker | unit | Empty stub: "When you play a card from face down, play a Gold gear token" absent | high |
| 443 | Called Shot | spell | Only draws 1; "look at top 2, draw one and recycle the other" not implemented; target reqs wrong (must_be_unit) | high |
| 446 | Fae Porter | unit | WhenIMoveToFB trigger, no onTrigger body: "pay [P] to move a unit to same battlefield" absent | high |
| 447 | Loyal Pup | unit | Empty stub: "When you defend at a battlefield, you may move me there" absent | high |
| 450 | Temptation | spell | moveToBase only; text is "move enemy unit to a location where there's a unit with same controller" (wrong destination semantics) | med |
| 452 | Ancient Warmonger | unit | Empty stub: variable "[Assault] equal to enemy units here" not implemented ([Accelerate] engine-handled) | high |
| 458 | Harpoon Squad | unit | Empty stub: "When I move from a battlefield, give me +2 [M] this turn" absent | high |
| 462 | Irelia, Graceful | unit | Empty stub: "Your spells that choose me cost [1] or [A] less" cost-reduction aura absent | high |
| 463 | Jae Medarda | unit | Empty stub: "When you choose me with a spell, draw 1" trigger absent | high |
| 464 | Sivir, Mercenary | unit | Empty stub: conditional "+2 [M] and [Ganking] if spent [A][A] this turn" absent ([Accelerate] engine-handled) | high |
| 472 | Bonds of Strength | spell | Buffs only 1 unit; text gives "two friendly units each +1 [M]" | med |
| 473 | Eminent Benefactor | unit | WhenIHold trigger, no onTrigger body: "play two Gold gear tokens exhausted" absent | high |
| 475 | Guards! | spell | Empty stub: "play a 2[M] Sand Soldier token; may pay [C] to ready it" absent | high |
| 478 | Royal Guard | unit | WhenYouPlayMe trigger, no onTrigger body: "play a 2[M] Sand Soldier token here" absent | high |
| 480 | Trusty Ramhound | unit | Empty stub: "While you have another unit here, I have +1 [M]" conditional aura absent | high |
| 481 | Zaun Punk | unit | Has target reqs but no additional-cost and no onTrigger: "kill a friendly gear as additional cost" + "if paid, kill a gear" absent | high |
| 483 | Blood Money | spell | Kills the unit but omits Gold-token payoff (1 if enemy, 2 if friendly) | med |
| 487 | Rally the Troops | spell | Only draws 1; delayed "when a friendly unit is played this turn, buff it" effect absent | high |
| 490 | Altar of Memories | gear | Wrong: exhausts the dead unit (targets[0]); should optionally exhaust self to draw 1 then put a hand card on top/bottom of deck | high |
| 491 | Rek'Sai, Swarm Queen | unit | Only banishes one target; "reveal top 2, banish one and play it (unit→here), recycle rest" not implemented | high |
| 492 | Renata Glasc, Industrialist | unit | Empty stub: "Your tokens enter ready" replacement/aura absent | high |
| 494 | Trove Golem | unit | WhenYouPlayMe trigger, no onTrigger body: "play four Gold gear tokens exhausted" absent | high |
| 495 | Undertitan | unit | Buffs only 1 target; text "give your OTHER units +2 [M] this turn" (all); "As I'm revealed from deck, [Add] [2]" absent | high |
| 497 | Azir, Sovereign | unit | moveToBase of one friendly unit; text "may move any number of your token units to this battlefield" (wrong direction, non-token, single) | high |
| 499 | Corina Veraza | unit | WhenIMoveToFB trigger, no onTrigger body: "play three 1[M] Recruit tokens here" absent ([Accelerate] engine-handled) | high |
| 500 | Fiora, Worthy | unit | Empty stub: "When a unit you control becomes [Mighty], you may pay [Y] to ready it" absent | high |

## Notes (not counted as gaps)
- 408/430/437/445 (World Atlas, Warmog's, Trinity Force, Doran's Ring), 471 (Last Rites): the .cpp implements an equipped-trigger / effect that is NOT present in the registry `ability_text` (text is bare [Equip]). These are *over*-implementations, not missing clauses, so excluded per audit scope.
- 424 Hexdrinker: grants [Deflect] keyword though registry text is bare [Equip] — over-implementation, excluded.
- 466 Switcheroo: "swap Might" modeled as reciprocal `giveTemporaryMight` deltas for the turn — faithful approximation, not flagged.
- 484 Deathgrip: kill + buff + draw all present; "if you do" optionality folded into 2-target requirement — acceptable, not flagged.
- 414/421/448 (Weaponmaster), 416 Direwing, 417/439/454/455/474/482/493 equip gear, 427 Ruin Runner, 431 Akshan, 435 Lucian, 440 Jax, 444 Corrupt Enforcer, 449 Overzealous Fan, 451 Treasure Hunter, 453 Beast Below, 457 Hard Bargain, 459 Windsinger, 460 Edge of Night, 461 Fizz, 465 Spirit Wheel, 467 Vex, 468 Downwell, 469 Draven, 470 Ezreal Prodigy, 476 Honest Broker, 477 Laurent Duelist ([Assault] engine kw), 479 Sandshifter, 485 Drag Under, 486 Glasc Mixologist, 488 Unsung Hero, 489 Vanguard Armory, 496 Xin Zhao, 498 Blade of the Ruined King, 401/402/403/406/407/409/412/413/418/428/456 — fully implement their effect clauses.
- [Repeat] (402,436,443,450,457,472) and [Accelerate]/[Tank]/keyword-only timing are engine-handled and not gaps.

## Range 501-600

Scope: card_id 501–600. ENGINE-HANDLED keywords ([Tank]/[Deflect]/[Ambush]/[Accelerate]/[Hidden]/[Shield]/[Assault]/[Ganking]/[Backline]/[Legion]/[Vision]/[Unique]/[Quick-Draw]/[Repeat]/[Temporary]/[Equip], [Action]/[Reaction] timing) are not counted as gaps. SimpleEquipGear/UniversalEquipGear "[Equip]+might+keyword" gear is treated as fully implemented. Confirmed engine-handled: generic [Repeat] cost loop re-resolves spells (501/571/579/594 fine); generic aura parser handles "other friendly units have" (547 Darius) and "Bird/Cat/Dog/Poro/Ivern +1M in Brush" (561 aura clause); battlefield replace machinery exists (561).

| id | name | type | missing/partial clause | severity |
|----|------|------|------------------------|----------|
| 503 | Relentless Pursuit | spell | Delayed "When I conquer, you may move me to my base" grant is a documented no-op; move/attach are best-effort (no agent choice of destination/equipment) | medium |
| 505 | Void Rush | spell | Almost entirely stubbed: no reveal of top 2, no "play banished card at -2 cost", "draw any not banished" reduced to a flat draw 1; targets a unit instead of own deck-top | high |
| 513 | Arise! | spell | Bare stub — no effect. Should play a 2[M] Sand Soldier token per Equipment controlled, then ready up to two | high |
| 516 | Hostile Takeover | spell | "Ready it / start combat if other enemies / else conquer" rider not modeled; end-of-turn recall-to-base not modeled (only take-control + ready) — documented | medium |
| 517 | Battle Mistress | legend | "When you recycle a rune, exhaust me to play a Gold token" clause not wired (no recycle-rune trigger event); only the enemy-death ready clause works | medium |
| 519 | Grand Duelist | legend | Entire effect unimplemented: "when one of your units becomes [Mighty]..." has no engine event — documented no-op | high |
| 520 | Riposte | spell | Completely wrong: gives +1[M] to a friendly unit instead of countering a spell + granting +[M] equal to that spell's Energy cost. No counter logic at all | high |
| 522 | Forge of the Fluft | battlefield | Empty — grants friendly legends "[E]: Attach an Equipment to a unit" while you control it; not implemented | high |
| 525 | Marai Spire | battlefield | Empty — "friendly [Repeat] costs cost [1] less" not applied (repeat-cost loop reads only the spell's own text; no battlefield reduction hook) | medium |
| 526 | Minefield | battlefield | triggerType set but no onTrigger body — "put top 2 of Main Deck into trash" on conquer not implemented | high |
| 528 | Power Nexus | battlefield | triggerType set but no onTrigger body — "when you hold here, pay [A][A][A][A] to score 1 point" not implemented | high |
| 533 | The Papertree | battlefield | triggerType set but no onTrigger body — "when you hold here, each player channels 1 rune exhausted" not implemented | high |
| 535 | Veiled Temple | battlefield | Only readies a friendly gear; missing "If it's an Equipment, you may detach it" clause | low |
| 537 | Vayne, Hunter | unit | Empty — "if opponent controls a battlefield, I enter ready" and "when I conquer, pay [1] to return me to hand" both unimplemented ([Assault 3] is engine-handled) | high |
| 539 | Ahri, Inquisitive | unit | -2[M] applied without the "to a minimum of 1 [M]" floor (giveTemporaryMight called with no minimum arg) | low |
| 540 | Bard, Mercurial | unit | Only target requirements set; the optional "exhaust legend" additional cost and "move any number of units to an open battlefield" effect are unimplemented | high |
| 544 | Yone, Blademaster | unit | Conquer damage clause omits the "battlefield that was uncontrolled" precondition — fires on any conquer | low |
| 546 | Yasuo, Windrider | unit | Empty — "the third time I move in a turn, you score 1 point" unimplemented ([Ganking] engine-handled) | high |
| 548 | Karma, Channeler | unit | Empty — [Vision] (look/recycle top on play) and "when you recycle 1+ cards to Main Deck, buff a friendly unit" both unimplemented | high |
| 550 | Soraka, Wanderer | unit | Empty — "assigned combat damage last" and the death-replacement (heal/exhaust/recall lower-Might allies) both unimplemented | high |
| 551 | Mechanized Menace | legend | "Your Mechs have [Shield]" not granted — engine aura parser only matches "your mechs each have", not "your mechs have" | high |
| 556 | Chem-Baroness | legend | "While score within 3 of Victory Score, your Gold [ADD] an additional [1]" passive not modeled (documented); token-on-hold clause works | medium |
| 561 | Brush | battlefield | +1[M] aura is engine-handled; but "When you score here, you may replace this with the battlefield it replaced" reverse-replace trigger is not wired on the card | medium |
| 562 | Mischievous Marai | unit | triggerType WhenYouPlayMe set but no onTrigger body — "deal 2 to an enemy unit here" unimplemented | high |
| 563 | Prepared Neophyte | unit | Empty — conditional "+4 [M] if you've spent [4]+ on a spell this turn" unimplemented | medium |
| 568 | Smite | spell | Missing "if it would die this turn, banish it instead" replacement (normal kill only) | medium |
| 570 | Towering Pairofant | unit | Empty — "if a unit died this turn, I enter ready" unimplemented ([Assault] engine-handled) | medium |
| 572 | Vault Breaker | spell | Grants [Assault 2] only; missing the [Ganking] grant | medium |
| 574 | Lord Broadmane | unit | triggerType WhenYouPlayMe set but no onTrigger body — "give your other units here [Assault] this turn" unimplemented | high |
| 576 | Monster Harpoon | spell | Missing "if you control a facedown card, deal 4 to it instead" branch (always deals 2) | medium |
| 580 | Yeti Brawler | unit | triggerType WhenIConquer set but no onTrigger body — "if you assigned 3+ excess damage, play two Gold tokens exhausted" unimplemented | high |
| 582 | Dancing Grenade | spell | Missing the recast loop: "controller may play this again for [A]" + escalating "+1 Bonus Damage per time it dealt damage this turn" (only the base deal-2 works) | high |
| 588 | Xerath, Freed | unit | Empty — activated "[R],[E]: Deal 3 to a unit (only while at a battlefield)" unimplemented | high |
| 591 | Red Brambleback | unit | Empty — "your conquer effects here trigger an additional time" and "when I conquer, buff a friendly unit" unimplemented ([Accelerate] engine-handled) | high |
| 592 | Vi, Hotheaded | unit | Empty — activated "[2][R]: Double my Might this turn" unimplemented ([Deflect] engine-handled) | high |
| 594 | Double Trouble | spell | Stubbed: flat "draw 1" instead of "look at top 3, may reveal+draw a unit, recycle the rest" (the [Repeat] cost loop is engine-handled, but the look/select effect is wrong) | high |
| 599 | Shadow Watcher | unit | Empty — "if a friendly unit died during your Beginning Phase this turn, I enter ready" unimplemented | medium |

Notes / non-gaps verified:
- 547 Darius "Other friendly units have +1 [M] here" — generic aura parser matches "other friendly units have" (applies as all-friendly aura; printed "here" scope is slightly broader but counted as implemented).
- 597 Monch, 587 Undying Legion ([Legion] trash-play), 578 Scorchclaw level-up, 590 Pyke, 585 Katarina, 593 Combat Experience, 600 Skyward Strike, 596 Herald of Spring, 595 Frisky Hunter, 583 Grim Apothecary, 581 Blighted Battleaxe, 584 Jhin, 589 Inviolus Vox, 521/524/527/529/531/532/534 battlefields, 502/506/512/514/552/554/555 legends, seals (536/538/541/542/545/549), 504/507/508/509 gear, 543 Sett, 510/511/518/571/575/577/579 spells — implemented.

## Range 601-700

Range note: ids 601–700 (600 excluded). All 100 ids in range have a per-card .cpp.

Key systemic finding: `Card::requiresLevel()` / `Card::levelThreshold()` are declared in
`src/cards/card.h` but are **never consumed by any engine code** (no references outside card.h
except the cards that override them). Any card whose Level-N effect is expressed ONLY via these
hooks has that effect entirely unimplemented. Affected here: 621, 637 (level part), 653 (cost part), 660.

| id | name | type | missing/partial clause | severity |
|----|------|------|------------------------|----------|
| 607 | Forgotten Signpost | gear | Bare GearCard stub. Entire `[Action][>]` ability (exhaust a unit, [E]: move a different unit to that unit's location) unimplemented. | high |
| 619 | Alpha Wildclaw | unit | Stub. Missing static: "Your units here with less Might than me can't be chosen by enemy spells/abilities." (Tank is engine-handled.) | high |
| 620 | Lillia, Protector of Dreams | unit | Stub. Missing both: "+1 [M] this turn when you play a token unit" and "Your token units have [Tank]" aura. | high |
| 621 | Master Yi, Unstoppable | unit | Only dead level hooks. Tiered cost reductions (L3 [2][G], L6 [4][G][G], L11 [6][G][G][G]) and L16 "can't be chosen by enemy spells/abilities" all unimplemented. | high |
| 624 | Dramatic Visionary | unit | Declares WhenIDie trigger but no `onTrigger` — [Deathknell] [Predict 2] unimplemented. | high |
| 626 | Fate Weaver | unit | Approximation: just `drawCards(1)`. Missing look-top-4, reveal-a-spell-cost-4+ filter, draw it, recycle the rest. | high |
| 627 | Icevale Archer | unit | Declares WhenIAttack trigger but no `onTrigger` — "pay [1] to give a unit here -1 [M] this turn" unimplemented. | high |
| 630 | Spectral Centaur | unit | Stub. Missing "When another friendly unit dies, give me +2 [M] this turn." | high |
| 636 | Frigid Jewel | gear | Stub. Missing "When you draw your second card each turn, give a friendly unit +2 [M] this turn." | high |
| 637 | Gustwalker | unit | Hunt 2 XP implemented, but L3 static "+1 [M] and [Ganking]" relies on dead level hooks (no applyPassiveAura) — unimplemented. | medium |
| 638 | Petal Pixie | unit | Stub. Missing self-buff aura "+1 [M] for each of your [Temporary] units at my battlefield." | high |
| 639 | Soul Shepherd | unit | Stub. Missing aura "Your token units have +1 [M]." | high |
| 646 | Sprite Queen | unit | Declares only WhenYouPlayMe (no `onTrigger`). Missing both the play-time AND start-of-Beginning-Phase 3 [M] Sprite token creation. | high |
| 647 | Sumpworks Map | gear | Stub. Missing "When an opponent scores, draw 1." ([Temporary] engine-handled.) | high |
| 648 | Zilean, Time Mage | unit | Stub. Missing "Once each turn, if you would play a token unit ... play that token and an additional copy instead." | high |
| 649 | Blue Sentinel | unit | Partial. "Your hold effects for holding here trigger an additional time" entirely missing; the [Add][A] "next Main Phase" deferral is approximated as immediate (documented). (Shield 2 engine-handled.) | medium |
| 651 | Jhin, Meticulous Killer | unit | Stub. Missing alt-cost "If you've spent [4]+ on a spell this turn, you may play me for [B]." (Vision engine-handled.) | medium |
| 652 | LeBlanc, Everywhere at Once | unit | Stub. Missing "Your [Temporary] effects at my battlefield don't trigger." (Backline engine-handled.) | high |
| 653 | Concentrate | spell | Draw 2 works; L6 (-2) and L11 (-4) cost reductions rely on dead level hooks — unimplemented. | medium |
| 660 | Targonian Visionary | unit | Only dead level hooks. L11 static "+4 [M]" unimplemented (no applyPassiveAura). | high |
| 663 | Call to Battle | spell | Wrong: just `moveToBase(target)`. Should move a friendly unit to a battlefield you control, then have an opponent move one of their units to the same battlefield. | high |
| 664 | Crowd Favorite | unit | Hunt implemented; missing activated "Spend 2 XP: [Buff] me." | medium |
| 667 | Imposing Challenger | unit | Partial/wrong: moves target to base. Should be optional "move an enemy unit here with less Might than me to a different battlefield" (no might filter, no optionality, wrong destination). | high |
| 669 | Stare Down | spell | Partial: moves ALL enemy units (anywhere) to base. Missing "at that battlefield" restriction and "with less Might than the chosen unit" filter. Gain 1 XP is present. | high |
| 670 | Wily Newtfish | unit | Stub. Missing conditional "If you've gained XP this turn, I have +1 [M] and [Ganking]." | high |
| 672 | Clash of Giants | spell | `onResolve` empty; only count=1 target. Missing "Choose two units; they deal damage equal to their Mights to each other." | high |
| 678 | Poppy, Paragon | unit | Declares WhenYouPlayMe (no `onTrigger`). Missing "if an opponent's score is within 3 of Victory Score, ready me and gain 3 XP." (Deflect engine-handled.) | high |
| 682 | Rengar, Trophy Hunter | unit | Stub. Missing "I can [Ambush] to a battlefield with enemy units even if you don't have units there" (extended Ambush placement). (Base Ambush engine-handled.) | medium |
| 684 | Crescent Guardian | unit | Stub. Missing "If you've played a spell this turn, you may pay [P] as additional cost; if you do, I enter ready." | high |
| 686 | Isolate | spell | Partial: moves enemy to base only. Missing "then, if there's an enemy unit alone at that battlefield, draw 1." | medium |
| 691 | Vicious Snapjaws | unit | Stub. Missing "When another friendly unit dies, gain 1 XP." | high |
| 692 | Walking Roost | unit | `onTrigger` empty (and spurious target req). Missing "choose an opponent; they play a 1 [M] Bird unit token with [Deflect]." (Deflect on self engine-handled.) | high |
| 697 | Insightful Investigator | unit | Wrong: unconditionally draws 1. Missing reveal-opponent-hand, "pay 2 XP to choose a card; if you do, they discard it and draw 1." | high |
| 699 | Sinister Poro | unit | Declares WhenIAttack (no `onTrigger`). Missing "pay [1] to move an enemy unit here to its base." | high |

## Range 701-790

Range covers card_ids 701–787 (788–790 do not exist). Engine-handled keywords
([Tank], [Deflect], [Ambush], [Backline], [Vision], [Assault], [Hidden],
[Action]/[Reaction] timing, etc.) are not counted as gaps. Rows below are real
effect-clause gaps only.

| id | name | type | missing/partial clause | severity |
|----|------|------|------------------------|----------|
| 704 | Heedless Resurrection | spell | Empty SpellCard — entire effect missing: additional-cost "kill a friendly unit", then "play a unit from your trash costing no more E/P than the killed unit, ignoring cost". Nothing implemented. | high |
| 706 | Maduli the Gatekeeper | unit | Empty UnitCard — "I can't be readied" not implemented; "[P]: move me to an occupied enemy battlefield if my Might > total enemy Might there" activated ability missing. | high |
| 708 | Syndra, Transcendent | unit | Empty UnitCard — "While I'm in a showdown, your spells have [Repeat] [2][P]" spell-modifier not implemented. | high |
| 710 | Cursed Sarcophagus | gear | Trigger banishes on-board friendly units (iterates `obj.location.has_value()`) instead of units from trash — wrong zone. Also "[E]: Play a unit banished with this (pay its costs)" activated ability entirely missing. | high |
| 715 | Carrion Dredger | unit | WhenIDie trigger declared but onTrigger has NO body — [Deathknell] "Play a 1[M] Bird token with [Deflect] to your base" not implemented. | high |
| 716 | Crimson Pigeons | unit | Empty UnitCard — "+2 [M] while I'm attacking with another unit" conditional buff not implemented. | medium |
| 717 | Heroic Charge | spell | Buffs the friendly target then stuns the SAME friendly target (`targets[0]`). Text: +1[M] to a friendly unit AND [Stun] an *enemy* unit at its location. Stuns wrong unit; no enemy target collected. | high |
| 719 | Scrutinizing Sergeant | unit | Gains a flat 1 XP. Text: "gain 1 XP for each friendly unit" — should count friendly units. | medium |
| 722 | Ultrasoft Poro | unit | Activated ability declared (exhaust) but NO onActivate body — "Play two 1[M] Bird tokens with [Deflect]" + "only while at a battlefield" gate missing. | high |
| 723 | Divining Shells | gear | Empty GearCard ([Vision] is engine-handled) — "[Action][>] Kill this, [E]: Give a unit +2[M] this turn" activated ability missing. | high |
| 725 | Mageseeker Investigator | unit | Empty UnitCard — "Opponents must pay [A] for each unit beyond the first to move multiple units to my battlefield at the same time" move-cost modifier not implemented. | medium |
| 726 | Safety Inspector | unit | Empty UnitCard — optional "spend 3 XP" additional cost + on-play "each player must kill one of their units (you skip if you paid)" not implemented. | high |
| 728 | Stalking Wolf | unit | Empty UnitCard ([Ambush] engine-handled) — additional-cost "kill a Bird/Cat/Dog/Poro you control" + "you may play me to its battlefield" not implemented. | high |
| 729 | Starhound | unit | Bounces an on-board friendly target to hand. Text: "return a Bird/Cat/Dog/Poro from your TRASH to your hand". Wrong zone (board not trash), no tag filter. | high |
| 730 | Undying Loyalty | spell | Empty SpellCard — "[2] less if you choose a Bird/Cat/Dog/Poro" cost reduction + "play a unit (cost ≤2 and ≤[A]) from trash ignoring cost" not implemented. | high |
| 731 | Ashe, Focused | unit | Reveal + banish implemented; the "when they hold, return it to their hand (even if I'm gone)" rider NOT implemented (documented — no board-independent hold hook). | medium |
| 732 | Atakhan | unit | Only target-requirements stub. "Kill a friendly unit as additional cost → cost reduced per E/P it cost" not implemented; "When I attack, the defender must kill one of their units here" trigger missing. ([Ganking] engine-handled.) | high |
| 733 | Galio, Indefatigable | unit | Empty UnitCard ([Deflect]/[Tank] engine-handled) — "I don't deal combat damage" not implemented. | medium |
| 740 | Poppy, Defender of the Meek | unit | Empty UnitCard ([Ambush]/[Tank] engine-handled) — optional "spend 3 XP → cost [3] less" not implemented. | medium |
| 741 | Rift Herald | unit | WhenIDie trigger declared but NO onTrigger body. Both clauses missing: "when I move to a battlefield, look top 3, draw a unit, recycle rest" AND the [Deathknell] "play a unit from hand to base ignoring Energy cost". | high |
| 749 | Bashful Bloom | legend | Token created with fixed energy=4 cost; "this ability costs [1] less for each friendly unit with [Temporary]" cost reduction not implemented. (Token also tagged "Fae" vs text "Sprite".) | medium |
| 758 | Void Assault | spell | Always moves both chosen units to base (`moveToBase`). Text allows moving to a battlefield ("if both move to a battlefield you don't control, you're the attacker") — battlefield destination + attacker designation unsupported. | medium |
| 762 | Altar of Blood | battlefield | Empty BattlefieldCard — death-replacement "if a unit here would die during combat, its controller may pay [A][A][A] to heal/exhaust/recall it" not implemented. | high |
| 768 | Frozen Fortress | battlefield | Damages only `targets[0]` (single target). Text: "deal 1 to EACH unit here" (all units, both players, each player's Beginning Phase). | high |
| 769 | Gardens of Becoming | battlefield | Empty BattlefieldCard — grant units here the ability "[E]: Gain 1 XP" not implemented. | high |
| 772 | The Academy | battlefield | WhenYouHoldHere trigger declared but NO onTrigger body — "give your next spell this turn [Repeat] equal to its base cost" not implemented. | high |
| 773 | Trapping Grounds | battlefield | WhenYouConquerHere trigger declared but NO onTrigger body — "if you assigned 3+ excess damage, play a 1[M] Bird token with [Deflect]" not implemented. | high |
| 774 | Valley of Idols | battlefield | Empty BattlefieldCard — "when a player plays a unit here, they may pay [1] to [Buff] it" not implemented. | high |
| 779 | Veteran Poro | unit | Derives from plain UnitCard, so [Weaponmaster] (a Play-Effect triggered ability, CR 821) is NOT implemented — should equip an Equipment on play. (Sibling card 421, same name, correctly uses WeaponmasterUnit base.) | medium |
| 784 | Wuju Master | legend | Only [Level 6] threshold modeled ("+1 [M]"). The [Level 11] "Your units enter ready" tier is not implemented (single-threshold level stub). | medium |
| 783 | Piltover Enforcer | legend | Ready-a-unit implemented, but the "if you assigned 3 or more excess damage" gate is approximated as always-true (documented). | low |
| 751 | Alpha Strike | spell | Damage auto-spread round-robin; text "split among enemy units" should be the controller's distribution choice. Functional approximation. | low |
| 755 | Moonfall | spell | "Choose a battlefield where you have units" auto-picks the first such BF; the move target also auto-picks the first movable enemy rather than agent choice. Functional approximation. | low |
| 763 | Amateur Recital | battlefield | "You may move a unit..." optionality lost — target requirement is mandatory (no optional flag), forcing a move if any legal target exists. | low |
| 712 | Vex, Apathetic | unit | [Stun] implemented, but "They can't move it this turn" is not modeled (no per-unit can't-move flag) — documented. | low |

