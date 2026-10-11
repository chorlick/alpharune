#include "cards/card.h"
#include "cards/card_helpers.h"
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

class FretfulFeline : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When I become ready, give me +2 [M] this turn."
    // (WhenIBecomeReady covers both effect readies and the Awaken phase.)
    TriggerType triggerType() const override { return TriggerType::WhenIBecomeReady; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        RIFT_TRACE(ctx.events, "FRETFUL FELINE: became ready -> +2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 858;
        d.def_id = R"RB(ven-071-166)RB";
        d.name = R"RB(Fretful Feline)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-071/166)RB";
        d.collector_number = 71;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Cat)RB", R"RB(Piltover)RB"};
        d.energy_cost = 6;
        d.might = 5;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(When I become ready, give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/24370c9e33705b393c40f426e221ae851a65dd05-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_858(CardRegistry& r) {
    r.registerCard(858, std::make_unique<FretfulFeline>());
}

}  // namespace riftbound
