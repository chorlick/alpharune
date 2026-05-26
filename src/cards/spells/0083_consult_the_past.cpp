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

class ConsultThePast : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 2);
        ctx.events.logTrace("CONSULT THE PAST: draw 2");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 83;
        d.def_id = R"RB(ogn-083-298)RB";
        d.name = R"RB(Consult the Past)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-083/298)RB";
        d.collector_number = 83;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 4;
        d.keywords.set(Keyword::Hidden);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Reaction] (Play any time, even before spells and abilities resolve.)
Draw 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/198d950d3933972273ec714e9264cbd563ea6920-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_83(CardRegistry& r) {
    r.registerCard(83, std::make_unique<ConsultThePast>());
}

} // namespace riftbound
