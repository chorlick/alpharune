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

class Meditation : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        auto find_ready_friendly = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value()) continue;
                if (obj.is_exhausted) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_ready_friendly() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Meditation: exhaust a friendly unit to draw 2 (else draw 1)?",
            still_legal);
        if (conf == -1) return;  // waiting for agent choice

        if (conf == 1) {
            auto target = find_ready_friendly();
            if (target != kInvalidId) {
                ctx.executor.exhaustObject(target);
                ctx.executor.drawCards(ctx.controller, 2);
                ctx.events.logTrace("MEDITATION: exhausted friendly -> draw 2");
                return;
            }
        }
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("MEDITATION: draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 48;
        d.def_id = R"RB(ogn-048-298)RB";
        d.name = R"RB(Meditation)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-048/298)RB";
        d.collector_number = 48;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
As an additional cost to play this, you may exhaust a friendly unit. If you do, draw 2. Otherwise, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7103336ab0d7f7bebdaa4155e4258d8d9beb06de-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_48(CardRegistry& r) {
    r.registerCard(48, std::make_unique<Meditation>());
}

} // namespace riftbound
