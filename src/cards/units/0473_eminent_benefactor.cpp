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

class EminentBenefactor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "When I hold, play two Gold gear tokens exhausted."
        createGoldExhausted(ctx);
        createGoldExhausted(ctx);
        ctx.events.logTrace("EMINENT BENEFACTOR: hold -> two Gold gear tokens exhausted");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 473;
        d.def_id = R"RB(sfd-152-221)RB";
        d.name = R"RB(Eminent Benefactor)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-152/221)RB";
        d.collector_number = 152;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Piltover)RB"};
        d.energy_cost = 6;
        d.might = 5;
        d.ability_text = R"RB(When I hold, play two Gold gear tokens exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8b1a3e89fc40bb81c44f0ee232a701d85a1f209f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_473(CardRegistry& r) {
    r.registerCard(473, std::make_unique<EminentBenefactor>());
}

} // namespace riftbound
