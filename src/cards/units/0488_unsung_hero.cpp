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

class UnsungHero : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (!isMighty(self)) {
            ctx.events.logTrace("UNSUNG HERO: died not Mighty — no draw");
            return;
        }
        ctx.executor.drawCards(ctx.controller, 2);
        ctx.events.logTrace("UNSUNG HERO: [Deathknell] was Mighty — draw 2");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 488;
        d.def_id = R"RB(sfd-167-221)RB";
        d.name = R"RB(Unsung Hero)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-167/221)RB";
        d.collector_number = 167;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Elite)RB", R"RB(Demacia)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — If I was [Mighty], draw 2. (When I die, get the effect. I'm Mighty while I have 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2a227ef7494af6409000c13c4f3d1094cec3a3a8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_488(CardRegistry& r) {
    r.registerCard(488, std::make_unique<UnsungHero>());
}

} // namespace riftbound
