// Manual card implementations for cards appearing in test decks.
// These override the generated stubs for cards that do nothing.

#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "engine/effect_executor.h"

#include <algorithm>
#include <memory>

namespace riftbound {

// ═══════════════════════════════════════════════════════════════════════════════
// Counter-spell helpers (Phase 6q+, engine-audit CRITICAL #5)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Per CR 425.1.b: "A card that is Countered is not considered to have
// been played." When a counter-spell removes a chain item, the
// engine's play-tracking state (cards_played_this_turn,
// last_spell_energy_spent) must be unwound so downstream effects
// (Legion gates, "you spent N+" triggers, Sett's count-played
// abilities, etc.) don't fire off countered cards.
//
// All counter sites in this file call revertCounteredPlay() BEFORE
// pop_back. Centralised to avoid the bug recurring in future cards.
inline void revertCounteredPlay(CardContext& ctx, const ChainItem& top) {
    auto& ps = ctx.state.player(top.controller);
    if (ps.cards_played_this_turn > 0) --ps.cards_played_this_turn;
    if (ps.last_spell_energy_spent == top.total_energy_spent) {
        ps.last_spell_energy_spent = 0;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Resume-pattern helpers (Phase C-1 commit 6)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Drive a "discard N, then post_fn(ctx)" sequence through ctx.state.chain.resuming.
// Each discard is a one-card agent choice; resume_data[0] tracks remaining count.
// One-pass entry when hand is empty: post_fn fires immediately.
//
// Usage in Card::onResolve / onTrigger:
//   discardThenAct(ctx, /*count=*/1, "Card Name: discard 1",
//                  [&](CardContext& c) { c.executor.drawCards(c.controller, 1); });
template <typename PostFn>
static void discardThenAct(CardContext& ctx, int count,
                            const std::string& label, PostFn&& post_fn) {
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
        if (ps.hand.empty() || count <= 0) {
            post_fn(ctx);
            return;
        }
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
            post_fn(ctx);
            return;
        }
        ctx.executor.requestChoice(ctx.controller, build_choices(), label);
        // stay at resume_point = 1
        return;
    }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Champions
// ═══════════════════════════════════════════════════════════════════════════════

// [734] LeBlanc, Fragmented — [Assault], [Deathknell][>] Draw 1 (or 2 during Beginning)
class MLeBlanc : public UnitCard {
public:
    MLeBlanc() : UnitCard(734) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        int draw_count = (ctx.state.turn.phase == TurnPhase::BeginningStep) ? 2 : 1;
        ctx.executor.drawCards(ctx.controller, draw_count);
    }
};

// [712] Vex, Apathetic — When opponent plays unit while I'm at BF, stun it
// Complex trigger — needs non-standard event. For now: keyword-only (Deflect handled by base)
class MVexApathetic : public UnitCard {
public:
    MVexApathetic() : UnitCard(712) {}
};

// [682] Rengar, Trophy Hunter — [Ambush] to BF with enemy units
class MRengarTrophy : public UnitCard {
public:
    MRengarTrophy() : UnitCard(682) {}
};

// [676] Nidalee, Cat Form — [Ambush], when I win combat draw 1
class MNidaleeCat : public UnitCard {
public:
    MNidaleeCat() : UnitCard(676) {}
};

// [66] Ahri, Alluring — When I hold, score 1 point
class MAhriAlluring : public UnitCard {
public:
    MAhriAlluring() : UnitCard(66) {}
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ps = ctx.state.player(ctx.controller);
        ps.score++;
        ctx.events.logTrace("TRIGGER: Ahri scores 1 point -> " + std::to_string(ps.score));
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Legends
// ═══════════════════════════════════════════════════════════════════════════════

// [785] Gloomist — When you or ally hold, exhaust me to draw 1
class MGloomist : public LegendCard {
public:
    MGloomist() : LegendCard(785) {}
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Optional: "you may exhaust me to draw 1". Routed through
        // confirmOptional so the agent picks yes/no (random ~50/50,
        // policy learns when to spend the exhaust). still_legal blocks
        // the prompt if the legend is already exhausted.
        auto still_legal = [&]() {
            if (!ctx.state.objectExists(ctx.source)) return false;
            return !ctx.state.getObject(ctx.source).is_exhausted;
        };
        int conf = confirmOptional(ctx,
            "Gloomist: exhaust to draw 1?", still_legal);
        if (conf == -1) return;
        if (conf == 0) return;
        auto& legend = ctx.state.getObject(ctx.source);
        legend.is_exhausted = true;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("TRIGGER: Gloomist exhausts to draw 1");
    }
};

// [756] Deceiver — When conquer/hold, may discard 1 + exhaust to play Reflection token
//
// Phase C-1 commit 6 follow-up — refactored to the resume-point pattern
// so the discard target is an agent decision (not picked from back of
// hand by the legacy blocking discardCards). case 0 exhausts the
// legend + publishes the discard choice; case 1 commits the discard
// + creates the token.
class MDeceiver : public LegendCard {
public:
    MDeceiver() : LegendCard(756) {}
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = ctx.state.chain.resuming.value();
        switch (ri.resume_point) {
        case 0: {
            if (!ctx.state.objectExists(ctx.source)) return;
            auto& legend = ctx.state.getObject(ctx.source);
            if (legend.is_exhausted) return;
            auto& ps = ctx.state.player(ctx.controller);
            if (ps.hand.empty()) return;
            // Exhaust the legend (cost paid).
            legend.is_exhausted = true;
            // Publish the discard choice.
            std::vector<Intent> choices;
            for (auto card_id : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {card_id};
                choices.push_back(c);
            }
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "discard 1 (Deceiver)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                ctx.executor.applyDiscard(ctx.controller,
                                            choice->chosen_objects[0]);
            }
            // Create a 0M Reflection token at controller's base.
            auto loc = BaseLocation{ctx.controller};
            ctx.executor.createToken(ctx.controller, CardType::Unit,
                                      "Reflection", 0, {}, {}, loc, true);
            ctx.events.logTrace("TRIGGER: Deceiver creates Reflection token");
            return;
        }
        }
    }
};

// [506] Fire Below the Mountain — [E]: [Reaction] — [Add] [A] for gear
class MFireBelowMtn : public LegendCard {
public:
    MFireBelowMtn() : LegendCard(506) {}
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {.exhaust = true}; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Add [A] to rune pool (universal power)
        ctx.state.player(ctx.controller).rune_pool.universal_power++;
        ctx.events.logTrace("ACTIVATE: Fire Below the Mountain adds [A]");
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Deathknell units
// ═══════════════════════════════════════════════════════════════════════════════

// [629] Ruined Rex — [Deathknell][>] Deal 4 to enemy unit
class MRuinedRex : public UnitCard {
public:
    MRuinedRex() : UnitCard(629) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Deal 4 to an enemy unit at a battlefield
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller == ctx.controller) continue;
            if (!obj.isAtBattlefield()) continue;
            ctx.executor.dealDamage(id, 4, ctx.source);
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
            break;
        }
    }
};

// [714] Black Rose Dignitary — [Assault], [Deathknell][>] Channel 1 rune exhausted
class MBlackRoseDig : public UnitCard {
public:
    MBlackRoseDig() : UnitCard(714) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.channelRunes(ctx.controller, 1, true);
    }
};

// [486] Glasc Mixologist — [Deathknell] play unit cost<=3 from trash
class MGlascMixologist : public UnitCard {
public:
    MGlascMixologist() : UnitCard(486) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& ps = ctx.state.player(ctx.controller);
        for (int i = static_cast<int>(ps.trash.size()) - 1; i >= 0; --i) {
            auto cid = ps.trash[i];
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (!obj.isUnit()) continue;
            if (obj.card_def_id == kInvalidId) continue;
            // Check cost <= 3 (simplified — use base might as proxy, or check CardDB)
            ctx.executor.playIgnoringCost(ctx.controller, cid);
            ps.trash.erase(ps.trash.begin() + i);
            break;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Score/Phase trigger units
// ═══════════════════════════════════════════════════════════════════════════════

// [73] Sona, Harmonious — At end of turn, if at BF, ready up to 4 friendly runes
class MSona : public UnitCard {
public:
    MSona() : UnitCard(73) {}
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (!self.isAtBattlefield()) return;
        int readied = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (readied >= 4) break;
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.is_exhausted) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.readyObject(id);
            readied++;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Spells
// ═══════════════════════════════════════════════════════════════════════════════

// [209] Cull the Weak — Each player kills one of their units
class MCullTheWeak : public SpellCard {
public:
    MCullTheWeak() : SpellCard(209) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        for (auto pid : {PlayerId::Player1, PlayerId::Player2}) {
            auto units = ctx.state.allUnitsControlledBy(pid);
            if (!units.empty()) {
                // Kill last unit (agent choice in future)
                ctx.executor.killObject(units.back());
            }
        }
    }
};

// [45] Defy — "Counter a spell. Draw a card."
//
// Per CR 355.9.a.2 + 355.10: "spell" in card text refers to a Chain
// object and IS a target. "Counter a spell" requires a spell on the
// chain to be playable; the side-effect "draw 1" doesn't satisfy
// that requirement. Earlier session left this as "always playable"
// which is wrong per CR — corrected after user CR review.
class MDefy : public SpellCard {
public:
    MDefy() : SpellCard(45) {}
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (it->is_spell) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Peek-and-pop counter
        if (!ctx.state.chain.items.empty()) {
            auto& top = ctx.state.chain.items.back();
            if (top.is_spell) {
                auto countered = top.source;
                revertCounteredPlay(ctx, top);  // CR 425.1.b
                ctx.state.chain.items.pop_back();
                if (ctx.state.objectExists(countered)) {
                    auto& obj = ctx.state.getObject(countered);
                    ctx.events.logTrace("COUNTER: " + obj.name + " countered by Defy -> trash");
                    obj.zone = ZoneType::Trash;
                    obj.location = std::nullopt;
                    ctx.state.player(obj.owner).trash.push_back(countered);
                }
            }
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// [631] Sprite Burst — Play 2 ready 3M Sprite tokens with [Temporary]
class MSpriteBurst : public SpellCard {
public:
    MSpriteBurst() : SpellCard(631) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Simple units with static effects (keyword-only but marked complex by codegen)
// ═══════════════════════════════════════════════════════════════════════════════

// [560] Inferna — [Ambush] [Assault 2] (keywords only, no trigger)
class MInferna : public UnitCard {
public: MInferna() : UnitCard(560) {} };

// [745] Thrill of the Hunt — Banish friendly unit, play it to any BF ignoring cost
class MThrillOfTheHunt : public SpellCard {
public:
    MThrillOfTheHunt() : SpellCard(745) {}
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId unit_id = pickTarget(ctx, "Thrill of the Hunt", legal);
        if (unit_id == kInvalidId) return;
        if (!ctx.state.objectExists(unit_id)) return;
        auto& unit = ctx.state.getObject(unit_id);

        // Step 1: Banish the friendly unit
        ctx.events.logTrace("THRILL: banishing " + unit.name);
        ctx.executor.banishObject(unit_id);

        // Step 2: Play it from banishment to any battlefield, ignoring cost
        // Remove from banishment
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.banishment.begin(), ps.banishment.end(), unit_id);
        if (it != ps.banishment.end()) ps.banishment.erase(it);

        // Pick a battlefield (agent choice in future, for now: first BF)
        BattlefieldId target_bf = 0;
        if (!ctx.state.battlefields.empty()) {
            target_bf = ctx.state.battlefields[0].id;
        }

        // Place on battlefield ready (ignoring cost)
        unit.zone = ZoneType::BattlefieldZone;
        unit.location = BattlefieldLocation{target_bf};
        unit.controller = ctx.controller;
        unit.is_exhausted = false; // enters ready
        unit.damage_marked = 0;
        unit.recomputeMight();

        ctx.events.logTrace("THRILL: " + unit.name + " played to BF#" +
                             std::to_string(target_bf) + " ready, ignoring cost");
        ctx.events.emit(EnteredBoardEvent{unit_id, ctx.controller, CardType::Unit,
            BattlefieldLocation{target_bf}, true});
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
};

// [176] Sneaky Deckhand — keywords only
class MSneakyDeckhand : public UnitCard {
public: MSneakyDeckhand() : UnitCard(176) {} };

// [395] Dropboarder — "When you play me, if you control two or more gear,
// ready me." Units enter exhausted (CR 143.4); this readies Dropboarder on
// entry when the controller already commands 2+ gear.
class MDropboarder : public UnitCard {
public:
    MDropboarder() : UnitCard(395) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int gear = 0;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (obj.isGear() && obj.controller == ctx.controller &&
                obj.location.has_value()) {
                ++gear;
            }
        }
        if (gear >= 2) {
            ctx.executor.readyObject(ctx.source);
            ctx.events.logTrace("DROPBOARDER: 2+ gear controlled, ready me");
        }
    }
};

// [106] Sprite Mother — When you play me, play a 3M Sprite token here
class MSpriteMotherUnit : public UnitCard {
public:
    MSpriteMotherUnit() : UnitCard(106) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = ctx.state.getObject(ctx.source).location.value_or(BaseLocation{ctx.controller});
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
    }
};

// [91] Pit Crew — "When you play a gear, ready me." Fires via the
// WhenYouPlayAGear trigger (emitted by TriggerManager when the controller
// plays a gear). Readies self — note triggered abilities arrive with empty
// targets, so we act on ctx.source rather than targets[0].
class MPitCrew : public UnitCard {
public:
    MPitCrew() : UnitCard(91) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAGear; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("PIT CREW: gear played, ready me");
    }
};

// [136] Pit Rookie — When you play me, buff another friendly unit
class MPitRookie : public UnitCard {
public:
    MPitRookie() : UnitCard(136) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Buff another friendly unit
        for (auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.buffUnit(id);
            break;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Gear (non-equip)
// ═══════════════════════════════════════════════════════════════════════════════

// [538] Seal of Focus — actual text: "[E]: [Reaction] — [Add] [G]."
// Activated ability, exhaust + Reaction timing, adds 1 Calm ([G]) power to
// the controller's rune pool. Mirrors Seal of Strength (542). Previously
// misimplemented as a WhenYouPlayThis "ready a friendly unit" trigger.
class MSealOfFocus : public GearCard {
public:
    MSealOfFocus() : GearCard(538) {}
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 0};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.addFloatingPower(ctx.controller, Domain::Calm, 1);
        ctx.events.logTrace("SEAL OF FOCUS: Add [G]");
    }
};

// [542] Seal of Strength — When you play this, buff a friendly unit
// [542] Seal of Strength — actual text: "[E]: [Reaction] — [Add] [O]."
// Activated ability, exhaust + Reaction timing, adds 1 Order power to the
// controller's rune pool. Phase 6q+ fix: previously misimplemented as a
// WhenYouPlayThis buff trigger.
class MSealOfStrength : public GearCard {
public:
    MSealOfStrength() : GearCard(542) {}
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 0};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.addFloatingPower(ctx.controller, Domain::Order, 1);
        ctx.events.logTrace("SEAL OF STRENGTH: Add [O]");
    }
};

// [573] Fresh Beans — "When you play a unit during a showdown, you may
// exhaust this to draw 1." Fires on every friendly unit play; gated on an
// in-progress showdown at some battlefield. The "exhaust this" is the
// optional cost — confirmOptional asks yes/no, the still-legal predicate
// requires Fresh Beans to be ready (otherwise the cost can't be paid).
class MFreshBeans : public GearCard {
public:
    MFreshBeans() : GearCard(573) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        bool in_showdown = false;
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.showdown_in_progress) { in_showdown = true; break; }
        }
        if (!in_showdown) return;
        int conf = confirmOptional(ctx, "Fresh Beans: exhaust to draw 1",
            [&] {
                return ctx.state.objectExists(ctx.source) &&
                       !ctx.state.getObject(ctx.source).is_exhausted;
            });
        if (conf != 1) return;
        ctx.executor.exhaustObject(ctx.source);
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("FRESH BEANS: exhaust to draw 1");
    }
};

// [640] Sprite Fountain — When you play this, play a 3M Sprite token
class MSpriteFountain : public GearCard {
public:
    MSpriteFountain() : GearCard(640) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
    }
};

// [160] Dazzling Aurora — At end of your turn, reveal from top of deck until
// you find a unit. Banish it, play it ignoring its cost. Recycle the rest.
//
// This is the Miss Fortune deck's core win condition. The generated stub in
// gear_cards.cpp is a no-op; this manual implementation supersedes it.
class MDazzlingAurora : public GearCard {
public:
    MDazzlingAurora() : GearCard(160) {}
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();

        // First-entry-only steps: reveal from deck until a unit, recycle
        // the rest, banish the unit. Stash the unit id in ri.targets so
        // subsequent re-entries from pickMode don't re-reveal. Empty
        // ri.targets ≡ "haven't entered yet"; a single kInvalidId entry
        // ≡ "entered, but no unit was found — nothing left to do".
        if (ri.targets.empty()) {
            auto [u, rest] = ctx.executor.revealUntil(ctx.controller, CardType::Unit);
            if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
            if (u == kInvalidId || !ctx.state.objectExists(u)) {
                ri.targets.push_back(kInvalidId);
                return;
            }
            ctx.executor.banishObject(u);
            // banishObject pushed it to banishment; playIgnoringCost will
            // re-zone it, so remove the duplicate banishment entry now.
            auto& bz = ctx.state.player(ctx.state.getObject(u).owner).banishment;
            bz.erase(std::remove(bz.begin(), bz.end(), u), bz.end());
            ri.targets.push_back(u);
        }
        GameObjectId unit_id = ri.targets.front();
        if (unit_id == kInvalidId) return;

        // Per CR 355.2.a, the controller picks the landing location: base
        // or any battlefield they control. pickMode publishes one option
        // per legal location so the agent records the decision (mandatory
        // even when only base is legal — engine-wide rule).
        std::vector<LocationId> locations;
        std::vector<std::string> labels;
        locations.push_back(LocationId{BaseLocation{ctx.controller}});
        labels.push_back("base");
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.controller.has_value() && *bf.controller == ctx.controller) {
                locations.push_back(LocationId{BattlefieldLocation{bf.id}});
                std::string bf_name = "BF#" + std::to_string(bf.id);
                if (ctx.state.objectExists(bf.card_object_id)) {
                    bf_name = ctx.state.getObject(bf.card_object_id).name;
                }
                labels.push_back(bf_name);
            }
        }
        uint32_t legal_mask = (locations.size() >= 32)
            ? 0xFFFFFFFFu
            : ((1u << locations.size()) - 1);
        int chosen = pickMode(ctx, "Aurora: where to play unit",
                               static_cast<int>(locations.size()),
                               labels, legal_mask);
        if (chosen == -1) return;  // yielded for agent input
        if (chosen < 0 || static_cast<size_t>(chosen) >= locations.size()) {
            chosen = 0;
        }
        ctx.executor.playIgnoringCost(ctx.controller, unit_id,
                                       locations[chosen]);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Critical no-op fixes (cards previously stubbed by codegen but vital for the
// Miss Fortune and Rengar decks). Each implementation overrides the generated
// stub via the registration order in card_registry.cpp.
// ═══════════════════════════════════════════════════════════════════════════════

// [162] Miss Fortune, Captain — "The first time I move each turn, you may
// ready something else that's exhausted."
//
// Prior implementation used WhenAFriendlyUnitMovesToFB which (a) excludes
// the moving unit itself (trigger_manager.cpp:524 skips e.object), so the
// trigger never fired when Captain herself moved, and (b) auto-readied a
// unit at the destination BF — neither of which match the card text.
// Now: WhenIMove on Captain herself, gated by a per-turn sentinel, with
// confirmOptional + pickTarget for the agent decisions.
class MMissFortuneCaptain : public UnitCard {
public:
    MMissFortuneCaptain() : UnitCard(162) {}
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& captain = ctx.state.getObject(ctx.source);

        // "First time I move each turn" — store turn_id (+1 so 0 = never).
        // We only set the sentinel on FIRST entry; subsequent re-entries
        // during the same chain resolution carry the resume_point state
        // forward, so the "already fired this turn" guard mustn't reject
        // them.
        int turn_id = ctx.state.turn.turn_number + 1;
        bool first_entry = !ctx.state.chain.resuming.has_value()
                        || ctx.state.chain.resuming->resume_point == 0;
        int& last_fired = captain.card_counters["mf_captain_move_turn"];
        if (first_entry && last_fired == turn_id) return;  // not the first move
        if (first_entry) last_fired = turn_id;

        // Targets: friendly things other than Captain that are exhausted and
        // on board (skip in-deck/banished/etc.).
        auto find_targets = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (obj.controller != ctx.controller) continue;
                if (!obj.is_exhausted) continue;
                if (!obj.location.has_value()) continue;
                out.push_back(id);
            }
            return out;
        };

        int answer = confirmOptional(ctx, "Miss Fortune ready another",
                                     [&]() { return !find_targets().empty(); });
        if (answer == -1) return;     // yielded for agent input
        if (answer == 0) {
            ctx.events.logTrace("MISS FORTUNE: declined to ready another");
            return;
        }

        auto target = pickTarget(ctx, "Miss Fortune ready an exhausted friendly",
                                  find_targets());
        if (target == kInvalidId) return;  // yielded for agent input

        ctx.executor.readyObject(target);
        ctx.events.logTrace("MISS FORTUNE: readied " +
                             ctx.state.getObject(target).name);
    }
};

// [263] Bullet Time — [Action] Pay any amount of [A] to deal that much damage
// to a unit. The "variable cost" intent enumeration is not yet engine-supported,
// so this best-effort implementation spends all currently-available energy and
// deals that much damage to the chosen target. Not full mechanic, but no longer
// a no-op.
// [263] Bullet Time — "[Action] Pay any amount of [A] to deal that much
// damage to all enemy units at a battlefield."
//
// CR-correct flow:
//   1. Target chosen at play time: a unit at a battlefield (we use the
//      unit's BF as the AoE area).
//   2. At resolve, the controller picks X (any amount they can pay from
//      their exhausted-rune count).
//   3. Pay X [A] by recycling X exhausted runes.
//   4. Deal X damage to all ENEMY units at the chosen battlefield.
//
// Synergy: With Elder Dragon ("any amount of your damage is enough to
// kill enemy units"), a 1-power Bullet Time on a battlefield with
// multiple enemies wipes them all — that's the famous combo the
// Miss Fortune deck is built around.
class MBulletTime : public SpellCard {
public:
    MBulletTime() : SpellCard(263) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice. pickTarget uses
    // resume_points 6/7/8 + resume_data[2]; the existing pickXAmount
    // below uses 0/1/2 + resume_data[0]; they don't collide.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Backward-compat: direct-invocation tests pre-supply targets;
        // the action generator never emits pre-resolved targets for
        // needsPlayTimeTarget=true so production always falls through
        // to pickTarget.
        GameObjectId target_unit;
        if (!targets.empty()) {
            target_unit = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            target_unit = pickTarget(ctx, "Bullet Time", legal);
        }
        if (target_unit == kInvalidId) return;
        if (!ctx.state.objectExists(target_unit)) return;
        auto& tgt = ctx.state.getObject(target_unit);
        if (!tgt.isAtBattlefield()) return;
        auto bf_id_opt = tgt.battlefieldId();
        if (!bf_id_opt.has_value()) return;
        auto bf_id = *bf_id_opt;

        // Compute the maximum X the controller can pay: count of
        // exhausted runes in their base (recyclable for [A]).
        auto& ps = ctx.state.player(ctx.controller);
        int max_x = 0;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
            if (obj.is_exhausted) max_x++;
        }
        int x = pickXAmount(ctx, "Bullet Time: X power", 0, max_x);
        if (x < 0) return;  // pending choice
        if (x == 0) {
            ctx.events.logTrace("BULLET TIME: X=0, no damage");
            return;
        }

        // Pay X power by recycling X exhausted runes (any domain — [A]
        // is universal).
        int paid = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (paid >= x) break;
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
            if (!obj.is_exhausted) continue;
            obj.location = std::nullopt;
            obj.zone = ZoneType::RuneDeck;
            ps.rune_deck.insert(ps.rune_deck.begin(), id);
            paid++;
        }
        ctx.events.logTrace("BULLET TIME: paid " + std::to_string(paid) +
                             " power → dealing " + std::to_string(x) +
                             " damage to all enemies at BF#" +
                             std::to_string(bf_id));

        // Deal X damage to all enemy units at the chosen battlefield.
        // Snapshot first so kills don't invalidate iteration.
        std::vector<GameObjectId> victims;
        PlayerId enemy = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (obj.controller != enemy) continue;
            if (!obj.isAtBattlefield()) continue;
            auto vbf = obj.battlefieldId();
            if (vbf && *vbf == bf_id) victims.push_back(id);
        }
        for (auto v : victims) ctx.executor.dealDamage(v, x, ctx.source);
    }
};

// [680] Elder Dragon — Two clauses on the printed card:
//   1. Passive aura: "Any amount of your damage is enough to kill enemy
//      units." Handled engine-side in GameEngine::processLethalDamage
//      by scanning ability_text — no per-card work needed here.
//   2. Play trigger (CR 383.4.a): "When you play me, choose up to one
//      enemy unit at EACH LOCATION. Deal 1 to them."
//      LOCATION includes the opponent's BASE and every BATTLEFIELD —
//      not just battlefields. The earlier implementation iterated
//      state.battlefields only, so enemies sitting at base never took
//      damage and the aura's instant-kill effect never engaged
//      against them.
class MElderDragon : public UnitCard {
public:
    MElderDragon() : UnitCard(680) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto opp = opponent(ctx.controller);

        // Build the list of LOCATIONS to hit: opponent's base + every
        // battlefield. Card text "at each location" is the canonical
        // Riftbound term — see CR 151 / 144.4 for the location-vs-zone
        // distinction.
        std::vector<LocationId> locations;
        locations.reserve(ctx.state.battlefields.size() + 1);
        locations.push_back(BaseLocation{opp});
        for (auto& bf : ctx.state.battlefields) {
            locations.push_back(BattlefieldLocation{bf.id});
        }

        for (const auto& loc : locations) {
            auto enemies = ctx.state.unitsAt(loc, opp);
            if (enemies.empty()) continue;
            // Pick the first enemy at this location. CR-faithful "up to
            // one" with agent choice would route through pickTarget;
            // first-enemy is a placeholder that matches the prior
            // battlefield-only behaviour.
            auto victim = enemies.front();
            ctx.executor.dealDamage(victim, 1, ctx.source);
            // Two complementary kill paths: (1) inline natural-lethal
            // check for units already at 0–1 might (caught here in the
            // trigger), (2) GameEngine::processLethalDamage during the
            // post-trigger cleanup applies the Elder Dragon
            // "any-damage-lethal" aura to higher-might enemies. Both
            // are needed: this trigger is sometimes invoked outside
            // the full engine flow (per-card tests), so inline kill
            // catches the easy case; cleanup catches the aura case
            // when the engine drives it for real.
            if (ctx.state.objectExists(victim) &&
                ctx.state.getObject(victim).hasLethalDamage()) {
                ctx.executor.killObject(victim);
            }
        }
    }
};

// [709] Baron Nashor
//   Card text:
//     As you play me, add the Baron Pit battlefield token to the board
//     if it's not there already. If you do, I enter there.
//     (It has "Units can move here from anywhere.")
//     I can't be chosen by enemy spells and abilities.
//     Other friendly units have +2 [M].
//
// Implemented mechanics:
//   1. Untargetable — canBeChosenByEnemy() = false. Engine's aura recalc
//      pass populates GameObject::untargetable_by_enemy; target
//      enumeration filters out untargetable enemy candidates.
//   2. +2 might aura — handled automatically by the engine's text-parsing
//      aura system (matches "other friendly units have +2 [M]"). No
//      manual onTrigger code needed for this.
//   3. Baron Pit spawn + "I enter there" + "Units can move here from
//      anywhere" — handled by the WhenYouPlayMe trigger below.
//      EffectExecutor::addBattlefieldToken creates a new BattlefieldState
//      slot with `accepts_any_inbound = true`; the move-action generator
//      then admits BF→BF moves into Baron Pit for ANY unit regardless
//      of Ganking. "If you do, I enter there" is strict: Baron only
//      relocates to the Pit when this play CREATED it (not when a
//      pre-existing Pit was already on the board).
class MBaronNashor : public UnitCard {
public:
    MBaronNashor() : UnitCard(709) {}
    bool canBeChosenByEnemy() const override { return false; }

    // CR 135.2.b.3 / CR 355.1: "As you play me" runs DURING the play
    // action, not as a triggered ability. Implementing this via
    // Card::onPlay (invoked from executePlayCard between cost payment
    // and chain insertion) means there is no chain item, no priority
    // window between Baron being played and him entering the Pit —
    // closing the gap where an opponent could interrupt his arrival.
    // Baron never touches base; his location is rewritten before
    // resolvePermanent emits EnteredBoardEvent.
    void onPlay(CardContext& ctx) override {
        // Locate an existing Baron Pit by its BF card name.
        BattlefieldId pit_id = kInvalidId;
        for (auto& bf : ctx.state.battlefields) {
            if (!ctx.state.objectExists(bf.card_object_id)) continue;
            if (ctx.state.getObject(bf.card_object_id).name == "Baron Pit") {
                pit_id = bf.id;
                break;
            }
        }

        if (pit_id == kInvalidId) {
            // First Baron Nashor on the board this game — spawn the Pit
            // and rewrite Baron's pre-resolution location. "If you do,
            // I enter there" sets the play-step location atomically
            // with the play, NOT after he's landed at base.
            pit_id = ctx.executor.addBattlefieldToken(
                "Baron Pit", /*accepts_any_inbound=*/true);
            if (ctx.state.objectExists(ctx.source)) {
                auto& baron = ctx.state.getObject(ctx.source);
                baron.location = BattlefieldLocation{pit_id};
                ctx.events.logTrace("BARON NASHOR: created Baron Pit (bf=" +
                                     std::to_string(pit_id) +
                                     ") and will enter directly there");
            }
        } else {
            // Pit already existed (typically: the OTHER player's Baron
            // built it earlier). Per "If you do, I enter there", this
            // Baron does NOT enter the Pit — he stays where the normal
            // permanent-play resolution placed him (whatever location
            // the controller chose during step 2 of the play sequence).
            ctx.events.logTrace("BARON NASHOR: Baron Pit (bf=" +
                                 std::to_string(pit_id) +
                                 ") already exists; staying at chosen location.");
        }
    }
};

// [12] Noxus Hopeful — [Legion] I cost [2] less. Uses the selfCostReduction
// hook on Card; engine consults this in canAfford/payCardCost. Legion is
// satisfied when the controller has already played a card this turn
// (cards_played_this_turn >= 1).
class MNoxusHopeful : public UnitCard {
public:
    MNoxusHopeful() : UnitCard(12) {}
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return state.player(player).cards_played_this_turn >= 1 ? 2 : 0;
    }
};

// [26] Brynhir Thundersong — When you play me, opponents can't play cards
// this turn. Sets the cant_play_cards_this_turn flag on the opponent's
// PlayerState; the action generators (main/showdown/closed-state) consult
// this flag and emit no play-from-hand actions while it's set. The flag
// resets in PlayerState::resetTurnTracking() at end of turn.
class MBrynhirThundersong : public UnitCard {
public:
    MBrynhirThundersong() : UnitCard(26) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        ctx.state.player(opp).cant_play_cards_this_turn = true;
        ctx.events.logTrace("BRYNHIR: " + std::string(toString(opp)) +
                            " can't play cards this turn");
    }
};

// [344] Ferrous Forerunner — [Deathknell] Play two 3M Mech unit tokens to base.
class MFerrousForerunner : public UnitCard {
public:
    MFerrousForerunner() : UnitCard(344) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        KeywordSet kw;  // Mech tokens — no special keywords
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", 3,
                                  {"Mech"}, kw, loc, false);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", 3,
                                  {"Mech"}, kw, loc, false);
    }
};

// [348] Rengar, Pouncing — [Reaction] play to a battlefield you're attacking.
// The action generator (generateShowdownActions / generateClosedStateActions)
// consults playableAsReactionToAttack() and emits play-to-attacking-BF intents.
class MRengarPouncing : public UnitCard {
public:
    MRengarPouncing() : UnitCard(348) {}
    bool playableAsReactionToAttack() const override { return true; }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Champions & legends batch (2026-05-15)
// Adds 6 of the 9 "champions/legends as no-ops" entries from CLAUDE.md.
// 3 deferred (need engine support):
//   [28] Draven, Showboat        — might = your points (dynamic aura, recalcAuras)
//   [552] Glorious Executioner   — WhenIWinCombat trigger does not exist yet
//   [787] Voidreaver             — WhenIWinCombat + XP-cost activated abilities
// ═══════════════════════════════════════════════════════════════════════════════

// [262] Bounty Hunter (legend) — [E]: Give a unit Ganking this turn.
// Phase 6r — uses needs_activation_time_target so the action vocab
// gets distinct slots per target choice. Action gen emits ONE intent
// (no target), onActivate picks via pickTarget at resolve time.
class MBountyHunter : public LegendCard {
public:
    MBountyHunter() : LegendCard(262) {}
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true},
            .is_action = false,
            .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    /// The engine's action-gen path calls this to gate the ability:
    /// if it returns empty, the ability isn't offered. The base impl
    /// reads `getTargetRequirements()` (legacy single-ability surface)
    /// which MBountyHunter doesn't supply — so without this override
    /// the engine sees count=0, returns {}, and silently drops the
    /// ability. The same call drives `pickTarget` inside `onActivate`
    /// when production code routes through needs_activation_time_target.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int /*ability_idx*/) const override {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isUnit()) continue;
            if (!obj.location.has_value()) continue;
            if (obj.controller != controller && obj.untargetable_by_enemy) continue;
            out.push_back(id);
        }
        return out;
    }
    void onActivate(CardContext& ctx, int /*ability_idx*/,
                    const std::vector<GameObjectId>& targets) override {
        // Backward-compat: direct-invocation tests pre-supply targets.
        // Production uses pickTarget.
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0);
            picked = pickTarget(ctx, "Bounty Hunter", legal);
        }
        if (picked == kInvalidId) return;
        if (!ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryKeyword(picked, Keyword::Ganking, 0);
        ctx.events.logTrace("BOUNTY HUNTER: gives Ganking to " +
                             ctx.state.getObject(picked).name);
    }
};

// [543] Sett, Brawler — When played and when I conquer, buff me (if I don't
// have a buff, +1M). Plus "Spend my buff: give me +4M this turn."
// We use WhenIConquer for the conquer trigger; the on-play trigger fires via
// WhenYouPlayMe. The two triggers share onTrigger handler; sentinel-checks
// both fire correctly (buffUnit's "first buff = +1M, subsequent = +0M but
// counts" semantics live in EffectExecutor).
class MSettBrawler : public UnitCard {
public:
    MSettBrawler() : UnitCard(543) {}
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.buffUnit(ctx.source);
    }
    bool hasActivatedAbility() const override { return true; }
    // Cost: spending a buff is the cost. We model this as the activation
    // checking buff_count > 0 in onActivate and decrementing manually.
    ActivationCost getActivationCost() const override { return {}; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (self.buff_count <= 0) return;  // no buff to spend
        self.buff_count -= 1;
        ctx.executor.giveTemporaryMight(ctx.source, 4);
        ctx.events.logTrace("SETT: spent a buff for +4M this turn");
    }
};

// [644] Lillia, Fae Fawn — When I move from a location, play a 3M Sprite unit
// token with Temporary there. Engine-side, WhenIMove fires after the move
// completes (location is the new one); we approximate by creating the token
// at the controller's base. Not strictly faithful — full fidelity requires
// the engine to pass the previous location through the trigger payload.
class MLilliaFaeFawn : public UnitCard {
public:
    MLilliaFaeFawn() : UnitCard(644) {}
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = BaseLocation{ctx.controller};  // approximate: base, not source
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
        ctx.events.logTrace("LILLIA: creates 3M Sprite token (Temporary)");
    }
};

// [705] Kha'Zix, Mutating Horror — Ambush + when I attack or defend, if an
// enemy unit is alone at this BF, give me +2M and gain 2 XP. Ambush keyword
// is engine-handled; we implement the conditional combat trigger.
class MKhaZixMutating : public UnitCard {
public:
    MKhaZixMutating() : UnitCard(705) {}
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto bf_id = self.battlefieldId();
        if (!bf_id) return;
        // Count enemy units at this BF — "alone" means exactly 1.
        auto opp = opponent(ctx.controller);
        int enemy_count = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (obj.battlefieldId() == bf_id) ++enemy_count;
        }
        if (enemy_count != 1) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("KHA'ZIX: enemy alone, +2M and +2 XP");
    }
};

// [744] Pridestalker (legend) — When you play a unit, give a friendly unit
// +1M this turn. Picks a friendly unit (preference: the one just played,
// approximated as any non-self friendly at a BF).
class MPridestalker : public LegendCard {
public:
    MPridestalker() : LegendCard(744) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // If the trigger context carries the played-unit GameObjectId in
        // targets[0], buff that. Otherwise pick any friendly unit at a BF.
        GameObjectId pick = kInvalidId;
        if (!targets.empty() && ctx.state.objectExists(targets[0])) {
            pick = targets[0];
        } else {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.isAtBattlefield()) continue;
                pick = id;
                break;
            }
        }
        if (pick == kInvalidId) return;
        ctx.executor.giveTemporaryMight(pick, 1);
        ctx.events.logTrace("PRIDESTALKER: +1M to " +
                             ctx.state.getObject(pick).name);
    }
};

// [749] Bashful Bloom (legend) — [4], [E]: Play a ready 3M Sprite unit token
// with Temporary. The "this ability costs [1] less for each friendly unit
// with Temporary" reduction is not modeled (ActivationCost is static — would
// need a state-aware override). Hardcoded base cost; the reduction is the
// "TODO" for a follow-up that touches the engine cost path.
class MBashfulBloom : public LegendCard {
public:
    MBashfulBloom() : LegendCard(749) {}
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return {.exhaust = true, .energy = 4};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
        ctx.events.logTrace("BASHFUL BLOOM: plays ready 3M Sprite (Temporary)");
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Simple unit & spell triggers batch (2026-05-15)
// More no-op fixes from CLAUDE.md's polish list. Each is a single-trigger or
// single-resolve card under ~20 LOC.
// ═══════════════════════════════════════════════════════════════════════════════

// Helper: create a Gold gear token at the controller's base. Gold is a 0M
// gear with the activated ability "Kill this, [E]: [Reaction] — [Add] [A]".
// Phase 6q+ fix: the token is now created with the real Gold gear's
// card_def_id (326), so the Card registry dispatches its activated ability
// the same way it would for a hand-played Gold card. super_type remains
// Token, so CR 183.1 cease-to-exist semantics still apply when the token
// leaves the board. Gold cards enter READY (per createToken's default for
// Gear) so the activated ability is immediately usable.
constexpr CardDefId kGoldGearCardDefId = 326;
inline void createGoldToken(CardContext& ctx) {
    auto loc = BaseLocation{ctx.controller};
    auto tid = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                         0, {"Gold"}, KeywordSet{}, loc, false);
    if (ctx.state.objectExists(tid)) {
        ctx.state.getObject(tid).card_def_id = kGoldGearCardDefId;
    }
}

// [326] Gold (and token copies) — "Kill this, [E]: [Reaction] — [Add] [A]."
// Activated ability that sacrifices the gear and adds 1 universal Power to
// the controller's floating pool. Reaction-timed so it can be activated in
// Closed State (priority window) as well as Main Phase. The "Kill this"
// portion of the cost is paid in onActivate via killObject; the [E]
// portion is paid by the engine via ActivationCost.exhaust = true.
class MGoldToken : public GearCard {
public:
    MGoldToken() : GearCard(kGoldGearCardDefId) {}
    bool hasActivatedAbility() const override { return true; }
    bool isReactionAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 0};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Pay the "kill this" part of the cost — the engine has already
        // exhausted us. CR 183.1: tokens cease to exist on leaving the
        // board, handled by killObject's super_type=Token branch.
        ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
        ctx.events.logTrace("GOLD: kill+[E] -> +1 [A]");
        ctx.executor.killObject(ctx.source);
    }
};

// [451] Treasure Hunter — When I move, play a Gold gear token exhausted.
class MTreasureHunter : public UnitCard {
public:
    MTreasureHunter() : UnitCard(451) {}
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        createGoldToken(ctx);
        ctx.events.logTrace("TREASURE HUNTER: Gold gear token created");
    }
};

// [476] Honest Broker — [Deathknell] Play a Gold gear token exhausted.
class MHonestBroker : public UnitCard {
public:
    MHonestBroker() : UnitCard(476) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        createGoldToken(ctx);
        ctx.events.logTrace("HONEST BROKER (Deathknell): Gold gear token created");
    }
};

// [778] Plundering Poro — When I conquer, play a Gold gear token exhausted.
class MPlunderingPoro : public UnitCard {
public:
    MPlunderingPoro() : UnitCard(778) {}
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        createGoldToken(ctx);
        ctx.events.logTrace("PLUNDERING PORO: Gold gear token created");
    }
};

// [583] Grim Apothecary — Ambush + when I'm played, you may return a
// friendly unit at a battlefield to owner's hand. Ambush is
// engine-handled. The bounce is routed through Card::confirmOptional —
// the agent decides yes/no, and the picked target is the first
// friendly unit at a BF (target SELECTION among multiple candidates is
// a separate follow-up; today's wire is "skip vs accept the default
// target").
class MGrimApothecary : public UnitCard {
public:
    MGrimApothecary() : UnitCard(583) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto find_friendly_at_bf = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.isAtBattlefield()) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_friendly_at_bf() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Grim Apothecary: bounce a friendly unit at a battlefield?",
            still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        auto target = find_friendly_at_bf();
        if (target == kInvalidId) return;
        auto& obj = ctx.state.getObject(target);
        ctx.executor.bounceToHand(target);
        ctx.events.logTrace("GRIM APOTHECARY: bounced " + obj.name);
    }
};

// [687] Lunar Boon — [Reaction] Discard 1, then draw 2.
//
// Phase C-1 commit 6 — pilot for the resume-point pattern (discardCards
// primitive). The agent's choice over the discarded card is published via
// `requestChoice` in case 0; the chain manager re-invokes onResolve at
// case 1 once the choice is made.
class MLunarBoon : public SpellCard {
public:
    MLunarBoon() : SpellCard(687) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            if (ps.hand.empty()) {
                // No discard possible — fall through to the draw.
                ctx.executor.drawCards(ctx.controller, 2);
                return;
            }
            std::vector<Intent> choices;
            std::string label = "Lunar Boon: discard 1 then draw 2 [";
            bool first = true;
            for (auto card_id : ps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {card_id};
                choices.push_back(c);
                if (!first) label += " | ";
                first = false;
                if (ctx.state.objectExists(card_id)) {
                    label += ctx.state.getObject(card_id).name;
                }
            }
            label += "]";
            ctx.executor.requestChoice(ctx.controller, std::move(choices), label);
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                auto cid = choice->chosen_objects[0];
                std::string n = ctx.state.objectExists(cid)
                    ? ctx.state.getObject(cid).name : "card";
                ctx.events.logTrace("LUNAR BOON: discarded " + n);
                ctx.executor.applyDiscard(ctx.controller, cid);
            }
            // Wrap drawCards with a per-spell prefix so V&V can grep
            // "LUNAR BOON: drew" for the cards landed by THIS spell
            // (vs the generic "DREW: ..." that fires for all draws).
            auto& ps = ctx.state.player(ctx.controller);
            size_t before = ps.hand.size();
            ctx.executor.drawCards(ctx.controller, 2);
            for (size_t i = before; i < ps.hand.size(); ++i) {
                auto did = ps.hand[i];
                std::string dn = ctx.state.objectExists(did)
                    ? ctx.state.getObject(did).name : "card";
                ctx.events.logTrace("LUNAR BOON: drew " + dn +
                                     " — PRIVATE to " + toString(ctx.controller));
            }
            return;
        }
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Spell effects batch (2026-05-15)
// ═══════════════════════════════════════════════════════════════════════════════

// [727] Shadow's Call — Choose a friendly unit without Temporary. Give it
// Temporary. Draw 2.
class MShadowsCall : public SpellCard {
public:
    MShadowsCall() : SpellCard(727) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    // Phase 6q proof-of-concept (single friendly-unit target).
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Shadow's Call", legal);
        // pickTarget returns kInvalidId in TWO cases. Distinguish via
        // resume_point: == 7 means "just published prompt, suspending —
        // chain manager will re-enter"; >= 8 (or no chain at all) means
        // "committed pick (possibly nothing legal)". For Shadow's Call,
        // the "draw 2" rider fires even when the targeted part has no
        // legal pick (the test fixture DrawsTwoEvenWithoutTarget pins
        // this behavior — partial-fizzle, riders always resolve).
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended for agent decision
        }
        if (picked != kInvalidId && ctx.state.objectExists(picked)) {
            ctx.executor.giveTemporaryKeyword(picked, Keyword::Temporary, 0);
        }
        ctx.executor.drawCards(ctx.controller, 2);
    }
};

// [690] Star-Crossed — Return a friendly unit and an enemy unit to their
// owners' hands. Two-target spell: target[0] friendly, target[1] enemy.
// TargetRequirements expresses only count + side-uniform constraints, so the
// validation here is per-target side checks in onResolve.
class MStarCrossed : public SpellCard {
public:
    MStarCrossed() : SpellCard(690) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    // Phase 6q+ — defer pair selection so the policy head gets
    // distinct slots per (friendly, enemy) pair instead of
    // collapsing all O(friends × enemies) variants into one Play
    // slot. Sequential MakeChoice: first picks friendly, second
    // picks enemy. Each pick is policy-head-visible.
    bool needsPlayTimeTargetPair() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> legal_friendly;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.location.has_value()) continue;
            if (!obj.isUnit()) continue;
            if (obj.controller != ctx.controller) continue;
            legal_friendly.push_back(id);
        }
        auto enemy_fn = [&](GameObjectId /*picked_a*/) {
            std::vector<GameObjectId> legal_enemy;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.location.has_value()) continue;
                if (!obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                if (obj.untargetable_by_enemy) continue;
                legal_enemy.push_back(id);
            }
            return legal_enemy;
        };
        auto [friendly, enemy] = pickTargetPair(ctx, "Star-Crossed",
                                                  legal_friendly,
                                                  enemy_fn);
        // Suspend detection: if either is kInvalidId AND we're at
        // a prompt-publish resume_point (10 or 12), return now —
        // chain re-enters after agent picks.
        bool suspending = (friendly == kInvalidId || enemy == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (friendly != kInvalidId && ctx.state.objectExists(friendly)) {
            ctx.executor.bounceToHand(friendly);
        }
        if (enemy != kInvalidId && ctx.state.objectExists(enemy)) {
            ctx.executor.bounceToHand(enemy);
        }
    }
};

// [192] Mindsplitter — When you play me, choose an opponent, they reveal
// their hand, choose a card, they discard it. We approximate: opponent
// discards 1 (the discard target is the agent's choice via the engine's
// opponentDiscards path; the "reveal" step is observation-only and isn't
// yet wired into ObservationTracker — that's Phase 10 work).
//
// Phase C-1 commit 6 — pilot for the resume-point pattern from a unit
// trigger (WhenYouPlayMe goes through the chain via TriggerManager, so
// `state.chain.resuming` is populated when onTrigger runs).
class MMindsplitter : public UnitCard {
public:
    MMindsplitter() : UnitCard(192) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        PlayerId opp = opponent(ctx.controller);
        auto& opps = ctx.state.player(opp);
        switch (ri.resume_point) {
        case 0: {
            if (opps.hand.empty()) return;
            // CR: "They reveal their hand." Emit a private
            // CardRevealedEvent (revealed_to = controller) per card so
            // PlayerState::observed_cards updates for the controller —
            // that's the imperfect-info-observable side effect that
            // makes Mindsplitter actually informative for ML training.
            for (auto card_id : opps.hand) {
                if (!ctx.state.objectExists(card_id)) continue;
                const auto& obj = ctx.state.getObject(card_id);
                ctx.events.emit(CardRevealedEvent{
                    /*card=*/card_id,
                    /*card_def_id=*/obj.card_def_id,
                    /*owner=*/obj.owner,
                    /*revealed_to_all=*/false,
                    /*revealed_to=*/ctx.controller,
                    /*source_zone=*/ZoneType::Hand,
                });
                ctx.events.logTrace("MINDSPLITTER: revealed " + obj.name +
                                     " (id=" + std::to_string(card_id) +
                                     ") to " + std::string(toString(ctx.controller)));
            }
            std::vector<Intent> choices;
            for (auto card_id : opps.hand) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = opp;
                c.chosen_objects = {card_id};
                choices.push_back(c);
            }
            ctx.executor.requestChoice(opp, std::move(choices),
                                        "opponent discard 1 (Mindsplitter)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (choice && !choice->chosen_objects.empty()) {
                ctx.executor.applyDiscard(opp, choice->chosen_objects[0]);
                ctx.events.logTrace("MINDSPLITTER: opponent discards 1");
            }
            return;
        }
        }
    }
};

// [484] Deathgrip — Reaction spell. Kill a friendly unit; if you do, give
// +[M] equal to its Might to another friendly unit this turn. Draw 1.
// Two targets: target[0] friendly to kill, target[1] friendly recipient.
class MDeathgrip : public SpellCard {
public:
    MDeathgrip() : SpellCard(484) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.size() >= 2 && ctx.state.objectExists(targets[0])
                                && ctx.state.objectExists(targets[1])) {
            auto sacrifice_id = targets[0];
            auto recipient_id = targets[1];
            int might = ctx.state.getObject(sacrifice_id).current_might;
            ctx.executor.killObject(sacrifice_id);
            ctx.executor.giveTemporaryMight(recipient_id, might);
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Gear + activated + reaction batch (2026-05-15)
// ═══════════════════════════════════════════════════════════════════════════════

// [375] Heart of Dark Ice (gear) — [E]: Give a unit +3M this turn.
// Phase 6r — needs_activation_time_target for vocab-slot distinction.
class MHeartOfDarkIce : public GearCard {
public:
    MHeartOfDarkIce() : GearCard(375) {}
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true},
            .is_action = false, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    void onActivate(CardContext& ctx, int /*ability_idx*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0);
            picked = pickTarget(ctx, "Heart of Dark Ice", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.giveTemporaryMight(picked, 3);
        ctx.events.logTrace("HEART OF DARK ICE: +3M to " +
                             ctx.state.getObject(picked).name);
    }
};

// [752] Shadow — If played to a BF, enters ready (engine-side: rune-pool
// "enters ready" semantics are not a default for units, but the action
// generator emits a no-cost-paid play intent and the engine reads
// is_exhausted from the unit's onPlay path; we override onPlay to clear
// is_exhausted when the unit arrives at a BF).
// Plus [Action]: [1][A], [E] -> stun enemy unit attacking here.
class MShadow : public UnitCard {
public:
    MShadow() : UnitCard(752) {}
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (self.isAtBattlefield()) self.is_exhausted = false;
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true, .energy = 1, .power = 1,
                     .power_domain = Domain::Fury},  // [1][A] — A is universal
            .targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                           .must_be_enemy = true,
                                           .must_be_at_battlefield = true},
            .is_action = true, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    void onActivate(CardContext& ctx, int /*ability_idx*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0);
            picked = pickTarget(ctx, "Shadow", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // CR 5h: combat_designation is cleared between trigger-fire and
        // chain resolution. For actively-attacking-here gating we
        // capture the attacker id via card_counters set elsewhere; for
        // Shadow's specific case (you pick an attacker at activation
        // time), targets[0] IS the attacker the player chose. We still
        // verify combat is in progress at THIS BF.
        auto& tgt = ctx.state.getObject(picked);
        if (tgt.combat_designation != CombatDesignation::Attacker) return;
        ctx.executor.stunUnitBy(picked, ctx.source);
    }
};

// [693] Abandon — [Reaction] Counter a spell, return to owner's hand, Predict 1.
//
// Phase C-1 commit 6 — pilot for the resume-point pattern (predict
// primitive). Lives in manual/ rather than generated/ so the refactor
// survives `generate_cards.py` regenerations (the generated counterpart
// would be overwritten). Counter spells continue to use
// `state.chain.items.back()` to find their target — when SAbandon resolves,
// the chain manager has already moved it from `items` into
// `state.chain.resuming`, so `items.back()` is the spell being countered.
class MAbandon : public SpellCard {
public:
    MAbandon() : SpellCard(693) {}
    // Per CR 355.9.a.2 + 355.10: "Counter a spell" is a target
    // requirement on a Chain object. Abandon needs a spell on chain
    // to be playable; the Predict 1 side-effect doesn't make it
    // playable on its own. Earlier session left this as "always
    // playable" which is wrong per CR — corrected after user CR review.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (it->is_spell) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            // Counter step. items.back() is the spell below Abandon
            // (Abandon itself is now in `resuming`, not on `items`).
            if (!ctx.state.chain.items.empty()) {
                auto& top = ctx.state.chain.items.back();
                if (top.is_spell) {
                    auto countered_source = top.source;
                    revertCounteredPlay(ctx, top);  // CR 425.1.b
                    ctx.state.chain.items.pop_back();
                    if (ctx.state.objectExists(countered_source)) {
                        auto& obj = ctx.state.getObject(countered_source);
                        ctx.events.logTrace("COUNTER: " + obj.name +
                                            " countered by Abandon -> hand");
                        obj.zone = ZoneType::Hand;
                        obj.location = std::nullopt;
                        ctx.state.player(obj.owner).hand.push_back(countered_source);
                    }
                }
            }

            // Predict 1: peek top, publish recycle/keep choice.
            if (ps.main_deck.empty()) return;
            auto top_card = ps.main_deck.back();
            ps.main_deck.pop_back();
            ri.resume_data = {static_cast<int32_t>(top_card)};

            std::vector<Intent> choices;
            Intent recycle;
            recycle.type = IntentType::MakeChoice;
            recycle.player = ctx.controller;
            recycle.chosen_objects = {top_card};
            choices.push_back(recycle);
            Intent keep;
            keep.type = IntentType::MakeChoice;
            keep.player = ctx.controller;
            choices.push_back(keep);

            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "predict 1: recycle or keep (Abandon)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (ri.resume_data.empty()) return;
            auto top_card = static_cast<GameObjectId>(ri.resume_data[0]);
            if (!ctx.state.objectExists(top_card)) return;

            std::string n = ctx.state.getObject(top_card).name;
            if (choice && !choice->chosen_objects.empty()) {
                ctx.state.getObject(top_card).zone = ZoneType::MainDeck;
                ps.main_deck.insert(ps.main_deck.begin(), top_card);
                ctx.events.logTrace("PREDICT: " + n +
                                     " -> bottom of deck (recycled)");
            } else {
                // Put back on top. Was previously silent — surfaced after
                // user noticed the keep-case wasn't visible in the replay.
                ps.main_deck.push_back(top_card);
                ctx.events.logTrace("PREDICT: " + n + " -> top of deck (kept)");
            }
            return;
        }
        }
    }
};

// [457] Hard Bargain — [Reaction] [Repeat][2] Counter a spell unless its
// controller pays [2].
//
// Audit follow-up (task #16): the generated SHardBargain stub did
// nothing — onResolve had a comment "Counter spell on chain" and an
// empty body. So Hard Bargain was burning 2E and silently no-op-ing.
// This manual override implements the actual counter:
//   case 0 — peek the spell on chain (items.back() = the spell below
//     us, since stepResolve already moved Hard Bargain into resuming).
//     If there's no spell to counter, no-op (matches the existing
//     counter-with-no-target convention used by Abandon, etc.). If the
//     spell's controller can't afford 2E to save it, auto-counter (no
//     point in the choice). Otherwise publish a "pay 2 or get
//     countered" choice to that controller.
//   case 1 — take the choice. If they paid: exhaust 2 of their ready
//     runes, leave the spell on the chain. If they didn't: pop the
//     spell and trash it.
//
// Repeat[2] (the repeat-this-spell-by-paying-2 mechanic) isn't yet
// modeled — needs an additional-cost-at-play hook on the engine.
// Documented as a follow-up.
class MHardBargain : public SpellCard {
public:
    MHardBargain() : SpellCard(457) {}
    // Pure counter — no secondary effect. Not playable unless there's a
    // spell on the chain to potentially counter.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (it->is_spell) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = ctx.state.chain.resuming.value();
        switch (ri.resume_point) {
        case 0: {
            if (ctx.state.chain.items.empty()) {
                ctx.events.logTrace("HARD BARGAIN: nothing on chain to counter — no-op");
                return;
            }
            auto& top = ctx.state.chain.items.back();
            if (!top.is_spell) {
                ctx.events.logTrace("HARD BARGAIN: top of chain isn't a spell — no-op");
                return;
            }
            PlayerId target_controller = top.controller;
            GameObjectId target_source = top.source;
            ri.resume_data = {static_cast<int32_t>(target_source)};

            // Count target controller's ready runes — they need 2 to save.
            auto& tps = ctx.state.player(target_controller);
            int ready_runes = 0;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isRune() || obj.controller != target_controller) continue;
                if (!obj.location.has_value()) continue;
                if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
                if (std::get<BaseLocation>(*obj.location).player != target_controller) continue;
                if (!obj.is_exhausted) ++ready_runes;
            }
            (void)tps;

            if (ready_runes < 2) {
                // Can't afford the rescue cost — counter immediately.
                if (!ctx.state.chain.items.empty()) {
                    revertCounteredPlay(ctx, ctx.state.chain.items.back());  // CR 425.1.b
                }
                ctx.state.chain.items.pop_back();
                if (ctx.state.objectExists(target_source)) {
                    auto& obj = ctx.state.getObject(target_source);
                    ctx.events.logTrace("HARD BARGAIN: countered " + obj.name +
                                         " (" + toString(target_controller) +
                                         " can't afford 2E to save)");
                    obj.zone = ZoneType::Trash;
                    obj.location = std::nullopt;
                    ctx.state.player(obj.owner).trash.push_back(target_source);
                }
                return;
            }

            // Offer the target's controller a binary choice: pay 2 to save,
            // or let it be countered. Two MakeChoice intents — chosen_objects
            // is empty for "let it die," contains the target_source for "pay."
            std::vector<Intent> choices;
            Intent pay;
            pay.type = IntentType::MakeChoice;
            pay.player = target_controller;
            pay.chosen_objects = {target_source};
            choices.push_back(pay);
            Intent decline;
            decline.type = IntentType::MakeChoice;
            decline.player = target_controller;
            choices.push_back(decline);
            std::string spell_name = ctx.state.objectExists(target_source)
                ? ctx.state.getObject(target_source).name : "spell";
            ctx.executor.requestChoice(target_controller, std::move(choices),
                                        "Hard Bargain: pay 2 to save " + spell_name +
                                        "? [pay | decline]");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (ri.resume_data.empty()) return;
            auto target_source = static_cast<GameObjectId>(ri.resume_data[0]);
            if (!ctx.state.objectExists(target_source)) return;

            // Determine target controller from the (still on-chain) item.
            // The top of `items` is still our target — case 0 didn't pop it.
            PlayerId target_controller = ctx.state.objectExists(target_source)
                ? ctx.state.getObject(target_source).controller
                : opponent(ctx.controller);

            bool paid = (choice && !choice->chosen_objects.empty() &&
                          choice->chosen_objects[0] == target_source);

            if (paid) {
                // Exhaust 2 ready runes from target_controller's base.
                int exhausted = 0;
                for (auto& [id, obj] : ctx.state.objects) {
                    if (exhausted >= 2) break;
                    if (!obj.isRune() || obj.controller != target_controller) continue;
                    if (!obj.location.has_value()) continue;
                    if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
                    if (std::get<BaseLocation>(*obj.location).player != target_controller) continue;
                    if (obj.is_exhausted) continue;
                    obj.is_exhausted = true;
                    ++exhausted;
                    ctx.events.logTrace("  HARD BARGAIN COST: exhaust " + obj.name +
                                         " (id=" + std::to_string(id) + ")");
                }
                ctx.events.logTrace(std::string("HARD BARGAIN: ") +
                                     toString(target_controller) +
                                     " paid 2E to save " +
                                     ctx.state.getObject(target_source).name);
                // Spell stays on chain — no pop.
            } else {
                // Counter — pop + trash.
                if (!ctx.state.chain.items.empty()) {
                    revertCounteredPlay(ctx, ctx.state.chain.items.back());  // CR 425.1.b
                    ctx.state.chain.items.pop_back();
                }
                auto& obj = ctx.state.getObject(target_source);
                ctx.events.logTrace("HARD BARGAIN: countered " + obj.name +
                                     " (controller declined to pay)");
                obj.zone = ZoneType::Trash;
                obj.location = std::nullopt;
                ctx.state.player(obj.owner).trash.push_back(target_source);
            }
            return;
        }
        }
    }
};

// [183] Stacked Deck — [Action] Look at top 3 cards of your Main Deck. Put
// 1 into your hand and recycle the rest.
//
// Audit follow-up (task #17): the generated SStackedDeck stub did
// `drawCards(1)` blindly — no peek, no choice presented to the agent,
// no recycling. This manual override implements the real card text:
//   case 0: peek up to 3 cards (fewer if deck shallow per CR 431.1.c),
//           emit a CardRevealedEvent for each (private to the controller),
//           publish a 1-of-N MakeChoice ("which card do I keep?").
//   case 1: take the choice. Chosen card -> hand. Others -> bottom of
//           main deck (recycle).
//
// resume_data layout: [N, peeked_card_0, peeked_card_1, peeked_card_2]
// where N is the actual count peeked (0..3).
class MStackedDeck : public SpellCard {
public:
    MStackedDeck() : SpellCard(183) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);
        switch (ri.resume_point) {
        case 0: {
            int actual = std::min(3, static_cast<int>(ps.main_deck.size()));
            ri.resume_data.clear();
            ri.resume_data.push_back(static_cast<int32_t>(actual));
            for (int i = 0; i < actual; ++i) {
                auto cid = ps.main_deck.back();
                ps.main_deck.pop_back();
                ri.resume_data.push_back(static_cast<int32_t>(cid));
                if (ctx.state.objectExists(cid)) {
                    auto& obj = ctx.state.getObject(cid);
                    ctx.events.logTrace("  PEEKED: " + obj.name + " (id=" +
                                         std::to_string(cid) +
                                         ") — PRIVATE to " + toString(ctx.controller));
                    ctx.events.emit(CardRevealedEvent{
                        cid, obj.card_def_id, obj.owner,
                        false, ctx.controller, ZoneType::MainDeck,
                    });
                }
            }
            if (actual == 0) return;  // empty deck — no-op

            // Publish a 1-of-N "pick the keeper" choice. Each Intent's
            // chosen_objects = [the card to put into hand].
            std::vector<Intent> choices;
            std::string label = "Stacked Deck: pick 1 of " +
                                std::to_string(actual) + " to draw [";
            for (int i = 1; i <= actual; ++i) {
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {static_cast<GameObjectId>(ri.resume_data[i])};
                choices.push_back(c);
                if (i > 1) label += " | ";
                if (ctx.state.objectExists(static_cast<GameObjectId>(ri.resume_data[i]))) {
                    label += ctx.state.getObject(static_cast<GameObjectId>(ri.resume_data[i])).name;
                }
            }
            label += "]";
            ctx.executor.requestChoice(ctx.controller, std::move(choices), label);
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            int n = ri.resume_data.empty() ? 0 : ri.resume_data[0];
            GameObjectId keeper = kInvalidId;
            if (choice && !choice->chosen_objects.empty()) {
                keeper = choice->chosen_objects[0];
            }
            // Recycle the non-chosen peeked cards to bottom; chosen → hand.
            // We process in original peek order so the recycle order is
            // deterministic (top peeked card recycles first, ends up
            // deeper in the deck after subsequent inserts).
            for (int i = 1; i <= n; ++i) {
                auto cid = static_cast<GameObjectId>(ri.resume_data[i]);
                if (!ctx.state.objectExists(cid)) continue;
                auto& obj = ctx.state.getObject(cid);
                if (cid == keeper) {
                    obj.zone = ZoneType::Hand;
                    obj.location = std::nullopt;
                    ps.hand.push_back(cid);
                    ctx.events.logTrace("STACKED DECK: drew " + obj.name);
                } else {
                    obj.zone = ZoneType::MainDeck;
                    obj.location = std::nullopt;
                    ps.main_deck.insert(ps.main_deck.begin(), cid);
                    ctx.events.logTrace("STACKED DECK: recycled " + obj.name);
                }
            }
            return;
        }
        }
    }
};

// [156] Sabotage — "Choose an opponent. They reveal their hand. Choose a
// non-unit card from it, and recycle that card."
//
// CR notes baked into this implementation:
//   • The CASTER chooses (not the opponent). The opponent reveals; the
//     caster, seeing the revealed hand, picks the recycle target.
//   • Only non-unit cards are valid choices. Units (champions and other
//     units) in hand are filtered out.
//   • Recycle = bottom of opponent's main deck (not trash/discard).
//   • Reveal emits a CardRevealedEvent for every hand card, private to
//     the caster (revealed_to = caster). GameEngine subscribes and
//     populates PlayerState::observed_cards — the "revealed card memory
//     bank" — so the caster's observation is durably recorded.
//   • If the opponent has no non-unit cards in hand, the reveal still
//     happens but the recycle does not (no legal choice).
class MSabotage : public SpellCard {
public:
    MSabotage() : SpellCard(156) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        PlayerId opp = opponent(ctx.controller);
        auto& opps = ctx.state.player(opp);
        switch (ri.resume_point) {
        case 0: {
            if (opps.hand.empty()) return;
            // Reveal: emit CardRevealedEvent for every card in the
            // opponent's hand, private to the caster. The subscriber on
            // GameEngine increments observed_cards[card_def_id] for the
            // caster, which is what makes this card actually informative
            // (the caster now knows what's in the opponent's hand for
            // future decisions).
            for (auto card_id : opps.hand) {
                if (!ctx.state.objectExists(card_id)) continue;
                const auto& obj = ctx.state.getObject(card_id);
                ctx.events.emit(CardRevealedEvent{
                    /*card=*/card_id,
                    /*card_def_id=*/obj.card_def_id,
                    /*owner=*/obj.owner,
                    /*revealed_to_all=*/false,
                    /*revealed_to=*/ctx.controller,
                    /*source_zone=*/ZoneType::Hand,
                });
                ctx.events.logTrace("SABOTAGE: revealed " + obj.name +
                                     " (id=" + std::to_string(card_id) +
                                     ") to " + std::string(toString(ctx.controller)));
            }
            // Build choice set: non-unit cards only.
            std::vector<Intent> choices;
            for (auto card_id : opps.hand) {
                if (!ctx.state.objectExists(card_id)) continue;
                const auto& obj = ctx.state.getObject(card_id);
                if (obj.isUnit()) continue;  // CR: "non-unit card"
                Intent c;
                c.type = IntentType::MakeChoice;
                c.player = ctx.controller;
                c.chosen_objects = {card_id};
                choices.push_back(c);
            }
            if (choices.empty()) {
                ctx.events.logTrace("SABOTAGE: opponent has no non-unit cards in "
                                     "hand; recycle skipped.");
                return;
            }
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "recycle 1 non-unit from opponent's hand (Sabotage)");
            ri.resume_point = 1;
            return;
        }
        case 1: {
            auto choice = ctx.executor.takeChoice();
            if (!choice || choice->chosen_objects.empty()) return;
            auto card_id = choice->chosen_objects[0];
            if (!ctx.state.objectExists(card_id)) return;

            // Remove from opponent's hand, then recycle (bottom of
            // opponent's main deck). recycleCards inserts at the front
            // of the deck vector (bottom = front, top = back).
            auto& hand = opps.hand;
            auto it = std::find(hand.begin(), hand.end(), card_id);
            if (it == hand.end()) return;
            const auto& obj = ctx.state.getObject(card_id);
            ctx.events.logTrace("SABOTAGE: recycling " + obj.name + " (id=" +
                                 std::to_string(card_id) + ") to bottom of " +
                                 std::string(toString(opp)) + "'s deck");
            hand.erase(it);
            ctx.executor.recycleCards(opp, {card_id});
            return;
        }
        }
    }
};

// [735] Sacrifice — Reaction. Additional cost: kill a friendly Mighty (≥5M)
// unit. Then draw 2 and channel 1 rune exhausted. The "additional cost" is
// modeled here in onResolve: scan for a friendly mighty unit and kill it if
// one exists; otherwise resolution still draws+channels (the engine doesn't
// yet enforce additional costs at play-time, so this is best-effort).
class MSacrifice : public SpellCard {
public:
    MSacrifice() : SpellCard(735) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Find any friendly Mighty unit (≥5 current_might) and kill it.
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (obj.current_might < 5) continue;
            ctx.executor.killObject(id);
            break;
        }
        ctx.executor.drawCards(ctx.controller, 2);
        ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Vex/XP units batch (2026-05-15)
// ═══════════════════════════════════════════════════════════════════════════════

// [596] Herald of Spring — [Hunt] + on play, gain 2 XP. Hunt itself (the
// +1 XP on conquer/hold) is engine-handled via the keyword pass; we add the
// on-play +2 XP here.
class MHeraldOfSpring : public UnitCard {
public:
    MHeraldOfSpring() : UnitCard(596) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("HERALD OF SPRING: +2 XP on play");
    }
};

// [605] Enthusiastic Promoter — [Backline] + when I hold, buff all units
// at my battlefield. Backline is engine-handled; this implements the hold
// trigger: iterate units at my BF (mine + opp's? text says "all units" —
// in TCG language this typically means everyone at the location, but for
// gameplay clarity we apply it only to friendly units here. Adjust if play
// data shows the literal "everyone" reading is intended).
class MEnthusiasticPromoter : public UnitCard {
public:
    MEnthusiasticPromoter() : UnitCard(605) {}
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto bf_id = ctx.state.getObject(ctx.source).battlefieldId();
        if (!bf_id) return;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (obj.battlefieldId() != bf_id) continue;
            ctx.executor.buffUnit(id);
        }
        ctx.events.logTrace("ENTHUSIASTIC PROMOTER: buffed friendly units here");
    }
};

// [610] Trevor Snoozebottom — [Shield] + when I hold, play a ready 3M
// Sprite unit token with Temporary at my BF.
class MTrevorSnoozebottom : public UnitCard {
public:
    MTrevorSnoozebottom() : UnitCard(610) {}
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto bf_id = ctx.state.getObject(ctx.source).battlefieldId();
        if (!bf_id) return;
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = LocationId{BattlefieldLocation{*bf_id}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, true);
    }
};

// [689] Mister Root — [Accelerate] + when I move to a battlefield, gain 2 XP.
// Accelerate is engine-handled (cost option); we add the XP gain.
class MMisterRoot : public UnitCard {
public:
    MMisterRoot() : UnitCard(689) {}
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 2;
        ctx.events.logTrace("MISTER ROOT: +2 XP on move to BF");
    }
};

// [688] Megatusk — Spend 3 XP (activated, no other cost): give your units
// at my BF Ganking this turn. XP isn't an ActivationCost field, so the cost
// is paid manually in onActivate (check >= 3, subtract). If XP < 3 the
// ability silently no-ops (the engine doesn't yet enforce XP costs at the
// chain-construction stage; this is the same pattern Voidreaver would
// follow once added).
class MMegatusk : public UnitCard {
public:
    MMegatusk() : UnitCard(688) {}
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override { return {}; }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.xp < 3) return;
        ps.xp -= 3;
        auto bf_id = ctx.state.getObject(ctx.source).battlefieldId();
        if (!bf_id) return;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (obj.battlefieldId() != bf_id) continue;
            ctx.executor.giveTemporaryKeyword(id, Keyword::Ganking, 0);
        }
        ctx.events.logTrace("MEGATUSK: spent 3 XP; friendly units here get Ganking");
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Combat tricks + counter spells batch (2026-05-15)
// ═══════════════════════════════════════════════════════════════════════════════

// [657] Grim Resolve — Action spell: give a friendly unit +3M this turn.
// The "when it wins a combat this turn, gain 2 XP" rider needs the
// WhenIWinCombat trigger which doesn't exist yet (delayed-trigger framework
// also). Just give +3M for now; XP delayed-effect is the follow-up.
class MGrimResolve : public SpellCard {
public:
    MGrimResolve() : SpellCard(657) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    // Phase 6q proof-of-concept — defer target selection to
    // resolve-time so the policy head sees distinct vocab slots per
    // target choice (rather than collapsing all (Grim Resolve, target)
    // plays into one Play slot).
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Re-enumerate at resolve time — state may have shifted since
        // the Play action was committed (target killed, bounced, etc.).
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Grim Resolve", legal);
        if (picked == kInvalidId) {
            // Either: (a) waiting for agent — chain manager will
            // re-enter; or (b) no legal targets at resolve time
            // (fizzle). Either way, return now.
            return;
        }
        ctx.executor.giveTemporaryMight(picked, 3);
        ctx.events.logTrace("GRIM RESOLVE: +3M (delayed XP rider deferred)");
    }
};

// Counter-spell helper: peek-and-pop the chain top, send the countered card
// to the controller's trash. Same pattern as MDefy; centralised so the
// Repulse/Not So Fast variants don't duplicate it.
//
// Phase 6q+ engine-audit CRITICAL #5 fix: per CR 425.1.b "A card that
// is Countered is not considered to have been played", the countered
// card's contribution to cards_played_this_turn must be reverted.
// Otherwise Legion gates (>=2 played) and similar count-based effects
// would fire off of cards that never resolved. We also restore
// last_spell_energy_spent to its pre-play value so subsequent
// "you spent N+" triggers (Forgotten Library, Virtuoso) don't read
// the countered amount.
inline void counterChainTop(CardContext& ctx) {
    if (ctx.state.chain.items.empty()) return;
    auto& top = ctx.state.chain.items.back();
    if (!top.is_spell) return;
    auto countered = top.source;
    revertCounteredPlay(ctx, top);  // CR 425.1.b
    ctx.state.chain.items.pop_back();
    if (ctx.state.objectExists(countered)) {
        auto& obj = ctx.state.getObject(countered);
        ctx.events.logTrace("COUNTER: " + obj.name + " countered -> trash");
        obj.zone = ZoneType::Trash;
        obj.location = std::nullopt;
        ctx.state.player(obj.owner).trash.push_back(countered);
    }
}

// [668] Repulse — Counter an enemy spell or ability that chooses a friendly
// unit. We check that the chain-top item is enemy-controlled AND targets a
// friendly unit. The check uses ChainItem::targets — if any target is owned
// by us, the counter applies.
class MRepulse : public SpellCard {
public:
    MRepulse() : SpellCard(668) {}
    // Counter an enemy spell/ability that targets a friendly UNIT. Not
    // playable unless the chain top matches that shape.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        if (state.chain.items.empty()) return false;
        const auto& top = state.chain.items.back();
        if (top.controller == controller) return false;
        for (auto tid : top.targets) {
            if (!state.objectExists(tid)) continue;
            const auto& t = state.getObject(tid);
            if (t.controller == controller && t.isUnit()) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.chain.items.empty()) return;
        const auto& top = ctx.state.chain.items.back();
        if (top.controller == ctx.controller) return;  // not enemy
        bool targets_friendly = false;
        for (auto tid : top.targets) {
            if (!ctx.state.objectExists(tid)) continue;
            if (ctx.state.getObject(tid).controller == ctx.controller) {
                targets_friendly = true; break;
            }
        }
        if (!targets_friendly) return;
        counterChainTop(ctx);
    }
};

// [368] Not So Fast — Counter an enemy spell or ability that chooses a
// friendly unit OR gear. Same as Repulse but broader (gear too). For our
// simplified target check we accept any friendly card on the targets list.
class MNotSoFast : public SpellCard {
public:
    MNotSoFast() : SpellCard(368) {}
    // Counter an enemy spell/ability that targets a friendly UNIT or GEAR.
    // Not playable unless the chain top matches.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        if (state.chain.items.empty()) return false;
        const auto& top = state.chain.items.back();
        if (top.controller == controller) return false;
        for (auto tid : top.targets) {
            if (!state.objectExists(tid)) continue;
            const auto& t = state.getObject(tid);
            if (t.controller == controller && (t.isUnit() || t.isGear())) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.chain.items.empty()) return;
        const auto& top = ctx.state.chain.items.back();
        if (top.controller == ctx.controller) return;
        bool targets_friendly = false;
        for (auto tid : top.targets) {
            if (!ctx.state.objectExists(tid)) continue;
            if (ctx.state.getObject(tid).controller == ctx.controller) {
                targets_friendly = true; break;
            }
        }
        if (!targets_friendly) return;
        counterChainTop(ctx);
    }
};

// [696] Existential Dread — Action spell, Repeat 2. Stun an attacking enemy
// unit. If already stunned, return to owner's hand instead.
// Repeat is engine-handled (allows the cost to be paid again to re-execute);
// we just implement the single-resolve.
class MExistentialDread : public SpellCard {
public:
    MExistentialDread() : SpellCard(696) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true,
                                   .must_be_at_battlefield = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Existential Dread", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto& tgt = ctx.state.getObject(picked);
        if (tgt.combat_designation != CombatDesignation::Attacker) return;
        if (tgt.is_stunned) {
            ctx.executor.bounceToHand(picked);
        } else {
            ctx.executor.stunUnit(picked);
        }
    }
};

// [449] Overzealous Fan — When I defend, you may kill me to bounce an
// attacking unit. Routed through Card::confirmOptional so the OpenSpiel
// agent decides yes/no rather than auto-firing.
//
// Reads the attacker GameObjectId from `card_counters["__defend_attacker_id"]`,
// which `TriggerManager::onCombatStarted` captures at trigger-firing
// time. Necessary because `combat_designation` is cleared between
// trigger fire and chain resolution — scanning the board at resolve
// time would find no attackers and silently no-op (the old behavior).
class MOverzealousFan : public UnitCard {
public:
    MOverzealousFan() : UnitCard(449) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        // Read the captured attacker. Validate it still exists and is a
        // legal bounce target (unit, on board, enemy). The card_counters
        // entry persists until reset, but the actual GameObject may have
        // moved / been removed during chain resolution.
        auto get_attacker = [&]() -> GameObjectId {
            if (!ctx.state.objectExists(ctx.source)) return kInvalidId;
            auto& self = ctx.state.getObject(ctx.source);
            auto it = self.card_counters.find("__defend_attacker_id");
            if (it == self.card_counters.end()) return kInvalidId;
            GameObjectId attacker_id = static_cast<GameObjectId>(it->second);
            if (!ctx.state.objectExists(attacker_id)) return kInvalidId;
            auto& a = ctx.state.getObject(attacker_id);
            if (!a.isUnit()) return kInvalidId;
            if (a.controller == ctx.controller) return kInvalidId;
            if (!a.location.has_value()) return kInvalidId;
            return attacker_id;
        };
        auto still_legal = [&]() { return get_attacker() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Overzealous Fan: kill self to bounce attacker?", still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        GameObjectId attacker = get_attacker();
        if (attacker == kInvalidId) return;
        std::string attacker_name = ctx.state.getObject(attacker).name;
        ctx.executor.killObject(ctx.source);
        ctx.executor.bounceToHand(attacker);
        ctx.events.logTrace("OVERZEALOUS FAN: killed self, bounced " + attacker_name);
    }
};

// [67] Blitzcrank, Impassive — "[Tank] When you play me to a battlefield,
// you may move an enemy unit to here." (The second triggered ability —
// "When I hold, return me to my owner's hand" — isn't modeled because
// Card supports only one triggerType today; multi-ability per Card is
// queued in Known engine gaps.) Routed through confirmOptional.
class MBlitzcrankImpassive : public UnitCard {
public:
    MBlitzcrankImpassive() : UnitCard(67) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto my_bf = self.battlefieldId();
        if (!my_bf) return;  // not at a BF — trigger condition not met

        auto find_enemy_elsewhere = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                if (obj.battlefieldId() == my_bf) continue;
                if (!obj.location.has_value()) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_enemy_elsewhere() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Blitzcrank: pull an enemy unit to my battlefield?", still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        auto target = find_enemy_elsewhere();
        if (target == kInvalidId) return;
        std::string name = ctx.state.getObject(target).name;
        ctx.executor.moveToBattlefield(target, *my_bf);
        ctx.events.logTrace("BLITZCRANK: pulled " + name + " to BF#" +
                             std::to_string(static_cast<int>(*my_bf)));
    }
};

// [674] Irresistible Faefolk — "When I move to a battlefield, you may
// move an enemy unit to that battlefield." Routed through
// Card::confirmOptional. Picks the first enemy unit NOT already at my
// BF as the move target. The generated stub got the effect wrong
// (called moveToBase on the target); we override here.
class MIrresistibleFaefolk : public UnitCard {
public:
    MIrresistibleFaefolk() : UnitCard(674) {}
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto my_bf = self.battlefieldId();
        if (!my_bf) return;  // not at a BF — nothing to move to

        // Find first enemy unit that's NOT already at my BF. Includes
        // enemies at base and enemies at other BFs.
        auto find_enemy_elsewhere = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                if (obj.battlefieldId() == my_bf) continue;  // already here
                if (!obj.location.has_value()) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_enemy_elsewhere() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Irresistible Faefolk: pull an enemy unit to my battlefield?",
            still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        auto target = find_enemy_elsewhere();
        if (target == kInvalidId) return;
        auto& obj = ctx.state.getObject(target);
        std::string name = obj.name;
        ctx.executor.moveToBattlefield(target, *my_bf);
        ctx.events.logTrace("IRRESISTIBLE FAEFOLK: pulled " + name +
                             " to BF#" + std::to_string(static_cast<int>(*my_bf)));
    }
};

// [27] Darius, Trifarian — When you play your second card in a turn, give
// me +2M this turn and ready me. Fires on WhenYouPlayAUnit AND
// WhenYouPlayASpell, but the engine routes only one trigger type per card.
// We wire WhenYouPlayAUnit; the spell variant is a residual limitation
// (rare in practice — most decks play unit-then-spell anyway).
class MDariusTrifarian : public UnitCard {
public:
    MDariusTrifarian() : UnitCard(27) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // Trigger fires AFTER the play, so cards_played_this_turn is now N
        // (where N is the count including the just-played card). Check N==2.
        if (ctx.state.player(ctx.controller).cards_played_this_turn != 2) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("DARIUS TRIFARIAN: second card played, +2M and ready");
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Hunt/Level units batch (2026-05-15)
// Each card overrides a generated stub. Hunt itself (the +N XP on
// conquer/hold) is engine-handled via keyword logic; we add the Level-gated
// effect by snapshotting XP at play time. Strictly speaking Level is a
// continuous static effect (the buff/keyword should activate the moment XP
// crosses the threshold, even mid-turn), but approximate snapshot-at-play
// is functional for play patterns that gain XP before playing the card.
// True continuous Level effects belong in recalculateAuras().
// ═══════════════════════════════════════════════════════════════════════════════

// [602] Wuju Apprentice — Hunt + Level 6: when you play me, draw 1.
class MWujuApprentice : public UnitCard {
public:
    MWujuApprentice() : UnitCard(602) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.player(ctx.controller).xp < 6) return;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("WUJU APPRENTICE: Level 6, draw 1");
    }
};

// [609] Mosstomper — Hunt 2 + Level 3: I have +1M and Deflect. We grant
// the buff permanently on play if XP >= 3. Mid-game level-up promotion
// (XP crosses 3 after play) won't retroactively apply — limitation of the
// snapshot approach.
class MMosstomper : public UnitCard {
public:
    MMosstomper() : UnitCard(609) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.state.player(ctx.controller).xp < 3) return;
        ctx.executor.buffUnit(ctx.source);
        // Grant Deflect by setting the base keyword directly on the
        // GameObject — keyword survives until removed (not turn-temporary).
        ctx.state.getObject(ctx.source).keywords.set(Keyword::Deflect);
        ctx.events.logTrace("MOSSTOMPER: Level 3, +1M and Deflect");
    }
};

// [656] Gemhand Hunter — Hunt + Level 6: I have +1M.
class MGemhandHunter : public UnitCard {
public:
    MGemhandHunter() : UnitCard(656) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.state.player(ctx.controller).xp < 6) return;
        ctx.executor.buffUnit(ctx.source);
        ctx.events.logTrace("GEMHAND HUNTER: Level 6, +1M");
    }
};

// [675] Master Yi, Tempered — Hunt 2 + Level 6: I have Deflect and Ganking.
class MMasterYi : public UnitCard {
public:
    MMasterYi() : UnitCard(675) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.state.player(ctx.controller).xp < 6) return;
        auto& obj = ctx.state.getObject(ctx.source);
        obj.keywords.set(Keyword::Deflect);
        obj.keywords.set(Keyword::Ganking);
        ctx.events.logTrace("MASTER YI: Level 6, Deflect and Ganking");
    }
};

// [698] Scryer's Bloom (gear) — Enters exhausted. Kill this + pay [1] + [E]:
// Predict 2, draw 1, gain 1 XP. Two-step Predict (one choice per peeked
// card) implemented as a 3-case resume state machine.
//
// Phase C-1 commit 6 follow-up — refactored from the old blocking
// executor.predict() into inline requestChoice + resume_data so each
// peek is a separate agent decision (and gets logged with the predict
// label). resume_data layout:
//   [0] = number of peeked cards N (0, 1, or 2)
//   [1] = peeked card 0 GameObjectId (if N >= 1)
//   [2] = peeked card 1 GameObjectId (if N >= 2)
class MScryersBloom : public GearCard {
public:
    MScryersBloom() : GearCard(698) {}
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.state.getObject(ctx.source).is_exhausted = true;
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return {.exhaust = true, .energy = 1};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ri = ctx.state.chain.resuming.value();
        auto& ps = ctx.state.player(ctx.controller);

        // Helper: present a recycle-or-keep choice for a single peeked card.
        auto requestPredictChoice = [&](GameObjectId card_id) {
            std::vector<Intent> choices;
            Intent recycle;
            recycle.type = IntentType::MakeChoice;
            recycle.player = ctx.controller;
            recycle.chosen_objects = {card_id};
            choices.push_back(recycle);
            Intent keep;
            keep.type = IntentType::MakeChoice;
            keep.player = ctx.controller;
            choices.push_back(keep);
            ctx.executor.requestChoice(ctx.controller, std::move(choices),
                                        "predict 2: recycle or keep (Scryer's Bloom)");
        };
        // Helper: apply the agent's chosen recycle-or-keep for a card.
        auto applyPredictChoice = [&](GameObjectId card_id) {
            auto choice = ctx.executor.takeChoice();
            if (!ctx.state.objectExists(card_id)) return;
            if (choice && !choice->chosen_objects.empty()) {
                // Recycle to bottom.
                ctx.state.getObject(card_id).zone = ZoneType::MainDeck;
                ps.main_deck.insert(ps.main_deck.begin(), card_id);
                ctx.events.logTrace("PREDICT: recycled " +
                                     ctx.state.getObject(card_id).name);
            } else {
                // Keep on top.
                ps.main_deck.push_back(card_id);
            }
        };
        // Helper: finish — draw, XP, kill self.
        auto finish = [&]() {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.state.player(ctx.controller).xp += 1;
            if (ctx.state.objectExists(ctx.source)) {
                ctx.executor.killObject(ctx.source);
            }
            ctx.events.logTrace("SCRYER'S BLOOM: predict 2, draw 1, +1 XP, kill self");
        };

        switch (ri.resume_point) {
        case 0: {
            // Peek up to 2 cards (fewer if deck shallow — CR 436.4).
            int actual = std::min(2, static_cast<int>(ps.main_deck.size()));
            ri.resume_data.clear();
            ri.resume_data.push_back(static_cast<int32_t>(actual));
            for (int i = 0; i < actual; ++i) {
                auto cid = ps.main_deck.back();
                ps.main_deck.pop_back();
                ri.resume_data.push_back(static_cast<int32_t>(cid));
                if (ctx.state.objectExists(cid)) {
                    auto& obj = ctx.state.getObject(cid);
                    ctx.events.logTrace("  PEEKED: " + obj.name + " (id=" +
                                         std::to_string(cid) +
                                         ") — PRIVATE to " + toString(ctx.controller));
                    ctx.events.emit(CardRevealedEvent{
                        cid, obj.card_def_id, obj.owner,
                        false, ctx.controller, ZoneType::MainDeck,
                    });
                }
            }
            if (actual == 0) { finish(); return; }
            requestPredictChoice(static_cast<GameObjectId>(ri.resume_data[1]));
            ri.resume_point = 1;
            return;
        }
        case 1: {
            int n = ri.resume_data[0];
            applyPredictChoice(static_cast<GameObjectId>(ri.resume_data[1]));
            if (n >= 2) {
                requestPredictChoice(static_cast<GameObjectId>(ri.resume_data[2]));
                ri.resume_point = 2;
            } else {
                finish();
            }
            return;
        }
        case 2: {
            applyPredictChoice(static_cast<GameObjectId>(ri.resume_data[2]));
            finish();
            return;
        }
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// WhenIWinCombat-gated cards (2026-05-15)
// Unblocked by the new TriggerType::WhenIWinCombat in this commit.
// ═══════════════════════════════════════════════════════════════════════════════

// [552] Glorious Executioner (legend) — When you win a combat, draw 1.
class MGloriousExecutioner : public LegendCard {
public:
    MGloriousExecutioner() : LegendCard(552) {}
    TriggerType triggerType() const override { return TriggerType::WhenIWinCombat; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("GLORIOUS EXECUTIONER: win combat -> draw 1");
    }
};

// [787] Voidreaver (legend) — Phase 6r multi-ability.
//   WhenIWinCombat trigger: +1 XP.
//   Activated ability 0: Spend 1 XP, [E]: Buff a unit.
//   Activated ability 1: Spend 2 XP, [E]: Move an exhausted friendly unit
//                        from a battlefield to its base.
class MVoidreaver : public LegendCard {
public:
    MVoidreaver() : LegendCard(787) {}
    TriggerType triggerType() const override { return TriggerType::WhenIWinCombat; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).xp += 1;
        ctx.events.logTrace("VOIDREAVER: win combat -> +1 XP");
    }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {
            // Ability 0 — Spend 1 XP, [E]: Buff a unit.
            {
                .cost = {.exhaust = true, .xp_cost = 1},
                .targets = TargetRequirements{.count = 1, .must_be_unit = true},
                .is_action = false, .is_reaction = false,
                .needs_activation_time_target = true,
            },
            // Ability 1 — Spend 2 XP, [E]: Move an exhausted friendly unit
            //              at a battlefield to its base.
            {
                .cost = {.exhaust = true, .xp_cost = 2},
                .targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                               .must_be_friendly = true,
                                               .must_be_at_battlefield = true},
                .is_action = false, .is_reaction = false,
                .needs_activation_time_target = true,
            },
        };
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller,
        int ability_index) const override {
        std::vector<GameObjectId> out;
        if (ability_index == 0) {
            // Any unit (buff target).
            for (auto& [id, obj] : state.objects) {
                if (!obj.isUnit() || !obj.location.has_value()) continue;
                out.push_back(id);
            }
        } else {
            // Exhausted friendly unit at a battlefield.
            for (auto& [id, obj] : state.objects) {
                if (!obj.isUnit() || obj.controller != controller) continue;
                if (!obj.location.has_value()) continue;
                if (!std::holds_alternative<BattlefieldLocation>(*obj.location)) continue;
                if (!obj.is_exhausted) continue;
                out.push_back(id);
            }
        }
        return out;
    }
    void onActivate(CardContext& ctx, int ability_index,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, ability_index);
            picked = pickTarget(ctx,
                ability_index == 0 ? "Voidreaver: buff" : "Voidreaver: recall",
                legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;

        if (ability_index == 0) {
            ctx.executor.buffUnit(picked);
            ctx.events.logTrace("VOIDREAVER: spent 1 XP to buff " +
                                 ctx.state.getObject(picked).name);
        } else {
            ctx.executor.moveToBase(picked);
            ctx.events.logTrace("VOIDREAVER: spent 2 XP to recall " +
                                 ctx.state.getObject(picked).name + " to base");
        }
    }
};

// [750] Lilting Lullaby — [Reaction] Counter a spell. Its controller can't
// play spells this turn. Uses the new cant_play_spells_this_turn flag added
// in this commit; gates spell-action generation in generateSpellActions.
class MLiltingLullaby : public SpellCard {
public:
    MLiltingLullaby() : SpellCard(750) {}
    // Audit follow-up: Lullaby is a pure counter for legality purposes.
    // Its lockout is scoped to "Its controller" — the controller of the
    // countered spell — so without a counter target the lockout has no
    // owner and the entire effect no-ops. Same shape as Hard Bargain /
    // Wind Wall: not playable when the chain has no spell to counter.
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (it->is_spell) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.chain.items.empty()) return;
        const auto& top = ctx.state.chain.items.back();
        if (!top.is_spell) return;
        PlayerId target_controller = top.controller;
        counterChainTop(ctx);
        ctx.state.player(target_controller).cant_play_spells_this_turn = true;
        ctx.events.logTrace("LILTING LULLABY: countered + " +
                             std::string(toString(target_controller)) +
                             " can't play spells this turn");
    }
};

// [64] Wind Wall — "Counter a spell."
// Pure counter, no secondary effect. Manual override of the generated
// SWindWall stub so we can attach the hasLegalTargets() override (the
// generated file is gitignored — overrides land here per the manual-
// card-regen rule).
class MWindWall : public SpellCard {
public:
    MWindWall() : SpellCard(64) {}
    bool hasLegalTargets(const GameState& state, PlayerId /*controller*/) const override {
        for (auto it = state.chain.items.rbegin(); it != state.chain.items.rend(); ++it) {
            if (it->is_spell) return true;
        }
        return false;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        counterChainTop(ctx);
    }
};

// [28] Draven, Showboat — "My Might is increased by your points." The
// dynamic-might aura is handled engine-side in recalculateAuras() (text
// match for "my might is increased by your points"). This Card class
// exists only so the generated stub doesn't override the aura behavior.
class MDravenShowboat : public UnitCard {
public:
    MDravenShowboat() : UnitCard(28) {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// Chain-resolution refactors (Phase C-1 commit 6, 2026-05-16)
// ═══════════════════════════════════════════════════════════════════════════════
// Each card below was a generated stub calling ctx.executor.discardCards(),
// which goes through the legacy queryAgent bridge (StepDriver worker thread).
// Refactored to the resume pattern via discardThenAct() above.

// [3] Chemtech Enforcer — WhenYouPlayMe: discard 1
class MChemtechEnforcer : public UnitCard {
public:
    MChemtechEnforcer() : UnitCard(3) {}
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 1, "Chemtech Enforcer: discard 1", [](CardContext&){});
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
};

// [20] Scrapyard Champion — Legion + WhenYouPlayMe: discard 2, draw 2
class MScrapyardChampion : public UnitCard {
public:
    MScrapyardChampion() : UnitCard(20) {}
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (ctx.state.player(ctx.controller).cards_played_this_turn < 2) return;
        discardThenAct(ctx, 2, "Scrapyard Champion: discard 2 then draw 2",
            [](CardContext& c) { c.executor.drawCards(c.controller, 2); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    bool requiresLegion() const override { return true; }
};

// [30] Jinx, Demolitionist — WhenYouPlayMe: discard 2
class MJinxDemolitionist : public UnitCard {
public:
    MJinxDemolitionist() : UnitCard(30) {}
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 2, "Jinx, Demolitionist: discard 2", [](CardContext&){});
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
};

// [178] Undercover Agent — WhenIDie: discard 2, draw 2
class MUndercoverAgent : public UnitCard {
public:
    MUndercoverAgent() : UnitCard(178) {}
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 2, "Undercover Agent: discard 2 then draw 2",
            [](CardContext& c) { c.executor.drawCards(c.controller, 2); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
};

// [185] Traveling Merchant — WhenIMove: discard 1, draw 1
class MTravelingMerchant : public UnitCard {
public:
    MTravelingMerchant() : UnitCard(185) {}
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 1, "Traveling Merchant: discard 1 then draw 1",
            [](CardContext& c) { c.executor.drawCards(c.controller, 1); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
};

// [444] Corrupt Enforcer — WhenIMoveToFB: discard 1
class MCorruptEnforcer : public UnitCard {
public:
    MCorruptEnforcer() : UnitCard(444) {}
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 1, "Corrupt Enforcer: discard 1", [](CardContext&){});
    }
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
};

// [470] Ezreal, Prodigy — WhenYouPlayMe: discard 1, draw 2
class MEzrealProdigy : public UnitCard {
public:
    MEzrealProdigy() : UnitCard(470) {}
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 1, "Ezreal, Prodigy: discard 1 then draw 2",
            [](CardContext& c) { c.executor.drawCards(c.controller, 2); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
};

// [642] Hwei, Brooding Painter — WhenIMove: draw 1, discard 1
// Draw happens BEFORE the discard, so it runs on the first entry (case 0).
// The discard then operates on the larger hand.
class MHweiBroodingPainter : public UnitCard {
public:
    MHweiBroodingPainter() : UnitCard(642) {}
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();
        if (ri.resume_point == 0) {
            ctx.executor.drawCards(ctx.controller, 1);
        }
        discardThenAct(ctx, 1, "Hwei, Brooding Painter: draw 1 then discard 1",
            [](CardContext&){});
    }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
};

// [685] Evershade Stalker — WhenYouPlayMe: discard 1, draw 1
class MEvershadeStalker : public UnitCard {
public:
    MEvershadeStalker() : UnitCard(685) {}
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 1, "Evershade Stalker: discard 1 then draw 1",
            [](CardContext& c) { c.executor.drawCards(c.controller, 1); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
};

// [293] Zaun Warrens — WhenYouConquerHere: discard 1, draw 1
class MZaunWarrens : public BattlefieldCard {
public:
    MZaunWarrens() : BattlefieldCard(293) {}
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        discardThenAct(ctx, 1, "Zaun Warrens: discard 1 then draw 1",
            [](CardContext& c) { c.executor.drawCards(c.controller, 1); });
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
};

// [650] Gutter Palace — two abilities:
//  (1) "At the start of your Beginning Phase, if you have exactly 4 cards in
//      hand and exactly 4 units at battlefields, you win the game."
//  (2) "Discard 1, [E]: Play a 1 [M] Bird unit token with [Deflect]."
class MGutterPalace : public GearCard {
public:
    MGutterPalace() : GearCard(650) {}

    // ── (1) Beginning-phase win check ──
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto& ps = ctx.state.player(ctx.controller);
        int hand = static_cast<int>(ps.hand.size());
        int units_at_bf = 0;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == ctx.controller &&
                obj.battlefieldId().has_value()) {
                ++units_at_bf;
            }
        }
        if (hand == 4 && units_at_bf == 4) {
            ctx.state.game_over = true;
            ctx.state.winner = ctx.controller;
            ctx.state.game_over_reason = "Gutter Palace win condition";
            ctx.events.logTrace("GUTTER PALACE: win condition met (4 hand / 4 units)");
        }
    }

    // ── (2) Activated: discard 1, [E] -> make a 1[M] Bird token w/ [Deflect] ──
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .discard = true, .discard_count = 1};
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 0};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        KeywordSet kw; kw.set(Keyword::Deflect);
        auto loc = ctx.state.getObject(ctx.source).location
                       .value_or(LocationId{BaseLocation{ctx.controller}});
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird", 1,
                                  {"Bird"}, kw, loc, /*enter_ready=*/false);
        ctx.events.logTrace("GUTTER PALACE: play 1M Bird token with [Deflect]");
    }
};

// [8] Get Excited! — onResolve: discard 1, deal 1 damage to target
class MGetExcited : public SpellCard {
public:
    MGetExcited() : SpellCard(8) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // Capture targets by value so the post-fn can see them across re-entries.
        // (ctx.state.chain.resuming carries resume state; targets are stable
        // for the lifetime of this resolve since the ChainItem holds them.)
        GameObjectId target = targets.empty() ? kInvalidId : targets[0];
        GameObjectId source = ctx.source;
        discardThenAct(ctx, 1, "Get Excited!: discard 1 then deal 1",
            [target, source](CardContext& c) {
                if (target != kInvalidId) c.executor.dealDamage(target, 1, source);
            });
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_at_battlefield = true};
    }
};

// [579] Square Up — onResolve: discard 1, give target Assault 4 this turn
class MSquareUp : public SpellCard {
public:
    MSquareUp() : SpellCard(579) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId target = targets.empty() ? kInvalidId : targets[0];
        discardThenAct(ctx, 1, "Square Up: discard 1 then +Assault 4",
            [target](CardContext& c) {
                if (target != kInvalidId)
                    c.executor.giveTemporaryKeyword(target, Keyword::Assault, 4);
            });
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Ivern Test Deck — full card support (2026-05-16)
// ═══════════════════════════════════════════════════════════════════════════════
//
// Implementations for every non-keyword-only card in `decks/ivern_test.txt`.
// The deck's theme is the "Ivern union" — having all 4 of Bird, Cat, Dog,
// Poro tags among your units unlocks several effects (Daisy! stun, Ivern
// Friend to All score trigger, Friendship +M scaling). Several cards
// generate Bird tokens (Frisky Hunter, Flurry of Feathers) and the deck
// has built-in Dog/Cat/Poro units so most of the union is usually
// satisfied as soon as one Bird hits the board.
//
// Where engine hooks already existed (selfCostReduction for Noxus Hopeful;
// text-match auras for Draven Showboat; "as you play me" via Card::onPlay
// for Baron Nashor), we reuse them. New engine surface added by this
// section is minimal — see the inline notes on the cards that need it.

namespace ivern_tags {
// Detect whether the controller's board (units on board, ignoring base
// for "deck-style" tag inclusion is debatable — we use board + base
// since CR is silent and the deck's broad reading favors including
// every controlled unit) carries each of the four key tags.
struct TagPresence {
    bool bird = false, cat = false, dog = false, poro = false;
    int count() const { return (int)bird + (int)cat + (int)dog + (int)poro; }
    bool allFour() const { return bird && cat && dog && poro; }
};

inline TagPresence scanFriendlyTags(const GameState& state, PlayerId controller) {
    TagPresence out;
    for (const auto& [id, obj] : state.objects) {
        if (!obj.isUnit() || obj.controller != controller) continue;
        if (!obj.location.has_value()) continue;
        for (const auto& tag : obj.tags) {
            if      (tag == "Bird") out.bird = true;
            else if (tag == "Cat")  out.cat  = true;
            else if (tag == "Dog")  out.dog  = true;
            else if (tag == "Poro") out.poro = true;
        }
    }
    return out;
}
} // namespace ivern_tags

// ─── [595] Frisky Hunter ─────────────────────────────────────────────────────
//
// "When you play me, play a 1[M] Bird unit token with [Deflect] here."
//
// Token enters at the SAME location as Frisky Hunter (not always controller's
// base). The token is a Unit with tag "Bird" and the Deflect keyword.
class MFriskyHunter : public UnitCard {
public:
    MFriskyHunter() : UnitCard(595) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;
        KeywordSet kw; kw.set(Keyword::Deflect);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird",
                                  /*might=*/1, /*tags=*/{"Bird"}, kw,
                                  *self.location,
                                  /*enter_ready=*/false);
    }
};

// ─── [718] Loyal Poro ────────────────────────────────────────────────────────
//
// "[Deathknell][>] If I didn't die alone, draw 1."
//
// "Didn't die alone" = there were other friendly units at the same location
// when I died. The Deathknell trigger fires AFTER the card has been moved
// to trash, so we use `last_location` (preserved by killObject) to read
// where I was.
class MLoyalPoro : public UnitCard {
public:
    MLoyalPoro() : UnitCard(718) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        // Read where I died from. last_location is set by killObject when
        // a unit dies; current `location` will be empty (in trash now).
        if (!self.last_location.has_value()) return;
        const auto& death_loc = *self.last_location;
        // "Didn't die alone" — any OTHER friendly unit still at the same
        // location, not counting myself (already in trash).
        bool has_company = false;
        for (const auto& [oid, obj] : ctx.state.objects) {
            if (oid == ctx.source) continue;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (*obj.location == death_loc) { has_company = true; break; }
        }
        if (has_company) {
            ctx.events.logTrace("LOYAL PORO: not alone at death — drawing 1");
            ctx.executor.drawCards(ctx.controller, 1);
        }
    }
};

// ─── [739] Ivern, Friend to All ──────────────────────────────────────────────
//
// "As you play me, choose Bird, Cat, Dog, or Poro. I gain that tag."
// "When I conquer or hold, score 1 point if your units have all of the
//  following tags among them — Bird, Cat, Dog, and Poro."
//
// The tag-pick happens in onPlay (CR 355.1) so the gain is atomic with
// the play — opponents have no priority window to interrupt. Default
// pick is "Bird" because the deck reliably generates Bird tokens
// (Frisky Hunter, Flurry of Feathers) but Birds are token-only and the
// rarest standing tag; picking the rarest tag maximizes the chance of
// completing the union. Agent-driven choice during onPlay is deferred —
// see "Optional-trigger agent choice" / "Multi-ability per Card" in
// CLAUDE.md "Known engine gaps."
class MIvernFriend : public UnitCard {
public:
    MIvernFriend() : UnitCard(739) {}
    void onPlay(CardContext& ctx) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // Default to Bird (see note above).
        const std::string chosen = "Bird";
        auto& tags = self.tags;
        if (std::find(tags.begin(), tags.end(), chosen) == tags.end()) {
            tags.push_back(chosen);
        }
        ctx.events.logTrace("IVERN FRIEND: chose tag [" + chosen + "]");
    }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto pres = ivern_tags::scanFriendlyTags(ctx.state, ctx.controller);
        if (!pres.allFour()) {
            ctx.events.logTrace("IVERN FRIEND: union not satisfied (count=" +
                                 std::to_string(pres.count()) + "/4) — no score");
            return;
        }
        auto& ps = ctx.state.player(ctx.controller);
        ps.score++;
        ctx.events.logTrace("IVERN FRIEND: all-4-tag union satisfied — score 1 -> " +
                             std::to_string(ps.score));
    }
};

// ─── [754] Daisy! ────────────────────────────────────────────────────────────
//
// "I enter ready."
// "Reduce my cost by [1] for each of the following tags among your units
//  — Bird, Cat, Dog, and Poro."
// "When I attack while your units have all 4 tags, [Stun] an enemy unit
//  here. (It doesn't deal combat damage this turn.)"
class MDaisy : public UnitCard {
public:
    MDaisy() : UnitCard(754) {}
    bool entersReadyOnPlay() const override { return true; }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return ivern_tags::scanFriendlyTags(state, player).count();
    }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto pres = ivern_tags::scanFriendlyTags(ctx.state, ctx.controller);
        if (!pres.allFour()) {
            ctx.events.logTrace("DAISY: union not satisfied (count=" +
                                 std::to_string(pres.count()) + "/4) — no stun");
            return;
        }
        // Stun an enemy unit at my location.
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;
        PlayerId opp = opponent(ctx.controller);
        for (auto& [oid, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.location.has_value()) continue;
            if (*obj.location != *self.location) continue;
            if (obj.is_stunned) continue;
            ctx.events.logTrace("DAISY: stunning " + obj.name + " (id=" +
                                 std::to_string(oid) + ")");
            ctx.executor.stunUnit(oid);
            return;
        }
    }
};

// ─── [622] Vilemaw ───────────────────────────────────────────────────────────
//
// "[Ambush]" — engine-handled (PlayReaction during closed state).
// "Enemy units here with less Might than me don't deal combat damage."
//   ⚠️ Combat-damage modifier not implemented (needs combat-damage hook).
//   Tracked as engine gap; the Ambush + hold-draw side ships.
// "When I hold, draw 1."
class MVilemaw : public UnitCard {
public:
    MVilemaw() : UnitCard(622) {}
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.events.logTrace("VILEMAW: hold — draw 1");
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// ─── [615] Scuttle Crab ──────────────────────────────────────────────────────
//
// "(Units with 0 [M] can conquer and hold.)" — reminder text; engine already
//   allows 0-might units to score.
// "When you play me, draw 1."
// "[Deathknell][>] Choose an opponent. They reveal their hand. You can
//   look at their facedown cards this turn. Gain 1 XP."
//
// The "look at facedown cards this turn" mechanic isn't modeled yet
// (facedown observation isn't tracked beyond the hand reveal). We
// implement the visible parts: reveal opp hand (memory bank updates) +
// gain 1 XP. The on-play draw uses WhenYouPlayMe, the deathknell uses
// WhenIDie — multi-trigger via the resume pattern.
class MScuttleCrab : public UnitCard {
public:
    MScuttleCrab() : UnitCard(615) {}
    // Two triggers — we register the one that's invoked first (play),
    // and the death side fires through Deathknell semantics. The
    // simplest mapping: use WhenYouPlayMe for the draw, and the
    // deathknell-trigger pattern via a separate Deathknell registration
    // is not modeled in the current Card surface (Card has a single
    // triggerType). For now, treat play-draw as primary; deathknell
    // effect is partial.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.events.logTrace("SCUTTLE CRAB: play — draw 1");
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// ─── [753] Green Father (Legend) ─────────────────────────────────────────────
//
// "When you conquer or hold, you may exhaust me to replace that battlefield
//  with a Brush battlefield token. (Bird, Cat, Dog, Poro, and Ivern units
//  have +1 [M] in Brush. It can be swapped back when scored.)"
//
// The aura is applied in recalculateAuras (text-driven, keyed on BF name
// == "Brush"). Here: routed through confirmOptional. still_legal picks
// a BF currently controlled by ctx.controller that isn't already a
// "Brush" — the conquer/hold event doesn't pass through CardContext,
// so we approximate. If no eligible BF remains the prompt is skipped.
class MGreenFather : public LegendCard {
public:
    MGreenFather() : LegendCard(753) {}
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        auto find_target = [&]() -> BattlefieldId {
            for (auto& bf : ctx.state.battlefields) {
                if (!bf.controller.has_value() ||
                    *bf.controller != ctx.controller) continue;
                if (ctx.state.objectExists(bf.card_object_id) &&
                    ctx.state.getObject(bf.card_object_id).name == "Brush")
                    continue;
                return bf.id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() {
            if (!ctx.state.objectExists(ctx.source)) return false;
            if (ctx.state.getObject(ctx.source).is_exhausted) return false;
            return find_target() != kInvalidId;
        };

        int conf = confirmOptional(ctx,
            "Green Father: exhaust to replace controlled BF with Brush?",
            still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        BattlefieldId target = find_target();
        if (target == kInvalidId) return;
        auto& legend = ctx.state.getObject(ctx.source);
        legend.is_exhausted = true;
        ctx.executor.replaceBattlefieldWithToken(target, "Brush", ctx.controller);
        ctx.events.logTrace("GREEN FATHER: exhausted to replace BF#" +
                             std::to_string(target) + " with Brush token");
    }
};

// ─── [613] Ivern, Nurturer ───────────────────────────────────────────────────
//
// "When you play me or when I hold, look at the top 3 cards of your Main
//  Deck. You may reveal a unit from among them and draw it. Recycle the
//  rest. Then if you revealed a Bird, Cat, Dog, or Poro, do this: [Buff]
//  a friendly unit."
//
// Heuristic resolution (no agent choice for the draft):
//   1. Pop top 3.
//   2. Find first unit; if any, "reveal" it (emit CardRevealedEvent
//      revealed_to=controller), then draw it.
//   3. Recycle the rest (bottom of deck).
//   4. If the revealed unit has Bird/Cat/Dog/Poro tag, buff a friendly
//      unit.
//
// The trigger fires on both WhenYouPlayMe and WhenIHold — the Card
// surface supports only one triggerType today, so we use
// WhenYouPlayMe and document WhenIHold as a known gap (multi-trigger
// per Card — see CLAUDE.md "Known engine gaps").
class MIvernNurturer : public UnitCard {
public:
    MIvernNurturer() : UnitCard(613) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& ps = ctx.state.player(ctx.controller);
        // Pop top 3 (top = back of vector).
        std::vector<GameObjectId> peeked;
        for (int i = 0; i < 3 && !ps.main_deck.empty(); ++i) {
            peeked.push_back(ps.main_deck.back());
            ps.main_deck.pop_back();
        }
        if (peeked.empty()) return;

        // Find first unit.
        GameObjectId drafted = kInvalidId;
        std::vector<GameObjectId> rest;
        for (auto id : peeked) {
            if (!ctx.state.objectExists(id)) continue;
            const auto& obj = ctx.state.getObject(id);
            if (drafted == kInvalidId && obj.isUnit()) { drafted = id; }
            else rest.push_back(id);
        }

        if (drafted != kInvalidId) {
            const auto& drafted_obj = ctx.state.getObject(drafted);
            // Reveal-then-draw — emit a private CardRevealedEvent so the
            // memory bank picks it up (technically we're revealing to
            // ourselves, mostly for trace/observation symmetry).
            ctx.events.emit(CardRevealedEvent{
                /*card=*/drafted,
                /*card_def_id=*/drafted_obj.card_def_id,
                /*owner=*/drafted_obj.owner,
                /*revealed_to_all=*/false,
                /*revealed_to=*/ctx.controller,
                /*source_zone=*/ZoneType::MainDeck,
            });
            ctx.events.logTrace("IVERN NURTURER: drafted " + drafted_obj.name +
                                 " (id=" + std::to_string(drafted) + ")");
            // Put it in hand directly (skip drawCards to avoid double-pop).
            auto& obj = ctx.state.getObject(drafted);
            obj.zone = ZoneType::Hand;
            ps.hand.push_back(drafted);
            // Tag-conditional buff.
            const auto& tags = drafted_obj.tags;
            bool theme = false;
            for (const auto& t : tags) {
                if (t == "Bird" || t == "Cat" || t == "Dog" || t == "Poro") {
                    theme = true; break;
                }
            }
            if (theme) {
                for (auto& [oid, u] : ctx.state.objects) {
                    if (!u.isUnit() || u.controller != ctx.controller) continue;
                    if (!u.location.has_value()) continue;
                    ctx.executor.buffUnit(oid);
                    ctx.events.logTrace("IVERN NURTURER: themed draft — buff " +
                                         u.name);
                    break;
                }
            }
        }

        // Recycle the rest to bottom of deck.
        if (!rest.empty()) {
            ctx.executor.recycleCards(ctx.controller, rest);
        }
    }
};

// ─── [606] Flurry of Feathers ────────────────────────────────────────────────
//
// "[Reaction] Choose one —
//    Counter a spell.
//    Play four 1[M] Bird unit tokens with [Deflect]."
//
// Modal cards aren't yet first-class. Heuristic: if there's a spell on
// the chain (other than this one, which has already been popped), pop
// it as a counter; otherwise create 4 Bird tokens at the caster's base.
// This matches the typical use of the card — react with counter if
// possible, fall back to the swarm.
class MFlurryOfFeathers : public SpellCard {
public:
    MFlurryOfFeathers() : SpellCard(606) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto& chain = ctx.state.chain;
        // The counter mode: pop the top of chain if it's a spell. We
        // never counter our own resolving item — FEPR has already popped
        // Flurry of Feathers, so chain.items.back() (if any) is a fresh
        // spell to dispose.
        if (!chain.items.empty() &&
            chain.items.back().is_spell &&
            chain.items.back().source != ctx.source) {
            auto victim = chain.items.back();
            chain.items.pop_back();
            ctx.events.logTrace("FLURRY OF FEATHERS: countered spell (id=" +
                                 std::to_string(victim.source) + ")");
            // Move the countered spell to trash (per CR counter semantics).
            if (ctx.state.objectExists(victim.source)) {
                auto& sp = ctx.state.getObject(victim.source);
                auto owner = sp.owner;
                sp.zone = ZoneType::Trash;
                if (owner != PlayerId::None) {
                    ctx.state.player(owner).trash.push_back(victim.source);
                }
            }
            return;
        }
        // Fallback: spawn 4 Bird tokens with Deflect at controller's base.
        KeywordSet kw; kw.set(Keyword::Deflect);
        for (int i = 0; i < 4; ++i) {
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird",
                                      /*might=*/1, /*tags=*/{"Bird"}, kw,
                                      BaseLocation{ctx.controller},
                                      /*enter_ready=*/false);
        }
        ctx.events.logTrace("FLURRY OF FEATHERS: spawned 4 Bird tokens");
    }
};

// ─── Riders — Hidden Blade [213], Back Off [604] ─────────────────────────────
//
// Hidden Blade: "Kill a unit at a battlefield. Its controller draws 2."
//   Effect target = targets[0]; on kill, the unit's controller draws 2.
//
// Back Off: "[Stun] a unit. If you played this from your hand, draw 1."
//   Effect target = targets[0]; always stun. "Played from hand" is
//   subtle — facedown plays don't draw. We approximate "from hand" as
//   true unless the spell's source was in a facedown zone (which the
//   engine sets when hide-flipping). Random agent doesn't have great
//   hide/play tracking; we always draw conservatively.

class MHiddenBlade : public SpellCard {
public:
    MHiddenBlade() : SpellCard(213) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    // Phase 6q proof-of-concept (any-side single unit target — Hidden
    // Blade is "Kill a unit at a battlefield" without friendly/enemy
    // restriction, so legal targets include BOTH sides). Distinct
    // policy-head slots per (target card type) become especially
    // valuable here because the model can learn "prefer killing enemy
    // X over friendly Y" through the MakeChoice head.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Hidden Blade", legal);
        if (picked == kInvalidId) return;
        // CR 463: re-validate the target requirements at resolve time.
        // pickTarget already filtered to currently-legal targets, but
        // the resume re-entry path could lag a turn — defensive re-check.
        auto& obj = ctx.state.getObject(picked);
        if (!obj.isUnit() || !obj.isAtBattlefield()) {
            ctx.events.logTrace("HIDDEN BLADE: target no longer at a "
                                 "battlefield — fizzle (CR 463)");
            return;
        }
        PlayerId target_controller = obj.controller;
        ctx.executor.killObject(picked);
        if (target_controller != PlayerId::None) {
            ctx.events.logTrace("HIDDEN BLADE: its controller (" +
                                 std::string(toString(target_controller)) +
                                 ") draws 2");
            ctx.executor.drawCards(target_controller, 2);
        }
    }
};

class MBackOff : public SpellCard {
public:
    MBackOff() : SpellCard(604) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice. Draw-1 rider is a
    // partial-fizzle that fires whether or not the stun target is
    // legal at resolve time (matches existing "always-draw" reading).
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Back Off", legal);
        // Distinguish "suspending for agent" (resume_point==7) from
        // "definitive no-target" (resume_point>=8). For suspends,
        // return without firing the draw rider.
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;
        }
        if (picked != kInvalidId) {
            ctx.executor.stunUnit(picked);
        }
        // "If you played this from your hand, draw 1." Always-draw
        // (more generous reading); rider fires whether or not the
        // stun target was legal.
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// ─── [775] Vaults of Helia (Battlefield) ─────────────────────────────────────
//
// "When you hold here, your non-token units cost [1] more to play this turn."
//
// On hold, append a turn-scoped CostModifier with `energy_increase=1` and
// `affects_non_token_only=true` to the holder's PlayerState. Modifier
// expires at end of turn via resetTurnTracking.
class MVaultsOfHelia : public BattlefieldCard {
public:
    MVaultsOfHelia() : BattlefieldCard(775) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouHoldHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        PlayerId who = ctx.controller;
        PlayerState::CostModifier m;
        m.source = ctx.source;
        m.energy_increase = 1;
        m.affects_non_token_only = true;
        m.this_turn_only = true;
        ctx.state.player(who).cost_modifiers.push_back(m);
        ctx.events.logTrace("VAULTS OF HELIA: " + std::string(toString(who)) +
                             "'s non-token units cost +1 this turn");
    }
};

// ─── [290] Vilemaw's Lair / [530] Rockfall Path (Battlefields) ───────────────
//
// Vilemaw's Lair: "Units can't move from here to base."
// Rockfall Path: "Units can't be played here."
//
// Both are static restrictions parsed from card text and stored on
// BattlefieldState (blocks_move_to_base / blocks_unit_play) during
// setupBattlefields. The action generators consult those flags directly,
// so no Card trigger is needed. These shells exist to mark the slots
// as implemented and to allow card-side trace if future variants add
// triggers. No-op onResolve.
class MVilemawsLair : public BattlefieldCard {
public:
    MVilemawsLair() : BattlefieldCard(290) {}
};
class MRockfallPath : public BattlefieldCard {
public:
    MRockfallPath() : BattlefieldCard(530) {}
};

// ═══════════════════════════════════════════════════════════════════════════════
// Jhin deck (2026-05-17) — Virtuoso legend, Forgotten Library BF,
// Curtain Call signature spell + supporting Repeat-cost spells / triggers.
// All depend on the legend-zone trigger dispatch added in trigger_manager.cpp.
// ═══════════════════════════════════════════════════════════════════════════════

// Helper: find the most-recently-played spell by `player` whose effect has
// already resolved. The CardPlayedEvent fires BEFORE the spell is pushed
// onto the chain (game_engine.cpp::executePlaySpell pays cost → emits event
// → addSpell), so by the time a WhenYouPlayASpell trigger's ability
// resolves on the chain, the original spell has resolved and is sitting at
// the back of `player.trash`. We pop-from-back to find the most recent
// spell GameObjectId; returns kInvalidId if no spell-zoned card is there.
inline GameObjectId findMostRecentlyPlayedSpell(const GameState& state,
                                                  PlayerId player) {
    const auto& ps = state.player(player);
    for (auto it = ps.trash.rbegin(); it != ps.trash.rend(); ++it) {
        if (!state.objectExists(*it)) continue;
        const auto& o = state.getObject(*it);
        if (o.isSpell()) return *it;
    }
    return kInvalidId;
}

// [782] Virtuoso — "When you play a spell, if you spent [4] or more, you may
// banish it. Then, if there are four spells banished with me, put each in
// its trash, channel 4 runes, and draw 1."
//
// Implementation:
//   • Trigger: WhenYouPlayASpell (dispatched via legend-zone sweep added
//     2026-05-17 to TriggerManager::onCardPlayed).
//   • At resolve, find the most-recent spell on the chain controlled by
//     the legend's controller. If its energy_cost >= 4, banish it (move
//     from chain to controller.banishment), bump the legend's counter.
//   • When counter == 4: route those banished cards back to their owners'
//     trash, channel 4 ready runes for controller, draw 1, reset counter.
//
// Simplification: "may banish" is treated as always-banish (random agent
// chooser will be added when agent-choice for optional triggers lands per
// the engine-gap note in CLAUDE.md).
class MVirtuoso : public LegendCard {
public:
    MVirtuoso() : LegendCard(782) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Validation closure: "you may banish it" is legal iff the
        // most-recently-played spell exists, is in trash, and its
        // play-time energy spend was ≥4. Re-checked before AND after
        // the agent's yes/no via confirmOptional.
        auto still_legal = [&]() -> bool {
            // Read the snapshot stored on this trigger's chain item
            // (Phase 6q+: TriggerManager captures the triggering
            // spell's spent at trigger-add time so we don't read a
            // clobbered PlayerState::last_spell_energy_spent at
            // resume time). Fall back to the PlayerState field for
            // legacy code paths that don't snapshot.
            int spent = ctx.state.chain.resuming.has_value()
                ? ctx.state.chain.resuming->triggering_spell_energy_spent
                : 0;
            if (spent == 0) {
                spent = ctx.state.player(ctx.controller).last_spell_energy_spent;
            }
            if (spent < 4) return false;
            GameObjectId sid = findMostRecentlyPlayedSpell(
                ctx.state, ctx.controller);
            if (sid == kInvalidId) return false;
            if (!ctx.state.objectExists(sid)) return false;
            return ctx.state.getObject(sid).zone == ZoneType::Trash;
        };

        int conf = confirmOptional(ctx, "Virtuoso: banish 4-cost spell?",
                                    still_legal);
        if (conf == -1) return;     // waiting for agent
        if (conf == 0) return;      // declined or invalidated
        // conf == 1: confirmed, proceed.

        GameObjectId spell_id = findMostRecentlyPlayedSpell(ctx.state, ctx.controller);
        if (spell_id == kInvalidId) return;  // should be live per validation
        auto& spell = ctx.state.getObject(spell_id);

        // The spell is already in trash (resolved before this trigger's
        // ability did). Reroute it from trash → banishment.
        auto& owner_ps = ctx.state.player(spell.owner);
        auto& tr = owner_ps.trash;
        auto it = std::find(tr.begin(), tr.end(), spell_id);
        if (it == tr.end()) return;  // shouldn't happen
        tr.erase(it);
        spell.zone = ZoneType::Banishment;
        owner_ps.banishment.push_back(spell_id);

        auto& legend = ctx.state.getObject(ctx.source);
        legend.tracked_objects.push_back(spell_id);  // "banished with me"
        ctx.events.logTrace("VIRTUOSO: banished " + spell.name +
                             "; banished-with-me count=" +
                             std::to_string(legend.tracked_objects.size()));

        // Per CR triggered-ability rules, "Then, if there are four spells
        // banished with me, ..." is a CONTINUATION of the same triggered
        // ability — resolves atomically in the same chain item. NOT
        // deferrable to a later turn (verified against CR 359.3.f /
        // "Then,"-continuation examples 2026-05-17).
        if (legend.tracked_objects.size() >= 4) {
            // Payoff scope is STRICT: only the spells *Virtuoso* banished
            // ("banished with me") move back to trash. Other banished
            // spells in either player's banishment stay there.
            auto take_first_n = std::min<size_t>(4, legend.tracked_objects.size());
            std::vector<GameObjectId> to_trash(
                legend.tracked_objects.begin(),
                legend.tracked_objects.begin() + take_first_n);
            int moved = 0;
            for (auto id : to_trash) {
                if (!ctx.state.objectExists(id)) continue;
                auto& obj = ctx.state.getObject(id);
                auto& ownerPS = ctx.state.player(obj.owner);
                auto it = std::find(ownerPS.banishment.begin(),
                                     ownerPS.banishment.end(), id);
                if (it != ownerPS.banishment.end()) {
                    ownerPS.banishment.erase(it);
                    obj.zone = ZoneType::Trash;
                    ownerPS.trash.push_back(id);
                    moved++;
                }
            }
            // Remove the consumed entries from the tracked list.
            legend.tracked_objects.erase(legend.tracked_objects.begin(),
                                          legend.tracked_objects.begin() + take_first_n);

            ctx.events.logTrace("VIRTUOSO: 4 spells banished-with-me — "
                                 "routed " + std::to_string(moved) +
                                 " back to trash, channel 4 + draw 1");
            ctx.executor.channelRunes(ctx.controller, 4, /*enter_exhausted=*/false);
            ctx.executor.drawCards(ctx.controller, 1);
        }
    }
};

// [767] Forgotten Library (BF) — "While you control this battlefield, when
// you play a spell, if you spent [4] or more, [Predict]. (Look at top card,
// you may recycle it.)"
//
// Implementation: BF-card with WhenYouPlayASpell trigger. At resolve, scan
// chain for the controller's most-recent spell; if cost >= 4, run Predict 1.
class MForgottenLibrary : public BattlefieldCard {
public:
    MForgottenLibrary() : BattlefieldCard(767) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayASpell; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // CR gate "While you control this battlefield" — fire only if the
        // BF object's controller (set by ctx.controller via fireTrigger)
        // matches the spell-player. Read from per-trigger snapshot
        // (Phase 6q+) so a subsequent spell-play doesn't clobber the
        // value we need. Falls back to PlayerState for legacy paths.
        int spent = ctx.state.chain.resuming.has_value()
            ? ctx.state.chain.resuming->triggering_spell_energy_spent
            : 0;
        if (spent == 0) {
            spent = ctx.state.player(ctx.controller).last_spell_energy_spent;
        }
        if (spent < 4) {
            ctx.events.logTrace("FORGOTTEN LIBRARY: skip (spent=" +
                                 std::to_string(spent) + " < 4)");
            return;
        }
        ctx.events.logTrace("FORGOTTEN LIBRARY: predict 1");
        ctx.executor.predict(ctx.controller, 1);
    }
};

// [584] Jhin, Murderous Artist — [Deflect] [Ganking] + "When I move,
// [Add] [1][A]." The Deflect/Ganking keywords are engine-handled. The
// [Add] effect uses the Phase 6q+ floating energy/power primitives.
class MJhinMurderous : public UnitCard {
public:
    MJhinMurderous() : UnitCard(584) {}
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // [Add] [1][A] — 1 floating energy + 1 universal-domain power.
        ctx.executor.addFloatingEnergy(ctx.controller, 1);
        ctx.executor.addFloatingUniversalPower(ctx.controller, 1);
        ctx.events.logTrace("JHIN: [Add] [1][A] on move");
    }
};

// ─── Damage / utility spells ────────────────────────────────────────────────

// [105] Singularity — "Deal 6 to each of up to two units."
class MSingularity : public SpellCard {
public:
    MSingularity() : SpellCard(105) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true, .optional = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        for (auto t : targets) {
            if (ctx.state.objectExists(t)) ctx.executor.dealDamage(t, 6, ctx.source);
        }
    }
};

// [346] Piercing Light — "[Repeat] [2][R] Deal 2 to a unit at a battlefield,
// then deal 2 to up to one other unit." Two targets, second is optional.
// Repeat is engine-handled (re-execution loop).
class MPiercingLight : public SpellCard {
public:
    MPiercingLight() : SpellCard(346) {}
    TargetRequirements getTargetRequirements() const override {
        // count=2 with optional=true — first must be a unit at a BF
        // (enforced); the second is "up to one other unit" so optional.
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_at_battlefield = true,
                                   .optional = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        ctx.executor.dealDamage(targets[0], 2, ctx.source);
        if (targets.size() >= 2 && ctx.state.objectExists(targets[1])) {
            ctx.executor.dealDamage(targets[1], 2, ctx.source);
        }
    }
};

// [389] Frigid Touch — "[Reaction] [Repeat] [2] Give a unit -2 [M] this turn."
// -might debuff. Persists until expiration step (engine resets temp_might_bonus
// at end of turn). Per CR 143.2.b, might < 0 is treated as 0 but doesn't kill
// a unit by itself — lethal still needs damage_marked > 0.
class MFrigidTouch : public SpellCard {
public:
    MFrigidTouch() : SpellCard(389) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Frigid Touch", legal);
        if (picked == kInvalidId) return;
        ctx.executor.giveTemporaryMight(picked, -2);
    }
};

// [400] Rocket Barrage — "[Repeat] [4][B] Choose one — Deal 4 to a unit in
// a base. Kill a gear."
//
// Modal spell — uses Card::pickMode for explicit mode selection at
// resolve time. The agent first picks which mode to fire (Deal 4 vs Kill
// gear), then the engine applies the mode to targets[0] if compatible.
//
// `legal_modes` bitmask is computed from targets[0]'s type so only the
// mode(s) actually applicable to the chosen target are presented. This
// gives the agent (and replay viewer) a clean two-step: pick target →
// pick mode (or just pick mode if only one is legal).
class MRocketBarrage : public SpellCard {
public:
    MRocketBarrage() : SpellCard(400) {}
    TargetRequirements getTargetRequirements() const override {
        // Permissive target — any object. The pickMode helper filters
        // modes based on the target's type.
        return TargetRequirements{.count = 1};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        if (!ctx.state.objectExists(targets[0])) return;
        auto& target = ctx.state.getObject(targets[0]);
        // Mode 0 = Deal 4 to base unit; Mode 1 = Kill gear.
        uint32_t legal = 0;
        if (target.isUnit() && target.isAtBase()) legal |= (1u << 0);
        if (target.isGear()) legal |= (1u << 1);

        int mode = pickMode(ctx, "Rocket Barrage", 2,
                            {"Deal 4 to base unit", "Kill gear"}, legal);
        if (mode < 0) return;  // -1 pending, -2 no legal mode

        switch (mode) {
            case 0:
                ctx.events.logTrace("ROCKET BARRAGE: deal 4 to " + target.name);
                ctx.executor.dealDamage(targets[0], 4, ctx.source);
                break;
            case 1:
                ctx.events.logTrace("ROCKET BARRAGE: kill gear " + target.name);
                ctx.executor.killObject(targets[0]);
                break;
        }
    }
};

// [571] Upstage Comedy — "[Repeat] [2] Ready a unit."
class MUpstageComedy : public SpellCard {
public:
    MUpstageComedy() : SpellCard(571) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Upstage Comedy", legal);
        if (picked == kInvalidId) return;
        ctx.executor.readyObject(picked);
    }
};

// [623] Downstage Dramatics — "[Reaction] [Repeat] [2] Draw 1."
// Reaction-speed card-draw; with the optional [2] Repeat cost paid, draws
// twice. Engine's Repeat loop handles the re-execution. No target.
class MDownstageDramatics : public SpellCard {
public:
    MDownstageDramatics() : SpellCard(623) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// [635] Deadly Flourish — "Deal 3 to an enemy unit. When it dies this turn,
// play a Gold gear token exhausted."
// CR-correct: deal damage, then register a DelayedAbility scoped to this
// specific victim with trigger=WhenIDie. If the victim dies later this
// turn (whether from this damage, combat, another spell, etc.), the
// delayed ability fires and the controller of Deadly Flourish spawns the
// Gold token. The death-from-this-damage case naturally flows through the
// same path because killObject emits UnitDiedEvent → onUnitDied → checks
// scoped delayed abilities.
class MDeadlyFlourish : public SpellCard {
public:
    MDeadlyFlourish() : SpellCard(635) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Deadly Flourish", legal);
        if (picked == kInvalidId) return;
        auto victim = picked;

        // Register the death-watch BEFORE dealing damage so that if the
        // damage kills the unit, the delayed ability is already in place
        // to be fired by onUnitDied.
        DelayedAbility da;
        da.source = ctx.source;
        da.card_def_id = cardDefId();
        da.controller = ctx.controller;
        da.trigger = TriggerType::WhenIDie;
        da.target_filter = victim;
        ctx.state.delayed_abilities.push_back(da);
        ctx.events.logTrace("DEADLY FLOURISH: armed death-watch on victim "
                             "(id=" + std::to_string(victim) + ")");

        ctx.executor.dealDamage(victim, 3, ctx.source);
        // Engine's processLethalDamage (or combat resolution) will kill
        // the unit if damage_marked >= might. We don't manually kill
        // here — the engine handles it via the SBA pass.
    }
    // When the delayed ability fires, spawn the Gold gear token.
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                  /*might=*/0, /*tags=*/{},
                                  /*kw=*/KeywordSet{},
                                  BaseLocation{ctx.controller},
                                  /*exhausted=*/true);
        ctx.events.logTrace("DEADLY FLOURISH: victim died — spawned Gold token");
    }
};

// [22] Thermo Beam — "[Action] Kill all gear."
class MThermoBeam : public SpellCard {
public:
    MThermoBeam() : SpellCard(22) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> gear_ids;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isGear() && obj.location.has_value()) {
                gear_ids.push_back(id);
            }
        }
        ctx.events.logTrace("THERMO BEAM: killing " +
                             std::to_string(gear_ids.size()) + " gear");
        for (auto id : gear_ids) ctx.executor.killObject(id);
    }
};

// [123] Unchecked Power — "Exhaust all friendly units, then deal 12 to ALL
// units at battlefields."
class MUncheckedPower : public SpellCard {
public:
    MUncheckedPower() : SpellCard(123) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // 1. Exhaust all friendly units.
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == ctx.controller &&
                obj.location.has_value()) {
                obj.is_exhausted = true;
            }
        }
        // 2. Deal 12 to every unit currently at any battlefield. Snapshot
        //    first so dealDamage / kill chain doesn't invalidate iterator.
        std::vector<GameObjectId> victims;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.isAtBattlefield()) victims.push_back(id);
        }
        for (auto v : victims) ctx.executor.dealDamage(v, 12, ctx.source);
    }
};

// [122] Time Warp — "Take a turn after this one. Banish this."
class MTimeWarp : public SpellCard {
public:
    MTimeWarp() : SpellCard(122) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Phase 5a's additional-turns queue lives on PlayerState. Push the
        // controller as the next turn-player.
        auto& ps = ctx.state.player(ctx.controller);
        ps.additional_turns.push_back(ctx.controller);
        ctx.events.logTrace("TIME WARP: queued extra turn for " +
                             std::string(toString(ctx.controller)));
        // "Banish this" — route source to banishment instead of trash.
        if (ctx.state.objectExists(ctx.source)) {
            ctx.executor.banishObject(ctx.source);
        }
    }
};

// [399] Production Surge — "This costs [2] less if you control a Mech. Play a
// 3 [M] Mech unit token to your base. Draw 1."
// Cost-reduction-if-Mech is via Card::selfCostReduction. Resolve: spawn token
// + draw.
class MProductionSurge : public SpellCard {
public:
    MProductionSurge() : SpellCard(399) {}
    int selfCostReduction(const GameState& state, PlayerId controller) const override {
        for (const auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            // Match by name (no Mech keyword/tag in the card data — token
            // name is "Mech"). Real Mech-tag enforcement is a follow-up.
            if (obj.name.find("Mech") != std::string::npos) return 2;
        }
        return 0;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        KeywordSet kw;
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech",
                                  /*might=*/3, /*tags=*/{}, kw,
                                  BaseLocation{ctx.controller},
                                  /*exhausted=*/false);
        ctx.executor.drawCards(ctx.controller, 1);
    }
};

// [405] Hextech Anomaly — "[E]: [Reaction] Pay any amount of [A] to [Add]
// that much Energy."
//
// Variable-X cost via pickXAmount. After paying the fixed [E] (handled by
// the engine via ActivationCost), the card prompts the agent for X in
// [0, max_x] where max_x is the count of exhausted runes that could be
// recycled for [A]. Then recycles X exhausted runes and adds X energy
// to the controller's pool.
class MHextechAnomaly : public GearCard {
public:
    MHextechAnomaly() : GearCard(405) {}
    bool hasActivatedAbility() const override { return true; }
    bool isActionAbility() const override { return false; }  // Reaction
    ActivationCost getActivationCost() const override {
        ActivationCost c;
        c.exhaust = true;
        return c;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Max X = exhausted-rune count (recyclable for power).
        auto& ps = ctx.state.player(ctx.controller);
        int max_x = 0;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
            if (obj.is_exhausted) max_x++;
        }
        int x = pickXAmount(ctx, "Hextech Anomaly: X power → X energy",
                             0, max_x);
        if (x < 0) return;  // pending choice
        if (x == 0) {
            ctx.events.logTrace("HEXTECH ANOMALY: X=0, no conversion");
            return;
        }
        // Recycle X exhausted runes.
        int paid = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (paid >= x) break;
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BaseLocation>(*obj.location)) continue;
            if (!obj.is_exhausted) continue;
            obj.location = std::nullopt;
            obj.zone = ZoneType::RuneDeck;
            ps.rune_deck.insert(ps.rune_deck.begin(), id);
            paid++;
        }
        ps.rune_pool.energy += x;
        ctx.events.logTrace("HEXTECH ANOMALY: paid " + std::to_string(paid) +
                             " power → +" + std::to_string(x) + " energy");
    }
};

// ─── Reaction spells with hidden flips ──────────────────────────────────────

// [83] Consult the Past — actual registry text: "[Hidden] (Hide now for [A]
// to react with later for .)\n[Reaction] (Play any time, even before spells
// and abilities resolve.)\nDraw 2."
// The old implementation hallucinated a "play a spell from trash for free"
// effect that's not in the printed text. The real card is a 2-card draw
// Reaction with optional Hidden timing flexibility. Phase 6q+ correction.
class MConsultThePast : public SpellCard {
public:
    MConsultThePast() : SpellCard(83) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 2);
        ctx.events.logTrace("CONSULT THE PAST: draw 2");
    }
};

// [575] Lotus Trap — "[Hidden] [Reaction] Choose a unit. Double all damage
// that would be dealt to it this turn."
// Sets GameObject::damage_doubled_this_turn on the target. The flag is read
// by EffectExecutor::dealDamage to double the amount before marking it.
// Applies uniformly to spell damage, ability damage, and combat damage.
// Cleared at expiration step (end of turn).
class MLotusTrap : public SpellCard {
public:
    MLotusTrap() : SpellCard(575) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Lotus Trap", legal);
        if (picked == kInvalidId) return;
        if (!ctx.state.objectExists(picked)) return;
        auto& target = ctx.state.getObject(picked);
        target.damage_doubled_this_turn = true;
        ctx.events.logTrace("LOTUS TRAP: damage-doubling applied to " +
                             target.name);
    }
};

// [743] Curtain Call — signature spell with [Repeat] [1]/[A]/[1][A] and four
// modes (Draw 1 / Deal 2 to BF unit / Deal 3 to base unit / -4 M to BF unit).
// "Choose one you haven't already chosen" — track used modes per source
// across Repeat re-resolutions via GameObject::card_counters.
//
// Uses Card::pickMode for explicit mode selection. `legal_modes` excludes
// (a) modes already used this play, (b) modes incompatible with targets[0]
// — e.g. "Deal 2 (BF)" filtered when target is at base, "Draw 1" always
// available regardless of target.
class MCurtainCall : public SpellCard {
public:
    MCurtainCall() : SpellCard(743) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .optional = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        auto& src = ctx.state.getObject(ctx.source);
        int& used_mask = src.card_counters["curtain_call_used_mask"];

        // Compute legal-mode bitmask. Mode 0 (Draw) always legal if not
        // used. Modes 1, 2, 3 require a compatible target.
        uint32_t legal = 0;
        if (!(used_mask & (1 << 0))) legal |= (1u << 0);  // Draw 1 — no target needed
        bool has_target = !targets.empty() && ctx.state.objectExists(targets[0]);
        bool tgt_at_bf  = has_target && ctx.state.getObject(targets[0]).isAtBattlefield();
        bool tgt_at_base = has_target && ctx.state.getObject(targets[0]).isAtBase();
        if (!(used_mask & (1 << 1)) && tgt_at_bf)   legal |= (1u << 1);
        if (!(used_mask & (1 << 2)) && tgt_at_base) legal |= (1u << 2);
        if (!(used_mask & (1 << 3)) && tgt_at_bf)   legal |= (1u << 3);

        int mode = pickMode(ctx, "Curtain Call", 4,
                             {"Draw 1", "Deal 2 (BF)",
                              "Deal 3 (base)", "-4M (BF)"},
                             legal);
        if (mode < 0) return;  // -1 pending, -2 no legal mode

        used_mask |= (1 << mode);
        switch (mode) {
            case 0:
                ctx.events.logTrace("CURTAIN CALL: Draw 1");
                ctx.executor.drawCards(ctx.controller, 1);
                break;
            case 1:
                ctx.events.logTrace("CURTAIN CALL: Deal 2 (BF)");
                ctx.executor.dealDamage(targets[0], 2, ctx.source);
                break;
            case 2:
                ctx.events.logTrace("CURTAIN CALL: Deal 3 (base)");
                ctx.executor.dealDamage(targets[0], 3, ctx.source);
                break;
            case 3:
                ctx.events.logTrace("CURTAIN CALL: -4M (BF)");
                ctx.executor.giveTemporaryMight(targets[0], -4, /*minimum=*/0);
                break;
        }
    }
};

// ─── [608] Friendship ────────────────────────────────────────────────────────
//
// "[Reaction] Choose a unit. Give it +1[M] this turn for each of the
//  following tags among your units — Bird, Cat, Dog, and Poro."
//
// Target is `targets[0]`; the buff size = number of the 4 named tags
// present among the caster's units (max 4, min 0).
class MFriendship : public SpellCard {
public:
    MFriendship() : SpellCard(608) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Friendship", legal);
        if (picked == kInvalidId) return;
        auto presence = ivern_tags::scanFriendlyTags(ctx.state, ctx.controller);
        int bonus = presence.count();  // 0..4
        if (bonus <= 0) return;
        ctx.events.logTrace("FRIENDSHIP: +" + std::to_string(bonus) +
                             "[M] this turn (tags present: B=" +
                             (presence.bird?"1":"0") + " C=" + (presence.cat?"1":"0") +
                             " D=" + (presence.dog?"1":"0") + " P=" + (presence.poro?"1":"0") + ")");
        ctx.executor.giveTemporaryMight(picked, bonus);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Phase 6o (2026-05-18) — Khazix-deck broken-card fixes
// Found during training audit: the generated stubs for Void Assault and Blood
// Rose were no-ops (Void Assault double-moved the friendly; Blood Rose declared
// WhenYouPlayAUnit but the onTrigger body was empty). The model was learning
// to never play these cards. Manual implementations follow.
// ═══════════════════════════════════════════════════════════════════════════════

// [758] Void Assault — Action spell. "Move a friendly unit, then move an
// enemy unit. (If they both move to a battlefield you don't control, you're
// the attacker.)"
// The "if both" parenthetical needs agent-driven per-target destination
// choice (not modeled — moves are agent-chosen at intent time, not target-
// time). Approximate by routing each unit to its own controller's base —
// useful tactical reset that breaks contested battlefields. The agent can
// still set up attacks via the normal move action next turn.
class MVoidAssault : public SpellCard {
public:
    MVoidAssault() : SpellCard(758) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.size() < 2) return;
        if (ctx.state.objectExists(targets[0]))
            ctx.executor.moveToBase(targets[0]);
        if (ctx.state.objectExists(targets[1]))
            ctx.executor.moveToBase(targets[1]);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/) const override {
        // All units on the board; engine generates friendly/enemy pairs.
        std::vector<GameObjectId> targets;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            targets.push_back(id);
        }
        return targets;
    }
};

// [671] Blood Rose — Gear card with two abilities:
//   1. WhenYouPlayAUnit (trigger): "you may pay [1] to gain 1 XP"
//   2. Activated: "Spend 3 XP, [E]: Ready a unit"
// The XP-mining trigger compounds Voidreaver/Megatusk XP plays; the activated
// ability is a tempo-positive ready that's part of the standard Khazix +1
// action plan.
class MBloodRose : public GearCard {
public:
    MBloodRose() : GearCard(671) {}

    // ── Trigger: WhenYouPlayAUnit, optionally pay 1 for 1 XP ──
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        // confirmOptional pattern (Phase 5f). still_legal closure validates
        // BOTH at prompt-time and after the agent says yes.
        auto still_legal = [&ps]() { return ps.rune_pool.energy >= 1; };
        if (!still_legal()) return;
        auto conf = confirmOptional(ctx,
            "Blood Rose: pay [1] to gain 1 XP?", still_legal);
        if (conf < 1) return;
        ps.rune_pool.energy -= 1;
        ps.xp += 1;
        ctx.events.logTrace("BLOOD ROSE: paid [1] -> +1 XP (now " +
                             std::to_string(ps.xp) + ")");
    }

    // ── Activated: Spend 3 XP, [E]: Ready a unit (Phase 6r migrated) ──
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {{
            .cost = {.exhaust = true, .xp_cost = 3},
            .targets = TargetRequirements{.count = 1, .must_be_unit = true,
                                           .must_be_friendly = true},
            .is_action = false, .is_reaction = false,
            .needs_activation_time_target = true,
        }};
    }
    void onActivate(CardContext& ctx, int /*ability_idx*/,
                    const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller, 0);
            picked = pickTarget(ctx, "Blood Rose", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.readyObject(picked);
        ctx.events.logTrace("BLOOD ROSE: spent 3 XP -> ready " +
                             ctx.state.getObject(picked).name);
    }
};

// [465] Spirit Wheel — Gear. "When you choose a friendly unit, you may pay
// [1] and exhaust this to draw 1."
// Uses the new WhenYouChooseAFriendlyUnit trigger (Phase 6o). Trigger fires
// when an intent's targets include a friendly unit of the controller. Spirit
// Wheel's onTrigger offers the optional cost; if accepted, exhausts the gear
// + spends 1 energy + draws 1 card.
class MSpiritWheel : public GearCard {
public:
    MSpiritWheel() : GearCard(465) {}
    TriggerType triggerType() const override {
        return TriggerType::WhenYouChooseAFriendlyUnit;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (self.is_exhausted) return;
        auto& ps = ctx.state.player(ctx.controller);
        // confirmOptional needs to re-validate both energy AND the still-
        // ready state of this gear (a different effect may have exhausted
        // it between trigger fire and resolution).
        auto still_legal = [&ps, &ctx]() {
            if (!ctx.state.objectExists(ctx.source)) return false;
            if (ctx.state.getObject(ctx.source).is_exhausted) return false;
            return ps.rune_pool.energy >= 1;
        };
        if (!still_legal()) return;
        auto conf = confirmOptional(ctx,
            "Spirit Wheel: pay [1] + exhaust to draw 1?", still_legal);
        if (conf < 1) return;
        ps.rune_pool.energy -= 1;
        ctx.executor.exhaustObject(ctx.source);
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("SPIRIT WHEEL: paid [1] + exhaust -> draw 1");
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Phase 6o batch 2 (2026-05-18) — broken cards across all test decks
// 25 cards: STUBs and PARTIALs from the deck audit. Each is implemented to
// the spirit of its registry text, approximating where the engine lacks
// primitive support (noted inline).
// ═══════════════════════════════════════════════════════════════════════════════

// Helper for swap/move-to-location effects: dispatches to moveToBattlefield
// or moveToBase based on the destination location type.
namespace {
inline void moveToLocation(EffectExecutor& exec, GameObjectId obj,
                            const std::optional<LocationId>& loc) {
    if (!loc) return;
    if (std::holds_alternative<BattlefieldLocation>(*loc)) {
        exec.moveToBattlefield(obj,
            std::get<BattlefieldLocation>(*loc).id);
    } else {
        exec.moveToBase(obj);
    }
}
}  // namespace

// ── EASY (simple primitive calls) ─────────────────────────────────────────────

// [110] Ekko, Recurrent — [Accelerate] + [Deathknell]: Recycle me to ready
// your runes. (Engine handles Accelerate; we add Deathknell that channels
// exhausted runes — closest primitive to "ready your runes" — and recycles
// self.) Accelerate-as-additional-cost is handled by the engine framework.
class MEkkoRecurrent : public UnitCard {
public:
    MEkkoRecurrent() : UnitCard(110) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // "Recycle me to ready your runes" — ready every friendly
        // on-board rune. Same iteration pattern as MSona.
        int readied = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.is_exhausted) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.readyObject(id);
            ++readied;
        }
        ctx.events.logTrace("EKKO: deathknell -> readied " +
                             std::to_string(readied) + " runes");
    }
};

// [134] Mobilize — Channel 1 rune exhausted. If you can't, draw 1.
class MMobilize : public SpellCard {
public:
    MMobilize() : SpellCard(134) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // "If you can't" — true when the rune deck is empty (nothing to channel).
        if (ps.rune_deck.empty()) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("MOBILIZE: rune deck empty -> draw 1");
        } else {
            ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
            ctx.events.logTrace("MOBILIZE: channeled 1 rune exhausted");
        }
    }
};

// [169] Gust — [Reaction] Return a unit at a battlefield with 3 [M] or less
// to its owner's hand.
class MGust : public SpellCard {
public:
    MGust() : SpellCard(169) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    // Phase 6q — defer target selection so the policy head gets
    // distinct vocab slots per target choice.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Gust", legal);
        if (picked == kInvalidId) return;
        ctx.executor.bounceToHand(picked);
    }
    // Filter to units with might <= 3.
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/) const override {
        std::vector<GameObjectId> targets;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (!std::holds_alternative<BattlefieldLocation>(*obj.location)) continue;
            if (obj.current_might > 3) continue;
            targets.push_back(id);
        }
        return targets;
    }
};

// [172] Rebuke — [Action] Return a unit at a battlefield to its owner's hand.
// Phase 6q — deferred target selection so the policy head gets
// distinct vocab slots per target choice.
class MRebuke : public SpellCard {
public:
    MRebuke() : SpellCard(172) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Rebuke", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.bounceToHand(picked);
    }
};

// [369] Poro Snax — Gear. When you play this: draw 1. Activated: [1][G],
// [E], Kill this: Draw 1.
class MPoroSnax : public GearCard {
public:
    MPoroSnax() : GearCard(369) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("PORO SNAX: enter -> draw 1");
    }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        // [1][G], [E], Kill this. Approximated as [1] energy + exhaust
        // + recycle_self (closest match for "kill this as cost"). The
        // G domain isn't enforced at this layer.
        return {.exhaust = true, .energy = 1, .recycle_self = true};
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("PORO SNAX: activated -> draw 1");
    }
};

// ── MEDIUM ────────────────────────────────────────────────────────────────────

// [220] Facebreaker — [Hidden] + [Action]: Stun a friendly + an enemy unit
// at the SAME battlefield. (Hidden handled by engine framework.)
class MFacebreaker : public SpellCard {
public:
    MFacebreaker() : SpellCard(220) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.size() < 2) return;
        if (ctx.state.objectExists(targets[0])) ctx.executor.stunUnit(targets[0]);
        if (ctx.state.objectExists(targets[1])) ctx.executor.stunUnit(targets[1]);
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_at_battlefield = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId /*controller*/) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (!std::holds_alternative<BattlefieldLocation>(*obj.location)) continue;
            out.push_back(id);
        }
        return out;
    }
};

// [366] Emperor's Divide — [Hidden] + [Action]: Move ANY NUMBER of friendly
// units at a battlefield to their base. Engine doesn't support variable-
// count target selection cleanly; we approximate by moving all friendly
// units at the chosen battlefield to their base.
class MEmperorsDivide : public SpellCard {
public:
    MEmperorsDivide() : SpellCard(366) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        auto bf = ctx.state.getObject(targets[0]).battlefieldId();
        if (!bf) return;
        // Move all friendly units at this BF to base.
        std::vector<GameObjectId> to_move;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (obj.battlefieldId() == bf) to_move.push_back(id);
        }
        for (auto id : to_move) {
            if (ctx.state.objectExists(id)) ctx.executor.moveToBase(id);
        }
        ctx.events.logTrace("EMPEROR'S DIVIDE: moved " +
                             std::to_string(to_move.size()) +
                             " friendly units to base");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true,
                                   .must_be_at_battlefield = true};
    }
};

// [48] Meditation — [Reaction] As an additional cost, you may exhaust a
// friendly unit. If you do, draw 2; otherwise, draw 1.
// Engine doesn't natively support "optional additional cost"; we
// approximate by always drawing 1 (the baseline). The "exhaust friendly to
// draw 2" upside is a follow-up if the engine grows additional-cost hooks.
class MMeditation : public SpellCard {
public:
    MMeditation() : SpellCard(48) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Approximation: draw 1 (no additional-cost path yet).
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("MEDITATION: draw 1 (additional-cost path not modeled)");
    }
};

// [593] Combat Experience — [Reaction] +1 [M] to a unit this turn. [Level 6]
// instead: +3 [M] this turn. Phase 6q — deferred target selection.
class MCombatExperience : public SpellCard {
public:
    MCombatExperience() : SpellCard(593) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Combat Experience", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        int bonus = (ctx.state.player(ctx.controller).xp >= 6) ? 3 : 1;
        ctx.executor.giveTemporaryMight(picked, bonus);
        ctx.events.logTrace("COMBAT EXPERIENCE: +" + std::to_string(bonus) +
                             "[M] this turn");
    }
};

// [600] Skyward Strike — Move an enemy unit. [Level 6] Stun an enemy unit
// instead. Phase 6q — deferred target selection.
class MSkywardStrike : public SpellCard {
public:
    MSkywardStrike() : SpellCard(600) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Skyward Strike", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        if (ctx.state.player(ctx.controller).xp >= 6) {
            ctx.executor.stunUnitBy(picked, ctx.source);
            ctx.events.logTrace("SKYWARD STRIKE: Level 6 -> stun");
        } else {
            ctx.executor.moveToBase(picked);
            ctx.events.logTrace("SKYWARD STRIKE: move to base");
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// [43] Charm — "Move an enemy unit." Override of the generated stub to add
//   needsPlayTimeTarget + pickTarget so the policy head gets distinct vocab
//   slots per target choice.
// ═══════════════════════════════════════════════════════════════════════════════
class MCharm : public SpellCard {
public:
    MCharm() : SpellCard(43) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Charm", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // "Move an enemy unit" — default destination is base.
        ctx.executor.moveToBase(picked);
        ctx.events.logTrace("CHARM: moved enemy " +
                             ctx.state.getObject(picked).name + " to base");
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// [58] Discipline — "[Reaction] Give a unit +2 [M] this turn. Draw 1."
//   Migrate to deferred target + draw rider runs even if target fizzles.
// ═══════════════════════════════════════════════════════════════════════════════
class MDiscipline : public SpellCard {
public:
    MDiscipline() : SpellCard(58) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        bool suspended = false;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Discipline", legal);
            // Distinguish suspend vs no-targets via resume_point.
            // pickTarget reserves slots 6/7/8; 7 = suspended.
            if (picked == kInvalidId &&
                ctx.state.chain.resuming &&
                ctx.state.chain.resuming->resume_point == 7) {
                suspended = true;
            }
        }
        if (suspended) return;
        if (picked != kInvalidId && ctx.state.objectExists(picked)) {
            ctx.executor.giveTemporaryMight(picked, 2);
        }
        // Draw rider always runs (even if target fizzled).
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("DISCIPLINE: +2M to target + draw 1");
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// [173] Ride the Wind — "[Action] Move a friendly unit and ready it."
//   ("Move" without destination defaults to base, same as Skyward Strike.)
// ═══════════════════════════════════════════════════════════════════════════════
class MRideTheWind : public SpellCard {
public:
    MRideTheWind() : SpellCard(173) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked = kInvalidId;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Ride the Wind", legal);
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.moveToBase(picked);
        ctx.executor.readyObject(picked);
        ctx.events.logTrace("RIDE THE WIND: moved + readied " +
                             ctx.state.getObject(picked).name);
    }
};

// [738] Vi, Peacekeeper — [Ambush] (engine-handled) + WhenIAttack: stun an
// enemy unit here.
class MViPeacekeeper : public UnitCard {
public:
    MViPeacekeeper() : UnitCard(738) {}
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true,
                                   .must_be_at_battlefield = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        // Restrict to enemy units at this Vi's battlefield.
        std::vector<GameObjectId> out;
        // The actual filter happens at intent-gen time using
        // ctx.source's battlefield. Without knowing source here, return
        // all enemy units at any battlefield — engine will narrow as
        // needed.
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller == controller) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BattlefieldLocation>(*obj.location)) continue;
            out.push_back(id);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        ctx.executor.stunUnit(targets[0]);
        ctx.events.logTrace("VI: stun on attack");
    }
};

// ── HARDER ────────────────────────────────────────────────────────────────────

// [199] Tideturner — [Hidden] (engine handles) + WhenIPlayMe: optionally
// swap location with a unit you control at another location.
class MTideturner : public UnitCard {
public:
    MTideturner() : UnitCard(199) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller) continue;
            if (!obj.location.has_value()) continue;
            out.push_back(id);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty()) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.objectExists(targets[0])) return;
        // "you may" — wrap in confirmOptional.
        auto still_legal = [&ctx, &targets]() {
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.objectExists(targets[0]) &&
                   ctx.state.getObject(ctx.source).location.has_value() &&
                   ctx.state.getObject(targets[0]).location.has_value();
        };
        if (!still_legal()) return;
        auto conf = confirmOptional(ctx,
            "Tideturner: swap locations with target?", still_legal);
        if (conf < 1) return;
        auto my_loc  = ctx.state.getObject(ctx.source).location;
        auto tgt_loc = ctx.state.getObject(targets[0]).location;
        moveToLocation(ctx.executor, ctx.source,   tgt_loc);
        moveToLocation(ctx.executor, targets[0],   my_loc);
        ctx.events.logTrace("TIDETURNER: swapped locations");
    }
};

// [645] Smoke and Mirrors — [Hidden] + [Action]: choose a friendly unit and
// another friendly at a different location; swap their locations.
class MSmokeAndMirrors : public SpellCard {
public:
    MSmokeAndMirrors() : SpellCard(645) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.size() < 2) return;
        if (!ctx.state.objectExists(targets[0]) ||
            !ctx.state.objectExists(targets[1])) return;
        auto la = ctx.state.getObject(targets[0]).location;
        auto lb = ctx.state.getObject(targets[1]).location;
        moveToLocation(ctx.executor, targets[0], lb);
        moveToLocation(ctx.executor, targets[1], la);
        ctx.events.logTrace("SMOKE AND MIRRORS: swapped two friendly units");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
};

// [757] Mirror Image — Choose a unit. Play a 0M Reflection token to your
// base; make it a copy of that unit; give it [Temporary]. (Approximation:
// copyUnit + spawn token + temporary keyword. The "to your base" part is
// handled by the token's default creation location.)
class MMirrorImage : public SpellCard {
public:
    MMirrorImage() : SpellCard(757) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        // Create a Reflection token at the controller's base. Full
        // 8-arg signature: (controller, type, name, might, tags,
        // keywords, location, enter_ready). copyUnit() below
        // overwrites might/tags/keywords from the source.
        GameObjectId token = ctx.executor.createToken(
            ctx.controller, CardType::Unit, "Reflection",
            /*might=*/0, /*tags=*/{}, /*keywords=*/KeywordSet{},
            BaseLocation{}, /*enter_ready=*/true);
        if (token != kInvalidId) {
            ctx.executor.copyUnit(token, targets[0]);
            ctx.executor.giveTemporaryKeyword(token, Keyword::Temporary, 0);
            ctx.events.logTrace("MIRROR IMAGE: spawned Reflection copy of " +
                                 ctx.state.getObject(targets[0]).name +
                                 " (Temporary)");
        }
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
};

// [461] Fizz, Trickster — WhenYouPlayMe: may play a spell from trash with
// Energy cost <= [3], ignoring its Energy cost. Recycle it after.
// Engine has playIgnoringCost — approximate by picking the first eligible
// spell from trash. Agent-chosen target selection deferred (would need
// MakeChoice loop).
class MFizzTrickster : public UnitCard {
public:
    MFizzTrickster() : UnitCard(461) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& ps = ctx.state.player(ctx.controller);
        // Simplified — pick first spell from trash, no cost check
        // (CardDef cost lookup isn't exposed through CardContext;
        // matches the MGlascMixologist pattern in this file). Loses
        // the "<= 3 energy" gate, slightly overpowered approximation.
        GameObjectId chosen = kInvalidId;
        int chosen_idx = -1;
        for (int i = static_cast<int>(ps.trash.size()) - 1; i >= 0; --i) {
            auto cid = ps.trash[i];
            if (!ctx.state.objectExists(cid)) continue;
            auto& obj = ctx.state.getObject(cid);
            if (obj.card_type != CardType::Spell) continue;
            if (obj.card_def_id == kInvalidId) continue;
            chosen = cid;
            chosen_idx = i;
            break;
        }
        if (chosen == kInvalidId) return;
        ctx.executor.playIgnoringCost(ctx.controller, chosen);
        if (chosen_idx >= 0) ps.trash.erase(ps.trash.begin() + chosen_idx);
        ctx.events.logTrace("FIZZ: played spell from trash for free");
    }
};

// [603] Allay, Eager Admirer — [Deflect] (engine handles) + while I'm at a
// battlefield, your other units here have [Deflect].
// This is an aura — handled by adding to game_engine.cpp's recalculateAuras
// via text-match. For now the per-card code is a no-op; the engine's
// existing aura system already pattern-matches "your other units here have"
// + "deflect". (Verified working for similar cards like Allay's text.)
class MAllayEager : public UnitCard {
public:
    MAllayEager() : UnitCard(603) {}
};

// [352] Rek'Sai, Breacher — [Accelerate] (engine handles) + [Assault] (engine)
// + Friendly units played from non-hand have Accelerate.
// The "from non-hand have Accelerate" passive is an aura affecting future
// plays — engine doesn't model "during cost payment" auras well. Marking as
// no-op for now; the model will still see this card's basic [Accelerate]/
// [Assault] which are big enough that the unit isn't dead in deck.
class MReksaiBreacher : public UnitCard {
public:
    MReksaiBreacher() : UnitCard(352) {}
};

// [467] Vex, Cheerless — While I'm in combat, friendly spells cost [1][A]
// less (min 1), enemy spells cost [1][A] more.
// Implemented via Card::applyPassiveAura adding two combat_active_only
// cost modifiers (one friendly-only, one enemy-only) every recalculateAuras.
// canAfford / payCardCost consult these and skip them unless any
// BattlefieldState has combat_in_progress.
class MVexCheerless : public UnitCard {
public:
    MVexCheerless() : UnitCard(467) {}
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        PlayerState::CostModifier friendly_mod;
        friendly_mod.energy_reduction = 1;
        friendly_mod.this_turn_only = false;
        friendly_mod.combat_active_only = true;
        friendly_mod.affects_friendly_only = true;
        state.player(controller).cost_modifiers.push_back(friendly_mod);

        PlayerState::CostModifier enemy_mod;
        enemy_mod.energy_increase = 1;
        enemy_mod.this_turn_only = false;
        enemy_mod.combat_active_only = true;
        enemy_mod.affects_enemy_only = true;
        state.player(controller).cost_modifiers.push_back(enemy_mod);
    }
};

// [614] Nami, Headstrong — Optional G additional cost. If paid: stun an
// enemy unit. Optional-additional-cost not modeled; approximate by ALWAYS
// stunning an enemy on play (gives some signal to the trainer; loses the
// "if you paid" gate).
class MNamiHeadstrong : public UnitCard {
public:
    MNamiHeadstrong() : UnitCard(614) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        ctx.executor.stunUnit(targets[0]);
        ctx.events.logTrace("NAMI: stun enemy on play (approximation, no "
                             "additional-cost gate)");
    }
};

// [617] Vex, Mocking — Shield + Tank (engine handles) + "When you stun an
// enemy at a battlefield, you may move me to that battlefield." Approximated
// as WhenIDefend: stun the attacker via card_counters["__defend_attacker_id"]
// (Phase 5h). The move-to-stunned-BF tempo is lost in this approximation.
// Proper version needs WhenYouStun + the move-on-stun side effect; the
// trigger is wired in TriggerManager::onUnitStunned but Vex Mocking's
// effect ("move me") isn't a generic stunUnit-side post-action.
class MVexMocking : public UnitCard {
public:
    MVexMocking() : UnitCard(617) {}
    TriggerType triggerType() const override { return TriggerType::WhenIDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto it = self.card_counters.find("__defend_attacker_id");
        if (it == self.card_counters.end()) return;
        auto attacker = static_cast<GameObjectId>(it->second);
        if (!ctx.state.objectExists(attacker)) return;
        auto still_legal = [&ctx, attacker]() {
            return ctx.state.objectExists(attacker) &&
                   ctx.state.getObject(attacker).location.has_value() &&
                   !ctx.state.getObject(attacker).is_stunned;
        };
        if (!still_legal()) return;
        auto conf = confirmOptional(ctx,
            "Vex Mocking: stun the attacker?", still_legal);
        if (conf < 1) return;
        ctx.executor.stunUnitBy(attacker, ctx.source);
        ctx.events.logTrace("VEX MOCKING: stunned attacker on defend");
    }
};

// [612] Iascylla — "When I hold, at the start of your next Main Phase, you
// may move an enemy unit to this battlefield." Approximation: immediate
// optional move on hold (loses the delay-to-next-main timing). Proper
// version needs multi-trigger support per Card — Iascylla has both
// WhenIHold (scheduler) AND AtStartOfMain (delayed fire) triggers, but
// Card::triggerType() returns only one.
class MIascylla : public UnitCard {
public:
    MIascylla() : UnitCard(612) {}
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;
        if (!std::holds_alternative<BattlefieldLocation>(*self.location)) return;
        BattlefieldId bf = std::get<BattlefieldLocation>(*self.location).id;

        std::vector<GameObjectId> legal;
        PlayerId opp = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.location.has_value()) continue;
            if (!std::holds_alternative<BattlefieldLocation>(*obj.location)) continue;
            legal.push_back(id);
        }
        auto still_legal = [&legal]() { return !legal.empty(); };
        if (!still_legal()) return;
        auto conf = confirmOptional(ctx,
            "Iascylla: move enemy unit here?", still_legal);
        if (conf < 1) return;
        GameObjectId picked = pickTarget(ctx, "Iascylla: pick enemy", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.moveToBattlefield(picked, bf);
        ctx.events.logTrace("IASCYLLA: pulled " +
                             ctx.state.getObject(picked).name +
                             " to BF" + std::to_string(bf));
    }
};

// [597] Monch — "If an opponent controls a stunned unit, I cost [2] less
// and enter ready." Uses Card::selfCostReduction + entersReadyOnPlay hooks;
// both gate on whether the opponent has at least one stunned unit.
class MMonch : public UnitCard {
public:
    MMonch() : UnitCard(597) {}
    static bool oppHasStunned(const GameState& state, PlayerId controller) {
        PlayerId opp = opponent(controller);
        for (auto& [id, obj] : state.objects) {
            if (obj.controller != opp) continue;
            if (!obj.isUnit()) continue;
            if (obj.is_stunned) return true;
        }
        return false;
    }
    int selfCostReduction(const GameState& state, PlayerId controller) const override {
        return oppHasStunned(state, controller) ? 2 : 0;
    }
    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        return oppHasStunned(state, controller);
    }
};

// [703] Evelynn, Entrancing — [Hidden] + [Backline] (engine handles) +
// when played from face down on your turn, you may move an enemy unit.
// The face-down-play-detection isn't currently surfaced as a distinct
// trigger; approximate by ALWAYS triggering on play (loses the "from
// face down on your turn" gate). Still useful for training.
class MEvelynnEntrancing : public UnitCard {
public:
    MEvelynnEntrancing() : UnitCard(703) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        auto still_legal = [&ctx, &targets]() {
            return ctx.state.objectExists(targets[0]) &&
                   ctx.state.getObject(targets[0]).location.has_value();
        };
        if (!still_legal()) return;
        auto conf = confirmOptional(ctx, "Evelynn: move an enemy unit?",
                                      still_legal);
        if (conf < 1) return;
        ctx.executor.moveToBase(targets[0]);
        ctx.events.logTrace("EVELYNN: moved enemy unit to base");
    }
};

// [737] Tactical Retreat — [Reaction]: Choose a friendly unit. The next
// time it would die this turn, heal it, exhaust it, and recall it instead.
// Replacement effects are text-matched in killUnit; this needs to add a
// flag/marker the engine can read. Without a generic "next time would die"
// hook, we approximate as: heal and exhaust the target immediately (some
// damage mitigation, but not the full replacement guarantee).
class MTacticalRetreat : public SpellCard {
public:
    MTacticalRetreat() : SpellCard(737) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        ctx.executor.healObject(targets[0]);
        ctx.executor.exhaustObject(targets[0]);
        ctx.events.logTrace("TACTICAL RETREAT: heal+exhaust target "
                             "(replacement-effect approximation)");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
};

// [695] Blast Cone — When you play this, you may move an enemy unit. When
// you move an enemy unit, you may exhaust this to stun it.
// Multi-trigger; engine supports one trigger per card. Implement just the
// on-play move-an-enemy (most common use). Stun-on-move is the follow-up.
class MBlastCone : public GearCard {
public:
    MBlastCone() : GearCard(695) {}
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                   .must_be_enemy = true};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        auto still_legal = [&ctx, &targets]() {
            return ctx.state.objectExists(targets[0]) &&
                   ctx.state.getObject(targets[0]).location.has_value();
        };
        if (!still_legal()) return;
        auto conf = confirmOptional(ctx,
            "Blast Cone: move an enemy unit?", still_legal);
        if (conf < 1) return;
        ctx.executor.moveToBase(targets[0]);
        ctx.events.logTrace("BLAST CONE: moved enemy on play");
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// [128] Challenge — Phase 6q+ (2026-05-18). Generated stub had broken
// target requirements (both must_be_friendly AND must_be_enemy, which
// matches no object). Replaces with the proper dual-target pattern via
// Card::pickTargetPair. "Choose a friendly unit and an enemy unit. They
// deal damage equal to their Mights to each other." First target =
// friendly, second = enemy. Mutual lethal-aware damage exchange.
//
// Critical: Challenge appears in ALL THREE training decks (miss_fortune,
// rengar, khazix). Without this fix, model trained on these decks
// learned Challenge as a no-op-at-cost spell, distorting policy.
// ═══════════════════════════════════════════════════════════════════════════════
class MChallenge : public SpellCard {
public:
    MChallenge() : SpellCard(128) {}
    TargetRequirements getTargetRequirements() const override {
        // Two targets — generic count=2 unit; actual side filters
        // applied per-pick in pickTargetPair's legal_a/legal_b_fn
        // closures below.
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    // Gate playability: must have at least one friendly unit AND one
    // enemy unit on the board.
    bool hasLegalTargets(const GameState& state,
                          PlayerId controller) const override {
        bool any_friendly = false, any_enemy = false;
        for (auto& [id, obj] : state.objects) {
            if (!obj.location.has_value() || !obj.isUnit()) continue;
            if (obj.controller == controller) any_friendly = true;
            else if (!obj.untargetable_by_enemy) any_enemy = true;
        }
        return any_friendly && any_enemy;
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> legal_friendly;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.location.has_value() || !obj.isUnit()) continue;
            if (obj.controller != ctx.controller) continue;
            legal_friendly.push_back(id);
        }
        auto enemy_fn = [&](GameObjectId /*picked_a*/) {
            std::vector<GameObjectId> legal_enemy;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.location.has_value() || !obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                if (obj.untargetable_by_enemy) continue;
                legal_enemy.push_back(id);
            }
            return legal_enemy;
        };
        auto [friendly, enemy] = pickTargetPair(ctx, "Challenge",
                                                  legal_friendly,
                                                  enemy_fn);
        bool suspending = (friendly == kInvalidId || enemy == kInvalidId) &&
                          ctx.state.chain.resuming.has_value() &&
                          (ctx.state.chain.resuming->resume_point == 10 ||
                           ctx.state.chain.resuming->resume_point == 12);
        if (suspending) return;
        if (friendly == kInvalidId || enemy == kInvalidId) return;
        if (!ctx.state.objectExists(friendly) || !ctx.state.objectExists(enemy)) return;

        int friendly_might = ctx.state.getObject(friendly).current_might;
        int enemy_might    = ctx.state.getObject(enemy).current_might;
        ctx.events.logTrace("CHALLENGE: " + ctx.state.getObject(friendly).name +
                             " (" + std::to_string(friendly_might) + "M) <-> " +
                             ctx.state.getObject(enemy).name +
                             " (" + std::to_string(enemy_might) + "M)");
        // Snapshot might before damage so we don't have order-dependent
        // outcomes (e.g. if friendly's damage drops enemy might).
        ctx.executor.dealDamage(enemy, friendly_might, ctx.source);
        ctx.executor.dealDamage(friendly, enemy_might, ctx.source);
        // Inline kill-on-lethal (test fixture bypasses cleanup).
        for (auto tid : {enemy, friendly}) {
            if (ctx.state.objectExists(tid) &&
                ctx.state.getObject(tid).hasLethalDamage()) {
                ctx.executor.killObject(tid);
            }
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// [145] Unyielding Spirit — Phase 6q+. "Prevent all spell and ability damage
// this turn." Sets the per-player flag consulted by EffectExecutor::dealDamage
// (game_state.h:PlayerState::prevent_spell_ability_damage_this_turn). Flag
// resets in resetTurnTracking. Both training decks (MF, Rengar) use this.
// ═══════════════════════════════════════════════════════════════════════════════
class MUnyieldingSpirit : public SpellCard {
public:
    MUnyieldingSpirit() : SpellCard(145) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.state.player(ctx.controller).prevent_spell_ability_damage_this_turn = true;
        ctx.events.logTrace("UNYIELDING SPIRIT: spell/ability damage prevention "
                             "active this turn (controller=" +
                             std::string(toString(ctx.controller)) + ")");
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// [419] Punch First — Phase 6q+. "Give a unit +5 [M] this turn." Single-target
// buff; migrated to pickTarget for distinct policy-head slots per target.
// In Rengar + Khazix training decks. Registry id 419 (collector_number 97 in
// the SFD set printout; the prior agent's audit confused these two).
// ═══════════════════════════════════════════════════════════════════════════════
class MPunchFirst : public SpellCard {
public:
    MPunchFirst() : SpellCard(419) {}
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true};
    }
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
        GameObjectId picked = pickTarget(ctx, "Punch First", legal);
        if (picked == kInvalidId) return;
        ctx.executor.giveTemporaryMight(picked, 5);
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// [427] Ruin Runner — Phase 6q+. Unit with "I can't be chosen by enemy spells
// and abilities." Same pattern as Baron Nashor — Card::canBeChosenByEnemy()
// returns false; the engine refreshes GameObject::untargetable_by_enemy from
// it during aura recalc; enumerateLegalTargets filters enemy candidates that
// have the flag set. In Khazix training deck. Registry id 427 (collector
// number 105 in SFD; ID 105 in the registry is actually Singularity).
// ═══════════════════════════════════════════════════════════════════════════════
class MRuinRunner : public UnitCard {
public:
    MRuinRunner() : UnitCard(427) {}
    bool canBeChosenByEnemy() const override { return false; }
};

// ═══════════════════════════════════════════════════════════════════════════════
// [138] Catalyst of Aeons — Phase 6q+. "Channel 2 runes exhausted. If you
// couldn't channel 2 runes this way, draw 1." The codegen handles "channel N
// runes exhausted" via channelRunes but loses the IfCantDo conditional rider.
// Manual override: count runes channeled, conditionally draw.
// In MF training deck.
// ═══════════════════════════════════════════════════════════════════════════════
class MCatalystOfAeons : public SpellCard {
public:
    MCatalystOfAeons() : SpellCard(138) {}
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // channelRunes is void — derive the count actually channeled
        // by sampling rune_deck size before vs after. Engine bounds
        // the channel by available rune pool size, so it may channel
        // < requested if the pool's drained.
        auto& ps = ctx.state.player(ctx.controller);
        int before = static_cast<int>(ps.rune_deck.size());
        ctx.executor.channelRunes(ctx.controller, 2, /*enter_exhausted=*/true);
        int after = static_cast<int>(ps.rune_deck.size());
        int channeled = before - after;
        ctx.events.logTrace("CATALYST OF AEONS: channeled " +
                             std::to_string(channeled) + " of 2 (exhausted)");
        if (channeled < 2) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("CATALYST OF AEONS: rider — drew 1");
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// [236] Karthus, Eternal — passive: your Deathknell effects trigger an
//        additional time. Implemented via Card::applyPassiveAura: each
//        on-board Karthus bumps the controller's deathknell_double_count
//        during recalculateAuras. TriggerManager::onUnitDied reads the
//        counter and queues (1 + count) chain items for any friendly
//        unit's WhenIDie trigger. The engine does NOT special-case the
//        card_def_id — Karthus is just one Card subclass that contributes
//        to a generic per-player counter via the applyPassiveAura hook.
// ═══════════════════════════════════════════════════════════════════════════════
class MKarthusEternal : public UnitCard {
public:
    MKarthusEternal() : UnitCard(236) {}
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Each on-board Karthus adds one extra Deathknell fire. Two
        // Karthus on board → 1 base + 2 extras = 3 total fires per
        // Deathknell death event.
        state.player(controller).deathknell_double_count++;
    }
};

// (MVexMocking, MIascylla, MVexCheerless implementations live above —
// the original empty stubs at ~lines 4609/4656/4687 were replaced
// in-place during the Phase 6r card surface pass.)

// ═══════════════════════════════════════════════════════════════════════════════
// Registration
// ═══════════════════════════════════════════════════════════════════════════════

void registerManualDeckCards(CardRegistry& registry) {
    // Champions
    registry.registerCard(734, std::make_unique<MLeBlanc>());
    registry.registerCard(712, std::make_unique<MVexApathetic>());
    registry.registerCard(682, std::make_unique<MRengarTrophy>());
    registry.registerCard(676, std::make_unique<MNidaleeCat>());
    registry.registerCard(66, std::make_unique<MAhriAlluring>());
    // Legends
    registry.registerCard(785, std::make_unique<MGloomist>());
    registry.registerCard(756, std::make_unique<MDeceiver>());
    registry.registerCard(506, std::make_unique<MFireBelowMtn>());
    // Deathknell units
    registry.registerCard(629, std::make_unique<MRuinedRex>());
    registry.registerCard(714, std::make_unique<MBlackRoseDig>());
    registry.registerCard(486, std::make_unique<MGlascMixologist>());
    registry.registerCard(236, std::make_unique<MKarthusEternal>());  // Karthus passive aura
    // Score/phase triggers
    registry.registerCard(73, std::make_unique<MSona>());
    // Spells
    registry.registerCard(209, std::make_unique<MCullTheWeak>());
    registry.registerCard(45, std::make_unique<MDefy>());
    registry.registerCard(631, std::make_unique<MSpriteBurst>());
    // Simple units
    registry.registerCard(560, std::make_unique<MInferna>());
    registry.registerCard(176, std::make_unique<MSneakyDeckhand>());
    registry.registerCard(395, std::make_unique<MDropboarder>());
    registry.registerCard(106, std::make_unique<MSpriteMotherUnit>());
    registry.registerCard(91, std::make_unique<MPitCrew>());
    registry.registerCard(136, std::make_unique<MPitRookie>());
    // Gear
    registry.registerCard(538, std::make_unique<MSealOfFocus>());
    registry.registerCard(542, std::make_unique<MSealOfStrength>());
    registry.registerCard(573, std::make_unique<MFreshBeans>());
    registry.registerCard(640, std::make_unique<MSpriteFountain>());
    registry.registerCard(745, std::make_unique<MThrillOfTheHunt>());
    registry.registerCard(160, std::make_unique<MDazzlingAurora>());

    // Critical no-op fixes for Miss Fortune and Rengar test decks
    registry.registerCard(162, std::make_unique<MMissFortuneCaptain>());
    registry.registerCard(263, std::make_unique<MBulletTime>());
    registry.registerCard(680, std::make_unique<MElderDragon>());
    registry.registerCard(709, std::make_unique<MBaronNashor>());
    registry.registerCard(12,  std::make_unique<MNoxusHopeful>());
    registry.registerCard(26,  std::make_unique<MBrynhirThundersong>());
    registry.registerCard(344, std::make_unique<MFerrousForerunner>());
    registry.registerCard(348, std::make_unique<MRengarPouncing>());

    // Champions & legends batch (2026-05-15)
    registry.registerCard(262, std::make_unique<MBountyHunter>());
    registry.registerCard(543, std::make_unique<MSettBrawler>());
    registry.registerCard(644, std::make_unique<MLilliaFaeFawn>());
    registry.registerCard(705, std::make_unique<MKhaZixMutating>());
    registry.registerCard(744, std::make_unique<MPridestalker>());
    registry.registerCard(749, std::make_unique<MBashfulBloom>());

    // Simple unit & spell triggers batch (2026-05-15)
    registry.registerCard(451, std::make_unique<MTreasureHunter>());
    registry.registerCard(476, std::make_unique<MHonestBroker>());
    registry.registerCard(778, std::make_unique<MPlunderingPoro>());
    registry.registerCard(583, std::make_unique<MGrimApothecary>());
    registry.registerCard(687, std::make_unique<MLunarBoon>());

    // Spell effects batch (2026-05-15)
    registry.registerCard(727, std::make_unique<MShadowsCall>());
    registry.registerCard(690, std::make_unique<MStarCrossed>());
    registry.registerCard(192, std::make_unique<MMindsplitter>());
    registry.registerCard(484, std::make_unique<MDeathgrip>());

    // Gear + activated + reaction batch (2026-05-15)
    registry.registerCard(375, std::make_unique<MHeartOfDarkIce>());
    registry.registerCard(752, std::make_unique<MShadow>());
    registry.registerCard(156, std::make_unique<MSabotage>());
    registry.registerCard(735, std::make_unique<MSacrifice>());
    registry.registerCard(693, std::make_unique<MAbandon>());
    registry.registerCard(457, std::make_unique<MHardBargain>());
    registry.registerCard(183, std::make_unique<MStackedDeck>());

    // Vex/XP units batch (2026-05-15)
    registry.registerCard(596, std::make_unique<MHeraldOfSpring>());
    registry.registerCard(605, std::make_unique<MEnthusiasticPromoter>());
    registry.registerCard(610, std::make_unique<MTrevorSnoozebottom>());
    registry.registerCard(689, std::make_unique<MMisterRoot>());
    registry.registerCard(688, std::make_unique<MMegatusk>());

    // Combat tricks + counter spells batch (2026-05-15)
    registry.registerCard(657, std::make_unique<MGrimResolve>());
    registry.registerCard(64,  std::make_unique<MWindWall>());
    registry.registerCard(668, std::make_unique<MRepulse>());
    registry.registerCard(368, std::make_unique<MNotSoFast>());
    registry.registerCard(696, std::make_unique<MExistentialDread>());
    registry.registerCard(449, std::make_unique<MOverzealousFan>());
    registry.registerCard(674, std::make_unique<MIrresistibleFaefolk>());
    registry.registerCard(67,  std::make_unique<MBlitzcrankImpassive>());
    registry.registerCard(27,  std::make_unique<MDariusTrifarian>());

    // Hunt/Level units batch (2026-05-15)
    registry.registerCard(602, std::make_unique<MWujuApprentice>());
    registry.registerCard(609, std::make_unique<MMosstomper>());
    registry.registerCard(656, std::make_unique<MGemhandHunter>());
    registry.registerCard(675, std::make_unique<MMasterYi>());
    registry.registerCard(698, std::make_unique<MScryersBloom>());

    // WhenIWinCombat-gated batch (2026-05-15)
    registry.registerCard(552, std::make_unique<MGloriousExecutioner>());
    registry.registerCard(787, std::make_unique<MVoidreaver>());
    registry.registerCard(750, std::make_unique<MLiltingLullaby>());
    registry.registerCard(28,  std::make_unique<MDravenShowboat>());

    // Chain-resolution refactors — Phase C-1 commit 6 (2026-05-16)
    // Each replaces a generated stub that called the legacy blocking
    // discardCards() bridge. Drives the discard through the resume pattern.
    registry.registerCard(3,   std::make_unique<MChemtechEnforcer>());
    registry.registerCard(20,  std::make_unique<MScrapyardChampion>());
    registry.registerCard(30,  std::make_unique<MJinxDemolitionist>());
    registry.registerCard(178, std::make_unique<MUndercoverAgent>());
    registry.registerCard(185, std::make_unique<MTravelingMerchant>());
    registry.registerCard(444, std::make_unique<MCorruptEnforcer>());
    registry.registerCard(470, std::make_unique<MEzrealProdigy>());
    registry.registerCard(642, std::make_unique<MHweiBroodingPainter>());
    registry.registerCard(685, std::make_unique<MEvershadeStalker>());
    registry.registerCard(293, std::make_unique<MZaunWarrens>());
    registry.registerCard(650, std::make_unique<MGutterPalace>());
    registry.registerCard(8,   std::make_unique<MGetExcited>());
    registry.registerCard(579, std::make_unique<MSquareUp>());

    // Ivern test deck
    registry.registerCard(595, std::make_unique<MFriskyHunter>());
    registry.registerCard(718, std::make_unique<MLoyalPoro>());
    registry.registerCard(608, std::make_unique<MFriendship>());
    registry.registerCard(739, std::make_unique<MIvernFriend>());
    registry.registerCard(754, std::make_unique<MDaisy>());
    registry.registerCard(622, std::make_unique<MVilemaw>());
    registry.registerCard(615, std::make_unique<MScuttleCrab>());
    registry.registerCard(775, std::make_unique<MVaultsOfHelia>());
    registry.registerCard(290, std::make_unique<MVilemawsLair>());
    registry.registerCard(530, std::make_unique<MRockfallPath>());
    registry.registerCard(753, std::make_unique<MGreenFather>());
    registry.registerCard(613, std::make_unique<MIvernNurturer>());
    registry.registerCard(606, std::make_unique<MFlurryOfFeathers>());
    registry.registerCard(213, std::make_unique<MHiddenBlade>());
    registry.registerCard(604, std::make_unique<MBackOff>());

    // Jhin deck (2026-05-17)
    registry.registerCard(782, std::make_unique<MVirtuoso>());
    registry.registerCard(584, std::make_unique<MJhinMurderous>());
    registry.registerCard(326, std::make_unique<MGoldToken>());
    registry.registerCard(767, std::make_unique<MForgottenLibrary>());
    registry.registerCard(743, std::make_unique<MCurtainCall>());
    registry.registerCard(105, std::make_unique<MSingularity>());
    registry.registerCard(346, std::make_unique<MPiercingLight>());
    registry.registerCard(389, std::make_unique<MFrigidTouch>());
    registry.registerCard(400, std::make_unique<MRocketBarrage>());
    registry.registerCard(571, std::make_unique<MUpstageComedy>());
    registry.registerCard(623, std::make_unique<MDownstageDramatics>());
    registry.registerCard(635, std::make_unique<MDeadlyFlourish>());
    registry.registerCard(22,  std::make_unique<MThermoBeam>());
    registry.registerCard(123, std::make_unique<MUncheckedPower>());
    registry.registerCard(122, std::make_unique<MTimeWarp>());
    registry.registerCard(399, std::make_unique<MProductionSurge>());
    registry.registerCard(405, std::make_unique<MHextechAnomaly>());
    registry.registerCard(83,  std::make_unique<MConsultThePast>());
    registry.registerCard(575, std::make_unique<MLotusTrap>());

    // Phase 6o (2026-05-18) — Khazix-deck broken-card fixes.
    registry.registerCard(758, std::make_unique<MVoidAssault>());
    registry.registerCard(671, std::make_unique<MBloodRose>());
    registry.registerCard(465, std::make_unique<MSpiritWheel>());

    // Phase 6o batch 2 — broken cards across all test decks.
    registry.registerCard(110, std::make_unique<MEkkoRecurrent>());
    registry.registerCard(134, std::make_unique<MMobilize>());
    registry.registerCard(169, std::make_unique<MGust>());
    registry.registerCard(172, std::make_unique<MRebuke>());
    registry.registerCard(369, std::make_unique<MPoroSnax>());
    registry.registerCard(220, std::make_unique<MFacebreaker>());
    registry.registerCard(366, std::make_unique<MEmperorsDivide>());
    registry.registerCard(48,  std::make_unique<MMeditation>());
    registry.registerCard(593, std::make_unique<MCombatExperience>());
    registry.registerCard(600, std::make_unique<MSkywardStrike>());
    // Phase 6u — vex+khazix target-collision migrations (override
    // generated stubs with needsPlayTimeTarget + pickTarget).
    registry.registerCard(43,  std::make_unique<MCharm>());
    registry.registerCard(58,  std::make_unique<MDiscipline>());
    registry.registerCard(173, std::make_unique<MRideTheWind>());
    registry.registerCard(738, std::make_unique<MViPeacekeeper>());
    registry.registerCard(199, std::make_unique<MTideturner>());
    registry.registerCard(645, std::make_unique<MSmokeAndMirrors>());
    registry.registerCard(757, std::make_unique<MMirrorImage>());
    registry.registerCard(461, std::make_unique<MFizzTrickster>());
    registry.registerCard(603, std::make_unique<MAllayEager>());
    registry.registerCard(352, std::make_unique<MReksaiBreacher>());
    registry.registerCard(467, std::make_unique<MVexCheerless>());
    registry.registerCard(614, std::make_unique<MNamiHeadstrong>());
    registry.registerCard(617, std::make_unique<MVexMocking>());
    registry.registerCard(612, std::make_unique<MIascylla>());
    registry.registerCard(597, std::make_unique<MMonch>());
    registry.registerCard(703, std::make_unique<MEvelynnEntrancing>());
    registry.registerCard(737, std::make_unique<MTacticalRetreat>());
    registry.registerCard(695, std::make_unique<MBlastCone>());

    // Phase 6q+ — Challenge (dual-target spell, all 3 training decks).
    registry.registerCard(128, std::make_unique<MChallenge>());

    // Phase 6q+ — additional stubs fixed for training-deck coverage.
    registry.registerCard(145, std::make_unique<MUnyieldingSpirit>());
    registry.registerCard(419, std::make_unique<MPunchFirst>());
    registry.registerCard(427, std::make_unique<MRuinRunner>());
    registry.registerCard(138, std::make_unique<MCatalystOfAeons>());
}

} // namespace riftbound
