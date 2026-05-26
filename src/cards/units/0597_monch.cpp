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

class Monch : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    static bool oppHasStunned(const GameState& state, PlayerId controller) {
        PlayerId opp = opponent(controller);
        for (auto& [id, obj] : state.objects) {
            if (obj.controller != opp) continue;
            if (!obj.isUnit()) continue;
            if (obj.is_stunned) return true;
        }
        return false;
    }
    int selfCostReduction(const GameState& state, PlayerId controller) const override {
        return oppHasStunned(state, controller) ? 2 : 0;
    }
    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        return oppHasStunned(state, controller);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 597;
        d.def_id = R"RB(unl-035-219)RB";
        d.name = R"RB(Monch)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-035/219)RB";
        d.collector_number = 35;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Bandle City)RB"};
        d.energy_cost = 6;
        d.might = 6;
        d.ability_text = R"RB(If an opponent controls a stunned unit, I cost [2] less and enter ready.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ef10724add3b6d9e69415cb194e895f6ea9969b7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_597(CardRegistry& r) {
    r.registerCard(597, std::make_unique<Monch>());
}

} // namespace riftbound
