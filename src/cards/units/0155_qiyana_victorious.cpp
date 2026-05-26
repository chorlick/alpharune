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

class QiyanaVictorious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        int mode = pickMode(ctx, "Qiyana: draw 1 / channel 1 exhausted", 2,
                            {"Draw 1", "Channel 1 rune exhausted"});
        if (mode < 0) return;  // suspended for agent decision
        if (mode == 0) {
            ctx.executor.drawCards(ctx.controller, 1);
        } else {
            ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
        }
        ctx.events.logTrace("QIYANA VICTORIOUS: conquer payoff (mode=" +
                            std::to_string(mode) + ")");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 155;
        d.def_id = R"RB(ogn-155-298)RB";
        d.name = R"RB(Qiyana, Victorious)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-155/298)RB";
        d.collector_number = 155;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Qiyana)RB", R"RB(Ixtal)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
When I conquer, draw 1 or channel 1 rune exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/90d1a07ef0c6c8282e6eee77e479254f50752eb2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_155(CardRegistry& r) {
    r.registerCard(155, std::make_unique<QiyanaVictorious>());
}

} // namespace riftbound
