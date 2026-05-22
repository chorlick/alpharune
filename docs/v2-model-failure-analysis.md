# V2 Model Failure Analysis — Constant-Trunk Collapse

**Date:** 2026-05-20
**Status:** Resolved by V3 architecture (Phase 6v)
**Impact:** Multiple full training runs over ~3 days, ~$30 of vast.ai GPU time, all producing models that failed to beat random.

## Summary

The V2 entity-token transformer model (introduced in Phase 6d–6i) has been producing **constant output regardless of input** since at least iter_4 of every training run. Probe diagnostic (`probe_v2_model`) detected this and reported `PERSPECTIVE_WIRING=BROKEN` for months without the underlying cause being identified. Every "Phase X" fix attempt (6k, 6m, 6t, 6v sims bump) treated downstream symptoms — the policy collapse to `EndTurn`, the value head random-init transition at iter 20 — without addressing the architectural root cause.

The model was working as a constant predictor: identical value (e.g. `-0.1761` for iter_18), identical top-5 policy logits across drastically different game states. MCTS, fed an effectively input-blind prior, then collapsed to the always-legal `EndTurn` action via legal-mask renormalization.

## How It Was Found

| Step | Diagnostic | Finding |
|---|---|---|
| 1. Symptom | Vast iter 20 first model self-play iter | 19/26 games hit the engine's `kMaxTurns=200` cap as draws |
| 2. Suspected cause | Value head freeze → unfreeze transition + entropy drop at iter 20 | Plausible but couldn't explain why probe was already failing at iter_4 |
| 3. Probe ladder | Ran `probe_v2_model` on iter_4, iter_8, iter_18 (3 different training runs) | All produced identical outputs across drastically different inputs; constants drifted slightly between iters |
| 4. Random-init control | Ran `probe_v2_model` with no checkpoint (random weights) | Outputs varied across states (0.31 vs 0.31 with ~0.004 spread — barely discriminating but technically non-constant) |
| 5. Architecture audit | Read OpenSpiel's poker / AlphaZero reference implementations | They use **fixed-size flat observation tensors + MLP/CNN trunks** — no entity-token transformer, no mean-pool aggregation, no per-layer LayerNorm |
| 6. Literature search | "AlphaZero card game transformer mean pool" | Recent papers (Robust Noise Attenuation via Adaptive Pooling, 2025) explicitly call out that AvgPool, MaxPool, and CLS-token pooling **converge to constants** under SNR-fluctuating inputs |

## Root Cause

The V2 model architecture is:

```
entity_tokens (variable N)
  → embedding lookups (card, zone, domain, stance, perspective, spatial, stats)
  → concat → LayerNorm → Linear(d_model=512)
  → TransformerEncoder × 6 layers × 8 heads (LayerNorm inside each layer)
  → spatial fusion × 2 cross-attention layers (more LayerNorm)
  → MEAN POOL over tokens                    ← THE PROBLEM
  → flat_policy_head (Linear) + value_head (Linear → tanh)
```

The fatal combination is **per-layer LayerNorm + mean-pool aggregation**:

1. **LayerNorm normalizes each token's mean to 0 and variance to 1** *per sample*. This is its design intent.
2. **6 transformer layers × LayerNorm at each + spatial fusion adds 2 more rounds** = aggressive homogenization of every token's distributional shape.
3. **Mean-pool then averages 20-256 LayerNorm'd tokens.** Mean of many vectors with similar mean (≈0) and similar variance (≈1) converges to a near-constant vector regardless of which tokens are present.
4. **Policy and value heads see the SAME pooled vector.** Both produce nearly the same output across all inputs.
5. **Training gradient flows back through this bottleneck.** The optimizer can't easily produce state-discriminative outputs because the architecture pushes everything toward a single representative point.

The math is borderline a feature, not a bug — LayerNorm's whole purpose is to make activations look similar across samples. Mean-pool just compounds it. Together they form a "constant predictor" attractor that training pressure (especially under noisy MCTS visit-distribution targets) makes worse, not better.

### Why It Wasn't Caught Sooner

- **Smoke tests checked output shapes**, not output variance across inputs. (Added in Phase 6v: `TrunkDifferentiatesDistinctInputs` regression test that pins the input-discrimination invariant.)
- **Probe ran but its assertion ("PERSPECTIVE_WIRING=BROKEN") was attributed to undertraining** ("the network just needs more iters"). Phase 6k-3 baseline report explicitly hypothesized this. The probe was a real signal that got mis-categorized.
- **The bootstrap MCTS path masked the failure.** With `HeuristicValueEvaluator`, MCTS doesn't need the model's value to be meaningful; it has `score_diff / 8.0` instead. So games during iters 0-19 looked normal. The collapse only became visible at the bootstrap→model transition.
- **Policy loss decreased modestly** (0.59 → 0.55 over 20 iters), making it look like learning was happening. In reality the network was just fitting the *marginal* action distribution (a constant-predictor optimum), not state-conditional policies.

### Architectural Anti-Pattern Confirmation

OpenSpiel's reference AlphaZero implementations for card games (`open_spiel/python/algorithms/alpha_zero/model_linen.py`):

- **MLP variant:** Flatten → N × `Linear(nn_width) + ReLU` blocks → policy + value heads. No LayerNorm anywhere. No aggregation.
- **ResNet variant:** Conv input → N × ResidualBlock with BatchNorm-only-on-conv. No LayerNorm. No aggregation (input is already fixed-shape).
- **Imperfect-info games (Kuhn, Leduc, Texas Hold'em):** Flat observation tensors with per-slot semantics. No transformer-over-tokens. No aggregation needed because input is fixed-size.

We had built the most complex possible architecture (transformer + spatial GNN + pointer heads + 30M params) for what is, fundamentally, a fixed-size policy prediction problem with a 4623-dim observation already available.

## The V3 Corrective Design

V3 matches OpenSpiel's proven pattern. Located at `src/ml/v3_model.{h,cpp}`.

```
input:     flat 4623-dim observation tensor
           (already produced by extractStateFeatures() —
            same one RiftboundState::ObservationTensor exposes)

stem:      Linear(4623 → 512) → ReLU

trunk:     × 6 residual blocks, each:
             h = Linear(512 → 512) → ReLU
             h = Linear(512 → 512)
             x = ReLU(x + h)            ← skip connection
           (no LayerNorm anywhere in the trunk — gradient flow
            protected by skip connections instead)

policy:    Linear(512 → 256) → ReLU
           Linear(256 → num_action_slots)

value:     Linear(512 → 128) → ReLU
           Linear(128 → 1) → tanh
```

**Param count:** ~8M (vs V2's ~30M). Trains faster, less prone to overfit a small dataset.

**Diff vs V2:**
- Fixed-size input → no aggregation step.
- Residual MLP trunk → no transformer attention layers → no LayerNorm collapse risk.
- Only the two heads we actually supervise (flat policy + value). Pointer heads removed entirely.

### Random-Init Probe Comparison

The first sanity check we ran after building V3:

| Metric | V2 random-init | V3 random-init |
|---|---|---|
| Value spread across 5 diverse states | 0.004 | **0.67** |
| Different top-5 policy slots per state? | No (identical) | **Yes (different per state)** |
| Probe `VARIANCE=` reading | COLLAPSED | **OK** |

V3 *structurally* discriminates inputs. The "perspective wiring" assertion (sign-flipping value when perspective flips) is still off at random init, but that's expected — the network hasn't learned to encode perspective semantically yet. The point is that distinct inputs produce distinct outputs, which V2 didn't.

### Configuration

`configs/gpu_bootstrap_miss_fortune_v3.json` sets `model.kind = "v3"`. V2 (`kind = "v2"`, default) stays in-tree for back-compat.

`config_driver.cpp` dispatches on the kind at the top of `runConfigTraining`:

```cpp
AnyTrainer trainer;
if (t.model.kind == "v3") {
    trainer.model_v3   = V3Model(buildV3ModelConfig(t.model));
    trainer.trainer_v3 = make_unique<TrainerV3>(...);
} else {
    trainer.model_v2   = V2Model(buildModelConfig(t.model));
    trainer.trainer_v2 = make_unique<Trainer>(...);
}
```

`AnyTrainer` is a small dispatch wrapper so the iter loop doesn't branch.

`SelfPlayConfig::model_kind` is propagated to `playOneGame()`, which captures either `flat_state` (V3) or `entity_tokens` (V2) into `ReplaySample`. The replay buffer carries both fields; only the matching one is populated per sample.

Checkpoint sidecar JSONs gain a `"kind"` field so the path-loading evaluator and probe binaries dispatch correctly.

## Regression Guards

Three guards added so this class of failure can't recur silently:

1. **`tests/test_v2_model_forward.cpp::TrunkDifferentiatesDistinctInputs`** — pinned for V2. Even at random init, two visibly different inputs must produce noticeably different value AND policy outputs.
2. **`tests/test_v3_model_forward.cpp::TrunkDifferentiatesDistinctInputs`** — same for V3.
3. **`probe_v3_model` reports `VARIANCE=OK|COLLAPSED`** based on spread > 1e-3 across diverse inputs. This is the in-band signal during long training runs.

## Lessons

- **Probe diagnostics are signals, not opinions.** When the probe says `BROKEN`, believe it. Don't attribute it to "undertraining" without a hypothesis that explains why training would fix the specific failure mode.
- **Constant-output failure modes look like learning.** Cross-entropy on a flat target distribution can plateau at the entropy of the marginal — the model "learns" the average action distribution but nothing state-conditional. Decreasing loss is necessary but not sufficient.
- **Smoke tests must check VARIANCE across inputs, not just shapes.** Shape checks pass for any random-init network. They tell you nothing about discrimination.
- **Reference implementations matter.** Three days of training-dynamics tuning would have been a one-hour fix had we read OpenSpiel's `model_linen.py` first. Always check the established design before reaching for a novel one.
- **Architecture choices interact non-linearly.** Mean-pool alone is fine. LayerNorm alone is fine. Together (× 6 layers + aggregation), they're a constant-predictor attractor under training pressure. Audit the *combination* of choices, not each in isolation.

## References

- Source: `src/ml/v2_model.{h,cpp}` (the failed architecture, kept for posterity), `src/ml/v3_model.{h,cpp}` (the fix).
- Tests: `tests/test_v3_model_forward.cpp`, `tests/test_v2_model_forward.cpp::TrunkDifferentiatesDistinctInputs`.
- Probe: `src/runner/probe_v2_model.cpp`, `src/runner/probe_v3_model.cpp`.
- OpenSpiel reference: `build-torch/_deps/open_spiel-src/open_spiel/python/algorithms/alpha_zero/model_linen.py`.
- Phase 6v commit: `git log --grep="Phase 6v"`.
- Failed iter 20 vast log: `/workspace/riftbound/logs/bootstrap_20260520_155755/rengar.log` (preserved on the prior vast instance).
