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

class JaeMedarda : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you choose me with a spell, draw 1."
    // The engine fires WhenYouChooseAFriendlyUnit on ALL the player's units
    // when ANY friendly unit is chosen, so self-filter: only draw if a chain
    // item actually chose ME.
    TriggerType triggerType() const override { return TriggerType::WhenYouChooseAFriendlyUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        bool chose_me = false;
        for (const auto& item : ctx.state.chain.items) {
            if (!item.is_spell) continue;
            for (auto t : item.targets)
                if (t == ctx.source) { chose_me = true; break; }
            if (chose_me) break;
        }
        if (!chose_me) return;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("JAE MEDARDA: chosen by a spell -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 463;
        d.def_id = R"RB(sfd-142-221)RB";
        d.name = R"RB(Jae Medarda)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-142/221)RB";
        d.collector_number = 142;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.power_cost = 2;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you choose me with a spell, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/92eac6bb020a068d9c1668b59ecaaeab05112b3f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_463(CardRegistry& r) {
    r.registerCard(463, std::make_unique<JaeMedarda>());
}

} // namespace riftbound
