#include "schema.h"

namespace riftbound::config {

// JSON Schema (draft 7) for the unified config file.
//
// Keep this in sync with src/config/config.h's C++ types and the
// example configs in tests/config/. The schema is the source of truth
// for what the binary accepts; if you add a new C++ field you MUST
// add it here too (and add a corresponding test).
//
// Encoded as a raw string literal so embedded double-quotes don't need
// escaping. The C++20 R"json(...)json" form keeps the schema readable
// in source.
const char* const kSchemaJson = R"json(
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "Riftbound runner config",
  "type": "object",
  "required": ["mode", "registry"],
  "additionalProperties": false,
  "properties": {
    "mode": {
      "description": "Top-level operating mode.",
      "type": "string",
      "enum": ["play", "train"]
    },
    "seed": {
      "description": "RNG base seed. 0 = nondeterministic (clock-seeded).",
      "type": "integer",
      "minimum": 0,
      "default": 0
    },
    "root_dir": {
      "description": "If set, all relative paths in this config resolve against this directory. If unset, all paths must be absolute.",
      "type": "string"
    },
    "registry": {
      "description": "Path to cards/registry.json. Required for both modes.",
      "type": "string"
    },
    "play": {
      "description": "Required when mode == 'play'. Configures a head-to-head match.",
      "type": "object",
      "required": ["games"],
      "additionalProperties": false,
      "properties": {
        "games":          { "type": "integer", "minimum": 1 },
        "max_decisions":  { "type": "integer", "minimum": 1, "default": 600 },
        "threads":        { "type": "integer", "minimum": 0, "default": 1 },
        "render_html":    { "type": "boolean", "default": false },
        "output_dir":     { "type": "string" }
      }
    },
    "agents": {
      "description": "Required when mode == 'play'. Agent assignments for p1 and p2.",
      "type": "object",
      "required": ["p1", "p2"],
      "additionalProperties": false,
      "properties": {
        "p1": { "$ref": "#/definitions/agent_spec" },
        "p2": { "$ref": "#/definitions/agent_spec" }
      }
    },
    "decks": {
      "description": "Required when mode == 'play'. Deck paths for p1 and p2 (single deck each).",
      "type": "object",
      "required": ["p1", "p2"],
      "additionalProperties": false,
      "properties": {
        "p1": { "type": "string" },
        "p2": { "type": "string" }
      }
    },
    "training": {
      "description": "Required when mode == 'train'. AlphaZero-style training loop config.",
      "type": "object",
      "required": ["legend", "iterations", "games_per_iter", "train_steps_per_iter",
                   "batch_size", "replay_capacity", "checkpoint_dir",
                   "self_play_agent", "deck_pools", "optimizer", "mcts", "model"],
      "additionalProperties": false,
      "properties": {
        "legend": {
          "description": "Legend name (e.g. 'jhin'). All decks in deck_pools.self must declare this legend; validation fails otherwise.",
          "type": "string",
          "minLength": 1
        },
        "iterations":           { "type": "integer", "minimum": 1 },
        "games_per_iter":       { "type": "integer", "minimum": 1 },
        "train_steps_per_iter": { "type": "integer", "minimum": 0 },
        "batch_size":           { "type": "integer", "minimum": 1 },
        "replay_capacity":      { "type": "integer", "minimum": 1 },
        "mirror_ratio": {
          "description": "Fraction of self-play games that are mirrors (both seats drawn from deck_pools.self). 1-ratio fraction draws p2 from deck_pools.opponent. Default 0.5.",
          "type": "number",
          "minimum": 0.0,
          "maximum": 1.0,
          "default": 0.5
        },
        "checkpoint_dir":         { "type": "string" },
        "checkpoint_every_iter":  { "type": "integer", "minimum": 1, "default": 1 },
        "keep_last_n":            { "type": "integer", "minimum": 0, "default": 0 },
        "num_self_play_workers":  { "type": "integer", "minimum": 1, "default": 1, "description": "Number of concurrent self-play games per iter (Phase 6n). Each worker shares the V2Model and ReplayBuffer." },
        "max_decisions_per_game": { "type": "integer", "minimum": 50, "default": 600, "description": "Cap on decisions per self-play game; games hitting the cap are recorded as draws. Bump for AlphaZero-style training where Dirichlet noise + temperature sampling slows down decisive play." },
        "bootstrap_iters": { "type": "integer", "minimum": 0, "default": 0, "description": "Phase 6q — MCTS-bootstrap warm-start. For the first N iters, self-play uses pure MCTS (RandomRolloutEvaluator) instead of the V2 model. Skips the cold-start trap where the model overfits a noisy random-init prior into a confidently-wrong direction. Trainer still consumes the tuples; model learns to imitate MCTS. After N iters, swaps to model-driven self-play. 0 = disabled (standard self-play from iter 0). With bootstrap_value_loss_threshold > 0 this becomes the safety cap (max bootstrap iters)." },
        "bootstrap_iters_min": { "type": "integer", "minimum": 0, "default": 0, "description": "Phase 6v+ — minimum bootstrap iters before value-loss gate can fire. Ignored when bootstrap_value_loss_threshold == 0. Prevents trivial models from skipping bootstrap when value_loss is accidentally low at iter 0." },
        "bootstrap_value_loss_threshold": { "type": "number", "minimum": 0.0, "default": 0.0, "description": "Phase 6v+ — value-loss threshold for early bootstrap exit. When > 0 and the most recent train value_loss falls below this AND iter >= bootstrap_iters_min, bootstrap ends and model-driven self-play begins. 0 = disabled (bootstrap_iters is the hard cutoff). Typical: 0.30." },
        "bootstrap_mcts_sims": { "type": "integer", "minimum": 1, "default": 40, "description": "MCTS sims used during bootstrap iters. Independent of self_play_agent.sims so bootstrap can run lighter (no model inference) at higher sim counts for stronger training targets. Default 40." },
        "log_jsonl":              { "type": "string" },
        "resume_checkpoint":      { "type": "string" },
        "start_iter":             { "type": "integer", "minimum": 0, "default": 0, "description": "Iteration index to start the loop at (Phase 6q+). 0 = fresh run; N>0 skips iters 0..N-1 and resumes at N. Paired with resume_checkpoint by interruptable-instance restart wrappers so a killed run picks up at iter N+1 without re-running completed iters." },
        "self_play_agent":        { "$ref": "#/definitions/agent_spec" },
        "deck_pools": {
          "type": "object",
          "required": ["self"],
          "additionalProperties": false,
          "properties": {
            "self": {
              "description": "Decks the training model plays from. All MUST declare the same legend (matching training.legend).",
              "type": "array",
              "minItems": 1,
              "items": { "type": "string" }
            },
            "opponent": {
              "description": "Decks for the opposing seat in non-mirror games. May be any legend.",
              "type": "array",
              "minItems": 0,
              "items": { "type": "string" }
            }
          }
        },
        "devices": {
          "description": "List of devices for self-play workers (e.g., ['cuda:0','cuda:1'] or ['cpu']). One worker per device.",
          "type": "array",
          "minItems": 1,
          "items": { "type": "string", "pattern": "^(cpu|cuda(:[0-9]+)?)$" },
          "default": ["cpu"]
        },
        "train_device": {
          "description": "Device the optimizer runs on. Must be one of devices or compatible.",
          "type": "string",
          "pattern": "^(cpu|cuda(:[0-9]+)?)$",
          "default": "cpu"
        },
        "optimizer": {
          "type": "object",
          "required": ["lr"],
          "additionalProperties": false,
          "properties": {
            "lr":           { "type": "number", "exclusiveMinimum": 0.0 },
            "weight_decay": { "type": "number", "minimum": 0.0, "default": 0.0 },
            "grad_clip":    { "type": "number", "minimum": 0.0, "default": 0.0 },
            "lr_schedule": {
              "type": "object",
              "required": ["kind"],
              "additionalProperties": false,
              "properties": {
                "kind":       { "type": "string", "enum": ["constant", "step", "cosine"] },
                "step_size":  { "type": "integer", "minimum": 1 },
                "gamma":      { "type": "number", "exclusiveMinimum": 0.0, "maximum": 1.0 },
                "t_max":      { "type": "integer", "minimum": 1 },
                "eta_min":    { "type": "number", "minimum": 0.0 }
              }
            }
          }
        },
        "mcts": {
          "type": "object",
          "required": ["uct_c"],
          "additionalProperties": false,
          "properties": {
            "uct_c":              { "type": "number", "exclusiveMinimum": 0.0 },
            "dirichlet_alpha":    { "type": "number", "exclusiveMinimum": 0.0, "default": 0.3 },
            "dirichlet_fraction": { "type": "number", "minimum": 0.0, "maximum": 1.0, "default": 0.25 },
            "temperature":        { "type": "number", "minimum": 0.0, "default": 1.0 },
            "temperature_drop_at_decision": {
              "description": "After this many decisions in a game, switch to temperature=0 (argmax). Standard AlphaZero pattern.",
              "type": "integer",
              "minimum": 0,
              "default": 30
            }
          }
        },
        "eval": {
          "type": "object",
          "additionalProperties": false,
          "properties": {
            "every_iter": { "type": "integer", "minimum": 1 },
            "games":      { "type": "integer", "minimum": 1 },
            "baseline":   { "type": "string", "enum": ["previous_iter", "random", "mcts"] },
            "baseline_sims": { "type": "integer", "minimum": 1 },
            "promote_threshold": { "type": "number", "minimum": 0.0, "maximum": 1.0, "default": 0.55 }
          }
        },
        "model": {
          "description": "Model architecture. 'kind' selects V2 (entity-token transformer, legacy) or V3 (flat-feature MLP+residual, recommended). See docs/v2-model-failure-analysis.md.",
          "type": "object",
          "required": ["d_model", "encoder_layers", "encoder_heads"],
          "additionalProperties": false,
          "properties": {
            "kind":             { "type": "string", "enum": ["v2", "v3"], "default": "v2" },
            "card_embed_dim":   { "type": "integer", "minimum": 8,  "default": 256 },
            "zone_embed_dim":   { "type": "integer", "minimum": 4,  "default": 64 },
            "domain_embed_dim": { "type": "integer", "minimum": 4,  "default": 64 },
            "stance_embed_dim": { "type": "integer", "minimum": 4,  "default": 64 },
            "stat_embed_dim":   { "type": "integer", "minimum": 4,  "default": 64 },
            "d_model":          { "type": "integer", "minimum": 32 },
            "encoder_layers":   { "type": "integer", "minimum": 1 },
            "encoder_heads":    { "type": "integer", "minimum": 1 },
            "encoder_ffn_dim":  { "type": "integer", "minimum": 32, "default": 2048 },
            "encoder_dropout":  { "type": "number",  "minimum": 0.0, "maximum": 1.0, "default": 0.1 },
            "gnn_layers":       { "type": "integer", "minimum": 0,  "default": 2 },
            "gnn_edge_types": {
              "type": "array",
              "items": { "type": "string" },
              "default": ["adjacent_move", "combat_range", "base_link"]
            },
            "activation":       { "type": "string", "enum": ["relu", "gelu", "silu"], "default": "gelu" },
            "init":             { "type": "string", "enum": ["xavier_uniform", "kaiming_normal"], "default": "xavier_uniform" },
            "trunk_hidden":     { "type": "integer", "minimum": 32, "default": 512, "description": "V3 only. Hidden width of the MLP trunk." },
            "trunk_blocks":     { "type": "integer", "minimum": 1,  "default": 6,   "description": "V3 only. Number of residual blocks in the MLP trunk." },
            "policy_hidden":    { "type": "integer", "minimum": 32, "default": 256, "description": "V3 only. Policy head hidden width." },
            "value_hidden":     { "type": "integer", "minimum": 32, "default": 128, "description": "V3 only. Value head hidden width." }
          }
        }
      }
    }
  },
  "definitions": {
    "agent_spec": {
      "description": "Polymorphic agent description. 'kind' discriminates required fields.",
      "type": "object",
      "required": ["kind"],
      "additionalProperties": false,
      "properties": {
        "kind": { "type": "string", "enum": ["random", "mcts", "ismcts", "model"] },
        "sims": {
          "description": "Required for mcts and ismcts. MCTS simulations per move.",
          "type": "integer",
          "minimum": 1
        },
        "path": {
          "description": "Required for model. Path to a .pt checkpoint loadable by torch::load.",
          "type": "string"
        },
        "seed": {
          "description": "Optional per-agent RNG seed. If unset, derives from top-level seed.",
          "type": "integer",
          "minimum": 0
        }
      },
      "allOf": [
        {
          "if":   { "properties": { "kind": { "const": "mcts" } } },
          "then": { "required": ["sims"] }
        },
        {
          "if":   { "properties": { "kind": { "const": "ismcts" } } },
          "then": { "required": ["sims"] }
        },
        {
          "if":   { "properties": { "kind": { "const": "model" } } },
          "then": { "required": ["path"] }
        }
      ]
    }
  },
  "allOf": [
    {
      "if":   { "properties": { "mode": { "const": "play" } } },
      "then": { "required": ["play", "agents", "decks"] }
    },
    {
      "if":   { "properties": { "mode": { "const": "train" } } },
      "then": { "required": ["training"] }
    }
  ]
}
)json";

}  // namespace riftbound::config
