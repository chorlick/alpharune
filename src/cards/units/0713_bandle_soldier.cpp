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

class BandleSoldier : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool requiresLevel() const override { return true; }
    int levelThreshold() const override { return 3; }
    // "[Level 3] I enter ready." — only enter ready while you have 3+ XP.
    bool entersReadyOnPlay(const GameState& state,
                           PlayerId controller) const override {
        return state.player(controller).xp >= levelThreshold();
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 713;
        d.def_id = R"RB(unl-151-219)RB";
        d.name = R"RB(Bandle Soldier)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-151/219)RB";
        d.collector_number = 151;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 5;
        d.keywords.set(Keyword::Level);
        d.ability_text = R"RB([Level 3][>] I enter ready. (While you have 3+ XP, get the effect.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e43b54f22e0a038ee1f94ba2e99f80dd89999d8e-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_713(CardRegistry& r) {
    r.registerCard(713, std::make_unique<BandleSoldier>());
}

} // namespace riftbound
