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

class SpiritWheel : public GearCard {
public:
    const CardDef& def() const override { return def_; }
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
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 465;
        d.def_id = R"RB(sfd-144-221)RB";
        d.name = R"RB(Spirit Wheel)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-144/221)RB";
        d.collector_number = 144;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.energy_cost = 2;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you choose a friendly unit, you may pay [1] and exhaust this to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d83d994e6dc7949eb783da16f4bbdb64342a8dfb-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_465(CardRegistry& r) {
    r.registerCard(465, std::make_unique<SpiritWheel>());
}

} // namespace riftbound
