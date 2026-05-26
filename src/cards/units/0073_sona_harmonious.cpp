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

class SonaHarmonious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (!self.isAtBattlefield()) return;
        int readied = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (readied >= 4) break;
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.is_exhausted) continue;
            if (!obj.location.has_value()) continue;
            ctx.executor.readyObject(id);
            readied++;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 73;
        d.def_id = R"RB(ogn-073-298)RB";
        d.name = R"RB(Sona, Harmonious)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-073/298)RB";
        d.collector_number = 73;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Sona)RB", R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(At the end of your turn, if I'm at a battlefield, ready up to 4 friendly runes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8582f6430821fb912fcb3619c5ce9405f254cb2f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_73(CardRegistry& r) {
    r.registerCard(73, std::make_unique<SonaHarmonious>());
}

} // namespace riftbound
