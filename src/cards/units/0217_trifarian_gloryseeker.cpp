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

class TrifarianGloryseeker : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (ctx.state.player(ctx.controller).cards_played_this_turn < 2) return;
        ctx.executor.buffUnit(ctx.source);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    bool requiresLegion() const override { return true; }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 217;
        d.def_id = R"RB(ogn-217-298)RB";
        d.name = R"RB(Trifarian Gloryseeker)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-217/298)RB";
        d.collector_number = 217;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.keywords.set(Keyword::Legion);
        d.ability_text = R"RB([Legion] — When you play me, buff me. (If I don't have a buff, I get a +1 [M] buff. Get the effect if you've played another card this turn.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/19ad6a0e743b56021b9651a2105034a488e172e8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_217(CardRegistry& r) {
    r.registerCard(217, std::make_unique<TrifarianGloryseeker>());
}

} // namespace riftbound
