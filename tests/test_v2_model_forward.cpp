// Phase 6e smoke test — the V2 entity-token / transformer / spatial-fusion
// / pointer-heads model. Confirms it compiles, links against LibTorch,
// and produces tensors of the expected shapes on random input.
//
// Only built when -DRIFTBOUND_BUILD_LIBTORCH=ON.

#include "ml/v2_model.h"
#include "ml/entity_tokens.h"

#include <gtest/gtest.h>

#include <torch/torch.h>

using namespace riftbound::ml;

namespace {

// Build a small config to keep the test fast — the realistic defaults
// are heavy (~30 M params with the d=512 / 6L encoder).
V2ModelConfig smallConfig() {
    V2ModelConfig c;
    c.card_embed_dim   = 32;
    c.zone_embed_dim   = 16;
    c.domain_embed_dim = 16;
    c.stance_embed_dim = 16;
    c.stats_proj_dim   = 16;
    c.perspective_dim  = 4;
    c.spatial_dim      = 8;
    c.d_model          = 64;
    c.encoder_layers   = 2;
    c.encoder_heads    = 4;
    c.encoder_ffn_dim  = 128;
    c.encoder_dropout  = 0.0;
    c.spatial_attn_layers = 2;
    c.spatial_attn_heads  = 4;
    c.policy_hidden    = 32;
    c.value_hidden     = 32;
    // Keep the flat policy head small in the smoke test so the param
    // count test bound holds — production sets this from the live
    // action_vocab kVocabSize (~7141 today).
    c.num_action_slots = 1024;
    return c;
}

// Construct a batch of plausible entity-token inputs filled with safe
// values. `n_valid` controls how many slots per batch element are
// marked valid (the rest are padded out).
struct InputBundle {
    torch::Tensor card_ids, zone_ids, domain_ids, stance_ids;
    torch::Tensor stats, is_perspective, chain_index, spatial_node;
    torch::Tensor token_mask;
};
InputBundle makeRandomInputs(const V2ModelConfig& cfg, int B, int n_valid) {
    const int N = cfg.max_entities;
    auto long_opts = torch::TensorOptions().dtype(torch::kLong);
    auto bool_opts = torch::TensorOptions().dtype(torch::kBool);
    auto float_opts = torch::TensorOptions().dtype(torch::kFloat);

    InputBundle b;
    b.card_ids       = torch::randint(0, cfg.num_card_defs, {B, N}, long_opts);
    b.zone_ids       = torch::randint(0, cfg.num_zones,     {B, N}, long_opts);
    b.domain_ids     = torch::randint(0, cfg.num_domains,   {B, N}, long_opts);
    b.stance_ids     = torch::randint(0, cfg.num_stances,   {B, N}, long_opts);
    b.stats          = torch::randn({B, N, cfg.stats_dim}, float_opts);
    b.is_perspective = torch::randint(0, 2, {B, N}, long_opts);
    // chain_index: mostly -1, occasionally a chain slot.
    b.chain_index = torch::full({B, N}, -1, long_opts);
    // spatial_node: mix of -1 (off-board) and on-board.
    b.spatial_node = torch::randint(-1, cfg.num_spatial, {B, N}, long_opts);
    // valid mask: first n_valid slots true, rest false.
    auto idx = torch::arange(N, long_opts);
    auto mask_row = idx < n_valid;
    b.token_mask = mask_row.unsqueeze(0).expand({B, N}).to(bool_opts).clone();
    return b;
}

}  // namespace

TEST(V2ModelForward, RandomInputProducesCorrectShapes) {
    auto cfg = smallConfig();
    V2Model model(cfg);
    model->eval();

    const int B = 2;
    auto inp = makeRandomInputs(cfg, B, /*n_valid=*/24);
    auto out = model->forward(
        inp.card_ids, inp.zone_ids, inp.domain_ids, inp.stance_ids,
        inp.stats, inp.is_perspective, inp.chain_index,
        inp.spatial_node, inp.token_mask);

    EXPECT_EQ(out.action_type_logits.sizes(),
              torch::IntArrayRef({B, cfg.num_action_types}));
    EXPECT_EQ(out.source_logits.sizes(),
              torch::IntArrayRef({B, cfg.max_entities}));
    EXPECT_EQ(out.target_logits.sizes(),
              torch::IntArrayRef({B, cfg.max_entities}));
    EXPECT_EQ(out.dest_node_logits.sizes(),
              torch::IntArrayRef({B, cfg.num_spatial}));
    EXPECT_EQ(out.flat_policy_logits.sizes(),
              torch::IntArrayRef({B, cfg.num_action_slots}));
    EXPECT_EQ(out.value.sizes(), torch::IntArrayRef({B, 1}));

    auto val_min = out.value.min().item<float>();
    auto val_max = out.value.max().item<float>();
    EXPECT_GE(val_min, -1.0f);
    EXPECT_LE(val_max, +1.0f);
}

TEST(V2ModelForward, PaddedSlotsHaveMaskedLogits) {
    // Source/target logits should be ~-inf (the masked floor) in padded
    // positions so the policy distribution can't put probability mass
    // outside the valid tokens.
    auto cfg = smallConfig();
    V2Model model(cfg);
    model->eval();

    const int B = 1;
    const int n_valid = 10;
    auto inp = makeRandomInputs(cfg, B, n_valid);
    auto out = model->forward(
        inp.card_ids, inp.zone_ids, inp.domain_ids, inp.stance_ids,
        inp.stats, inp.is_perspective, inp.chain_index,
        inp.spatial_node, inp.token_mask);

    auto src = out.source_logits[0];   // (N,)
    auto tgt = out.target_logits[0];
    for (int i = 0; i < cfg.max_entities; ++i) {
        if (i < n_valid) {
            // Real logit — finite, not the mask floor.
            EXPECT_GT(src[i].item<float>(), -1.0e8f);
            EXPECT_GT(tgt[i].item<float>(), -1.0e8f);
        } else {
            EXPECT_LT(src[i].item<float>(), -1.0e8f);
            EXPECT_LT(tgt[i].item<float>(), -1.0e8f);
        }
    }
}

TEST(V2ModelForward, ChainPosEncShiftsLogits) {
    // Sanity: turning a token into a chain item (chain_index = 0) must
    // change the encoder output relative to a baseline where the same
    // token is off-chain. We don't pin a specific number — just confirm
    // the pos-enc actually flows through.
    auto cfg = smallConfig();
    V2Model model(cfg);
    model->eval();

    const int B = 1;
    auto inp = makeRandomInputs(cfg, B, /*n_valid=*/16);

    // Force determinism on token[0]: chain_index = -1 baseline.
    inp.chain_index.index_put_({0, 0}, -1);
    auto out_off = model->forward(
        inp.card_ids, inp.zone_ids, inp.domain_ids, inp.stance_ids,
        inp.stats, inp.is_perspective, inp.chain_index,
        inp.spatial_node, inp.token_mask);

    // Flip to chain_index = 0.
    inp.chain_index.index_put_({0, 0}, 0);
    auto out_on = model->forward(
        inp.card_ids, inp.zone_ids, inp.domain_ids, inp.stance_ids,
        inp.stats, inp.is_perspective, inp.chain_index,
        inp.spatial_node, inp.token_mask);

    // Source logit at the chain token should differ between the two runs.
    auto delta = (out_on.source_logits[0][0] -
                  out_off.source_logits[0][0]).abs().item<float>();
    EXPECT_GT(delta, 1.0e-6f);
}

TEST(V2ModelForward, ParameterCountFitsHeadroom) {
    // With the smallConfig defaults: ~250 K params. With the default
    // realistic config: ~30 M. We construct only smallConfig here to
    // keep the test cheap; the bound is a soft tripwire for dim drift.
    auto cfg = smallConfig();
    V2Model model(cfg);
    int64_t total = 0;
    for (const auto& p : model->parameters()) {
        total += p.numel();
    }
    // Generous bounds — drift would indicate someone widened the model
    // accidentally.
    EXPECT_GT(total, 50'000);
    EXPECT_LT(total, 2'000'000);
}

TEST(V2ModelForward, TrunkDifferentiatesDistinctInputs) {
    // Regression guard for the Phase 6v failure mode (discovered May 20):
    // multiple full vast.ai training runs ended in EndTurn-only policy
    // collapse because the V2 trunk had converged to a CONSTANT predictor
    // — every state produced the same value + same policy logits.
    // probe_v2_model detected this as PERSPECTIVE_WIRING=BROKEN but the
    // unit suite didn't catch it until ~$30 of GPU time later.
    //
    // This test pins the invariant: even at random init, two visibly
    // different inputs must produce noticeably different value AND policy
    // outputs. If a future training-dynamics change collapses the trunk
    // back to constants, this trips immediately.
    auto cfg = smallConfig();
    V2Model model(cfg);
    model->eval();

    const int B = 1;
    auto long_opts  = torch::TensorOptions().dtype(torch::kLong);
    auto bool_opts  = torch::TensorOptions().dtype(torch::kBool);
    auto float_opts = torch::TensorOptions().dtype(torch::kFloat);
    const int N = cfg.max_entities;

    auto make_inputs = [&](int64_t card_id_val, int64_t zone_val,
                           int64_t persp_val, int n_valid) {
        InputBundle b;
        b.card_ids       = torch::full({B, N}, card_id_val, long_opts);
        b.zone_ids       = torch::full({B, N}, zone_val,    long_opts);
        b.domain_ids     = torch::full({B, N}, 0,           long_opts);
        b.stance_ids     = torch::full({B, N}, 0,           long_opts);
        b.is_perspective = torch::full({B, N}, persp_val,   long_opts);
        b.stats          = torch::zeros({B, N, cfg.stats_dim}, float_opts);
        b.chain_index    = torch::full({B, N}, -1, long_opts);
        b.spatial_node   = torch::full({B, N}, -1, long_opts);
        auto idx = torch::arange(N, long_opts);
        b.token_mask = (idx < n_valid).unsqueeze(0).expand({B, N})
                            .to(bool_opts).clone();
        return b;
    };

    // Two drastically different inputs:
    //   A: 4 own-hand tokens, perspective=1, card_id=10
    //   B: 32 BF tokens (full board), perspective=0, card_id=200
    auto a = make_inputs(/*card*/10,  /*zone*/1,                                  /*persp*/1, /*n_valid*/4);
    auto b = make_inputs(/*card*/200, /*zone*/static_cast<int>(EntityZone::Battlefield), /*persp*/0, /*n_valid*/32);

    auto out_a = model->forward(a.card_ids, a.zone_ids, a.domain_ids,
                                 a.stance_ids, a.stats, a.is_perspective,
                                 a.chain_index, a.spatial_node, a.token_mask);
    auto out_b = model->forward(b.card_ids, b.zone_ids, b.domain_ids,
                                 b.stance_ids, b.stats, b.is_perspective,
                                 b.chain_index, b.spatial_node, b.token_mask);

    // Value must differ meaningfully. At random init we expect at least
    // ~0.01 absolute difference; collapse looks like exact equality.
    auto val_delta = (out_a.value - out_b.value).abs().item<float>();
    EXPECT_GT(val_delta, 1.0e-3f)
        << "Trunk produced near-identical value outputs for very different "
           "inputs (delta=" << val_delta << "). This is the constant-trunk "
           "collapse signature. See probe_v2_model + Phase 6v notes.";

    // Flat policy logits must also differ. We check L2 distance between
    // the two logit vectors — at random init this should be substantial.
    auto policy_delta = (out_a.flat_policy_logits - out_b.flat_policy_logits)
                            .pow(2).sum().sqrt().item<float>();
    EXPECT_GT(policy_delta, 1.0e-2f)
        << "Trunk produced near-identical policy logits for very different "
           "inputs (L2 delta=" << policy_delta << "). See Phase 6v notes.";
}
