#include "config/config.h"
#include "config/schema.h"

#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace riftbound::config {

namespace fs = std::filesystem;
using nlohmann::json;
using nlohmann::json_schema::json_validator;

namespace {

// ─── JSON loader ────────────────────────────────────────────────────────────

json loadJsonFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw ConfigError("Could not open file: " + path);
    }
    json j;
    try {
        in >> j;
    } catch (const json::parse_error& e) {
        throw ConfigError("JSON parse error in " + path + ": " + e.what());
    }
    return j;
}

// ─── Path resolution ────────────────────────────────────────────────────────

// If `p` is already absolute, return it normalized. If relative AND
// root_dir is set, resolve against root_dir. If relative AND root_dir
// is unset, throw — the config-file contract requires absolute paths
// when no root_dir is declared.
std::string resolvePath(const std::string& p,
                         const std::optional<std::string>& root_dir,
                         const std::string& field_name) {
    if (p.empty()) {
        throw ConfigError("Empty path in field: " + field_name);
    }
    fs::path path(p);
    if (path.is_absolute()) {
        return fs::weakly_canonical(path).string();
    }
    if (!root_dir.has_value()) {
        throw ConfigError(
            "Path is relative but `root_dir` is not set in this config: "
            + field_name + " = \"" + p + "\". Either set `root_dir` at "
            "the top level of the config, or change this path to absolute.");
    }
    fs::path resolved = fs::path(*root_dir) / path;
    return fs::weakly_canonical(resolved).string();
}

void requireFileExists(const std::string& abs_path,
                        const std::string& field_name) {
    if (!fs::exists(abs_path)) {
        throw ConfigError("File does not exist: " + field_name + " = "
                          + abs_path);
    }
    if (!fs::is_regular_file(abs_path)) {
        throw ConfigError("Path exists but is not a regular file: "
                          + field_name + " = " + abs_path);
    }
}

void requireDirExistsOrCreatable(const std::string& abs_path,
                                   const std::string& field_name) {
    if (fs::exists(abs_path)) {
        if (!fs::is_directory(abs_path)) {
            throw ConfigError("Path exists but is not a directory: "
                              + field_name + " = " + abs_path);
        }
        return;
    }
    // Parent must exist; we'll create the leaf at runtime if needed.
    fs::path parent = fs::path(abs_path).parent_path();
    if (!parent.empty() && !fs::exists(parent)) {
        throw ConfigError("Parent directory does not exist for: "
                          + field_name + " = " + abs_path);
    }
}

// ─── Agent parsing ──────────────────────────────────────────────────────────

AgentKind parseAgentKind(const std::string& s) {
    if (s == "random") return AgentKind::Random;
    if (s == "mcts")   return AgentKind::Mcts;
    if (s == "ismcts") return AgentKind::Ismcts;
    if (s == "model")  return AgentKind::Model;
    throw ConfigError("Unknown agent kind: " + s);
}

AgentSpec parseAgent(const json& j,
                      const std::optional<std::string>& root_dir,
                      const std::string& field_path) {
    AgentSpec spec;
    spec.kind = parseAgentKind(j.at("kind").get<std::string>());
    if (j.contains("sims")) spec.sims = j.at("sims").get<int>();
    if (j.contains("seed")) spec.seed = j.at("seed").get<int>();
    if (j.contains("path")) {
        std::string p = j.at("path").get<std::string>();
        std::string resolved = resolvePath(p, root_dir, field_path + ".path");
        requireFileExists(resolved, field_path + ".path");
        spec.path = resolved;
    }
    return spec;
}

// ─── Deck-legend cross-check ────────────────────────────────────────────────

// Read the `legend.name` field from a deck JSON (the canonical
// "what legend does this deck pilot" field). Returns lowercased
// name. Throws if the deck doesn't have the field or it's malformed.
std::string readDeckLegendName(const std::string& deck_path) {
    json deck;
    try {
        deck = loadJsonFile(deck_path);
    } catch (const std::exception& e) {
        throw ConfigError(std::string("Could not load deck for legend "
                                       "consistency check: ") + e.what());
    }
    if (!deck.contains("legend") || !deck["legend"].is_object()) {
        throw ConfigError("Deck " + deck_path + " is missing the "
                          "`legend` object (cannot validate legend "
                          "consistency).");
    }
    if (!deck["legend"].contains("name") ||
        !deck["legend"]["name"].is_string()) {
        throw ConfigError("Deck " + deck_path + " is missing "
                          "`legend.name` (cannot validate legend "
                          "consistency).");
    }
    std::string name = deck["legend"]["name"].get<std::string>();
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return name;
}

// ─── Sub-block parsers ──────────────────────────────────────────────────────

OptimizerConfig parseOptimizer(const json& j) {
    OptimizerConfig o;
    o.lr           = j.at("lr").get<double>();
    if (j.contains("weight_decay")) o.weight_decay = j.at("weight_decay").get<double>();
    if (j.contains("grad_clip"))    o.grad_clip    = j.at("grad_clip").get<double>();
    if (j.contains("lr_schedule")) {
        const auto& s = j.at("lr_schedule");
        o.lr_schedule_kind = s.at("kind").get<std::string>();
        if (s.contains("step_size")) o.lr_step_size = s.at("step_size").get<int>();
        if (s.contains("gamma"))     o.lr_gamma     = s.at("gamma").get<double>();
        if (s.contains("t_max"))     o.lr_t_max     = s.at("t_max").get<int>();
        if (s.contains("eta_min"))   o.lr_eta_min   = s.at("eta_min").get<double>();
    }
    return o;
}

MctsConfig parseMcts(const json& j) {
    MctsConfig m;
    m.uct_c = j.at("uct_c").get<double>();
    if (j.contains("dirichlet_alpha"))    m.dirichlet_alpha    = j.at("dirichlet_alpha").get<double>();
    if (j.contains("dirichlet_fraction")) m.dirichlet_fraction = j.at("dirichlet_fraction").get<double>();
    if (j.contains("temperature"))        m.temperature        = j.at("temperature").get<double>();
    if (j.contains("temperature_drop_at_decision"))
        m.temperature_drop_at_decision = j.at("temperature_drop_at_decision").get<int>();
    return m;
}

EvalConfig parseEval(const json& j) {
    EvalConfig e;
    if (j.contains("every_iter"))    e.every_iter    = j.at("every_iter").get<int>();
    if (j.contains("games"))         e.games         = j.at("games").get<int>();
    if (j.contains("baseline"))      e.baseline      = j.at("baseline").get<std::string>();
    if (j.contains("baseline_sims")) e.baseline_sims = j.at("baseline_sims").get<int>();
    if (j.contains("promote_threshold"))
        e.promote_threshold = j.at("promote_threshold").get<double>();
    return e;
}

ModelConfig parseModel(const json& j) {
    ModelConfig m;
    auto take_int = [&](const char* key, int& out) {
        if (j.contains(key)) out = j.at(key).get<int>();
    };
    auto take_double = [&](const char* key, double& out) {
        if (j.contains(key)) out = j.at(key).get<double>();
    };
    auto take_string = [&](const char* key, std::string& out) {
        if (j.contains(key)) out = j.at(key).get<std::string>();
    };
    // Phase 6v — model architecture selector. V2 stays default for
    // back-compat with existing configs (which don't set this field).
    take_string("kind", m.kind);
    // V2 transformer fields. V3 ignores these but parsing them
    // unconditionally keeps the schema permissive across both kinds.
    take_int("card_embed_dim",   m.card_embed_dim);
    take_int("zone_embed_dim",   m.zone_embed_dim);
    take_int("domain_embed_dim", m.domain_embed_dim);
    take_int("stance_embed_dim", m.stance_embed_dim);
    take_int("stat_embed_dim",   m.stat_embed_dim);
    // V3 configs don't need to specify these (defaults are fine). We
    // only require them for V2 to keep the legacy contract.
    if (m.kind == "v2") {
        m.d_model        = j.at("d_model").get<int>();
        m.encoder_layers = j.at("encoder_layers").get<int>();
        m.encoder_heads  = j.at("encoder_heads").get<int>();
    } else {
        take_int("d_model",        m.d_model);
        take_int("encoder_layers", m.encoder_layers);
        take_int("encoder_heads",  m.encoder_heads);
    }
    take_int("encoder_ffn_dim",  m.encoder_ffn_dim);
    take_double("encoder_dropout", m.encoder_dropout);
    take_int("gnn_layers",       m.gnn_layers);
    if (j.contains("gnn_edge_types")) {
        m.gnn_edge_types.clear();
        for (const auto& v : j.at("gnn_edge_types")) {
            m.gnn_edge_types.push_back(v.get<std::string>());
        }
    }
    take_string("activation", m.activation);
    take_string("init",       m.init);
    // V3-specific fields.
    take_int("trunk_hidden",  m.trunk_hidden);
    take_int("trunk_blocks",  m.trunk_blocks);
    take_int("policy_hidden", m.policy_hidden);
    take_int("value_hidden",  m.value_hidden);
    return m;
}

TrainingConfig parseTraining(const json& j,
                              const std::optional<std::string>& root_dir) {
    TrainingConfig t;
    t.legend               = j.at("legend").get<std::string>();
    t.iterations           = j.at("iterations").get<int>();
    t.games_per_iter       = j.at("games_per_iter").get<int>();
    t.train_steps_per_iter = j.at("train_steps_per_iter").get<int>();
    t.batch_size           = j.at("batch_size").get<int>();
    t.replay_capacity      = j.at("replay_capacity").get<int>();
    if (j.contains("mirror_ratio")) t.mirror_ratio = j.at("mirror_ratio").get<double>();
    t.checkpoint_dir = resolvePath(j.at("checkpoint_dir").get<std::string>(),
                                    root_dir, "training.checkpoint_dir");
    requireDirExistsOrCreatable(t.checkpoint_dir, "training.checkpoint_dir");
    if (j.contains("checkpoint_every_iter")) t.checkpoint_every_iter = j.at("checkpoint_every_iter").get<int>();
    if (j.contains("keep_last_n"))           t.keep_last_n           = j.at("keep_last_n").get<int>();
    if (j.contains("num_self_play_workers"))
        t.num_self_play_workers = j.at("num_self_play_workers").get<int>();
    if (j.contains("max_decisions_per_game"))
        t.max_decisions_per_game = j.at("max_decisions_per_game").get<int>();
    if (j.contains("bootstrap_iters"))
        t.bootstrap_iters = j.at("bootstrap_iters").get<int>();
    if (j.contains("bootstrap_mcts_sims"))
        t.bootstrap_mcts_sims = j.at("bootstrap_mcts_sims").get<int>();
    if (j.contains("bootstrap_iters_min"))
        t.bootstrap_iters_min = j.at("bootstrap_iters_min").get<int>();
    if (j.contains("bootstrap_value_loss_threshold"))
        t.bootstrap_value_loss_threshold =
            j.at("bootstrap_value_loss_threshold").get<double>();
    if (j.contains("log_jsonl")) {
        std::string p = j.at("log_jsonl").get<std::string>();
        t.log_jsonl = resolvePath(p, root_dir, "training.log_jsonl");
        // Don't require existence — the file is being WRITTEN.
        fs::path parent = fs::path(*t.log_jsonl).parent_path();
        if (!parent.empty() && !fs::exists(parent)) {
            throw ConfigError("Parent directory does not exist for "
                              "training.log_jsonl: " + *t.log_jsonl);
        }
    }
    if (j.contains("resume_checkpoint")) {
        std::string p = j.at("resume_checkpoint").get<std::string>();
        t.resume_checkpoint = resolvePath(p, root_dir, "training.resume_checkpoint");
        requireFileExists(*t.resume_checkpoint, "training.resume_checkpoint");
    }
    if (j.contains("start_iter")) {
        t.start_iter = j.at("start_iter").get<int>();
    }
    t.self_play_agent = parseAgent(j.at("self_play_agent"), root_dir,
                                     "training.self_play_agent");

    // Deck pools — resolve every path, check existence, then verify
    // legend consistency on the `self` pool against training.legend.
    {
        const auto& pools = j.at("deck_pools");
        std::string target_legend = t.legend;
        std::transform(target_legend.begin(), target_legend.end(),
                       target_legend.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        for (const auto& v : pools.at("self")) {
            std::string p = v.get<std::string>();
            std::string resolved = resolvePath(p, root_dir,
                                                "training.deck_pools.self");
            requireFileExists(resolved, "training.deck_pools.self");
            std::string deck_legend = readDeckLegendName(resolved);
            if (deck_legend != target_legend) {
                throw ConfigError(
                    "Legend mismatch in training.deck_pools.self: "
                    "deck " + resolved + " declares legend \"" +
                    deck_legend + "\" but training.legend = \"" +
                    target_legend + "\". Every deck in self_pool must "
                    "match the training legend.");
            }
            t.deck_pools.self.push_back(resolved);
        }
        if (pools.contains("opponent")) {
            for (const auto& v : pools.at("opponent")) {
                std::string p = v.get<std::string>();
                std::string resolved = resolvePath(p, root_dir,
                                                    "training.deck_pools.opponent");
                requireFileExists(resolved, "training.deck_pools.opponent");
                t.deck_pools.opponent.push_back(resolved);
            }
        }
    }

    if (j.contains("devices")) {
        t.devices.clear();
        for (const auto& v : j.at("devices")) {
            t.devices.push_back(v.get<std::string>());
        }
    }
    if (j.contains("train_device")) {
        t.train_device = j.at("train_device").get<std::string>();
    }

    t.optimizer = parseOptimizer(j.at("optimizer"));
    t.mcts      = parseMcts(j.at("mcts"));
    t.model     = parseModel(j.at("model"));
    if (j.contains("eval")) t.eval = parseEval(j.at("eval"));
    return t;
}

PlayBlock parsePlayBlock(const json& j,
                          const std::optional<std::string>& root_dir) {
    PlayBlock pb;
    const auto& p = j.at("play");
    pb.play.games         = p.at("games").get<int>();
    if (p.contains("max_decisions")) pb.play.max_decisions = p.at("max_decisions").get<int>();
    if (p.contains("threads"))       pb.play.threads       = p.at("threads").get<int>();
    if (p.contains("render_html"))   pb.play.render_html   = p.at("render_html").get<bool>();
    if (p.contains("output_dir")) {
        std::string out = p.at("output_dir").get<std::string>();
        pb.play.output_dir = resolvePath(out, root_dir, "play.output_dir");
        requireDirExistsOrCreatable(*pb.play.output_dir, "play.output_dir");
    }

    pb.p1_agent = parseAgent(j.at("agents").at("p1"), root_dir, "agents.p1");
    pb.p2_agent = parseAgent(j.at("agents").at("p2"), root_dir, "agents.p2");

    pb.p1_deck = resolvePath(j.at("decks").at("p1").get<std::string>(),
                              root_dir, "decks.p1");
    pb.p2_deck = resolvePath(j.at("decks").at("p2").get<std::string>(),
                              root_dir, "decks.p2");
    requireFileExists(pb.p1_deck, "decks.p1");
    requireFileExists(pb.p2_deck, "decks.p2");
    return pb;
}

}  // namespace

// ─── Public API ─────────────────────────────────────────────────────────────

Config parseConfigString(const std::string& json_text,
                          const std::string& base_path_for_relative) {
    json j;
    try {
        j = json::parse(json_text);
    } catch (const json::parse_error& e) {
        throw ConfigError(std::string("JSON parse error: ") + e.what());
    }

    // 1. Schema validation.
    json schema_doc;
    try {
        schema_doc = json::parse(kSchemaJson);
    } catch (const json::parse_error& e) {
        // This is a programmer error — the embedded schema is malformed.
        throw ConfigError(std::string("INTERNAL: embedded schema is "
                                       "not valid JSON: ") + e.what());
    }
    json_validator validator;
    try {
        validator.set_root_schema(schema_doc);
    } catch (const std::exception& e) {
        throw ConfigError(std::string("INTERNAL: embedded schema is not "
                                       "valid JSON Schema: ") + e.what());
    }
    try {
        validator.validate(j);
    } catch (const std::exception& e) {
        throw ConfigError(std::string("Schema validation failed: ") + e.what());
    }

    // 2. Post-schema parse + path resolution + cross-file checks.
    Config cfg;
    std::string mode_str = j.at("mode").get<std::string>();
    cfg.mode = (mode_str == "play") ? Mode::Play : Mode::Train;
    if (j.contains("seed")) cfg.seed = j.at("seed").get<int>();

    // root_dir: resolve relative-to-config-file if provided.
    if (j.contains("root_dir")) {
        std::string rd = j.at("root_dir").get<std::string>();
        fs::path p(rd);
        if (!p.is_absolute()) {
            if (base_path_for_relative.empty()) {
                throw ConfigError("`root_dir` is relative but no base path "
                                  "is known (parseConfigString called without "
                                  "a base directory). Pass an absolute path "
                                  "or call parseConfig(file) instead.");
            }
            p = fs::path(base_path_for_relative) / p;
        }
        cfg.root_dir = fs::weakly_canonical(p).string();
    }

    cfg.registry = resolvePath(j.at("registry").get<std::string>(),
                                cfg.root_dir, "registry");
    requireFileExists(cfg.registry, "registry");

    // Mode-discriminated parsing.
    if (cfg.mode == Mode::Play) {
        cfg.play_block = parsePlayBlock(j, cfg.root_dir);
    } else {
        cfg.training = parseTraining(j.at("training"), cfg.root_dir);
    }
    return cfg;
}

Config parseConfig(const std::string& config_path) {
    fs::path path(config_path);
    if (!path.is_absolute()) {
        path = fs::weakly_canonical(fs::current_path() / path);
    }
    if (!fs::exists(path)) {
        throw ConfigError("Config file does not exist: " + path.string());
    }
    std::ifstream in(path);
    if (!in) {
        throw ConfigError("Could not open config file: " + path.string());
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return parseConfigString(ss.str(), path.parent_path().string());
}

}  // namespace riftbound::config
