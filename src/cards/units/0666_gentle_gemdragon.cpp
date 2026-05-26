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

class GentleGemdragon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenYouPlayAUnit};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // NOTE: WhenYouPlayAUnit doesn't surface the played unit's tags, so the
        // "another Dragon" qualifier can't be checked precisely; we ready on any
        // friendly unit play (documented approximation).
        int readied = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (readied >= 2) break;
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.is_exhausted || !obj.location.has_value()) continue;
            ctx.executor.readyObject(id);
            ++readied;
        }
        if (readied > 0) {
            ctx.events.logTrace("GENTLE GEMDRAGON: readied " +
                                std::to_string(readied) + " rune(s)");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 666;
        d.def_id = R"RB(unl-104-219)RB";
        d.name = R"RB(Gentle Gemdragon)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-104/219)RB";
        d.collector_number = 104;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Dragon)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 8;
        d.might = 8;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me or another Dragon, ready up to 2 runes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/65fcaff267b7f27cbda09ad23f2449188195d28e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_666(CardRegistry& r) {
    r.registerCard(666, std::make_unique<GentleGemdragon>());
}

} // namespace riftbound
