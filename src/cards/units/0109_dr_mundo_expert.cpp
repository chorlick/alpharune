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

class DrMundoExpert : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "My Might is increased by the number of cards in your trash."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        int trash_count = static_cast<int>(state.player(controller).trash.size());
        if (trash_count <= 0) return;
        for (auto& [sid, self] : state.objects) {
            if (self.card_def_id != cardDefId()) continue;
            if (self.controller != controller || !self.location.has_value()) continue;
            GameObject::AuraEffect ae;
            ae.source = sid;
            ae.might_bonus = trash_count;
            self.aura_effects.push_back(ae);
        }
    }

    // "At the start of your Beginning Phase, recycle 3 from your trash."
    TriggerType triggerType() const override { return TriggerType::AtStartOfBeginning; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        std::vector<GameObjectId> to_recycle;
        // Recycle up to 3 from trash (front of trash = oldest).
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            to_recycle.push_back(cid);
            if (to_recycle.size() >= 3) break;
        }
        if (to_recycle.empty()) return;
        for (auto cid : to_recycle) {
            auto it = std::find(ps.trash.begin(), ps.trash.end(), cid);
            if (it != ps.trash.end()) ps.trash.erase(it);
        }
        ctx.executor.recycleCards(ctx.controller, to_recycle);
        ctx.events.logTrace("DR. MUNDO: recycled " +
                             std::to_string(to_recycle.size()) + " from trash");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 109;
        d.def_id = R"RB(ogn-109-298)RB";
        d.name = R"RB(Dr. Mundo, Expert)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-109/298)RB";
        d.collector_number = 109;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Dr. Mundo)RB", R"RB(Zaun)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 6;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(My Might is increased by the number of cards in your trash.
At the start of your Beginning Phase, recycle 3 from your trash.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/cf51fe8bedf5139e8cc1fc062969949ea7b121f0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_109(CardRegistry& r) {
    r.registerCard(109, std::make_unique<DrMundoExpert>());
}

} // namespace riftbound
