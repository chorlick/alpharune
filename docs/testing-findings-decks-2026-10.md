# Testing phase 2026-10 — the constructed deck lists: findings

Stream `test-decks`, branch `test/decks`, engine sources at `55df656` (master after the review fix run).
Nothing in the engine, the cards or the tests was changed: this document records, it does not fix.

The request: "run a series of tests using the decks in the ./decks directory and verify behavior using real
card mechanics."

## 1. Summary

- **Games.** 1,116 fixed-seed traced games, every one finished with a winner at 8 points and exit code 0 (no
  engine error): every ordered pairing of the 18 lists (324, random vs random), the same pairings with each
  list's whole sideboard exchanged into its Main Deck (324), 72 games with `ismcts:sims=20` in one seat — these
  720 are the set the cards were read against — and a second set of 396 (a second seed per pairing, ISMCTS
  with the post-sideboard lists) used for the deck-level checks and for counts.
- **Cards.** The 18 lists hold 261 distinct cards (18 legends, 104 units, 86 spells, 21 gear, 26 battlefields,
  6 runes). Every one of them was played, put in play or channelled in the traced games.
- **Reading.** All 261 distinct cards were read against traced games, by nine readers (section 2.2): **198 in full, 63 in part, 0 not at all.** "In part" means that at least one printed clause was not seen at work in any of the 720 games (named per card in section 4, collected in section 8). "In full" is the reader's statement that every clause was compared with a traced instance; where a clause leaves no trace line (a passive, "recycle the rest") the comparison was with the rendered board or with a count over all games, and the card's row says so. The six runes count as read in full (channelling, exhausting and recycling by script over all games, the Domain of every one of 9,886 Power payments). The lead read no card clause by clause on its own account: its part is the engine-level sequences (D1-D13), the script checks and the checking of the readers' findings.
- **Findings.** **81 distinct findings, D1-D81** (13 from the lead's engine-level reading, 68 from the readers; 28 more reader findings were the same defect as another and stand under it as further evidence). By confidence: 40 CERTAIN, 23 LIKELY, 4 CODE-ONLY, 2 QUESTION, 12 with parts of different confidence (index at the head of section 5). Judge questions: section 7.
- **What is most wrong** (each is engine-wide, shows in real games of several lists, and has a one-command
  replay in section 5):
  1. **Combat is not a Showdown State for the action generator** (D5): any spell — with neither [Action] nor
     [Reaction] — is offered and played at Focus in a combat, by the defender on the opponent's turn too.
     Found independently by six readers.
  2. **Triggered abilities are not processed where the rules process them.** A declined, unpayable or
     targetless trigger stays on the chain and opens a priority window for both players (D4: 2,249 of 2,279
     declined triggers). A trigger queued during a showdown or by a conquer is left pending while Focus is
     passed, the showdown closes and combat damage is dealt (D7, D29). "Becomes Mighty" is noticed only at
     top-level cleanups (D35, D36). A unit that joins a combat already under way never gets its "When I
     attack / defend" (D68). A condition printed with the trigger is tested when the ability resolves (D19,
     D27, D33, D51, D77). Simultaneous triggers are never ordered by their controller (D40).
  3. **An option that does nothing** (D6): with such a late trigger on the chain, a non-Reaction spell is
     offered at chain priority; choosing it is silently dropped and the same decision is asked again — 52 times
     in 18 games. An agent that prefers that option never leaves the decision.
  4. **No cleanup between the items of a chain**: a unit with lethal damage stays on the board through
     priority windows (D44), Equipment whose bearer died is not recalled (D10), and continuous effects lag — a
     combat that goes straight to its damage step is fought without Forbidding Waste's -2, Wuju Bladesman's
     +2, Trifarian War Camp's +1 on a unit just moved in, a granted Shield (D60, D76, D23, D42). D60 and D76
     changed who died.
  5. **Spells go on the chain without their targets** (D8, D17, D32, D57): Star-Crossed, Switcheroo,
     Facebreaker, Defiant Dance are offered and paid for with no legal pair; a spell replayed by Fizz,
     Trickster or played by Void Burrower / Void Rush resolves with no target chosen. Deflect is charged for a
     unit Switcheroo then does not choose, or twice (D79 — the `payDeflectForChoices` lead of the fix run).
  6. **[Add] abilities are ordinary chain items** (D2): both players get priority, the opponent can respond on
     top, the resource arrives late — it cannot pay the cost it was meant for (D30, D64) and an Add activated
     with Focus hands Focus over (D31).
  7. **Choices the rules make at finalization are made at resolution** (D3, D16, D22, D24, D41, D54, D67,
     D78): move destinations, Weaponmaster's "may" and Equipment, several "you may" + target abilities.
     Equip and [Quick-Draw] attach without a chain item at all (D14, D15). Counter spells never choose their
     spell: only the top item of the chain can be countered (D21).
  8. **Options withheld**: a facedown card is never offered in its controller's own Main Phase (D25); Rengar's
     Ambush to an enemy-held battlefield never at ordinary timing, never from the Champion Zone (D37, D63);
     the engine picks the runes for every payment but a card's base cost (D9); Accelerate is not discounted by
     Ezreal, Prodigy (D43) and is asked after the play abilities' choices (D1).
  9. **Hidden information** in five kinds of log lines whose audience is both players (D11).
  10. Card-level defects with a changed outcome in a traced game, among others: Falling Star against Zhonya's
     Hourglass (D20), Astral Heron's discount kept for a later turn (D18), Star Spring triggering twice (D52),
     Master of Shadows empowered by the opponent's banish (D75), Vex, Cheerless's tax (D26), Sprite Call and
     Windsinger from facedown (D69, D70), Thrill of the Hunt onto Rockfall Path (D62), Bellows Breath's
     Repeat (D45), Diana, Lunari never triggering in combat (D29).
- **Deck-level checks** (section 2.3): winner and reason, score steps and the winning-point rule, one score per
  battlefield per turn, holds against control at the Scoring Step, prompts answered before anything else is
  asked, units exhausted on entry, pools empty at the Main Phase, card counts — all clean over the 1,116
  games except where a finding above explains the exception.

## 2. Method

### 2.1 What was run, and how to run it again

Build (lane B, `-j3`): `scripts/testing/build_lane_b.sh` under the lane lock builds the `riftbound` binary with
`-DRIFTBOUND_TEST_OPT_LEVEL=0` and `CCACHE_NAMESPACE=test-decks`. The binary reports `v0.1.0+55df656`.

Every game is one process and one command; `<sb>` is the directory `scripts/testing/sideboarded_decks.py decks <sb>`
writes the post-sideboard lists to:

```
./build/riftbound --deck1 decks/<a>.txt --deck2 decks/<b>.txt --seed <seed> \
    --agent1 <random|ismcts:sims=20> --agent2 <random|ismcts:sims=20> --trace --debug
```

A game is named `<a>__<b>__s<seed>__<agent1>__<agent2>` everywhere below; `[D:012.03]` is the prefix the trace
prints, so "game X, D:012" is a place in the output of that command. (`DECISION #n` inside a line and the
replay's decision index run ahead of D; a few citations give the decision index, written "decision #n".)

| plan (`scripts/testing/make_plan.py`) | games | seeds | set |
|---|---|---|---|
| every ordered pairing of the 18 lists, random vs random | 324 | 20261100 + 18·i + j | read |
| the same with the post-sideboard lists (`<deck>_sb`) | 324 | 20262100 + 18·i + j | read |
| per list 4 games with `ismcts:sims=20` in the list's seat | 72 | 20263000 + 4·i + k | read |
| a second seed for every ordered pairing | 324 | 20264100 + 18·i + j | checks |
| the ISMCTS plan with the post-sideboard lists | 72 | 20265000 + 4·i + k | checks |

`scripts/testing/run_deck_games.py` runs a plan (at most 3 at a time) with `--render-html on --show-hand` as
well and turns each HTML replay into text (`replay_to_text.py`): per decision the rendered board with every
hand and facedown card named, the legal options, the option chosen and the trace lines it caused. The log is
what a finding quotes; the replay text is where "what was offered" comes from.

Scripts, all in `scripts/testing/`: `defs_from_source.py` and `deck_inventory.py` (card table and per-list
inventory; the table equals `--dump-registry` field for field on all 955 cards), `write_inventory_md.py`,
`sideboarded_decks.py`, `make_plan.py`, `run_deck_games.py`, `replay_to_text.py`, `card_coverage.py` (where each
card was played / triggered / activated / resolved), `card_dossier.py` (a card's text and trace windows, or
the lines and board of one decision of one game), `deck_level_checks.py`, `hold_check.py`, `priority_check.py`,
`choice_timing.py`, `hidden_info_scan.py`, `check_citations.py` (every trace line quoted in a findings file is
looked up in the game it is cited from), `assemble_findings.py` (this document from its parts).

### 2.2 How the cards were read

Each distinct card belongs to the first list (alphabetically) that holds it and was read once, by the reader
of that list: nine readers (sub-agents of this stream, read-only), each with a brief, a card list and the
dossier tool. Per card: the printed text and the card's class side by side with traced instances — cost,
timing, where the choices were made, the targets offered, the effect, the triggers off it, the windows the
opponent got — until each clause had been seen at work or no trace exercised it. Then, per list, the
interactions the list is built around. The lead built the tools, ran the games and the deck-level scripts,
read the engine-level sequences (sections D1-D12) and checked the readers' findings: every quoted trace line
was looked up mechanically in the cited game (662 quoted fragments; the 24 not found literally are quotes the author shortened with "..." or wrote as a pattern; two wrong game names in the lead's own findings were caught this way and corrected), and 29 of the readers' 96 findings were read
in full by the lead, four of them re-run with the dossier against the log (the instances of D5, D20, D60, D79).

What the reading is not: no board was hand-built, no game was steered. A clause that random or ISMCTS play did
not exercise in 720 games is listed in section 8 as not observed.

### 2.3 Deck-level checks (by script, over all 1,116 games)

| check | looked at | result |
|---|---|---|
| every game ends `Game over: winner=N \| reason=...`, the winner at 8 or more and ahead; nothing is decided after the winning point | 1,116 games | clean; reason is always "Pn reached 8 points" |
| scores step by 1, never pass 8; the point that reaches 8 by Conquer only with every battlefield scored that turn (CR 473.1.b); the turn header repeats the score | 13,247 score lines, 226 winning conquers, 192 refused | clean (Baron Pit counted as a third battlefield) |
| one score per battlefield per player per turn (CR 472) | 6,097 conquers, 7,338 holds | clean |
| holds = the battlefields the turn player controls as the Scoring Step begins (CR 315.2.b) | 21,041 turns | clean; in 121 turns the first hold reached 8 and ended the game |
| a prompt that was requested is the next decision asked | 15,177 prompts | clean |
| who is asked at chain priority: the controller of the top item, or the other player after a pass (CR 337.4, 339, 340.4) | 76,301 priority decisions (first 720 games) | 14 not explained: all are D6 (the same player asked again after a play that did nothing) |
| units are exhausted on entry (CR 359.2.c) | 15,326 unit plays | clean at the first top-level decision after entry: 345 ready by a paid Accelerate, 41 Xin Zhao, Vigilant by his text; the transient ready state before that is D1 |
| pools are empty at the first decision of every Main Phase (CR 316.3) | 24,816 pool readings | clean; 15,490 more could not be read (the 80-column header cuts the pool off when the line is long) |
| cards per player: deck + hand + trash + banishment + Champion Zone + own permanents + attachments + facedown = 40; no name in hand / trash / banishment / facedown more often than the lists hold it | 57,126 boards (top-level Main Phase decisions) | clean except: 1,924 boards of 117 LeBlanc games over by 1 to 3 (Reflection tokens rendered under the copied unit's name), and 1 board short by one card (D10) |

Not checked, and why:

- **A seat's view of the log.** The binary prints every line without its audience, so "what P2 may read" cannot
  be replayed from stdout. Instead the emitting statement of every line kind that named a card hidden at that
  moment was read in the source (`hidden_info_scan.py` produces the list): finding D11.
- **A card in two zones, by object id.** The rendered board has names, not ids; the count and the per-name
  check above stand in for it.
- **Pools at end of turn** and at the lines the header truncates.
- **"A player never has priority with an item of their own being finalized"** beyond the prompt check: a
  pending and a finalized item look the same on the rendered chain. D7 is what that check would have found.

## 3. Inventory

### Per list

**akali** — legend Rogue Assassin (#926, Fury/Calm), champion Akali, Deadly Weapon (#808)

- battlefields: Sigil of the Storm (#282), Void Gate (#291), Forgotten Monument (#523)
- runes: 6 Fury Rune, 6 Calm Rune
- Main Deck (39): 3 Defy (#45), 3 Discipline (#58), 3 Zhonya's Hourglass (#77), 3 Stellacorn Herder (#371), 3 Mournful Witness (#815), 3 Astral Heron (#831), 3 Shuriken Flip (#927), 2 Falling Star (#29), 2 Long Sword (#345), 2 Mischievous Marai (#562), 2 Back Off (#604), 1 Noxus Hopeful (#12), 1 Block (#57), 1 Not So Fast (#368), 1 Sterak's Gage (#379), 3 Kai'Sa, Survivor (#39), 2 Blitzcrank, Impassive (#67), 1 Darius, Trifarian (#27)
- sideboard (10): 2 Ferrous Forerunner (#344), 2 Disarming Rake (#355), 1 Thermo Beam (#22), 1 En Garde (#46), 1 Against the Odds (#323), 1 Not So Fast (#368), 1 Decree of Focus (#827), 1 Akali, Silent (#825)

**azir** — legend Emperor of the Sands (#512, Calm/Order), champion Azir, Sovereign (#497)

- battlefields: Trifarian War Camp (#289), Hall of Legends (#524), Seat of Power (#531)
- runes: 7 Calm Rune, 5 Order Rune
- Main Deck (39): 3 Defy (#45), 3 Discipline (#58), 3 Hidden Blade (#213), 3 Doran's Shield (#356), 3 Brutalizer (#365), 3 Eye of the Herald (#474), 3 Guards! (#475), 3 B.F. Sword (#482), 3 Arise! (#513), 3 Soul Sword (#601), 3 Back Off (#604), 2 En Garde (#46), 2 Deathgrip (#484), 2 Hand Hammer (#814)
- sideboard (10): 2 Salvage (#224), 2 Ashe, Focused (#731), 2 Decree of Focus (#827), 2 Decree of Unity (#918), 1 Charm (#43), 1 Disarming Rake (#355)

**diana** — legend Scorn of the Moon (#786, Mind/Chaos), champion Diana, Lunari (#641)

- battlefields: Targon's Peak (#284), Seat of Power (#531), Abandoned Hall (#761)
- runes: 7 Chaos Rune, 5 Mind Rune
- Main Deck (39): 3 Stupefy (#95), 3 Ravenbloom Student (#103), 3 Traveling Merchant (#185), 3 Tideturner (#199), 3 Moonfall (#755), 2 Gust (#169), 2 Ride the Wind (#173), 2 Hard Bargain (#457), 2 Eclipse (#625), 2 Sprite Fountain (#640), 2 Star-Crossed (#690), 1 Smoke Screen (#93), 1 Rebuke (#172), 1 Acceptable Losses (#179), 1 Flash (#309), 1 Switcheroo (#466), 3 Hwei, Brooding Painter (#642), 3 Vex, Apathetic (#712), 1 Fizz, Trickster (#461)
- sideboard (10): 2 Turn to Dust (#632), 2 Decree of Insight (#848), 1 Singularity (#105), 1 Acceptable Losses (#179), 1 Fizz, Trickster (#461), 1 Star-Crossed (#690), 1 Up from the Deep (#887), 1 Tail-Cloaked Matriarch (#891)

**ezreal** — legend Prodigal Explorer (#514, Mind/Chaos), champion Ezreal, Prodigy (#470)

- battlefields: Sigil of the Storm (#282), Vilemaw's Lair (#290), Frozen Fortress (#768)
- runes: 6 Mind Rune, 6 Chaos Rune
- Main Deck (39): 3 Stupefy (#95), 3 Gust (#169), 3 Pack of Wonders (#181), 3 Wages of Pain (#393), 3 Bellows Breath (#402), 3 Deadly Flourish (#635), 3 Bewitching Spirit (#683), 2 Thousand-Tailed Watcher (#116), 2 Treasure Trove (#186), 2 Star-Crossed (#690), 2 The List (#700), 1 Singularity (#105), 1 Acceptable Losses (#179), 1 Turn to Dust (#632), 1 Sprite Fountain (#640), 3 Fizz, Trickster (#461), 3 Vex, Apathetic (#712)
- sideboard (10): 2 Kha'Zix, Mutating Horror (#705), 2 Ravenbloom Prefect (#889), 1 Singularity (#105), 1 Acceptable Losses (#179), 1 Pickpocket (#397), 1 Turn to Dust (#632), 1 Star-Crossed (#690), 1 Angler Beast (#694)

**fiora** — legend Grand Duelist (#519, Body/Order), champion Fiora, Worthy (#500)

- battlefields: Amateur Recital (#763), Risen Altar (#950), Sunken Temple (#532)
- runes: 8 Body Rune, 4 Order Rune
- Main Deck (39): 2 Challenge (#128), 3 Dazzling Aurora (#160), 2 Divining Shells (#723), 3 Elder Dragon (#680), 2 Flurry of Blades (#133), 2 Harnessed Dragon (#234), 3 Mobilize (#134), 2 Poppy, Paragon (#678), 3 Rampage (#870), 3 Rengar, Trophy Hunter (#682), 2 Repulse (#668), 3 Rift Herald (#741), 2 Riposte (#520), 3 Sacrifice (#735), 1 Sett, Brawler (#164), 3 Shepherd's Heirloom (#720)
- sideboard (10): 1 Akshan, Mischievous (#431), 1 Fiora, Victorious (#232), 1 Forge of the Future (#212), 3 Kayle, Justified (#921), 2 Punch First (#419), 2 Salvage (#224)

**irelia** — legend Blade Dancer (#555, Calm/Chaos), champion Irelia, Fervent (#380)

- battlefields: Targon's Peak (#284), Sunken Temple (#532), Abandoned Hall (#761)
- runes: 6 Calm Rune, 6 Chaos Rune
- Main Deck (39): 3 Discipline (#58), 3 Zhonya's Hourglass (#77), 3 Tideturner (#199), 3 Stellacorn Herder (#371), 3 Defiant Dance (#511), 3 Scuttle Crab (#615), 3 Astral Heron (#831), 2 Defy (#45), 2 Treasure Hunter (#451), 2 Boots of Swiftness (#454), 2 Edge of Night (#460), 2 Switcheroo (#466), 1 Adaptatron (#56), 1 Gust (#169), 1 Rebuke (#172), 1 Ride the Wind (#173), 1 Flash (#309), 1 Not So Fast (#368), 1 Star-Crossed (#690), 1 Pyke, Returned (#707)
- sideboard (10): 2 Vex, Apathetic (#712), 2 Ravenbloom Prefect (#889), 1 Adaptatron (#56), 1 Gust (#169), 1 Not So Fast (#368), 1 Hard Bargain (#457), 1 Vex, Cheerless (#467), 1 Star-Crossed (#690)

**kaisa** — legend Daughter of the Void (#294, Fury/Mind), champion Kai'Sa, Survivor (#39)

- battlefields: Void Gate (#291), Rockfall Path (#530), Frozen Fortress (#768)
- runes: 7 Fury Rune, 5 Mind Rune
- Main Deck (39): 3 Noxus Hopeful (#12), 3 Brynhir Thundersong (#26), 3 Falling Star (#29), 3 Lecturing Yordle (#87), 3 Stupefy (#95), 3 Watchful Sentry (#96), 3 Thousand-Tailed Watcher (#116), 3 Plundering Poro (#392), 3 Temporal Breach (#853), 2 Hextech Ray (#9), 2 Smoke Screen (#93), 2 Time Warp (#122), 2 Bellows Breath (#402), 1 Progress Day (#114), 1 Ferrous Forerunner (#344), 1 Rocket Barrage (#400), 1 Perfect Execution (#799)
- sideboard (10): 2 Thermo Beam (#22), 2 Ferrous Forerunner (#344), 2 Decree of Rage (#802), 1 Progress Day (#114), 1 Bellows Breath (#402), 1 Sprite Burst (#631), 1 Decree of Insight (#848)

**khazix** — legend Voidreaver (#787, Body/Chaos), champion Kha'Zix, Mutating Horror (#705)

- battlefields: Zaun Warrens (#293), Star Spring (#771), Sandswept Tomb (#951)
- runes: 6 Body Rune, 6 Chaos Rune
- Main Deck (39): 3 Punch First (#419), 3 Kinkou Initiate (#659), 3 Irresistible Faefolk (#674), 3 Void Assault (#758), 3 Shadow Order Disciple (#882), 2 Sabotage (#156), 2 Star-Crossed (#690), 2 Rampage (#870), 2 Up from the Deep (#887), 2 Tail-Cloaked Matriarch (#891), 1 Ride the Wind (#173), 1 Traveling Merchant (#185), 1 Tideturner (#199), 1 Switcheroo (#466), 1 Tornado Warrior (#886), 3 Fizz, Trickster (#461), 2 Rengar, Trophy Hunter (#682), 2 Vex, Apathetic (#712), 1 Evelynn, Entrancing (#703), 1 Zed, Without a Sound (#899)
- sideboard (10): 2 Gust (#169), 2 Acceptable Losses (#179), 2 Ravenbloom Prefect (#889), 1 Sabotage (#156), 1 Hard Bargain (#457), 1 Rampage (#870), 1 Decree of Strength (#872)

**leblanc** — legend Deceiver (#756, Mind/Order), champion LeBlanc, Fragmented (#734)

- battlefields: Windswept Hillock (#292), Dusk Rose Lab (#765), Star Spring (#771)
- runes: 8 Order Rune, 4 Mind Rune
- Main Deck (39): 3 Watchful Sentry (#96), 3 Cull the Weak (#209), 3 Hidden Blade (#213), 3 Soaring Scout (#216), 3 Deathgrip (#484), 3 Glasc Mixologist (#486), 3 Ruined Rex (#629), 3 Mirror Image (#757), 2 Thousand-Tailed Watcher (#116), 2 Honest Broker (#476), 1 Stupefy (#95), 1 B.F. Sword (#482), 1 Chakram Dancer (#633), 1 Black Rose Dignitary (#714), 1 Sacrifice (#735), 1 Ki Barrier (#913), 3 Karthus, Eternal (#236), 1 Vi, Peacekeeper (#738), 1 Kennen, Keeper of Balance (#922)
- sideboard (10): 3 Decree of Unity (#918), 2 Salvage (#224), 2 Decree of Insight (#848), 1 Time Warp (#122), 1 LeBlanc, Everywhere at Once (#652), 1 Black Rose Dignitary (#714)

**mel** — legend Soul's Reflection (#938, Mind/Chaos), champion Mel, Newly Awakened (#856)

- battlefields: Targon's Peak (#284), Rockfall Path (#530), Frozen Fortress (#768)
- runes: 6 Mind Rune, 6 Chaos Rune
- Main Deck (39): 3 Stupefy (#95), 3 Ravenbloom Student (#103), 3 Traveling Merchant (#185), 3 Tideturner (#199), 3 Dredge Up (#836), 3 Applied Researchers (#842), 3 Rebuttal (#939), 2 Time Warp (#122), 2 Gust (#169), 2 Ride the Wind (#173), 2 Acceptable Losses (#179), 2 Star-Crossed (#690), 2 Shock Blast (#846), 1 Smoke Screen (#93), 1 Switcheroo (#466), 3 Hwei, Brooding Painter (#642), 1 Fizz, Trickster (#461)
- sideboard (10): 2 Pickpocket (#397), 2 Turn to Dust (#632), 2 Decree of Insight (#848), 2 Ravenbloom Prefect (#889), 1 Unchecked Power (#123), 1 Fizz, Trickster (#461)

**reksai** — legend Void Burrower (#553, Fury/Order), champion Rek'Sai, Breacher (#352)

- battlefields: Void Gate (#291), Rockfall Path (#530), Forbidding Waste (#766)
- runes: 7 Fury Rune, 5 Order Rune
- Main Deck (39): 3 Cleave (#4), 3 Noxus Hopeful (#12), 3 Falling Star (#29), 3 Cull the Weak (#209), 3 Faithful Manufactor (#211), 3 Blood Rush (#325), 3 Void Rush (#505), 3 Inferna (#560), 3 Carrion Dredger (#715), 3 Noxian Emissary (#915), 2 Honest Broker (#476), 2 Mischievous Marai (#562), 1 Hextech Ray (#9), 1 Hidden Blade (#213), 1 Imperial Decree (#221), 1 Undertitan (#495), 1 Shadow Fiend (#801)
- sideboard (10): 3 Ferrous Forerunner (#344), 2 Smite (#568), 1 Thermo Beam (#22), 1 Forge of the Future (#212), 1 Imperial Decree (#221), 1 Salvage (#224), 1 Brittle Steel (#790)

**rengar** — legend Pridestalker (#744, Fury/Body), champion Rengar, Trophy Hunter (#682)

- battlefields: Emperor's Dais (#521), Seat of Power (#531), Star Spring (#771)
- runes: 8 Body Rune, 4 Fury Rune
- Main Deck (39): 3 Noxus Hopeful (#12), 3 Pit Rookie (#136), 3 Sabotage (#156), 3 Punch First (#419), 3 Inferna (#560), 3 Grim Apothecary (#583), 3 Kinkou Initiate (#659), 3 Irresistible Faefolk (#674), 2 First Mate (#132), 2 Thrill of the Hunt (#745), 2 Rampage (#870), 1 Pakaa Cub (#135), 1 Ferrous Forerunner (#344), 3 Kai'Sa, Survivor (#39), 2 Nidalee, Cat Form (#676), 1 Darius, Trifarian (#27), 1 Pyke, Dockside Butcher (#590)
- sideboard (10): 2 Ferrous Forerunner (#344), 2 Brittle Steel (#790), 2 Decree of Strength (#872), 1 Pyke, Dockside Butcher (#590), 1 Thrill of the Hunt (#745), 1 Noxian Demolitionist (#867), 1 Rampage (#870)

**teemo** — legend Swift Scout (#259, Mind/Chaos), champion Teemo, Strategist (#121)

- battlefields: Grove of the God-Willow (#275), Startipped Peak (#283), Dusk Rose Lab (#765)
- runes: 6 Mind Rune, 6 Chaos Rune
- Main Deck (39): 3 Consult the Past (#83), 3 Sprite Call (#94), 3 Tideturner (#199), 3 Switcheroo (#466), 3 Sprite Fountain (#640), 3 Temporal Breach (#853), 2 Sneaky Deckhand (#176), 2 Guerilla Warfare (#260), 2 Windsinger (#459), 1 Singularity (#105), 1 Thousand-Tailed Watcher (#116), 1 Invert Timelines (#201), 1 Downwell (#468), 1 Existential Dread (#696), 1 Baron Nashor (#709), 2 Teemo, Strategist (#121), 3 Nocturne, Horrifying (#194), 3 Teemo, Scout (#197), 1 Vex, Apathetic (#712)
- sideboard (10): 3 Acceptable Losses (#179), 2 Star-Crossed (#690), 1 Existential Dread (#696), 1 Evelynn, Entrancing (#703), 1 Vex, Apathetic (#712), 1 Decree of Insight (#848), 1 Ravenbloom Prefect (#889)

**vex** — legend Gloomist (#785, Calm/Chaos), champion Vex, Apathetic (#712)

- battlefields: Bandle Tree (#273), Fortified Position (#274), Startipped Peak (#283)
- runes: 7 Chaos Rune, 5 Calm Rune
- Main Deck (39): 3 Defy (#45), 3 Discipline (#58), 3 Switcheroo (#466), 3 Mutated Mouser (#598), 3 Steel Paws (#830), 3 Tornado Warrior (#886), 2 Zhonya's Hourglass (#77), 2 Rebuke (#172), 2 Tideturner (#199), 2 Boots of Swiftness (#454), 2 Scuttle Crab (#615), 2 Astral Heron (#831), 2 Kharox (#901), 1 Charm (#43), 1 Block (#57), 1 Ride the Wind (#173), 1 Back Off (#604), 1 Abandon (#693), 1 Irelia, Fervent (#380), 1 Pyke, Returned (#707)
- sideboard (10): 3 Tomb-Raider Barbara (#824), 2 Gust (#169), 2 Decree of Focus (#827), 1 Not So Fast (#368), 1 Star-Crossed (#690), 1 Kha'Zix, Mutating Horror (#705)

**viktor** — legend Herald of the Arcane (#298, Mind/Order), champion Viktor, Leader (#246)

- battlefields: Seat of Power (#531), Dusk Rose Lab (#765), Forbidding Waste (#766)
- runes: 7 Order Rune, 5 Mind Rune
- Main Deck (39): 3 Stupefy (#95), 3 Cull the Weak (#209), 3 Hidden Blade (#213), 3 Imperial Decree (#221), 3 Honest Broker (#476), 3 Carrion Dredger (#715), 3 Keeper of Law (#906), 2 Singularity (#105), 2 Facebreaker (#220), 2 Bellows Breath (#402), 2 Sprite Fountain (#640), 2 Dragon Form (#903), 1 Thousand-Tailed Watcher (#116), 1 Blood Money (#483), 1 Eclipse (#625), 3 Xin Zhao, Vigilant (#496), 2 Vi, Peacekeeper (#738)
- sideboard (10): 3 Salvage (#224), 2 Sprite Burst (#631), 2 Ashe, Focused (#731), 2 Decree of Insight (#848), 1 Sprite Fountain (#640)

**yasuao** — legend Unforgiven (#256, Calm/Chaos), champion Yasuo, Windrider (#205)

- battlefields: Seat of Power (#531), Sunken Temple (#532), Forbidding Waste (#766)
- runes: 6 Calm Rune, 6 Chaos Rune
- Main Deck (39): 3 Defy (#45), 3 En Garde (#46), 3 Discipline (#58), 3 Tideturner (#199), 3 Flash (#309), 3 Lonely Poro (#359), 2 Gust (#169), 2 Rebuke (#172), 2 Ride the Wind (#173), 2 Stellacorn Herder (#371), 2 Shadow Order Disciple (#882), 2 Up from the Deep (#887), 1 Zhonya's Hourglass (#77), 1 Hard Bargain (#457), 1 Switcheroo (#466), 3 Akali, Silent (#825), 2 Irelia, Fervent (#380), 1 Fizz, Trickster (#461)
- sideboard (10): 2 Wind Wall (#64), 2 Acceptable Losses (#179), 1 Charm (#43), 1 Gust (#169), 1 Yasuo, Remorseful (#76), 1 Star-Crossed (#690), 1 Shadow Order Disciple (#882), 1 Up from the Deep (#887)

**yi_rebuild** — legend Wuju Bladesman - Starter (#317, Calm/Body), champion Master Yi, Tempered (#675)

- battlefields: Emperor's Dais (#521), Seat of Power (#531), Star Spring (#771)
- runes: 7 Body Rune, 5 Calm Rune
- Main Deck (39): 3 Charm (#43), 3 Defy (#45), 3 Discipline (#58), 3 First Mate (#132), 3 Pit Rookie (#136), 3 Lonely Poro (#359), 3 Punch First (#419), 3 Scuttle Crab (#615), 2 En Garde (#46), 2 Zhonya's Hourglass (#77), 2 Sabotage (#156), 2 Ruin Runner (#427), 2 Rampage (#870), 1 Back Off (#604), 1 Onslaught (#868), 3 Rengar, Trophy Hunter (#682)
- sideboard (10): 3 Disarming Rake (#355), 2 Fiora, Peerless (#432), 1 Not So Fast (#368), 1 Ruin Runner (#427), 1 Alpha Strike (#751), 1 Decree of Focus (#827), 1 Decree of Strength (#872)

**zed** — legend Master of Shadows (#930, Fury/Chaos), champion Zed, Without a Sound (#899)

- battlefields: Zaun Warrens (#293), Minefield (#526), Sandswept Tomb (#951)
- runes: 7 Chaos Rune, 5 Fury Rune
- Main Deck (39): 3 Noxus Hopeful (#12), 3 Traveling Merchant (#185), 3 Rhasa the Sunderer (#195), 3 Seal of Discord (#204), 3 Last Rites (#471), 3 Shadow Order Disciple (#882), 3 Shadowblade Lurker (#883), 3 Death Mark (#931), 2 Ride the Wind (#173), 2 Tideturner (#199), 2 Star-Crossed (#690), 2 Perfect Execution (#799), 3 Kai'Sa, Survivor (#39), 3 Kennen, Storm of Shuriken (#900), 1 Fizz, Trickster (#461)
- sideboard (10): 2 Piercing Light (#346), 2 Hard Bargain (#457), 2 Switcheroo (#466), 2 Ravenbloom Prefect (#889), 1 Brittle Steel (#790), 1 Up from the Deep (#887)

### Every distinct card

261 distinct cards. `lists` counts the lists that hold the card in any section; `(sb)` = sideboard only in that list.

| card | id | type | cost | Might | lists | printed text (def()) |
|---|---|---|---|---|---|---|
| Blade Dancer | 555 | Legend |  |  | 1: irelia | When you choose a friendly unit, you may exhaust me and pay [A] to ready it. / When you conquer, you may pay [1] to ready me. |
| Daughter of the Void | 294 | Legend |  |  | 1: kaisa | [E]: [Reaction] — [Add] [A]. Use only to play spells. (Abilities that add resources can't be reacted to.) |
| Deceiver | 756 | Legend |  |  | 1: leblanc | When you conquer or hold, you may discard 1 and exhaust me to play a ready Reflection unit token there. Then do this: It becomes a copy of another unit there. Give it [Temporary]. |
| Emperor of the Sands | 512 | Legend |  |  | 1: azir | Your Sand Soldiers have [Weaponmaster]. / [1], [E]: Play a 2 [M] Sand Soldier unit token to your base. Use only if you've played an Equipment this turn. |
| Gloomist | 785 | Legend |  |  | 1: vex | When you or an ally hold, you may exhaust me to draw 1. |
| Grand Duelist | 519 | Legend |  |  | 1: fiora | When one of your units becomes [Mighty], you may exhaust me to channel 1 rune exhausted. (A unit is Mighty while it has 5+ [M].) |
| Herald of the Arcane | 298 | Legend |  |  | 1: viktor | [1], [E]: Play a 1 [M] Recruit unit token. |
| Master of Shadows | 930 | Legend |  |  | 1: zed | When you banish a card you own, empower me. (I become Empowered if I'm not already.) / [Action][>] Disempower me, [E]: Discard 1, then draw 1. |
| Pridestalker | 744 | Legend |  |  | 1: rengar | When you play a unit, give a unit +1 [M] this turn. |
| Prodigal Explorer | 514 | Legend |  |  | 1: ezreal | [E]: [Reaction] — Draw 1. Use only if you've chosen enemy units and/or gear twice this turn with spells or unit abilities. |
| Rogue Assassin | 926 | Legend |  |  | 1: akali | [Empower] [3][A] ([3][A]: Empower this. Use only if not Empowered.) / [Action][>] [E]: If it's your turn, move a friendly unit in a showdown to base and if I'm [Empowered], ready it. |
| Scorn of the Moon | 786 | Legend |  |  | 1: diana | [Reaction][>] [E]: [Add] [1]. Spend this Energy only during showdowns. |
| Soul's Reflection | 938 | Legend |  |  | 1: mel | When you empower something else, empower me. (I become Empowered if I'm not already.) / Disempower me, [E]: Give a unit at a battlefield -2 [M] this turn. |
| Swift Scout | 259 | Legend |  |  | 1: teemo | You may pay [1] to hide a card with [Hidden] instead of [A]. / [1], [E]: Put a Teemo unit you own into your hand from your Champion Zone or the board. |
| Unforgiven | 256 | Legend |  |  | 1: yasuao | [2], [E]: Move a friendly unit to or from its base. |
| Void Burrower | 553 | Legend |  |  | 1: reksai | When you conquer, you may exhaust me to reveal the top 2 cards of your Main Deck. You may banish one, then play it. Recycle the rest. |
| Voidreaver | 787 | Legend |  |  | 1: khazix | When you win a combat, gain 1 XP. / Spend 1 XP, [E]: [Buff] a unit. / Spend 2 XP, [E]: Move an exhausted friendly unit from a battlefield to its base. |
| Wuju Bladesman - Starter | 317 | Legend |  |  | 1: yi_rebuild | While a friendly unit defends alone, it gets +2 [M]. |
| Adaptatron | 56 | Unit | [4] | 3 | 1: irelia | When I conquer, you may kill a gear. If you do, buff me. (If I don't have a buff, I get a +1 [M] buff.) |
| Akali, Deadly Weapon | 808 | Unit/Champion | [3] | 3 | 1: akali | [Empower] [2][R] ([2][R]: Empower me. Use only if not Empowered.) / When I move, you may deal 1 to a unit at a battlefield I moved to or from. If I'm [Empowered], deal 2 instead. / [Empowered][>] I have +1 [M]. |
| Akali, Silent | 825 | Unit/Champion | [4][C] | 4 | 2: akali(sb), yasuao | I can't be chosen by enemy spells and abilities unless I'm in combat. / When I move to a battlefield, give me +2 [M] this turn. |
| Akshan, Mischievous | 431 | Unit/Champion | [4] | 4 | 1: fiora(sb) | [Weaponmaster] / You may pay [O][O] as an additional cost to play me. / When you play me, if you paid the additional cost, move an enemy gear to your base. You control it until I leave the board. If it's an Equipment, attach it to me. |
| Angler Beast | 694 | Unit | [5][C] | 5 | 1: ezreal(sb) | When you play me, return all units with 2 [M] or less to their owners' hands. |
| Applied Researchers | 842 | Unit | [4] | 4 | 1: mel | [Empower] [3] ([3]: Empower me. Use only if not Empowered.) / [Empowered][>] Your spells cost [1][A] less, to a minimum of [1]. |
| Ashe, Focused | 731 | Unit/Champion | [5][O] | 4 | 2: azir(sb), viktor(sb) | When you play me, choose an opponent. They reveal their hand. Choose a card revealed this way and banish it. When they hold, return it to their hand (even if I'm no longer on the board). |
| Astral Heron | 831 | Unit | [7] | 7 | 3: akali, irelia, vex | When you play your first card each turn, if I'm at a battlefield, your next card costs [2][A][A] less. |
| Azir, Sovereign | 497 | Unit/Champion | [4] | 4 | 1: azir | [Accelerate] (You may pay [1][Y] as an additional cost to have me enter ready.) / When I attack, you may move any number of your token units to this battlefield. |
| Baron Nashor | 709 | Unit | [10][C][C][C] | 12 | 1: teemo | As you play me, add the Baron Pit battlefield token to the board if it's not there already. If you do, I enter there. (It has "Units can move here from anywhere.") / I can't be chosen by enemy spells and abilities. / Other friendly units have +2 [M]. |
| Bewitching Spirit | 683 | Unit | [3] | 2 | 1: ezreal | When you play me, choose a player. They discard 1. |
| Black Rose Dignitary | 714 | Unit | [3] | 2 | 1: leblanc | [Assault] (+1 [M] while I'm an attacker.) / [Deathknell][>] Channel 1 rune exhausted. (When I die, get the effect.) |
| Blitzcrank, Impassive | 67 | Unit/Champion | [5][C] | 5 | 1: akali | [Tank] (I must be assigned combat damage first.) / When you play me to a battlefield, you may move an enemy unit to here. / When I hold, return me to my owner's hand. |
| Brynhir Thundersong | 26 | Unit | [6] | 5 | 1: kaisa | When you play me, opponents can't play cards this turn. |
| Carrion Dredger | 715 | Unit | [2] | 1 | 2: reksai, viktor | [Deathknell][>] Play a 1 [M] Bird unit token with [Deflect] to your base. (When I die, get the effect. Opponents must pay [A] to choose a [Deflect] unit with a spell or ability.) |
| Chakram Dancer | 633 | Unit | [3] | 3 | 1: leblanc | [Ambush] (You may play me as a [Reaction] to a battlefield where you have units.) / When you play me, give your other units here [Shield] this turn. (+1 [M] while they're defenders.) |
| Darius, Trifarian | 27 | Unit/Champion | [5][F] | 5 | 2: akali, rengar | When you play your second card in a turn, give me +2 [M] this turn and ready me. |
| Diana, Lunari | 641 | Unit/Champion | [3] | 3 | 1: diana | When a showdown begins here, you may pay [1] to [Predict], then reveal the top card of your Main Deck. If it's a spell, draw it. (To Predict, look at the top card of your Main Deck. You may recycle it.) |
| Disarming Rake | 355 | Unit | [3][C] | 2 | 3: akali(sb), azir(sb), yi_rebuild(sb) | When you play me, you may kill a gear. |
| Elder Dragon | 680 | Unit | [12][B][B][B][B] | 10 | 1: fiora | Any amount of your damage is enough to kill enemy units. / When you play me, choose up to one enemy unit at each location. Deal 1 to them. |
| Evelynn, Entrancing | 703 | Unit/Champion | [2] | 2 | 2: khazix, teemo(sb) | [Hidden] (Hide now for [A] to react with later for .) / [Backline] (I must be assigned combat damage last.) / When you play me from face down on your turn, you may move an enemy unit at a different location to my battlefield. |
| Ezreal, Prodigy | 470 | Unit/Champion | [3][C] | 3 | 1: ezreal | When you play me, discard 1, then draw 2. / Optional additional costs you pay cost [1] or [A] less. |
| Faithful Manufactor | 211 | Unit | [3] | 2 | 1: reksai | When you play me, play a 1 [M] Recruit unit token here. |
| Ferrous Forerunner | 344 | Unit | [6][F] | 6 | 4: akali(sb), kaisa, reksai(sb), rengar | [Deathknell] — Play two 3 [M] Mech unit tokens to your base. (When I die, get the effect.) |
| Fiora, Peerless | 432 | Unit/Champion | [3][B] | 3 | 1: yi_rebuild(sb) | When I attack or defend one on one, double my Might this combat. |
| Fiora, Victorious | 232 | Unit/Champion | [4] | 4 | 1: fiora(sb) | While I'm [Mighty], I have [Deflect], [Ganking], and [Shield]. (I'm Mighty while I have 5+ [M].) |
| Fiora, Worthy | 500 | Unit/Champion | [3] | 3 | 1: fiora | When a unit you control becomes [Mighty], you may pay [Y] to ready it. (A unit is Mighty while it has 5+ [M].) |
| First Mate | 132 | Unit | [3] | 3 | 2: rengar, yi_rebuild | When you play me, ready another unit. |
| Fizz, Trickster | 461 | Unit/Champion | [3][C] | 3 | 6: diana, ezreal, khazix, mel, yasuao, zed | When you play me, you may play a spell from your trash with Energy cost no more than [3], ignoring its Energy cost. Then recycle it. (You must still pay its Power cost.) |
| Glasc Mixologist | 486 | Unit | [5][O] | 5 | 1: leblanc | [Deathknell] — You may play a unit with cost no more than [3] and no more than [A] from your trash, ignoring its cost. (When I die, get the effect.) |
| Grim Apothecary | 583 | Unit | [3] | 3 | 1: rengar | [Ambush] (You may play me as a [Reaction] to a battlefield where you have units.) / When you play me, you may return a friendly unit at a battlefield to its owner's hand. |
| Harnessed Dragon | 234 | Unit | [8][O][O] | 6 | 1: fiora | When you play me, kill an enemy unit. |
| Honest Broker | 476 | Unit | [2] | 2 | 3: leblanc, reksai, viktor | [Deathknell] — Play a Gold gear token exhausted. (When I die, get the effect.) |
| Hwei, Brooding Painter | 642 | Unit/Champion | [5][M] | 5 | 2: diana, mel | When I move, draw 1, then discard 1. Then, do the following based on the discarded card's type: / Spell — Draw 1.Gear — Ready up to 2 runes.Unit — Give me +3 [M] this turn. |
| Inferna | 560 | Unit | [2] | 1 | 2: reksai, rengar | [Ambush] (You may play me as a [Reaction] to a battlefield where you have units.) / [Assault 2] (+2 [M] while I'm an attacker.) |
| Irelia, Fervent | 380 | Unit/Champion | [5] | 4 | 3: irelia, vex, yasuao | [Deflect] (Opponents must pay [A] to choose me with a spell or ability.) / When you choose or ready me, give me +1 [M] this turn. |
| Irresistible Faefolk | 674 | Unit | [2] | 1 | 2: khazix, rengar | When I move to a battlefield, you may move an enemy unit to that battlefield. |
| Kai'Sa, Survivor | 39 | Unit/Champion | [4] | 4 | 4: akali, kaisa, rengar, zed | [Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.) / When I conquer, draw 1. |
| Karthus, Eternal | 236 | Unit/Champion | [3][O] | 3 | 1: leblanc | Your [Deathknell] effects trigger an additional time. |
| Kayle, Justified | 921 | Unit/Champion | [3] | 3 | 1: fiora(sb) | [Empower] [3] ([3]: Empower me.) / I can be [Empowered] up to three times. / I have +2 [M] for each time I'm [Empowered]. / While I'm [Empowered] three times, I have [Deflect 3] and [Ganking]. |
| Keeper of Law | 906 | Unit | [5][O] | 5 | 1: viktor | I cost [2][Y] less if you control a battlefield with exactly two units there. |
| Kennen, Keeper of Balance | 922 | Unit/Champion | [3] | 2 | 1: leblanc | [Hidden] (Hide now for [A] to react with later for [0].) / When you play me or I attack, you may pay [2] to [Stun] a unit. (It doesn't deal combat damage this turn.) / While there's a stunned enemy unit here, I have +2 [M]. |
| Kennen, Storm of Shuriken | 900 | Unit/Champion | [3][C] | 4 | 1: zed | When you play me, [Burn 2]. (Put the top 2 cards of your Main Deck into your trash.) / When I conquer, give a spell in your trash [Flow] equal to its cost this turn. (You may play it from your trash for its Flow cost. Then banish it.) |
| Kha'Zix, Mutating Horror | 705 | Unit/Champion | [4][C] | 4 | 3: ezreal(sb), khazix, vex(sb) | [Ambush] (You may play me as a [Reaction] to a battlefield where you have units.) / When I attack or defend, if an enemy unit is alone here, give me +2 [M] this turn and gain 2 XP. |
| Kharox | 901 | Unit | [6] | 5 | 1: vex | [Empower] [6][P][P] ([6][P][P]: Empower me. Use only if not Empowered.) / When I become [Empowered], choose an opponent. They [Burn 3]. Then you may do this: Choose a unit in their trash and play it, ignoring its cost. (To Burn 3, they put the top 3 cards of their Main Deck into their trash.) |
| Kinkou Initiate | 659 | Unit | [3] | 3 | 2: khazix, rengar | When you play me, draw 1 if your other units have total Might 5 or more. |
| LeBlanc, Everywhere at Once | 652 | Unit/Champion | [4] | 4 | 1: leblanc(sb) | [Backline] (I must be assigned combat damage last.) / Your [Temporary] effects at my battlefield don't trigger. |
| LeBlanc, Fragmented | 734 | Unit/Champion | [3][O] | 3 | 1: leblanc | [Assault] (+1 [M] while I'm an attacker.) / [Deathknell][>] Draw 1. If it's your Beginning Phase, draw 2 instead. (When I die, get the effect.) |
| Lecturing Yordle | 87 | Unit | [3] | 2 | 1: kaisa | [Tank] (I must be assigned combat damage first.) / When you play me, draw 1. |
| Lonely Poro | 359 | Unit | [2] | 2 | 2: yasuao, yi_rebuild | [Deathknell] — If I died alone, draw 1. (When I die, get the effect. I'm alone if there are no other friendly units here.) |
| Master Yi, Tempered | 675 | Unit/Champion | [4] | 4 | 1: yi_rebuild | [Hunt 2] (When I conquer or hold, gain 2 XP.) / [Level 6][>] I have [Deflect] and [Ganking]. (While you have 6+ XP, opponents must pay [A] to choose me with a spell or ability and I can move from battlefield to battlefield.) |
| Mel, Newly Awakened | 856 | Unit/Champion | [4][M] | 4 | 1: mel | When you play me, draw 1. / [Empower] [3] ([3]: Empower me. Use only if not Empowered.) / [Empowered][>] Your spells and abilities can't be countered. If a spell or ability you control would give -[M] to a unit it chooses, it gives an additional -1 [M]. |
| Mischievous Marai | 562 | Unit | [2] | 2 | 2: akali, reksai | [Hidden] (Hide now for [A] to react with later for .) / When you play me to a battlefield, deal 2 to an enemy unit here. |
| Mournful Witness | 815 | Unit | [2] | 2 | 1: akali | When a combat that I was in ends, empower me. (I become Empowered if I'm not already.) / [Empowered][>] I have +2 [M]. |
| Mutated Mouser | 598 | Unit | [2] | 1 | 1: vex | [Shield 2] (+2 [M] while I'm a defender.) / [Tank] (I must be assigned combat damage first.) |
| Nidalee, Cat Form | 676 | Unit/Champion | [3][B] | 4 | 1: rengar | [Ambush] (You may play me as a [Reaction] to a battlefield where you have units.) / When I win a combat, draw 1. (I win if I remain after combat.) |
| Nocturne, Horrifying | 194 | Unit/Champion | [4][C] | 4 | 1: teemo | [Ganking] (I can move from battlefield to battlefield.) / As you look at or reveal me from the top of your deck, you may banish me. If you do, you may play me for [A]. |
| Noxian Demolitionist | 867 | Unit | [2] | 1 | 1: rengar(sb) | When I conquer, you may kill a gear with Energy cost no more than my Might. |
| Noxian Emissary | 915 | Unit | [2] | 2 | 1: reksai | [Empower] [1][Y] ([1][Y]: Empower me. Use only if not Empowered.) / [Empowered][>][>>][Deathknell][>] Play two 1 [M] Recruit unit tokens to your base. (When I die while Empowered, get the effect.) |
| Noxus Hopeful | 12 | Unit | [4] | 4 | 5: akali, kaisa, reksai, rengar, zed | [Legion] — I cost [2] less. (Get the effect if you've played another card this turn.) |
| Pakaa Cub | 135 | Unit | [3] | 3 | 1: rengar | [Hidden] (Hide now for [A] to react with later for .) |
| Pickpocket | 397 | Unit | [3] | 3 | 2: ezreal(sb), mel(sb) | When you play me, you may kill a gear with Energy cost no more than [1]. If you do, play a Gold gear token exhausted. |
| Pit Rookie | 136 | Unit | [2] | 2 | 2: rengar, yi_rebuild | When you play me, buff another friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff.) |
| Plundering Poro | 392 | Unit | [2] | 2 | 1: kaisa | When I conquer, play a Gold gear token exhausted. |
| Poppy, Paragon | 678 | Unit/Champion | [5] | 5 | 1: fiora | [Deflect] (Opponents must pay [A] to choose me with a spell or ability.) / When you play me, if an opponent's score is within 3 points of the Victory Score, ready me and gain 3 XP. |
| Pyke, Dockside Butcher | 590 | Unit/Champion | [3] | 2 | 1: rengar | [Hidden] (Hide now for [A] to react with later for .) / [Ganking] (I can move from battlefield to battlefield.) / You may pay [R] as an additional cost to play me. / When you play me, if you paid the additional cost, ready me and give me +2 [M] this turn. |
| Pyke, Returned | 707 | Unit/Champion | [3] | 3 | 2: irelia, vex | [Hidden] (Hide now for [A] to react with later for .) / [Backline] (I must be assigned combat damage last.) / Once each turn, when an enemy unit dies while I'm at a battlefield, play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A].") |
| Ravenbloom Prefect | 889 | Unit | [3] | 3 | 6: ezreal(sb), irelia(sb), khazix(sb), mel(sb), teemo(sb), zed(sb) | When an opponent plays a gear, you may banish me to banish it. |
| Ravenbloom Student | 103 | Unit | [2] | 2 | 2: diana, mel | When you play a spell, give me +1 [M] this turn. |
| Rek'Sai, Breacher | 352 | Unit/Champion | [3] | 3 | 1: reksai | [Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.) / [Assault] (+1 [M] while I'm an attacker.) / Friendly units played from anywhere other than a player's hand have [Accelerate]. |
| Rengar, Trophy Hunter | 682 | Unit/Champion | [5][B] | 6 | 4: fiora, khazix, rengar, yi_rebuild | [Ambush] (You may play me as a [Reaction] to a battlefield where you have units.) / I can [Ambush] to a battlefield where there are enemy units, even if you don't have units there. |
| Rhasa the Sunderer | 195 | Unit | [10][C] | 6 | 1: zed | I cost [1] less for each card in your trash. |
| Rift Herald | 741 | Unit | [8][O] | 7 | 1: fiora | When I move to a battlefield, look at the top 3 cards of your Main Deck. You may reveal a unit from among them and draw it. Recycle the rest. / [Deathknell][>] Play a unit from your hand to your base, ignoring its Energy cost. (When I die, get the effect. You must still pay its Power cost.) |
| Ruin Runner | 427 | Unit | [6] | 5 | 1: yi_rebuild | I can't be chosen by enemy spells and abilities. |
| Ruined Rex | 629 | Unit | [6][M] | 6 | 1: leblanc | [Deathknell][>] Deal 4 to an enemy unit. (When I die, get the effect.) |
| Scuttle Crab | 615 | Unit | [2] |  | 3: irelia, vex, yi_rebuild | (Units with 0 [M] can conquer and hold.) / When you play me, draw 1. / [Deathknell][>] Choose an opponent. They reveal their hand. You can look at their facedown cards this turn. Gain 1 XP. (When I die, get the effects.) |
| Sett, Brawler | 164 | Unit/Champion | [5][B] | 4 | 1: fiora | When I'm played and when I conquer, buff me. (If I don't have a buff, I get a +1 [M] buff.) / Spend my buff: Give me +4 [M] this turn. |
| Shadow Fiend | 801 | Unit | [2] | 2 | 1: reksai | [Empower] [2][R] ([2][R]: Empower me. Use only if not Empowered.) / [Empowered][>] I have [Assault 3]. (+3 [M] while I'm an attacker.) |
| Shadow Order Disciple | 882 | Unit | [2] | 2 | 3: khazix, yasuao, zed | When I move, you may [Burn 1] to give me +1 [M] this turn. (To Burn 1, put the top card of your Main Deck into your trash.) |
| Shadowblade Lurker | 883 | Unit | [5] | 5 | 1: zed | I cost [2] less for each card with my name in your trash. |
| Sneaky Deckhand | 176 | Unit | [3] | 2 | 1: teemo | You may play me to an open battlefield. |
| Soaring Scout | 216 | Unit | [2] | 1 | 1: leblanc | [Deathknell] — Channel 1 rune exhausted. (When I die, get the effect.) |
| Steel Paws | 830 | Unit | [1] | 0 | 1: vex | [Deflect] (Opponents must pay [A] to choose me with a spell or ability.) / [Empower] [7] ([7]: Empower me. Use only if not Empowered.) / [Empowered][>] I have +7 [M]. |
| Stellacorn Herder | 371 | Unit | [4] | 3 | 3: akali, irelia, yasuao | When I move, draw 1. |
| Tail-Cloaked Matriarch | 891 | Unit | [4] | 4 | 2: diana(sb), khazix | [Empower] [2][P] ([2][P]: Empower me. Use only if not Empowered.) / When I become [Empowered], you may choose a unit in your trash with Energy cost no more than [3] and Power cost no more than [A]. Play it to your base, ignoring its cost. |
| Teemo, Scout | 197 | Unit/Champion | [2] | 1 | 1: teemo | [Hidden] (Hide now for [A] to react with later for .) / When you play me, give me +3 [M] this turn. |
| Teemo, Strategist | 121 | Unit/Champion | [2][M] | 2 | 1: teemo | [Hidden] (Hide now for [A] to react with later for [0].) / When I defend, choose an enemy unit here and reveal the top 5 cards of your Main Deck. Deal 1 to that unit for each card with [Hidden] revealed this way, then recycle the revealed cards. |
| Thousand-Tailed Watcher | 116 | Unit | [7][M] | 7 | 5: ezreal, kaisa, leblanc, teemo, viktor | [Accelerate] (You may pay [1][B] as an additional cost to have me enter ready.) / When you play me, give enemy units -3 [M] this turn, to a minimum of 1 [M]. |
| Tideturner | 199 | Unit | [2] | 2 | 8: diana, irelia, khazix, mel, teemo, vex, yasuao, zed | [Hidden] (Hide now for [A] to react with later for [0].) / When you play me, you may choose a unit you control at another location. Move me to its location and it to my original location. |
| Tomb-Raider Barbara | 824 | Unit | [4] | 4 | 1: vex(sb) | When you play me, if you control 7 or more runes, choose an enemy gear. If it's [Empowered], disempower it. Otherwise, kill it. |
| Tornado Warrior | 886 | Unit | [3] | 3 | 2: khazix, vex | [Hidden] (Hide now for [A] to react with later for [0].) / When you play me from face down, you may empower something here. Disempower it at end of turn. |
| Traveling Merchant | 185 | Unit | [2] | 2 | 4: diana, khazix, mel, zed | When I move, discard 1, then draw 1. |
| Treasure Hunter | 451 | Unit | [2] | 1 | 1: irelia | When I move, play a Gold gear token exhausted. |
| Undertitan | 495 | Unit | [6][O] | 5 | 1: reksai | When you play me, give your other units +2 [M] this turn. / As I'm revealed from your deck, [Add] [2]. |
| Vex, Apathetic | 712 | Unit/Champion | [4] | 4 | 6: diana, ezreal, irelia(sb), khazix, teemo, vex | [Deflect] (Opponents must pay [A] to choose me with a spell or ability.) / When an opponent plays a unit while I'm at a battlefield, [Stun] it. They can't move it this turn. (It doesn't deal combat damage this turn.) |
| Vex, Cheerless | 467 | Unit/Champion | [5][C] | 5 | 1: irelia(sb) | While I'm in combat, friendly spells cost [1][A] less to a minimum of [1], and enemy spells cost [1][A] more. |
| Vi, Peacekeeper | 738 | Unit/Champion | [5][O] | 5 | 2: leblanc, viktor | [Ambush] (You may play me as a [Reaction] to a battlefield where you have units.) / When I attack, [Stun] an enemy unit here. (It doesn't deal combat damage this turn.) |
| Viktor, Leader | 246 | Unit/Champion | [4][O] | 4 | 1: viktor | When another non-Recruit unit you control dies, play a 1 [M] Recruit unit token into your base. |
| Watchful Sentry | 96 | Unit | [2] | 1 | 2: kaisa, leblanc | [Deathknell] — Draw 1. (When I die, get the effect.) |
| Windsinger | 459 | Unit | [2] | 1 | 1: teemo | [Hidden] (Hide now for [A] to react with later for .) / When you play me, you may return another unit at a battlefield with 3 [M] or less to its owner's hand. |
| Xin Zhao, Vigilant | 496 | Unit/Champion | [3][O] | 4 | 1: viktor | [Tank] (I must be assigned combat damage first.) / I enter ready if you have two or more other units in your base. |
| Yasuo, Remorseful | 76 | Unit/Champion | [6][C][C] | 6 | 1: yasuao(sb) | When I attack, deal damage equal to my Might to an enemy unit here. |
| Yasuo, Windrider | 205 | Unit/Champion | [5][C] | 4 | 1: yasuao | [Ganking] (I can move from battlefield to battlefield.) / The third time I move in a turn, you score 1 point. |
| Zed, Without a Sound | 899 | Unit/Champion | [5] | 5 | 2: khazix, zed | When I conquer, play a 0 [M] Shadow Clone unit token to your base. (It has "When I attack, you may banish a unit from your trash. If you do, give me [Assault 4] this turn.") / [Action][>] [1][P]: Move me and a Shadow Clone you control to each other's locations. |
| Abandon | 693 | Spell | [2] |  | 1: vex | [Reaction] (Play any time, even before spells and abilities resolve.) / Counter a spell. Return it to its owner's hand instead of putting it in their trash. / [Predict]. (Look at the top card of your Main Deck. You may recycle it.) |
| Acceptable Losses | 179 | Spell | [1] |  | 6: diana, ezreal, khazix(sb), mel, teemo(sb), yasuao(sb) | [Action] (Play on your turn or in showdowns.) / Each player kills one of their gear. |
| Against the Odds | 323 | Spell | [2] |  | 1: akali(sb) | [Reaction] (Play any time, even before spells and abilities resolve.) / Give a friendly unit at a battlefield +2 [M] this turn for each enemy unit there. |
| Alpha Strike | 751 | Spell/Signature | [3][C] |  | 1: yi_rebuild(sb) | [Action] (Play on your turn or in showdowns.) / Choose a friendly unit. It deals damage equal to its Might split among enemy units at battlefields. Then for each unit this kills, do this: Gain 1 XP. |
| Arise! | 513 | Spell/Signature | [6][C] |  | 1: azir | Play a 2 [M] Sand Soldier unit token for each Equipment you control. Then do this: Ready up to two of them. |
| Back Off | 604 | Spell | [3] |  | 4: akali, azir, vex, yi_rebuild | [Hidden] (Hide now for [A] to react with later for .) / [Action] (Play on your turn or in showdowns.) / [Stun] a unit. (It doesn't deal combat damage this turn.) / If you played this from your hand, draw 1. |
| Bellows Breath | 402 | Spell | [1][M] |  | 3: ezreal, kaisa, viktor | [Action] (Play on your turn or in showdowns.) / [Repeat] [1][B] (You may pay the additional cost to repeat this spell's effect.) / Deal 1 to up to three units at the same location. |
| Block | 57 | Spell | [2] |  | 2: akali, vex | [Hidden] (Hide now for [A] to react with later for .) / [Action] (Play on your turn or in showdowns.) / Give a unit [Shield 3] and [Tank] this turn. (+3 [M] while it's a defender. It must be assigned combat damage first.) |
| Blood Money | 483 | Spell | [2] |  | 1: viktor | [Action] (Play on your turn or in showdowns.) / Kill a unit at a battlefield with 2 [M] or less. If it was an enemy unit, play a Gold gear token exhausted. If it was a friendly unit, play two Gold gear tokens exhausted. |
| Blood Rush | 325 | Spell | [1] |  | 1: reksai | [Action] (Play on your turn or in showdowns.) / [Repeat] [1] (You may pay the additional cost to repeat this spell's effect.) / Give a unit [Assault 2] this turn. (+2 [M] while it's an attacker.) |
| Brittle Steel | 790 | Spell | [2][F] |  | 3: reksai(sb), rengar(sb), zed(sb) | Kill a gear. / [Flow] [4][R] (You may play this from your trash for its Flow cost. Then banish it.) |
| Challenge | 128 | Spell | [2][B] |  | 1: fiora | [Action] (Play on your turn or in showdowns.) / Choose a friendly unit and an enemy unit. They deal damage equal to their Mights to each other. |
| Charm | 43 | Spell | [1][C] |  | 4: azir(sb), vex, yasuao(sb), yi_rebuild | Move an enemy unit. |
| Cleave | 4 | Spell | [1] |  | 1: reksai | [Action] (Play on your turn or in showdowns.) / Give a unit [Assault 3] this turn. (+3 [M] while it's an attacker.) |
| Consult the Past | 83 | Spell | [4] |  | 1: teemo | [Hidden] (Hide now for [A] to react with later for .) / [Reaction] (Play any time, even before spells and abilities resolve.) / Draw 2. |
| Cull the Weak | 209 | Spell | [2][O] |  | 3: leblanc, reksai, viktor | Each player kills one of their units. |
| Deadly Flourish | 635 | Spell | [4] |  | 1: ezreal | Deal 3 to an enemy unit. When it dies this turn, play a Gold gear token exhausted. (It has "[Reaction][>] Kill this, [E]: [Add] [A].") |
| Death Mark | 931 | Spell/Signature | [2][F] |  | 1: zed | [Burn 3]. (Put the top 3 cards of your Main Deck into your trash.) / Play a 0 [M] Shadow Clone unit token. (It has "When I attack, you may banish a unit from your trash. If you do, give me [Assault 4] this turn.") / [Flow] [1][A][A] (You may play this from your trash for its Flow cost. Then banish it.) |
| Deathgrip | 484 | Spell | [2] |  | 2: azir, leblanc | [Reaction] (Play any time, even before spells and abilities resolve.) / Kill a friendly unit. If you do, give +[M] equal to its Might to another friendly unit this turn. / Draw 1. |
| Decree of Focus | 827 | Spell | [1] |  | 4: akali(sb), azir(sb), vex(sb), yi_rebuild(sb) | [Reaction] (Play any time, even before spells and abilities resolve.) / Choose a friendly unit that's in combat with an enemy Fury ([R]) unit or that's being chosen by an enemy Fury spell. Give it +4 [M] this turn. |
| Decree of Insight | 848 | Spell | [1] |  | 6: diana(sb), kaisa(sb), leblanc(sb), mel(sb), teemo(sb), viktor(sb) | [Reaction] (Play any time, even before spells and abilities resolve.) / Ignore [Deflect] while paying this spell's cost. / Give an enemy Body ([O]) unit -5 [M] this turn. |
| Decree of Rage | 802 | Spell | [1][F] |  | 1: kaisa(sb) | [Action] (Play on your turn or in showdowns.) / This can't be countered. / Deal 4 to an enemy Calm ([G]) unit. |
| Decree of Strength | 872 | Spell | [1] |  | 3: khazix(sb), rengar(sb), yi_rebuild(sb) | Choose an opponent. They reveal their hand and you choose a Mind ([B]) card from it. They recycle that card. |
| Decree of Unity | 918 | Spell | [2][O] |  | 2: azir(sb), leblanc(sb) | Kill an enemy Chaos ([P]) unit or gear. |
| Defiant Dance | 511 | Spell/Signature | [1][C] |  | 1: irelia | [Reaction] (Play any time, even before spells and abilities resolve.) / Give a unit +2 [M] this turn and another unit -2 [M] this turn. |
| Defy | 45 | Spell | [1][C] |  | 6: akali, azir, irelia, vex, yasuao, yi_rebuild | [Reaction] (Play any time, even before spells and abilities resolve.) / Counter a spell that costs no more than [4] and no more than [A]. |
| Discipline | 58 | Spell | [2] |  | 6: akali, azir, irelia, vex, yasuao, yi_rebuild | [Reaction] (Play any time, even before spells and abilities resolve.) / Give a unit +2 [M] this turn. Draw 1. |
| Downwell | 468 | Spell | [8][C][C] |  | 1: teemo | Return all units and gear to their owners' hands. |
| Dragon Form | 903 | Spell | [3] |  | 1: viktor | Choose a unit. Its base Might becomes 5 this turn. / [Flow] [3] (You may play this from your trash for its Flow cost. Then banish it.) |
| Dredge Up | 836 | Spell | [2] |  | 1: mel | Draw 1. / [Flow] [2] (You may play this from your trash for its Flow cost. Then banish it.) |
| Eclipse | 625 | Spell | [3] |  | 2: diana, viktor | [Reaction] (Play any time, even before spells and abilities resolve.) / Give a unit -4 [M] this turn. / [Predict]. (Look at the top card of your Main Deck. You may recycle it.) |
| En Garde | 46 | Spell | [1] |  | 4: akali(sb), azir, yasuao, yi_rebuild | [Reaction] (Play any time, even before spells and abilities resolve.) / Give a friendly unit +1 [M] this turn, then an additional +1 [M] this turn if it is the only unit you control there. |
| Existential Dread | 696 | Spell | [1][C] |  | 1: teemo | [Action] (Play on your turn or in showdowns.) / [Repeat] [2] (You may pay the additional cost to repeat this spell's effect.) / [Stun] an attacking enemy unit. If it's already stunned, return it to its owner's hand instead. (A stunned unit doesn't deal combat damage this turn.) |
| Facebreaker | 220 | Spell | [2] |  | 1: viktor | [Hidden] (Hide now for [A] to react with later for .) / [Action] (Play on your turn or in showdowns.) / Stun a friendly unit and an enemy unit at the same battlefield. (They don't deal combat damage this turn.) |
| Falling Star | 29 | Spell | [2][F][F] |  | 3: akali, kaisa, reksai | Deal 3 to a unit. / Deal 3 to a unit. |
| Flash | 309 | Spell | [2] |  | 3: diana, irelia, yasuao | [Reaction] (Play any time, even before spells and abilities resolve.) / Move up to 2 friendly units to base. |
| Flurry of Blades | 133 | Spell | [1] |  | 1: fiora | [Reaction] (Play any time, even before spells and abilities resolve.) / Deal 1 to all units at battlefields. |
| Guards! | 475 | Spell | [3] |  | 1: azir | [Hidden] (Hide now for [A] to react with later for [0].) / Play a 2 [M] Sand Soldier unit token. Then do this: You may pay [C] to ready it. |
| Guerilla Warfare | 260 | Spell/Signature | [2][M] |  | 1: teemo | Return up to two cards with [Hidden] from your trash to your hand. You can hide cards ignoring costs this turn. |
| Gust | 169 | Spell | [1] |  | 7: diana, ezreal, irelia, khazix(sb), mel, vex(sb), yasuao | [Reaction] (Play any time, even before spells and abilities resolve.) / Return a unit at a battlefield with 3 [M] or less to its owner's hand. |
| Hard Bargain | 457 | Spell | [2] |  | 5: diana, irelia(sb), khazix(sb), yasuao, zed(sb) | [Reaction] (Play any time, even before spells and abilities resolve.) / [Repeat] [2] (You may pay the additional cost to repeat this spell's effect.) / Counter a spell unless its controller pays [2]. |
| Hextech Ray | 9 | Spell | [1][F] |  | 2: kaisa, reksai | [Action] (Play on your turn or in showdowns.) / Deal 3 to a unit at a battlefield. |
| Hidden Blade | 213 | Spell | [2][O] |  | 4: azir, leblanc, reksai, viktor | [Hidden] (Hide now for [A] to react with later for .) / [Action] (Play on your turn or in showdowns.) / Kill a unit at a battlefield. Its controller draws 2. |
| Imperial Decree | 221 | Spell | [5][O][O] |  | 2: reksai, viktor | [Action] (Play on your turn or in showdowns.) / When any unit takes damage this turn, kill it. |
| Invert Timelines | 201 | Spell | [3][C] |  | 1: teemo | Each player discards their hand, then draws 4. |
| Ki Barrier | 913 | Spell | [2][O] |  | 1: leblanc | [Reaction] (Play any time, even before spells and abilities resolve.) / Choose a unit. Prevent the next 7 damage that would be dealt to it this turn. (Opponents can assign it extra combat damage to kill it.) |
| Mirror Image | 757 | Spell/Signature | [3][M][M] |  | 1: leblanc | Choose a unit. Play a ready Reflection unit token to your base. Then do this: It becomes a copy of that unit. Give it [Temporary]. (Kill it at the start of its controller's Beginning Phase, before scoring.) |
| Mobilize | 134 | Spell | [2] |  | 1: fiora | Channel 1 rune exhausted. If you can't, draw 1. |
| Moonfall | 755 | Spell/Signature | [3][M] |  | 1: diana | [Action] (Play on your turn or in showdowns.) / Choose a battlefield where you have units. You may move up to one enemy unit to that battlefield. Then give enemy units there -2 [M] this turn. |
| Not So Fast | 368 | Spell | [2][C] |  | 4: akali, irelia, vex(sb), yi_rebuild(sb) | [Reaction] (Play any time, even before spells and abilities resolve.) / Counter an enemy spell or ability that chooses a friendly unit or gear. |
| Onslaught | 868 | Spell | [4] |  | 1: yi_rebuild | Give a unit +6 [M] this turn. / [Flow] [4] (You may play this from your trash for its Flow cost. Then banish it.) |
| Perfect Execution | 799 | Spell | [3][F] |  | 2: kaisa, zed | Ready a unit and give it [Assault 3] this turn. (+3 [M] while it's an attacker.) / [Flow] [3][R] (You may play this from your trash for its Flow cost. Then banish it.) |
| Piercing Light | 346 | Spell | [2][F] |  | 1: zed(sb) | [Repeat] [2][R] (You may pay the additional cost to repeat this spell's effect.) / Deal 2 to a unit at a battlefield, then deal 2 to up to one other unit. |
| Progress Day | 114 | Spell | [6][M] |  | 1: kaisa | Draw 4. |
| Punch First | 419 | Spell | [1][B][B] |  | 4: fiora(sb), khazix, rengar, yi_rebuild | [Action] (Play on your turn or in showdowns.) / Give a unit +5 [M] this turn. |
| Rampage | 870 | Spell | [3] |  | 4: fiora, khazix, rengar, yi_rebuild | As you play this, you may pay [O] as an additional cost. / Choose a friendly unit and an enemy unit. If you paid the additional cost, give the friendly unit +2 [M] this turn. They deal damage equal to their Mights to each other. |
| Rebuke | 172 | Spell | [2][C][C] |  | 4: diana, irelia, vex, yasuao | [Action] (Play on your turn or in showdowns.) / Return a unit at a battlefield to its owner's hand. |
| Rebuttal | 939 | Spell/Signature | [1][M] |  | 1: mel | [Reaction] (Play any time, even before spells and abilities resolve.) / Choose a spell with Energy cost no more than [4]. You may pay [A]. If you do, gain control of it and you may make new choices for it. Otherwise, counter it. |
| Repulse | 668 | Spell | [1][B] |  | 1: fiora | [Reaction] (Play any time, even before spells and abilities resolve.) / Choose a friendly unit at a battlefield. Counter an enemy spell or ability that chooses it and no other friendly unit. |
| Ride the Wind | 173 | Spell | [2][C] |  | 7: diana, irelia, khazix, mel, vex, yasuao, zed | [Action] (Play on your turn or in showdowns.) / Move a friendly unit and ready it. |
| Riposte | 520 | Spell/Signature | [2][B][B] |  | 1: fiora | [Reaction] (Play any time, even before spells and abilities resolve.) / Choose a friendly unit and a spell. Counter that spell and give that unit +[M] equal to that spell's Energy cost this turn. |
| Rocket Barrage | 400 | Spell | [4][M] |  | 1: kaisa | [Repeat] [4][B] (You may pay the additional cost to repeat this spell's effect, and may make different choices.) / Choose one — / Deal 4 to a unit in a base.Kill a gear. |
| Sabotage | 156 | Spell | [1][B] |  | 3: khazix, rengar, yi_rebuild | Choose an opponent. They reveal their hand. Choose a non-unit card from it, and recycle that card. |
| Sacrifice | 735 | Spell | [1] |  | 2: fiora, leblanc | [Reaction] (Play any time, even before spells and abilities resolve.) / As an additional cost to play this, kill a friendly [Mighty] unit. (A unit is Mighty while it has 5+ [M].) / Draw 2 and channel 1 rune exhausted. |
| Salvage | 224 | Spell | [2][O] |  | 5: azir(sb), fiora(sb), leblanc(sb), reksai(sb), viktor(sb) | [Action] (Play on your turn or in showdowns.) / You may kill up to one gear. Draw 1. |
| Shock Blast | 846 | Spell | [3][M] |  | 1: mel | [Action] (Play on your turn or in showdowns.) / This costs [2] less if you control something that's [Empowered]. / Deal 4 to a unit at a battlefield. |
| Shuriken Flip | 927 | Spell/Signature | [1][F] |  | 1: akali | Deal 2 to up to one enemy unit at a battlefield, then move a friendly unit. / [Flow] [3][A] (You may play this from your trash for its Flow cost. Then banish it.) |
| Singularity | 105 | Spell | [6][M][M] |  | 4: diana(sb), ezreal, teemo, viktor | Deal 6 to each of up to two units. |
| Smite | 568 | Spell | [2][F] |  | 1: reksai(sb) | [Action] (Play on your turn or in showdowns.) / Deal 3 to a unit at a battlefield. If it would die this turn, banish it instead. |
| Smoke Screen | 93 | Spell | [2][M] |  | 3: diana, kaisa, mel | [Reaction] (Play any time, even before spells and abilities resolve.) / Give a unit -4 [M] this turn, to a minimum of 1 [M]. |
| Sprite Burst | 631 | Spell | [5] |  | 2: kaisa(sb), viktor(sb) | Play two ready 3 [M] Sprite unit tokens with [Temporary]. (Kill each at the start of its controller's Beginning Phase, before scoring.) |
| Sprite Call | 94 | Spell | [3] |  | 1: teemo | [Hidden] (Hide now for [A] to react with later for .) / [Action] (Play on your turn or in showdowns.) / Play a ready 3 [M] Sprite unit token with [Temporary]. (Kill it at the start of its controller's Beginning Phase, before scoring.) |
| Star-Crossed | 690 | Spell | [3][C] |  | 9: diana, ezreal, irelia, khazix, mel, teemo(sb), vex(sb), yasuao(sb), zed | [Reaction] (Play any time, even before spells and abilities resolve.) / Return a friendly unit and an enemy unit to their owners' hands. |
| Stupefy | 95 | Spell | [1] |  | 6: diana, ezreal, kaisa, leblanc, mel, viktor | [Reaction] (Play any time, even before spells and abilities resolve.) / Give a unit -1 [M] this turn, to a minimum of 1 [M]. Draw 1. |
| Switcheroo | 466 | Spell | [2][C][C] |  | 8: diana, irelia, khazix, mel, teemo, vex, yasuao, zed(sb) | [Hidden] (Hide now for [A] to react with later for .) / [Action] (Play on your turn or in showdowns.) / Swap the Might of two units at the same battlefield this turn. |
| Temporal Breach | 853 | Spell | [2][M] |  | 2: kaisa, teemo | [Hidden] (Hide now for [A] to react with later for [0].) / Banish a unit, then its owner plays it to the same location, ignoring its cost. |
| Thermo Beam | 22 | Spell | [5][F][F] |  | 3: akali(sb), kaisa(sb), reksai(sb) | [Action] (Play on your turn or in showdowns.) / Kill all gear. |
| Thrill of the Hunt | 745 | Spell/Signature | [2][F] |  | 1: rengar | [Reaction] (Play any time, even before spells and abilities resolve.) / Banish a friendly unit, then its owner plays it to any battlefield, ignoring its cost. |
| Time Warp | 122 | Spell | [10][M][M][M][M] |  | 3: kaisa, leblanc(sb), mel | Take a turn after this one. Banish this. |
| Turn to Dust | 632 | Spell | [2] |  | 3: diana(sb), ezreal, mel(sb) | Give a gear [Temporary]. (Kill it at the start of its controller's Beginning Phase, before scoring.) |
| Unchecked Power | 123 | Spell | [7][M][M] |  | 1: mel(sb) | Exhaust all friendly units, then deal 12 to ALL units at battlefields. |
| Up from the Deep | 887 | Spell | [3] |  | 4: diana(sb), khazix, yasuao, zed(sb) | Play two 1 [M] Tentacle unit tokens from Bilgewater. / [Flow] [3] (You may play this from your trash for its Flow cost. Then banish it.) |
| Void Assault | 758 | Spell/Signature | [2][B] |  | 1: khazix | Move a friendly unit, then move an enemy unit. (If they both move to a battlefield you don't control, you're the attacker.) |
| Void Rush | 505 | Spell/Signature | [2][F] |  | 1: reksai | Reveal the top 2 cards of your Main Deck. You may banish one, then play it, reducing its cost by [2]. Draw any you didn't banish. |
| Wages of Pain | 393 | Spell | [3] |  | 1: ezreal | [Hidden] (Hide now for [A] to react with later for .) / [Action] (Play on your turn or in showdowns.) / Deal 3 to a unit at a battlefield. Play a Gold gear token exhausted. |
| Wind Wall | 64 | Spell | [3][C][C] |  | 1: yasuao(sb) | [Reaction] (Play any time, even before spells and abilities resolve.) / Counter a spell. |
| B.F. Sword | 482 | Gear | [4] |  | 2: azir, leblanc | [Equip] [Y] ([Y]: Attach this to a unit you control.) |
| Boots of Swiftness | 454 | Gear | [3] |  | 2: irelia, vex | [Equip] [P] ([P]: Attach this to a unit you control.) // effect text: [Ganking] (I can move from battlefield to battlefield.) |
| Brutalizer | 365 | Gear | [2] |  | 1: azir | [Equip] [G] ([G]: Attach this to a unit you control.) // effect text: If this was attached to me this turn, I have an additional +2 [M]. |
| Dazzling Aurora | 160 | Gear | [9][B][B] |  | 1: fiora | At the end of your turn, reveal cards from the top of your Main Deck until you reveal a unit and banish it. Play it, ignoring its cost, and recycle the rest. |
| Divining Shells | 723 | Gear | [2] |  | 1: fiora | [Vision] (When you play this, look at the top card of your Main Deck. You may recycle it.) / [Action][>] Kill this, [E]: Give a unit +2 [M] this turn. |
| Doran's Shield | 356 | Gear | [1] |  | 1: azir | [Equip] [G] ([G]: Attach this to a unit you control.) // effect text: [Tank] (I must be assigned combat damage first.) |
| Edge of Night | 460 | Gear | [3] |  | 1: irelia | [Hidden] (Hide now for [A] to react with later for [0].) / When you play this from face down, attach it to a unit you control (here). / [Equip] [C] ([C]: Attach this to a unit you control.) |
| Eye of the Herald | 474 | Gear | [1] |  | 1: azir | [Equip] [Y] ([Y]: Attach this to a unit you control.) // effect text: When I move, play a 1 [M] Recruit unit token here. |
| Forge of the Future | 212 | Gear | [2] |  | 2: fiora(sb), reksai(sb) | When you play this, play a 1 [M] Recruit unit token at your base. / Kill this: Recycle up to 4 cards from trashes. |
| Hand Hammer | 814 | Gear | [2] |  | 1: azir | [Equip] [G] ([G]: Attach this to a unit you control.) // effect text: I have +2 [M] while I'm at a battlefield with exactly one other unit you control. |
| Last Rites | 471 | Gear | [3] |  | 1: zed | [Equip] — [P], Recycle 2 cards from your trash (Pay the cost: Attach this to a unit you control.) // effect text: When I conquer or hold, you may play a unit from your trash. (You still pay its costs.) |
| Long Sword | 345 | Gear | [2][F] |  | 1: akali | [Quick-Draw] (This has [Reaction]. When you play it, attach it to a unit you control.) / [Equip] [R] ([R]: Attach this to a unit you control.) |
| Pack of Wonders | 181 | Gear | [2] |  | 1: ezreal | [E]: Return another friendly gear, unit, or facedown card to its owner's hand. |
| Seal of Discord | 204 | Gear | [0][C] |  | 1: zed | [E]: [Reaction] — [Add] [P]. (Abilities that add resources can't be reacted to.) |
| Shepherd's Heirloom | 720 | Gear | [2] |  | 1: fiora | When you play this, gain 1 XP. / [Equip] — Spend 1 XP (Pay the cost: Attach this to a unit you control.) |
| Soul Sword | 601 | Gear | [1] |  | 1: azir | [Equip] [G] ([G]: Attach this to a unit you control.) // effect text: [Level 3][>] I have an additional +1 [M]. (While you have 3+ XP, get the effect.) |
| Sprite Fountain | 640 | Gear | [2][M] |  | 4: diana, ezreal, teemo, viktor | [Temporary] (Kill this at the start of its controller's Beginning Phase, before scoring.) / When you play this, play a ready 3 [M] Sprite unit token with [Temporary] to your base. / [Deathknell][>] Repeat this gear's play effect. (When this dies, get the effect.) |
| Sterak's Gage | 379 | Gear | [3][C][C] |  | 1: akali | [Quick-Draw] (This has [Reaction]. When you play it, attach it to a unit you control.) / [Equip] [G] ([G]: Attach this to a unit you control.) |
| The List | 700 | Gear | [1] |  | 1: ezreal | As you play this, name a tag. (For example, Miss Fortune, Demacia, and Poro are tags.) / [E]: Give a unit with the named tag -2 [M] this turn. |
| Treasure Trove | 186 | Gear | [2] |  | 1: ezreal | When this leaves the board, draw 1 and channel 1 rune exhausted. / [P], [E]: Kill this. |
| Zhonya's Hourglass | 77 | Gear | [2] |  | 5: akali, irelia, vex, yasuao, yi_rebuild | [Hidden] (Hide now for [A] to react with later for [0].) / If a friendly unit would die, kill this instead. Heal that unit, exhaust it, and recall it. (Send it to base. This isn't a move.) |
| Abandoned Hall | 761 | Battlefield |  |  | 2: diana, irelia | When a player plays a spell, they may give a unit they control here +1 [M] this turn. |
| Amateur Recital | 763 | Battlefield |  |  | 1: fiora | When you hold here, you may move a unit at a battlefield to its base. |
| Bandle Tree | 273 | Battlefield |  |  | 1: vex | You may hide an additional card here. |
| Dusk Rose Lab | 765 | Battlefield |  |  | 3: leblanc, teemo, viktor | At the start of your Beginning Phase, you may kill a unit you control here to draw 1. (This happens before scoring.) |
| Emperor's Dais | 521 | Battlefield |  |  | 2: rengar, yi_rebuild | When you conquer here, you may pay [1] and return a unit you control here to its owner's hand to play a 2 [M] Sand Soldier unit token here. |
| Forbidding Waste | 766 | Battlefield |  |  | 3: reksai, viktor, yasuao | While a unit here is defending alone, it has -2 [M]. (It's alone if there are no other friendly units here.) |
| Forgotten Monument | 523 | Battlefield |  |  | 1: akali | Players can't score here until their third turn. |
| Fortified Position | 274 | Battlefield |  |  | 1: vex | When you defend here, choose a unit. It gains [Shield 2] this combat. (+2 [M] while it's a defender.) |
| Frozen Fortress | 768 | Battlefield |  |  | 3: ezreal, kaisa, mel | At the start of each player's Beginning Phase, deal 1 to each unit here. (This happens before scoring.) |
| Grove of the God-Willow | 275 | Battlefield |  |  | 1: teemo | When you hold here, draw 1. |
| Hall of Legends | 524 | Battlefield |  |  | 1: azir | When you conquer here, you may pay [1] to ready your legend. |
| Minefield | 526 | Battlefield |  |  | 1: zed | When you conquer here, put the top 2 cards of your Main Deck into your trash. |
| Risen Altar | 950 | Battlefield |  |  | 1: fiora | [Empower] costs of your units here cost [1] or [A] less. |
| Rockfall Path | 530 | Battlefield |  |  | 3: kaisa, mel, reksai | Units can't be played here. |
| Sandswept Tomb | 951 | Battlefield |  |  | 2: khazix, zed | Each spell that chooses one or more units here that are friendly to it costs [A] less. |
| Seat of Power | 531 | Battlefield |  |  | 6: azir, diana, rengar, viktor, yasuao, yi_rebuild | When you conquer here, draw 1 for each other battlefield you or allies control. |
| Sigil of the Storm | 282 | Battlefield |  |  | 2: akali, ezreal | When you conquer here, you must recycle one of your runes. (This doesn't choose anything.) |
| Star Spring | 771 | Battlefield |  |  | 4: khazix, leblanc, rengar, yi_rebuild | The first time a player plays a non-token unit here each turn, they may move another unit they control here to its base. |
| Startipped Peak | 283 | Battlefield |  |  | 2: teemo, vex | When you hold here, you may channel 1 rune exhausted. |
| Sunken Temple | 532 | Battlefield |  |  | 3: fiora, irelia, yasuao | When you conquer here with one or more [Mighty] units, you may pay [1] to draw 1. (A unit is Mighty while it has 5+ [M].) |
| Targon's Peak | 284 | Battlefield |  |  | 3: diana, irelia, mel | When you conquer here, ready up to 2 runes at the end of this turn. |
| Trifarian War Camp | 289 | Battlefield |  |  | 1: azir | Units here have +1 [M]. (This includes attackers.) |
| Vilemaw's Lair | 290 | Battlefield |  |  | 1: ezreal | Units can't move from here to base. |
| Void Gate | 291 | Battlefield |  |  | 3: akali, kaisa, reksai | Spells and abilities deal 1 Bonus Damage to units here. (Each instance of damage the spell deals to a unit here is increased by 1.) |
| Windswept Hillock | 292 | Battlefield |  |  | 1: leblanc | Units here have [Ganking]. (They can move from battlefield to battlefield.) |
| Zaun Warrens | 293 | Battlefield |  |  | 2: khazix, zed | When you conquer here, discard 1, then draw 1. |
| Body Rune | 126 | Rune |  |  | 4: fiora, khazix, rengar, yi_rebuild |  |
| Calm Rune | 42 | Rune |  |  | 6: akali, azir, irelia, vex, yasuao, yi_rebuild |  |
| Chaos Rune | 166 | Rune |  |  | 9: diana, ezreal, irelia, khazix, mel, teemo, vex, yasuao, zed |  |
| Fury Rune | 7 | Rune |  |  | 5: akali, kaisa, reksai, rengar, zed |  |
| Mind Rune | 89 | Rune |  |  | 7: diana, ezreal, kaisa, leblanc, mel, teemo, viktor |  |
| Order Rune | 214 | Rune |  |  | 5: azir, fiora, leblanc, reksai, viktor |  |

## 4. Coverage: every card of the lists against a trace

One table per reader (the lists whose cards that reader owned: a card is read once, by the reader of the first list that holds it).
`read`: FULL = every clause of the printed text was compared against a traced instance that exercised it; PART = the named clause was not seen at work; NONE = not read.

### reader 1: akali

| card | id | type | played (game, D) | ability triggered / activated (game, D) | resolved (game, D) | read | result |
|---|---|---|---|---|---|---|---|
| Rogue Assassin | 926 | Legend | - | Empower: akali__azir__s20263000__ismcts-sims20__random D:016; move: same game D:081 (non-combat showdown), akali__reksai__s20261110__random__random D:078 (combat); not Empowered: akali__azir__s20261101__random__random D:110 | D:018 / D:083 / D:080 / D:112; mistarget irelia__akali__s20261190__random__random D:098 | FULL | ok (189 move activations, all on own turn; 67 Empowers, one per legend) |
| Akali, Deadly Weapon | 808 | Unit (champion) | akali__reksai__s20261110__random__random (3E) | Empower akali__fiora__s20261104__random__random D:225 (rune check), akali__reksai D:072; move trigger akali__reksai D:073, D:080, D:085; akali__azir__s20263000 D:077 | akali__reksai D:077 (2 +1 Void Gate), D:084; azir ismcts D:310 | FULL | ok; D9 (R1-9) touches the Empower payment |
| Defy | 45 | Spell | akali__azir__s20263000__ismcts-sims20__random D:15x; 353 plays | - | same game D:155 (counters Block); 60 distinct countered spells checked against printed costs | PART (the cost caps are confirmed only by absence: no countered spell above [4] / one Power in any game; "spell not on top" never observed) | ok; D21 (R1-10) (CODE-ONLY) |
| Discipline | 58 | Spell | akali__azir__s20263000 D:198 (reaction, target at play) | - | D:203 (+2, draw 1); akali__ezreal__s20261103 D:179 | FULL | ok |
| Zhonya's Hourglass | 77 | Gear | akali__azir__s20263000 D:006 (2E); hide D:220 (`PAY_ADD ... 1 power (any)`), played from facedown D:239 (opponent's turn, combat) | replacement: D:173, D:242; two copies, order asked: akali__irelia__s20263001__ismcts-sims20__random D:116; hidden, played in a Closed State: zed_sb__yi_rebuild_sb__s20262422__random__random D:179-181 | board after D:242 (Akali healed, exhausted, in base, still Empowered 4M) | FULL | ok; see QUESTIONS (recall of a gear played from facedown) |
| Stellacorn Herder | 371 | Unit | akali__azir__s20263000 | move trigger D:208 (Shuriken Flip move), D:315; after Rogue Assassin: akali__khazix__s20263031__random__ismcts-sims20; no trigger on Zhonya recall D:173.13 | D:317 (draw 1) | FULL | ok |
| Mournful Witness | 815 | Unit | akali__fiora__s20261104__random__random | akali__fiora D:217 (combat ended by Rogue Assassin), akali__akali__s20261100 D:236.13 (after Zhonya recall) | D:219 (empower -> 4M board D:225) | FULL | ok (44 damage steps with her: 35 triggers, the other 18 she died; 0 triggers after a non-combat showdown) |
| Astral Heron | 831 | Unit | akali__ezreal__s20261103__random__random D:166 (7E) | D:173 (first card = itself), akali__akali__s20261100 D:234, azir ismcts D:362; at base: akali__akali D:146 | D:181, D:236; discount used ezreal D:200 (next turn but one), D:209 (Shuriken Flip E=0 P=0), akali__akali D:239 (Kai'Sa E=2), azir ismcts D:368 (Blitzcrank E=3 P=0) | FULL | D18 (R1-6), D19 (R1-7) |
| Shuriken Flip | 927 | Spell (signature) | azir ismcts D:195 (hand), D:310 (Flow, in combat), akali__irelia__s20263001 D:016 (no units) | - | D:205-208, D:312-315, D:020 | FULL | D5 (R1-1), D16 (R1-4), D17 (R1-5) |
| Falling Star | 29 | Spell | akali__akali__s20261100 D:101; akali__azir__s20261101 D:121 (combat focus); akali__kaisa__s20261106 D:204 (opponent's turn) | - | akali__akali D:109 (+1 Void Gate on one target); kaisa__yasuao__s20261223 D:112 | FULL | D5 (R1-1), D20 (R1-8) |
| Long Sword | 345 | Gear | akali__azir__s20261101 D:189; azir ismcts D:304 (reaction) | Quick-Draw D:193, D:308; Equip 31 activations | attach lines same D | FULL | D14 (R1-2), D15 (R1-3) |
| Mischievous Marai | 562 | Unit | azir ismcts D:019 (base, no trigger); hidden play akali__teemo__s20261112__random__random D:110 | trigger D:110 (targets = enemy units here) ; 27 `TGT_NOT_OFFERED` | D:113 (2 damage) | FULL | ok; K1 applies (play trigger queued before she enters), no outcome change seen |
| Back Off | 604 | Spell | azir ismcts D:360 (hand), akali__irelia__s20263001 D:116 (showdown, target at play); hide + facedown play akali__khazix__s20263031__random__ismcts-sims20 D:199 / D:216 | - | D:362 (stun + draw); khazix D:218 (facedown: stun, no draw; stunned unit deals no damage D:218.05) | FULL | ok |
| Noxus Hopeful | 12 | Unit | 285 plays tallied: E=4 with no earlier card that turn (110), E=2 with one (168) | - | - | FULL | ok (the E=2-without-prior cases are D18 (R1-6) carry-overs, e.g. akali__ezreal D:200) |
| Block | 57 | Spell | akali__khazix__s20263031 D:187 | - | D:196 (Tentacle 1M defends as 4M and takes the damage first) | PART (play from facedown not read) | ok |
| Not So Fast | 368 | Spell | akali__khazix__s20261107__random__random D:152 (in response to Voidreaver choosing Astral Heron) | - | D:157 (ability countered); 24 spell counters tallied | FULL | ok; D21 (R1-10) (CODE-ONLY) |
| Sterak's Gage | 379 | Gear | akali__reksai__s20261110 D:064..070 (3E+2P, reaction) | Quick-Draw D:070; Equip azir ismcts D:338 | attach same D | FULL | D14 (R1-2), D15 (R1-3) |
| Kai'Sa, Survivor | 39 | Unit (champion) | akali__azir__s20261101 D:226 / D:242 | Accelerate prompt `pay [1][Fury]` D:227, D:242 (after the base cost: K4); conquer trigger 124 fired | 122 x `conquer -> draw 1` | FULL | ok; K1/K4 |
| Blitzcrank, Impassive | 67 | Unit (champion) | akali__zed__s20261117__random__random D:09x (5E+1P) | play trigger D:094 (may + target at finalization); hold 17 x | D:102 (pulled Shadow Clone, combat with the pulled unit's controller as attacker); `returned to owner's hand on hold` | PART ([Tank] in damage assignment not read) | ok; K1 |
| Darius, Trifarian | 27 | Unit (champion) | akali__kaisa__s20261106__random__random | D:201 (after Shuriken Flip resolved as 2nd card) | D:203 (+2, ready; he then moves D:204) | FULL | ok |
| Sigil of the Storm | 282 | Battlefield | - | azir ismcts D:158, D:364 | D:160-161, D:366-367 (rune picked at resolution: CR 355.10.f) | FULL | ok |
| Void Gate | 291 | Battlefield | - | - | akali__reksai D:077, D:084 (ability 2 -> 3), akali__akali D:109 (spell 3 -> 4 at Void Gate, 3 elsewhere) | FULL | ok |
| Forgotten Monument | 523 | Battlefield | - | - | `SCORE_GATED` akali__akali__s20261100 D:025, akali_sb__leblanc_sb__s20262108 D:021 (both a player's 2nd turn); ungated on later turns | PART (a Hold on turn 1-2 never arose) | ok |
| Ferrous Forerunner | 344 | Unit | 127 plays (6E+1P) | akali__rengar__s20263046__random__ismcts-sims20 D:132 | D:134 (two 3M Mech tokens) | PART (where the tokens were put not read on a board) | ok |
| Disarming Rake | 355 | Unit | akali_sb__diana_sb__s20262102__random__random D:33x | D:332 (may, target at finalization) | D:337 (attached Sterak's Gage killed) | FULL | ok; K1 |
| Thermo Beam | 22 | Spell | 54 plays (5E+2P) | - | akali_sb__azir_sb__s20262101__random__random D:191 (6 gear, both players', attached included) | FULL | ok (2 plays by the non-turn player are D5 (R1-1): it has [Action], so those are legal) |
| En Garde | 46 | Spell | 213 plays | - | 262 resolutions tallied (+1, and +1 more in 120) | PART (the "only unit you control there" count not checked on a board) | ok |
| Against the Odds | 323 | Spell | akali_sb__azir_sb__s20262101 D:114 | - | D:118 (no enemy there: no change); 3 resolutions with a bonus | PART (the amount not checked against the number of enemy units) | ok |
| Decree of Focus | 827 | Spell | akali_sb__azir_sb__s20262101 D:230 | - | D:234 (+4) | PART ("being chosen by an enemy Fury spell" never seen; the combat condition not checked on a board) | ok |
| Akali, Silent | 825 | Unit | 68 plays (4E+1P) | move-to-battlefield trigger 99 x; no trigger when Rogue Assassin moves her to base (4 cases) | +2 this turn | PART ("can't be chosen" checked only on the lines that name her: every choosing spell/ability was her controller's or chose nothing; "unless I'm in combat" not seen) | ok |

Totals: FULL 21, PART 9, NONE 0.

### reader 2: azir, irelia

| card | id | type | played (game, D) | ability triggered / activated (game, D) | resolved (game, D) | read | result |
|---|---|---|---|---|---|---|---|
| Emperor of the Sands | 512 | Legend | - | activate akali__azir__s20261101__random__random D:074; akali__azir__s20263000__ismcts-sims20__random D:137; gate "only if an Equipment was played this turn" checked on all 588 offers in 78 games (0 offers without an Equipment play that turn; 0 misses with legend ready + a ready rune; never offered outside own Neutral Open) | D:076 (token to base, exhausted, Weaponmaster trigger D:076.04) | FULL | D22 (R2-1) (the granted Weaponmaster), D4 (R2-9) |
| Azir, Sovereign | 497 | Unit (champion) | azir__azir__s20261119__random__random D:140 (Accelerate paid: [1] + one Order rune, 13 payments checked against rune counts); azir__diana__s20263004__ismcts-sims20__random D:026 (declined) | attack trigger azir__azir__s20261119 D:343, D:358; azir__teemo__s20261130__random__random D:336; rengar__azir__s20261299__random__random D:304; azir_sb__akali_sb__s20262118__random__random D:240 | D:347, D:362; D:343; D:308; D:247 | FULL | D23 (R2-3), D24 (R2-4); K1/K4 (Accelerate asked after the base cost) |
| Hidden Blade | 213 | Spell | akali__azir__s20261101 D:050 (target at a battlefield, 2E+1 Order); facedown azir__azir__s20261119 idx=439 D:405 (targets only at the hidden battlefield; cost 0; hidden an earlier turn: 51 facedown plays checked) | - | akali__azir__s20261101 D:056 (enemy unit: P1 draws 2); akali__azir__s20263000 D:057 (own unit: P2 draws 2) | PART (no window read where it chose a [Deflect] unit; replaced-death cases of "its controller" not seen) | D25 (R2-6) |
| Doran's Shield | 356 | Gear | akali__azir__s20261101 D:072 | Equip akali__azir__s20261101 D:085 ([G]; offered only with a Calm rune, n=1021 offers) | attach D:085.04; +1 and [Tank] in azir__diana__s20261120__random__random D:223.04, azir__azir__s20261119 D:362.10 | FULL | D15 (R2-2) |
| Brutalizer | 365 | Gear | akali__azir__s20263000 D:248 | Equip azir__azir__s20261119 D:018, D:072, D:117 | +1 and +2 the turn it is attached (5M at idx=19, 3M from the next turn's Scoring Step, SNAP 35) | FULL | D15 (R2-2); stale +2 through the next Awaken / Beginning Step, see D23 (R2-3) "also seen" |
| Eye of the Herald | 474 | Gear | azir__diana__s20263004 D:051 | Equip ([Y]); "When I move" azir__azir__s20261119 D:356.02, azir_sb__akali_sb__s20262118 D:247.03 (bearer moved by Azir's trigger), azir__teemo__s20261130 D:332.04 | D:358 (Recruit 1M here, exhausted), D:249, D:334 | FULL | D15 (R2-2) |
| Guards! | 475 | Spell | hand: azir__diana__s20263004 D:112-119; facedown: azir__kaisa__s20263005__ismcts-sims20__random D:149 (hidden D:142 turn 7, played turn 8 for 0) | reflexive "pay [C] to ready it": FINALIZE D:116.08 / D:152.08 | D:115-119 (base only: no battlefield controlled); D:151-155 (only Trifarian War Camp offered from facedown); Weaponmaster D:157 | FULL | D5 (R2-5) (azir__teemo__s20261130 D:343.04), D25 (R2-6) (azir__diana__s20263004 idx=53), D22 (R2-1) |
| B.F. Sword | 482 | Gear | akali__azir__s20261101 D:026 | Equip ([Y], n=829 offers with an Order rune); Weaponmaster D:080 | +3 in Might sums (akali__azir__s20263000 D:242.05, D:291.05, D:317.06) | FULL | D15 (R2-2) |
| Arise! | 513 | Spell | azir__diana__s20261120 D:193.03 (6E+1P) | reflexive "ready up to two": FINALIZE D:206.07 (0, 1 or 2 selectable) | D:202-210: 4 Equipment -> 4 tokens, location per token, 4 Weaponmaster triggers | FULL | D5 (R2-5) (this very play: opponent's turn, combat showdown), D22 (R2-1) |
| Soul Sword | 601 | Gear | akali__azir__s20261101 D:005 | Equip ([G]); Weaponmaster azir__azir__s20261119 D:342 | +1 in sums (azir__azir__s20261119 SNAP 472: 8M = 2+1+3+1+1) | PART ([Level 3] +1 never active: no bearer with a 3+ XP controller found) | D15 (R2-2) |
| Deathgrip | 484 | Spell | akali__azir__s20261101 D:021 (one target); akali__azir__s20263000 D:167 (two targets, as a Reaction) | - | D:025 (kill, draw 1); D:171 (kill 2M token, +2 to the other, draw 1) | PART ("If you do" with the death replaced not seen) | D28 (R2-10) |
| Hand Hammer | 814 | Gear | akali__azir__s20263000 D:320 | Equip azir__azir__s20261119 D:276, D:333 | +1; +2 only with exactly one other friendly unit at its battlefield: azir__diana__s20261120 SNAP 468 (5M, one other), azir__azir__s20261119 SNAP 472 (alone, no +2), SNAP 507-534 (two others, no +2), base (3M) | FULL | D15 (R2-2) |
| Trifarian War Camp | 289 | Battlefield | - | static +1: akali__azir__s20263000 D:239.06, D:242.05, D:291.05, D:317.06; azir__irelia__s20261123__random__random D:351.04; azir__kaisa__s20263005 D:194.05 (attackers and defenders) | - | FULL | D23 (R2-3) |
| Hall of Legends | 524 | Battlefield | - | azir__akali__s20261118__random__random D:069 (may + [1] at finalization), D:128; azir__teemo__s20261130 D:351.28 | D:072 (legend readied), D:132 | FULL | D4 (R2-9) (declined / not offered items stay on the chain) |
| Seat of Power | 531 | Battlefield | - | azir__diana__s20263004 D:027.07; 336 resolutions checked against the board (draw = other battlefields the conqueror controls; 0 mismatches) | akali__rengar__s20261111__random__random idx=311 (draw 1), idx=140 (draw 0) | FULL | ok |
| Salvage | 224 | Spell | 168 plays (122 choosing no gear, 46 choosing a gear of either player, tokens included); cost 2E+1P | - | 165 | PART (read by aggregate and class only: no single window opened) | ok on what was read |
| Ashe, Focused | 731 | Unit | akali_sb__azir_sb__s20262101__random__random D:125 (5E+1 Order) | play trigger D:131.02; delayed "when they hold" D:140.09 | D:133-134 (hand of 5 revealed, one banished, chosen at resolution); D:142 (returned on their hold) | FULL | ok; K1 |
| Decree of Unity | 918 | Spell | 27 plays; every chosen target is an enemy Chaos unit or gear (17 distinct, domains checked); Deflect paid for Vex, Apathetic (e.g. D:190-193, D:029-032, D:173-176 of the three games printed by `grep 'Decree of Unity -> Vex, Apathetic'`) | - | 26 | PART (target list read from the chosen targets, not from a full option list; no resolution window opened) | D5 (R2-5) (irelia_sb__azir_sb__s20262191 idx=147 and three more: played in a combat showdown) |
| Charm | 43 | Spell | vex_sb__irelia_sb__s20262339__random__random D:295.07 (1E+1P, enemy unit) | - | D:300-301 (destination chosen after RESOLVE) | PART (Deflect on a Charm target not read) | K3; D5 (R2-5); D26 (R2-7) |
| Blade Dancer | 555 | Legend | - | choose-trigger irelia__teemo__s20263051__random__ismcts-sims20 D:125; irelia__yi_rebuild__s20263066__random__ismcts-sims20 D:064; from Abandoned Hall kaisa_sb__irelia_sb__s20262213__random__random D:141; conquer-trigger same teemo game D:143 | D:130 (exhaust + [A] paid at finalization, unit readied at resolution); D:069; D:145 ([1] -> ready me) | FULL | D15 (R2-2) (Equip does not trigger it), D4 (R2-9) |
| Irelia, Fervent | 380 | Unit (champion) | 160 plays (5E) | chosen by its controller: D:125.05 (teemo game), diana__irelia__s20261141__random__random D:314; readied: by Blade Dancer D:130.03, in Awaken (68 Awaken phases checked: trigger iff she was exhausted); Deflect: 58 payments, e.g. irelia_sb__vex_sb__s20262203__random__random D:037.05 | D:128, D:132 (+1 each) | FULL | D15 (R2-2) |
| Defiant Dance | 511 | Spell | irelia_sb__diana_sb__s20262192__random__random D:120; irelia_sb__vex_sb__s20262203 D:035 (1E + Calm or Chaos power; pair chosen at finalization; Deflect per enemy target) | - | D:129; D:041 (+2 / -2) | FULL | ok |
| Scuttle Crab | 615 | Unit | irelia__fiora__s20261194__random__random D:258 (2E), play trigger D:260.02 | Deathknell diana__vex__s20263054__random__ismcts-sims20 D:277.07 | D:279 (+1 XP; reveal) | PART (what the reveal / "look at their facedown cards" shows to whom is not visible in an omniscient trace) | K1 |
| Treasure Hunter | 451 | Unit | irelia__akali__s20261190__random__random D:207 | akali__irelia__s20261105__random__random D:110.02; akali__irelia__s20263001__ismcts-sims20__random D:245.04; move to base irelia__khazix__s20261197 D:256.01 | D:247 (Gold token, exhausted) | FULL | ok; K2 (the Gold token's [Add] waits on the chain: akali__irelia__s20261105 D:145-148) |
| Boots of Swiftness | 454 | Gear | 102 plays | Equip ([P]) irelia__akali__s20261190 D:206, D:213 | +2 (Scuttle Crab 2M); [Ganking]: battlefield-to-battlefield moves offered only to a bearer (29 offers, 0 without Boots) | FULL | D15 (R2-2) |
| Edge of Night | 460 | Gear | hand akali__irelia__s20261105 D:102 (3E); facedown azir__irelia__s20261123 D:323 | play trigger D:105.02 (from hand), D:323.04 (from facedown, unit here chosen at finalization) ; Equip D:108 | D:107 (nothing); attach D:324 | FULL | D27 (R2-8), D15 (R2-2), D25 (R2-6); K1 |
| Adaptatron | 56 | Unit | 55 plays (4E) | ezreal_sb__irelia_sb__s20262159__random__random D:195.04 (may + gear at finalization; any player's gear) | D:199 (gear killed, buffed to 4M) | FULL | ok |
| Pyke, Returned | 707 | Unit | 48 plays, 18 hides, 11 facedown plays (all a later turn, cost 0) | diana_sb__irelia_sb__s20262141__random__random D:329.09; azir__irelia__s20261123 D:354.08 + D:354.11 | D:331 (Gold exhausted); D:356.02 and D:358.02 (nothing) | PART ([Backline] damage ordering not read) | D27 (R2-8), D25 (R2-6) |
| Vex, Cheerless | 467 | Unit | 16 plays (5E+1P) | static: only one spell was played while she was in combat in all games (vex_sb__irelia_sb__s20262339 D:296.02) | - | PART (friendly discount and the [1] minimum never exercised) | D26 (R2-7) |

### reader 3: diana

| card | id | type | played (game, D) | ability triggered / activated (game, D) | resolved (game, D) | read | result |
|---|---|---|---|---|---|---|---|
| Scorn of the Moon | 786 | Legend | - | akali__diana__s20261102__random__random D:008 (Neutral Open, own turn); diana__ezreal__s20261139__random__random D:221 (Closed, opponent's turn); yi_rebuild_sb__diana_sb__s20262390__random__random D:143 (with Focus in combat) | akali__diana__s20261102__random__random D:010; spent: yi_rebuild_sb__diana_sb__s20262390__random__random D:145 (Singularity E=6 with 5 runes) | FULL | K2; D30 (R3-4), D31 (R3-5) (outcomes of K2); spending restriction ok |
| Diana, Lunari | 641 | Unit (champion) | azir__diana__s20263004__ismcts-sims20__random D:240 (3E from Champion Zone) | diana__azir__s20261137__random__random D:064; diana__ezreal__s20263008__ismcts-sims20__random D:140 | diana__ezreal__s20263008__ismcts-sims20__random D:145 (spell -> drawn); diana__fiora__s20261140__random__random D:027 (non-spell); diana__azir__s20261137__random__random D:079 (Predict recycled, new top revealed) | FULL | D29 (R3-1), D7 (R3-3), D30 (R3-4); effect itself ok |
| Stupefy | 95 | Spell | diana_sb__khazix_sb__s20262143__random__random D:120 | - | akali_sb__ezreal_sb__s20262103__random__random D:134 (-1, draw); min 1: diana_sb__diana_sb__s20262138__random__random D:018 (`-> 1M (min 1)`) | FULL | D32 (R3-6) (when replayed by Fizz) |
| Ravenbloom Student | 103 | Unit | diana__khazix__s20263009__ismcts-sims20__random D:136 | azir__diana__s20263004__ismcts-sims20__random D:164 | same D:166 | FULL | ok (trigger after the spell resolves is CR 420.4.a) |
| Traveling Merchant | 185 | Unit | diana_sb__leblanc_sb__s20262144__random__random D:098 | diana__khazix__s20263009__ismcts-sims20__random D:077 | same D:078 | PART: "discard 1" with an empty hand (class draws anyway) not looked for in a trace | ok |
| Tideturner | 199 | Unit | azir__diana__s20263004__ismcts-sims20__random D:258 (hand); diana__khazix__s20263009__ismcts-sims20__random D:231 (from facedown, Reaction, opponent's turn); vex__mel__s20261343__random__random D:190 (from facedown, E=0); hide: diana__khazix__s20263009__ismcts-sims20__random D:207 | azir__diana__s20263004__ismcts-sims20__random D:260 | same D:264 (swap; Hwei's move trigger fires); blocked by Vex: diana__khazix__s20261143__random__random D:365 | FULL | K1 (choices before it enters; no outcome change seen); ok |
| Moonfall | 755 | Spell | diana__diana__s20261138__random__random D:301 | - | diana__khazix__s20263009__ismcts-sims20__random D:162; with move: diana__vex__s20263054__random__ismcts-sims20 D:276 (moved + -2 each, then damage step) | FULL | ok |
| Gust | 169 | Spell | azir__diana__s20263004__ismcts-sims20__random D:076; akali__irelia__s20263001__ismcts-sims20__random D:219 | - | same D:080 / D:223 | FULL | D8 (R3-7) (via Fizz with no legal target) |
| Ride the Wind | 173 | Spell | diana_sb__rengar_sb__s20262147__random__random (resolves D:026) | - | same D:026-027; diana__ezreal__s20263008__ismcts-sims20__random D:139 | FULL | K3 |
| Hard Bargain | 457 | Spell | azir__diana__s20261120__random__random D:172; diana__azir__s20261137__random__random D:236 | - | azir__diana__s20261120__random__random D:177 (paid [2], spell saved); akali_sb__irelia_sb__s20262105__random__random D:113 (countered; Repeat execution no-op) | PART: a chain with two spells (choice of which to counter; Repeat countering a second) not found | D21 (R3-9); D8 (R3-7) (via Fizz) |
| Eclipse | 625 | Spell | diana__khazix__s20263009__ismcts-sims20__random D:079 | - | same D:084; via Fizz diana__vex__s20261149__random__random D:208 | FULL | D32 (R3-6) |
| Sprite Fountain | 640 | Gear | diana__mel__s20263039__random__ismcts-sims20 D:045 (2E+1P) | play effect D:048; Deathknell D:115 | D:050 (ready 3M Sprite, moves same turn D:072); Temporary kill in own Beginning Step before scoring D:115; Deathknell token D:117 | FULL | ok |
| Star-Crossed | 690 | Spell | diana__vex__s20263054__random__ismcts-sims20 D:186 (Deflect paid for Vex before the cost) | - | same D:192 | FULL | D8 (R3-7) |
| Smoke Screen | 93 | Spell | irelia__diana__s20261192__random__random D:142 | - | same D:146 (-4, min 1) | FULL | D32 (R3-6) (via Fizz) |
| Rebuke | 172 | Spell | yasuao__diana__s20263061__ismcts-sims20__random D:118 (3 options = the units at battlefields) | - | azir__diana__s20263004__ismcts-sims20__random D:164 | FULL | ok |
| Acceptable Losses | 179 | Spell | akali_sb__ezreal_sb__s20262103__random__random D:061 (via Fizz) | - | same D:063; diana__teemo__s20261148__random__random D:174 (each player picks own gear at resolution, CR 355.10.e) | PART: an instance where BOTH players had gear was not opened | ok |
| Flash | 309 | Spell | reksai__diana__s20261282__random__random D:239 (units chosen before the cost) | - | same D:243 | PART: two units moved in one cast not opened | ok |
| Switcheroo | 466 | Spell | diana__ezreal__s20263008__ismcts-sims20__random D:083 (Deflect for Vex paid at the pick) | - | same D:090 (4 <-> 2) | PART: a play from facedown restricted to that battlefield (CR 811.1.d.2) not checked against a board | D8 (R3-7) |
| Hwei, Brooding Painter | 642 | Unit | diana__khazix__s20263009__ismcts-sims20__random D:090 (5E+1P) | same D:166 | D:168-171 (Spell); azir__diana__s20263004__ismcts-sims20__random D:267-271 (Gear, runes named at finalization); Unit +3: akali__mel__s20261109__random__random D:190 | FULL | ok |
| Vex, Apathetic | 712 | Unit | diana__khazix__s20263009__ismcts-sims20__random D:208 | diana__ezreal__s20261139__random__random D:402 | same D:405 (stun); can't move: diana__khazix__s20261143__random__random D:365, diana__yasuao__s20261151__random__random D:163; Deflect: diana__vex__s20263054__random__ismcts-sims20 D:187 | FULL | D33 (R3-8) |
| Fizz, Trickster | 461 | Unit | akali__diana__s20261102__random__random D:027 (3E+1P) | same D:031 | D:035 (Stupefy from trash, Energy waived); Power still paid: diana__mel__s20263039__random__ismcts-sims20 D:447 (`PAY_COST: Switcheroo E=0 P=2`) | PART: "Then recycle it" has no trace line (class + chain_manager.cpp:547-562 read) | D32 (R3-6), D8 (R3-7); K1 |
| Targon's Peak | 284 | Battlefield | - | diana_sb__rengar_sb__s20262147__random__random D:031 | D:033 (scheduled); Ending Step D:034-037 (runes named when the delayed ability is finalized) | FULL | ok (opponent's runes offered: judge ruling) |
| Abandoned Hall | 761 | Battlefield | - | diana__ezreal__s20261139__random__random D:225 (for the non-owner P2 too) | accepted: diana__diana__s20261138__random__random D:119-122 | FULL | ok |
| Turn to Dust | 632 | Spell | diana_sb__akali_sb__s20262136__random__random D:245 | - | same D:249; the gear dies in its controller's Beginning Step D:264 | FULL | D5 (R3-2), D32 (R3-6) |
| Decree of Insight | 848 | Spell | diana_sb__khazix_sb__s20262143__random__random D:141 | - | same D:145 (-5, no minimum) | PART: "Ignore [Deflect]" on a Body unit WITH Deflect not found in a trace | D34 (R3-10) |
| Singularity | 105 | Spell | yi_rebuild_sb__diana_sb__s20262390__random__random D:145 | - | diana_sb__khazix_sb__s20262143__random__random D:184 (6 to each of two) | FULL | D5 (R3-2) |
| Up from the Deep | 887 | Spell | diana_sb__irelia_sb__s20262141__random__random D:323; Flow: akali__khazix__s20263031__random__ismcts-sims20 D:158 | - | D:327; D:160-162 (location asked per token) | PART: "Then banish it" after a Flow play has no trace line | D5 (R3-2) |
| Tail-Cloaked Matriarch | 891 | Unit | diana_sb__leblanc_sb__s20262144__random__random D:041 | Empower: azir__khazix__s20261125__random__random D:292 ([2]+1 power) | D:294-298 (unit from trash to base, its play effect fires) | PART (only the Empower cost and one of the two plays from the trash were read) (only 2 traced plays from trash exist) | ok |

Seat of Power (the list's third battlefield, not on my card file): `SEAT OF POWER: conquer -> draw 0` x282,
`draw 1` x54 in the scan; diana__ezreal__s20263008__ismcts-sims20__random D:143; ok.

### reader 4: fiora

| card | id | type | played (game, D) | ability triggered / activated (game, D) | resolved (game, D) | read | result |
|---|---|---|---|---|---|---|---|
| Grand Duelist | 519 | Legend | - (legend) | akali__fiora__s20261104__random__random D:040.06; 485 triggers in 78 games | same game D:049.02 (channel 1 exhausted); declined: akali_sb__fiora_sb__s20262104__random__random D:070.03-072.02 | FULL | D35 (R4-1), D36 (R4-2), D4 (R4-3), D6 (R4-4), D40 (R4-10), Q1 |
| Fiora, Worthy | 500 | Unit (champion) | ezreal_sb__fiora_sb__s20262158__random__random D:062.03 | akali__fiora__s20261104__random__random D:040.05, cost [Y] paid at finalization D:041.04 | akali__fiora__s20261104__random__random D:257.03 (readied Poppy); subject gone: same game D:051.02 | FULL | D35 (R4-1), D36 (R4-2), D4 (R4-3), D40 (R4-10), Q1 |
| Challenge | 128 | Spell | fiora__diana__s20261174__random__random D:103.03 | - | akali__fiora__s20261104__random__random D:132.02 | FULL | ok (offered in "combat staged" windows too, legal for [Action]) |
| Dazzling Aurora | 160 | Gear | fiora__fiora__s20261176__random__random D:102.03 | diana__fiora__s20261140__random__random D:238.03 (Ending Step) | same game D:240.02-241.03 | PART: "recycle the rest" has no trace line (order / placement not checked); a deck with no unit not seen | D35 (R4-1) (the played unit's becomes-Mighty trigger is finalized after the Expiration body), Q3 |
| Divining Shells | 723 | Gear | fiora__akali__s20261172__random__random D:013.03, [Vision] D:017.02-018.01 | fiora__reksai__s20261182__random__random D:027.05-028.01 | same game D:034.02 | FULL | D38 (R4-8) |
| Elder Dragon | 680 | Unit | diana__fiora__s20261140__random__random D:136.03 | same game D:152.02 (one prompt per location with enemies) | fiora__diana__s20261174__random__random D:245.02 (1 damage kills) | FULL | ok; K1 (play trigger finalized before entering: no outcome change seen); D35 (R4-1) |
| Flurry of Blades | 133 | Spell | ezreal_sb__fiora_sb__s20262158__random__random D:095.03 | - | fiora__reksai__s20261182__random__random D:032.02; scan 55/55 resolutions hit exactly the units at battlefields | FULL | ok |
| Harnessed Dragon | 234 | Unit | diana__fiora__s20261140__random__random (D:084 resolve) | 39 target prompts; scan 37/39 lists = all enemy units, 2 = Ruin Runner left out (can't be chosen) | diana__fiora__s20261140__random__random D:084 | FULL | ok; K1 |
| Mobilize | 134 | Spell | ezreal_sb__fiora_sb__s20262158__random__random D:024.03 | - | fiora__irelia__s20263016__ismcts-sims20__random D:076.02 (channel); 6x "rune deck empty -> draw 1" | FULL | D5 (R4-5), D6 (R4-4) |
| Poppy, Paragon | 678 | Unit | akali__fiora__s20261104__random__random D:244.03 | same game D:249.02 | D:251.02 (opp at 4: nothing); D:302.02 (ready + 3 XP); scan 62 resolutions: fires iff opponent score >= 5 | FULL | ok; K1; Q1 |
| Rampage | 870 | Spell | akali__fiora__s20261104__random__random D:076.03 | additional cost D:082.00-082.02 | same game D:084.02 | FULL | D5 (R4-5), D1 (R4-13), D6 (R4-4) |
| Rengar, Trophy Hunter | 682 | Unit | hand: akali_sb__fiora_sb__s20262104__random__random D:063.03; [Ambush]: fiora__irelia__s20263016__ismcts-sims20__random D:077.03 (enemy-only battlefield), akali_sb__fiora_sb__s20262104__random__random D:164.03 | - | - | FULL | D37 (R4-6); Q1 |
| Repulse | 668 | Spell | akali__fiora__s20261104__random__random D:147.03 | - | same game D:151.02-152.04 (counter spell); fiora__akali__s20261172__random__random D:246.02 | PART: countering an ABILITY and the "no other friendly unit" refusal not looked at in a trace | D41 (R4-11), D21 (R4-7) (same top-only design) |
| Rift Herald | 741 | Unit | fiora__fiora__s20261176__random__random D:070.03 | move: ezreal__fiora__s20263012__ismcts-sims20__random D:184.04; deathknell: fiora__fiora__s20261176__random__random D:083.05 | move D:186.02-188.06; deathknell D:088.02-090.02 | FULL | ok; D39 (R4-9), D35 (R4-1) |
| Riposte | 520 | Spell | vex_sb__fiora_sb__s20262338__random__random D:136.03 | - | fiora__diana__s20261174__random__random D:116.02-116.05 | FULL | D21 (R4-7) |
| Sacrifice | 735 | Spell | akali__fiora__s20261104__random__random D:043.03 (kill as cost D:044.04) | - | fiora_sb__diana_sb__s20262174__random__random D:340.02; scan 203/203: offered iff a friendly Mighty unit and [1] | FULL | D39 (R4-9) |
| Sett, Brawler | 164 | Unit | fiora__diana__s20261174__random__random D:060.03 | played: akali__fiora__s20261104__random__random D:038.02; conquer: fiora__zed__s20261189__random__random D:103.06; ability: ezreal_sb__fiora_sb__s20262158__random__random D:197.01 | D:040.02; D:107.02; D:199.02 | FULL | D36 (R4-2); K1 |
| Shepherd's Heirloom | 720 | Gear | diana__fiora__s20261140__random__random D:232.03 | play: D:234.02; Equip: D:237.01 (1 XP, +2) | D:236.02 | PART: that Equip is withheld at 0 XP was not checked against a board | ok |
| Amateur Recital | 763 | Battlefield | - | azir__fiora__s20261122__random__random D:050.09 | D:054.02 (unit in base on next board) | PART: no pick of an ENEMY unit verified (list = every unit at a battlefield in 45/45 prompts) | ok; D4 (R4-3) |
| Risen Altar | 950 | Battlefield | - | kaisa_sb__fiora_sb__s20262212__random__random D:188.03 (Kayle's Empower [2]) | - | PART: one instance; the "[A] less" half not exercised | ok |
| Sunken Temple | 532 | Battlefield | - | azir_sb__fiora_sb__s20262122__random__random D:179.06 | D:182.02; scan 185 conquests there: trigger iff a Mighty conquering unit (66 / 119) | FULL | ok; D4 (R4-3) |
| Akshan, Mischievous | 431 | Unit | fiora_sb__akali_sb__s20262172__random__random D:126.03 | D:131.03-132.03 | D:134.02 | PART: "if it's an Equipment, attach it to me" and control reverting when he leaves not seen (3 steals, none Equipment) | D1 (R4-13), D40 (R4-10) |
| Fiora, Victorious | 232 | Unit | ezreal_sb__fiora_sb__s20262158__random__random D:253.03 | passive: Deflect charged diana_sb__fiora_sb__s20262140__random__random D:213.03 | - | PART: [Ganking] never used; [Shield] see D42 (R4-12) | D42 (R4-12) |
| Forge of the Future | 212 | Gear | fiora_sb__diana_sb__s20262174__random__random D:350.03 (Recruit D:354.04) | same game D:368.01 (killed at finalization D:368.04) | D:370.02-374.03 | FULL | D41 (R4-11) |
| Kayle, Justified | 921 | Unit | ezreal_sb__fiora_sb__s20262158__random__random D:010.03 | fiora_sb__leblanc_sb__s20262180__random__random D:321.01 | D:323.03 (third: 9 [M] in D:324.05); [Deflect 3] vex_sb__fiora_sb__s20262338__random__random D:133.05 | PART: [Ganking] at three Empowers not used | ok |
| Punch First | 419 | Spell | ezreal_sb__fiora_sb__s20262158__random__random D:168.03 | - | same game D:174.02 (30 resolutions, all +5) | FULL | ok |

18 FULL, 8 PART, 0 NONE.

### reader 5: ezreal, kaisa

| card | id | type | played (game, D) | ability triggered / activated (game, D) | resolved (game, D) | read | result |
|---|---|---|---|---|---|---|---|
| Prodigal Explorer | 514 | Legend (ezreal) | - | activate: ezreal__fiora__s20261158__random__random D:192; ezreal__teemo__s20261166__random__random D:129; as reaction akali__ezreal__s20261103__random__random D:248; on opp's turn ezreal__akali__s20261154__random__random D:115 | ezreal__fiora__s20261158 D:194 (draw 1) | FULL | ok. Count checked over all 76 non-mirror ezreal games: 75 turns with an offer, every one after >= 2 enemy units/gear chosen by spells (each target, each Repeat choice); 1582 turns without an offer, none with 2 such choices. 71 resolutions, never twice a turn, never offered again once exhausted. See QUESTIONS Q1 |
| Ezreal, Prodigy | 470 | Unit/Champion (ezreal) | diana__ezreal__s20263008__ismcts-sims20__random D:099 (3E + Chaos power); ezreal__fiora__s20263012__ismcts-sims20__random D:015 | play trigger: diana__ezreal__s20263008 D:103; discount on Repeat: ezreal__teemo__s20261166 D:162-163, mel__ezreal__s20261265 D:544-545 | diana__ezreal__s20263008 D:109-110; ezreal__fiora__s20263012 D:021-022 | FULL | D43 (R5-1) (Accelerate not discounted); K1. Repeat discount ok; Hide and Deflect not discounted (correct) |
| Pack of Wonders | 181 | Gear (ezreal) | diana__ezreal__s20263008__ismcts-sims20__random D:393 | ezreal__leblanc__s20263013__ismcts-sims20__random D:075, D:698; facedown card: ezreal__teemo__s20261166 D:334 | D:078 (unit), ezreal__ezreal__s20261157 D:208 (gear), ezreal__teemo__s20261166 D:337 (facedown card) | FULL | ok (offered only in own Neutral Open main phase: 716 offers, 0 elsewhere) |
| Wages of Pain | 393 | Spell (ezreal) | from hand in showdown ezreal__reksai__s20263043__random__ismcts-sims20 D:044; hide diana__ezreal__s20263008 D:396; from facedown ezreal__leblanc__s20263013 D:431 | - | ezreal__reksai__s20263043 D:049; diana__ezreal__s20263008 D:392, D:402 | FULL | D44 (R5-3), D25 (R5-5). Targets always a unit at a battlefield (598 options checked); from facedown always at that battlefield; Gold exhausted |
| Bellows Breath | 402 | Spell (ezreal; kaisa side) | ezreal__leblanc__s20261162__random__random D:216; zero targets ezreal__teemo__s20261166 D:159 | Repeat: ezreal__leblanc__s20261162 D:219-221 | D:230-231 | FULL | D45 (R5-4) |
| Deadly Flourish | 635 | Spell (ezreal) | ezreal__fiora__s20263012__ismcts-sims20__random D:289 | delayed "when it dies": diana__ezreal__s20261139 D:049 | ezreal__fiora__s20263012 D:296; diana__ezreal__s20261139 D:047-051 | FULL | D5 (R5-2), D44 (R5-3). Enemy units only (options checked); 47 same-turn deaths -> Gold, 2 later-turn deaths -> none |
| Bewitching Spirit | 683 | Unit (ezreal) | diana__ezreal__s20263008 D:069; ezreal__fiora__s20263012 D:052 | D:072 / D:055 (player chosen at finalization) | D:075 / D:058 | FULL | ok; K1 |
| Thousand-Tailed Watcher | 116 | Unit (ezreal; kaisa side) | ezreal__leblanc__s20261162__random__random D:185 | D:193 trigger; Accelerate D:194 | D:196 | FULL | D43 (R5-1); K1/K4 |
| Treasure Trove | 186 | Gear (ezreal) | ezreal__leblanc__s20263013 D:426 | activate ezreal__fiora__s20263012 D:099; leave-board trigger D:101 (kill), ezreal__ezreal__s20261157 D:208 (returned to hand) | D:101, D:103 | FULL | ok (the trace does not name the rune recycled for [P]; class asks Chaos) |
| The List | 700 | Gear (ezreal) | diana__ezreal__s20263008 D:300; ezreal__leblanc__s20263013 D:693 | name a tag D:301-304; activate ezreal__viktor__s20263058__random__ismcts-sims20 D:125 | D:128, D:248 | FULL | D46 (R5-6) |
| Vilemaw's Lair | 290 | Battlefield (ezreal) | - | - | standard moves: 21 games, 0 "Move -> Base" options for a unit there (112 for units at other battlefields); effect move ezreal_sb__yasuao_sb__s20262169__random__random D:092 | PART | ok. Not exercised: a recall from the Lair (no combat tie there in the 21 games) |
| Frozen Fortress | 768 | Battlefield (ezreal, kaisa) | - | 1636 triggers, all in a Beginning Step, one per turn, both players' turns: ezreal__fiora__s20261158 D:173.07 (P1's turn), D:195.07 (P2's turn) | D:175; ezreal__reksai__s20261164 D:059; ezreal__viktor__s20263058 D:068 | FULL | ok. See NOT OBSERVED (a hold stopped on the holder's own turn) |
| Kha'Zix, Mutating Horror | 705 | Unit/Champion (ezreal sb) | Ambush as reaction ezreal_sb__azir_sb__s20262155__random__random D:137 | ezreal_sb__akali_sb__s20262154__random__random D:137.05 | D:139, D:141 (+2, 2 XP each of two copies); no effect with two enemies D:280 | FULL | ok |
| Ravenbloom Prefect | 889 | Unit (ezreal sb) | 156 plays | kaisa_sb__ezreal_sb__s20262211__random__random D:289.04 (Gold token), irelia_sb__ezreal_sb__s20262193__random__random D:219 (gear card) | D:292 (banish), D:220 | FULL | ok (cost paid at finalization) |
| Pickpocket | 397 | Unit (ezreal sb) | 50 plays | ezreal_sb__azir_sb__s20262155 D:228 | D:231; leblanc_sb__ezreal_sb__s20262247 D:483 | FULL | ok; K1 |
| Angler Beast | 694 | Unit (ezreal sb) | 18 plays | ezreal_sb__azir_sb__s20262155 D:199 | D:199 (own Bewitching Spirit and enemy Disarming Rake returned); token ezreal_sb__reksai_sb__s20262164 D:305 | FULL | ok; K1 |
| Daughter of the Void | 294 | Legend (kaisa) | - | kaisa__khazix__s20263024__ismcts-sims20__random D:012, D:205 (reaction above Hextech Ray) | D:014, D:207; spent on a spell kaisa__rengar__s20263025 D:202 (Flow replay) | PART | K2; D50 (R5-10) (CODE-ONLY). Not exercised: its Power paying a Deflect or a Repeat (0 of 704 activations) |
| Brynhir Thundersong | 26 | Unit (kaisa) | akali_sb__kaisa_sb__s20262106__random__random D:054 | D:060 | D:065 | FULL | ok; K1. 66 resolutions: no Play / Hide option offered to the opponent for the rest of the turn |
| Lecturing Yordle | 87 | Unit (kaisa) | kaisa__khazix__s20263024 | kaisa__khazix__s20263024 D:071 (draw) | D:071; Tank: kaisa__zed__s20261225 D:379.11 | FULL | ok (27 combats checked: lethal to it before any other defender) |
| Watchful Sentry | 96 | Unit (kaisa) | kaisa__khazix__s20263024 D:015 | Deathknell kaisa__khazix__s20263024 D:063 | D:063; doubled by Karthus ezreal__leblanc__s20261162 D:233, D:235 | FULL | ok |
| Plundering Poro | 392 | Unit (kaisa) | kaisa__khazix__s20263024 D:218 | D:285.06 (conquered with it) ; no trigger when another unit conquers D:222 | kaisa_sb__ezreal_sb__s20262211 D:289 | FULL | ok |
| Temporal Breach | 853 | Spell (kaisa) | kaisa__azir__s20261209__random__random D:044; hide + from facedown 177 offers | - | D:049; kaisa_sb__zed_sb__s20262225 D:101 | FULL | D47 (R5-7), D48 (R5-8), D25 (R5-5), D5 (R5-2) |
| Hextech Ray | 9 | Spell (kaisa) | kaisa__azir__s20261209 D:243; in showdown 45 times | - | kaisa__khazix__s20263024 D:209 | FULL | ok (392 options: target always at a battlefield; +1 at Void Gate 17 times) |
| Time Warp | 122 | Spell (kaisa) | kaisa__azir__s20261209 D:225 (10E + 4 Mind power) | - | D:240, extra turn D:242.04, banished | FULL | D5 (R5-2) |
| Progress Day | 114 | Spell (kaisa) | kaisa__zed__s20261225 | - | kaisa__zed__s20261225 D:376 (draw 4) | FULL | D5 (R5-2) |
| Rocket Barrage | 400 | Spell (kaisa) | kaisa__khazix__s20263024 D:225 (played in a combat) | Repeat teemo_sb__kaisa_sb__s20262322__random__random D:330 | D:334 (twice); kill gear 5 times | FULL | D5 (R5-2); Repeat asked after the base cost (see D45 (R5-4), also seen) |
| Perfect Execution | 799 | Spell (kaisa) | kaisa__rengar__s20263025 D:201 (Flow, from trash) | - | D:204 (readied, Assault 3, banished) | FULL | D5 (R5-2) |
| Rockfall Path | 530 | Battlefield (kaisa) | - | - | 83 games: 0 "Play <unit> -> Rockfall Path" options (cards, hidden units, Ambush); moves there are offered | FULL | D47 (R5-7) |
| Decree of Rage | 802 | Spell (kaisa sb) | kaisa_sb__akali_sb__s20262208__random__random D:051; Deflect kaisa_sb__irelia_sb__s20262213 D:148-150 | - | D:056 (4 to an enemy Calm unit) | PART | ok. Not exercised: "This can't be countered" (no counter was ever played at it; 5 plays) |
| Sprite Burst | 631 | Spell (kaisa sb) | azir_sb__kaisa_sb__s20262124__random__random | - | D:306-308 (two ready tokens, location asked for each); Temporary D:352.07 | FULL | D5 (R5-2) |

### reader 6: khazix, leblanc

| card | id | type | played (game, D) | ability triggered / activated (game, D) | resolved (game, D) | read | result |
|---|---|---|---|---|---|---|---|
| Voidreaver | 787 | Legend | - | win-combat trigger: akali__khazix__s20261107__random__random D:103.06 (attacker), leblanc_sb__khazix_sb__s20262251__random__random D:256.03 (defender, attacker bounced), kaisa__khazix__s20261215__random__random D:160.04 (enemy moved out); buff: akali__khazix__s20261107__random__random D:109; move: diana__khazix__s20263009__ismcts-sims20__random D:256, ezreal_sb__khazix_sb__s20262161__random__random D:256 | D:105.03 (+1 XP), D:112.03 (Buff), D:259.03 (move blocked by Vex) | FULL | ok (all 136 triggers follow a COMBAT, none a non-combat showdown, none a tie / no-result: script) |
| Kinkou Initiate | 659 | Unit | akali__khazix__s20261107__random__random D:018 | play trigger, 109 resolutions (tally: both ">= 5 -> draw 1" and "< 5 -> no draw") | D:018.03 | FULL | ok; K1 (no outcome change: the count is taken at resolution) |
| Irresistible Faefolk | 674 | Unit | khazix__akali__s20263002__random__ismcts-sims20 D:084 (on board) | akali__khazix__s20261107__random__random D:257.04 (may + target at finalization) | D:261.03 pulled Astral Heron, combat follows | FULL | D4 (R6-4) (declined trigger keeps its chain window) |
| Void Assault | 758 | Spell | akali__khazix__s20261107__random__random D:019; kaisa__khazix__s20261215__random__random D:151.07 | - | D:024-025, kaisa D:148-149 and D:155-156 | FULL | K3 (destinations after RESOLVE); attacker/defender ok in all 19 combats it caused |
| Shadow Order Disciple | 882 | Unit | (in hand / on board many) | akali__khazix__s20261107__random__random D:216.06 (Burn paid at finalization), D:240.03 declined | D:219.03 +1 [M] | FULL | D4 (R6-4) |
| Sabotage | 156 | Spell | akali__yi_rebuild__s20261116__random__random D:124.03 | - | D:129.03 recycling; akali__khazix__s20261107__random__random D:256 (no non-unit card: skipped) | FULL | ok (147 recycles, none a unit: script) |
| Tornado Warrior | 886 | Unit | from facedown: akali_sb__vex_sb__s20262113__random__random D:062; from hand: azir_sb__khazix_sb__s20262125__random__random D:220 | D:062.03 | D:064.03-066.05 (may + target after RESOLVE), disempower D:078.04 | FULL | D54 (R6-6), D51 (R6-1) (trigger on the chain for a hand play), K1 |
| Evelynn, Entrancing | 703 | Unit | from hand: azir_sb__khazix_sb__s20262125__random__random D:007; from facedown on own turn: diana_sb__khazix_sb__s20262143__random__random D:057 | D:057.03 | D:082.02 (she had been bounced: nothing) | PART | the pull ("move an enemy unit ... to my battlefield") was never prompted in any trace (36 triggers, 0 prompts); D51 (R6-1), D54 (R6-6) (code-only for her) |
| Zed, Without a Sound | 899 | Unit | khazix__leblanc__s20261234__random__random (hand) | conquer token: 41 (tally); swap: kaisa__zed__s20261225__random__random D:196 (same location: nothing), diana__zed__s20261153__random__random D:204.04 (in a combat showdown); Shadow Clone attack: khazix_sb__rengar_sb__s20262237__random__random D:272.05 | D:206.03 swapped; D:276.03-04 banish + [Assault 4] -> 4M | FULL | ok |
| Zaun Warrens | 293 | Battlefield | - | akali__khazix__s20261107__random__random D:105.07 | D:107-108 discard then draw; empty hand: diana__zed__s20261153__random__random D:219.02 (draw only) | FULL | ok |
| Star Spring | 771 | Battlefield | - | akali__yi_rebuild__s20261116__random__random D:117.03; irelia__khazix__s20261197__random__random D:082.03 and D:099.03 | D:121.03 moved Lonely Poro | FULL | D52 (R6-2), D4 (R6-4), K1 |
| Sandswept Tomb | 951 | Battlefield | - | passive: kaisa__khazix__s20261215__random__random D:151.08 (Void Assault P=0), diana_sb__khazix_sb__s20262143__random__random D:069.06 (Star-Crossed P=0 + Deflect), leblanc_sb__khazix_sb__s20262251__random__random D:133.02 | - | FULL | ok (48 games; every discounted spell read chose a friendly unit there; once per spell; Power only) |
| Decree of Strength | 872 | Spell | azir_sb__khazix_sb__s20262125__random__random D:202.05 | - | D:205.03-05 (reveal, no Mind card); diana_sb__rengar_sb__s20262147__random__random D:041.07-042.03 | PART | the recycle line itself not read in a window; offered lists = exactly the Mind cards revealed (script, 18 prompts) |
| Deceiver | 756 (legend of leblanc) | Legend | - | conquer: azir__leblanc__s20263035__random__ismcts-sims20 D:039.05; hold: D:090.07; reflexive: D:043.05 | D:043.03 token, D:046.03 copy, Temporary kill D:078.07 | FULL | D53 (R6-3), D4 (R6-4) |
| LeBlanc, Fragmented | 734 | Unit | azir__leblanc__s20263035__random__ismcts-sims20 D:047.03 | Deathknell: ezreal__leblanc__s20263013__ismcts-sims20__random D:055.04; khazix__leblanc__s20263028__ismcts-sims20__random D:210.14 | D:057.03 (draw 1, opponent's Beginning Phase); akali__leblanc__s20261108__random__random D:037.02 (draw 2); Assault: D:210.07 (4M attacking) | FULL | ok; D56 (R6-8) (code-only) |
| Cull the Weak | 209 | Spell | ezreal__leblanc__s20263013__ismcts-sims20__random D:159.03 | - | D:164-166 (each player picks, both die together); no units: ezreal__reksai__s20263043__random__ismcts-sims20 D:014.02 | FULL | ok |
| Soaring Scout | 216 | Unit | (many) | khazix__leblanc__s20263028__ismcts-sims20__random D:041.11 | D:043.03-04 channel 1 exhausted | FULL | ok |
| Glasc Mixologist | 486 | Unit | (many) | leblanc_sb__akali_sb__s20262244__random__random D:133.10 (as a Temporary Reflection copy) | D:137-138 (unit from trash, location picked after RESOLVE) | FULL | ok; see CHECKED (play location at resolution) |
| Ruined Rex | 629 | Unit | (many) | akali_sb__leblanc_sb__s20262108__random__random D:368.11-12 (twice, Karthus) | targets at finalization D:368.17, D:369.05 | PART | the "deal 4" resolution line was not read in a window |
| Mirror Image | 757 | Spell | akali_sb__leblanc_sb__s20262108__random__random D:010.03 | reflexive D:017.04 | D:017.03 token to base, D:019.03 copy | FULL | ok |
| Honest Broker | 476 | Unit | (many) | azir__leblanc__s20263035__random__ismcts-sims20 D:075.15, D:078.09 (Temporary copy) | D:077.03, D:090.03 Gold token | FULL | ok; K2 (the Gold it makes: see CHECKED) |
| Chakram Dancer | 633 | Unit | akali_sb__leblanc_sb__s20262108__random__random D:162 | D:162.02 | D:164.03-06 Shield to 3 other units here | PART | an [Ambush] reaction play of hers was not read; K1 |
| Black Rose Dignitary | 714 | Unit | (many) | leblanc__azir__s20261245__random__random D:568.29 (twice, Karthus) | tally only | PART | [Assault] not read in a damage step; Deathknell read by tally only |
| Ki Barrier | 913 | Spell | azir_sb__leblanc_sb__s20262126__random__random D:072.03; khazix__leblanc__s20263028__ismcts-sims20__random D:208 | - | D:078.03; prevention at D:227.08 ("assigned 2 -> dealt 0") | PART | "opponents can assign it extra combat damage" never chosen by an agent |
| Karthus, Eternal | 236 | Unit | azir_sb__leblanc_sb__s20262126__random__random D:081.03 | passive: akali_sb__leblanc_sb__s20262108__random__random D:368.13; khazix__leblanc__s20263028__ismcts-sims20__random D:210.16 | dies together with the Deathknell unit and still doubles: leblanc__azir__s20261245__random__random D:568.02 | FULL | ok (CR 383.2.c.3) |
| Vi, Peacekeeper | 738 | Unit | Ambush: azir__leblanc__s20263035__random__ismcts-sims20 D:079.03; ezreal_sb__leblanc_sb__s20262162__random__random D:237.03 | ezreal_sb__leblanc_sb__s20262162__random__random D:462.05 (target at finalization, Deflect paid) | D:465.03 stun; stunned unit deals no damage D:465.09 | FULL | D68 (R6-5) |
| Kennen, Keeper of Balance | 922 | Unit | akali_sb__leblanc_sb__s20262108__random__random D:404.03; hidden diana__leblanc__s20261144__random__random D:128, played from facedown D:140 | D:407.02 (play), azir_sb__leblanc_sb__s20262126__random__random D:521.05 (attack) | D:411.03, D:525.03 | PART | "+2 [M] while there's a stunned enemy unit here" not read in a damage step; K1, D4 (R6-4) |
| Windswept Hillock | 292 | Battlefield | - | passive (move options): khazix__leblanc__s20261234__random__random idx=62 | - | FULL | ok (138 option sets with BF->BF moves from the Hillock, none from the other battlefield for a unit without Ganking: script over the replay texts) |
| Dusk Rose Lab | 765 | Battlefield | - | akali__leblanc__s20261108__random__random D:077.07; akali__viktor__s20261114__random__random D:352.07 | D:080.03, D:355.03 | FULL | D53 (R6-3), D4 (R6-4) |
| LeBlanc, Everywhere at Once | 652 | Unit | fiora_sb__leblanc_sb__s20262180__random__random D:283 | yasuao_sb__leblanc_sb__s20262378__random__random D:273.07-09 | - | PART | D55 (R6-7); [Backline] not read against a mixed defender line |

(Deceiver's card id was not looked up; the class is `src/cards/legends/*deceiver.cpp`.)

### reader 7: mel, reksai, rengar

| card | id | type | played (game, D) | ability triggered / activated (game, D) | resolved (game, D) | read | result |
|---|---|---|---|---|---|---|---|
| Soul's Reflection | 938 | Legend | - | trigger akali__mel__s20261109__random__random D:069.04; activate mel_sb__azir_sb__s20262263__random__random D:136 | D:078.03 (empower); D:139.03 (-3 with Mel) | FULL | ok (cost paid before the target prompt, see CHECKED) |
| Mel, Newly Awakened | 856 | Unit/Champion | mel__reksai__s20263036__ismcts-sims20__random D:014.03 | play trigger mel__kaisa__s20261268__random__random D:061; Empower diana_sb__mel_sb__s20262145__random__random D:180 | D:061.03 draw 1; D:182.03 empower; -M: mel_sb__kaisa_sb__s20262268__random__random D:081, viktor_sb__mel_sb__s20262361__random__random D:118 | PART (an opponent's counter against her controller's spell while Empowered: no trace; "-[M] to a unit it does not choose": no trace) | K1; D65 (R7-11) (CODE-ONLY); Q3 |
| Dredge Up | 836 | Spell | mel__diana__s20263010__random__ismcts-sims20 D:168.04; Flow akali__mel__s20261109__random__random D:126 | - | D:172.02; D:128 (Flow, then banished) | FULL | ok |
| Applied Researchers | 842 | Unit | leblanc__mel__s20263032__ismcts-sims20__random D:059.03 | Empower akali__mel__s20261109__random__random D:067 | D:069.03; discount seen akali__mel D:126.04, leblanc__mel__s20263032 D:227.02 | FULL | ok |
| Rebuttal | 939 | Spell | mel__azir__s20261263__random__random D:079; diana__mel__s20263039__random__ismcts-sims20 D:201 | - | D:083-087; D:205-208; mel_sb__azir_sb__s20262263__random__random D:151-152 | FULL | D21 (R7-8) |
| Shock Blast | 846 | Spell | leblanc__mel__s20263032__ismcts-sims20__random D:226 | - | mel__diana__s20263010__random__ismcts-sims20 D:265.02; aggregate 55 "deal 4", 2 mistargets | FULL | ok |
| Unchecked Power | 123 | Spell | irelia_sb__mel_sb__s20262199__random__random D:220.03 | - | D:231.02; ezreal_sb__mel_sb__s20262163__random__random D:245.02 | FULL | D44 (R7-9) |
| Void Burrower | 553 | Legend | - | ezreal__reksai__s20263043__random__ismcts-sims20 D:094.10; reksai__azir__s20261281__random__random D:183 | D:097; D:184-187 (unit, location prompt, cost paid) | FULL | D57 (R7-1), D58 (R7-2), Q1 |
| Rek'Sai, Breacher | 352 | Unit/Champion | irelia__reksai__s20263021__ismcts-sims20__random D:133 (Accelerate paid D:137) | - | attacks at 4M: ezreal_sb__reksai_sb__s20262164__random__random D:232.08 | PART (a friendly unit played from the trash or the Champion Zone while she is on the board: the list has no such play) | K1/K4; D58 (R7-2), D59 (R7-3); Q2 |
| Cleave | 4 | Spell | ezreal__reksai__s20263043__random__ismcts-sims20 (D:085) | - | D:085.03 [Assault 3]; aggregate 71 of 78 resolutions with the effect | FULL | D57 (R7-1) |
| Faithful Manufactor | 211 | Unit | irelia__reksai__s20263021__ismcts-sims20__random D:074.03 | D:077.02 | D:079.03 | PART ("here": the token's location was not compared with the Manufactor's on a board) | K1 |
| Blood Rush | 325 | Spell | reksai__ezreal__s20263014__random__ismcts-sims20 D:063; D:071 (Repeat paid) | - | D:067; D:076 (two executions, two targets) | FULL | K4 (Repeat asked after the base cost); D57 (R7-1) (same class, code) |
| Void Rush | 505 | Spell | mel__reksai__s20263036__ismcts-sims20__random D:162.03 | - | D:167-169; ezreal__reksai__s20263043__random__ismcts-sims20 D:126-127 (declined, draw 2) | FULL | D57 (R7-1) |
| Inferna | 560 | Unit | irelia__reksai__s20263021__ismcts-sims20__random D:115.04; Ambush akali__rengar__s20261111__random__random D:102.04 | - | attacks at 3M akali__rengar__s20261111 D:115.05 | FULL | D61 (R7-5) |
| Carrion Dredger | 715 | Unit | mel__reksai__s20263036__ismcts-sims20__random D:066.03 | reksai__zed__s20261297__random__random D:124.12 | D:126.03 (Bird 1M; Deflect on board diana__reksai__s20261146 D:222) | PART ("to your base": the token's location at creation not compared on a board) | D59 (R7-3) (its copy was the stale-flag instance) |
| Noxian Emissary | 915 | Unit | irelia__reksai__s20263021__ismcts-sims20__random D:009.03 | Empower reksai__rengar__s20261291__random__random D:055; death diana__reksai__s20261146__random__random D:214 | D:057.03; D:216 (two Recruits); 47 death triggers, 24 with tokens | FULL | Q4 |
| Imperial Decree | 221 | Spell | akali_sb__reksai_sb__s20262110__random__random D:020.03 | delayed: akali__viktor__s20261114__random__random D:347.08 | D:346.02; D:349-351 | FULL | ok |
| Undertitan | 495 | Unit | akali__reksai__s20261110__random__random D:170.03 | reveal: reksai__rengar__s20261291__random__random D:050.03 | D:053.03 (+2 Energy); play half D:179.09 (not read line by line) | PART ("give your other units +2 [M]": only the card's own trace line seen, the unit list not checked) | K1; K2 / D64 (R7-10) |
| Shadow Fiend | 801 | Unit | azir_sb__reksai_sb__s20262128__random__random D:140.03 | Empower azir__reksai__s20261128__random__random D:397 (16 payments, all [2]+1 power) | attacks at 5M kaisa__reksai__s20261218__random__random D:354.07 | FULL | ok |
| Forbidding Waste | 766 | Battlefield | - | - | kaisa_sb__reksai_sb__s20262218__random__random D:141 (applied), D:232 / D:234 (not applied) | FULL | D60 (R7-4) |
| Smite | 568 | Spell | (normal plays: aggregate 18 "deal 3") | - | reksai_sb__akali_sb__s20262280__random__random D:219 (banished instead) | PART (a unit that survives the 3 and dies later the same turn: not looked for) | D57 (R7-1) |
| Brittle Steel | 790 | Spell | akali_sb__rengar_sb__s20262111__random__random D:133; Flow leblanc_sb__rengar_sb__s20262255__random__random D:420 | - | aggregate 25 kills, 10 unattach first, 2 no target | FULL | ok |
| Pridestalker | 744 | Legend | - | azir_sb__rengar_sb__s20262129__random__random D:054.04; token ezreal_sb__rengar_sb__s20262165__random__random D:029.04 | D:057; rengar_sb__reksai_sb__s20262308__random__random D:173 (enemy unit) | FULL | K1 |
| Pit Rookie | 136 | Unit | kaisa__rengar__s20263025__ismcts-sims20__random D:023.03 | rengar_sb__teemo_sb__s20262310__random__random D:124.05 (list without itself) | rengar__fiora__s20263018__random__ismcts-sims20 D:105 | PART (a target that already has a buff: not looked for) | K1 |
| Grim Apothecary | 583 | Unit | rengar__fiora__s20263018__random__ismcts-sims20 D:024.03; Ambush akali__rengar__s20261111__random__random D:025 | D:060 (may + unit at finalization) | D:065.03 | FULL | K1 |
| First Mate | 132 | Unit | reksai__rengar__s20263040__ismcts-sims20__random D:148.03 | rengar__akali__s20263003__random__ismcts-sims20 D:104 | D:109.02 (Inferna exh -> rdy, replay idx 114 -> 115) | FULL | K1 |
| Thrill of the Hunt | 745 | Spell | rengar_sb__reksai_sb__s20262308__random__random D:162.06 | - | D:168-169; kaisa_sb__rengar_sb__s20262219__random__random D:039-040 | FULL | D62 (R7-6) |
| Pakaa Cub | 135 | Unit | reksai__rengar__s20263040__ismcts-sims20__random D:083.03; hide azir__rengar__s20261129__random__random D:069/070 | - | played from facedown azir__rengar__s20261129 D:074 | FULL | ok |
| Nidalee, Cat Form | 676 | Unit | reksai__rengar__s20263040__ismcts-sims20__random D:006.03; Ambush fiora__rengar__s20261183__random__random D:101.03 | D:176.05 | D:178.03 | FULL | ok |
| Pyke, Dockside Butcher | 590 | Unit | azir_sb__rengar_sb__s20262129__random__random D:050 (paid); akali__rengar__s20261111__random__random D:125 (declined) | D:054.03 | D:059 (+2, ready, moves D:060) | PART (played from facedown with / without the [R]; a [Ganking] battlefield-to-battlefield move: not looked for) | K1; K4-like; Q4 |
| Emperor's Dais | 521 | Battlefield | - | ezreal_sb__rengar_sb__s20262165__random__random D:025.07 | D:026-029 | FULL | ok |
| Noxian Demolitionist | 867 | Unit | akali_sb__rengar_sb__s20262111__random__random D:163.03 | rengar_sb__azir_sb__s20262299__random__random D:113.07 (may offered, declined); 3 x not offered | D:118.02 | PART (the kill and its gear list: never accepted in any trace) | ok |

### reader 8: teemo, vex, viktor

| card | id | type | played (game, D) | ability triggered / activated (game, D) | resolved (game, D) | read | result |
|---|---|---|---|---|---|---|---|
| Swift Scout | 259 | Legend | - | activate: akali__teemo__s20261112__random__random D:103 (board Teemo), akali_sb__teemo_sb__s20262112__random__random D:050 (Champion Zone); hide for [1]: teemo__akali__s20261316__random__random D:100; tally 265 activations all own turn / open, 67 hides paid [1], 64 paid [A] | D:105-106, D:052-053 | FULL | D67 (R8-2) (unit chosen after RESOLVE; activated 109 times with no Teemo unit) |
| Teemo, Strategist | 121 | Unit/Champion | from hand E=2 P=1 (158 tally); from facedown teemo__akali__s20261316__random__random D:095, teemo__viktor__s20261330__random__random D:146 | defend trigger teemo__vex__s20261329__random__random D:045-047 (Deflect paid for Steel Paws), D:067-069 | D:060 (5 revealed, 4 Hidden, 4 damage, kill), D:071 | FULL | D68 (R8-3) (no defend trigger when he enters a combat already under way); D71 (R8-6) touches (reveals Nocturne) |
| Consult the Past | 83 | Spell | E=4 (59 tally); as a reaction teemo__vex__s20261329__random__random D:048 (countered by Defy D:058) | - | 61 resolutions, each "draws 2" (tally) | PART | ok — a play from facedown of this card was not singled out and read (the facedown path was read on Teemo, Sprite Call, Windsinger, Facebreaker) |
| Sprite Call | 94 | Spell | E=3 (52 tally); from facedown ezreal__teemo__s20261166__random__random D:386, teemo__viktor__s20261330__random__random D:155 | - | D:388-389, D:157-158; ready 3M Temporary Sprite killed at next Beginning Step teemo__akali__s20261316__random__random D:098.07 | FULL | D69 (R8-4) (from facedown the Sprite may go to base / another battlefield) |
| Sneaky Deckhand | 176 | Unit | E=3 (65 tally); option "-> Grove of the God-Willow [Ctrl:--]" offered leblanc_sb__teemo_sb__s20262256__random__random D:595 (decision #642) while Baron Nashor in the same list was not | - | - | PART | ok — the option was read; a play that actually went to an open battlefield was not read |
| Guerilla Warfare | 260 | Spell/Signature | E=2 P=1 (36 tally) azir__teemo__s20261130__random__random D:089 | - | D:094-096 (two cards, one of them the other copy of Guerilla Warfare id=76, the resolving one is id=77), diana__teemo__s20261148__random__random D:244-246; "Return no more" offered first; hides after it are free: 15 of 15 by the same player (tally), azir__teemo D:102 | FULL | D67 (R8-2) (the cards are chosen after RESOLVE) |
| Windsinger | 459 | Unit | E=2 (60 tally); from facedown teemo_sb__ezreal_sb__s20262319__random__random D:270, khazix__teemo__s20263029__ismcts-sims20__random D:296 | diana__teemo__s20261148__random__random D:259-261 (may, then target, both at finalization; own units offered; itself not offered) | D:263; ezreal__teemo__s20261166__random__random D:066 | FULL | D70 (R8-5) (from facedown a unit at another battlefield is offered); K1 (board shows her ready/at location during the prompts; no outcome change seen); D4 (R8-8) (declined may stays on chain) |
| Invert Timelines | 201 | Spell | E=3 P=1 (26 tally) | - | rengar__teemo__s20263044__ismcts-sims20__random D:052 (both hands discarded, each draws 4) | FULL | ok |
| Downwell | 468 | Spell | E=8 P=2 (11 tally) | - | diana__teemo__s20261148__random__random D:109 (2 units returned) | PART | ok — the instance read had no gear and no enemy unit on the board |
| Existential Dread | 696 | Spell | E=1 P=1, target at play azir_sb__teemo_sb__s20262130__random__random D:199-200; Repeat [2] paid teemo_sb__rengar_sb__s20262327__random__random D:126-127 (repeat's target at finalization) | - | D:204 (stunned attacker deals 0); D:129 (stun, then already stunned -> hand); countered teemo_sb__fiora_sb__s20262320__random__random D:357 | FULL | ok |
| Baron Nashor | 709 | Unit | E=10 P=3 kaisa_sb__teemo_sb__s20262220__random__random D:148; Pit created D:161, he enters it, conquer D:165; replayed by Temporal Breach with the Pit present D:241 ("staying at chosen location") | - | +2 aura: Nocturne 6M in DAMAGE_STEP D:190; hit by a mass -3 (not chosen) D:178; no enemy prompt in any teemo game lists him (tally) | FULL | ok; see QUESTIONS 2 |
| Nocturne, Horrifying | 194 | Unit/Champion | E=4 P=1 (73 tally) | "trigger" 20 times, all from Teemo, Strategist's reveal: teemo__vex__s20261329__random__random D:060-064 | banished and played for [A] to battlefield #1 D:064 (11 played, 3 banished only, 6 declined: tally) | PART | D71 (R8-6); [Ganking] move battlefield-to-battlefield not read |
| Teemo, Scout | 197 | Unit/Champion | E=2 (85 tally); hidden kaisa_sb__teemo_sb__s20262220__random__random D:294 | 106 triggers | 105 "+3M this turn" (102 -> 4M, 3 -> 6M) | FULL | ok; K1 applies to the trigger's timing |
| Grove of the God-Willow | 275 | Battlefield | - | hold trigger 76 | 69 resolutions, each "draws 1" (tally) | FULL | D72 (R8-7) |
| Startipped Peak | 283 | Battlefield | - | akali__teemo__s20261112__random__random D:079-080 (declined), D:110 | 69 accepted: each channels 1 rune exhausted (66 CHANNELED exhausted lines read by tally) | FULL | D72 (R8-7), D4 (R8-8) |
| Gloomist | 785 | Legend | - | 319 fires; kaisa__vex__s20263055__random__ismcts-sims20 D:327-330 (accepted, exhaust paid at finalization) and D:330.10 (second hold of the turn: MAY_NOT_OFFERED, legend already exhausted) | 146 draws | FULL | D72 (R8-7), D4 (R8-8) |
| Mutated Mouser | 598 | Unit | E=2 (69), E=0 with a discount (5) | - | Shield: 3M as sole defender (18), 1M as attacker without gear (13), 3M as attacker only with Boots (read 5); Tank: vex__azir__s20261335__random__random D:327 damage assigned to it first | FULL | ok |
| Steel Paws | 830 | Unit | E=1 (78) | Empower: 24 activations, each pays [7], own turn / open; vex__azir__s20261335__random__random D:200-202 | 7M after empower (rengar__vex__s20261311__random__random D:175 DAMAGE_STEP); Deflect paid by Teemo, Strategist's controller teemo__vex__s20261329__random__random D:047 | FULL | ok |
| Kharox | 901 | Unit | E=6 (44), E=4 discounted (4) | Empower [6]+2 Power 19 times; trigger akali_sb__vex_sb__s20262113__random__random D:257 | burn 3, reflexive item, may + unit (the just-burned unit is in the list) + location at its finalization D:257-260, played D:262 | FULL | ok |
| Abandon | 693 | Spell | E=2 (19), E=0 discounted (3); kaisa__vex__s20263055__random__ismcts-sims20 D:110 | - | D:114-115: counter -> owner's hand, Predict (12 recycled, 7 kept: tally) | FULL | D11 (R8-9) (Predict card named in a public line); D21 (R8-13) (CODE-ONLY) |
| Bandle Tree | 273 | Battlefield | - | - | two facedown cards akali__vex__s20261113__random__random D:052 board; tally over every board of 720 games: more than one facedown card at a battlefield 105 times, all Bandle Tree, never more than 2 | FULL | ok |
| Fortified Position | 274 | Battlefield | - | teemo__vex__s20261329__random__random D:045-046 (prompt lists units of both players, anywhere), rengar__vex__s20261311__random__random D:172 (the defender who is not the Vex player gets it) | D:066 "give Scuttle Crab [Shield] (value=2) this turn" | FULL | D73 (R8-10) |
| Tomb-Raider Barbara | 824 | Unit | E=4 (46) | 41 triggers of 47 plays (condition 7 runes), 29 with no enemy gear, 7 prompts | 3 kills (Boots of Swiftness) | PART | D4 (R8-8) (29 items with nothing to choose stay on the chain); the [Empowered] gear branch was never exercised |
| Herald of the Arcane | 298 | Legend | - | 256 activations, all own turn / open; akali__viktor__s20261114__random__random D:141 | D:143 | FULL | D66 (R8-1) |
| Viktor, Leader | 246 | Unit/Champion | E=4 P=1 (71) | 101 fires; deaths before them tallied: Sprite, Bird, Carrion Dredger, Honest Broker, Xin Zhao ...; never a Recruit alone | 98; the 3 others: Viktor died in the same batch and the ability was taken back, viktor__rengar__s20261363__random__random D:278.13-16, yi_rebuild_sb__viktor_sb__s20262402__random__random D:090.24-26 | FULL | ok |
| Keeper of Law | 906 | Unit | 61 plays tallied against the board: E=3 P=0 exactly when a battlefield the player controls holds exactly two units (8), E=5 P=1 otherwise (53); akali__viktor__s20261114__random__random decision #150 | - | - | FULL | ok |
| Facebreaker | 220 | Spell | from hand E=2 (12); hidden 47; from facedown viktor__kaisa__s20261358__random__random D:061, rengar__viktor__s20261312__random__random D:066 | - | D:065 (both stunned; stunned attacker dealt 0 at D:067) | FULL | D74 (R8-11) |
| Dragon Form | 903 | Spell | E=3 (56); Flow [3] from trash 37 times, akali__viktor__s20261114__random__random D:147-148 (then in Banish, board at decision #166) | - | D:150; with a -1 on the unit: 4M viktor__ezreal__s20261355__random__random D:431; Deflect paid viktor__diana__s20261354__random__random D:116 | FULL | ok |
| Blood Money | 483 | Spell | E=2 (27) | - | 26: enemy -> 1 Gold (11), friendly -> 2 Gold (15); every Gold on the next board is exhausted (26 of 26) | FULL | ok |
| Xin Zhao, Vigilant | 496 | Unit/Champion | 70 plays tallied against the board: 0 other units in base -> exhausted (19), 1 -> exhausted (9; zed_sb__viktor_sb__s20262420__random__random decision #18: the one other unit is another Xin Zhao), 2+ -> ready (22 in base; also ready when he himself is played to a battlefield: teemo__viktor__s20261330__random__random decision #112, viktor__akali__s20261352__random__random decision #46) | - | Tank: viktor games D:344.08, D:077.06 damage assigned to him first | FULL | ok |

### reader 9: yasuao, yi_rebuild, zed

| card | id | type | played (game, D) | ability triggered / activated (game, D) | resolved (game, D) | read | result |
|---|---|---|---|---|---|---|---|
| Unforgiven | 256 | Legend | - | all 245 activations classified. From base to an empty battlefield: fiora__yasuao__s20263062__random__ismcts-sims20 D:066, leblanc__yasuao__s20263063__random__ismcts-sims20 D:038; from a battlefield to base: akali__yasuao__s20261115__random__random D:076; from base to an enemy-held battlefield (combat): same game D:266; countered / responded to: irelia_sb__yasuao_sb__s20262205__random__random D:071 | D:068-071; D:040-041; D:078; D:268-271 | FULL | cost [2] + exhaust, unit chosen at activation, never battlefield-to-battlefield, never offered at a Pass Focus decision (0 of the showdown / combat decisions of all yasuao games): ok. K3 (destination picked after `RESOLVE:`). |
| Yasuo, Windrider | 205 | Unit (champion) | kaisa__yasuao__s20261223__random__random D:117 (E=5 P=1) | 142 triggers in 111 player-turns (87 turns x1, 21 x2, 3 x3). Third move: kaisa__yasuao__s20261223 D:160/169/173; yasuao__irelia__s20261375__random__random D:095/136/145; yasuao__yasuao__s20261385__random__random D:084/095/098 | point scored at D:175, D:147, D:117 ("SCORE ... via effect") | PART: a 4th move in a turn, the point as the 8th point, and [Ganking] battlefield-to-battlefield as a separate check were not exercised | effect moves (Unforgiven, Ride the Wind) and standard moves count, recalls do not (no trigger on any `RECALL:`), exactly on the third: ok. D77 (R9-5) |
| Lonely Poro | 359 | Unit | akali__yasuao__s20261115__random__random D:276 (E=2) | 75 Deathknell triggers; alone: khazix__yi_rebuild__s20261242__random__random D:060; not alone: ezreal_sb__yasuao_sb__s20262169__random__random D:099, diana_sb__yi_rebuild_sb__s20262152__random__random D:259 | 60 "died alone -> draw 1", 15 "did not die alone" | FULL | D80 (R9-10) (question: simultaneous deaths) |
| Wind Wall | 64 | Spell | all 20 plays read (E=3 P=2; the recycled runes are covered by the 9,886-payment Domain scan under RUNES), e.g. azir_sb__yasuao_sb__s20262133__random__random D:023 | - | 17 counters (D:030 Gust, ...); countered itself by Hard Bargain: yasuao_sb__yi_rebuild_sb__s20262386__random__random D:141-152; did nothing: mel_sb__yasuao_sb__s20262277__random__random D:214-221 | FULL | D21 (R9-4) |
| Yasuo, Remorseful | 76 | Unit (champion) | ezreal_sb__yasuao_sb__s20262169__random__random D:180 (E=6 P=2, two Calm runes) | attack trigger, target at finalization, only enemy units "here": same game D:217; yasuao_sb__akali_sb__s20262370__random__random D:279; yasuao_sb__irelia_sb__s20262375__random__random D:151 | D:222 (6 dmg), D:282 (6+1 bonus), D:154 (kill -> no damage step -> conquer) | FULL | ok |
| Wuju Bladesman - Starter | 317 | Legend | - | passive, no trace lines; all 60 combats with exactly one yi-side defender read from DAMAGE_STEP | - | FULL | D76 (R9-2) |
| Master Yi, Tempered | 675 | Unit (champion) | yi_rebuild__irelia__s20263023__random__ismcts-sims20 D:007 (E=4) | Hunt 2: 135 triggers, 83 after "via Hold", 47 after "via Conquer" (yi_rebuild__zed__s20263064__ismcts-sims20__random D:233); Level 6: Deflect charged in 5 traces (diana__yi_rebuild__s20261152__random__random D:183, XP 6 since D:150; teemo__yi_rebuild__s20261332__random__random D:197/198; yi_rebuild__ezreal__s20261391__random__random D:193/248 — XP at that moment checked by hand only in the first); battlefield-to-battlefield move offered in 59 decisions at 6+ XP and in 4 below 6 XP, all 4 at Windswept Hillock (yi_rebuild_sb__leblanc_sb__s20262396__random__random idx 94) | D:235 (+2 XP) | FULL | ok; D79 (R9-7) touches its Deflect |
| Ruin Runner | 427 | Unit | fiora_sb__yi_rebuild_sb__s20262188__random__random D:092 (E=6) | passive: in the 76 games with yi_rebuild in one seat no opponent prompt, play-target or intent target ever lists it (40 prompts of its controller do); enemy effects that reached it are non-choosing (Thousand-Tailed Watcher, Flurry of Blades) | - | FULL (by absence) | ok |
| Onslaught | 868 | Spell | yi_rebuild_sb__zed_sb__s20262405__random__random D:033 (E=4); Flow [4]: teemo__yi_rebuild__s20261332__random__random D:303 | - | D:306 (+6); fiora_sb__yi_rebuild_sb__s20262188 D:129 (Ruin Runner -> 11M) | FULL | target chosen as played ("TARGET (play)"), Flow banish empowers nothing on the yi side: ok. D5 (R9-3) |
| Fiora, Peerless | 432 | Unit (champion) | yi_rebuild_sb__zed_sb__s20262405__random__random D:009 (E=3 P=1) | all 19 triggers read: 5 one on one (akali_sb__yi_rebuild_sb__s20262116__random__random D:126, D:156), 14 not | D:128 (3 -> 6M), D:158 (8M) | FULL | bonus gone after the combat (idx 151): ok. D76 (R9-2) (8M where 10M is due), D77 (R9-5) (same pattern) |
| Alpha Strike | 751 | Spell | yi_rebuild_sb__rengar_sb__s20262399__random__random D:111 (E=3 P=1 Body) | reflexive "gain 1 XP" trigger: D:119 | D:119-121; viktor_sb__yi_rebuild_sb__s20262368__random__random D:229; zed_sb__yi_rebuild_sb__s20262422__random__random D:149 | PART: damage actually divided among two or more units not seen (every traced split put all of it on one unit) | ok |
| Master of Shadows | 930 | Legend | - | 174 triggers, classified by the line that precedes each (Flow spell leaving the chain, Shadow Clone, `EFFECT: banish`, countered Flow spell): no category without a banish; activation 73x own main phase (kaisa__zed__s20263070__random__ismcts-sims20 D:096), offered at 146 Pass Focus decisions and chosen 59x (akali__zed__s20261117__random__random D:038, opponent's turn) | D:025 (empower -> 1); D:098-099 (discard chosen at resolution, then draw) | FULL | D75 (R9-1) |
| Rhasa the Sunderer | 195 | Unit | all 52 plays: Energy paid = max(0, 10 - cards in trash on the board), Power stays 1; akali_sb__zed_sb__s20262117__random__random D:185 (trash 11, E=0, the [P] from Seal of Discord's floating Power), azir__zed__s20261135__random__random D:361 (trash 8, E=2) | - | - | FULL | ok |
| Seal of Discord | 204 | Gear | azir__zed__s20261135__random__random D:075 (P=1, Chaos rune recycled), enters ready | 64 activations: akali__zed__s20261117 D:013, azir__zed__s20261135 D:077 | D:015, D:083 (+1 floating Chaos Power) | FULL | D2 (R9-9) (K2) |
| Last Rites | 471 | Gear | zed__akali__s20263068__ismcts-sims20__random D:006 (E=3) | Equip: kaisa__zed__s20263070 D:114 ([P] + two trash cards recycled); conquer / hold trigger: azir__zed__s20261135 D:184 ("may" and unit at finalization), fiora__zed__s20261189__random__random D:161 (hold, in the Scoring Step) | D:190-195 (Kai'Sa paid E=4), D:161-166 | FULL | Equip Power is Chaos: in ~50 activations compared on the board only a Chaos rune leaves, never a Fury one. Play location picked after `RESOLVE:` (as the class intends, CR 355.2.a): noted, not reported |
| Shadowblade Lurker | 883 | Unit | all 48 plays: E = 5 - 2 x copies in trash; akali_sb__zed_sb__s20262117 D:056 (E=5), azir__zed__s20261135 D:346 (E=3) | - | - | FULL | ok |
| Death Mark | 931 | Spell | akali__zed__s20261117 D:016 (E=2, P from Seal); Flow [1][A][A]: D:021 ("exhausted 1, recycled 2") | - | 130 resolutions: 3 BURNED + 1 Shadow Clone each (D:020, D:023), location prompt when a battlefield is controlled (akali_sb__zed_sb__s20262117 D:104); burn out: vex__zed__s20261351__random__random D:384 | FULL | D5 (R9-3) |
| Kennen, Storm of Shuriken | 900 | Unit | zed__akali__s20263068__ismcts-sims20__random D:016 (E=3 P=1 Chaos) | play trigger D:020; conquer trigger: vex_sb__zed_sb__s20262351__random__random D:164 (24 grants in all) | D:166-167; granted spell replayed: yasuao_sb__zed_sb__s20262387__random__random D:139 (the only use in the traces) | FULL | K1 (no outcome change), D78 (R9-6) |
| Minefield | 526 | Battlefield | - | 49 triggers, each directly after a Conquer there by either player (azir_sb__zed_sb__s20262135__random__random D:072) | D:074 | PART: the trace does not name the two cards; a deck of fewer than 2 cards not exercised | D81 (R9-11) (code-only) |
| Piercing Light | 346 | Spell | fiora_sb__zed_sb__s20262189__random__random D:216 (E=2 P=1 Fury, Repeat paid, Deflect paid once per time Poppy is chosen) | - | D:224 (2+2 twice); Flow via Kennen: yasuao_sb__zed_sb__s20262387 D:139-144 | FULL (the Domain of the Repeat [R] is not readable from `PAY_ADD`) | D5 (R9-3), D44 (R9-8) |
| RUNES | 7/42/89/126/166/214 | Rune | all 720 games | see CHECKED AND CORRECT | - | FULL | ok |
| COMBAT | - | procedure | 2087 DAMAGE_STEPs in 720 games | see CHECKED AND CORRECT | - | FULL | D76 (R9-2), D5 (R9-3) |

### Counts over the 720 read games

From `card_coverage.py`: how often each card was played, hidden, had an ability queued / activated / resolved, and the first sighting of each (game, D). A play from facedown prints no `PLAY` / `SPELL` line, so `played` undercounts cards with [Hidden]; `activated` counts Main Phase activations only (an activation at chain priority or at Focus prints no source: read `resolved` for those); a legend is never played; for a battlefield `played` is the number of games it was the battlefield in play, for a rune the number of times one was channelled.

| card | type | lists | games | played | hidden | triggered | activated | resolved (ability / spell) | first play | first trigger | first activation |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Blade Dancer | Legend | 1 | 78 | 0 | 0 | 552 | 0 | 546 / 0 | — | akali__irelia__s20261105__random__random D:026 | — |
| Daughter of the Void | Legend | 1 | 78 | 0 | 0 | 0 | 136 | 704 / 0 | — | — | akali__kaisa__s20261106__random__random D:056 |
| Deceiver | Legend | 1 | 78 | 0 | 0 | 690 | 0 | 650 / 0 | — | akali__leblanc__s20261108__random__random D:140 | — |
| Emperor of the Sands | Legend | 1 | 78 | 0 | 0 | 0 | 112 | 112 / 0 | — | — | akali__azir__s20261101__random__random D:074 |
| Gloomist | Legend | 1 | 78 | 0 | 0 | 319 | 0 | 279 / 0 | — | akali__vex__s20261113__random__random D:033 | — |
| Grand Duelist | Legend | 1 | 78 | 0 | 0 | 485 | 0 | 483 / 0 | — | akali__fiora__s20261104__random__random D:040 | — |
| Herald of the Arcane | Legend | 1 | 78 | 0 | 0 | 0 | 256 | 256 / 0 | — | — | akali__viktor__s20261114__random__random D:024 |
| Master of Shadows | Legend | 1 | 78 | 0 | 0 | 174 | 73 | 435 / 0 | — | akali__zed__s20261117__random__random D:023 | akali__zed__s20261117__random__random D:068 |
| Pridestalker | Legend | 1 | 78 | 0 | 0 | 874 | 0 | 872 / 0 | — | akali__rengar__s20261111__random__random D:009 | — |
| Prodigal Explorer | Legend | 1 | 78 | 0 | 0 | 0 | 9 | 71 / 0 | — | — | ezreal__fiora__s20261158__random__random D:192 |
| Rogue Assassin | Legend | 1 | 78 | 0 | 0 | 0 | 67 | 256 / 0 | — | — | akali__akali__s20261100__random__random D:041 |
| Scorn of the Moon | Legend | 1 | 78 | 0 | 0 | 0 | 185 | 672 / 0 | — | — | akali__diana__s20261102__random__random D:008 |
| Soul's Reflection | Legend | 1 | 78 | 0 | 0 | 55 | 38 | 93 / 0 | — | akali__mel__s20261109__random__random D:069 | akali__mel__s20261109__random__random D:122 |
| Swift Scout | Legend | 1 | 78 | 0 | 0 | 0 | 265 | 421 / 0 | — | — | akali__teemo__s20261112__random__random D:103 |
| Unforgiven | Legend | 1 | 78 | 0 | 0 | 0 | 245 | 417 / 0 | — | — | akali__yasuao__s20261115__random__random D:035 |
| Void Burrower | Legend | 1 | 78 | 0 | 0 | 242 | 0 | 363 / 0 | — | akali__reksai__s20261110__random__random D:047 | — |
| Voidreaver | Legend | 1 | 78 | 0 | 0 | 136 | 110 | 245 / 0 | — | akali__khazix__s20261107__random__random D:103 | akali__khazix__s20261107__random__random D:109 |
| Wuju Bladesman - Starter | Legend | 1 | 78 | 0 | 0 | 0 | 0 | 0 / 0 | — | — | — |
| Adaptatron | Unit | 1 | 78 | 55 | 0 | 18 | 0 | 16 / 0 | akali_sb__irelia_sb__s20262105__random__random D:020 | diana_sb__irelia_sb__s20262141__random__random D:291 | — |
| Akali, Deadly Weapon | Unit | 1 | 78 | 76 | 0 | 242 | 41 | 283 / 0 | akali__akali__s20261100__random__random D:006 | akali__akali__s20261100__random__random D:021 | akali__akali__s20261100__random__random D:111 |
| Akali, Silent | Unit | 2 | 111 | 68 | 0 | 99 | 0 | 99 / 0 | akali__yasuao__s20261115__random__random D:260 | akali__yasuao__s20261115__random__random D:269 | — |
| Akshan, Mischievous | Unit | 1 | 35 | 17 | 0 | 42 | 0 | 42 / 0 | diana_sb__fiora_sb__s20262140__random__random D:322 | diana_sb__fiora_sb__s20262140__random__random D:327 | — |
| Angler Beast | Unit | 1 | 35 | 18 | 0 | 18 | 0 | 18 / 0 | azir_sb__ezreal_sb__s20262121__random__random D:158 | azir_sb__ezreal_sb__s20262121__random__random D:164 | — |
| Applied Researchers | Unit | 1 | 78 | 80 | 0 | 0 | 30 | 30 / 0 | akali__mel__s20261109__random__random D:038 | — | akali__mel__s20261109__random__random D:067 |
| Ashe, Focused | Unit | 2 | 68 | 48 | 0 | 49 | 0 | 135 / 0 | akali_sb__azir_sb__s20262101__random__random D:125 | akali_sb__azir_sb__s20262101__random__random D:131 | — |
| Astral Heron | Unit | 3 | 220 | 149 | 0 | 396 | 0 | 396 / 0 | akali__akali__s20261100__random__random D:139 | akali__akali__s20261100__random__random D:146 | — |
| Azir, Sovereign | Unit | 1 | 78 | 80 | 0 | 29 | 0 | 47 / 0 | akali__azir__s20261101__random__random D:009 | akali_sb__azir_sb__s20262101__random__random D:317 | — |
| Baron Nashor | Unit | 1 | 78 | 10 | 0 | 0 | 0 | 0 / 0 | kaisa_sb__teemo_sb__s20262220__random__random D:148 | — | — |
| Bewitching Spirit | Unit | 1 | 78 | 101 | 0 | 102 | 0 | 102 / 0 | azir__ezreal__s20261121__random__random D:062 | azir__ezreal__s20261121__random__random D:065 | — |
| Black Rose Dignitary | Unit | 1 | 78 | 47 | 0 | 59 | 0 | 59 / 0 | akali_sb__leblanc_sb__s20262108__random__random D:400 | azir_sb__leblanc_sb__s20262126__random__random D:059 | — |
| Blitzcrank, Impassive | Unit | 1 | 78 | 53 | 0 | 73 | 0 | 70 / 0 | akali__akali__s20261100__random__random D:027 | akali__akali__s20261100__random__random D:033 | — |
| Brynhir Thundersong | Unit | 1 | 78 | 63 | 0 | 66 | 0 | 66 / 0 | akali__kaisa__s20261106__random__random D:274 | akali__kaisa__s20261106__random__random D:280 | — |
| Carrion Dredger | Unit | 2 | 152 | 144 | 0 | 98 | 0 | 98 / 0 | akali__reksai__s20261110__random__random D:090 | akali__reksai__s20261110__random__random D:209 | — |
| Chakram Dancer | Unit | 1 | 78 | 45 | 0 | 47 | 0 | 47 / 0 | akali_sb__leblanc_sb__s20262108__random__random D:159 | akali_sb__leblanc_sb__s20262108__random__random D:162 | — |
| Darius, Trifarian | Unit | 2 | 150 | 47 | 0 | 73 | 0 | 73 / 0 | akali__akali__s20261100__random__random D:261 | akali__akali__s20261100__random__random D:267 | — |
| Diana, Lunari | Unit | 1 | 78 | 97 | 0 | 48 | 0 | 48 / 0 | akali__diana__s20261102__random__random D:092 | akali__diana__s20261102__random__random D:134 | — |
| Disarming Rake | Unit | 3 | 99 | 70 | 0 | 71 | 0 | 71 / 0 | akali_sb__akali_sb__s20262100__random__random D:011 | akali_sb__akali_sb__s20262100__random__random D:015 | — |
| Elder Dragon | Unit | 1 | 78 | 15 | 0 | 40 | 0 | 40 / 0 | diana__fiora__s20261140__random__random D:136 | diana__fiora__s20261140__random__random D:152 | — |
| Evelynn, Entrancing | Unit | 2 | 111 | 27 | 13 | 36 | 0 | 36 / 0 | azir_sb__khazix_sb__s20262125__random__random D:005 | azir_sb__khazix_sb__s20262125__random__random D:007 | — |
| Ezreal, Prodigy | Unit | 1 | 78 | 131 | 0 | 131 | 0 | 262 / 0 | akali__ezreal__s20261103__random__random D:033 | akali__ezreal__s20261103__random__random D:037 | — |
| Faithful Manufactor | Unit | 1 | 78 | 67 | 0 | 71 | 0 | 71 / 0 | akali__reksai__s20261110__random__random D:026 | akali__reksai__s20261110__random__random D:029 | — |
| Ferrous Forerunner | Unit | 4 | 211 | 127 | 0 | 44 | 0 | 44 / 0 | akali__rengar__s20261111__random__random D:204 | akali__rengar__s20261111__random__random D:287 | — |
| Fiora, Peerless | Unit | 1 | 35 | 22 | 0 | 19 | 0 | 19 / 0 | akali_sb__yi_rebuild_sb__s20262116__random__random D:046 | akali_sb__yi_rebuild_sb__s20262116__random__random D:126 | — |
| Fiora, Victorious | Unit | 1 | 35 | 16 | 0 | 0 | 0 | 0 / 0 | azir_sb__fiora_sb__s20262122__random__random D:189 | — | — |
| Fiora, Worthy | Unit | 1 | 78 | 86 | 0 | 211 | 0 | 211 / 0 | akali__fiora__s20261104__random__random D:020 | akali__fiora__s20261104__random__random D:040 | — |
| First Mate | Unit | 2 | 151 | 114 | 0 | 118 | 0 | 118 / 0 | akali__rengar__s20261111__random__random D:071 | akali__rengar__s20261111__random__random D:074 | — |
| Fizz, Trickster | Unit | 6 | 403 | 328 | 0 | 329 | 0 | 329 / 0 | akali__diana__s20261102__random__random D:027 | akali__diana__s20261102__random__random D:031 | — |
| Glasc Mixologist | Unit | 1 | 78 | 74 | 0 | 60 | 0 | 80 / 0 | akali__leblanc__s20261108__random__random D:208 | akali_sb__leblanc_sb__s20262108__random__random D:131 | — |
| Grim Apothecary | Unit | 1 | 78 | 86 | 0 | 96 | 0 | 96 / 0 | akali__rengar__s20261111__random__random D:025 | akali__rengar__s20261111__random__random D:028 | — |
| Harnessed Dragon | Unit | 1 | 78 | 36 | 0 | 50 | 0 | 49 / 0 | azir__fiora__s20261122__random__random D:228 | azir__fiora__s20261122__random__random D:238 | — |
| Honest Broker | Unit | 3 | 222 | 206 | 0 | 182 | 0 | 182 / 0 | akali__viktor__s20261114__random__random D:027 | akali__viktor__s20261114__random__random D:246 | — |
| Hwei, Brooding Painter | Unit | 2 | 150 | 127 | 0 | 364 | 0 | 546 / 0 | akali__mel__s20261109__random__random D:110 | akali__mel__s20261109__random__random D:185 | — |
| Inferna | Unit | 2 | 151 | 179 | 0 | 0 | 0 | 0 / 0 | akali__reksai__s20261110__random__random D:045 | — | — |
| Irelia, Fervent | Unit | 3 | 222 | 160 | 0 | 385 | 0 | 385 / 0 | akali__irelia__s20261105__random__random D:048 | akali__irelia__s20261105__random__random D:089 | — |
| Irresistible Faefolk | Unit | 2 | 152 | 109 | 0 | 71 | 0 | 71 / 0 | akali__khazix__s20261107__random__random D:174 | akali__khazix__s20261107__random__random D:257 | — |
| Kai'Sa, Survivor | Unit | 4 | 282 | 225 | 0 | 124 | 0 | 122 / 0 | akali__akali__s20261100__random__random D:239 | akali__irelia__s20261105__random__random D:208 | — |
| Karthus, Eternal | Unit | 1 | 78 | 76 | 0 | 0 | 0 | 0 / 0 | akali__leblanc__s20261108__random__random D:072 | — | — |
| Kayle, Justified | Unit | 1 | 35 | 43 | 0 | 0 | 43 | 43 / 0 | azir_sb__fiora_sb__s20262122__random__random D:043 | — | azir_sb__fiora_sb__s20262122__random__random D:124 |
| Keeper of Law | Unit | 1 | 78 | 61 | 0 | 0 | 0 | 0 / 0 | akali__viktor__s20261114__random__random D:137 | — | — |
| Kennen, Keeper of Balance | Unit | 1 | 78 | 28 | 13 | 48 | 0 | 48 / 0 | akali_sb__leblanc_sb__s20262108__random__random D:404 | akali_sb__leblanc_sb__s20262108__random__random D:407 | — |
| Kennen, Storm of Shuriken | Unit | 1 | 78 | 66 | 0 | 98 | 0 | 121 / 0 | akali_sb__zed_sb__s20262117__random__random D:027 | akali_sb__zed_sb__s20262117__random__random D:031 | — |
| Kha'Zix, Mutating Horror | Unit | 3 | 142 | 135 | 0 | 121 | 0 | 121 / 0 | akali__khazix__s20261107__random__random D:070 | akali__khazix__s20261107__random__random D:120 | — |
| Kharox | Unit | 1 | 78 | 48 | 0 | 38 | 19 | 57 / 0 | akali__vex__s20261113__random__random D:141 | akali_sb__vex_sb__s20262113__random__random D:255 | akali_sb__vex_sb__s20262113__random__random D:253 |
| Kinkou Initiate | Unit | 2 | 152 | 105 | 0 | 109 | 0 | 109 / 0 | akali__khazix__s20261107__random__random D:013 | akali__khazix__s20261107__random__random D:016 | — |
| LeBlanc, Everywhere at Once | Unit | 1 | 35 | 17 | 0 | 0 | 0 | 0 / 0 | diana_sb__leblanc_sb__s20262144__random__random D:245 | — | — |
| LeBlanc, Fragmented | Unit | 1 | 78 | 85 | 0 | 122 | 0 | 122 / 0 | akali__leblanc__s20261108__random__random D:013 | akali__leblanc__s20261108__random__random D:035 | — |
| Lecturing Yordle | Unit | 1 | 78 | 80 | 0 | 91 | 0 | 91 / 0 | akali_sb__kaisa_sb__s20262106__random__random D:094 | akali_sb__kaisa_sb__s20262106__random__random D:097 | — |
| Lonely Poro | Unit | 2 | 151 | 152 | 0 | 75 | 0 | 75 / 0 | akali__yasuao__s20261115__random__random D:276 | akali__yasuao__s20261115__random__random D:317 | — |
| Master Yi, Tempered | Unit | 1 | 78 | 78 | 0 | 135 | 0 | 123 / 0 | akali__yi_rebuild__s20261116__random__random D:017 | akali__yi_rebuild__s20261116__random__random D:028 | — |
| Mel, Newly Awakened | Unit | 1 | 78 | 85 | 0 | 85 | 26 | 111 / 0 | akali_sb__mel_sb__s20262109__random__random D:162 | akali_sb__mel_sb__s20262109__random__random D:167 | diana_sb__mel_sb__s20262145__random__random D:180 |
| Mischievous Marai | Unit | 2 | 152 | 95 | 34 | 37 | 0 | 37 / 0 | akali__akali__s20261100__random__random D:177 | akali__irelia__s20263001__ismcts-sims20__random D:088 | — |
| Mournful Witness | Unit | 1 | 78 | 64 | 0 | 35 | 0 | 35 / 0 | akali__akali__s20261100__random__random D:124 | akali__akali__s20261100__random__random D:204 | — |
| Mutated Mouser | Unit | 1 | 78 | 74 | 0 | 0 | 0 | 0 / 0 | akali__vex__s20261113__random__random D:016 | — | — |
| Nidalee, Cat Form | Unit | 1 | 78 | 64 | 0 | 35 | 0 | 35 / 0 | akali__rengar__s20261111__random__random D:093 | akali__rengar__s20261111__random__random D:180 | — |
| Nocturne, Horrifying | Unit | 1 | 78 | 73 | 0 | 20 | 0 | 20 / 0 | azir__teemo__s20263050__random__ismcts-sims20 D:060 | akali__teemo__s20261112__random__random D:174 | — |
| Noxian Demolitionist | Unit | 1 | 35 | 9 | 0 | 4 | 0 | 4 / 0 | akali_sb__rengar_sb__s20262111__random__random D:163 | rengar_sb__azir_sb__s20262299__random__random D:020 | — |
| Noxian Emissary | Unit | 1 | 78 | 69 | 0 | 47 | 44 | 91 / 0 | diana__reksai__s20261146__random__random D:067 | diana__reksai__s20261146__random__random D:210 | diana__reksai__s20261146__random__random D:102 |
| Noxus Hopeful | Unit | 5 | 341 | 276 | 0 | 0 | 0 | 0 / 0 | akali__akali__s20261100__random__random D:130 | — | — |
| Pakaa Cub | Unit | 1 | 78 | 14 | 11 | 0 | 0 | 0 / 0 | fiora_sb__rengar_sb__s20262183__random__random D:187 | — | — |
| Pickpocket | Unit | 2 | 68 | 50 | 0 | 50 | 0 | 50 / 0 | akali_sb__ezreal_sb__s20262103__random__random D:310 | akali_sb__ezreal_sb__s20262103__random__random D:313 | — |
| Pit Rookie | Unit | 2 | 151 | 139 | 0 | 147 | 0 | 147 / 0 | akali__rengar__s20261111__random__random D:230 | akali__rengar__s20261111__random__random D:232 | — |
| Plundering Poro | Unit | 1 | 78 | 76 | 0 | 37 | 0 | 37 / 0 | akali__kaisa__s20261106__random__random D:059 | akali__kaisa__s20261106__random__random D:078 | — |
| Poppy, Paragon | Unit | 1 | 78 | 49 | 0 | 62 | 0 | 62 / 0 | akali__fiora__s20261104__random__random D:244 | akali__fiora__s20261104__random__random D:249 | — |
| Pyke, Dockside Butcher | Unit | 1 | 78 | 22 | 13 | 35 | 0 | 35 / 0 | akali__rengar__s20261111__random__random D:125 | akali__rengar__s20261111__random__random D:129 | — |
| Pyke, Returned | Unit | 2 | 152 | 48 | 18 | 114 | 0 | 106 / 0 | azir__irelia__s20261123__random__random D:311 | azir__irelia__s20261123__random__random D:354 | — |
| Ravenbloom Prefect | Unit | 6 | 180 | 156 | 0 | 22 | 0 | 22 / 0 | akali_sb__mel_sb__s20262109__random__random D:068 | akali_sb__mel_sb__s20262109__random__random D:173 | — |
| Ravenbloom Student | Unit | 2 | 150 | 169 | 0 | 341 | 0 | 341 / 0 | akali__diana__s20261102__random__random D:005 | akali__diana__s20261102__random__random D:037 | — |
| Rek'Sai, Breacher | Unit | 1 | 78 | 83 | 0 | 0 | 0 | 0 / 0 | akali__reksai__s20261110__random__random D:006 | — | — |
| Rengar, Trophy Hunter | Unit | 4 | 285 | 284 | 0 | 0 | 0 | 0 / 0 | akali__fiora__s20261104__random__random D:061 | — | — |
| Rhasa the Sunderer | Unit | 1 | 78 | 52 | 0 | 0 | 0 | 0 / 0 | akali_sb__zed_sb__s20262117__random__random D:185 | — | — |
| Rift Herald | Unit | 1 | 78 | 60 | 0 | 70 | 0 | 141 / 0 | akali__fiora__s20261104__random__random D:188 | akali__fiora__s20261104__random__random D:240 | — |
| Ruin Runner | Unit | 1 | 78 | 53 | 0 | 0 | 0 | 0 / 0 | azir__yi_rebuild__s20261134__random__random D:035 | — | — |
| Ruined Rex | Unit | 1 | 78 | 60 | 0 | 62 | 0 | 60 / 0 | akali__leblanc__s20261108__random__random D:131 | akali_sb__leblanc_sb__s20262108__random__random D:368 | — |
| Scuttle Crab | Unit | 3 | 220 | 220 | 0 | 330 | 0 | 330 / 0 | akali__irelia__s20261105__random__random D:016 | akali__irelia__s20261105__random__random D:018 | — |
| Sett, Brawler | Unit | 1 | 78 | 34 | 0 | 57 | 27 | 84 / 0 | akali__fiora__s20261104__random__random D:032 | akali__fiora__s20261104__random__random D:038 | ezreal_sb__fiora_sb__s20262158__random__random D:197 |
| Shadow Fiend | Unit | 1 | 78 | 34 | 0 | 0 | 16 | 16 / 0 | azir__reksai__s20261128__random__random D:392 | — | azir__reksai__s20261128__random__random D:395 |
| Shadow Order Disciple | Unit | 3 | 222 | 185 | 0 | 275 | 0 | 275 / 0 | akali__khazix__s20261107__random__random D:179 | akali__khazix__s20261107__random__random D:216 | — |
| Shadowblade Lurker | Unit | 1 | 78 | 48 | 0 | 0 | 0 | 0 / 0 | akali_sb__zed_sb__s20262117__random__random D:056 | — | — |
| Sneaky Deckhand | Unit | 1 | 78 | 65 | 0 | 0 | 0 | 0 / 0 | azir__teemo__s20261130__random__random D:149 | — | — |
| Soaring Scout | Unit | 1 | 78 | 86 | 0 | 113 | 0 | 113 / 0 | akali__leblanc__s20261108__random__random D:201 | akali_sb__leblanc_sb__s20262108__random__random D:094 | — |
| Steel Paws | Unit | 1 | 78 | 83 | 0 | 0 | 24 | 24 / 0 | akali_sb__vex_sb__s20262113__random__random D:045 | — | irelia_sb__vex_sb__s20262203__random__random D:267 |
| Stellacorn Herder | Unit | 3 | 221 | 207 | 0 | 313 | 0 | 313 / 0 | akali__akali__s20261100__random__random D:016 | akali__akali__s20261100__random__random D:079 | — |
| Tail-Cloaked Matriarch | Unit | 2 | 111 | 54 | 0 | 23 | 22 | 45 / 0 | azir__khazix__s20261125__random__random D:246 | azir__khazix__s20261125__random__random D:294 | azir__khazix__s20261125__random__random D:292 |
| Teemo, Scout | Unit | 1 | 78 | 85 | 22 | 106 | 0 | 106 / 0 | akali__teemo__s20261112__random__random D:140 | akali__teemo__s20261112__random__random D:142 | — |
| Teemo, Strategist | Unit | 1 | 78 | 158 | 58 | 61 | 0 | 61 / 0 | akali__teemo__s20261112__random__random D:020 | akali__teemo__s20261112__random__random D:150 | — |
| Thousand-Tailed Watcher | Unit | 5 | 347 | 177 | 0 | 179 | 0 | 179 / 0 | akali__kaisa__s20261106__random__random D:133 | akali__kaisa__s20261106__random__random D:141 | — |
| Tideturner | Unit | 8 | 501 | 438 | 161 | 580 | 0 | 580 / 0 | akali__khazix__s20263031__random__ismcts-sims20 D:090 | akali__diana__s20261102__random__random D:052 | — |
| Tomb-Raider Barbara | Unit | 1 | 35 | 47 | 0 | 41 | 0 | 40 / 0 | akali_sb__vex_sb__s20262113__random__random D:021 | azir_sb__vex_sb__s20262131__random__random D:055 | — |
| Tornado Warrior | Unit | 2 | 152 | 65 | 47 | 102 | 0 | 152 / 0 | azir_sb__khazix_sb__s20262125__random__random D:217 | akali_sb__vex_sb__s20262113__random__random D:062 | — |
| Traveling Merchant | Unit | 4 | 285 | 249 | 0 | 308 | 0 | 599 / 0 | akali__diana__s20261102__random__random D:069 | akali__diana__s20261102__random__random D:097 | — |
| Treasure Hunter | Unit | 1 | 78 | 54 | 0 | 72 | 0 | 72 / 0 | akali__irelia__s20261105__random__random D:031 | akali__irelia__s20261105__random__random D:054 | — |
| Undertitan | Unit | 1 | 78 | 31 | 0 | 46 | 0 | 46 / 0 | akali__reksai__s20261110__random__random D:170 | akali__reksai__s20261110__random__random D:177 | — |
| Vex, Apathetic | Unit | 6 | 369 | 368 | 0 | 900 | 0 | 900 / 0 | akali__diana__s20261102__random__random D:153 | akali__khazix__s20261107__random__random D:138 | — |
| Vex, Cheerless | Unit | 1 | 35 | 16 | 0 | 0 | 0 | 0 / 0 | akali_sb__irelia_sb__s20262105__random__random D:078 | — | — |
| Vi, Peacekeeper | Unit | 2 | 152 | 76 | 0 | 17 | 0 | 16 / 0 | akali__viktor__s20261114__random__random D:181 | azir__leblanc__s20261126__random__random D:359 | — |
| Viktor, Leader | Unit | 1 | 78 | 71 | 0 | 101 | 0 | 98 / 0 | akali__viktor__s20261114__random__random D:098 | akali__viktor__s20261114__random__random D:111 | — |
| Watchful Sentry | Unit | 2 | 152 | 153 | 0 | 143 | 0 | 143 / 0 | akali__leblanc__s20261108__random__random D:040 | akali__leblanc__s20261108__random__random D:069 | — |
| Windsinger | Unit | 1 | 78 | 60 | 8 | 72 | 0 | 72 / 0 | akali_sb__teemo_sb__s20262112__random__random D:104 | akali_sb__teemo_sb__s20262112__random__random D:106 | — |
| Xin Zhao, Vigilant | Unit | 1 | 78 | 70 | 0 | 0 | 0 | 0 / 0 | azir_sb__viktor_sb__s20262132__random__random D:078 | — | — |
| Yasuo, Remorseful | Unit | 1 | 35 | 13 | 0 | 6 | 0 | 6 / 0 | azir_sb__yasuao_sb__s20262133__random__random D:143 | ezreal_sb__yasuao_sb__s20262169__random__random D:217 | — |
| Yasuo, Windrider | Unit | 1 | 78 | 70 | 0 | 142 | 0 | 142 / 0 | akali__yasuao__s20261115__random__random D:100 | akali__yasuao__s20261115__random__random D:121 | — |
| Zed, Without a Sound | Unit | 2 | 152 | 98 | 0 | 42 | 54 | 149 / 0 | akali__zed__s20261117__random__random D:278 | diana__zed__s20261153__random__random D:186 | diana_sb__zed_sb__s20262153__random__random D:209 |
| Abandon | Spell | 1 | 78 | 22 | 0 | 0 | 0 | 0 / 38 | fiora__vex__s20261185__random__random D:262 | — | — |
| Acceptable Losses | Spell | 6 | 300 | 260 | 0 | 0 | 0 | 0 / 359 | akali__diana__s20261102__random__random D:161 | — | — |
| Against the Odds | Spell | 1 | 35 | 12 | 0 | 0 | 0 | 0 / 12 | akali_sb__azir_sb__s20262101__random__random D:114 | — | — |
| Alpha Strike | Spell | 1 | 35 | 10 | 0 | 5 | 0 | 5 / 10 | diana_sb__yi_rebuild_sb__s20262152__random__random D:152 | viktor_sb__yi_rebuild_sb__s20262368__random__random D:229 | — |
| Arise! | Spell | 1 | 78 | 72 | 0 | 62 | 0 | 62 / 171 | akali__azir__s20263000__ismcts-sims20__random D:087 | akali__azir__s20263000__ismcts-sims20__random D:096 | — |
| Back Off | Spell | 4 | 286 | 157 | 45 | 0 | 0 | 0 / 181 | akali__akali__s20261100__random__random D:095 | — | — |
| Bellows Breath | Spell | 3 | 220 | 247 | 0 | 0 | 0 | 0 / 349 | akali__ezreal__s20261103__random__random D:060 | — | — |
| Block | Spell | 2 | 151 | 53 | 8 | 0 | 0 | 0 / 48 | akali__azir__s20261101__random__random D:088 | — | — |
| Blood Money | Spell | 1 | 78 | 27 | 0 | 0 | 0 | 0 / 26 | akali_sb__viktor_sb__s20262114__random__random D:023 | — | — |
| Blood Rush | Spell | 1 | 78 | 87 | 0 | 0 | 0 | 0 / 120 | akali_sb__reksai_sb__s20262110__random__random D:046 | — | — |
| Brittle Steel | Spell | 3 | 99 | 26 | 0 | 0 | 0 | 0 / 27 | akali_sb__rengar_sb__s20262111__random__random D:133 | — | — |
| Challenge | Spell | 1 | 78 | 35 | 0 | 0 | 0 | 0 / 30 | akali__fiora__s20261104__random__random D:125 | — | — |
| Charm | Spell | 4 | 212 | 113 | 0 | 0 | 0 | 0 / 190 | akali__yi_rebuild__s20261116__random__random D:090 | — | — |
| Cleave | Spell | 1 | 78 | 74 | 0 | 0 | 0 | 0 / 78 | akali__reksai__s20261110__random__random D:185 | — | — |
| Consult the Past | Spell | 1 | 78 | 59 | 13 | 0 | 0 | 0 / 61 | akali__teemo__s20261112__random__random D:064 | — | — |
| Cull the Weak | Spell | 3 | 222 | 205 | 0 | 0 | 0 | 0 / 544 | akali__viktor__s20261114__random__random D:104 | — | — |
| Deadly Flourish | Spell | 1 | 78 | 64 | 0 | 0 | 0 | 47 / 62 | akali__ezreal__s20261103__random__random D:153 | — | — |
| Death Mark | Spell | 1 | 78 | 137 | 0 | 0 | 0 | 0 / 192 | akali__zed__s20261117__random__random D:016 | — | — |
| Deathgrip | Spell | 2 | 150 | 170 | 0 | 0 | 0 | 0 / 153 | akali__azir__s20261101__random__random D:021 | — | — |
| Decree of Focus | Spell | 4 | 128 | 17 | 0 | 0 | 0 | 0 / 15 | akali_sb__azir_sb__s20262101__random__random D:230 | — | — |
| Decree of Insight | Spell | 6 | 180 | 28 | 0 | 0 | 0 | 0 / 28 | diana_sb__fiora_sb__s20262140__random__random D:158 | — | — |
| Decree of Rage | Spell | 1 | 35 | 6 | 0 | 0 | 0 | 0 / 6 | akali_sb__kaisa_sb__s20262106__random__random D:123 | — | — |
| Decree of Strength | Spell | 3 | 99 | 52 | 0 | 0 | 0 | 0 / 69 | azir_sb__khazix_sb__s20262125__random__random D:202 | — | — |
| Decree of Unity | Spell | 2 | 68 | 27 | 0 | 0 | 0 | 0 / 26 | azir_sb__diana_sb__s20262120__random__random D:205 | — | — |
| Defiant Dance | Spell | 1 | 78 | 86 | 0 | 0 | 0 | 0 / 79 | akali__irelia__s20261105__random__random D:171 | — | — |
| Defy | Spell | 6 | 402 | 357 | 0 | 0 | 0 | 0 / 341 | akali__akali__s20261100__random__random D:047 | — | — |
| Discipline | Spell | 6 | 402 | 420 | 0 | 0 | 0 | 0 / 373 | akali__akali__s20261100__random__random D:042 | — | — |
| Downwell | Spell | 1 | 78 | 11 | 0 | 0 | 0 | 0 / 11 | diana__teemo__s20261148__random__random D:097 | — | — |
| Dragon Form | Spell | 1 | 78 | 93 | 0 | 0 | 0 | 0 / 90 | akali__viktor__s20261114__random__random D:074 | — | — |
| Dredge Up | Spell | 1 | 78 | 154 | 0 | 0 | 0 | 0 / 148 | akali__mel__s20261109__random__random D:126 | — | — |
| Eclipse | Spell | 2 | 152 | 104 | 0 | 0 | 0 | 0 / 102 | akali__viktor__s20261114__random__random D:048 | — | — |
| En Garde | Spell | 4 | 250 | 213 | 0 | 0 | 0 | 0 / 193 | akali__azir__s20263000__ismcts-sims20__random D:104 | — | — |
| Existential Dread | Spell | 1 | 78 | 6 | 0 | 0 | 0 | 0 / 6 | azir_sb__teemo_sb__s20262130__random__random D:199 | — | — |
| Facebreaker | Spell | 1 | 78 | 12 | 47 | 0 | 0 | 0 / 42 | akali_sb__viktor_sb__s20262114__random__random D:320 | — | — |
| Falling Star | Spell | 3 | 222 | 210 | 0 | 0 | 0 | 0 / 209 | akali__akali__s20261100__random__random D:101 | — | — |
| Flash | Spell | 3 | 221 | 138 | 0 | 0 | 0 | 0 / 123 | akali__irelia__s20261105__random__random D:011 | — | — |
| Flurry of Blades | Spell | 1 | 78 | 58 | 0 | 0 | 0 | 0 / 55 | akali__fiora__s20261104__random__random D:016 | — | — |
| Guards! | Spell | 1 | 78 | 67 | 24 | 72 | 0 | 72 / 144 | akali__azir__s20261101__random__random D:057 | akali__azir__s20261101__random__random D:063 | — |
| Guerilla Warfare | Spell | 1 | 78 | 36 | 12 | 0 | 0 | 0 / 100 | azir__teemo__s20261130__random__random D:089 | — | — |
| Gust | Spell | 7 | 394 | 396 | 0 | 0 | 0 | 0 / 376 | akali__diana__s20261102__random__random D:101 | — | — |
| Hard Bargain | Spell | 5 | 238 | 121 | 0 | 0 | 0 | 0 / 226 | akali_sb__irelia_sb__s20262105__random__random D:108 | — | — |
| Hextech Ray | Spell | 2 | 152 | 107 | 0 | 0 | 0 | 0 / 102 | akali__kaisa__s20261106__random__random D:365 | — | — |
| Hidden Blade | Spell | 4 | 285 | 197 | 63 | 0 | 0 | 0 / 232 | akali__azir__s20261101__random__random D:050 | — | — |
| Imperial Decree | Spell | 2 | 152 | 93 | 0 | 0 | 0 | 67 / 97 | akali__reksai__s20261110__random__random D:137 | — | — |
| Invert Timelines | Spell | 1 | 78 | 26 | 0 | 0 | 0 | 0 / 26 | akali_sb__teemo_sb__s20262112__random__random D:088 | — | — |
| Ki Barrier | Spell | 1 | 78 | 32 | 0 | 0 | 0 | 0 / 31 | azir_sb__leblanc_sb__s20262126__random__random D:072 | — | — |
| Mirror Image | Spell | 1 | 78 | 103 | 0 | 102 | 0 | 102 / 102 | akali_sb__leblanc_sb__s20262108__random__random D:010 | akali_sb__leblanc_sb__s20262108__random__random D:017 | — |
| Mobilize | Spell | 1 | 78 | 65 | 0 | 0 | 0 | 0 / 58 | akali__fiora__s20261104__random__random D:108 | — | — |
| Moonfall | Spell | 1 | 78 | 63 | 0 | 0 | 0 | 0 / 57 | akali_sb__diana_sb__s20262102__random__random D:180 | — | — |
| Not So Fast | Spell | 4 | 211 | 43 | 0 | 0 | 0 | 0 / 39 | akali__kaisa__s20261106__random__random D:212 | — | — |
| Onslaught | Spell | 1 | 78 | 35 | 0 | 0 | 0 | 0 / 32 | fiora_sb__yi_rebuild_sb__s20262188__random__random D:122 | — | — |
| Perfect Execution | Spell | 2 | 150 | 157 | 0 | 0 | 0 | 0 / 152 | akali__kaisa__s20261106__random__random D:081 | — | — |
| Piercing Light | Spell | 1 | 35 | 21 | 0 | 0 | 0 | 0 / 24 | diana_sb__zed_sb__s20262153__random__random D:136 | — | — |
| Progress Day | Spell | 1 | 78 | 31 | 0 | 0 | 0 | 0 / 31 | azir__kaisa__s20261124__random__random D:080 | — | — |
| Punch First | Spell | 4 | 250 | 219 | 0 | 0 | 0 | 0 / 218 | akali__khazix__s20261107__random__random D:325 | — | — |
| Rampage | Spell | 4 | 285 | 190 | 0 | 0 | 0 | 0 / 167 | akali__fiora__s20261104__random__random D:076 | — | — |
| Rebuke | Spell | 4 | 285 | 177 | 0 | 0 | 0 | 0 / 161 | akali__vex__s20261113__random__random D:217 | — | — |
| Rebuttal | Spell | 1 | 78 | 74 | 0 | 0 | 0 | 0 / 144 | diana__mel__s20263039__random__ismcts-sims20 D:201 | — | — |
| Repulse | Spell | 1 | 78 | 24 | 0 | 0 | 0 | 0 / 39 | akali__fiora__s20261104__random__random D:147 | — | — |
| Ride the Wind | Spell | 7 | 456 | 314 | 0 | 0 | 0 | 0 / 589 | akali__irelia__s20263001__ismcts-sims20__random D:167 | — | — |
| Riposte | Spell | 1 | 78 | 58 | 0 | 0 | 0 | 0 / 57 | akali__fiora__s20261104__random__random D:111 | — | — |
| Rocket Barrage | Spell | 1 | 78 | 19 | 0 | 0 | 0 | 0 / 20 | akali__kaisa__s20261106__random__random D:230 | — | — |
| Sabotage | Spell | 3 | 221 | 177 | 0 | 0 | 0 | 0 / 317 | akali__yi_rebuild__s20261116__random__random D:124 | — | — |
| Sacrifice | Spell | 2 | 152 | 95 | 0 | 0 | 0 | 0 / 89 | akali__fiora__s20261104__random__random D:043 | — | — |
| Salvage | Spell | 5 | 155 | 168 | 0 | 0 | 0 | 0 / 165 | akali_sb__azir_sb__s20262101__random__random D:058 | — | — |
| Shock Blast | Spell | 1 | 78 | 61 | 0 | 0 | 0 | 0 / 58 | azir__mel__s20261127__random__random D:132 | — | — |
| Shuriken Flip | Spell | 1 | 78 | 131 | 0 | 0 | 0 | 0 / 393 | akali__azir__s20261101__random__random D:202 | — | — |
| Singularity | Spell | 4 | 249 | 145 | 0 | 0 | 0 | 0 / 139 | akali__ezreal__s20261103__random__random D:239 | — | — |
| Smite | Spell | 1 | 35 | 19 | 0 | 0 | 0 | 0 / 21 | azir_sb__reksai_sb__s20262128__random__random D:180 | — | — |
| Smoke Screen | Spell | 3 | 220 | 140 | 0 | 0 | 0 | 0 / 137 | akali__kaisa__s20261106__random__random D:167 | — | — |
| Sprite Burst | Spell | 2 | 68 | 31 | 0 | 0 | 0 | 0 / 53 | azir_sb__kaisa_sb__s20262124__random__random D:296 | — | — |
| Sprite Call | Spell | 1 | 78 | 52 | 19 | 0 | 0 | 0 / 82 | akali_sb__teemo_sb__s20262112__random__random D:028 | — | — |
| Star-Crossed | Spell | 9 | 467 | 439 | 0 | 0 | 0 | 0 / 426 | akali__diana__s20261102__random__random D:136 | — | — |
| Stupefy | Spell | 6 | 400 | 444 | 0 | 0 | 0 | 0 / 440 | akali__diana__s20261102__random__random D:013 | — | — |
| Switcheroo | Spell | 8 | 473 | 257 | 98 | 0 | 0 | 0 / 303 | akali__irelia__s20263001__ismcts-sims20__random D:072 | — | — |
| Temporal Breach | Spell | 2 | 152 | 109 | 69 | 0 | 0 | 0 / 162 | akali__kaisa__s20261106__random__random D:287 | — | — |
| Thermo Beam | Spell | 3 | 99 | 53 | 0 | 0 | 0 | 0 / 51 | akali_sb__akali_sb__s20262100__random__random D:194 | — | — |
| Thrill of the Hunt | Spell | 1 | 78 | 63 | 0 | 0 | 0 | 0 / 117 | akali__rengar__s20263046__random__ismcts-sims20 D:024 | — | — |
| Time Warp | Spell | 3 | 183 | 46 | 0 | 0 | 0 | 0 / 46 | azir_sb__leblanc_sb__s20262126__random__random D:192 | — | — |
| Turn to Dust | Spell | 3 | 142 | 53 | 0 | 0 | 0 | 0 / 55 | akali__ezreal__s20261103__random__random D:078 | — | — |
| Unchecked Power | Spell | 1 | 35 | 12 | 0 | 0 | 0 | 0 / 12 | ezreal_sb__mel_sb__s20262163__random__random D:234 | — | — |
| Up from the Deep | Spell | 4 | 212 | 242 | 0 | 0 | 0 | 0 / 444 | akali__khazix__s20263031__random__ismcts-sims20 D:065 | — | — |
| Void Assault | Spell | 1 | 78 | 52 | 0 | 0 | 0 | 0 / 106 | akali__khazix__s20261107__random__random D:019 | — | — |
| Void Rush | Spell | 1 | 78 | 65 | 0 | 0 | 0 | 0 / 181 | azir__reksai__s20261128__random__random D:116 | — | — |
| Wages of Pain | Spell | 1 | 78 | 76 | 31 | 0 | 0 | 0 / 105 | akali__ezreal__s20261103__random__random D:125 | — | — |
| Wind Wall | Spell | 1 | 35 | 20 | 0 | 0 | 0 | 0 / 19 | azir_sb__yasuao_sb__s20262133__random__random D:023 | — | — |
| B.F. Sword | Gear | 2 | 150 | 93 | 0 | 0 | 110 | 0 / 0 | akali__azir__s20261101__random__random D:026 | — | akali__azir__s20261101__random__random D:155 |
| Boots of Swiftness | Gear | 2 | 152 | 102 | 0 | 0 | 140 | 0 / 0 | akali_sb__irelia_sb__s20262105__random__random D:010 | — | akali_sb__irelia_sb__s20262105__random__random D:025 |
| Brutalizer | Gear | 1 | 78 | 68 | 0 | 0 | 82 | 0 / 0 | akali__azir__s20263000__ismcts-sims20__random D:248 | — | akali__azir__s20263000__ismcts-sims20__random D:260 |
| Dazzling Aurora | Gear | 1 | 78 | 22 | 0 | 88 | 0 | 170 / 0 | diana__fiora__s20261140__random__random D:056 | diana__fiora__s20261140__random__random D:075 | — |
| Divining Shells | Gear | 1 | 78 | 54 | 0 | 54 | 32 | 100 / 0 | akali__fiora__s20261104__random__random D:289 | akali__fiora__s20261104__random__random D:291 | akali__fiora__s20261104__random__random D:310 |
| Doran's Shield | Gear | 1 | 78 | 83 | 0 | 0 | 97 | 0 / 0 | akali__azir__s20261101__random__random D:072 | — | akali__azir__s20261101__random__random D:085 |
| Edge of Night | Gear | 1 | 78 | 31 | 21 | 51 | 48 | 51 / 0 | akali__irelia__s20261105__random__random D:102 | akali__irelia__s20261105__random__random D:105 | akali__irelia__s20261105__random__random D:108 |
| Eye of the Herald | Gear | 1 | 78 | 76 | 0 | 0 | 92 | 124 / 0 | akali__azir__s20263000__ismcts-sims20__random D:115 | — | akali__azir__s20263000__ismcts-sims20__random D:179 |
| Forge of the Future | Gear | 2 | 68 | 20 | 0 | 21 | 17 | 92 / 0 | azir_sb__reksai_sb__s20262128__random__random D:040 | azir_sb__reksai_sb__s20262128__random__random D:042 | azir_sb__reksai_sb__s20262128__random__random D:051 |
| Hand Hammer | Gear | 1 | 78 | 63 | 0 | 0 | 64 | 0 / 0 | akali__azir__s20263000__ismcts-sims20__random D:320 | — | akali__azir__s20263000__ismcts-sims20__random D:393 |
| Last Rites | Gear | 1 | 78 | 48 | 0 | 0 | 68 | 79 / 0 | azir__zed__s20261135__random__random D:033 | — | azir__zed__s20261135__random__random D:046 |
| Long Sword | Gear | 1 | 78 | 62 | 0 | 0 | 31 | 0 / 0 | akali__azir__s20261101__random__random D:189 | — | akali__azir__s20261101__random__random D:221 |
| Pack of Wonders | Gear | 1 | 78 | 111 | 0 | 0 | 154 | 154 / 0 | akali_sb__ezreal_sb__s20262103__random__random D:352 | — | akali_sb__ezreal_sb__s20262103__random__random D:392 |
| Seal of Discord | Gear | 1 | 78 | 59 | 0 | 0 | 64 | 263 / 0 | akali__zed__s20261117__random__random D:005 | — | akali__zed__s20261117__random__random D:013 |
| Shepherd's Heirloom | Gear | 1 | 78 | 51 | 0 | 51 | 49 | 51 / 0 | akali_sb__fiora_sb__s20262104__random__random D:121 | akali_sb__fiora_sb__s20262104__random__random D:123 | akali_sb__fiora_sb__s20262104__random__random D:209 |
| Soul Sword | Gear | 1 | 78 | 84 | 0 | 0 | 103 | 0 / 0 | akali__azir__s20261101__random__random D:005 | — | akali__azir__s20261101__random__random D:071 |
| Sprite Fountain | Gear | 4 | 285 | 219 | 0 | 419 | 0 | 419 / 0 | akali__ezreal__s20261103__random__random D:011 | akali__ezreal__s20261103__random__random D:014 | — |
| Sterak's Gage | Gear | 1 | 78 | 30 | 0 | 0 | 12 | 0 / 0 | akali__akali__s20261100__random__random D:168 | — | akali__azir__s20261101__random__random D:117 |
| The List | Gear | 1 | 78 | 86 | 0 | 86 | 56 | 228 / 0 | akali__ezreal__s20261103__random__random D:051 | akali__ezreal__s20261103__random__random D:052 | diana__ezreal__s20261139__random__random D:440 |
| Treasure Trove | Gear | 1 | 78 | 71 | 0 | 68 | 59 | 127 / 0 | akali__ezreal__s20261103__random__random D:092 | akali__ezreal__s20261103__random__random D:147 | akali__ezreal__s20261103__random__random D:145 |
| Zhonya's Hourglass | Gear | 5 | 345 | 165 | 106 | 0 | 0 | 0 / 0 | akali__akali__s20261100__random__random D:127 | — | — |
| Abandoned Hall | Battlefield | 2 | 152 | 48 | 0 | 653 | 0 | 653 / 0 | akali__diana__s20261102__random__random D:002 | akali__diana__s20261102__random__random D:024 | — |
| Amateur Recital | Battlefield | 1 | 78 | 28 | 0 | 92 | 0 | 81 / 0 | azir__fiora__s20261122__random__random D:002 | azir__fiora__s20261122__random__random D:050 | — |
| Bandle Tree | Battlefield | 1 | 78 | 23 | 0 | 0 | 0 | 0 / 0 | akali__vex__s20261113__random__random D:002 | — | — |
| Dusk Rose Lab | Battlefield | 3 | 222 | 86 | 0 | 1754 | 0 | 1754 / 0 | akali__leblanc__s20261108__random__random D:002 | akali__leblanc__s20261108__random__random D:004 | — |
| Emperor's Dais | Battlefield | 2 | 151 | 56 | 0 | 138 | 0 | 133 / 0 | akali__rengar__s20263046__random__ismcts-sims20 D:002 | akali__rengar__s20263046__random__ismcts-sims20 D:038 | — |
| Forbidding Waste | Battlefield | 3 | 220 | 79 | 0 | 0 | 0 | 0 / 0 | akali__yasuao__s20261115__random__random D:002 | — | — |
| Forgotten Monument | Battlefield | 1 | 78 | 26 | 0 | 0 | 0 | 0 / 0 | akali__akali__s20261100__random__random D:001 | — | — |
| Fortified Position | Battlefield | 1 | 78 | 33 | 0 | 72 | 0 | 72 / 0 | akali_sb__vex_sb__s20262113__random__random D:002 | akali_sb__vex_sb__s20262113__random__random D:068 | — |
| Frozen Fortress | Battlefield | 3 | 222 | 79 | 0 | 1636 | 0 | 1636 / 0 | akali__kaisa__s20261106__random__random D:002 | akali__kaisa__s20261106__random__random D:004 | — |
| Grove of the God-Willow | Battlefield | 1 | 78 | 25 | 0 | 76 | 0 | 69 / 0 | azir_sb__teemo_sb__s20262130__random__random D:002 | azir_sb__teemo_sb__s20262130__random__random D:144 | — |
| Hall of Legends | Battlefield | 1 | 78 | 25 | 0 | 67 | 0 | 64 / 0 | akali__azir__s20261101__random__random D:002 | akali__azir__s20261101__random__random D:045 | — |
| Minefield | Battlefield | 1 | 78 | 21 | 0 | 49 | 0 | 47 / 0 | azir_sb__zed_sb__s20262135__random__random D:002 | azir_sb__zed_sb__s20262135__random__random D:072 | — |
| Risen Altar | Battlefield | 1 | 78 | 36 | 0 | 0 | 0 | 0 / 0 | akali__fiora__s20261104__random__random D:002 | — | — |
| Rockfall Path | Battlefield | 3 | 221 | 86 | 0 | 0 | 0 | 0 / 0 | akali__mel__s20261109__random__random D:002 | — | — |
| Sandswept Tomb | Battlefield | 2 | 152 | 48 | 0 | 0 | 0 | 0 / 0 | akali__zed__s20261117__random__random D:002 | — | — |
| Seat of Power | Battlefield | 6 | 402 | 142 | 0 | 349 | 0 | 336 / 0 | akali__rengar__s20261111__random__random D:002 | akali__rengar__s20261111__random__random D:122 | — |
| Sigil of the Storm | Battlefield | 2 | 152 | 56 | 0 | 151 | 0 | 284 / 0 | akali__azir__s20261101__random__random D:001 | akali__azir__s20261101__random__random D:081 | — |
| Star Spring | Battlefield | 4 | 286 | 118 | 0 | 235 | 0 | 235 / 0 | akali__yi_rebuild__s20261116__random__random D:002 | akali__yi_rebuild__s20261116__random__random D:117 | — |
| Startipped Peak | Battlefield | 2 | 151 | 53 | 0 | 180 | 0 | 162 / 0 | akali__teemo__s20261112__random__random D:002 | akali__teemo__s20261112__random__random D:050 | — |
| Sunken Temple | Battlefield | 3 | 219 | 73 | 0 | 75 | 0 | 68 / 0 | akali__irelia__s20263001__ismcts-sims20__random D:002 | akali__irelia__s20263001__ismcts-sims20__random D:053 | — |
| Targon's Peak | Battlefield | 3 | 220 | 90 | 0 | 228 | 0 | 445 / 0 | akali__irelia__s20261105__random__random D:002 | akali__irelia__s20261105__random__random D:026 | — |
| Trifarian War Camp | Battlefield | 1 | 78 | 30 | 0 | 0 | 0 | 0 / 0 | akali__azir__s20263000__ismcts-sims20__random D:002 | — | — |
| Vilemaw's Lair | Battlefield | 1 | 78 | 22 | 0 | 0 | 0 | 0 / 0 | ezreal__azir__s20261155__random__random D:001 | — | — |
| Void Gate | Battlefield | 3 | 222 | 82 | 0 | 0 | 0 | 0 / 0 | akali__akali__s20261100__random__random D:002 | — | — |
| Windswept Hillock | Battlefield | 1 | 78 | 19 | 0 | 0 | 0 | 0 / 0 | khazix__leblanc__s20261234__random__random D:002 | — | — |
| Zaun Warrens | Battlefield | 2 | 152 | 56 | 0 | 142 | 0 | 269 / 0 | akali__khazix__s20261107__random__random D:002 | akali__khazix__s20261107__random__random D:059 | — |
| Body Rune | Rune | 4 | 285 | 3421 | 0 | 0 | 0 | 0 / 0 | akali__fiora__s20261104__random__random D:010 | — | — |
| Calm Rune | Rune | 6 | 402 | 3816 | 0 | 0 | 0 | 0 / 0 | akali__akali__s20261100__random__random D:004 | — | — |
| Chaos Rune | Rune | 9 | 546 | 6971 | 0 | 0 | 0 | 0 / 0 | akali__diana__s20261102__random__random D:012 | — | — |
| Fury Rune | Rune | 5 | 341 | 3370 | 0 | 0 | 0 | 0 / 0 | akali__akali__s20261100__random__random D:005 | — | — |
| Mind Rune | Rune | 7 | 454 | 4718 | 0 | 0 | 0 | 0 / 0 | akali__diana__s20261102__random__random D:004 | — | — |
| Order Rune | Rune | 5 | 347 | 3950 | 0 | 0 | 0 | 0 / 0 | akali__azir__s20261101__random__random D:008 | — | — |

## 5. Findings

Numbered D1... in the order lead (engine-level sequences), then reader by reader. A reader's finding that is the
same defect as an earlier one is not numbered again: its text stands under that finding as further evidence.
Confidence: CERTAIN = the card text or a rule says so in terms; LIKELY = a reading of the rules, cited;
QUESTION = the rules do not settle it (section 7); CODE-ONLY = seen in the card's class or the engine source,
not in a trace (the finding says which board would show it).

| id | finding | confidence | from |
|---|---|---|---|
| D1 | Every permanent with a play trigger; Accelerate units: "when you play" abilities are queued and finalized before the permanent has entered, and Accelerate is asked after them | CERTAIN | lead (+1 merged) |
| D2 | Daughter of the Void, Scorn of the Moon, Gold, Seal of Discord: an [Add] ability is an ordinary chain item — both players get priority before it resolves, the opponent can respond on top of it, and the resource arrives late | CERTAIN | lead (+1 merged) |
| D3 | Ride the Wind, Unforgiven, Charm, Void Assault (and the cards the readers add: Shuriken Flip, Thrill of the Hunt): the destination of a move is chosen as the spell or ability resolves | CERTAIN | lead |
| D4 | Every "you may" trigger, and triggers with nothing to choose: a declined, unpayable or targetless triggered ability stays on the chain, both players get priority, and it "resolves" | CERTAIN / LIKELY | lead (+4 merged) |
| D5 | Combat is not a Showdown State for the action generator: spells with neither [Action] nor [Reaction] are offered and played at Focus in a combat, by the defender on the opponent's turn too | CERTAIN | lead (+6 merged) |
| D6 | Any non-Reaction spell: offered as a plain play at a chain-priority decision; choosing it does nothing and the same decision is asked again | CERTAIN | lead (+1 merged) |
| D7 | A triggered or delayed ability queued during a showdown is left pending: Focus is passed, the showdown closes and combat damage is dealt with the item still on the chain | CERTAIN | lead (+1 merged) |
| D8 | Star-Crossed, Switcheroo, Facebreaker, Defiant Dance: offered, paid for and put on the chain without a legal choice for each target | CERTAIN | lead (+1 merged) |
| D9 | Every payment that is not a card's base cost: the engine chooses which runes are exhausted and recycled | LIKELY | lead (+1 merged) |
| D10 | Equipment whose bearer dies is not recalled by the next cleanup: it lies unattached at the battlefield through priority windows, once through a Main Phase decision | LIKELY | lead |
| D11 | Hidden information in log lines whose audience is both players | CERTAIN | lead (+1 merged) |
| D12 | Deck validator: no rule for [Unique] | CODE-ONLY | lead |
| D13 | Combat damage: the attacker's damage is dealt before the defender has assigned | LIKELY | lead |
| D14 | Long Sword, Sterak's Gage ([Quick-Draw]): the attach is done inline as the gear lands, not as a triggered ability on the chain | CERTAIN | reader 1, R1-2 |
| D15 | Long Sword, Sterak's Gage ([Equip]): the Equip ability attaches at activation — no chain item, no priority | CERTAIN | reader 1, R1-3 (+1 merged) |
| D16 | Shuriken Flip: the enemy unit, the friendly unit and the move destination are all chosen as it resolves | CERTAIN | reader 1, R1-4 |
| D17 | Shuriken Flip: playable with no friendly unit to move | LIKELY | reader 1, R1-5 |
| D18 | Astral Heron: the discount is kept for a later turn | CERTAIN | reader 1, R1-6 |
| D19 | Astral Heron: the ability is put on the chain while the Heron is not at a battlefield | LIKELY | reader 1, R1-7 |
| D20 | Falling Star: kills between its two instructions, so a death replaced after the first hit is died again on the second | CERTAIN | reader 1, R1-8 |
| D21 | Counter spells (Defy, Not So Fast, Hard Bargain, Riposte, Repulse, Rebuttal, Abandon, Wind Wall): the spell to counter is never chosen — only the top item of the chain can be countered | CODE-ONLY / LIKELY / CERTAIN | reader 1, R1-10 (+5 merged) |
| D22 | Emperor of the Sands / Sand Soldier [Weaponmaster]: the "may" and the Equipment are chosen at resolution, not at finalization | CERTAIN | reader 2, R2-1 |
| D23 | Trifarian War Camp / Azir, Sovereign (and other continuous effects): Might is not recomputed before the next window or the damage step — a token moved in by Azir's trigger fights without the +1 [M] | CERTAIN | reader 2, R2-3 |
| D24 | Azir, Sovereign: "you may move any number of your token units" is asked at resolution | LIKELY | reader 2, R2-4 |
| D25 | Hidden cards (Guards!, Hidden Blade, Edge of Night, Pyke, Returned, Wages of Pain, Temporal Breach): a facedown card is never offered in its controller's own Neutral Open State | LIKELY / CERTAIN | reader 2, R2-6 (+1 merged) |
| D26 | Vex, Cheerless: an enemy spell played while she is in combat is not taxed | CERTAIN | reader 2, R2-7 |
| D27 | Pyke, Returned / Edge of Night: the trigger's condition is tested at resolution, so the ability goes on the chain when it should not trigger | LIKELY | reader 2, R2-8 |
| D28 | Deathgrip: playable with a single friendly unit (no "another friendly unit" to choose) | QUESTION | reader 2, R2-10 |
| D29 | Diana, Lunari: "When a showdown begins here" never triggers on a combat showdown | CERTAIN | reader 3, R3-1 |
| D30 | Scorn of the Moon cannot pay Diana, Lunari's [1]: the "may" is withheld with the legend ready (outcome of K2) | CERTAIN | reader 3, R3-4 |
| D31 | Scorn of the Moon activated with Focus passes Focus to the opponent (outcome of K2) | CERTAIN | reader 3, R3-5 |
| D32 | Fizz, Trickster: a spell whose target is taken from the play intent is replayed with no target (Stupefy, Eclipse, Smoke Screen, Turn to Dust) | CERTAIN | reader 3, R3-6 |
| D33 | Vex, Apathetic: the ability is put on the chain while Vex is in a base; "while I'm at a battlefield" is tested at resolution only | LIKELY / CODE-ONLY | reader 3, R3-8 (+1 merged) |
| D34 | Decree of Insight: its target is chosen after the cost is paid | CERTAIN | reader 3, R3-10 |
| D35 | Grand Duelist / Fiora, Worthy: "becomes [Mighty]" is only noticed at the engine's top-level cleanups, so the triggers arrive late | CERTAIN / LIKELY | reader 4, R4-1 |
| D36 | Sett, Brawler + Grand Duelist / Fiora, Worthy: a unit that stops being Mighty and becomes Mighty again inside one chain triggers nothing | CERTAIN | reader 4, R4-2 |
| D37 | Rengar, Trophy Hunter: the enemy-occupied battlefield is never a play location in a Neutral Open state | LIKELY | reader 4, R4-6 |
| D38 | Divining Shells: "Kill this" is paid when the ability resolves | CERTAIN | reader 4, R4-8 |
| D39 | Sacrifice + Rift Herald: an ability triggered by paying a spell's additional cost is put UNDER that spell | LIKELY | reader 4, R4-9 |
| D40 | Grand Duelist + Fiora, Worthy (and Akshan's two play triggers): the controller is never asked in which order simultaneous triggers go on the chain | CERTAIN | reader 4, R4-10 |
| D41 | Repulse, Forge of the Future: choices the rules place at finalization are made after RESOLVE (K3 kind) | LIKELY | reader 4, R4-11 |
| D42 | Fiora, Victorious: her [Shield] adds nothing to her Might while a chain of the combat showdown is running | LIKELY | reader 4, R4-12 |
| D43 | Ezreal, Prodigy + Thousand-Tailed Watcher (any Accelerate): Accelerate is not an "optional additional cost" for Ezreal's discount | CERTAIN | reader 5, R5-1 |
| D44 | A unit with lethal damage stays on the board until the chain is empty: no cleanup between the items of a chain (Wages of Pain, Deadly Flourish, Singularity, Unchecked Power, Piercing Light) | CERTAIN / LIKELY | reader 5, R5-3 (+2 merged) |
| D45 | Bellows Breath: the Repeat's choices ignore "at the same location", are forced to the first execution's count, and are not asked at all when the first execution chose nothing | CERTAIN | reader 5, R5-4 |
| D46 | The List: "As you play this, name a tag" is run as a triggered ability with priority windows, and only tags already public can be named | LIKELY / QUESTION | reader 5, R5-6 |
| D47 | Temporal Breach + Rockfall Path: the banished unit is played back onto a battlefield where "Units can't be played" | LIKELY | reader 5, R5-7 |
| D48 | Temporal Breach (and every effect that plays a unit "ignoring its cost"): Accelerate is never offered | LIKELY | reader 5, R5-8 |
| D49 | Void Gate: no Bonus Damage for a unit that arrived there earlier on the same chain (Akali, Deadly Weapon's move ability) | LIKELY | reader 5, R5-9 |
| D50 | Daughter of the Void: its "[A]. Use only to play spells" cannot pay the Deflect of a spell | CODE-ONLY / LIKELY | reader 5, R5-10 |
| D51 | Kha'Zix, Mutating Horror (also Evelynn, Entrancing; Tornado Warrior): the condition printed with the trigger is tested when the ability resolves, not when the trigger condition is met | CERTAIN | reader 6, R6-1 |
| D52 | Star Spring: "the first time ... each turn" triggers a second time when the same card is played there again that turn | CERTAIN | reader 6, R6-2 |
| D53 | [Temporary] (Deceiver / Mirror Image Reflections; Dusk Rose Lab): the kill is done directly at the start of the Beginning Step, never as a triggered ability on the chain | LIKELY | reader 6, R6-3 |
| D54 | Tornado Warrior (Evelynn, Entrancing by code): the leading "you may" and the target are decided after RESOLVE | CERTAIN / LIKELY | reader 6, R6-6 |
| D55 | LeBlanc, Everywhere at Once: the [Temporary] kill still happens at her battlefield, and the dying unit's other triggered abilities are suppressed instead | CERTAIN | reader 6, R6-7 |
| D56 | LeBlanc, Fragmented: Awaken Phase is counted as "your Beginning Phase" | CODE-ONLY | reader 6, R6-8 |
| D57 | Void Burrower / Void Rush + Cleave, Smite (Blood Rush): a spell played by an effect that needs a target resolves with none | CERTAIN | reader 7, R7-1 |
| D58 | Rek'Sai, Breacher + Void Burrower: a unit played from banishment is not offered Accelerate | CERTAIN | reader 7, R7-2 |
| D59 | Rek'Sai, Breacher: the grant marked in D58 (R7-2) stays on the card; a later play from hand without Rek'Sai on the board is offered Accelerate | CERTAIN | reader 7, R7-3 |
| D60 | Forbidding Waste: the -2 [M] is missing when the combat goes straight to the damage step | CERTAIN | reader 7, R7-4 |
| D61 | Inferna ([Assault 2]) played with [Ambush] into her side's attack is listed as an attacker without the Assault | LIKELY | reader 7, R7-5 |
| D62 | Thrill of the Hunt offers and plays to Rockfall Path | CERTAIN | reader 7, R7-6 |
| D63 | Rengar, Trophy Hunter in the Champion Zone is never offered as an [Ambush] play | LIKELY | reader 7, R7-7 |
| D64 | Undertitan "As I'm revealed from your deck, [Add] [2]" waits on the chain and arrives after the reveal effect is over (K2) | LIKELY | reader 7, R7-10 |
| D65 | Mel, Newly Awakened: the extra -1 [M] is added to every -[M] her controller's resolving item gives, chosen unit or not | CODE-ONLY | reader 7, R7-11 |
| D66 | Herald of the Arcane: the Recruit always goes to the base; no location is offered | LIKELY | reader 8, R8-1 |
| D67 | Swift Scout, Guerilla Warfare: the objects are chosen after RESOLVE; Swift Scout can be activated with nothing to choose | LIKELY | reader 8, R8-2 |
| D68 | Teemo, Strategist (every "When I defend / attack"): a unit that joins a combat already under way never triggers | CERTAIN / LIKELY | reader 8, R8-3 (+1 merged) |
| D69 | Sprite Call played from facedown: the Sprite may be put in the base or at another battlefield | CERTAIN | reader 8, R8-4 |
| D70 | Windsinger played from facedown: a unit at another battlefield is offered | CERTAIN | reader 8, R8-5 |
| D71 | Nocturne, Horrifying: the "As you look at or reveal me" replacement is a triggered ability on the chain | CERTAIN / QUESTION | reader 8, R8-6 |
| D72 | Scoring Step: battlefields are held one at a time, each hold's abilities resolved before the next hold | LIKELY | reader 8, R8-7 |
| D73 | Fortified Position: the [Shield 2] lasts the turn, not the combat | CERTAIN / CODE-ONLY | reader 8, R8-10 |
| D74 | Facebreaker from facedown: offered with no friendly + enemy pair at its battlefield (and as up to 20 identical intents) | CERTAIN | reader 8, R8-11 |
| D75 | Master of Shadows: empowered when an OPPONENT banishes a card its controller owns | CERTAIN | reader 9, R9-1 |
| D76 | Wuju Bladesman - Starter: the +2 [M] is missing whenever nothing resolves between "COMBAT:" and the damage step | CERTAIN | reader 9, R9-2 |
| D77 | Yasuo, Windrider: an ability is put on the chain for every move, not only "the third time I move in a turn" (same pattern: Fiora, Peerless when not one on one) | LIKELY | reader 9, R9-5 |
| D78 | Kennen, Storm of Shuriken: the spell that gets [Flow] is chosen at resolution | LIKELY | reader 9, R9-6 |
| D79 | Switcheroo on a unit with Deflect (Master Yi, Tempered at Level 6): Deflect is charged for the play option's unit, then both units are asked again — charged without choosing it, or charged twice | CERTAIN | reader 9, R9-7 |
| D80 | Lonely Poro: "died alone" when every other friendly unit there died in the same damage step | QUESTION | reader 9, R9-10 |
| D81 | Minefield: no Burn Out when the Main Deck has fewer than 2 cards | CODE-ONLY | reader 9, R9-11 |

### D1  Every permanent with a play trigger; Accelerate units: "when you play" abilities are queued and finalized before the permanent has entered, and Accelerate is asked after them

_Origin: lead._

- happened: `rengar__diana__s20261300__random__random`, D:113-121: `[D:113.03] PLAY: Kai'Sa, Survivor (id=35, Unit, cost=4E)`, four `EXHAUST_RUNE`, then `[D:117.02] TRIGGER: Pridestalker (P1) fires ability` / `[D:117.04] TGT_PROMPT: Pridestalker: give a unit +1 [M] [Grim Apothecary/Kai'Sa, Survivor/Nidalee, Cat Form/Ravenbloom Student/Traveling Merchant/Traveling Merchant]` / `[D:118.03] TGT_PICKED: Pridestalker: give a unit +1 [M] = Traveling Merchant (id=63)` and only then `[D:119.00] DECISION #127 (P1): pay [1][Fury] for Accelerate on Kai'Sa, Survivor? [decline|pay] [2 options]` / `[D:119.01] ACCELERATE: Kai'Sa, Survivor pays [1] + power to enter ready`. `akali__diana__s20261102__random__random`, D:027-034: Fizz, Trickster is on the board as `Fizz, Trickster(3M,rdy)` on the boards of decisions #32 and #33 (the prompts of its own play ability) and `(3M,exh)` from #34. Code: `GameEngine::executePlayCard` emits `CardPlayedEvent` (triggers are queued) and only then `addPermanent` + `runChain`; the finalize step takes the oldest pending item first, so the abilities' choices come before `resolvePermanent` (the Accelerate prompt, `is_exhausted`).
- should: additional costs chosen and paid with the card's cost; the unit enters (exhausted, or ready by Accelerate); then the play abilities are put on the chain and their choices made.
- confidence: CERTAIN on the order (383.4.a.2, 355.1.a in terms). No changed outcome was found in the traces: the choices are made against a board that shows a ready unit the rules never show (the deck-level script flagged 1,720 unit plays whose first rendered board has them ready; 0 remain at the first top-level decision).
- also seen: every Accelerate decision in the 1,116 games follows the play triggers; readers' K1 notes list the cards (Tideturner, Fizz, Mischievous Marai, Blitzcrank, Disarming Rake, Scuttle Crab, Ashe, Kinkou Initiate, Tornado Warrior, Chakram Dancer, Kennen, Azir, Kai'Sa).

**Further evidence — reader 4, D1 (R4-13): Rampage, Akshan, Mischievous: the optional additional cost is asked after the base cost has been paid (K4 kind)**

- happened: akali__fiora__s20261104__random__random `[D:078.04] PAY_COST: Rampage E=3 P=0`, three `EXHAUST_RUNE`, `[D:082.00] DECISION #84 (P2): pay additional cost for Rampage? [decline|pay]`, `[D:082.01] PAY_ADD: P2 pays [0] + 1 power`. fiora_sb__akali_sb__s20262172__random__random D:126.04-131.02 (Akshan, [O][O]).
- should: decided with the other choices, paid with the cost.
- confidence: CERTAIN on the order; no changed outcome seen.
- also seen: 73 `pay additional cost for Rampage?` decisions in the fiora games; the two read both come after the Energy is paid.

### D2  Daughter of the Void, Scorn of the Moon, Gold, Seal of Discord: an [Add] ability is an ordinary chain item — both players get priority before it resolves, the opponent can respond on top of it, and the resource arrives late

_Origin: lead._

- happened: `kaisa__akali__s20261208__random__random`: `[D:076.01] CHOSE: [P1] ActivateReactionAbility` / `[D:077.00] DECISION #79 (P1 — chain priority): 1 legal actions` / `[D:078.00] DECISION #80 (P2 — chain priority): 2 legal actions` / `[D:078.01] CHOSE: [P2] PlayReaction` / `[D:078.03] SPELL: Discipline (id=59) targets=[]` — the board at that decision has the chain `[top] Daughter of the Void (P1)` / `[bot] Thousand-Tailed Watcher (P1)`; the Power is added only at `[D:085.02] RESOLVE: Daughter of the Void (chain_item=19, ability=1, spell=0)` / `[D:085.03] DAUGHTER OF THE VOID: [E] -> [Add] [A]`. Scorn of the Moon: `akali__diana__s20261102__random__random` D:020-022 (`[D:022.02] RESOLVE: Scorn of the Moon (chain_item=5, ability=1, spell=0)` after both players passed).
- should: the ability resolves as it is finalized; nobody receives priority; the Energy / Power is in the pool at once (and can pay the cost being paid).
- confidence: CERTAIN.
- also seen: 704 Daughter of the Void and 672 Scorn of the Moon resolutions, all through priority. Outcomes recorded by the readers as findings of their own (kept below): the legend's Energy cannot pay a cost due at finalization (D30 (R3-4)), an Add activated with Focus hands Focus to the opponent (D31 (R3-5)).

**Further evidence — reader 9, D2 (R9-9): Seal of Discord (204): its [Add] ability is a chain item with a priority window (K2, another card)**

- happened: azir__zed__s20261135__random__random, D:077-083: `CHOSE: [P2] ActivateAbility (src=Seal of Discord)` / `DECISION #83 (P1 — chain priority): 2 legal actions` / `CHOSE: [P1] PlayReaction` / `[D:081.02] RESOLVE: Back Off ...` / `[D:083.02] RESOLVE: Seal of Discord (chain_item=16, ability=1, spell=0)` / `EFFECT: P2 adds 1 floating Chaos Power (pool=1)`.
- should: no chain item, no window. (The class comment says "Resolves immediately (CR 429.2), not via the chain".)
- confidence: CERTAIN (the card's own reminder text). All 64 activations go through the chain; the opponent reacted on top in azir__zed__s20261135 D:077 and teemo__zed__s20263049__ismcts-sims20__random D:078. The payment prompts I read list only runes ("pick rune to exhaust / recycle"), so it is used as a separate action beforehand, not while paying (CR 445.2.c) — not scanned.

### D3  Ride the Wind, Unforgiven, Charm, Void Assault (and the cards the readers add: Shuriken Flip, Thrill of the Hunt): the destination of a move is chosen as the spell or ability resolves

_Origin: lead._

- happened: `teemo__vex__s20261329__random__random`: Ride the Wind chooses Irelia, Fervent as it is played; both players pass; `[D:242.02] RESOLVE: Ride the Wind (chain_item=59, ability=0, spell=1)` / `[D:242.03] MODE_PROMPT: Ride the Wind: move it to [Battlefield #0/Battlefield #1] (agent decides among 2 modes)` / `[D:243.03] MODE_PICKED: Ride the Wind: move it to = mode#2 (Battlefield #1)`. Unforgiven: `yasuao__akali__s20261370__random__random` `[D:138.01] CHOSE: [P1] ActivateAbility (src=Unforgiven) tgt=[Stellacorn Herder]`, both pass, `[D:140.02] RESOLVE: Unforgiven (chain_item=32, ability=1, spell=0)` / `[D:140.03] MODE_PROMPT: Unforgiven: move it to which battlefield? [Battlefield #0/Battlefield #1] (agent decides among 2 modes)`.
- should: the destination is named when the spell is played or the ability activated; the opponent responds knowing it.
- confidence: CERTAIN.
- also seen: `scripts/testing/choice_timing.py` over the 720 games: Ride the Wind 292 destination prompts after RESOLVE and 0 before; Unforgiven 172 / 0; Charm 95 / 0; Void Assault 50 / 0; Thrill of the Hunt ("which battlefield") 58 / 0.

### D4  Every "you may" trigger, and triggers with nothing to choose: a declined, unpayable or targetless triggered ability stays on the chain, both players get priority, and it "resolves"

_Origin: lead._

- happened: `akali__akali__s20261100__random__random`: `[D:055.03] TRIGGER: Akali, Deadly Weapon (P1) fires ability` / `[D:056.03] MAY_DECLINED: Akali, Deadly Weapon: deal damage to a unit?` / `[D:057.00] DECISION #59 (P1 — chain priority): 2 legal actions` / `[D:057.01] CHOSE: [P1] PlayReaction` / `[D:057.03] SPELL: Discipline (id=8) targets=[]` — the board of the next decision shows the chain `[top] Akali, Deadly Weapon (P1)`; P2 then receives priority (`[D:062.00] DECISION #64 (P2 — chain priority)`). Nothing to choose: `akali__diana__s20261102__random__random` `[D:024.06] MAY_NOT_OFFERED: Abandoned Hall: give a unit here +1 [M] this turn? (precondition failed at prompt time)`, both players asked, `[D:026.02] RESOLVE: Abandoned Hall (chain_item=6, ability=1, spell=0)`.
- should: the item leaves the chain at finalization; if it was the only item there is no Closed State, nobody receives priority and no [Reaction] can be played.
- confidence: CERTAIN.
- also seen: 2,279 `MAY_DECLINED` at finalization in the 720 games; 2,249 of those items later print `RESOLVE`; in 256 a Reaction was played while the declined item was on the chain. By card: Grand Duelist 221, Deceiver 199, Blade Dancer 151, Fizz, Trickster 143, Dusk Rose Lab 138, Shadow Order Disciple 136, Gloomist 121, Void Burrower 118, Tideturner 108, Star Spring 103, Fiora, Worthy 98, Abandoned Hall 96. Readers' D4 (R2-9) and D4 (R6-4) are the same defect seen from Blade Dancer and Dusk Rose Lab.

**Further evidence — reader 2, D4 (R2-9): "You may" triggers that are declined, or cannot be paid, stay on the chain and resolve as empty items**

- happened: akali__irelia__s20261105__random__random D:141-144: `[D:141.02] TRIGGER: Blade Dancer (P2) fires ability` / `[D:142.03] MAY_DECLINED: Blade Dancer: exhaust me + pay [A] to ready the chosen unit?` / `[D:143.00] DECISION #150 (P2 — chain priority): 3 legal actions` / `[D:144.00] DECISION #151 (P1 — chain priority): 1 legal actions` / `[D:144.02] RESOLVE: Blade Dancer (chain_item=35, ability=1, spell=0)`.
- should: the declined ability leaves the chain at finalization; no priority round is held for it.
- confidence: CERTAIN on the rule; effect on the game: an extra Closed-State priority round per declined trigger (a window for Reactions that the rules do not give), and "considered to have not triggered" is not honoured.
- also seen: Blade Dancer in the irelia games: 151 `MAY_DECLINED` and 238 `MAY_NOT_OFFERED` items, every one later `RESOLVE`d (0 removed). Same pattern for Hall of Legends (19 declined, 17 not offered) and for the Sand Soldier Weaponmaster "may" (207 declined, at resolution — D22 (R2-1)).

**Further evidence — reader 4, D4 (R4-3): Grand Duelist, Fiora, Worthy, Sunken Temple, Amateur Recital: a "you may" trigger that is declined stays on the chain**

- happened: akali_sb__fiora_sb__s20262104__random__random `[D:070.03] MAY_DECLINED: Grand Duelist: exhaust me to channel 1 rune?`, `[D:071.00] DECISION #80 (P2 — chain priority): 1 legal actions`, `[D:072.00] DECISION #81 (P1 — chain priority): 1 legal actions`, `[D:072.02] RESOLVE: Grand Duelist (chain_item=15, ability=1, spell=0)`. Same for `MAY_NOT_OFFERED` (cost unpayable): same game D:182.06 -> `[D:184.02] RESOLVE: Grand Duelist (chain_item=42 ...)`.
- should: the item leaves the chain at once; with nothing else on the chain there is no Closed state and neither player receives priority.
- confidence: CERTAIN (rule in terms). The window is real: both players are asked, and a player holding a [Reaction] may play it there.
- also seen: 319 `MAY_DECLINED` and 68 `MAY_NOT_OFFERED` of the two Fiora cards in the 78 games; Sunken Temple 31 declined (all games), Amateur Recital 38.

**Further evidence — reader 6, D4 (R6-4): Dusk Rose Lab, Deceiver, Star Spring (and every "you may" trigger read): an ability that is declined, or whose cost cannot be paid, stays on the chain, both players get priority, and it then "resolves"**

- happened: ezreal__leblanc__s20263013__ismcts-sims20__random, D:051-055 (`./build/riftbound --deck1 decks/ezreal.txt --deck2 decks/leblanc.txt --seed 20263013 --agent1 ismcts:sims=20 --agent2 random --trace --debug`), P1's Beginning Step, P1 has no unit at Dusk Rose Lab:
  `[D:051.07] TRIGGER: Dusk Rose Lab (P1) fires ability` / `[D:051.09] MAY_NOT_OFFERED: Dusk Rose Lab: kill a unit here to draw 1? (precondition failed at prompt time)` / `[D:052.00] DECISION #54 (P1 — chain priority): 1 legal actions` / `[D:053.00] DECISION #55 (P2 — chain priority): 3 legal actions` / `[D:053.01] CHOSE: [P2] PlayReaction` / `[D:055.02] RESOLVE: Hidden Blade (chain_item=14, ability=0, spell=1)` / `[D:055.03] EFFECT: kill LeBlanc, Fragmented (id=55)`.
  Second: azir_sb__leblanc_sb__s20262126__random__random D:072.01 `CHOSE: [P2] PlayReaction` (Ki Barrier) ... `[D:080.02] RESOLVE: Dusk Rose Lab (chain_item=18, ability=1, spell=0)`. Declined: azir__leblanc__s20263035__random__ismcts-sims20 `[D:091.03] MAY_DECLINED: Deceiver: ...` / `[D:092.00] DECISION #99 (P2 — chain priority)` / `[D:093.02] RESOLVE: Deceiver (chain_item=20, ability=1, spell=0)`.
- should: the ability leaves the chain at finalization; no priority window is opened by it; with nothing else on the chain in a Beginning Phase no card can be played.
- confidence: CERTAIN.
- also seen (script over 720 logs; an action other than Pass taken between the decline / not-offered line and the item's RESOLVE): Dusk Rose Lab not offered 254 times (171 PlayReaction, 83 reaction abilities) of 1473, declined 21 of 138; Deceiver declined 31 of 199, not offered 2 of 57; Star Spring declined 10 of 103, not offered 1 of 18. Same shape read for Shadow Order Disciple, Irresistible Faefolk, Kennen (Keeper of Balance) and Shadow Clone declines (kaisa__zed__s20261225__random__random D:201.03-202.01). Dusk Rose Lab alone opens this window in every Beginning Phase of every game it is in (1754 triggers in 86 games; it triggers only for the turn player, in the Beginning Step, and offers the kill only to a player who controls the battlefield with a unit there — those parts are right).

**Further evidence — reader 8, D4 (R8-8): An ability whose "may" is declined, or that has nothing to choose, stays on the chain and gives both players priority**

- happened: akali__teemo__s20261112__random__random, P1's Scoring Step: `[D:080.03] MAY_DECLINED: Startipped Peak: channel 1 rune exhausted?` / `[D:081.00] DECISION #94 (P1 — chain priority): 1 legal actions` / `[D:082.00] DECISION #95 (P2 — chain priority): 3 legal actions` / `[D:082.01] CHOSE: [P2] PlayReaction` (a hidden Switcheroo, resolved D:086). A second shape: akali__viktor__s20261114__random__random decision #169 = D:154, P1's Beginning Step, chain `[top] Dusk Rose Lab (P1)` (P1 has no unit there, `MAY_NOT_OFFERED`), `[D:154.01] CHOSE: [P2] PlayReaction` (hidden Facebreaker).
- should: the item is removed as it is finalized; with nothing else on the chain there is no Closed State and no window to play a [Reaction] / hidden card in that step.
- confidence: LIKELY (403.1.a / 403.4 in terms; whether a player could otherwise act in that step is the consequence).
- also seen: over Gloomist / Startipped Peak / Windsinger alone, 276 declined or not-offered items, 36 of them followed by a play or activation before the item "resolved". Dusk Rose Lab: 546 `MAY_NOT_OFFERED` in the viktor games. Tomb-Raider Barbara: 29 `TGT_NOT_OFFERED ... (no legal targets at resolve time)` items that resolve. Vex, Apathetic: see D33 (R8-12).

### D5  Combat is not a Showdown State for the action generator: spells with neither [Action] nor [Reaction] are offered and played at Focus in a combat, by the defender on the opponent's turn too

_Origin: lead._

- happened: `akali_sb__ezreal_sb__s20262103__random__random`, turn 11 (P1's turn), combat at Sigil of the Storm (`P1: Kai'Sa, Survivor(4M,exh,Acc,ATK)` / `P2: Fizz, Trickster(2M,rdy,DEF)`): `[D:136.03] INTENT: PlayCard by P2` / `[D:136.04] SPELL: Singularity (id=78) targets=[Kai'Sa, Survivor(id=29),Noxus Hopeful(id=23)]` / `[D:136.05] PAY_COST: Singularity E=6 P=2`. Singularity prints "Deal 6 to each of up to two units." and no keyword.
- should: at Focus in a combat only [Action] / [Reaction] cards and abilities, [Ambush] units and hidden cards are offered.
- confidence: CERTAIN.
- also seen: found independently by three readers (D5 (R1-1), D5 (R2-5), D5 (R3-2), kept below with their instances and counts: 6,479 combat focus decisions without the showdown state; offers of Up from the Deep 197, Singularity 156, Falling Star 134, Cull the Weak 130, Rampage 125, Perfect Execution 120, Void Assault 119, Death Mark 93 ...). Code named by the readers: `runCombat` never sets `turn.ns_state = Showdown`.

**Further evidence — reader 1, D5 (R1-1): Falling Star, Shuriken Flip (and every spell without [Action]): offered at the focus decision of a combat showdown, to either player; Flow replays too**

  CR 806.1.b ([Action] is what permits showdown play, also off-turn). CR 347.1 (in a showdown: a card "that is
  legally timed"). CR 829.1.b.2 "Playing a spell for its Flow cost does not change the timing at which it can be played".
- happened: at the focus decision of a COMBAT the board header carries no `SHOWDOWN` state
  (`| TURN 9 | MainPhase | P1's turn |` with the battlefield marked `CONTESTED  COMBAT  (combat staged)`), while a
  non-combat showdown shows `SHOWDOWN`. `generateSpellActions` / `generateTrashReplayActions`
  (`src/engine/game_engine.cpp` ~4616, ~4915) then take the `isNeutralOpen()` branch: "any spell".
  - akali__azir__s20261101__random__random, D:121: `COMBAT: BF#0 attacker=P1 defender=P2` / options
    `[0] Pass Focus [1] Play Falling Star #29[S] [2] Play Long Sword ... [3] Activate Rogue Assassin` /
    `INTENT: PlayCard by P1` / `SPELL: Falling Star (id=24)`.
  - off-turn: akali__kaisa__s20261106__random__random, D:204 (turn 14, P1's turn):
    `COMBAT: BF#0 attacker=P1 defender=P2` / P2's options `[0] Pass Focus [1] Play Falling Star #29[S]` /
    `INTENT: PlayCard by P2` / `SPELL: Falling Star (id=62)`.
  - Flow: akali__azir__s20263000__ismcts-sims20__random, D:310: `COMBAT: BF#1 attacker=P1 defender=P2` / options
    `[0] Pass Focus [1] Play Back Off [2] Play Shuriken Flip #927[S] [3] Activate Rogue Assassin` /
    `SPELL: Shuriken Flip (id=21)` / `PAY_ADD: P1 pays [3] + 1 power (any)` / `TRASH_REPLAY: P1 replayed card id=21`.
    Off-turn Flow: akali_sb__zed_sb__s20262117__random__random D:179 (P1 on P2's turn).
  - from hand in combat: akali__yasuao__s20261115__random__random D:302 (`PAY_COST: Shuriken Flip E=1 P=1`).
- should: at a combat showdown's focus only [Action] / [Reaction] cards and abilities are offered; a spell with
  neither (Falling Star, Shuriken Flip from hand or by Flow) is not, and never to the non-turn player.
- confidence: CERTAIN (CR 155, 806.1.b, 829.1.b.2 in terms).
- also seen: 1122 `INTENT: PlayCard` plays made at a focus decision over the 720 games (657 directly after a
  `COMBAT:` line); in the akali__* replays 298 focus decisions have the Neutral header against 387 with `SHOWDOWN`.
  Falling Star 49 times (23 by the non-turn player), Shuriken Flip 24 (17 Flow + 7 hand). Other lists' spells in
  the same tally (not checked by me for [Action]): Up from the Deep, Rampage, Cull the Weak, Switcheroo,
  Sabotage, Acceptable Losses, Death Mark, Dredge Up, Perfect Execution, Dragon Form.

**Further evidence — reader 2, D5 (R2-5): Combat showdowns are treated as a Neutral Open State for spells: cards without [Action] / [Reaction] are offered and played, also on the opponent's turn**

- happened: azir__diana__s20261120__random__random D:193 (`./build/riftbound --deck1 decks/azir.txt --deck2 decks/diana.txt --seed 20261120 --agent1 random --agent2 random --trace --debug`), P2's turn 11: `[D:193.01] MOVE: Diana, Lunari (id=55) -> BF#0` / `[D:193.02] COMBAT: BF#0 attacker=P2 defender=P1` / `[D:193.03] INTENT: PlayCard by P1` / `[D:193.04] SPELL: Arise! (id=29) targets=[]` / `[D:193.05] PAY_COST: Arise! E=6 P=1` and it resolves (D:202-210, four tokens). Replay text idx=203: options `[0] Pass Focus  [1] Play Arise!`; board header `| TURN 11 | MainPhase | P2's turn | D:203` without the `SHOWDOWN` tag.
  azir__teemo__s20261130__random__random idx=372: `2 options: [0] Pass Focus [1] Play Guards! #475[S]`, `[D:343.04] INTENT: PlayCard by P1` / `[D:343.06] PAY_COST: Guards! E=3 P=0` (from hand, drawn that turn at D:328.10).
- should: in a showdown only [Action] / [Reaction] cards (and facedown cards, CR 811.6) are offered.
- confidence: CERTAIN. Scope measured over all 720 games: of 16,231 "Pass Focus" decisions, the 9,752 in non-combat showdowns (trace `SHOWDOWN:`) carry the `SHOWDOWN` header and offer only Action / Reaction cards; the 6,479 in combat showdowns (trace `COMBAT:`) have no `SHOWDOWN` header and offer every spell (`generateSpellActions` takes its `isNeutralOpen()` branch; the intent is logged as `PlayCard`, not `PlayActionCard`).
- also seen: 36 such plays of my four cards alone (Arise! 14, Guards! 15 incl. 4 legal facedown ones, Charm 4, Decree of Unity 4), e.g. azir__irelia__s20261123 D:240.03, leblanc__azir__s20261245 D:511.05, vex_sb__irelia_sb__s20262339 D:295.07 (Charm, opponent's turn). Other lists' cards offered the same way (counts of offers): Singularity 156, Falling Star 138, Cull the Weak 133, Rampage 128, Void Assault 119, Mirror Image 70, Sabotage 69, Void Rush 35, Time Warp 35, Flow spells (Up from the Deep 209, Perfect Execution 129, Death Mark 97, Dragon Form 88, ...).

**Further evidence — reader 3, D5 (R3-2): A combat showdown is not treated as a Showdown State: spells without [Action]/[Reaction] are playable in combat, by the defender on the opponent's turn too**

  spell can be played during an Open State outside of Showdowns on its controller's turn"), CR 308.1 / 308.1.a
  ("If a Showdown or Combat is in progress, the turn is in a Showdown State. Only cards and abilities with the
  Action or Reaction keywords can be played"), CR 343.1.a, CR 358.4.
- happened: yi_rebuild_sb__diana_sb__s20262390__random__random, decision #164 (D:145), board
  `Seat of Power (P2's) [Ctrl:P1] CONTESTED COMBAT (combat staged)`, options `[0] Pass Focus [1] Play Singularity #105[S] ...`;
  `[D:145.04] INTENT: PlayCard by P2` / `[D:145.05] SPELL: Singularity (id=92) targets=[]` / `[D:145.06] PAY_COST: Singularity E=6 P=2`.
  Defender on the opponent's turn: akali_sb__ezreal_sb__s20262103__random__random D:136 (P1's turn)
  `[D:136.03] INTENT: PlayCard by P2` / `[D:136.04] SPELL: Singularity (id=78) targets=[Kai'Sa, Survivor(id=29),Noxus Hopeful(id=23)]`.
  From the trash by Flow, defender, opponent's turn, the tokens join the defence:
  akali__khazix__s20263031__random__ismcts-sims20 D:158 `SPELL: Up from the Deep (id=77)` /
  `TRASH_REPLAY: P2 replayed card id=77 for [3]+power` ... `[D:162.04] DAMAGE_STEP: BF#0 ATK=[Akali, Deadly Weapon(6M)] DEF=[Tentacle(1M), Tentacle(1M,STUNNED), Tentacle(1M)]`.
  Turn to Dust by the non-turn player: yasuao_sb__diana_sb__s20262372__random__random D:110.
  Scan of the replay texts (decisions offering `Pass Focus` together with `Play <card>` whose def has neither
  keyword, units with [Ambush] and hidden cards aside): Up from the Deep 197 offers, Singularity 156,
  Turn to Dust 15; chosen 69 / 7 / 6 times (10 / 1 / 1 of them by the non-turn player).
  Code: `runCombat` (game_engine.cpp:5282) never sets `turn.ns_state = Showdown` (only `runShowdown` does,
  :5127), so `generateSpellActions` (:4619) sees `isNeutralOpen()` and allows "any spell"; the trace has
  2314 `COMBAT:` lines and no `SHOWDOWN: ... (combat)` line.
- should: during combat only [Action] / [Reaction] cards (and [Ambush] units, hidden cards with their
  granted [Reaction]) are offered; nothing else, and nothing without [Reaction] to a player on the other
  player's turn outside Focus.
- confidence: CERTAIN.
- also seen (other lists, same scan): Falling Star 134 offers, Cull the Weak 130, Rampage 125, Perfect
  Execution 120, Void Assault 119, Death Mark 93, Dragon Form 77, Shuriken Flip 72, Mirror Image 70, Sabotage
  66, Charm 57, Dredge Up 52, Void Rush 34, Time Warp 34, Onslaught 29, Arise! 24, Deadly Flourish 24, Piercing
  Light 24, ... Non-combat showdowns gate correctly (diana__azir__s20261137__random__random decision #65:
  only Gust / Moonfall / Acceptable Losses / Scorn offered).

**Further evidence — reader 4, D5 (R4-5): Mobilize, Rampage: playable in a combat showdown, also by the player whose turn it is not**

- happened: azir_sb__fiora_sb__s20262122__random__random, P1's turn, board `Sunken Temple [Ctrl:P2] CONTESTED COMBAT (combat staged)`, header `| TURN 4 | MainPhase | P1's turn | D:32` (no `SHOWDOWN`), P2's options `[0] Pass Focus [1] Play Salvage #224[S] [2] Play Mobilize #134[S]`, `[D:028.03] INTENT: PlayCard by P2`, `[D:028.04] SPELL: Mobilize (id=67) targets=[]`, `[D:028.05] PAY_COST: Mobilize E=2 P=0`. akali_sb__fiora_sb__s20262104__random__random idx 227 (own turn, combat): `[D:224.05] SPELL: Rampage (id=70)` ... `[D:232.02] RESOLVE: Rampage`, then the damage step D:232.08. teemo__fiora__s20261320__random__random `[D:156.03] COMBAT: BF#1 attacker=P1 defender=P2`, `[D:156.04] INTENT: PlayCard by P2`, `[D:156.05] SPELL: Rampage (id=75)`.
- should: in a showdown only [Action] / [Reaction] cards are offered; a spell with neither only in a Neutral Open state on its controller's turn.
- confidence: CERTAIN. Cause visible in the trace: in focus windows of a staged combat the state is still Neutral (no `SHOWDOWN` in the header), and `generateSpellActions` allows "any spell" when `isNeutralOpen()`.
- also seen: script, fiora games: in `Pass Focus` decisions without the `SHOWDOWN` flag Mobilize is listed 18 times (9 on the opponent's turn) and Rampage 36 (14); in `Pass Focus` decisions with the flag (non-combat showdowns) 0 times in 55 where one of them was in hand. Over all 720 games Rampage is listed at least 27 times this way; other lists' keyword-less spells will show the same.

**Further evidence — reader 5, D5 (R5-2): Engine (Deadly Flourish, Perfect Execution, Rocket Barrage, Temporal Breach, Time Warp, Progress Day, Sprite Burst, ...): spells without [Action] / [Reaction] are offered and played during COMBAT showdowns, by either player**

- happened: kaisa__khazix__s20263024__ismcts-sims20__random, P1 attacks at Star Spring (`[D:223.04] COMBAT: BF#1 attacker=P1 defender=P2`); decision #251 (board header `TURN 13 | MainPhase | P1's turn | D:251`, battlefield row `CONTESTED  COMBAT  (combat staged)`) offers
  `[0] Pass Focus   [1] Play Rocket Barrage #400[S]   [2] Play Falling Star #29[S]   [3] Play Perfect Execution #799[S]`
  and `[D:225.06] INTENT: PlayCard by P1` / `[D:225.07] SPELL: Rocket Barrage (id=40) targets=[]` follows.
  On the OPPONENT's turn: akali__kaisa__s20261106__random__random decision #214 (P1's turn, combat at Void Gate): P2 is offered `[0] Pass Focus  [1] Play Falling Star #29[S]` and plays it (`[D:204.05] INTENT: PlayCard by P2`).
  Over all 720 games: 2360 focus decisions in combats (header without the SHOWDOWN flag) against 2801 in non-combat showdowns (flag `SHOWDOWN`); a spell with neither keyword was offered in the combat ones more than 1,400 times (Up from the Deep 192, Rampage 123, Cull the Weak 128, Falling Star 128, Perfect Execution 114, Rocket Barrage 21, Temporal Breach 31, Time Warp 33, Progress Day 18, Sprite Burst 16, Deadly Flourish 20, Singularity 6, Turn to Dust 9 ...) and chosen 606 times; in non-combat showdowns 0 times. Units and non-[Action] abilities are not offered.
  Code: `generateSpellActions` allows any spell when `state_.turn.isNeutralOpen()`; during a combat the state is still Neutral/Open (the header shows no SHOWDOWN), so the [Action] gate is never reached.
- should: in a combat showdown only [Action] / [Reaction] cards (and hidden cards) are offered, to both players.
- confidence: CERTAIN (310.1.a, 806.1.b).
- also seen: Time Warp offered to the non-turn player in a combat: kaisa__akali__s20261208__random__random decision #338.

**Further evidence — reader 9, D5 (R9-3): Combat showdowns: spells without [Action] / [Reaction] are offered and played by both players, from hand and from the trash (Death Mark, Onslaught, Piercing Light)**

- happened: ezreal_sb__zed_sb__s20262171__random__random, idx 322 / D:305, P1's turn 19, combat in progress, P2 has Focus:
  options `[0] Pass Focus  [1] Play Death Mark #931[S]  [2] Activate Zed, Without a Sound ...`, chosen `Play Death Mark`; `[D:305.04] INTENT: PlayCard by P2` / `SPELL: Death Mark (id=71)` / `PAY_ADD: P2 pays [1] + 2 power (any): exhausted 1, recycled 2` / `TRASH_REPLAY: P2 replayed card id=71 for [1]+power`.
  teemo__yi_rebuild__s20261332__random__random, idx 335 / D:303: `COMBAT: BF#0 attacker=P1 defender=P2` / `INTENT: PlayCard by P2` / `SPELL: Onslaught (id=91)` / `TRASH_REPLAY: P2 replayed card id=91 for [4]+power`; options were `[0] Pass Focus [1] Play Onslaught #868[S]`.
- should: neither card has [Action] or [Reaction]; not playable with Focus in a showdown, on either player's turn.
- confidence: CERTAIN. In the replay text the board header of every combat Pass Focus decision carries no `SHOWDOWN` flag (373 of 373 in a 1-in-6 sample of games, units marked ATK/DEF), while non-combat showdowns carry it (497 of 497); `generateTrashReplayActions` and the spell generator allow any spell when `state_.turn.isNeutralOpen()`. In the yasuao / yi_rebuild / zed games: Death Mark offered at 79 combat Focus decisions (chosen 34), Onslaught 29 (10), Piercing Light 1 (1) — and at 0 flagged showdowns.
- also seen (other readers' cards, same defect): by the non-turn player in a combat, from hand: Falling Star 23, Cull the Weak 13 (akali_sb__reksai_sb__s20262110__random__random D:104), Sabotage 13, Rampage 12, Charm 11, Mobilize 5 (azir_sb__fiora_sb__s20262122__random__random D:028), Progress Day, Time Warp; from the trash for Flow: Perfect Execution 10, Dredge Up 9, Up from the Deep 7, Shuriken Flip 2. Permanents and non-[Action] abilities (Unforgiven, Last Rites' Equip) are NOT offered there.

### D6  Any non-Reaction spell: offered as a plain play at a chain-priority decision; choosing it does nothing and the same decision is asked again

_Origin: lead._

- happened: `akali__zed__s20261117__random__random`, P2's turn: `[D:052.07] TRIGGER: Sigil of the Storm (P2) fires ability`, then with the chain `[top] Sigil of the Storm (P2)` the options are `[0] Pass Priority [1] Play Death Mark #931[S]` (Death Mark has neither keyword): `[D:053.00] DECISION #64 (P2 — chain priority): 2 legal actions` / `[D:053.01] CHOSE: [P2] PlayCard` / `[D:054.00] DECISION #65 (P2 — chain priority): 2 legal actions` / `[D:054.01] CHOSE: [P2] PlayCard` / `[D:055.00] DECISION #66 (P2 — chain priority): 2 legal actions` / `[D:055.01] CHOSE: [P2] PlayCard` / `[D:056.01] CHOSE: [P2] PassPriority`. No `INTENT`, `SPELL` or `PAY_COST` line follows a `PlayCard`; the card stays in hand. Eight times in a row with the non-turn player: `diana_sb__ezreal_sb__s20262139__random__random` D:272-280 (`Play Ride the Wind #173[S]` under `[top] Deadly Flourish (P2)`).
- should: the option is not offered. As it is, an agent that prefers that option never leaves the decision (the decision index advances each time).
- confidence: CERTAIN.
- also seen: 52 such no-op choices in 18 of the 1,116 games; always under a triggered or delayed ability that was queued by a conquer, a showdown start, a unit becoming Mighty or a death during a resolution (chain top: Grand Duelist 20, Diana, Lunari 13, Deadly Flourish 8, Fiora, Worthy 7, Sigil of the Storm 3, Seat of Power 1). Offered plays: Ride the Wind, Rampage, Turn to Dust, Salvage, Death Mark, Singularity, Up from the Deep, Moonfall, Mobilize, Flurry of Blades, Stupefy, Mirror Image, Falling Star, Challenge, Guerilla Warfare, Consult the Past, Guards!.

**Further evidence — reader 4, D6 (R4-4): Mobilize, Rampage, Challenge (and Salvage): listed in a chain-priority window, and choosing them does nothing**

- happened: fiora_sb__leblanc_sb__s20262180__random__random: a Grand Duelist trigger fired in the Beginning Step (`[D:054.03]`) is finalized in the Main Phase (`[D:054.12]`, `[D:055.03] MAY_ACCEPTED`); the board header of that decision has no `CLOSED` although the chain shows `[top] Grand Duelist (P1)`; options `[0] Pass Priority [1] Play Mobilize #134[S] [2] Play Salvage #224[S]`; `[D:056.00] DECISION #56 (P1 — chain priority): 3 legal actions`, `[D:056.01] CHOSE: [P1] PlayCard`, `[D:057.00] DECISION #57 (P1 — chain priority): 3 legal actions`, `[D:057.01] CHOSE: [P1] PlayCard`, `[D:058.01] CHOSE: [P1] PlayCard`, `[D:059.01] CHOSE: [P1] PassPriority` — no `SPELL:` line, nothing paid. teemo__fiora__s20261320__random__random `[D:154.00] DECISION #167 (P2 — chain priority): 2 legal actions` (options `Pass Priority`, `Play Rampage #870[S]`, on P1's turn), `[D:154.01] CHOSE: [P2] PlayCard`, `[D:155.00] DECISION #168 (P2 — chain priority): 2 legal actions`.
- should: with an item on the chain the state is Closed: only [Reaction] cards are options; an option that is listed must do what it says.
- confidence: CERTAIN (an option offered wrongly; the intent is dropped and the same decision asked again).
- also seen: script, fiora games: `Play Mobilize` 4, `Play Rampage` 2, `Play Challenge` 3 in decisions that list `Pass Priority` (fiora__reksai__s20261182__random__random idx 286 for Challenge).

### D7  A triggered or delayed ability queued during a showdown is left pending: Focus is passed, the showdown closes and combat damage is dealt with the item still on the chain

_Origin: lead._

- happened: `diana_sb__ezreal_sb__s20262139__random__random`, combat at Targon's Peak: `[D:270.02] RESOLVE: Deadly Flourish (chain_item=59, ability=0, spell=1)` / `[D:270.05] KILL: Ravenbloom Student (id=4, 2M, dmg=3)` / `[D:270.06] DELAYED_ABILITY: firing for Deadly Flourish`; the next two decisions (#287, #288) are Focus decisions — options `[0] Pass Focus [1] Play Ride the Wind #173[S]` — with `[top] Deadly Flourish (P2)` on the chain; then `[D:270.07] DAMAGE_STEP: BF#0 ATK=[Thousand-Tailed Watcher(7M)] DEF=[Tentacle(1M), Hwei, Brooding Painter(5M)]` and both damage assignments, still with that item on the chain; only then `[D:271.00] DECISION #291 (P2 — chain priority)` and `[D:280.02] RESOLVE: Deadly Flourish (chain_item=60, ability=1, spell=0)` / `[D:280.03] TOKEN: created Gold (0M, id=113) for P2`. A whole showdown inside a pending conquer ability: `akali__zed__s20261117__random__random` `[D:052.07] TRIGGER: Sigil of the Storm (P2) fires ability` / `[D:052.08] SHOWDOWN: BF#1 (non-combat)` / `[D:052.09] CONQUER: P2 conquers BF#1 (score was 1)` and the ability resolves at D:057.
- should: the pending item is finalized, both players receive priority, it resolves; then the showdown continues in an Open State.
- confidence: CERTAIN.
- also seen: in the 1,116 games, a `Pass Focus` decision is offered with an unresolved item on the chain in 125 games (chain top: Diana, Lunari 124 decisions, Grand Duelist 74, Deadly Flourish 25, Zaun Warrens 10, Targon's Peak 6, Blade Dancer 4, Scuttle Crab 4, Seat of Power 4 ...), and a combat damage assignment in 18 games (chain top: Deadly Flourish 20 decisions, Grand Duelist 14, Scuttle Crab 2, Pyke, Returned 2; the 27 further games with Imperial Decree on top are D13). Reader finding D7 (R3-3) is this defect for Diana, Lunari, whose "When a showdown begins here" ability is always asked after the showdown.

**Further evidence — reader 3, D7 (R3-3): Diana, Lunari: the showdown-start trigger stays pending while Focus is passed; its choices and resolution come after the showdown has closed (after the Conquer)**

  taken in a Showdown OPEN state; pending items are finalized first), CR 359.3.b, CR 383.3 (cost and "may" at
  finalization), CR 466.2.b-d.
- happened: diana__azir__s20261137__random__random: `[D:064.03] SHOWDOWN: BF#0 (non-combat)` /
  `[D:064.04] TRIGGER: Diana, Lunari (P1) fires ability`; then decision #65 `[MainPhase] SHOWDOWN`
  `[0] Pass Focus [1] Play Gust [2] Play Moonfall [3] Play Acceptable Losses [4] Activate Scorn of the Moon`
  (Action spells offered with the trigger unfinalized on the chain), both players pass; then
  `[D:064.05] CONQUER: P1 conquers BF#0` / `[D:064.06] SCORE: P1 +1 via Conquer -> 1` /
  `[D:064.07] TRIGGER: Targon's Peak` and only then `[D:064.08] FINALIZE-CHOICES: Diana, Lunari` /
  `[D:064.09] MAY_PROMPT: Diana Lunari: pay [1] to Predict + reveal?` ... `[D:065.04] PAY_ADD: P1 pays [1]`.
  A card played first: diana__diana__s20261138__random__random `[D:301.06] SHOWDOWN` / `[D:301.07] TRIGGER: Diana, Lunari (P2)` /
  `[D:301.08] INTENT: PlayActionCard by P2` / `SPELL: Moonfall` (chain_item=76, finalized and paid D:304) /
  `[D:307.02] FINALIZE-CHOICES: Diana, Lunari (chain_item=75 ...)`.
  Of the 48 triggers: in 25 the Conquer precedes Diana's finalization, in 12 a card play does.
- should: as the showdown begins the ability is finalized ("may", the [1]) and resolves — Predict, reveal,
  draw — before the first player may act with Focus and before the showdown can close.
- confidence: CERTAIN on the order; consequence for the list: the [1] is asked when no showdown is under
  way any more, so Scorn's Energy (`showdownUnderway`) can never pay it there (see D30 (R3-4)), and the card drawn
  cannot be used in that showdown.
- also seen: `runShowdownLoop` (game_engine.cpp:5253) gives Focus without running the chain first; `runCombat`
  does drain it (:5338), but Diana does not trigger there (D29 (R3-1)).

### D8  Star-Crossed, Switcheroo, Facebreaker, Defiant Dance: offered, paid for and put on the chain without a legal choice for each target

_Origin: lead._

- happened: `diana__khazix__s20263009__ismcts-sims20__random`, turn 3: P1 controls no unit (the board lists only P1's runes; P2 has Kha'Zix, Mutating Horror); `[D:017.01] CHOSE: [P1] PlayCard Star-Crossed` / `[D:017.03] SPELL: Star-Crossed (id=29) targets=[]` / `[D:017.05] TGT_PAIR_NOT_OFFERED: Star-Crossed (no legal first targets at resolve time)` / `[D:017.06] PAY_COST: Star-Crossed E=3 P=1`; the spell resolves with no effect.
- should: not offered without a legal friendly AND enemy unit (two units at one battlefield for Switcheroo, and so on).
- confidence: CERTAIN.
- also seen: in the 720 games at finalization: Switcheroo 141 (no second unit at that battlefield), Star-Crossed 85 (no first target) + 57 (no second), Facebreaker 19, Defiant Dance 15 + 8. Readers' D8 (R3-7) (with Star-Crossed returning only the player's own unit, and Fizz replaying targetless spells), D17 (R1-5) (Shuriken Flip) and D28 (R2-10) (Deathgrip, as a question) are the same gate.

**Further evidence — reader 3, D8 (R3-7): Spells are put on the chain without a valid choice for every target: Switcheroo with one unit, Star-Crossed without an enemy unit, Fizz replaying Gust / Decree of Insight / Hard Bargain with nothing to choose**

  targets"), CR 420.2.a, CR 420.3.c ("If there are no eligible cards to Play ... nothing happens"), CR
  811.1.d for a play from facedown.
- happened: Switcheroo ("two units at the same battlefield"): diana__vex__s20263054__random__ismcts-sims20
  `[D:043.08] TGT_PAIR_PROMPT_A: Switcheroo [Vex, Apathetic]` / `[D:044.04] TGT_PAIR_NOT_OFFERED: Switcheroo (no legal second targets after A picked)` /
  `[D:044.05] PAY_COST: Switcheroo E=2 P=2` / `[D:050.02] RESOLVE: Switcheroo` (nothing) — 141 such lines in
  the logs. Star-Crossed ("a friendly unit and an enemy unit"): diana__azir__s20261137__random__random
  `[D:023.04] TGT_PAIR_NOT_OFFERED: Star-Crossed (no legal second targets after A picked)` /
  `[D:023.05] PAY_COST: Star-Crossed E=3 P=1` / `[D:032.03] EFFECT: bounce Tideturner (id=12) to hand` — the
  spell returned only the player's own unit; 57 such lines. Fizz: diana__ezreal__s20261139__random__random
  (no unit at any battlefield) `[D:223.03] PAY_COST: Gust E=0 P=0` / `[D:225.03] TGT_NOT_OFFERED: Gust (no legal targets at resolve time)` /
  `[D:225.04] TRIGGER: Abandoned Hall (P2) fires ability`; diana__fiora__s20261140__random__random
  `[D:202.04] FIZZ: played Hard Bargain from trash` / `[D:204.03] HARD BARGAIN: nothing on chain to counter — no-op` /
  `[D:204.04] TRIGGER: Ravenbloom Student`; diana_sb__vex_sb__s20262149__random__random D:268-270 (Decree of Insight).
- should: the spell is not offered (hand / facedown), respectively is not an eligible card for Fizz: it stays
  in the trash (it is not recycled) and no "when you play a spell" ability triggers.
- confidence: CERTAIN (CR 355.8).
- also seen: Defiant Dance `no legal second targets after A picked` x8 (other lists). Fizz's `playable_spells`
  filters on Energy <= 3 and payable Power only, not on `hasLegalTargets`.

### D9  Every payment that is not a card's base cost: the engine chooses which runes are exhausted and recycled

_Origin: lead._

- happened: `akali__diana__s20261102__random__random`: Akali, Deadly Weapon's ability chooses Vex, Apathetic ([Deflect]): `[D:177.03] TGT_PICKED: Akali, Deadly Weapon: choose a unit = Vex, Apathetic (id=93)` / `[D:177.04] PAY_ADD: P1 pays [0] + 1 power (any): exhausted 0, recycled 1` / `[D:177.05] DEFLECT: P1 pays 1 [A] to choose Vex, Apathetic` with no rune prompt; P1's runes before `Calm Rune(rdy)x4 Fury Rune(exh)x2 Fury Rune(rdy)x2`, after `Calm Rune(rdy)x4 Fury Rune(exh) Fury Rune(rdy)x2`. A card's base cost does ask (`DECISION ... pick rune to exhaust for energy`, `pick rune to recycle for power`).
- should: the paying player chooses the runes — with [A] also which Domain leaves the board, which decides what the rest of the turn can pay.
- confidence: LIKELY (every payment made is legal; the choice is withheld. `engine/resource_payment.h` documents the fixed order: floating first, Energy from ready runes that cannot pay the Power, Power from exhausted runes before ready ones, lowest id first).
- also seen: all 5,217 `PAY_ADD` lines of the 720 games (Deflect, Equip, Empower, legend and unit abilities, Hide, Accelerate, Repeat, Flow). Reader finding D9 (R1-9) is the same.

**Further evidence — reader 1, D9 (R1-9): Empower, Equip, Hide, Accelerate, Flow payments: the engine picks the runes; the player is not asked**

  does ask (`DECISION ... pick rune to exhaust for energy [n options]`, `pick rune to recycle for power`).
- happened: akali__fiora__s20261104__random__random, D:225: `CHOSE: [P1] ActivateAbility (src=Akali, Deadly
  Weapon)` / `PAY_ADD: P1 pays [2] + 1 power: exhausted 2, recycled 1`, next line already `DECISION #241 (P1 — chain
  priority)`. Board before: Calm exh 1 / rdy 5, Fury exh 1 / rdy 3; after: Calm exh 3 / rdy 3, Fury rdy 3 (the
  engine exhausted two Calm and recycled the exhausted Fury — correct domain, but its choice).
- should: the player chooses which runes are exhausted and which is recycled (it decides what is left for the
  rest of the turn; with "(any)" Power — Hide, Flow, the legend's Empower — also which Domain leaves the board).
- confidence: LIKELY (no rule gives the choice to anyone but the paying player; listed by `CLAUDE.md` as a
  deliberate split between `payCardCost` and `resource::pay`).
- also seen: every `PAY_ADD` line (193 Hide payments of my four [Hidden] cards, 41 Akali Empowers, 67 legend
  Empowers, 43 Equips, 54 Flow replays, Kai'Sa's Accelerate at akali__azir__s20261101 D:227).

### D10  Equipment whose bearer dies is not recalled by the next cleanup: it lies unattached at the battlefield through priority windows, once through a Main Phase decision

_Origin: lead._

- happened: second game set, `fiora__vex__s20264185__random__random`: `[D:244.15] EFFECT: kill Vex, Apathetic (id=55)` / `[D:244.16] EFFECT: unattach Boots of Swiftness (id=81)`; two priority decisions and `[D:246.02] RESOLVE: Grand Duelist (chain_item=53, ability=1, spell=0)` later the chain is empty and P1 is asked `[D:247.00] DECISION #266 (P1): 4 legal actions` with the Boots still at Risen Altar (the board of that decision lists them nowhere: 79 of the 80 cards are accounted for); they come back only after P1's next action: `[D:247.03] MOVE: Elder Dragon (id=10) -> Base(P1)` / `[D:247.04] RECALL (CR 323.6): Boots of Swiftness (id=81)`.
- should: recalled in the cleanup that follows the kill, before anyone receives priority.
- confidence: LIKELY (323.6 has no Open-State condition; 323.5 has one and 323.6 does not).
- also seen: 670 unattach-then-recall pairs in the 1,116 games: 417 with no decision in between, 252 with 2 to 22 chain-priority or prompt decisions in between, 1 with a Main Phase decision (the game above).

### D11  Hidden information in log lines whose audience is both players

_Origin: lead._

- happened (the binary prints no audience, so this is the emitting statement read in the source, with a traced line for each):
  - (a) `GameEngine::queryAgent`, `RIFT_TRACE(events_, "CHOSE: [" ... choice_str)`: `akali__azir__s20261101__random__random` `[D:048.01] CHOSE: [P1] HideCard Mischievous Marai bf=1` — the engine's own next lines are `[D:048.03] HIDE: P1 hides a card at BF#1` (public) and `[D:048.04] HIDE: Mischievous Marai at BF#1 by P1` (private). 648 such lines in the 720 games.
  - (b) the mulligan: `RIFT_TRACE(events_, "CHOSE: " + who + "Mulligan " ...)`: same game `[D:003.01] CHOSE: [P2] Mulligan 1 [Guards!]`, while `[D:003.02] MULLIGAN: P2 set aside Guards! (id=76)` is private.
  - (c) the Hide-cost prompt (asked when Swift Scout offers [1] instead of [A]): the `DECISION` line is a plain `logTrace`: `akali__teemo__s20261112__random__random` D:196 `DECISION #227 (P2): Hide cost for Temporal Breach [2 options]`. 73 lines.
  - (d) `TARGET (finalize): <source> -> <object>` is `RIFT_TRACE`: `ezreal__teemo__s20261166__random__random` `[D:335.01] TARGET (finalize): Pack of Wonders -> Wages of Pain` — Wages of Pain is a facedown card being returned to its owner's hand.
  - (e) Abandon's [Predict]: `src/cards/spells/0693_abandon.cpp` writes `RIFT_TRACE(ctx.events, "PREDICT: " + n + " -> bottom of deck (recycled)")` and `" -> top of deck (kept)"`: `vex_sb__zed_sb__s20262351__random__random` `[D:064.03] PREDICT: Star-Crossed -> bottom of deck (recycled)`. The executor's own Predict line is `RIFT_TRACE_PRIVATE`.
  - (f) CODE-ONLY: `Card::pickTarget` writes `TGT_PROMPT` with `promptAudience(...)` but `TGT_PICKED` with plain `RIFT_TRACE`; in the traces every hidden card picked this way became public by the effect itself.
- should: each of these lines for the acting player only (or without the card's name).
- confidence: CERTAIN for (a)-(e) as statements in the source; what a seat-filtered consumer receives could not be replayed.
- also seen: `scripts/testing/hidden_info_scan.py` lists every line kind that names a card hidden right after the line; the remaining kinds are plays, reveals and discards.

**Further evidence — reader 8, D11 (R8-9): Abandon: the Predict card is named in a public log line**

- happened: kaisa__vex__s20263055__random__ismcts-sims20 `[TRC] [D:115.03] PREDICT: Rebuke -> bottom of deck (recycled)`; the class writes both lines with `RIFT_TRACE` (audience: both): `"PREDICT: " + n + " -> bottom of deck (recycled)"` and `" -> top of deck (kept)"`. `EffectExecutor::predict` writes the same information with `RIFT_TRACE_PRIVATE` (`effect_executor.cpp:2326`).
- should: private to the player who looked.
- confidence: CERTAIN (hidden information in a line every seat may read; the audience is read from the code, the trace file does not show audiences).
- also seen: 19 Predicts by Abandon (12 recycled, 7 kept).

### D12  Deck validator: no rule for [Unique]

_Origin: lead._

- happened: `src/rules/deck_validator.cpp` has no check for `Keyword::Unique`; three registered cards print it (Forgefire Cape #507, Rabadon's Deathcrown #508, Shurelya's Requiem #509). None of the 18 lists holds one.
- should: two copies of a [Unique] card are refused.
- confidence: CODE-ONLY (a list with two copies of one of the three cards would show it).

### D13  Combat damage: the attacker's damage is dealt before the defender has assigned

_Origin: lead._

- happened: `akali__viktor__s20261114__random__random`, combat at BF#1 with Imperial Decree ("When any unit takes damage this turn, kill it.") in effect: `[D:347.06] ASSIGN_DAMAGE: P2 assigns 3 damage:` / `[D:347.07]   -> Astral Heron takes 3 (was 0/9M)` / `[D:347.08] DELAYED_ABILITY: firing for Imperial Decree` — the ability that watches damage being taken has fired — and only then is P1 asked to assign (decision #387, with `[top] Imperial Decree (P2)` on the chain): `[D:347.09] ASSIGN_DAMAGE: P1 assigns 13 damage:` / `[D:347.10]   -> Sprite takes 13 (was 0/3M)` / `[D:347.11] DELAYED_ABILITY: firing for Imperial Decree`.
- should: both players assign, then all of it is dealt at once; abilities that trigger on that damage are put on the chain together, in turn order (CR 383.3.d.1).
- confidence: LIKELY (the rule is in terms; the only thing seen to differ is when and in which order those abilities are queued — no changed kill).
- also seen: 27 games of the 1,116 show the second assignment asked with an Imperial Decree item already on the chain; and on 2,149 damage-assignment decisions in 939 games the board the second player is shown already carries the first player's damage as marked damage (a unit with damage equal to or above its Might).

### D14  Long Sword, Sterak's Gage ([Quick-Draw]): the attach is done inline as the gear lands, not as a triggered ability on the chain

_Origin: reader 1, R1-2._

  "Quick-Draw is a Triggered Ability keyword", 819.1.d "functionally short for [Reaction] and 'When you play this,
  attach it to a Unit you control.'"; triggered abilities go on the chain and both players get priority.
- happened: akali__azir__s20263000__ismcts-sims20__random, D:304-309:
  `PLAY: Long Sword (id=27, Gear, cost=2E)` / `PAY_COST: Long Sword E=2 P=1` / ... /
  `DECISION #340 (P1): choose a unit to attach Long Sword to [3 options]` /
  `QUICK_DRAW: auto-attaching Long Sword to Akali, Deadly Weapon` / `ATTACH: Long Sword (id=27) -> Akali, Deadly Weapon`
  and only then `DECISION #341 (P1 — chain priority)`. No `TRIGGER:` line, no chain item, no priority window
  between the gear entering and the attach (`game_engine.cpp` 3307-3344: `decideNow` + `attachGear`).
- should: the gear enters at base; its "when you play this" ability is put on the chain (unit chosen at
  finalization, CR 383.3); the opponent may respond before it attaches (a unit removed in response = mistarget,
  the gear stays unattached).
- confidence: CERTAIN (819.1 names it a triggered ability).
- also seen: all 73 `QUICK_DRAW` attaches (48 Long Sword, 25 Sterak's Gage), e.g. akali__azir__s20261101 D:193,
  akali__reksai__s20261110 D:070.

### D15  Long Sword, Sterak's Gage ([Equip]): the Equip ability attaches at activation — no chain item, no priority

_Origin: reader 1, R1-3._

  keyword", 818.1.b.1 "Equip's choice is a Target", 818.1.c.1 (triggers caused by paying it go "on the chain above
  this ability"); CR 377.3 "Activated Abilities use the chain."
- happened: akali__azir__s20263000__ismcts-sims20__random, D:338:
  `CHOSE: [P1] ActivateAbility (src=Sterak's Gage) tgt=[Noxus Hopeful]` / `INTENT: ActivateAbility by P1` /
  `PAY_ADD: P1 pays [0] + 1 power: exhausted 0, recycled 1` / `ATTACH: Sterak's Gage (id=35) -> Noxus Hopeful` /
  `DECISION #379 (P1): 4 legal actions` (a main-phase decision; no `chain priority` decision for either player).
- should: the ability goes on the chain, both players receive priority, it attaches on resolution (the unit can
  be answered in response).
- confidence: CERTAIN (377.3 + 818.1). Compare the Empower ability, which does use the chain (same game D:016-018).
- also seen: all 43 Equip activations of the two gears (31 + 12); the same shape for other lists' Equipment in my
  games (B.F. Sword, azir ismcts D:243).

**Further evidence — reader 2, D15 (R2-2): Equip (all Equipment): the activation attaches at once, is never a chain item, and does not choose its unit — Irelia, Fervent and Blade Dancer do not trigger**

- happened: irelia__mel__s20261199__random__random D:448 (`./build/riftbound --deck1 decks/irelia.txt --deck2 decks/mel.txt --seed 20261199 --agent1 random --agent2 random --trace --debug`):
  `[D:448.01] CHOSE: [P1] ActivateAbility (src=Boots of Swiftness) tgt=[Irelia, Fervent]` / `[D:448.03] PAY_ADD: P1 pays [0] + 1 power: exhausted 0, recycled 1` / `[D:448.04] ATTACH: Boots of Swiftness (id=29) -> Irelia, Fervent (id=2, might_bonus=2)` / `[D:449.00] DECISION #470 (P1): 4 legal actions`. No `TRIGGER: Irelia, Fervent`, no `TRIGGER: Blade Dancer`, no chain-priority decision for either player; she then attacks at 7M (`[D:449.05] DAMAGE_STEP: BF#0 ATK=[Irelia, Fervent(7M)]`: 4 + 2 Boots + 1 Awaken ready) where 8M is due.
- should: the Equip goes on the chain with the unit as its target; "When you choose" abilities trigger above it (Irelia +1 [M]; Blade Dancer's offer); both players get priority before the gear attaches.
- confidence: CERTAIN (818.1.b.1 + 383.4.b; 377.3). The engine comment in `GameEngine::payAndQueueAbility` says "The Equip itself adds no chain item".
- also seen: irelia__akali__s20261190__random__random D:206, D:213 (Boots -> Irelia); irelia__fiora__s20261194__random__random D:257, irelia__khazix__s20261197__random__random D:254 (Edge of Night -> Irelia); every Equip of the azir list (akali__azir__s20261101 D:085 `ATTACH` directly after `PAY_ADD`). All eight Equipment on my lists.

### D16  Shuriken Flip: the enemy unit, the friendly unit and the move destination are all chosen as it resolves

_Origin: reader 1, R1-4._

  card chooses are chosen at finalization), 355.4 (move destination at finalization), 355.13 ("up to": zero is a
  choice made then). Same family as K3; this card is not in the K3 list.
- happened: akali__azir__s20263000__ismcts-sims20__random: D:195 `SPELL: Shuriken Flip (id=22) targets=[]` /
  `PAY_COST: Shuriken Flip E=1 P=1`; D:205 `RESOLVE: Shuriken Flip` / `CHOICE-REQUEST: Shuriken Flip: deal 2 to up
  to one enemy unit [2 options]`; D:206 `CHOICE-REQUEST: Shuriken Flip: move a friendly unit [4 options]`; D:207
  `CHOICE-REQUEST: Shuriken Flip: move it where? [2 options]`. Between play and resolution P1 itself played
  Discipline (D:198) with nothing of the Flip yet chosen.
- should: all three chosen as the spell is played, before its cost; Deflect paid then; the opponent responds
  knowing them (Not So Fast "that chooses a friendly unit" can never see a Flip's choice as it is).
- confidence: CERTAIN (355.4, 355.5).
- also seen: every one of the 131 plays (393 `RESOLVE:` re-entries), e.g. D:296-297, D:312-315 of the same game.

### D17  Shuriken Flip: playable with no friendly unit to move

_Origin: reader 1, R1-5._

  choices must be made for all targets." ("up to one" enemy may be zero, 355.13; "a friendly unit" may not.)
- happened: akali__irelia__s20263001__ismcts-sims20__random, D:016: P1 controls no unit (board: P1 BASE holds
  runes only, both battlefields empty); option `[6] Play Shuriken Flip #927[S]` is offered and taken:
  `SPELL: Shuriken Flip (id=23) targets=[]` / `PAY_COST: Shuriken Flip E=1 P=1` / D:020 `RESOLVE: Shuriken Flip`
  with no prompt and no effect.
- should: not offered while its controller has no friendly unit that can be chosen (it also stamps a "card
  played" for Legion / Darius / Astral Heron for 1E+1P).
- confidence: LIKELY (355.8; follows from D16 (R1-4) — with choices at finalization the play gate would see it).
- also seen: not searched further.

### D18  Astral Heron: the discount is kept for a later turn

_Origin: reader 1, R1-6._

  play your first card each turn, if I'm at a battlefield, the next card you play **this turn** costs [2][A][A]
  less." The class holds the old wording and sets `m.this_turn_only = false; // "your next card", whenever it is played`.
- happened: akali__ezreal__s20261103__random__random: turn 14 (P1) D:166 `PLAY: Astral Heron`, D:174 Discipline
  played in response, D:181 `ASTRAL HERON: first card this turn -> next card costs [2][A][A] less`; P1 plays no
  further card on turn 14. Turn 16 (P1), first card of the turn: D:200 `PLAY: Noxus Hopeful (id=32, Unit, cost=4E)`
  / `PAY_COST: Noxus Hopeful E=2 P=0` (no earlier card that turn, so [Legion] is off: the 2 is turn 14's discount).
- should: the unused discount ends with turn 14; Noxus Hopeful costs 4.
- confidence: CERTAIN (erratum text).
- also seen: the tally of Noxus Hopeful payments has 9 x E=2 and 1 x E=0 with no earlier card that turn (not each
  opened).

### D19  Astral Heron: the ability is put on the chain while the Heron is not at a battlefield

_Origin: reader 1, R1-7._

  additional conditional statement immediately after the Condition must be true in order for the Condition to be
  fulfilled. Such a conditional statement is part of the Trigger Condition and not the Effect."
- happened: akali__akali__s20261100__random__random, D:146-148 (Heron just played to base, exhausted):
  `TRIGGER: Astral Heron (P1) fires ability` / `DECISION #163 (P1 — chain priority)` / `DECISION #164 (P2 — chain
  priority)` / `RESOLVE: Astral Heron (chain_item=32, ability=1, spell=0)` with no effect line. The class tests
  `isAtBattlefield()` in `onTrigger`, not in `triggerCondition`.
- should: no trigger; the opponent gets no priority window from it (a permanent played in an Open State gives none).
- confidence: LIKELY (383.2.a.1; the outcome difference is the extra Closed-State window).
- also seen: 20 such empty resolutions in the akali__* games; over all games 396 triggers against 182 with an effect.

### D20  Falling Star: kills between its two instructions, so a death replaced after the first hit is died again on the second

_Origin: reader 1, R1-8._

  occur", 323.4 (units with lethal damage are killed in the cleanup), 158.3.a.
- happened: kaisa__yasuao__s20261223__random__random, D:112 (both halves on Tideturner, its controller has a
  Zhonya's Hourglass): `RESOLVE: Falling Star` / `EFFECT: deal 3 damage to Tideturner (id=66, was 0/2M)` /
  `EFFECT: kill Tideturner (id=66)` / `ZHONYA'S HOURGLASS: kill self instead of Tideturner — heal/exhaust/recall it`
  / `EFFECT: kill Zhonya's Hourglass (id=86)` / `EFFECT: deal 3 damage to Tideturner (id=66, was 0/2M)` /
  `EFFECT: kill Tideturner (id=66)`. The class kills inside `hit()` after each half.
- should: 6 damage marked, the spell finishes, the cleanup finds lethal damage once, the Hourglass replaces that
  one death: Tideturner is healed and recalled and survives.
- confidence: CERTAIN (321 / 323.4).
- also seen: same inline kill with no outcome change at akali__akali__s20261100 D:109; Mischievous Marai's class
  kills inline too (single instruction: no outcome change seen).

### D21  Counter spells (Defy, Not So Fast, Hard Bargain, Riposte, Repulse, Rebuttal, Abandon, Wind Wall): the spell to counter is never chosen — only the top item of the chain can be countered

_Origin: reader 1, R1-10._

  ability that chooses a friendly unit or gear."; CR 355.9.a (a target may be any "finalized spell or ability on
  the chain").
- happened: class only — `Defy::counterable` and `NotSoFast::choosesFriendlyUnitOrGear` read
  `state.chain.items.back()`; `hasLegalTargets` uses the same test, so the card is not offered otherwise.
- should: offered, and able to counter, for any qualifying spell on the chain.
- confidence: CODE-ONLY. Board that shows it: opponent plays Gust; a triggered ability (or the Defy player's own
  Discipline) is put on the chain above it; the Defy player has priority with [1] and a Power — Defy must be
  offered against Gust and is not.
- also seen: -

**Further evidence — reader 3, D21 (R3-9): Hard Bargain: counters only the top of the chain, names no target as it is played, and the Repeat's second spell is never chosen**

  refer to finalized objects on the Chain": any of them), CR 355.5 (chosen as it is put on the chain), CR
  820.2 / 820.2.a (the Repeat's choices are made in the Make Relevant Choices step and may differ).
- happened: `SPELL: Hard Bargain (id=..) targets=[]` on every play, no target line; the spell answered is
  whatever is on top when it resolves. With Repeat paid: akali_sb__irelia_sb__s20262105__random__random
  `[D:111.01] PAY_ADD: P2 pays [2]` / `[D:111.02] REPEAT: paid 1 extra tranche(s) for Hard Bargain` /
  `[D:114.03] HARD BARGAIN: countered En Garde (its controller did not pay [2])` /
  `[D:114.05] HARD BARGAIN: top of chain isn't a spell — no-op` (27 of the 31 no-op lines in the logs are
  such second executions). The Repeat is also asked after the base cost is paid (`PAY_COST` D:108, question
  D:111) — the K4 pattern.
- should: the controller names a spell on the chain (not necessarily the top one; not Hard Bargain itself)
  when playing it, and a second one for a paid Repeat.
- confidence: LIKELY (rules cited; the engine's peek-and-pop is a documented design) — CODE-ONLY for the
  withheld choice: board = two finalized spells on the chain with Hard Bargain's controller wanting the lower
  one, or a spell under a triggered ability.

**Further evidence — reader 4, D21 (R4-7): Riposte (and Repulse): the spell to counter is never chosen — always the top of the chain**

- happened: azir__fiora__s20261122__random__random idx 74, chain `[top] En Garde (P1) / Discipline (P1) / [bot] Hidden Blade (P1)`, P2's options `[0] Pass Priority [1] Play Riposte #520[S]` — one Riposte option, and its only later prompt is `choose a target for Riposte [3 options]` (the friendly unit, D:077.00).
- should: three spells are on the chain; the player chooses which one Riposte counters (and gets + [M] equal to that spell's Energy cost).
- confidence: CERTAIN on the text; by design in the engine ("peek-and-pop").
- also seen: 16 decisions with Riposte offered while a second spell lay under the top. A spell lying under an ability with Riposte affordable did not occur (CODE-ONLY for that half: `Riposte::hasLegalTargets` needs `items.back().is_spell`). Repulse has the same restriction (`answerable` looks at `items.back()` only).

**Further evidence — reader 7, D21 (R7-8): Rebuttal acts only on the top chain item; "Choose a spell" is never a choice**

- happened: mel__azir__s20261263__random__random: the chain holds Discipline (P2, cost 2) and above it En Garde (P2). `[D:079.03] SPELL: Rebuttal (id=22) targets=[]` / `[D:079.04] PAY_COST: Rebuttal E=1 P=1` / no prompt for a spell / `[D:084.05] REBUTTAL: paid [A] - gained control of En Garde` / `[D:089.02] RESOLVE: Discipline (chain_item=14, ability=0, spell=1)`.
- should: the player chooses between En Garde and Discipline when playing Rebuttal.
- confidence: LIKELY (the text gives the choice; the class says it takes "the item on top of the chain" by design, and `hasLegalTargets` refuses the play when an ability sits above a spell that would qualify). CODE-ONLY consequence, not seen: the spell is not recorded, so if the top spell leaves the chain before Rebuttal resolves, Rebuttal takes whichever spell is then on top.
- also seen: Fizz, Trickster plays Rebuttal from the trash with an empty chain and it does nothing: mel__kaisa__s20261268__random__random `[D:241.03] PAY_COST: Rebuttal E=0 P=1` / `[D:244.03] REBUTTAL: no spell costing [4] or less on top of the chain`; teemo__mel__s20261325__random__random D:392-394. (Fizz is another reader's card; the Power was paid for nothing.)

**Further evidence — reader 8, D21 (R8-13): Abandon (CODE-ONLY): "Counter a spell" can only take the top item of the chain**

- happened: not in a trace. Class: `hasLegalTargets` = "the top item is a spell"; `onResolve` counters `chain.items.back()`.
- should / board that shows it: chain = [opponent's spell, an ability or a second spell on top]. Abandon cannot be played at all if the top item is an ability, and with two spells it cannot be aimed at the lower one; whatever is on top when Abandon resolves is what it counters, not what was chosen.
- confidence: CODE-ONLY.

**Further evidence — reader 9, D21 (R9-4): Wind Wall: resolves without countering when an ability sits above the spell on the chain**

- happened: mel_sb__yasuao_sb__s20262277__random__random, idx 246 / D:214. Chain shown on the board: `[top] Ravenbloom Student (P1)` / `[bot] Shock Blast (P1)`; options `[0] Pass Priority [1] Play Wind Wall #64[S]`.
  `[D:214.03] SPELL: Wind Wall (id=88) targets=[]` / `PAY_COST: Wind Wall E=3 P=2` / `[D:221.02] RESOLVE: Wind Wall (chain_item=46, ability=0, spell=1)` (no COUNTER line) / `[D:223.02] RESOLVE: Ravenbloom Student` / `[D:225.02] RESOLVE: Shock Blast (chain_item=43, ...)`.
- should: Wind Wall counters Shock Blast, the only spell on the chain (and the reason it was playable: `hasLegalTargets` scans the whole chain).
- confidence: CERTAIN. `counterChainTop` looks only at `chain.items.back()` and returns if it is not a spell; which spell is countered is never chosen (`targets=[]`), it is whatever lies directly beneath at resolution.
- also seen: 1 of the 20 Wind Wall plays. Any "When you play a spell" trigger of the spell's controller (Ravenbloom Student, 337 firings in the traces) protects the spell this way; every card using `counterChainTop` shares it. With two spells on the chain the player cannot choose the lower one (not seen in a trace).

### D22  Emperor of the Sands / Sand Soldier [Weaponmaster]: the "may" and the Equipment are chosen at resolution, not at finalization

_Origin: reader 2, R2-1._

- happened: akali__azir__s20261101__random__random, D:076-080 (`./build/riftbound --deck1 decks/akali.txt --deck2 decks/azir.txt --seed 20261101 --agent1 random --agent2 random --trace --debug`):
  `[D:076.04] TRIGGER: Sand Soldier [Weaponmaster] (P2)` ... both players pass ... `[D:078.02] RESOLVE: Sand Soldier (chain_item=16, ability=1, spell=0)` / `[D:078.03] MAY_PROMPT: Weaponmaster: equip an Equipment to me? (P=P2, deferred to agent)` / `[D:079.04] TGT_PROMPT: Weaponmaster: choose an Equipment [Doran's Shield/B.F. Sword/Soul Sword]`. No `FINALIZE-CHOICES` line for the token's trigger.
- should: the "may" and the Equipment are chosen as the trigger is finalized (before either player gets priority); a declined trigger leaves the chain. The opponent currently passes priority without knowing whether, and which, Equipment will move.
- confidence: CERTAIN (383.3.a in terms; the card-class path does it: `FINALIZE-CHOICES: Akshan, Mischievous` precedes its Weaponmaster prompt).
- also seen: all 403 Weaponmaster prompts of Sand Soldier tokens in the 75 games that have one follow `RESOLVE: Sand Soldier` (tokens from Emperor, Guards!, Arise!). The path is the keyword-granted ability (`keywordAbilityCard`, trigger_manager `queueKeywordAbilities`), so any unit that has [Weaponmaster] from a grant is affected.

### D23  Trifarian War Camp / Azir, Sovereign (and other continuous effects): Might is not recomputed before the next window or the damage step — a token moved in by Azir's trigger fights without the +1 [M]

_Origin: reader 2, R2-3._

- happened: azir__azir__s20261119__random__random D:358-362 (`./build/riftbound --deck1 decks/azir.txt --deck2 decks/azir.txt --seed 20261119 --agent1 random --agent2 random --trace --debug`), combat at BF#1 = Trifarian War Camp: `[D:362.03] AZIR SOVEREIGN: attack -> moved 1 token unit(s) to this battlefield` / `[D:362.04] DAMAGE_STEP: BF#1 ATK=[Recruit(2M), Azir, Sovereign(11M), Sand Soldier(2M)] DEF=[Sand Soldier(4M), Sand Soldier(3M), Recruit(2M)]` / `[D:362.05] ASSIGN_DAMAGE: P2 assigns 15 damage:`. Every other unit there has the +1 (Recruit 2M, Sand Soldier 3M); the Sand Soldier that had just been moved in is 2M.
- should: Sand Soldier 3M, 16 damage assigned.
- confidence: CERTAIN (the card text names attackers).
- also seen: rengar__azir__s20261299__random__random D:308.04 `ATK=[Sand Soldier(2M), Sand Soldier(4M), Sand Soldier(3M), Azir, Sovereign(5M)]`, `P2 assigns 14 damage` (15 due). The reverse staleness, in a reaction window: same azir__azir game, idx=439 (D:405): Azir, Sovereign and a Sand Soldier moved Trifarian War Camp -> base at `[D:403.01]`/`[D:403.02]`, their Eye of the Herald triggers are on the chain, P1 has priority, and the board shows `Azir, Sovereign(11M,...EQ:Eye+B.F. Sword+Brutalizer+Doran's+Doran's+Eye)` (10 due) and `Sand Soldier(3M,exh)` with no Equipment (2 due) in P2's base — a Might-reading effect played in that window ("3 [M] or less", damage) would use the wrong numbers. Brutalizer's "+2 if attached this turn" is still on the bearer in the next turn's Awaken Phase and Beginning Step snapshots and drops at the Scoring Step (azir__azir__s20261119 SNAP 33-35: 5M, 5M, 3M). D26 (R2-7) may be the same root (aura pass not run when combat starts).

### D24  Azir, Sovereign: "you may move any number of your token units" is asked at resolution

_Origin: reader 2, R2-4._

- happened: azir__azir__s20261119 D:343-347: `[D:343.05] TRIGGER: Azir, Sovereign (P1) fires ability` / passes / `[D:345.02] RESOLVE: Azir, Sovereign (chain_item=69, ability=1, spell=0)` / `[D:345.03] SELECT_PROMPT: Azir, Sovereign: move a token unit here [12 options]` ... `[D:347.03] AZIR SOVEREIGN: attack -> moved 1 token unit(s) to this battlefield`.
- should: whether to move, and which tokens, are fixed as the trigger is finalized; the opponent's priority window comes after that.
- confidence: LIKELY (same family as K3; the class comment says "as the ability resolves").
- also seen: every Azir trigger (azir__teemo__s20261130 D:338-343, rengar__azir__s20261299 D:306-308, azir_sb__akali_sb__s20262118 D:245-247).

### D25  Hidden cards (Guards!, Hidden Blade, Edge of Night, Pyke, Returned, Wages of Pain, Temporal Breach): a facedown card is never offered in its controller's own Neutral Open State

_Origin: reader 2, R2-6._

- happened: azir__diana__s20263004__ismcts-sims20__random (`./build/riftbound --deck1 decks/azir.txt --deck2 decks/diana.txt --seed 20263004 --agent1 ismcts:sims=20 --agent2 random --trace --debug`): `[D:031.06] HIDDEN: Guards! hidden at BF#0 by P1` (turn 3). Turn 5, P1's Main Phase, idx=53: board `| Seat of Power [Ctrl:P1] | P1: Sand Soldier(2M,rdy) | [HIDDEN: P1 — facedown: Guards!]`, 10 options (End Turn, moves, Play Brutalizer / Eye of the Herald / Hand Hammer / Deathgrip x2 / Hidden Blade) — no "Play Guards!". The same card is offered and played at idx=63 in a Closed State of that turn.
- should: "Play Guards!" (from facedown, cost 0) among the Main Phase options.
- confidence: LIKELY (813.2 and 811.1.b read together; nothing in 811 limits the play to Closed States).
- also seen: over the games of the lists holding these four cards, an eligible facedown copy (hidden on an earlier turn) was offered 148 times, always in a Closed State or under "Pass Focus", and 35 times not offered in an own-turn Neutral Open decision: Guards! 4, Edge of Night 3 (irelia__khazix__s20261197__random__random idx=124), Pyke, Returned 1 (mel_sb__irelia_sb__s20262267__random__random idx=55), Hidden Blade 27 (azir__leblanc__s20263035__random__ismcts-sims20 idx=184; some of these may lack a target at that battlefield).

**Further evidence — reader 5, D25 (R5-5): Hidden cards (Wages of Pain, Temporal Breach, any): a facedown card is not offered in its controller's own Neutral Open main phase**

- happened: ezreal__ezreal__s20261157__random__random. `[D:260.04] HIDE: Wages of Pain at BF#1 by P1` (turn 16). Turn 18, P1's main phase, decision #284 (open, 13 options, `[0] End Turn`): board `Vilemaw's Lair (P2's) [Ctrl:P1]  P1: Ezreal, Prodigy(3M,rdy)  [HIDDEN: P1 — facedown: Wages of Pain]`; the options are End Turn, 2 moves, 3 Fizz plays, 3 gear plays, 3 Stupefy plays, Activate Pack of Wonders — no "Play Wages of Pain", although a unit is at that battlefield and the cost is [0].
  kaisa_sb__zed_sb__s20262225__random__random decision #105 (turn 10; Temporal Breach hidden turn 8 at Rockfall Path with Plundering Poro there): 13 options, no Temporal Breach; at decision #107 of the same turn (after a move, no longer the plain action list) it is offered.
  Code: `generateHiddenPlayActions` is called from the showdown generator and the closed-state generator only.
- should: from the turn after it was hidden, the card is also an option in its controller's main phase.
- confidence: CERTAIN (811.6 + 813.2).
- also seen: Temporal Breach 20 such main-phase decisions, Wages of Pain 18 (count includes payment sub-decisions; the two above are action lists). In closed states and in showdown / combat focus windows it is offered (Temporal Breach 92 + 11 decisions, Wages of Pain 42 + 2).

### D26  Vex, Cheerless: an enemy spell played while she is in combat is not taxed

_Origin: reader 2, R2-7._

- happened: vex_sb__irelia_sb__s20262339__random__random D:293-296 (`./build/riftbound --deck1 <sb>/vex_sb.txt --deck2 <sb>/irelia_sb.txt --seed 20262339 --agent1 random --agent2 random --trace --debug`): `[D:293.01] MOVE: Vex, Cheerless (id=94) -> BF#1` ... `[D:295.06] COMBAT: BF#1 attacker=P2 defender=P1` / `[D:295.07] INTENT: PlayCard by P1` / `[D:295.08] SPELL: Charm (id=25) targets=[]` / `[D:296.02] PAY_COST: Charm E=1 P=1`. Board at idx=324: `P2: Vex, Cheerless(5M,exh,ATK)`; P1 `Runes:1/9+3 (rdy:1P, exh:4G 4P) Pool:E0/P0`.
- should: Charm costs 2 Energy + 2 Power for P1; with one ready rune P1 cannot pay it, so it is not offered. (It then moved Tideturner into the combat.)
- confidence: CERTAIN on the text; a single instance (the only spell played with Vex, Cheerless in combat in all 720 games). Cause not established — `applyPassiveAura` adds the tax in the aura pass, which apparently had not run since she became an attacker (cf. D23 (R2-3)). Charm being playable there at all is D5 (R2-5).
- also seen: none.

### D27  Pyke, Returned / Edge of Night: the trigger's condition is tested at resolution, so the ability goes on the chain when it should not trigger

_Origin: reader 2, R2-8._

- happened: azir__irelia__s20261123__random__random D:353-358: `[D:353.04] RECALL: Pyke, Returned (id=94) to base` / `[D:354.06] KILL: Sand Soldier (id=116, 3M, dmg=3)` / `[D:354.08] TRIGGER: Pyke, Returned (P2) fires ability` / `[D:354.09] KILL: Sand Soldier (id=113, 3M, dmg=3)` / `[D:354.11] TRIGGER: Pyke, Returned (P2) fires ability` / passes / `[D:356.02] RESOLVE: Pyke, Returned (chain_item=94, ...)` (no effect) / passes / `[D:358.02] RESOLVE: Pyke, Returned (chain_item=93, ...)` (no effect): two chain items for a once-each-turn ability, both from a Pyke the engine had already put in base.
  akali__irelia__s20261105__random__random D:102-107: Edge of Night played from hand (`[D:102.04] PAY_COST: Edge of Night E=3 P=0`) / `[D:105.02] TRIGGER: Edge of Night (P2) fires ability` / `[D:105.04] TGT_NOT_OFFERED: Edge of Night: attach to which unit here? (no legal targets at resolve time)` / both players pass / `[D:107.02] RESOLVE: Edge of Night`.
- should: no trigger (no chain item, no priority round) for the second enemy death of a turn, for a death while Pyke is not at a battlefield, or for an Edge of Night played from hand. With the test at resolution a Pyke that is at a battlefield when the unit dies and leaves before resolution gets nothing, and one that arrives before resolution gets a Gold it is not due (CODE-ONLY for those two boards).
- confidence: LIKELY (383.2.a.1 / 383.3.e.1). In the traces read the outcome is unchanged apart from the extra items and windows.
- also seen: Pyke: 106 resolutions, 64 of them empty; 28 (game, turn) pairs with 2-4 triggers in one turn (irelia_sb__zed_sb__s20262207__random__random turn 10: 3). Edge of Night: 31 plays from hand, each with a trigger on the chain (akali__irelia__s20263001__ismcts-sims20__random D:242).

### D28  Deathgrip: playable with a single friendly unit (no "another friendly unit" to choose)

_Origin: reader 2, R2-10._

- happened: akali__azir__s20261101__random__random D:021-025: `[D:021.01] CHOSE: [P2] PlayCard Deathgrip tgt=[Azir, Sovereign]` / `[D:021.03] SPELL: Deathgrip (id=91) targets=[Azir, Sovereign(id=55)]` / `[D:025.03] EFFECT: kill Azir, Sovereign (id=55)` / `[D:025.04] EFFECT: P2 draws 1`. (`enumerateLegalTargetSets`: "With a single friendly unit the spell is still playable on it alone".)
- should: if "another friendly unit" is a target (it is chosen by the controller, 355.5), Deathgrip cannot be put on the chain with one friendly unit on the board.
- confidence: QUESTION (see QUESTIONS 2): the second unit sits inside an "If you do" sentence, and the rules do not say whether such a target is required at finalization.
- also seen: not counted.

### D29  Diana, Lunari: "When a showdown begins here" never triggers on a combat showdown

_Origin: reader 3, R3-1._

  Showdown will be opened as the first step of Combat"), CR 466.2.b ("If the Combat opened with a Combat
  Showdown, start of showdown effects happen simultaneously").
- happened: all 48 `TRIGGER: Diana, Lunari` lines in the 720 logs directly follow `SHOWDOWN: BF#n (non-combat)`;
  in 52 combats whose `DAMAGE_STEP` lists Diana, Lunari there is no trigger between `COMBAT:` and the damage
  step. E.g. diana__akali__s20261136__random__random D:268: `[D:268.06] COMBAT: BF#0 attacker=P1 defender=P2`
  / `[D:268.07] DAMAGE_STEP: BF#0 ATK=[Diana, Lunari(3M), Tideturner(2M), Hwei, Brooding Painter(5M)] DEF=[...]`;
  azir__diana__s20261120__random__random D:193 -> D:223 (Diana attacks alone, no trigger).
  Code: `ShowdownStartedEvent` is emitted only in `GameEngine::runShowdown` (game_engine.cpp:5130);
  `runCombat` goes straight to `runShowdownLoop`.
- should: the ability triggers (with its "may pay [1]") whenever a combat begins at Diana's battlefield, for
  the attacker's and the defender's Diana alike.
- confidence: CERTAIN (CR 344.1, 466.2.b in terms).
- also seen: any other "When a showdown begins here" source is dispatched from the same event (battlefields
  through `fireBattlefieldTriggers(WhenAShowdownBeginsHere)`).

### D30  Scorn of the Moon cannot pay Diana, Lunari's [1]: the "may" is withheld with the legend ready (outcome of K2)

_Origin: reader 3, R3-4._

  (Add abilities finalize and resolve immediately), CR 430.3 and 445.2.c ("Players may activate abilities that
  Add resources with the Reaction keyword at any time that they are instructed to Pay resources").
- happened: diana_sb__rengar_sb__s20262147__random__random: `[D:027.05] SHOWDOWN: BF#0 (non-combat)` /
  `[D:027.06] TRIGGER: Diana, Lunari (P1) fires ability` / `[D:027.07] INTENT: ActivateActionAbility by P1`
  (Scorn, with Focus; P1 has no ready rune) / `[D:027.08] FINALIZE-CHOICES: Diana, Lunari` /
  `[D:027.09] MAY_NOT_OFFERED: Diana Lunari: pay [1] to Predict + reveal? (precondition failed at prompt time)` /
  `[D:029.02] RESOLVE: Scorn of the Moon` / `[D:029.03] ACTIVATE: Scorn of the Moon adds [1] Energy, showdowns only` /
  `[D:031.02] RESOLVE: Diana, Lunari` (nothing). Same with the legend untouched and the showdown already
  closed: diana_sb__leblanc_sb__s20262144__random__random D:051 (`CONQUER` ... `MAY_NOT_OFFERED` ... Scorn
  activated afterwards, D:054). No trace of the 48 has Scorn's Energy paying Diana's [1].
- should: when Diana's cost is due the player may exhaust Scorn, the Energy is there at once, the showdown
  is in progress, and the [1] is paid with it.
- confidence: CERTAIN (CR 430.2, 430.3, 445.2.c).
- also seen: `payableEnergy` counts floating Energy and ready runes only; the same applies to every cost paid
  at finalization or resolution (Hard Bargain's "pays [2]", Deflect).

### D31  Scorn of the Moon activated with Focus passes Focus to the opponent (outcome of K2)

_Origin: reader 3, R3-5._

  being added to the chain"), CR 430.2.a ("Priority and Focus will not pass from Add abilities").
- happened: yi_rebuild_sb__diana_sb__s20262390__random__random decision #160 `[0] Pass Focus [1] Activate Scorn of the Moon`
  -> `[D:145.03] ACTIVATE: Scorn of the Moon adds [1] Energy` -> decision #163 `==> P1 (forced): Pass Focus`
  -> decision #164 P2 again. The opponent acting in between: diana_sb__vex_sb__s20262149__random__random
  decision #17 (P1 activates Scorn with Focus) -> decision #20 `P2: Play Rebuke`;
  diana_sb__teemo_sb__s20262148__random__random #98 -> #101 `P2: Play Acceptable Losses`;
  akali_sb__diana_sb__s20262102__random__random #90 -> #93; diana__zed__s20261153__random__random #221 -> #224.
  In the 78 diana replays: 50 activations with Focus, 44 followed by the opponent holding Focus.
- should: the activating player keeps Focus (and the passes already made are not reset by an Add).
- confidence: CERTAIN.

### D32  Fizz, Trickster: a spell whose target is taken from the play intent is replayed with no target (Stupefy, Eclipse, Smoke Screen, Turn to Dust)

_Origin: reader 3, R3-6._

  steps of Play as normal"), CR 355.5 / 355.8 (targets are chosen as it is put on the chain).
- happened: diana__vex__s20261149__random__random (board at decision #216: Vex, Apathetic and Fizz at Targon's
  Peak, Traveling Merchant and an enemy Vex on the board): `[D:203.03] PAY_COST: Eclipse E=0 P=0` /
  `[D:203.04] FIZZ: played Eclipse from trash ...` / `[D:208.02] RESOLVE: Eclipse` / `[D:208.03] PREDICT: P1 looks at top 1 cards`
  — no target prompt, no "-4 [M]". Stupefy: akali__diana__s20261102__random__random `[D:035.03] PAY_COST: Stupefy E=0 P=0` /
  `[D:037.02] RESOLVE: Stupefy` / `[D:037.03] EFFECT: P2 draws 1` (no -1). Turn to Dust with a gear on the
  board: ezreal__fiora__s20261158__random__random D:209-211 (`RESOLVE: Turn to Dust`, nothing; The List is
  activated at D:212). Smoke Screen: rengar_sb__diana_sb__s20262300__random__random D:076.
  Scan: Stupefy via Fizz 11 times, Eclipse 3, Turn to Dust 6, Smoke Screen 1 — never a target line.
  Code: the executor's `queue_spell_` hook (game_engine.cpp:69) adds the spell with `{}` targets and calls
  `chooseTargetsAtFinalize`, which returns at :1485 for a spell that neither `needsPlayTimeTarget()` nor has
  `kFinalizeResolve`. Spells with either (Gust, Ride the Wind, Decree of Insight: `TARGET (finalize)`;
  Star-Crossed, Switcheroo, Moonfall, Flash: prompts) do get their choices.
- should: the controller chooses the unit / gear as the spell is put on the chain (Deflect included).
- confidence: CERTAIN.
- also seen: any effect that plays a spell through `playPayingCost` / `playIgnoringCost` shares the path.

### D33  Vex, Apathetic: the ability is put on the chain while Vex is in a base; "while I'm at a battlefield" is tested at resolution only

_Origin: reader 3, R3-8._

  turn." The "while" belongs to the trigger condition (CR 383: an ability triggers when its condition is met).
- happened: azir__teemo__s20263050__random__ismcts-sims20, board at decision #178 `P2 BASE Units: Vex, Apathetic(4M,exh,Dfl) ...`:
  `[D:164.03] TOKEN: created Sand Soldier (2M, id=111) for P1` / `[D:164.04] TRIGGER: Vex, Apathetic (P2) fires ability`,
  both players then receive priority for an ability that resolves with nothing. 900 triggers, 372 stuns in
  the logs (the other 528 are this, a second Vex on an already-stunned unit, or the played unit gone).
- should: no trigger and no chain item (no priority window, no Closed State) while Vex is not at a
  battlefield; and (CODE-ONLY, board: Vex at a battlefield when the unit is played, Vex returned to hand or
  moved to base in response while the played unit stays) the ability still stuns — the class returns at
  `if (!self.battlefieldId().has_value()) return;`.
- confidence: LIKELY for the spurious chain item (my reading of where the condition is checked); CODE-ONLY
  for the second half — the 6 traces where Vex left in response (e.g. diana__vex__s20263054__random__ismcts-sims20
  D:192-194) all removed the played unit as well.

**Further evidence — reader 8, D33 (R8-12): Vex, Apathetic: the ability goes on the chain while she is not at a battlefield; the condition is tested as it resolves**

- happened: vex__viktor__s20263052__ismcts-sims20__random, decision #34: Vex, Apathetic is in P1's base (`| P1 BASE | Units: Vex, Apathetic(4M,exh,Dfl)`), chain `[top] Vex, Apathetic (P1)`: `[DBG] [D:033.02] TRIGGER: Vex, Apathetic (P1) fires ability` / `[D:034.00] DECISION #34 (P1 — chain priority)` / `[D:035.00] DECISION #35 (P2 — chain priority)` / `[D:035.02] RESOLVE: Vex, Apathetic (chain_item=5, ability=1, spell=0)` (no effect). Class: `if (!self.battlefieldId().has_value()) return;` inside `onTrigger`, no `triggerCondition`.
- should: no ability when she is in the base as the unit is played; and an ability that did trigger stuns even if she has left the battlefield since (the engine does not), while one that should not have triggered must not stun because she moved to a battlefield in response (the engine would).
- confidence: LIKELY (383.2.a.1 and its example).
- also seen: 900 fires, 371 stuns (tally): 529 items on the chain with nothing to do, each a priority window for both players. Otherwise correct: tokens count (Sprite, Recruit, Tentacle, Sand Soldier, Mech, Reflection, Shadow Clone, Bird: 60 fires after `TOKEN: created`), units an effect plays count (Temporal Breach's replay kaisa_sb__teemo_sb__s20262220__random__random D:174-176), a stunned defender dealt no damage (same game D:178), a stunned ready unit was offered no move that turn (8 of 8 boards). Not one of my 30 cards; read as a deck interaction.

### D34  Decree of Insight: its target is chosen after the cost is paid

_Origin: reader 3, R3-10._

- happened: diana_sb__khazix_sb__s20262143__random__random `[D:141.06] SPELL: Decree of Insight (id=38) targets=[]` /
  `[D:141.07] PAY_COST: Decree of Insight E=1 P=0` / `[D:143.00] DECISION #152 (P1): choose a target for Decree of Insight [1 options]` /
  `[D:143.01] TARGET (finalize): Decree of Insight -> Kinkou Initiate`. All 29 target lines of the card are
  `TARGET (finalize)` after `PAY_COST`; Gust / Rebuke / Ride the Wind from hand are `TARGET (play)` before it (887 lines).
- should: target first, then the cost.
- confidence: CERTAIN on the order; no changed outcome observed (the card ignores Deflect, so no cost hangs on the choice).

### D35  Grand Duelist / Fiora, Worthy: "becomes [Mighty]" is only noticed at the engine's top-level cleanups, so the triggers arrive late

_Origin: reader 4, R4-1._

- happened (four shapes, one cause: `GameEngine::cleanup()` is where the crossing is detected and it does not run between the resolutions of one chain, after the second chain run of an action, or after the Expiration body):
  - (a) inside a chain: akali_sb__fiora_sb__s20262104__random__random D:177.03 `EFFECT: give Fiora, Worthy 2 [M] this turn -> 5M`; then `[D:178.00] DECISION #192 (P1 — chain priority)`, `[D:179.00] DECISION #193 (P2 — chain priority)`, `[D:179.02] RESOLVE: Astral Heron`, and only then `[D:179.05] TRIGGER: Fiora, Worthy (P2) fires ability` (twice: Fiora, and the Rengar that entered at D:164-170). Same in fiora__irelia__s20263016__ismcts-sims20__random: Rengar enters D:083.01, `[D:085.00] DECISION #87 (P2 — chain priority): 3 legal actions`, `[D:085.02] RESOLVE: Abandoned Hall`, `[D:085.03] TRIGGER: Fiora, Worthy`.
  - (b) after a whole further decision: fiora__zed__s20261189__random__random `[D:091.03] TGT_PICKED: Rift Herald (deathknell: unit from hand) = Rengar, Trophy Hunter`, `[D:092.02] RIFT HERALD: deathknell -> played a unit from hand to base`, chain empties at D:096.02, `[D:097.00] DECISION #101 (P1): 3 legal actions`, `[D:097.01] CHOSE: [P1] StandardMove units=[Sett, Brawler]`, `[D:097.03] MOVE: Sett, Brawler (id=38) -> BF#1`, `[D:097.04] TRIGGER: Fiora, Worthy (P1) fires ability` (for the Rengar; Sett is 4 [M]). In fiora__fiora__s20261176__random__random the same Rengar-by-deathknell (D:089.03-090.02) is announced only after `[D:095.01] CHOSE: [P2] EndTurn`, in the Ending Step (D:095.03), and finalized in the Expiration Step (D:095.06).
  - (c) a "-N [M] this turn" that expires: diana_sb__fiora_sb__s20262140__random__random `[D:165.03] EFFECT: give Rengar, Trophy Hunter -5 [M] this turn -> 1M`, `[D:166.03] ── Expiration Step ──`, `[D:166.04] ════ TURN 12 (P2)`, `[D:166.05] ── Awaken Phase ──`, `[D:166.06] ── Beginning Step ──`, `[D:166.07] TRIGGER: Fiora, Worthy (P2)`, `[D:166.08] TRIGGER: Grand Duelist (P2)`, `[D:166.09] ── Scoring Step ──`, `[D:166.10] SCORE: P2 +1 via Hold BF#0 -> 4`, `[D:166.11] FINALIZE-CHOICES: Fiora, Worthy`. Same game D:224.03-224.16: fired in P1's Beginning Step, finalized after `── Main Phase ──` of the opponent's turn.
  - (d) a unit played by Dazzling Aurora in the Ending Step: diana__fiora__s20261140__random__random `[D:241.04] TRIGGER: Grand Duelist (P2)`, `[D:241.05] ── Expiration Step ──`, `[D:241.06] FINALIZE-CHOICES: Grand Duelist` (the Expiration body — heal, "this turn" expiry — runs before the pending item is finalized).
- should: the abilities trigger when the Might changes and go on the chain as pending items in the cleanup that follows (before the next item of that chain resolves; before the next Main Phase decision). In (c) they belong to the Expiration Step of the turn that ends: the legend exhausted there and the rune channelled exhausted would both be readied by the next Awaken Phase, whereas the engine asks after the Awaken Phase.
- confidence: CERTAIN for (b) and (c) (a player decision / a phase boundary lies between the change and the trigger; CR 709, 317.2.f); LIKELY for (a) and (d) (reading of CR 319.5 / 320).
- also seen: 17 Grand Duelist triggers right after `── Beginning Step ──`, 2 after `── Ending Step ──`, 44 after `RESOLVE: Poppy, Paragon`, 19 after `RESOLVE: Elder Dragon` (fiora games, by script). The same staleness shows in D36 (R4-2) and D42 (R4-12).

### D36  Sett, Brawler + Grand Duelist / Fiora, Worthy: a unit that stops being Mighty and becomes Mighty again inside one chain triggers nothing

_Origin: reader 4, R4-2._

- happened: ezreal_sb__fiora_sb__s20262158__random__random `[D:197.04] EFFECT: P2 spends a buff from Sett, Brawler -> 4M` (cost, at finalization), two priority passes, `[D:199.03] EFFECT: give Sett, Brawler 4 [M] this turn -> 8M`, `[D:199.04] SETT, BRAWLER: spent a buff for +4M this turn`, `[D:200.00] DECISION #207 (P2): 2 legal actions` — no `TRIGGER:` line. fiora__diana__s20261174__random__random D:091.04 / D:093.03 identical.
- should: Sett went 5 -> 4 -> 8: at D:199.03 his Might changed from less than 5 to 5 or more, so Grand Duelist (and Fiora, Worthy if on board) trigger.
- confidence: CERTAIN (CR 709; the engine compares only the Might at one cleanup with the Might at the next).
- also seen: script over the fiora games: 25 activations with 4M -> 8M, 25 without a trigger (the other two started at 6 and 8). The reverse miss (up and down again between two cleanups) follows from the same mechanism; not seen in a trace.

### D37  Rengar, Trophy Hunter: the enemy-occupied battlefield is never a play location in a Neutral Open state

_Origin: reader 4, R4-6._

- happened: script over every decision of all games that offers `Play Rengar, Trophy Hunter`: in Main Phase decisions that list `End Turn` (Neutral Open, own turn) a battlefield with enemy units and none of the player's was a destination 0 times out of 471 (first: akali__fiora__s20261104__random__random idx 165); in chain-priority and `Pass Focus` decisions it was a destination 120 times out of 125 (of the 5 others the one inspected, kaisa__yi_rebuild__s20261224__random__random idx 108, is Rockfall Path). Base is offered in the open state (924) and never in a closed state or showdown (0 of 249).
- should: the location permission is not tied to the [Reaction] timing; on his controller's turn in a Neutral Open state Rengar may be played to a battlefield where there are enemy units.
- confidence: LIKELY (822.1.b gives two separate abilities; no FAQ in `rules/patch-notes/`).
- also seen: the closed-state / showdown side is correct: fiora__irelia__s20263016__ismcts-sims20__random D:077.03 (Abandoned Hall, enemy units only, during a chain on his own turn; combat follows at D:091.04), akali_sb__fiora_sb__s20262104__random__random D:164.03 (opponent's turn, to a battlefield with own units).

### D38  Divining Shells: "Kill this" is paid when the ability resolves

_Origin: reader 4, R4-8._

- happened: fiora__reksai__s20261182__random__random `[D:027.05] INTENT: ActivateActionAbility by P1`, `[D:028.01] TARGET (finalize): Divining Shells -> Recruit`, `[D:029.00] DECISION #31 (P1 — chain priority): 2 legal actions` ... (a Flurry of Blades is played and resolves on top) ... `[D:034.02] RESOLVE: Divining Shells (chain_item=7, ability=1, spell=0)`, `[D:034.03] EFFECT: kill Divining Shells (id=9)`, `[D:034.04] EFFECT: give Recruit 2 [M] this turn -> 3M`.
- should: the gear is killed at activation; while the ability waits on the chain the gear is no longer on the board, and a countered ability does not give it back.
- confidence: CERTAIN (the class kills in `onActivate` without `kFinalizeActivate` / `costDueNow`; Forge of the Future, same cost wording, kills at finalization: fiora_sb__diana_sb__s20262174__random__random D:368.04).
- also seen: 32 activations in the fiora games; the class has no other path, so every one does (13 of them made a unit Mighty: `DIVINING SHELLS: killed self -> +2 [M] this turn` directly before the triggers).

### D39  Sacrifice + Rift Herald: an ability triggered by paying a spell's additional cost is put UNDER that spell

_Origin: reader 4, R4-9._

- happened: fiora__fiora__s20261176__random__random `[D:082.04] FINALIZE-CHOICES: Sacrifice (chain_item=18 ...)`, `[D:083.04] EFFECT: kill Rift Herald (id=83)`, `[D:083.05] TRIGGER: Rift Herald (P2) fires ability`, `[D:083.06] PAY_COST: Sacrifice E=1 P=0`, `[D:086.02] RESOLVE: Sacrifice (chain_item=20 ...)`, `[D:086.04] DREW: Rengar, Trophy Hunter (id=80)`, `[D:088.02] RESOLVE: Rift Herald (chain_item=19 ...)`, `[D:089.03] TGT_PICKED: Rift Herald (deathknell: unit from hand) = Rengar, Trophy Hunter (id=80)`. The spell's item is renumbered 18 -> 20, above the trigger's 19.
- should: the [Deathknell] resolves first, with the hand as it was; Sacrifice's draw 2 comes after. Here the unit the Deathknell played was drawn by the Sacrifice that should have resolved after it.
- confidence: LIKELY (reading of CR 354-359; the spell analogue of K1).
- also seen: fiora__zed__s20261189__random__random D:084.04-091.03, the same three cards and the same result (Rengar id=25 drawn at D:088.05, played at D:091.03).

### D40  Grand Duelist + Fiora, Worthy (and Akshan's two play triggers): the controller is never asked in which order simultaneous triggers go on the chain

_Origin: reader 4, R4-10._

- happened: akali__fiora__s20261104__random__random `[D:040.05] TRIGGER: Fiora, Worthy (P2)`, `[D:040.06] TRIGGER: Grand Duelist (P2)`, finalized in that order, resolved Grand Duelist first (D:049.02) then Fiora (D:051.02) — in every instance; no ordering decision exists (decision kinds of all fiora games enumerated by script: none). Two units at once: akali_sb__fiora_sb__s20262104__random__random D:179.05-179.08, fixed Fiora / Duelist / Fiora / Duelist.
- should: an order choice whenever one player has two or more triggers from one event.
- confidence: CERTAIN on the rule; for this pair alone the order rarely changes an outcome (both costs are paid at finalization).
- also seen: fiora_sb__akali_sb__s20262172__random__random D:131.03-131.06 (Akshan: Weaponmaster and the gear steal).

### D41  Repulse, Forge of the Future: choices the rules place at finalization are made after RESOLVE (K3 kind)

_Origin: reader 4, R4-11._

- happened: akali__fiora__s20261104__random__random `[D:151.02] RESOLVE: Repulse (chain_item=33 ...)`, `[D:151.03] TGT_PROMPT: Repulse: choose a friendly unit [Fiora, Worthy]`, `[D:152.03] TGT_PICKED`. fiora_sb__diana_sb__s20262174__random__random `[D:370.02] RESOLVE: Forge of the Future`, `[D:370.03] SELECT_PROMPT: Forge of the Future: recycle a card from a trash (1 of up to 4) [28 options]` ... D:374.03.
- should: both chosen as the spell / ability is put on the chain.
- confidence: LIKELY.
- also seen: both Repulse resolutions read (the other: fiora__akali__s20261172__random__random D:246.02-247.03); both classes choose inside `onResolve` without a finalization pass, so every play does.

### D42  Fiora, Victorious: her [Shield] adds nothing to her Might while a chain of the combat showdown is running

_Origin: reader 4, R4-12._

- happened: diana_sb__fiora_sb__s20262140__random__random, `[D:252.04] COMBAT: BF#0 attacker=P1 defender=P2`; she has Shepherd's Heirloom (4+2); the boards of the seven decisions D:252-258 (Moonfall being played and paid) show `Fiora,Vict(6M,rdy,DEF,EQ:Shephe..)`. Units with a printed Shield show the bonus on the same kind of board (`Mutated Mouser(3M,Shd2,Tnk,DEF)`, base 1).
- should: 7 [M] from the moment she is a defender.
- confidence: LIKELY. The fold in `recalculateAuras` does add an aura-granted [Shield] for a Defender, so the reading is that the Might is stale until the next cleanup (same cause as D35 (R4-1)); no board of any game shows her at printed+bonus+1, so "never applied" cannot be excluded. No outcome changed here (Moonfall's -2 took her to 4 and out of Mighty, D:258.03).
- also seen: only this combat (245 boards with her at 6M, 7 of them as defender).

### D43  Ezreal, Prodigy + Thousand-Tailed Watcher (any Accelerate): Accelerate is not an "optional additional cost" for Ezreal's discount

_Origin: reader 5, R5-1._

- happened: ezreal__leblanc__s20261162__random__random, Ezreal, Prodigy in P1's base (board at D:183: `P1 BASE  Units: Ezreal, Prodigy(3M,rdy)`), P1 plays Thousand-Tailed Watcher:
  `[D:194.00] DECISION #204 (P1): pay [1][Mind] for Accelerate on Thousand-Tailed Watcher? [decline|pay] [2 options]`
  `[D:194.01] ACCELERATE: Thousand-Tailed Watcher pays [1] + power to enter ready`
  `[D:194.02] PAY_ADD: P1 pays [1] + 1 power: exhausted 1, recycled 1`
  No "optional additional cost: [1] or [A] less" decision; the full [1] + Power was paid. The same player's Repeat IS discounted (ezreal__teemo__s20261166: `[D:163.00] DECISION #165 (P1): optional additional cost: [1] or [A] less [2 options]` / `[D:163.01] PAY_ADD: P1 pays [0] + 1 power: exhausted 0, recycled 1`).
  Code: `GameEngine::resolvePermanent` pays Accelerate with `payAdditionalCost(item.controller, 1, 1, *domain_to_pay, true)` and gates the offer with `canPayAdditionalCost(..., 1, 1, ...)`; only Repeat and "you may pay" costs go through `payOptionalCost`.
- should: with Ezreal, Prodigy on the board Accelerate costs [1] or 1 Power less (payer's choice), and it is offered when only the reduced cost can be paid.
- confidence: CERTAIN (805.1 calls Accelerate an Optional Additional Cost; 356.1.b.3's example too).
- also seen: ezreal_sb__ezreal_sb__s20262157__random__random D:303 (P1, Ezreal in base) and D:425 (P2, Ezreal at a battlefield), both `pays [1] + 1 power: exhausted 1, recycled 1`. Any unit with Accelerate.

### D44  A unit with lethal damage stays on the board until the chain is empty: no cleanup between the items of a chain (Wages of Pain, Deadly Flourish, Singularity, Unchecked Power, Piercing Light)

_Origin: reader 5, R5-3._

- happened: akali__ezreal__s20261103__random__random (P2 reacts to its own Turn to Dust with a hidden Wages of Pain):
  `[D:083.02] RESOLVE: Wages of Pain (chain_item=19, ability=0, spell=1)`
  `[D:083.03] EFFECT: deal 3 damage to Ezreal, Prodigy (id=55, was 0/3M)`
  `[D:084.00] DECISION #90 (P2 — chain priority): 1 legal actions` ... `[D:085.01] CHOSE: [P1] PlayReaction` / `[D:085.03] PLAY: Long Sword (id=27, Gear, cost=2E)` ...
  `[D:091.02] RESOLVE: Turn to Dust (chain_item=18, ability=0, spell=1)`
  `[D:091.04] KILL: Ezreal, Prodigy (id=55, 3M, dmg=3)`
  The unit with 3 damage on 3 Might lived through four priority decisions and a gear play. `GameEngine::cleanup()` (which calls `processLethalDamage`) runs after `runChain()`, never between chain items; cards that kill inline (Hextech Ray, Bellows Breath, Frozen Fortress) hide it, the others show it.
- should: the Cleanup after the spell leaves the chain kills the unit before anyone receives priority; "when it dies" triggers go on the chain then.
- confidence: CERTAIN (319.5 + 323.4).
- also seen: diana__ezreal__s20261139__random__random D:047.04 -> KILL at D:049.03 (Deadly Flourish; an Abandoned Hall ability resolved in between and the Deadly Flourish Gold trigger came after it); diana__ezreal__s20263008__ismcts-sims20__random D:402.03 (lethal) -> Frozen Fortress `deal 1 damage to Fizz, Trickster (id=90, was 3/3M)` at D:406.03; ezreal__viktor__s20263058__random__ismcts-sims20 D:064.03 -> D:068.03; ezreal__leblanc__s20263013 D:433.03 -> D:435.03.

**Further evidence — reader 7, D44 (R7-9): Unchecked Power: units with lethal damage stay on the board through two priority windows**

- happened: irelia_sb__mel_sb__s20262199__random__random: `[D:231.02] RESOLVE: Unchecked Power` / `[D:231.03] EFFECT: deal 12 damage to Treasure Hunter (id=17, was 0/1M)` / `[D:231.04] EFFECT: deal 12 damage to Vex, Apathetic (id=37, was 0/4M)` / `[D:231.05] TRIGGER: Ravenbloom Student (P2) fires ability` / `[D:232.00] DECISION #252 (P2 — chain priority)` / `[D:233.00] DECISION #253 (P1 — chain priority)` / `[D:233.02] RESOLVE: Ravenbloom Student` / `[D:233.05] KILL: Treasure Hunter (id=17, 1M, dmg=12)` / `[D:233.06] KILL: Vex, Apathetic (id=37, 4M, dmg=12)`. Replay idx 252 and 253 show `Treasure Hunter(1M,12dmg,rdy)` and `Vex, Apathetic(4M,12dmg,exh,Dfl)` at their battlefields while both players hold priority.
- should: both die in the cleanup that follows Unchecked Power leaving the chain, before anyone gets priority on the Student's ability.
- confidence: LIKELY (CR 319.5 / 323.4). The class deals the damage and leaves the kills to the cleanup; Shock Blast and Smite kill in the card and do not show this. Probably general to any damage effect that does not kill by itself when a trigger is queued during its resolution.
- also seen: only this instance looked at (the other 11 resolutions hit no unit or were not read).

**Further evidence — reader 9, D44 (R9-8): A unit with lethal damage from a spell stays on the board while a trigger from that resolution is finalized and both players get priority (Piercing Light replayed for Flow + Master of Shadows)**

- happened: yasuao_sb__zed_sb__s20262387__random__random, D:142-144: `[D:142.04] EFFECT: deal 2 damage to Shadow Order Disciple (id=67, was 0/2M)` / `[D:142.05] TRIGGER: Master of Shadows (P2) fires ability` / `DECISION #164 (P2 — chain priority)` / `DECISION #165 (P1 — chain priority)` / `[D:144.02] RESOLVE: Master of Shadows` / `[D:144.04] KILL: Shadow Order Disciple (id=67, 2M, dmg=2)`.
- should: the kill in the cleanup that follows Piercing Light leaving the chain, before the trigger is finalized and before anyone has priority.
- confidence: LIKELY. No changed outcome found in the traces (I looked for a unit saved by a Might bonus given in that window: none), but the unit is a legal choice meanwhile: diana__ezreal__s20261139__random__random D:109-110, `EFFECT: deal 6 damage to Vex, Apathetic (id=92, was 0/4M)` then `TGT_PROMPT: Abandoned Hall: choose a unit here [Vex, Apathetic]`.
- also seen: about 40 instances with a trigger in between (first trigger Abandoned Hall 17, Ravenbloom Student 12, Akali, Deadly Weapon 5, Stellacorn Herder 3, ...). Spells whose class kills its own victims (Falling Star, Hextech Ray) are not affected.

### D45  Bellows Breath: the Repeat's choices ignore "at the same location", are forced to the first execution's count, and are not asked at all when the first execution chose nothing

_Origin: reader 5, R5-4._

- happened: ezreal__leblanc__s20261162__random__random. First execution: `[D:216.03] SPELL: Bellows Breath (id=17) targets=[Karthus, Eternal(id=90),Watchful Sentry(id=56)]` (both in P2's base). Repeat paid, then
  `[D:220.00] DECISION #232 (P1): make a new choice [5 options]` — options: Karthus, Eternal @Base / Thousand-Tailed Watcher @BF / Fizz, Trickster @BF / Fizz, Trickster @BF / Watchful Sentry @Base; chosen Karthus @Base;
  `[D:221.00] DECISION #233 (P1): make a new choice [4 options]` — Thousand-Tailed Watcher @BF / Fizz @BF / Fizz @BF / Watchful Sentry @Base; chosen Fizz, Trickster @BF (Vilemaw's Lair).
  The repeat's targets are in two locations; at resolution the card then treats its own illegal choice as a mistarget:
  `[D:230.11] MODE_PROMPT: Bellows Breath: units at the same location [Deal 1 to Karthus, Eternal/Deal 1 to Fizz, Trickster] (agent decides among 2 modes)`.
  Exactly two picks were demanded (not zero, one or three). With no target the first time nothing is asked: ezreal__teemo__s20261166 `[D:159.03] SPELL: Bellows Breath (id=15) targets=[]`, `[D:163.02] REPEAT: paid 1 extra tranche(s) for Bellows Breath`, next line `[D:164.00] DECISION #166 (P1 — chain priority)`.
  Code: Repeat choices of a spell with play-time targets go through `GameEngine::makeNewChoicesFor` ("Targets locked in as the spell was played: choose that many again"), one object at a time from `enumerateLegalTargets`, never `enumerateLegalTargetSets`.
- should: the Repeat execution chooses any legal set — zero to three units sharing a location — independent of the first.
- confidence: CERTAIN (355.11.a, 355.13, 820.2.a).
- also seen: every Bellows Breath Repeat (98 paid). The Repeat decision and its choices come after the base cost is paid (`PAY_COST` D:216.04 ... `pay Repeat tranche 1` D:219.00), against CR 355.1.a (decided in the choices step) — the Accelerate ordering of K4, for Repeat; same for Rocket Barrage (teemo_sb__kaisa_sb__s20262322 D:324.04 then D:330.00).

### D46  The List: "As you play this, name a tag" is run as a triggered ability with priority windows, and only tags already public can be named

_Origin: reader 5, R5-6._

- happened: diana__ezreal__s20263008__ismcts-sims20__random:
  `[D:300.03] PLAY: The List (id=84, Gear, cost=1E)` / `[D:300.04] PAY_COST: The List E=1 P=0`
  `[D:301.02] TRIGGER: The List (P2) fires ability`
  `[D:302.00] DECISION #319 (P2 — chain priority): 1 legal actions` / `[D:303.00] DECISION #320 (P1 — chain priority): 1 legal actions`
  `[D:303.02] RESOLVE: The List (chain_item=76, ability=1, spell=0)` / `[D:304.00] DECISION #321 (P2): The List: name a tag [12 options]` / `[D:304.03] THE LIST: named tag "Ionia"`
  The gear is in play with no tag while both players hold priority, and the naming is an ability on the chain (it can be responded to, and countered by anything that counters abilities). The options (10-13 in the traces, capped at 16 in the class) are the tags on units on the board, legends, Chosen Champions and unit cards in trash / banishment.
- should: the tag is named while the card is being played (before its cost, no chain item, no priority); any tag may be named, e.g. one on a unit the opponent has not shown yet.
- confidence: LIKELY for the timing (355.1); QUESTION for the restricted list (see Q4).
- also seen: all 86 plays of The List.

### D47  Temporal Breach + Rockfall Path: the banished unit is played back onto a battlefield where "Units can't be played"

_Origin: reader 5, R5-7._

- happened: kaisa_sb__zed_sb__s20262225__random__random. Board before (decision #112): `Rockfall Path [Ctrl:P1]  P1: Plundering Poro(2M,rdy)`.
  `[D:101.02] RESOLVE: Temporal Breach (chain_item=21, ability=0, spell=1)` / `[D:101.03] EFFECT: banish Plundering Poro (id=16)`
  Board at the next decision (#113): `Rockfall Path [Ctrl:P1]  P1: Plundering Poro(2M,exh)` — played there again.
  Code: `EffectExecutor::playIgnoringCost(owner, picked, *where)` never consults `blocks_unit_play`.
- should: the unit cannot be played to Rockfall Path, so it stays banished (ruling #3 by analogy) — or, if a judge says otherwise, goes somewhere legal; not back onto Rockfall Path.
- confidence: LIKELY (054.1; the ruling is on a different card).
- also seen: kaisa__zed__s20261225__random__random decision #190 (Lecturing Yordle at Rockfall Path, rdy -> exh after the Breach). Every unit-play option generator does respect Rockfall Path (83 games, 0 options).

### D48  Temporal Breach (and every effect that plays a unit "ignoring its cost"): Accelerate is never offered

_Origin: reader 5, R5-8._

- happened: kaisa__azir__s20261209__random__random. P1 (3 ready Fury runes and 2 ready Mind runes at D:044) plays Temporal Breach on its own Kai'Sa, Survivor ([Accelerate] [1][R]):
  `[D:049.02] RESOLVE: Temporal Breach (chain_item=7, ability=0, spell=1)` / `[D:049.03] EFFECT: banish Kai'Sa, Survivor (id=2)` / `[D:050.00] DECISION #50 (P1): 6 legal actions`
  No "pay [1][Fury] for Accelerate on Kai'Sa, Survivor?" decision; the unit comes back exhausted.
  Code: `EffectExecutor::playIgnoringCost` sets `is_exhausted` from "enters ready" effects only; the Accelerate question lives in `GameEngine::resolvePermanent` (plays from hand).
- should: the owner is asked, and may pay [1][R] to have the unit enter ready.
- confidence: LIKELY (the 356.1.b.3 example is this case; "its owner plays it" is a play).
- also seen: 11 Temporal Breaches on Kai'Sa, Survivor, none with the question (azir__kaisa__s20263005 D:177, irelia__kaisa__s20261196 D:114, kaisa__diana__s20261210 D:100, ezreal__kaisa__s20261160 D:271 ...).

### D49  Void Gate: no Bonus Damage for a unit that arrived there earlier on the same chain (Akali, Deadly Weapon's move ability)

_Origin: reader 5, R5-9._

- happened: akali__ezreal__s20261103__random__random. Board at D:116: `Void Gate [Ctrl:--] CONTESTED (showdown staged)  P1: Akali,Dead(5M,exh,EQ:Long S..)`.
  `[D:116.02] RESOLVE: Akali, Deadly Weapon (chain_item=26, ability=1, spell=0)` / `[D:116.03] EFFECT: deal 1 damage to Akali, Deadly Weapon (id=2, was 0/5M)` / `[D:116.04] AKALI, DEADLY WEAPON: moved -> dealt 1` — no `EFFECT: +1 Bonus Damage` line.
  Over the 82 games with Void Gate: 84 spell / ability damage instances on a unit at Void Gate carry the +1 (Falling Star 29, Hextech Ray 17, Akali, Deadly Weapon 9, Bellows Breath 8 ...), 352 on units elsewhere do not, and 11 on a unit at Void Gate lack it — all 11 are Akali's move ability hitting a unit that moved in with that same move. The bonus is an aura (`aura_bonus_damage_taken`) rebuilt in `recalculateAuras`, which runs in the Cleanup — and no Cleanup runs while the chain is not empty (D44 (R5-3)).
- should: 2 damage (1 + Void Gate).
- confidence: LIKELY (the cause is read from the code; the 11/11 pattern is traced).
- also seen: akali__akali__s20261100 D:198.03, akali__irelia__s20261105 D:077.03 and D:087.03, akali__mel__s20261109 D:100.03, khazix__akali__s20263002 D:052.03. Checked correct for the kaisa list's own spells (each instance +1; units at Void Gate only; never on combat damage).

### D50  Daughter of the Void: its "[A]. Use only to play spells" cannot pay the Deflect of a spell

_Origin: reader 5, R5-10._

- happened: not in any trace (0 of 704 activations met a Deflect payment). Code: Deflect is paid by `EffectExecutor::payAnyPower` -> `resource::pay(state_, events_, player, 0, amount, Domain::Count)` with the default `SpendFor::Other`, or `payAnyPowerAheadOfCost`; `payableAnyPower` (what decides whether a Deflect target is offered) counts `universal_power`, per-Domain Power and runes, not `spell_power`. So the Power of Daughter is skipped and a rune is recycled, and a Deflect target is not offered when that Power is all the player has left.
- should: when a SPELL chooses the Deflect unit, the Deflect [A] may be paid with Daughter's Power (not when an ability chooses it).
- confidence: CODE-ONLY. Board that shows it: Kai'Sa player with no runes to recycle, Daughter activated (spell Power in the pool), Stupefy / Hextech Ray in hand with enough Energy, the only enemy unit has [Deflect] (Vex, Apathetic; Irelia, Fervent): the spell is not offered at that unit. The rules reading is LIKELY, see Q2.
- also seen: observed correct uses: pays a spell's own Power of a specific Domain (89 + 34 spell payments one recycle short right after an activation, e.g. kaisa__khazix__s20263024 D:212.04 Falling Star); pays a Flow replay (kaisa__rengar__s20263025 `[D:202.02] PAY_ADD: P1 pays [3] + 1 power: exhausted 3, recycled 0`); not used for a unit's Power (18 unit plays with it floating, all fully recycled), Accelerate (8) or Hide (12); 599 of 704 unspent at end of turn / Draw Phase.

### D51  Kha'Zix, Mutating Horror (also Evelynn, Entrancing; Tornado Warrior): the condition printed with the trigger is tested when the ability resolves, not when the trigger condition is met

_Origin: reader 6, R6-1._

- happened: leblanc_sb__khazix_sb__s20262251__random__random, D:249-256 (`./build/riftbound --deck1 <sb>/leblanc_sb.txt --deck2 <sb>/khazix_sb.txt --seed 20262251 --agent1 random --agent2 random --trace --debug`). Board at D:249: Sandswept Tomb holds only `Kha'Zix,Muta(4M,rdy)` for P2.
  `[D:249.01] MOVE: Honest Broker (id=20) -> BF#1` / `[D:249.02] COMBAT: BF#1 attacker=P1 defender=P2` / `[D:249.03] TRIGGER: Kha'Zix, Mutating Horror (P2) fires ability` / `[D:250.03] SPELL: Gust (id=87)` / `[D:254.03] EFFECT: bounce Honest Broker (id=20) to hand` / `[D:256.02] RESOLVE: Kha'Zix, Mutating Horror (chain_item=48, ability=1, spell=0)` / `[D:256.03] TRIGGER: Voidreaver (P2) fires ability` — no `KHA'ZIX: enemy alone` line, no `XP:` line.
  The class (`src/cards/units/*kha_zix_mutating*.cpp`, `onTrigger`) counts enemy units at resolution and has no `triggerCondition`.
- should: the lone attacker made the condition true when Kha'Zix became a defender; the ability resolves in full: +2 [M] this turn and 2 XP. Conversely, when the condition is false at that moment the ability must not be put on the chain at all.
- confidence: CERTAIN (CR 383.2.a.1 in terms).
- also seen: 25 of the 73 Kha'Zix triggers in the 720 games were put on the chain although the condition was false and resolved with nothing (khazix__rengar__s20261237__random__random D:103.05, viktor__khazix__s20261359__random__random D:137.05 — three defenders, khazix_sb__vex_sb__s20262239__random__random D:259.05): each is a chain item with a priority window that should not exist. Same pattern, condition inside the trigger event: Evelynn, Entrancing ("When you play me from face down on your turn") — 27 hand plays and 8 facedown plays on the opponent's turn put a trigger on the chain (azir_sb__khazix_sb__s20262125__random__random D:007.02 ... `[D:009.02] RESOLVE: Evelynn, Entrancing`); Tornado Warrior ("When you play me from face down") — 65 hand plays (azir_sb__khazix_sb__s20262125__random__random D:220.02).

### D52  Star Spring: "the first time ... each turn" triggers a second time when the same card is played there again that turn

_Origin: reader 6, R6-2._

- happened: irelia__khazix__s20261197__random__random, turn 7 (`./build/riftbound --deck1 decks/irelia.txt --deck2 decks/khazix.txt --seed 20261197 --agent1 random --agent2 random --trace --debug`). Both plays chose "Play Scuttle Crab #615(0M) -> Star Spring" (replay options at D:080 and D:097).
  `[D:080.03] PLAY: Scuttle Crab (id=19, Unit, cost=2E)` / `[D:082.03] TRIGGER: Star Spring (P1) fires ability` / `[D:083.03] MAY_DECLINED: Star Spring: move another unit here to its base?` / `[D:095.03] EFFECT: bounce Scuttle Crab (id=19) to hand` (Gust) / `[D:097.03] PLAY: Scuttle Crab (id=19, Unit, cost=2E)` / `[D:099.03] TRIGGER: Star Spring (P1) fires ability` / `[D:099.05] MAY_PROMPT: Star Spring: move another unit here to its base? (P=P1, deferred to agent)`.
  Cause in the class: `isFirstHereThisTurn` answers true again when the recorded unit id equals the played one (kept for the finalize/resolve re-entry); a card returned to hand and replayed keeps its object id.
- should: no second trigger that turn.
- confidence: CERTAIN.
- also seen: the only other second trigger in a turn is in the 720-game tally (2 in 235); not read.

### D53  [Temporary] (Deceiver / Mirror Image Reflections; Dusk Rose Lab): the kill is done directly at the start of the Beginning Step, never as a triggered ability on the chain

_Origin: reader 6, R6-3._

- happened: azir__leblanc__s20263035__random__ismcts-sims20, D:078: `[D:078.06] ── Beginning Step ──` / `[D:078.07] KILL: Honest Broker (id=109, 3M, dmg=0)` / `[D:078.08] TOKEN CEASES TO EXIST (CR 183.1): Honest Broker (id=109)` / `[D:078.09] TRIGGER: Honest Broker (P2) fires ability` — no `TRIGGER:` / priority for the Temporary itself. With Dusk Rose Lab: akali__leblanc__s20261108__random__random D:194.06 — the Temporary kill precedes `TRIGGER: Dusk Rose Lab` in all 85 Beginning Steps that have both (script: orders `K0, T:Dusk Rose Lab` 66, `K0, K0, T:Dusk Rose Lab` 10, with another trigger between 9+...); never the other order. `GameEngine::beginningStep` kills the Temporary permanents in a loop before emitting the phase event.
- should: the Temporary ability and Dusk Rose Lab's ability trigger at the same time; their controller orders them (CR 383.3.d), so the LeBlanc player may finalize Dusk Rose Lab first and pay its cost with the Reflection that is about to die (draw 1, and the copy's Deathknell on top); both players also get priority while the Temporary ability is on the chain.
- confidence: LIKELY (816.1 and 383.3.d say so; no FAQ entry read on this exact pairing).
- also seen: every Temporary kill in the run (277 Reflections; Sprites of other lists).

### D54  Tornado Warrior (Evelynn, Entrancing by code): the leading "you may" and the target are decided after RESOLVE

_Origin: reader 6, R6-6._

- happened: akali_sb__vex_sb__s20262113__random__random D:062-066: `[D:062.03] TRIGGER: Tornado Warrior (P2) fires ability` / `[D:063.01] CHOSE: [P2] PassPriority` / `[D:064.01] CHOSE: [P1] PassPriority` / `[D:064.02] RESOLVE: Tornado Warrior (chain_item=12, ability=1, spell=0)` / `[D:064.03] MAY_PROMPT: Tornado Warrior: empower something here? (P=P2, deferred to agent)` / `[D:065.04] TGT_PROMPT: Tornado Warrior: empower [Tomb-Raider Barbara/Boots of Swiftness/Tornado Warrior/Akali, Deadly Weapon]` / `[D:066.03] TGT_PICKED`. No `FINALIZE-CHOICES:` line; the class has no `choicesAtFinalization()`.
- should: may and target at finalization, before the opponent gets priority (the opponent responds knowing the target).
- confidence: CERTAIN for the "you may"; LIKELY for the target ("something here" is a choice of an object).
- also seen: all 36 prompted Tornado Warrior triggers. Evelynn, Entrancing's class is built the same way (`confirmOptional` / `pickTarget` with no finalize pass): CODE-ONLY for her, no trace reached her prompt (the one facedown play on her own turn with an enemy unit elsewhere, diana_sb__khazix_sb__s20262143__random__random D:057, had her bounced before resolution: under the rules the target would have been chosen at D:057).

### D55  LeBlanc, Everywhere at Once: the [Temporary] kill still happens at her battlefield, and the dying unit's other triggered abilities are suppressed instead

_Origin: reader 6, R6-7._

- happened: yasuao_sb__leblanc_sb__s20262378__random__random, D:273 (`./build/riftbound --deck1 <sb>/yasuao_sb.txt --deck2 <sb>/leblanc_sb.txt --seed 20262378 --agent1 random --agent2 random --trace --debug`). Board before: Star Spring [Ctrl:P2] `P2: Glasc Mixologist(5M,exh,Tmp,Dkn)  LeBlanc,Ever(7M,exh,EQ:B.F. S..)  Ruined Rex(6M,exh,Dkn)  Glasc Mixologist(5M,exh,Dkn)`.
  `[D:273.04] ════ TURN 18 (P2) ════` / `[D:273.06] ── Beginning Step ──` / `[D:273.07] KILL: Glasc Mixologist (id=111, 5M, dmg=0)` / `[D:273.08] TOKEN CEASES TO EXIST (CR 183.1): Glasc Mixologist (id=111)` / `[D:273.09] TRIGGER_SUPPRESSED: Glasc Mixologist (Temporary) at LeBlanc's battlefield` / `[D:273.10] ── Scoring Step ──`.
  `GameEngine::beginningStep` kills every Temporary permanent of the turn player without asking; `TriggerManager::temporaryTriggerSuppressed` (called from `fireTrigger`) drops every triggered ability whose source is a friendly unit with [Temporary] at her battlefield — here the copy's [Deathknell].
- should: the Reflection copy is not killed (its Temporary does not trigger); had it died some other way, its [Deathknell] would trigger normally.
- confidence: CERTAIN (both halves contradict the printed sentence).
- also seen: the only `TRIGGER_SUPPRESSED` line in 720 games; no Reflection survived a Beginning Phase of its controller in any game (script: 277 died before or at it, 22 alive at game end with none passed).

### D56  LeBlanc, Fragmented: Awaken Phase is counted as "your Beginning Phase"

_Origin: reader 6, R6-8._

- happened: class only — `my_beginning` is true for `TurnPhase::AwakenPhase`, `BeginningStep`, `ScoringStep`.
- should: 2 cards only in the Beginning Step / Scoring Step of her controller's turn.
- confidence: CODE-ONLY. Board that would show it: she dies during her controller's Awaken Phase (needs an effect that kills in that phase; none in these lists). In the traces the count was right in all 122 resolutions (draw 2: 42, all in the controller's Beginning / Scoring Step; draw 1 otherwise, including 5 in the opponent's Beginning Phase).

### D57  Void Burrower / Void Rush + Cleave, Smite (Blood Rush): a spell played by an effect that needs a target resolves with none

_Origin: reader 7, R7-1._

- happened: the cost is paid, the spell goes on the chain, and it resolves with no effect line.
  diana__reksai__s20261146__random__random D:226-229 (P2 has six units on the board):
  `[D:226.03] VOID BURROWER: banished Cleave then play it` / `[D:226.05] PAY_COST: Cleave E=1 P=0` / `[D:229.02] RESOLVE: Cleave (chain_item=54, ability=0, spell=1)` / `[D:230.00] DECISION #254 (P2): 11 legal actions`.
  azir_sb__reksai_sb__s20262128__random__random D:075-080: `[D:075.03] VOID BURROWER: banished Smite then play it` / `[D:075.05] PAY_COST: Smite E=2 P=1` / `[D:080.02] RESOLVE: Smite (chain_item=17, ability=0, spell=1)` / next line is the main-phase decision.
  mel__reksai__s20263036__ismcts-sims20__random D:169-171 (through Void Rush): `[D:169.05] PAY_COST: Cleave E=0 P=0` / `[D:169.06] VOID RUSH: banished and played Cleave, its cost reduced by [2]` / `[D:171.02] RESOLVE: Cleave (chain_item=40, ability=0, spell=1)` / nothing.
- should: the player chooses the unit as the spell is played and the spell does its effect.
- confidence: CERTAIN. Cause read in the code: `setQueueSpell` (game_engine.cpp:69) calls `addSpell(spell, p, {})` and then `chooseTargetsAtFinalize`; that serves cards that pick with `pickTarget` (`needsPlayTimeTarget`), but Cleave, Blood Rush and Smite read the play intent's `targets`, which is empty here.
- also seen: reksai__yi_rebuild__s20261296__random__random D:025-028 (Cleave). Over all logs 7 of 78 Cleave resolutions have no effect line. Falling Star and Cull the Weak played the same way do make their choices (they use the helpers).

### D58  Rek'Sai, Breacher + Void Burrower: a unit played from banishment is not offered Accelerate

_Origin: reader 7, R7-2._

- happened: reksai__khazix__s20261287__random__random D:096-100, Rek'Sai, Breacher on the board (board at D:096: `P1: Rek'Sai,Brea(3M,exh,Asl,Acc)  Noxus Hopeful(4M,exh)`), P1 has 10 ready runes:
  `[D:098.03] VOID BURROWER: banished Carrion Dredger then play it` / `[D:098.05] PAY_COST: Carrion Dredger E=2 P=0` / two rune picks / `[D:100.02] VOID BURROWER: recycled the rest` / `[D:101.00] DECISION #110 (P1): 2 legal actions`. No "pay [1][..] for Accelerate" decision.
  reksai__ezreal__s20263014__random__ismcts-sims20 D:169-173, Rek'Sai in P1's base, 12 ready runes: `[D:171.03] VOID BURROWER: banished Honest Broker then play it` / `[D:171.05] PAY_COST: Honest Broker E=2 P=0` / `[D:173.02] VOID BURROWER: recycled the rest`. No Accelerate decision.
- should: the player is asked whether to pay [1] + a Power of the unit's Domain to have it enter ready.
- confidence: CERTAIN. In the code the grant is only marked (`card_counters["__granted_accelerate"] = 1` in the cost-payment function, game_engine.cpp:7362-7371) and is consumed in `resolvePermanent`; a unit played by an effect goes through `EffectExecutor::playIgnoringCost`, which never reaches it. No log of the 720 shows Accelerate offered on an effect play.
- also seen: the legend's whole interaction with its champion is this case; every unit Void Burrower / Void Rush played with Rek'Sai on the board is affected.

### D59  Rek'Sai, Breacher: the grant marked in D58 (R7-2) stays on the card; a later play from hand without Rek'Sai on the board is offered Accelerate

_Origin: reader 7, R7-3._

- happened: the same two cards, later in the same two games, back in hand, Rek'Sai, Breacher in the trash.
  reksai__khazix__s20261287__random__random D:167-170 (board at D:166: P1 base holds only `Gold`, `Trash: ... Rek'Sai, Breacher ...`): `[D:167.01] CHOSE: [P1] PlayCard Carrion Dredger` / `[D:167.04] PAY_COST: Carrion Dredger E=2 P=0` / `[D:170.00] DECISION #192 (P1): pay [1][Order] for Accelerate on Carrion Dredger? [decline|pay] [2 options]` / `[D:170.01] ACCELERATE: Carrion Dredger declined`.
  reksai__ezreal__s20263014__random__ismcts-sims20 D:298-301 (Rek'Sai in the trash): `[D:298.01] CHOSE: [P1] PlayCard Honest Broker` / `[D:301.00] DECISION #316 (P1): pay [1][Order] for Accelerate on Honest Broker? [decline|pay] [2 options]` / `[D:301.01] ACCELERATE: Honest Broker declined`.
- should: no Accelerate offer.
- confidence: CERTAIN. These are the only two "ACCELERATE: <non-Accelerate card>" lines in the 720 logs, and both cards are the ones D58 (R7-2) marked (ids 27 and 33). Both were declined, so no unit entered ready wrongly in the traces.

### D60  Forbidding Waste: the -2 [M] is missing when the combat goes straight to the damage step

_Origin: reader 7, R7-4._

- happened: kaisa_sb__reksai_sb__s20262218__random__random, Forbidding Waste = BF#1, one P2 unit there.
  `[D:232.01] MOVE: Lecturing Yordle (id=8) -> BF#1` / `[D:232.02] COMBAT: BF#1 attacker=P1 defender=P2` / `[D:232.03] DAMAGE_STEP: BF#1 ATK=[Lecturing Yordle(2M)] DEF=[Ferrous Forerunner(6M)]` / `[D:232.06] ASSIGN_DAMAGE: P2 assigns 6 damage:`.
  `[D:234.03] DAMAGE_STEP: BF#1 ATK=[Thousand-Tailed Watcher(7M)] DEF=[Ferrous Forerunner(6M)]` / `[D:234.07]   -> Thousand-Tailed Watcher takes 6 (was 1/7M)` / `[D:234.10] KILL: Thousand-Tailed Watcher (id=15, 7M, dmg=7)`.
  Same game, same battlefield, with a chain item resolved between COMBAT and the damage step, it is applied: `[D:139.03] COMBAT: BF#1 ...` / `[D:141.02] RESOLVE: Daughter of the Void ...` / `[D:141.04] DAMAGE_STEP: BF#1 ATK=[Mech(3M), Mech(3M)] DEF=[Honest Broker(0M)]`.
- should: Ferrous Forerunner defends at 4 [M]; at D:234 it deals 4, the Watcher has 5 of 7 and survives.
- confidence: CERTAIN (the game outcome changed at D:234). Pattern over the lone-defender combats I listed at Forbidding Waste: applied whenever something resolved after the `COMBAT:` line, not applied when `DAMAGE_STEP` follows `COMBAT:` directly.
- also seen (not applied): reksai__zed__s20261297__random__random D:127.05 `DEF=[Carrion Dredger(1M)]` and it deals 1; mel_sb__reksai_sb__s20262272__random__random D:069.05 `DEF=[Mel, Newly Awakened(4M)]`; reksai__akali__s20261280__random__random D:081.06 `DEF=[Noxus Hopeful(4M)]`. Applied: akali_sb__reksai_sb__s20262110__random__random D:156.04 `DEF=[Mournful Witness(0M)]`; reksai__zed__s20261297 D:146.04 `DEF=[Shadowblade Lurker(3M)]`.

### D61  Inferna ([Assault 2]) played with [Ambush] into her side's attack is listed as an attacker without the Assault

_Origin: reader 7, R7-5._

- happened: rengar__vex__s20261311__random__random. `[D:287.02] COMBAT: BF#1 attacker=P1 defender=P2` / `[D:290.01] CHOSE: [P1] PlayReaction` / `[D:290.03] PLAY: Inferna (id=15, Unit, cost=2E)` / four chain items resolve / `[D:301.05] DAMAGE_STEP: BF#1 ATK=[Inferna(1M), Rengar, Trophy Hunter(6M)] DEF=[Scuttle Crab(0M), Steel Paws(7M)]` / `[D:301.06] ASSIGN_DAMAGE: P1 assigns 7 damage:`.
- should: Inferna 3 [M]; P1 assigns 9, enough for Scuttle Crab and Steel Paws (7) both.
- confidence: LIKELY (the engine itself lists her under ATK; one instance). In akali__rengar__s20261111__random__random D:102-115 an Inferna played as an [Action] in the showdown of her side's attack does show `Inferna(3M)`; the difference I can see is Reaction timing in a Closed State.
- also seen: none. The other `Inferna(1M)` attacker (diana__rengar__s20261147 D:346) is a Moonfall -2.

### D62  Thrill of the Hunt offers and plays to Rockfall Path

_Origin: reader 7, R7-6._

- happened: rengar_sb__reksai_sb__s20262308__random__random: `[D:168.03] MODE_PROMPT: Thrill: which battlefield [Emperor's Dais/Rockfall Path] (agent decides among 2 modes)` / `[D:169.03] MODE_PICKED: Thrill: which battlefield = mode#1 (Rockfall Path)` / `[D:169.04] EFFECT: banish First Mate (id=18)` / `[D:169.07] THRILL: First Mate played to Rockfall Path, ignoring cost`.
- should: Rockfall Path is not offered.
- confidence: CERTAIN. The class builds the list from every battlefield with `legal_mask` all ones; it does not use `playLocations` / `battlefieldTakesUnitsFrom`.
- also seen: kaisa__rengar__s20263025__ismcts-sims20__random D:070.07 `THRILL: Pit Rookie played to Rockfall Path, ignoring cost`. No other card puts a unit there: no replay of the 720 has a "Play ... -> Rockfall Path" option.

### D63  Rengar, Trophy Hunter in the Champion Zone is never offered as an [Ambush] play

_Origin: reader 7, R7-7._

- happened: akali__rengar__s20261111__random__random replay idx=25 (D:025), P2 has priority in a Closed State, 6 ready runes (`rdy:1R 5O`), `>> Champion Zone: Rengar, Trophy Hunter [6M | 5E 1P | READY TO PLAY]`, own unit at Forgotten Monument:
  `2 options: [0] Pass Priority [1] Play Grim Apothecary #583(3M) -> Forgotten Monument`. Grim Apothecary (in hand) is offered by its Ambush; Rengar is not.
- should: "Play Rengar, Trophy Hunter -> Forgotten Monument" is offered too.
- confidence: LIKELY (rules reading; the code is plain: both Ambush generators, game_engine.cpp:4227 and :4379, walk `player.hand` only, while `generateReactionPermanentActions` right above adds the Champion Zone). In the rengar list he is the Chosen Champion, so this is the legend-and-champion plan of the list. A scan of the rengar games for priority windows of the rengar seat with Rengar in the Champion Zone, at least 6 ready runes with a Body rune, a non-Rockfall battlefield holding any unit and no Rengar option gave 64 windows (approximate filter). His Ambush from hand works (see CHECKED).
- also seen: akali__rengar__s20263046__random__ismcts-sims20 idx 144 and 189.

### D64  Undertitan "As I'm revealed from your deck, [Add] [2]" waits on the chain and arrives after the reveal effect is over (K2)

_Origin: reader 7, R7-10._

- happened: reksai__rengar__s20261291__random__random: `[D:050.02] RESOLVE: Void Burrower` / `[D:050.03] TRIGGER: Undertitan (P1) fires ability` / `[D:051.03] VOID BURROWER: banished Undertitan then play it` / `[D:051.04] EFFECT: banish Undertitan (id=40)` (no PAY_COST: not affordable, it stays banished) / `[D:051.05] VOID BURROWER: recycled the rest` / two priority decisions / `[D:053.02] RESOLVE: Undertitan (chain_item=11, ability=1, spell=0)` / `[D:053.03] EFFECT: P1 adds 2 floating Energy (pool=2)`.
- should: the [2] is added as the card is revealed, so it can pay for the card the same effect lets the player play.
- confidence: LIKELY (K2 says [Add] abilities do not use the chain; here the delay changed what could be paid).
- also seen: reksai__yi_rebuild__s20261296__random__random D:028-030 (Energy added after the Cleave the same reveal played was paid); reksai__reksai__s20261290__random__random D:036.

### D65  Mel, Newly Awakened: the extra -1 [M] is added to every -[M] her controller's resolving item gives, chosen unit or not

_Origin: reader 7, R7-11._

- happened: not in a trace. `EffectExecutor::giveTemporaryMight` (effect_executor.cpp:978-982) subtracts `extra_minus_might` of the resolving item's controller for any negative amount; it does not know whether the unit was chosen.
- should: only for a unit the spell or ability chose.
- confidence: CODE-ONLY. Board that shows it: Mel Empowered; her controller resolves an effect that gives -[M] to units it does not choose, e.g. a Moonfall ("-2 [M] to enemy units at the battlefield") taken with Rebuttal.
- also seen: every traced -[M] by her controller while she was Empowered went to a chosen unit and was right (see CHECKED).

### D66  Herald of the Arcane: the Recruit always goes to the base; no location is offered

_Origin: reader 8, R8-1._

- happened: akali__viktor__s20261114__random__random, decision #154 = D:141. Board: `Dusk Rose Lab [Ctrl:P2]` with three P2 units. `[D:141.01] CHOSE: [P2] ActivateAbility (src=Herald of the Arcane)` ... `[D:143.02] RESOLVE: Herald of the Arcane (chain_item=34, ability=1, spell=0)` / `[D:143.03] TOKEN: created Recruit (1M, id=112) for P2` — no prompt; the next board shows the Recruit in P2's base. Class: `loc = LocationId{BaseLocation{ctx.controller}}`.
- should: the controller chooses the base or a battlefield they control.
- confidence: LIKELY (355.2.a applied to a played token; nothing in the text names the base, unlike Viktor, Leader's "into your base").
- also seen: 93 activations were made while the player controlled a battlefield (tally over `*viktor*.replay.txt`), e.g. azir__viktor__s20261132__random__random decisions #192, #197.

### D67  Swift Scout, Guerilla Warfare: the objects are chosen after RESOLVE; Swift Scout can be activated with nothing to choose

_Origin: reader 8, R8-2._

- happened: akali__teemo__s20261112__random__random D:103-106: `[D:103.03] PAY_ADD: P2 pays [1] + 0 power` ... both players pass ... `[D:105.02] RESOLVE: Swift Scout` / `[D:105.03] TGT_PROMPT: Swift Scout (Teemo unit to hand) [Teemo, Strategist/Teemo, Strategist]` / `[D:106.03] TGT_PICKED`. With nothing to choose: diana__teemo__s20261148__random__random D:247-249: `PAY_ADD: P2 pays [1]` ... `[D:249.03] TGT_NOT_OFFERED: Swift Scout (Teemo unit to hand) (no legal targets at resolve time)`. Guerilla Warfare: azir__teemo__s20261130__random__random `[D:094.02] RESOLVE: Guerilla Warfare` then `[D:095.00] DECISION #95 (P2): Guerilla Warfare: return a [Hidden] card from your trash (up to two) [7 options]`.
- should: the Teemo unit / the trash cards are chosen as the ability / spell is finalized (the opponent then reacts knowing them); Swift Scout is not offered when the player owns no Teemo unit in the Champion Zone or on the board.
- confidence: LIKELY (rests on 355.10 making these objects targets; if they are, 403.2 / 403.3 say the rest in terms). Same family as K3.
- also seen: Swift Scout resolved with no legal unit 109 times (tally of `TGT_NOT_OFFERED: Swift Scout`), i.e. [1] and the legend's exhaust spent for nothing.

### D68  Teemo, Strategist (every "When I defend / attack"): a unit that joins a combat already under way never triggers

_Origin: reader 8, R8-3._

- happened: teemo__viktor__s20261330__random__random: `[D:140.04] COMBAT: BF#1 attacker=P2 defender=P1` ... `[D:146.00] DECISION #161 (P1 — chain priority): 3 legal actions` / `[D:146.01] CHOSE: [P1] PlayReaction` (replay: `==> P1: Play Teemo, Strategist #121(2M)`, board `[HIDDEN: P1 — facedown: Teemo, Strategist]` at that battlefield) — no `TRIGGER: Teemo, Strategist` follows — `[D:158.06] DAMAGE_STEP: BF#1 ATK=[Recruit(1M)] DEF=[Sprite(3M), Teemo, Strategist(2M), Nocturne, Horrifying(4M), Vex, Apathetic(4M)]`. He defended and his ability never went on the chain.
- should: when he enters and gains the Defender designation, "When I defend" triggers (once per combat).
- confidence: CERTAIN (383.4.f in terms). Code: `TriggerManager` fires `WhenIDefend` / `WhenIAttack` only on the combat-start event (`trigger_manager.cpp:719-755`).
- also seen: 4 of 4 plays of a hidden Teemo, Strategist into a combat as defender: teemo__akali__s20261316__random__random D:095, teemo_sb__leblanc_sb__s20262324__random__random decision #148, teemo__reksai__s20261326__random__random decision #174. When he was on the battlefield before the combat started the trigger fired (10 of 10). The same applies to any unit that arrives late (Ambush, a token played into the combat, an effect move) and to "When I attack".

**Further evidence — reader 6, D68 (R6-5): Vi, Peacekeeper / Kha'Zix, Mutating Horror: "When I attack" does not trigger for a unit that joins a combat already under way ([Ambush])**

- happened: ezreal_sb__leblanc_sb__s20262162__random__random, D:237-245: `[D:237.01] CHOSE: [P2] PlayReaction` / `[D:237.03] PLAY: Vi, Peacekeeper (id=84, Unit, cost=5E)` ... `[D:245.08] DAMAGE_STEP: BF#1 ATK=[Vi, Peacekeeper(5M), Glasc Mixologist(5M)] DEF=[Kha'Zix, Mutating Horror(6M)]` — Vi deals combat damage as an attacker, and there is no `TRIGGER: Vi, Peacekeeper` between the play and the damage step.
- should: Vi's "When I attack, [Stun] an enemy unit here" is put on the chain when she gains the Attacker designation.
- confidence: LIKELY (the three rules together say it; no example in the rules names Ambush).
- also seen: Vi 6 times (akali__viktor__s20261114__random__random D:232.07, diana_sb__viktor_sb__s20262150__random__random D:074.06), Kha'Zix 4 times played into the attacking side (azir_sb__ezreal_sb__s20262121__random__random D:113.06, diana_sb__vex_sb__s20262149__random__random D:287.05, ezreal_sb__irelia_sb__s20262159__random__random D:427.03) — for Kha'Zix the "alone" condition was not checked by me in those four. No trace has either unit Ambushed into the defending side of a combat.

### D69  Sprite Call played from facedown: the Sprite may be put in the base or at another battlefield

_Origin: reader 8, R8-4._

- happened: ezreal__teemo__s20261166__random__random: `[DBG] [D:372.02] HIDDEN: Sprite Call hidden at BF#0 by P2`; `[D:386.01] CHOSE: [P2] PlayReaction` (replay decision #406: `[HIDDEN: P2 — facedown: Sprite Call]` at Vilemaw's Lair, `==> P2: Play Sprite Call #94[S]`); `[D:388.03] MODE_PROMPT: Sprite Call: play the Sprite to [Base/Battlefield #0]`; `[D:389.03] MODE_PICKED: Sprite Call: play the Sprite to = mode#0 (Base)`; `[D:389.04] TOKEN: created Sprite (3M, id=114) for P2`.
- should: only the battlefield it was hidden at.
- confidence: CERTAIN.
- also seen: all 11 plays of Sprite Call from facedown offered the base (2 also the other battlefield: teemo__viktor__s20261330__random__random D:157, teemo__yasuao__s20261331__random__random D:209); 6 chose the base (irelia__teemo__s20261202__random__random D:143, leblanc_sb__teemo_sb__s20262256__random__random D:101, teemo__khazix__s20261323__random__random D:096, teemo__yasuao D:209, vex__teemo__s20261346__random__random D:178). The destination is also chosen after RESOLVE (K3 family).

### D70  Windsinger played from facedown: a unit at another battlefield is offered

_Origin: reader 8, R8-5._

- happened: teemo_sb__ezreal_sb__s20262319__random__random, decision #293: Windsinger facedown at Grove of the God-Willow (P1 has Sneaky Deckhand and Tideturner there), P2's Pickpocket is at Vilemaw's Lair. `[D:270.01] CHOSE: [P1] PlayReaction` / `[D:270.03] TRIGGER: Windsinger (P1) fires ability` / `[D:271.03] MAY_ACCEPTED` / `[D:271.04] TGT_PROMPT: Windsinger [Tideturner/Sneaky Deckhand/Pickpocket] (agent decides among 3 targets)`.
- should: only Tideturner and Sneaky Deckhand (the units at Grove). The class builds the list from every battlefield and never reads `__played_from_facedown` / `__facedown_play_bf`.
- confidence: CERTAIN (an option that should not be there; the agent happened to pick Tideturner).
- also seen: only instance where a unit elsewhere existed and the may was accepted (8 plays from facedown in all).

### D71  Nocturne, Horrifying: the "As you look at or reveal me" replacement is a triggered ability on the chain

_Origin: reader 8, R8-6._

- happened: teemo__vex__s20261329__random__random: `[D:060.02] RESOLVE: Teemo, Strategist` / `[DBG] [D:060.03] TRIGGER: Nocturne, Horrifying (P1) fires ability` / `[D:060.04] TEEMO, STRATEGIST: revealed 5 — 4 Hidden` (Teemo's ability then finishes: damage, kill, recycle of all five revealed cards, Nocturne among them) / `[D:060.07] FINALIZE-CHOICES: Nocturne, Horrifying (chain_item=14, ability=1, spell=0)` / `[D:061.03] MAY_ACCEPTED` / `[D:062.03] MODE_PICKED ... (Play me for [A] to battlefield #1)` / `[D:062.04] PAY_ADD: P1 pays [0] + 1 power (any)` / `[D:063.00] DECISION #67 (P1 — chain priority)` / `[D:064.00] DECISION #68 (P2 — chain priority)` / `[D:064.02] RESOLVE: Nocturne, Horrifying` / `[D:064.03] EFFECT: banish Nocturne, Horrifying (id=35)`.
- should: the banish happens as he is revealed, inside the revealing effect, with no chain item and no priority window; he is then not among the cards Teemo recycles. Here he was recycled to the bottom, an item waited on the chain, both players got priority (the opponent may react, e.g. counter the ability), and he was banished from the bottom of the deck.
- confidence: CERTAIN that it is a replacement and not a triggered ability (the rules example); the exact step at which "you may play me for [A]" is carried out is not spelled out (see QUESTIONS 3).
- also seen: all 20 sightings are reveals by Teemo, Strategist; 14 accepted.

### D72  Scoring Step: battlefields are held one at a time, each hold's abilities resolved before the next hold

_Origin: reader 8, R8-7._

- happened: kaisa__vex__s20263055__random__ismcts-sims20: `[D:327.06] SCORE: P2 +1 via Hold BF#0 -> 4` / `TRIGGER: Gloomist (P2) fires ability` / may accepted / both players pass / `[D:330.02] RESOLVE: Gloomist` / draw / `[D:330.06] SCORE: P2 +1 via Hold BF#1 -> 5` / `[D:330.07] TRIGGER: Gloomist` / `[D:330.08] TRIGGER: Startipped Peak` / `[D:330.10] MAY_NOT_OFFERED: Gloomist: exhaust to draw 1?`.
- should: both battlefields are held together; all hold abilities (Gloomist twice, Grove, Startipped Peak) are pending together and their controller orders them; nothing an ability or a reaction does in between can change whether the second battlefield is held.
- confidence: LIKELY (315.2.b.2 reads as one action; the rules do not give an example).
- also seen: 12 `MAY_NOT_OFFERED: Gloomist` (second hold of a turn), all of this shape. Cards touched: Gloomist, Grove of the God-Willow, Startipped Peak.

### D73  Fortified Position: the [Shield 2] lasts the turn, not the combat

_Origin: reader 8, R8-10._

- happened: teemo__vex__s20261329__random__random `[D:066.03] EFFECT: give Scuttle Crab [Shield] (value=2) this turn`; the class calls `giveTemporaryKeyword`, whose only duration is the turn (`effect_executor.cpp:1010`; nothing in `src/engine` ends an effect with a combat).
- should: gone when that combat ends; a unit that defends again later in the turn (a second combat at the same or another battlefield) must not have it.
- confidence: CERTAIN for the duration (the engine's own line); the consequence is CODE-ONLY: in no traced turn did the chosen unit defend in a later combat (two combats after a grant: akali_sb__vex_sb__s20262113__random__random turn 9, where the unit attacked). A board that shows it: attacker moves into Fortified Position, defender chooses a unit at the other battlefield, that combat ends, the attacker then attacks the other battlefield the same turn.
- also seen: the trigger fired a second time in the same turn in 8 games (e.g. teemo__vex D:045 and D:067), each time a new grant on top of the first.

### D74  Facebreaker from facedown: offered with no friendly + enemy pair at its battlefield (and as up to 20 identical intents)

_Origin: reader 8, R8-11._

- happened: akali__viktor__s20261114__random__random decision #169 = D:154. Board: Dusk Rose Lab `[Ctrl:P2]`, P2 has Recruit, Keeper of Law, Carrion Dredger, Honest Broker there, P1 has no unit at any battlefield. Options: `[1] Play Facebreaker #220[S] @ Recruit` ... `[12] Play Facebreaker #220[S] @ Honest Broker #476` (12 options). `[D:154.01] CHOSE: [P2] PlayReaction` / `[D:154.03] FINALIZE-CHOICES: Facebreaker (chain_item=37, ability=0, spell=1)` / `[D:154.04] TGT_PAIR_NOT_OFFERED: Facebreaker (no legal first targets at resolve time)` / `[D:156.02] RESOLVE: Facebreaker` (nothing happens; the card is spent).
- should: not offered. From hand the card is gated by `hasLegalTargets`; `generateHiddenPlayActions` instead pairs any two units at the battlefield (`getTargetRequirements` count 2) without the friendly / enemy roles. The intent's targets are then ignored (the pair is asked in the finalize pass), so with a legal pair present the same play is offered many times under labels that mean nothing: rengar__viktor__s20261312__random__random decision #69, 20 "Play Facebreaker @ ..." options, chosen `@ Nidalee, Cat Form`, then `[D:066.04] TGT_PAIR_PROMPT_A: Facebreaker [Bird]`.
- confidence: CERTAIN (811.1.d in terms).
- also seen: 19 such plays (`TGT_PAIR_NOT_OFFERED: Facebreaker`), among them ezreal__viktor__s20263058__random__ismcts-sims20, vex__viktor__s20263052__ismcts-sims20__random, viktor__ezreal__s20263015__random__ismcts-sims20, viktor_sb__viktor_sb__s20262366__random__random (twice). The legal case was read and is right: viktor__kaisa__s20261358__random__random D:061-065 (only units at its battlefield offered, both stunned).

### D75  Master of Shadows: empowered when an OPPONENT banishes a card its controller owns

_Origin: reader 9, R9-1._

- happened: kaisa__zed__s20261225__random__random, D:041. P1 (kaisa) played Temporal Breach ("Banish a unit, then its owner plays it ...") at D:021:
  `[D:041.02] RESOLVE: Temporal Breach (chain_item=4, ability=0, spell=1)` / `[D:041.03] EFFECT: banish Noxus Hopeful (id=58)` / `[D:041.04] TRIGGER: Master of Shadows (P2) fires ability` / `[D:043.03] EFFECT: empower Master of Shadows (id=54) -> 1`.
  viktor_sb__zed_sb__s20262369__random__random, D:124: P1's Ashe, Focused ("Choose a card revealed this way and banish it") : `TGT_PICKED: Ashe Focused: banish a revealed card = Seal of Discord (id=62)` / `EFFECT: banish Seal of Discord (id=62)` / `TRIGGER: Master of Shadows (P2) fires ability` / `[D:126.03] EFFECT: empower Master of Shadows (id=54) -> 1`.
- should: no trigger; the opponent performed the banish. (`TriggerManager::onObjectStateChanged`, "banished_owned", fires for the card's owner whoever banished it.)
- confidence: CERTAIN (CR 412.4 in terms).
- also seen: kaisa__zed__s20263070__random__ismcts-sims20 D:213; teemo__zed__s20261333__random__random D:146; zed_sb__teemo_sb__s20262418__random__random D:188 (all opponent's Temporal Breach); zed_sb__viktor_sb__s20262420__random__random D:160, D:226 (opponent's Ashe, Focused). The reverse case is right: zed banishing an opponent's card does not trigger (zed__reksai / Smite on a token, zed_sb__reksai_sb__s20262416 D:256).

### D76  Wuju Bladesman - Starter: the +2 [M] is missing whenever nothing resolves between "COMBAT:" and the damage step

_Origin: reader 9, R9-2._

- happened: akali__yi_rebuild__s20261116__random__random, D:100 (Master Yi, Tempered, printed 4 [M], the only defender):
  `[D:100.06] COMBAT: BF#0 attacker=P1 defender=P2` / `[D:100.07] DAMAGE_STEP: BF#0 ATK=[Akali, Deadly Weapon(3M)] DEF=[Master Yi, Tempered(4M)]` / `-> Master Yi, Tempered takes 3 (was 2/4M)` / `ASSIGN_DAMAGE: P2 assigns 4 damage:` / `KILL: Master Yi, Tempered (id=55, 4M, dmg=5)`.
  With the bonus it is 6 [M], assigns 6 and survives 5 damage; P2 would have kept the battlefield.
  khazix__yi_rebuild__s20261242__random__random D:060: `DEF=[Lonely Poro(2M)]`, takes 2, killed (4 [M] would survive). irelia__yi_rebuild__s20263066__random__ismcts-sims20 D:031: `DEF=[Pit Rookie(2M)]`, killed by 2.
  Where a chain item resolved inside the showdown the bonus is there: yi_rebuild__zed__s20263064__ismcts-sims20__random D:228 `DEF=[Master Yi, Tempered(6M)]` (a Death Mark and a Master of Shadows trigger resolved first); yi_rebuild__khazix__s20261395__random__random D:118 `DEF=[Lonely Poro(4M)]` (Sabotage resolved first) — and in the same line group, the next combat, `DEF=[Scuttle Crab(0M)]` without it.
- should: +2 from the moment the unit is the only defender.
- confidence: CERTAIN. Of the 60 lone-defender combats on the yi side, 42 go from `COMBAT:` straight to `DAMAGE_STEP:` (bonus missing in every one whose printed Might I checked) and 18 have chain activity in between (bonus present). The aura is only written when auras are recalculated, and nothing recalculates between designation and damage.
- also seen: (a) Fiora, Peerless defending one on one shows 8 [M] (akali_sb__yi_rebuild_sb__s20262116__random__random D:158, yi_rebuild_sb__zed_sb__s20262405__random__random D:055): her trigger doubled 3 because the +2 was not yet applied, then +2 was added; with the legend applied on time the doubling at resolution is of 5 -> 10. (b) The same lag the other way: Fiora's "this combat" bonus is still shown (6M, ATK flag gone) during the three decisions of the conquer trigger after the combat (akali_sb__yi_rebuild_sb__s20262116 idx 148-150), and is gone at idx 151.

### D77  Yasuo, Windrider: an ability is put on the chain for every move, not only "the third time I move in a turn" (same pattern: Fiora, Peerless when not one on one)

_Origin: reader 9, R9-5._

- happened: fiora__yasuao__s20263062__random__ismcts-sims20, D:069, Yasuo's first move of the turn:
  `[D:069.03] MODE_PICKED: Unforgiven: move it to which battlefield? = mode#0` / `[D:069.04] TRIGGER: Yasuo, Windrider (P2) fires ability` / `DECISION #73 (P2 — chain priority)` / `DECISION #74 (P1 — chain priority)` / `[D:071.02] RESOLVE: Yasuo, Windrider (chain_item=14, ability=1, spell=0)` (nothing happens) / `SHOWDOWN: BF#0 (non-combat)`.
- should: nothing on the chain for the first and second move; no priority window, no Closed state.
- confidence: LIKELY. 139 of the 142 Yasuo triggers are for a first or second move; the move count is incremented when the item resolves (`onTrigger`), so it is also lost if that item never resolves. Reactions were played into such a window: yasuao__diana__s20261372__random__random D:172 (opponent activates a Reaction ability on top of a non-third-move trigger), azir__yasuao__s20261133__random__random D:267.
- also seen: Fiora, Peerless ("When I attack or defend one on one"): 14 of 19 triggers were put on the chain with more than one attacker or defender and resolved as nothing (yi_rebuild_sb__fiora_sb__s20262392__random__random D:254; kaisa_sb__yi_rebuild_sb__s20262224__random__random D:155); the condition is read at resolution, so removing a unit in response would switch the doubling on.

### D78  Kennen, Storm of Shuriken: the spell that gets [Flow] is chosen at resolution

_Origin: reader 9, R9-6._

- happened: vex_sb__zed_sb__s20262351__random__random, D:164-167: `[D:164.08] TRIGGER: Kennen, Storm of Shuriken (P2) fires ability` / both players pass / `[D:166.02] RESOLVE: Kennen, Storm of Shuriken` / `[D:166.03] TGT_PROMPT: Kennen, Storm of Shuriken: grant [Flow] [Perfect Execution/Piercing Light/Ride the Wind/...]` / `[D:167.03] TGT_PICKED: ... = Ride the Wind (id=73)`.
- should: the spell is chosen as the ability is finalized, before the priority window.
- confidence: LIKELY (K3 family; the class has no `kFinalizeTrigger`). All 24 grants in the traces are picked after `RESOLVE:`.

### D79  Switcheroo on a unit with Deflect (Master Yi, Tempered at Level 6): Deflect is charged for the play option's unit, then both units are asked again — charged without choosing it, or charged twice

_Origin: reader 9, R9-7._

- happened: diana__yi_rebuild__s20261152__random__random, idx 194 / D:183. Option chosen: `Play Switcheroo #466[S] @ Master Yi, Tempered #675`.
  `[D:183.05] PAY_ADD: P1 pays [0] + 1 power (any): exhausted 0, recycled 1` / `[D:183.06] DEFLECT: P1 pays 1 [A] to choose Master Yi, Tempered` / `[D:183.08] TGT_PAIR_PROMPT_A: Switcheroo [Diana, Lunari/Ravenbloom Student/Master Yi, Tempered/...]` / `[D:184.03] TGT_PAIR_PICKED_A: Switcheroo = Diana, Lunari (id=2)` / `[D:185.03] TGT_PAIR_PICKED_B: Switcheroo = Ravenbloom Student (id=6)`.
  The rune was recycled; Master Yi was not chosen.
- should: Deflect paid once per time the unit is actually chosen, none here.
- confidence: CERTAIN for the charge without a choice (trace is unambiguous).
- also seen: charged twice for one choice — diana__irelia__s20261141__random__random D:077.04 `DEFLECT: P2 pays 1 [A] to choose Vex, Apathetic`, then `TGT_PAIR_PICKED_B: Switcheroo = Vex, Apathetic` and D:079.05 `DEFLECT: P2 pays 1 [A] to choose Vex, Apathetic` again. Same shape (Deflect line before `TGT_PAIR_PROMPT_A`): fiora__teemo__s20261184 D:160, vex__irelia__s20261339 D:177, yasuao__diana__s20261372 D:259, yasuao__khazix__s20261377 D:190 (Switcheroo), fiora__viktor__s20261186 D:117 (Facebreaker). Deflect on triggered and activated abilities is right (teemo__yi_rebuild D:197, yi_rebuild__ezreal D:193).

### D80  Lonely Poro: "died alone" when every other friendly unit there died in the same damage step

_Origin: reader 9, R9-10._

- confidence: QUESTION.

### D81  Minefield: no Burn Out when the Main Deck has fewer than 2 cards

_Origin: reader 9, R9-11._

- happened: not in a trace. `Minefield::onTrigger` loops `for (i < 2 && !ps.main_deck.empty())` and stops.
- should: with 0 or 1 card left, move what is there, Burn Out (opponent gains a point), then move the rest — as Death Mark's Burn 3 does (vex__zed__s20261351 D:384, `BURN_OUT ... P1 gains 1 point (CR 432.2.c) -> 8`).
- confidence: CODE-ONLY. Board: a player conquers Minefield with 0 or 1 card in the Main Deck. (It also emits no "burned" event; no card in the engine listens to `WhenYouBurn`, so nothing shows.)

## 6. Checked and correct

**Lead (engine-level, by script and by hand)**

- A unit that is rendered ready just after it is played — it is exhausted at the first top-level decision after it has entered, in all 15,326 unit plays of the 1,116 games, unless Accelerate was paid (345) or the card says so (Xin Zhao, Vigilant, 41). The ready rendering in between is D1. — CR 359.2.c
- Akali, Deadly Weapon moving base -> battlefield is offered only units at that battlefield (herself included), not units in the base — "a battlefield I moved to or from"; a base is not a battlefield. — `akali__akali__s20261100__random__random` D:021-025
- A showdown at the battlefield Akali moved to opens only after her move ability has resolved — CR 323.11 (a showdown begins in a Neutral Open State). — same game, D:025.05
- Abandoned Hall ("When a player plays a spell") triggers when the spell resolves and not for a spell that was countered — CR 350.1 and the example of 359.3.e.10 ("the unit's ability still triggers as the spell resolves"). — `akali__diana__s20261102__random__random` D:024
- Burn Out: with an empty Main Deck the trash is recycled, the opponent gains 1 point, then the draw is made; a Burn Out point may be the winning point — CR 432.2, 473.1.a.1. Six in the 1,116 games; `vex__zed__s20261351__random__random` D:384 ends the game.
- The winning point by Conquer is refused, and a card drawn instead, unless every battlefield was scored that turn — CR 473.1.b.1; 192 refusals, none wrong; with Baron Pit on the board the rule counts three battlefields (`teemo__diana__s20261318__random__random` D:452).
- A player at 7 who controls two battlefields holds one and the game ends (121 turns): the second hold is never logged — CR 323.1.
- Recycling a ready rune for Power is offered by the card-cost prompt and loses its Energy — CR 164.2.b.
- More than 80 cards on a rendered board in LeBlanc games: Reflection tokens are rendered under the name of the unit they copy (all 117 games with an excess have the leblanc list in one seat; the excess is 1 to 3).
- The Deflect of a unit chosen by a spell that is offered once per target (`ChainManager::payDeflectForChoices`, a lead of the fix run) is paid in `addSpell`, which `executePlaySpell` calls after `payCardCost`: it is paid after the spell's own cost. No `DEFLECT_UNPAID` line and no engine error in the 1,116 games (485 Deflect payments). That path is nevertheless wrong for the pair-target spells: see R9-7 (Switcheroo and Facebreaker pay it for the unit named by the play option and then ask for both units again).

**Reader 1**

- Akali, Deadly Weapon offers herself as a target after moving to an empty battlefield — "a unit at a battlefield I moved to or from" includes her (CR 355.9.c: an ability may target its source) — akali__azir__s20263000 D:077.
- Akali's move trigger after Rogue Assassin pulls her to base lists the units at the battlefield she left and not herself — "moved to or from"; the base is not a battlefield — akali__reksai__s20261110 D:081.
- Akali's trigger resolves before the combat she moved into opens (`COMBAT:` after `AKALI, DEADLY WEAPON: moved`) — the showdown opens in a Neutral Open State cleanup, after the chain is empty (CR 344.1, 323.12) — akali__reksai D:077.
- Empowered Akali deals 2, +1 at Void Gate = 3; not Empowered 1 — card text + Void Gate — akali__reksai D:077 / D:084; 50 x `dealt 1`, 42 x `dealt 2`.
- Akali's Empower pays its Power with a Fury rune ([2][R]) — board before/after akali__fiora__s20261104 D:225, akali__irelia__s20261105 D:201.
- Rogue Assassin: target chosen at finalization, legend exhausted as the cost, the move triggers "When I move" (Akali, Stellacorn Herder) but not Akali, Silent's "move to a battlefield"; unit readied only when the legend is Empowered — CR 135.2.b.5.b example — azir ismcts D:081-086 (ready, 4M in base) vs akali__azir__s20261101 D:110-113 (stays exhausted).
- Rogue Assassin pulls the last attacker out of a combat: combat ends, Mournful Witness's "combat I was in ends" fires — akali__fiora__s20261104 D:217.
- Rogue Assassin mistarget when the unit is bounced in response (Gust) — CR 359.3.e.5 — irelia__akali__s20261190 D:098.
- Rogue Assassin's move ability is never offered on the opponent's turn — its effect would do nothing there ("If it's your turn" covers both instructions, CR 135.2.b.5.b); withholding it costs nothing but the option to exhaust the legend. Recorded as correct in practice.
- Zhonya's Hourglass played from facedown on the opponent's turn with no cost line, then `RECALL (CR 323.6)` to base — hidden since an earlier turn, has [Reaction], is played to that battlefield, an unattached gear at a battlefield is recalled (CR 811.1.b / .d.1, 323.6) — azir ismcts D:220 / D:239.
- Zhonya's Hourglass is mandatory and saves a token; with two copies the controller is asked which applies first (CR 372) — akali__irelia__s20263001 D:116.
- A recalled unit keeps Empowered and does not trigger "When I move" — "This isn't a move", it never left the board — azir ismcts D:242, board at D:243; D:173.13.
- A facedown Zhonya's Hourglass never replaced a death: over all games the replacements per game never exceed face-up plays + facedown plays (the one apparent excess, zed_sb__yi_rebuild_sb__s20262422 D:181, is a facedown play at D:179 that prints no PLAY line).
- Astral Heron triggers when the first card (a spell) has resolved, after a card played in response — CR 420.4.a — akali__ezreal__s20261103 D:173-181; its discount applies to the Flow cost ([3][A] -> [1]) and to Power (Blitzcrank 5+1 -> 3+0).
- Defy countered nothing above [4] or above one Power in any game (60 distinct spells checked against printed costs); it may counter its controller's own spell ("a spell").
- Back Off from facedown: stun but no draw — "If you played this from your hand" — akali__khazix__s20263031 D:218.
- Sigil of the Storm asks for the rune at resolution, not at finalization — "must" instruction, CR 355.10.f — azir ismcts D:160.
- Forgotten Monument: the conquer attempt on a player's 2nd turn takes control without a score or a conquer trigger; `turns_taken` counts the current turn (the trace numbers the first turn 0) — akali_sb__leblanc_sb__s20262108 D:021.
- Blitzcrank pulling an enemy unit starts a combat with the pulled unit's controller as attacker — that player's unit applied Contested (CR 323.8) — akali__zed__s20261117 D:102.
- Frozen Fortress and Vex, Apathetic affect an out-of-combat Akali, Silent — neither chooses her (area damage; "stun it" refers to the played unit) — ezreal__yasuao__s20261169 D:302, teemo__yasuao__s20261331 D:221.

**Reader 2**

- Emperor's ability offered right after an Equipment play and never without one — "Use only if you've played an Equipment this turn" (CR 377.2.b) — 588 offers / 78 games, 0 exceptions; only in the controller's own Neutral Open State.
- Sand Soldier equips for nothing through Weaponmaster (`PAY_ADD: P2 pays [0] + 0 power`) — Equip [G] / [Y] reduced by one Power (CR 821.1.c) — akali__azir__s20261101 D:080; 196 payments, all 0. (See QUESTIONS 1 for the letter of 821.1.c.3.)
- Weaponmaster takes an Equipment off another unit (`EFFECT: unattach B.F. Sword`) — "even if it's already attached", CR 821.1.c.5 — akali__azir__s20261101 D:080.05.
- A normal Equip of an attached Equipment is not offered — its Rules Text is Inactive while attached (CR 718.2, 721.2) — option lists, e.g. akali__azir__s20261101 idx=77.
- Equip offered only when a rune of the Equip's Domain exists (Calm for Doran's Shield / Brutalizer / Soul Sword / Hand Hammer, Order for Eye of the Herald / B.F. Sword, Chaos for Boots / Edge of Night) — 6,607 offers, 0 without the Domain.
- Accelerate on Azir paid with an Order rune every time (13 payments compared with the rune counts before/after) — "[1][Y]".
- A token moved into a combat by Azir's trigger deals and takes combat damage — azir__azir__s20261119 D:347.04-09.
- Eye of the Herald fires when its bearer is moved by an effect (Azir's trigger), not only by a Standard Move — "When I move" — azir_sb__akali_sb__s20262118 D:247.03.
- Guards! from facedown offers only the battlefield it was hidden at (CR 811.1.d.5) — azir__kaisa__s20263005 D:151.03; from hand with no battlefield controlled only Base — azir__diana__s20263004 D:115.03.
- Guards!' and Arise!'s "Then do this" parts are separate chain items whose choices and cost are settled at their finalization (CR 388) — D:116.08 / azir__diana__s20261120 D:206.07.
- Hidden cards: 97 facedown plays of Guards! / Hidden Blade / Edge of Night / Pyke, all on a later turn than the hide and all without a `PAY_COST` (CR 811.1.b).
- Hand Hammer gives no +2 alone, with two other friendly units, or in base, and +2 with exactly one — games listed in the table.
- Seat of Power draws 0 when the other Seat of Power (opponent's copy) is the only other battlefield and not controlled — 336 resolutions consistent.
- Blade Dancer readying a ready Irelia does not trigger "ready me"; readying an exhausted one does (CR: "becomes ready") — irelia__yi_rebuild__s20263066 D:069 vs irelia__teemo__s20263051 D:130.03.
- Irelia's trigger does not fire when the opponent chooses her; the opponent pays [A] (any Domain) per choice — 58 `DEFLECT:` lines, e.g. irelia_sb__vex_sb__s20262203 D:037.05; her controller never pays.
- Irelia's Awaken-phase "+1 [M]" fires iff she was exhausted — 68 Awaken phases.
- Abandoned Hall's choice of a friendly unit triggers Blade Dancer and Irelia — kaisa_sb__irelia_sb__s20262213 D:141.04-05.
- Tideturner's swap choosing Irelia triggers both, and the Blade Dancer ready resolves before the swap — irelia__teemo__s20263051 D:125-134.
- Defiant Dance may choose enemy units and pays Deflect per enemy target; its Power is paid with a Chaos rune ([C] of a Calm/Chaos card, CR 135.2.e.6.c) — irelia_sb__vex_sb__s20262203 D:035-041.
- Decree of Unity only ever chose enemy Chaos units or gear, Equipment included (Edge of Night, Last Rites).
- Ashe, Focused: the banished card is picked at resolution from the revealed hand (the hand is not a public zone: not a target, CR 355.10.a) and returns on that opponent's next hold even after Ashe left — akali_sb__azir_sb__s20262101 D:133-142.
- Boots of Swiftness: battlefield-to-battlefield Standard Moves offered only to its bearer — 29 offers.
- Sunken Temple offered when the conquering unit is Mighty after combat (Vex, Cheerless 5M with 4 damage) — vex_sb__irelia_sb__s20262339 D:302.

**Reader 3**

- Ravenbloom Student / Abandoned Hall trigger only after the spell has resolved, not as it is played — CR 420.4.a ("trigger when the act of playing the card has been completed by the resolution of the card") — azir__diana__s20263004__ismcts-sims20__random D:164; no trigger for a countered spell (420.4.a.1) — diana__azir__s20261137__random__random D:241 fires for Hard Bargain itself only.
- Scorn's Energy spent only with a showdown or combat under way: 97 payments in the diana logs with one rune fewer than the Energy cost, all after a `SHOWDOWN:` / `COMBAT:` line of the same turn (RunePool::showdown_energy, resource_payment.h `showdownUnderway`). The one apparent exception, diana__ezreal__s20263008__ismcts-sims20__random D:085 (Switcheroo E=2, one rune), is the rune exhausted and then recycled for Vex's Deflect at D:084.04, whose Energy was still floating.
- Scorn's Energy added in the Beginning Step is gone by the Main Phase (pool empties at end of Draw Phase) — same game D:080 -> D:083.
- Scorn's Energy pays for anything during a showdown (spells, a Repeat, an ability cost) — the text restricts when, not what — yi_rebuild_sb__diana_sb__s20262390__random__random D:145; teemo_sb__diana_sb__s20262318__random__random D:165 (`pays [3] ... exhausted 2`).
- Diana's effect order: Predict (private look, may recycle), then reveal the new top, draw it if a spell — diana__azir__s20261137__random__random D:079-080 (`PREDICT: recycled Fizz, Trickster` / `revealed spell Eclipse -> draw it`); non-spell stays — diana__fiora__s20261140__random__random D:028.
- Tideturner from facedown may choose a unit anywhere — CR 811.1.d.3, whose example is this card — vex__mel__s20261343__random__random D:191.
- Tideturner's swap is a move for the other unit (Hwei triggers) — azir__diana__s20263004__ismcts-sims20__random D:264; a Tideturner stunned by Vex ("can't move it this turn") does not move itself — diana__khazix__s20261143__random__random D:365.
- Ride the Wind on a unit that can't move this turn still readies it ("and ready it") — diana__yasuao__s20261151__random__random D:163.
- Acceptable Losses: each player names their own gear at resolution, nothing is targeted — CR 355.10.e — diana__teemo__s20261148__random__random D:174.
- Moonfall moving Hwei fires the enemy Hwei's "When I move" — mel__diana__s20263010__random__ismcts-sims20 D:201; Moonfall's battlefield, "may" and unit are chosen before the cost — diana__diana__s20261138__random__random D:301-304.
- Hwei's move trigger (and its reflexive part) resolves before the combat it caused begins — diana__khazix__s20263009__ismcts-sims20__random D:166-171.
- Gust offers only units at battlefields with Might 3 or less — akali__irelia__s20263001__ismcts-sims20__random D:220 (2 options; 4M, 11M and the base unit absent).
- Deflect for Vex is paid as the unit is chosen, ahead of the spell's cost — diana__vex__s20263054__random__ismcts-sims20 D:187; the player's own Vex costs nothing (Tideturner, diana__ezreal__s20263008__ismcts-sims20__random D:469).
- Sprite Fountain: [Temporary] kills gear and token at the start of the controller's Beginning Step, before scoring; Deathknell makes a new Sprite — diana__mel__s20263039__random__ismcts-sims20 D:115-117.
- Targon's Peak names the runes when the delayed ability is finalized in the Ending Step — CR 355.5.b — diana_sb__rengar_sb__s20262147__random__random D:034.
- Turn to Dust's [Temporary] is permanent on the gear and kills it in its controller's next Beginning Step — diana_sb__akali_sb__s20262136__random__random D:249 -> D:264.

**Reader 4**

- Once per crossing: Kayle's first Empower (3 -> 5) triggers, the second and third (7, 9) do not — 25 / 10 / 3 `EFFECT: empower Kayle` lines, 25 triggers (CR 709 second example) — fiora_sb__leblanc_sb__s20262180__random__random D:323.03. Rampage's +2 on a 6 [M] Rengar: no trigger — akali__fiora__s20261104__random__random D:084.03.
- Crossing by a buff (Sett D:040.03 akali__fiora__s20261104__random__random), a "this turn" bonus (Discipline, Riposte, Divining Shells, Punch First), Equipment (`ATTACH: Shepherd's Heirloom -> Fiora, Worthy`, azir__fiora__s20261122__random__random D:120) and Empower all trigger both cards.
- A unit that becomes Mighty because a "-N [M] this turn" ends: it triggers. Right by CR 709 (the Might changes from <5 to >=5); only the timing is wrong (D35 (R4-1) c).
- Second unit in one event: the second Grand Duelist is `MAY_NOT_OFFERED` because the legend is already exhausted — akali_sb__fiora_sb__s20262104__random__random D:182.06. Correct (cost unpayable).
- Fiora, Worthy pays [Y] at finalization (CR 383.3.b) with an Order rune, an exhausted one first (script: 78 payments with an exhausted Order rune available, 78 took it); the unit killed in response is not readied and the Power stays paid — akali__fiora__s20261104__random__random D:041.04 / D:051.02.
- Fiora, Worthy readies herself when she is the unit — akali_sb__fiora_sb__s20262104__random__random D:190.03.
- Riposte on its controller's own spell (Mobilize) — "a spell", any controller — ezreal_sb__fiora_sb__s20262158__random__random D:024.03-028.01.
- Sacrifice kills the unit before the Energy is paid and keeps nothing to choose at resolution (additional cost) — akali__fiora__s20261104__random__random D:044.04-044.05.
- Rengar is never offered to Base or to an empty battlefield in a closed state / showdown (0 of 249), and Rockfall Path is left out — kaisa__yi_rebuild__s20261224__random__random idx 108.
- Poppy at opponent score 4 of 8: no ready, no XP ("within 3" = 5 or more) — akali__fiora__s20261104__random__random D:251.02.
- Elder Dragon: "up to one enemy unit at each location" asks only where there is an enemy unit and offers "None"; 1 damage kills (`ELDER_DRAGON: ... killed by any-damage rule`) — fiora__diana__s20261174__random__random D:245.03-245.05.
- Harnessed Dragon's list leaves out Ruin Runner (can't be chosen) — fiora_sb__yi_rebuild_sb__s20262188__random__random D:214.02.
- Fiora, Victorious at 6-4 = 2 and at 6-2 = 4 has no keywords and no Shield bonus (CR 478.4 second example) — diana_sb__fiora_sb__s20262140__random__random D:218.03, D:258.08.
- Risen Altar: "your" = the battlefield's controller (CR 190.6.d); Kayle there pays [2] — kaisa_sb__fiora_sb__s20262212__random__random D:188.03.
- Rift Herald's "you may reveal" and Dazzling Aurora's location are chosen at resolution: neither is the first "you may" and the cards are unknown before (CR 383.3.a.3, 355.18).

**Reader 5**

- Prodigal Explorer offered after ONE spell — Singularity chose two enemy units, each choice counts (see Q1) — akali__ezreal__s20261103 D:239.03, offer at decision #270.
- Prodigal Explorer after two Gusts on "Ezreal, Prodigy" — the target was the opponent's Reflection copy of it (Mirror Image), an enemy unit; a play-time target counts once: not offered after the first Gust (decision #222, 2 options), offered after the second (#225) — leblanc__ezreal__s20261247 D:213-218.
- Friendly choices do not count: Star-Crossed (own Ezreal + enemy Recruit) = 1, not offered at decision #95; Wages of Pain on a Recruit makes 2, offered at #102 — reksai__ezreal__s20261283 D:081-092.
- A gear ability does not count: Deadly Flourish on Vi (1) + The List on Vi; decision #134 has 1 legal action — ezreal__viktor__s20263058 D:126-127. A unit ability does: Pickpocket choosing enemy gear goes through `commitChoice` -> `noteChosen` with a unit source (class read; ezreal_sb__azir_sb__s20262155 D:229).
- Count on the opponent's turn and reset each turn (`resetTurnTracking`): ezreal__akali__s20261154 turn 7 (P2's turn), kaisa__ezreal__s20261211 D:034-040.
- Each Repeat choice counts: Bellows Breath with one enemy target + Repeat -> offered — ezreal_sb__azir_sb__s20262155 D:024-029 (the choices are made again, CR 820.2).
- Ezreal, Prodigy: Repeat [1][B] reduced to [0]+1 Power or [1]+0 Power at the payer's choice (CR 356.4.c, 356.4.f) — ezreal__teemo__s20261166 D:163, mel__ezreal__s20261265 D:545.
- Ezreal, Prodigy does not discount Hide — Hide is a Discretionary Action, "not a subset of Play" (CR 811.1.c.1): ezreal__viktor__s20263058 D:059.05 `pays [0] + 1 power (any)` with Ezreal at the battlefield. Nor Deflect (mandatory additional cost, CR 356.2.a.2).
- A unit / gear is not given priority before it enters (Kha'Zix played as a reaction is on the board at once) — CR 337.2 — ezreal_sb__azir_sb__s20262155 D:137-143.
- Hidden Wages of Pain: hide costs [A], not playable the turn it is hidden (217 + 54 same-turn decisions without the option), [0] later, target only at its battlefield (CR 811.1.d.2; 0 options elsewhere out of 235) — ezreal__leblanc__s20263013 D:425-433.
- Wages of Pain / Hextech Ray target lists never hold a unit in a base (990 options).
- Bellows Breath with zero targets is playable (CR 355.13) — ezreal__teemo__s20261166 D:159.
- Deadly Flourish: Gold only if the unit dies the same turn (47 yes / 2 later-turn deaths no, ezreal__teemo__s20261166 D:288 -> D:293); Gold exhausted (board `Gear: Gold(,exh)`).
- Bewitching Spirit: the player is chosen at finalization (a player is a target, CR 355.7), the discarding player picks the card — diana__ezreal__s20263008 D:072-076.
- Vilemaw's Lair stops an effect's move to base as well: `[D:092.03] MOVE_BLOCKED: Lonely Poro can't move to base` (Unforgiven) — ezreal_sb__yasuao_sb__s20262169. (The ability was still offered at that unit; whether it should be is the Unforgiven reader's question, CR 355.4.a / 355.16.)
- Frozen Fortress triggers in both players' Beginning Steps, before the Scoring Step, and damages both players' units — ezreal__fiora__s20261158 D:175.03-05, D:197; a unit it kills on the opponent's turn loses the battlefield: ezreal__viktor__s20263058 D:068 then P2 conquers at D:069.
- Sigil of the Storm: the rune is chosen at resolution ("must": not a target, CR 355.10.f) — ezreal__fiora__s20263012 D:045-046.
- Ravenbloom Prefect triggers on a Gold token being played, banishes itself at finalization, banishes the token at resolution — kaisa_sb__ezreal_sb__s20262211 D:289-292.
- Kha'Zix, Mutating Horror: two triggers at D:137.05-06 are two copies (both lines end `-> 6M`), not a double trigger — ezreal_sb__akali_sb__s20262154.
- Daughter of the Void's Power pays Perfect Execution's Flow cost — playing a spell from the trash is playing a spell — kaisa__rengar__s20263025 D:202.
- Brynhir Thundersong triggers again when Temporal Breach makes its owner play it (66 triggers for 63 plays from hand).
- Brynhir: the opponent may still react while the trigger is on the chain (2 legal actions at decisions #63, #65), nothing after it resolves; the lock ends with the turn — akali_sb__kaisa_sb__s20262106 D:060-068.
- Lecturing Yordle [Tank] took only 1 damage while Noxus Hopeful took 18 — Elder Dragon makes any damage lethal — kaisa_sb__fiora_sb__s20262212 D:288.09-11.
- Sprite tokens of Sprite Burst survive the opponent's turn and die in their controller's Beginning Step before scoring — azir_sb__kaisa_sb__s20262124 D:328 / D:352.07.
- Rocket Barrage's second execution hits a unit that already has lethal damage: both executions are one resolution ("Deal 4. Deal 4.", CR 820.1.d) — teemo_sb__kaisa_sb__s20262322 D:334.
- Rocket Barrage offers only the modes that have an object; "a unit in a base" lists both bases — kaisa__khazix__s20263024 D:225-226.
- Power Domains paid: 861 recycles for Time Warp, Hextech Ray, Progress Day, Rocket Barrage, Perfect Execution, Temporal Breach, Decree of Rage, Bellows Breath, Ezreal, Thousand-Tailed Watcher, Angler Beast, Kha'Zix, Singularity — all of the card's Domain.
- Kai'Sa, Survivor: Accelerate asks [1][Fury] (175 times), draw 1 on conquer — kaisa__rengar__s20263025 D:103.12-105.05.

**Reader 6**

- Voidreaver gained XP after a combat with no damage step (defender recalled by Zhonya's Hourglass and then killed in base; attacker bounced by Gust; enemy moved out by Void Assault) — the Voidreaver player was the only player with units left there at the Resolution Step (CR 468.3.a) — akali__khazix__s20261107__random__random D:095-105; leblanc_sb__khazix_sb__s20262251__random__random D:254-256; kaisa__khazix__s20261215__random__random D:155-162.
- Voidreaver trigger resolves after the Deathknells of the combat (CR 468.2 before 468.3) — khazix__leblanc__s20263028__ismcts-sims20__random D:041-043.
- Voidreaver "Spend 2 XP": XP spent and nothing moved (`MOVE_BLOCKED: Shadow Order Disciple can't be moved this turn`) — Vex, Apathetic's effect on a unit played that turn; the cost is still paid — diana__khazix__s20263009__ismcts-sims20__random D:256-259.
- Void Assault made the caster the DEFENDER — the units met at a battlefield the caster controlled; the reminder text only covers a battlefield "you don't control" — kaisa__khazix__s20261215__random__random D:149.05; azir_sb__khazix_sb__s20262125__random__random D:304.05; khazix__akali__s20263002__random__ismcts-sims20 D:153.05.
- Void Assault / Star-Crossed / Hidden Blade / Rebuke / Smoke Screen paid one Power less in Sandswept Tomb games although some are "removal" — each chose a unit friendly to its caster there (random play) — leblanc_sb__khazix_sb__s20262251__random__random D:133.02; yasuao__zed__s20261387__random__random D:241.02.
- Void Assault at full cost with a friendly Irresistible Faefolk at Sandswept Tomb — two Faefolks, the chosen one (id=10) was in base — khazix__akali__s20263002__random__ismcts-sims20 D:084.
- Dusk Rose Lab: kills as a cost at finalization, draws at resolution, the emptied battlefield does not score a Hold (CR 471.2, 319) — akali__viktor__s20261114__random__random D:353.04-355.07.
- Deceiver: cost (discard + exhaust) at finalization, token at resolution, copy as a reflexive trigger with its target at that trigger's finalization; the copy takes printed traits and keeps being a token; its Deathknell fires when Temporary kills it (CR 446, 479.1.b, 183.1) — azir__leblanc__s20263035__random__ismcts-sims20 D:039-046, D:078.
- Vex, Apathetic triggers on a Reflection ("opponent played a unit") — the text says "play a ... token" — 20 times after `TOKEN: created Reflection`.
- Karthus doubles a Deathknell when he dies in the same damage step (CR 383.2.c.3) — leblanc__azir__s20261245__random__random D:568.02.
- Kennen, Keeper of Balance played for free as a Reaction the turn after hiding, on the opponent's turn (CR 811.6); there is no `PLAY:` line for a facedown play — diana__leblanc__s20261144__random__random D:128, D:140.
- Zed's swap activated with Zed and the clone at the same location (cost paid, nothing moves) — a legal activation that does nothing — kaisa__zed__s20261225__random__random D:196-198.
- Zaun Warrens with an empty hand draws without a discard; it also triggers on a conquer whose point was blocked (5 times) — diana__zed__s20261153__random__random D:219.
- Glasc Mixologist: "you may" and the unit at finalization, the play location after RESOLVE — the location is part of playing the card at resolution, not a target of the ability — leblanc_sb__akali_sb__s20262244__random__random D:134-138. (If the lead counts play locations under K3, this is one more.)
- K2, more [Add] abilities that wait on the chain with priority for both players: Gold (`RESOLVE: Gold (chain_item=19, ...)` after two priority decisions, azir__leblanc__s20263035__random__ismcts-sims20 D:086-088) and Seal of Discord (kaisa__zed__s20261225__random__random D:202-204).

**Reader 7**

- Mel Empowered: the legend's -2 becomes -3 — mel_sb__azir_sb__s20262263__random__random D:139.03 `give Recruit -3 [M] this turn -> 0M`; Stupefy -1 becomes -2 with the floor kept — mel_sb__kaisa_sb__s20262268__random__random D:081.03 `give Tideturner -2 [M] this turn -> 1M (min 1)`; Smoke Screen -4 becomes -5 — viktor_sb__mel_sb__s20262361__random__random D:118.03 `-5 [M] this turn -> 1M (min 1)`.
- An opponent's -[M] on Mel's side gets nothing extra — mel__diana__s20261264__random__random D:139.03 (Eclipse -4), D:165.04 (Moonfall -2); a Switcheroo controlled by the opponent mel__irelia__s20261267__random__random D:337.03.
- Shock Blast paid `E=0 P=0` from hand looked like a broken "minimum of [1]" — CR 356.4.e: the minimum belongs to Applied Researchers' discount only, and the player may apply it first (3 -> 2) and Shock Blast's own -2 after — leblanc__mel__s20263032__ismcts-sims20__random D:227.02.
- Dredge Up's Flow [2] paid as [1] — Applied Researchers Empowered (D:069), spells cost [1] less — akali__mel__s20261109__random__random D:126.04. Two Researchers stack (Time Warp `E=8 P=2`, aggregate).
- Empower "only if not Empowered": over 731 empower / disempower lines in the 720 logs no object but Kayle, Justified (stacking) is empowered twice. Soul's Reflection: 55 triggers, 46 empower lines (the rest: already Empowered). The legend's ability was only ever activated when it could disempower itself (38 = 38), and it ends Exhausted (replay idx 142 of mel_sb__azir_sb__s20262263).
- Soul's Reflection "a unit at a battlefield": the 5 options at mel_sb__azir_sb__s20262263 D:137 are the five units at Seat of Power; base units are not offered. The cost (disempower, exhaust) is paid before the target prompt (`[D:136.03] EFFECT: disempower` then `[D:137.00] choose a target`); the outcome is the same, the order is the reverse of CR 355 -> 356.
- Rebuttal on its controller's own spell with Mel Empowered: `[D:152.04] REBUTTAL: that spell can't be countered` — mel_sb__azir_sb__s20262263 D:152. New choices after gaining control are made at once and pay Deflect — diana__mel__s20263039__random__ismcts-sims20 D:206-208.
- Void Burrower: may + exhaust at finalization, reveal and "banish one" at resolution (CR 383.3.a.3); the cost of the banished card is paid (`PAY_COST`), a unit asks "where to play it" (base or a controlled battlefield) — reksai__azir__s20261281__random__random D:184-187; with only Rockfall Path controlled no location is asked — reksai__khazix__s20261287 D:098. The rest is recycled.
- Rek'Sai's own Accelerate: [1] + a Fury Power, 28 payments, all `[1] + 1 power` — irelia__reksai__s20263021__ismcts-sims20__random D:137. Attacks at 4 [M] in all 16 damage steps.
- Blood Rush [Repeat]: the second execution takes a new choice — reksai__ezreal__s20263014__random__ismcts-sims20 D:073-076 (Mischievous Marai, then Inferna).
- Imperial Decree kills a unit that took non-lethal damage — akali__viktor__s20261114__random__random D:347.07 (Astral Heron takes 3 of 9) / D:351.03 `EFFECT: kill Astral Heron`.
- Unchecked Power exhausts base units too ("all friendly units") and damages only units at battlefields — ezreal_sb__mel_sb__s20262163__random__random replay idx 251 -> 253; irelia_sb__mel_sb__s20262199 D:231.
- Pridestalker: any unit, enemy included (with Deflect paid) — rengar__vex__s20261311__random__random D:293.03-.05; fires for a token — ezreal_sb__rengar_sb__s20262165__random__random D:029.04; for a unit Thrill replays — rengar_sb__reksai_sb__s20262308 D:169.06; for a unit played from facedown — azir__rengar__s20261129 D:074.04. K1 does not change its list: the played unit is offered either way.
- Rengar, Trophy Hunter from hand: offered as a Reaction to battlefields holding only enemy units — akali_sb__fiora_sb__s20262104__random__random D:164 (options "-> Sigil of the Storm" (enemy only) and "-> Risen Altar"); the combat that follows has him as attacker — khazix__azir__s20261227__random__random D:121.03, D:135.05-.06. Rockfall Path is never among his options.
- Thrill of the Hunt: the battlefield is chosen as the spell resolves; that is the moment the unit is played (CR 355.2.a), not a K3 case. The replayed unit's own play ability and the legend fire.
- First Mate "ready another unit" offers enemy units too — the text says "another unit" — rengar_sb__reksai_sb__s20262308 D:169.09.
- Star Spring: fires only for a unit played there (trigger_manager.cpp:440), offers the other units of that player there, not the played one, on either player's turn — azir__rengar__s20261129__random__random D:074-076.
- Emperor's Dais: may, [1] and the returned unit at finalization, token at resolution — ezreal_sb__rengar_sb__s20262165__random__random D:026-029.
- Pyke's additional cost paid: ready, +2 [M], moves the same turn — azir_sb__rengar_sb__s20262129__random__random D:054-060.
- Rockfall Path: no "Play <unit> -> Rockfall Path" option in any of the 720 replays; hiding there is offered.

**Reader 8**

- Xin Zhao enters ready although he is played to a battlefield — the text counts other units in the base and says nothing about where he is (card text) — teemo__viktor__s20261330__random__random decision #112, viktor__akali__s20261352__random__random decision #46.
- Xin Zhao enters exhausted with one other unit in base that is another Xin Zhao — one is fewer than two — zed_sb__viktor_sb__s20262420__random__random decision #18.
- Viktor, Leader fires and then nothing resolves — he died in the same damage step; ability taken back (CR 383.2.c.2) — viktor__rengar__s20261363__random__random D:278.13-16.
- Carrion Dredger "defends" and deals 1 after being banished by Temporal Breach, and is no longer stunned — the spell replays it at the same location as a new object (CR 124.1) — viktor__kaisa__s20261358__random__random D:065-067.
- Mutated Mouser defends at 1M — a "-2 [M] this turn" from Soul's Reflection plus Shield 2 — vex__mel__s20261343__random__random D:107, D:117; attacks at 3M only with Boots of Swiftness attached.
- Dragon Form leaves Vex, Apathetic at 4M — base 5 with a -1 already on her — viktor__ezreal__s20261355__random__random D:427-431.
- Guerilla Warfare returns "Guerilla Warfare" — the other copy (id=76) from the trash, not the resolving one (id=77); the card has [Hidden] — azir__teemo__s20261130__random__random D:096.
- Teemo, Strategist's controller pays [A] to choose Steel Paws — Deflect applies to a triggered ability's choice — teemo__vex__s20261329__random__random D:047.
- A facedown Teemo goes to the trash when the last unit moves away — the card stays only "for as long as you control that battlefield" (CR 811.1.b) — akali__teemo__s20261112__random__random D:109.04.
- Hide timing and occupancy — 954 Hide actions, all by the turn player at a top-level open decision; no battlefield ever held two facedown cards except Bandle Tree (105 boards, never three) (CR 811.1.b).
- Hide cost — [A] or, with Swift Scout, [1] by choice (prompt "Hide cost for X [2 options]": 67 paid [1], 64 paid [A]); free after Guerilla Warfare (15 of 15).
- What the opponent is told about a hide — public line `HIDE: P2 hides a card at BF#0`; the lines naming the card (`HIDE: Teemo, Strategist at BF#0 by P2`, `HIDDEN: ... hidden at`) are `RIFT_TRACE_PRIVATE` / `LogAudience::only` in `executeHideCard` (game_engine.cpp:3735-3792). `HIDDEN: X removed from facedown (control lost)` is public, as is the trash it goes to.
- Fortified Position offers the attacker's units and units elsewhere — "choose a unit" — teemo__vex D:045.06; the defender who gets the trigger is whoever defends there, not the list's owner — rengar__vex__s20261311__random__random D:172.
- Baron Nashor at 9M — an opponent's "enemy units -3 [M]" affects him without choosing him — kaisa_sb__teemo_sb__s20262220__random__random D:178.05.
- Existential Dread's Repeat stuns and then returns the same unit — the repeat's target is chosen at finalization and the unit is "already stunned" by then (CR 820.2) — teemo_sb__rengar_sb__s20262327__random__random D:127-129.
- Keeper of Law's discount counts units of both players at the battlefield — "exactly two units there" (61 plays against the board).

**Reader 9**

- Yasuo's trigger after a single Unforgiven move — it is the per-move item of D77 (R9-5), not a point; points only on the third (3 instances, counts verified move by move) — kaisa__yasuao__s20261223 D:160-175.
- Unforgiven moves an EXHAUSTED unit into an enemy-held battlefield and it attacks — a move by effect does not need a ready unit; combat follows (CR 448, 463) — akali__yasuao__s20261115 D:266-271.
- Master of Shadows fires after "TOKEN: created Shadow Clone" — the token is not the cause: Death Mark was replayed for Flow and is banished as it leaves the chain (CR 829.1.b) — akali__zed__s20261117 D:021-023. A COUNTERED Flow spell is banished too and empowers (Vendetta FAQ: "it will be banished when it attempts to leave the chain") — akali__zed__s20261117 D:065.
- Rhasa played for "E=0 P=1" with no rune recycled — the Power came from Seal of Discord's floating Chaos Power added earlier in the turn (CR 166, 167) — akali_sb__zed_sb__s20262117 D:157 / D:185; azir__zed__s20261135 D:360 / D:361.
- Piercing Light replayed for "[2] + 0 power" although it costs [2][R] — it chose a friendly unit at Sandswept Tomb ("costs [A] less") — yasuao_sb__zed_sb__s20262387 D:139. 7 spells choosing a friendly unit at the Tomb paid 1 less, 28 choosing elsewhere paid in full (one Void Assault at khazix__akali__s20263002 D:084 paid in full with a same-named unit at the Tomb and one in base: not decidable from the trace).
- The defender "conquers" after a combat — control goes to whoever alone remains, "This does not have to be the player that applied Contested" (CR 468.5.e) — akali__khazix__s20261107 D:261. The attacker there is the player whose unit applied Contested, although the enemy unit was pulled in afterwards (CR 466.2.c.1).
- Deflect paid twice by one Piercing Light — once for the spell, once for the Repeat's new choice ("for each time they choose", CR 809.1.c) — fiora_sb__zed_sb__s20262189 D:220-222.
- Combat assignment with three non-lethal amounts — Elder Dragon's any-damage rule (own trace lines) — yi_rebuild_sb__fiora_sb__s20262392 D:256.
- RUNES. Second player channels 3 in the first Channel Phase, everyone else 2, ready (CR 487.7, 315.3, 431.2.a): 720 of 720 games. Rune Deck short: 423 Channel Phases with 1 rune, 938 with 0 (CR 431.3); by hand akali__akali__s20261100 turn 15 (board 11/11+1 -> 12/12+0, one rune channeled). Energy: number of EXHAUST_RUNE lines = E in 20,252 PAY_COSTs; the 383 exceptions are 1-2 short with floating Energy / Power from an [Add] earlier (sampled). Power: 9,886 RECYCLE_RUNE-for-power lines, every rune's Domain is one of the paid card's Domains (0 exceptions); `PAY_ADD` hides the rune, so ~50 Last Rites Equips ([P] = Chaos) were compared on the board before and after: only the Chaos count drops. Recycled runes go to the bottom of the Rune Deck: for players with no untraced rune movement, 1,840 re-channels of recycled runes come back in exact first-recycled-first order and only after all 12 originals (0 violations). Pool empties: lead's script.
- COMBAT (2,087 damage steps). Attacker = the side whose unit applied Contested (in every window read by hand, about 30; not scanned). Damage assigned by each side = sum of its units' Might, stunned units 0, in all 4,018 assignments; at most one unit per assignment gets a non-lethal amount (CR 467.2.c.3; only exception Elder Dragon); units with [Tank] get lethal first in 54 of 54 mixed assignments, Backline last (vex__vex__s20261347 D:142). Both sides surviving: attackers recalled (CR 468.1.a.2) — azir__zed__s20261135 D:087 `RECALL: Traveling Merchant (id=61) to base`; 23 instances read by script, 22 more where my scan window was cut by a replacement prompt and the recall was confirmed by hand in three. Healing at the end: same game idx 95 (Azir undamaged). Conquer by the attacker after the combat: 494 instances among the 1,548 combats whose unit names are all distinct, every one with no defender left; 19 conquers by the defender alone remaining (CR 468.5.e). Simultaneous deaths with a replacement ask the order (CR 373) — akali__irelia__s20263001 D:148.
- Master Yi's Hunt goes on the chain in the Scoring Step and resolves before the Channel Phase — yi_rebuild__zed__s20263064 D:233-235.
- Last Rites plays a unit in the Scoring Step of its controller's turn, paying with runes readied in the Awaken Phase — fiora__zed__s20261189 D:161-166.

## 7. Judge questions

The questions below came up while reading; each is written for a judge in the format of `docs/judge-questions-2026-10.md`. Where a reader also recorded a finding on the engine's present behaviour, the finding says which answer the engine implements.

**Reader 1**

- Zhonya's Hourglass (any non-unit gear) played from facedown in a Closed State. Situation:
  zed_sb__yi_rebuild_sb__s20262422__random__random D:179 — P2 plays the hidden Hourglass as a Reaction with
  Fiora's trigger on the chain; no `RECALL (CR 323.6)` line follows before the damage step at D:181, where it
  replaces a death from the battlefield. When played at a showdown focus the recall is immediate (azir ismcts
  D:239.04). Question: is the recall of CR 323.6 performed in the cleanup that follows the gear entering even in a
  Closed State (CR 319.6 "after any number of Game Objects enter or leave the Board"), i.e. is the gear in base
  before the chain continues? Rules: 319.6 / 323.6 say every cleanup recalls; nothing limits it to an Open State
  (323.5, by contrast, names the Open State). No outcome difference for the Hourglass itself.
- Mischievous Marai played to a battlefield with no enemy unit: the ability is put on the chain with
  `TGT_NOT_OFFERED` (27 times) and resolves to nothing, giving both players a priority window. Question: does a
  triggered ability whose only instruction needs a target, and has none, go on the chain at all? Rules: 355.8
  ("valid choices must be made for all targets" to put an ability on the chain) against 383.x (a triggered
  ability triggers whenever its condition is met).
- Rogue Assassin on the opponent's turn: may its controller activate it ([Action], in a showdown) knowing "If
  it's your turn" will make it do nothing? Rules: 377.2.b restricts only a condition on *using* the ability;
  358.3.a allows finalizing something that will do nothing. The engine does not offer it.

**Reader 2**

1. Weaponmaster and a Domain-specific Equip cost. Text: CR 821.1.c "Pay the cost of its Equip ability, reduced by [A]"; 821.1.c.3 "If the chosen card's Equip cost does not contain [A], it can still be paid, but will not be reduced."; Equipment print "[Equip] [C]" (one Power of the card's Domain, CR 135.2.e.6), e.g. Doran's Shield "[Equip] [G]". Situation: a Sand Soldier with [Weaponmaster] (Emperor of the Sands) chooses Doran's Shield. Question: is [G] reduced to nothing (the engine: `PAY_ADD: pays [0] + 0 power`), or does a cost of [G] "not contain [A]" (the rainbow symbol, 135.2.e.5) and stay at [G]? For the engine's reading: the cards' reminder text and the 206.1 example use [A] for "a Power"; against: 821.1.c.3 read literally with 135.2.e.4/5 distinguishing a Domain's Power from [A].
2. Deathgrip with one friendly unit. Text above. Situation: the controller has exactly one unit. Question: can Deathgrip be played (kill it, no pump, draw 1), or does CR 355.8 require a valid "another friendly unit" at finalization? For: the second sentence is conditional. Against: 355.8, 355.5.
3. Pyke, Returned and a simultaneous replaced death. Situation (azir__irelia__s20261123 D:351-358): Pyke attacks, takes lethal combat damage together with two enemy units; Zhonya's Hourglass replaces his death (heal, exhaust, recall to base). Question: did the enemy units die "while I'm at a battlefield" (all combat damage deaths are simultaneous), so that one Gold is due — or is he already in base? CR 383.2.c.2 speaks of leaving the zone at the same time; base and battlefield are both the board. The engine gave no Gold.
4. Order of simultaneous triggers of one controller. Guards!: the token's Weaponmaster trigger and the reflexive "pay [C] to ready it" are both the controller's; the engine always resolves the reflexive one first (azir__kaisa__s20263005 D:155 then D:157) without asking. CR 383.3.d gives the controller the order — does it apply between a reflexive trigger and a play effect that arise during the same resolution?

**Reader 3**

- Diana, Lunari. Text: "When a showdown begins here, you may pay [1] to [Predict], then reveal the top card of your Main Deck. If it's a spell, draw it." Situation: the player declines (or cannot pay) the [1]. Question: is only the Predict bought by the [1], with the reveal-and-draw happening regardless, or is the whole "[Predict], then reveal ... draw it" the effect of paying? The engine treats all of it as paid for (class: `if (conf < 1) return;`). For the engine's reading: CR 383.3 "[cost] to [effect]" with the effect running to the end of the sentence chain; against: "then reveal" could be read as a separate instruction after the optional one.
- Hard Bargain with [Repeat] paid and one spell on the chain: may the Repeat be paid when no second spell can be chosen (CR 820.2 says the Repeat's choices are made at the usual time; CR 355.8 requires valid choices for all targets), and if paid, does the second execution simply do nothing (CR 820.1.d.2)?
- Vex, Apathetic: when Vex leaves the battlefield (or the board) after its ability has triggered and before it resolves, is the played unit still stunned? (Condition met at trigger time; the text has no "if I'm still at a battlefield".)

**Reader 4**

- Q1 (Grand Duelist "When one of your units becomes [Mighty] ...", Fiora, Worthy). Situation: a unit with printed Might 5+ is played (Rengar 6, Poppy 5, Rift Herald 7, Elder Dragon 10, Harnessed Dragon 6). The engine treats entering the board as becoming Mighty: akali_sb__fiora_sb__s20262104__random__random D:069.02 (Rengar from hand), akali__fiora__s20261104__random__random D:251.03 (Poppy; Fiora then readies her, D:257.03); 110 Grand Duelist triggers follow a card's cost payment directly. Question: does a unit that enters the board with 5+ [M] "become Mighty"? For: nothing was a Mighty unit on the board before. Against: CR 709 requires the Might to change "from being less than 5"; CR 711 evaluates a card in a Non-Board Zone by its printed Might, so it was already Mighty in hand. Tokens created at 5+ [M] and a unit that enters with a buff or an aura (3 printed, 5 on entering) need the same answer.
- Q2 (Rengar; D37 (R4-6)). Is the battlefield permission of [Ambush] usable at ordinary timing (own turn, Neutral Open), or only when the unit is played as a [Reaction]? CR 822.1.b lists the location permission and the [Reaction] grant as two abilities.
- Q3 (Dazzling Aurora + Rengar). "Play it, ignoring its cost": may a Rengar revealed by Aurora be played to a battlefield where there are enemy units (822.1.c: Ambush adds valid locations when a card is played)? The engine offers base and controlled battlefields only (`MODE_PROMPT: Aurora: where to play unit [base/Amateur Recital]`, diana__fiora__s20261140__random__random D:240.06).

**Reader 5**

- Q1. Prodigal Explorer: "[E]: [Reaction] — Draw 1. Use only if you've chosen enemy units and/or gear twice this turn with spells or unit abilities." Situation: I play one Singularity ("Deal 6 to each of up to two units") choosing two enemy units; nothing else this turn. Question: have I "chosen enemy units twice" (two objects chosen), or once (one spell)? And does paying a Repeat and choosing the same enemy unit again count a second time? The engine counts every chosen object and every Repeat choice. Rules: 355.5 / 355.7 speak of choosing objects one by one ("Each Target is valid, and contributes to Targeting Effects individually", 355.14.d, for split damage); nothing defines "twice".
- Q2. Daughter of the Void: "[Add] [A]. Use only to play spells." Situation: I play Hextech Ray choosing Vex, Apathetic ([Deflect]). Question: may the Deflect [A] be paid with Daughter's Power? For: 356.2.a.2 makes Deflect a mandatory additional cost of playing that spell. Against: "to play spells" could be read as the spell's own costs only. And for a Repeat cost (820.1: optional additional cost of the spell)? (The engine: Repeat yes, Deflect no.)
- Q3. Temporal Breach: "Banish a unit, then its owner plays it to the same location, ignoring its cost." Rockfall Path: "Units can't be played here." Situation: the unit is at Rockfall Path. Question: is it banished and left there (054.1, ruling #3 on Guards!), played to another legal location, or may Temporal Breach not choose it at all (355.16)? Also: if the owner cannot or need not play it, may the owner decline?
- Q4. The List: "As you play this, name a tag." May I name any tag that exists (763.1), including one that appears on no card currently public? (The engine offers at most 16, taken from public cards.)

**Reader 6**

- Star Spring: "The first time a player plays a non-token unit here each turn, they may move another unit they control here to its base." Situation: on one turn the turn player plays a unit there and the opponent then plays one there with [Ambush]. Question: is it the first play there by any player (one trigger a turn) or each player's first play there (the engine: one per player)? Rules: nothing found either way; "a player ... they" reads per player, "the first time ... each turn" reads once.
- Voidreaver: "Spend 1 XP, [E]: [Buff] a unit." The engine logs `ACTIVATE_COST: spend 1 XP` before the target is chosen. Question: none on the outcome (both happen before priority); recorded only because CR 355 orders choices before costs.

**Reader 7**

- Q1 (Void Burrower). Text: "You may banish one, then play it. Recycle the rest." Situation: one of the two revealed cards cannot be paid for. The engine offers it anyway; if chosen it is banished and stays in banishment (reksai__yasuao__s20263041__ismcts-sims20__random D:061.03-.05 Blood Rush; akali_sb__reksai_sb__s20262110__random__random D:064 Ferrous Forerunner). Void Rush, with the same sentence, offers only cards that can be paid for. Question: may a player banish a card with this ability when they cannot play it, and does it then stay banished? Rules: CR 356.1.b.2 (the cost is not waived); nothing found on an instruction "banish one, then play it" whose second half cannot be done.
- Q2 (Rek'Sai, Breacher). Text: "Friendly units played from anywhere other than a player's hand have [Accelerate]." Situation: with Rek'Sai on the board a unit token is played (Faithful Manufactor's Recruit, Carrion Dredger's Bird). The engine never offers Accelerate for a token (the cost path returns for an object without a card; no ACCELERATE line names a token in 720 logs). Question: is a token "played from anywhere other than a player's hand", and may its controller pay [1] + [A] (CR 805.1.b.2, no Domain) for it to enter ready? Rules: CR 350.2 (tokens are played), 805.2.a (paid only as part of playing a card).
- Q3 (Mel, Newly Awakened + Switcheroo). Situation: mel__ezreal__s20261265__random__random D:503: Mel Empowered, her controller's Switcheroo swaps Mel (4) and Hwei (5): `give Mel, Newly Awakened 1 [M] this turn -> 5M` / `give Hwei, Brooding Painter -2 [M] this turn -> 3M` / `SWITCHEROO: swapped Might 4 <-> 5`. Question: does Mel add -1 to the decreasing half of a swap? Rules: CR 434.1.d says a Might swap "is the equivalent to giving those units a Might increasing effect and a Might decreasing effect", which supports the engine; the card says "Swap", not "give -[M]".
- Q4 (Pyke, Dockside Butcher; Noxian Emissary). Situation: Pyke's "When you play me, if you paid the additional cost, ..." is put on the chain when the cost was declined (akali__rengar__s20261111__random__random D:129.01-.02) and Noxian Emissary's [Empowered] Deathknell is put on the chain when it died without being Empowered (47 triggers, 24 with an effect; diana__reksai__s20261146 D:214.02 resolves with nothing). Both resolve doing nothing, but both players get a priority window. Question: does an ability whose condition is false as its event happens trigger at all? Rules: CR 828.1.b.1 (the Dependent Ability exists only while Empowered); CR 383.4.e.2.b says it for attack triggers only.

**Reader 8**

1. Herald of the Arcane — "[1], [E]: Play a 1 [M] Recruit unit token." The player controls a battlefield. May the token be played there, or only to the base? For: 185.2.a + 355.2.a (a played token follows the steps of playing a card: base or a battlefield you control), 184.2 (a location restriction is something the effect states). Against: nothing found; Viktor, Leader prints "into your base" where the base is meant. (D66 (R8-1) assumes yes.)
2. Baron Nashor — "As you play me, add the Baron Pit battlefield token ... If you do, I enter there." The Pit is new and uncontrolled. Does his entering it score a point by Conquer that turn? The engine opens a non-combat showdown and scores: kaisa_sb__teemo_sb__s20262220__random__random `[D:161.04] SHOWDOWN: BF#2 (non-combat)` / `[D:165.05] CONQUER: P2 conquers BF#2 (score was 0)`. Rules seen: 471 (conquer = gaining control of a battlefield not scored this turn); nothing found that treats a battlefield created this turn differently.
3. Nocturne, Horrifying — he is revealed by Teemo, Strategist's ability in the middle of its resolution and banished. When is "If you do, you may play me for [A]" carried out: inside the reveal (during Teemo's resolution, before the damage), or after the resolving ability has finished? Is that play a normal play that the opponent can respond to? Rules seen: the example at line 1509 (replacement, simultaneous with the reveal); 402.2 (while an effect resolves, continue resolving it).
4. Scoring Step — a player controls two battlefields. Are both held at once (all hold abilities pending together, ordered by their controller), or one after the other with the chain emptied in between? 315.2.b.2 says "Holds all Battlefields they Control" as one task. (D72 (R8-7) assumes at once.)

**Reader 9**

- Lonely Poro — "[Deathknell] — If I died alone, draw 1. (... I'm alone if there are no other friendly units here.)" Situation: Lonely Poro and every other friendly unit at its battlefield are killed in the same combat cleanup (vex_sb__yi_rebuild_sb__s20262350__random__random D:181: `KILL: Lonely Poro` and `KILL: Master Yi, Tempered` of one damage step; engine: `LONELY PORO: died alone -> draw 1`). Question: did it die alone? For "no": the deaths are simultaneous (CR 323.4, 370.1.a.2), an effect "can look back at its characteristics before it changes zones" (CR 359.3.e.13), and just before the deaths another friendly unit was there (CR 740.2.a). For "yes": the ability resolves later and the engine reads the board then, when none is left. The engine's reading also means a friendly unit arriving at that location before the trigger resolves cancels the draw. 14 traced instances (ezreal__yasuao__s20261169 D:210, khazix__yi_rebuild__s20261242 D:210, mel__yasuao__s20261277 D:122, yasuao__kaisa__s20261376 D:119 — two Lonely Poros dying together each draw —, zed__yasuao__s20261421 D:265, ...).
- Master of Shadows — "When you banish a card you own". Situation: the opponent's Smite kills zed's Ravenbloom Prefect and the Prefect's own replacement turns the death into a banish (zed_sb__reksai_sb__s20262416__random__random D:059: `REPLACEMENT (self): Ravenbloom Prefect is banished instead of dying` / `EFFECT: banish Ravenbloom Prefect (id=39)` / `TRIGGER: Master of Shadows (P1) fires ability`). Question: who is responsible for a banish that replaces a kill the opponent performed — the opponent (CR 412.1: the player performing the kill) or the controller of the replacement effect? CR 412.5 separates attribution from responsibility but does not cover replaced actions.

## 8. Not observed

**Reader 1**

- Akali, Deadly Weapon: Empowered status dropped when she leaves the board and is replayed — not a clause of her text; no instance opened.
- Defy: an enemy spell above the caps on the chain with Defy in hand and payable (to see it withheld) — not searched in the replay options.
- Block: played from facedown.
- Blitzcrank, Impassive: [Tank] in a damage assignment.
- Forgotten Monument: a Hold in a player's first two turns (needs control from turn 1; only 5 gate events in all games, all conquers).
- Ferrous Forerunner: location of the two Mech tokens.
- En Garde / Against the Odds: the counted quantity against the board.
- Decree of Focus: "being chosen by an enemy Fury spell" (needs an enemy Fury spell choosing the unit while the Decree's controller has priority).
- Akali, Silent: an enemy spell or ability choosing her while she is in combat.
- Thermo Beam / Disarming Rake with a facedown gear on the board: no instance (class: facedown cards have no location and are not listed).

**Reader 2**

- Soul Sword: "[Level 3][>] I have an additional +1 [M]" — the azir list has no XP source; no bearer whose controller had 3+ XP was found.
- Vex, Cheerless: "friendly spells cost [1][A] less to a minimum of [1]" — her controller played no spell while she was in combat (16 plays of her in all games).
- Hidden Blade: choosing a [Deflect] unit; "its controller draws 2" when the death is replaced or the unit was taken from its owner — no such window read.
- Deathgrip: "If you do" when the kill is replaced (Zhonya's Hourglass is in the other list).
- Charm: a [Deflect] target; Charm from the azir sideboard choosing a unit at a battlefield in a non-combat situation was read only through its K3 behaviour.
- Scuttle Crab: who sees the revealed hand and the facedown cards (the trace is omniscient; the class marks the hand reveal to all and the facedown look to its controller only).
- Pyke, Returned: [Backline] ordering of combat damage — not checked.
- Emperor of the Sands used twice in a turn after a Hall of Legends ready — no turn with two activations exists in the 78 games (script: 0).
- Salvage, Decree of Unity: read through counts and the class; no single resolution window opened (see table).

**Reader 3**

- Diana, Lunari: paying the [1] with Scorn's Energy — never happens in a trace (D7 (R3-3), D30 (R3-4)); the trigger in a combat (D29 (R3-1)).
- Scorn of the Moon: an opponent reacting on top of the pending [Add] (K2) — the random opponents always passed in the instances opened.
- Tideturner: hidden-play restriction cases other than the free choice (it has none); 161 hides but only a handful of plays from facedown (random agents rarely play it back).
- Switcheroo: from facedown with targets limited to that battlefield (CR 811.1.d.2) — not compared against a board.
- Decree of Insight: an enemy Body unit with [Deflect] (the "Ignore [Deflect]" clause) — no such pairing found in the 28 plays.
- Hard Bargain: two spells on the chain; an opponent's spell under an ability.
- Vex, Apathetic: leaving the battlefield in response with the played unit remaining (D33 (R3-8), second half).
- Fizz, Trickster / Up from the Deep: the recycle ("Then recycle it") and the banish after Flow leave no trace line; read in code only (chain_manager.cpp:547-562; CLAUDE.md Flow note).
- Flash: two units in one cast; Acceptable Losses: both players with gear; Traveling Merchant: empty hand.

**Reader 4**

- Grand Duelist / Fiora, Worthy: crossing 5 through [Assault] or [Shield] at the start of a combat — no fiora-side unit with Assault attacked at 4 or defended at 4 with Shield (the one Assault 3 was given to a unit in base, kaisa_sb__fiora_sb__s20262212__random__random D:087.03).
- Grand Duelist / Fiora, Worthy: crossing through a standing battlefield bonus — the list's battlefields give none; no opposing battlefield gave a static Might bonus to a 4 [M] fiora unit in the games read.
- Riposte: a spell under an ability with Riposte affordable (see D21 (R4-7)).
- Repulse: countering an ability; an item choosing two friendly units.
- Akshan, Mischievous: stealing an Equipment ("attach it to me"); control reverting when he leaves the board.
- Fiora, Victorious, Kayle, Justified: [Ganking] used for a move.
- Risen Altar: an Empower cost with no Energy ("[A] less").
- Dazzling Aurora: a Main Deck without a unit.
- Amateur Recital: the holder moving an enemy unit (and paying Deflect for it).

**Reader 5**

- Vilemaw's Lair: a recall from it (combat tie, CR 461.1.a.2) — no tied combat there in its 21 games. Code: `recallToBase` is not blocked.
- Frozen Fortress: its damage killing the turn player's only unit there and so stopping that player's hold — in all 40 kills the battlefield was controlled by the other player (a holder has already survived the opponent's tick).
- Ezreal, Prodigy: the play trigger with an empty hand (131 resolutions, a card was always discarded).
- Daughter of the Void: its Power paying a Deflect or a Repeat; a unit paid with it (none: 18 unit plays with it floating were fully recycled; 1 unit play with a Gold floating too cannot be told apart in the trace — fiora__kaisa__s20261178 D:287).
- Decree of Rage: "This can't be countered" — nobody played a counter at it.
- Treasure Trove: leaving the board by banishment or recycle (kill 61, return to hand 7).
- Pack of Wonders: returning a hidden UNIT (the 3 facedown returns are spells).
- Rockfall Path: a token-making effect choosing a location there (the helpers skip it; only Temporal Breach reaches it, D47 (R5-7)).
- Kha'Zix, Mutating Horror's XP being used (the ezreal list has no XP card).

**Reader 6**

- Evelynn, Entrancing: the pull — only one facedown play on her own turn in 720 games, and she was bounced first.
- Kha'Zix, Mutating Horror: [Ambush] into the defending side of a combat — never played that way.
- LeBlanc, Everywhere at Once: a Temporary unit surviving at her battlefield — the engine kills it (D55 (R6-7)); [Backline] with mixed defenders.
- LeBlanc, Fragmented: death in her controller's Awaken Phase (D56 (R6-8)).
- Ki Barrier: an opponent assigning more than lethal to get through the prevention — agents never did.
- Kennen, Keeper of Balance: the +2 [M] in a damage step; Chakram Dancer: an Ambush reaction play; Black Rose Dignitary: [Assault] in a damage step; Ruined Rex and Decree of Strength: the final effect line — not read (time), not absent.
- Voidreaver: an activation with a buffed unit as target (CR 427.1.c) — not looked for.

**Reader 7**

- Mel, Newly Awakened: "Your spells and abilities can't be countered" against an opponent's counter — no counter was aimed at her controller's spells while she was Empowered and on the board (the one hit is her controller's own Rebuttal).
- Rek'Sai, Breacher: the grant for a play from the trash or the Champion Zone — the reksai list has no such play of another unit; a Void Rush play of a unit with her on the board was not looked for.
- Smite: "If it would die this turn" for a death later in the turn.
- Noxian Demolitionist: the kill — offered once (rengar_sb__azir_sb__s20262299 D:113), declined; 9 plays in all (sideboard card).
- Pyke, Dockside Butcher: a play from facedown; a Ganking move between battlefields.
- Rebuttal: refusal against a spell costing more than [4] at offer time (only the "nothing on top" message was seen, through Fizz).
- Faithful Manufactor / Carrion Dredger: where the token is put (the TOKEN line carries no location; no board was compared).
- Undertitan: the unit list of "+2 [M] to your other units".
- Pit Rookie: choosing a unit that already has a buff.

**Reader 8**

- Nocturne, Horrifying: [Ganking] battlefield-to-battlefield move — not searched for; reveal by anything other than Teemo, Strategist (a look, a Predict) — no trace: all 20 sightings are Teemo's reveal.
- Tomb-Raider Barbara: "If it's [Empowered], disempower it" — no empowered enemy gear in any of her 7 prompts.
- Downwell: gear returned, enemy permanents, tokens ceasing to exist — the one resolution read had two units only (11 resolutions exist).
- Sneaky Deckhand: a play to an open battlefield carried out — only the offered option was read.
- Consult the Past: a play from facedown identified as such.
- Fortified Position: the granted Shield mattering in a later combat of the same turn (D73 (R8-10)) — no trace.
- Windsinger from facedown bouncing a unit at another battlefield — offered once (D70 (R8-5)), not chosen.
- Abandon against a chain whose top is not the spell to counter (D21 (R8-13)) — no trace.
- Winning point with three battlefields (Baron Pit) — not re-read; the lead's script check covers it.
- Kharox: Power paid in the Chaos domain — the PAY_ADD line gives the amount only.

**Reader 9**

- Yasuo, Windrider: a fourth move in one turn; the third-move point as the eighth point (no Conquer restriction); the trigger's item being countered or Yasuo leaving the board between moves (the count lives on the object and is only raised at resolution).
- Unforgiven: its target leaving or changing control before resolution; a move into a battlefield where a showdown or combat is already running (never activatable there).
- Wind Wall: two spells on the chain (which one is countered cannot be chosen, D21 (R9-4)); an uncounterable spell.
- Wuju Bladesman - Starter: a second defender joining or leaving during the showdown (bonus switching off / on).
- Master Yi, Tempered: XP dropping below 6 while he is at a battlefield (Deflect / Ganking switching off).
- Alpha Strike: damage divided among two or more units; the chosen unit's Might changing between finalization and resolution; a Deflect unit among the split targets (the cost is paid before the split targets are picked: `PAY_COST` precedes `SPLIT_PROMPT`).
- Master of Shadows: activation with an empty hand; a second empower while empowered shows no EFFECT line (resolves as nothing: seen, yi_rebuild__zed__s20263064 D:095).
- Seal of Discord: used while a cost is being paid (never offered there, D2 (R9-9)).
- Last Rites: the bearer conquering with no unit in the trash that can be paid for (`MAY_NOT_OFFERED` path not read); the chosen unit leaving the trash before resolution.
- Kennen, Storm of Shuriken: a granted spell with a Power cost replayed without Sandswept Tomb's discount (one replay in all traces); the grant expiring at end of turn.
- Minefield: fewer than 2 cards in the Main Deck (D81 (R9-11)).
- Ruin Runner: only absence can be shown (never offered to an opponent); no trace has an opponent's spell with Ruin Runner as its only possible unit.

## 9. Deck interactions

**Reader 1**

- akali: Rogue Assassin's showdown move + ready when Empowered (Akali moves in, shoots, is pulled home ready, shoots again, moves again) — akali__reksai__s20261110__random__random D:072-086; akali__azir__s20263000__ismcts-sims20__random D:076-086 — ok
- akali: Akali, Deadly Weapon's move damage and Empower (1 / 2, both ends of the move, +1M) — same games; azir ismcts D:300-310 — ok
- akali: Equipment with [Quick-Draw] played as a Reaction onto Akali with her trigger on the chain — azir ismcts D:304-310 — D14 (R1-2) (attach is inline); Equip — D15 (R1-3)
- akali: Zhonya's Hourglass replacing a death (combat, spell, ability; hidden copy played in combat; two copies) — azir ismcts D:173, D:239-243; akali__irelia__s20263001 D:116 — ok; with Falling Star's two hits — kaisa__yasuao__s20261223 D:112 — D20 (R1-8)
- akali: Stellacorn Herder draws on every move (Standard Move, Shuriken Flip, Rogue Assassin), not on a recall — azir ismcts D:208, D:315-317 — ok
- akali: Astral Heron discount on the next card (Kai'Sa 2E, Blitzcrank 3E+0P, Shuriken Flip free, Flow for [1]) — akali__akali__s20261100 D:236-239; azir ismcts D:364-368 — ok this turn; D18 (R1-6) across turns; D19 (R1-7) at base
- akali: Void Gate bonus damage with Akali's ability, Falling Star (per instance, only for the unit at Void Gate) — akali__reksai D:077, D:084; akali__akali__s20261100 D:109 — ok
- akali: Forgotten Monument's score gate — akali__akali__s20261100 D:025; akali_sb__leblanc_sb__s20262108 D:021 — ok (conquer only)
- akali: Sigil of the Storm (conquer -> recycle a rune, chosen at resolution) — azir ismcts D:158-161, D:364-367 — ok
- akali: Shuriken Flip from hand and by Flow, banished after the Flow play (board at D:310 shows it in Banish) — azir ismcts D:195-208, D:310-315 — D5 (R1-1) (played in combat), D16 (R1-4), D17 (R1-5)

**Reader 2**

- azir: Emperor's token ability only after an Equipment play — all 588 offers — ok.
- azir: "Your Sand Soldiers have [Weaponmaster]" on tokens from Emperor / Guards! / Arise!, Equip at a discount (CR 821) — akali__azir__s20261101 D:076-080; azir__diana__s20261120 D:203-215 (four triggers, each its own item) — the discount and the attach are right; timing D22 (R2-1).
- azir: Azir, Sovereign's attack move of tokens (Eye of the Herald on a moved token fires; moved tokens fight) — azir_sb__akali_sb__s20262118 D:240-249; azir__azir__s20261119 D:343-347 — D24 (R2-4) (when it is asked), D23 (R2-3) (Might of the moved token at Trifarian War Camp).
- azir: Accelerate on Azir ([1] + Order) — azir__azir__s20261119 D:140 — ok (K1/K4 order of asking).
- azir: Equipment suite and Equip costs — table rows — costs and Might right; D15 (R2-2) (no chain item, unit not chosen).
- azir: Arise! counts Equipment on the board, attached or not — azir__diana__s20261120 D:202 (4) — ok; playable in combat showdowns and on the opponent's turn: D5 (R2-5).
- azir: Guards! from hand / from facedown — ok; D5 (R2-5), D25 (R2-6).
- azir: Hall of Legends readies the legend for [1] at conquer — azir__akali__s20261118 D:069-072 — ok (D4 (R2-9) for declined ones).
- azir: Trifarian War Camp's +1 in combat sums — six damage steps recomputed by hand (table) — ok except D23 (R2-3).
- azir: Seat of Power — 336 resolutions — ok.
- irelia: Blade Dancer on a chosen friendly unit (spells, Tideturner, Abandoned Hall, Edge of Night from facedown) — irelia__teemo__s20263051 D:125-134 — ok; not on Equip: D15 (R2-2).
- irelia: Blade Dancer "When you conquer, pay [1] to ready me" — irelia__teemo__s20263051 D:143-145 — ok.
- irelia: Irelia, Fervent "+1 [M]" on choose and on ready, stacking with Blade Dancer's ready (two +1s) — D:128, D:132 — ok; Equip: D15 (R2-2).
- irelia: [Deflect] against the opponent — 58 payments — ok.
- irelia: spells that choose friendly units (Discipline, Ride the Wind, Defiant Dance, Rebuke on own unit) each fire Blade Dancer once per spell — diana__irelia__s20261141 D:311-315, irelia__fiora__s20261194 D:241-246 — ok.
- irelia: Abandoned Hall — kaisa_sb__irelia_sb__s20262213 D:139-147 — ok.
- irelia: Sunken Temple — vex_sb__irelia_sb__s20262339 D:302-305 — ok (card owned by another reader; one window read).
- irelia: Targon's Peak — "scheduled ready-2-runes for end of turn" 21 times, "readied 2 runes" 16 times in the irelia games; no window read line by line — not verified beyond that.
- irelia: Treasure Hunter / Pyke Gold tokens — Gold's "[Reaction][>] Kill this, [E]: [Add] [A]" is put on the chain with priority passes (akali__irelia__s20261105 D:145-148: `RESOLVE: Gold (chain_item=37, ability=1, spell=0)` / `GOLD: kill+[E] -> +1 [A]`) — K2, another [Add] ability doing it.

Aside for the owner of Hard Bargain (not my card): irelia_sb__vex_sb__s20262203__random__random D:043.02-05 prints `RESOLVE: Hard Bargain (chain_item=6 ...)` twice — first "countered Charm (P2 can't pay [2] to save it)", then "nothing on chain to counter — no-op".

**Reader 3**

- diana: Scorn of the Moon's Energy spent in showdowns on [Action]/[Reaction] spells — yi_rebuild_sb__diana_sb__s20262390__random__random D:145, azir__diana__s20263004__ismcts-sims20__random D:159 (Rebuke E=2 with one rune) — restriction ok; but K2, D31 (R3-5) (Focus passes), D5 (R3-2) (non-Action spells in combat).
- diana: Scorn's Energy for Diana, Lunari's [1] — diana_sb__rengar_sb__s20262147__random__random D:027, diana_sb__leblanc_sb__s20262144__random__random D:051 — D30 (R3-4) (never possible), D7 (R3-3) (asked after the showdown), D29 (R3-1) (never in combat).
- diana: Diana, Lunari drawing a spell for the showdown — diana__ezreal__s20263008__ismcts-sims20__random D:140-146 — the draw comes after the Conquer: D7 (R3-3).
- diana: [Action]/[Reaction] spells in showdowns (Moonfall, Gust, Rebuke, Stupefy, Eclipse, Flash, Switcheroo, Star-Crossed) — diana__diana__s20261138__random__random D:301, reksai__diana__s20261282__random__random D:239 — ok in non-combat showdowns; D8 (R3-7) for Switcheroo / Star-Crossed with a missing target.
- diana: Hwei, Brooding Painter moved by Standard Move / Tideturner / Ride the Wind / enemy Moonfall, three branches — diana__khazix__s20263009__ismcts-sims20__random D:166, azir__diana__s20263004__ismcts-sims20__random D:264-271 — ok.
- diana: Vex, Apathetic stun + can't move (including the opponent's own Tideturner swap, Ride the Wind, Unforgiven, Voidreaver) and [Deflect] — diana__khazix__s20261143__random__random D:363-365, diana__yasuao__s20261151__random__random D:163 / D:188, diana__vex__s20263054__random__ismcts-sims20 D:187 — ok; D33 (R3-8).
- diana: Fizz, Trickster replaying spells from the trash — akali__diana__s20261102__random__random D:031-037 — D32 (R3-6) (targetless Stupefy / Eclipse / Smoke Screen / Turn to Dust), D8 (R3-7) (spells with nothing to choose); Power still charged, Energy waived: ok.
- diana: Tideturner hidden and from hand — diana__khazix__s20263009__ismcts-sims20__random D:207 (hide, [A]) and D:231 (Reaction play at the hidden battlefield for 0), azir__diana__s20263004__ismcts-sims20__random D:258 — ok (K1).
- diana: Abandoned Hall (+1 after each spell, either player) — diana__ezreal__s20261139__random__random D:225 — ok.
- diana: Targon's Peak (delayed ready of up to 2 runes in the Ending Step) — diana_sb__rengar_sb__s20262147__random__random D:031-037 — ok.
- diana: Seat of Power (draw per other battlefield controlled) — diana__ezreal__s20263008__ismcts-sims20__random D:143 — ok.

**Reader 4**

- fiora: unit becomes Mighty -> Grand Duelist channels 1 exhausted + Fiora, Worthy readies it for [Y] — akali__fiora__s20261104__random__random D:040.05-051.02; ISMCTS: fiora__irelia__s20263016__ismcts-sims20__random D:085.03-091.03 (ambushed Rengar readied, then attacks) — effect ok; timing D35 (R4-1), order D40 (R4-10), decline D4 (R4-3).
- fiora: what counts as becoming Mighty — buff, "this turn" bonus, Equipment, Empower: ok; entering at 5+: Q1; end of a "-N this turn": triggers, late (D35 (R4-1) c); Assault / Shield / battlefield bonus: not observed.
- fiora: stops and becomes Mighty again (Sett: spend the buff, +4) — ezreal_sb__fiora_sb__s20262158__random__random D:197.04-199.04 — D36 (R4-2).
- fiora: once per crossing — fiora_sb__leblanc_sb__s20262180__random__random D:323.03; two units in one event, one legend exhaust — akali_sb__fiora_sb__s20262104__random__random D:179.05-190.03 — ok.
- fiora: Might pumps (Punch First +5, Rampage +2, Riposte + cost, Divining Shells +2, Kayle's Empower, Shepherd's Heirloom +2) into the two triggers — ezreal_sb__fiora_sb__s20262158__random__random D:174.02; fiora__akali__s20261172__random__random D:132 — ok; Divining Shells' cost D38 (R4-8); Rampage / Mobilize timing D5 (R4-5).
- fiora: Rengar's [Ambush] — as a [Reaction] in a closed state and in showdowns, to battlefields with own or with enemy units, never base: ok (fiora__irelia__s20263016__ismcts-sims20__random D:077.03; scan above); in a Neutral Open state: D37 (R4-6).
- fiora: Sacrifice on a Mighty unit, with Rift Herald's [Deathknell] — fiora__fiora__s20261176__random__random D:082.04-090.02 — D39 (R4-9), D35 (R4-1) b.
- fiora: Sunken Temple, conquer with a Mighty unit, pay [1], draw 1 — azir_sb__fiora_sb__s20262122__random__random D:179.06-182.05; 185 conquests checked — ok.
- fiora: Risen Altar + Kayle's Empower — kaisa_sb__fiora_sb__s20262212__random__random D:188.03 — ok (one instance).
- fiora: Amateur Recital, hold, move a unit at a battlefield to its base — azir__fiora__s20261122__random__random D:050.09-054.02 — ok.
- fiora: Poppy (5 [M], enters, readied by Fiora) — akali__fiora__s20261104__random__random D:244.03-257.03 — ok subject to Q1.
- fiora_sb: Akshan's gear steal — fiora_sb__akali_sb__s20262172__random__random D:126.03-134.04 — ok, D1 (R4-13); Fiora, Victorious while Mighty — diana_sb__fiora_sb__s20262140__random__random D:213.03 (Deflect), D:252-258 — D42 (R4-12).

**Reader 5**

- ezreal: Prodigal Explorer fed by the deck's targeting spells (Stupefy, Gust, Wages of Pain, Deadly Flourish, Star-Crossed, Singularity, Bellows Breath + Repeat) — offered exactly when two enemy units / gear have been chosen by spells or unit abilities; as a reaction above the second spell — ezreal__fiora__s20261158 D:176-194, akali_sb__ezreal_sb__s20262103 D:128-146, diana__ezreal__s20263008 D:049-061 — ok (Q1).
- ezreal: not counted — friendly targets (reksai__ezreal__s20261283 D:081), The List's gear ability (ezreal__viktor__s20263058 D:126), battlefield abilities (code: source is not a unit) — ok.
- ezreal: Ezreal, Prodigy + Bellows Breath Repeat — discounted, payer picks [1] or [A] — ezreal__teemo__s20261166 D:162-163 — ok.
- ezreal: Ezreal, Prodigy + Thousand-Tailed Watcher Accelerate — not discounted — ezreal__leblanc__s20261162 D:194 — D43 (R5-1).
- ezreal: Ezreal, Prodigy + Hide (Wages of Pain) — not discounted, correctly — ezreal__viktor__s20263058 D:059 — ok.
- ezreal: Bellows Breath Repeat choosing units in two locations — ezreal__leblanc__s20261162 D:220-231 — D45 (R5-4).
- ezreal: Wages of Pain / Deadly Flourish lethal damage and the unit lingering through the chain — akali__ezreal__s20261103 D:083-091 — D44 (R5-3).
- ezreal: hidden Wages of Pain not playable in the own main phase — ezreal__ezreal__s20261157 decision #284 — D25 (R5-5).
- ezreal: Deadly Flourish, Singularity, Turn to Dust played in combats — D5 (R5-2).
- ezreal: Vilemaw's Lair — no standard move to base, effect move blocked — ezreal_sb__yasuao_sb__s20262169 D:092 — ok; Frozen Fortress both turns before scoring — ok; Sigil of the Storm rune recycled on conquer — ezreal__fiora__s20263012 D:043-046 — ok.
- kaisa: Daughter of the Void — reaction window on the chain (K2: kaisa__khazix__s20263024 D:205-207 above Hextech Ray); Power spent on spells of either Domain and on Flow, never on units / Accelerate / Hide; lost at end of turn — ok; Deflect — D50 (R5-10) (code only).
- kaisa: Kai'Sa, Survivor — Accelerate [1][Fury], draw on conquer — kaisa__rengar__s20263025 D:103-105 — ok; Temporal Breach on her never offers Accelerate — kaisa__azir__s20261209 D:049 — D48 (R5-8).
- kaisa: Void Gate + Falling Star (+1 on each of its two instances: reksai_sb__kaisa_sb__s20262286 D:366.03-08), Hextech Ray (4 damage, 17 times), Bellows Breath (8) — units there only, no combat damage — ok; stale after a move on the same chain — D49 (R5-9).
- kaisa: Rockfall Path — no unit card, hidden unit or Ambush unit can be played there (83 games) — ok; Temporal Breach puts the unit back there — kaisa_sb__zed_sb__s20262225 D:101 — D47 (R5-7).
- kaisa: Frozen Fortress — each player's Beginning Step, before scoring; inline kill; no overlap with Void Gate (the bonus is for units at Void Gate) — ok.
- kaisa: Rocket Barrage, Perfect Execution, Temporal Breach, Time Warp, Progress Day, Sprite Burst, Falling Star played during combats, also on the opponent's turn — kaisa__khazix__s20263024 decision #251, akali__kaisa__s20261106 decision #214 — D5 (R5-2).
- kaisa: Brynhir Thundersong + Temporal Breach (replayed, locks the opponent again) — ok.

**Reader 6**

- khazix: Voidreaver XP engine (win a combat -> 1 XP; Kha'Zix vs a lone enemy -> 2 XP; spend on Buff / pull-back) — azir__khazix__s20261125__random__random D:162-164 (XP 0 -> 2 -> 3), akali__khazix__s20261107__random__random D:109-112 — ok.
- khazix: Kha'Zix "alone" bonus lost when the lone attacker is removed in response — leblanc_sb__khazix_sb__s20262251__random__random D:249-256 — D51 (R6-1).
- khazix: Kha'Zix [Ambush] — reaction plays only to a battlefield with friendly units (options at akali_sb__vex_sb__s20262113__random__random D:191, diana_sb__ezreal_sb__s20262139__random__random D:193); Ambushed into an attack he gets no attack trigger — azir_sb__ezreal_sb__s20262121__random__random D:113 — D68 (R6-5).
- khazix: Void Assault + Kha'Zix (pull one enemy onto Kha'Zix's battlefield: he defends against a lone unit) — kaisa__khazix__s20261215__random__random D:149-151 (`KHA'ZIX: enemy alone, +2M and +2 XP`) — ok; destinations after RESOLVE — K3.
- khazix: Sandswept Tomb discount on the list's own spells (Void Assault, Star-Crossed, Switcheroo 2 -> 1 once per spell) — kaisa__khazix__s20261215__random__random D:041.05, D:151.08 — ok.
- khazix: Star Spring replay of a bounced unit — irelia__khazix__s20261197__random__random D:099 — D52 (R6-2).
- khazix: Zed, Without a Sound / Shadow Clone (conquer token, clone attack with banish for [Assault 4], swap in a showdown) — khazix_sb__rengar_sb__s20262237__random__random D:272-276, diana__zed__s20261153__random__random D:204-206 — ok.
- leblanc: Deceiver Reflection of LeBlanc, Fragmented dying to Temporary in its controller's Beginning Phase -> draw 2 (x2 with Karthus) — akali__leblanc__s20261108__random__random D:037 (40 such resolutions) — ok.
- leblanc: Dusk Rose Lab + a Temporary Reflection (kill the copy that is about to die for a card) — impossible in the engine, the Temporary kill always comes first — akali__leblanc__s20261108__random__random D:194 — D53 (R6-3).
- leblanc: Dusk Rose Lab's empty trigger as a free reaction window every Beginning Phase — ezreal__leblanc__s20263013__ismcts-sims20__random D:051-055 — D4 (R6-4).
- leblanc: LeBlanc, Everywhere at Once keeping Reflections alive — yasuao_sb__leblanc_sb__s20262378__random__random D:273 — D55 (R6-7).
- leblanc: Karthus + Deathknell units (Soaring Scout, Honest Broker, Ruined Rex, Glasc Mixologist, Black Rose Dignitary), also as Reflection copies — akali_sb__leblanc_sb__s20262108__random__random D:368; leblanc_sb__akali_sb__s20262244__random__random D:133-138 — ok.
- leblanc: Vi, Peacekeeper [Ambush] into an attack — ezreal_sb__leblanc_sb__s20262162__random__random D:237-245 — D68 (R6-5); her stun on a declared attack — D:462-465 — ok.
- leblanc: Windswept Hillock Ganking for units there (BF -> BF standard moves offered only from the Hillock) — khazix__leblanc__s20261234__random__random idx=62 — ok.

**Reader 7**

- mel: Empower Mel or Applied Researchers -> Soul's Reflection becomes Empowered -> disempower + exhaust for -2 (-3 with Mel) — akali__mel__s20261109__random__random D:067-078, mel_sb__azir_sb__s20262263__random__random D:128-139 — ok.
- mel: Mel's extra -1 with Stupefy / Smoke Screen / the legend, floor "to a minimum of 1" kept — games above — ok; with Switcheroo — Q3; only for chosen units — D65 (R7-11) (CODE-ONLY).
- mel: Applied Researchers' discount with Shock Blast's own and with Flow — leblanc__mel__s20263032__ismcts-sims20__random D:227, akali__mel__s20261109 D:126 — ok.
- mel: Rebuttal — D21 (R7-8).
- mel: Rockfall Path — never a play destination for the mel list — ok.
- reksai: conquer -> Void Burrower -> banish and play, cost paid — reksai__azir__s20261281__random__random D:183-187 — ok for units; a targeted spell does nothing — D57 (R7-1).
- reksai: Void Burrower / Void Rush play + Rek'Sai, Breacher's Accelerate grant — reksai__khazix__s20261287__random__random D:098, reksai__ezreal__s20263014__random__ismcts-sims20 D:171 — D58 (R7-2); the "ACCELERATE: Honest Broker declined" / "Carrion Dredger declined" lines are plays from HAND with Rek'Sai in the trash — D59 (R7-3).
- reksai: Undertitan revealed by the legend — D64 (R7-10).
- reksai: Forbidding Waste, lone defender — D60 (R7-4).
- rengar: every unit play -> Pridestalker +1 [M], any unit — azir_sb__rengar_sb__s20262129__random__random D:054-057 — ok (K1 timing).
- rengar: Rengar, Trophy Hunter as Chosen Champion ambushing — never offered from the Champion Zone — D63 (R7-7); from hand ok.
- rengar: Thrill of the Hunt on a unit with a play ability (First Mate, Pyke, Grim Apothecary) — ability and legend fire again — rengar_sb__reksai_sb__s20262308 D:169 — ok; destination Rockfall Path — D62 (R7-6).
- rengar: Inferna / Nidalee / Grim Apothecary ambushed into an attack — Inferna without Assault once — D61 (R7-5); Nidalee "win a combat, draw 1" — reksai__rengar__s20263040__ismcts-sims20__random D:176-178 — ok.
- rengar: Darius, Trifarian, second card of the turn — khazix_sb__rengar_sb__s20262237__random__random D:202 — ok.
- rengar: Emperor's Dais and Star Spring — ok (see CHECKED).

**Reader 8**

- teemo: Hide on own turn in an open state, one facedown card per battlefield — 954 hides / every board — ok.
- teemo: playable from the next turn on as a [Reaction], ignoring cost — teemo__akali__s20261316__random__random (hidden D:099 turn 9, generator `hidden_on_turn < turn_number`), plays at D:095 / D:212 in the opponent's turn with no PAY_COST — ok. (A play from facedown writes no PLAY: / SPELL: line; only `INTENT: PlayReaction`.)
- teemo: a hidden permanent is played to its battlefield — teemo__akali D:095 (enters Sigil of the Storm), teemo__viktor__s20261330__random__random D:146 — ok.
- teemo: a hidden spell's / play effect's choices restricted to that battlefield — Sprite Call D69 (R8-4), Windsinger D70 (R8-5), Facebreaker D74 (R8-11) (the legal Facebreaker pair is restricted correctly: viktor__kaisa__s20261358__random__random D:061).
- teemo: facedown card to the trash when control of the battlefield is lost — akali__teemo__s20261112__random__random D:109.04 — ok.
- teemo: Swift Scout returns Teemo from the board, he is hidden again the same turn — akali__teemo__s20261112__random__random D:103-108 — ok apart from D67 (R8-2).
- teemo: hidden Teemo, Strategist played into a combat to defend — teemo__viktor__s20261330__random__random D:146-158 — D68 (R8-3).
- teemo: Teemo, Strategist reveals Nocturne — teemo__vex__s20261329__random__random D:060-064 — D71 (R8-6).
- teemo: Teemo, Scout +3 — 105 resolutions — ok.
- teemo: Baron Nashor and the Baron Pit (third battlefield; held and conquered like the others: `SCORE: P2 +1 via Hold BF#2` kaisa_sb__teemo_sb__s20262220__random__random D:216, D:269; cards hidden there D:226) — ok; QUESTIONS 2.
- teemo: Grove of the God-Willow, Startipped Peak — D72 (R8-7), D4 (R8-8); effects ok. Dusk Rose Lab — D4 (R8-8).
- vex: Gloomist once per hold while ready — kaisa__vex__s20263055__random__ismcts-sims20 D:327-330 — ok apart from D72 (R8-7) / D4 (R8-8).
- vex: Vex, Apathetic ([Deflect] paid: viktor__diana__s20261354__random__random D:116; tokens and effect-played units stunned; no move that turn; no combat damage) — D33 (R8-12); K1 applies to the timing (her item is queued before the unit has entered: akali__vex__s20261113__random__random D:052, board shows Blitzcrank in base "rdy").
- vex: Bandle Tree's second facedown card — akali__vex__s20261113__random__random D:052 — ok.
- vex: Fortified Position — D73 (R8-10).
- vex: Startipped Peak — as above.
- viktor: Herald of the Arcane — D66 (R8-1).
- viktor: Viktor, Leader with non-Recruit tokens, not with Recruits, not when he dies in the same batch — tallies above — ok.
- viktor: Xin Zhao, Vigilant ready / exhausted against the board — 70 plays — ok.
- viktor: hidden Facebreaker — D74 (R8-11).
- viktor: Vi, Peacekeeper (6 attack triggers with a stun prompt), Seat of Power ("conquer -> draw 0" 67, "draw 1" 12), Forbidding Waste (24 damage steps with a lone defender), Dusk Rose Lab (57 accepted, 53 "killed a unit here -> draw 1") — counted only, not read clause by clause (not among my 30 cards); Dusk Rose Lab's item with nothing to kill: D4 (R8-8).

**Reader 9**

- yasuao: Unforgiven moves Yasuo, Windrider from base, counted as a move for his third-move point; with Ride the Wind and a standard move the point is scored — kaisa__yasuao__s20261223 D:151-175 (Unforgiven, Ride the Wind, standard move with Akali, Silent) — ok (D77 (R9-5) for the two no-op items).
- yasuao: Unforgiven only base <-> battlefield (of 245 activations: 71 to base, 172 from base with a destination prompt over all battlefields — 55 into an enemy-held one -> combat, 83 into an empty one -> non-combat showdown, 34 into one already held —, 1 countered, 1 unresolved in my window) — ok, K3.
- yasuao: Akali, Silent moved by Unforgiven gets +2 [M] this turn and attacks — akali__yasuao__s20261115 D:266-271 — ok. Irelia, Fervent and the move spells (Ride the Wind, Flash, Charm) were read only where they cross my cards (Flash under Wind Wall, irelia_sb__yasuao_sb__s20262205 D:072-084: ok); they belong to other readers.
- yasuao: Wind Wall against the list's own counters and triggers — D21 (R9-4).
- yi_rebuild: Wuju Bladesman with a single defender — D76 (R9-2) (42 of 60 combats without the bonus); with Fiora, Peerless one on one — 8 [M] instead of 10 (D76 (R9-2)).
- yi_rebuild: Master Yi, Tempered holds / conquers -> Hunt 2 -> 6 XP -> Deflect and Ganking — yi_rebuild__irelia__s20263023__random__ismcts-sims20 D:074/096/141 (XP 2, 4, 6), then battlefield-to-battlefield moves offered — ok.
- yi_rebuild: Emperor's Dais on conquer: "may" + [1] + the unit returned are settled at finalization, the Sand Soldier arrives at resolution — yi_rebuild__fiora__s20261392__random__random D:205-209 (Ruin Runner returned by its own controller) — ok.
- yi_rebuild: Onslaught from the trash for Flow by the DEFENDER in the opponent's combat — teemo__yi_rebuild__s20261332 D:303 — D5 (R9-3).
- zed: banish sources and the legend. Own banishes empower: Flow spell leaving the chain (Death Mark akali__zed__s20261117 D:023; countered D:065), Shadow Clone's attack ability ("may" + unit at finalization, banish + [Assault 4] at resolution, D:120-125), Ravenbloom Prefect banishing itself (diana_sb__zed_sb__s20262153 D:032) — ok. Opponent's banishes empower too — D75 (R9-1).
- zed: Master of Shadows' activation as an [Action] in showdowns on either turn, disempower + exhaust paid at activation, discard chosen at resolution, then draw — akali__zed__s20261117 D:038; kaisa__zed__s20263070 D:096-099 — ok.
- zed: Seal of Discord's floating [P] pays Death Mark, Rhasa, Last Rites' Equip in the same turn — akali__zed__s20261117 D:015-016; akali_sb__zed_sb__s20262117 D:157-185; azir__zed__s20261135 D:083-088 — ok (D2 (R9-9) for the chain item).
- zed: Rhasa's cost against the trash (52 of 52) and Shadowblade Lurker's against its copies in the trash (48 of 48) — ok.
- zed: Kennen conquers -> a spell in the trash gets Flow -> replayed -> banished -> legend empowered — yasuao_sb__zed_sb__s20262387 D:138-144 — ok except D78 (R9-6) (choice at resolution) and D44 (R9-8) (lethally damaged unit kept through the window).
- zed: Minefield (top 2 to the trash on conquer, feeding Rhasa / Last Rites / Shadow Clone), Zaun Warrens (discard chosen at resolution, then draw: azir__zed__s20261135 D:026-029), Sandswept Tomb ([A] less for a spell choosing a friendly unit there) — ok; D81 (R9-11) code-only.
- zed: Death Mark / Perfect Execution / Up from the Deep replayed for Flow during the opponent's combat — ezreal_sb__zed_sb__s20262171 D:305 — D5 (R9-3).

## 10. Notes for the fix batch

- **Replaying.** Every citation is a game name and a D number; the command is in section 2.1. A post-sideboard
  list (`<deck>_sb`) is produced by `python3 scripts/testing/sideboarded_decks.py decks <dir>` (deterministic).
- **One root, several findings.** The findings are written per defect as it shows on a card, but several share
  a root the readers named in the source: `runCombat` not setting the showdown state (D5); triggers queued
  outside the chain's finalize loop — by `cleanup()`, by a conquer, by the start of a showdown, during a
  resolution — and processed late (D4, D6, D7 and the "becomes Mighty" findings); the cleanup's own steps not run between the items of one chain — lethally damaged units stay (157
  priority decisions in 54 games show one), unattached Equipment stays (D10), a Might threshold goes unnoticed;
  `recalculateAuras` not run
  between a combat's start and its damage step (the Forbidding Waste, Trifarian War Camp and Shield findings);
  the effect-play path (`setQueueSpell` / `playPayingCost`) not choosing targets for spells that take them from
  the play intent (the Fizz and Void Burrower findings); counters that read `chain.items.back()`.
- **Inventory notes.** All 18 lists pass the validator and the constructed rules (40 cards with the champion, 12
  runes of the legend's Domains, 3 battlefields with distinct names, at most 3 copies by name over Main Deck +
  sideboard + champion, at most 3 Signature cards of the legend's tag, sideboard of 10, no banned card; the
  ban list in the card classes equals the Rules Hub list of 2026-09-18). Two things the validator and the
  tournament rules do not agree on, neither met by a list: no [Unique] rule (D12); and a list names cards, so
  a reprint resolves to the first registered id — Sett, Brawler 164 (not 543), Yasuo, Windrider 205 (not 546),
  Lonely Poro 359 (not 777), Plundering Poro 392 (not 778), Seal of Discord 204 (not 545). The printings differ
  in tags (Sett and Yasuo carry "Ionia" only on the first printing) and Lonely Poro in the spelling of its
  text, not in behaviour: which printing a list means is the open "reprint region tags" question of the fix run.
- **Not part of this stream:** the play UI; the whole-pool fuzz (lane A).
