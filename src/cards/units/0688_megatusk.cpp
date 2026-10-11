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
    // "Spend 3 XP: Give your units here [Ganking] this turn." The XP is the
    // activation cost (ActivationCost::xp_cost: checked and charged by the
    // engine as the ability is activated). Megatusk has no [Ganking] of its
    // own until it is given some.
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        ActivationCost cost;
        cost.xp_cost = 3;
        return cost;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;
        const LocationId here = *self.location;   // a battlefield, or my base
        for (auto& [id, obj] : ctx.state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value() || *obj.location != here) continue;
            ctx.executor.giveTemporaryKeyword(id, Keyword::Ganking, 0);
        }
        RIFT_TRACE(ctx.events, "MEGATUSK: spent 3 XP; friendly units here get Ganking");
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
