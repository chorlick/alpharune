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

class FaeDragon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, buff up to four friendly units. (Give each a +1 [M]
    //  buff if it doesn't have one.)"
    // Auto-applies to up to four currently-unbuffed friendly units — faithful
    // since buffing unbuffed units is strictly beneficial and never declined.
    //
    // ENGINE GAP: "When you spend a buff, play a Gold gear token exhausted."
    // There is no "spend a buff" trigger event in TriggerType, so this second
    // clause is left unimplemented.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int buffed = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (buffed >= 4) break;
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (obj.buff_count > 0) continue;  // "if it doesn't have one"
            ctx.executor.buffUnit(id);
            ++buffed;
        }
        ctx.events.logTrace("FAE DRAGON: buffed " + std::to_string(buffed) +
                            " friendly units (up to 4)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 423;
        d.def_id = R"RB(sfd-101-221)RB";
        d.name = R"RB(Fae Dragon)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-101/221)RB";
        d.collector_number = 101;
        d.artist = R"RB(Bubble Cat Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Fae)RB", R"RB(Dragon)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 7;
        d.power_cost = 1;
        d.might = 7;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, buff up to four friendly units. (Give each a +1 [M] buff if it doesn't have one.)
When you spend a buff, play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/76aeda6bf59acee92b512f4a0272892673cfce90-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_423(CardRegistry& r) {
    r.registerCard(423, std::make_unique<FaeDragon>());
}

} // namespace riftbound
