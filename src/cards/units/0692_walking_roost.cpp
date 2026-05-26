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

class WalkingRoost : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, choose an opponent. They play a 1 [M] Bird unit token
    //  with [Deflect]." ([Deflect] on self engine-handled.) In 1v1 the chosen
    //  opponent is the single opponent; the token is theirs, at their base.
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        PlayerId opp = opponent(ctx.controller);
        KeywordSet kw; kw.set(Keyword::Deflect);
        ctx.executor.createToken(opp, CardType::Unit, "Bird",
                                  /*might=*/1, /*tags=*/{"Bird"}, kw,
                                  LocationId{BaseLocation{opp}},
                                  /*enter_ready=*/false);
        ctx.events.logTrace("WALKING ROOST: opponent plays a 1 [M] Bird token with [Deflect]");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 692;
        d.def_id = R"RB(unl-130-219)RB";
        d.name = R"RB(Walking Roost)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-130/219)RB";
        d.collector_number = 130;
        d.artist = R"RB(Xavier Leroux)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Shadow Isles)RB"};
        d.energy_cost = 5;
        d.might = 6;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
When you play me, choose an opponent. They play a 1 [M] Bird unit token with [Deflect].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/d9b344b4b29b2baab0a1dc2e16a6e946192edaf5-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_692(CardRegistry& r) {
    r.registerCard(692, std::make_unique<WalkingRoost>());
}

} // namespace riftbound
