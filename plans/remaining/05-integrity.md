# 5. Integrity and consistency

Open items only. Closed items of this plan are in `plans/completed/05-integrity.md`.

## Types and invariants

- [ ] **5.5 Id types are plain aliases; `CardDefId` has two "none" values** [confirmed]
  - Where: `src/core/types.h:18-25`. All six ids are `using X = uint32_t`, so the documented `cardDefId()` / `ctx.source` mix-up compiles silently.
  - `GameObject::card_def_id` and `ChainItem::card_def_id` use `kInvalidId`; `CardDef::id`, `DeckSubmission`, `granted_ability_def`, `GrantedAbilityRef` use 0. `recordReveal` (`game_state.h:710`) only filters `kInvalidId`.
