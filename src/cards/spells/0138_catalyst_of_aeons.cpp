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

class CatalystOfAeons : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // channelRunes is void — derive the count actually channeled
        // by sampling rune_deck size before vs after. Engine bounds
        // the channel by available rune pool size, so it may channel
        // < requested if the pool's drained.
        auto& ps = ctx.state.player(ctx.controller);
        int before = static_cast<int>(ps.rune_deck.size());
        ctx.executor.channelRunes(ctx.controller, 2, /*enter_exhausted=*/true);
        int after = static_cast<int>(ps.rune_deck.size());
        int channeled = before - after;
        ctx.events.logTrace("CATALYST OF AEONS: channeled " +
                             std::to_string(channeled) + " of 2 (exhausted)");
        if (channeled < 2) {
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("CATALYST OF AEONS: rider — drew 1");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 138;
        d.def_id = R"RB(ogn-138-298)RB";
        d.name = R"RB(Catalyst of Aeons)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-138/298)RB";
        d.collector_number = 138;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Body};
        d.energy_cost = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Channel 2 runes exhausted. If you couldn't channel 2 runes this way, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/4753707323ab6f9ac572c097f29f2f76ac62f54f-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_138(CardRegistry& r) {
    r.registerCard(138, std::make_unique<CatalystOfAeons>());
}

} // namespace riftbound
