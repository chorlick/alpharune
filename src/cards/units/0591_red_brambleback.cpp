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

// "[Accelerate] Your conquer effects for conquering here trigger an additional
//  time. When I conquer, [Buff] a friendly unit."

class RedBrambleback : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // ENGINE GAP: "Your conquer effects for conquering here trigger an
    // additional time" has no engine counter analogous to
    // deathknell_double_count for conquer triggers; not wired.
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "[Buff] a friendly unit." Pick a friendly unit on the board.
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == ctx.controller && obj.location.has_value())
                legal.push_back(id);
        }
        GameObjectId picked = pickTarget(ctx, "Red Brambleback: buff a friendly unit", legal);
        if (picked == kInvalidId) return;
        if (!ctx.state.objectExists(picked)) return;
        ctx.executor.buffUnit(picked);
        ctx.events.logTrace("RED BRAMBLEBACK: conquer -> buffed a friendly unit "
                            "(additional-conquer-trigger doubling not engine-wired)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 591;
        d.def_id = R"RB(unl-029-219)RB";
        d.name = R"RB(Red Brambleback)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-029/219)RB";
        d.collector_number = 29;
        d.artist = R"RB(JunHuan)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Accelerate);
        d.ability_text = R"RB([Accelerate] (You may pay [1][R] as an additional cost to have me enter ready.)
Your conquer effects for conquering here trigger an additional time.
When I conquer, [Buff] a friendly unit. (Give it a +1 [M] buff if it doesn't have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/14e9d009985f6a0d8ab63416aee7570ede42c657-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_591(CardRegistry& r) {
    r.registerCard(591, std::make_unique<RedBrambleback>());
}

} // namespace riftbound
