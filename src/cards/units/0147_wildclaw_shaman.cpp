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

class WildclawShaman : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, you may spend a buff to buff me and ready me. (If I
    // don't have a buff, I get a +1 [M] buff.)" Optional: on yes, ensure I have
    // a buff (gain +1 if unbuffed per the parenthetical), then ready me.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto still_legal = [&]() { return ctx.state.objectExists(ctx.source); };
        int conf = confirmOptional(ctx, "Wildclaw Shaman: buff & ready me?",
                                   still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (self.buff_count <= 0) ctx.executor.buffUnit(ctx.source);
        ctx.executor.readyObject(ctx.source);
        ctx.events.logTrace("WILDCLAW SHAMAN: buffed & readied me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 147;
        d.def_id = R"RB(ogn-147-298)RB";
        d.name = R"RB(Wildclaw Shaman)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-147/298)RB";
        d.collector_number = 147;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, you may spend a buff to buff me and ready me. (If I don't have a buff, I get a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7abc938fee4ba397f52c8ea60d350857a7517b0c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_147(CardRegistry& r) {
    r.registerCard(147, std::make_unique<WildclawShaman>());
}

} // namespace riftbound
