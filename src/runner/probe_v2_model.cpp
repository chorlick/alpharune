/// @file probe_v2_model.cpp
/// Value-head probe — Phase 4 fallback diagnostic for Phase 6k.
///
/// Loads a V2 checkpoint and runs forward() on a handful of
/// hand-crafted GameStates with known correct-value answers. Prints
/// value + top-5 flat_policy_logits per state. Goal: confirm the
/// inference-time perspective wiring is sign-correct before burning
/// more GPU on retraining.
///
/// Usage:
///   probe_v2_model --ckpt <path/to/iter_N.pt> [--registry cards/registry.json]
///
/// Test states (all from P1's perspective):
///   1. P1 about to win   (P1.score = 7, P2.score = 0)  → expect value > 0
///   2. P2 about to win   (P1.score = 0, P2.score = 7)  → expect value < 0
///   3. Symmetric opening (both scores 0, empty board)  → expect value ≈ 0
///
/// A sign-correct value head returns values consistent with those
/// expectations across legends. Sign-inverted = perspective bug.

#include "core/card_db.h"
#include "core/game_object.h"
#include "core/game_state.h"
#include "core/types.h"
#include "ml/entity_tokens.h"
#include "ml/v2_model.h"

#include <nlohmann/json.hpp>
#include <torch/torch.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace riftbound;
using ::riftbound::ml::EntityTokens;
using ::riftbound::ml::V2Model;
using ::riftbound::ml::V2ModelConfig;
using ::riftbound::ml::V2ModelImpl;
using ::riftbound::ml::extractEntityTokens;
using ::riftbound::ml::kEntityStatsDim;

namespace {

// Find cards/registry.json from a few common spots.
std::string findRegistry(const std::string& override_path) {
    namespace fs = std::filesystem;
    if (!override_path.empty() && fs::exists(override_path)) return override_path;
    if (const char* env = std::getenv("RIFTBOUND_ROOT")) {
        auto p = fs::path(env) / "cards" / "registry.json";
        if (fs::exists(p)) return p.string();
    }
    for (const auto& c : {"cards/registry.json",
                          "../cards/registry.json",
                          "../../cards/registry.json"}) {
        if (fs::exists(c)) return c;
    }
    return override_path.empty() ? "cards/registry.json" : override_path;
}

// Copied from libtorch_evaluator.cpp's loadConfigSidecar — we need to
// honor the checkpoint's training-time model config so the V2Model
// state-dict load doesn't shape-mismatch.
bool loadConfigSidecar(const std::string& ckpt_path, V2ModelConfig& out) {
    auto side = ckpt_path + ".config.json";
    if (ckpt_path.empty() || !std::filesystem::exists(side)) return false;
    nlohmann::json j;
    std::ifstream(side) >> j;
    auto gi = [&](const char* k, int& dst) { if (j.contains(k)) dst = j[k].get<int>(); };
    auto gd = [&](const char* k, double& dst) { if (j.contains(k)) dst = j[k].get<double>(); };
    gi("num_card_defs",       out.num_card_defs);
    gi("num_zones",           out.num_zones);
    gi("num_domains",         out.num_domains);
    gi("num_stances",         out.num_stances);
    gi("num_spatial",         out.num_spatial);
    gi("max_entities",        out.max_entities);
    gi("stats_dim",           out.stats_dim);
    gi("num_action_types",    out.num_action_types);
    gi("max_chain_index",     out.max_chain_index);
    gi("card_embed_dim",      out.card_embed_dim);
    gi("zone_embed_dim",      out.zone_embed_dim);
    gi("domain_embed_dim",    out.domain_embed_dim);
    gi("stance_embed_dim",    out.stance_embed_dim);
    gi("stats_proj_dim",      out.stats_proj_dim);
    gi("perspective_dim",     out.perspective_dim);
    gi("spatial_dim",         out.spatial_dim);
    gi("d_model",             out.d_model);
    gi("encoder_layers",      out.encoder_layers);
    gi("encoder_heads",       out.encoder_heads);
    gi("encoder_ffn_dim",     out.encoder_ffn_dim);
    gd("encoder_dropout",     out.encoder_dropout);
    gi("spatial_attn_layers", out.spatial_attn_layers);
    gi("spatial_attn_heads",  out.spatial_attn_heads);
    gi("num_edge_types",      out.num_edge_types);
    gi("policy_hidden",       out.policy_hidden);
    gi("value_hidden",        out.value_hidden);
    gi("num_action_slots",    out.num_action_slots);
    return true;
}

// Pack a single EntityTokens into batched (B=1) tensors. Mirrors
// libtorch_evaluator's packOne() — kept inline so this probe is
// self-contained and doesn't pull in the OpenSpiel deps.
struct PackedInputs {
    torch::Tensor card_ids, zone_ids, domain_ids, stance_ids, stats;
    torch::Tensor is_perspective, chain_index, spatial_node, token_mask;
};
PackedInputs packOne(const EntityTokens& t) {
    const int N = static_cast<int>(t.card_def_id.size());
    auto fi = [&](const std::vector<int32_t>& v) {
        return torch::from_blob(const_cast<int32_t*>(v.data()), {1, N},
                                torch::TensorOptions().dtype(torch::kInt32))
                    .to(torch::kLong).clone();
    };
    PackedInputs out;
    out.card_ids       = fi(t.card_def_id);
    out.zone_ids       = fi(t.zone_id);
    out.domain_ids     = fi(t.domain_id);
    out.stance_ids     = fi(t.stance_id);
    out.is_perspective = fi(t.is_perspective);
    out.chain_index    = fi(t.chain_index);
    out.spatial_node   = fi(t.spatial_node);
    out.stats = torch::from_blob(const_cast<float*>(t.stats.data()),
                                  {1, N, kEntityStatsDim},
                                  torch::TensorOptions().dtype(torch::kFloat))
                    .clone();
    auto mask_int = fi(t.valid_mask);
    out.token_mask = mask_int.to(torch::kBool);
    return out;
}

// Build a minimal but realistic GameState with explicit scores.
// Both players get a small hand of stub objects so the entity-token
// extractor has something to chew on (a wholly empty state isn't
// representative of what the model saw during training).
GameState buildState(int p1_score, int p2_score, int hand_size = 5) {
    GameState state{};
    state.players[0].id    = PlayerId::Player1;
    state.players[1].id    = PlayerId::Player2;
    state.players[0].score = p1_score;
    state.players[1].score = p2_score;

    for (int p = 0; p < 2; ++p) {
        PlayerId pid = (p == 0) ? PlayerId::Player1 : PlayerId::Player2;
        for (int i = 0; i < hand_size; ++i) {
            GameObjectId id = state.createObject();
            auto& obj = state.getObject(id);
            obj.owner       = pid;
            obj.controller  = pid;
            obj.card_type   = CardType::Unit;
            obj.zone        = ZoneType::Hand;
            obj.card_def_id = 100 + i;  // arbitrary; needs to exist in CardDB
            state.players[p].hand.push_back(id);
        }
    }
    return state;
}

// Build an ASYMMETRIC board state — `dominant` controls 5 ready units
// at the first battlefield (all with might=4 and the same simple
// card_def_id) and 5 cards in hand; the other player has nothing in
// play and an empty hand. Both battlefields exist but neither is
// scored. This is a strong "dominant is winning" signal that the
// model SHOULD encode in its value head regardless of which side is
// the perspective.
GameState buildAsymmetricBoard(PlayerId dominant) {
    GameState state{};
    state.players[0].id = PlayerId::Player1;
    state.players[1].id = PlayerId::Player2;
    // Two battlefields, neither scored, neither contested. Dominant
    // player controls both (this matches what "winning" typically
    // looks like in self-play data).
    state.battlefields.clear();
    for (int i = 0; i < 2; ++i) {
        BattlefieldState bf{};
        bf.id            = static_cast<BattlefieldId>(i);
        bf.card_object_id = kInvalidId;
        bf.controller    = dominant;
        bf.is_contested  = false;
        state.battlefields.push_back(bf);
    }
    int dom_idx = (dominant == PlayerId::Player1) ? 0 : 1;
    for (int i = 0; i < 5; ++i) {
        GameObjectId id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner            = dominant;
        obj.controller       = dominant;
        obj.card_type        = CardType::Unit;
        obj.zone             = ZoneType::BattlefieldZone;
        obj.card_def_id      = 300 + i;
        obj.location         = BattlefieldLocation{static_cast<BattlefieldId>(0)};
        obj.base_might       = 4;
        obj.current_might    = 4;
        obj.is_exhausted     = false;
    }
    (void)dom_idx;
    // Both players get a 5-card hand of distinct stubs.
    for (int p = 0; p < 2; ++p) {
        PlayerId pid = (p == 0) ? PlayerId::Player1 : PlayerId::Player2;
        for (int i = 0; i < 5; ++i) {
            GameObjectId id = state.createObject();
            auto& obj = state.getObject(id);
            obj.owner       = pid;
            obj.controller  = pid;
            obj.card_type   = CardType::Unit;
            obj.zone        = ZoneType::Hand;
            obj.card_def_id = 400 + i;
            state.players[p].hand.push_back(id);
        }
    }
    return state;
}

// Run forward and return the scalar value. Caller decides how to
// print / compare it.
double forwardValue(V2Model& model, const CardDB& db,
                    const GameState& state, PlayerId perspective) {
    torch::NoGradGuard nograd;
    auto tokens = extractEntityTokens(state, perspective, db);
    auto in = packOne(tokens);
    auto out = model->forward(in.card_ids, in.zone_ids, in.domain_ids,
                               in.stance_ids, in.stats, in.is_perspective,
                               in.chain_index, in.spatial_node, in.token_mask);
    return out.value.item<double>();
}

// Print value + top-5 flat_policy_logits for a single state.
void probeState(const std::string& label,
                V2Model& model,
                const CardDB& db,
                const GameState& state,
                PlayerId perspective,
                double expect_min,
                double expect_max) {
    torch::NoGradGuard nograd;
    auto tokens = extractEntityTokens(state, perspective, db);
    auto in = packOne(tokens);
    auto out = model->forward(in.card_ids, in.zone_ids, in.domain_ids,
                               in.stance_ids, in.stats, in.is_perspective,
                               in.chain_index, in.spatial_node, in.token_mask);

    double v = out.value.item<double>();
    bool pass = (v >= expect_min && v <= expect_max);

    std::cout << "\n  --- " << label << " ---\n";
    std::cout << "  P1.score=" << state.players[0].score
              << "  P2.score=" << state.players[1].score
              << "  perspective=" << (perspective == PlayerId::Player1 ? "P1" : "P2")
              << "\n";
    std::cout << "  value = " << std::fixed << std::setprecision(4) << v
              << "  (expected in [" << expect_min << ", " << expect_max << "])"
              << (pass ? "   PASS" : "   FAIL") << "\n";

    // Top-5 logits.
    auto flat = out.flat_policy_logits.contiguous().view({-1});
    auto [topv, topi] = torch::topk(flat, 5);
    std::cout << "  top-5 policy slots: ";
    for (int i = 0; i < 5; ++i) {
        std::cout << topi[i].item<int64_t>() << "(" << std::setprecision(2)
                  << topv[i].item<double>() << ") ";
    }
    std::cout << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IOLBF, 0);

    std::string ckpt_path;
    std::string registry_override;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--ckpt" && i + 1 < argc)         ckpt_path = argv[++i];
        else if (a == "--registry" && i + 1 < argc) registry_override = argv[++i];
        else if (a == "-h" || a == "--help") {
            std::cout << "usage: probe_v2_model --ckpt <path.pt> "
                         "[--registry cards/registry.json]\n";
            return 0;
        }
    }
    if (ckpt_path.empty()) {
        std::cout << "(no --ckpt) Probing FRESHLY-INITIALIZED V2Model "
                     "(d_model=128 to match stable_* training config)\n";
    }

    // Load CardDB so the extractor can look up card metadata.
    CardDB db;
    db.loadFromRegistry(findRegistry(registry_override));

    // Load model with config from sidecar if present (otherwise defaults).
    V2ModelConfig cfg;
    if (ckpt_path.empty()) {
        // Match the d_model used by stable_* training configs so the
        // random-init baseline is directly comparable to the trained
        // checkpoints.
        cfg.d_model        = 128;
        cfg.encoder_layers = 2;
        cfg.encoder_heads  = 4;
        cfg.encoder_ffn_dim = 256;
        cfg.card_embed_dim = 64;
        cfg.spatial_attn_layers = 1;
    }
    bool got_sidecar = !ckpt_path.empty() && loadConfigSidecar(ckpt_path, cfg);
    std::cout << "Probe: " << (ckpt_path.empty() ? "<random-init>" : ckpt_path)
              << "\n";
    std::cout << "Config: " << (got_sidecar ? "sidecar"
                                            : "defaults (no sidecar found)")
              << "  d_model=" << cfg.d_model
              << "  num_action_slots=" << cfg.num_action_slots << "\n";

    V2Model model(cfg);
    if (!ckpt_path.empty()) {
        torch::load(model, ckpt_path, torch::kCPU);
    }
    model->eval();

    // ── Group 1: score-only states (sanity check that pipeline works) ──
    // These are expected to all return the same value because the V2
    // entity-token extractor does NOT encode player score as a per-
    // token feature — finding from 2026-05-18 first probe run.
    std::cout << "\n## Group 1 — score-only (model can't see score; "
                 "all four should match)\n";
    probeState("P1 about to win  (7-0) from P1", model, db,
               buildState(7, 0), PlayerId::Player1, -1.0, 1.0);
    probeState("P2 about to win  (0-7) from P1", model, db,
               buildState(0, 7), PlayerId::Player1, -1.0, 1.0);
    probeState("Symmetric        (0-0) from P1", model, db,
               buildState(0, 0), PlayerId::Player1, -1.0, 1.0);

    // ── Group 2: asymmetric board, both perspectives ──
    // This IS the perspective test. P1 controls 5 ready units at BFs
    // (clear winning position). From P1's perspective we expect a
    // POSITIVE value; from P2's we expect a NEGATIVE value (the same
    // state, sign-flipped). If both are positive or both negative,
    // the perspective wiring is broken.
    std::cout << "\n## Group 2 — asymmetric board, P1 dominant (the "
                 "perspective test)\n";
    auto p1_dom = buildAsymmetricBoard(PlayerId::Player1);
    probeState("P1 dominant — from P1 perspective", model, db,
               p1_dom, PlayerId::Player1, 0.05, 1.0);
    probeState("P1 dominant — from P2 perspective", model, db,
               p1_dom, PlayerId::Player2, -1.0, -0.05);

    auto p2_dom = buildAsymmetricBoard(PlayerId::Player2);
    probeState("P2 dominant — from P1 perspective", model, db,
               p2_dom, PlayerId::Player1, -1.0, -0.05);
    probeState("P2 dominant — from P2 perspective", model, db,
               p2_dom, PlayerId::Player2, 0.05, 1.0);

    // Summary line that's easy to grep for.
    double v_p1d_p1 = forwardValue(model, db, p1_dom, PlayerId::Player1);
    double v_p1d_p2 = forwardValue(model, db, p1_dom, PlayerId::Player2);
    double v_p2d_p1 = forwardValue(model, db, p2_dom, PlayerId::Player1);
    double v_p2d_p2 = forwardValue(model, db, p2_dom, PlayerId::Player2);
    bool perspective_ok =
        (v_p1d_p1 > 0 && v_p1d_p2 < 0 && v_p2d_p1 < 0 && v_p2d_p2 > 0);
    std::cout << "\nPERSPECTIVE_WIRING="
              << (perspective_ok ? "OK" : "BROKEN")
              << "  (p1dom_p1=" << v_p1d_p1
              << " p1dom_p2=" << v_p1d_p2
              << " p2dom_p1=" << v_p2d_p1
              << " p2dom_p2=" << v_p2d_p2 << ")\n";

    std::cout << "\nDone.\n";
    return 0;
}
