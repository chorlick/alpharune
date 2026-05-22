/// @file probe_v3_model.cpp
/// V3 model diagnostic probe — mirror of probe_v2_model for the V3
/// MLP+residual architecture. Loads a V3 checkpoint and runs forward()
/// on hand-crafted GameStates with known correct-value expectations.
///
/// Phase 6v. Validates the V3 trunk discriminates inputs — the failure
/// mode V2 exhibited (constant output across drastically different
/// states) is what we're explicitly checking against.
///
/// Usage:
///   probe_v3_model --ckpt <path/to/iter_N.pt> [--registry cards/registry.json]
///   probe_v3_model                       # random-init probe (no ckpt)
///
/// Reports PERSPECTIVE_WIRING=OK or BROKEN based on the asymmetric
/// board test (same as V2 probe). Also reports VARIANCE_OK / COLLAPSED
/// based on whether outputs differ across Group 1/Group 2 states.

#include "core/card_db.h"
#include "core/game_object.h"
#include "core/game_state.h"
#include "core/types.h"
#include "ml/feature_extractor.h"
#include "ml/v3_model.h"

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
using ::riftbound::ml::V3Model;
using ::riftbound::ml::V3ModelConfig;
using ::riftbound::ml::V3ModelImpl;
using ::riftbound::ml::extractStateFeatures;
using ::riftbound::ml::kStateFeatureDim;

namespace {

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

bool loadConfigSidecar(const std::string& ckpt_path, V3ModelConfig& out) {
    auto side = ckpt_path + ".config.json";
    if (ckpt_path.empty() || !std::filesystem::exists(side)) return false;
    nlohmann::json j;
    std::ifstream(side) >> j;
    auto gi = [&](const char* k, int& dst) { if (j.contains(k)) dst = j[k].get<int>(); };
    gi("input_dim",        out.input_dim);
    gi("trunk_hidden",     out.trunk_hidden);
    gi("trunk_blocks",     out.trunk_blocks);
    gi("policy_hidden",    out.policy_hidden);
    gi("value_hidden",     out.value_hidden);
    gi("num_action_slots", out.num_action_slots);
    return true;
}

torch::Tensor packFlatState(const GameState& state, PlayerId perspective,
                             const CardDB& db, int input_dim) {
    auto feats = extractStateFeatures(state, perspective, db);
    auto t = torch::zeros({1, input_dim},
                          torch::TensorOptions().dtype(torch::kFloat));
    auto acc = t.accessor<float, 2>();
    const int n = std::min<int>(input_dim, static_cast<int>(feats.size()));
    for (int j = 0; j < n; ++j) acc[0][j] = feats[j];
    return t;
}

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
            obj.card_def_id = 100 + i;
            state.players[p].hand.push_back(id);
        }
    }
    return state;
}

GameState buildAsymmetricBoard(PlayerId dominant) {
    GameState state{};
    state.players[0].id = PlayerId::Player1;
    state.players[1].id = PlayerId::Player2;
    state.battlefields.clear();
    for (int i = 0; i < 2; ++i) {
        BattlefieldState bf{};
        bf.id            = static_cast<BattlefieldId>(i);
        bf.card_object_id = kInvalidId;
        bf.controller    = dominant;
        bf.is_contested  = false;
        state.battlefields.push_back(bf);
    }
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

double forwardValue(V3Model& model, const CardDB& db,
                    const GameState& state, PlayerId perspective,
                    int input_dim) {
    torch::NoGradGuard no_grad;
    auto x = packFlatState(state, perspective, db, input_dim);
    auto out = model->forward(x);
    return out.value.item<double>();
}

void probeState(const std::string& label,
                V3Model& model,
                const CardDB& db,
                const GameState& state,
                PlayerId perspective,
                double expect_min,
                double expect_max,
                int input_dim) {
    torch::NoGradGuard no_grad;
    auto x = packFlatState(state, perspective, db, input_dim);
    auto out = model->forward(x);

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
        if (a == "--ckpt" && i + 1 < argc)          ckpt_path = argv[++i];
        else if (a == "--registry" && i + 1 < argc) registry_override = argv[++i];
        else if (a == "-h" || a == "--help") {
            std::cout << "usage: probe_v3_model --ckpt <path.pt> "
                         "[--registry cards/registry.json]\n";
            return 0;
        }
    }
    if (ckpt_path.empty()) {
        std::cout << "(no --ckpt) Probing FRESHLY-INITIALIZED V3Model "
                     "with default config\n";
    }

    CardDB db;
    db.loadFromRegistry(findRegistry(registry_override));

    V3ModelConfig cfg;
    bool got_sidecar = !ckpt_path.empty() && loadConfigSidecar(ckpt_path, cfg);
    std::cout << "Probe: " << (ckpt_path.empty() ? "<random-init>" : ckpt_path)
              << "\n";
    std::cout << "Config: " << (got_sidecar ? "sidecar"
                                            : "defaults (no sidecar found)")
              << "  input_dim=" << cfg.input_dim
              << "  trunk_hidden=" << cfg.trunk_hidden
              << "×" << cfg.trunk_blocks
              << "  num_action_slots=" << cfg.num_action_slots << "\n";

    V3Model model(cfg);
    if (!ckpt_path.empty()) {
        torch::load(model, ckpt_path, torch::kCPU);
    }
    model->eval();

    std::cout << "\n## Group 1 — score-only sanity\n";
    probeState("P1 about to win  (7-0) from P1", model, db,
               buildState(7, 0), PlayerId::Player1, -1.0, 1.0, cfg.input_dim);
    probeState("P2 about to win  (0-7) from P1", model, db,
               buildState(0, 7), PlayerId::Player1, -1.0, 1.0, cfg.input_dim);
    probeState("Symmetric        (0-0) from P1", model, db,
               buildState(0, 0), PlayerId::Player1, -1.0, 1.0, cfg.input_dim);

    std::cout << "\n## Group 2 — asymmetric board, both perspectives\n";
    auto p1_dom = buildAsymmetricBoard(PlayerId::Player1);
    probeState("P1 dominant — from P1 perspective", model, db,
               p1_dom, PlayerId::Player1, 0.05, 1.0, cfg.input_dim);
    probeState("P1 dominant — from P2 perspective", model, db,
               p1_dom, PlayerId::Player2, -1.0, -0.05, cfg.input_dim);
    auto p2_dom = buildAsymmetricBoard(PlayerId::Player2);
    probeState("P2 dominant — from P1 perspective", model, db,
               p2_dom, PlayerId::Player1, -1.0, -0.05, cfg.input_dim);
    probeState("P2 dominant — from P2 perspective", model, db,
               p2_dom, PlayerId::Player2, 0.05, 1.0, cfg.input_dim);

    // Variance check — the V2 failure mode signature. If V3 trunk
    // discriminates, these four values should NOT all be identical.
    double v_g1_a = forwardValue(model, db, buildState(7, 0),
                                  PlayerId::Player1, cfg.input_dim);
    double v_g2_p1d_p1 = forwardValue(model, db, p1_dom,
                                       PlayerId::Player1, cfg.input_dim);
    double v_g2_p1d_p2 = forwardValue(model, db, p1_dom,
                                       PlayerId::Player2, cfg.input_dim);
    double v_g2_p2d_p1 = forwardValue(model, db, p2_dom,
                                       PlayerId::Player1, cfg.input_dim);
    double v_g2_p2d_p2 = forwardValue(model, db, p2_dom,
                                       PlayerId::Player2, cfg.input_dim);

    // Are all values within 1e-3 of each other? That's the V2-style
    // constant-trunk signature.
    double vmin = std::min({v_g1_a, v_g2_p1d_p1, v_g2_p1d_p2,
                             v_g2_p2d_p1, v_g2_p2d_p2});
    double vmax = std::max({v_g1_a, v_g2_p1d_p1, v_g2_p1d_p2,
                             v_g2_p2d_p1, v_g2_p2d_p2});
    double spread = vmax - vmin;
    bool variance_ok = spread > 1e-3;

    bool perspective_ok =
        (v_g2_p1d_p1 > 0 && v_g2_p1d_p2 < 0 &&
         v_g2_p2d_p1 < 0 && v_g2_p2d_p2 > 0);

    std::cout << "\nVARIANCE="
              << (variance_ok ? "OK" : "COLLAPSED")
              << "  (spread=" << spread
              << " across diverse states; <1e-3 = constant-trunk collapse)\n";
    std::cout << "PERSPECTIVE_WIRING="
              << (perspective_ok ? "OK" : "BROKEN")
              << "  (p1dom_p1=" << v_g2_p1d_p1
              << " p1dom_p2=" << v_g2_p1d_p2
              << " p2dom_p1=" << v_g2_p2d_p1
              << " p2dom_p2=" << v_g2_p2d_p2 << ")\n";

    std::cout << "\nDone.\n";
    return 0;
}
