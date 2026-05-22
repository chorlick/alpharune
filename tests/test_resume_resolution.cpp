// Phase C-1 commit 6 — resumable chain-resolution unit tests.
//
// Validates that:
//   (1) ChainManager::stepResolve correctly drives a Card whose onResolve
//       yields a pending choice via EffectExecutor::requestChoice — the
//       card re-enters at advanced resume_point on the next iteration.
//   (2) state.chain.resuming holds the resolving item across iterations.
//   (3) The agent-chosen Intent is delivered to the Card via
//       executor.takeChoice() and the card-specific apply runs.
//   (4) Single-shot Cards (no resume) still loop exactly once and dispose
//       normally.
//
// Test agents are scripted: ScriptedDiscardAgent always picks the first
// card in hand to discard, FirstChoiceAgent always picks legal[0].

#include "agents/agent_interface.h"
#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <functional>

using namespace riftbound;

namespace {

constexpr CardDefId kLunarBoonId   = 687;  // Reaction: discard 1, draw 2
constexpr CardDefId kAbandonId     = 693;  // Reaction: counter + predict 1

std::string findRegistryPath() {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    if (root) return std::string(root) + "/cards/registry.json";
    return "../cards/registry.json";
}

class FirstChoiceAgent {
public:
    int calls = 0;            // total times queried
    int make_choice_calls = 0; // times queried with a MakeChoice-only set
                              // (i.e. mid-resolve discard / predict / etc.)
    Intent operator()(PlayerId, const std::vector<Intent>& legal) {
        calls++;
        if (!legal.empty() && legal.front().type == IntentType::MakeChoice) {
            make_choice_calls++;
        }
        return legal.front();
    }
};

class ResumeFixture : public ::testing::Test {
protected:
    CardDB card_db;
    EventBus events;
    CardRegistry card_registry;
    GameState state;

    void SetUp() override {
        card_db.loadFromRegistry(findRegistryPath());
        card_registry.loadAll();

        state.mode = ModeOfPlay{};
        state.players[0].id = PlayerId::Player1;
        state.players[1].id = PlayerId::Player2;
        // Two empty battlefields so location math doesn't crash.
        BattlefieldState bf0; bf0.id = 0; state.battlefields.push_back(bf0);
        BattlefieldState bf1; bf1.id = 1; state.battlefields.push_back(bf1);
    }

    // Create a card instance with the matching def, owner, current zone.
    GameObjectId makeCard(PlayerId owner, CardDefId def_id, ZoneType zone) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        const auto& def = card_db.get(def_id);
        obj.card_def_id = def_id;
        obj.owner = owner;
        obj.controller = owner;
        obj.name = def.name;
        obj.card_type = def.card_type;
        obj.super_type = def.super_type;
        obj.keywords = def.keywords;
        obj.domains = def.domains;
        obj.zone = zone;
        if (zone == ZoneType::Hand) state.player(owner).hand.push_back(id);
        else if (zone == ZoneType::MainDeck) state.player(owner).main_deck.push_back(id);
        return id;
    }
};

// ─── (1) The resumable Card actually yields and re-enters ──────────────────

TEST_F(ResumeFixture, LunarBoonYieldsForDiscardAndApplies) {
    // Hand: [Lunar Boon (the resolving spell), a vanilla hand card]
    // After resolution: Lunar Boon → trash, hand card → trash (chosen
    // discard), then drew 2 cards from main_deck. We seed 4 cards in deck
    // so the post-discard `drawCards(2)` actually draws.
    auto boon = makeCard(PlayerId::Player1, kLunarBoonId, ZoneType::Hand);
    auto hand_card = makeCard(PlayerId::Player1, kAbandonId, ZoneType::Hand);
    auto deck_a = makeCard(PlayerId::Player1, kAbandonId, ZoneType::MainDeck);
    auto deck_b = makeCard(PlayerId::Player1, kAbandonId, ZoneType::MainDeck);

    EffectExecutor exec(state, events, card_db);
    ChainManager cm(state, events, card_db);
    cm.setEffectExecutor(&exec);

    // Add Lunar Boon to chain. Mimic what executePlaySpell does in
    // production: remove the spell from the controller's hand (addSpell
    // only updates zone, not the hand vector — that's the engine's job).
    {
        auto& ps = state.player(PlayerId::Player1);
        ps.hand.erase(std::find(ps.hand.begin(), ps.hand.end(), boon));
    }
    cm.addSpell(boon, PlayerId::Player1, /*targets=*/{});

    // Track how many times Lunar Boon's onResolve gets invoked. We do that
    // by wrapping resolve_spell with a counter.
    int on_resolve_calls = 0;
    FirstChoiceAgent agent;

    cm.processFEPR(
        [&](PlayerId p, const std::vector<Intent>& legal) -> Intent {
            return agent(p, legal);
        },
        [&](const ChainItem&) { /* no permanents in this test */ },
        [&](const ChainItem& item) {
            on_resolve_calls++;
            Card* c = card_registry.get(item.card_def_id);
            ASSERT_NE(c, nullptr);
            CardContext ctx{state, events, exec, item.controller, item.source};
            c->onResolve(ctx, item.targets);
        },
        /*gen_closed_actions=*/nullptr
    );

    // ─── Assertions ───
    // 1. onResolve was called TWICE — once for case 0 (request choice), once
    //    for case 1 (consume choice, draw). This is the core resume signal.
    EXPECT_EQ(on_resolve_calls, 2);
    // 2. Agent was queried for the mid-resolve discard choice exactly once.
    //    Total agent calls also include priority-passing queries from
    //    stepExecuteAndPass; we count those separately via make_choice_calls.
    EXPECT_EQ(agent.make_choice_calls, 1);
    // 3. The chosen hand card landed in trash + flag set.
    auto& ps1 = state.player(PlayerId::Player1);
    EXPECT_EQ(std::find(ps1.hand.begin(), ps1.hand.end(), hand_card),
              ps1.hand.end()) << "discarded card should be gone from hand";
    EXPECT_NE(std::find(ps1.trash.begin(), ps1.trash.end(), hand_card),
              ps1.trash.end()) << "discarded card should be in trash";
    EXPECT_TRUE(ps1.has_discarded_this_turn);
    // 4. Drew 2 cards from deck → hand. Lunar Boon is also in trash now.
    EXPECT_NE(std::find(ps1.hand.begin(), ps1.hand.end(), deck_a),
              ps1.hand.end());
    EXPECT_NE(std::find(ps1.hand.begin(), ps1.hand.end(), deck_b),
              ps1.hand.end());
    EXPECT_NE(std::find(ps1.trash.begin(), ps1.trash.end(), boon),
              ps1.trash.end()) << "resolved spell should be in trash";
    // 5. Chain is empty + resuming cleared.
    EXPECT_FALSE(state.chain.exists());
    EXPECT_FALSE(state.chain.resuming.has_value());
    // 6. No leftover pending choice / recorded choice.
    EXPECT_FALSE(exec.hasPendingChoice());
    EXPECT_FALSE(exec.hasRecordedChoice());
}

// ─── (2) state.chain.resuming holds the resolving item across iterations ──

TEST_F(ResumeFixture, ResumingSlotPopulatedDuringResolution) {
    auto boon = makeCard(PlayerId::Player1, kLunarBoonId, ZoneType::Hand);
    auto hand_card = makeCard(PlayerId::Player1, kAbandonId, ZoneType::Hand);
    makeCard(PlayerId::Player1, kAbandonId, ZoneType::MainDeck);

    EffectExecutor exec(state, events, card_db);
    ChainManager cm(state, events, card_db);
    cm.setEffectExecutor(&exec);

    {
        auto& ps = state.player(PlayerId::Player1);
        ps.hand.erase(std::find(ps.hand.begin(), ps.hand.end(), boon));
    }
    cm.addSpell(boon, PlayerId::Player1, /*targets=*/{});

    bool saw_resuming_in_case0 = false;
    int saw_resume_point_in_case1 = -1;

    cm.processFEPR(
        [&](PlayerId, const std::vector<Intent>& legal) -> Intent {
            return legal.front();
        },
        [&](const ChainItem&) {},
        [&](const ChainItem& item) {
            // Capture whether resuming is populated and what its
            // resume_point looks like at this invocation.
            if (state.chain.resuming.has_value()) {
                if (state.chain.resuming->resume_point == 0) {
                    saw_resuming_in_case0 = true;
                }
                if (state.chain.resuming->resume_point == 1 &&
                    saw_resume_point_in_case1 == -1) {
                    saw_resume_point_in_case1 = 1;
                }
            }
            Card* c = card_registry.get(item.card_def_id);
            CardContext ctx{state, events, exec, item.controller, item.source};
            c->onResolve(ctx, item.targets);
        },
        nullptr
    );

    EXPECT_TRUE(saw_resuming_in_case0)
        << "resuming should be populated when onResolve runs at case 0";
    EXPECT_EQ(saw_resume_point_in_case1, 1)
        << "resume_point should have advanced to 1 for the second invocation";
    EXPECT_FALSE(state.chain.resuming.has_value())
        << "resuming must be cleared after resolution completes";
    (void)hand_card;
}

// ─── (3) Single-shot Cards run exactly once (no regression) ────────────────

// A trivial fake Card that resolves in one shot — never publishes a pending
// choice. We assert ChainManager invokes resolve_spell exactly once.
TEST_F(ResumeFixture, SingleShotCardResolvesInOneInvocation) {
    // Push an arbitrary chain item (we won't actually look up a Card object —
    // the resolve_spell lambda decides what "resolution" means).
    ChainItem item;
    item.id = state.chain.allocateId();
    item.status = ChainItemStatus::Finalized;
    item.source = state.createObject();
    state.getObject(item.source).owner = PlayerId::Player1;
    state.getObject(item.source).controller = PlayerId::Player1;
    state.getObject(item.source).card_type = CardType::Spell;
    state.getObject(item.source).zone = ZoneType::Chain;
    item.card_def_id = 1;  // arbitrary
    item.controller = PlayerId::Player1;
    item.is_spell = true;
    state.chain.items.push_back(item);

    EffectExecutor exec(state, events, card_db);
    ChainManager cm(state, events, card_db);
    cm.setEffectExecutor(&exec);

    int resolve_calls = 0;
    cm.processFEPR(
        [&](PlayerId, const std::vector<Intent>& legal) -> Intent {
            return legal.front();
        },
        [&](const ChainItem&) {},
        [&](const ChainItem&) {
            resolve_calls++;
            // intentionally don't requestChoice — single-shot
        },
        nullptr
    );

    EXPECT_EQ(resolve_calls, 1)
        << "single-shot Card must NOT trigger the resume loop";
    EXPECT_FALSE(state.chain.exists());
    EXPECT_FALSE(state.chain.resuming.has_value());
}

}  // namespace
