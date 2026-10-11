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

class HeimerdingerInventor : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I have all [E] abilities of all friendly legends, units, and gear."
    // Wired via the aura-granted-ability pipeline: applyPassiveAura appends
    // GrantedAbilityRefs naming each friendly legend/unit/gear card onto each
    // Heimerdinger, once per card name however many copies are in play. Every
    // ability index is named (the action generator skips an index the granter
    // doesn't have) and each ref is marked exhaust_cost_only, so only the
    // abilities whose cost includes [E] are granted. Activation routes to the
    // granting card's onActivate with Heimer as the bearer (ctx.source), and
    // that card's own gates are asked of Heimer.
    // APPROX: abilities that deeply reference their own source object act as
    // Heimer's.
    static constexpr int kMaxGrantedAbilityIndex = 4;   // no card prints more abilities
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        std::vector<GameObjectId> heimers;
        for (auto& [id, obj] : state.objects)
            if (obj.card_def_id == cardDefId() && obj.controller == controller &&
                obj.location.has_value())
                heimers.push_back(id);
        if (heimers.empty()) return;
        std::vector<CardDefId> granters;
        for (auto& [id, obj] : state.objects) {
            if (obj.controller != controller || obj.card_def_id == kInvalidId) continue;
            if (obj.card_def_id == cardDefId()) continue;  // not myself / other Heimers
            const bool on_board = obj.location.has_value();
            const bool legend = obj.zone == ZoneType::LegendZone;
            if (!on_board && !legend) continue;
            if (!(obj.isUnit() || obj.isGear() || obj.isLegend())) continue;
            if (std::find(granters.begin(), granters.end(), obj.card_def_id) == granters.end())
                granters.push_back(obj.card_def_id);
        }
        std::sort(granters.begin(), granters.end());
        for (auto hid : heimers) {
            auto& h = state.getObject(hid);
            for (auto def_id : granters)
                for (int idx = 0; idx < kMaxGrantedAbilityIndex; ++idx)
                    h.granted_abilities.push_back({def_id, idx, /*exhaust_cost_only=*/true});
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 111;
        d.def_id = R"RB(ogn-111-298)RB";
        d.name = R"RB(Heimerdinger, Inventor)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-111/298)RB";
        d.collector_number = 111;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Yordle)RB", R"RB(Heimerdinger)RB", R"RB(Piltover)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(I have all [E] abilities of all friendly legends, units, and gear.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5b14a5f9d567c90329c151a8cc72d870b47b1434-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_111(CardRegistry& r) {
    r.registerCard(111, std::make_unique<HeimerdingerInventor>());
}

} // namespace riftbound
