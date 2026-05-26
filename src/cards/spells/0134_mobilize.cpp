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

class Mobilize : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        // "If you can't" — true when the rune deck is empty (nothing to channel).
        if (ps.rune_deck.empty()) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("MOBILIZE: rune deck empty -> draw 1");
        } else {
            ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
            ctx.events.logTrace("MOBILIZE: channeled 1 rune exhausted");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 134;
        d.def_id = R"RB(ogn-134-298)RB";
        d.name = R"RB(Mobilize)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-134/298)RB";
        d.collector_number = 134;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 2;
        d.ability_text = R"RB(Channel 1 rune exhausted. If you can't, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cb22153e84ce250192b8e6e75e7f4dc0b66a728c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_134(CardRegistry& r) {
    r.registerCard(134, std::make_unique<Mobilize>());
}

} // namespace riftbound
