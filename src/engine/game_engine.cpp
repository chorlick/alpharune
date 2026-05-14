#include "game_engine.h"
#include "cards/card.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace riftbound {

GameEngine::GameEngine(const CardDB& card_db, EventBus& event_bus,
                       const CardRegistry& card_registry)
    : card_db_(card_db), events_(event_bus), card_registry_(card_registry) {
    // Chain subsystems are initialized per-game in runGame
}

// ═══════════════════════════════════════════════════════════════════════════════
// Game lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

GameResult GameEngine::runGame(
    const DeckSubmission& deck1,
    const DeckSubmission& deck2,
    AgentInterface& agent1,
    AgentInterface& agent2,
    uint64_t seed) {

    rng_.seed(seed == 0 ? std::random_device{}() : seed);
    agents_[0] = &agent1;
    agents_[1] = &agent2;

    state_ = GameState{};
    state_.mode = ModeOfPlay{};

    // Initialize Phase 2 subsystems
    chain_manager_ = std::make_unique<ChainManager>(state_, events_, card_db_);
    chain_manager_->setAffordCheck(
        [this](PlayerId p, GameObjectId card) { return canAfford(p, card); });
    chain_manager_->setPayCost(
        [this](PlayerId p, GameObjectId card) { return payCardCost(p, card); });
    effect_executor_ = std::make_unique<EffectExecutor>(state_, events_, card_db_);
    effect_executor_->setRng(&rng_);
    effect_executor_->setAgentQuery(
        [this](PlayerId p, const std::vector<Intent>& actions) -> Intent {
            return queryAgentForChain(p, actions);
        });
    trigger_manager_ = std::make_unique<TriggerManager>(
        state_, events_, card_db_, *chain_manager_, card_registry_);
    trigger_manager_->setEffectExecutor(effect_executor_.get());
    trigger_manager_->subscribe();

    setupGame(deck1, deck2);
    drawOpeningHands();
    runMulligans();

    events_.emit(PhaseChangedEvent{TurnPhase::Setup, TurnPhase::AwakenPhase,
                                    state_.turn.turn_player});

    runTurnLoop();

    GameResult result;
    result.winner = state_.winner;
    result.final_scores[0] = state_.players[0].score;
    result.final_scores[1] = state_.players[1].score;
    result.total_turns = state_.turn.turn_number;
    result.total_decisions = state_.decision_index;
    result.termination_reason = state_.game_over_reason;
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Setup
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::setupGame(const DeckSubmission& deck1,
                            const DeckSubmission& deck2) {
    state_.turn.phase = TurnPhase::Setup;

    state_.players[0].id = PlayerId::Player1;
    state_.players[1].id = PlayerId::Player2;

    setupPlayer(PlayerId::Player1, deck1);
    setupPlayer(PlayerId::Player2, deck2);

    determineTurnOrder();
    setupBattlefields(deck1, deck2);
}

void GameEngine::setupPlayer(PlayerId player, const DeckSubmission& deck) {
    auto& ps = state_.player(player);

    // Legend → Legend Zone
    auto legend_id = instantiateCard(deck.legend, player);
    auto& legend_obj = state_.getObject(legend_id);
    legend_obj.zone = ZoneType::LegendZone;
    ps.legend_zone = legend_id;

    // Chosen Champion → Champion Zone (starts here, can be played from here)
    auto champ_id = instantiateCard(deck.chosen_champion, player);
    auto& champ_obj = state_.getObject(champ_id);
    champ_obj.zone = ZoneType::ChampionZone;
    // Champion zone has no board location — it's a non-board zone
    ps.champion_zone = champ_id;

    // Main Deck → shuffled
    for (auto card_def_id : deck.main_deck) {
        auto obj_id = instantiateCard(card_def_id, player);
        state_.getObject(obj_id).zone = ZoneType::MainDeck;
        ps.main_deck.push_back(obj_id);
    }
    shuffleDeck(player);

    // Rune Deck → shuffled
    for (auto rune_def_id : deck.rune_deck) {
        auto obj_id = instantiateCard(rune_def_id, player);
        state_.getObject(obj_id).zone = ZoneType::RuneDeck;
        ps.rune_deck.push_back(obj_id);
    }
    shuffleRuneDeck(player);

    // Store battlefield card defs (selection happens in setupBattlefields)
    // For now, store all 3 as potential choices
}

void GameEngine::setupBattlefields(const DeckSubmission& deck1,
                                     const DeckSubmission& deck2) {
    // 1v1: Each player randomly selects 1 of their 3 battlefields (CR 480.5)
    // Phase 1: random selection
    auto pickRandom = [&](const std::vector<CardDefId>& bfs) -> CardDefId {
        std::uniform_int_distribution<size_t> dist(0, bfs.size() - 1);
        return bfs[dist(rng_)];
    };

    auto bf1_def = pickRandom(deck1.battlefields);
    auto bf2_def = pickRandom(deck2.battlefields);

    // Create battlefield game objects and state
    BattlefieldId next_bf_id = 0;

    auto addBF = [&](CardDefId def_id, PlayerId contributor) {
        auto obj_id = instantiateCard(def_id, contributor);
        auto& obj = state_.getObject(obj_id);
        obj.zone = ZoneType::BattlefieldZone;

        BattlefieldState bf;
        bf.id = next_bf_id++;
        bf.card_object_id = obj_id;
        bf.contributed_by = contributor;
        state_.battlefields.push_back(bf);
    };

    addBF(bf1_def, PlayerId::Player1);
    addBF(bf2_def, PlayerId::Player2);
}

void GameEngine::determineTurnOrder() {
    // Random coin flip for first player (CR 116)
    std::uniform_int_distribution<int> coin(0, 1);
    state_.turn.turn_player = coin(rng_) == 0 ? PlayerId::Player1 : PlayerId::Player2;
    state_.turn.starting_player = state_.turn.turn_player;
    state_.turn.turn_number = 0;

    // Mark both players as being on their first turn
    state_.players[0].is_first_turn = true;
    state_.players[1].is_first_turn = true;
}

void GameEngine::drawOpeningHands() {
    // Each player draws 4 (CR 117)
    events_.logTrace("── Opening Hands ──");
    events_.logTrace(std::string("DRAW_HAND: ") + toString(PlayerId::Player1) + " draws 4");
    drawCards(PlayerId::Player1, 4);
    events_.logTrace(std::string("DRAW_HAND: ") + toString(PlayerId::Player2) + " draws 4");
    drawCards(PlayerId::Player2, 4);
}

void GameEngine::runMulligans() {
    // In turn order, each player may mulligan up to 2 cards (CR 118)
    events_.logTrace("── Mulligan Phase ──");
    state_.turn.phase = TurnPhase::Mulligan;

    for (auto player : {state_.turn.turn_player,
                        opponent(state_.turn.turn_player)}) {
        auto actions = generateMulliganActions(player);
        if (actions.empty()) continue;

        state_.decision_index++;
        if (player == PlayerId::Player1) state_.turn.turn_decisions_p1++;
        else state_.turn.turn_decisions_p2++;

        events_.logTrace("DECISION #" + std::to_string(state_.decision_index) +
                         " (" + toString(player) + "): Mulligan, " +
                         std::to_string(actions.size()) + " options");

        auto chosen = getAgent(player).selectAction(state_, actions);

        // Log what was chosen
        if (chosen.cards_to_mulligan.empty()) {
            events_.logTrace(std::string("  CHOSE: Keep hand"));
        } else {
            std::string mull_str;
            for (auto cid : chosen.cards_to_mulligan) {
                if (!mull_str.empty()) mull_str += ", ";
                if (state_.objectExists(cid)) mull_str += state_.getObject(cid).name;
            }
            events_.logTrace("  CHOSE: Mulligan " + std::to_string(chosen.cards_to_mulligan.size()) +
                             " [" + mull_str + "]");
        }

        if (on_decision) {
            on_decision(state_, actions, chosen);
        }

        if (chosen.type == IntentType::MulliganDecision) {
            executeMulligan(chosen);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Turn loop
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::runTurnLoop() {
    // Safety limit to prevent infinite games
    constexpr int kMaxTurns = 200;

    while (!state_.game_over && state_.turn.turn_number < kMaxTurns) {
        runTurn(state_.turn.turn_player);

        if (state_.game_over) break;

        // Check for additional turns (CR 734-738)
        // Check both players' queues
        auto& p1 = state_.players[0];
        auto& p2 = state_.players[1];
        if (!p1.additional_turns.empty()) {
            auto next = p1.additional_turns.front();
            p1.additional_turns.erase(p1.additional_turns.begin());
            state_.turn.turn_player = next;
            state_.turn.is_additional_turn = true;
            events_.logTrace(std::string("ADDITIONAL_TURN: ") + toString(next));
        } else if (!p2.additional_turns.empty()) {
            auto next = p2.additional_turns.front();
            p2.additional_turns.erase(p2.additional_turns.begin());
            state_.turn.turn_player = next;
            state_.turn.is_additional_turn = true;
            events_.logTrace(std::string("ADDITIONAL_TURN: ") + toString(next));
        } else {
            // Normal turn alternation
            state_.turn.turn_player = opponent(state_.turn.turn_player);
            state_.turn.is_additional_turn = false;
        }
        state_.turn.turn_number++;
    }

    if (!state_.game_over) {
        // Max turns reached — draw
        state_.game_over = true;
        state_.winner = PlayerId::None;
        state_.game_over_reason = "Max turns reached";
        events_.emit(GameOverEvent{PlayerId::None, "Max turns reached"});
    }
}

void GameEngine::runTurn(PlayerId player) {
    events_.logTrace("════ TURN " + std::to_string(state_.turn.turn_number) +
                     " (" + toString(player) + ") ════ Score: P1=" +
                     std::to_string(state_.players[0].score) + " P2=" +
                     std::to_string(state_.players[1].score));
    state_.player(player).resetTurnTracking();
    state_.turn.turn_decisions_p1 = 0;
    state_.turn.turn_decisions_p2 = 0;

    awakenPhase();
    if (state_.game_over) return;

    beginningStep();
    if (state_.game_over) return;

    scoringStep();
    if (state_.game_over) return;

    channelPhase();
    if (state_.game_over) return;

    drawPhase();
    if (state_.game_over) return;

    mainPhase();
    if (state_.game_over) return;

    endingStep();
    if (state_.game_over) return;

    expirationStep();

    // Mark first turn as done
    state_.player(player).is_first_turn = false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Phases
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::awakenPhase() {
    events_.logTrace("── Awaken Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::AwakenPhase;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::AwakenPhase,
                                    state_.turn.turn_player});

    // Ready all game objects the turn player controls (CR 315.1.b)
    auto player = state_.turn.turn_player;
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller == player && obj.is_exhausted && obj.location.has_value()) {
            obj.is_exhausted = false;
            events_.emit(ObjectStateChangedEvent{id, "readied"});
        }
    }
    // Also ready the legend
    auto legend_id = state_.player(player).legend_zone;
    if (legend_id != kInvalidId && state_.objectExists(legend_id)) {
        auto& legend = state_.getObject(legend_id);
        if (legend.is_exhausted) {
            legend.is_exhausted = false;
            events_.emit(ObjectStateChangedEvent{legend_id, "readied"});
        }
    }
}

void GameEngine::beginningStep() {
    events_.logTrace("── Beginning Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::BeginningStep;

    // Kill Temporary units before scoring (CR: "Kill me at the start of
    // my controller's Beginning Phase, before scoring")
    std::vector<GameObjectId> to_kill;
    for (auto& [id, obj] : state_.objects) {
        if (obj.isUnit() && obj.keywords.has(Keyword::Temporary) &&
            obj.controller == state_.turn.turn_player && obj.location.has_value()) {
            to_kill.push_back(id);
        }
    }
    for (auto id : to_kill) {
        killUnit(id);
    }

    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::BeginningStep,
                                    state_.turn.turn_player});

    // Process "At the start of your Beginning Phase" triggers
    if (chain_manager_->chainExists()) runChain();
}

void GameEngine::scoringStep() {
    events_.logTrace("── Scoring Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::ScoringStep;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::ScoringStep,
                                    state_.turn.turn_player});

    // Turn player Holds all battlefields they control (CR 315.2.b)
    auto player = state_.turn.turn_player;
    for (auto& bf : state_.battlefields) {
        if (bf.controller.has_value() && *bf.controller == player) {
            scoreHold(player, bf.id);
            if (state_.game_over) return;
            // Process hold/score triggers
            if (chain_manager_->chainExists()) runChain();
            if (state_.game_over) return;
        }
    }

    cleanup();
}

void GameEngine::channelPhase() {
    events_.logTrace("── Channel Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::ChannelPhase;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::ChannelPhase,
                                    state_.turn.turn_player});

    auto player = state_.turn.turn_player;
    int count = 2;

    // Second player channels an extra rune on first turn (CR 480.7)
    if (state_.player(player).is_first_turn &&
        player != state_.turn.turn_player) {
        // This is the player going second — but wait, on their first turn
        // they ARE the turn player. The rule is about being the second player.
    }
    // Simplified: if this is the second player's first turn, channel 3
    auto first_player = state_.turn.turn_number == 0 ? state_.turn.turn_player
                        : opponent(state_.turn.turn_player);
    // The player who did NOT go first gets the extra channel on their first turn
    if (state_.player(player).is_first_turn && player != first_player) {
        count = 3;
    }

    channelRunes(player, count);
}

void GameEngine::drawPhase() {
    events_.logTrace("── Draw Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::DrawPhase;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::DrawPhase,
                                    state_.turn.turn_player});

    auto player = state_.turn.turn_player;

    // Draw 1 card (CR 315.4.b)
    drawCards(player, 1);

    // Empty rune pools (CR 315.4.d)
    emptyRunePools();
}

void GameEngine::mainPhase() {
    events_.logTrace("── Main Phase ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::MainPhase;
    state_.turn.ns_state = NeutralShowdownState::Neutral;
    state_.turn.oc_state = OpenClosedState::Open;
    state_.turn.priority_holder = state_.turn.turn_player;

    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::MainPhase,
                                    state_.turn.turn_player});

    // Main phase loop: turn player takes actions until they end their turn
    // or the game ends
    constexpr int kMaxActions = 500; // safety
    int action_count = 0;

    while (!state_.game_over && action_count < kMaxActions) {
        // Check for staged showdowns/combats after cleanup
        processContestedBattlefields();
        if (state_.game_over) return;

        // If in showdown, run it
        for (auto& bf : state_.battlefields) {
            if (bf.showdown_staged && !bf.showdown_in_progress &&
                !bf.combat_staged) {
                runShowdown(bf.id);
                if (state_.game_over) return;
                bf.showdown_staged = false;
            }
            if (bf.combat_staged && !bf.combat_in_progress) {
                runCombat(bf.id);
                if (state_.game_over) return;
                bf.combat_staged = false;
            }
        }

        // Generate legal actions and query agent
        auto actions = generateLegalActions();
        if (actions.empty()) break;

        auto intent = queryAgent(state_.turn.turn_player);
        if (intent.type == IntentType::EndTurn) break;

        executeIntent(intent);

        // Process any triggered abilities that were queued during execution
        // (play triggers, spell resolution triggers, etc.)
        if (chain_manager_->chainExists()) runChain();

        cleanup();

        // Process death triggers / cleanup triggers
        if (chain_manager_->chainExists()) runChain();

        action_count++;
    }
}

void GameEngine::endingStep() {
    events_.logTrace("── Ending Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::EndingStep;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::EndingStep,
                                    state_.turn.turn_player});

    // Process "At the end of your turn" triggers (e.g., Dazzling Aurora)
    if (chain_manager_->chainExists()) runChain();
}

void GameEngine::expirationStep() {
    events_.logTrace("── Expiration Step ──");
    auto old_phase = state_.turn.phase;
    state_.turn.phase = TurnPhase::ExpirationStep;
    events_.emit(PhaseChangedEvent{old_phase, TurnPhase::ExpirationStep,
                                    state_.turn.turn_player});

    // Heal all units (CR 317.2.b)
    healAllUnits();

    // Clear stun on all units (CR 423: stun expires at Ending Step)
    for (auto& [id, obj] : state_.objects) {
        if (obj.is_stunned) {
            obj.is_stunned = false;
        }
    }

    // Expire "this turn" effects (CR 317.2.c)
    for (auto& [id, obj] : state_.objects) {
        if (obj.temp_might_bonus != 0) {
            obj.buff_count -= obj.temp_might_bonus;
            obj.temp_might_bonus = 0;
        }
        if (obj.temp_assault_value != 0) {
            obj.assault_value -= obj.temp_assault_value;
            obj.temp_assault_value = 0;
            if (obj.assault_value <= 0) {
                obj.assault_value = 0;
                // Only clear keyword if base card doesn't have it
                if (obj.card_def_id != kInvalidId) {
                    const auto& def = card_db_.get(obj.card_def_id);
                    if (!def.keywords.has(Keyword::Assault)) {
                        obj.keywords.clear(Keyword::Assault);
                    }
                } else {
                    obj.keywords.clear(Keyword::Assault);
                }
            }
        }
        if (obj.temp_shield_value != 0) {
            obj.shield_value -= obj.temp_shield_value;
            obj.temp_shield_value = 0;
            if (obj.shield_value <= 0) {
                obj.shield_value = 0;
                if (obj.card_def_id != kInvalidId) {
                    const auto& def = card_db_.get(obj.card_def_id);
                    if (!def.keywords.has(Keyword::Shield)) {
                        obj.keywords.clear(Keyword::Shield);
                    }
                } else {
                    obj.keywords.clear(Keyword::Shield);
                }
            }
        }
        if (obj.temp_might_bonus != 0 || obj.temp_assault_value != 0 ||
            obj.temp_shield_value != 0) {
            obj.recomputeMight();
        }
    }
    // Recompute might for all units after expiration
    for (auto& [id, obj] : state_.objects) {
        if (obj.isUnit() && obj.location.has_value()) {
            obj.recomputeMight();
        }
    }

    // Empty rune pools (CR 317.2.d)
    emptyRunePools();

    // Expire delayed abilities that expire at end of turn (CR 389)
    state_.delayed_abilities.erase(
        std::remove_if(state_.delayed_abilities.begin(), state_.delayed_abilities.end(),
            [&](const DelayedAbility& da) {
                return da.expires_on_turn == state_.turn.turn_number;
            }),
        state_.delayed_abilities.end());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Action execution
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::executeIntent(const Intent& intent) {
    events_.logTrace(std::string("INTENT: ") + toString(intent.type) + " by " + toString(intent.player));

    switch (intent.type) {
        case IntentType::PlayCard:
        case IntentType::PlayActionCard: {
            auto& card = state_.getObject(intent.card);
            if (card.isSpell()) {
                executePlaySpell(intent);
            } else {
                executePlayCard(intent);
            }
            break;
        }
        case IntentType::ActivateAbility:
        case IntentType::ActivateActionAbility: {
            auto& source = state_.getObject(intent.ability_source);
            // Skip card-def lookup for tokens (no CardDef). The rest of this
            // handler reads from `source` directly; the previous `def` binding
            // was dead and threw std::out_of_range when source was a token.
            if (source.card_def_id == kInvalidId) break;

            // Equip: card handles its own cost payment and attachment
            if (source.isGear() && !intent.targets.empty() && !source.attached_to.has_value()) {
                Card* gear_card = card_registry_.get(source.card_def_id);
                if (gear_card && gear_card->hasEquipAbility()) {
                    CardContext equip_ctx{state_, events_, *effect_executor_,
                                         intent.player, intent.ability_source};
                    if (gear_card->onEquip(equip_ctx, intent.targets[0])) {
                        cleanup();
                    }
                    break;
                }
            }

            // Activated ability: exhaust source, pay cost, add to chain

            // Pay activation cost via Card object
            Card* ability_card = card_registry_.get(source.card_def_id);
            ActivationCost act_cost = ability_card ? ability_card->getActivationCost() : ActivationCost{};
            if (act_cost.exhaust) {
                source.is_exhausted = true;
                events_.emit(ObjectStateChangedEvent{intent.ability_source, "exhausted"});
            }
            // Pay energy cost
            if (act_cost.energy > 0) {
                int needed = act_cost.energy;
                auto base_loc = BaseLocation{intent.player};
                for (auto& [id, obj] : state_.objects) {
                    if (needed <= 0) break;
                    if (!obj.isRune() || obj.controller != intent.player || obj.is_exhausted) continue;
                    if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
                    obj.is_exhausted = true;
                    needed--;
                    events_.emit(ObjectStateChangedEvent{id, "exhausted"});
                }
            }
            // Pay recycle-self cost [C]
            if (act_cost.recycle_self) {
                events_.logTrace("ACTIVATE_COST: recycle self " + source.name);
                source.zone = ZoneType::MainDeck;
                source.location = std::nullopt;
                state_.player(intent.player).main_deck.insert(
                    state_.player(intent.player).main_deck.begin(), intent.ability_source);
            }
            // Pay discard cost
            if (act_cost.discard && act_cost.discard_count > 0) {
                events_.logTrace("ACTIVATE_COST: discard " + std::to_string(act_cost.discard_count));
                effect_executor_->discardCards(intent.player, act_cost.discard_count);
            }

            // Add ability to chain and resolve
            chain_manager_->addAbility(intent.ability_source, intent.player,
                                        source.card_def_id, intent.targets);
            runChain();
            break;
        }
        case IntentType::StandardMove:
            executeStandardMove(intent);
            break;
        case IntentType::EndTurn:
            executeEndTurn(intent);
            break;
        case IntentType::MulliganDecision:
            executeMulligan(intent);
            break;
        case IntentType::AssignCombatDamage:
            executeAssignCombatDamage(intent);
            break;
        case IntentType::PassFocus:
            executePassFocus(intent);
            break;
        case IntentType::HideCard:
            executeHideCard(intent);
            break;
        case IntentType::Concede:
            state_.game_over = true;
            state_.winner = opponent(intent.player);
            state_.game_over_reason = toString(intent.player) +
                                       std::string(" conceded");
            events_.emit(GameOverEvent{state_.winner, state_.game_over_reason});
            break;
        default:
            break;
    }
}

void GameEngine::executePlayCard(const Intent& intent) {
    auto& ps = state_.player(intent.player);
    auto& card = state_.getObject(intent.card);
    int trace_cost = (card.card_def_id != kInvalidId)
        ? card_db_.get(card.card_def_id).energy_cost : 0;
    events_.logTrace("PLAY: " + card.name + " (id=" + std::to_string(intent.card) +
                     ", " + toString(card.card_type) + ", cost=" +
                     std::to_string(trace_cost) + "E)");

    // Remove from current zone (hand or champion zone) (CR 354: step 1)
    if (card.zone == ZoneType::Hand) {
        auto it = std::find(ps.hand.begin(), ps.hand.end(), intent.card);
        if (it != ps.hand.end()) ps.hand.erase(it);
    } else if (card.zone == ZoneType::ChampionZone) {
        ps.champion_zone = kInvalidId;
    }

    // Pay the card's cost (CR 357)
    payCardCost(intent.player, intent.card);

    // Track play count
    ps.cards_played_this_turn++;
    events_.emit(CardPlayedEvent{intent.card, intent.player,
        card.card_type, ps.cards_played_this_turn});

    // Store the play location on the game object so resolvePermanent can use it.
    // Permanents choose location during finalization (CR 355.2.a).
    card.location = intent.play_location.value_or(BaseLocation{intent.player});

    // Route through chain — permanent resolves immediately at Finalize (CR 337.1.c)
    chain_manager_->addPermanent(intent.card, intent.player);
    runChain();
}

void GameEngine::executePlaySpell(const Intent& intent) {
    auto& ps = state_.player(intent.player);
    auto& card = state_.getObject(intent.card);
    {
        std::string tgt_str;
        for (auto t : intent.targets) {
            if (!tgt_str.empty()) tgt_str += ",";
            tgt_str += state_.objectExists(t) ? state_.getObject(t).name : "?";
            tgt_str += "(id=" + std::to_string(t) + ")";
        }
        events_.logTrace("SPELL: " + card.name + " (id=" + std::to_string(intent.card) +
                         ") targets=[" + tgt_str + "]");
    }

    // Remove from hand
    if (card.zone == ZoneType::Hand) {
        auto it = std::find(ps.hand.begin(), ps.hand.end(), intent.card);
        if (it != ps.hand.end()) ps.hand.erase(it);
    }

    // Pay cost
    payCardCost(intent.player, intent.card);

    // Track play count
    ps.cards_played_this_turn++;
    events_.emit(CardPlayedEvent{intent.card, intent.player,
        card.card_type, ps.cards_played_this_turn});

    // Add spell to chain with targets
    chain_manager_->addSpell(intent.card, intent.player, intent.targets);

    // Run the FEPR loop
    runChain();
}

void GameEngine::runChain() {
    chain_manager_->processFEPR(
        // Agent query callback
        [this](PlayerId player, const std::vector<Intent>& actions) -> Intent {
            return queryAgentForChain(player, actions);
        },
        // Permanent resolution callback (CR 337.1.c)
        [this](const ChainItem& item) {
            resolvePermanent(item);
        },
        // Spell resolution callback
        [this](const ChainItem& item) {
            resolveSpell(item);
        },
        // Closed State action generator (handles targeting + affordability)
        [this](PlayerId player) -> std::vector<Intent> {
            return generateClosedStateActions(player);
        }
    );
}

void GameEngine::resolveSpell(const ChainItem& item) {
    {
        std::string name = state_.objectExists(item.source) ? state_.getObject(item.source).name : "?";
        events_.logTrace("RESOLVE: " + name + " (chain_item=" + std::to_string(item.id) +
                         ", ability=" + std::to_string(item.is_ability) +
                         ", spell=" + std::to_string(item.is_spell) + ")");
    }

    // Validate targets still legal at resolution time
    Card* card = card_registry_.get(item.card_def_id);
    if (card) {
        auto reqs = card->getTargetRequirements();
        if (reqs.count > 0 && !item.targets.empty()) {
            // Check targets are still valid
            auto legal = card->enumerateLegalTargets(state_, item.controller);
            bool valid = true;
            for (auto t : item.targets) {
                if (std::find(legal.begin(), legal.end(), t) == legal.end()) {
                    valid = false;
                    break;
                }
            }
            if (!valid) return; // fizzle
        }
    }

    // Dispatch through Card object
    CardContext ctx{state_, events_, *effect_executor_,
                    item.controller, item.source};

    if (card) {
        if (item.is_ability) {
            // Triggered or activated ability
            card->onTrigger(ctx, item.targets);
        } else {
            // Spell resolution
            card->onResolve(ctx, item.targets);
        }
    }
}

void GameEngine::resolvePermanent(const ChainItem& item) {
    // CR 337.1.c / CR 359.2: Permanent leaves chain, becomes game object on board.
    // Location was pre-selected during executePlayCard and stored on the object.
    auto& card = state_.getObject(item.source);
    auto loc = card.location.value_or(BaseLocation{item.controller});

    card.zone = ZoneType::Base;
    card.location = loc;

    // Units enter exhausted (CR 143.4) unless Accelerate or Ambush-to-BF
    if (card.isUnit()) {
        bool enters_ready = false;
        // Accelerate: pay additional cost to enter ready (CR 805)
        if (card.keywords.has(Keyword::Accelerate) &&
            canAfford(item.controller, item.source)) {
            payCardCost(item.controller, item.source);
            enters_ready = true;
        }
        // Ambush units entering a battlefield enter ready
        if (card.hasKeyword(Keyword::Ambush) && card.isAtBattlefield()) {
            enters_ready = true;
        }
        card.is_exhausted = !enters_ready;
    }
    // Gear enters ready (CR 149.1)
    if (card.isGear()) {
        card.is_exhausted = false;
    }

    card.recomputeMight();

    events_.emit(EnteredBoardEvent{item.source, item.controller,
        card.card_type, loc, true});

    // Quick-Draw: auto-attach to target unit if specified
    if (card.isGear() && card.hasKeyword(Keyword::QuickDraw) && !item.targets.empty()) {
        auto target_unit = item.targets[0];
        if (state_.objectExists(target_unit)) {
            events_.logTrace("QUICK_DRAW: auto-attaching " + card.name + " to " +
                             state_.getObject(target_unit).name);
            attachGearToUnit(item.source, target_unit);
        }
    }

    // Check if playing to a battlefield contests it
    if (card.isUnit() && std::holds_alternative<BattlefieldLocation>(loc)) {
        auto bf_id = std::get<BattlefieldLocation>(loc).id;
        auto& bf = getBattlefield(bf_id);

        if (!bf.controller.has_value() || *bf.controller != item.controller) {
            if (!bf.is_contested) {
                bf.is_contested = true;
                bf.contested_by = item.controller;
                events_.emit(ContestedEvent{bf_id, item.controller});

                bool has_opponent = bf.hasUnitsFrom(
                    opponent(item.controller), state_.objects);
                if (has_opponent) {
                    bf.combat_staged = true;
                } else {
                    bf.showdown_staged = true;
                }
            }
        }
    }
}

Intent GameEngine::queryAgentForChain(PlayerId player,
                                       const std::vector<Intent>& actions) {
    state_.decision_index++;
    auto chosen = getAgent(player).selectAction(state_, actions);
    if (on_decision) {
        on_decision(state_, actions, chosen);
    }
    return chosen;
}

void GameEngine::executeStandardMove(const Intent& intent) {
    if (!intent.move_destination.has_value()) return;
    auto dest = *intent.move_destination;

    for (auto unit_id : intent.units_to_move) {
        auto& unit_obj = state_.getObject(unit_id);
        std::string dest_str = std::holds_alternative<BattlefieldLocation>(dest)
            ? "BF#" + std::to_string(std::get<BattlefieldLocation>(dest).id)
            : "Base(" + std::string(toString(std::get<BaseLocation>(dest).player)) + ")";
        events_.logTrace("MOVE: " + unit_obj.name + " (id=" + std::to_string(unit_id) +
                         ") -> " + dest_str);
        auto& unit = state_.getObject(unit_id);

        // Pay cost: exhaust (CR 144.2)
        unit.is_exhausted = true;
        events_.emit(ObjectStateChangedEvent{unit_id, "exhausted"});

        auto old_location = unit.location;
        moveUnit(unit_id, dest);

        events_.emit(UnitMovedEvent{
            unit_id, intent.player,
            old_location.value_or(BaseLocation{intent.player}),
            dest, true
        });
    }

    // Check if this contests a battlefield
    if (std::holds_alternative<BattlefieldLocation>(dest)) {
        auto bf_id = std::get<BattlefieldLocation>(dest).id;
        auto& bf = getBattlefield(bf_id);

        // Contested if moving to a BF we don't control (CR 187.3.a)
        if (!bf.controller.has_value() || *bf.controller != intent.player) {
            if (!bf.is_contested) {
                bf.is_contested = true;
                bf.contested_by = intent.player;
                events_.emit(ContestedEvent{bf_id, intent.player});

                // Stage showdown or combat
                bool has_opponent_units = bf.hasUnitsFrom(
                    opponent(intent.player), state_.objects);
                if (has_opponent_units) {
                    bf.combat_staged = true;
                } else {
                    bf.showdown_staged = true;
                }
            }
        }
    }
}

void GameEngine::executeEndTurn(const Intent&) {
    // Nothing to do — main phase loop handles this
}

void GameEngine::executeMulligan(const Intent& intent) {
    auto& ps = state_.player(intent.player);

    if (intent.cards_to_mulligan.empty()) return;

    // Set aside chosen cards (CR 118.1)
    std::vector<GameObjectId> set_aside;
    for (auto card_id : intent.cards_to_mulligan) {
        auto it = std::find(ps.hand.begin(), ps.hand.end(), card_id);
        if (it != ps.hand.end()) {
            set_aside.push_back(*it);
            ps.hand.erase(it);
        }
    }

    // Draw replacements (CR 118.2)
    drawCards(intent.player, static_cast<int>(set_aside.size()));

    // Recycle set-aside cards to bottom of deck in random order (CR 118.3)
    std::shuffle(set_aside.begin(), set_aside.end(), rng_);
    for (auto card_id : set_aside) {
        state_.getObject(card_id).zone = ZoneType::MainDeck;
        ps.main_deck.insert(ps.main_deck.begin(), card_id); // bottom = front
    }
}

void GameEngine::executeAssignCombatDamage(const Intent& intent) {
    // Apply damage assignments
    for (auto& assignment : intent.damage_assignments) {
        auto& target = state_.getObject(assignment.target_unit);
        target.damage_marked += assignment.damage;
        events_.emit(DamageDealtEvent{
            assignment.target_unit, assignment.damage,
            kInvalidId, true
        });
    }
}

void GameEngine::executePassFocus(const Intent& intent) {
    state_.turn.players_passed_focus.insert(intent.player);
}

void GameEngine::executeHideCard(const Intent& intent) {
    auto& ps = state_.player(intent.player);
    auto& card = state_.getObject(intent.card);

    events_.logTrace("HIDE: " + card.name + " at BF#" +
                     std::to_string(intent.chosen_battlefield) +
                     " by " + toString(intent.player));

    // Remove from hand
    auto it = std::find(ps.hand.begin(), ps.hand.end(), intent.card);
    if (it != ps.hand.end()) ps.hand.erase(it);

    // Pay [A] cost — recycle a ready rune (universal power cost)
    auto base_loc = BaseLocation{intent.player};
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isRune() || obj.controller != intent.player || obj.is_exhausted) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
        // Recycle the rune for [A]
        events_.logTrace("HIDE_COST: recycled " + obj.name + " (id=" +
                         std::to_string(id) + ") to pay [A]");
        obj.location = std::nullopt;
        obj.zone = ZoneType::RuneDeck;
        ps.rune_deck.insert(ps.rune_deck.begin(), id);
        events_.emit(LeftBoardEvent{id, intent.player, CardType::Rune,
            base_loc, ZoneType::RuneDeck, false});
        break;
    }

    // Place facedown at the chosen battlefield
    auto bf_id = intent.chosen_battlefield;
    auto& bf = getBattlefield(bf_id);
    bf.facedown.push_back(intent.card);

    card.zone = ZoneType::FacedownZone;
    card.location = std::nullopt; // facedown zones are not locations per rules
    card.is_hidden = true;
    card.hidden_at = bf_id;
    card.hidden_on_turn = state_.turn.turn_number;

    events_.logDebug(std::string("HIDDEN: ") + card.name + " hidden at BF#" +
                     std::to_string(bf_id) + " by " + toString(intent.player));
}

void GameEngine::executePlayFromHidden(const Intent& intent) {
    auto& card = state_.getObject(intent.card);
    auto bf_id = card.hidden_at;
    auto& bf = getBattlefield(bf_id);

    // Remove from facedown zone
    auto it = std::find(bf.facedown.begin(), bf.facedown.end(), intent.card);
    if (it != bf.facedown.end()) bf.facedown.erase(it);

    card.is_hidden = false;
    card.hidden_at = kInvalidId;

    // Play ignoring base cost — permanents go to the BF they were hidden at
    if (card.isPermanent()) {
        card.location = BattlefieldLocation{bf_id};
        // Route through chain like normal plays
        auto& ps = state_.player(intent.player);
        ps.cards_played_this_turn++;
        events_.emit(CardPlayedEvent{intent.card, intent.player,
            card.card_type, ps.cards_played_this_turn});
        chain_manager_->addPermanent(intent.card, intent.player);
        runChain();
    } else if (card.isSpell()) {
        auto& ps = state_.player(intent.player);
        ps.cards_played_this_turn++;
        events_.emit(CardPlayedEvent{intent.card, intent.player,
            card.card_type, ps.cards_played_this_turn});
        chain_manager_->addSpell(intent.card, intent.player, intent.targets);
        runChain();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Legal action generation
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<Intent> GameEngine::generateLegalActions() const {
    auto player = state_.turn.turn_player;

    // In Closed State, the priority holder acts (not necessarily turn player)
    if (state_.turn.isClosedState() && state_.turn.priority_holder.has_value()) {
        return generateClosedStateActions(*state_.turn.priority_holder);
    }

    switch (state_.turn.phase) {
        case TurnPhase::Mulligan:
            return generateMulliganActions(player);
        case TurnPhase::MainPhase:
            if (state_.turn.isNeutralOpen()) {
                return generateMainPhaseActions(player);
            }
            if (state_.turn.isShowdownOpen()) {
                // Focus holder acts in showdowns
                if (state_.turn.focus_holder.has_value()) {
                    return generateShowdownActions(*state_.turn.focus_holder);
                }
                return generateShowdownActions(player);
            }
            break;
        default:
            break;
    }

    return {};
}

std::vector<Intent> GameEngine::generateMainPhaseActions(PlayerId player) const {
    std::vector<Intent> actions;

    // Always can end turn
    actions.push_back(Intent::endTurn(player));

    // Standard Move: exhaust ready units to move them
    auto all_units = state_.allUnitsControlledBy(player);
    for (auto unit_id : all_units) {
        auto& unit = state_.getObject(unit_id);
        if (unit.is_exhausted) continue; // must be ready

        if (unit.isAtBase()) {
            // Can move from base to any battlefield (CR 144.4.a)
            for (auto& bf : state_.battlefields) {
                // Can't move to BF with units from 2 other players (CR 144.4.a.1)
                // In 1v1, this is never an issue
                LocationId dest = BattlefieldLocation{bf.id};
                actions.push_back(Intent::standardMove(
                    player, {unit_id}, dest));
            }
        } else if (unit.isAtBattlefield()) {
            // Can always move back to base (CR 144.4.b)
            actions.push_back(Intent::standardMove(
                player, {unit_id}, BaseLocation{player}));

            // Ganking: can move battlefield to battlefield (CR 144.4.c)
            if (unit.hasKeyword(Keyword::Ganking)) {
                for (auto& bf : state_.battlefields) {
                    auto unit_bf = unit.battlefieldId();
                    if (unit_bf && *unit_bf != bf.id) {
                        actions.push_back(Intent::standardMove(
                            player, {unit_id}, BattlefieldLocation{bf.id}));
                    }
                }
            }
        }
    }

    // Play units from hand — must be able to afford the cost.
    auto& ps = state_.player(player);
    const bool locked_out = ps.cant_play_cards_this_turn;  // Brynhir Thundersong
    for (auto card_id : ps.hand) {
        if (locked_out) break;  // skip all play-from-hand actions
        auto& card = state_.getObject(card_id);
        if (!card.isUnit()) continue;
        if (card.card_def_id == kInvalidId) continue;  // tokens have no CardDef
        if (!canAfford(player, card_id)) continue;

        // Check play-to-location abilities from ability_text
        const auto& def = card_db_.get(card.card_def_id);
        auto& text = def.ability_text;
        bool can_play_open_bf = (text.find("play me to an open battlefield") != std::string::npos);
        bool can_play_enemy_bf = (text.find("play me to an occupied enemy battlefield") != std::string::npos);
        bool can_play_any_bf = (text.find("play me to a battlefield") != std::string::npos ||
                                text.find("play me at a battlefield") != std::string::npos);

        // Can always play to base (CR 355.2.a)
        Intent play_intent;
        play_intent.type = IntentType::PlayCard;
        play_intent.player = player;
        play_intent.card = card_id;
        play_intent.play_location = BaseLocation{player};
        actions.push_back(play_intent);

        for (auto& bf : state_.battlefields) {
            bool controlled = bf.controller.has_value() && *bf.controller == player;
            bool open = !bf.controller.has_value();
            bool enemy_occupied = bf.controller.has_value() && *bf.controller != player &&
                                  bf.hasUnitsFrom(opponent(player), state_.objects);

            bool can_play_here = controlled; // default: controlled BFs
            if (can_play_open_bf && open) can_play_here = true;
            if (can_play_enemy_bf && enemy_occupied) can_play_here = true;
            if (can_play_any_bf) can_play_here = true;

            if (can_play_here) {
                Intent bf_play = play_intent;
                bf_play.play_location = BattlefieldLocation{bf.id};
                actions.push_back(bf_play);
            }
        }
    }

    // Play champion from champion zone (also must afford)
    if (ps.champion_zone != kInvalidId) {
        auto& champ = state_.getObject(ps.champion_zone);
        if (champ.zone == ZoneType::ChampionZone &&
            canAfford(player, ps.champion_zone)) {
            Intent play_intent;
            play_intent.type = IntentType::PlayCard;
            play_intent.player = player;
            play_intent.card = ps.champion_zone;
            play_intent.play_location = BaseLocation{player};
            actions.push_back(play_intent);
        }
    }

    // Play gear from hand (to base only, CR 149.2)
    for (auto card_id : ps.hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isGear()) continue;
        if (!canAfford(player, card_id)) continue;

        Intent play_intent;
        play_intent.type = IntentType::PlayCard;
        play_intent.player = player;
        play_intent.card = card_id;
        play_intent.play_location = BaseLocation{player};
        actions.push_back(play_intent);
    }

    // Play spells from hand — Neutral Open allows any spell (Action or not).
    // Brynhir lockout suppresses card plays but not activated abilities.
    if (!locked_out) {
        generateSpellActions(player, /*action_ok=*/true, /*reaction_ok=*/true,
                              actions);
    }

    // Activate abilities on gear/legends/units ([E]: abilities)
    generateActivateAbilityActions(player, actions);

    // Equip gear to units (CR 818)
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isGear() || obj.controller != player) continue;
        if (!obj.location.has_value()) continue;
        if (obj.attached_to.has_value()) continue; // already attached

        Card* gear_card = card_registry_.get(obj.card_def_id);
        if (!gear_card || !gear_card->hasEquipAbility()) continue;

        // Find friendly units on the board as equip targets
        for (auto& [uid, unit] : state_.objects) {
            if (!unit.isUnit() || unit.controller != player) continue;
            if (!unit.location.has_value()) continue;

            Intent equip;
            equip.type = IntentType::ActivateAbility;
            equip.player = player;
            equip.ability_source = id;
            equip.targets = {uid};
            actions.push_back(equip);
        }
    }

    // Hide cards with [Hidden] keyword facedown at controlled battlefields (CR 811)
    for (auto card_id : ps.hand) {
        auto& card = state_.getObject(card_id);
        if (!card.keywords.has(Keyword::Hidden)) continue;

        // Need at least one rune to pay [A] cost
        bool has_rune = false;
        auto base_loc = BaseLocation{player};
        for (auto& [id, obj] : state_.objects) {
            if (obj.isRune() && obj.controller == player && !obj.is_exhausted &&
                obj.location.has_value() && *obj.location == LocationId{base_loc}) {
                has_rune = true;
                break;
            }
        }
        if (!has_rune) continue;

        // Can hide at any controlled BF with available facedown slot
        for (auto& bf : state_.battlefields) {
            if (!bf.controller.has_value() || *bf.controller != player) continue;
            if (static_cast<int>(bf.facedown.size()) >= bf.facedown_max_occupancy) continue;

            Intent hide;
            hide.type = IntentType::HideCard;
            hide.player = player;
            hide.card = card_id;
            hide.chosen_battlefield = bf.id;
            actions.push_back(hide);
        }
    }

    return actions;
}

std::vector<Intent> GameEngine::generateMulliganActions(PlayerId player) const {
    std::vector<Intent> actions;
    auto& ps = state_.player(player);

    // Can mulligan 0 cards (keep hand)
    actions.push_back(Intent::mulligan(player, {}));

    // Can mulligan 1 card
    for (auto card_id : ps.hand) {
        actions.push_back(Intent::mulligan(player, {card_id}));
    }

    // Can mulligan 2 cards
    for (size_t i = 0; i < ps.hand.size(); ++i) {
        for (size_t j = i + 1; j < ps.hand.size(); ++j) {
            actions.push_back(Intent::mulligan(
                player, {ps.hand[i], ps.hand[j]}));
        }
    }

    return actions;
}

std::vector<Intent> GameEngine::generateShowdownActions(PlayerId player) const {
    std::vector<Intent> actions;

    // Can always pass focus
    actions.push_back(Intent::passFocus(player));

    const bool locked_out = state_.player(player).cant_play_cards_this_turn;

    // Can play Action or Reaction spells during showdowns (CR 806, 813).
    // Lockout (Brynhir Thundersong) suppresses card plays but not abilities.
    if (!locked_out) {
        generateSpellActions(player, /*action_ok=*/true, /*reaction_ok=*/true,
                              actions);
    }

    // Ambush: play units with [Ambush] during showdowns
    for (auto card_id : state_.player(player).hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isUnit() || !card.hasKeyword(Keyword::Ambush)) continue;
        if (!canAfford(player, card_id)) continue;

        for (auto& bf : state_.battlefields) {
            if (!bf.hasUnitsFrom(player, state_.objects)) continue;
            Intent ambush;
            ambush.type = IntentType::PlayActionCard;
            ambush.player = player;
            ambush.card = card_id;
            ambush.play_location = BattlefieldLocation{bf.id};
            actions.push_back(ambush);
        }
    }

    // Reaction-to-attack plays (e.g., Rengar, Pouncing — play to a BF where
    // you are currently attacking).
    for (auto card_id : state_.player(player).hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isUnit()) continue;
        if (card.card_def_id == kInvalidId) continue;
        Card* card_obj = card_registry_.get(card.card_def_id);
        if (!card_obj || !card_obj->playableAsReactionToAttack()) continue;
        if (!canAfford(player, card_id)) continue;

        for (auto& bf : state_.battlefields) {
            if (!bf.combat_in_progress) continue;
            if (!bf.attacker.has_value() || *bf.attacker != player) continue;
            Intent pounce;
            pounce.type = IntentType::PlayReaction;
            pounce.player = player;
            pounce.card = card_id;
            pounce.play_location = BattlefieldLocation{bf.id};
            actions.push_back(pounce);
        }
    }

    // Can activate [E]: abilities with [Action] timing during showdowns (CR 806)
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != player) continue;
        if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;

        Card* card = card_registry_.get(obj.card_def_id);
        if (!card || !card->hasActivatedAbility()) continue;
        if (!card->isActionAbility()) continue;
        auto act_cost = card->getActivationCost();
        if (act_cost.exhaust && obj.is_exhausted) continue;

        auto legal_targets = card->enumerateLegalTargets(state_, player);
        auto req = card->getTargetRequirements();

        if (req.count > 0 && legal_targets.empty() && !req.optional) continue;

        if (req.count == 0) {
            Intent activate;
            activate.type = IntentType::ActivateActionAbility;
            activate.player = player;
            activate.ability_source = id;
            actions.push_back(activate);
        } else {
            for (auto target : legal_targets) {
                Intent activate;
                activate.type = IntentType::ActivateActionAbility;
                activate.player = player;
                activate.ability_source = id;
                activate.targets = {target};
                actions.push_back(activate);
            }
        }
    }

    return actions;
}

std::vector<Intent> GameEngine::generateClosedStateActions(
    PlayerId player) const {
    std::vector<Intent> actions;

    // Can always pass priority
    actions.push_back(Intent::passPriority(player));

    const bool locked_out = state_.player(player).cant_play_cards_this_turn;

    // Can only play Reaction spells in Closed State (CR 309.1.a).
    // Brynhir lockout suppresses these.
    if (!locked_out) {
        generateSpellActions(player, /*action_ok=*/false, /*reaction_ok=*/true,
                              actions);
    }

    // Quick-Draw: play gear with [Quick-Draw] as Reactions targeting a friendly unit
    for (auto card_id : state_.player(player).hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isGear() || !card.hasKeyword(Keyword::QuickDraw)) continue;
        if (!canAfford(player, card_id)) continue;

        for (auto& [uid, unit] : state_.objects) {
            if (!unit.isUnit() || unit.controller != player) continue;
            if (!unit.location.has_value()) continue;
            Intent qd;
            qd.type = IntentType::PlayReaction;
            qd.player = player;
            qd.card = card_id;
            qd.targets = {uid}; // target unit to auto-attach
            actions.push_back(qd);
        }
    }

    // Ambush: play units with [Ambush] as Reactions to BFs where you have units
    for (auto card_id : state_.player(player).hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isUnit() || !card.hasKeyword(Keyword::Ambush)) continue;
        if (!canAfford(player, card_id)) continue;

        for (auto& bf : state_.battlefields) {
            if (!bf.hasUnitsFrom(player, state_.objects)) continue;
            Intent ambush;
            ambush.type = IntentType::PlayReaction;
            ambush.player = player;
            ambush.card = card_id;
            ambush.play_location = BattlefieldLocation{bf.id};
            actions.push_back(ambush);
        }
    }

    // Reaction-to-attack plays during Closed State (Rengar, Pouncing).
    for (auto card_id : state_.player(player).hand) {
        if (locked_out) break;
        auto& card = state_.getObject(card_id);
        if (!card.isUnit()) continue;
        if (card.card_def_id == kInvalidId) continue;
        Card* card_obj = card_registry_.get(card.card_def_id);
        if (!card_obj || !card_obj->playableAsReactionToAttack()) continue;
        if (!canAfford(player, card_id)) continue;

        for (auto& bf : state_.battlefields) {
            if (!bf.combat_in_progress) continue;
            if (!bf.attacker.has_value() || *bf.attacker != player) continue;
            Intent pounce;
            pounce.type = IntentType::PlayReaction;
            pounce.player = player;
            pounce.card = card_id;
            pounce.play_location = BattlefieldLocation{bf.id};
            actions.push_back(pounce);
        }
    }

    // Can play hidden cards (they gain Reaction on next turn after hiding)
    for (auto& bf : state_.battlefields) {
        for (auto card_id : bf.facedown) {
            if (!state_.objectExists(card_id)) continue;
            auto& card = state_.getObject(card_id);
            if (card.controller != player) continue;
            // Must be hidden on a previous turn to gain Reaction
            if (card.hidden_on_turn >= state_.turn.turn_number) continue;

            Intent play;
            play.type = IntentType::PlayReaction;
            play.player = player;
            play.card = card_id;
            actions.push_back(play);
        }
    }

    return actions;
}

void GameEngine::generateSpellActions(PlayerId player, bool action_ok,
                                       bool reaction_ok,
                                       std::vector<Intent>& actions) const {
    auto& ps = state_.player(player);
    for (auto card_id : ps.hand) {
        auto& card = state_.getObject(card_id);
        if (!card.isSpell()) continue;
        if (!canAfford(player, card_id)) continue;

        // Check timing keywords
        bool has_action = card.keywords.has(Keyword::Action);
        bool has_reaction = card.keywords.has(Keyword::Reaction);

        // Reaction grants all Action permissions too (CR 813.1.b)
        if (has_reaction) has_action = true;

        // In Neutral Open, any spell can be played (no timing restriction)
        // In Showdown, need Action or Reaction
        // In Closed State, need Reaction only
        bool allowed = false;
        if (state_.turn.isNeutralOpen()) {
            allowed = true; // any spell
        } else if (state_.turn.isShowdownOpen()) {
            allowed = has_action || has_reaction;
        } else if (state_.turn.isClosedState()) {
            allowed = has_reaction;
            if (!reaction_ok) allowed = false;
        } else {
            if (action_ok && has_action) allowed = true;
            if (reaction_ok && has_reaction) allowed = true;
        }

        if (!allowed) continue;

        // Get legal targets for this spell via Card object
        Card* spell_card = card_registry_.get(card.card_def_id);
        auto legal_targets = spell_card
            ? spell_card->enumerateLegalTargets(state_, player)
            : std::vector<GameObjectId>{};
        auto req = spell_card
            ? spell_card->getTargetRequirements()
            : TargetRequirements{};

        if (req.count > 0 && legal_targets.empty() && !req.optional) {
            continue; // spell needs targets but none exist
        }

        // Determine intent type based on context
        IntentType intent_type = IntentType::PlayCard;
        if (state_.turn.isShowdownOpen()) {
            intent_type = IntentType::PlayActionCard;
        } else if (state_.turn.isClosedState()) {
            intent_type = IntentType::PlayReaction;
        }

        if (req.count == 0) {
            // No targeting needed
            Intent play;
            play.type = intent_type;
            play.player = player;
            play.card = card_id;
            actions.push_back(play);
        } else if (req.optional && legal_targets.empty()) {
            // Optional targeting with no targets — play without targets
            Intent play;
            play.type = intent_type;
            play.player = player;
            play.card = card_id;
            actions.push_back(play);
        } else if (req.count == 2) {
            // Dual targeting (e.g., Challenge: friendly + enemy)
            // Separate targets into friendly and enemy
            std::vector<GameObjectId> friendly_targets, enemy_targets;
            for (auto tid : legal_targets) {
                auto& t = state_.getObject(tid);
                if (t.controller == player) friendly_targets.push_back(tid);
                else enemy_targets.push_back(tid);
            }
            // Generate all valid pairs
            for (auto ft : friendly_targets) {
                for (auto et : enemy_targets) {
                    Intent play;
                    play.type = intent_type;
                    play.player = player;
                    play.card = card_id;
                    play.targets = {ft, et};
                    actions.push_back(play);
                }
            }
        } else {
            // Generate one intent per legal target
            for (auto target : legal_targets) {
                Intent play;
                play.type = intent_type;
                play.player = player;
                play.card = card_id;
                play.targets = {target};
                actions.push_back(play);
            }
        }
    }
}

void GameEngine::generateActivateAbilityActions(PlayerId player,
                                                  std::vector<Intent>& actions) const {
    // Find all objects on board with activated abilities
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != player) continue;
        if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;

        Card* card = card_registry_.get(obj.card_def_id);
        if (!card || !card->hasActivatedAbility()) continue;

        // Check activation cost: must be ready if exhaust required
        auto act_cost = card->getActivationCost();
        if (act_cost.exhaust && obj.is_exhausted) continue;

        // Check energy cost affordability
        if (act_cost.energy > 0) {
            if (availableEnergy(player) < act_cost.energy) continue;
        }
        // Check discard cost affordability
        if (act_cost.discard && act_cost.discard_count > 0) {
            if (static_cast<int>(state_.player(player).hand.size()) < act_cost.discard_count) continue;
        }

        // Get legal targets for the ability's effects
        auto legal_targets = card->enumerateLegalTargets(state_, player);
        auto req = card->getTargetRequirements();

        if (req.count > 0 && legal_targets.empty() && !req.optional) continue;

        if (req.count == 0) {
            Intent activate;
            activate.type = IntentType::ActivateAbility;
            activate.player = player;
            activate.ability_source = id;
            actions.push_back(activate);
        } else {
            for (auto target : legal_targets) {
                Intent activate;
                activate.type = IntentType::ActivateAbility;
                activate.player = player;
                activate.ability_source = id;
                activate.targets = {target};
                actions.push_back(activate);
            }
        }
    }
}

std::vector<Intent> GameEngine::generateCombatDamageActions(
    PlayerId player) const {
    std::vector<Intent> actions;
    // Phase 1: simplified — assign all damage to one unit at a time,
    // obeying lethal-before-next rule (CR 460.2.c.3)
    // For now, generate a single "spread evenly" assignment
    // TODO: Full damage assignment enumeration

    // Find the battlefield with active combat
    for (auto& bf : state_.battlefields) {
        if (!bf.combat_in_progress) continue;

        PlayerId enemy = opponent(player);
        auto enemy_units = state_.unitsAt(
            BattlefieldLocation{bf.id}, enemy);
        auto my_units = state_.unitsAt(
            BattlefieldLocation{bf.id}, player);

        if (enemy_units.empty() || my_units.empty()) continue;

        // Sum my might
        int total_might = 0;
        for (auto uid : my_units) {
            total_might += state_.getObject(uid).current_might;
        }

        // Generate one assignment: kill units in order until damage runs out
        std::vector<DamageAssignment> assignments;
        int remaining = total_might;
        for (auto target_id : enemy_units) {
            if (remaining <= 0) break;
            auto& target = state_.getObject(target_id);
            int lethal = target.current_might - target.damage_marked;
            if (lethal <= 0) continue;
            int assigned = std::min(remaining, lethal);
            assignments.push_back({target_id, assigned});
            remaining -= assigned;
        }
        // Dump remaining damage on last unit
        if (remaining > 0 && !assignments.empty()) {
            assignments.back().damage += remaining;
        }

        actions.push_back(Intent::assignCombatDamage(player, assignments));
        break;
    }

    return actions;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Combat and Showdown
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::runShowdown(BattlefieldId bf_id) {
    auto& bf = getBattlefield(bf_id);
    events_.logTrace("SHOWDOWN: BF#" + std::to_string(bf_id) +
                     (bf.combat_staged ? " (combat)" : " (non-combat)"));

    bf.showdown_in_progress = true;
    state_.turn.ns_state = NeutralShowdownState::Showdown;
    state_.turn.oc_state = OpenClosedState::Open;

    events_.emit(ShowdownStartedEvent{bf_id, bf.combat_staged});

    // Showdown loop: players alternate focus, playing Action/Reaction spells.
    // Showdown closes when all players pass focus consecutively (CR 348.1).
    runShowdownLoop(bf_id);

    bf.showdown_in_progress = false;
    state_.turn.ns_state = NeutralShowdownState::Neutral;
    state_.turn.oc_state = OpenClosedState::Open;
    events_.emit(ShowdownEndedEvent{bf_id});

    // Non-combat showdown resolution: if only one player has units,
    // they establish control (CR 348.2.a)
    auto p1_units = state_.unitsAt(BattlefieldLocation{bf_id}, PlayerId::Player1);
    auto p2_units = state_.unitsAt(BattlefieldLocation{bf_id}, PlayerId::Player2);

    PlayerId sole_player = PlayerId::None;
    if (!p1_units.empty() && p2_units.empty()) sole_player = PlayerId::Player1;
    if (p1_units.empty() && !p2_units.empty()) sole_player = PlayerId::Player2;

    if (sole_player != PlayerId::None) {
        auto old_controller = bf.controller;
        bf.controller = sole_player;
        bf.is_contested = false;

        events_.emit(ControlChangedEvent{bf_id, old_controller, sole_player});

        // Conquer if not already scored this turn (CR 464.1)
        scoreConquer(sole_player, bf_id);
    }
}

void GameEngine::runShowdownLoop(BattlefieldId bf_id) {
    // Focus passing loop (CR 313, CR 348.1).
    // Turn player gets focus first. Players alternate.
    // When both pass focus consecutively without adding chain items, showdown closes.
    state_.turn.players_passed_focus.clear();
    PlayerId current_focus = state_.turn.turn_player;

    constexpr int kMaxShowdownActions = 100; // safety
    int action_count = 0;

    while (action_count < kMaxShowdownActions && !state_.game_over) {
        state_.turn.focus_holder = current_focus;
        state_.turn.priority_holder = current_focus;
        state_.turn.oc_state = OpenClosedState::Open;

        events_.emit(PriorityGrantedEvent{current_focus, true});

        auto actions = generateShowdownActions(current_focus);
        state_.decision_index++;
        auto chosen = getAgent(current_focus).selectAction(state_, actions);
        if (on_decision) {
            on_decision(state_, actions, chosen);
        }

        if (chosen.type == IntentType::PassFocus) {
            state_.turn.players_passed_focus.insert(current_focus);

            // Check if all players passed focus
            bool all_passed = true;
            for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
                if (state_.turn.players_passed_focus.find(pid) ==
                    state_.turn.players_passed_focus.end()) {
                    all_passed = false;
                    break;
                }
            }

            if (all_passed) return; // showdown closes

            current_focus = opponent(current_focus);
        } else if (chosen.type == IntentType::PlayActionCard) {
            // Player played a spell during showdown
            state_.turn.players_passed_focus.clear(); // reset passes
            executeIntent(chosen);
            cleanup();
            // After chain resolves, focus passes to opponent
            current_focus = opponent(current_focus);
        }

        action_count++;
    }
}

void GameEngine::runCombat(BattlefieldId bf_id) {
    auto& bf = getBattlefield(bf_id);
    bf.combat_in_progress = true;

    // Determine attacker/defender (CR 459.2.b)
    bf.attacker = bf.contested_by;
    bf.defender = opponent(bf.contested_by);
    events_.logTrace("COMBAT: BF#" + std::to_string(bf_id) + " attacker=" +
                     toString(*bf.attacker) + " defender=" + toString(*bf.defender));

    // Assign combat designations to units
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.isAtBattlefield()) continue;
        auto unit_bf = obj.battlefieldId();
        if (!unit_bf || *unit_bf != bf_id) continue;

        if (obj.controller == *bf.attacker) {
            obj.combat_designation = CombatDesignation::Attacker;
        } else if (obj.controller == *bf.defender) {
            obj.combat_designation = CombatDesignation::Defender;
        }
    }

    // Recompute might (assault/shield apply based on designation)
    for (auto& [id, obj] : state_.objects) {
        if (obj.isUnit() && obj.combat_designation != CombatDesignation::None) {
            obj.recomputeMight();
        }
    }

    events_.emit(CombatStartedEvent{bf_id, *bf.attacker, *bf.defender});

    // Step 1: Combat Showdown — players can play Action/Reaction spells
    bf.combat_phase = CombatPhase::ShowdownStep;
    runShowdownLoop(bf_id);

    // After showdown: check if combat can continue (CR 460.1)
    // Damage step only occurs if both sides still have units
    auto att_check = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.attacker);
    auto def_check = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.defender);

    if (att_check.empty() || def_check.empty()) {
        // One side was eliminated during showdown — skip damage, go to resolution
        bf.combat_phase = CombatPhase::ResolutionStep;
        combatResolutionStep(bf_id);
        return;
    }

    // Step 2: Combat Damage
    bf.combat_phase = CombatPhase::DamageStep;
    combatDamageStep(bf_id);

    // Step 3: Resolution
    bf.combat_phase = CombatPhase::ResolutionStep;
    combatResolutionStep(bf_id);
}

void GameEngine::combatDamageStep(BattlefieldId bf_id) {
    auto& bf = getBattlefield(bf_id);

    auto att_units = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.attacker);
    auto def_units = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.defender);

    if (att_units.empty() || def_units.empty()) return;

    {
        std::string att_str, def_str;
        for (auto uid : att_units) {
            auto& u = state_.getObject(uid);
            if (!att_str.empty()) att_str += ", ";
            att_str += u.name + "(" + std::to_string(u.current_might) + "M" +
                       (u.is_stunned ? ",STUNNED" : "") + ")";
        }
        for (auto uid : def_units) {
            auto& u = state_.getObject(uid);
            if (!def_str.empty()) def_str += ", ";
            def_str += u.name + "(" + std::to_string(u.current_might) + "M" +
                       (u.is_stunned ? ",STUNNED" : "") + ")";
        }
        events_.logTrace("DAMAGE_STEP: BF#" + std::to_string(bf_id) +
                         " ATK=[" + att_str + "] DEF=[" + def_str + "]");
    }

    // Reorder targets: Tank first, Backline last (CR 460.2.c)
    auto sortByTankBackline = [&](std::vector<GameObjectId>& units) {
        std::stable_sort(units.begin(), units.end(),
            [&](GameObjectId a, GameObjectId b) {
                auto& oa = state_.getObject(a);
                auto& ob = state_.getObject(b);
                bool a_tank = oa.hasKeyword(Keyword::Tank);
                bool b_tank = ob.hasKeyword(Keyword::Tank);
                bool a_back = oa.hasKeyword(Keyword::Backline);
                bool b_back = ob.hasKeyword(Keyword::Backline);
                // Tank before non-Tank, non-Backline before Backline
                if (a_tank != b_tank) return a_tank > b_tank;
                if (a_back != b_back) return a_back < b_back;
                return false;
            });
    };
    sortByTankBackline(att_units);
    sortByTankBackline(def_units);

    // Sum attacker might (CR 460.2.a) — stunned units contribute 0 (CR 423)
    int att_might = 0;
    for (auto uid : att_units) {
        auto& u = state_.getObject(uid);
        if (!u.is_stunned) att_might += u.current_might;
    }

    // Sum defender might (CR 460.2.b)
    int def_might = 0;
    for (auto uid : def_units) {
        auto& u = state_.getObject(uid);
        if (!u.is_stunned) def_might += u.current_might;
    }

    // Both players assign damage to opponent's units (CR 460.2.c)
    // Tank must be assigned lethal before non-Tank. Backline assigned last.
    // Agent chooses distribution within those constraints.
    auto queryDamageAssignment = [&](PlayerId assigner, int total_damage,
                                     const std::vector<GameObjectId>& targets) {
        if (total_damage <= 0 || targets.empty()) return;

        // Build default assignment (greedy lethal, respecting Tank/Backline order)
        auto buildDefault = [&]() -> std::vector<DamageAssignment> {
            std::vector<DamageAssignment> da;
            int remaining = total_damage;
            for (auto tid : targets) {
                if (remaining <= 0) break;
                auto& t = state_.getObject(tid);
                int lethal = std::max(1, t.current_might - t.damage_marked);
                int assigned = std::min(remaining, lethal);
                da.push_back({tid, assigned});
                remaining -= assigned;
            }
            if (remaining > 0 && !da.empty()) {
                da.back().damage += remaining;
            }
            return da;
        };

        // Query agent for damage assignment
        std::vector<Intent> options;
        // Option 0: default greedy-lethal
        options.push_back(Intent::assignCombatDamage(assigner, buildDefault()));

        // Generate alternative distributions for the agent to choose from:
        if (targets.size() > 1) {
            // Option: spread damage evenly
            {
                std::vector<DamageAssignment> spread;
                int per = total_damage / static_cast<int>(targets.size());
                int extra = total_damage % static_cast<int>(targets.size());
                for (size_t i = 0; i < targets.size(); ++i) {
                    int dmg = per + (static_cast<int>(i) < extra ? 1 : 0);
                    if (dmg > 0) spread.push_back({targets[i], dmg});
                }
                if (!spread.empty())
                    options.push_back(Intent::assignCombatDamage(assigner, std::move(spread)));
            }

            // Option: all damage on first target (overkill for guaranteed kill)
            {
                std::vector<DamageAssignment> focus;
                focus.push_back({targets[0], total_damage});
                options.push_back(Intent::assignCombatDamage(assigner, std::move(focus)));
            }

            // Option: lethal on first, rest on second
            if (targets.size() >= 2) {
                auto& t0 = state_.getObject(targets[0]);
                int lethal0 = std::max(1, t0.current_might - t0.damage_marked);
                if (lethal0 < total_damage) {
                    std::vector<DamageAssignment> split;
                    split.push_back({targets[0], lethal0});
                    split.push_back({targets[1], total_damage - lethal0});
                    options.push_back(Intent::assignCombatDamage(assigner, std::move(split)));
                }
            }
        }

        // Query the agent
        state_.decision_index++;
        auto chosen = getAgent(assigner).selectAction(state_, options);
        if (on_decision) {
            on_decision(state_, options, chosen);
        }

        // Log and apply chosen assignment
        events_.logTrace(std::string("ASSIGN_DAMAGE: ") + toString(assigner) +
                         " assigns " + std::to_string(total_damage) + " damage:");
        for (auto& da : chosen.damage_assignments) {
            if (da.damage > 0) {
                auto& t = state_.getObject(da.target_unit);
                events_.logTrace("  -> " + t.name + " takes " +
                                 std::to_string(da.damage) + " (was " +
                                 std::to_string(t.damage_marked) + "/" +
                                 std::to_string(t.current_might) + "M)");
                t.damage_marked += da.damage;
                events_.emit(DamageDealtEvent{da.target_unit, da.damage, kInvalidId, true});
            }
        }
    };

    // Attacker assigns damage to defender's units, defender assigns to attacker's
    queryDamageAssignment(*bf.attacker, att_might, def_units);
    queryDamageAssignment(*bf.defender, def_might, att_units);
}

void GameEngine::combatResolutionStep(BattlefieldId bf_id) {
    auto& bf = getBattlefield(bf_id);

    // Combat Cleanup: heal all units, kill lethally damaged (CR 461.1.a)
    processLethalDamage();

    // Heal surviving units (CR 461.1.a.1)
    for (auto& [id, obj] : state_.objects) {
        if (obj.isUnit() && obj.isAtBattlefield()) {
            auto unit_bf = obj.battlefieldId();
            if (unit_bf && *unit_bf == bf_id) {
                obj.damage_marked = 0;
            }
        }
    }

    // Recall attackers if defenders still present (CR 461.1.a.2)
    auto att_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.attacker);
    auto def_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.defender);

    if (!att_remaining.empty() && !def_remaining.empty()) {
        // Both still have units — recall attackers
        for (auto uid : att_remaining) {
            moveUnit(uid, BaseLocation{*bf.attacker});
            events_.emit(UnitMovedEvent{uid, *bf.attacker,
                BattlefieldLocation{bf_id}, BaseLocation{*bf.attacker}, false});
        }
    }

    // Determine combat result (CR 461.3)
    att_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.attacker);
    def_remaining = state_.unitsAt(BattlefieldLocation{bf_id}, *bf.defender);

    PlayerId combat_winner = PlayerId::None;
    if (!att_remaining.empty() && def_remaining.empty()) {
        combat_winner = *bf.attacker;
    } else if (att_remaining.empty() && !def_remaining.empty()) {
        combat_winner = *bf.defender;
    }

    // Clear combat designations
    for (auto& [id, obj] : state_.objects) {
        if (obj.combat_designation != CombatDesignation::None) {
            obj.combat_designation = CombatDesignation::None;
            obj.recomputeMight();
        }
    }

    // End combat
    bf.combat_in_progress = false;
    bf.combat_phase = CombatPhase::None;
    bf.is_contested = false;
    bf.attacker = std::nullopt;
    bf.defender = std::nullopt;

    events_.emit(CombatEndedEvent{bf_id, combat_winner});

    // Establish control (CR 461.5)
    if (combat_winner != PlayerId::None) {
        auto old_controller = bf.controller;
        bf.controller = combat_winner;
        events_.emit(ControlChangedEvent{bf_id, old_controller, combat_winner});
        scoreConquer(combat_winner, bf_id);
    } else if (att_remaining.empty() && def_remaining.empty()) {
        bf.controller = std::nullopt;
    }

    state_.turn.ns_state = NeutralShowdownState::Neutral;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Scoring
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::scoreConquer(PlayerId player, BattlefieldId bf) {
    auto& ps = state_.player(player);

    // Can only score once per BF per turn (CR 465)
    if (ps.battlefields_scored_this_turn.count(bf)) return;

    ps.battlefields_scored_this_turn.insert(bf);
    events_.logTrace(std::string("CONQUER: ") + toString(player) + " conquers BF#" +
                     std::to_string(bf) + " (score was " + std::to_string(ps.score) + ")");

    if (isWinningPointAttempt(player)) {
        if (canGainWinningPointViaConquer(player)) {
            ps.score++;
            events_.logTrace(std::string("SCORE: ") + toString(player) + " +1 via Conquer -> " +
                             std::to_string(ps.score));
            events_.emit(ScoreEvent{player, bf, ScoreMethod::Conquer, ps.score});
        } else {
            events_.logTrace(std::string("SCORE_BLOCKED: ") + toString(player) +
                             " winning point blocked (not all BFs scored), draw 1 instead");
            // Draw a card instead (CR 466.1.b.2)
            drawCards(player, 1);
        }
    } else {
        ps.score++;
        events_.logTrace(std::string("SCORE: ") + toString(player) + " +1 via Conquer -> " +
                         std::to_string(ps.score));
        events_.emit(ScoreEvent{player, bf, ScoreMethod::Conquer, ps.score});
    }

    checkWinCondition();
}

void GameEngine::scoreHold(PlayerId player, BattlefieldId bf) {
    auto& ps = state_.player(player);

    if (ps.battlefields_scored_this_turn.count(bf)) return;
    ps.battlefields_scored_this_turn.insert(bf);

    // Hold always grants Winning Point (CR 466.1.b.1)
    ps.score++;
    events_.logTrace(std::string("SCORE: ") + toString(player) + " +1 via Hold BF#" +
                     std::to_string(bf) + " -> " + std::to_string(ps.score));
    events_.emit(ScoreEvent{player, bf, ScoreMethod::Hold, ps.score});

    checkWinCondition();
}

bool GameEngine::isWinningPointAttempt(PlayerId player) const {
    return state_.player(player).score >= state_.mode.victory_score - 1;
}

bool GameEngine::canGainWinningPointViaConquer(PlayerId player) const {
    // Must have scored every battlefield this turn (CR 466.1.b.2)
    return state_.player(player).battlefields_scored_this_turn.size() >=
           state_.totalBattlefields();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Aura recalculation (Phase 5b — tagged effects)
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::recalculateAuras() {
    // Step 1: Clear all aura effects from all objects
    for (auto& [id, obj] : state_.objects) {
        obj.aura_effects.clear();
        obj.aura_might_bonus = 0;
        obj.aura_keywords.reset();
    }

    // Step 2: Scan all objects on board for aura abilities
    for (auto& [src_id, src] : state_.objects) {
        if (!src.location.has_value() && src.zone != ZoneType::LegendZone) continue;
        if (src.card_def_id == kInvalidId) continue; // skip tokens without CardDef

        const auto& def = card_db_.get(src.card_def_id);
        auto& text = def.ability_text;
        if (text.empty()) continue;

        // Parse aura from ability_text (lowercase matching)
        std::string lower;
        lower.reserve(text.size());
        for (char c : text) lower += std::tolower(c);

        // Strip parenthetical for matching
        std::string clean;
        int depth = 0;
        for (char c : lower) {
            if (c == '(') depth++;
            else if (c == ')') { if (depth > 0) depth--; }
            else if (depth == 0) clean += c;
        }

        // Determine aura scope and effect
        struct AuraDef {
            bool is_keyword = false;
            Keyword keyword = Keyword::Count;
            int keyword_value = 0;
            int might_mod = 0;
            int might_min = 0;
            bool friendly = false;
            bool enemy = false;
            bool here_only = false;    // "here" = same battlefield as source
            bool other_only = false;   // "other" = excludes self
            bool all_friendly = false; // all friendly units anywhere
            bool tag_filter = false;
            std::string required_tag;
            bool stunned_only = false;
        };

        std::vector<AuraDef> auras;

        // "Other friendly units here have [Assault]"
        auto findAura = [&](const std::string& pattern, AuraDef base) -> bool {
            auto pos = clean.find(pattern);
            if (pos == std::string::npos) return false;
            auto clause = clean.substr(pos, 80);

            // Determine keyword or might
            if (clause.find("[assault]") != std::string::npos) {
                base.is_keyword = true; base.keyword = Keyword::Assault; base.keyword_value = 1;
            } else if (clause.find("[shield]") != std::string::npos) {
                base.is_keyword = true; base.keyword = Keyword::Shield; base.keyword_value = 1;
            } else if (clause.find("[ganking]") != std::string::npos) {
                base.is_keyword = true; base.keyword = Keyword::Ganking;
            } else if (clause.find("[vision]") != std::string::npos) {
                base.is_keyword = true; base.keyword = Keyword::Vision;
            } else if (clause.find("[deflect]") != std::string::npos) {
                base.is_keyword = true; base.keyword = Keyword::Deflect; base.keyword_value = 1;
            } else if (clause.find("+") != std::string::npos && clause.find("[m]") != std::string::npos) {
                // Extract +N
                auto plus = clause.find('+');
                int val = 0;
                for (size_t i = plus + 1; i < clause.size() && std::isdigit(clause[i]); ++i)
                    val = val * 10 + (clause[i] - '0');
                if (val == 0) val = 1;
                base.might_mod = val;
            } else if (clause.find("-") != std::string::npos && clause.find("[m]") != std::string::npos) {
                auto minus = clause.find('-');
                int val = 0;
                for (size_t i = minus + 1; i < clause.size() && std::isdigit(clause[i]); ++i)
                    val = val * 10 + (clause[i] - '0');
                if (val == 0) val = 1;
                base.might_mod = -val;
                // Check for minimum
                auto min_pos = clause.find("minimum of");
                if (min_pos != std::string::npos) {
                    int m = 0;
                    for (size_t i = min_pos + 10; i < clause.size() && std::isdigit(clause[i]); ++i)
                        m = m * 10 + (clause[i] - '0');
                    if (m == 0) m = 1;
                    base.might_min = m;
                }
            } else {
                return false; // unrecognized aura effect
            }

            auras.push_back(base);
            return true;
        };

        // Match patterns (most specific first)
        AuraDef d;

        // "Other friendly units here have ..."
        d = {}; d.friendly = true; d.here_only = true; d.other_only = true;
        findAura("other friendly units here have", d);

        // "Other friendly units have ..."
        d = {}; d.friendly = true; d.all_friendly = true; d.other_only = true;
        findAura("other friendly units have", d);

        // "Your units have ..."
        d = {}; d.friendly = true; d.all_friendly = true;
        findAura("your units have", d);

        // "Units here have ..."
        d = {}; d.here_only = true;
        findAura("units here have", d);

        // "Stunned enemy units here have ..."
        d = {}; d.enemy = true; d.here_only = true; d.stunned_only = true;
        if (clean.find("stunned enemy units here have") != std::string::npos) {
            findAura("stunned enemy units here have", d);
        }

        // "Your Mechs each have [Assault]"
        if (clean.find("your mechs each have") != std::string::npos) {
            d = {}; d.friendly = true; d.all_friendly = true; d.tag_filter = true;
            d.required_tag = "Mech";
            findAura("your mechs each have", d);
        }

        // Step 3: Apply each aura to matching targets
        for (auto& aura : auras) {
            for (auto& [tgt_id, tgt] : state_.objects) {
                if (!tgt.isUnit()) continue;
                if (!tgt.location.has_value()) continue;

                // Self-exclusion
                if (aura.other_only && tgt_id == src_id) continue;

                // Ownership filter
                if (aura.friendly && tgt.controller != src.controller) continue;
                if (aura.enemy && tgt.controller == src.controller) continue;

                // Location filter
                if (aura.here_only) {
                    auto src_bf = src.battlefieldId();
                    auto tgt_bf = tgt.battlefieldId();
                    // Source must be at a battlefield, and target at the same one
                    if (!src_bf || !tgt_bf || *src_bf != *tgt_bf) {
                        // Also check if source IS the battlefield card
                        bool src_is_bf = src.isBattlefield();
                        if (src_is_bf) {
                            // Find which BF this card belongs to
                            bool match = false;
                            for (auto& bf : state_.battlefields) {
                                if (bf.card_object_id == src_id) {
                                    auto tbf = tgt.battlefieldId();
                                    if (tbf && *tbf == bf.id) match = true;
                                    break;
                                }
                            }
                            if (!match) continue;
                        } else {
                            continue;
                        }
                    }
                }

                // Tag filter
                if (aura.tag_filter) {
                    bool has_tag = false;
                    for (auto& tag : tgt.tags) {
                        if (tag == aura.required_tag) { has_tag = true; break; }
                    }
                    if (!has_tag) continue;
                }

                // Stunned filter
                if (aura.stunned_only && !tgt.is_stunned) continue;

                // Apply the aura effect
                GameObject::AuraEffect ae;
                ae.source = src_id;
                ae.might_bonus = aura.might_mod;
                ae.might_minimum = aura.might_min;
                if (aura.is_keyword) {
                    ae.keyword = aura.keyword;
                    ae.keyword_value = aura.keyword_value;
                }
                tgt.aura_effects.push_back(ae);
            }
        }
    }

    // Step 3b: Conditional self-effects ("If X, I have [Keyword]")
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.card_def_id == kInvalidId) continue; // skip tokens
        const auto& def = card_db_.get(obj.card_def_id);
        auto& text = def.ability_text;
        if (text.empty()) continue;

        std::string clean;
        { int d = 0;
          for (char c : text) {
              if (c == '(') d++;
              else if (c == ')') { if (d > 0) d--; }
              else if (d == 0) clean += std::tolower(c);
          }
        }

        // "If you've discarded a card this turn, I have [Assault] and [Ganking]"
        if (clean.find("if you've discarded") != std::string::npos ||
            clean.find("if you\xe2\x80\x99ve discarded") != std::string::npos) {
            if (state_.player(obj.controller).has_discarded_this_turn) {
                if (clean.find("[assault]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Assault; ae.keyword_value = 1;
                    obj.aura_effects.push_back(ae);
                }
                if (clean.find("[ganking]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Ganking;
                    obj.aura_effects.push_back(ae);
                }
            }
        }

        // "While I'm buffed, I have [Ganking]"
        if (clean.find("while i'm buffed") != std::string::npos ||
            clean.find("while i\xe2\x80\x99m buffed") != std::string::npos) {
            if (obj.buff_count > 0) {
                if (clean.find("[ganking]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Ganking;
                    obj.aura_effects.push_back(ae);
                }
                // "While I'm buffed, I have an additional +1 [M]"
                if (clean.find("additional +1 [m]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id; ae.might_bonus = 1;
                    obj.aura_effects.push_back(ae);
                }
            }
        }

        // "While I'm [Mighty], I have [Deflect], [Ganking], and [Shield]"
        if (clean.find("while i'm [mighty]") != std::string::npos ||
            clean.find("while i\xe2\x80\x99m [mighty]") != std::string::npos) {
            // Mighty = 5+ might (check base + buffs, before this aura)
            int pre_aura_might = obj.base_might + obj.buff_count + obj.attachment_might_bonus;
            if (pre_aura_might >= 5) {
                if (clean.find("[deflect]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Deflect; ae.keyword_value = 1;
                    obj.aura_effects.push_back(ae);
                }
                if (clean.find("[ganking]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Ganking;
                    obj.aura_effects.push_back(ae);
                }
                if (clean.find("[shield]") != std::string::npos) {
                    GameObject::AuraEffect ae; ae.source = id;
                    ae.keyword = Keyword::Shield; ae.keyword_value = 1;
                    obj.aura_effects.push_back(ae);
                }
            }
        }
    }

    // Step 3c: Equipment-granted keywords and stats
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        for (auto gear_id : obj.attachments) {
            if (!state_.objectExists(gear_id)) continue;
            Card* gear_card = card_registry_.get(state_.getObject(gear_id).card_def_id);
            if (!gear_card) continue;

            // Keywords from effect_text
            auto ekw = gear_card->equippedKeywords();
            if (ekw.bits != 0) {
                for (int k = 0; k < static_cast<int>(Keyword::Count); ++k) {
                    if (ekw.has(static_cast<Keyword>(k))) {
                        GameObject::AuraEffect ae;
                        ae.source = gear_id;
                        ae.keyword = static_cast<Keyword>(k);
                        obj.aura_effects.push_back(ae);
                    }
                }
            }
            // Assault/Shield/Deflect from effect_text
            int ea = gear_card->equippedAssault();
            if (ea > 0) {
                GameObject::AuraEffect ae;
                ae.source = gear_id;
                ae.keyword = Keyword::Assault;
                ae.keyword_value = ea;
                obj.aura_effects.push_back(ae);
            }
            int es = gear_card->equippedShield();
            if (es > 0) {
                GameObject::AuraEffect ae;
                ae.source = gear_id;
                ae.keyword = Keyword::Shield;
                ae.keyword_value = es;
                obj.aura_effects.push_back(ae);
            }
        }
    }

    // Step 4: Rebuild cached aura values on all objects
    for (auto& [id, obj] : state_.objects) {
        obj.aura_might_bonus = 0;
        obj.aura_keywords.reset();
        for (auto& ae : obj.aura_effects) {
            obj.aura_might_bonus += ae.might_bonus;
            if (ae.keyword != Keyword::Count) {
                obj.aura_keywords.set(ae.keyword);
            }
        }
        // Recompute might with new aura values
        if (obj.isUnit() && obj.location.has_value()) {
            obj.recomputeMight();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cleanup
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::cleanup() {
    constexpr int kMaxCleanupPasses = 20;
    for (int pass = 0; pass < kMaxCleanupPasses; ++pass) {
        bool changed = false;

        if (checkWinCondition()) return;
        processLethalDamage();
        updateBattlefieldControl();
        recalculateAuras();

        // If nothing changed, we're stable
        if (!changed) break;
    }
}

bool GameEngine::checkWinCondition() {
    // A player wins if they have >= victory score AND more than opponent (CR 467)
    for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
        auto& ps = state_.player(pid);
        auto& opp = state_.player(opponent(pid));
        if (ps.score >= state_.mode.victory_score && ps.score > opp.score) {
            state_.game_over = true;
            state_.winner = pid;
            state_.game_over_reason = toString(pid) + std::string(" reached ") +
                                       std::to_string(ps.score) + " points";
            events_.emit(GameOverEvent{pid, state_.game_over_reason});
            return true;
        }
    }
    return false;
}

void GameEngine::processLethalDamage() {
    // Check if either player has Elder Dragon's "any damage = lethal" effect
    bool elder_p1 = false, elder_p2 = false;
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.card_def_id == kInvalidId) continue;
        const auto& def = card_db_.get(obj.card_def_id);
        if (def.ability_text.find("Any amount of your damage is enough to kill") != std::string::npos ||
            def.ability_text.find("any amount of your damage is enough to kill") != std::string::npos) {
            if (obj.controller == PlayerId::Player1) elder_p1 = true;
            if (obj.controller == PlayerId::Player2) elder_p2 = true;
        }
    }

    std::vector<GameObjectId> to_kill;
    for (auto& [id, obj] : state_.objects) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.damage_marked <= 0) continue;

        bool lethal = obj.hasLethalDamage();

        // Elder Dragon: any damage kills enemy units
        if (!lethal) {
            PlayerId damager = opponent(obj.controller);
            if ((damager == PlayerId::Player1 && elder_p1) ||
                (damager == PlayerId::Player2 && elder_p2)) {
                lethal = true;
                events_.logTrace("ELDER_DRAGON: " + obj.name + " killed by any-damage rule");
            }
        }

        if (lethal) to_kill.push_back(id);
    }
    for (auto id : to_kill) {
        killUnit(id);
    }
}

void GameEngine::updateBattlefieldControl() {
    for (auto& bf : state_.battlefields) {
        if (bf.combat_in_progress || bf.showdown_in_progress) continue;

        if (bf.controller.has_value()) {
            bool has_units = bf.hasUnitsFrom(*bf.controller, state_.objects);
            if (!has_units && state_.turn.isNeutralOpen()) {
                auto old = bf.controller;
                bf.controller = std::nullopt;
                events_.emit(ControlChangedEvent{bf.id, old, std::nullopt});
            }
        }

        // Remove facedown cards that belong to a player who doesn't control this BF (CR 323.7.5)
        std::vector<GameObjectId> to_remove;
        for (auto card_id : bf.facedown) {
            if (!state_.objectExists(card_id)) continue;
            auto& card = state_.getObject(card_id);
            if (!bf.controller.has_value() || *bf.controller != card.controller) {
                to_remove.push_back(card_id);
            }
        }
        for (auto card_id : to_remove) {
            auto it = std::find(bf.facedown.begin(), bf.facedown.end(), card_id);
            if (it != bf.facedown.end()) bf.facedown.erase(it);

            auto& card = state_.getObject(card_id);
            card.is_hidden = false;
            card.hidden_at = kInvalidId;
            card.zone = ZoneType::Trash;
            card.location = std::nullopt;
            state_.player(card.owner).trash.push_back(card_id);
            events_.logDebug(std::string("HIDDEN: ") + card.name +
                             " removed from facedown (control lost)");
        }
    }
}

void GameEngine::processContestedBattlefields() {
    // Already handled in executeStandardMove — contested status is set there
    // and showdown/combat are staged
}

// ═══════════════════════════════════════════════════════════════════════════════
// Board operations
// ═══════════════════════════════════════════════════════════════════════════════

GameObjectId GameEngine::instantiateCard(CardDefId def_id, PlayerId owner) {
    auto obj_id = state_.createObject();
    auto& obj = state_.getObject(obj_id);
    const auto& def = card_db_.get(def_id);

    obj.card_def_id = def_id;
    obj.owner = owner;
    obj.controller = owner;
    obj.name = def.name;
    obj.card_type = def.card_type;
    obj.super_type = def.super_type;
    obj.tags = def.tags;
    obj.domains = def.domains;
    obj.base_might = def.might;
    obj.current_might = def.might;
    obj.keywords = def.keywords;
    obj.assault_value = def.assault_value;
    obj.shield_value = def.shield_value;
    obj.deflect_value = def.deflect_value;
    obj.might_bonus = def.might_bonus;

    return obj_id;
}

void GameEngine::drawCards(PlayerId player, int count) {
    auto& ps = state_.player(player);
    int drawn = 0;
    for (int i = 0; i < count; ++i) {
        if (ps.main_deck.empty()) {
            // Burn Out (CR 431.2): shuffle trash into deck, lose 1 point
            if (ps.trash.empty()) {
                events_.logTrace(std::string("BURN_OUT: ") + toString(player) +
                                 " deck AND trash empty, cannot draw");
                break; // truly empty — nothing to do
            }
            events_.logTrace(std::string("BURN_OUT: ") + toString(player) +
                             " deck empty, shuffling " + std::to_string(ps.trash.size()) +
                             " trash cards into deck, losing 1 point");
            ps.burned_out = true;
            // Shuffle trash into deck
            for (auto card_id : ps.trash) {
                state_.getObject(card_id).zone = ZoneType::MainDeck;
                ps.main_deck.push_back(card_id);
            }
            ps.trash.clear();
            shuffleDeck(player);
            // Lose 1 point (min 0)
            if (ps.score > 0) {
                ps.score--;
                events_.logTrace(std::string("BURN_OUT: ") + toString(player) +
                                 " score -> " + std::to_string(ps.score));
            }
            // If deck still empty after shuffle (shouldn't happen), stop
            if (ps.main_deck.empty()) break;
        }
        auto card_id = ps.main_deck.back();
        ps.main_deck.pop_back();
        ps.hand.push_back(card_id);
        state_.getObject(card_id).zone = ZoneType::Hand;
        events_.logTrace("  DREW: " + state_.getObject(card_id).name +
                         " (id=" + std::to_string(card_id) + ")");
        drawn++;
    }
    if (drawn > 0) {
        events_.emit(CardsDrawnEvent{player, drawn});
    }
}

void GameEngine::channelRunes(PlayerId player, int count) {
    auto& ps = state_.player(player);
    for (int i = 0; i < count; ++i) {
        if (ps.rune_deck.empty()) break;
        auto rune_id = ps.rune_deck.back();
        ps.rune_deck.pop_back();

        auto& rune = state_.getObject(rune_id);
        rune.zone = ZoneType::Base;
        rune.location = BaseLocation{player};

        events_.emit(RuneChanneledEvent{rune_id, player});
        events_.emit(EnteredBoardEvent{
            rune_id, player, CardType::Rune, BaseLocation{player}, false});
    }
}

void GameEngine::shuffleDeck(PlayerId player) {
    auto& deck = state_.player(player).main_deck;
    std::shuffle(deck.begin(), deck.end(), rng_);
}

void GameEngine::shuffleRuneDeck(PlayerId player) {
    auto& deck = state_.player(player).rune_deck;
    std::shuffle(deck.begin(), deck.end(), rng_);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cost payment
// ═══════════════════════════════════════════════════════════════════════════════

int GameEngine::availableEnergy(PlayerId player) const {
    // Energy comes from exhausting ready runes (CR 163.2.a: [E]: Add [1])
    // Each ready rune in base can be exhausted for 1 Energy
    int count = 0;
    auto base_loc = BaseLocation{player};
    for (auto& [id, obj] : state_.objects) {
        if (obj.isRune() && obj.controller == player && !obj.is_exhausted &&
            obj.location.has_value() && *obj.location == LocationId{base_loc}) {
            count++;
        }
    }
    // Also count Energy already in the pool
    count += state_.player(player).rune_pool.energy;
    return count;
}

int GameEngine::availablePower(PlayerId player, Domain domain) const {
    // Power comes from recycling a rune of matching domain (CR 163.2.b)
    // Each rune in base (exhausted or ready) of matching domain can be recycled for 1 Power
    int count = 0;
    auto base_loc = BaseLocation{player};
    for (auto& [id, obj] : state_.objects) {
        if (obj.isRune() && obj.controller == player &&
            obj.location.has_value() && *obj.location == LocationId{base_loc}) {
            for (auto d : obj.domains) {
                if (d == domain) { count++; break; }
            }
        }
    }
    // Also count matching Power already in pool
    count += state_.player(player).rune_pool.power[static_cast<int>(domain)];
    count += state_.player(player).rune_pool.universal_power;
    return count;
}

int GameEngine::availableAnyPower(PlayerId player) const {
    // Any rune in base can be recycled for 1 Power of its domain
    int count = 0;
    auto base_loc = BaseLocation{player};
    for (auto& [id, obj] : state_.objects) {
        if (obj.isRune() && obj.controller == player &&
            obj.location.has_value() && *obj.location == LocationId{base_loc}) {
            count++;
        }
    }
    return count;
}

bool GameEngine::canAfford(PlayerId player, GameObjectId card_obj) const {
    auto& card = state_.getObject(card_obj);
    if (card.card_def_id == kInvalidId) return false;  // tokens have no cost
    const auto& def = card_db_.get(card.card_def_id);

    int energy_needed = def.energy_cost;
    int power_needed = def.power_cost;

    // Apply cost reductions (Phase 5)
    auto& ps_const = state_.player(player);
    int min_cost = 0;
    for (auto& mod : ps_const.cost_modifiers) {
        if (mod.next_spell_only && !card.isSpell()) continue;
        if (mod.next_unit_only && !card.isUnit()) continue;
        energy_needed -= mod.energy_reduction;
        if (mod.min_cost > min_cost) min_cost = mod.min_cost;
    }
    // Self-cost reduction hook (e.g., Noxus Hopeful "[Legion] I cost [2] less").
    if (auto* self_card = card_registry_.get(card.card_def_id)) {
        energy_needed -= self_card->selfCostReduction(state_, player);
    }
    energy_needed = std::max(min_cost, energy_needed);
    energy_needed = std::max(0, energy_needed);

    // Count available runes in base
    auto base_loc = BaseLocation{player};
    std::vector<GameObjectId> ready_runes;
    std::vector<GameObjectId> matching_domain_runes;

    for (auto& [id, obj] : state_.objects) {
        if (!obj.isRune() || obj.controller != player) continue;
        if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;

        if (!obj.is_exhausted) {
            ready_runes.push_back(id);
        }

        // Check domain match for power cost
        if (power_needed > 0) {
            for (auto d : def.domains) {
                for (auto rd : obj.domains) {
                    if (d == rd) {
                        matching_domain_runes.push_back(id);
                        goto next_rune;
                    }
                }
            }
        }
        next_rune:;
    }

    // Energy: need to exhaust ready runes. Each gives 1 Energy.
    // But we also need to reserve runes for Power (recycling removes them).
    // Worst case: we recycle power_needed runes for Power, and those might
    // have been ready (reducing available Energy).

    // Simple check: total runes in base >= energy_needed + power_needed
    // AND matching domain runes >= power_needed
    // AND ready runes >= energy_needed (after accounting for recycled ones)

    int total_runes = ready_runes.size();
    // Some ready runes might be the same ones we need for Power recycling.
    // Count how many matching-domain runes are also ready.
    int matching_ready = 0;
    for (auto mr : matching_domain_runes) {
        auto& rune = state_.getObject(mr);
        if (!rune.is_exhausted) matching_ready++;
    }

    if (static_cast<int>(matching_domain_runes.size()) < power_needed) {
        return false; // Not enough matching-domain runes to recycle
    }

    // Energy available after recycling power_needed runes:
    // We prefer to recycle exhausted runes for power (preserving ready runes for energy).
    int exhausted_matching = static_cast<int>(matching_domain_runes.size()) - matching_ready;
    int recycle_from_ready = std::max(0, power_needed - exhausted_matching);
    int energy_available = total_runes - recycle_from_ready;

    // Also add any energy already in the pool
    energy_available += state_.player(player).rune_pool.energy;

    return energy_available >= energy_needed;
}

bool GameEngine::payCardCost(PlayerId player, GameObjectId card_obj) {
    auto& card = state_.getObject(card_obj);
    if (card.card_def_id == kInvalidId) return true;  // tokens cost nothing
    const auto& def = card_db_.get(card.card_def_id);

    int energy_needed = def.energy_cost;
    int power_needed = def.power_cost;

    // Apply cost reductions (Phase 5)
    auto& ps = state_.player(player);
    int min_cost = 0;
    for (auto& mod : ps.cost_modifiers) {
        if (mod.next_spell_only && !card.isSpell()) continue;
        if (mod.next_unit_only && !card.isUnit()) continue;
        energy_needed -= mod.energy_reduction;
        if (mod.min_cost > min_cost) min_cost = mod.min_cost;
    }
    // Self-cost reduction hook (e.g., Noxus Hopeful Legion discount).
    if (auto* self_card = card_registry_.get(card.card_def_id)) {
        energy_needed -= self_card->selfCostReduction(state_, player);
    }
    energy_needed = std::max(min_cost, energy_needed);
    energy_needed = std::max(0, energy_needed);

    // Consume one-shot modifiers that applied
    ps.cost_modifiers.erase(
        std::remove_if(ps.cost_modifiers.begin(), ps.cost_modifiers.end(),
            [&](const PlayerState::CostModifier& m) {
                if (m.next_spell_only && card.isSpell()) return true;
                if (m.next_unit_only && card.isUnit()) return true;
                return false;
            }),
        ps.cost_modifiers.end());

    events_.logTrace("PAY_COST: " + card.name + " E=" + std::to_string(energy_needed) +
                     " P=" + std::to_string(power_needed));

    auto base_loc = BaseLocation{player};

    // Step 1: Pay Energy FIRST by exhausting ready runes.
    // This is more efficient — exhausted runes can then be recycled for power.
    // Agent chooses which runes to exhaust.
    if (energy_needed > 0) {
        // First spend any energy already in pool
        int from_pool = std::min(ps.rune_pool.energy, energy_needed);
        ps.rune_pool.energy -= from_pool;
        energy_needed -= from_pool;

        // Gather all ready runes
        std::vector<GameObjectId> ready_runes;
        for (auto& [id, obj] : state_.objects) {
            if (!obj.isRune() || obj.controller != player || obj.is_exhausted) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            ready_runes.push_back(id);
        }

        // Agent selects which runes to exhaust for energy
        for (int e = 0; e < energy_needed && !ready_runes.empty(); ++e) {
            std::vector<Intent> choices;
            for (auto rune_id : ready_runes) {
                Intent choice;
                choice.type = IntentType::MakeChoice;
                choice.player = player;
                choice.chosen_objects = {rune_id};
                choices.push_back(choice);
            }

            GameObjectId chosen_rune = ready_runes[0]; // default
            if (choices.size() > 1) {
                state_.decision_index++;
                auto chosen = getAgent(player).selectAction(state_, choices);
                if (!chosen.chosen_objects.empty()) {
                    chosen_rune = chosen.chosen_objects[0];
                }
                if (on_decision) on_decision(state_, choices, chosen);
            }

            auto& rune = state_.getObject(chosen_rune);
            events_.logTrace("  EXHAUST_RUNE: " + rune.name + " (id=" +
                             std::to_string(chosen_rune) + ") for energy [agent choice]");
            rune.is_exhausted = true;
            events_.emit(ObjectStateChangedEvent{chosen_rune, "exhausted"});

            ready_runes.erase(
                std::remove(ready_runes.begin(), ready_runes.end(), chosen_rune),
                ready_runes.end());
        }
    }

    // Step 2: Pay Power by recycling domain-matching runes.
    // Now that energy is paid, exhausted runes (including just-exhausted) are available.
    // Agent chooses which runes to recycle.
    if (power_needed > 0) {
        std::vector<GameObjectId> matching_runes;
        for (auto& [id, obj] : state_.objects) {
            if (!obj.isRune() || obj.controller != player) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            bool domain_match = false;
            for (auto d : def.domains) {
                for (auto rd : obj.domains) {
                    if (d == rd) { domain_match = true; break; }
                }
                if (domain_match) break;
            }
            if (domain_match) matching_runes.push_back(id);
        }

        for (int p = 0; p < power_needed && !matching_runes.empty(); ++p) {
            std::vector<Intent> choices;
            for (auto rune_id : matching_runes) {
                Intent choice;
                choice.type = IntentType::MakeChoice;
                choice.player = player;
                choice.chosen_objects = {rune_id};
                choices.push_back(choice);
            }

            // Default: prefer exhausted runes (already spent for energy)
            std::sort(matching_runes.begin(), matching_runes.end(),
                [&](GameObjectId a, GameObjectId b) {
                    return state_.getObject(a).is_exhausted > state_.getObject(b).is_exhausted;
                });
            GameObjectId chosen_rune = matching_runes[0];

            if (choices.size() > 1) {
                state_.decision_index++;
                auto chosen = getAgent(player).selectAction(state_, choices);
                if (!chosen.chosen_objects.empty()) {
                    chosen_rune = chosen.chosen_objects[0];
                }
                if (on_decision) on_decision(state_, choices, chosen);
            }

            auto& rune = state_.getObject(chosen_rune);
            events_.logTrace("  RECYCLE_RUNE: " + rune.name + " (id=" +
                             std::to_string(chosen_rune) + ", " +
                             (rune.is_exhausted ? "exhausted" : "ready") + ") for power [agent choice]");
            rune.location = std::nullopt;
            rune.zone = ZoneType::RuneDeck;
            rune.is_exhausted = false;
            ps.rune_deck.insert(ps.rune_deck.begin(), chosen_rune);
            events_.emit(LeftBoardEvent{chosen_rune, player, CardType::Rune,
                base_loc, ZoneType::RuneDeck, false});

            matching_runes.erase(
                std::remove(matching_runes.begin(), matching_runes.end(), chosen_rune),
                matching_runes.end());
        }
    }

    return true;
}

void GameEngine::moveUnit(GameObjectId unit_id, LocationId destination) {
    auto& unit = state_.getObject(unit_id);
    unit.location = destination;
    // Update zone type based on location
    if (std::holds_alternative<BaseLocation>(destination)) {
        unit.zone = ZoneType::Base;
    } else {
        unit.zone = ZoneType::BattlefieldZone;
    }
}

void GameEngine::killUnit(GameObjectId unit_id) {
    auto& unit = state_.getObject(unit_id);
    auto controller = unit.controller;
    events_.logTrace("KILL: " + unit.name + " (id=" + std::to_string(unit_id) +
                     ", " + std::to_string(unit.current_might) + "M, dmg=" +
                     std::to_string(unit.damage_marked) + ")");

    // Check for replacement effects: "would die → instead heal/exhaust/recall"
    // Scan all objects controlled by the same player for replacement abilities
    for (auto& [id, obj] : state_.objects) {
        if (obj.controller != controller) continue;
        if (id == unit_id) continue;
        if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) continue;
        if (obj.card_def_id == kInvalidId) continue;

        const auto& def = card_db_.get(obj.card_def_id);
        auto& text = def.ability_text;

        // Check for "would die" replacement pattern
        if (text.find("would die") != std::string::npos &&
            text.find("instead") != std::string::npos) {

            // Replacement applies — heal, exhaust, recall instead of dying
            events_.logDebug(std::string("REPLACEMENT: ") + obj.name +
                             " prevents " + unit.name + " from dying");

            unit.damage_marked = 0;
            unit.is_exhausted = true;
            // Recall to base
            auto old_loc = unit.location;
            unit.location = BaseLocation{controller};
            unit.zone = ZoneType::Base;
            events_.emit(ObjectStateChangedEvent{unit_id, "healed"});
            events_.emit(UnitMovedEvent{unit_id, controller,
                old_loc.value_or(BaseLocation{controller}),
                BaseLocation{controller}, false});

            // If the replacement source is a gear/spell that says "kill this instead",
            // destroy the replacement source
            if (text.find("kill this instead") != std::string::npos ||
                text.find("kill me instead") != std::string::npos) {
                auto rep_was_at = obj.location;
                obj.zone = ZoneType::Trash;
                obj.location = std::nullopt;
                state_.player(obj.owner).trash.push_back(id);
                events_.emit(LeftBoardEvent{id, obj.controller, obj.card_type,
                    rep_was_at.value_or(BaseLocation{obj.controller}),
                    ZoneType::Trash, true});
            }

            return; // replacement consumed — unit does NOT die
        }
    }

    // No replacement — unit dies normally
    // Detach all gear first (CR 719.5)
    detachAllGear(unit_id);

    auto was_at = unit.location;
    int might = unit.current_might;

    unit.zone = ZoneType::Trash;
    unit.location = std::nullopt;
    unit.damage_marked = 0;
    unit.combat_designation = CombatDesignation::None;
    state_.player(unit.owner).trash.push_back(unit_id);

    events_.emit(UnitDiedEvent{unit_id, controller,
        was_at.value_or(BaseLocation{controller}), might});
    events_.emit(LeftBoardEvent{unit_id, controller, CardType::Unit,
        was_at.value_or(BaseLocation{controller}), ZoneType::Trash, true});
}

void GameEngine::healAllUnits() {
    for (auto& [id, obj] : state_.objects) {
        if (obj.isUnit() && obj.damage_marked > 0 && obj.location.has_value()) {
            obj.damage_marked = 0;
        }
    }
}

void GameEngine::emptyRunePools() {
    state_.players[0].rune_pool.clear();
    state_.players[1].rune_pool.clear();
}

Intent GameEngine::queryAgent(PlayerId player) {
    auto actions = generateLegalActions();
    state_.decision_index++;
    if (player == PlayerId::Player1) state_.turn.turn_decisions_p1++;
    else state_.turn.turn_decisions_p2++;

    events_.logTrace("DECISION #" + std::to_string(state_.decision_index) +
                     " (" + toString(player) + "): " +
                     std::to_string(actions.size()) + " legal actions");

    auto chosen = getAgent(player).selectAction(state_, actions);

    {
        std::string choice_str = toString(chosen.type);
        if (chosen.card != kInvalidId && state_.objectExists(chosen.card)) {
            choice_str += " " + state_.getObject(chosen.card).name;
        }
        if (chosen.ability_source != kInvalidId && state_.objectExists(chosen.ability_source)) {
            choice_str += " (src=" + state_.getObject(chosen.ability_source).name + ")";
        }
        if (!chosen.targets.empty()) {
            choice_str += " tgt=[";
            for (size_t i = 0; i < chosen.targets.size(); ++i) {
                if (i > 0) choice_str += ",";
                if (state_.objectExists(chosen.targets[i]))
                    choice_str += state_.getObject(chosen.targets[i]).name;
                else
                    choice_str += "?" + std::to_string(chosen.targets[i]);
            }
            choice_str += "]";
        }
        if (!chosen.units_to_move.empty()) {
            choice_str += " units=[";
            for (size_t i = 0; i < chosen.units_to_move.size(); ++i) {
                if (i > 0) choice_str += ",";
                if (state_.objectExists(chosen.units_to_move[i]))
                    choice_str += state_.getObject(chosen.units_to_move[i]).name;
            }
            choice_str += "]";
        }
        events_.logTrace("  CHOSE: " + choice_str);
    }

    if (on_decision) {
        on_decision(state_, actions, chosen);
    }
    return chosen;
}

AgentInterface& GameEngine::getAgent(PlayerId player) {
    return *agents_[playerIndex(player)];
}

PlayerId GameEngine::activePlayer() const {
    if (state_.turn.priority_holder.has_value())
        return *state_.turn.priority_holder;
    return state_.turn.turn_player;
}

BattlefieldState& GameEngine::getBattlefield(BattlefieldId id) {
    for (auto& bf : state_.battlefields) {
        if (bf.id == id) return bf;
    }
    throw std::runtime_error("Battlefield not found: " + std::to_string(id));
}

const BattlefieldState& GameEngine::getBattlefield(BattlefieldId id) const {
    for (auto& bf : state_.battlefields) {
        if (bf.id == id) return bf;
    }
    throw std::runtime_error("Battlefield not found: " + std::to_string(id));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Battlefield Replace/Swap-back (CR 438)
// ═══════════════════════════════════════════════════════════════════════════════

void GameEngine::replaceBattlefield(BattlefieldId original_bf,
                                     GameObjectId token_bf_card,
                                     PlayerId controller) {
    auto& bf = getBattlefield(original_bf);

    events_.logTrace("BF_REPLACE: BF#" + std::to_string(original_bf) +
                     " replaced by token (id=" + std::to_string(token_bf_card) + ")");

    // Original BF card goes to Banishment
    if (state_.objectExists(bf.card_object_id)) {
        auto& orig = state_.getObject(bf.card_object_id);
        auto owner = orig.owner;
        orig.zone = ZoneType::Banishment;
        orig.location = std::nullopt;
        state_.player(owner).banishment.push_back(bf.card_object_id);
    }

    // Token takes the slot
    bf.replaced_card = bf.card_object_id;
    bf.was_replaced = true;
    bf.is_token = true;
    bf.card_object_id = token_bf_card;
    bf.contributed_by = controller;

    // Set token card location
    auto& token_obj = state_.getObject(token_bf_card);
    token_obj.zone = ZoneType::BattlefieldZone;
    token_obj.location = BattlefieldLocation{original_bf};
}

void GameEngine::swapBackBattlefield(BattlefieldId token_bf) {
    auto& bf = getBattlefield(token_bf);
    if (!bf.was_replaced || !bf.replaced_card.has_value()) return;

    auto original_card = *bf.replaced_card;

    events_.logTrace("BF_SWAPBACK: BF#" + std::to_string(token_bf) +
                     " restoring original card (id=" + std::to_string(original_card) + ")");

    // Remove token card
    auto& token_obj = state_.getObject(bf.card_object_id);
    token_obj.zone = ZoneType::Trash;
    token_obj.location = std::nullopt;

    // Restore original from Banishment
    if (state_.objectExists(original_card)) {
        auto& orig = state_.getObject(original_card);
        auto owner = orig.owner;
        // Remove from banishment
        auto& ban = state_.player(owner).banishment;
        ban.erase(std::remove(ban.begin(), ban.end(), original_card), ban.end());

        orig.zone = ZoneType::BattlefieldZone;
        orig.location = BattlefieldLocation{token_bf};
    }

    bf.card_object_id = original_card;
    bf.replaced_card = std::nullopt;
    bf.was_replaced = false;
    bf.is_token = false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Equip/Attach (CR 716-725)
// ═══════════════════════════════════════════════════════════════════════════════

bool GameEngine::attachGearToUnit(GameObjectId gear_id, GameObjectId unit_id) {
    if (!state_.objectExists(gear_id) || !state_.objectExists(unit_id)) return false;

    auto& gear = state_.getObject(gear_id);
    auto& unit = state_.getObject(unit_id);

    if (!gear.isGear() || !unit.isUnit()) return false;
    if (gear.attached_to.has_value()) return false; // already attached

    events_.logTrace("ATTACH: " + gear.name + " (id=" + std::to_string(gear_id) +
                     ") -> " + unit.name + " (id=" + std::to_string(unit_id) + ")");

    gear.attached_to = unit_id;
    gear.is_rules_text_inactive = true; // CR 718.2: rules text inactive while attached
    unit.attachments.push_back(gear_id);

    // Move gear to same location as unit
    gear.location = unit.location;
    gear.zone = unit.zone;

    // Apply might bonus (CR 472.3.c — Arithmetic layer)
    unit.attachment_might_bonus += gear.might_bonus;
    unit.recomputeMight();

    events_.emit(ObjectStateChangedEvent{gear_id, "attached"});
    events_.emit(ObjectStateChangedEvent{unit_id, "equipped"});

    return true;
}

void GameEngine::detachAllGear(GameObjectId unit_id) {
    if (!state_.objectExists(unit_id)) return;
    auto& unit = state_.getObject(unit_id);

    for (auto gear_id : unit.attachments) {
        if (!state_.objectExists(gear_id)) continue;
        auto& gear = state_.getObject(gear_id);

        events_.logTrace("DETACH: " + gear.name + " from " + unit.name);

        unit.attachment_might_bonus -= gear.might_bonus;
        gear.attached_to = std::nullopt;
        gear.is_rules_text_inactive = false;
        // Detached gear stays at its current location (CR 719.5)
        events_.emit(ObjectStateChangedEvent{gear_id, "detached"});
    }
    unit.attachments.clear();
    unit.recomputeMight();
}

} // namespace riftbound
