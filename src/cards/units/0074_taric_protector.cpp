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

class TaricProtector : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "Other friendly units here have [Shield]." Static aura: for each on-board
    //  instance of me, grant Shield to every OTHER friendly unit sharing my
    //  location (base or battlefield). Mirrors Forecaster's keyword-grant aura.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            if (!self.location.has_value()) continue;
            for (auto& [uid, u] : state.objects) {
                if (uid == sid) continue;             // "Other"
                if (!u.isUnit() || u.controller != controller) continue;
                if (!u.location.has_value() || *u.location != *self.location) continue;  // "here"
                GameObject::AuraEffect ae;
                ae.source = sid;
                ae.keyword = Keyword::Shield;
                ae.keyword_value = 1;
                u.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 74;
        d.def_id = R"RB(ogn-074-298)RB";
        d.name = R"RB(Taric, Protector)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-074/298)RB";
        d.collector_number = 74;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Taric)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.)
Other friendly units here have [Shield].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7d08e3f64401cb87b8a0564a1cbe6fc94aee03a7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_74(CardRegistry& r) {
    r.registerCard(74, std::make_unique<TaricProtector>());
}

} // namespace riftbound
