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

class GrandStrategem : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Give friendly units +5 [M] this turn." — every friendly unit on the
    // board as the spell resolves; nothing is chosen.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> units;
        for (auto& [id, obj] : ctx.state.objects.onBoard())
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                units.push_back(id);
        for (auto id : units) ctx.executor.giveTemporaryMight(id, 5);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 233;
        d.def_id = R"RB(ogn-233-298)RB";
        d.name = R"RB(Grand Strategem)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-233/298)RB";
        d.collector_number = 233;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 6;
        d.power_cost = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give friendly units +5 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bda7514aefa94ac3f368690cdd2d0f955d44c089-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_233(CardRegistry& r) {
    r.registerCard(233, std::make_unique<GrandStrategem>());
}

} // namespace riftbound
