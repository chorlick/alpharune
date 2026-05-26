#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class DragonSRage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Move an enemy unit. Then do this: Choose another enemy unit at its
    //  destination. They deal damage equal to their Mights to each other."
    // The agent picks the enemy unit to move; we move it to a battlefield that
    // holds another enemy unit (so the clash is meaningful), then the two deal
    // damage equal to their Might to each other (collect-then-kill).
    bool needsPlayTimeTarget() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        PlayerId enemy = opponent(ctx.controller);
        GameObjectId mover;
        if (!targets.empty()) {
            mover = targets[0];
        } else {
            auto legal = enumerateLegalTargets(ctx.state, ctx.controller);
            mover = pickTarget(ctx, "Dragon's Rage (enemy unit to move)", legal);
        }
        if (mover == kInvalidId || !ctx.state.objectExists(mover)) return;
        auto cur_bf = ctx.state.getObject(mover).battlefieldId();

        // Pick a destination battlefield that holds ANOTHER enemy unit and is
        // not the mover's current battlefield (so it can clash).
        BattlefieldId dest = kInvalidId;
        GameObjectId other = kInvalidId;
        for (const auto& bf : ctx.state.battlefields) {
            if (cur_bf && bf.id == *cur_bf) continue;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == mover) continue;
                if (!obj.isUnit() || obj.controller != enemy) continue;
                if (obj.battlefieldId() == bf.id) { dest = bf.id; other = id; break; }
            }
            if (dest != kInvalidId) break;
        }
        if (dest == kInvalidId) {
            // No other enemy unit elsewhere — just move it (to first BF or base).
            if (!ctx.state.battlefields.empty())
                ctx.executor.moveToBattlefield(mover, ctx.state.battlefields.front().id);
            ctx.events.logTrace("DRAGON'S RAGE: moved enemy unit (no clash target)");
            return;
        }
        ctx.executor.moveToBattlefield(mover, dest);
        if (!ctx.state.objectExists(mover) || !ctx.state.objectExists(other)) return;
        // "They deal damage equal to their Mights to each other."
        int m_mover = ctx.state.getObject(mover).current_might;
        int m_other = ctx.state.getObject(other).current_might;
        ctx.executor.dealDamage(other, m_mover, mover);
        ctx.executor.dealDamage(mover, m_other, other);
        for (auto id : {mover, other}) {
            if (ctx.state.objectExists(id) && ctx.state.getObject(id).hasLethalDamage())
                ctx.executor.killObject(id);
        }
        ctx.events.logTrace("DRAGON'S RAGE: moved + mutual-Might damage");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_enemy = true};
    }
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
