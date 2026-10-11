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

class DragonSRage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move an enemy unit. Then do this: Choose another enemy unit at its
    //  destination. They deal damage equal to their Mights to each other."
    // The caster chooses the unit (as the spell is played) and where it
    // moves. "Then do this" is a reflexive trigger (CR 388): once the unit has
    // been moved it goes on the chain as an item of its own, and which other
    // enemy unit it fights is chosen as that item is finalized.
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId mover;
        if (!targets.empty()) {
            mover = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            mover = pickTarget(ctx, "Dragon's Rage (enemy unit to move)", legal);
        }
        if (mover == kInvalidId || !ctx.state.chain.resuming.has_value()) return;
        auto& ri = *ctx.state.chain.resuming;
        if (ri.resume_point == kDone) return;
        const auto& u = ctx.state.getObject(mover);
        if (!u.isUnit() || !u.location.has_value()) return;
        const int dest = pickMoveDest(*this, ctx, "Dragon's Rage: move it to", u);
        if (dest < 0) return;   // waiting, or nowhere to move it
        moveTo(ctx, mover, dest);
        ri.resume_point = kDone;
        ctx.events.logTrace("DRAGON'S RAGE: moved an enemy unit");
        ctx.executor.queueReflexiveTrigger(ctx.source, ctx.controller, 0,
                                           {static_cast<int32_t>(mover)});
    }
    bool hasReflexiveTrigger() const override { return true; }
    // `data[0]`: the unit that was moved. "Choose another enemy unit at its
    // destination. They deal damage equal to their Mights to each other."
    void onReflexiveTrigger(CardContext& ctx, int /*index*/,
                            const std::vector<int32_t>& data) override {
        if (data.empty()) return;
        const auto mover = static_cast<GameObjectId>(data[0]);
        auto in_play = [&](GameObjectId id) {
            return ctx.state.objectExists(id) && ctx.state.getObject(id).isUnit() &&
                   ctx.state.getObject(id).location.has_value();
        };
        std::vector<GameObjectId> others;
        if (in_play(mover)) {
            const auto here = ctx.state.getObject(mover).location;
            others = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.controller != ctx.controller && o.id != mover && o.location == here;
            });
            std::sort(others.begin(), others.end());
        }
        const GameObjectId other = pickTarget(
            ctx, "Dragon's Rage: another enemy unit at its destination", others);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 388.4)
        if (other == kInvalidId || !in_play(other) || !in_play(mover)) return;
        // "They deal damage equal to their Mights to each other." — their
        // Mights where they now stand.
        ctx.executor.recalculateContinuousEffects();
        int m_mover = ctx.state.getObject(mover).current_might;
        int m_other = ctx.state.getObject(other).current_might;
        ctx.executor.dealDamage(other, m_mover, mover);
        ctx.executor.dealDamage(mover, m_other, other);
        {   // the batch is for these kills only
            DeathBatch together(ctx.executor, std::vector<GameObjectId>{mover, other});   // they die at the same time
            for (auto id : {mover, other}) {
                const auto& obj = ctx.state.getObject(id);
                if (obj.location.has_value() && obj.hasLethalDamage()) ctx.executor.killObject(id);
            }
        }
        ctx.events.logTrace("DRAGON'S RAGE: mutual-Might damage");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true};
    }
private:
    enum : int { kDone = 22 };
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 255;
        d.def_id = R"RB(ogn-258-298)RB";
        d.name = R"RB(Dragon's Rage)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-258/298)RB";
        d.collector_number = 258;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Body};
        d.tags = {R"RB(Lee Sin)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Move an enemy unit. Then do this: Choose another enemy unit at its destination. They deal damage equal to their Mights to each other.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7f4cbd4fb340cc13b3fbe0ec0db706464d9b29f4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_255(CardRegistry& r) {
    r.registerCard(255, std::make_unique<DragonSRage>());
}

} // namespace riftbound
