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

class AncientWarmonger : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I have [Assault] equal to the number of enemy units here. (+1 [M] while
    //  I'm an attacker for each instance of Assault.)" ([Accelerate] engine-handled.)
    //
    // Granted as an aura [Assault N] with N = the live enemy count: the
    // engine's aura fold adds the +N [M] while I'm an attacker. With no enemy
    // units here there is no Assault at all (a bare Assault effect would count
    // as Assault 1).
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects.onBoard()) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            auto my_bf = self.battlefieldId();
            if (!my_bf) continue;
            int enemies = 0;
            for (auto& [tid, tgt] : state.objects.onBoard()) {
                if (!tgt.isUnit() || tgt.controller == controller) continue;
                if (tgt.battlefieldId() == my_bf) ++enemies;
            }
            if (enemies <= 0) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.keyword = Keyword::Assault;
            ae.keyword_value = enemies;
            self.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 452;
        d.def_id = R"RB(sfd-131-221)RB";
        d.name = R"RB(Ancient Warmonger)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-131/221)RB";
        d.collector_number = 131;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Spirit)RB", R"RB(Noxus)RB"};
        d.energy_cost = 5;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][P] as an additional cost to have me enter ready.)
I have [Assault] equal to the number of enemy units here. (+1 [M] while I'm an attacker for each instance of Assault.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8ba05afcfbe6901fb4bdb736d5e61656a551f668-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_452(CardRegistry& r) {
    r.registerCard(452, std::make_unique<AncientWarmonger>());
}

} // namespace riftbound
