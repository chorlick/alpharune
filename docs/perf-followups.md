# Performance follow-ups (not part of the 2026-10 review fix run)

Recommendations from the run's measurements. None is a defect. The user decided on 2026-10-11 not to
rework the event bus or move header bodies now; these are recorded so the numbers are not lost.

- **Per-clone event bus.** In `mcts:sims=50` an engine is built and torn down for each of ~19,300 advanced
  clones (5 games). One empty `EventBus` costs 7.2 µs to build and destroy (38 Boost.Signals2 signals, each
  allocating its implementation, slot list and mutex) plus ~0.15 µs per connection: about 12% of the batch.
  Reusing or pooling the bus and its subscriptions per clone is worth roughly three times what
  copy-on-write of the state could still save. (plans/reports/strong-ids.md §3)
- **Signal emission.** 19 ns with no slot, 46 ns with one, 60 ns with two, against 1.3 ns for a
  `std::function` call: 6-8% of a random-play batch, 3-4% of `mcts:sims=50`. The mutex is NOT the cost (a
  dummy-mutex build is not measurably faster); it is the slot-list snapshot and the type-erased call.
- **State copy.** 6.2 µs per `GameState` copy after 9.KK1 (was 14.9 µs), about 4% of `mcts:sims=50`. Of it,
  the 109 map nodes are about half, `last_known` 0.95 µs, six usually-empty vectors 1.6 µs. A contiguous
  object store removes the node allocations; copy-on-write per object most of the rest.
- **Header bodies.** `card_helpers.h` (76 inline helpers) and the bodies in `card.h` force a full rebuild on
  any edit. Moving bodies to `.cpp` shrinks the rebuild; LTO would recover the inlining for binaries that
  generate games. Measure before doing either.
- **Card optimisation level.** Card units are a quarter of a full rebuild; `-O1` / `-O2` for them was not
  measured.
