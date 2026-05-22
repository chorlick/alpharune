#include "ml/v2_model.h"

#include <cmath>
#include <string>

namespace riftbound::ml {

namespace {

// ── Per-token input width ──────────────────────────────────────────────────
// concat( card | zone | domain | stance | perspective | stats_proj | spatial )
// → projected to d_model via token_proj_.
int tokenInputDim(const V2ModelConfig& c) {
    return c.card_embed_dim + c.zone_embed_dim + c.domain_embed_dim +
           c.stance_embed_dim + c.perspective_dim + c.stats_proj_dim +
           c.spatial_dim;
}

// Constant for masked-logit floor. Avoids -inf (NaN-safe under amp).
constexpr float kMaskedLogit = -1.0e9f;

}  // namespace

V2ModelImpl::V2ModelImpl(const V2ModelConfig& cfg)
    : cfg_(cfg),
      card_embed_(torch::nn::EmbeddingOptions(cfg.num_card_defs, cfg.card_embed_dim)),
      zone_embed_(torch::nn::EmbeddingOptions(cfg.num_zones,     cfg.zone_embed_dim)),
      domain_embed_(torch::nn::EmbeddingOptions(cfg.num_domains, cfg.domain_embed_dim)),
      stance_embed_(torch::nn::EmbeddingOptions(cfg.num_stances, cfg.stance_embed_dim)),
      perspective_embed_(torch::nn::EmbeddingOptions(2,          cfg.perspective_dim)),
      // +1 for the "off-board" slot at index 0 (kSpatialNodeOffBoard == -1
      // gets shifted to 0; real nodes occupy 1..num_spatial).
      spatial_embed_(torch::nn::EmbeddingOptions(cfg.num_spatial + 1, cfg.spatial_dim)),
      stats_proj_(torch::nn::LinearOptions(cfg.stats_dim, cfg.stats_proj_dim)),
      token_ln_(torch::nn::LayerNormOptions({tokenInputDim(cfg)})),
      token_proj_(torch::nn::LinearOptions(tokenInputDim(cfg), cfg.d_model)),
      action_type_head_(torch::nn::LinearOptions(cfg.d_model, cfg.num_action_types)),
      source_query_proj_(torch::nn::LinearOptions(cfg.d_model, cfg.d_model)),
      target_query_proj_(torch::nn::LinearOptions(cfg.d_model, cfg.d_model)),
      dest_query_proj_(torch::nn::LinearOptions(cfg.d_model,   cfg.d_model)),
      dest_node_key_proj_(torch::nn::LinearOptions(cfg.d_model, cfg.d_model)),
      flat_policy_head_(torch::nn::LinearOptions(cfg.d_model, cfg.num_action_slots)),
      value_hidden_(torch::nn::LinearOptions(cfg.d_model, cfg.value_hidden)),
      value_out_(torch::nn::LinearOptions(cfg.value_hidden, 1)) {
    register_module("card_embed",        card_embed_);
    register_module("zone_embed",        zone_embed_);
    register_module("domain_embed",      domain_embed_);
    register_module("stance_embed",      stance_embed_);
    register_module("perspective_embed", perspective_embed_);
    register_module("spatial_embed",     spatial_embed_);
    register_module("stats_proj",        stats_proj_);
    register_module("token_ln",          token_ln_);
    register_module("token_proj",        token_proj_);

    // ── Sinusoidal positional encoding for chain items (buffer) ──
    chain_pos_enc_ = buildChainPositionalEncoding();
    register_buffer("chain_pos_enc", chain_pos_enc_);

    // ── Transformer encoder ──
    auto encoder_layer_opts = torch::nn::TransformerEncoderLayerOptions(
                                  cfg.d_model, cfg.encoder_heads)
                                  .dim_feedforward(cfg.encoder_ffn_dim)
                                  .dropout(cfg.encoder_dropout)
                                  .activation(torch::kReLU);
    auto encoder_opts = torch::nn::TransformerEncoderOptions(
                            torch::nn::TransformerEncoderLayer(encoder_layer_opts),
                            cfg.encoder_layers);
    encoder_ = torch::nn::TransformerEncoder(encoder_opts);
    register_module("encoder", encoder_);

    // ── Spatial fusion stack ──
    spatial_attns_.reserve(cfg.spatial_attn_layers);
    spatial_lns_pre_.reserve(cfg.spatial_attn_layers);
    spatial_ffns1_.reserve(cfg.spatial_attn_layers);
    spatial_ffns2_.reserve(cfg.spatial_attn_layers);
    spatial_lns_post_.reserve(cfg.spatial_attn_layers);
    for (int l = 0; l < cfg.spatial_attn_layers; ++l) {
        auto attn = torch::nn::MultiheadAttention(
            torch::nn::MultiheadAttentionOptions(cfg.d_model, cfg.spatial_attn_heads)
                .dropout(cfg.encoder_dropout));
        auto ln_pre = torch::nn::LayerNorm(
            torch::nn::LayerNormOptions({cfg.d_model}));
        auto ffn1 = torch::nn::Linear(
            torch::nn::LinearOptions(cfg.d_model, cfg.encoder_ffn_dim));
        auto ffn2 = torch::nn::Linear(
            torch::nn::LinearOptions(cfg.encoder_ffn_dim, cfg.d_model));
        auto ln_post = torch::nn::LayerNorm(
            torch::nn::LayerNormOptions({cfg.d_model}));

        register_module("spatial_attn_"     + std::to_string(l), attn);
        register_module("spatial_ln_pre_"   + std::to_string(l), ln_pre);
        register_module("spatial_ffn1_"     + std::to_string(l), ffn1);
        register_module("spatial_ffn2_"     + std::to_string(l), ffn2);
        register_module("spatial_ln_post_"  + std::to_string(l), ln_post);

        spatial_attns_.push_back(attn);
        spatial_lns_pre_.push_back(ln_pre);
        spatial_ffns1_.push_back(ffn1);
        spatial_ffns2_.push_back(ffn2);
        spatial_lns_post_.push_back(ln_post);
    }

    // ── Edge bias matrix (learnable; initialized to zero) ──
    edge_bias_ = register_parameter(
        "edge_bias",
        torch::zeros({cfg.num_edge_types, cfg.num_spatial, cfg.num_spatial}));

    // ── Pointer heads + value head ──
    register_module("action_type_head",   action_type_head_);
    register_module("source_query_proj",  source_query_proj_);
    register_module("target_query_proj",  target_query_proj_);
    register_module("dest_query_proj",    dest_query_proj_);
    register_module("dest_node_key_proj", dest_node_key_proj_);
    register_module("flat_policy_head",   flat_policy_head_);
    register_module("value_hidden",       value_hidden_);
    register_module("value_out",          value_out_);
}

torch::Tensor V2ModelImpl::buildChainPositionalEncoding() {
    // Standard sinusoidal positional encoding ("Attention is All You Need").
    // Shape: (max_chain_index, d_model). Even dims sin, odd dims cos.
    auto pe = torch::zeros({cfg_.max_chain_index, cfg_.d_model});
    auto position = torch::arange(0, cfg_.max_chain_index,
                                  torch::TensorOptions().dtype(torch::kFloat))
                        .unsqueeze(1);  // (max_chain, 1)
    auto div_term = torch::exp(
        torch::arange(0, cfg_.d_model, 2, torch::TensorOptions().dtype(torch::kFloat)) *
        (-std::log(10000.0) / cfg_.d_model));  // (d_model/2,)
    auto sinv = torch::sin(position * div_term);
    auto cosv = torch::cos(position * div_term);
    pe.index({torch::indexing::Slice(),
              torch::indexing::Slice(0, cfg_.d_model, 2)}) = sinv;
    // Cos goes into odd dims. If d_model is odd, the last odd index might
    // be out of range; d_model=512 is even so this is safe by default.
    pe.index({torch::indexing::Slice(),
              torch::indexing::Slice(1, cfg_.d_model, 2)}) = cosv;
    return pe;
}

V2ModelImpl::Output V2ModelImpl::forward(
    torch::Tensor card_ids,
    torch::Tensor zone_ids,
    torch::Tensor domain_ids,
    torch::Tensor stance_ids,
    torch::Tensor stats,
    torch::Tensor is_perspective,
    torch::Tensor chain_index,
    torch::Tensor spatial_node,
    torch::Tensor token_mask) {

    const auto B = card_ids.size(0);
    const auto N = card_ids.size(1);
    const auto device = card_ids.device();

    // ── 1. Embedding lookups ──
    // Clamp categorical inputs into valid ranges to avoid out-of-bounds
    // lookups on synthetic / corrupted data. The token_mask still hides
    // padded entries downstream.
    auto card_e = card_embed_->forward(
        card_ids.clamp(0, cfg_.num_card_defs - 1));               // (B, N, card_embed_dim)
    auto zone_e = zone_embed_->forward(
        zone_ids.clamp(0, cfg_.num_zones - 1));                   // (B, N, zone_embed_dim)
    auto dom_e  = domain_embed_->forward(
        domain_ids.clamp(0, cfg_.num_domains - 1));               // (B, N, domain_embed_dim)
    auto sta_e  = stance_embed_->forward(
        stance_ids.clamp(0, cfg_.num_stances - 1));               // (B, N, stance_embed_dim)
    auto per_e  = perspective_embed_->forward(
        is_perspective.clamp(0, 1));                              // (B, N, perspective_dim)

    // Spatial node embedding — shift -1 → 0, then map real nodes to 1..N.
    auto spatial_shifted = (spatial_node + 1).clamp(0, cfg_.num_spatial);
    auto spa_e = spatial_embed_->forward(spatial_shifted);        // (B, N, spatial_dim)

    // Stats projection (raw integer stats → small MLP slice).
    auto stats_proj = stats_proj_->forward(stats);                // (B, N, stats_proj_dim)

    // ── 2. Token assembly ──
    auto tokens = torch::cat({card_e, zone_e, dom_e, sta_e, per_e,
                              stats_proj, spa_e}, /*dim=*/-1);    // (B, N, token_in_dim)
    tokens = token_ln_->forward(tokens);
    tokens = token_proj_->forward(tokens);                        // (B, N, d_model)

    // ── 3. Add sinusoidal positional encoding to chain items ──
    // chain_index >= 0 → fetch chain_pos_enc_[chain_index], else add 0.
    auto chain_clamped = chain_index.clamp(0, cfg_.max_chain_index - 1);
    auto chain_pe = chain_pos_enc_.to(device)
                        .index_select(0, chain_clamped.view(-1))
                        .view({B, N, cfg_.d_model});
    auto chain_active = (chain_index >= 0)
                            .to(torch::kFloat)
                            .unsqueeze(-1);                       // (B, N, 1)
    tokens = tokens + chain_pe * chain_active;

    // ── 4. Transformer encoder ──
    // Default TransformerEncoder expects (S, B, E); transpose around it.
    auto src = tokens.transpose(0, 1);                            // (N, B, d_model)
    // key_padding_mask: True where padded (i.e., token_mask == false).
    auto key_padding_mask = ~token_mask;                          // (B, N) bool
    auto enc = encoder_->forward(src, /*src_mask=*/{}, key_padding_mask);
    auto encoder_out = enc.transpose(0, 1);                       // (B, N, d_model)

    // ── 5. Spatial fusion ──
    // Mean-pool tokens by spatial_node into (B, num_spatial, d_model).
    // valid_spatial[b, n] := token_mask[b, n] && spatial_node[b, n] >= 0
    auto valid_spatial = token_mask & (spatial_node >= 0);        // (B, N) bool
    // One-hot assignment matrix, dim = num_spatial + 1, then drop col 0.
    auto shifted_for_assign =
        ((spatial_node + 1) *
         valid_spatial.to(torch::kLong)).clamp(0, cfg_.num_spatial);     // (B, N) long
    auto one_hot = torch::nn::functional::one_hot(
        shifted_for_assign, cfg_.num_spatial + 1);                       // (B, N, num_spatial+1)
    one_hot = one_hot.slice(/*dim=*/2, /*start=*/1,
                            /*end=*/cfg_.num_spatial + 1);               // (B, N, num_spatial)
    auto assign = one_hot.to(torch::kFloat).transpose(1, 2);             // (B, num_spatial, N)
    auto counts = assign.sum(/*dim=*/2, /*keepdim=*/true).clamp_min(1.0f); // (B, num_spatial, 1)
    auto node_features = torch::matmul(assign, encoder_out) / counts;    // (B, num_spatial, d_model)

    // Sum edge biases across edge types → (num_spatial, num_spatial)
    // attn_mask added to attention logits.
    auto edge_bias_summed = edge_bias_.sum(/*dim=*/0);                   // (num_spatial, num_spatial)

    // Spatial attn layers — pre-LN, residual MHA, residual FFN.
    auto x = node_features;
    for (int l = 0; l < cfg_.spatial_attn_layers; ++l) {
        // PyTorch MultiheadAttention default: (target_seq_len, batch, embed).
        auto h = spatial_lns_pre_[l]->forward(x).transpose(0, 1);        // (S, B, d)
        auto [attn_out, attn_weights] = spatial_attns_[l]->forward(
            h, h, h,
            /*key_padding_mask=*/torch::Tensor{},
            /*need_weights=*/false,
            /*attn_mask=*/edge_bias_summed);
        x = x + attn_out.transpose(0, 1);                                // (B, S, d) residual
        // FFN with post-LN residual.
        auto h2 = spatial_lns_post_[l]->forward(x);
        auto ffn = spatial_ffns2_[l]->forward(
                       torch::relu(spatial_ffns1_[l]->forward(h2)));
        x = x + ffn;
    }
    auto spatial_features = x;                                            // (B, num_spatial, d_model)

    // ── 6. Masked mean-pool encoder output for pointer-head queries ──
    auto mask_f = token_mask.to(torch::kFloat).unsqueeze(-1);            // (B, N, 1)
    auto pooled = (encoder_out * mask_f).sum(/*dim=*/1) /
                  mask_f.sum(/*dim=*/1).clamp_min(1.0f);                  // (B, d_model)

    // ── 7. Action-type head + flat policy head ──
    auto action_type_logits = action_type_head_->forward(pooled);         // (B, num_action_types)
    auto flat_policy_logits = flat_policy_head_->forward(pooled);         // (B, num_action_slots)

    // ── 8. Pointer heads ──
    // source: dot-product (B, d_model) against (B, N, d_model).
    auto src_q = source_query_proj_->forward(pooled).unsqueeze(-1);       // (B, d_model, 1)
    auto src_logits = torch::bmm(encoder_out, src_q).squeeze(-1);         // (B, N)
    auto neg_inf = torch::full_like(src_logits, kMaskedLogit);
    src_logits = torch::where(token_mask, src_logits, neg_inf);

    // target: same pattern with a separate query projection.
    auto tgt_q = target_query_proj_->forward(pooled).unsqueeze(-1);       // (B, d_model, 1)
    auto tgt_logits = torch::bmm(encoder_out, tgt_q).squeeze(-1);         // (B, N)
    tgt_logits = torch::where(token_mask, tgt_logits, neg_inf);

    // dest_node: dot-product against projected spatial features.
    auto dest_q = dest_query_proj_->forward(pooled).unsqueeze(-1);        // (B, d_model, 1)
    auto dest_k = dest_node_key_proj_->forward(spatial_features);         // (B, num_spatial, d_model)
    auto dest_logits = torch::bmm(dest_k, dest_q).squeeze(-1);            // (B, num_spatial)

    // ── 9. Value head ──
    auto v = torch::relu(value_hidden_->forward(pooled));
    v = value_out_->forward(v);
    auto value = torch::tanh(v);                                           // (B, 1)

    return Output{
        /*.action_type_logits =*/ action_type_logits,
        /*.source_logits      =*/ src_logits,
        /*.target_logits      =*/ tgt_logits,
        /*.dest_node_logits   =*/ dest_logits,
        /*.flat_policy_logits =*/ flat_policy_logits,
        /*.value              =*/ value,
    };
}

}  // namespace riftbound::ml
