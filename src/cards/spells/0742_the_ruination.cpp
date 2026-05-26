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

class TheRuination : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        // AoE kill
        {
            std::vector<GameObjectId> to_kill;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.location.has_value()) continue;
                if (!obj.isUnit()) continue;
                to_kill.push_back(id);
            }
            for (auto id : to_kill) ctx.executor.killObject(id);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 742;
        d.def_id = R"RB(unl-180-219)RB";
        d.name = R"RB(The Ruination)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-180/219)RB";
        d.collector_number = 180;
        d.artist = R"RB(Oliver Chipping)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 9;
        d.power_cost = 3;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Kill all units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f1fea5bfad864f8d58b1b55b04de951285f6ad4e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_742(CardRegistry& r) {
    r.registerCard(742, std::make_unique<TheRuination>());
}

} // namespace riftbound
