# 09 — Defects found during the fix run

Open items only. Closed items of this plan are in `plans/completed/09-found-during-fix-run.md`.

## VV. Found with the user's new deck lists (2026-10-11)

- [ ] **9.VV1** A real game ends as `engine_error`: `GameEngine::executePlaySpell: Switcheroo made choices it cannot be paid for with ...`. Reproduce: `--deck1 decks/vex.txt --deck2 decks/vex.txt --games 1 --seed 20261016` (also vex vs fiora seed 20261045, leblanc vs vex seed 20261046; the deck lists are the user's new untracked files in the main checkout's `decks/`: `vex.txt`, `leblanc.txt`, `zed.txt`). Switcheroo ("[Hidden] [Action] Swap the Might of two units at the same battlefield this turn.") is a finalize-pass spell whose choices can leave it unpayable although 9.DD1 says an offered play always completes and is paid. Find why the offer or the viable pass let it through (hidden play? Deflect on the second target? the pair picker outside `choosePrePlayViable`?), fix at the root red-first on the engine path, and then run every pairing and seating of the five deck lists (fiora, yi_rebuild, leblanc, vex, and zed with its three banned Stacked Deck replaced for the run) for at least 500 random games each on the optimised and the checked build: zero engine errors, or each one becomes a finding. — **owner: checkpoint**
