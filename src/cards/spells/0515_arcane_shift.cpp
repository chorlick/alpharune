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
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_friendly = true};
    }
    // Both units are targets (CR 355.5): the friendly unit is named in the
    // play intent, the enemy unit is chosen in the finalize pass, before the
    // cost and before anyone can respond. A spell needs a valid choice for
    // every target to be played (CR 355.8).
    unsigned choicesAtFinalization() const override { return kFinalizeResolve; }
    FinalizeChoice mayChooseAtFinalization(const GameState& state, PlayerId controller,
                                           GameObjectId object) const override {
        return friendlyUnitChoice(state, controller, object);
    }
    static std::vector<GameObjectId> enemiesAtBattlefields(const GameState& state,
                                                           PlayerId controller) {
        return chooseableUnits(state, controller, [&](const GameObject& o) {
            return o.controller != controller && o.isAtBattlefield();
        });
    }
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        return Card::hasLegalTargets(state, controller) &&
               !enemiesAtBattlefields(state, controller).empty();
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        const GameObjectId unit =
            (!targets.empty() && ctx.state.objectExists(targets[0])) ? targets[0] : kInvalidId;
        const auto picked_opt = selectTargetOrNone(*this, ctx, "Arcane Shift: deal 3 to enemy unit",
                                                   enemiesAtBattlefields(ctx.state, ctx.controller),
                                                   7, /*allow_none=*/false);
        if (!picked_opt.has_value()) return;   // waiting for the agent
        if (finalizingChoices(ctx)) return;    // choices end here (CR 355)
        const GameObjectId picked = *picked_opt;   // kInvalidId: a mistarget
        // Where the unit comes back: its owner's base or a battlefield they
        // control (CR 355.2.a), chosen as it is played and finalized, at
        // resolution (CR 420.3.b.2). A token is not played back (CR 183.1).
        std::optional<LocationId> back_to;
        const bool replay = unit != kInvalidId && !ctx.state.getObject(unit).isToken();
        if (replay) {
            const PlayerId unit_owner = ctx.state.getObject(unit).owner;
            const auto where = playLocations(ctx.state, unit_owner);
            // playLocations leaves out a battlefield units can't be played to
            // (Rockfall Path; a player who may only play units to base).
            const int mode = pickMode(ctx, "Arcane Shift: where the unit is played",
                                      where.count(), where.labels);
            if (mode < 0 || mode >= where.count()) return;   // waiting for the agent
            back_to = where.locations[mode];
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
