#pragma once
/// @file config.h
/// Parsed config types. One-to-one mirror of the JSON schema (schema.cpp).
///
/// Two top-level modes are represented by populating either `play` /
/// `agents` / `decks` (mode=play) or `training` (mode=train). The other
/// block stays nullopt. Parser enforces this via JSON Schema first;
/// post-schema checks (legend consistency across self_pool, absolute-
/// path requirement when root_dir is unset, deck-file existence) run in
/// `parseConfig` AFTER schema validation succeeds.

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace riftbound::config {

enum class Mode { Play, Train };

enum class AgentKind { Random, Mcts, Ismcts, Model };

struct AgentSpec {
    AgentKind kind = AgentKind::Random;
    std::optional<int> sims;          // mcts / ismcts only
    std::optional<std::string> path;  // model only (absolute, post-resolved)
    std::optional<int> seed;
};

struct PlayConfig {
    int games = 1;
    int max_decisions = 600;
    int threads = 1;
    bool render_html = false;
    std::optional<std::string> output_dir;
};

struct OptimizerConfig {
    double lr = 0.001;
    double weight_decay = 0.0;
    double grad_clip = 0.0;
    std::string lr_schedule_kind = "constant";  // constant | step | cosine
    std::optional<int>    lr_step_size;
    std::optional<double> lr_gamma;
    std::optional<int>    lr_t_max;
    std::optional<double> lr_eta_min;
};

struct MctsConfig {
    double uct_c = 1.4;
    double dirichlet_alpha = 0.3;
    double dirichlet_fraction = 0.25;
    double temperature = 1.0;
    int    temperature_drop_at_decision = 30;
};

struct EvalConfig {
    std::optional<int>         every_iter;
    std::optional<int>         games;
    std::optional<std::string> baseline;  // previous_iter | random | mcts
    std::optional<int>         baseline_sims;
    double promote_threshold = 0.55;
};

struct ModelConfig {
    // Phase 6v — model architecture selector. "v2" = entity-token
    // transformer + spatial fusion + mean-pool aggregation (legacy,
    // suffers from constant-trunk collapse; see
    // docs/v2-model-failure-analysis.md). "v3" = flat-feature MLP +
    // residual blocks, matching OpenSpiel's reference AlphaZero design
    // for card games. Default "v2" for back-compat; new configs should
    // explicitly set "v3".
    std::string kind     = "v2";

    // V2-specific fields (entity-token transformer).
    int card_embed_dim   = 256;
    int zone_embed_dim   = 64;
    int domain_embed_dim = 64;
    int stance_embed_dim = 64;
    int stat_embed_dim   = 64;
    int d_model          = 512;
    int encoder_layers   = 6;
    int encoder_heads    = 8;
    int encoder_ffn_dim  = 2048;
    double encoder_dropout = 0.1;
    int gnn_layers       = 2;
    std::vector<std::string> gnn_edge_types = {"adjacent_move", "combat_range", "base_link"};
    std::string activation = "gelu";    // relu | gelu | silu
    std::string init       = "xavier_uniform";  // xavier_uniform | kaiming_normal

    // V3-specific fields (MLP + residual stack on flat features).
    int trunk_hidden     = 512;
    int trunk_blocks     = 6;
    int policy_hidden    = 256;
    int value_hidden     = 128;
};

struct DeckPools {
    std::vector<std::string> self;      // absolute paths (post-resolved)
    std::vector<std::string> opponent;  // absolute paths (post-resolved)
};

struct TrainingConfig {
    std::string legend;
    int iterations           = 1;
    int games_per_iter       = 1;
    int train_steps_per_iter = 0;
    int batch_size           = 1;
    int replay_capacity      = 1;
    double mirror_ratio      = 0.5;
    std::string checkpoint_dir;
    int  checkpoint_every_iter = 1;
    int  keep_last_n           = 0;
    // Phase 6n — number of concurrent self-play games per iter. Each
    // worker plays one game at a time on its own thread, sharing the
    // V2Model (read-only forward) and ReplayBuffer (mutex-protected).
    // Set ~min(games_per_iter, physical_cores/2) for best throughput
    // on a single GPU box. 1 = sequential (legacy behavior).
    int  num_self_play_workers = 1;

    // Phase 6m+ — cap on decisions per self-play game. Games that hit
    // this cap are counted as draws (value=0 training target). Too
    // low → many games end as draws → value-head trains on zeros →
    // cold-start trap. Default 600 worked for sims=20; sims=40 with
    // Dirichlet noise + tau=1 sampling needs ~1200 for games to
    // resolve cleanly during the exploration phase.
    int  max_decisions_per_game = 600;

    // Phase 6q — MCTS-bootstrap warm-start. For the first
    // `bootstrap_iters` iters, self-play uses pure MCTS
    // (RandomRolloutEvaluator) — no model inference. Trainer still
    // consumes the tuples, so the network learns to imitate MCTS
    // before having to bootstrap itself. After N iters, swaps to
    // model-driven self-play. 0 = disabled.
    int  bootstrap_iters     = 0;
    int  bootstrap_mcts_sims = 40;

    // Phase 6v+ — value-loss-gated bootstrap exit. When threshold > 0,
    // bootstrap can end early once `iter >= bootstrap_iters_min` AND
    // the most recent training step's value_loss falls below
    // `bootstrap_value_loss_threshold`. `bootstrap_iters` becomes the
    // safety cap (max). When threshold == 0 (default), the gate is
    // disabled and `bootstrap_iters` is the hard cutoff as before —
    // existing configs are unaffected.
    //
    // Rationale: value_loss is a direct measure of whether the value
    // head has learned to predict outcomes. V2 (broken) stayed at ~1.0
    // (random-init constant) through bootstrap; V3 (working) crashes
    // to ~0.15 within 2 iters. Gating on value_loss removes the
    // guesswork in picking bootstrap_iters per model architecture.
    int    bootstrap_iters_min          = 0;
    double bootstrap_value_loss_threshold = 0.0;

    std::optional<std::string> log_jsonl;
    std::optional<std::string> resume_checkpoint;
    // Iteration index to start at. If >0, the trainer skips iters 0..N-1
    // and resumes the loop at N. Used by interruptable-instance restart
    // wrappers (e.g. scripts/vastai_launch_interruptable.sh) which
    // auto-detect the latest iter_*.pt and set start_iter to that+1 so
    // restart picks up where the prior run left off. Default 0.
    int  start_iter = 0;
    AgentSpec self_play_agent;
    DeckPools deck_pools;
    std::vector<std::string> devices = {"cpu"};
    std::string train_device = "cpu";
    OptimizerConfig optimizer;
    MctsConfig      mcts;
    EvalConfig      eval;
    ModelConfig     model;
};

struct PlayBlock {
    PlayConfig                  play;
    AgentSpec                   p1_agent;
    AgentSpec                   p2_agent;
    std::string                 p1_deck;
    std::string                 p2_deck;
};

struct Config {
    Mode mode = Mode::Play;
    int  seed = 0;
    std::optional<std::string> root_dir;  // absolute (post-resolved), or nullopt
    std::string registry;                 // absolute (post-resolved)

    // Mode-discriminated payload. Exactly one of these is populated.
    std::optional<PlayBlock>     play_block;
    std::optional<TrainingConfig> training;
};

/// Thrown on any validation failure — schema mismatch, missing required
/// fields per mode, illegal path, missing file, legend inconsistency
/// across self_pool decks, etc. The message is human-readable and
/// includes the JSON pointer or field path where the error occurred.
class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Parse + validate a config from a path on disk. Performs:
///   1. JSON parse via nlohmann::json.
///   2. JSON Schema validation against schema.cpp::kSchemaJson.
///   3. Path resolution (relative → absolute against root_dir, or
///      reject relative if root_dir unset).
///   4. File existence checks for every referenced path (registry,
///      deck pools, model checkpoint paths).
///   5. Legend consistency: every deck in training.deck_pools.self
///      must declare the same legend as training.legend (the deck
///      JSON's `legend.name` field).
///
/// Throws ConfigError on any failure. Returns a fully-populated and
/// path-resolved Config on success.
Config parseConfig(const std::string& config_path);

/// Same as parseConfig but reads from an in-memory string instead of a
/// file. Used by tests and for stdin-style invocations.
Config parseConfigString(const std::string& json_text,
                         const std::string& base_path_for_relative);

}  // namespace riftbound::config
