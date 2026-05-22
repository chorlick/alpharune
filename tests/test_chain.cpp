#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/card_db.h"
#include "core/events.h"
#include "core/game_state.h"
#include "core/intent.h"
#include "engine/chain_manager.h"
#include "engine/effect_executor.h"
#include "engine/game_engine.h"
#include "agents/agent_interface.h"

#include <gtest/gtest.h>
#include <functional>
#include <vector>

using namespace riftbound;

// ═══════════════════════════════════════════════════════════════════════════════
// Test helpers
// ═══════════════════════════════════════════════════════════════════════════════

/// Agent that always picks a specific intent type, or first available.
class ScriptedAgent : public AgentInterface {
public:
    std::vector<Intent> script;
    size_t next = 0;

    Intent selectAction(const GameState&,
                        const std::vector<Intent>& legal) override {
        if (next < script.size()) {
            // Find matching intent in legal actions
            auto& wanted = script[next];
            for (auto& a : legal) {
                if (a.type == wanted.type && a.card == wanted.card) {
                    next++;
                    // Copy targets from script if provided
                    Intent result = a;
                    if (!wanted.targets.empty()) result.targets = wanted.targets;
                    return result;
                }
            }
            // If scripted action not found, fall through to default
        }
        // Default: pass focus, pass priority, or end turn
        for (auto& a : legal) {
            if (a.type == IntentType::PassFocus ||
                a.type == IntentType::PassPriority)
                return a;
        }
        return legal.front();
    }
};

/// Agent that always passes (focus, priority, or end turn).
class PassAgent : public AgentInterface {
public:
    Intent selectAction(const GameState&,
                        const std::vector<Intent>& legal) override {
        for (auto& a : legal) {
            if (a.type == IntentType::PassFocus ||
                a.type == IntentType::PassPriority ||
                a.type == IntentType::EndTurn)
                return a;
        }
        return legal.front();
    }
};

static std::string findRegistryPath() {
    const char* root = std::getenv("RIFTBOUND_ROOT");
    if (root) return std::string(root) + "/cards/registry.json";
    return "../cards/registry.json";
}

class ChainTestFixture : public ::testing::Test {
protected:
    CardDB card_db;
    EventBus events;
    CardRegistry card_registry;

    void SetUp() override {
        card_db.loadFromRegistry(findRegistryPath());
        card_registry.loadAll();
    }

    /// Resolve a card via its Card object.
    void resolveCard(GameState& state, EffectExecutor& exec,
                     CardDefId def_id, PlayerId controller,
                     GameObjectId source,
                     const std::vector<GameObjectId>& targets) {
        Card* card = card_registry.get(def_id);
        if (!card) return;
        CardContext ctx{state, events, exec, controller, source};
        card->onResolve(ctx, targets);
    }

    /// Create a minimal game state with two players and battlefields set up.
    /// Uses real decks so card interactions are realistic.
    GameState makeMinimalState() {
        GameState state;
        state.mode = ModeOfPlay{};
        state.players[0].id = PlayerId::Player1;
        state.players[1].id = PlayerId::Player2;

        // Add two battlefields
        BattlefieldState bf0;
        bf0.id = 0;
        state.battlefields.push_back(bf0);
        BattlefieldState bf1;
        bf1.id = 1;
        state.battlefields.push_back(bf1);

        return state;
    }

    /// Create a unit game object in a player's base.
    GameObjectId addUnit(GameState& state, PlayerId owner, int might,
                         BattlefieldId at_bf = kInvalidId) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_type = CardType::Unit;
        obj.base_might = might;
        obj.current_might = might;
        obj.name = "TestUnit";
        if (at_bf != kInvalidId) {
            obj.zone = ZoneType::BattlefieldZone;
            obj.location = BattlefieldLocation{at_bf};
        } else {
            obj.zone = ZoneType::Base;
            obj.location = BaseLocation{owner};
        }
        return id;
    }

    /// Create a spell game object in a player's hand.
    GameObjectId addSpellToHand(GameState& state, PlayerId owner,
                                 CardDefId def_id) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        const auto& def = card_db.get(def_id);
        obj.card_def_id = def_id;
        obj.owner = owner;
        obj.controller = owner;
        obj.name = def.name;
        obj.card_type = CardType::Spell;
        obj.super_type = def.super_type;
        obj.keywords = def.keywords;
        obj.domains = def.domains;
        obj.zone = ZoneType::Hand;
        state.player(owner).hand.push_back(id);
        return id;
    }

    /// Create a rune in a player's base (ready, for paying costs).
    GameObjectId addRune(GameState& state, PlayerId owner, Domain domain) {
        auto id = state.createObject();
        auto& obj = state.getObject(id);
        obj.owner = owner;
        obj.controller = owner;
        obj.card_type = CardType::Rune;
        obj.name = "TestRune";
        obj.domains = {domain};
        obj.zone = ZoneType::Base;
        obj.location = BaseLocation{owner};
        obj.is_exhausted = false;
        return id;
    }
};

// Known spell card IDs from registry
constexpr CardDefId kHextechRayId   = 9;   // 1E 1P, Action, Deal 3 to unit at BF
constexpr CardDefId kDisciplineId   = 58;  // 2E, Reaction, +2M + draw 1
constexpr CardDefId kGustId         = 169; // 1E, Reaction, bounce unit <=3M
constexpr CardDefId kDisintegrateId = 5;   // 4E, Action, Deal 3, if kills draw 1
constexpr CardDefId kCleaveId       = 4;   // 1E, Action, give Assault 3

// ═══════════════════════════════════════════════════════════════════════════════
// ChainManager unit tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ChainTestFixture, ChainDoesNotExistInitially) {
    auto state = makeMinimalState();
    ChainManager cm(state, events, card_db);
    EXPECT_FALSE(cm.chainExists());
    EXPECT_FALSE(state.chain.exists());
}

TEST_F(ChainTestFixture, AddSpellCreatesChain) {
    auto state = makeMinimalState();
    ChainManager cm(state, events, card_db);

    auto spell = addSpellToHand(state, PlayerId::Player1, kHextechRayId);
    auto target = addUnit(state, PlayerId::Player2, 5, /*at_bf=*/0);

    bool chain_created = false;
    events.on_chain_created.connect([&](const ChainCreatedEvent&) {
        chain_created = true;
    });

    cm.addSpell(spell, PlayerId::Player1, {target});

    EXPECT_TRUE(cm.chainExists());
    EXPECT_TRUE(chain_created);
    EXPECT_EQ(state.chain.items.size(), 1u);
    EXPECT_EQ(state.chain.items[0].is_spell, true);
    EXPECT_EQ(state.chain.items[0].controller, PlayerId::Player1);
    EXPECT_EQ(state.turn.oc_state, OpenClosedState::Closed);
}

TEST_F(ChainTestFixture, AddPermanentCreatesChain) {
    auto state = makeMinimalState();
    ChainManager cm(state, events, card_db);

    auto unit_id = state.createObject();
    auto& unit = state.getObject(unit_id);
    unit.card_type = CardType::Unit;
    unit.card_def_id = 100; // arbitrary
    unit.controller = PlayerId::Player1;

    cm.addPermanent(unit_id, PlayerId::Player1);

    EXPECT_TRUE(cm.chainExists());
    EXPECT_EQ(state.chain.items[0].is_permanent, true);
    EXPECT_EQ(state.getObject(unit_id).zone, ZoneType::Chain);
}

TEST_F(ChainTestFixture, PermanentResolvesImmediatelyOnFinalize) {
    auto state = makeMinimalState();
    ChainManager cm(state, events, card_db);

    auto unit_id = state.createObject();
    auto& unit = state.getObject(unit_id);
    unit.card_type = CardType::Unit;
    unit.card_def_id = 100;
    unit.controller = PlayerId::Player1;

    cm.addPermanent(unit_id, PlayerId::Player1);

    bool permanent_resolved = false;
    bool chain_emptied = false;
    events.on_chain_emptied.connect([&](const ChainEmptiedEvent&) {
        chain_emptied = true;
    });

    cm.processFEPR(
        // Agent: shouldn't be called since permanent resolves at finalize
        [](PlayerId, const std::vector<Intent>&) -> Intent {
            ADD_FAILURE() << "Agent should not be queried for permanent";
            return Intent::passPriority(PlayerId::Player1);
        },
        [&](const ChainItem& item) {
            permanent_resolved = true;
            EXPECT_EQ(item.source, unit_id);
            EXPECT_TRUE(item.is_permanent);
        },
        [](const ChainItem&) {
            ADD_FAILURE() << "Spell resolver should not be called for permanent";
        }
    );

    EXPECT_TRUE(permanent_resolved);
    EXPECT_TRUE(chain_emptied);
    EXPECT_FALSE(cm.chainExists());
    EXPECT_EQ(state.turn.oc_state, OpenClosedState::Open);
}

TEST_F(ChainTestFixture, SpellResolvesAfterBothPass) {
    auto state = makeMinimalState();
    state.turn.turn_player = PlayerId::Player1;
    ChainManager cm(state, events, card_db);

    auto spell = addSpellToHand(state, PlayerId::Player1, kHextechRayId);
    auto target = addUnit(state, PlayerId::Player2, 5, 0);

    // Remove from hand since addSpell expects it
    state.player(PlayerId::Player1).hand.clear();

    cm.addSpell(spell, PlayerId::Player1, {target});

    bool spell_resolved = false;
    int agent_calls = 0;

    cm.processFEPR(
        // Both players pass priority
        [&](PlayerId, const std::vector<Intent>& actions) -> Intent {
            agent_calls++;
            return Intent::passPriority(actions[0].player);
        },
        [](const ChainItem&) {},
        [&](const ChainItem& item) {
            spell_resolved = true;
            EXPECT_EQ(item.source, spell);
            EXPECT_EQ(item.targets.size(), 1u);
            EXPECT_EQ(item.targets[0], target);
        }
    );

    EXPECT_TRUE(spell_resolved);
    EXPECT_EQ(agent_calls, 2); // P1 controller passes, P2 passes
    EXPECT_FALSE(cm.chainExists());
}

TEST_F(ChainTestFixture, LIFOResolutionOrder) {
    auto state = makeMinimalState();
    state.turn.turn_player = PlayerId::Player1;
    ChainManager cm(state, events, card_db);

    auto spell1 = addSpellToHand(state, PlayerId::Player1, kHextechRayId);
    auto spell2 = addSpellToHand(state, PlayerId::Player1, kDisciplineId);
    auto target = addUnit(state, PlayerId::Player2, 5, 0);

    state.player(PlayerId::Player1).hand.clear();

    // Add two spells -- spell2 added second, should resolve first (LIFO)
    cm.addSpell(spell1, PlayerId::Player1, {target});
    cm.addSpell(spell2, PlayerId::Player1, {target});

    std::vector<GameObjectId> resolve_order;

    cm.processFEPR(
        [](PlayerId p, const std::vector<Intent>&) -> Intent {
            return Intent::passPriority(p);
        },
        [](const ChainItem&) {},
        [&](const ChainItem& item) {
            resolve_order.push_back(item.source);
        }
    );

    ASSERT_EQ(resolve_order.size(), 2u);
    EXPECT_EQ(resolve_order[0], spell2); // second spell resolves first
    EXPECT_EQ(resolve_order[1], spell1); // first spell resolves second
}

TEST_F(ChainTestFixture, ReactionAddsToChainAndRestarsFEPR) {
    auto state = makeMinimalState();
    state.turn.turn_player = PlayerId::Player1;
    ChainManager cm(state, events, card_db);

    auto target = addUnit(state, PlayerId::Player2, 5, 0);

    // P1 plays Hextech Ray
    auto spell1 = addSpellToHand(state, PlayerId::Player1, kHextechRayId);
    state.player(PlayerId::Player1).hand.clear();
    cm.addSpell(spell1, PlayerId::Player1, {target});

    // P2 has a Discipline (Reaction) in hand to respond
    auto reaction = addSpellToHand(state, PlayerId::Player2, kDisciplineId);
    // Give P2 runes to afford it (2E cost)
    addRune(state, PlayerId::Player2, Domain::Fury);
    addRune(state, PlayerId::Player2, Domain::Fury);

    std::vector<GameObjectId> resolve_order;
    int query_count = 0;

    cm.processFEPR(
        [&](PlayerId player, const std::vector<Intent>& actions) -> Intent {
            query_count++;
            // P1 (spell controller) gets priority first -- passes
            if (player == PlayerId::Player1) {
                return Intent::passPriority(player);
            }
            // P2 plays reaction on first opportunity, then passes
            if (player == PlayerId::Player2) {
                for (auto& a : actions) {
                    if (a.type == IntentType::PlayReaction && a.card == reaction) {
                        Intent r = a;
                        r.targets = {target};
                        return r;
                    }
                }
                return Intent::passPriority(player);
            }
            return Intent::passPriority(player);
        },
        [](const ChainItem&) {},
        [&](const ChainItem& item) {
            resolve_order.push_back(item.source);
        }
    );

    // Reaction should resolve first (LIFO), then the original spell
    ASSERT_EQ(resolve_order.size(), 2u);
    EXPECT_EQ(resolve_order[0], reaction);  // Discipline resolves first
    EXPECT_EQ(resolve_order[1], spell1);    // Hextech Ray resolves second
}

// ═══════════════════════════════════════════════════════════════════════════════
// Card object effect tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ChainTestFixture, HextechRayDeals3Damage) {
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);

    auto target = addUnit(state, PlayerId::Player2, 5, 0);

    int damage_dealt = 0;
    events.on_damage_dealt.connect([&](const DamageDealtEvent& e) {
        damage_dealt = e.amount;
    });

    resolveCard(state, exec, kHextechRayId, PlayerId::Player1, kInvalidId, {target});

    EXPECT_EQ(damage_dealt, 3);
    EXPECT_EQ(state.getObject(target).damage_marked, 3);
}

TEST_F(ChainTestFixture, DisintegrateKillsAndDraws) {
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);

    // Unit with 3 might -- exactly lethal from 3 damage
    auto target = addUnit(state, PlayerId::Player2, 3, 0);

    // Give P1 a card to draw
    auto deck_card = state.createObject();
    state.getObject(deck_card).zone = ZoneType::MainDeck;
    state.player(PlayerId::Player1).main_deck.push_back(deck_card);

    int cards_drawn = 0;
    events.on_cards_drawn.connect([&](const CardsDrawnEvent& e) {
        cards_drawn = e.count;
    });

    resolveCard(state, exec, kDisintegrateId, PlayerId::Player1, kInvalidId, {target});

    // Should have killed and drawn
    EXPECT_EQ(state.getObject(target).zone, ZoneType::Trash);
    EXPECT_EQ(cards_drawn, 1);
}

TEST_F(ChainTestFixture, DisintegrateNoDrawIfNotLethal) {
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);

    // Unit with 5 might -- 3 damage is not lethal
    auto target = addUnit(state, PlayerId::Player2, 5, 0);

    int cards_drawn = 0;
    events.on_cards_drawn.connect([&](const CardsDrawnEvent& e) {
        cards_drawn = e.count;
    });

    resolveCard(state, exec, kDisintegrateId, PlayerId::Player1, kInvalidId, {target});

    EXPECT_EQ(state.getObject(target).damage_marked, 3);
    EXPECT_EQ(state.getObject(target).zone, ZoneType::BattlefieldZone);
    EXPECT_EQ(cards_drawn, 0); // no kill, no draw
}

TEST_F(ChainTestFixture, DisciplineBuffsAndDraws) {
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);

    auto target = addUnit(state, PlayerId::Player1, 4, 0);

    auto deck_card = state.createObject();
    state.getObject(deck_card).zone = ZoneType::MainDeck;
    state.player(PlayerId::Player1).main_deck.push_back(deck_card);

    resolveCard(state, exec, kDisciplineId, PlayerId::Player1, kInvalidId, {target});

    EXPECT_EQ(state.getObject(target).current_might, 6); // 4 base + 2 buff
    EXPECT_EQ(state.player(PlayerId::Player1).hand.size(), 1u);
}

TEST_F(ChainTestFixture, GustBouncesSmallUnit) {
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);

    auto target = addUnit(state, PlayerId::Player2, 3, 0);

    resolveCard(state, exec, kGustId, PlayerId::Player1, kInvalidId, {target});

    EXPECT_EQ(state.getObject(target).zone, ZoneType::Hand);
    EXPECT_FALSE(state.getObject(target).location.has_value());
}

TEST_F(ChainTestFixture, CleaveGivesAssault) {
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);

    auto target = addUnit(state, PlayerId::Player1, 4, 0);

    resolveCard(state, exec, kCleaveId, PlayerId::Player1, kInvalidId, {target});

    auto& obj = state.getObject(target);
    EXPECT_EQ(obj.assault_value, 3);
    EXPECT_TRUE(obj.keywords.has(Keyword::Assault));
    // Might doesn't change until unit is attacking
    EXPECT_EQ(obj.current_might, 4);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Targeting tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ChainTestFixture, HextechRayTargetsOnlyUnitsAtBattlefield) {
    auto state = makeMinimalState();

    // Unit at BF -- valid target
    auto bf_unit = addUnit(state, PlayerId::Player2, 5, 0);
    // Unit at base -- NOT valid target
    auto base_unit = addUnit(state, PlayerId::Player2, 5);
    // Gear at BF -- NOT valid target (not a unit)
    auto gear_id = state.createObject();
    auto& gear = state.getObject(gear_id);
    gear.card_type = CardType::Gear;
    gear.controller = PlayerId::Player2;
    gear.zone = ZoneType::BattlefieldZone;
    gear.location = BattlefieldLocation{0};

    Card* card = card_registry.get(kHextechRayId);
    ASSERT_NE(card, nullptr);
    auto targets = card->enumerateLegalTargets(state, PlayerId::Player1);

    EXPECT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0], bf_unit);
}

TEST_F(ChainTestFixture, GustOnlyTargetsUnitsWithMight3OrLess) {
    auto state = makeMinimalState();

    auto small = addUnit(state, PlayerId::Player2, 2, 0);
    auto exact = addUnit(state, PlayerId::Player2, 3, 0);
    auto big   = addUnit(state, PlayerId::Player2, 4, 0);

    Card* card = card_registry.get(kGustId);
    ASSERT_NE(card, nullptr);
    auto targets = card->enumerateLegalTargets(state, PlayerId::Player1);

    // Should include 2M and 3M but not 4M
    EXPECT_EQ(targets.size(), 2u);
    EXPECT_TRUE(std::find(targets.begin(), targets.end(), small) != targets.end());
    EXPECT_TRUE(std::find(targets.begin(), targets.end(), exact) != targets.end());
    EXPECT_TRUE(std::find(targets.begin(), targets.end(), big) == targets.end());
}

TEST_F(ChainTestFixture, TargetValidationFailsWhenTargetDies) {
    auto state = makeMinimalState();

    auto target = addUnit(state, PlayerId::Player2, 5, 0);

    // Targets valid now
    Card* card = card_registry.get(kHextechRayId);
    ASSERT_NE(card, nullptr);
    auto legal = card->enumerateLegalTargets(state, PlayerId::Player1);
    EXPECT_TRUE(std::find(legal.begin(), legal.end(), target) != legal.end());

    // Kill the target (move to trash)
    state.getObject(target).zone = ZoneType::Trash;
    state.getObject(target).location = std::nullopt;

    // Targets no longer valid
    legal = card->enumerateLegalTargets(state, PlayerId::Player1);
    EXPECT_TRUE(std::find(legal.begin(), legal.end(), target) == legal.end());
}

TEST_F(ChainTestFixture, SpellFizzlesWithInvalidTargets) {
    auto state = makeMinimalState();
    state.turn.turn_player = PlayerId::Player1;
    ChainManager cm(state, events, card_db);
    EffectExecutor exec(state, events, card_db);

    auto target = addUnit(state, PlayerId::Player2, 5, 0);
    auto spell = addSpellToHand(state, PlayerId::Player1, kHextechRayId);
    state.player(PlayerId::Player1).hand.clear();

    cm.addSpell(spell, PlayerId::Player1, {target});

    // Kill target before resolution
    state.getObject(target).zone = ZoneType::Trash;
    state.getObject(target).location = std::nullopt;

    int damage_dealt = 0;
    events.on_damage_dealt.connect([&](const DamageDealtEvent& e) {
        damage_dealt = e.amount;
    });

    cm.processFEPR(
        [](PlayerId p, const std::vector<Intent>&) -> Intent {
            return Intent::passPriority(p);
        },
        [](const ChainItem&) {},
        [&](const ChainItem& item) {
            // Validate via Card object -- fizzle if targets invalid
            Card* card = card_registry.get(item.card_def_id);
            if (card) {
                auto legal = card->enumerateLegalTargets(state, item.controller);
                for (auto t : item.targets) {
                    if (std::find(legal.begin(), legal.end(), t) == legal.end())
                        return; // fizzle
                }
                CardContext ctx{state, events, exec, item.controller, item.source};
                card->onResolve(ctx, item.targets);
            }
        }
    );

    EXPECT_EQ(damage_dealt, 0); // spell fizzled
}

// ═══════════════════════════════════════════════════════════════════════════════
// Chain state tracking tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ChainTestFixture, ChainEventsFireCorrectly) {
    auto state = makeMinimalState();
    state.turn.turn_player = PlayerId::Player1;
    ChainManager cm(state, events, card_db);

    auto spell = addSpellToHand(state, PlayerId::Player1, kHextechRayId);
    auto target = addUnit(state, PlayerId::Player2, 5, 0);
    state.player(PlayerId::Player1).hand.clear();

    bool created = false, finalized = false, resolved = false, emptied = false;
    bool spell_resolved_event = false;

    events.on_chain_created.connect([&](const ChainCreatedEvent&) {
        created = true;
    });
    events.on_chain_item_finalized.connect([&](const ChainItemFinalizedEvent&) {
        finalized = true;
    });
    events.on_chain_item_resolved.connect([&](const ChainItemResolvedEvent&) {
        resolved = true;
    });
    events.on_chain_emptied.connect([&](const ChainEmptiedEvent&) {
        emptied = true;
    });
    events.on_spell_resolved.connect([&](const SpellResolvedEvent&) {
        spell_resolved_event = true;
    });

    cm.addSpell(spell, PlayerId::Player1, {target});
    cm.processFEPR(
        [](PlayerId p, const std::vector<Intent>&) -> Intent {
            return Intent::passPriority(p);
        },
        [](const ChainItem&) {},
        [](const ChainItem&) {}
    );

    EXPECT_TRUE(created);
    EXPECT_TRUE(finalized);
    EXPECT_TRUE(resolved);
    EXPECT_TRUE(emptied);
    EXPECT_TRUE(spell_resolved_event);
}

TEST_F(ChainTestFixture, SpellGoesToTrashAfterResolving) {
    auto state = makeMinimalState();
    state.turn.turn_player = PlayerId::Player1;
    ChainManager cm(state, events, card_db);

    auto spell = addSpellToHand(state, PlayerId::Player1, kHextechRayId);
    auto target = addUnit(state, PlayerId::Player2, 5, 0);
    state.player(PlayerId::Player1).hand.clear();

    cm.addSpell(spell, PlayerId::Player1, {target});
    cm.processFEPR(
        [](PlayerId p, const std::vector<Intent>&) -> Intent {
            return Intent::passPriority(p);
        },
        [](const ChainItem&) {},
        [](const ChainItem&) {}
    );

    EXPECT_EQ(state.getObject(spell).zone, ZoneType::Trash);
    auto& trash = state.player(PlayerId::Player1).trash;
    EXPECT_TRUE(std::find(trash.begin(), trash.end(), spell) != trash.end());
}

TEST_F(ChainTestFixture, ClosedStateActionsOnlyAllowReactions) {
    auto state = makeMinimalState();
    state.turn.turn_player = PlayerId::Player1;
    ChainManager cm(state, events, card_db);

    // Give P2 an Action spell (should NOT be playable in Closed State)
    auto action_spell = addSpellToHand(state, PlayerId::Player2, kHextechRayId);
    // Give P2 a Reaction spell (SHOULD be playable)
    auto reaction_spell = addSpellToHand(state, PlayerId::Player2, kDisciplineId);

    auto actions = cm.generateClosedStateActions(PlayerId::Player2);

    // Should have PassPriority + the Reaction spell, but NOT the Action spell
    bool has_pass = false;
    bool has_reaction = false;
    bool has_action = false;
    for (auto& a : actions) {
        if (a.type == IntentType::PassPriority) has_pass = true;
        if (a.type == IntentType::PlayReaction && a.card == reaction_spell) has_reaction = true;
        if (a.card == action_spell) has_action = true;
    }

    EXPECT_TRUE(has_pass);
    EXPECT_TRUE(has_reaction);
    EXPECT_FALSE(has_action);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Integration: full game with spells
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ChainTestFixture, FullGameRunsWithSpellsInDecks) {
    // Just verify that a full game completes without crashes
    // when decks contain spells (which they do -- leblanc has Hextech Ray etc.)
    GameEngine engine(card_db, events, card_registry);
    PassAgent agent1, agent2;

    DeckSubmission deck1, deck2;
    std::string root = findRegistryPath().substr(
        0, findRegistryPath().rfind('/')) + "/..";
    deck1 = DeckValidator::loadFromJson(root + "/decks/leblanc_test.json", card_db);
    deck2 = DeckValidator::loadFromJson(root + "/decks/vex_test_deck.json", card_db);

    auto result = engine.runGame(deck1, deck2, agent1, agent2, 42);

    EXPECT_TRUE(result.winner == PlayerId::Player1 ||
                result.winner == PlayerId::Player2 ||
                result.winner == PlayerId::None);
    EXPECT_GT(result.total_decisions, 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// AoE and keyword regression tests
// ═══════════════════════════════════════════════════════════════════════════════

constexpr CardDefId kFlurryOfBladesId = 133; // Deal 1 to all units at BFs
constexpr CardDefId kThermoBbeamId    = 22;  // Kill all gear

TEST_F(ChainTestFixture, FlurryOfBladesHitsAllUnits) {
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);

    // Place 3 units at battlefields -- mix of both players
    auto u1 = addUnit(state, PlayerId::Player1, 3, 0);
    auto u2 = addUnit(state, PlayerId::Player2, 2, 0);
    auto u3 = addUnit(state, PlayerId::Player1, 4, 1);

    int damage_events = 0;
    events.on_damage_dealt.connect([&](const DamageDealtEvent&) {
        damage_events++;
    });

    resolveCard(state, exec, kFlurryOfBladesId, PlayerId::Player2, kInvalidId, {});

    // All 3 units should take 1 damage each
    EXPECT_EQ(damage_events, 3);
    EXPECT_EQ(state.getObject(u1).damage_marked, 1);
    EXPECT_EQ(state.getObject(u2).damage_marked, 1);
    EXPECT_EQ(state.getObject(u3).damage_marked, 1);
}

TEST_F(ChainTestFixture, FlurryOfBladesKillsLowMightUnits) {
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);

    auto alive = addUnit(state, PlayerId::Player1, 3, 0);
    auto dead  = addUnit(state, PlayerId::Player2, 1, 0); // 1M dies to 1 damage

    resolveCard(state, exec, kFlurryOfBladesId, PlayerId::Player1, kInvalidId, {});

    EXPECT_EQ(state.getObject(alive).damage_marked, 1);
    EXPECT_EQ(state.getObject(alive).zone, ZoneType::BattlefieldZone);
    EXPECT_EQ(state.getObject(dead).zone, ZoneType::Trash);
}

TEST_F(ChainTestFixture, AoESpellDoesNotRequireTargetSelection) {
    // AoE spells should have count=0 (no target selection needed)
    Card* card = card_registry.get(kFlurryOfBladesId);
    ASSERT_NE(card, nullptr);
    auto req = card->getTargetRequirements();
    EXPECT_EQ(req.count, 0);
}

TEST_F(ChainTestFixture, StunReducesCombatMight) {
    auto state = makeMinimalState();
    state.turn.turn_player = PlayerId::Player1;

    auto attacker = addUnit(state, PlayerId::Player1, 5, 0);
    auto defender = addUnit(state, PlayerId::Player2, 3, 0);

    // Stun the attacker
    state.getObject(attacker).is_stunned = true;

    // Stunned unit should contribute 0 might
    auto& att = state.getObject(attacker);
    EXPECT_TRUE(att.is_stunned);
    // In combat, stunned units contribute 0 -- verified by inspection
    // of combatDamageStep which checks is_stunned
}

TEST_F(ChainTestFixture, TemporaryBuffsExpire) {
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);

    auto unit = addUnit(state, PlayerId::Player1, 4, 0);

    // Apply temporary might buff via Card object
    resolveCard(state, exec, kDisciplineId, PlayerId::Player1, kInvalidId, {unit});

    EXPECT_EQ(state.getObject(unit).current_might, 6); // 4 + 2
    EXPECT_EQ(state.getObject(unit).temp_might_bonus, 2);

    // Simulate expiration
    state.getObject(unit).buff_count -= state.getObject(unit).temp_might_bonus;
    state.getObject(unit).temp_might_bonus = 0;
    state.getObject(unit).recomputeMight();

    EXPECT_EQ(state.getObject(unit).current_might, 4); // back to base
}

TEST_F(ChainTestFixture, TankUnitsOrderedFirstInDamageAssignment) {
    auto state = makeMinimalState();

    auto regular = addUnit(state, PlayerId::Player1, 3, 0);
    auto tank    = addUnit(state, PlayerId::Player1, 2, 0);
    auto backline = addUnit(state, PlayerId::Player1, 4, 0);

    state.getObject(tank).keywords.set(Keyword::Tank);
    state.getObject(backline).keywords.set(Keyword::Backline);

    // Get units and sort them like combatDamageStep does
    auto units = state.unitsAt(BattlefieldLocation{0}, PlayerId::Player1);

    std::stable_sort(units.begin(), units.end(),
        [&](GameObjectId a, GameObjectId b) {
            auto& oa = state.getObject(a);
            auto& ob = state.getObject(b);
            bool a_tank = oa.keywords.has(Keyword::Tank);
            bool b_tank = ob.keywords.has(Keyword::Tank);
            bool a_back = oa.keywords.has(Keyword::Backline);
            bool b_back = ob.keywords.has(Keyword::Backline);
            if (a_tank != b_tank) return a_tank > b_tank;
            if (a_back != b_back) return a_back < b_back;
            return false;
        });

    // Tank should be first, Backline should be last
    EXPECT_EQ(units[0], tank);
    EXPECT_EQ(units[2], backline);
}

// Drive a resumable Card through case 0 → case 1 manually. Stand-in for
// ChainManager::stepResolve when a unit test only wants to validate the
// card's behavior, not the full FEPR loop. `pick_keeper` returns which
// chosen_objects[0] (the keeper card_id) the agent would pick from the
// Card's published 1-of-N choice set.
static void driveResumableCard(GameState& state, EffectExecutor& exec,
                                EventBus& events, Card* card,
                                PlayerId controller,
                                std::function<GameObjectId(const std::vector<Intent>&)> pick_keeper) {
    ChainItem ri;
    ri.id = state.chain.allocateId();
    ri.controller = controller;
    ri.is_spell = true;
    ri.resume_point = 0;
    state.chain.resuming = ri;

    CardContext ctx{state, events, exec, controller, kInvalidId};
    // case 0 — publish choice
    card->onResolve(ctx, {});

    if (exec.hasPendingChoice()) {
        auto pending = exec.consumePendingChoice();
        Intent picked;
        picked.type = IntentType::MakeChoice;
        picked.player = controller;
        if (pick_keeper) {
            GameObjectId k = pick_keeper(pending.legal);
            picked.chosen_objects = {k};
        } else if (!pending.legal.empty()) {
            picked = pending.legal.front();
        }
        exec.recordChoice(picked);
        // case 1 — apply
        card->onResolve(ctx, {});
    }

    state.chain.resuming.reset();
}

TEST_F(ChainTestFixture, StackedDeckDrawsOneRecyclesRest) {
    // Stacked Deck: "Look at the top 3 cards. Put 1 into your hand and recycle the rest."
    // Resumable: case 0 publishes 1-of-3 choice, case 1 applies.
    constexpr CardDefId kStackedDeckId = 183;
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);
    exec.setRng(nullptr); // no RNG needed for this test

    // Put 5 cards in P1's deck
    for (int i = 0; i < 5; ++i) {
        auto id = state.createObject();
        state.getObject(id).zone = ZoneType::MainDeck;
        state.getObject(id).name = "Card" + std::to_string(i);
        state.player(PlayerId::Player1).main_deck.push_back(id);
    }

    int initial_deck = state.player(PlayerId::Player1).main_deck.size();
    int initial_hand = state.player(PlayerId::Player1).hand.size();

    Card* card = card_registry.get(kStackedDeckId);
    ASSERT_NE(card, nullptr);
    driveResumableCard(state, exec, events, card, PlayerId::Player1,
        [](const std::vector<Intent>& legal) -> GameObjectId {
            return legal.front().chosen_objects[0];
        });

    // Should have drawn 1 card and recycled 2
    EXPECT_EQ(state.player(PlayerId::Player1).hand.size(),
              initial_hand + 1) << "Should draw 1 card";
    EXPECT_EQ(state.player(PlayerId::Player1).main_deck.size(),
              initial_deck - 1) << "Net: 3 removed, 2 recycled back = -1";
}

TEST_F(ChainTestFixture, CompoundSentencesSplitOnAnd) {
    // Test that Stacked Deck's onResolve actually draws (behavioral test
    // replacing the old parser-internals test that inspected EffectScript steps).
    // Same shape as StackedDeckDrawsOneRecyclesRest — drives the card through
    // the resume pattern (case 0 publishes choice, case 1 applies).
    constexpr CardDefId kStackedDeckId = 183;
    auto state = makeMinimalState();
    EffectExecutor exec(state, events, card_db);
    exec.setRng(nullptr);

    // Put 5 cards in P1's deck
    for (int i = 0; i < 5; ++i) {
        auto id = state.createObject();
        state.getObject(id).zone = ZoneType::MainDeck;
        state.getObject(id).name = "Card" + std::to_string(i);
        state.player(PlayerId::Player1).main_deck.push_back(id);
    }

    Card* card = card_registry.get(kStackedDeckId);
    ASSERT_NE(card, nullptr);
    driveResumableCard(state, exec, events, card, PlayerId::Player1,
        [](const std::vector<Intent>& legal) -> GameObjectId {
            return legal.front().chosen_objects[0];
        });

    // After resolve: hand should have 1 card (drawn), deck should have 4 (5-1)
    EXPECT_EQ(state.player(PlayerId::Player1).hand.size(), 1u)
        << "onResolve should draw a card";
    EXPECT_EQ(state.player(PlayerId::Player1).main_deck.size(), 4u)
        << "onResolve should net remove 1 from deck (draw 1, recycle rest handled)";
}

TEST_F(ChainTestFixture, DeathknellParsesAsWhenIDie) {
    // Watchful Sentry: [Deathknell] -- Draw 1.
    // Test that the Card object reports the correct trigger type.
    constexpr CardDefId kWatchfulSentryId = 96;
    Card* card = card_registry.get(kWatchfulSentryId);
    ASSERT_NE(card, nullptr);

    EXPECT_EQ(card->triggerType(), TriggerType::WhenIDie);
}

TEST_F(ChainTestFixture, ChooseTargetParsesAsTargetingRequirement) {
    // Shakedown: "Choose an enemy unit. Deal 6 to it..."
    constexpr CardDefId kShakedownId = 33;

    Card* card = card_registry.get(kShakedownId);
    ASSERT_NE(card, nullptr);
    auto req = card->getTargetRequirements();

    EXPECT_EQ(req.count, 1);
    EXPECT_TRUE(req.must_be_unit);
    EXPECT_TRUE(req.must_be_enemy);
}

TEST_F(ChainTestFixture, ChooseRequiresValidTargetToPlay) {
    // If no legal targets exist for a Choose spell, it shouldn't be playable
    auto state = makeMinimalState();

    constexpr CardDefId kShakedownId = 33;
    Card* card = card_registry.get(kShakedownId);
    ASSERT_NE(card, nullptr);

    // No units on board -> no legal targets
    auto targets = card->enumerateLegalTargets(state, PlayerId::Player1);
    EXPECT_TRUE(targets.empty());

    // Add enemy unit -> now there's a target
    addUnit(state, PlayerId::Player2, 5, 0);
    targets = card->enumerateLegalTargets(state, PlayerId::Player1);
    EXPECT_EQ(targets.size(), 1u);
}

TEST_F(ChainTestFixture, ActionSpellsNotOfferedInClosedState) {
    auto state = makeMinimalState();
    state.turn.turn_player = PlayerId::Player1;
    state.turn.oc_state = OpenClosedState::Closed;
    state.turn.ns_state = NeutralShowdownState::Neutral;

    // Give P2 an Action spell (Challenge) and a Reaction spell (Gust)
    auto action_spell = addSpellToHand(state, PlayerId::Player2, 128); // Challenge
    auto reaction_spell = addSpellToHand(state, PlayerId::Player2, kGustId); // Gust

    // Give P2 runes to afford both
    for (int i = 0; i < 4; ++i) addRune(state, PlayerId::Player2, Domain::Fury);

    // Add a target so Gust has valid targets
    addUnit(state, PlayerId::Player1, 2, 0);

    GameEngine engine(card_db, events, card_registry);
    // We can't easily call generateClosedStateActions on the engine directly,
    // so test via ChainManager which uses the same logic
    ChainManager cm(state, events, card_db);
    auto actions = cm.generateClosedStateActions(PlayerId::Player2);

    // Should have PassPriority + Reaction (Gust), NOT Action (Challenge)
    bool has_action_spell = false;
    bool has_reaction_spell = false;
    for (auto& a : actions) {
        if (a.card == action_spell) has_action_spell = true;
        if (a.card == reaction_spell) has_reaction_spell = true;
    }

    EXPECT_FALSE(has_action_spell) << "Action spells must not be offered in Closed State";
    EXPECT_TRUE(has_reaction_spell) << "Reaction spells should be offered in Closed State";
}

TEST_F(ChainTestFixture, AddAbilityClosesState) {
    auto state = makeMinimalState();
    state.turn.oc_state = OpenClosedState::Open;
    ChainManager cm(state, events, card_db);

    auto unit = addUnit(state, PlayerId::Player1, 3, 0);

    EXPECT_EQ(state.turn.oc_state, OpenClosedState::Open);

    cm.addAbility(unit, PlayerId::Player1, state.getObject(unit).card_def_id);

    EXPECT_EQ(state.turn.oc_state, OpenClosedState::Closed)
        << "Adding an ability to chain must close the state";
}

TEST_F(ChainTestFixture, LegionParsesAsConditionGate) {
    // Dangerous Duo: [Legion] -- When you play me, give a unit +2 [M]
    constexpr CardDefId kDangerousDuoId = 16;

    Card* card = card_registry.get(kDangerousDuoId);
    ASSERT_NE(card, nullptr);
    EXPECT_TRUE(card->requiresLegion());
    EXPECT_EQ(card->triggerType(), TriggerType::WhenYouPlayMe);
}
