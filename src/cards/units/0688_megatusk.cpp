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

class Megatusk : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
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
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 688;
        d.def_id = R"RB(unl-126-219)RB";
        d.name = R"RB(Megatusk)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-126/219)RB";
        d.collector_number = 126;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB(Spend 3 XP: Give your units here [Ganking] this turn. (We can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d073247bc2437443d8a9901f089632a15d11e7a0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_688(CardRegistry& r) {
    r.registerCard(688, std::make_unique<Megatusk>());
}

} // namespace riftbound
