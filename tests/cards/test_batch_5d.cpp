/// Batch 5d (testing phase): T1 — the prompts batch 3 added say what they ask;
/// M2, M3, M4.

#include <gtest/gtest.h>

#include "card_test_fixture.h"
#include "cards/card_helpers.h"
#include "engine/prompt_description.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace riftbound::test::batch_5d {

constexpr CardDefId kWindWall = 64;
constexpr CardDefId kDredgeUp = 836;        // "Draw 1."
constexpr CardDefId kPremonition = 409;     // "Draw 3."
constexpr CardDefId kPowerNexus = 528;
constexpr CardDefId kGold564 = 564;
constexpr CardDefId kFizzTrickster = 461;
constexpr CardDefId kPunchFirst = 419;

class Batch5dTest : public CardTestFixture {
protected:
    GameObject& o(GameObjectId id) { return state.getObject(id); }
    PlayerState& ps(PlayerId p) { return state.player(p); }
    void fund(PlayerId p, int energy, int power) {
        ps(p).rune_pool.energy += energy;
        ps(p).rune_pool.universal_power += power;
    }
    GameObjectId spellOnChain(PlayerId controller, CardDefId def) {
        const auto src = pushSpellOnChain(controller, def, {});
        state.chain.items.back().status = ChainItemStatus::Finalized;
        o(src).owner = controller;
        return src;
    }
    GameObjectId addToTrash(PlayerId owner, CardDefId def_id) {
        auto id = addToHand(owner, def_id);
        auto& h = ps(owner).hand;
        h.erase(std::remove(h.begin(), h.end(), id), h.end());
        o(id).zone = ZoneType::Trash;
        ps(owner).trash.push_back(id);
        return id;
    }
    GameObjectId addGear(PlayerId owner, CardDefId def_id) {
        auto id = state.createObject();
        auto& g = o(id);
        g.owner = g.controller = owner;
        g.card_type = CardType::Gear;
        g.card_def_id = def_id;
        g.name = card_db.get(def_id).name;
        g.zone = ZoneType::Base;
        g.location = BaseLocation{owner};
        return id;
    }
};

/// An agent that keeps what every decision it is given asks (the engine's
/// Prompt and the description it may be shown), then answers as `inner`.
class Describes : public AgentInterface {
public:
    struct Seen {
        Prompt prompt;
        PromptDescription mine;
        std::vector<Intent> legal;
    };
    std::vector<Seen> seen;
    AgentInterface& inner;
    const CardDB& db;
    Describes(AgentInterface& a, const CardDB& d) : inner(a), db(d) {}
    Intent selectAction(const GameState& s, const std::vector<Intent>& legal) override {
        return inner.selectAction(s, legal);
    }
    Intent decide(const GameState& s, const std::vector<Intent>& legal,
                  const Prompt& prompt) override {
        seen.push_back({prompt, describePrompt(s, prompt, legal, prompt.decider, db), legal});
        return inner.decide(s, legal, prompt);
    }
    const Seen* first(PromptKind k) const {
        for (const auto& x : seen) if (x.prompt.kind == k) return &x;
        return nullptr;
    }
};

class PlaysWhen : public EnginePathAgent {
public:
    GameObjectId card;
    std::function<bool(const GameState&)> when;
    bool played = false;
    PlaysWhen(GameObjectId c, Picker p) : EnginePathAgent(std::move(p)), card(c) {}
    Intent selectAction(const GameState& s, const std::vector<Intent>& legal) override {
        if (!played && (!when || when(s)))
            for (const auto& i : legal)
                if (i.card == card && (i.type == IntentType::PlayReaction ||
                                       i.type == IntentType::PlayCard ||
                                       i.type == IntentType::PlayActionCard)) {
                    played = true;
                    return i;
                }
        return EnginePathAgent::selectAction(s, legal);
    }
};

// ─── T1: batch 3's new prompts (plans/reports/batch-3.md section 9) ─────────

// Card::pickChainItem: Wind Wall asks which spell it counters. The options are
// told apart by the item's place on the chain and its source as the decider
// sees it; the chain item's id (the option's value) is not shown.
TEST_F(Batch5dTest, T1_ACountersQuestionNamesTheChainItemsByPlaceNotById) {
    for (int i = 0; i < 5; ++i) addToDeck(P2, kDredgeUp);
    auto lower = spellOnChain(P2, kDredgeUp);
    auto upper = spellOnChain(P2, kPremonition);
    auto wall = addToHand(P1, kWindWall);
    fund(P1, 5, 5);
    PlaysWhen plays(wall, nullptr);
    plays.when = [](const GameState& s) {
        int n = 0;
        for (const auto& ci : s.chain.items) n += ci.is_spell ? 1 : 0;
        return n >= 2;
    };
    Describes me(plays, card_db);
    withEngine([&](GameEngine& e) { e.testHook_runChain(); }, &me, nullptr, P2);
    const auto* asked = me.first(PromptKind::ChainItem);
    ASSERT_NE(asked, nullptr) << "Wind Wall asks which spell it counters";
    EXPECT_EQ(asked->mine.source_card, CardDefId{64});
    EXPECT_EQ(asked->mine.ability, PromptDescription::Ability::Spell);
    EXPECT_TRUE(asked->mine.at_finalization) << "chosen as it is played (CR 355.9.a.2)";
    std::set<int> places;
    std::set<GameObjectId> sources;
    for (const auto& opt : asked->mine.options) {
        EXPECT_EQ(opt.role, OptionRole::ChainItem);
        EXPECT_EQ(opt.value, -1) << "the item's id is not shown";
        places.insert(opt.chain_position);
        sources.insert(asked->legal[opt.index].chosen_objects.front());
        EXPECT_NE(opt.text.find("the spell"), std::string::npos) << opt.text;
    }
    EXPECT_EQ(places, (std::set<int>{0, 1}));
    EXPECT_EQ(sources, (std::set<GameObjectId>{lower, upper}));
}

// The "[Add] for this payment" prompt (EffectExecutor::addForPayment): Power
// Nexus's "may pay [A][A][A][A]" with two runes and two Gold tokens. Which
// Gold pays is asked, and the question says it is an [Add] for a payment, of
// Power Nexus's ability.
TEST_F(Batch5dTest, T1_AnAddForAPaymentSaysWhoseAbilityItPaysFor) {
    auto nexus = state.createObject();
    {
        auto& b = o(nexus);
        b.card_def_id = kPowerNexus;
        b.card_type = CardType::Battlefield;
        b.name = card_db.get(kPowerNexus).name;
        b.owner = b.controller = P1;
        b.zone = ZoneType::BattlefieldZone;
        state.battlefields[0].card_object_id = nexus;
        state.battlefields[0].controller = P1;
    }
    for (int i = 0; i < 2; ++i) addRune(P1, Domain::Body);
    addGear(P1, kGold564);
    addGear(P1, kGold564);
    EnginePathAgent yes([](const std::vector<Intent>& legal) {
        for (const auto& i : legal) if (i.chosen_value == 1) return i;   // "may": yes
        return legal.front();
    });
    Describes me(yes, card_db);
    fireTriggerThroughEngine(nexus, TriggerType::WhenYouHoldHere, &me);
    const auto* asked = me.first(PromptKind::AddForPayment);
    ASSERT_NE(asked, nullptr) << "which Gold pays is asked";
    EXPECT_EQ(asked->mine.source_card, kPowerNexus) << "the payment is Power Nexus's";
    EXPECT_EQ(asked->mine.ability, PromptDescription::Ability::Triggered);
    ASSERT_EQ(asked->mine.options.size(), 2u);
    for (const auto& opt : asked->mine.options) {
        EXPECT_EQ(opt.role, OptionRole::Action) << "an activation";
        EXPECT_NE(opt.text.find("Gold"), std::string::npos) << opt.text;
    }
    const auto* may = me.first(PromptKind::May);
    ASSERT_NE(may, nullptr);
    EXPECT_EQ(may->mine.source_card, kPowerNexus);
    EXPECT_EQ(may->mine.options.size(), 2u);
    int yes_roles = 0, no_roles = 0;
    for (const auto& opt : may->mine.options) {
        yes_roles += opt.role == OptionRole::Yes;
        no_roles += opt.role == OptionRole::No;
    }
    EXPECT_EQ(yes_roles, 1);
    EXPECT_EQ(no_roles, 1);
}

// The effect-play target prompt (GameEngine::chooseSpellBeforeItsCost): Fizz
// plays Punch First from the trash, and its target is asked before its Power.
// The question is a target, for Punch First (the spell played), not for Fizz.
TEST_F(Batch5dTest, T1_AnEffectPlaysTargetQuestionIsTheSpellsTarget) {
    auto mine = addUnit(P1, kInvalidId, 2, 0);
    for (int i = 0; i < 2; ++i) addRune(P1, Domain::Body);
    auto punch = addToTrash(P1, kPunchFirst);
    auto fizz = addToHand(P1, kFizzTrickster);
    fund(P1, 3, 1);
    EnginePathAgent picks([&](const std::vector<Intent>& legal) {
        for (const auto& i : legal)
            if (!i.chosen_objects.empty() && (i.chosen_objects[0] == mine || i.chosen_objects[0] == punch))
                return i;
        for (const auto& i : legal) if (i.chosen_value == 1) return i;
        return legal.front();
    });
    Describes me(picks, card_db);
    playThroughEngine(fizz, &me, std::nullopt, nullptr, /*fund=*/false);
    const Describes::Seen* target = nullptr;
    for (const auto& x : me.seen)
        if (x.prompt.kind == PromptKind::Target && x.prompt.source == punch) target = &x;
    ASSERT_NE(target, nullptr) << "Punch First's target is asked, as Punch First's";
    EXPECT_EQ(target->mine.source_card, kPunchFirst);
    bool names_mine = false;
    for (const auto& opt : target->mine.options) {
        EXPECT_EQ(opt.role, OptionRole::Object);
        if (target->legal[opt.index].chosen_objects.front() == mine) names_mine = true;
    }
    EXPECT_TRUE(names_mine);
}

}  // namespace riftbound::test::batch_5d
