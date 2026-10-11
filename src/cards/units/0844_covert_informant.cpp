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

class CovertInformant : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "[Empower] [3]"
    EmpowerSpec empowerSpec() const override { return empowerCost(3); }
    // "[Empowered][>] When I move, draw 1."
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.getObject(ctx.source).isEmpowered()) return;
        ctx.executor.drawCards(ctx.controller, 1);
        RIFT_TRACE(ctx.events, "COVERT INFORMANT: moved while Empowered -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 844;
        d.def_id = R"RB(ven-057-166)RB";
        d.name = R"RB(Covert Informant)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-057/166)RB";
        d.collector_number = 57;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [3] ([3]: Empower me. Use only if not Empowered.)
[Empowered][>] When I move, draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ff33454bf980a5c884b63e524c6580408e9b94e3-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_844(CardRegistry& r) {
    r.registerCard(844, std::make_unique<CovertInformant>());
}

}  // namespace riftbound
