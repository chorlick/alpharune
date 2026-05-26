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

class NidaleeCatForm : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIWinCombat; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("NIDALEE, CAT FORM: win combat -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 676;
        d.def_id = R"RB(unl-114-219)RB";
        d.name = R"RB(Nidalee, Cat Form)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-114/219)RB";
        d.collector_number = 114;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Cat)RB", R"RB(Ixtal)RB", R"RB(Nidalee)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ambush);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
When I win a combat, draw 1. (I win if I remain after combat.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0e51e39dfe4f922b2c8c9bff785d170350e9b803-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_676(CardRegistry& r) {
    r.registerCard(676, std::make_unique<NidaleeCatForm>());
}

} // namespace riftbound
