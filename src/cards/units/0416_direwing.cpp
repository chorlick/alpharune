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

class Direwing : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    bool entersReadyOnPlay(const GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (!obj.isUnit() || obj.controller != controller || !obj.location.has_value())
                continue;
            if (obj.card_def_id == cardDefId()) continue;   // "another Dragon"
            if (hasTag(obj, "Dragon")) return true;
        }
        return false;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 416;
        d.def_id = R"RB(sfd-094-221)RB";
        d.name = R"RB(Direwing)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-094/221)RB";
        d.collector_number = 94;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Dragon)RB", R"RB(Demacia)RB"};
        d.energy_cost = 7;
        d.might = 7;
        d.ability_text = R"RB(I enter ready if you control another Dragon.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/52edf5b0f045d7c65360a8eb3c21f0954750cd1a-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_416(CardRegistry& r) {
    r.registerCard(416, std::make_unique<Direwing>());
}

} // namespace riftbound
