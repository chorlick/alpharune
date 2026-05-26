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

class BlitzcrankImpassive : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIHold};
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        // WhenIHold (delayed-fire path leaves firing_trigger=None, so treat
        // anything that isn't WhenYouPlayMe as the hold branch only when the
        // unit is actually holding a battlefield — but here both triggers are
        // event-driven and carry firing_trigger, so branch on it directly).
        if (ctx.firing_trigger == TriggerType::WhenIHold) {
            ctx.executor.bounceToHand(ctx.source);
            ctx.events.logTrace("BLITZCRANK: returned to owner's hand on hold");
            return;
        }

        // WhenYouPlayMe: pull an enemy unit to my battlefield.
        auto& self = ctx.state.getObject(ctx.source);
        auto my_bf = self.battlefieldId();
        if (!my_bf) return;  // not at a BF — trigger condition not met

        auto find_enemy_elsewhere = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                if (obj.battlefieldId() == my_bf) continue;
                if (!obj.location.has_value()) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_enemy_elsewhere() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Blitzcrank: pull an enemy unit to my battlefield?", still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        auto target = find_enemy_elsewhere();
        if (target == kInvalidId) return;
        std::string name = ctx.state.getObject(target).name;
        ctx.executor.moveToBattlefield(target, *my_bf);
        ctx.events.logTrace("BLITZCRANK: pulled " + name + " to BF#" +
                             std::to_string(static_cast<int>(*my_bf)));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 67;
        d.def_id = R"RB(ogn-067-298)RB";
        d.name = R"RB(Blitzcrank, Impassive)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-067/298)RB";
        d.collector_number = 67;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Mech)RB", R"RB(Blitzcrank)RB", R"RB(Zaun)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Tank] (I must be assigned combat damage first.)
When you play me to a battlefield, you may move an enemy unit to here.
When I hold, return me to my owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/654dcc4aef0a0b5a0c6e928d7aae397a52c3ab17-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_67(CardRegistry& r) {
    r.registerCard(67, std::make_unique<BlitzcrankImpassive>());
}

} // namespace riftbound
