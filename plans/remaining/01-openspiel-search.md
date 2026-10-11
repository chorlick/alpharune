# 1. OpenSpiel / search correctness

Open items only. Closed items of this plan are in `plans/completed/01-openspiel-search.md`.

## Bugs

- [ ] **1.9 State features mix absolute and perspective-relative ordering** [confirmed]
  - Where: `src/ml/feature_extractor.cpp:355-358`, `:389-392` versus `:541-542`.
  - Battlefield controller is encoded P1=1 / P2=2; the per-side base block and the 96-float extended block are P1-then-P2; the Tier-2 block is self-then-opp. No feature gives the absolute seat.
  - Failure: for `perspective = Player2`, "bf0 side0 unit0" is the opponent's unit in the base block and P2's own in Tier-2. The comment at `:496-497` says these line up.
  - `tests/test_observation_tensor.cpp` checks only hand and deck masking.
  - Changing the layout invalidates trainer checkpoints.

## Integrity

- [ ] **1.12 Slot collisions remove legal options** [confirmed; documented as "accepted" in `action_vocab.h:26-30`]
  - Where: `action_vocab.cpp:59-64`, `:96-98`, `:181-190`; dedup at `riftbound_state.cpp:189`.
  - Mulligan encodes the count only (cannot choose which card).
  - `StandardMove` encodes the destination only ("move A" = "move A+B").
  - `MakeChoice` encodes the first object's def id; in a mirror match "target my X" and "target their X" are one slot.
  - An object with an invalid def id encodes to slot 0, same as skip/decline (reach not confirmed).
  - Champion play and its alt-cost play share a slot (`action_vocab.cpp:101-107`).
  - `MakeChoice` options that differ only in `ability_index` collide: Party Favors (71) — the opponent can never choose Runes under OpenSpiel (07 item 7.33).
    - c2: 71 Party Favors — FIXED on the card side (07 item 7.33): its options now carry distinct `chosen_value`s; test: ReviewFixC2Test.PartyFavors_TheTwoAnswersAreDistinctValues
  - Gear-target hash overwrites `ability_index` (`:143-152`). Comment at `:73-83` says 16 buckets; code uses 128.
