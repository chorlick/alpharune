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

class AllayEagerAdmirer : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller) continue;
            if (!self.isAtBattlefield()) continue;     // "While I'm at a battlefield"
            auto bf = self.battlefieldId();
            if (!bf) continue;
            for (auto& [tid, tgt] : state.objects) {
                if (tid == sid) continue;               // "other"
                if (!tgt.isUnit() || tgt.controller != controller) continue; // "your units"
                auto tbf = tgt.battlefieldId();
                if (!tbf || *tbf != *bf) continue;      // "here"
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.keyword = Keyword::Deflect;
                tgt.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 603;
        d.def_id = R"RB(unl-041-219)RB";
        d.name = R"RB(Allay, Eager Admirer)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-041/219)RB";
        d.collector_number = 41;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Yordle)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB([Deflect] (Opponents must pay [A] to choose me with a spell or ability.)
While I'm at a battlefield, your other units here have [Deflect].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bf1045b559b20d3e86383a07de54cd1893e5a94b-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_603(CardRegistry& r) {
    r.registerCard(603, std::make_unique<AllayEagerAdmirer>());
}

} // namespace riftbound
