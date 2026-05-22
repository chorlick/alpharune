#include "training/config_driver.h"

#include "ml/v2_model.h"
#include "ml/v3_model.h"
#include "ml/feature_extractor.h"
#include "openspiel/action_vocab.h"
#include "training/replay_buffer.h"
#include "training/self_play.h"
#include "training/trainer.h"
#include "training/trainer_v3.h"
#include "training/watchdog.h"

#include "open_spiel/spiel.h"

#include <torch/torch.h>

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace riftbound::training {

namespace {

// Sample one deck pair per game from the configured pools.
// `mirror_ratio` is the probability that both decks come from the self
// pool (the model playing itself with the same legend).
struct DeckPair {
    std::string deck1;
    std::string deck2;
};
DeckPair samplePair(const ::riftbound::config::DeckPools& pools,
                     double mirror_ratio,
                     std::mt19937& rng) {
    if (pools.self.empty()) {
        throw std::runtime_error("deck_pools.self is empty — at least one "
                                 "deck for the trained legend is required");
    }
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    std::uniform_int_distribution<size_t> self_dist(0, pools.self.size() - 1);

    DeckPair p;
    p.deck1 = pools.self[self_dist(rng)];
    bool mirror = (coin(rng) < mirror_ratio) || pools.opponent.empty();
    if (mirror) {
        p.deck2 = pools.self[self_dist(rng)];
    } else {
        std::uniform_int_distribution<size_t> opp_dist(0, pools.opponent.size() - 1);
        p.deck2 = pools.opponent[opp_dist(rng)];
    }
    return p;
}

// Build the OpenSpiel game-string for a deck pair.
std::string gameString(const std::string& registry,
                        const DeckPair& pair,
                        uint64_t seed) {
    return "riftbound(deck1=" + pair.deck1 +
           ",deck2=" + pair.deck2 +
           ",registry=" + registry +
           ",seed=" + std::to_string(seed) + ")";
}

// Map config::ModelConfig → ml::V2ModelConfig.
::riftbound::ml::V2ModelConfig buildModelConfig(
    const ::riftbound::config::ModelConfig& m) {
    ::riftbound::ml::V2ModelConfig out;
    out.card_embed_dim     = m.card_embed_dim;
    out.zone_embed_dim     = m.zone_embed_dim;
    out.domain_embed_dim   = m.domain_embed_dim;
    out.stance_embed_dim   = m.stance_embed_dim;
    out.stats_proj_dim     = m.stat_embed_dim;
    out.d_model            = m.d_model;
    out.encoder_layers     = m.encoder_layers;
    out.encoder_heads      = m.encoder_heads;
    out.encoder_ffn_dim    = m.encoder_ffn_dim;
    out.encoder_dropout    = m.encoder_dropout;
    out.spatial_attn_layers = m.gnn_layers;
    out.num_edge_types     = static_cast<int>(m.gnn_edge_types.size());
    // Sync the flat policy head to the live action vocab so checkpoints
    // shape-match whatever's currently in src/openspiel/action_vocab.h.
    out.num_action_slots   = ::riftbound::openspiel::kVocabSize;
    return out;
}

// Phase 6v — map config::ModelConfig → ml::V3ModelConfig.
::riftbound::ml::V3ModelConfig buildV3ModelConfig(
    const ::riftbound::config::ModelConfig& m) {
    ::riftbound::ml::V3ModelConfig out;
    out.input_dim        = ::riftbound::ml::kStateFeatureDim;
    out.trunk_hidden     = m.trunk_hidden;
    out.trunk_blocks     = m.trunk_blocks;
    out.policy_hidden    = m.policy_hidden;
    out.value_hidden     = m.value_hidden;
    out.num_action_slots = ::riftbound::openspiel::kVocabSize;
    return out;
}

// Phase 6v — Tiny wrapper holding either a V2 or V3 trainer+model. The
// iter loop calls into this with the same API; AnyTrainer dispatches
// internally. Exactly one of {trainer_v2_, trainer_v3_} is non-null at
// any time (set in runConfigTraining based on cfg.training.model.kind).
struct AnyTrainer {
    std::unique_ptr<Trainer>   trainer_v2;
    std::unique_ptr<TrainerV3> trainer_v3;
    // Kept alongside the trainer so we can pass to runSelfPlay/V3
    // without re-borrowing through the trainer pointer.
    ::riftbound::ml::V2Model   model_v2{nullptr};
    ::riftbound::ml::V3Model   model_v3{nullptr};

    bool isV3() const { return static_cast<bool>(trainer_v3); }

    void setEntropyCoef(double c) {
        if (trainer_v3) trainer_v3->setEntropyCoef(c);
        else            trainer_v2->setEntropyCoef(c);
    }
    void setFreezeValueHead(bool f) {
        if (trainer_v3) trainer_v3->setFreezeValueHead(f);
        else            trainer_v2->setFreezeValueHead(f);
    }
    std::tuple<float, float, float>
    step(const std::vector<ReplaySample>& b) {
        return trainer_v3 ? trainer_v3->step(b) : trainer_v2->step(b);
    }
    void save(const std::string& p) {
        if (trainer_v3) trainer_v3->save(p);
        else            trainer_v2->save(p);
    }
};

torch::Device parseDevice(const std::string& s) {
    if (s == "cpu") return torch::kCPU;
    if (s == "cuda" || s == "cuda:0") {
        if (!torch::cuda::is_available()) {
            throw std::runtime_error(
                "config.training.train_device='" + s +
                "' requested but torch::cuda is not available in this build");
        }
        return torch::kCUDA;
    }
    // cuda:N
    if (s.rfind("cuda:", 0) == 0) {
        if (!torch::cuda::is_available()) {
            throw std::runtime_error(
                "cuda device requested but torch::cuda is not available");
        }
        int idx = std::stoi(s.substr(5));
        return torch::Device(torch::kCUDA, static_cast<torch::DeviceIndex>(idx));
    }
    throw std::runtime_error("unknown device string: '" + s + "'");
}

// Prune old checkpoints so only the most-recent N remain (matches
// `keep_last_n` setting). N==0 → keep everything.
void pruneCheckpoints(const std::string& dir, int keep_last_n) {
    if (keep_last_n <= 0) return;
    namespace fs = std::filesystem;
    std::vector<fs::directory_entry> ckpts;
    for (auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == ".pt" &&
            e.path().filename().string().rfind("iter_", 0) == 0) {
            ckpts.push_back(e);
        }
    }
    std::sort(ckpts.begin(), ckpts.end(),
              [](const fs::directory_entry& a, const fs::directory_entry& b) {
                  return a.last_write_time() < b.last_write_time();
              });
    while (static_cast<int>(ckpts.size()) > keep_last_n) {
        std::error_code ec;
        fs::remove(ckpts.front().path(), ec);
        ckpts.erase(ckpts.begin());
    }
}

}  // namespace

int runConfigTraining(const ::riftbound::config::Config& cfg) {
    if (cfg.mode != ::riftbound::config::Mode::Train || !cfg.training.has_value()) {
        throw std::runtime_error("runConfigTraining called on a non-train config");
    }
    const auto& t = *cfg.training;

    std::filesystem::create_directories(t.checkpoint_dir);

    // ── Build model + trainer ──
    // Phase 6v — dispatch on cfg.training.model.kind. V2 = legacy
    // entity-token transformer (constant-trunk failure mode; see
    // docs/v2-model-failure-analysis.md). V3 = MLP+residual on flat
    // state features (matches OpenSpiel's reference design).
    auto device = parseDevice(t.train_device);
    ::riftbound::training::TrainerConfig train_cfg{};
    train_cfg.learning_rate = t.optimizer.lr;
    train_cfg.weight_decay  = t.optimizer.weight_decay;
    train_cfg.grad_clip     = t.optimizer.grad_clip;

    AnyTrainer trainer;
    if (t.model.kind == "v3") {
        auto v3_cfg = buildV3ModelConfig(t.model);
        trainer.model_v3 = ::riftbound::ml::V3Model(v3_cfg);
        if (t.resume_checkpoint.has_value() && !t.resume_checkpoint->empty()) {
            std::cout << "Resuming V3 from " << *t.resume_checkpoint << "\n";
            torch::load(trainer.model_v3, *t.resume_checkpoint);
        }
        trainer.trainer_v3 = std::make_unique<::riftbound::training::TrainerV3>(
            trainer.model_v3, train_cfg, device);
        std::cout << "Model architecture: V3 (MLP + residual on flat features, "
                  << "input_dim=" << v3_cfg.input_dim
                  << "  trunk=" << v3_cfg.trunk_blocks << "×"
                  << v3_cfg.trunk_hidden << ")\n";
    } else {
        auto model_cfg = buildModelConfig(t.model);
        trainer.model_v2 = ::riftbound::ml::V2Model(model_cfg);
        if (t.resume_checkpoint.has_value() && !t.resume_checkpoint->empty()) {
            std::cout << "Resuming V2 from " << *t.resume_checkpoint << "\n";
            torch::load(trainer.model_v2, *t.resume_checkpoint);
        }
        trainer.trainer_v2 = std::make_unique<::riftbound::training::Trainer>(
            trainer.model_v2, train_cfg, device);
        std::cout << "Model architecture: V2 (entity-token transformer, "
                  << "d_model=" << model_cfg.d_model
                  << "  layers=" << model_cfg.encoder_layers << ")\n";
    }

    ::riftbound::training::ReplayBuffer buffer(
        static_cast<size_t>(t.replay_capacity));

    std::mt19937 rng(static_cast<uint32_t>(cfg.seed));

    // ── Iteration loop ──
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Training legend='" << t.legend
              << "'  self_pool=" << t.deck_pools.self.size()
              << "  opp_pool=" << t.deck_pools.opponent.size()
              << "  mirror_ratio=" << t.mirror_ratio << "\n";

    // Phase 6q — track the best checkpoint seen by eval. Used to gate
    // promotion: a checkpoint is "promoted" (kept) only when its eval
    // win-rate beats `eval.promote_threshold`. Failing candidates are
    // deleted from disk so downstream tooling (validate, baselines)
    // never sees a regression. best_iter is the iter number that
    // currently holds the "best" checkpoint; promoted_path is its
    // file path.
    double best_winrate = -1.0;  // -1 sentinel = nothing promoted yet
    int    best_iter    = -1;
    std::string best_path;

    auto t_total_start = std::chrono::steady_clock::now();
    // Phase 6q+ — start_iter lets an interruptable-restart wrapper
    // (e.g. scripts/vastai_launch_interruptable.sh) resume the loop
    // at iter N+1 after pairing with resume_checkpoint=iter_N.pt, so
    // a killed run picks up where it left off without re-running
    // already-completed iters.
    if (t.start_iter > 0) {
        std::cout << "Resuming at iter " << t.start_iter
                  << " (skipping " << t.start_iter << " completed iters)\n";
    }

    // Phase 6v+ — value-loss-gated bootstrap exit. Once
    // `bootstrap_ended_at_iter` is set, subsequent iters are no longer
    // bootstrap regardless of `bootstrap_iters`. The gate looks at the
    // most-recent train step's value_loss. Defaults preserve legacy
    // behavior (threshold==0 → gate disabled, bootstrap_iters is hard cap).
    std::optional<int> bootstrap_ended_at_iter;
    double last_value_loss = std::numeric_limits<double>::infinity();
    const bool gate_enabled = (t.bootstrap_value_loss_threshold > 0.0);
    if (gate_enabled) {
        std::cout << "Bootstrap exit gate: value_loss < "
                  << t.bootstrap_value_loss_threshold
                  << " AND iter >= " << t.bootstrap_iters_min
                  << " (safety cap at iter " << t.bootstrap_iters << ")\n";
    }

    // Watchdog disabled — see commit history. RingLoggers in
    // self_play.cpp still record decision-level events for post-mortem
    // diagnostics, but no auto-abort. The IsTerminal cap at MoveNumber
    // >= 600 prevents the original "MCTSearch infinite loop" class of
    // hang; what the watchdog was actually catching was slow-but-
    // progressing cap-hit MCTS calls, which is a perf issue not a
    // correctness one. We addressed it by reducing num_self_play_workers
    // (matching parallelism to physical cores) instead.

    for (int iter = t.start_iter; iter < t.iterations; ++iter) {
        std::cout << "\n=== iter " << iter << " / " << t.iterations
                  << " ===\n";

        // Phase 6q — MCTS bootstrap. For the first bootstrap_iters,
        // self-play uses pure MCTS (RandomRolloutEvaluator) and the
        // model is bypassed. Use bootstrap_mcts_sims for sim count
        // since the model isn't carrying any computational cost —
        // we can afford deeper MCTS. After the bootstrap, swap to
        // model-driven self-play using self_play_agent.sims.
        //
        // Phase 6v+ — gated exit. Once the value-loss gate fires
        // (or the safety cap is hit), bootstrap is permanently off.
        const int effective_bootstrap_cap =
            bootstrap_ended_at_iter.value_or(t.bootstrap_iters);
        const bool is_bootstrap = (iter < effective_bootstrap_cap);
        // Phase 6t — bootstrap-aware trainer config. During bootstrap
        // the model is uninformative; entropy regularization prevents
        // policy collapse onto a single action, and freezing the value
        // head prevents it from converging to the degenerate "predict
        // zero" optimum on mirror-match data.
        //
        // Phase 6v — smooth the bootstrap→model handoff. The previous
        // cliff-drop transition (entropy 0.02→0.0, value frozen→unfrozen,
        // heuristic→model evaluator, all at exactly iter==bootstrap_iters)
        // produced an EndTurn-only policy collapse at iter 20: value head
        // was still at random init the first time MCTS depended on it, so
        // Q-values were noise and the trained policy prior took over.
        //
        // Two smoothing changes:
        //   1. Unfreeze value head 5 iters BEFORE bootstrap ends so it
        //      gets gradient on heuristic-evaluator self-play data before
        //      model-driven self-play starts.
        //   2. Linearly decay entropy_coef from 0.02 → 0.0 over the 10
        //      iters AFTER bootstrap ends (instead of cliff-dropping).
        const int kValueWarmupIters   = 5;
        const int kEntropyDecayIters  = 10;
        // Phase 6v+ — the value-head freeze was a V2 safety measure
        // against the "predict zero on balanced mirror data" collapse
        // that V2's specific architecture (mean-pool of LayerNorm'd
        // tokens) was prone to. V3 doesn't have that pathology — its
        // value head trains cleanly from iter 0 (validated by the
        // smoke run: value_loss 0.76 → 0.22 in 1 iter). Skip the freeze
        // entirely for V3 so the value-loss signal moves immediately.
        const bool freeze_value =
            (t.model.kind != "v3") &&
            (iter < std::max(0, t.bootstrap_iters - kValueWarmupIters));
        double entropy_coef;
        if (is_bootstrap) {
            entropy_coef = 0.02;
        } else {
            int post_bootstrap = iter - t.bootstrap_iters;
            if (post_bootstrap >= kEntropyDecayIters) {
                entropy_coef = 0.0;
            } else {
                entropy_coef = 0.02 * (1.0 - static_cast<double>(post_bootstrap) /
                                                kEntropyDecayIters);
            }
        }
        trainer.setEntropyCoef(entropy_coef);
        trainer.setFreezeValueHead(freeze_value);

        // Self-play across the deck pool — one game per sampled pair.
        ::riftbound::training::SelfPlayConfig sp_cfg{};
        sp_cfg.num_games     = 1;  // we drive the count externally
        sp_cfg.mcts_sims     = is_bootstrap
            ? t.bootstrap_mcts_sims
            : t.self_play_agent.sims.value_or(50);
        sp_cfg.max_decisions = t.max_decisions_per_game;
        sp_cfg.uct_c         = t.mcts.uct_c;
        sp_cfg.use_model     = !is_bootstrap;
        // AlphaZero exploration recipe (Phase 6m). Pulled from
        // config.training.mcts — schema parses and defaults these.
        sp_cfg.dirichlet_alpha              = t.mcts.dirichlet_alpha;
        sp_cfg.dirichlet_fraction           = t.mcts.dirichlet_fraction;
        sp_cfg.temperature                  = t.mcts.temperature;
        sp_cfg.temperature_drop_at_decision = t.mcts.temperature_drop_at_decision;
        // Phase 6v — propagate model architecture choice so playOneGame
        // captures the matching ReplaySample field (flat_state for V3,
        // entity tokens for V2) and uses the matching evaluator path.
        sp_cfg.model_kind    = t.model.kind;
        sp_cfg.verbose       = false;

        if (is_bootstrap) {
            std::cout << "  [bootstrap] iter " << iter << " < "
                      << t.bootstrap_iters
                      << " — pure MCTS self-play (sims="
                      << sp_cfg.mcts_sims << ", no model)\n";
        }

        ::riftbound::training::SelfPlayStats agg{};
        auto t_sp_start = std::chrono::steady_clock::now();

        // Phase 6n — pre-sample all (deck_pair, seed) tuples
        // sequentially from the main rng so dispatch order is
        // deterministic w.r.t. cfg.seed. Each worker thread then runs
        // its own game using the pre-sampled pair + a per-game RNG
        // seeded from the deterministic game_seed.
        struct GameDispatch { DeckPair pair; uint64_t seed; };
        std::vector<GameDispatch> dispatches;
        dispatches.reserve(t.games_per_iter);
        for (int g = 0; g < t.games_per_iter; ++g) {
            GameDispatch d;
            d.pair = samplePair(t.deck_pools, t.mirror_ratio, rng);
            d.seed = static_cast<uint64_t>(cfg.seed) ^
                     (static_cast<uint64_t>(iter) * 1000003u) ^
                     (static_cast<uint64_t>(g) * 31337u);
            dispatches.push_back(d);
        }

        // Parallel game dispatch. The V2Model is read-only during
        // self-play (forward in eval mode is thread-safe in libtorch);
        // the ReplayBuffer mutex-protects push(). std::cout writes
        // and `agg` updates serialize on stats_mu so per-game progress
        // lines don't interleave.
        const int num_workers = std::max(1, t.num_self_play_workers);
        std::mutex stats_mu;
        {
            boost::asio::thread_pool pool(num_workers);
            for (int g = 0; g < t.games_per_iter; ++g) {
                boost::asio::post(pool, [&, g] {
                    std::mt19937 worker_rng(
                        static_cast<uint32_t>(dispatches[g].seed));
                    auto t_game_start = std::chrono::steady_clock::now();
                    auto game = ::open_spiel::LoadGame(
                        gameString(cfg.registry, dispatches[g].pair,
                                   dispatches[g].seed));
                    auto s = trainer.isV3()
                        ? ::riftbound::training::runSelfPlayV3(
                              *game, trainer.model_v3, buffer, sp_cfg,
                              worker_rng, iter, g)
                        : ::riftbound::training::runSelfPlay(
                              *game, trainer.model_v2, buffer, sp_cfg,
                              worker_rng, iter, g);
                    auto game_secs = std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - t_game_start).count();

                    std::lock_guard<std::mutex> lk(stats_mu);
                    agg.games_played    += s.games_played;
                    agg.p1_wins         += s.p1_wins;
                    agg.p2_wins         += s.p2_wins;
                    agg.draws           += s.draws;
                    agg.tuples_added    += s.tuples_added;
                    agg.total_decisions += s.total_decisions;
                    agg.exit_terminal   += s.exit_terminal;
                    agg.exit_cap        += s.exit_cap;
                    agg.exit_stuck      += s.exit_stuck;
                    const char* outcome = (s.p1_wins   ? "P1"
                                         : s.p2_wins   ? "P2"
                                         : s.draws     ? "draw"
                                                        : "?");
                    static const char* exit_tags[] = {"term", "cap", "stuck"};
                    const char* exit_tag = (s.last_exit_reason >= 0 &&
                                            s.last_exit_reason <= 2)
                                            ? exit_tags[s.last_exit_reason]
                                            : "?";
                    std::cout << "  [iter " << iter << "] game "
                              << (g + 1) << "/" << t.games_per_iter
                              << "  dec=" << s.total_decisions
                              << "  win=" << outcome
                              << "  exit=" << exit_tag
                              << "  iter_WLD=" << agg.p1_wins << "/"
                              << agg.p2_wins << "/" << agg.draws
                              << "  buf=" << buffer.size()
                              << "  (" << game_secs << "s)\n"
                              << std::flush;
                });
            }
            pool.join();  // wait for all games to finish
        }
        auto sp_secs = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_sp_start).count();
        std::cout << "  self-play: " << agg.games_played << " games, "
                  << agg.tuples_added << " tuples, W/L/D="
                  << agg.p1_wins << "/" << agg.p2_wins << "/" << agg.draws
                  << "  exit_term/cap/stuck="
                  << agg.exit_terminal << "/" << agg.exit_cap << "/"
                  << agg.exit_stuck
                  << "  avg_dec=" << (agg.games_played > 0
                        ? agg.total_decisions / agg.games_played : 0)
                  << "  (" << sp_secs << "s)\n";

        if (buffer.size() < static_cast<size_t>(t.batch_size)) {
            std::cout << "  buffer too small (" << buffer.size()
                      << " < batch_size " << t.batch_size
                      << ") — skipping train\n";
            continue;
        }

        // Train
        auto t_tr_start = std::chrono::steady_clock::now();
        double sum_pl = 0, sum_vl = 0, sum_tl = 0;
        for (int s = 0; s < t.train_steps_per_iter; ++s) {
            auto batch = buffer.sample(
                static_cast<size_t>(t.batch_size), rng);
            auto [pl, vl, tl] = trainer.step(batch);
            sum_pl += pl; sum_vl += vl; sum_tl += tl;
        }
        auto tr_secs = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_tr_start).count();
        if (t.train_steps_per_iter > 0) {
            std::cout << "  train:     " << t.train_steps_per_iter << " steps, "
                      << "loss policy=" << (sum_pl / t.train_steps_per_iter)
                      << " value=" << (sum_vl / t.train_steps_per_iter)
                      << " total=" << (sum_tl / t.train_steps_per_iter)
                      << "  (" << tr_secs << "s)\n";
        }

        // Phase 6v+ — value-loss gate check. The gate fires when the
        // model's value head has learned to predict outcomes (value
        // loss low) AND we've done at least bootstrap_iters_min iters.
        // bootstrap_iters remains the safety cap. Once fired, the next
        // iter switches to model-driven self-play.
        if (gate_enabled && !bootstrap_ended_at_iter.has_value()
                && is_bootstrap
                && t.train_steps_per_iter > 0) {
            last_value_loss = sum_vl / t.train_steps_per_iter;
            if (iter + 1 >= t.bootstrap_iters_min &&
                last_value_loss < t.bootstrap_value_loss_threshold) {
                bootstrap_ended_at_iter = iter + 1;
                std::cout << "  [gate] bootstrap_value_loss "
                          << last_value_loss << " < threshold "
                          << t.bootstrap_value_loss_threshold
                          << " — bootstrap ends at iter " << (iter + 1)
                          << " (cap was " << t.bootstrap_iters << ")\n";
            }
        }

        // Checkpoint
        std::string saved_ckpt;
        if (t.checkpoint_every_iter > 0 &&
            (iter % t.checkpoint_every_iter == 0 ||
             iter == t.iterations - 1)) {
            saved_ckpt = t.checkpoint_dir + "/iter_" +
                        std::to_string(iter) + ".pt";
            trainer.save(saved_ckpt);
            std::cout << "  saved " << saved_ckpt << "\n";
            pruneCheckpoints(t.checkpoint_dir, t.keep_last_n);
        }

        // Phase 6m+o — Eval block.
        // If config.training.eval.every_iter is set and this iter
        // matches, spawn a subprocess of riftbound_openspiel to play
        // `eval.games` games of (just-saved checkpoint) vs (baseline)
        // with --render-html so a human can spot-check trained-model
        // behavior. Eval blocks training for its duration (sequential),
        // so set every_iter conservatively (5 / 10) so the cumulative
        // eval cost stays well under 25% of total wallclock.
        if (t.eval.every_iter.has_value() &&
            t.eval.games.has_value() &&
            !saved_ckpt.empty() &&
            iter % *t.eval.every_iter == 0) {
            int eval_games = *t.eval.games;
            int eval_sims  = t.eval.baseline_sims.value_or(10);
            std::string baseline = t.eval.baseline.value_or("random");
            std::string baseline_spec;
            if (baseline == "random") {
                baseline_spec = "random";
            } else if (baseline == "mcts") {
                baseline_spec = "mcts:sims=" + std::to_string(eval_sims);
            } else if (baseline == "previous_iter") {
                // Find the prior saved checkpoint via checkpoint_every_iter.
                int prev = iter - t.checkpoint_every_iter;
                if (prev < 0) {
                    std::cout << "  eval: no previous_iter checkpoint yet "
                                 "(iter=" << iter << ")\n";
                    continue;
                }
                auto prev_ckpt = t.checkpoint_dir + "/iter_" +
                                  std::to_string(prev) + ".pt";
                baseline_spec = "model:sims=" + std::to_string(eval_sims) +
                                ",path=" + prev_ckpt;
            } else {
                std::cout << "  eval: unknown baseline '" << baseline << "'\n";
                continue;
            }

            // Build subprocess command. Pipes stdout to a temp file so
            // we can grep the win rate without polluting the training
            // log with every per-game line.
            std::string deck = t.deck_pools.self.empty()
                ? std::string() : t.deck_pools.self[0];
            int sims = t.self_play_agent.sims.value_or(40);
            std::string log_path = "/tmp/eval_iter_" +
                                    std::to_string(iter) + ".log";
            // Phase 6q+ — resolve openspiel binary path with fallbacks so
            // the eval subprocess works for both the legacy build-torch/
            // out-of-tree build and the standard build/ tree the GPU
            // deploy uses. RIFTBOUND_OPENSPIEL env var overrides if set.
            const char* osbin_env = std::getenv("RIFTBOUND_OPENSPIEL");
            std::string osbin;
            if (osbin_env && *osbin_env) {
                osbin = osbin_env;
            } else if (std::filesystem::exists(
                "build/src/openspiel/riftbound_openspiel")) {
                osbin = "build/src/openspiel/riftbound_openspiel";
            } else if (std::filesystem::exists(
                "build-release/src/openspiel/riftbound_openspiel")) {
                osbin = "build-release/src/openspiel/riftbound_openspiel";
            } else {
                osbin = "./build-torch/src/openspiel/riftbound_openspiel";
            }
            std::ostringstream cmd;
            cmd << osbin
                << " --agent1 model:sims=" << sims
                <<                ",path=" << saved_ckpt
                << " --agent2 " << baseline_spec
                << " --deck1 " << deck
                << " --deck2 " << deck
                << " --games " << eval_games
                << " --seed " << (static_cast<int>(cfg.seed) + iter * 9001)
                << " --threads 1"
                << " --render-html"
                << " > " << log_path << " 2>&1";

            std::cout << "  eval: " << baseline_spec << " x "
                      << eval_games << " games...\n" << std::flush;
            auto t_eval_start = std::chrono::steady_clock::now();
            int rc = std::system(cmd.str().c_str());
            auto eval_secs = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - t_eval_start).count();

            // Grep the win-rate lines from the temp log. The CLI prints
            // "P1 wins: K (X%)" / "P2 wins: ..." / "Draws: ...".
            std::string grep_cmd = "grep -E '^  P[12] wins|^  Draws' " +
                                    log_path + " | head -3";
            auto p1_win = std::string("?");
            auto p2_win = std::string("?");
            auto draws  = std::string("?");
            if (FILE* fp = popen(grep_cmd.c_str(), "r")) {
                char buf[512];
                while (fgets(buf, sizeof(buf), fp)) {
                    std::string line(buf);
                    if (line.find("P1 wins") != std::string::npos) {
                        p1_win = line.substr(line.find(":") + 1);
                    } else if (line.find("P2 wins") != std::string::npos) {
                        p2_win = line.substr(line.find(":") + 1);
                    } else if (line.find("Draws") != std::string::npos) {
                        draws = line.substr(line.find(":") + 1);
                    }
                }
                pclose(fp);
            }
            // Trim trailing newlines.
            for (auto* s : {&p1_win, &p2_win, &draws}) {
                while (!s->empty() && (s->back() == '\n' || s->back() == ' '))
                    s->pop_back();
            }
            std::cout << "  eval rc=" << rc
                      << "  P1=" << p1_win
                      << "  P2=" << p2_win
                      << "  D=" << draws
                      << "  (" << eval_secs << "s)\n";

            // ── Phase 6q — validation-gated promotion ──
            //
            // Parse the integer percent from p1_win (format
            // " K (XX%)" / " K (XX.YY%)"). If it's at or above
            // eval.promote_threshold (×100), keep the checkpoint and
            // update best_*. If below, delete the just-saved file so
            // downstream tooling never picks up a regression.
            //
            // We pin the gate to P1 win-rate only because eval today
            // runs model-as-P1 only. When eval grows split P1/P2 (so
            // we can subtract first-mover advantage cleanly), update
            // this to use overall_winrate instead.
            double cand_winrate = -1.0;
            // p1_win looks like " K (XX%)" — find first '(' and parse
            // the integer/float after it.
            auto lp = p1_win.find('(');
            auto rp = p1_win.find('%');
            if (rc == 0 && lp != std::string::npos && rp != std::string::npos &&
                rp > lp + 1) {
                try {
                    cand_winrate = std::stod(p1_win.substr(lp + 1,
                                                            rp - lp - 1)) / 100.0;
                } catch (...) {
                    cand_winrate = -1.0;
                }
            }
            double threshold = t.eval.promote_threshold;
            bool promote = false;
            std::string verdict;
            if (cand_winrate < 0) {
                verdict = "UNDETERMINED (eval failed to parse)";
            } else if (cand_winrate >= threshold) {
                promote = true;
                verdict = "PROMOTE (winrate=" +
                          std::to_string(cand_winrate) + " >= threshold=" +
                          std::to_string(threshold) + ")";
            } else {
                verdict = "REJECT (winrate=" +
                          std::to_string(cand_winrate) + " < threshold=" +
                          std::to_string(threshold) + ")";
            }
            std::cout << "  promotion: " << verdict << "\n";

            if (promote) {
                // Update best-tracker. >= (not >) intentionally — when
                // a later iter TIES the best winrate, swing best.pt to
                // the most-recent checkpoint. Reasoning: small-N eval
                // games make tied "100%" common, and a later iter has
                // had more training data, so it's the better default
                // even when the score is a wash. Bumping eval.games
                // breaks the tie statistically; until then, recency
                // wins.
                if (cand_winrate >= best_winrate) {
                    best_winrate = cand_winrate;
                    best_iter    = iter;
                    best_path    = saved_ckpt;
                    // Also write a stable best.pt symlink for downstream
                    // tooling that doesn't want to grep iter numbers.
                    std::error_code ln_ec;
                    namespace fs2 = std::filesystem;
                    fs2::path best_link = fs2::path(t.checkpoint_dir) /
                                           "best.pt";
                    fs2::remove(best_link, ln_ec);  // ignore "no such file"
                    fs2::create_symlink(fs2::path(saved_ckpt).filename(),
                                         best_link, ln_ec);
                    if (ln_ec) {
                        // Symlink failed (Windows, no perms, etc.) —
                        // fall back to a hard copy.
                        fs2::copy_file(saved_ckpt, best_link,
                                        fs2::copy_options::overwrite_existing,
                                        ln_ec);
                    }
                    std::cout << "  best.pt → " << saved_ckpt
                              << " (iter " << iter << ", winrate "
                              << cand_winrate << ")\n";
                }
            } else {
                // Reject: delete the just-saved checkpoint so it
                // can't be mistaken for "valid" by validate scripts
                // or downstream baselines.
                std::error_code rm_ec;
                std::filesystem::remove(saved_ckpt, rm_ec);
                if (!rm_ec) {
                    std::cout << "  deleted regressed checkpoint: "
                              << saved_ckpt << "\n";
                    if (best_iter >= 0) {
                        std::cout << "  best so far: iter " << best_iter
                                  << " (winrate " << best_winrate
                                  << " at " << best_path << ")\n";
                    } else {
                        std::cout << "  no promoted checkpoint yet — "
                                  << "best.pt symlink will not exist\n";
                    }
                }
            }

            // Move the just-produced HTMLs into a per-(legend, iter)
            // subfolder so they don't all pile up in ./replays/ with
            // UUID names. Use t.legend (the config-declared legend
            // name) to namespace per-legend.
            std::string legend_slug = t.legend;
            for (auto& ch : legend_slug) {
                if (ch == ' ' || ch == ',' || ch == '\'') ch = '_';
            }
            // lower-case
            for (auto& ch : legend_slug) {
                if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
            }
            namespace fs = std::filesystem;
            fs::path dest_dir = fs::path("replays") /
                                ("eval_" + legend_slug + "_iter_" +
                                 std::to_string(iter));
            std::error_code mkdir_ec;
            fs::create_directories(dest_dir, mkdir_ec);
            int moved = 0;
            // Move only files written more recently than the start of
            // THIS eval invocation (cuts cross-legend pollution if
            // both legends happen to eval at the same iter). Using
            // file_time_type::clock to avoid system_clock/file_clock
            // conversion portability issues.
            auto cutoff = fs::file_time_type::clock::now() -
                std::chrono::seconds(
                    static_cast<int>(eval_secs) + 30);
            for (auto& entry : fs::directory_iterator("replays")) {
                if (!entry.is_regular_file()) continue;
                auto p = entry.path();
                if (p.extension() != ".html") continue;
                if (p.filename().string().rfind("replay_", 0) != 0) continue;
                if (fs::last_write_time(p) < cutoff) continue;
                fs::path dest = dest_dir / p.filename();
                std::error_code mv_ec;
                fs::rename(p, dest, mv_ec);
                if (!mv_ec) ++moved;
            }
            std::cout << "  eval replays: " << moved
                      << " HTMLs → " << dest_dir.string() << "/\n";
        }
    }

    auto total_secs = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_total_start).count();
    std::cout << "\nDone. " << t.iterations << " iterations in "
              << total_secs << "s\n";
    return t.iterations;
}

}  // namespace riftbound::training
