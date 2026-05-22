// V3 model — MLP + residual stack on flat state features.
//
// V3 is the corrective architecture after V2 (entity-token transformer +
// mean-pool aggregation) was found to collapse to a constant predictor.
// These tests pin the contract: V3 must (a) produce correct-shape outputs,
// (b) bound value to [-1, +1] via tanh, and (c) discriminate distinct
// inputs at random init.

#include "ml/v3_model.h"

#include <gtest/gtest.h>

#include <torch/torch.h>

using namespace riftbound::ml;

namespace {

// Small config to keep the test fast — production sets input_dim from
// kStateFeatureDim and num_action_slots from the live action_vocab.
V3ModelConfig smallConfig() {
    V3ModelConfig c;
    c.input_dim        = 256;     // small flat input
    c.trunk_hidden     = 64;
    c.trunk_blocks     = 3;
    c.policy_hidden    = 32;
    c.value_hidden     = 32;
    c.num_action_slots = 512;
    return c;
}

}  // namespace

TEST(V3ModelForward, RandomInputProducesCorrectShapes) {
    auto cfg = smallConfig();
    V3Model model(cfg);
    model->eval();

    const int B = 4;
    auto x = torch::randn({B, cfg.input_dim});
    auto out = model->forward(x);

    EXPECT_EQ(out.flat_policy_logits.sizes(),
              torch::IntArrayRef({B, cfg.num_action_slots}));
    EXPECT_EQ(out.value.sizes(), torch::IntArrayRef({B, 1}));

    // tanh bounds.
    auto val_min = out.value.min().item<float>();
    auto val_max = out.value.max().item<float>();
    EXPECT_GE(val_min, -1.0f);
    EXPECT_LE(val_max, +1.0f);
}

TEST(V3ModelForward, TrunkDifferentiatesDistinctInputs) {
    // Regression guard. V2 failed this with its mean-pool aggregation —
    // even at random init, mean-pool of LayerNorm'd tokens drove
    // outputs toward a constant across drastically different inputs.
    // V3 has no aggregation step (input is already fixed-size flat),
    // and no per-layer LayerNorm in the trunk, so distinct inputs MUST
    // produce distinct outputs.
    auto cfg = smallConfig();
    V3Model model(cfg);
    model->eval();

    // Two clearly different inputs.
    auto a = torch::randn({1, cfg.input_dim}) * 0.1f;            // small-magnitude
    auto b = torch::randn({1, cfg.input_dim}) * 0.1f + 5.0f;     // shifted+scaled

    auto out_a = model->forward(a);
    auto out_b = model->forward(b);

    auto val_delta = (out_a.value - out_b.value).abs().item<float>();
    EXPECT_GT(val_delta, 1.0e-3f)
        << "V3 trunk produced near-identical value outputs for very "
           "different inputs (delta=" << val_delta << "). This is the "
           "constant-trunk collapse signature V3 was designed to avoid.";

    auto policy_delta =
        (out_a.flat_policy_logits - out_b.flat_policy_logits)
            .pow(2).sum().sqrt().item<float>();
    EXPECT_GT(policy_delta, 1.0e-2f)
        << "V3 trunk produced near-identical policy logits for very "
           "different inputs (L2 delta=" << policy_delta << ").";
}

TEST(V3ModelForward, SameInputProducesSameOutput) {
    // Determinism sanity — same input → same output in eval mode.
    auto cfg = smallConfig();
    V3Model model(cfg);
    model->eval();

    auto x = torch::randn({1, cfg.input_dim});
    auto out_1 = model->forward(x);
    auto out_2 = model->forward(x);

    EXPECT_TRUE(torch::allclose(out_1.value, out_2.value));
    EXPECT_TRUE(torch::allclose(out_1.flat_policy_logits,
                                 out_2.flat_policy_logits));
}

TEST(V3ModelForward, ParameterCountFitsHeadroom) {
    // smallConfig produces ~50K params. Production (input_dim=4623,
    // hidden=512, blocks=6) produces ~8M params — well under V2's 30M.
    auto cfg = smallConfig();
    V3Model model(cfg);
    int64_t total = 0;
    for (const auto& p : model->parameters()) {
        total += p.numel();
    }
    EXPECT_GT(total, 10'000);
    EXPECT_LT(total, 500'000);
}

TEST(V3ModelForward, TrainableViaSingleStep) {
    // End-to-end gradient flow sanity. After one Adam step on a synthetic
    // policy target, the loss should decrease (or at least change).
    auto cfg = smallConfig();
    V3Model model(cfg);
    model->train();

    const int B = 8;
    auto x = torch::randn({B, cfg.input_dim});
    auto target = torch::zeros({B}, torch::TensorOptions().dtype(torch::kLong));

    torch::optim::Adam opt(model->parameters(), torch::optim::AdamOptions(1e-3));

    auto out0 = model->forward(x);
    auto loss0 = torch::nn::functional::cross_entropy(out0.flat_policy_logits, target);
    auto loss0_val = loss0.item<float>();

    opt.zero_grad();
    loss0.backward();
    opt.step();

    auto out1 = model->forward(x);
    auto loss1 = torch::nn::functional::cross_entropy(out1.flat_policy_logits, target);
    auto loss1_val = loss1.item<float>();

    EXPECT_LT(loss1_val, loss0_val + 1e-4f)
        << "Loss should not increase after a single Adam step on the "
           "same batch. Got loss0=" << loss0_val
           << " loss1=" << loss1_val;
}
