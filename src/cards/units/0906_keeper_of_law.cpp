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

class KeeperOfLaw : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // "I cost [2][Y] less if you control a battlefield with exactly two units
    //  there." [2] off the Energy cost (selfCostReduction) and [Y] off the
    //  Power cost (selfPowerCostReduction); the engine clamps both at 0.
    static bool conditionMet(const GameState& state, PlayerId player) {
        for (const auto& bf : state.battlefields) {
            if (!bf.controller.has_value() || *bf.controller != player) continue;
            int units = 0;
            for (const auto& [id, obj] : state.objects.onBoard()) {
                if (!obj.isUnit()) continue;
                auto obf = obj.battlefieldId();
                if (obf && *obf == bf.id) ++units;
            }
            if (units == 2) return true;
        }
        return false;
    }
    int selfCostReduction(const GameState& state, PlayerId player) const override {
        return conditionMet(state, player) ? 2 : 0;
    }
    int selfPowerCostReduction(const GameState& state, PlayerId player) const override {
        return conditionMet(state, player) ? 1 : 0;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 906;
        d.def_id = R"RB(ven-119-166)RB";
        d.name = R"RB(Keeper of Law)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-119/166)RB";
        d.collector_number = 119;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Shurima)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(I cost [2][Y] less if you control a battlefield with exactly two units there.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/42b965736c715293da57ec23472684506f49ba7f-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_906(CardRegistry& r) {
    r.registerCard(906, std::make_unique<KeeperOfLaw>());
}

}  // namespace riftbound
