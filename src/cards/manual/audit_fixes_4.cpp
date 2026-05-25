// Audit-fix batch 4 — isolated override file. Classes prefixed AF4_ and
// registered LAST so they win over generated stubs and earlier manual
// classes for the same card ids.
//
// Cards in this batch:
//   46  En Garde            (spell)   — conditional +1M "if only unit here"
//   220 Facebreaker         (spell)   — same-battlefield friendly+enemy stun
//   381 Ornn, Blacksmith    (unit)    — look-4 / reveal-gear / recycle (+WhenIHold)
//   444 Corrupt Enforcer    (unit)    — move:discard1 + win-combat:draw1
//   498 Blade of Ruined King(gear)    — equip cost [Y] + kill a friendly unit
//   543 Sett, Brawler       (unit)    — played/conquer buff; spend buff +4M
//   612 Iascylla            (unit)    — hold -> schedule next-main-phase move
//   643 Keeper of Masks     (unit)    — two Reflection tokens copy me
//   676 Nidalee, Cat Form   (unit)    — win-combat draw 1
//   737 Tactical Retreat    (spell)   — next death this turn -> heal/exhaust/recall
//   766 Forbidding Waste    (battlefield) — defending-alone -2M aura

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <string>
#include <vector>

namespace riftbound {

// ═══════════════════════════════════════════════════════════════════════════
// [46] En Garde — [Reaction] Give a friendly unit +1 [M] this turn, then an
// additional +1 [M] this turn if it is the only unit you control there.
// ═══════════════════════════════════════════════════════════════════════════
class AF4_EnGarde : public SpellCard {
public:
    AF4_EnGarde() : SpellCard(46) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId tid = targets[0];
        auto& tgt = ctx.state.getObject(tid);

        // Base +1 M this turn.
        ctx.executor.giveTemporaryMight(tid, 1);

        // Additional +1 M if it is the ONLY unit the controller has at its
        // location ("there"). Count friendly units sharing the target's
        // location (only meaningful when the unit is on board somewhere).
        if (!tgt.location.has_value()) return;
        int friendly_here = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (*obj.location != *tgt.location) continue;
            friendly_here++;
        }
        if (friendly_here == 1) {
            ctx.executor.giveTemporaryMight(tid, 1);
            ctx.events.logTrace("EN GARDE: only unit there -> additional +1 M");
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [220] Facebreaker — [Hidden][Action] Stun a friendly unit and an enemy unit
// at the same battlefield. (They don't deal combat damage this turn.)
// ═══════════════════════════════════════════════════════════════════════════
class AF4_Facebreaker : public SpellCard {
public:
    AF4_Facebreaker() : SpellCard(220) {}
    TargetRequirements getTargetRequirements() const override {
        // Two targets — friendly first, enemy second; the same-battlefield
        // pairing is enforced in pickTargetPair's legal_b_fn below.
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }

    // Playable only if some battlefield holds both a friendly and an enemy
    // unit (so a legal same-BF pair exists).
    bool hasLegalTargets(const GameState& state,
                          PlayerId controller) const override {
        for (auto& [fid, f] : state.objects) {
            if (!f.isUnit() || f.controller != controller) continue;
            auto fbf = f.battlefieldId();
            if (!fbf) continue;
            for (auto& [eid, e] : state.objects) {
                if (!e.isUnit() || e.controller == controller) continue;
                if (e.untargetable_by_enemy) continue;
                auto ebf = e.battlefieldId();
                if (ebf && *ebf == *fbf) return true;
            }
        }
        return false;
    }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // First pick: a friendly unit at a battlefield that ALSO has a
        // valid enemy unit (otherwise the pair can't complete).
        std::vector<GameObjectId> legal_friendly;
        for (auto& [fid, f] : ctx.state.objects) {
            if (!f.isUnit() || f.controller != ctx.controller) continue;
            auto fbf = f.battlefieldId();
            if (!fbf) continue;
            bool enemy_here = false;
            for (auto& [eid, e] : ctx.state.objects) {
                if (!e.isUnit() || e.controller == ctx.controller) continue;
                if (e.untargetable_by_enemy) continue;
                auto ebf = e.battlefieldId();
                if (ebf && *ebf == *fbf) { enemy_here = true; break; }
            }
            if (enemy_here) legal_friendly.push_back(fid);
        }

        auto enemy_fn = [&ctx](GameObjectId picked_a) {
            std::vector<GameObjectId> legal_enemy;
            if (!ctx.state.objectExists(picked_a)) return legal_enemy;
            auto abf = ctx.state.getObject(picked_a).battlefieldId();
            if (!abf) return legal_enemy;
            for (auto& [eid, e] : ctx.state.objects) {
                if (!e.isUnit() || e.controller == ctx.controller) continue;
                if (e.untargetable_by_enemy) continue;
                auto ebf = e.battlefieldId();
                if (ebf && *ebf == *abf) legal_enemy.push_back(eid);
            }
            return legal_enemy;
        };

        auto [friendly, enemy] = pickTargetPair(ctx, "Facebreaker",
                                                  legal_friendly, enemy_fn);
        bool suspending = (friendly == kInvalidId || enemy == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (friendly != kInvalidId && ctx.state.objectExists(friendly)) {
            ctx.executor.stunUnit(friendly);
        }
        if (enemy != kInvalidId && ctx.state.objectExists(enemy)) {
            ctx.executor.stunUnit(enemy);
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [381] Ornn, Blacksmith — When you play me or when I hold, look at the top 4
// cards of your Main Deck. You may reveal a gear from among them and draw it.
// Then recycle the rest.
// ═══════════════════════════════════════════════════════════════════════════
class AF4_OrnnBlacksmith : public UnitCard {
public:
    AF4_OrnnBlacksmith() : UnitCard(381) {}
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);

        // Look at top 4 (top = back of vector).
        std::vector<GameObjectId> peeked;
        for (int i = 0; i < 4 && !ps.main_deck.empty(); ++i) {
            peeked.push_back(ps.main_deck.back());
            ps.main_deck.pop_back();
        }
        if (peeked.empty()) return;

        // Reveal a gear from among them and draw it (first gear; "may" is
        // treated as always-take when a gear is present — consistent with
        // the other look-N drafters in this engine).
        GameObjectId drafted = kInvalidId;
        std::vector<GameObjectId> rest;
        for (auto id : peeked) {
            if (!ctx.state.objectExists(id)) { continue; }
            const auto& obj = ctx.state.getObject(id);
            if (drafted == kInvalidId && obj.isGear()) drafted = id;
            else rest.push_back(id);
        }

        if (drafted != kInvalidId) {
            auto& drafted_obj = ctx.state.getObject(drafted);
            ctx.events.emit(CardRevealedEvent{
                /*card=*/drafted,
                /*card_def_id=*/drafted_obj.card_def_id,
                /*owner=*/drafted_obj.owner,
                /*revealed_to_all=*/false,
                /*revealed_to=*/ctx.controller,
                /*source_zone=*/ZoneType::MainDeck,
            });
            ctx.events.logTrace("ORNN, BLACKSMITH: drafted gear " +
                                 drafted_obj.name);
            drafted_obj.zone = ZoneType::Hand;
            ps.hand.push_back(drafted);
        }

        // Recycle the rest to the bottom of the deck.
        if (!rest.empty()) {
            ctx.executor.recycleCards(ctx.controller, rest);
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [444] Corrupt Enforcer — When I move to a battlefield, discard 1. When I win
// a combat, draw 1.
// ═══════════════════════════════════════════════════════════════════════════
class AF4_CorruptEnforcer : public UnitCard {
public:
    AF4_CorruptEnforcer() : UnitCard(444) {}
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenIMoveToFB, TriggerType::WhenIWinCombat};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (ctx.firing_trigger == TriggerType::WhenIWinCombat) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("CORRUPT ENFORCER: win combat -> draw 1");
            return;
        }
        // WhenIMoveToFB (default branch): discard 1 via the resumable
        // single-card discard choice.
        discardThenAct(ctx, 1, "Corrupt Enforcer: discard 1");
    }

private:
    // Local copy of the resumable discard helper (the deck_cards.cpp version
    // is file-local and not exported). Posts one MakeChoice per hand card;
    // resume_point 1 commits the chosen discard.
    static void discardThenAct(CardContext& ctx, int count,
                                const std::string& label) {
        if (!ctx.state.chain.resuming.has_value()) {
            // No chain context (e.g. direct test invocation): best-effort
            // discard via executor's own chooser.
            ctx.executor.discardCards(ctx.controller, count);
            return;
        }
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        auto build_choices = [&]() {
            std::vector<Intent> out;
            for (auto cid : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {cid};
                out.push_back(std::move(c));
            }
            return out;
        };
        switch (ri.resume_point) {
        case 0: {
            if (ps.hand.empty() || count <= 0) return;
            ri.resume_data = {count};
            ctx.executor.requestChoice(ctx.controller, build_choices(), label);
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                ctx.executor.applyDiscard(ctx.controller, choice->chosen_objects[0]);
            }
            if (!ri.resume_data.empty()) ri.resume_data[0]--;
            if (ri.resume_data.empty() || ri.resume_data[0] <= 0 || ps.hand.empty()) {
                return;
            }
            ctx.executor.requestChoice(ctx.controller, build_choices(), label);
            return;
        }
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [498] Blade of the Ruined King — [Equip] — [Y], Kill a friendly unit.
// might_bonus=4. Additional equip cost: pay [Y] (recycle an Order rune) AND
// kill a friendly unit (other than the one being equipped).
// ═══════════════════════════════════════════════════════════════════════════
class AF4_BladeOfRuinedKing : public GearCard {
public:
    AF4_BladeOfRuinedKing() : GearCard(498) {}
    bool hasEquipAbility() const override { return true; }

    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& state = ctx.state;
        auto player = ctx.controller;
        auto& ps = state.player(player);
        auto base_loc = BaseLocation{player};

        // PRE-CHECK both portions of the additional cost before committing
        // anything (mirror standardEquip's all-or-nothing discipline):
        //   1) an Order ([Y]) power rune to recycle, and
        //   2) a killable friendly unit OTHER than the equip target.
        GameObjectId order_rune = kInvalidId;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isRune() || obj.controller != player) continue;
            if (!obj.location.has_value() ||
                *obj.location != LocationId{base_loc}) continue;
            for (auto d : obj.domains) {
                if (d == Domain::Order) { order_rune = id; break; }
            }
            if (order_rune != kInvalidId) break;
        }
        if (order_rune == kInvalidId) return false;

        std::vector<GameObjectId> killable;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != player) continue;
            if (!obj.location.has_value()) continue;
            if (id == unit) continue;  // can't kill the unit we're equipping
            killable.push_back(id);
        }
        if (killable.empty()) return false;

        // Commit cost 1: kill a friendly unit (agent choice).
        GameObjectId victim = pickTarget(ctx, "Blade of the Ruined King: "
                                              "kill a friendly unit", killable);
        if (victim == kInvalidId) {
            // Suspended for the agent choice — re-entry will resume.
            return false;
        }
        if (!state.objectExists(victim)) return false;
        ctx.executor.killObject(victim);

        // Commit cost 2: recycle the Order power rune for [Y].
        {
            auto& dr = state.getObject(order_rune);
            ctx.events.logTrace("  EQUIP_COST: recycled " + dr.name + " for [Y]");
            dr.location = std::nullopt;
            dr.zone = ZoneType::RuneDeck;
            ps.rune_deck.insert(ps.rune_deck.begin(), order_rune);
        }

        // Attach.
        if (!state.objectExists(unit)) return false;
        auto& gear = state.getObject(ctx.source);
        auto& unit_obj = state.getObject(unit);
        ctx.events.logTrace("EQUIP: " + gear.name + " -> " + unit_obj.name +
                             " (might_bonus=" + std::to_string(gear.might_bonus) + ")");
        gear.attached_to = unit;
        unit_obj.attachments.push_back(ctx.source);
        gear.location = unit_obj.location;
        gear.zone = unit_obj.zone;
        unit_obj.attachment_might_bonus += gear.might_bonus;
        unit_obj.recomputeMight();
        ctx.events.emit(ObjectStateChangedEvent{ctx.source, "attached"});
        ctx.events.emit(ObjectStateChangedEvent{unit, "equipped"});
        return true;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [543] Sett, Brawler — When I'm played and when I conquer, buff me. (If I
// don't have a buff, I get a +1 [M] buff.)  Spend my buff: Give me +4 [M] this
// turn.
// ═══════════════════════════════════════════════════════════════════════════
class AF4_SettBrawler : public UnitCard {
public:
    AF4_SettBrawler() : UnitCard(543) {}

    // Fix: fire on play AND conquer (NOT hold — the old impl over-fired via
    // WhenIConquerOrHold).
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquer};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // "If I don't have a buff, I get a +1 [M] buff." So only buff while
        // unbuffed (a buff is a persistent +1M counter; engine models it via
        // buff_count).
        if (self.buff_count > 0) return;
        ctx.executor.buffUnit(ctx.source);
        ctx.events.logTrace("SETT: buff me (+1M buff)");
    }

    // Activated: "Spend my buff: Give me +4 [M] this turn." Spending a buff
    // is the whole cost; no exhaust / rune cost. Gated on having a buff.
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {}; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (self.buff_count <= 0) return;  // no buff to spend
        self.buff_count -= 1;
        ctx.executor.giveTemporaryMight(ctx.source, 4);
        ctx.events.logTrace("SETT: spent a buff for +4M this turn");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [612] Iascylla — When I hold, at the start of your next Main Phase, you may
// move an enemy unit to this battlefield.
//
// Fix: the old impl moved immediately on hold. Correct timing schedules a
// DelayedAbility (trigger=AtStartOfMain) on hold; the move resolves at the
// start of the controller's next Main Phase. The triggering battlefield is
// stashed in card_counters so the deferred fire targets the right BF.
// ═══════════════════════════════════════════════════════════════════════════
class AF4_Iascylla : public UnitCard {
public:
    AF4_Iascylla() : UnitCard(612) {}
    // Only WhenIHold is declared as a static trigger. The deferred
    // "next Main Phase" move is delivered through a DelayedAbility
    // (checkDelayedAbilities fires it independently of triggerTypes()),
    // which resolves through onTrigger with firing_trigger == None — so
    // we MUST NOT also declare AtStartOfMain here, or the engine's
    // per-object AtStartOfMain scan would (double-)fire it every turn.
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);

        if (ctx.firing_trigger == TriggerType::WhenIHold) {
            // Schedule the deferred move for the controller's next Main
            // Phase. Record which battlefield "this" is, so the deferred
            // fire pulls the enemy to the correct BF even if Iascylla moves.
            auto bf = self.battlefieldId();
            if (!bf) return;
            self.card_counters["__iascylla_pull_bf"] = static_cast<int>(*bf);

            DelayedAbility da;
            da.source = ctx.source;
            da.card_def_id = cardDefId();
            da.controller = ctx.controller;
            da.trigger = TriggerType::AtStartOfMain;
            da.expires_on_turn = -1;  // persists past current turn until it fires
            ctx.state.delayed_abilities.push_back(da);
            ctx.events.logTrace("IASCYLLA: scheduled next-Main-Phase enemy pull");
            return;
        }

        // AtStartOfMain (deferred fire). Only act if a pull was scheduled.
        // IMPORTANT: do NOT erase the stash up front — confirmOptional /
        // pickTarget suspend resolution and the chain re-enters this onTrigger;
        // the stash must survive those re-entries. Erase only once the decision
        // definitively commits (no legal target / declined / moved).
        auto it = self.card_counters.find("__iascylla_pull_bf");
        if (it == self.card_counters.end()) return;
        BattlefieldId bf = static_cast<BattlefieldId>(it->second);

        std::vector<GameObjectId> legal;
        PlayerId opp = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.isAtBattlefield()) continue;
            legal.push_back(id);
        }
        auto still_legal = [&legal]() { return !legal.empty(); };
        if (!still_legal()) { self.card_counters.erase("__iascylla_pull_bf"); return; }
        auto conf = confirmOptional(ctx, "Iascylla: move enemy unit here?",
                                      still_legal);
        if (conf == -1) return;  // suspended for agent input — keep the stash
        if (conf < 1) { self.card_counters.erase("__iascylla_pull_bf"); return; }
        GameObjectId picked = pickTarget(ctx, "Iascylla: pick enemy", legal);
        if (picked == kInvalidId) return;  // suspended (legal non-empty) — keep stash
        self.card_counters.erase("__iascylla_pull_bf");  // committing now
        if (!ctx.state.objectExists(picked)) return;
        ctx.executor.moveToBattlefield(picked, bf);
        ctx.events.logTrace("IASCYLLA: pulled " +
                             ctx.state.getObject(picked).name +
                             " to BF" + std::to_string(bf));
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [643] Keeper of Masks — [Hidden][Temporary] When you play me, play two
// Reflection unit tokens here. Then do this: They become copies of me.
// ═══════════════════════════════════════════════════════════════════════════
class AF4_KeeperOfMasks : public UnitCard {
public:
    AF4_KeeperOfMasks() : UnitCard(643) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        // "here" = Keeper's current location (its battlefield, or base).
        LocationId loc = self.location.value_or(LocationId{BaseLocation{ctx.controller}});

        for (int i = 0; i < 2; ++i) {
            GameObjectId tok = ctx.executor.createToken(
                ctx.controller, CardType::Unit, "Reflection", /*might=*/0,
                /*tags=*/{}, /*kw=*/KeywordSet{}, loc, /*enter_ready=*/false);
            if (tok == kInvalidId || !ctx.state.objectExists(tok)) continue;
            // Become a copy of me (base traits — copyUnit inherits
            // card_def_id so death triggers still fire, per Mirror Image).
            ctx.executor.copyUnit(tok, ctx.source);
            ctx.events.logTrace("KEEPER OF MASKS: Reflection token copies me");
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [676] Nidalee, Cat Form — [Ambush] When I win a combat, draw 1.
// ([Ambush] is engine-handled from the keyword set.)
// ═══════════════════════════════════════════════════════════════════════════
class AF4_NidaleeCat : public UnitCard {
public:
    AF4_NidaleeCat() : UnitCard(676) {}
    TriggerType triggerType() const override { return TriggerType::WhenIWinCombat; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("NIDALEE, CAT FORM: win combat -> draw 1");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [737] Tactical Retreat — [Reaction] Choose a friendly unit. The next time it
// would die this turn, heal it, exhaust it, and recall it instead. (Send it to
// base. This isn't a move.)
//
// Engine limitation: a true "next time it would die" deferred replacement
// needs a persistent on-board hook. GameEngine::killUnit consults
// applyReplacement only on OTHER on-board objects (it skips the dying unit
// itself, and a resolved [Reaction] spell has already gone to trash with no
// location), so a transient spell cannot install a future-death replacement
// without an engine-side per-unit guard flag — which would require an engine
// edit (disallowed). We therefore keep the established approximation and
// complete it: heal, exhaust, AND recall to base immediately at resolve. The
// prior partial only did heal + exhaust; the recall clause is now included.
// ═══════════════════════════════════════════════════════════════════════════
class AF4_TacticalRetreat : public SpellCard {
public:
    AF4_TacticalRetreat() : SpellCard(737) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        GameObjectId tid = targets[0];
        ctx.executor.healObject(tid);
        ctx.executor.exhaustObject(tid);
        ctx.executor.moveToBase(tid);  // recall (this isn't a move)
        ctx.events.logTrace("TACTICAL RETREAT: heal + exhaust + recall "
                             "(immediate approximation of deferred death "
                             "replacement)");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// [766] Forbidding Waste — While a unit here is defending alone, it has -2 [M].
// (It's alone if there are no other friendly units here.)
//
// Modeled as a passive aura sourced from this battlefield: for each unit at
// THIS battlefield that is currently a Defender and is the only unit its
// controller has here, push a -2 M aura effect.
//
// NOTE / engine limitation: the engine's recalculateAuras only invokes
// applyPassiveAura on objects with a location, and battlefield-card objects
// carry no location, so this hook does not currently fire for battlefield
// cards. The implementation is the semantically-correct model and will take
// effect if/when the engine extends applyPassiveAura dispatch to battlefield
// cards. Implemented without any engine edit (per project rule: no engine
// tweaks for card behavior). See report.
// ═══════════════════════════════════════════════════════════════════════════
class AF4_ForbiddingWaste : public BattlefieldCard {
public:
    AF4_ForbiddingWaste() : BattlefieldCard(766) {}
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        // Locate the BattlefieldState whose card object is THIS card.
        for (const auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            if (state.getObject(bf.card_object_id).card_def_id != cardDefId()) continue;

            // For each unit at this battlefield that is defending: check if
            // it's alone (no other friendly units here). If so, -2 M.
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit()) continue;
                auto ubf = u.battlefieldId();
                if (!ubf || *ubf != bf.id) continue;
                if (u.combat_designation != CombatDesignation::Defender) continue;
                int friendly_here = 0;
                for (auto& [oid, o] : state.objects) {
                    if (!o.isUnit() || o.controller != u.controller) continue;
                    auto obf = o.battlefieldId();
                    if (obf && *obf == bf.id) friendly_here++;
                }
                if (friendly_here == 1) {  // alone
                    GameObject::AuraEffect ae;
                    ae.source = bf.card_object_id;
                    ae.might_bonus = -2;
                    u.aura_effects.push_back(ae);
                }
            }
        }
    }
};

// ─── Registration ────────────────────────────────────────────────────────────

void registerAuditFixes4(CardRegistry& registry) {
    registry.registerCard(46, std::make_unique<AF4_EnGarde>());
    registry.registerCard(220, std::make_unique<AF4_Facebreaker>());
    registry.registerCard(381, std::make_unique<AF4_OrnnBlacksmith>());
    registry.registerCard(444, std::make_unique<AF4_CorruptEnforcer>());
    registry.registerCard(498, std::make_unique<AF4_BladeOfRuinedKing>());
    registry.registerCard(543, std::make_unique<AF4_SettBrawler>());
    registry.registerCard(612, std::make_unique<AF4_Iascylla>());
    registry.registerCard(643, std::make_unique<AF4_KeeperOfMasks>());
    registry.registerCard(676, std::make_unique<AF4_NidaleeCat>());
    registry.registerCard(737, std::make_unique<AF4_TacticalRetreat>());
    registry.registerCard(766, std::make_unique<AF4_ForbiddingWaste>());
}

} // namespace riftbound
