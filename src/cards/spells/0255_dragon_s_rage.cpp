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
    // The caster chooses the unit (as the spell is played), where it moves,
    // and — once it is there — which other enemy unit it fights. The move
    // happens between two choices, so it is recorded on the resolving item
    // (resume_point kMoved) and done once.
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
        if (ri.resume_point != kMoved && ri.resume_point != kAwait) {
            const auto& u = ctx.state.getObject(mover);
            if (!u.isUnit() || !u.location.has_value()) return;
            const int dest = pickMoveDest(*this, ctx, "Dragon's Rage: move it to", u);
            if (dest < 0) return;   // waiting, or nowhere to move it
            moveTo(ctx, mover, dest);
            ri.resume_point = kMoved;
        }

        // "Choose another enemy unit at its destination."
        std::vector<GameObjectId> others;
        if (ctx.state.getObject(mover).location.has_value()) {
            const auto here = ctx.state.getObject(mover).location;
            others = chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& u) {
                return u.controller != ctx.controller && u.id != mover && u.location == here;
            });
            std::sort(others.begin(), others.end());
            others = offerableChoices(ctx, others);
        }
        if (ri.resume_point != kAwait) {
            if (others.empty()) {
                ri.resume_point = kDone;
                ctx.events.logTrace("DRAGON'S RAGE: moved enemy unit (no other enemy unit there)");
                return;
            }
            std::vector<Intent> options;
            for (auto id : others) options.push_back(objChoice(ctx.controller, id));
            ctx.executor.requestChoice(ctx.controller, std::move(options),
                                       "Dragon's Rage: another enemy unit at its destination");
            ri.resume_point = kAwait;
            return;
        }
        ri.resume_point = kDone;
        const GameObjectId other = takenObject(ctx);
        if (std::find(others.begin(), others.end(), other) == others.end()) return;
        commitChosenObject(ctx, other);
        // "They deal damage equal to their Mights to each other." — their
        // Mights where they now stand.
        ctx.executor.recalculateContinuousEffects();
        int m_mover = ctx.state.getObject(mover).current_might;
        int m_other = ctx.state.getObject(other).current_might;
        ctx.executor.dealDamage(other, m_mover, mover);
        ctx.executor.dealDamage(mover, m_other, other);
        DeathBatch together(ctx.executor, std::vector<GameObjectId>{mover, other});   // they die at the same time
        for (auto id : {mover, other}) {
            const auto& obj = ctx.state.getObject(id);
            if (obj.location.has_value() && obj.hasLethalDamage()) ctx.executor.killObject(id);
        }
        ctx.events.logTrace("DRAGON'S RAGE: moved + mutual-Might damage");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true};
    }
private:
    enum : int { kMoved = 20, kAwait = 21, kDone = 22 };
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
