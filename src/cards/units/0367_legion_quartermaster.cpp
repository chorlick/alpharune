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

class LegionQuartermaster : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "As an additional cost to play me, return a friendly gear to its
    // owner's hand." A mandatory additional cost (CR 356.2.a): without a
    // friendly gear I cannot be played, and the gear — the player's choice —
    // goes back as I am played, before my cost is paid and before I enter.
    static std::vector<GameObjectId> friendlyGear(const GameState& state, PlayerId controller) {
        std::vector<GameObjectId> out;
        for (auto& [id, obj] : state.objects.onBoard())
            if (obj.isGear() && obj.controller == controller && obj.location.has_value())
                out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }
    bool canBePlayed(const GameState& state, PlayerId player) const override {
        return !friendlyGear(state, player).empty();
    }
    AdditionalPlayCost additionalPlayCost(const GameState& state, const CardDB& db,
                                          PlayerId player, GameObjectId self) const override {
        (void)db; (void)self;
        AdditionalPlayCost c;
        c.exists = true;
        c.mandatory = true;
        c.payable = !friendlyGear(state, player).empty();
        return c;
    }
    PlayCostAdjust payAdditionalPlayCost(
        CardContext& ctx,
        const std::function<bool(const PlayCostAdjust&)>& affordable) override {
        (void)affordable;
        const GameObjectId gear = ctx.executor.chooseObjectNow(
            ctx.controller, friendlyGear(ctx.state, ctx.controller));
        if (gear != kInvalidId) {
            ctx.executor.bounceToHand(gear);
            RIFT_TRACE(ctx.events, "LEGION QUARTERMASTER: returned a friendly gear to hand "
                                "(additional cost)");
        }
        return {};
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 367;
        d.def_id = R"RB(sfd-044-221)RB";
        d.name = R"RB(Legion Quartermaster)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-044/221)RB";
        d.collector_number = 44;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Trifarian)RB", R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(As an additional cost to play me, return a friendly gear to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c09cc35ffc98b46f919c09e09a93b603d63eb73e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_367(CardRegistry& r) {
    r.registerCard(367, std::make_unique<LegionQuartermaster>());
}

} // namespace riftbound
