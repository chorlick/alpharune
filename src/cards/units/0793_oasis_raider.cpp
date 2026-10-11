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

class OasisRaider : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "If you control fewer runes than an opponent at the start of your
    //  Beginning Phase, give me +2 [M] and [Ganking] this turn."
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        if (runesControlled(ctx.state, ctx.controller) >=
            runesControlled(ctx.state, opponent(ctx.controller))) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.executor.giveTemporaryKeyword(ctx.source, Keyword::Ganking, 0);
        RIFT_TRACE(ctx.events, "OASIS RAIDER: fewer runes -> +2 [M] and [Ganking] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 793;
        d.def_id = R"RB(ven-006-166)RB";
        d.name = R"RB(Oasis Raider)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-006/166)RB";
        d.collector_number = 6;
        d.artist = R"RB(JiHun Lee)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(If you control fewer runes than an opponent at the start of your Beginning Phase, give me +2 [M] and [Ganking] this turn. (I can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d44355b9b129054e63b6f2c8de30518fe87b6295-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_793(CardRegistry& r) {
    r.registerCard(793, std::make_unique<OasisRaider>());
}

}  // namespace riftbound
