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

class PykeDocksideButcher : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/0, /*power=*/1, Domain::Fury,
                /*any_domain=*/false, /*paid_flag=*/"__pyke_db_paid"};
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        if (self.card_counters["__pyke_db_paid"] != 1) return;
        ctx.executor.readyObject(ctx.source);
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.events.logTrace("PYKE DOCKSIDE BUTCHER: paid [R] -> ready me + +2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 590;
        d.def_id = R"RB(unl-028-219)RB";
        d.name = R"RB(Pyke, Dockside Butcher)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-028/219)RB";
        d.collector_number = 28;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Pyke)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Ganking);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Ganking] (I can move from battlefield to battlefield.)
You may pay [R] as an additional cost to play me.
When you play me, if you paid the additional cost, ready me and give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e0ce05148d7368070bcc8c0a43f66ece0834653a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_590(CardRegistry& r) {
    r.registerCard(590, std::make_unique<PykeDocksideButcher>());
}

} // namespace riftbound
