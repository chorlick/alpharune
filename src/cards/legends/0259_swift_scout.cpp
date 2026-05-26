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

class SwiftScout : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    // "You may pay [1] to hide a card with [Hidden] instead of [A]."
    //   ENGINE GAP: hide-cost is an alternative-cost modifier with no card-side
    //   hook; left unmodeled.
    // "[1], [E]: Put a Teemo unit you own into your hand from your Champion Zone
    //   or the board."
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return ActivationCost{.exhaust = true, .energy = 1};
    }

    std::vector<GameObjectId> teemoUnits(const GameState& state, PlayerId controller) const {
        std::vector<GameObjectId> out;
        auto& ps = state.player(controller);
        // Champion Zone occupant.
        if (ps.champion_zone != kInvalidId && state.objectExists(ps.champion_zone)) {
            const auto& cz = state.getObject(ps.champion_zone);
            if (cz.isUnit() && hasTag(cz, "Teemo")) out.push_back(ps.champion_zone);
        }
        // On board.
        for (auto& [id, obj] : state.objects) {
            if (obj.owner != controller) continue;
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (hasTag(obj, "Teemo")) out.push_back(id);
        }
        return out;
    }

    std::vector<GameObjectId> enumerateLegalTargets(
        const GameState& state, PlayerId controller) const override {
        return teemoUnits(state, controller);
    }

    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        GameObjectId picked;
        if (!targets.empty()) {
            picked = targets[0];
        } else {
            auto legal = teemoUnits(ctx.state, ctx.controller);
            picked = pickTarget(ctx, "Swift Scout (Teemo unit to hand)", legal);
            if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7) {
                return;  // suspended for agent decision
            }
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto& ps = ctx.state.player(ctx.controller);
        auto& obj = ctx.state.getObject(picked);
        if (picked == ps.champion_zone) {
            // From Champion Zone: move directly to hand.
            ps.champion_zone = kInvalidId;
            obj.zone = ZoneType::Hand;
            obj.location = std::nullopt;
            ps.hand.push_back(picked);
            ctx.events.logTrace("SWIFT SCOUT: return Teemo from Champion Zone to hand");
        } else {
            ctx.executor.bounceToHand(picked);
            ctx.events.logTrace("SWIFT SCOUT: return Teemo from board to hand");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 259;
        d.def_id = R"RB(ogn-263-298)RB";
        d.name = R"RB(Swift Scout)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-263/298)RB";
        d.collector_number = 263;
        d.artist = R"RB(Shawn Lee)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Chaos};
        d.tags = {R"RB(Teemo)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB(You may pay [1] to hide a card with [Hidden] instead of [A].
[1], [E]: Put a Teemo unit you own into your hand from your Champion Zone or the board.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/9723181e3392bb61c2aabc804a44f7b0558cedf1-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_259(CardRegistry& r) {
    r.registerCard(259, std::make_unique<SwiftScout>());
}

} // namespace riftbound
