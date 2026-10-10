# 8. Cards — full review (all 955 files)

Every card file under `src/cards/<type>/` was read in full and compared against
its own `ability_text`: each sentence, trigger type, targets, numbers, costs,
optional vs mandatory, durations, keywords and def data, auras, and C++
defects. Nothing was built or run; all findings come from reading code.

**Result: 335 findings — 114 high, about 134 medium, about 87 low.** "High"
means the card's effect is missing or wrong in normal play.

## Where the findings are

Per-card findings live in one file per batch, each a checkbox list with
file:line, the card text, what the code does, and a game scenario.

| Batch | Files | Scope | Findings | High |
|---|---|---|---|---|
| [00](cards/batch-00.md) | 83 | battlefields, gear 17–387 | 31 | 10 |
| [01](cards/batch-01.md) | 90 | gear 396+, legends to 928 | 40 | 20 |
| [02](cards/batch-02.md) | 93 | legends 930+, runes, spells to 363 | 42 | 16 |
| [03](cards/batch-03.md) | 91 | spells 368–933 | 39 | 14 |
| [04](cards/batch-04.md) | 89 | spells 939+, units to 218 | 25 | 8 |
| [05](cards/batch-05.md) | 91 | units 219–494 | 39 | 12 |
| [06](cards/batch-06.md) | 89 | units 495–709 (incl. reprints 537–569) | 46 | 20 |
| [07](cards/batch-07.md) | 88 | units 711–954 (Vendetta, tokens) | 18 | 4 |
| [08](cards/batch-08.md) | 81 | re-read: hand-rolled / finalization cards, part 1 | 18 | 1 |
| [09](cards/batch-09.md) | 81 | re-read, part 2 | 20 | 5 |
| [10](cards/batch-10.md) | 79 | re-read, part 3 | 17 | 4 |
| **Total** | **955** | | **335** | **114** |

Batches 08–10 are the 241 files earlier passes had read only for narrow defect
classes; they list only what is new beyond [03-cards.md](03-cards.md) and
[07-cards-second-pass.md](07-cards-second-pass.md). Batches 02 and 04–06 include
the file ranges in their names, not strict type boundaries.

## A. Root causes — fix these first

About half of the high findings trace to a handful of engine or shared-helper
defects. Fixing the root cause fixes every card listed under it, so work this
section before the per-card lists. Items already in an earlier plan file are
cross-referenced.

- [ ] **8.1 Engine text-parsed auras collide with card text** (extends 02 item 2.1)
  - The parser at `game_engine.cpp:4500-4795` matches substrings of `ability_text` and applies an aura on top of, or instead of, the card's own.
  - Newly confirmed victims: Leona, Zealot 79 (every unit at her battlefield, herself included, gets -8 [M] min 1), Allay 603 (enemies there get [Deflect]; allies get it twice), Darius, Executioner 243 / 547 and Garen, Commander 311 ("here" trails the clause, so the aura is global), Wizened Elder 65 (+1 twice), Wuju Master 784 (no Level gate), Rage Amplifier 805 (+1 extra), Rumble, Hotheaded 349.
  - Fix: delete the parser; give Darius 243/547 and Leona 79 a card-side `applyPassiveAura` first, since they rely on it.

- [ ] **8.2 Permanent `CostModifier` pushed on every aura pass, never removed**
  - `recalculateAuras` (`game_engine.cpp:4389-4455`) does not clear `cost_modifiers`; the only erasers are the end-of-turn `this_turn_only` sweep (`game_state.h:295`) and the one-shot `next_*` sweep (`game_engine.cpp:5877`).
  - Ornn's Forge 527 (gear discount stacks without limit, outlives control), Eager Apprentice 84, Herald of Scales 140 (-2 per pass, no Dragon filter), Vex, Cheerless 467 (two modifiers per pass, all card types). After a few cleanups every card that player plays is near free for the rest of the game.
  - Related scope errors: Vaults of Helia 775 taxes spells and gear as well as units; Risen Altar 950.
  - Fix: tag aura-sourced modifiers and clear them at the top of `recalculateAuras`, the same way `aura_effects` are rebuilt.

- [ ] **8.3 Aura-granted [Assault N] / [Shield N] add no Might**
  - The aura fold (`game_engine.cpp:4843-4850`) sums only `might_bonus` and sets the keyword bit; `recomputeMight` (`game_object.h:325-331`) adds only the object's own `assault_value` / `shield_value`. `auraKeyword()` in `card_helpers.h` exists to add the conditional Might and is not used.
  - No effect at all: Serrated Dirk 332, Cloth Armor 387, Black Flame Altar 764, Purifier 502, Mechanized Menace 551, Rumble, Hotheaded 349 ("Your Mechs each have [Assault]").
  - Fix: fold `keyword_value` in the engine, or route every one through `auraKeyword()`.

- [ ] **8.4 Aura-granted keywords the engine never honours**
  - Vision is read from base keywords only, at play time, before any aura exists (`trigger_manager.cpp:317`): Forecaster 388 does nothing.
  - `Keyword::Weaponmaster` is read nowhere: Emperor of the Sands 512.

- [ ] **8.5 Gear never gets `WhenIDie`**
  - `UnitDiedEvent` is emitted only for units (`effect_executor.cpp:261`, `game_engine.cpp:6419`); a dying gear emits only `LeftBoardEvent`, which `TriggerManager` does not subscribe to.
  - Treasure Trove 186 does nothing; Scrapheap 182's "killed" branch is dead; Sprite Fountain 640's Deathknell never fires.

- [ ] **8.6 Multi-target spell generation is wrong**
  - The generator (`game_engine.cpp:3369`) turns any `count == 2` requirement into one-friendly-plus-one-enemy pairs, and emits a single target for any other count.
  - Singularity 105 and Piercing Light 346 always hit one of the caster's own units, and are not offered when either side has no target.
  - Deathgrip 484 and Smoke and Mirrors 645 (two friendly targets) are never offered from hand.
  - Bellows Breath 402 (`count = 3`) only ever gets one target.

- [ ] **8.7 `requiresLegion()` and `requiresLevel()` are never read by the engine**
  - Declared in `card.h:639`, called nowhere. Cards that rely on them have no gate: Hand of Noxus 296, Vanguard Captain 218, Sun Disc 21, Undying Legion 587, Scorchclaw 578. (Scrapyard Champion 20 and Trifarian Gloryseeker 217 check `cards_played_this_turn` themselves and work.)

- [ ] **8.8 Keywords set on the def because the text mentions them**
  - `Keyword::Temporary` on the card itself — the unit is killed at its controller's next Beginning Phase (`game_engine.cpp:761`): Sprite Mother 106, Trevor Snoozebottom 610, Petal Pixie 638, Lillia, Fae Fawn 644, Sprite Queen 646, LeBlanc, Everywhere at Once 652.
  - Other spurious keywords: Ava Achiever 107 (`Hidden` — can be hidden for [A] and played for [0]), Gem Jammer 330 (`Ganking`), Yuumi 618 and Lillia, Protector of Dreams 620 (`Tank`), Carrion Dredger 715 and Ultrasoft Poro 722 (`Deflect`, which only their tokens should have).
  - Missing: Windsinger 459 lacks `Keyword::Hidden`, so it is never offered to hide. Baccai Witherclaw 865 and Noxian Emissary 915 lack `Deathknell` (data only — their triggers work).
  - Looks like one extraction error; each batch file lists its instances.

- [ ] **8.9 Repeat** (extends 02 item 2.4 and 07 item 7.4)
  - Repeat spells that request a choice lose the repeat and leave a stale pending choice: Frigid Touch 389, Upstage Comedy 571, Existential Dread 696, Rocket Barrage 400 — with Called Shot 443, Hard Bargain 457, Marching Orders 436, Temptation 450 and Bonds of Strength 472 that is nine cards. A granted Repeat (The Academy, Syndra) on any choice-requesting spell hits the same gap [plausible].
  - `parseRepeatCost` rejects costs written as `[Repeat] — ...`: Square Up 579 and Curtain Call 743 never repeat.
  - A Repeat cost can be paid up to 32 times; CR 820.1.c.3 allows once.

- [ ] **8.10 Unit [Reaction] is not implemented**
  - `Keyword::Reaction` is consulted only for spells (`game_engine.cpp:3260-3265`, `chain_manager.cpp:510-511`). Shen, Kinkou 241, Janna, Savior 376 and Rengar, Pouncing 348 cannot be played at Reaction timing (see also 02 item 2.7).

- [ ] **8.11 `WhenYouChooseAFriendlyUnit` never fires on the chosen unit**
  - `trigger_manager.cpp:1105-1119` skips the chosen object. Jae Medarda 463 never draws; Irelia, Fervent 380 gets +1 only when a different unit is chosen.

- [ ] **8.12 Quick-Draw gear does not attach when played in the main phase**
  - The attach needs a targeted play intent (`game_engine.cpp:2195`), which only `generateClosedStateActions` emits (`:3052-3068`). Long Sword 345, Sterak's Gage 379, Cloth Armor 387.

- [ ] **8.13 `playIgnoringCost` leaves the card in its source zone** (extends 07 item 7.10)
  - Rell, Magnetic 347 (Equipment on board and still in hand), Rift Herald 741. Fix in the executor rather than per card.

- [ ] **8.14 Off-board source or target** (extends 02 item 2.28, 07 §E)
  - `objectExists` is the only guard on about 80 more sites found in this pass (the S2 list below). One on-board guard in the executor's move / bounce / damage / kill / buff / ready functions closes most of them.

- [ ] **8.15 Equipped triggers run outside the chain**
  - `onEquippedTrigger` is called directly from `fireEquippedTriggers`, so `chain.resuming` is empty and the choice helpers return defaults: Last Rites 471 always says yes and takes the first unit in trash (see 02 item 2.17).

- [ ] **8.16 Smaller engine gaps, one card each**
  - `xp_gained_this_turn` is never incremented: Wily Newtfish 670.
  - `PlayerState::bonus_damage_dealt` is never set: Rabadon's Deathcrown 508.
  - `may_play_from_trash` / `trash_goes_to_banish` are never read: Endless Riches 809.
  - A spell that banishes itself while resolving is then filed in trash as well: Arcane Shift 515; Time Warp 122 similar.
  - Gear `onPlay` exhaust is overwritten by `resolvePermanent` (`:2183-2186`): Iron Ballista 17 enters ready.
  - A hidden card has no battlefield location, so "hidden here" checks never match: Mushroom Pouch 101.
  - An ability declaring `targets.count = 1` without an `enumerateLegalTargets` overload is never offered: Heart of Dark Ice 375.
  - `untargetable_by_enemy` is ignored by hand-rolled choices: Falling Star 29, Switcheroo 466, Angle Shot 334, Whirlwind 187.
  - Temporary Deflect is permanent and stacks (`effect_executor.cpp:667`): Jayce, Hammer in Hand 875 (confirms 02 item 2.31).
  - Legends are skipped by sweeps that require a location: Battle Mistress 517 (02 item 2.13).
  - `[A][A]` Flow can only be paid from one domain: Death Mark 931.
  - "This turn" delayed death-watches never expire: Deadly Flourish 635, Siphoning Strike 933.

## B. Cards with a missing or unimplemented effect

Each has a full entry in its batch file. Listed here so the scale is visible;
"documented" means the card's own comment admits the gap.

- **Blank or nearly blank:** Magma Wurm 11 (documented), Nine-Tailed Fox 252 (documented), Alpha Wildclaw 619 (documented), Maduli the Gatekeeper 706 (documented), Atakhan 732 (documented), Temporal Portal 401 (documented), Svellsongur 382, Spiderling 884, Undying Legion 587, Arachnoid Horror 679.
- **A whole sentence missing:** Last Breath 257 (damage), Detonate 328 (draw 2), Eclipse 625 (Predict), Confront 129, Rally the Troops 487, Renekton, Brute 879 (empower at 10 Might), Ezreal, Prodigy 470, Red Brambleback 591, Blue Sentinel 649, Herald of Spring 596, Ashe, Focused 731, Sivir, Ambitious 441, Yeti Brawler 580, Karma, Channeler 548, Chem-Baroness 556, Swift Scout 259, Crescent Strike 634 (splash), Lillia, Protector of Dreams 620, Noxian Guillotine 251, Imperial Decree 221, Bushwhack 327, Relentless Pursuit 503.
- **Multi-target or "all" effect coded as one target:** Back to Back 206, Grand Strategem 233, Decisive Strike 322, Danger Zone 501, On the Hunt 518, Icathian Rain 247, Tricksy Tentacles 616, Against the Odds 323 (flat +2).
- **Wrong scope or zone:** Anivia, Primal 148 (every battlefield), Tibbers 316 (hits bases), Cannon Barrage 127 (not combat-only), Fox-Fire 253 (kills the caster's own units first), Ride the Wind 173 (base only), Unforgiven 256, Harpoon Squad 458, Gentle Gemdragon 666, Viktor, Leader 246, Right of Conquest 577, Overzealous Fan 449 (bounces instead of moving to base).
- **Missing gate:** Loose Cannon 249 (draws regardless of hand size), Prodigal Explorer 514 (no "chosen twice" gate, wrong timing), Grandmaster at Arms 554 (attaches Equipment still in deck/hand/trash), Baited Hook 242 (cost coded as recycle-self, Power never charged), Altar of Memories 490, Forgotten Signpost 607, Yordle Explorer 422 (never draws), Seal of Strength 542 (wrong domain).

## C. Systemic lists

Cards affected by a defect already described elsewhere; one fix per class.
Full per-batch lists, with line numbers, are in each batch file's "Systemic"
section.

- [ ] **S1 Target-less triggers** — nothing new beyond the 26 in 07 item 7.1.
- [ ] **S2 Off-board source/target guarded only by `objectExists`** (see 8.14) — about 80 more cards, most low impact. Higher-impact ones: Baited Hook 242, Icathian Rain 247, Treasure Trove 186, Pit Crew 91 (no guard at all), and gear attached to an off-board unit by Armed Assailant 324, Combat Chef 414, Veteran Poro 421, Master Bingwen 448, Ornn, Forge God 407, Lucian, Merciless 435, Akshan, Mischievous 431.
- [ ] **S3 Aura multiplies with copies** (02 item 2.2) — about 45 more cards, including Captain Farron 15, Raging Soul 19, Wielder of Water 55, Wizened Elder 65, Dr. Mundo 109, Lee Sin, Centered 151, Fiora, Victorious 232, Sett, Kingpin 240, Yi, Meditative 302, Garen 311, Gearhead 391, Ornn 407, Ancient Warmonger 452, Sivir, Mercenary 464, Trusty Ramhound 480, Soraka 550, Vilemaw 622, Master Yi, Tempered 675, Crimson Pigeons 716, Brutalizer 365, Soul Sword 601.
  - Battlefields have the same problem when both players bring the same battlefield: Trifarian War Camp 289, Void Gate 291, Kinkou Temple 946, Forbidding Waste 766, Black Flame Altar 764, Ornn's Forge 527, Gardens of Becoming 769, Risen Altar 950.
- [ ] **S4 Unlimited buff** (02 item 2.14) — Arena Bar 124, Stand United 53, Overt Operation 153, Adaptatron 56, Karma 235, Poro Herder 61, Pit Rookie 136, Trifarian Gloryseeker 217, Warmog's Armor 430, Blind Monk 254, Radiant Dawn 297, Aphelios 372, Red Brambleback 591, Ivern 613, Nami 614, Enthralling Protector 724, Voidreaver 787, Navori Fighting Pit 278, Spirit's Refuge 63.
  - Exempt when fixing: Lee Sin, Ascetic 78 ("I can have any number of buffs").
- [ ] **S5 First match taken where the player (or opponent) should choose** — about 70 more cards. Each batch file lists them with what should be chosen.
- [ ] **S6 Helper ordering** (03 item 3.1) — nothing new beyond Thrill of the Hunt 745, Altar of Memories 490, Zenith Blade 258, Blast Cone 695; Keeper's Verdict 760 has it on a fallback path; Forgotten Signpost 607 has two `pickTarget` calls.
- [ ] **S7 `giveTemporaryKeyword(Temporary)` stripped the same turn** — Turn to Dust 632, with Shadow's Call 727 (07 item 7.9).

## D. Corrections to earlier plan files

These have been applied to the files named.

- **07 "Read in full and clean" list** was true only for the narrow classes that pass checked. Under the full checklist these have findings: spells 0187, 0334, 0466 (untargetable filter), battlefield 0274, legend 0259, units 0440, 0470, 0726, 0728. Still clean: 0626, 0642, 0729, 0919.
- **07 item 7.1, "engine route fixes ~22 at once"** was too optimistic. Eight of the cards (76, 130, 132, 188, 191, 199, 238, 361) have target requirements that cannot express the printed restriction ("here", "another", "from a battlefield"), so engine-chosen targets would be wrong. Only 16, 51, 82, 92, 97, 234, 330 are correct as written; the rest need card work either way.
- **07 item 7.1, Yuumi 618 row:** the def also puts [Tank] on Yuumi herself.
- **07 item 7.6:** Legion Quartermaster 367's playability gate (`hasLegalTargets`) is never consulted for a unit play, so it is free with no gear on board, like Cruel Patron.
- **03 item 3.10, Overzealous Fan 449:** the entry treated the bounce as the intended effect; the printed effect is a move to base, so the bounce itself is wrong.
- **README open items:** Baccai Witherclaw 865 and Noxian Emissary 915 Deathknell triggers work (`onUnitDied` dispatches on `cardFiresOn(WhenIDie)`, not the keyword). Turn to Dust 632 is confirmed broken (S7).

## E. Checked and clean across all 955 files

- Timing keywords ([Action] / [Reaction]) match on every spell.
- Counter spells all use peek-and-pop; chain scans appear only in `hasLegalTargets` (520, 750, 826 can offer a counter that then fizzles).
- AoE spells damage first and kill in a second pass (Elder Dragon 680 is the one exception, 07 item 7.36).
- Flow costs match the text on all ten Flow cards; Burn and Empower go through the executor as designed.
- Champion units carry their champion tag, except Allay 603 (none) and Karma 235 (wrong tag). Region tags drift on the four champion reprints.
- No card was left unread.
