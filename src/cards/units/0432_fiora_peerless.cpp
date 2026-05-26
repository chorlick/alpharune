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

class FioraPeerless : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When I attack or defend one on one, double my Might this combat."
    // One on one = exactly one attacker and one defender at my battlefield.
    TriggerType triggerType() const override { return TriggerType::WhenIAttackOrDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        int attackers = 0, defenders = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.battlefieldId() != my_bf) continue;
            if (obj.combat_designation == CombatDesignation::Attacker) ++attackers;
            else if (obj.combat_designation == CombatDesignation::Defender) ++defenders;
        }
        if (attackers != 1 || defenders != 1) return;  // not one on one
        // Double my Might this combat: add my current Might as temporary might.
        int cur = ctx.state.getObject(ctx.source).current_might;
        ctx.executor.giveTemporaryMight(ctx.source, cur);
        ctx.events.logTrace("FIORA PEERLESS: one on one -> doubled Might this combat (+" +
                            std::to_string(cur) + ")");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 432;
        d.def_id = R"RB(sfd-110-221)RB";
        d.name = R"RB(Fiora, Peerless)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-110/221)RB";
        d.collector_number = 110;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Fiora)RB", R"RB(Demacia)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I attack or defend one on one, double my Might this combat.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3fb80fed6fb916f47b7cebd5b31a970f1d0d562a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_432(CardRegistry& r) {
    r.registerCard(432, std::make_unique<FioraPeerless>());
}

} // namespace riftbound
