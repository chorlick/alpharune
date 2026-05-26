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

class MonasteryOfHirana : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override {
        return TriggerType::WhenYouConquerHere;
    }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto find_buffed_friendly = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (obj.buff_count <= 0) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_buffed_friendly() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Monastery of Hirana: spend a buff to draw 1?", still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        auto target = find_buffed_friendly();
        if (target == kInvalidId) return;
        auto& obj = ctx.state.getObject(target);
        obj.buff_count -= 1;
        obj.recomputeMight();
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("MONASTERY OF HIRANA: spent a buff on " + obj.name +
                             " -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 277;
        d.def_id = R"RB(ogn-282-298)RB";
        d.name = R"RB(Monastery of Hirana)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-282/298)RB";
        d.collector_number = 282;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here, you may spend a buff to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8767d9ed30e2873d3fa1045f973be229da56175d-1038x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_277(CardRegistry& r) {
    r.registerCard(277, std::make_unique<MonasteryOfHirana>());
}

} // namespace riftbound
