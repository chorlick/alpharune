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

class AlbusFerros : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "spend any number of buffs. For each buff spent, channel 1 rune
        // exhausted."
        int total_buffs = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                total_buffs += obj.buff_count;
        }
        if (total_buffs <= 0) return;
        int x = pickXAmount(ctx, "Albus Ferros (buffs to spend)", 0, total_buffs);
        if (x == -1) return;  // suspended for agent decision
        if (x <= 0) return;
        // Spend x buffs from friendly units (greedy).
        int remaining = x;
        for (auto& [id, obj] : ctx.state.objects) {
            if (remaining <= 0) break;
            if (!obj.isUnit() || obj.controller != ctx.controller || !obj.location.has_value())
                continue;
            while (obj.buff_count > 0 && remaining > 0) {
                obj.buff_count -= 1;
                remaining--;
            }
            obj.recomputeMight();
        }
        ctx.executor.channelRunes(ctx.controller, x, /*enter_exhausted=*/true);
        ctx.events.logTrace("ALBUS FERROS: spend " + std::to_string(x) +
                            " buffs -> channel " + std::to_string(x) + " runes exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 230;
        d.def_id = R"RB(ogn-230-298)RB";
        d.name = R"RB(Albus Ferros)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-230/298)RB";
        d.collector_number = 230;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, spend any number of buffs. For each buff spent, channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ec0be8d2a79196b689fccf9ee42ce8baa7e9c35e-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_230(CardRegistry& r) {
    r.registerCard(230, std::make_unique<AlbusFerros>());
}

} // namespace riftbound
