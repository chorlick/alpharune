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

class InvertTimelines : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "Each player discards their hand, then draws 4."
        // The whole hand is discarded (non-selective), so move each card to
        // trash directly; then both players draw 4.
        for (auto p : {ctx.controller, opponent(ctx.controller)}) {
            auto hand = ctx.state.player(p).hand;  // copy: applyDiscard mutates
            for (auto cid : hand) ctx.executor.applyDiscard(p, cid);
        }
        for (auto p : {ctx.controller, opponent(ctx.controller)})
            ctx.executor.drawCards(p, 4);
        ctx.events.logTrace("INVERT TIMELINES: each player discards hand, draws 4");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 201;
        d.def_id = R"RB(ogn-201-298)RB";
        d.name = R"RB(Invert Timelines)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-201/298)RB";
        d.collector_number = 201;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Chaos};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Each player discards their hand, then draws 4.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b916326fdcf61af4630d953c7540a4aa97e6db01-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_201(CardRegistry& r) {
    r.registerCard(201, std::make_unique<InvertTimelines>());
}

} // namespace riftbound
