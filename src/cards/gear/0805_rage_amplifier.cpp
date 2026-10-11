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

class RageAmplifier : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "[Empower] [6][R]"
    EmpowerSpec empowerSpec() const override { return empowerCost(6, 1, Domain::Fury); }
    // "Your units have +1 [M]. If I'm [Empowered], they have +2 [M] instead."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller,
            [&](GameObjectId sid, GameObject& self) {
                const int bonus = self.isEmpowered() ? 2 : 1;
                for (auto& [uid, u] : state.objects.onBoard()) {
                    if (!u.isUnit() || u.controller != controller) continue;
                    if (!u.location.has_value()) continue;
                    auraMight(u, sid, bonus);
                }
            });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 805;
        d.def_id = R"RB(ven-018-166)RB";
        d.name = R"RB(Rage Amplifier)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-018/166)RB";
        d.collector_number = 18;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [6][R] ([6][R]: Empower this. Use only if not Empowered.)
Your units have +1 [M]. If I'm [Empowered], they have +2 [M] instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/83a7cd497dd42f535f8f0adc72c3c4324f9aec14-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_805(CardRegistry& r) {
    r.registerCard(805, std::make_unique<RageAmplifier>());
}

}  // namespace riftbound
