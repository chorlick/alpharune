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

class SoaringScout : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        ctx.executor.channelRunes(ctx.controller, 1, true);
    }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 216;
        d.def_id = R"RB(ogn-216-298)RB";
        d.name = R"RB(Soaring Scout)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-216/298)RB";
        d.collector_number = 216;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Bird)RB", R"RB(Freljord)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — Channel 1 rune exhausted. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/75b6ce420888035b566c3795cabe0999a9a918b0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_216(CardRegistry& r) {
    r.registerCard(216, std::make_unique<SoaringScout>());
}

} // namespace riftbound
