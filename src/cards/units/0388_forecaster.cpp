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

class Forecaster : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Your Mechs have [Vision]." Granted by aura, once per Forecaster on the
    // board (each grant is an instance of [Vision], CR 817.2).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId self_id, GameObject& self) {
            if (!self.location.has_value()) return;
            for (auto& [uid, u] : state.objects) {
                if (!u.isUnit() || u.controller != controller || !u.location.has_value())
                    continue;
                if (std::find(u.tags.begin(), u.tags.end(), "Mech") == u.tags.end()) continue;
                auraKeyword(u, self_id, Keyword::Vision);
            }
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 388;
        d.def_id = R"RB(sfd-065-221)RB";
        d.name = R"RB(Forecaster)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-065/221)RB";
        d.collector_number = 65;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mech)RB", R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 2;
        d.might = 2;
        // No printed [Vision] of its own: it has the keyword, once, from its
        // own continuous effect (it is a Mech) — applyPassiveAura above. A
        // keyword here as well would be a second instance (CR 817.2).
        d.ability_text = R"RB(Your Mechs have [Vision]. (When you play us, look at the top card of your Main Deck. You may recycle it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3f58f0c58503e4f8caecb7ea7f77b478a1e92961-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_388(CardRegistry& r) {
    r.registerCard(388, std::make_unique<Forecaster>());
}

} // namespace riftbound
