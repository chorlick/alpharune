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

class DuneSurfer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "You ignore [Tank] while assigning combat damage here." Continuous flag on
    // my battlefield, re-asserted every aura pass (reset in recalculateAuras).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.isUnit()) continue;
            auto bf = obj.battlefieldId();
            if (!bf) continue;
            for (auto& b : state.battlefields)
                if (b.id == *bf) b.ignore_tank[playerIndex(controller)] = true;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 791;
        d.def_id = R"RB(ven-004-166)RB";
        d.name = R"RB(Dune Surfer)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-004/166)RB";
        d.collector_number = 4;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(You ignore [Tank] while assigning combat damage here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/acab9ec71d58f1e109cd8879c0fef0bdeb866432-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_791(CardRegistry& r) {
    r.registerCard(791, std::make_unique<DuneSurfer>());
}

}  // namespace riftbound
