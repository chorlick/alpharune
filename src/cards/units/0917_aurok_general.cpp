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

class AurokGeneral : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ── behaviour (Vendetta batch C) ──
    // "[Empower] [3][Y]"
    EmpowerSpec empowerSpec() const override { return empowerCost(3, 1, Domain::Order); }
    // "[Empowered][>] Your units that are [Empowered] have +2 [M] (including me)."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachEmpoweredSelf(*this, state, controller, [&](GameObjectId sid, GameObject&) {
            for (auto& [id, obj] : state.objects.onBoard()) {
                if (!obj.isUnit() || obj.controller != controller) continue;
                if (!obj.location.has_value() || !obj.isEmpowered()) continue;
                auraMight(obj, sid, 2);
            }
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 917;
        d.def_id = R"RB(ven-130-166)RB";
        d.name = R"RB(Aurok General)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-130/166)RB";
        d.collector_number = 130;
        d.artist = R"RB(Slawomir Maniak)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [3][Y] ([3][Y]: Empower me. Use only if not Empowered.)
[Empowered][>] Your units that are [Empowered] have +2 [M] (including me).)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/2324531e93383ce0d7efbe67cbf7ee226127dea2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_917(CardRegistry& r) {
    r.registerCard(917, std::make_unique<AurokGeneral>());
}

}  // namespace riftbound
