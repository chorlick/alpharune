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

class CithriaOfCloudfield : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play another unit, buff me. (If I don't have a buff, I get a
    //  +1 [M] buff.)" WhenYouPlayAUnit fires on OTHER on-board friendly units
    //  (the played unit is excluded by the trigger manager), so ctx.source is
    //  me. Buff only when I currently have no buff counter.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayAUnit; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (self.buff_count > 0) {
            ctx.events.logTrace("CITHRIA OF CLOUDFIELD: already buffed -> no-op");
            return;  // "If I don't have a buff" — already buffed, do nothing.
        }
        ctx.executor.buffUnit(ctx.source);
        ctx.events.logTrace("CITHRIA OF CLOUDFIELD: another unit played -> +1 buff");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 139;
        d.def_id = R"RB(ogn-139-298)RB";
        d.name = R"RB(Cithria of Cloudfield)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-139/298)RB";
        d.collector_number = 139;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Elite)RB", R"RB(Demacia)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play another unit, buff me. (If I don't have a buff, I get a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6f0931db65e25e0a8d8351ffb97f8deed5dc0aa9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_139(CardRegistry& r) {
    r.registerCard(139, std::make_unique<CithriaOfCloudfield>());
}

} // namespace riftbound
