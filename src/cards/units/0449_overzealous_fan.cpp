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

class OverzealousFan : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIDefend; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;

        // Read the captured attacker. Validate it still exists and is a
        // legal bounce target (unit, on board, enemy). The card_counters
        // entry persists until reset, but the actual GameObject may have
        // moved / been removed during chain resolution.
        auto get_attacker = [&]() -> GameObjectId {
            if (!ctx.state.objectExists(ctx.source)) return kInvalidId;
            auto& self = ctx.state.getObject(ctx.source);
            auto it = self.card_counters.find("__defend_attacker_id");
            if (it == self.card_counters.end()) return kInvalidId;
            GameObjectId attacker_id = static_cast<GameObjectId>(it->second);
            if (!ctx.state.objectExists(attacker_id)) return kInvalidId;
            auto& a = ctx.state.getObject(attacker_id);
            if (!a.isUnit()) return kInvalidId;
            if (a.controller == ctx.controller) return kInvalidId;
            if (!a.location.has_value()) return kInvalidId;
            return attacker_id;
        };
        auto still_legal = [&]() { return get_attacker() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Overzealous Fan: kill self to bounce attacker?", still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        GameObjectId attacker = get_attacker();
        if (attacker == kInvalidId) return;
        std::string attacker_name = ctx.state.getObject(attacker).name;
        ctx.executor.killObject(ctx.source);
        ctx.executor.bounceToHand(attacker);
        ctx.events.logTrace("OVERZEALOUS FAN: killed self, bounced " + attacker_name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 449;
        d.def_id = R"RB(sfd-128-221)RB";
        d.name = R"RB(Overzealous Fan)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-128/221)RB";
        d.collector_number = 128;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.ability_text = R"RB(When I defend, you may kill me to move an attacking unit to its base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/922d7f337fbf14f0d62d43f69ca2a1e7480dd022-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_449(CardRegistry& r) {
    r.registerCard(449, std::make_unique<OverzealousFan>());
}

} // namespace riftbound
