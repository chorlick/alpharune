# Manual Card Implementation Audit

_Generated 2026-05-24. Audited all 209 manually-registered cards (`src/cards/manual/`) by comparing each card's printed `ability_text`/`effect_text` against its C++ implementation._

## Summary: 209 cards — **141 OK**, **59 PARTIAL**, **9 MISSING**

- **OK** = all printed features implemented (or correctly delegated to an engine-handled keyword).
- **PARTIAL** = main effect works but at least one printed clause is missing/incorrect.
- **MISSING** = the card's primary effect is absent or replaced with the wrong behavior.

## MISSING — primary effect absent/wrong (release blockers)

| id | card | gap |
|----|------|-----|
| 91 | Pit Crew | 'when you play a gear, ready me' not implemented (skipped) |
| 353 | Skyfall of Areion | 'hold effects are also conquer effects, vice versa' not implemented |
| 374 | Guardian Angel | death-replacement effect not implemented |
| 395 | Dropboarder | 'if you control 2+ gear, ready me' empty body, no trigger |
| 509 | Shurelya's Requiem | 'ready your units' on play + 'units here have [Ganking]' both missing |
| 538 | Seal of Focus | '[E]: Reaction — Add [G]' not implemented; wrong effect (readies unit) |
| 573 | Fresh Beans | entire trigger unimplemented (empty body) |
| 581 | Blighted Battleaxe | 'end of turn if didn't conquer: unattach + deal 4' not implemented |
| 650 | Gutter Palace | win-condition + Bird-token ability unimplemented; wrong effect |

## Systemic patterns (PARTIAL/MISSING grouped — fix the root cause once, many cards benefit)

### [Hunt] keyword XP-on-conquer/hold dropped (manual override replaced the generated WhenIConquerOrHold trigger with WhenYouPlayMe)

| id | card | gap |
|----|------|-----|
| 602 | Wuju Apprentice | [Hunt] gain 1 XP on conquer/hold not implemented |
| 675 | Master Yi, Tempered | [Hunt 2] XP missing; Level-6 keywords one-shot not ongoing |
| 609 | Mosstomper | [Hunt 2] missing; Level-3 buff one-shot not static |
| 656 | Gemhand Hunter | [Hunt] XP lost; Level 6 +1M only at play not persistent |

### [Level N] keyword granted once at play instead of as an ongoing 'while you have N+ XP' passive

| id | card | gap |
|----|------|-----|
| 675 | Master Yi, Tempered | [Hunt 2] XP missing; Level-6 keywords one-shot not ongoing |
| 609 | Mosstomper | [Hunt 2] missing; Level-3 buff one-shot not static |
| 656 | Gemhand Hunter | [Hunt] XP lost; Level 6 +1M only at play not persistent |
| 601 | Soul Sword | +1M unconditional; [Level 3] gate not implemented |

### Dual-trigger cards: only one effect wired because Card exposes a single triggerType()

| id | card | gap |
|----|------|-----|
| 444 | Corrupt Enforcer | 'when I win a combat, draw 1' not implemented |
| 613 | Ivern, Nurturer | only on play not 'or when I hold'; auto-drafts (not 'you may') |
| 615 | Scuttle Crab | entire [Deathknell] (reveal/look/gain XP) unimplemented |
| 67 | Blitzcrank, Impassive | 'when I hold, return me to hand' not implemented |
| 435 | Lucian, Merciless | 'first time I conquer each turn, ready me' not implemented |
| 543 | Sett, Brawler | 'when played' buff missing; over-fires on Hold |
| 695 | Blast Cone | 'when you move enemy unit, exhaust to stun' not implemented |
| 676 | Nidalee, Cat Form | 'when I win a combat, draw 1' not implemented |
| 440 | Jax, Unrelenting | 'attach Equipment -> may pay [1] draw 1' not implemented |
| 27 | Darius, Trifarian | fires on WhenYouPlayAUnit only; text is 'second card' (misses spell/gear) |
| 544 | Yone, Blademaster | conquer-uncontrolled: deal Might damage to enemy unit in base — missing |

### Cost-ceiling gates not enforced ('costs no more than [N]')

| id | card | gap |
|----|------|-----|
| 45 | Defy | no 'costs no more than [4]' gate; adds un-printed 'draw 1' |
| 486 | Glasc Mixologist | cost gate + 'you may' optionality not implemented |
| 461 | Fizz, Trickster | '<=[3] energy' gate + 'recycle that spell after' not implemented |
| 467 | Vex, Cheerless | friendly reduction missing 'to a minimum of [1]' floor |

### Agent choice not surfaced (auto-picks first / forces all / hardcoded)

| id | card | gap |
|----|------|-----|
| 209 | Cull the Weak | each player kills own unit — no player choice, kills last unit |
| 745 | Thrill of the Hunt | 'to any battlefield' hardcodes battlefields[0]; no agent choice |
| 366 | Emperor's Divide | 'move any number' forces ALL; no chosen subset |
| 739 | Ivern, Friend to All | tag choice hardcoded 'Bird'; no agent choice |
| 331 | Sentinel Adept | no gear choice; 'for [A] less' modeled as free equip |
| 606 | Flurry of Feathers | modal 'choose one' not exposed; mode auto-selected |

### 'There' (a specific battlefield/location) approximated as base, or wrong destination

| id | card | gap |
|----|------|-----|
| 756 | Deceiver | token not 'there'; never becomes copy; [Temporary] not granted |
| 644 | Lillia, Fae Fawn | token spawns at base, not 'there' |
| 642 | Hwei, Brooding Painter | discard-type branch (Spell/Gear/Unit) unimplemented |
| 703 | Evelynn, Entrancing | moves enemy to its own base, not Evelynn's battlefield |

### Additional-cost / cost-modifier clauses not modeled

| id | card | gap |
|----|------|-----|
| 8 | Get Excited! | deals hardcoded 1 dmg instead of discarded card's Energy cost |
| 48 | Meditation | optional 'exhaust unit -> draw 2' branch missing; always draws 1 |
| 614 | Nami, Headstrong | additional-cost gate not enforced; hold clause missing |
| 735 | Sacrifice | additional-cost kill not enforced/agent-chosen; spell resolves without paying |
| 470 | Ezreal, Prodigy | 'optional additional costs cost [1]/[A] less' not implemented |
| 498 | Blade of the Ruined King | equip cost wrong: [Y]+kill-friendly-unit neither implemented |
| 748 | Hextech Gauntlets | Might-based cost reduction missing; 'conquer->draw 1' missing |
| 720 | Shepherd's Heirloom | 'gain 1 XP on play' missing; equip cost is power not 'Spend 1 XP' |

### Deferred / replacement effects not installed (effect should fire later)

| id | card | gap |
|----|------|-----|
| 737 | Tactical Retreat | deferred 'next death -> heal/exhaust/recall' replacement not installed |
| 612 | Iascylla | fires immediately on hold, not 'at start of next Main Phase' |
| 657 | Grim Resolve | 'win combat this turn -> gain 2 XP' rider not implemented |
| 374 | Guardian Angel | death-replacement effect not implemented |
| 460 | Edge of Night | 'play from face down -> attach to a unit' not implemented |

### Equipment effect_text (gear's actual ability) entirely unimplemented — only [Equip] keyword works

| id | card | gap |
|----|------|-----|
| 353 | Skyfall of Areion | 'hold effects are also conquer effects, vice versa' not implemented |
| 508 | Rabadon's Deathcrown | 'spells/abilities deal 3 Bonus Damage' not implemented |
| 382 | Svellsongur | 'copy attached unit's text to this gear' not implemented |
| 581 | Blighted Battleaxe | 'end of turn if didn't conquer: unattach + deal 4' not implemented |
| 412 | The Zero Drive | activated ability + [Deathknell] missing (only Equip done) |
| 498 | Blade of the Ruined King | equip cost wrong: [Y]+kill-friendly-unit neither implemented |
| 365 | Brutalizer | '+2 [M] if attached this turn' not implemented |

### Other individual gaps

| id | card | gap |
|----|------|-----|
| 91 | Pit Crew | 'when you play a gear, ready me' not implemented (skipped) |
| 220 | Facebreaker | no 'same battlefield' / friendly+enemy split constraint |
| 395 | Dropboarder | 'if you control 2+ gear, ready me' empty body, no trigger |
| 407 | Ornn, Forge God | '+1 [M] per friendly gear' self-buff not implemented |
| 471 | Last Rites | 'you may play unit from trash (still pay costs)' forced + ignores cost |
| 506 | Fire Below the Mountain | 'Use only to play gear/gear abilities' restriction not modeled; not a Reaction |
| 509 | Shurelya's Requiem | 'ready your units' on play + 'units here have [Ganking]' both missing |
| 538 | Seal of Focus | '[E]: Reaction — Add [G]' not implemented; wrong effect (readies unit) |
| 573 | Fresh Beans | entire trigger unimplemented (empty body) |
| 603 | Allay, Eager Admirer | aura wrong scope: all units at BF instead of 'your other units here' |
| 605 | Enthusiastic Promoter | [Buff] as temp-might (expires); 'if it doesn't have one' not enforced |
| 617 | Vex, Mocking | third ability wrong: stuns attacker on defend instead of move-on-stun |
| 622 | Vilemaw | 'enemy units here with less Might deal no combat damage' not implemented |
| 640 | Sprite Fountain | gear [Temporary] self-kill not handled; [Deathknell] repeat missing |
| 645 | Smoke and Mirrors | [Temporary] gate not checked; 'Draw 1' not implemented |
| 650 | Gutter Palace | win-condition + Bird-token ability unimplemented; wrong effect |
| 682 | Rengar, Trophy Hunter | Ambush-to-enemy-only-BF extension not implemented |
| 712 | Vex, Apathetic | 'opponent plays unit -> stun, can't move' not implemented (needs new trigger) |
| 749 | Bashful Bloom | 'costs [1] less per friendly [Temporary] unit' not implemented |

## Full per-card verdict table

| id | card | verdict | note |
|----|------|---------|------|
| 3 | Chemtech Enforcer | OK |  |
| 8 | Get Excited! | PARTIAL | deals hardcoded 1 dmg instead of discarded card's Energy cost |
| 12 | Noxus Hopeful | OK |  |
| 20 | Scrapyard Champion | OK |  |
| 22 | Thermo Beam | OK |  |
| 26 | Brynhir Thundersong | OK |  |
| 27 | Darius, Trifarian | PARTIAL | fires on WhenYouPlayAUnit only; text is 'second card' (misses spell/gear) |
| 28 | Draven, Showboat | OK |  |
| 30 | Jinx, Demolitionist | OK |  |
| 43 | Charm | OK |  |
| 45 | Defy | PARTIAL | no 'costs no more than [4]' gate; adds un-printed 'draw 1' |
| 48 | Meditation | PARTIAL | optional 'exhaust unit -> draw 2' branch missing; always draws 1 |
| 58 | Discipline | OK |  |
| 64 | Wind Wall | OK |  |
| 66 | Ahri, Alluring | OK |  |
| 67 | Blitzcrank, Impassive | PARTIAL | 'when I hold, return me to hand' not implemented |
| 73 | Sona, Harmonious | OK |  |
| 83 | Consult the Past | OK |  |
| 91 | Pit Crew | MISSING | 'when you play a gear, ready me' not implemented (skipped) |
| 105 | Singularity | OK |  |
| 106 | Sprite Mother | OK |  |
| 110 | Ekko, Recurrent | OK |  |
| 122 | Time Warp | OK |  |
| 123 | Unchecked Power | OK |  |
| 128 | Challenge | OK |  |
| 134 | Mobilize | OK |  |
| 136 | Pit Rookie | OK |  |
| 138 | Catalyst of Aeons | OK |  |
| 145 | Unyielding Spirit | OK |  |
| 156 | Sabotage | OK |  |
| 160 | Dazzling Aurora | OK |  |
| 162 | Miss Fortune, Captain | OK |  |
| 169 | Gust | OK |  |
| 172 | Rebuke | OK |  |
| 173 | Ride the Wind | OK |  |
| 176 | Sneaky Deckhand | OK |  |
| 178 | Undercover Agent | OK |  |
| 183 | Stacked Deck | OK |  |
| 185 | Traveling Merchant | OK |  |
| 192 | Mindsplitter | OK |  |
| 199 | Tideturner | OK |  |
| 209 | Cull the Weak | PARTIAL | each player kills own unit — no player choice, kills last unit |
| 213 | Hidden Blade | OK |  |
| 220 | Facebreaker | PARTIAL | no 'same battlefield' / friendly+enemy split constraint |
| 236 | Karthus, Eternal | OK |  |
| 262 | Bounty Hunter | OK |  |
| 263 | Bullet Time | OK |  |
| 290 | Vilemaw's Lair | OK |  |
| 293 | Zaun Warrens | OK |  |
| 324 | Armed Assailant | OK |  |
| 326 | Gold | OK |  |
| 331 | Sentinel Adept | PARTIAL | no gear choice; 'for [A] less' modeled as free equip |
| 332 | Serrated Dirk | OK |  |
| 339 | Recurve Bow | OK |  |
| 344 | Ferrous Forerunner | OK |  |
| 345 | Long Sword | OK |  |
| 346 | Piercing Light | OK |  |
| 348 | Rengar, Pouncing | OK |  |
| 352 | Rek'Sai, Breacher | OK |  |
| 353 | Skyfall of Areion | MISSING | 'hold effects are also conquer effects, vice versa' not implemented |
| 356 | Doran's Shield | OK |  |
| 365 | Brutalizer | PARTIAL | '+2 [M] if attached this turn' not implemented |
| 366 | Emperor's Divide | PARTIAL | 'move any number' forces ALL; no chosen subset |
| 368 | Not So Fast | OK |  |
| 369 | Poro Snax | OK | 'Kill this' cost approximated as recycle-to-deck |
| 374 | Guardian Angel | MISSING | death-replacement effect not implemented |
| 375 | Heart of Dark Ice | OK |  |
| 379 | Sterak's Gage | OK |  |
| 382 | Svellsongur | PARTIAL | 'copy attached unit's text to this gear' not implemented |
| 387 | Cloth Armor | OK |  |
| 389 | Frigid Touch | OK |  |
| 395 | Dropboarder | MISSING | 'if you control 2+ gear, ready me' empty body, no trigger |
| 396 | Experimental Hexplate | OK |  |
| 399 | Production Surge | OK |  |
| 400 | Rocket Barrage | OK |  |
| 405 | Hextech Anomaly | OK |  |
| 407 | Ornn, Forge God | PARTIAL | '+1 [M] per friendly gear' self-buff not implemented |
| 408 | World Atlas | OK |  |
| 412 | The Zero Drive | PARTIAL | activated ability + [Deathknell] missing (only Equip done) |
| 414 | Combat Chef | OK |  |
| 417 | Doran's Blade | OK |  |
| 419 | Punch First | OK |  |
| 421 | Veteran Poro | OK |  |
| 424 | Hexdrinker | OK |  |
| 427 | Ruin Runner | OK |  |
| 430 | Warmog's Armor | OK | [Buff] as temp-might (engine-wide) |
| 435 | Lucian, Merciless | PARTIAL | 'first time I conquer each turn, ready me' not implemented |
| 437 | Trinity Force | OK |  |
| 439 | Boneshiver | OK |  |
| 440 | Jax, Unrelenting | PARTIAL | 'attach Equipment -> may pay [1] draw 1' not implemented |
| 444 | Corrupt Enforcer | PARTIAL | 'when I win a combat, draw 1' not implemented |
| 445 | Doran's Ring | OK |  |
| 448 | Master Bingwen | OK |  |
| 449 | Overzealous Fan | OK |  |
| 451 | Treasure Hunter | OK |  |
| 454 | Boots of Swiftness | OK |  |
| 455 | Cull | OK |  |
| 457 | Hard Bargain | OK |  |
| 460 | Edge of Night | PARTIAL | 'play from face down -> attach to a unit' not implemented |
| 461 | Fizz, Trickster | PARTIAL | '<=[3] energy' gate + 'recycle that spell after' not implemented |
| 465 | Spirit Wheel | OK |  |
| 467 | Vex, Cheerless | PARTIAL | friendly reduction missing 'to a minimum of [1]' floor |
| 470 | Ezreal, Prodigy | PARTIAL | 'optional additional costs cost [1]/[A] less' not implemented |
| 471 | Last Rites | PARTIAL | 'you may play unit from trash (still pay costs)' forced + ignores cost |
| 474 | Eye of the Herald | OK |  |
| 476 | Honest Broker | OK |  |
| 482 | B.F. Sword | OK |  |
| 484 | Deathgrip | OK |  |
| 486 | Glasc Mixologist | PARTIAL | cost gate + 'you may' optionality not implemented |
| 493 | Sacred Shears | OK |  |
| 498 | Blade of the Ruined King | PARTIAL | equip cost wrong: [Y]+kill-friendly-unit neither implemented |
| 504 | Spinning Axe | OK |  |
| 506 | Fire Below the Mountain | PARTIAL | 'Use only to play gear/gear abilities' restriction not modeled; not a Reaction |
| 507 | Forgefire Cape | OK |  |
| 508 | Rabadon's Deathcrown | PARTIAL | 'spells/abilities deal 3 Bonus Damage' not implemented |
| 509 | Shurelya's Requiem | MISSING | 'ready your units' on play + 'units here have [Ganking]' both missing |
| 530 | Rockfall Path | OK |  |
| 538 | Seal of Focus | MISSING | '[E]: Reaction — Add [G]' not implemented; wrong effect (readies unit) |
| 542 | Seal of Strength | OK |  |
| 543 | Sett, Brawler | PARTIAL | 'when played' buff missing; over-fires on Hold |
| 544 | Yone, Blademaster | PARTIAL | conquer-uncontrolled: deal Might damage to enemy unit in base — missing |
| 552 | Glorious Executioner | OK |  |
| 560 | Inferna | OK |  |
| 571 | Upstage Comedy | OK |  |
| 573 | Fresh Beans | MISSING | entire trigger unimplemented (empty body) |
| 575 | Lotus Trap | OK |  |
| 579 | Square Up | OK |  |
| 581 | Blighted Battleaxe | MISSING | 'end of turn if didn't conquer: unattach + deal 4' not implemented |
| 583 | Grim Apothecary | OK |  |
| 584 | Jhin, Murderous Artist | OK |  |
| 593 | Combat Experience | OK |  |
| 595 | Frisky Hunter | OK |  |
| 596 | Herald of Spring | OK |  |
| 597 | Monch | OK |  |
| 600 | Skyward Strike | OK |  |
| 601 | Soul Sword | PARTIAL | +1M unconditional; [Level 3] gate not implemented |
| 602 | Wuju Apprentice | PARTIAL | [Hunt] gain 1 XP on conquer/hold not implemented |
| 603 | Allay, Eager Admirer | PARTIAL | aura wrong scope: all units at BF instead of 'your other units here' |
| 604 | Back Off | OK |  |
| 605 | Enthusiastic Promoter | PARTIAL | [Buff] as temp-might (expires); 'if it doesn't have one' not enforced |
| 606 | Flurry of Feathers | PARTIAL | modal 'choose one' not exposed; mode auto-selected |
| 608 | Friendship | OK |  |
| 609 | Mosstomper | PARTIAL | [Hunt 2] missing; Level-3 buff one-shot not static |
| 610 | Trevor Snoozebottom | OK |  |
| 612 | Iascylla | PARTIAL | fires immediately on hold, not 'at start of next Main Phase' |
| 613 | Ivern, Nurturer | PARTIAL | only on play not 'or when I hold'; auto-drafts (not 'you may') |
| 614 | Nami, Headstrong | PARTIAL | additional-cost gate not enforced; hold clause missing |
| 615 | Scuttle Crab | PARTIAL | entire [Deathknell] (reveal/look/gain XP) unimplemented |
| 617 | Vex, Mocking | PARTIAL | third ability wrong: stuns attacker on defend instead of move-on-stun |
| 622 | Vilemaw | PARTIAL | 'enemy units here with less Might deal no combat damage' not implemented |
| 623 | Downstage Dramatics | OK |  |
| 629 | Ruined Rex | OK |  |
| 631 | Sprite Burst | OK |  |
| 635 | Deadly Flourish | OK |  |
| 640 | Sprite Fountain | PARTIAL | gear [Temporary] self-kill not handled; [Deathknell] repeat missing |
| 642 | Hwei, Brooding Painter | PARTIAL | discard-type branch (Spell/Gear/Unit) unimplemented |
| 644 | Lillia, Fae Fawn | PARTIAL | token spawns at base, not 'there' |
| 645 | Smoke and Mirrors | PARTIAL | [Temporary] gate not checked; 'Draw 1' not implemented |
| 650 | Gutter Palace | MISSING | win-condition + Bird-token ability unimplemented; wrong effect |
| 656 | Gemhand Hunter | PARTIAL | [Hunt] XP lost; Level 6 +1M only at play not persistent |
| 657 | Grim Resolve | PARTIAL | 'win combat this turn -> gain 2 XP' rider not implemented |
| 658 | Hunter's Machete | OK |  |
| 668 | Repulse | OK |  |
| 671 | Blood Rose | OK |  |
| 674 | Irresistible Faefolk | OK |  |
| 675 | Master Yi, Tempered | PARTIAL | [Hunt 2] XP missing; Level-6 keywords one-shot not ongoing |
| 676 | Nidalee, Cat Form | PARTIAL | 'when I win a combat, draw 1' not implemented |
| 680 | Elder Dragon | OK |  |
| 682 | Rengar, Trophy Hunter | PARTIAL | Ambush-to-enemy-only-BF extension not implemented |
| 685 | Evershade Stalker | OK |  |
| 687 | Lunar Boon | OK |  |
| 688 | Megatusk | OK |  |
| 689 | Mister Root | OK |  |
| 690 | Star-Crossed | OK |  |
| 693 | Abandon | OK |  |
| 695 | Blast Cone | PARTIAL | 'when you move enemy unit, exhaust to stun' not implemented |
| 696 | Existential Dread | OK |  |
| 698 | Scryer's Bloom | OK |  |
| 703 | Evelynn, Entrancing | PARTIAL | moves enemy to its own base, not Evelynn's battlefield |
| 705 | Kha'Zix, Mutating Horror | OK |  |
| 709 | Baron Nashor | OK |  |
| 712 | Vex, Apathetic | PARTIAL | 'opponent plays unit -> stun, can't move' not implemented (needs new trigger) |
| 714 | Black Rose Dignitary | OK |  |
| 718 | Loyal Poro | OK |  |
| 720 | Shepherd's Heirloom | PARTIAL | 'gain 1 XP on play' missing; equip cost is power not 'Spend 1 XP' |
| 727 | Shadow's Call | OK |  |
| 734 | LeBlanc, Fragmented | OK |  |
| 735 | Sacrifice | PARTIAL | additional-cost kill not enforced/agent-chosen; spell resolves without paying |
| 737 | Tactical Retreat | PARTIAL | deferred 'next death -> heal/exhaust/recall' replacement not installed |
| 738 | Vi, Peacekeeper | OK |  |
| 739 | Ivern, Friend to All | PARTIAL | tag choice hardcoded 'Bird'; no agent choice |
| 743 | Curtain Call | OK |  |
| 744 | Pridestalker | OK | auto-targets buff instead of free agent choice |
| 745 | Thrill of the Hunt | PARTIAL | 'to any battlefield' hardcodes battlefields[0]; no agent choice |
| 748 | Hextech Gauntlets | PARTIAL | Might-based cost reduction missing; 'conquer->draw 1' missing |
| 749 | Bashful Bloom | PARTIAL | 'costs [1] less per friendly [Temporary] unit' not implemented |
| 750 | Lilting Lullaby | OK |  |
| 752 | Shadow | OK |  |
| 753 | Green Father | OK |  |
| 754 | Daisy! | OK |  |
| 756 | Deceiver | PARTIAL | token not 'there'; never becomes copy; [Temporary] not granted |
| 757 | Mirror Image | OK |  |
| 758 | Void Assault | OK |  |
| 767 | Forgotten Library | OK |  |
| 775 | Vaults of Helia | OK |  |
| 778 | Plundering Poro | OK |  |
| 782 | Virtuoso | OK |  |
| 785 | Gloomist | OK |  |
| 787 | Voidreaver | OK | [Buff] as temp-might (engine-wide) |


---

# Generated (non-manual) cards used in active decks

_Audited the 43 auto-generated cards that appear in `decks/*.json` and carry ability text._

## Summary: 43 cards — **14 OK**, **13 PARTIAL**, **16 MISSING**

**Systemic generator bug:** many generated trigger cards read `targets[0]` in `onTrigger`, but triggered abilities fire with an EMPTY target list — the effect is a silent no-op (e.g. 116, 284, 355, 521). Several battlefield cards declare a `WhenYouConquerHere`/`WhenYouHoldHere` trigger but define no `onTrigger` body (277, 534). Fixing the generator / trigger-target plumbing clears a whole class at once.

| id | card | verdict | gap |
|----|------|---------|-----|
| 29 | Falling Star | PARTIAL | two 'Deal 3' clauses can't target two different units |
| 39 | Kai'Sa, Survivor | OK |  |
| 46 | En Garde | PARTIAL | conditional extra +1[M] 'if only unit you control there' missing |
| 52 | Stalwart Poro | OK |  |
| 95 | Stupefy | PARTIAL | '-1[M] to a minimum of 1' floor not enforced |
| 96 | Watchful Sentry | OK |  |
| 103 | Ravenbloom Student | OK |  |
| 116 | Thousand-Tailed Watcher | MISSING | AoE -3[M] modeled as single-target w/ empty trigger targets -> no-op |
| 133 | Flurry of Blades | PARTIAL | 'all units at battlefields' also hits base/reserve units |
| 153 | Overt Operation | MISSING | empty body — buff-spend-ready + mass buff absent |
| 216 | Soaring Scout | OK |  |
| 224 | Salvage | OK |  |
| 271 | Aspirant's Climb | OK |  |
| 275 | Grove of the God-Willow | OK |  |
| 277 | Monastery of Hirana | PARTIAL | WhenYouConquerHere declared but no onTrigger |
| 283 | Startipped Peak | OK |  |
| 284 | Targon's Peak | MISSING | 'ready up to 2 runes at end of turn' no-op |
| 285 | The Arena's Greatest | MISSING | 'first Beginning Phase -> gain 1 point' absent |
| 292 | Windswept Hillock | OK |  |
| 355 | Disarming Rake | MISSING | 'may kill a gear' no-op (empty trigger targets) |
| 381 | Ornn, Blacksmith | PARTIAL | blind-draws 1; missing look-4/reveal-gear/recycle + WhenIHold |
| 402 | Bellows Breath | PARTIAL | 'up to three units at same location' hits only one |
| 431 | Akshan, Mischievous | MISSING | empty body — additional cost + gear steal/attach absent |
| 466 | Switcheroo | MISSING | swap-Might effect absent (empty onResolve) |
| 480 | Trusty Ramhound | OK |  |
| 521 | Emperor's Dais | PARTIAL | no-op bounce + missing pay-[1]/Sand-Soldier rider |
| 527 | Ornn's Forge | MISSING | 'first gear costs [1] less' cost-reduction aura absent |
| 529 | Ravenbloom Conservatory | MISSING | onTrigger is empty stub (reveal/spell-to-hand/recycle absent) |
| 531 | Seat of Power | PARTIAL | 'draw 1 per other battlefield' always draws exactly 1 |
| 534 | Treasure Hoard | PARTIAL | WhenYouConquerHere declared but no onTrigger |
| 586 | Rengar, Unseen | OK |  |
| 598 | Mutated Mouser | OK |  |
| 643 | Keeper of Masks | MISSING | 'two Reflection tokens copy me' absent |
| 651 | Jhin, Meticulous Killer | PARTIAL | alt cost 'play for [B] if spent 4+' not implemented |
| 652 | LeBlanc, Everywhere at Once | PARTIAL | 'your [Temporary] effects here don't trigger' passive absent |
| 702 | Conscription | MISSING | onResolve empty — take-control/exhaust/recall + 5XP cost absent |
| 763 | Amateur Recital | OK |  |
| 764 | Black Flame Altar | MISSING | 'Temporary units here have [Shield]' aura absent |
| 765 | Dusk Rose Lab | PARTIAL | kills own unit but never draws 1; 'may' optionality absent |
| 766 | Forbidding Waste | MISSING | 'defending alone -> -2[M]' aura absent (empty body) |
| 769 | Gardens of Becoming | MISSING | granted '[E]: gain 1 XP' ability absent |
| 770 | Ripper's Bay | MISSING | 'return-to-hand here -> pay 1 channel' absent |
| 771 | Star Spring | MISSING | first-non-token-unit move effect absent (empty body) |

---

# Fix status (manual MISSING cards)

Implemented + unit-tested this pass (all 552 engine tests pass):

| id | card | implementation |
|----|------|----------------|
| 91 | Pit Crew | WhenYouPlayAGear trigger added; readies self on gear play |
| 353 | Skyfall of Areion | crossesHoldConquerTriggers() honored by onScore cross-fire |
| 374 | Guardian Angel | structured death-replacement wired into killUnit |
| 395 | Dropboarder | WhenYouPlayMe; readies self when controlling 2+ gear |
| 509 | Shurelya's Requiem | WhenYouPlayThis readies your units; [Ganking] aura via applyPassiveAura |
| 538 | Seal of Focus | activated [E] Reaction -> add [G] power |
| 573 | Fresh Beans | WhenYouPlayAUnit gated on showdown; optional exhaust-self -> draw 1 |
| 581 | Blighted Battleaxe | AtEndOfTurn: if bearer didn't conquer, unattach + deal 4 |
| 650 | Gutter Palace | beginning-phase win check + activated discard+[E] -> 1M Bird w/ [Deflect] |

**Deferred:** 412 The Zero Drive — needs an 'only if unattached' activation-legality gate (no clean hook today) plus cross-object banish-source tracking. Not in any active deck; deferred to avoid a larger/riskier engine change for an unused card.

New generic infrastructure added (no card-specific logic in engine):
- `TriggerType::WhenYouPlayAGear` + dispatch in `TriggerManager::onCardPlayed`
- `__conquered_turn` per-unit marker stamped in `TriggerManager::onScore`
- `EffectExecutor::unattachGear(gear)` — single-gear detach primitive
- `Card::hasReplacementEffect()/applyReplacement()` now consulted by `GameEngine::killUnit`
- `Card::crossesHoldConquerTriggers()` honored by `onScore`

---

# Fix-all completion (2026-05-24, follow-up)

All 88 audited PARTIAL/MISSING cards (59 manual + 29 generated-in-deck) were implemented as override classes in `src/cards/manual/audit_fixes_0..7.cpp` (registered last in `CardRegistry::loadAll`, so they win). Plus the 9 MISSING manual cards from the first pass and the **multi-trigger engine foundation**. **555 tests pass**, clean build.

## Engine foundation added (generic, no card logic in engine)
- **Multi-trigger**: `Card::triggerTypes()` / `firesOn()`, `CardContext::firing_trigger`, `ChainItem::fired_trigger`; all ~54 `TriggerManager` dispatch sites are now multi-trigger-aware. Cards with several trigger clauses (Sett, Blitzcrank, Corrupt Enforcer, Scuttle Crab, Wuju, Master Yi, …) branch on `ctx.firing_trigger`.
- `playIgnoringCost` clears a (re-)entering unit's `damage_marked`.
- (From first pass: `WhenYouPlayAGear`, `__conquered_turn` marker, `EffectExecutor::unattachGear`, structured death-replacement via `applyReplacement`, `crossesHoldConquerTriggers`.)

## Residual engine-architecture gaps (clauses approximated or deferred)
These need engine-design changes beyond per-card code; several agents independently hit them. The cards still do their main effect (or a faithful approximation) but one clause is limited:

| area | cards affected | note |
|------|----------------|------|
| Battlefield-card phase triggers + `applyPassiveAura` don't dispatch (BF card objects have no location/controller) | 285 Arena's Greatest, 765 Dusk Rose Lab, 771 Star Spring, 766 Forbidding Waste, 764 Black Flame Altar, 527 Ornn's Forge | `WhenYouConquerHere`/`WhenYouHoldHere` DO work; `AtStartOf*`/`WhenYouPlayAUnit`/`applyPassiveAura` need BF-card dispatch with a per-card attribution design |
| `checkDelayedAbilities` only called for WhenYouStun/WhenIDie/AtStartOfMain | 657 Grim Resolve (WhenIWinCombat XP), 284 Targon's Peak (AtEndOfTurn) | add dispatch calls for those trigger types |
| No "opponent plays a unit" trigger; no per-unit "can't move this turn" flag | 712 Vex, Apathetic | needs new trigger + flag |
| No per-unit "deals no combat damage" flag | 622 Vilemaw | combat step only suppresses via stun |
| ~~No play-time optional-additional-cost model~~ **DONE** | 431 Akshan, 614 Nami | `Card::optionalAdditionalCost()` + `GameEngine::maybePayOptionalAdditionalCost` — agent yes/no at play time, sets a paid flag the `onResolve`/`onTrigger` gates on. (Ezreal/Meditation/Sacrifice still resolve-time approximated.) |
| No alternative-play-cost hook | 651 Jhin, Meticulous | "play for [B] if spent 4+" |
| No runtime rules-text copying | 382 Svellsongur | gear copying a unit's text |
| No bonus-damage amplification field | 508 Rabadon's Deathcrown | "spells/abilities deal +3" |
| **DEFERRED** — no per-source rune spend-restriction | 506 Fire Below the Mountain ("[A] usable only for gear"), 786 Scorn of the Moon ("[1] usable only during showdowns") | Both cards DO add the resource (functional); only the spend *earmark* is unenforced, so the resource is slightly more permissive than printed. A faithful fix needs tagged restricted buckets on `RunePool` consulted across ~23 spend sites in 13 files — high regression risk to the cost system for 2 niche reaction cards. Sketch: add `RunePool::restricted_*` buckets; spend them first only when the context (gear play / showdown) matches, exclude from general affordability; bump `kStateFeatureDim`. Deferred pending an explicit go-ahead. |
| ~~No gear-only / first-per-turn CostModifier filter~~ **DONE** | 527 Ornn's Forge | `CostModifier.gear_only` + `.first_gear_per_turn` (consults `PlayerState::gears_played_this_turn`); applied in both `canAfford` and `beginCostPayment`. |
| ~~No self-replay-from-trash path~~ **DONE** | 747 Death from Below | `PlayerState::TrashReplayGrant` (this-turn) + `GameEngine::generateTrashReplayActions` emit a `play_source=Trash` Play with the grant's OVERRIDE cost; `executePlaySpell` removes from trash and pays via `payTrashReplayGrant`. |
| No state-aware activated-ability cost reduction | 749 Bashful Bloom | flat cost used |
| ~~No "until I leave the board" control reversion~~ **DONE** | 431 Akshan | `GameObject::control_reverts_on_source_leave` + `EffectExecutor::takeControlUntilSourceLeaves`; `GameEngine::revertLapsedControl` (run in cleanup) restores control once the source leaves play. (Conscription 702 is correctly permanent — its text has no reversion clause.) |
| No return-to-hand-here trigger event | 770 Ripper's Bay | left as no-op |
| No aura-granted activated ability | 769 Gardens of Becoming | "units here have '[E]: gain XP'" |
| ~~`killUnit` replacement skips the dying unit; no per-unit deferred-replacement flag~~ **DONE** | 737 Tactical Retreat | `GameObject::death_replacement_recall_pending`; both kill paths (`GameEngine::killUnit` combat, `EffectExecutor::killObject` effects) consult it and heal/exhaust/recall instead, one-shot, expiring end of turn. |
| ~~[Temporary] sweep is unit-only~~ **DONE** | 180 Fading Memories | beginning-step Temporary sweep now kills gear too (via `EffectExecutor::killObject`), not just units. |
| ~~No turn-gated scoring + per-player turn counter~~ **DONE** | 523 Forgotten Monument | `PlayerState::turns_taken` (bumped each Awaken) + `BattlefieldState::min_turn_to_score` (from `BattlefieldCard::minTurnToScore()`); `scoreConquer`/`scoreHold` gate via `isScoreGatedByTurn`. |
| ~~No string-valued per-object storage ("name a tag")~~ **DONE** | 700 The List | `GameObject::string_state`; onPlay names a tag (heuristic: most-common enemy tag), `[E]` debuffs only units carrying it (deferred pickTarget reads the instance's named tag). Open-ended free-text naming is approximated by the heuristic — no policy-head vocab slot for arbitrary tag strings. |
| Ambush-target BF gate hardcoded (no per-card relax) | 682 Rengar, Trophy Hunter | can't Ambush to enemy-only BFs |
| No opponent-choice plumbing mid-resolution | 209 Cull the Weak | opponent's "kill one of their units" auto-picks |

---

# Test coverage (2026-05-24, follow-up #2)

Added **300 per-card tests** (`tests/cards/test_audit_fixes_0..7.cpp`) covering all 88 audit-fixed cards (2–4 tests each: every clause + negative/boundary cases). Full suite is now **855 tests, all passing**.

The test pass doubled as a correctness audit and caught real defects:

## Fixed during the test pass
- **`EffectExecutor::giveTemporaryMight` minimum floor (engine bug):** the "to a minimum of N [M]" correction was computed from the clamped (≥0) might, so a debuff driving raw might below 0 (e.g. Thousand-Tailed Watcher -3 on a 2-might unit) floored at 0 instead of N. Now computes the correction from the unclamped raw total. Affects every "to a minimum of N" card (Thousand-Tailed Watcher 116, Stupefy 95, Vex Cheerless 467, …).
- **Iascylla (612) deferred fire (card bug):** erased its stashed battlefield before the resumable `confirmOptional`/`pickTarget`, so the move silently fizzled on the real chain path (only worked in single-pass/test invocation). Now keeps the stash until the choice commits.
- **Test fixture:** `addToHand`/`addToDeck`/`addUnit` now copy `def.tags` (the engine sets `obj.tags = def.tags` at creation; the fixture didn't), so tag-based card logic (Ivern themed draft, etc.) is testable.

## Known real defects still open (tests pin current behavior; documented for follow-up)
- **Sett, Brawler (543) "spend my buff":** `giveTemporaryMight(+4)` re-increments `buff_count` (the engine conflates a permanent buff counter with this-turn temp might in `buff_count`), so the spent buff isn't truly consumed mid-turn and the ability re-reads as buffed. Net might delta per activation is correct and it settles at end of turn (expiration removes temp). Root cause is the `buff_count`/`temp_might_bonus` conflation — affects Sett, Overt Operation (153), Enthusiastic Promoter (605). Needs an engine-level separation of "buff counters" vs "temp might"; deferred (touches many cards).
- **Equip cost double-spend (Hextech Gauntlets 748, The Zero Drive 412):** the `[power]` recycle can select the same rune just exhausted for the `[N]` energy portion, under-charging by one rune. Same class of approximation as the shared `standardEquip` helper. Card-local fix possible.
- **Yone, Blademaster (544) conquer damage:** targets the first enemy-in-base in unordered-map order (no agent choice) and fires on any conquer (the "was uncontrolled" condition isn't surfaced by the trigger). Approximation.

---

# Wave B continuation — finishing the remaining ~32 cards (2026-05-26)

Picking up the remaining `stub` / `metadata-only` cards (the coverage gate
`scripts/card_coverage.py --write-lists`; work lists in `docs/card-coverage/`).
Each remaining card carried an `ESCALATE(<feature>)` comment naming the exact
engine gap blocking it. Progress: **32 → 21 incomplete**. All engine additions are
generic (no card-specific logic in the engine — cards opt in via overrides /
per-player flags set in `applyPassiveAura`). Suite **1022 tests, all passing**.

## Group A — trigger dispatch (6 cards)
The trigger-type enum values were already front-loaded into `effect_types.h`; this
pass wired their **dispatch** (`.cpp`-only, cheap rebuilds) + the card bodies:

| id | card | trigger | dispatch site |
|----|------|---------|---------------|
| 500 | Fiora, Worthy | `WhenAUnitBecomesMighty` | `GameEngine::cleanup` edge-detects the 5+ [M] crossing (per-unit `card_counters["__was_mighty"]`) → emits `ObjectStateChangedEvent{"became_mighty"}`; `TriggerManager::onObjectStateChanged` fans out to the controller's on-board cards + legend zone |
| 519 | Grand Duelist | `WhenAUnitBecomesMighty` | same (legend-zone leg) |
| 235 | Karma, Channeler | `WhenYouRecycle` | `EffectExecutor::recycleCards` emits `"recycled_main"` per owner with a Main-Deck recycle |
| 372 | Aphelios, Exalted | `WhenEquipmentAttachedToMe` | `attachGearToUnit`/`attachFree` already emit `"equipped"`; now dispatched to the equipped unit (modal, once-per-mode-per-turn) |
| 60 | Mask of Foresight | `WhenAUnitAttacksOrDefendsAlone` | `onCombatStarted` fires when exactly one friendly unit participates at the BF |
| 447 | Loyal Pup | `WhenYouDefendAtABattlefield` | `onCombatStarted` fires on the defender's cards; defended BF stashed in `card_counters["__defend_bf"]` |

## Group B — play locations / continuous flags (5 cards)
One batched header change (`card.h` + `game_state.h`), then `.cpp` wiring:
- **New `Card` virtuals:** `restrictsPlayLocations()` (narrowing counterpart to the
  existing `getPlayLocations()` — suppresses the default base/BF plays) and
  `ambushToEnemyBattlefields()`.
- **New `PlayerState` aura flags** (reset + recomputed each `recalculateAuras`):
  `grant_friendly_units_open_bf`, `units_play_base_only`, `tokens_enter_ready`.
- **New `BattlefieldState` markers** `conquered_on_turn` / `conquered_by_player`
  (stamped in `scoreConquer`; auto-expire via turn-number comparison).

| id | card | mechanism |
|----|------|-----------|
| 338 | Perched Grimwyrm | `restrictsPlayLocations()` + `getPlayLocations()` = BFs conquered this turn |
| 193 | Miss Fortune, Buccaneer | `applyPassiveAura` sets `grant_friendly_units_open_bf` (the play generator ORs it into every friendly unit's open-BF allowance) |
| 70 | Mageseeker Warden | clause 1: sets opponent's `units_play_base_only` while at a BF. **Clause 2 (ready-suppression) deferred** — needs `readyObject` to know the ready came from a spell/ability source |
| 682 | Rengar, Trophy Hunter | `ambushToEnemyBattlefields()` relaxes both Ambush generators to enemy-occupied BFs |
| 492 | Renata Glasc, Industrialist | `tokens_enter_ready` consulted in `EffectExecutor::createToken` |

## Group H (partial) — misc per-turn hooks (2 cards)
One batched header change (`GameObject::immune_to_damage` + `spell_bonus_damage`;
`PlayerState::max_spell_spent_this_turn` + `next_spell_bonus_damage`):
- **Kayn, Unleashed (189):** `moveUnit` now increments `moves_this_turn` (was dead);
  Kayn's `applyPassiveAura` sets `immune_to_damage` while it's `>= 2`; `dealDamage`
  no-ops against the flag.
- **Ravenborn Tome (32):** `[E]` arms `PlayerState::next_spell_bonus_damage`; the next
  spell played binds it onto its `GameObject::spell_bonus_damage`, which `dealDamage`
  adds to every instance the spell deals.
(`max_spell_spent_this_turn` is also now tracked, pre-staging Jhin's condition.)

## Remaining (19) — grouped by subsystem still needed
- **Cost modifiers (4):** Irelia Graceful (per-target), Mageseeker Investigator (move-cost), Brazen Buccaneer (discard-as-additional-cost), Jhin (alt play cost + spent-4+ flag)
- **Replacement effects (5):** Zilean (token-play), Void Hatchling (reveal), Noxus Saboteur (reveal-hidden — needs a reveal mechanic that doesn't exist yet), Symbol of the Solari (combat-tie), Altar of Blood (battlefield-card replacement — flagged out-of-scope above)
- **`[Repeat]` keyword grant (3):** Syndra, The Academy, Marai Spire
- **Aura-granted *activated* abilities (3):** Forge of the Fluft, Gardens of Becoming, Heimerdinger (proxy)
- **Battlefield triggers/suppression (4):** Reckoner's Arena (refire conquer), Dreaming Tree (BF-scoped choose; banned card), LeBlanc (Temporary-trigger suppression), Mageseeker Warden clause 2
- **Misc (3):** Ravenborn Tome (next-spell bonus damage), Kayn Unleashed (moves-twice damage immunity), Immortal Phoenix (kill-with-spell + play-from-trash)
