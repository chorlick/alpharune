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

class Soulgorger : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, you may play a unit from your trash, ignoring its
    // Energy cost. (You must still pay its Power cost.)" Only units whose
    // Power cost can be paid are offered.
    std::vector<GameObjectId> trashUnits(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        const EffectExecutor::EffectPlayCost power_only{.ignore_energy = true};
        const auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : ps.trash) {
            if (!ctx.state.getObject(cid).isUnit()) continue;
            if (ctx.executor.canPlayPayingCost(ctx.controller, cid, power_only)) out.push_back(cid);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = [&]() { return !trashUnits(ctx).empty(); };
        int conf = confirmOptional(ctx, "Soulgorger: play a unit from trash?",
                                   still_legal);
        if (conf < 1) return;    // waiting / declined / nothing eligible
        GameObjectId picked = pickTarget(ctx, "Soulgorger (unit from trash)", trashUnits(ctx));
        // To my base or a battlefield I control (CR 355.2.a).
        int where = 0;
        if (picked != kInvalidId) {
            where = pickEffectPlayLocation(*this, ctx, "Soulgorger: play it to");
            if (where == -1) return;          // waiting for the agent
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId) return;     // none / no longer in trash / waiting
        // "ignoring its Energy cost. (You must still pay its Power cost.)"
        // playPayingCost takes the unit out of the trash only if it is paid for.
        const EffectExecutor::EffectPlayCost power_only{.ignore_energy = true};
        if (!ctx.executor.playPayingCost(ctx.controller, picked,
                                         effectPlayLocation(ctx.state, ctx.controller, where),
                                         power_only)) {
            ctx.events.logTrace("SOULGORGER: can't pay that unit's Power cost");
            return;
        }
        ctx.events.logTrace("SOULGORGER: played a unit from trash, ignoring its Energy cost");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 196;
        d.def_id = R"RB(ogn-196-298)RB";
        d.name = R"RB(Soulgorger)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-196/298)RB";
        d.collector_number = 196;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, you may play a unit from your trash, ignoring its Energy cost. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/439c4fa7c8c47cba41df8bce3fe3c8068d297382-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_196(CardRegistry& r) {
    r.registerCard(196, std::make_unique<Soulgorger>());
}

} // namespace riftbound
