#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/card_helpers.h"

namespace riftbound {
namespace {

class ArcaneShift : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Banish a friendly unit, then its owner plays it, ignoring its cost. Deal
    //  3 to an enemy unit at a battlefield. Banish this."
    bool isActionAbility() const override { return true; }
    // Both units are targets (CR 355.5), chosen as the spell is played — in
    // its finalize pass, before its cost and before anyone can respond — as a
    // pair: the enemy unit with the friendly one, so a pair is offered only
    // when its Deflect and the cost can be paid together, and the spell only
    // when such a pair exists (CR 355.8; the engine's pair gate and probe).
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true};
    }
    bool needsPlayTimeTargetPair() const override { return true; }
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    static std::vector<GameObjectId> friendlyUnits(const GameState& state, PlayerId controller) {
        return chooseableUnits(state, controller, [&](const GameObject& o) {
            return o.controller == controller;
        });
    }
    static std::vector<GameObjectId> enemiesAtBattlefields(const GameState& state,
                                                           PlayerId controller) {
        return chooseableUnits(state, controller, [&](const GameObject& o) {
            return o.controller != controller && o.isAtBattlefield();
        });
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return !friendlyUnits(state, controller).empty() &&
               !enemiesAtBattlefields(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const auto [unit, picked] = pickTargetPair(ctx,
            "Arcane Shift: banish a friendly unit, then deal 3 to an enemy unit at a battlefield",
            friendlyUnits(ctx.state, ctx.controller),
            [&](GameObjectId) { return enemiesAtBattlefields(ctx.state, ctx.controller); });
        if (finalizingChoices(ctx)) return;    // choices end here (CR 355)
        if ((unit == kInvalidId || picked == kInvalidId) && ctx.state.chain.resuming.has_value() &&
            (ctx.state.chain.resuming->resume_point == 10 ||
             ctx.state.chain.resuming->resume_point == 12))
            return;                            // waiting for the agent
        // A target no longer legal comes back as kInvalidId: that instruction
        // is ignored (CR 359.3.e.5), the other still happens.
        // Where the unit comes back: its owner's base, a battlefield they
        // control or where the unit's own permissions allow (CR 355.2;
        // playLocationsFor leaves out a battlefield units can't be played
        // to), chosen as it is played and finalized, at resolution (CR
        // 420.3.b.2). The OWNER plays it, so the owner chooses — not the
        // spell's controller when the unit is one they took control of
        // (testing phase R11, as M4 for Thrill of the Hunt). A token is not
        // played back (CR 183.1).
        std::optional<LocationId> back_to;
        const bool replay = unit != kInvalidId && !ctx.state.getObject(unit).isToken();
        if (replay) {
            const PlayerId unit_owner = ctx.state.getObject(unit).owner;
            const auto where = playLocationsFor(ctx, unit_owner, unit);
            const auto picked = pickValueAs(ctx, unit_owner, "Arcane Shift: where the unit is played",
                                            where.labels, PromptKind::Location);
            if (!picked.has_value()) return;   // waiting for the owner
            const int mode = *picked;
            if (mode < 0 || mode >= where.count()) return;
            back_to = where.locations[static_cast<size_t>(mode)];
        }

        // 1. Banish a friendly unit, then its owner plays it ignoring its cost.
        if (unit != kInvalidId) {
            const PlayerId owner = ctx.state.getObject(unit).owner;
            ctx.executor.banishObject(unit);   // sheds its gear
            if (replay) {
                // It comes back as a new object, with no damage, buffs or
                // statuses: banishObject made it one as it left (CR 124.1,
                // EffectExecutor::leftBoard), so nothing is reset here.
                ctx.executor.playIgnoringCost(owner, unit, back_to);
            }
            RIFT_TRACE(ctx.events, "ARCANE SHIFT: banished + replayed a friendly unit");
        }

        // 2. Deal 3 to an enemy unit at a battlefield.
        if (picked != kInvalidId && ctx.state.objectExists(picked)) {
            ctx.executor.dealDamage(picked, 3, ctx.source);
            RIFT_TRACE(ctx.events, "ARCANE SHIFT: dealt 3 to an enemy unit");
        }

        // 3. "Banish this.": it is banished as it leaves the chain instead of
        // going to the trash (ChainManager reads the flag).
        if (ctx.state.objectExists(ctx.source))
            ctx.state.getObject(ctx.source).banish_on_leave_chain = true;
        RIFT_TRACE(ctx.events, "ARCANE SHIFT: banished this");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 515;
        d.def_id = R"RB(sfd-200-221)RB";
        d.name = R"RB(Arcane Shift)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-200/221)RB";
        d.collector_number = 200;
        d.artist = R"RB(Original Force)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Ezreal)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Banish a friendly unit, then its owner plays it, ignoring its cost. Deal 3 to an enemy unit at a battlefield. Banish this.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a5d1d26dee01db24f32a1432639478753d22aa92-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_515(CardRegistry& r) {
    r.registerCard(515, std::make_unique<ArcaneShift>());
}

} // namespace riftbound
