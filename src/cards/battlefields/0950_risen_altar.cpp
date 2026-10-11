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

class RisenAltar : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Empower] costs of your units here cost [1] or [A] less."
    // "Your" is each Altar's own controller: the hook is called once per slot
    // holding this card and is not told which, so it sets every Altar's
    // discount for that Altar's controller (idempotent).
    // GameObject::empower_cost_reduction takes [1] off the Empower cost's
    // Energy (Card::activationCostReductionFor) and, where there is no Energy
    // to take — Gangplank, Naval's [O][O] — one Power instead
    // (Card::activationPowerReductionFor).
    void applyPassiveAura(GameState& state, PlayerId /*controller*/) const override {
        for (auto& bf : state.battlefields) {
            if (!state.objectExists(bf.card_object_id)) continue;
            if (state.getObject(bf.card_object_id).card_def_id != cardDefId()) continue;
            if (!bf.controller.has_value()) continue;
            bf.empower_cost_reduction[playerIndex(*bf.controller)] = 1;
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 950;
        d.def_id = R"RB(ven-163-166)RB";
        d.name = R"RB(Risen Altar)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-163/166)RB";
        d.collector_number = 163;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] costs of your units here cost [1] or [A] less.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/47402ed9d5eebb9e38176315bdaf1a556845eef7-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_950(CardRegistry& r) {
    r.registerCard(950, std::make_unique<RisenAltar>());
}

}  // namespace riftbound
