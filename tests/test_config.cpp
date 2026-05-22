/// @file test_config.cpp
/// Tests for the unified config parser + JSON Schema validator
/// (Phase 6a). Covers:
///   - Schema enforcement (missing required fields, wrong types, bad
///     enum values, mode-discriminated requireds).
///   - Path resolution (absolute kept absolute, relative resolved against
///     root_dir, relative-without-root_dir rejected).
///   - Cross-file checks (deck file existence, legend consistency
///     across self_pool).
///   - Agent kind discrimination (mcts/ismcts require sims, model
///     requires path).

#include "config/config.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using riftbound::config::Config;
using riftbound::config::ConfigError;
using riftbound::config::Mode;
using riftbound::config::parseConfigString;

namespace {

// ─── Temp file fixtures ─────────────────────────────────────────────────────
//
// Tests that exercise file-existence checks need real files on disk.
// We materialize the registry path + deck files + checkpoint dir in
// a tmp tree per test, with deck JSON that matches what the parser
// reads (the `legend.name` field for consistency checks).

class ConfigTempTree : public ::testing::Test {
protected:
    fs::path tmp_root;

    void SetUp() override {
        tmp_root = fs::temp_directory_path()
                 / ("riftbound_config_test_"
                    + std::to_string(::testing::UnitTest::GetInstance()
                                      ->current_test_info()->name()
                                      [0])
                    + "_" + std::to_string(std::rand()));
        fs::create_directories(tmp_root);
        // Materialize a fake registry file (parser only checks existence).
        std::ofstream(tmp_root / "registry.json") << "{}\n";
        fs::create_directories(tmp_root / "checkpoints");
        fs::create_directories(tmp_root / "logs");
        fs::create_directories(tmp_root / "decks");
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmp_root, ec);
    }

    // Write a deck JSON whose `legend.name` field is `legend_name`.
    fs::path writeDeck(const std::string& filename,
                       const std::string& legend_name) {
        fs::path p = tmp_root / "decks" / filename;
        std::ofstream out(p);
        out << R"({"legend":{"name":")" << legend_name << R"("}})";
        return p;
    }
};

// ─── Schema validation failures ─────────────────────────────────────────────

TEST(ConfigSchema, MissingMode_Throws) {
    const std::string j = R"({"registry":"/x"})";
    EXPECT_THROW(parseConfigString(j, "/tmp"), ConfigError);
}

TEST(ConfigSchema, MissingRegistry_Throws) {
    const std::string j = R"({"mode":"play"})";
    EXPECT_THROW(parseConfigString(j, "/tmp"), ConfigError);
}

TEST(ConfigSchema, UnknownMode_Throws) {
    const std::string j = R"({"mode":"banana","registry":"/x"})";
    EXPECT_THROW(parseConfigString(j, "/tmp"), ConfigError);
}

TEST(ConfigSchema, PlayMode_MissingPlayBlock_Throws) {
    const std::string j = R"({
        "mode":"play","registry":"/x",
        "agents":{"p1":{"kind":"random"},"p2":{"kind":"random"}},
        "decks":{"p1":"/x","p2":"/x"}
    })";
    EXPECT_THROW(parseConfigString(j, "/tmp"), ConfigError);
}

TEST(ConfigSchema, PlayMode_MissingAgents_Throws) {
    const std::string j = R"({
        "mode":"play","registry":"/x",
        "play":{"games":1},
        "decks":{"p1":"/x","p2":"/x"}
    })";
    EXPECT_THROW(parseConfigString(j, "/tmp"), ConfigError);
}

TEST(ConfigSchema, TrainMode_MissingTrainingBlock_Throws) {
    const std::string j = R"({"mode":"train","registry":"/x"})";
    EXPECT_THROW(parseConfigString(j, "/tmp"), ConfigError);
}

TEST(ConfigSchema, AgentMcts_RequiresSims) {
    const std::string j = R"({
        "mode":"play","registry":"/x",
        "play":{"games":1},
        "agents":{"p1":{"kind":"mcts"},"p2":{"kind":"random"}},
        "decks":{"p1":"/x","p2":"/x"}
    })";
    EXPECT_THROW(parseConfigString(j, "/tmp"), ConfigError);
}

TEST(ConfigSchema, AgentIsmcts_RequiresSims) {
    const std::string j = R"({
        "mode":"play","registry":"/x",
        "play":{"games":1},
        "agents":{"p1":{"kind":"ismcts"},"p2":{"kind":"random"}},
        "decks":{"p1":"/x","p2":"/x"}
    })";
    EXPECT_THROW(parseConfigString(j, "/tmp"), ConfigError);
}

TEST(ConfigSchema, AgentModel_RequiresPath) {
    const std::string j = R"({
        "mode":"play","registry":"/x",
        "play":{"games":1},
        "agents":{"p1":{"kind":"model"},"p2":{"kind":"random"}},
        "decks":{"p1":"/x","p2":"/x"}
    })";
    EXPECT_THROW(parseConfigString(j, "/tmp"), ConfigError);
}

TEST(ConfigSchema, AdditionalProperties_Rejected) {
    const std::string j = R"({
        "mode":"play","registry":"/x",
        "unknown_top_level_field": 42,
        "play":{"games":1},
        "agents":{"p1":{"kind":"random"},"p2":{"kind":"random"}},
        "decks":{"p1":"/x","p2":"/x"}
    })";
    EXPECT_THROW(parseConfigString(j, "/tmp"), ConfigError);
}

// ─── Path resolution ────────────────────────────────────────────────────────

TEST_F(ConfigTempTree, AbsolutePaths_Accepted) {
    auto p1_deck = writeDeck("d1.json", "jhin");
    auto p2_deck = writeDeck("d2.json", "leblanc");
    std::ostringstream ss;
    ss << R"({
        "mode":"play",
        "registry":")" << (tmp_root / "registry.json").string() << R"(",
        "play":{"games":1},
        "agents":{"p1":{"kind":"random"},"p2":{"kind":"random"}},
        "decks":{"p1":")" << p1_deck.string() << R"(","p2":")" << p2_deck.string() << R"("}
    })";
    Config cfg = parseConfigString(ss.str(), "/nonexistent");  // base shouldn't matter
    EXPECT_EQ(cfg.mode, Mode::Play);
    EXPECT_TRUE(fs::path(cfg.registry).is_absolute());
    EXPECT_TRUE(fs::path(cfg.play_block->p1_deck).is_absolute());
}

TEST_F(ConfigTempTree, RelativePath_WithoutRootDir_Throws) {
    std::ostringstream ss;
    ss << R"({
        "mode":"play",
        "registry":"registry.json",
        "play":{"games":1},
        "agents":{"p1":{"kind":"random"},"p2":{"kind":"random"}},
        "decks":{"p1":"d1.json","p2":"d2.json"}
    })";
    EXPECT_THROW(parseConfigString(ss.str(), tmp_root.string()), ConfigError);
}

TEST_F(ConfigTempTree, RelativePath_WithRootDir_Resolves) {
    writeDeck("d1.json", "jhin");
    writeDeck("d2.json", "leblanc");
    std::ostringstream ss;
    ss << R"({
        "mode":"play",
        "root_dir":")" << tmp_root.string() << R"(",
        "registry":"registry.json",
        "play":{"games":1},
        "agents":{"p1":{"kind":"random"},"p2":{"kind":"random"}},
        "decks":{"p1":"decks/d1.json","p2":"decks/d2.json"}
    })";
    Config cfg = parseConfigString(ss.str(), tmp_root.string());
    EXPECT_TRUE(fs::path(cfg.registry).is_absolute());
    EXPECT_NE(cfg.registry.find("registry.json"), std::string::npos);
    EXPECT_NE(cfg.play_block->p1_deck.find("d1.json"), std::string::npos);
}

TEST_F(ConfigTempTree, MissingDeckFile_Throws) {
    std::ostringstream ss;
    ss << R"({
        "mode":"play",
        "root_dir":")" << tmp_root.string() << R"(",
        "registry":"registry.json",
        "play":{"games":1},
        "agents":{"p1":{"kind":"random"},"p2":{"kind":"random"}},
        "decks":{"p1":"decks/nonexistent.json","p2":"decks/also_missing.json"}
    })";
    EXPECT_THROW(parseConfigString(ss.str(), tmp_root.string()), ConfigError);
}

// ─── Training mode + deck-pool legend consistency ───────────────────────────

TEST_F(ConfigTempTree, TrainMode_LegendConsistency_Pass) {
    writeDeck("jhin_aggro.json", "jhin");
    writeDeck("jhin_control.json", "JHIN");  // case-insensitive
    writeDeck("leblanc.json", "leblanc");
    std::ostringstream ss;
    ss << R"({
        "mode":"train",
        "root_dir":")" << tmp_root.string() << R"(",
        "registry":"registry.json",
        "training":{
            "legend":"jhin",
            "iterations":1,
            "games_per_iter":1,
            "train_steps_per_iter":1,
            "batch_size":1,
            "replay_capacity":1,
            "checkpoint_dir":"checkpoints",
            "self_play_agent":{"kind":"ismcts","sims":10},
            "deck_pools":{
                "self":["decks/jhin_aggro.json","decks/jhin_control.json"],
                "opponent":["decks/leblanc.json"]
            },
            "optimizer":{"lr":0.001},
            "mcts":{"uct_c":1.4},
            "model":{"d_model":64,"encoder_layers":1,"encoder_heads":1}
        }
    })";
    Config cfg = parseConfigString(ss.str(), tmp_root.string());
    EXPECT_EQ(cfg.mode, Mode::Train);
    EXPECT_EQ(cfg.training->legend, "jhin");
    EXPECT_EQ(cfg.training->deck_pools.self.size(), 2u);
    EXPECT_EQ(cfg.training->deck_pools.opponent.size(), 1u);
}

TEST_F(ConfigTempTree, TrainMode_LegendInconsistency_InSelfPool_Throws) {
    writeDeck("jhin.json", "jhin");
    writeDeck("imposter.json", "leblanc");  // wrong legend in self pool
    std::ostringstream ss;
    ss << R"({
        "mode":"train",
        "root_dir":")" << tmp_root.string() << R"(",
        "registry":"registry.json",
        "training":{
            "legend":"jhin",
            "iterations":1,
            "games_per_iter":1,
            "train_steps_per_iter":1,
            "batch_size":1,
            "replay_capacity":1,
            "checkpoint_dir":"checkpoints",
            "self_play_agent":{"kind":"ismcts","sims":10},
            "deck_pools":{
                "self":["decks/jhin.json","decks/imposter.json"]
            },
            "optimizer":{"lr":0.001},
            "mcts":{"uct_c":1.4},
            "model":{"d_model":64,"encoder_layers":1,"encoder_heads":1}
        }
    })";
    EXPECT_THROW(parseConfigString(ss.str(), tmp_root.string()), ConfigError);
}

TEST_F(ConfigTempTree, TrainMode_OpponentPoolCanBeAnyLegend) {
    writeDeck("jhin.json", "jhin");
    writeDeck("mixed_lb.json", "leblanc");
    writeDeck("mixed_mf.json", "missfortune");
    std::ostringstream ss;
    ss << R"({
        "mode":"train",
        "root_dir":")" << tmp_root.string() << R"(",
        "registry":"registry.json",
        "training":{
            "legend":"jhin",
            "iterations":1,
            "games_per_iter":1,
            "train_steps_per_iter":1,
            "batch_size":1,
            "replay_capacity":1,
            "checkpoint_dir":"checkpoints",
            "self_play_agent":{"kind":"ismcts","sims":10},
            "deck_pools":{
                "self":["decks/jhin.json"],
                "opponent":["decks/mixed_lb.json","decks/mixed_mf.json"]
            },
            "optimizer":{"lr":0.001},
            "mcts":{"uct_c":1.4},
            "model":{"d_model":64,"encoder_layers":1,"encoder_heads":1}
        }
    })";
    // Should succeed — opponent pool isn't legend-checked.
    EXPECT_NO_THROW(parseConfigString(ss.str(), tmp_root.string()));
}

TEST_F(ConfigTempTree, TrainMode_DeckMissingLegendField_Throws) {
    // Write a deck WITHOUT the legend.name field — parser should
    // reject because it can't validate consistency.
    fs::path p = tmp_root / "decks" / "bad.json";
    std::ofstream(p) << "{}";  // empty deck
    std::ostringstream ss;
    ss << R"({
        "mode":"train",
        "root_dir":")" << tmp_root.string() << R"(",
        "registry":"registry.json",
        "training":{
            "legend":"jhin",
            "iterations":1,
            "games_per_iter":1,
            "train_steps_per_iter":1,
            "batch_size":1,
            "replay_capacity":1,
            "checkpoint_dir":"checkpoints",
            "self_play_agent":{"kind":"ismcts","sims":10},
            "deck_pools":{"self":["decks/bad.json"]},
            "optimizer":{"lr":0.001},
            "mcts":{"uct_c":1.4},
            "model":{"d_model":64,"encoder_layers":1,"encoder_heads":1}
        }
    })";
    EXPECT_THROW(parseConfigString(ss.str(), tmp_root.string()), ConfigError);
}

// ─── Agent path validation ──────────────────────────────────────────────────

TEST_F(ConfigTempTree, ModelAgentPath_FileMustExist) {
    writeDeck("d1.json", "jhin");
    writeDeck("d2.json", "leblanc");
    std::ostringstream ss;
    ss << R"({
        "mode":"play",
        "root_dir":")" << tmp_root.string() << R"(",
        "registry":"registry.json",
        "play":{"games":1},
        "agents":{
            "p1":{"kind":"model","path":"nonexistent.pt"},
            "p2":{"kind":"random"}
        },
        "decks":{"p1":"decks/d1.json","p2":"decks/d2.json"}
    })";
    EXPECT_THROW(parseConfigString(ss.str(), tmp_root.string()), ConfigError);
}

TEST_F(ConfigTempTree, ModelAgentPath_ExistingFile_Accepted) {
    writeDeck("d1.json", "jhin");
    writeDeck("d2.json", "leblanc");
    fs::path model = tmp_root / "fake_model.pt";
    std::ofstream(model) << "fake bytes";
    std::ostringstream ss;
    ss << R"({
        "mode":"play",
        "root_dir":")" << tmp_root.string() << R"(",
        "registry":"registry.json",
        "play":{"games":1},
        "agents":{
            "p1":{"kind":"model","path":"fake_model.pt"},
            "p2":{"kind":"random"}
        },
        "decks":{"p1":"decks/d1.json","p2":"decks/d2.json"}
    })";
    EXPECT_NO_THROW(parseConfigString(ss.str(), tmp_root.string()));
}

// ─── Defaults wired correctly ───────────────────────────────────────────────

TEST_F(ConfigTempTree, TrainMode_DefaultMirrorRatio_IsHalf) {
    writeDeck("jhin.json", "jhin");
    std::ostringstream ss;
    ss << R"({
        "mode":"train",
        "root_dir":")" << tmp_root.string() << R"(",
        "registry":"registry.json",
        "training":{
            "legend":"jhin",
            "iterations":1,
            "games_per_iter":1,
            "train_steps_per_iter":1,
            "batch_size":1,
            "replay_capacity":1,
            "checkpoint_dir":"checkpoints",
            "self_play_agent":{"kind":"ismcts","sims":10},
            "deck_pools":{"self":["decks/jhin.json"]},
            "optimizer":{"lr":0.001},
            "mcts":{"uct_c":1.4},
            "model":{"d_model":64,"encoder_layers":1,"encoder_heads":1}
        }
    })";
    Config cfg = parseConfigString(ss.str(), tmp_root.string());
    EXPECT_DOUBLE_EQ(cfg.training->mirror_ratio, 0.5);
    EXPECT_DOUBLE_EQ(cfg.training->mcts.dirichlet_alpha, 0.3);
    EXPECT_DOUBLE_EQ(cfg.training->mcts.dirichlet_fraction, 0.25);
    EXPECT_EQ(cfg.training->mcts.temperature_drop_at_decision, 30);
    EXPECT_DOUBLE_EQ(cfg.training->optimizer.weight_decay, 0.0);
}

}  // namespace
