#include "core/board_state.h"
#include "effect_executor.h"

#include "cards/card.h"
#include "cards/card_registry.h"
#include "engine/deck_ops.h"
#include "engine/trash_replacement.h"
#include "cards/card_helpers.h"

#include <algorithm>
#include <cassert>

namespace riftbound {

// Counter on a unit: the turn (+1) a spell of `by` last dealt it damage.
static const char* spellDamageKey(PlayerId by) {
    return by == PlayerId::Player1 ? "__spell_damage_turn_p1" : "__spell_damage_turn_p2";
}


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
        RIFT_TRACE(events_, "PREVENT: " + obj.name + " is immune to damage");
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
        RIFT_TRACE(events_, "PREVENT: " + std::to_string(prevented) + " damage to " + obj.name);
        if (amount <= 0) return;
    }
    if (amount > 0 && obj.prevent_next_damage_this_turn) {
        obj.prevent_next_damage_this_turn = false;
        RIFT_TRACE(events_, "PREVENT: next damage to " + obj.name +
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
            RIFT_TRACE(events_, "PREVENT: enemy spell/ability damage to " + obj.name);
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
                RIFT_TRACE(events_, "EFFECT: prevented " + std::to_string(amount) +
                                 " spell/ability damage to " + obj.name +
                                 " (id=" + toString(target) +
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
                    RIFT_TRACE(events_, "EFFECT: +" + std::to_string(bonus) +
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
        RIFT_TRACE(events_, "EFFECT: damage-doubling on " + obj.name +
                         " (Lotus Trap-style): " + std::to_string(amount) +
                         " -> " + std::to_string(dealt));
    }

    RIFT_TRACE(events_, "EFFECT: deal " + std::to_string(dealt) + " damage to " +
                     obj.name + " (id=" + toString(target) + ", was " +
                     std::to_string(obj.damage_marked) + "/" +
                     std::to_string(obj.current_might) + "M)");
    if (obj.damage_marked == 0) forgetDamageSources(obj);
    obj.damage_marked += dealt;
    obj.damage_taken_this_turn += dealt;
    // "Your damage" is the damage you marked: credit the source's controller.
    if (state_.objectExists(source)) {
        const PlayerId by = state_.getObject(source).controller;
        if (by != PlayerId::None) {
            obj.damage_marked_by[playerIndex(by)] += dealt;
            // Remembered for a death this damage causes later, in a cleanup
            // ("When you kill a unit with a spell"): turn-stamped.
            if (dealt > 0 && state_.getObject(source).isSpell())
                obj.card_counters[spellDamageKey(by)] = state_.turn.turn_number + 1;
        }
    }
    events_.emit(DamageDealtEvent{target, dealt, source, false});
}

// Who is killing `obj` right now: the controller of the spell or ability that
// is resolving, else the player whose damage is lethal (the larger share of
// what is marked; the other player's on a tie).
static PlayerId killerOf(const GameState& state, const GameObject& obj) {
    if (state.chain.resuming.has_value()) return state.chain.resuming->controller;
    if (obj.damage_marked <= 0 || obj.controller == PlayerId::None) return PlayerId::None;
    const PlayerId other = opponent(obj.controller);
    const int theirs = obj.damage_marked_by[playerIndex(other)];
    const int own = obj.damage_marked_by[playerIndex(obj.controller)];
    if (theirs <= 0 && own <= 0) return PlayerId::None;
    return theirs >= own ? other : obj.controller;
}

// What an effect applied to a unit changes about it, Might and keywords it was
// GIVEN included ("this turn" grants, buffs, Empowered). The computed totals
// (current_might, the keyword set with auras folded in) are left out on
// purpose: another unit of the batch dying can take an aura away mid-batch,
// and that must not look like the unit was changed.
static uint64_t deathBatchStamp(const GameObject& obj) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    mix(static_cast<uint64_t>(static_cast<int64_t>(obj.damage_marked)));
    mix(static_cast<uint64_t>(static_cast<int64_t>(obj.temp_might_bonus)));
    mix(static_cast<uint64_t>(static_cast<int64_t>(obj.temp_buff_count)));
    mix(static_cast<uint64_t>(static_cast<int64_t>(obj.temp_assault_value)));
    mix(static_cast<uint64_t>(static_cast<int64_t>(obj.temp_shield_value)));
    mix(static_cast<uint64_t>(static_cast<int64_t>(obj.temp_deflect_value)));
    mix(static_cast<uint64_t>(static_cast<int64_t>(obj.empowered)));
    mix(obj.temp_keywords.bits);
    mix(static_cast<uint64_t>(static_cast<int64_t>(obj.buff_count)));
    mix(obj.is_stunned ? 1 : 0);
    mix(static_cast<uint64_t>(obj.controller));
    mix(obj.attachments.size());
    if (obj.location.has_value()) {
        mix(obj.location->index() + 1);
        if (auto bf = obj.battlefieldId()) mix(static_cast<uint64_t>((*bf).raw()) + 17);
    }
    return h;
}

DeathBatchMark EffectExecutor::beginDeathBatch(const std::vector<GameObjectId>& units) {
    DeathBatchMark mark;
    mark.first_item = state_.chain.next_id;
    mark.oc_state = state_.turn.oc_state;
    mark.chain_was_empty = state_.chain.items.empty();
    auto on_board = [&](GameObjectId id) {
        return state_.objectExists(id) && state_.getObject(id).location.has_value();
    };
    // Replacements first, for every member, while none of them has died: a
    // card that dies in this batch still replaces the deaths of the others
    // (CR 370.4), and one that is put in a trash by this batch is not there
    // yet to replace anything (the CR 370 example).
    //
    // Order (CR 373, 373.1): the turn player's dying units, then the other
    // player's. Within one player the PLAYER decides which death is taken
    // first — "Two units controlled by the same player die in the same
    // cleanup. That player also controls Zhonya's Hourglass. They must decide
    // which event to apply Zhonya's Hourglass to first." — so when two or
    // more of that player's dying units each have something that could
    // replace their death, the player is asked which unit goes next. With one
    // such unit, or none, there is nothing to decide.
    auto settle = [&](GameObjectId id) {
        if (!on_board(id)) return;                   // left through another one's replacement
        auto& lk = state_.getObject(id).last_known;
        lk.batch_pending = false;
        lk.batch_replaced = false;
        // The snapshot a lone kill takes before its replacements, so that a
        // replacement which reads the dying unit sees what it always saw.
        snapshotLastKnown(id);
        if (applyDeathReplacements(id)) {
            if (state_.objectExists(id)) state_.getObject(id).last_known.batch_replaced = true;
        }
    };
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<GameObjectId> mine;
        PlayerId chooser = PlayerId::None;
        for (auto id : units) {
            if (!on_board(id) || !state_.getObject(id).isUnit()) continue;
            const PlayerId controller = state_.getObject(id).controller;
            if ((controller == state_.turn.turn_player) != (pass == 0)) continue;
            if (std::find(mine.begin(), mine.end(), id) != mine.end()) continue;
            mine.push_back(id);
            chooser = controller;
        }
        while (!mine.empty()) {
            std::vector<GameObjectId> replaceable;
            for (auto id : mine)
                if (on_board(id) && couldReplaceDeath(id)) replaceable.push_back(id);
            if (replaceable.empty()) break;
            GameObjectId next = replaceable.front();
            if (replaceable.size() > 1 && agent_query_ && chooser != PlayerId::None) {
                std::vector<Intent> options;
                for (auto id : replaceable) {
                    Intent o;
                    o.type = IntentType::MakeChoice;
                    o.player = chooser;
                    o.chosen_objects = {id};
                    o.choice_label = state_.getObject(id).name +
                                     " would die — decide what replaces this death first";
                    options.push_back(std::move(o));
                }
                const Intent chosen = ask(chooser, options,
                    "which death is replaced first (CR 373)");
                if (!chosen.chosen_objects.empty() &&
                    std::find(replaceable.begin(), replaceable.end(),
                              chosen.chosen_objects.front()) != replaceable.end())
                    next = chosen.chosen_objects.front();
            }
            settle(next);
            mine.erase(std::remove(mine.begin(), mine.end(), next), mine.end());
        }
        // The rest have nothing that could replace their death; this uses up
        // "the next time it would die" effects that no longer apply.
        for (auto id : mine) settle(id);
    }
    for (auto id : units) {
        if (!on_board(id)) continue;
        if (state_.getObject(id).last_known.batch_replaced) continue;   // it does not die
        state_.getObject(id).last_known.batch_pending = false;
        snapshotLastKnown(id);
        state_.getObject(id).last_known.batch_pending = true;
        state_.getObject(id).last_known.batch_stamp = deathBatchStamp(state_.getObject(id));
    }
    return mark;
}

// A member of an open death batch: its replacements were applied when the
// batch began (and none replaced the death).
static bool replacementsSettledByBatch(const GameObject& obj) {
    return obj.last_known.batch_pending && obj.last_known.valid &&
           obj.last_known.batch_stamp == deathBatchStamp(obj);
}

DeathBatch::DeathBatch(EffectExecutor& executor, std::vector<GameObjectId> units,
                       Members members)
    : executor_(executor), units_(std::move(units)) {
    if (members == LethalOnly) executor_.keepLethallyDamaged(units_);
    mark_ = executor_.beginDeathBatch(units_);
}
DeathBatch::~DeathBatch() { executor_.endDeathBatch(units_, mark_); }

bool EffectExecutor::deathReplacementsSettled(GameObjectId unit) const {
    return state_.objectExists(unit) && replacementsSettledByBatch(state_.getObject(unit));
}

bool EffectExecutor::anyDamageIsLethalFor(PlayerId damager) const {
    for (const auto& [id, obj] : state_.objects.onBoard()) {
        if (!obj.isUnit() || !obj.location.has_value()) continue;
        if (obj.controller != damager || obj.card_def_id == kInvalidId) continue;
        const Card* card = cardFor(obj.card_def_id);
        if (card && card->anyDamageIsLethal()) return true;
    }
    return false;
}

bool EffectExecutor::damageKills(const GameObject& obj, LethalRule& rule,
                                 bool* by_any_damage) const {
    if (by_any_damage) *by_any_damage = false;
    if (!obj.isUnit() || !obj.location.has_value() || obj.damage_marked <= 0) return false;
    if (obj.hasLethalDamage()) return true;
    // Elder Dragon: any amount of that player's damage kills. "Your damage"
    // is the damage you marked, so damage a unit took from its own side does
    // not count. Asked only for a unit carrying damage short of lethal, and
    // once per player per `rule`.
    if (obj.controller == PlayerId::None) return false;
    const PlayerId damager = opponent(obj.controller);
    if (std::min(obj.damage_marked_by[playerIndex(damager)], obj.damage_marked) <= 0) return false;
    auto& known = rule.any_damage_lethal[playerIndex(damager)];
    if (!known.has_value()) known = anyDamageIsLethalFor(damager);
    if (!*known) return false;
    if (by_any_damage) *by_any_damage = true;
    return true;
}

bool EffectExecutor::damageKills(GameObjectId unit) const {
    LethalRule rule;
    return state_.objectExists(unit) && damageKills(state_.getObject(unit), rule);
}

void EffectExecutor::keepLethallyDamaged(std::vector<GameObjectId>& units) const {
    LethalRule rule;
    units.erase(std::remove_if(units.begin(), units.end(), [&](GameObjectId id) {
        return !state_.objectExists(id) || !damageKills(state_.getObject(id), rule);
    }), units.end());
}

std::vector<GameObjectId>
EffectExecutor::killLethallyDamagedTogether(std::vector<GameObjectId> damaged) {
    // Each unit once, in the order given.
    std::vector<GameObjectId> dying;
    LethalRule rule;
    for (auto id : damaged) {
        if (!state_.objectExists(id)) continue;
        if (std::find(dying.begin(), dying.end(), id) != dying.end()) continue;
        bool by_any_damage = false;
        if (!damageKills(state_.getObject(id), rule, &by_any_damage)) continue;
        if (by_any_damage)
            RIFT_TRACE(events_, "ELDER_DRAGON: " + state_.getObject(id).name +
                             " killed by any-damage rule");
        dying.push_back(id);
    }
    std::vector<GameObjectId> died;
    const DeathBatchMark mark = beginDeathBatch(dying);
    for (auto id : dying) {
        if (!state_.objectExists(id) || !state_.getObject(id).location.has_value()) continue;
        killObject(id);
        if (!state_.objectExists(id) || !state_.getObject(id).location.has_value())
            died.push_back(id);
    }
    endDeathBatch(dying, mark);
    return died;
}

void EffectExecutor::endDeathBatch(const std::vector<GameObjectId>& units,
                                   const DeathBatchMark& mark) {
    for (auto id : units)
        if (state_.objectExists(id)) {
            state_.getObject(id).last_known.batch_pending = false;
            state_.getObject(id).last_known.batch_replaced = false;
        }
    // CR 383.2.c.2: an object "will not be able to successfully evaluate its
    // Trigger Condition ... if it leaves the zone that its Trigger is active
    // from at the same time that its Trigger is satisfied". A unit of the
    // batch that was still on the board when an earlier one was killed put
    // its "when another unit dies" ability on the chain; if it then died in
    // this batch too, it left with the unit it watched, and the ability never
    // triggered. Its own "When I die" is another trigger type and stays
    // (CR 383.2.c.3). A unit of the batch that lived saw the others die.
    // (Left in the batch by dying, or by what replaced its death: banished.)
    auto left_here = [&](GameObjectId id) {
        if (std::find(units.begin(), units.end(), id) == units.end()) return false;
        return !state_.objectExists(id) || !state_.getObject(id).location.has_value();
    };
    auto watches_a_death = [](TriggerType t) {
        return t == TriggerType::WhenAFriendlyUnitDies || t == TriggerType::WhenAnEnemyUnitDies ||
               t == TriggerType::WhenAnEnemyUnitDiesHere;
    };
    auto& items = state_.chain.items;
    const auto before = items.size();
    items.erase(std::remove_if(items.begin(), items.end(), [&](const ChainItem& ci) {
        if (ci.id < mark.first_item || ci.status != ChainItemStatus::Pending) return false;
        if (!ci.is_ability || ci.is_activated_ability) return false;
        if (!watches_a_death(ci.fired_trigger)) return false;
        if (ci.triggering_subject == ci.source) return false;
        if (!left_here(ci.source) || !left_here(ci.triggering_subject)) return false;
        RIFT_DEBUG(events_, "TRIGGER: " + state_.getObject(ci.source).name +
                         " left with the unit it watched — its ability did not trigger "
                         "(CR 383.2.c.2)");
        return true;
    }), items.end());
    // Nothing else of the batch on a chain that was empty before it: there is
    // no chain (ChainManager::addAbility closed the state for the items taken back).
    if (items.size() != before && items.empty() && mark.chain_was_empty &&
        !state_.chain.resuming.has_value())
        state_.turn.oc_state = mark.oc_state;
}

void EffectExecutor::snapshotLastKnown(GameObjectId id, bool dying) {
    if (!state_.objectExists(id)) return;
    auto& obj = state_.getObject(id);
    auto& lk = obj.last_known;
    // Taken with the rest of its batch while they were all on the board
    // (beginDeathBatch): that is the state "as it died" for every one of them.
    // A unit that survived its batch and was changed since (buffed, moved,
    // stunned, damaged again) is not "as it was then": take it afresh.
    if (dying && lk.batch_pending && lk.valid && lk.batch_stamp == deathBatchStamp(obj)) return;
    lk.batch_pending = false;
    lk.valid = true;
    lk.controller = obj.controller;
    lk.died = dying;
    lk.stunned = obj.is_stunned;
    lk.exhausted = obj.is_exhausted;
    lk.location = obj.location;
    lk.other_friendly_units_here.clear();
    if (obj.location.has_value())
        for (const auto& [oid, other] : state_.objects.atLocation(*obj.location))
            if (oid != id && other.isUnit() && other.controller == obj.controller &&
                other.location.has_value() && *other.location == *obj.location)
                lk.other_friendly_units_here.push_back(oid);
    lk.killed_by = dying ? killerOf(state_, obj) : PlayerId::None;
    if (!dying) {
        lk.killed_with_spell = false;
    } else if (state_.chain.resuming.has_value()) {
        lk.killed_with_spell = state_.chain.resuming->is_spell;
    } else {
        // Lethal damage found in a cleanup: with a spell if that player's
        // spell dealt damage that is still marked on it.
        lk.killed_with_spell = false;
        if (lk.killed_by != PlayerId::None && obj.damage_marked > 0) {
            auto it = obj.card_counters.find(spellDamageKey(lk.killed_by));
            lk.killed_with_spell = it != obj.card_counters.end() &&
                                   it->second == state_.turn.turn_number + 1;
        }
    }
    lk.text_copies = obj.location.has_value() ? textCopiesOn(id) : 0;
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
    // CR 429.2.a: only something on the board can be killed. A dead unit is
    // in a trash, a dead token or a unit banished as it died (Smite, Endless
    // Riches) in Banishment, a bounced unit in a hand — killing any of them
    // again would announce a second death. The test is the board, not a zone.
    if (!onBoard(obj)) return;
    // Its death was replaced when the batch it dies in began (beginDeathBatch):
    // this kill is the one that was replaced.
    if (obj.last_known.batch_replaced) return;
    RIFT_TRACE(events_, "EFFECT: kill " + obj.name + " (id=" + toString(target) + ")");
    const bool replacements_settled = obj.isUnit() && replacementsSettledByBatch(obj);
    // Last-known information for "When I die" (CR 383.2.c.3.b).
    snapshotLastKnown(target);

    auto was_at = obj.location;
    auto controller = obj.controller;

    if (obj.isUnit()) {
        // Everything that can replace this death — on the unit, on its
        // battlefield, among its controller's cards — in the order its
        // controller chooses (CR 372). GameEngine::killUnit makes the same call.
        // (Already done for a unit of a death batch, before any of them died.)
        if (!replacements_settled && applyDeathReplacements(target)) return;
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
            settleInTrash(state_, events_, target);
        } else {
            RIFT_TRACE(events_, "  TOKEN CEASES TO EXIST (CR 183.1): " + obj.name +
                             " (id=" + toString(target) + ")");
        }

        // "If a unit died this turn" bookkeeping — the same as a combat death
        // (GameEngine::killUnit): a kill is a kill.
        state_.turn.any_unit_died_this_turn = true;
        state_.player(controller).unit_died_this_turn = true;
        {
            const TurnPhase ph = state_.turn.phase;
            const bool in_beginning = ph == TurnPhase::AwakenPhase ||
                                      ph == TurnPhase::BeginningStep ||
                                      ph == TurnPhase::ScoringStep;
            if (in_beginning && state_.turn.turn_player == controller)
                state_.player(controller).unit_died_in_beginning_this_turn = true;
        }
        events_.emit(UnitDiedEvent{target, controller,
            was_at.value_or(BaseLocation{controller}), might, obj.last_known.killed_by});
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
        // The death is announced and its triggers are queued (they read the
        // unit as it died from last_known): what is in the trash is a new
        // object (CR 124.1).
        leftBoard(target);
    } else if (obj.isGear()) {
        // A gear that dies while attached comes off its unit first.
        if (obj.attached_to.has_value()) unattachGear(target);
        const bool is_token = (obj.isToken());
        obj.zone = is_token ? ZoneType::Banishment : ZoneType::Trash;
        obj.last_location = obj.location;
        obj.location = std::nullopt;
        if (!is_token) {
            state_.player(obj.owner).trash.push_back(target);
            settleInTrash(state_, events_, target);
        } else {
            RIFT_TRACE(events_, "  TOKEN CEASES TO EXIST (CR 183.1): " + obj.name +
                             " (id=" + toString(target) + ")");
        }

        events_.emit(LeftBoardEvent{target, controller, CardType::Gear,
            was_at.value_or(BaseLocation{controller}),
            is_token ? ZoneType::Banishment : ZoneType::Trash, true});
        leftBoard(target);
    }
}

void EffectExecutor::drawCards(PlayerId player, int count) {
    RIFT_TRACE(events_, std::string("EFFECT: ") + toString(player) + " draws " +
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
        auto card_id = deck::drawTop(state_, player);
        // Only the drawing player knows which card it is.
        RIFT_TRACE_PRIVATE(events_, player, "  DREW: " + state_.getObject(card_id).name +
                                        " (id=" + toString(card_id) + ")");
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
        RIFT_TRACE(events_, std::string("SCORE_BLOCKED: ") + toString(player) +
                         " can't gain points");
        return 0;
    }
    ps.score += amount;
    RIFT_TRACE(events_, std::string("SCORE: ") + toString(player) + " +" +
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
    // Printed value plus what was given "this turn" (temp_deflect_value,
    // zeroed at the expiration step).
    int value = obj.deflect_value + obj.temp_deflect_value;
    for (const auto& ae : obj.aura_effects)
        if (ae.keyword == Keyword::Deflect) value += std::max(1, ae.keyword_value);
    return std::max(1, value);
}

int payableAnyPower(const GameState& state, PlayerId player) {
    const auto& pool = state.player(player).rune_pool;
    int n = pool.universal_power;
    for (int di = 0; di < static_cast<int>(Domain::Count); ++di) n += pool.power[di];
    LocationId base{BaseLocation{player}};
    for (const auto& [id, obj] : state.objects.atLocation(base))
        if (obj.isRune() && obj.controller == player &&
            obj.location.has_value() && *obj.location == base) ++n;
    return n;
}

Card* EffectExecutor::cardFor(CardDefId def) const {
    if (!card_registry_ || def == kInvalidId) return nullptr;
    return card_registry_->get(def);
}

int EffectExecutor::deflectCostFor(PlayerId chooser, GameObjectId target) const {
    return ::riftbound::deflectCostFor(state_, chooser, target);
}

int EffectExecutor::payableAnyPower(PlayerId player) const {
    return ::riftbound::payableAnyPower(state_, player);
}

bool EffectExecutor::payAnyPower(PlayerId player, int amount) {
    if (amount <= 0) return true;
    // A spell that is making its choices before its cost is paid (CR 355 ->
    // 356: ChainItem::pre_play_choices) pays a chosen unit's Deflect here,
    // ahead of that cost. What is left must still pay it: take what the
    // spell's own cost cannot use, and keep the Energy of a ready rune
    // (resource::payAnyPowerAheadOfCost; plans/09 item 9.VV1).
    if (const auto& pre = state_.chain.resuming;
        pre.has_value() && pre->pre_play_choices && pre->controller == player &&
        state_.objectExists(pre->source)) {
        const CardDefId def = state_.getObject(pre->source).card_def_id;
        if (def != kInvalidId)
            return resource::payAnyPowerAheadOfCost(
                state_, events_, player, amount, resource::maskOf(card_db_.get(def).domains));
    }
    // The one payment rule (engine/resource_payment.h): floating Power, then
    // runes recycled from the base, exhausted ones first.
    return resource::pay(state_, events_, player, 0, amount, Domain::Count);
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
        RIFT_TRACE(events_, std::string("BURN_OUT: ") + toString(player) +
                         (ps.main_deck.empty() ? " deck and trash empty; "
                                               : " deck empty, shuffled trash; ") +
                         toString(opp_id) + " can't gain points");
        return !ps.main_deck.empty();
    }
    opp_ps.score++;
    RIFT_TRACE(events_, std::string("BURN_OUT: ") + toString(player) +
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
    RIFT_TRACE(events_, "EFFECT: empower " + obj.name + " (id=" + toString(target) +
                     ") -> " + std::to_string(obj.empowered));
    events_.emit(ObjectStateChangedEvent{target, "empowered"});
    return true;
}

bool EffectExecutor::disempower(GameObjectId target) {
    if (!state_.objectExists(target)) return false;
    auto& obj = state_.getObject(target);
    if (obj.empowered <= 0) return false;     // CR 443.1.a.1
    obj.empowered--;
    RIFT_TRACE(events_, "EFFECT: disempower " + obj.name + " (id=" + toString(target) +
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
        RIFT_TRACE(events_, std::string("  BURNED: ") + toString(player) + " " + card.name +
                         " (id=" + toString(card_id) + ")");
    }
    // "When you burn" abilities execute after burning has occurred (CR 441.1.a).
    for (auto cid : burned) events_.emit(ObjectStateChangedEvent{cid, "burned"});
    return burned;
}

// CR 719.5: gear attached to a unit that leaves the board is shed first, and
// gear that itself leaves stops being attached.
void EffectExecutor::leftBoard(GameObjectId object) {
    if (!state_.objectExists(object)) return;
    {
        auto& obj = state_.getObject(object);
        if (obj.attached_to.has_value()) unattachGear(object);
        const auto attached = obj.attachments;          // copy: unattachGear edits it
        for (auto gid : attached) unattachGear(gid);
    }
    auto& obj = state_.getObject(object);
    obj.attachments.clear();
    const CardDef* def = nullptr;
    if (obj.card_def_id != kInvalidId && card_registry_)
        def = card_registry_->classDef(obj.card_def_id);
    forgetBoardState(obj, def);
}

void EffectExecutor::putIntoDeckFromBoard(GameObjectId target, bool to_top) {
    if (!state_.objectExists(target) || !onBoard(state_.getObject(target))) return;
    auto& u = state_.getObject(target);
    const LocationId was_at = *u.location;
    const PlayerId controller = u.controller;
    const PlayerId owner = u.owner;
    const CardType type = u.card_type;
    const bool token = u.isToken();
    snapshotLastKnown(target, /*dying=*/false);   // as it was on the board
    // Gear comes off first (CR 719.5), while the card is still where it was.
    {
        if (u.attached_to.has_value()) unattachGear(target);
        const auto attached = u.attachments;
        for (auto gid : attached) unattachGear(gid);
    }
    auto& obj = state_.getObject(target);
    obj.location = std::nullopt;
    if (token) {
        obj.zone = ZoneType::Banishment;   // CR 183.1: it ceases to exist
    } else if (to_top) {
        deck::putOnTop(state_, owner, target, deck::Place::Public, knowledge::everyone());
    } else {
        deck::putOnBottom(state_, owner, target, deck::Place::Public, knowledge::everyone());
    }
    events_.emit(LeftBoardEvent{target, controller, type, was_at,
                                token ? ZoneType::Banishment : ZoneType::MainDeck,
                                /*was_killed=*/false});
    leftBoard(target);
}

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
    // As it was on the board, for whatever reads it after the move ("return a
    // unit to its owner's hand, then ... its Might"): CR 359.3.e.13.
    snapshotLastKnown(target, /*dying=*/false);
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
        RIFT_TRACE(events_, "EFFECT: bounce " + obj.name + " (id=" +
                         toString(target) +
                         ") — token ceases to exist (CR 183.1)");
        obj.zone = ZoneType::Banishment;
        obj.location = std::nullopt;
        events_.emit(LeftBoardEvent{target, controller, obj.card_type,
            was_at.value_or(BaseLocation{controller}), ZoneType::Banishment, false});
        leftBoard(target);
        return;
    }

    RIFT_TRACE(events_, "EFFECT: bounce " + obj.name + " (id=" + toString(target) + ") to hand");
    // Both players saw which card went to the hand: deck::putInHand tells the
    // knowledge tracker.
    deck::putInHand(state_, target);

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
    // Every announcement of the return has been made with the card still as
    // it left: the card in the hand is a new object (CR 124.1).
    leftBoard(target);
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

    RIFT_TRACE(events_, "EFFECT: give " + obj.name + " " + std::to_string(amount) +
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
    RIFT_TRACE(events_, "EFFECT: give " + obj.name + " [" + toString(kw) + "]" +
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
    // Kept apart from the printed deflect_value (deflectCostFor adds the two);
    // two grants in one turn add up.
    else if (kw == Keyword::Deflect) obj.temp_deflect_value += value;
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
            RIFT_TRACE(events_, "EFFECT: [Buff] " + obj.name +
                             " already has a buff — not buffed (CR 427.1.b.1)");
            return false;
        }
    }
    obj.buff_count += 1;
    obj.recomputeMight();
    RIFT_TRACE(events_, "EFFECT: [Buff] " + obj.name + " (+1 permanent) -> " +
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
        RIFT_TRACE(events_, "READY_SUPPRESSED: " + obj.name +
                         " can't be readied by effects (Mageseeker Warden)");
        return;
    }
    if (card_registry_ && obj.card_def_id != kInvalidId) {
        const Card* card = card_registry_->get(obj.card_def_id);
        if (card && !card->canBeReadied()) return;   // "I can't be readied."
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
    RIFT_TRACE(events_, "EFFECT: unattach " + g.name + " (id=" + toString(gear) + ")");
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
        RIFT_TRACE(events_, "MOVE_BLOCKED: " + obj.name + " can't be moved by enemy effects");
        return;
    }
    // "They can't move it this turn" (GameObject::cant_move_this_turn): its
    // controller's spells and abilities don't move it either.
    if (obj.cant_move_this_turn && state_.chain.resuming.has_value() &&
        state_.chain.resuming->controller == obj.controller) {
        RIFT_TRACE(events_, "MOVE_BLOCKED: " + obj.name + " can't be moved this turn");
        return;
    }
    // "Units can't move from here to base." / "Units can't move to base."
    // (moveToBaseBlocked, card_helpers.h — what the Standard Move generator
    // refuses too.) An effect's move is a move like any other; a recall is
    // not (recallToBase, CR 458.3).
    if (obj.isUnit() && moveToBaseBlocked(state_, obj)) {
        RIFT_TRACE(events_, "MOVE_BLOCKED: " + obj.name + " can't move to base");
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

void EffectExecutor::recallToBase(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    if (!onBoard(obj) || obj.controller == PlayerId::None) return;
    const LocationId home{BaseLocation{obj.controller}};
    if (*obj.location == home) return;
    RIFT_TRACE(events_, "RECALL: " + obj.name + " (id=" + toString(target) + ") to base");
    obj.location = home;
    obj.zone = ZoneType::Base;
    // It has left the battlefield its combat was at (CR 323.2.c).
    obj.combat_designation = CombatDesignation::None;
    carryAttachments(state_, obj);                 // CR 719.3.a
    events_.emit(ObjectStateChangedEvent{target, "recalled"});
}

void EffectExecutor::healExhaustRecall(GameObjectId unit) {
    if (!state_.objectExists(unit)) return;
    healObject(unit);
    exhaustObject(unit);
    recallToBase(unit);
}

namespace {
// What could replace one unit's death, in the order it is offered.
enum class DeathReplacementKind { RecallPending, Pending, BanishInstead, Card };
struct DeathReplacementCandidate {
    DeathReplacementKind kind;
    GameObjectId card_object = kInvalidId;                 // Kind::Card
    GameObject::PendingDeathReplacement pending{};         // Kind::Pending
    std::string label;
};
// Read-only: applyDeathReplacements uses up the unit's pending ones itself.
std::vector<DeathReplacementCandidate>
deathReplacementCandidates(const GameState& state, const EffectExecutor& exec,
                           GameObjectId unit_id) {
    using Kind = DeathReplacementKind;
    std::vector<DeathReplacementCandidate> candidates;
    if (!state.objectExists(unit_id)) return candidates;
    const auto& unit = state.getObject(unit_id);
    if (!unit.isUnit() || !unit.location.has_value()) return candidates;
    const PlayerId controller = unit.controller;
    // "The next time it would die this turn, heal it, exhaust it, and
    // recall it instead." (Tactical Retreat, Highlander)
    if (unit.death_replacement_recall_pending)
        candidates.push_back({Kind::RecallPending, kInvalidId, {},
                              "heal it, exhaust it and recall it instead"});
    // Replacements an effect put on this unit.
    for (const auto& r : unit.pending_death_replacements) {
        if (r.turn != state.turn.turn_number) continue;   // its turn is over
        const Card* card = exec.cardFor(r.card);
        if (!card) continue;
        candidates.push_back({Kind::Pending, kInvalidId, r, card->def().name});
    }
    // "If it would die this turn, banish it instead." (Smite)
    if (unit.banish_if_would_die_this_turn)
        candidates.push_back({Kind::BanishInstead, kInvalidId, {}, "banish it instead"});
    // "If a unit here would die ..." on the battlefield it is at (Altar of
    // Blood); then its controller's cards in play, by object id.
    std::vector<GameObjectId> sources;
    if (const auto dying_bf = unit.battlefieldId())
        for (const auto& bf : state.battlefields)
            if (bf.id == *dying_bf && state.objectExists(bf.card_object_id))
                sources.push_back(bf.card_object_id);
    std::vector<GameObjectId> in_play;
    for (const auto& [id, other] : state.objects.inPlay()) {
        if (other.controller != controller || id == unit_id) continue;
        if (!other.location.has_value() && other.zone != ZoneType::LegendZone) continue;
        in_play.push_back(id);
    }
    std::sort(in_play.begin(), in_play.end());
    sources.insert(sources.end(), in_play.begin(), in_play.end());
    for (auto sid : sources) {
        const Card* card = exec.cardFor(state.getObject(sid).card_def_id);
        if (!card || !card->hasReplacementEffect()) continue;
        if (!card->replacementApplies(state, sid, controller, unit_id)) continue;
        candidates.push_back({Kind::Card, sid, {}, state.getObject(sid).name});
    }
    return candidates;
}
}  // namespace

bool EffectExecutor::couldReplaceDeath(GameObjectId unit) const {
    return !deathReplacementCandidates(state_, *this, unit).empty();
}

bool EffectExecutor::applyDeathReplacements(GameObjectId unit_id) {
    if (!state_.objectExists(unit_id)) return false;
    if (!state_.getObject(unit_id).isUnit() || !onBoard(state_.getObject(unit_id))) return false;
    const PlayerId controller = state_.getObject(unit_id).controller;

    using Kind = DeathReplacementKind;
    using Candidate = DeathReplacementCandidate;
    std::vector<Candidate> candidates = deathReplacementCandidates(state_, *this, unit_id);
    // "The next time it would die": this death uses each pending one up,
    // applied or not.
    state_.getObject(unit_id).pending_death_replacements.clear();

    auto apply = [&](const Candidate& c) -> bool {
        auto& unit = state_.getObject(unit_id);
        switch (c.kind) {
            case Kind::RecallPending:
                unit.death_replacement_recall_pending = false;
                RIFT_DEBUG(events_, "REPLACEMENT (self): " + unit.name +
                                 " heals/exhausts/recalls instead of dying");
                healExhaustRecall(unit_id);
                return true;
            case Kind::BanishInstead:
                unit.banish_if_would_die_this_turn = false;
                RIFT_DEBUG(events_, "REPLACEMENT (self): " + unit.name +
                                 " is banished instead of dying");
                banishObject(unit_id);
                return true;
            case Kind::Pending: {
                Card* card = cardFor(c.pending.card);
                CardContext rctx{state_, events_, *this, c.pending.controller, c.pending.source};
                if (!card || !card->applyReplacement(rctx, unit_id)) return false;
                RIFT_DEBUG(events_, "REPLACEMENT (self): " + state_.getObject(unit_id).name +
                                 " does not die");
                return true;
            }
            case Kind::Card: {
                if (!state_.objectExists(c.card_object)) return false;
                Card* card = cardFor(state_.getObject(c.card_object).card_def_id);
                CardContext rctx{state_, events_, *this, controller, c.card_object};
                if (!card || !card->applyReplacement(rctx, unit_id)) return false;
                RIFT_DEBUG(events_, "REPLACEMENT: " + state_.getObject(c.card_object).name +
                                 " prevents " + state_.getObject(unit_id).name + " from dying");
                return true;
            }
        }
        return false;
    };

    while (!candidates.empty()) {
        // The unit may have left the board through an earlier candidate's
        // side effects without that counting as a replacement.
        if (!state_.objectExists(unit_id) || !onBoard(state_.getObject(unit_id))) return true;
        size_t pick = 0;
        if (candidates.size() > 1 && agent_query_ && controller != PlayerId::None) {
            std::vector<Intent> options;
            for (size_t i = 0; i < candidates.size(); ++i) {
                Intent o;
                o.type = IntentType::MakeChoice;
                o.player = controller;
                o.chosen_value = static_cast<int>(i);
                o.choice_label = state_.getObject(unit_id).name + " would die — apply: " +
                                 candidates[i].label;
                options.push_back(std::move(o));
            }
            const int chosen = ask(controller, options,
                state_.getObject(unit_id).name + " would die — which replacement applies first (CR 372)")
                .chosen_value.value_or(0);
            if (chosen >= 0 && chosen < static_cast<int>(candidates.size()))
                pick = static_cast<size_t>(chosen);
        }
        const Candidate c = candidates[pick];
        candidates.erase(candidates.begin() + static_cast<ptrdiff_t>(pick));
        if (apply(c)) return true;
    }
    return false;
}

bool EffectExecutor::attachGear(GameObjectId gear_id, GameObjectId unit_id) {
    if (!state_.objectExists(gear_id) || !state_.objectExists(unit_id)) return false;
    // Both ends are on the board (CR 716): a unit that died or was returned
    // to hand in response cannot be equipped, and a gear that left cannot attach.
    if (!state_.getObject(gear_id).isGear() || !onBoard(state_.getObject(gear_id))) return false;
    if (!state_.getObject(unit_id).isUnit() || !onBoard(state_.getObject(unit_id))) return false;
    if (state_.getObject(gear_id).attached_to == std::optional<GameObjectId>{unit_id}) return true;
    // CR 718.5.d: attached to one card at a time — it comes off the old one.
    if (state_.getObject(gear_id).attached_to.has_value()) unattachGear(gear_id);
    auto& gear = state_.getObject(gear_id);
    auto& unit = state_.getObject(unit_id);
    RIFT_TRACE(events_, "ATTACH: " + gear.name + " (id=" + toString(gear_id) + ") -> " +
                     unit.name + " (id=" + toString(unit_id) + ", might_bonus=" +
                     std::to_string(gear.might_bonus) + ")");
    gear.attached_to = unit_id;
    gear.is_rules_text_inactive = true;            // CR 718.2
    unit.attachments.push_back(gear_id);
    gear.location = unit.location;                 // CR 719.3
    gear.zone = unit.zone;
    unit.attachment_might_bonus += gear.might_bonus;
    unit.recomputeMight();
    events_.emit(ObjectStateChangedEvent{gear_id, "attached"});
    events_.emit(ObjectStateChangedEvent{unit_id, "equipped"});
    return true;
}

int EffectExecutor::textCopiesOn(GameObjectId unit_id) const {
    if (!state_.objectExists(unit_id)) return 0;
    const auto& unit = state_.getObject(unit_id);
    if (!unit.isUnit()) return 0;
    if (!unit.location.has_value())
        return unit.last_known.valid ? unit.last_known.text_copies : 0;
    int n = 0;
    for (auto gid : unit.attachments) {
        if (!state_.objectExists(gid)) continue;
        const auto& gear = state_.getObject(gid);
        if (!gear.attached_to.has_value() || *gear.attached_to != unit_id) continue;
        const Card* card = cardFor(gear.card_def_id);
        if (card && card->copiesBearerText()) ++n;
    }
    return n;
}

bool EffectExecutor::spendBuff(GameObjectId unit_id, PlayerId spender) {
    if (!state_.objectExists(unit_id)) return false;
    auto& unit = state_.getObject(unit_id);
    if (!onBoard(unit) || unit.buff_count <= 0) return false;
    unit.buff_count -= 1;
    unit.recomputeMight();
    RIFT_TRACE(events_, std::string("EFFECT: ") + toString(spender) + " spends a buff from " +
                     unit.name + " -> " + std::to_string(unit.current_might) + "M");
    // The spender rides in a counter on the unit for the dispatch (the event
    // carries only the object); read at once by TriggerManager.
    unit.card_counters["__buff_spent_by"] = static_cast<int>(spender);
    events_.emit(ObjectStateChangedEvent{unit_id, "buff_spent"});
    return true;
}

void EffectExecutor::announceRecycledToMainDeck(GameObjectId card) {
    if (!state_.objectExists(card)) return;
    if (state_.getObject(card).zone != ZoneType::MainDeck) return;
    events_.emit(ObjectStateChangedEvent{card, "recycled_main"});
}

void EffectExecutor::drawHeldCard(PlayerId player, GameObjectId card, bool seen_by_all) {
    if (!state_.objectExists(card)) return;
    auto& obj = state_.getObject(card);
    deck::putInHand(state_, card,
                    seen_by_all ? knowledge::everyone() : knowledge::only(player),
                    deck::Place::Held, player);
    if (seen_by_all)
        RIFT_TRACE(events_, "  DREW: " + obj.name + " (id=" + toString(card) + ")");
    else
        RIFT_TRACE_PRIVATE(events_, player, "  DREW: " + obj.name + " (id=" +
                                        toString(card) + ")");
    state_.player(player).draws_this_turn += 1;
    events_.emit(CardsDrawnEvent{player, 1});
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
        RIFT_TRACE(events_, "MOVE_BLOCKED: " + obj.name + " can't be moved by enemy effects");
        return;
    }
    // "They can't move it this turn" (GameObject::cant_move_this_turn): its
    // controller's spells and abilities don't move it either.
    if (obj.cant_move_this_turn && state_.chain.resuming.has_value() &&
        state_.chain.resuming->controller == obj.controller) {
        RIFT_TRACE(events_, "MOVE_BLOCKED: " + obj.name + " can't be moved this turn");
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
    RIFT_TRACE(events_, "EFFECT: stun " + obj.name + " (id=" + toString(target) + ")");

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
    } else if (state_.chain.resuming.has_value()) {
        // No source named: the stun is the effect of the spell or ability
        // that is resolving, so its controller is who stunned the unit.
        stunner_ctrl = state_.chain.resuming->controller;
    }
    events_.emit(UnitStunnedEvent{target, stunner_ctrl, obj.controller});
}

void EffectExecutor::discardCards(PlayerId player, int count) {
    RIFT_TRACE(events_, std::string("EFFECT: ") + toString(player) + " discards " +
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
            auto chosen = ask(player, choices, "discard a card");
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
            RIFT_TRACE(events_, "  DISCARDED: " + state_.getObject(to_discard).name +
                             " (id=" + toString(to_discard) + ")");
            ps.hand.erase(it);
            state_.getObject(to_discard).zone = ZoneType::Trash;
            ps.trash.push_back(to_discard);
            settleInTrash(state_, events_, to_discard);
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
    RIFT_TRACE(events_, std::string("EFFECT: ") + toString(opp) + " forced to discard " +
                     std::to_string(count));
    discardCards(opp, count);
}

void EffectExecutor::applyDiscard(PlayerId player, GameObjectId card_id) {
    if (!state_.objectExists(card_id)) return;
    auto& ps = state_.player(player);
    auto it = std::find(ps.hand.begin(), ps.hand.end(), card_id);
    if (it == ps.hand.end()) return;
    auto& obj = state_.getObject(card_id);
    RIFT_TRACE(events_, "  DISCARDED: " + obj.name + " (id=" +
                     toString(card_id) + ")");
    ps.hand.erase(it);
    obj.zone = ZoneType::Trash;
    ps.trash.push_back(card_id);
    settleInTrash(state_, events_, card_id);
    ps.has_discarded_this_turn = true;
    events_.emit(CardsDiscardedEvent{player, {card_id}});
    events_.emit(ObjectStateChangedEvent{card_id, "discarded"});
}

void EffectExecutor::recycleCards(PlayerId effect_controller,
                                   const std::vector<GameObjectId>& cards,
                                   knowledge::Seers shown) {
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
    // A permanent recycled from the board leaves it like any other departure:
    // what is attached comes off first (CR 719.5), a token ceases to exist
    // instead of entering a deck (CR 183.1), and the departure is announced
    // once the card is in its deck.
    struct Left { GameObjectId id; PlayerId controller; CardType type; LocationId was_at; };
    std::vector<Left> left_board;
    std::vector<GameObjectId> to_recycle;
    std::vector<std::pair<GameObjectId, deck::Place>> came_from;
    for (auto cid : cards) {
        if (!state_.objectExists(cid)) continue;
        if (std::find(to_recycle.begin(), to_recycle.end(), cid) != to_recycle.end()) continue;
        if (onBoard(state_.getObject(cid))) {
            snapshotLastKnown(cid, /*dying=*/false);   // as it was on the board
            shedAttachments(state_, *this, cid);
            auto& obj = state_.getObject(cid);
            const LocationId was_at = *obj.location;
            if (obj.isToken()) {
                RIFT_TRACE(events_, "EFFECT: recycle " + obj.name + " (id=" + toString(cid) +
                                 ") — token ceases to exist (CR 183.1)");
                const PlayerId controller = obj.controller;
                obj.zone = ZoneType::Banishment;
                obj.location = std::nullopt;
                events_.emit(LeftBoardEvent{cid, controller, obj.card_type, was_at,
                                            ZoneType::Banishment, false});
                leftBoard(cid);
                continue;
            }
            // (It becomes a new object when it has left and that has been
            // announced: leftBoard, the last thing this function does.)
            left_board.push_back({cid, obj.controller, obj.card_type, was_at});
        }
        // Where it is for the knowledge tracker, read before it is taken out
        // of its zone's list (a caller that already removed it loses nothing).
        came_from.push_back({cid, deck::whereIs(state_, cid)});
        for (auto& zps : state_.players) {
            auto drop = [&](std::vector<GameObjectId>& v) {
                v.erase(std::remove(v.begin(), v.end(), cid), v.end());
            };
            drop(zps.hand);
            drop(zps.trash);
            drop(zps.banishment);
        }
        to_recycle.push_back(cid);
    }
    auto placeOf = [&](GameObjectId cid) {
        for (const auto& [id, place] : came_from) if (id == cid) return place;
        return deck::Place::Public;
    };
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
            group.push_back({cid, placeOf(cid), shown});
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
    for (const auto& l : left_board)
        events_.emit(LeftBoardEvent{l.id, l.controller, l.type, l.was_at,
            l.type == CardType::Rune ? ZoneType::RuneDeck : ZoneType::MainDeck, false});

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
    // Announced, every one of them as it left: in a deck each is a new object
    // (CR 124.1).
    for (const auto& l : left_board) leftBoard(l.id);
}

void EffectExecutor::banishObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    // From the board: as it was there, for whatever reads it after the move.
    // (Banished instead of dying, Smite: it did not die.)
    if (onBoard(state_.getObject(target))) snapshotLastKnown(target, /*dying=*/false);
    shedAttachments(state_, *this, target);
    auto& obj = state_.getObject(target);
    RIFT_TRACE(events_, "EFFECT: banish " + obj.name + " (id=" + toString(target) + ")");
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

    if (was_at.has_value()) {
        events_.emit(LeftBoardEvent{target, controller, obj.card_type,
            *was_at, ZoneType::Banishment, false});
    }
    // "When you banish a card you own" (Master of Shadows).
    if (!is_token) events_.emit(ObjectStateChangedEvent{target, "banished_owned"});
    // Announced as it left: what is in Banishment is a new object (CR 124.1).
    if (was_at.has_value()) leftBoard(target);
}

void EffectExecutor::forgetDamageSources(GameObject& obj) {
    obj.damage_marked_by[0] = obj.damage_marked_by[1] = 0;
    obj.card_counters.erase(spellDamageKey(PlayerId::Player1));
    obj.card_counters.erase(spellDamageKey(PlayerId::Player2));
}

void EffectExecutor::healObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    obj.damage_marked = 0;
    forgetDamageSources(obj);
    events_.emit(ObjectStateChangedEvent{target, "healed"});
}

void EffectExecutor::exhaustObject(GameObjectId target) {
    if (!state_.objectExists(target)) return;
    auto& obj = state_.getObject(target);
    obj.is_exhausted = true;
    events_.emit(ObjectStateChangedEvent{target, "exhausted"});
}

void EffectExecutor::channelRunes(PlayerId player, int count, bool enter_exhausted) {
    RIFT_TRACE(events_, std::string("EFFECT: ") + toString(player) + " channels " +
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

        RIFT_TRACE(events_, "  CHANNELED: " + rune.name + " (id=" +
                         toString(rune_id) +
                         (enter_exhausted ? ", exhausted" : ", ready") + ")");
        events_.emit(RuneChanneledEvent{rune_id, player});
        events_.emit(EnteredBoardEvent{
            rune_id, player, CardType::Rune, BaseLocation{player}, false});
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Add abilities (CR 429) — floating energy / power / universal power
// ═══════════════════════════════════════════════════════════════════════════════

bool EffectExecutor::confirmNow(PlayerId player, const std::string& label) {
    if (!agent_query_) return false;
    Intent no;
    no.type = IntentType::MakeChoice;
    no.player = player;
    no.chosen_value = 0;
    no.choice_label = "No — " + label;
    Intent yes = no;
    yes.chosen_value = 1;
    yes.choice_label = "Yes — " + label;
    return ask(player, {no, yes}, label).chosen_value.value_or(0) == 1;
}

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
    const Intent chosen = ask(player, options, "choose one");
    if (!chosen.chosen_objects.empty() &&
        std::find(objects.begin(), objects.end(), chosen.chosen_objects.front()) != objects.end())
        return chosen.chosen_objects.front();
    return objects.front();
}

LocationId EffectExecutor::chooseLocationNow(PlayerId player,
                                             const std::vector<LocationId>& locations,
                                             const std::string& label) {
    assert(!locations.empty());
    if (!agent_query_ || locations.size() == 1) return locations.front();
    std::vector<Intent> options;
    options.reserve(locations.size());
    for (size_t i = 0; i < locations.size(); ++i) {
        Intent o;
        o.type = IntentType::MakeChoice;
        o.player = player;
        o.chosen_value = static_cast<int>(i);
        const auto& loc = locations[i];
        o.choice_label = label + (std::holds_alternative<BaseLocation>(loc)
            ? std::string(" base")
            : " battlefield #" + toString(std::get<BattlefieldLocation>(loc).id));
        options.push_back(std::move(o));
    }
    const int pick = ask(player, options, label).chosen_value.value_or(0);
    if (pick < 0 || pick >= static_cast<int>(locations.size())) return locations.front();
    return locations[static_cast<size_t>(pick)];
}

void EffectExecutor::addUnitOnlyEnergy(PlayerId player, int amount) {
    state_.player(player).rune_pool.unit_energy += amount;
    RIFT_TRACE(events_, "EFFECT: " + std::string(toString(player)) + " adds " +
                     std::to_string(amount) + " Energy for units / unit abilities only");
}

void EffectExecutor::addFloatingEnergy(PlayerId player, int amount) {
    state_.player(player).rune_pool.energy += amount;
    RIFT_TRACE(events_, "EFFECT: " + std::string(toString(player)) + " adds " +
                     std::to_string(amount) + " floating Energy (pool=" +
                     std::to_string(state_.player(player).rune_pool.energy) + ")");
}

void EffectExecutor::addFloatingPower(PlayerId player, Domain domain, int amount) {
    int idx = static_cast<int>(domain);
    state_.player(player).rune_pool.power[idx] += amount;
    RIFT_TRACE(events_, "EFFECT: " + std::string(toString(player)) + " adds " +
                     std::to_string(amount) + " floating " + toString(domain) +
                     " Power (pool=" +
                     std::to_string(state_.player(player).rune_pool.power[idx]) + ")");
}

void EffectExecutor::addFloatingUniversalPower(PlayerId player, int amount) {
    state_.player(player).rune_pool.universal_power += amount;
    RIFT_TRACE(events_, "EFFECT: " + std::string(toString(player)) + " adds " +
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
    RIFT_TRACE(events_, "EFFECT: " + obj.name + " (id=" + toString(target) +
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
    RIFT_TRACE(events_, "EFFECT: " + obj.name + " (id=" + toString(target) +
                     ") control: " + toString(old_controller) + " -> " +
                     toString(new_controller) + " (until source id=" +
                     toString(source) + " leaves board)");
    events_.emit(ObjectStateChangedEvent{target, "controller_changed"});
}

// ═══════════════════════════════════════════════════════════════════════════════
// Copy effects
// ═══════════════════════════════════════════════════════════════════════════════

void EffectExecutor::copyUnit(GameObjectId token_id, GameObjectId source_id) {
    if (!state_.objectExists(token_id) || !state_.objectExists(source_id)) return;

    auto& token = state_.getObject(token_id);
    auto& src = state_.getObject(source_id);

    RIFT_TRACE(events_, "COPY: " + token.name + " (id=" + toString(token_id) +
                     ") becomes copy of " + src.name + " (id=" + toString(source_id) + ")");

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
                                          bool enter_ready,
                                          const std::vector<LocationId>& location_choices,
                                          CardDefId token_card, bool enters_exhausted) {
    if (token_depth_ == 0) last_tokens_.clear();
    const bool asked_enter_ready = enter_ready;
    auto id = state_.createObject();
    last_tokens_.push_back(id);
    auto& obj = state_.getObject(id);
    obj.owner = controller;
    obj.controller = controller;
    obj.card_type = type;
    obj.super_type = SuperType::Token;
    obj.is_token = true;   // CR 185.1 intrinsic
    obj.card_def_id = token_card;
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
    // "Units you play this turn enter ready." (a resolved spell's effect), and
    // "Other friendly units enter ready." (Magma Wurm) which covers unit tokens too.
    if (type == CardType::Unit &&
        (state_.player(controller).units_enter_ready_this_turn ||
         state_.player(controller).otherUnitsEnterReady(obj.id)))
        enter_ready = true;
    // Units enter exhausted unless ready; anything enters exhausted when its
    // instruction says so and "your tokens enter ready" does not replace that.
    obj.is_exhausted = (!enter_ready && type == CardType::Unit) ||
                       (enters_exhausted && !state_.player(controller).tokens_enter_ready);

    if (std::holds_alternative<BaseLocation>(location)) {
        obj.zone = ZoneType::Base;
    } else {
        obj.zone = ZoneType::BattlefieldZone;
    }

    obj.recomputeMight();

    RIFT_TRACE(events_, "TOKEN: created " + name + " (" + std::to_string(might) +
                     "M, id=" + toString(id) + ") for " + toString(controller));

    events_.emit(TokenCreatedEvent{id, controller, type, name, location});
    events_.emit(EnteredBoardEvent{id, controller, type, location, false});

    // "Once each turn, if you would play a token unit ..., you may play that
    // token and an additional copy of it instead." (Zilean, Time Mage; the
    // card counts itself in PlayerState::additional_token_copy_sources while
    // its condition holds; each source is once each turn on its own.) A
    // "may": the controller is asked. The copy is played by the same
    // instruction, so it goes where that instruction allows: the named
    // location, or — when the controller chose among several — a location
    // chosen again for the copy. Using the effect stamps the turn first, which
    // also keeps the copy from being doubled in turn.
    if (type == CardType::Unit && token_depth_ == 0) {
        auto& ps = state_.player(controller);
        // With no agent attached (an executor driven on its own) the "may" is
        // taken, as Card::confirmOptional does outside the chain.
        if (ps.additional_token_copies_turn != state_.turn.turn_number) {
            ps.additional_token_copies_turn = state_.turn.turn_number;
            ps.additional_token_copies_used = 0;
        }
        // Each source is its own Replacement Effect: all of them can apply to
        // this one play, each once (CR 370.2, 372), and each "may" is asked
        // on its own — one that is declined has not been applied this turn
        // (CR 371.2.b). A source applies once to the event or to what
        // replaced it, so each adds one copy: two sources, three tokens.
        const int available = ps.additional_token_copy_sources - ps.additional_token_copies_used;
        for (int n = 0; n < available; ++n) {
            // With no agent attached (an executor driven on its own) the
            // "may" is taken, as Card::confirmOptional does outside the chain.
            if (agent_query_ &&
                !confirmNow(controller, "play an additional copy of " + name + "?"))
                continue;
            ++state_.player(controller).additional_token_copies_used;
            const std::vector<LocationId>& choices =
                location_choices.empty() ? token_location_choices_ : location_choices;
            LocationId copy_at = location;
            if (choices.size() > 1)
                copy_at = chooseLocationNow(controller, choices,
                                            "play the additional " + name + " to");
            RIFT_TRACE(events_, "TOKEN: an additional copy of " + name + " is played");
            ++token_depth_;
            createToken(controller, type, name, might, tags, keywords, copy_at,
                        asked_enter_ready, location_choices, token_card, enters_exhausted);
            --token_depth_;
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
    bf.id = BattlefieldId::fromRaw(state_.battlefields.size());
    bf.card_object_id = bf_card_id;
    bf.is_token = true;
    bf.accepts_any_inbound = accepts_any_inbound;
    state_.battlefields.push_back(bf);

    RIFT_TRACE(events_, "BF_TOKEN: added '" + name + "' (id=" +
                     toString(bf.id) + ", card=" +
                     toString(bf_card_id) +
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

    RIFT_TRACE(events_, "BF_REPLACE: BF#" + toString(target_bf) +
                     " replaced by '" + token_name + "' token (card=" +
                     toString(token_id) + ")");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Composite operations
// ═══════════════════════════════════════════════════════════════════════════════

// Void Hatchling (341): "If you would reveal cards from a deck, look at the
// top card first. You may recycle it. Then reveal those cards."
void EffectExecutor::lookBeforeReveal(PlayerId player, ZoneType deck) {
    auto& ps = state_.player(player);
    if (deck == ZoneType::RuneDeck) {
        // The Rune Deck is "a deck" (judge ruling 2026-10-11, plans/09 9.R5):
        // the look and the "may recycle it" come before a reveal from it too.
        // Its top is the back of the list; a recycled rune goes to the bottom.
        //
        // Knowledge (plans/09 9.NN4). After this the player knows the top
        // rune (left there) or the bottom one (recycled), and a rune a card
        // then reveals and recycles is known to both players at the bottom.
        // None of it is recorded, and none of it can reach an observer:
        //  - the state features hold only the SIZE of each Rune Deck
        //    (ml/feature_extractor.cpp, `ps.rune_deck.size()`), so neither
        //    ObservationTensor nor InfoSetId (a hash of those features) can
        //    depend on its order;
        //  - KnowledgeView answers "identity unknown" for every object in a
        //    Rune Deck, for both observers (knowledge_tracker.cpp);
        //  - the play server sends a Rune Deck's cards only to a viewer who
        //    may see the decks (`show_decks`); fair-play sends the count.
        // (Pinned by PerfEngineRuneDeck.* in tests/test_review_fix_perf_engine.cpp.)
        // What a human at the table would know and an agent cannot use is
        // therefore a missing FEATURE, for the v6 token layout: per observer,
        // the known top and bottom runes of each Rune Deck.
        // The one place the rune is named is the private trace line below:
        // the option labels and the public lines do not name it.
        if (!ps.has_reveal_peek || ps.rune_deck.empty() || !agent_query_) return;
        const GameObjectId top = ps.rune_deck.back();
        if (!state_.objectExists(top)) return;
        RIFT_TRACE_PRIVATE(events_, player, "  PEEKED: " + state_.getObject(top).name + " (id=" +
                         toString(top) + ") on top of the Rune Deck [Void Hatchling]");
        Intent keep;  keep.type  = IntentType::MakeChoice; keep.player  = player; keep.chosen_value  = 0;
        Intent recyc; recyc.type = IntentType::MakeChoice; recyc.player = player; recyc.chosen_value = 1;
        keep.choice_label = "Void Hatchling: leave the top rune of your Rune Deck";
        recyc.choice_label = "Void Hatchling: recycle the top rune of your Rune Deck";
        const auto chosen = ask(player, {keep, recyc},
            "Void Hatchling: look at the top rune of your Rune Deck");
        if (chosen.chosen_value.value_or(0) == 1) {
            ps.rune_deck.pop_back();
            recycleCards(player, {top});
            RIFT_TRACE(events_, "VOID HATCHLING: recycled the peeked top rune before revealing");
        }
        return;
    }
    if (!ps.has_reveal_peek || ps.main_deck.empty() || !agent_query_) return;
    GameObjectId top = ps.main_deck.back();
    if (!state_.objectExists(top)) return;
    // A private look at the top card. The card is named on one line only,
    // readable by the player who looked (LogEvent::visibleTo); the options
    // do not name it, because option labels are not audience-limited
    // (plans/09 9.OO2 — the same form as the Rune Deck look above).
    knowledge::shown(state_, top, knowledge::only(player));
    RIFT_TRACE_PRIVATE(events_, player, "  PEEKED: " + state_.getObject(top).name + " (id=" +
                     toString(top) + ") on top of the Main Deck [Void Hatchling]");
    Intent keep;  keep.type  = IntentType::MakeChoice; keep.player  = player; keep.chosen_value  = 0;
    Intent recyc; recyc.type = IntentType::MakeChoice; recyc.player = player; recyc.chosen_value = 1;
    keep.choice_label = "Void Hatchling: leave the top card of your Main Deck";
    recyc.choice_label = "Void Hatchling: recycle the top card of your Main Deck";
    auto chosen = ask(player, {keep, recyc}, "Void Hatchling: look at the top card of your Main Deck");
    if (chosen.chosen_value.value_or(0) == 1) {
        deck::takeTop(state_, player, deck::Place::Held, knowledge::only(player));
        recycleCards(player, {top});
        RIFT_TRACE(events_, "VOID HATCHLING: recycled the peeked top card before revealing");
    }
}

std::pair<GameObjectId, std::vector<GameObjectId>>
EffectExecutor::revealUntil(PlayerId player, CardType condition) {
    auto& ps = state_.player(player);
    lookBeforeReveal(player);
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
        RIFT_TRACE(events_, "  REVEAL: " + obj.name + " (id=" +
                         toString(card_id) + ", " +
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

    RIFT_TRACE(events_, std::string("PREDICT: ") + toString(player) +
                     " looks at top " + std::to_string(actual) + " cards");

    // Peek at top N cards (remove from deck temporarily)
    const auto looker = knowledge::only(player);
    std::vector<GameObjectId> peeked;
    for (int i = 0; i < actual; ++i) {
        peeked.push_back(deck::takeTop(state_, player, deck::Place::Held, looker));
    }
    // Per CR 437.1, Predict is a PRIVATE look — only the predicting
    // player sees the cards. Log each peeked card with a "PRIVATE" tag
    // so V&V can verify what the agent saw (the engine doesn't yet
    // persist this — see Phase 10 ObservationTracker — but the trace
    // does). Emit CardRevealedEvent with revealed_to=player /
    // revealed_to_all=false so a future Phase 10 subscriber can record
    // the private observation.
    for (auto card_id : peeked) {
        if (!state_.objectExists(card_id)) continue;
        auto& obj = state_.getObject(card_id);
        RIFT_TRACE_PRIVATE(events_, player, "  PEEKED: " + obj.name + " (id=" +
                                        toString(card_id) +
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

    // The player decides, card by card, whether to recycle it (CR 437.1), and
    // then the order of the cards that stay.
    if (agent_query_) {
        // Offer choice: recycle each card or keep it
        GameObjectId recycled = kInvalidId;
        std::vector<GameObjectId> kept;   // in the order they were on the deck, top first
        for (auto card_id : peeked) {
            std::vector<Intent> choices;
            // Choice 1: recycle (put on bottom)
            Intent recycle_choice;
            recycle_choice.type = IntentType::MakeChoice;
            recycle_choice.player = player;
            recycle_choice.chosen_objects = {card_id};
            choices.push_back(recycle_choice);
            // Choice 2: keep on top
            // "Keep it": named by value 0, so the option can be told from the
            // other by more than its position (a card's prompt is answered by
            // value or by object).
            Intent keep_choice;
            keep_choice.type = IntentType::MakeChoice;
            keep_choice.player = player;
            keep_choice.chosen_value = 0;
            keep_choice.choice_label = "Keep it";
            choices.push_back(keep_choice);

            auto chosen = ask(player, choices, "Predict: recycle the top card or keep it");
            if (!chosen.chosen_objects.empty()) {
                // Recycle: put on bottom
                deck::putOnBottom(state_, player, card_id, deck::Place::Held, looker);
                RIFT_TRACE_PRIVATE(events_, player, "PREDICT: recycled " +
                                                state_.getObject(card_id).name);
                recycled = card_id;
            } else {
                kept.push_back(card_id);
            }
        }
        // The cards that stay go back "in any order": with two or more the
        // player names the one that ends up on top, then the next one down.
        // (Taking the first option each time puts them back as they were.)
        std::vector<GameObjectId> order;   // order[0] = top of the deck
        while (kept.size() > 1) {
            std::vector<Intent> choices;
            for (auto kid : kept) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = player;
                c.chosen_objects = {kid};
                c.choice_label = "Predict: put on top";
                choices.push_back(std::move(c));
            }
            auto chosen = ask(player, choices, "Predict: which card goes on top");
            GameObjectId top = kept.front();
            if (!chosen.chosen_objects.empty() &&
                std::find(kept.begin(), kept.end(), chosen.chosen_objects.front()) != kept.end())
                top = chosen.chosen_objects.front();
            order.push_back(top);
            kept.erase(std::remove(kept.begin(), kept.end(), top), kept.end());
        }
        if (!kept.empty()) order.push_back(kept.front());
        for (auto it = order.rbegin(); it != order.rend(); ++it)
            deck::putOnTop(state_, player, *it, deck::Place::Held, looker);
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
    lookBeforeReveal(player);
    int actual = std::min(count, static_cast<int>(ps.main_deck.size()));
    std::vector<GameObjectId> chosen_cards;
    if (actual <= 0) return chosen_cards;

    RIFT_TRACE(events_, std::string("REVEAL: ") + toString(player) +
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
        RIFT_TRACE(events_, "  REVEALED: " + obj.name + " (id=" +
                         toString(card_id) + ") — PUBLIC");

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
            skip_it.chosen_value = 0;      // named by value, as "Keep it" is
            skip_it.choice_label = "Leave it";
            choices.push_back(skip_it);

            auto chosen = ask(player, choices, "reveal: take a card or leave it");
            if (!chosen.chosen_objects.empty()) {
                deck::putInHand(state_, card_id, knowledge::everyone(), deck::Place::Held, player);
                chosen_cards.push_back(card_id);
                RIFT_TRACE(events_, "  CHOSE: draw " + obj.name);
            } else {
                // Recycle to bottom
                deck::putOnBottom(state_, player, card_id, deck::Place::Held, knowledge::everyone());
                RIFT_TRACE(events_, "  CHOSE: recycle " + obj.name);
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
    // `from` is the zone the card is being played from: a card played by an
    // effect from anywhere but a hand is "played from somewhere other than
    // your hand" for its own cost (Drag Under).
    StagedEffectCost(PlayerState& p, const EffectExecutor::EffectPlayCost& c,
                     ZoneType from = ZoneType::Hand)
        : ps(p), source_(p.current_play_source) {
        ps.transient_ignore_energy = c.ignore_energy;
        ps.transient_ignore_power = c.ignore_power;
        ps.transient_play_discount += c.energy_reduction;
        reduction_ = c.energy_reduction;
        // The zone it is really played from (every reader today only asks
        // "not from hand", but the value must be true).
        switch (from) {
            case ZoneType::Hand: break;
            case ZoneType::Trash:        ps.current_play_source = Intent::PlaySource::Trash; break;
            case ZoneType::ChampionZone: ps.current_play_source = Intent::PlaySource::ChampionZone; break;
            case ZoneType::FacedownZone: ps.current_play_source = Intent::PlaySource::Hidden; break;
            case ZoneType::Chain:        ps.current_play_source = Intent::PlaySource::ChainZone; break;
            case ZoneType::MainDeck:     ps.current_play_source = Intent::PlaySource::MainDeck; break;
            default:                     ps.current_play_source = Intent::PlaySource::Banishment; break;
        }
    }
    ~StagedEffectCost() {
        ps.transient_ignore_energy = false;
        ps.transient_ignore_power = false;
        ps.transient_play_discount -= reduction_;
        ps.current_play_source = source_;
    }
    int reduction_ = 0;
    Intent::PlaySource source_;
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
    for (auto& [id, obj] : state.objects.atLocation(base))
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
        // Snapshot, move, announce, then the new object (CR 124.1) — the
        // order of every other way off the board. (This site announced
        // nothing before: "When you recycle a rune" did not hear it.)
        snapshotAsItLeaves(r);
        r.location = std::nullopt;
        r.zone = ZoneType::RuneDeck;
        ps.rune_deck.insert(ps.rune_deck.begin(), rid);
        ps.power_spent_this_turn++;
        events.emit(LeftBoardEvent{rid, player, CardType::Rune, base,
                                   ZoneType::RuneDeck, false});
        forgetBoardState(state.getObject(rid), nullptr);
        --p_need;
    }
    return true;
}
}  // namespace

bool EffectExecutor::canPlayPayingCost(PlayerId player, GameObjectId card,
                                       EffectPlayCost cost) const {
    if (!state_.objectExists(card)) return false;
    if (!can_afford_) {
        StagedEffectCost staged(state_.player(player), cost, state_.getObject(card).zone);
        return plainPay(state_, events_, player,
                        plainCostOf(state_, card_db_, player, card), /*pay=*/false);
    }
    StagedEffectCost staged(state_.player(player), cost, state_.getObject(card).zone);
    return can_afford_(player, card);
}

bool EffectExecutor::playPayingCost(PlayerId player, GameObjectId card,
                                    std::optional<LocationId> location,
                                    EffectPlayCost cost) {
    if (!state_.objectExists(card)) return false;
    if (!can_afford_ || !pay_cost_) {
        StagedEffectCost staged(state_.player(player), cost, state_.getObject(card).zone);
        if (!plainPay(state_, events_, player,
                      plainCostOf(state_, card_db_, player, card), /*pay=*/true))
            return false;
    } else {
        StagedEffectCost staged(state_.player(player), cost, state_.getObject(card).zone);
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
    // A new play, and nothing is paid for it: what an earlier play of this
    // card paid is forgotten (Card::forgetPlayPayments).
    Card::forgetPlayPayments(state_, cardFor(obj.card_def_id), player, card);

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
        // Units enter exhausted (CR 143.4) unless a continuous effect on the
        // board says otherwise ("Other friendly units enter ready.", "Units you
        // play this turn enter ready.") or the unit's own text does ("I enter
        // ready", Card::entersReadyOnPlay) — as GameEngine::resolvePermanent
        // decides it for a unit played from hand.
        bool ready = state_.player(player).otherUnitsEnterReady(card) ||
                     state_.player(player).units_enter_ready_this_turn;
        if (const Card* self_card = cardFor(obj.card_def_id))
            ready = ready || self_card->entersReadyOnPlay(state_, player, obj);
        obj.is_exhausted = !ready;
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
