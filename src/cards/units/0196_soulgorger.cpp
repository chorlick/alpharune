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

class Soulgorger : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, you may play a unit from your trash, ignoring its
    // Energy cost." (Power cost approximated as free — same convention as
    // Spectral Matron / Fizz.)
    std::vector<GameObjectId> trashUnits(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        const auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : ps.trash) {
            if (!ctx.state.objectExists(cid)) continue;
            if (ctx.state.getObject(cid).isUnit()) out.push_back(cid);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto still_legal = [&]() { return !trashUnits(ctx).empty(); };
        int conf = confirmOptional(ctx, "Soulgorger: play a unit from trash?",
                                   still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / nothing eligible
        auto eligible = trashUnits(ctx);
        if (eligible.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Soulgorger (unit from trash)", eligible);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.trash.begin(), ps.trash.end(), picked);
        if (it != ps.trash.end()) ps.trash.erase(it);
        ctx.executor.playIgnoringCost(ctx.controller, picked);
        ctx.events.logTrace("SOULGORGER: played a unit from trash for free");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 196;
        d.def_id = R"RB(ogn-196-298)RB";
        d.name = R"RB(Soulgorger)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-196/298)RB";
        d.collector_number = 196;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Spirit)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 8;
        d.power_cost = 2;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, you may play a unit from your trash, ignoring its Energy cost. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/439c4fa7c8c47cba41df8bce3fe3c8068d297382-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_196(CardRegistry& r) {
    r.registerCard(196, std::make_unique<Soulgorger>());
}

} // namespace riftbound
