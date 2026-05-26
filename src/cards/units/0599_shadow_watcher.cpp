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

class ShadowWatcher : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ENGINE GAP: "If a friendly unit died during your Beginning Phase this
    // turn, I enter ready." Requires tracking unit deaths scoped to the
    // controller's Beginning Phase this turn; no such per-phase death counter
    // exists on GameState/TurnState, and entersReadyOnPlay is play-time. Needs
    // an engine signal (deaths-during-beginning-phase flag). Not wired.
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 599;
        d.def_id = R"RB(unl-037-219)RB";
        d.name = R"RB(Shadow Watcher)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-037/219)RB";
        d.collector_number = 37;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 5;
        d.ability_text = R"RB(If a friendly unit died during your Beginning Phase this turn, I enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f6f5ff413efc57d7948b4b9a46a4bba6eebd39d2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_599(CardRegistry& r) {
    r.registerCard(599, std::make_unique<ShadowWatcher>());
}

} // namespace riftbound
