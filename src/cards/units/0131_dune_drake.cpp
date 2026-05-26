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

class DuneDrake : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto bf = self.battlefieldId();
        if (!bf) return;
        PlayerId opp = opponent(ctx.controller);
        bool ready_enemy_here = false;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            auto obf = obj.battlefieldId();
            if (!obf || *obf != *bf) continue;
            if (!obj.is_exhausted) { ready_enemy_here = true; break; }
        }
        if (ready_enemy_here) {
            ctx.executor.giveTemporaryMight(ctx.source, 2);
            ctx.events.logTrace("DUNE DRAKE: +2 [M] this turn (ready enemy here)");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 131;
        d.def_id = R"RB(ogn-131-298)RB";
        d.name = R"RB(Dune Drake)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-131/298)RB";
        d.collector_number = 131;
        d.artist = R"RB(Bubble Cat Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Dragon)RB", R"RB(Shurima)RB"};
        d.energy_cost = 5;
        d.might = 5;
        d.ability_text = R"RB(When I attack, give me +2 [M] this turn if there is a ready enemy unit here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ba18bf4a5fa9777a0cb5ae69cc7e6f049bbceaa0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_131(CardRegistry& r) {
    r.registerCard(131, std::make_unique<DuneDrake>());
}

} // namespace riftbound
