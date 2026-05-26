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

// "You may exhaust your legend as an additional cost to play me. When you play
//  me, if you paid the additional cost, move any number of your units to an
//  open battlefield."
// The "exhaust your legend" additional cost is not expressible via
// OptionalAdditionalCost (energy/power only), so it is modeled inline at
// WhenYouPlayMe: the player may choose to exhaust their (ready) legend, and if
// they do, all their units are moved to the first open (uncontrolled)
// battlefield.

class BardMercurial : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        doMove(ctx);
    }

private:
    static GameObjectId findReadyLegend(const GameState& state, PlayerId controller) {
        for (auto& [id, obj] : state.objects) {
            if (obj.controller != controller) continue;
            if (obj.card_type != CardType::Legend) continue;
            if (obj.is_exhausted) continue;
            return id;
        }
        return kInvalidId;
    }
    static BattlefieldId findOpenBattlefield(const GameState& state) {
        for (auto& bf : state.battlefields) {
            if (!bf.controller.has_value()) return bf.id;  // uncontrolled = open
        }
        return kInvalidId;
    }

    void doMove(CardContext& ctx) {
        if (!ctx.state.objectExists(ctx.source)) return;

        int conf = confirmOptional(ctx,
            "Bard, Mercurial: exhaust your legend to move your units to an open battlefield?",
            [&]() {
                return findReadyLegend(ctx.state, ctx.controller) != kInvalidId &&
                       findOpenBattlefield(ctx.state) != kInvalidId;
            });
        if (conf == -1) return;  // waiting for agent
        if (conf == 0) return;

        GameObjectId legend = findReadyLegend(ctx.state, ctx.controller);
        BattlefieldId dest = findOpenBattlefield(ctx.state);
        if (legend == kInvalidId || dest == kInvalidId) return;
        ctx.executor.exhaustObject(legend);
        // Move all of the player's units to the open battlefield.
        std::vector<GameObjectId> to_move;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value()) continue;
            to_move.push_back(id);
        }
        for (auto id : to_move) {
            if (ctx.state.objectExists(id)) ctx.executor.moveToBattlefield(id, dest);
        }
        ctx.events.logTrace("BARD, MERCURIAL: exhausted legend -> moved units to open BF");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 540;
        d.def_id = R"RB(sfd-228-221)RB";
        d.name = R"RB(Bard, Mercurial)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-228/221)RB";
        d.collector_number = 228;
        d.artist = R"RB(Felicia Chen)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Bard)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(You may exhaust your legend as an additional cost to play me.
When you play me, if you paid the additional cost, move any number of your units to an open battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9c33fbc2dc78bb6af3d31fab0c0ca58e1141141d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_540(CardRegistry& r) {
    r.registerCard(540, std::make_unique<BardMercurial>());
}

} // namespace riftbound
