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

class DecisiveStrike : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // "Give friendly units +2 [M] this turn." — every friendly unit on the
    // board as the spell resolves; nothing is chosen.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> units;
        for (auto& [id, obj] : ctx.state.objects.onBoard())
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                units.push_back(id);
        for (auto id : units) ctx.executor.giveTemporaryMight(id, 2);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 322;
        d.def_id = R"RB(ogs-024-024)RB";
        d.name = R"RB(Decisive Strike)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-024/024)RB";
        d.collector_number = 24;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Garen)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Action);
        d.ability_text = R"RB([Action] (Play on your turn or in showdowns.)
Give friendly units +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/986782271be078c184a8d1c196e375106459c6c5-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_322(CardRegistry& r) {
    r.registerCard(322, std::make_unique<DecisiveStrike>());
}

} // namespace riftbound
