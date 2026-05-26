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

class Adaptatron : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // Collect legal gears on the board.
        auto gears = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (obj.isGear() && obj.location.has_value()) out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        int conf = confirmOptional(ctx, "Adaptatron: kill a gear?",
                                   [&]() { return !gears().empty(); });
        if (conf == -1) return;  // waiting on agent
        if (conf == 0) return;   // declined / no legal gear
        auto legal = gears();
        if (legal.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Adaptatron: kill which gear", legal);
        if (picked == kInvalidId) return;  // suspended or fizzled
        if (!ctx.state.objectExists(picked)) return;
        ctx.executor.killObject(picked);
        // "If you do" — buff me.
        if (ctx.state.objectExists(ctx.source)) {
            ctx.executor.buffUnit(ctx.source);
            ctx.events.logTrace("ADAPTATRON: killed gear -> buff me");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 56;
        d.def_id = R"RB(ogn-056-298)RB";
        d.name = R"RB(Adaptatron)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-056/298)RB";
        d.collector_number = 56;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Mech)RB", R"RB(Piltover)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I conquer, you may kill a gear. If you do, buff me. (If I don't have a buff, I get a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a3ddb00a2a872eaceb96469739531414aa27455d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_56(CardRegistry& r) {
    r.registerCard(56, std::make_unique<Adaptatron>());
}

} // namespace riftbound
