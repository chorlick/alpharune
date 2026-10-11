/// Pins the constants the token layout shares with the rest of the engine.
/// Nothing in the engine library includes token_layout.h (the trainer does),
/// so this TU is what compiles its static_asserts on every engine build.

#include <gtest/gtest.h>

#include "ml/feature_extractor.h"
#include "ml/token_layout.h"
#include "openspiel/action_vocab.h"

using namespace riftbound;

TEST(TokenLayout, CardIdSpacesAgree) {
    EXPECT_EQ(ml::kCardEmbeddingRows, ml::kCardVocabSize + 1);
    EXPECT_EQ(openspiel::kNumCardDefIds, ml::kCardVocabSize);
}
