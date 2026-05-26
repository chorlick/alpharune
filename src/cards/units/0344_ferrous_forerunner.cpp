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

class FerrousForerunner : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDie; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        KeywordSet kw;  // Mech tokens — no special keywords
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", 3,
                                  {"Mech"}, kw, loc, false);
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech", 3,
                                  {"Mech"}, kw, loc, false);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 344;
        d.def_id = R"RB(sfd-021-221)RB";
        d.name = R"RB(Ferrous Forerunner)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-021/221)RB";
        d.collector_number = 21;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Mech)RB", R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 6;
        d.power_cost = 1;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Deathknell);
        d.ability_text = R"RB([Deathknell] — Play two 3 [M] Mech unit tokens to your base. (When I die, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b2873a352c0158a677255894cec458d1c17709f7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_344(CardRegistry& r) {
    r.registerCard(344, std::make_unique<FerrousForerunner>());
}

} // namespace riftbound
