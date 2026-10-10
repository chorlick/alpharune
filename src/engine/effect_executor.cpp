#include "effect_executor.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "engine/deck_ops.h"

#include <algorithm>
#include <cassert>

namespace riftbound {

EffectExecutor::EffectExecutor(GameState& state, EventBus& events,
                               const CardDB& card_db,
                               const CardRegistry* card_registry)
    : state_(state), events_(events), card_db_(card_db),
      card_registry_(card_registry) {}

namespace {

// `objectExists` is true for every id ever created, so it says nothing about
// where an object is. The board actions below (damage, kill, move, return to
// hand, buff, stun, ready) act only on an object that is on the board — at a
// base or a battlefield. An object in a hand, deck, trash, banishment or
// facedown zone has no location.
bool onBoard(const GameObject& obj) { return obj.location.has_value(); }

// Deflect granted "this turn" (giveTemporaryKeyword). Kept apart from the
// printed `deflect_value` and stamped with the turn it was given on, so it
// stops counting when that turn ends.
constexpr const char* kTempDeflect = "__temp_deflect";
constexpr const char* kTempDeflectTurn = "__temp_deflect_turn";

int temporaryDeflect(const GameState& state, const GameObject& obj) {
    auto turn = obj.card_counters.find(kTempDeflectTurn);
    if (turn == obj.card_counters.end() || turn->second != state.turn.turn_number) return 0;
    auto value = obj.card_counters.find(kTempDeflect);
    return value == obj.card_counters.end() ? 0 : value->second;
}

// CR 719.3: a Top-Most card and everything attached to it are at the same
// location, so attached gear goes wherever its unit goes.
void carryAttachments(GameState& state, const GameObject& unit) {
    if (!unit.location.has_value()) return;
    for (auto attached_id : unit.attachments) {
        if (!state.objectExists(attached_id)) continue;
        auto& a = state.getObject(attached_id);
        a.location = unit.location;
        a.zone = unit.zone;
    }
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════════
// Atomic game operations
// ═══════════════════════════════════════════════════════════════════════════════

void EffectExecutor::dealDamage(GameObjectId target, int amount,
                                 GameObjectId source) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    // Damage is marked on units on the board (CR 418.1.b); a unit that has
    // left it takes none.
    if (!onBoard(obj)) return;

    // Continuous per-unit damage immunity (Kayn, Unleashed: "if I have moved
    // twice this turn, I don't take damage"). Recomputed each recalculateAuras
    // by the card's applyPassiveAura; blocks ALL damage (combat + spell/ability).
    if (amount > 0 && obj.immune_to_damage) {
        events_.logTrace("PREVENT: " + obj.name + " is immune to damage");
        return;
    }

    // One-shot per-unit damage prevention (Counter Strike 510): "the next time
    // it would be dealt damage this turn, prevent it." Consumes the flag and
    // prevents this damage instance entirely. Checked before Unyielding Spirit
    // and the actual damage application.
    // Counted prevention (Ki Barrier: "prevent the next 7 damage").
    if (amount > 0 && obj.prevent_damage_remaining > 0) {
        int prevented = std::min(amount, obj.prevent_damage_remaining);
        obj.prevent_damage_remaining -= prevented;
        amount -= prevented;
        events_.logTrace("PREVENT: " + std::to_string(prevented) + " damage to " + obj.name);
        if (amount <= 0) return;
    }
    if (amount > 0 && obj.prevent_next_damage_this_turn) {
        obj.prevent_next_damage_this_turn = false;
        events_.logTrace("PREVENT: next damage to " + obj.name +
                         " prevented (Counter Strike)");
        return;
    }

    // Unyielding Spirit [145] — "Prevent all spell and ability damage
    // this turn." Set by the spell's onResolve on the controller side
    // (typically the player who cast it, but the text is global —
    // applies to ALL spell/ability damage in the turn). Only prevents
    // damage whose source object is a spell or activated ability;
    // combat damage (source object's controller initiated combat,
    // dealt by units) passes through. Checked against the target's
    // controller AND the casting player's flag — either being set
    // prevents the damage. Phase 6q+.
    if (state_.objectExists(source)) {
        const auto& src_obj = state_.getObject(source);
        bool from_spell_or_ability =
            src_obj.isSpell() ||
            src_obj.zone == ZoneType::Chain ||
            // Activated abilities resolve through chain items whose
            // `source` points back at the activator object. We can't
            // perfectly distinguish "activated by source unit" from
            // "combat damage by source unit" at this layer, but for
            // Unyielding Spirit's MTG-style intent, all non-combat
            // damage qualifies. Check the chain — if there's a current
            // resolving item whose source matches `source`, treat as
            // ability damage.
            (state_.chain.resuming.has_value() &&
             state_.chain.resuming->source == source);
        // Esteemed Hierophant: "prevent all damage that enemy spells and
        // abilities would deal to me."
        if (from_spell_or_ability && amount > 0 &&
            obj.prevent_enemy_spell_ability_damage &&
            src_obj.controller != obj.controller) {
            events_.logTrace("PREVENT: enemy spell/ability damage to " + obj.name);
            return;
        }
        if (from_spell_or_ability) {
            // Check both players' prevention flags (the spell is
            // global, so whichever side cast it sets BOTH? Today the
            // card only sets it on the caster — but the spec says
            // "prevent all", so check defender side first, fall back
            // to caster side).
            bool prevented =
                state_.player(obj.controller).prevent_spell_ability_damage_this_turn ||
                state_.player(src_obj.controller).prevent_spell_ability_damage_this_turn;
            if (prevented && amount > 0) {
                events_.logTrace("EFFECT: prevented " + std::to_string(amount) +
                                 " spell/ability damage to " + obj.name +
                                 " (id=" + std::to_string(target) +
                                 ") via Unyielding Spirit");
                return;
            }
            // Bonus Damage: +N to spell/ability damage from the source's
            // controller (Annie, Fiery) and/or against the target's location
            // (Void Gate). Applied before any doubling.
            if (amount > 0) {
                int bonus = state_.player(src_obj.controller).bonus_damage_dealt +
                            obj.aura_bonus_damage_taken +
                            // Ravenborn Tome rider, bound to this spell object at
                            // play time — applies to every instance it deals.
                            src_obj.spell_bonus_damage;
                if (bonus > 0) {
                    events_.logTrace("EFFECT: +" + std::to_string(bonus) +
                                     " Bonus Damage to " + obj.name);
                    amount += bonus;
                }
            }
        }
    }

    // Lotus Trap-style damage doubling. The flag is set by cards like
    // Lotus Trap [575] ("Double all damage that would be dealt to it this
    // turn") and cleared at end of turn in resetTurnTracking. Applies to
    // every damage source — spells, abilities, combat damage — until
    // expiration. Per CR 437, this is a replacement effect on the
    // damage-dealing step.
    int dealt = amount;
    if (obj.damage_doubled_this_turn && amount > 0) {
        dealt = amount * 2;
        events_.logTrace("EFFECT: damage-doubling on " + obj.name +
                         " (Lotus Trap-style): " + std::to_string(amount) +
                         " -> " + std::to_string(dealt));
    }

    events_.logTrace("EFFECT: deal " + std::to_string(dealt) + " damage to " +
                     obj.name + " (id=" + std::to_string(target) + ", was " +
                     std::to_string(obj.damage_marked) + "/" +
                     std::to_string(obj.current_might) + "M)");
    if (obj.damage_marked == 0) obj.damage_marked_by[0] = obj.damage_marked_by[1] = 0;
    obj.damage_marked += dealt;
    obj.damage_taken_this_turn += dealt;
    // "Your damage" is the damage you marked: credit the source's controller.
    if (state_.objectExists(source)) {
        const PlayerId by = state_.getObject(source).controller;
        if (by != PlayerId::None) obj.damage_marked_by[playerIndex(by)] += dealt;
    }
    events_.emit(DamageDealtEvent{target, dealt, source, false});
}

void EffectExecutor::snapshotLastKnown(GameObjectId id) {
    if (!state_.objectExists(id)) return;
    auto& obj = state_.getObject(id);
    auto& lk = obj.last_known;
    lk.valid = true;
    lk.might = obj.current_might;
    lk.empowered = obj.empowered;
    lk.buff_count = obj.buff_count;
    lk.combat_designation = obj.combat_designation;
    lk.attachments = obj.attachments;
    lk.keywords = obj.keywords | obj.aura_keywords;
    lk.in_combat = false;
    if (auto bf = obj.battlefieldId())
        for (const auto& b : state_.battlefields)
            if (b.id == *bf) lk.in_combat = b.combat_in_progress;
    // Kept for cards that read the older key.
    obj.card_counters["__died_empowered"] = obj.empowered;
}

void EffectExecutor::killObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    if (obj.zone == ZoneType::Trash) return; // already dead
    // CR 429.2.a: only something on the board can be killed. A dead token
    // sits in Banishment, a bounced unit in a hand — killing either again
    // would announce a second death.
    if (!onBoard(obj)) return;
    events_.logTrace("EFFECT: kill " + obj.name + " (id=" + std::to_string(target) + ")");
    // Last-known information for "When I die" (CR 383.2.c.3.b).
    snapshotLastKnown(target);

    auto was_at = obj.location;
    auto controller = obj.controller;

    if (obj.isUnit()) {
        // Deferred one-shot death replacement on the unit itself (Tactical
        // Retreat 737) — applies to effect/ability kills too, not just combat
        // lethal (GameEngine::killUnit has the parallel check).
        if (obj.death_replacement_recall_pending) {
            obj.death_replacement_recall_pending = false;
            for (auto gear_id : obj.attachments) {
                if (state_.objectExists(gear_id)) {
                    state_.getObject(gear_id).attached_to = std::nullopt;
                }
            }
            obj.attachments.clear();
            obj.attachment_might_bonus = 0;
            auto old_loc = obj.location;
            obj.damage_marked = 0;
            obj.is_exhausted = true;
            obj.combat_designation = CombatDesignation::None;
            obj.location = BaseLocation{controller};
            obj.zone = ZoneType::Base;
            events_.logTrace("  REPLACEMENT (self): " + obj.name +
                             " heals/exhausts/recalls instead of dying");
            events_.emit(ObjectStateChangedEvent{target, "healed"});
            events_.emit(UnitMovedEvent{target, controller,
                old_loc.value_or(BaseLocation{controller}),
                BaseLocation{controller}, false});
            return;
        }

        // Card-defined death replacements (The Boss, Zhonya's Hourglass,
        // Guardian Angel, Soraka, ...). Effect kills consult them exactly as
        // combat deaths do (GameEngine::killUnit): a kill is a kill.
        if (card_registry_) {
            std::vector<GameObjectId> replacers;
            for (auto& [id, other] : state_.objects) {
                if (other.controller != controller || id == target) continue;
                if (!other.location.has_value() && other.zone != ZoneType::LegendZone) continue;
                if (other.card_def_id == kInvalidId) continue;
                const Card* c = card_registry_->get(other.card_def_id);
                if (c && c->hasReplacementEffect()) replacers.push_back(id);
            }
            for (auto rid : replacers) {
                Card* c = card_registry_->get(state_.getObject(rid).card_def_id);
                CardContext rctx{state_, events_, *this, controller, rid};
                if (c && c->applyReplacement(rctx, target)) {
                    events_.logDebug(std::string("REPLACEMENT: ") + state_.getObject(rid).name +
                                     " prevents " + state_.getObject(target).name + " from dying");
                    return;
                }
            }
        }
        // Its gear is shed (CR 719.5) after the death is announced, so abilities
        // that trigger on it see the unit as it died — including what it was
        // a copy of while an Equipment made it one.
        const auto attached = obj.attachments;   // copy

        int might = obj.current_might;
        const bool is_token = (obj.isToken());
        // CR 183.1: tokens cease to exist when leaving the board. Route
        // them to Banishment and skip the player's trash vector so
        // they're truly gone. Non-token units go to trash as usual.
        obj.zone = is_token ? ZoneType::Banishment : ZoneType::Trash;
        obj.last_location = obj.location;  // preserve for Deathknell triggers
        obj.location = std::nullopt;
        obj.damage_marked = 0;
        obj.combat_designation = CombatDesignation::None;
        if (!is_token) {
            state_.player(obj.owner).trash.push_back(target);
        } else {
            events_.logTrace("  TOKEN CEASES TO EXIST (CR 183.1): " + obj.name +
                             " (id=" + std::to_string(target) + ")");
        }

        // "If a unit died this turn" bookkeeping — the same as a combat death
        // (GameEngine::killUnit): a kill is a kill.
        state_.turn.any_unit_died_this_turn = true;
        {
            const TurnPhase ph = state_.turn.phase;
            const bool in_beginning = ph == TurnPhase::AwakenPhase ||
                                      ph == TurnPhase::BeginningStep ||
                                      ph == TurnPhase::ScoringStep;
            if (in_beginning && state_.turn.turn_player == controller)
                state_.player(controller).unit_died_in_beginning_this_turn = true;
        }
        events_.emit(UnitDiedEvent{target, controller,
            was_at.value_or(BaseLocation{controller}), might});
        events_.emit(LeftBoardEvent{target, controller, CardType::Unit,
            was_at.value_or(BaseLocation{controller}),
            is_token ? ZoneType::Banishment : ZoneType::Trash, true});
        // Through unattachGear so each gear's onUnattached hook runs.
        for (auto gear_id : attached) unattachGear(gear_id);
        if (state_.objectExists(target)) {
            auto& dead = state_.getObject(target);
            dead.attachments.clear();
            dead.attachment_might_bonus = 0;
        }
    } else if (obj.isGear()) {
        // A gear that dies while attached comes off its unit first.
        if (obj.attached_to.has_value()) unattachGear(target);
        const bool is_token = (obj.isToken());
        obj.zone = is_token ? ZoneType::Banishment : ZoneType::Trash;
        obj.last_location = obj.location;
        obj.location = std::nullopt;
        if (!is_token) {
            state_.player(obj.owner).trash.push_back(target);
        } else {
            events_.logTrace("  TOKEN CEASES TO EXIST (CR 183.1): " + obj.name +
                             " (id=" + std::to_string(target) + ")");
        }

        events_.emit(LeftBoardEvent{target, controller, CardType::Gear,
            was_at.value_or(BaseLocation{controller}),
            is_token ? ZoneType::Banishment : ZoneType::Trash, true});
    }
}

void EffectExecutor::drawCards(PlayerId player, int count) {
    events_.logTrace(std::string("EFFECT: ") + toString(player) + " draws " +
                     std::to_string(count) + " (deck=" +
                     std::to_string(state_.player(player).main_deck.size()) + ")");
    auto& ps = state_.player(player);
    int drawn = 0;
    for (int i = 0; i < count; ++i) {
        if (ps.main_deck.empty()) {
            if (!burnOutUntilDeckHasCards(player)) {
                if (state_.game_over) return;
                break;
            }
        }
        auto card_id = deck::takeTop(state_, player, deck::Place::Hand);
        ps.hand.push_back(card_id);
        state_.getObject(card_id).zone = ZoneType::Hand;
        events_.logTrace("  DREW: " + state_.getObject(card_id).name +
                         " (id=" + std::to_string(card_id) + ")");
        drawn++;
    }
    if (drawn > 0) {
        state_.player(player).draws_this_turn += drawn;
        events_.emit(CardsDrawnEvent{player, drawn});
    }
}

// Victory Score in effect right now: the mode's score plus what the
// battlefields in play add to it (Card::victoryScoreModifier).
int EffectExecutor::effectiveVictoryScore() const {
    int v = state_.mode.victory_score;
    if (!card_registry_) return v;
    for (const auto& bf : state_.battlefields) {
        if (!state_.objectExists(bf.card_object_id)) continue;
        const auto def_id = state_.getObject(bf.card_object_id).card_def_id;
        if (def_id == kInvalidId) continue;
        if (const Card* card = card_registry_->get(def_id)) v += card->victoryScoreModifier();
    }
    return v;
}

int EffectExecutor::scorePoints(PlayerId player, int amount) {
    if (amount <= 0 || player == PlayerId::None) return 0;
    auto& ps = state_.player(player);
    if (ps.cannot_gain_points) {
        events_.logTrace(std::string("SCORE_BLOCKED: ") + toString(player) +
                         " can't gain points");
        return 0;
    }
    ps.score += amount;
    events_.logTrace(std::string("SCORE: ") + toString(player) + " +" +
                     std::to_string(amount) + " via effect -> " + std::to_string(ps.score));
    events_.emit(PointsGainedEvent{player, amount, ps.score});
    // CR 466.1.a.1: points from effects have no Winning Point restriction.
    const auto& opp = state_.player(opponent(player));
    if (!state_.game_over && ps.score >= effectiveVictoryScore() && ps.score > opp.score) {
        state_.game_over = true;
        state_.winner = player;
        state_.game_over_reason = std::string(toString(player)) + " reached " +
                                   std::to_string(ps.score) + " points";
        events_.emit(GameOverEvent{player, state_.game_over_reason});
    }
    return amount;
}

int deflectCostFor(const GameState& state, PlayerId chooser, GameObjectId target) {
    if (!state.objectExists(target)) return 0;
    const auto& obj = state.getObject(target);
    if (obj.controller == chooser || !obj.location.has_value()) return 0;
    if (!obj.hasKeyword(Keyword::Deflect) && obj.deflect_value <= 0) return 0;
    // Heisho: "Players ignore [Deflect] while paying for spells and abilities
    // choosing something here."
    if (auto bf = obj.battlefieldId())
        for (const auto& b : state.battlefields)
            if (b.id == *bf && b.ignore_deflect_here) return 0;
    int value = obj.deflect_value + temporaryDeflect(state, obj);
    for (const auto& ae : obj.aura_effects)
        if (ae.keyword == Keyword::Deflect) value += std::max(1, ae.keyword_value);
    return std::max(1, value);
}

int payableAnyPower(const GameState& state, PlayerId player) {
    const auto& pool = state.player(player).rune_pool;
    int n = pool.universal_power;
    for (int di = 0; di < static_cast<int>(Domain::Count); ++di) n += pool.power[di];
    LocationId base{BaseLocation{player}};
    for (const auto& [id, obj] : state.objects)
        if (obj.isRune() && obj.controller == player &&
            obj.location.has_value() && *obj.location == base) ++n;
    return n;
}

int EffectExecutor::deflectCostFor(PlayerId chooser, GameObjectId target) const {
    return ::riftbound::deflectCostFor(state_, chooser, target);
}

int EffectExecutor::payableAnyPower(PlayerId player) const {
    return ::riftbound::payableAnyPower(state_, player);
}

bool EffectExecutor::payAnyPower(PlayerId player, int amount) {
    if (amount <= 0) return true;
    if (payableAnyPower(player) < amount) return false;
    auto& ps = state_.player(player);
    auto take = [&amount](int& from) {
        int n = std::min(from, amount);
        from -= n;
        amount -= n;
    };
    take(ps.rune_pool.universal_power);
    for (int di = 0; di < static_cast<int>(Domain::Count); ++di) take(ps.rune_pool.power[di]);
    if (amount <= 0) return true;
    // Recycle runes, exhausted ones first (they have nothing else to give).
    LocationId base{BaseLocation{player}};
    std::vector<GameObjectId> runes;
    for (auto& [id, obj] : state_.objects)
        if (obj.isRune() && obj.controller == player &&
            obj.location.has_value() && *obj.location == base) runes.push_back(id);
    std::stable_sort(runes.begin(), runes.end(), [&](GameObjectId a, GameObjectId b) {
        return state_.getObject(a).is_exhausted > state_.getObject(b).is_exhausted;
    });
    for (auto rid : runes) {
        if (amount <= 0) break;
        auto& rune = state_.getObject(rid);
        rune.location = std::nullopt;
        rune.zone = ZoneType::RuneDeck;
        rune.is_exhausted = false;
        ps.rune_deck.insert(ps.rune_deck.begin(), rid);
        ps.power_spent_this_turn++;
        events_.emit(LeftBoardEvent{rid, player, CardType::Rune, base,
                                    ZoneType::RuneDeck, false});
        --amount;
    }
    return amount <= 0;
}

bool EffectExecutor::burnOut(PlayerId player) {
    auto& ps = state_.player(player);
    // CR 432.2: recycle the trash into the Main Deck (possibly nothing —
    // CR 432.3: the deck may stay empty), then an opponent gains 1 point.
    // 1v1 has exactly one opponent to choose.
    ps.burned_out = true;
    // The trash is public, so both players know every card that goes in;
    // the shuffle leaves nobody knowing where.
    const auto recycled = ps.trash;
    ps.trash.clear();
    for (auto cid : recycled) deck::putToShuffle(state_, player, cid, deck::Place::Public);
    if (rng_) {
        deck::shuffle(state_, player, *rng_);
    } else {
        knowledge::shuffled(state_, player);
    }
    PlayerId opp_id = opponent(player);
    auto& opp_ps = state_.player(opp_id);
    // The burn-out point is a point gained like any other: "can't gain
    // points" (Tianna Crownguard) stops it, and it is announced.
    if (opp_ps.cannot_gain_points) {
        events_.logTrace(std::string("BURN_OUT: ") + toString(player) +
                         (ps.main_deck.empty() ? " deck and trash empty; "
                                               : " deck empty, shuffled trash; ") +
                         toString(opp_id) + " can't gain points");
        return !ps.main_deck.empty();
    }
    opp_ps.score++;
    events_.logTrace(std::string("BURN_OUT: ") + toString(player) +
                     (ps.main_deck.empty() ? " deck and trash empty; "
                                           : " deck empty, shuffled trash; ") +
                     toString(opp_id) + " gains 1 point (CR 432.2.c) -> " +
                     std::to_string(opp_ps.score));
    events_.emit(PointsGainedEvent{opp_id, 1, opp_ps.score});
    // CR 432.3.c.1: a burn-out point that reaches the Victory Score with more
    // points than every opponent wins immediately, without waiting for cleanup.
    if (opp_ps.score >= effectiveVictoryScore() &&
        opp_ps.score > ps.score) {
        state_.game_over = true;
        state_.winner = opp_id;
        state_.game_over_reason = std::string(toString(opp_id)) +
                                   " wins via burn-out point (CR 432.3)";
        events_.emit(GameOverEvent{opp_id, state_.game_over_reason});
        return false;
    }
    return !ps.main_deck.empty();
}

// CR 432.3 / 432.3.a: with nothing to recycle the deck stays empty, and trying
// to finish the original action burns out again — repeatedly, a point each
// time, until the opponent wins. Returns true once the deck has a card;
// false if the game ended.
bool EffectExecutor::burnOutUntilDeckHasCards(PlayerId player) {
    auto& ps = state_.player(player);
    // The opponent gains a point per pass, so this always terminates; the
    // bound is only a guard against a state where nobody can win.
    const auto& opp_ps = state_.player(opponent(player));
    for (int guard = 0; ps.main_deck.empty() && !state_.game_over && guard < 256; ++guard) {
        const int score_before = opp_ps.score;
        burnOut(player);
        // No card came back and no point was gained (the opponent can't gain
        // points): another pass would change nothing.
        if (ps.main_deck.empty() && opp_ps.score == score_before) break;
    }
    return !ps.main_deck.empty() && !state_.game_over;
}

// "If a spell or ability that chooses me would <stun me / give me -[M] / return
// me to hand>, <do something else> instead." Applies only to effects of the
// chain item now resolving, and only if that item chose `target`.
static bool hostileEffectReplaced(GameState& state, EventBus& events, EffectExecutor& exec,
                                  const CardRegistry* registry, GameObjectId target,
                                  Card::HostileEffect kind) {
    if (!registry || !state.chain.resuming.has_value() || !state.objectExists(target)) return false;
    const auto& item = *state.chain.resuming;
    const bool chose = std::find(item.chosen_objects.begin(), item.chosen_objects.end(), target)
                           != item.chosen_objects.end() ||
                       std::find(item.targets.begin(), item.targets.end(), target)
                           != item.targets.end();
    if (!chose) return false;
    const auto& obj = state.getObject(target);
    if (obj.card_def_id == kInvalidId || !obj.location.has_value()) return false;
    Card* card = registry->get(obj.card_def_id);
    if (!card) return false;
    CardContext rctx{state, events, exec, obj.controller, target};
    return card->replaceHostileEffect(rctx, target, kind);
}

bool EffectExecutor::empower(GameObjectId target) {
    if (!state_.objectExists(target)) return false;
    auto& obj = state_.getObject(target);
    // Empowered is a status of objects on the board / in the Legend Zone (CR 442.2).
    if (!obj.location.has_value() && obj.zone != ZoneType::LegendZone) return false;
    int cap = 1;
    if (card_registry_ && obj.card_def_id != kInvalidId)
        if (const Card* c = card_registry_->get(obj.card_def_id)) cap = c->maxEmpower();
    if (obj.empowered >= cap) return false;   // CR 442.1.c: nothing additional happens
    obj.empowered++;
    events_.logTrace("EFFECT: empower " + obj.name + " (id=" + std::to_string(target) +
                     ") -> " + std::to_string(obj.empowered));
    events_.emit(ObjectStateChangedEvent{target, "empowered"});
    return true;
}

bool EffectExecutor::disempower(GameObjectId target) {
    if (!state_.objectExists(target)) return false;
    auto& obj = state_.getObject(target);
    if (obj.empowered <= 0) return false;     // CR 443.1.a.1
    obj.empowered--;
    events_.logTrace("EFFECT: disempower " + obj.name + " (id=" + std::to_string(target) +
                     ") -> " + std::to_string(obj.empowered));
    events_.emit(ObjectStateChangedEvent{target, "disempowered"});
    return true;
}

std::vector<GameObjectId> EffectExecutor::burn(PlayerId player, int count) {
    auto& ps = state_.player(player);
    std::vector<GameObjectId> burned;
    for (int i = 0; i < count; ++i) {
        // CR 441.4: burn as many as possible, burn out, then burn the rest.
        // The just-burned cards are in the trash, so a burn out reshuffles them.
        if (ps.main_deck.empty() && !burnOutUntilDeckHasCards(player)) break;
        auto card_id = deck::takeTop(state_, player, deck::Place::Public);
        auto& card = state_.getObject(card_id);
        card.zone = ZoneType::Trash;
        card.location = std::nullopt;
        ps.trash.push_back(card_id);
        ps.cards_burned_this_turn++;
        burned.push_back(card_id);
        events_.logTrace(std::string("  BURNED: ") + toString(player) + " " + card.name +
                         " (id=" + std::to_string(card_id) + ")");
    }
    // "When you burn" abilities execute after burning has occurred (CR 441.1.a).
    for (auto cid : burned) events_.emit(ObjectStateChangedEvent{cid, "burned"});
    return burned;
}

// CR 719.5: gear attached to a unit that leaves the board is shed first, and
// gear that itself leaves stops being attached.
static void shedAttachments(GameState& state, EffectExecutor& exec, GameObjectId id) {
    if (!state.objectExists(id)) return;
    auto& obj = state.getObject(id);
    if (obj.attached_to.has_value()) exec.unattachGear(id);
    const auto attached = obj.attachments;          // copy: unattachGear edits it
    for (auto gid : attached) exec.unattachGear(gid);
}

void EffectExecutor::bounceToHand(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    // Returning to hand takes something off the board; a card already in a
    // hand, deck, trash or banishment stays where it is.
    if (!onBoard(state_.getObject(target))) return;
    if (hostileEffectReplaced(state_, events_, *this, card_registry_, target,
                              Card::HostileEffect::ReturnToHand)) return;
    shedAttachments(state_, *this, target);
    auto& obj = state_.getObject(target);
    auto was_at = obj.location;
    auto controller = obj.controller;

    // CR 183.1: "If a token is put into any Non-Board Zone besides the
    // chain, it ceases to exist immediately after moving to its new
    // zone." So a bounced token never actually lands in hand — it's
    // gone the moment it leaves the board. We keep the GameObject
    // alive in state.objects (so any in-flight references stay valid)
    // but route it to Banishment and skip the player's hand vector,
    // matching "ceases to exist" semantically.
    if (obj.isToken()) {
        events_.logTrace("EFFECT: bounce " + obj.name + " (id=" +
                         std::to_string(target) +
                         ") — token ceases to exist (CR 183.1)");
        obj.zone = ZoneType::Banishment;
        obj.location = std::nullopt;
        obj.damage_marked = 0;
        obj.combat_designation = CombatDesignation::None;
        obj.is_exhausted = false;
        events_.emit(LeftBoardEvent{target, controller, obj.card_type,
            was_at.value_or(BaseLocation{controller}), ZoneType::Banishment, false});
        return;
    }

    events_.logTrace("EFFECT: bounce " + obj.name + " (id=" + std::to_string(target) + ") to hand");
    obj.zone = ZoneType::Hand;
    obj.location = std::nullopt;
    obj.damage_marked = 0;
    obj.combat_designation = CombatDesignation::None;
    obj.is_exhausted = false;
    state_.player(obj.owner).hand.push_back(target);

    events_.emit(LeftBoardEvent{target, controller, obj.card_type,
        was_at.value_or(BaseLocation{controller}), ZoneType::Hand, false});

    // "When a unit here is returned to hand" (Ripper's Bay). Only fires if
    // the unit was at a battlefield when bounced.
    BattlefieldId from_bf = kInvalidId;
    if (was_at.has_value() && std::holds_alternative<BattlefieldLocation>(*was_at)) {
        from_bf = std::get<BattlefieldLocation>(*was_at).id;
    }
    if (from_bf != kInvalidId) {
        events_.emit(UnitReturnedToHandEvent{target, obj.owner, from_bf});
    }
}

void EffectExecutor::giveTemporaryMight(GameObjectId target, int amount, int minimum) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);

    if (amount < 0 && hostileEffectReplaced(state_, events_, *this, card_registry_, target,
                                            Card::HostileEffect::ReduceMight)) return;

    // Mel, Newly Awakened: "If a spell or ability you control would give -[M]
    // to a unit it chooses, it gives an additional -1 [M]."
    if (amount < 0 && state_.chain.resuming.has_value()) {
        amount -= state_.player(state_.chain.resuming->controller).extra_minus_might;
    }

    // This-turn temporary might only — does NOT touch buff_count (permanent
    // buff counters). Expires at the expiration step (temp_might_bonus zeroed).
    obj.temp_might_bonus += amount;
    obj.recomputeMight();

    // Enforce minimum (e.g., "to a minimum of 1 [M]"). recomputeMight clamps
    // current_might at 0, so a debuff that drives the raw total below 0 would
    // make a correction computed from the clamped value too small. Compute the
    // correction from the UNCLAMPED raw might instead.
    if (minimum > 0) {
        int raw = obj.base_might + obj.buff_count + obj.temp_might_bonus +
                  obj.attachment_might_bonus + obj.aura_might_bonus;
        if (raw < minimum) {
            obj.temp_might_bonus += (minimum - raw);
            obj.recomputeMight();
        }
    }

    events_.logTrace("EFFECT: give " + obj.name + " " + std::to_string(amount) +
                     " [M] this turn -> " + std::to_string(obj.current_might) + "M" +
                     (minimum > 0 ? " (min " + std::to_string(minimum) + ")" : ""));
    // Not "buffed": +/-[M] this turn places no Buff counter (CR 427.1), so it
    // must not fire "When you buff" abilities.
    events_.emit(ObjectStateChangedEvent{target, "might_changed"});
}

void EffectExecutor::giveTemporaryKeyword(GameObjectId target,
                                            Keyword kw, int value) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    events_.logTrace("EFFECT: give " + obj.name + " [" + toString(kw) + "]" +
                     (value > 0 ? " (value=" + std::to_string(value) + ")" : "") +
                     " this turn");
    obj.keywords.set(kw);
    // Track the "this turn" provenance so expirationStep can revoke
    // pure-flag keywords (Ganking, Tank, Backline, etc.). Parameterised
    // keywords (Assault/Shield) already get their values reset in the
    // expiration body via temp_assault_value / temp_shield_value, and
    // that code path independently clears the keyword bit if the value
    // returns to 0 AND the CardDef doesn't print it. Tagging temp_keywords
    // here doesn't conflict with that flow — both paths converge on
    // "clear the bit if neither base nor aura still grants it".
    obj.temp_keywords.set(kw);
    if (kw == Keyword::Assault) { obj.assault_value += value; obj.temp_assault_value += value; }
    else if (kw == Keyword::Shield) { obj.shield_value += value; obj.temp_shield_value += value; }
    else if (kw == Keyword::Deflect) {
        // No temp_* field to expire, so the grant is stamped with this turn
        // (see temporaryDeflect); two grants in one turn add up.
        const int turn = state_.turn.turn_number;
        auto& stamp = obj.card_counters[kTempDeflectTurn];
        auto& granted = obj.card_counters[kTempDeflect];
        if (stamp != turn) granted = 0;
        stamp = turn;
        granted += value;
    }
    obj.recomputeMight();
    // Not "buffed": a keyword given this turn is not a Buff counter (CR 427.1).
    events_.emit(ObjectStateChangedEvent{target, "keyword_granted"});
}

bool EffectExecutor::buffUnit(GameObjectId target) {
    // [Buff] = a PERMANENT +1 [M] counter (CR), not this-turn temp might. It
    // persists past the expiration step and is what "spend a buff" / "if it
    // has a buff" gates count.
    if (!state_.objectExists(target)) return false;
    auto& obj = state_.getObject(target);
    if (!onBoard(obj)) return false;
    // CR 427.1.b / 702.3: a unit holds one Buff at a time. A unit that already
    // has one can still be chosen but is not buffed (427.1.c) — nothing is
    // placed and "When you buff" abilities don't trigger — unless its card
    // says it can have any number (427.1.b.2: Lee Sin, Ascetic).
    if (obj.buff_count > 0) {
        bool any_number = false;
        if (card_registry_ && obj.card_def_id != kInvalidId)
            if (const Card* c = card_registry_->get(obj.card_def_id))
                any_number = c->canHaveAnyNumberOfBuffs();
        if (!any_number) {
            events_.logTrace("EFFECT: [Buff] " + obj.name +
                             " already has a buff — not buffed (CR 427.1.b.1)");
            return false;
        }
    }
    obj.buff_count += 1;
    obj.recomputeMight();
    events_.logTrace("EFFECT: [Buff] " + obj.name + " (+1 permanent) -> " +
                     std::to_string(obj.current_might) + "M");
    events_.emit(ObjectStateChangedEvent{target, "buffed"});
    return true;
}

void EffectExecutor::readyObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    // Only something in play is ready or exhausted: an object on the board, a
    // legend (in its Legend Zone, which is not a location) or a battlefield.
    if (!onBoard(obj) && !obj.isLegend() && !obj.isBattlefield()) return;
    // Mageseeker Warden cl2: "spells and abilities can't ready enemy units and
    // gear." The flag is set on the restricted player; readyObject is the
    // effect-readying path (Awaken readies directly, bypassing this), so this
    // refuses only effect/spell-driven readying of units & gear.
    if ((obj.isUnit() || obj.isGear()) &&
        state_.player(obj.controller).effects_cant_ready_my_units) {
        events_.logTrace("READY_SUPPRESSED: " + obj.name +
                         " can't be readied by effects (Mageseeker Warden)");
        return;
    }
    bool was_exhausted = obj.is_exhausted;
    obj.is_exhausted = false;
    events_.emit(ObjectStateChangedEvent{target, "readied"});
    // Fire the "when you ready me" trigger (Irelia, Fervent) only on an
    // actual exhausted->ready transition, so re-readying a ready unit is a
    // no-op for triggers.
    if (was_exhausted) {
        events_.emit(UnitReadiedEvent{target, obj.controller});
    }
}

void EffectExecutor::unattachGear(GameObjectId gear) {
    if (!state_.objectExists(gear)) return;
    auto& g = state_.getObject(gear);
    if (!g.attached_to.has_value()) return;
    GameObjectId unit_id = *g.attached_to;
    if (state_.objectExists(unit_id)) {
        auto& unit = state_.getObject(unit_id);
        unit.attachment_might_bonus -= g.might_bonus;
        auto& att = unit.attachments;
        att.erase(std::remove(att.begin(), att.end(), gear), att.end());
        unit.recomputeMight();
    }
    g.attached_to = std::nullopt;
    g.is_rules_text_inactive = false;
    // "For as long as this is attached" effects end here (Shady Spectacles).
    if (card_registry_ && g.card_def_id != kInvalidId) {
        if (Card* gc = card_registry_->get(g.card_def_id)) {
            CardContext uctx{state_, events_, *this, g.controller, gear};
            gc->onUnattached(uctx, unit_id);
        }
    }
    // CR 719.5 — detached gear stays at its current location, unattached.
    events_.logTrace("EFFECT: unattach " + g.name + " (id=" + std::to_string(gear) + ")");
    events_.emit(ObjectStateChangedEvent{gear, "detached"});
}

void EffectExecutor::moveToBase(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    // CR 421.1: a move is between two locations on the board. Something that
    // has left the board can't be moved back onto it, and "moving" to where
    // it already is moves nothing.
    if (!onBoard(obj)) return;
    if (*obj.location == LocationId{BaseLocation{obj.controller}}) return;
    // Jagged Cutlass: "can't be moved by enemy spells and abilities."
    if (obj.cant_be_moved_by_enemy && state_.chain.resuming.has_value() &&
        state_.chain.resuming->controller != obj.controller) {
        events_.logTrace("MOVE_BLOCKED: " + obj.name + " can't be moved by enemy effects");
        return;
    }
    auto old_loc = obj.location;

    LocationId dest = BaseLocation{obj.controller};
    // Same bookkeeping as GameEngine::moveUnit: the move counts toward "moved
    // N times this turn" (Kayn, Unleashed) and attached gear comes along.
    obj.moves_this_turn++;
    obj.location = dest;
    obj.zone = ZoneType::Base;
    carryAttachments(state_, obj);

    events_.emit(UnitMovedEvent{target, obj.controller,
        old_loc.value_or(BaseLocation{obj.controller}), dest, false});
}

void EffectExecutor::moveToBattlefield(GameObjectId target, BattlefieldId bf) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    // CR 421.1, as in moveToBase.
    if (!onBoard(obj)) return;
    if (*obj.location == LocationId{BattlefieldLocation{bf}}) return;
    // Jagged Cutlass: "can't be moved by enemy spells and abilities."
    if (obj.cant_be_moved_by_enemy && state_.chain.resuming.has_value() &&
        state_.chain.resuming->controller != obj.controller) {
        events_.logTrace("MOVE_BLOCKED: " + obj.name + " can't be moved by enemy effects");
        return;
    }
    auto old_loc = obj.location;

    LocationId dest = BattlefieldLocation{bf};
    obj.moves_this_turn++;
    obj.location = dest;
    obj.zone = ZoneType::BattlefieldZone;
    carryAttachments(state_, obj);

    events_.emit(UnitMovedEvent{target, obj.controller,
        old_loc.value_or(BaseLocation{obj.controller}), dest, false});
}

void EffectExecutor::stunUnit(GameObjectId target) {
    stunUnitBy(target, target);
}

void EffectExecutor::stunUnitBy(GameObjectId target, GameObjectId stunner_source) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);

    if (!onBoard(obj)) return;
    // CR 423: already stunned units can't be stunned again
    if (obj.is_stunned) return;
    if (hostileEffectReplaced(state_, events_, *this, card_registry_, target,
                              Card::HostileEffect::Stun)) return;
    events_.logTrace("EFFECT: stun " + obj.name + " (id=" + std::to_string(target) + ")");

    obj.is_stunned = true;
    events_.emit(ObjectStateChangedEvent{target, "stunned"});

    // Typed event for the WhenYouStun trigger. `stunner` is the controller
    // of the card/effect that applied the stun (caller passes stunner_source
    // GameObjectId; we resolve to controller). When the source is the unit
    // itself (legacy single-arg stunUnit), stunner_controller == victim_controller
    // and the trigger correctly no-ops (you don't stun your own unit "as
    // the enemy" — the WhenYouStun trigger only fires when victim is enemy).
    PlayerId stunner_ctrl = obj.controller;
    if (stunner_source != target && state_.objectExists(stunner_source)) {
        stunner_ctrl = state_.getObject(stunner_source).controller;
    }
    events_.emit(UnitStunnedEvent{target, stunner_ctrl, obj.controller});
}

void EffectExecutor::discardCards(PlayerId player, int count) {
    events_.logTrace(std::string("EFFECT: ") + toString(player) + " discards " +
                     std::to_string(count) + " (hand=" +
                     std::to_string(state_.player(player).hand.size()) + ")");
    auto& ps = state_.player(player);
    std::vector<GameObjectId> discarded;

    for (int i = 0; i < count && !ps.hand.empty(); ++i) {
        GameObjectId to_discard = kInvalidId;

        // CR 422.1.a — discarding always prompts even when only one card
        // is in hand. The decision is forced (single option) but surfacing
        // it through the agent gives consistent decision-counter / trace
        // semantics. Phase 6q+ fix: pre-fix the 1-card case skipped the
        // prompt and silent-discarded back-of-hand, breaking V&V scans
        // for discard-choice trace lines.
        if (agent_query_ && !ps.hand.empty()) {
            std::vector<Intent> choices;
            for (auto card_id : ps.hand) {
                Intent choice;
                choice.type = IntentType::MakeChoice;
                choice.player = player;
                choice.chosen_objects = {card_id};
                choices.push_back(choice);
            }
            auto chosen = agent_query_(player, choices);
            if (!chosen.chosen_objects.empty()) {
                to_discard = chosen.chosen_objects[0];
            }
        }

        // Fallback: discard last card in hand (legacy path when no
        // agent is installed — applies to direct-invocation test scenes).
        if (to_discard == kInvalidId) {
            to_discard = ps.hand.back();
        }

        auto it = std::find(ps.hand.begin(), ps.hand.end(), to_discard);
        if (it != ps.hand.end()) {
            events_.logTrace("  DISCARDED: " + state_.getObject(to_discard).name +
                             " (id=" + std::to_string(to_discard) + ")");
            ps.hand.erase(it);
            state_.getObject(to_discard).zone = ZoneType::Trash;
            ps.trash.push_back(to_discard);
            discarded.push_back(to_discard);
        }
    }

    if (!discarded.empty()) {
        ps.has_discarded_this_turn = true;
        events_.emit(CardsDiscardedEvent{player, discarded});
        for (auto did : discarded) events_.emit(ObjectStateChangedEvent{did, "discarded"});
    }
}

void EffectExecutor::opponentDiscards(PlayerId opp, int count) {
    events_.logTrace(std::string("EFFECT: ") + toString(opp) + " forced to discard " +
                     std::to_string(count));
    discardCards(opp, count);
}

void EffectExecutor::applyDiscard(PlayerId player, GameObjectId card_id) {
    if (!state_.objectExists(card_id)) return;
    auto& ps = state_.player(player);
    auto it = std::find(ps.hand.begin(), ps.hand.end(), card_id);
    if (it == ps.hand.end()) return;
    auto& obj = state_.getObject(card_id);
    events_.logTrace("  DISCARDED: " + obj.name + " (id=" +
                     std::to_string(card_id) + ")");
    ps.hand.erase(it);
    obj.zone = ZoneType::Trash;
    ps.trash.push_back(card_id);
    ps.has_discarded_this_turn = true;
    events_.emit(CardsDiscardedEvent{player, {card_id}});
    events_.emit(ObjectStateChangedEvent{card_id, "discarded"});
}

void EffectExecutor::recycleCards(PlayerId effect_controller,
                                   const std::vector<GameObjectId>& cards) {
    // CR 416.1.c: each card goes to the BOTTOM of its OWNER'S main deck,
    // not the effect-controller's. Phase 6q+ fix — pre-fix the
    // effect_controller's deck was used, which meant Sabotage-style
    // "recycle opponent's card" effects mistakenly pushed the opponent's
    // card onto the caster's deck. Iterate per owner so each card lands
    // in the correct deck.
    //
    // CR 416.5 — Main Deck multi-recycle is RANDOM order.
    // CR 416.5.a — Rune Deck multi-recycle is OWNER'S CHOICE; we use
    // input order as the choice (caller decides; no shuffle).
    auto to_recycle = cards;
    // Partition: shuffle only the non-rune subset (per CR 416.5).
    std::vector<GameObjectId> shuffleable;
    std::vector<GameObjectId> rune_ordered;
    for (auto cid : to_recycle) {
        if (!state_.objectExists(cid)) continue;
        if (state_.getObject(cid).card_type == CardType::Rune) {
            rune_ordered.push_back(cid);
        } else {
            shuffleable.push_back(cid);
        }
    }
    if (rng_) {
        std::shuffle(shuffleable.begin(), shuffleable.end(), *rng_);
    }
    // Main Deck cards: each owner's cards go down together, in the shuffled
    // order. Where each came from decides who can identify it (a card from
    // the board or trash is known to both players, one from a hand is not).
    for (PlayerId owner : {PlayerId::Player1, PlayerId::Player2}) {
        std::vector<deck::Placed> group;
        for (auto cid : shuffleable) {
            // A card with no owner (only in hand-built test states) goes to
            // the deck of the player whose effect it is.
            PlayerId to = state_.getObject(cid).owner;
            if (to == PlayerId::None) to = effect_controller;
            if (to != owner) continue;
            group.push_back({cid, deck::whereIs(state_, cid), knowledge::nobody()});
        }
        deck::putOnBottomTogether(state_, owner, group);
    }
    for (auto cid : rune_ordered) {
        auto& obj = state_.getObject(cid);
        obj.zone = ZoneType::RuneDeck;
        obj.location = std::nullopt;
        state_.player(obj.owner).rune_deck.insert(
            state_.player(obj.owner).rune_deck.begin(), cid);
    }

    // "When you recycle one or more cards to your Main Deck" (Karma, Channeler
    // 235). Fires once per OWNER who had >=1 non-rune card land in their Main
    // Deck (runes recycle to the Rune Deck and don't count — CR text: "Runes
    // aren't cards."). Announce via the generic ObjectStateChangedEvent so
    // TriggerManager dispatches WhenYouRecycle on that owner's on-board cards.
    std::vector<PlayerId> announced;
    for (auto cid : shuffleable) {
        if (!state_.objectExists(cid)) continue;
        auto& obj = state_.getObject(cid);
        if (obj.zone != ZoneType::MainDeck) continue;
        if (std::find(announced.begin(), announced.end(), obj.owner) != announced.end())
            continue;
        announced.push_back(obj.owner);
        events_.emit(ObjectStateChangedEvent{cid, "recycled_main"});
    }
}

void EffectExecutor::banishObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    shedAttachments(state_, *this, target);
    auto& obj = state_.getObject(target);
    events_.logTrace("EFFECT: banish " + obj.name + " (id=" + std::to_string(target) + ")");
    auto was_at = obj.location;
    auto controller = obj.controller;

    // Leaving from a non-board zone: drop it from that zone's list.
    {
        auto& ops = state_.player(obj.owner);
        auto drop = [&](std::vector<GameObjectId>& v) {
            v.erase(std::remove(v.begin(), v.end(), target), v.end());
        };
        if (obj.zone == ZoneType::Trash) drop(ops.trash);
        else if (obj.zone == ZoneType::Hand) drop(ops.hand);
        else if (obj.zone == ZoneType::MainDeck) deck::take(state_, target, deck::Place::Public);
    }
    const bool is_token = obj.isToken();
    obj.zone = ZoneType::Banishment;
    obj.location = std::nullopt;
    // CR 183.1: a token ceases to exist instead of sitting in banishment.
    if (!is_token) {
        state_.player(obj.owner).banishment.push_back(target);
        state_.player(obj.owner).cards_banished_this_turn++;
    }

    if (was_at.has_value())
        events_.emit(LeftBoardEvent{target, controller, obj.card_type,
            *was_at, ZoneType::Banishment, false});
    // "When you banish a card you own" (Master of Shadows).
    if (!is_token) events_.emit(ObjectStateChangedEvent{target, "banished_owned"});
}

void EffectExecutor::healObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    obj.damage_marked = 0;
    events_.emit(ObjectStateChangedEvent{target, "healed"});
}

void EffectExecutor::exhaustObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    obj.is_exhausted = true;
    events_.emit(ObjectStateChangedEvent{target, "exhausted"});
}

void EffectExecutor::channelRunes(PlayerId player, int count, bool enter_exhausted) {
    events_.logTrace(std::string("EFFECT: ") + toString(player) + " channels " +
                     std::to_string(count) + " runes" +
                     (enter_exhausted ? " exhausted" : ""));
    auto& ps = state_.player(player);
    for (int i = 0; i < count; ++i) {
        if (ps.rune_deck.empty()) break;
        auto rune_id = ps.rune_deck.back();
        ps.rune_deck.pop_back();

        auto& rune = state_.getObject(rune_id);
        rune.zone = ZoneType::Base;
        rune.location = BaseLocation{player};
        rune.is_exhausted = enter_exhausted;

        events_.logTrace("  CHANNELED: " + rune.name + " (id=" +
                         std::to_string(rune_id) +
                         (enter_exhausted ? ", exhausted" : ", ready") + ")");
        events_.emit(RuneChanneledEvent{rune_id, player});
        events_.emit(EnteredBoardEvent{
            rune_id, player, CardType::Rune, BaseLocation{player}, false});
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Add abilities (CR 429) — floating energy / power / universal power
// ═══════════════════════════════════════════════════════════════════════════════

GameObjectId EffectExecutor::chooseObjectNow(PlayerId player,
                                              const std::vector<GameObjectId>& objects) {
    if (objects.empty()) return kInvalidId;
    if (!agent_query_) return objects.front();
    std::vector<Intent> options;
    options.reserve(objects.size());
    for (auto id : objects) {
        Intent i;
        i.type = IntentType::MakeChoice;
        i.player = player;
        i.chosen_objects = {id};
        options.push_back(std::move(i));
    }
    const Intent chosen = agent_query_(player, options);
    if (!chosen.chosen_objects.empty() &&
        std::find(objects.begin(), objects.end(), chosen.chosen_objects.front()) != objects.end())
        return chosen.chosen_objects.front();
    return objects.front();
}

void EffectExecutor::addUnitOnlyEnergy(PlayerId player, int amount) {
    state_.player(player).rune_pool.unit_energy += amount;
    events_.logTrace("EFFECT: " + std::string(toString(player)) + " adds " +
                     std::to_string(amount) + " Energy for units / unit abilities only");
}

void EffectExecutor::addFloatingEnergy(PlayerId player, int amount) {
    state_.player(player).rune_pool.energy += amount;
    events_.logTrace("EFFECT: " + std::string(toString(player)) + " adds " +
                     std::to_string(amount) + " floating Energy (pool=" +
                     std::to_string(state_.player(player).rune_pool.energy) + ")");
}

void EffectExecutor::addFloatingPower(PlayerId player, Domain domain, int amount) {
    int idx = static_cast<int>(domain);
    state_.player(player).rune_pool.power[idx] += amount;
    events_.logTrace("EFFECT: " + std::string(toString(player)) + " adds " +
                     std::to_string(amount) + " floating " + toString(domain) +
                     " Power (pool=" +
                     std::to_string(state_.player(player).rune_pool.power[idx]) + ")");
}

void EffectExecutor::addFloatingUniversalPower(PlayerId player, int amount) {
    state_.player(player).rune_pool.universal_power += amount;
    events_.logTrace("EFFECT: " + std::string(toString(player)) + " adds " +
                     std::to_string(amount) + " floating [A] (pool=" +
                     std::to_string(state_.player(player).rune_pool.universal_power) +
                     ")");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Take control (CR 416, "take control of")
// ═══════════════════════════════════════════════════════════════════════════════

void EffectExecutor::takeControl(GameObjectId target, PlayerId new_controller,
                                  bool until_end_of_turn) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    if (obj.controller == new_controller) return;  // no-op
    if (until_end_of_turn && !obj.control_reverts_eot) {
        // Snapshot only ONCE — if multiple take-control effects stack
        // during the same turn, the snapshot keeps the EARLIEST owner
        // so revert at end of turn returns to the original.
        obj.original_controller_eot = obj.controller;
        obj.control_reverts_eot = true;
    }
    auto old_controller = obj.controller;
    obj.controller = new_controller;
    events_.logTrace("EFFECT: " + obj.name + " (id=" + std::to_string(target) +
                     ") control: " + toString(old_controller) + " -> " +
                     toString(new_controller) +
                     (until_end_of_turn ? " (this turn)" : " (permanent)"));
    events_.emit(ObjectStateChangedEvent{target, "controller_changed"});
}

void EffectExecutor::takeControlUntilSourceLeaves(GameObjectId target,
                                                  PlayerId new_controller,
                                                  GameObjectId source) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    // Snapshot the restore-controller once (so re-applications keep the
    // earliest owner), then flip control.
    if (!obj.control_reverts_on_source_leave) {
        obj.control_revert_to = obj.controller;
        obj.control_reverts_on_source_leave = true;
        obj.control_source_obj = source;
    }
    if (obj.controller == new_controller) return;  // already controlled
    auto old_controller = obj.controller;
    obj.controller = new_controller;
    events_.logTrace("EFFECT: " + obj.name + " (id=" + std::to_string(target) +
                     ") control: " + toString(old_controller) + " -> " +
                     toString(new_controller) + " (until source id=" +
                     std::to_string(source) + " leaves board)");
    events_.emit(ObjectStateChangedEvent{target, "controller_changed"});
}

// ═══════════════════════════════════════════════════════════════════════════════
// Copy effects
// ═══════════════════════════════════════════════════════════════════════════════

void EffectExecutor::copyUnit(GameObjectId token_id, GameObjectId source_id) {
    if (!state_.objectExists(token_id) || !state_.objectExists(source_id)) return;

    auto& token = state_.getObject(token_id);
    auto& src = state_.getObject(source_id);

    events_.logTrace("COPY: " + token.name + " (id=" + std::to_string(token_id) +
                     ") becomes copy of " + src.name + " (id=" + std::to_string(source_id) + ")");

    // Copy PRINTED traits (CR 472.1.b.3) — look up from CardDB when
    // src has a card_def_id, so a copy of a buffed/aura'd unit takes its
    // pristine printed values, not the live state. Falls back to src's
    // live values for token sources (no CardDef). Phase 6q+ fix.
    // CR 185.1: whether the copier is a token is intrinsic — pin it before its
    // supertype is overwritten by the copied one.
    token.is_token = token.isToken();
    token.card_def_id = src.card_def_id;
    if (src.card_def_id != kInvalidId) {
        const auto& def = card_db_.get(src.card_def_id);
        token.name = def.name;
        token.card_type = def.card_type;
        token.tags = def.tags;
        token.domains = def.domains;
        token.base_might = def.might;
        token.keywords = def.keywords;
        token.assault_value = def.assault_value;
        token.shield_value = def.shield_value;
        token.deflect_value = def.deflect_value;
        // The copy has the copied supertype (a copy of a Champion is a
        // Champion); whether it is a token is intrinsic and unchanged.
        token.super_type = def.super_type == SuperType::Token ? SuperType::None
                                                              : def.super_type;
    } else {
        token.name = src.name;
        token.card_type = src.card_type;
        token.tags = src.tags;
        token.domains = src.domains;
        token.base_might = src.base_might;
        // Copy effects copy printed traits only (CR 446): leave out keywords
        // the source merely has "this turn".
        token.keywords = src.keywords;
        token.keywords.bits &= ~src.temp_keywords.bits;
        token.super_type = SuperType::None;
        token.assault_value = src.assault_value;
        token.shield_value = src.shield_value;
        token.deflect_value = src.deflect_value;
    }

    // Keep token's own: owner, controller, location, zone, super_type (stays Token)
    // Reset computed values — aura recalculation will rebuild them
    token.current_might = token.base_might;
    token.aura_effects.clear();
    token.aura_might_bonus = 0;
    token.aura_keywords.reset();
    token.recomputeMight();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Token creation
// ═══════════════════════════════════════════════════════════════════════════════

GameObjectId EffectExecutor::createToken(PlayerId controller, CardType type,
                                          const std::string& name, int might,
                                          const std::vector<std::string>& tags,
                                          KeywordSet keywords, LocationId location,
                                          bool enter_ready) {
    auto id = state_.createObject();
    auto& obj = state_.getObject(id);
    obj.owner = controller;
    obj.controller = controller;
    obj.card_type = type;
    obj.super_type = SuperType::Token;
    obj.is_token = true;   // CR 185.1 intrinsic
    obj.name = name;
    obj.base_might = might;
    obj.current_might = might;
    obj.tags = tags;
    obj.keywords = keywords;
    obj.location = location;
    // Renata Glasc, Industrialist: "Your tokens enter ready." Continuous
    // replacement on token creation — a friendly token never enters exhausted
    // while the flag is set (refreshed each cleanup via applyPassiveAura).
    if (state_.player(controller).tokens_enter_ready) enter_ready = true;
    obj.is_exhausted = !enter_ready && type == CardType::Unit; // units enter exhausted unless ready

    if (std::holds_alternative<BaseLocation>(location)) {
        obj.zone = ZoneType::Base;
    } else {
        obj.zone = ZoneType::BattlefieldZone;
    }

    obj.recomputeMight();

    events_.logTrace("TOKEN: created " + name + " (" + std::to_string(might) +
                     "M, id=" + std::to_string(id) + ") for " + toString(controller));

    events_.emit(TokenCreatedEvent{id, controller, type, name, location});
    events_.emit(EnteredBoardEvent{id, controller, type, location, false});

    // Zilean, Time Mage (648): "Once each turn, if you would play a token unit
    // while I'm at a battlefield, you may play that token and an additional copy
    // instead." Approximated as automatic (token creation isn't an agent
    // decision point). Stamp the turn BEFORE the recursive call to bound it to
    // once per turn and to stop the recursion (the copy sees the stamp set).
    if (type == CardType::Unit) {
        auto& ps = state_.player(controller);
        if (ps.zilean_present &&
            ps.zilean_double_token_turn != state_.turn.turn_number) {
            ps.zilean_double_token_turn = state_.turn.turn_number;
            events_.logTrace("ZILEAN: play an additional copy of the token unit");
            createToken(controller, type, name, might, tags, keywords, location, enter_ready);
        }
    }

    return id;
}

BattlefieldId EffectExecutor::addBattlefieldToken(const std::string& name,
                                                    bool accepts_any_inbound) {
    // Create the GameObject that represents the BF card itself. No
    // card_def_id — this is a pure token spawned by an in-engine effect
    // (e.g. Baron Nashor's "add the Baron Pit battlefield token to the
    // board"). Owner/controller stay neutral; a BF doesn't belong to
    // any player until control is established by holding it.
    auto bf_card_id = state_.createObject();
    auto& obj = state_.getObject(bf_card_id);
    obj.owner = PlayerId::None;
    obj.controller = PlayerId::None;
    obj.card_type = CardType::Battlefield;
    obj.super_type = SuperType::Token;
    obj.is_token = true;   // CR 185.1 intrinsic
    obj.name = name;
    obj.zone = ZoneType::BattlefieldZone;

    // Append a fresh BattlefieldState slot. Engine iteration is already
    // dynamic over state_.battlefields, so no other site needs to know
    // the count grew. New slot inherits no controller until a player
    // holds the BF for scoring; `accepts_any_inbound` carries the
    // movement-rule relaxer onto the slot itself.
    BattlefieldState bf;
    bf.id = static_cast<BattlefieldId>(state_.battlefields.size());
    bf.card_object_id = bf_card_id;
    bf.is_token = true;
    bf.accepts_any_inbound = accepts_any_inbound;
    state_.battlefields.push_back(bf);

    events_.logTrace("BF_TOKEN: added '" + name + "' (id=" +
                     std::to_string(bf.id) + ", card=" +
                     std::to_string(bf_card_id) +
                     (accepts_any_inbound ? ", accepts_any_inbound" : "") +
                     ")");
    return bf.id;
}

void EffectExecutor::replaceBattlefieldWithToken(BattlefieldId target_bf,
                                                   const std::string& token_name,
                                                   PlayerId controller,
                                                   CardDefId token_def) {
    // Locate target slot.
    BattlefieldState* slot = nullptr;
    for (auto& b : state_.battlefields) {
        if (b.id == target_bf) { slot = &b; break; }
    }
    if (!slot) return;

    // Banish the original BF card (CR 438): move to its owner's banishment.
    if (state_.objectExists(slot->card_object_id)) {
        auto& orig = state_.getObject(slot->card_object_id);
        PlayerId owner = orig.owner;
        orig.zone = ZoneType::Banishment;
        orig.location = std::nullopt;
        if (owner != PlayerId::None) {
            state_.player(owner).banishment.push_back(slot->card_object_id);
        }
    }

    // Build the replacement token BF card.
    auto token_id = state_.createObject();
    auto& tok = state_.getObject(token_id);
    tok.owner = PlayerId::None;
    tok.controller = PlayerId::None;
    tok.card_type = CardType::Battlefield;
    tok.super_type = SuperType::Token;
    tok.is_token = true;   // CR 185.1 intrinsic
    tok.name = token_name;
    tok.card_def_id = token_def;
    tok.zone = ZoneType::BattlefieldZone;
    tok.location = BattlefieldLocation{target_bf};

    // Swap the slot in place — keeps any units at the BF in their slot.
    slot->replaced_card = slot->card_object_id;
    slot->was_replaced = true;
    slot->is_token = true;
    slot->card_object_id = token_id;
    slot->contributed_by = controller;

    events_.logTrace("BF_REPLACE: BF#" + std::to_string(target_bf) +
                     " replaced by '" + token_name + "' token (card=" +
                     std::to_string(token_id) + ")");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Composite operations
// ═══════════════════════════════════════════════════════════════════════════════

std::pair<GameObjectId, std::vector<GameObjectId>>
EffectExecutor::revealUntil(PlayerId player, CardType condition) {
    auto& ps = state_.player(player);
    // Void Hatchling (341): "If you would reveal cards from a deck, look at the
    // top card first. You may recycle it." Peek + optional recycle before reveal.
    if (ps.has_reveal_peek && !ps.main_deck.empty() && agent_query_) {
        GameObjectId top = ps.main_deck.back();
        if (state_.objectExists(top)) {
            // A private look at the top card.
            knowledge::seenInDeck(state_, top, knowledge::only(player), DeckEnd::Top, 0);
            Intent keep;  keep.type  = IntentType::MakeChoice; keep.player  = player; keep.chosen_value  = 0;
            Intent recyc; recyc.type = IntentType::MakeChoice; recyc.player = player; recyc.chosen_value = 1;
            auto chosen = agent_query_(player, {keep, recyc});
            if (chosen.chosen_value.value_or(0) == 1) {
                deck::takeTop(state_, player, deck::Place::Held, knowledge::only(player));
                recycleCards(player, {top});
                events_.logTrace("VOID HATCHLING: recycled the peeked top card before revealing");
            }
        }
    }
    std::vector<GameObjectId> revealed;
    GameObjectId matched = kInvalidId;

    while (!ps.main_deck.empty()) {
        auto card_id = deck::takeTop(state_, player, deck::Place::Held,
                                     knowledge::everyone());
        revealed.push_back(card_id);

        auto& obj = state_.getObject(card_id);
        // Reveal is public per CR — both players see each peeked card.
        // Emit + trace so the replay shows the cascade (and the
        // observation tracker / opponent's observed_cards vector can
        // pick it up once Phase 10 wires those features through).
        events_.logTrace("  REVEAL: " + obj.name + " (id=" +
                         std::to_string(card_id) + ", " +
                         (obj.card_type == condition ? "MATCH" : "skip") +
                         ") from top of " + toString(player) + "'s deck");
        events_.emit(CardRevealedEvent{
            /*card=*/card_id,
            /*card_def_id=*/obj.card_def_id,
            /*owner=*/obj.owner,
            /*revealed_to_all=*/true,
            /*revealed_to=*/PlayerId::None,
            /*source_zone=*/ZoneType::MainDeck,
        });
        if (obj.card_type == condition) {
            matched = card_id;
            break;
        }
    }

    // Split into matched and rest
    std::vector<GameObjectId> rest;
    for (auto id : revealed) {
        if (id != matched) rest.push_back(id);
    }

    return {matched, rest};
}

void EffectExecutor::predict(PlayerId player, int count) {
    auto& ps = state_.player(player);
    int actual = std::min(count, static_cast<int>(ps.main_deck.size()));
    if (actual <= 0) return;

    events_.logTrace(std::string("PREDICT: ") + toString(player) +
                     " looks at top " + std::to_string(actual) + " cards");

    // Peek at top N cards (remove from deck temporarily)
    const auto looker = knowledge::only(player);
    std::vector<GameObjectId> peeked;
    for (int i = 0; i < actual; ++i) {
        peeked.push_back(deck::takeTop(state_, player, deck::Place::Held, looker));
    }
    // Per CR 436.1, Predict is a PRIVATE look — only the predicting
    // player sees the cards. Log each peeked card with a "PRIVATE" tag
    // so V&V can verify what the agent saw (the engine doesn't yet
    // persist this — see Phase 10 ObservationTracker — but the trace
    // does). Emit CardRevealedEvent with revealed_to=player /
    // revealed_to_all=false so a future Phase 10 subscriber can record
    // the private observation.
    for (auto card_id : peeked) {
        if (!state_.objectExists(card_id)) continue;
        auto& obj = state_.getObject(card_id);
        events_.logTrace("  PEEKED: " + obj.name + " (id=" +
                         std::to_string(card_id) +
                         ") — PRIVATE to " + toString(player));
        events_.emit(CardRevealedEvent{
            /*card=*/card_id,
            /*card_def_id=*/obj.card_def_id,
            /*owner=*/obj.owner,
            /*revealed_to_all=*/false,
            /*revealed_to=*/player,
            /*source_zone=*/ZoneType::MainDeck,
        });
    }

    // Agent chooses which to recycle via MakeChoice
    // For now: random agent recycles all (keeps top of deck fresh)
    // Future: proper MakeChoice with multiple selections
    if (agent_query_) {
        // Offer choice: recycle each card or keep it
        GameObjectId recycled = kInvalidId;
        for (auto card_id : peeked) {
            std::vector<Intent> choices;
            // Choice 1: recycle (put on bottom)
            Intent recycle_choice;
            recycle_choice.type = IntentType::MakeChoice;
            recycle_choice.player = player;
            recycle_choice.chosen_objects = {card_id};
            choices.push_back(recycle_choice);
            // Choice 2: keep on top
            Intent keep_choice;
            keep_choice.type = IntentType::MakeChoice;
            keep_choice.player = player;
            choices.push_back(keep_choice);

            auto chosen = agent_query_(player, choices);
            if (!chosen.chosen_objects.empty()) {
                // Recycle: put on bottom
                deck::putOnBottom(state_, player, card_id, deck::Place::Held, looker);
                events_.logTrace("PREDICT: recycled " + state_.getObject(card_id).name);
                recycled = card_id;
            } else {
                // Keep: put back on top
                deck::putOnTop(state_, player, card_id, deck::Place::Held, looker);
            }
        }
        // CR 437.1: the cards a Predict puts on the bottom are Recycled, so
        // "When you recycle one or more cards to your Main Deck" sees them —
        // once for the whole Predict, as recycleCards announces it.
        if (recycled != kInvalidId)
            events_.emit(ObjectStateChangedEvent{recycled, "recycled_main"});
    } else {
        // No agent query — put all back on top (no recycling)
        for (int i = static_cast<int>(peeked.size()) - 1; i >= 0; --i) {
            deck::putOnTop(state_, player, peeked[i], deck::Place::Held, looker);
        }
    }
}

std::vector<GameObjectId> EffectExecutor::revealAndChoose(PlayerId player, int count) {
    auto& ps = state_.player(player);
    // Void Hatchling (341): peek top, may recycle before revealing (see revealUntil).
    if (ps.has_reveal_peek && !ps.main_deck.empty() && agent_query_) {
        GameObjectId top = ps.main_deck.back();
        if (state_.objectExists(top)) {
            // A private look at the top card.
            knowledge::seenInDeck(state_, top, knowledge::only(player), DeckEnd::Top, 0);
            Intent keep;  keep.type  = IntentType::MakeChoice; keep.player  = player; keep.chosen_value  = 0;
            Intent recyc; recyc.type = IntentType::MakeChoice; recyc.player = player; recyc.chosen_value = 1;
            auto chosen = agent_query_(player, {keep, recyc});
            if (chosen.chosen_value.value_or(0) == 1) {
                deck::takeTop(state_, player, deck::Place::Held, knowledge::only(player));
                recycleCards(player, {top});
                events_.logTrace("VOID HATCHLING: recycled the peeked top card before revealing");
            }
        }
    }
    int actual = std::min(count, static_cast<int>(ps.main_deck.size()));
    std::vector<GameObjectId> chosen_cards;
    if (actual <= 0) return chosen_cards;

    events_.logTrace(std::string("REVEAL: ") + toString(player) +
                     " reveals top " + std::to_string(actual));

    // Peek at top N
    std::vector<GameObjectId> revealed;
    for (int i = 0; i < actual; ++i) {
        revealed.push_back(deck::takeTop(state_, player, deck::Place::Held,
                                         knowledge::everyone()));
    }

    // Per CR 424, Reveal is PUBLIC — every player sees each card.
    // Emit CardRevealedEvent with revealed_to_all=true so a future
    // observation tracker (Phase 10) can record the public observation
    // for both seats. Trace already logged the "REVEALED" line below;
    // event emission is forward-compatible scaffolding.
    for (auto card_id : revealed) {
        if (!state_.objectExists(card_id)) continue;
        auto& obj = state_.getObject(card_id);
        events_.emit(CardRevealedEvent{
            /*card=*/card_id,
            /*card_def_id=*/obj.card_def_id,
            /*owner=*/obj.owner,
            /*revealed_to_all=*/true,
            /*revealed_to=*/PlayerId::None,
            /*source_zone=*/ZoneType::MainDeck,
        });
    }

    // Agent chooses which to draw, rest get recycled
    GameObjectId recycled = kInvalidId;
    for (auto card_id : revealed) {
        if (!state_.objectExists(card_id)) continue;
        auto& obj = state_.getObject(card_id);
        events_.logTrace("  REVEALED: " + obj.name + " (id=" +
                         std::to_string(card_id) + ") — PUBLIC");

        if (agent_query_) {
            std::vector<Intent> choices;
            Intent draw_it;
            draw_it.type = IntentType::MakeChoice;
            draw_it.player = player;
            draw_it.chosen_objects = {card_id};
            choices.push_back(draw_it);
            Intent skip_it;
            skip_it.type = IntentType::MakeChoice;
            skip_it.player = player;
            choices.push_back(skip_it);

            auto chosen = agent_query_(player, choices);
            if (!chosen.chosen_objects.empty()) {
                ps.hand.push_back(card_id);
                obj.zone = ZoneType::Hand;
                knowledge::moved(state_, card_id, deck::Place::Held, deck::Place::Hand,
                                 knowledge::everyone());
                chosen_cards.push_back(card_id);
                events_.logTrace("  CHOSE: draw " + obj.name);
            } else {
                // Recycle to bottom
                deck::putOnBottom(state_, player, card_id, deck::Place::Held, knowledge::everyone());
                events_.logTrace("  CHOSE: recycle " + obj.name);
                recycled = card_id;
            }
        } else {
            // No agent: put back on top
            deck::putOnTop(state_, player, card_id, deck::Place::Held, knowledge::everyone());
        }
    }

    if (!chosen_cards.empty()) {
        state_.player(player).draws_this_turn += static_cast<int>(chosen_cards.size());
        events_.emit(CardsDrawnEvent{player, static_cast<int>(chosen_cards.size())});
    }
    // The rest were recycled: announce it once, as recycleCards does.
    if (recycled != kInvalidId)
        events_.emit(ObjectStateChangedEvent{recycled, "recycled_main"});
    return chosen_cards;
}

namespace {
// Stage the "what is ignored" flags on the player for the duration of one cost
// query / payment, and always clear them.
struct StagedEffectCost {
    PlayerState& ps;
    StagedEffectCost(PlayerState& p, const EffectExecutor::EffectPlayCost& c) : ps(p) {
        ps.transient_ignore_energy = c.ignore_energy;
        ps.transient_ignore_power = c.ignore_power;
        ps.transient_play_discount += c.energy_reduction;
        reduction_ = c.energy_reduction;
    }
    ~StagedEffectCost() {
        ps.transient_ignore_energy = false;
        ps.transient_ignore_power = false;
        ps.transient_play_discount -= reduction_;
    }
    int reduction_ = 0;
};
}  // namespace

// Plain cost path used when no engine cost path is attached (an executor driven
// directly, outside a game): the card's printed cost less what is staged as
// ignored or reduced, paid from the floating pool and then from runes. It has
// none of the engine path's cost modifiers or rune choices.
namespace {
struct PlainCost { int energy = 0; int power = 0; std::vector<Domain> domains; };

PlainCost plainCostOf(const GameState& state, const CardDB& db, PlayerId player,
                      GameObjectId card) {
    PlainCost c;
    const auto& obj = state.getObject(card);
    if (obj.card_def_id == kInvalidId) return c;
    const auto& def = db.get(obj.card_def_id);
    const auto& ps = state.player(player);
    c.domains = def.domains;
    c.energy = ps.transient_ignore_energy ? 0
        : std::max(0, def.energy_cost - ps.transient_play_discount);
    c.power = ps.transient_ignore_power ? 0
        : std::max(0, def.power_cost - ps.transient_power_discount);
    return c;
}

bool runeMatches(const GameObject& rune, const std::vector<Domain>& domains) {
    for (auto d : domains)
        for (auto rd : rune.domains) if (d == rd) return true;
    return false;
}

// With `pay` false this only answers whether the cost is payable.
bool plainPay(GameState& state, EventBus& events, PlayerId player, const PlainCost& cost,
              bool pay) {
    auto& ps = state.player(player);
    int e = cost.energy - ps.rune_pool.energy;
    int p = cost.power - ps.rune_pool.universal_power;
    for (auto d : cost.domains) p -= ps.rune_pool.power[static_cast<int>(d)];
    LocationId base{BaseLocation{player}};
    std::vector<GameObjectId> runes;
    for (auto& [id, obj] : state.objects)
        if (obj.isRune() && obj.controller == player &&
            obj.location.has_value() && *obj.location == base) runes.push_back(id);
    int ready = 0, matching = 0;
    for (auto rid : runes) {
        const auto& r = state.getObject(rid);
        if (!r.is_exhausted) ++ready;
        if (runeMatches(r, cost.domains)) ++matching;
    }
    if (ready < e || matching < p) return false;
    if (!pay) return true;

    auto take = [](int& from, int& need) {
        int n = std::min(from, std::max(0, need));
        from -= n;
        need -= n;
    };
    int e_need = cost.energy, p_need = cost.power;
    take(ps.rune_pool.energy, e_need);
    for (auto d : cost.domains) take(ps.rune_pool.power[static_cast<int>(d)], p_need);
    take(ps.rune_pool.universal_power, p_need);
    // Energy from runes that can't pay the Power first; then recycle matching
    // runes, exhausted ones first (CR 164.2).
    std::stable_sort(runes.begin(), runes.end(), [&](GameObjectId a, GameObjectId b) {
        return runeMatches(state.getObject(a), cost.domains) <
               runeMatches(state.getObject(b), cost.domains);
    });
    for (auto rid : runes) {
        if (e_need <= 0) break;
        auto& r = state.getObject(rid);
        if (r.is_exhausted) continue;
        r.is_exhausted = true;
        events.emit(ObjectStateChangedEvent{rid, "exhausted"});
        --e_need;
    }
    std::stable_sort(runes.begin(), runes.end(), [&](GameObjectId a, GameObjectId b) {
        return state.getObject(a).is_exhausted > state.getObject(b).is_exhausted;
    });
    for (auto rid : runes) {
        if (p_need <= 0) break;
        auto& r = state.getObject(rid);
        if (!runeMatches(r, cost.domains)) continue;
        r.location = std::nullopt;
        r.zone = ZoneType::RuneDeck;
        r.is_exhausted = false;
        ps.rune_deck.insert(ps.rune_deck.begin(), rid);
        ps.power_spent_this_turn++;
        --p_need;
    }
    return true;
}
}  // namespace

bool EffectExecutor::canPlayPayingCost(PlayerId player, GameObjectId card,
                                       EffectPlayCost cost) const {
    if (!state_.objectExists(card)) return false;
    if (!can_afford_) {
        StagedEffectCost staged(state_.player(player), cost);
        return plainPay(state_, events_, player,
                        plainCostOf(state_, card_db_, player, card), /*pay=*/false);
    }
    StagedEffectCost staged(state_.player(player), cost);
    return can_afford_(player, card);
}

bool EffectExecutor::playPayingCost(PlayerId player, GameObjectId card,
                                    std::optional<LocationId> location,
                                    EffectPlayCost cost) {
    if (!state_.objectExists(card)) return false;
    if (!can_afford_ || !pay_cost_) {
        StagedEffectCost staged(state_.player(player), cost);
        if (!plainPay(state_, events_, player,
                      plainCostOf(state_, card_db_, player, card), /*pay=*/true))
            return false;
    } else {
        StagedEffectCost staged(state_.player(player), cost);
        if (!can_afford_(player, card)) return false;
        if (!pay_cost_(player, card)) return false;
    }
    playIgnoringCost(player, card, location);   // costs are settled; put it into play
    return true;
}

void EffectExecutor::playIgnoringCost(PlayerId player, GameObjectId card,
                                       std::optional<LocationId> location) {
    if (!state_.objectExists(card)) return;
    auto& obj = state_.getObject(card);

    // A played card leaves the zone it is played from. Done here so no caller
    // can leave it listed in a hand, trash, banishment or deck while it is
    // also on the board (callers that already took it out lose nothing).
    for (auto& zps : state_.players) {
        auto drop = [&](std::vector<GameObjectId>& v) {
            v.erase(std::remove(v.begin(), v.end(), card), v.end());
        };
        drop(zps.hand);
        drop(zps.trash);
        drop(zps.banishment);
        if (zps.champion_zone == card) zps.champion_zone = kInvalidId;
    }
    deck::take(state_, card, deck::Place::Public, knowledge::everyone());

    // Landing zone: caller-supplied (CR 355.2.a — controller picks base
    // or a battlefield they control) or, by default, the controller's
    // base for back-compat. The card's onPlay hook below may override
    // this (Baron Nashor relocates himself to the Pit token he just
    // created). Per CR 355.1, "As you play me" effects run DURING
    // the play, not after — that gives onPlay the opportunity to
    // redirect before any EnteredBoardEvent fires.
    // A spell played by an effect is still played: it goes on the chain and
    // resolves (above whatever is resolving now). It is not a permanent, so
    // none of the board placement below applies.
    if (obj.isSpell() && queue_spell_) {
        obj.controller = player;
        obj.location = std::nullopt;
        auto& sps = state_.player(player);
        sps.cards_played_this_turn++;
        events_.emit(CardPlayedEvent{card, player, obj.card_type,
                                     sps.cards_played_this_turn});
        queue_spell_(player, card);
        return;
    }

    LocationId chosen_loc = location.value_or(LocationId{BaseLocation{player}});
    if (std::holds_alternative<BattlefieldLocation>(chosen_loc)) {
        obj.zone = ZoneType::BattlefieldZone;
    } else {
        obj.zone = ZoneType::Base;
    }
    obj.location = chosen_loc;
    obj.controller = player;

    if (obj.isUnit()) {
        obj.is_exhausted = true;
        // A unit (re-)entering play is a fresh presence — clear any marked
        // damage carried from a prior life (e.g. banished-then-replayed by
        // Thrill of the Hunt). New plays from hand are already at 0, so this
        // is a no-op for the common case.
        obj.damage_marked = 0;
    }
    obj.recomputeMight();

    auto& ps = state_.player(player);
    ps.cards_played_this_turn++;

    // Fire the card's onPlay hook (CR 355.1 "As you play me"). Without
    // this, cards played by Dazzling Aurora / Mirror Image / etc. via
    // playIgnoringCost skip their inline play-time effects entirely —
    // Baron Nashor wouldn't spawn the Baron Pit, etc. The normal
    // GameEngine::executePlayCard path calls this between cost payment
    // and chain insertion; we mirror that ordering here even though
    // there's no chain step in the ignore-cost path.
    if (card_registry_ && obj.card_def_id != kInvalidId) {
        if (Card* card_obj = card_registry_->get(obj.card_def_id)) {
            CardContext ctx{state_, events_, *this, player, card};
            card_obj->onPlay(ctx);
        }
    }

    // Re-read the (possibly-relocated) location for the EnteredBoardEvent
    // so subscribers see where the unit actually landed, not the default
    // base location set above. WhenIEnterTheBoard / WhenYouPlayMe
    // triggers fired through CardPlayedEvent + EnteredBoardEvent below
    // will then observe the final post-onPlay state.
    LocationId final_loc = obj.location.value_or(BaseLocation{player});

    events_.emit(CardPlayedEvent{card, player, obj.card_type,
        ps.cards_played_this_turn});
    events_.emit(EnteredBoardEvent{card, player, obj.card_type,
        final_loc, true});
}

} // namespace riftbound
