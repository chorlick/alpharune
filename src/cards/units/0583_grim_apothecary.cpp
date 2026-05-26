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

class GrimApothecary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto find_friendly_at_bf = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.isAtBattlefield()) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_friendly_at_bf() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Grim Apothecary: bounce a friendly unit at a battlefield?",
            still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        auto target = find_friendly_at_bf();
        if (target == kInvalidId) return;
        auto& obj = ctx.state.getObject(target);
        ctx.executor.bounceToHand(target);
        ctx.events.logTrace("GRIM APOTHECARY: bounced " + obj.name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 583;
        d.def_id = R"RB(unl-021-219)RB";
        d.name = R"RB(Grim Apothecary)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-021/219)RB";
        d.collector_number = 21;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
When you play me, you may return a friendly unit at a battlefield to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/af548bbfc856e306c5feefb34eb6bd9a4e442904-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_583(CardRegistry& r) {
    r.registerCard(583, std::make_unique<GrimApothecary>());
}

} // namespace riftbound
