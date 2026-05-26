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

class Vilemaw : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.events.logTrace("VILEMAW: hold — draw 1");
        ctx.executor.drawCards(ctx.controller, 1);
    }
    // "Enemy units here with less Might than me don't deal combat damage."
    // Continuous conditional → an aura recomputed each cleanup: for each
    // on-board Vilemaw, suppress combat damage on enemy units at its
    // battlefield whose current Might is below Vilemaw's.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            auto my_bf = self.battlefieldId();
            if (!my_bf) continue;
            for (auto& [tid, tgt] : state.objects) {
                if (!tgt.isUnit() || tgt.controller == controller) continue;  // enemy
                if (tgt.battlefieldId() != my_bf) continue;                    // here
                if (tgt.current_might >= self.current_might) continue;          // less Might
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.suppress_combat_damage = true;
                tgt.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 622;
        d.def_id = R"RB(unl-060-219)RB";
        d.name = R"RB(Vilemaw)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-060/219)RB";
        d.collector_number = 60;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Shadow Isles)RB", R"RB(Spider)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 8;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
Enemy units here with less Might than me don't deal combat damage.
When I hold, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cb6dde0284f9ef84cbc9c0f07d2e7aac2fe6f919-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_622(CardRegistry& r) {
    r.registerCard(622, std::make_unique<Vilemaw>());
}

} // namespace riftbound
