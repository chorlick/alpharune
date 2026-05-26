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

class ShowOfStrength : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    bool isReactionAbility() const override { return true; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int n = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            if (isMighty(obj)) ++n;
        }
        if (n > 0) ctx.executor.drawCards(ctx.controller, n);
        ctx.events.logTrace("SHOW OF STRENGTH: drew " + std::to_string(n) +
                            " (one per Mighty unit)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 428;
        d.def_id = R"RB(sfd-106-221)RB";
        d.name = R"RB(Show of Strength)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-106/221)RB";
        d.collector_number = 106;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Draw 1 for each of your [Mighty] units. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d04ac6687931018fd68368c65cc024155867cab4-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_428(CardRegistry& r) {
    r.registerCard(428, std::make_unique<ShowOfStrength>());
}

} // namespace riftbound
