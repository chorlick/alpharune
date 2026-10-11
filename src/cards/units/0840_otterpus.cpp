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

class Otterpus : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "If a player would score 1 point from conquering or holding during their
    //  first or second turn, they draw 1 instead." TurnState::early_score_draws_instead
    // is reset every aura pass; GameEngine::scoreConquer / scoreHold consult it.
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.card_def_id == cardDefId() && obj.location.has_value()) {
                state.turn.early_score_draws_instead = true;
                return;
            }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 840;
        d.def_id = R"RB(ven-053-166)RB";
        d.name = R"RB(Otterpus)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-053/166)RB";
        d.collector_number = 53;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Bandle City)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(If a player would score 1 point from conquering or holding during their first or second turn, they draw 1 instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/62f77b80a6d8f28a92c039595baf5943c4e055f0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_840(CardRegistry& r) {
    r.registerCard(840, std::make_unique<Otterpus>());
}

}  // namespace riftbound
