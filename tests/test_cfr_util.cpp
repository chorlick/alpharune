/// Tests for ml/cfr_util: info-set ids and regret matching.

#include <gtest/gtest.h>

#include "ml/cfr_util.h"

#include <numeric>
#include <vector>

using namespace riftbound::ml;

TEST(CfrUtil, InfoSetIdIsDeterministicAndPerPlayer) {
    const std::vector<float> feats = {0.0f, 1.0f, 2.5f, -3.0f};
    EXPECT_EQ(computeInfoSetId(feats, 0), computeInfoSetId(feats, 0));
    EXPECT_NE(computeInfoSetId(feats, 0), computeInfoSetId(feats, 1));
}

TEST(CfrUtil, InfoSetIdSeesEveryFeatureAndItsPosition) {
    const std::vector<float> a = {1.0f, 0.0f, 0.0f};
    const std::vector<float> b = {0.0f, 1.0f, 0.0f};
    const std::vector<float> c = {1.0f, 0.0f, 0.5f};
    EXPECT_NE(computeInfoSetId(a, 0), computeInfoSetId(b, 0));
    EXPECT_NE(computeInfoSetId(a, 0), computeInfoSetId(c, 0));
    EXPECT_NE(computeInfoSetId(a, 0), computeInfoSetId({1.0f, 0.0f}, 0));
}

// 1.13 — the same board with a different question is a different info set.
TEST(CfrUtil, DecisionIsPartOfTheInfoSetId) {
    const uint64_t board = computeInfoSetId({1.0f, 2.0f}, 0);
    const uint64_t modes   = mixDecisionIntoInfoSetId(board, 0, {10, 11, 12});
    const uint64_t targets = mixDecisionIntoInfoSetId(board, 0, {40, 41});
    EXPECT_NE(modes, targets);
    EXPECT_EQ(modes, mixDecisionIntoInfoSetId(board, 0, {10, 11, 12}));
    // Who decides matters even with nothing offered to this player.
    EXPECT_NE(mixDecisionIntoInfoSetId(board, 0, {}),
              mixDecisionIntoInfoSetId(board, 1, {}));
    // A list is not confused with a longer one sharing its prefix.
    EXPECT_NE(mixDecisionIntoInfoSetId(board, 0, {10}),
              mixDecisionIntoInfoSetId(board, 0, {10, 0}));
}

TEST(CfrUtil, RegretMatchingNormalizesPositiveRegrets) {
    const auto p = regretMatching({3.0, -5.0, 1.0, 0.0});
    ASSERT_EQ(p.size(), 4u);
    EXPECT_DOUBLE_EQ(p[0], 0.75);
    EXPECT_DOUBLE_EQ(p[1], 0.0);
    EXPECT_DOUBLE_EQ(p[2], 0.25);
    EXPECT_DOUBLE_EQ(p[3], 0.0);
    EXPECT_DOUBLE_EQ(std::accumulate(p.begin(), p.end(), 0.0), 1.0);
}

TEST(CfrUtil, RegretMatchingIsUniformWithoutPositiveRegret) {
    for (const auto& regrets : {std::vector<double>{-1.0, -2.0, 0.0, -0.5},
                                std::vector<double>{0.0, 0.0, 0.0, 0.0}}) {
        const auto p = regretMatching(regrets);
        ASSERT_EQ(p.size(), 4u);
        for (double v : p) EXPECT_DOUBLE_EQ(v, 0.25);
    }
}

TEST(CfrUtil, RegretMatchingHandlesEmptyAndReusesTheOutputBuffer) {
    EXPECT_TRUE(regretMatching({}).empty());
    std::vector<double> out = {9.0, 9.0, 9.0, 9.0, 9.0};
    regretMatching({2.0, 2.0}, out);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_DOUBLE_EQ(out[0], 0.5);
    EXPECT_DOUBLE_EQ(out[1], 0.5);
}
