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

class RumbleScrapper : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "Your Mechs have +1 [M] (including me)."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        GameObjectId self_id = kInvalidId;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller || !self.location.has_value()) continue;
            self_id = sid;
            break;
        }
        if (self_id == kInvalidId) return;
        for (auto& [uid, u] : state.objects) {
            if (!u.isUnit() || u.controller != controller || !u.location.has_value()) continue;
            bool is_mech = false;
            for (auto& tag : u.tags) if (tag == "Mech") { is_mech = true; break; }
            if (!is_mech) continue;  // self is also a Mech, so "including me" holds
            GameObject::AuraEffect ae;
            ae.source = self_id;
            ae.might_bonus = 1;
            u.aura_effects.push_back(ae);
        }
    }

    // "When I hold, play a 3 [M] Mech unit token to your base."
    TriggerType triggerType() const override { return TriggerType::WhenIHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Mech",
                                 3, {"Mech"}, KeywordSet{},
                                 LocationId{BaseLocation{ctx.controller}}, false);
        ctx.events.logTrace("RUMBLE SCRAPPER: hold -> 3[M] Mech token to base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 411;
        d.def_id = R"RB(sfd-089-221)RB";
        d.name = R"RB(Rumble, Scrapper)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-089/221)RB";
        d.collector_number = 89;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Mech)RB", R"RB(Yordle)RB", R"RB(Rumble)RB", R"RB(Bandle City)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Your Mechs have +1 [M] (including me).
When I hold, play a 3 [M] Mech unit token to your base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6528aa0656cac59ff7299bc8bdac371238038080-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_411(CardRegistry& r) {
    r.registerCard(411, std::make_unique<RumbleScrapper>());
}

} // namespace riftbound
