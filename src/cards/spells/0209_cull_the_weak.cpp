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

class CullTheWeak : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Controller's own kill — agent-chosen via pickTarget.
        std::vector<GameObjectId> own;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == ctx.controller &&
                obj.location.has_value()) {
                own.push_back(id);
            }
        }
        if (!own.empty()) {
            GameObjectId picked = pickTarget(ctx, "Cull the Weak (your unit)", own);
            // Suspended for agent decision — chain manager re-enters.
            if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) {
                return;
            }
            if (picked != kInvalidId && ctx.state.objectExists(picked)) {
                ctx.executor.killObject(picked);
            }
        }

        // Opponent's own kill — first available (opponent choice not plumbable).
        PlayerId opp = opponent(ctx.controller);
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == opp && obj.location.has_value()) {
                ctx.executor.killObject(id);
                break;
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 209;
        d.def_id = R"RB(ogn-209-298)RB";
        d.name = R"RB(Cull the Weak)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-209/298)RB";
        d.collector_number = 209;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.ability_text = R"RB(Each player kills one of their units.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/49338a4b31727c6ef50e7dbc54e7004dcd2b6f4c-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_209(CardRegistry& r) {
    r.registerCard(209, std::make_unique<CullTheWeak>());
}

} // namespace riftbound
