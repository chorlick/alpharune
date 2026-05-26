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

class CarrionDredger : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    // [Deathknell]: "Play a 1 [M] Bird unit token with [Deflect] to your base."
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        KeywordSet kw; kw.set(Keyword::Deflect);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird",
                                  /*might=*/1, /*tags=*/{"Bird"}, kw,
                                  BaseLocation{ctx.controller},
                                  /*enter_ready=*/false);
        ctx.events.logTrace("CARRION DREDGER: deathknell -> 1[M] Bird w/ [Deflect]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 715;
        d.def_id = R"RB(unl-153-219)RB";
        d.name = R"RB(Carrion Dredger)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-153/219)RB";
        d.collector_number = 153;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Mech)RB", R"RB(Zaun)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deathknell);
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deathknell][>] Play a 1 [M] Bird unit token with [Deflect] to your base. (When I die, get the effect. Opponents must pay [A] to choose a [Deflect] unit with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/84d8b6d7a9cab2b78a465a8e0caf597f3ef5a175-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_715(CardRegistry& r) {
    r.registerCard(715, std::make_unique<CarrionDredger>());
}

} // namespace riftbound
