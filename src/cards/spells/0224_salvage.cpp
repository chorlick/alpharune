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

class Salvage : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty() && ctx.state.objectExists(targets[0])) {
            auto& obj = ctx.state.getObject(targets[0]);
            if (obj.isGear()) ctx.executor.killObject(targets[0]);
        }
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("SALVAGE: (optional gear kill) + draw 1");
    }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_gear = true, .optional = true};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 224;
        d.def_id = R"RB(ogn-224-298)RB";
        d.name = R"RB(Salvage)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-224/298)RB";
        d.collector_number = 224;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
You may kill up to one gear. Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9bdbac358e7c9415c1354b6d1f6888dcf9c5519b-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_224(CardRegistry& r) {
    r.registerCard(224, std::make_unique<Salvage>());
}

} // namespace riftbound
