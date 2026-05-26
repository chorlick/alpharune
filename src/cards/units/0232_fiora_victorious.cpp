#include "cards/card.h"
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

class FioraVictorious : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller) continue;
            if (!self.location.has_value()) continue;
            if (self.current_might < 5) continue;   // "While I'm [Mighty]"
            for (Keyword kw : {Keyword::Deflect, Keyword::Ganking, Keyword::Shield}) {
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.keyword = kw;
                self.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 232;
        d.def_id = R"RB(ogn-232-298)RB";
        d.name = R"RB(Fiora, Victorious)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-232/298)RB";
        d.collector_number = 232;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Fiora)RB", R"RB(Demacia)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.shield_value = 1;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Ganking);
        d.keywords.set(Keyword::Shield);
        d.ability_text = R"RB(While I'm [Mighty], I have [Deflect], [Ganking], and [Shield]. (I'm Mighty while I have 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/24d4d1997e6a5b145412e402fc69399d590ecda0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_232(CardRegistry& r) {
    r.registerCard(232, std::make_unique<FioraVictorious>());
}

} // namespace riftbound
