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

class DazzlingAurora : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::AtEndOfTurn; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ri = ctx.state.chain.resuming.value();

        // First-entry-only steps: reveal from deck until a unit, recycle
        // the rest, banish the unit. Stash the unit id in ri.targets so
        // subsequent re-entries from pickMode don't re-reveal. Empty
        // ri.targets ≡ "haven't entered yet"; a single kInvalidId entry
        // ≡ "entered, but no unit was found — nothing left to do".
        if (ri.targets.empty()) {
            auto [u, rest] = ctx.executor.revealUntil(ctx.controller, CardType::Unit);
            if (!rest.empty()) ctx.executor.recycleCards(ctx.controller, rest);
            if (u == kInvalidId || !ctx.state.objectExists(u)) {
                ri.targets.push_back(kInvalidId);
                return;
            }
            ctx.executor.banishObject(u);
            // banishObject pushed it to banishment; playIgnoringCost will
            // re-zone it, so remove the duplicate banishment entry now.
            auto& bz = ctx.state.player(ctx.state.getObject(u).owner).banishment;
            bz.erase(std::remove(bz.begin(), bz.end(), u), bz.end());
            ri.targets.push_back(u);
        }
        GameObjectId unit_id = ri.targets.front();
        if (unit_id == kInvalidId) return;

        // Per CR 355.2.a, the controller picks the landing location: base
        // or any battlefield they control. pickMode publishes one option
        // per legal location so the agent records the decision (mandatory
        // even when only base is legal — engine-wide rule).
        std::vector<LocationId> locations;
        std::vector<std::string> labels;
        locations.push_back(LocationId{BaseLocation{ctx.controller}});
        labels.push_back("base");
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.controller.has_value() && *bf.controller == ctx.controller) {
                locations.push_back(LocationId{BattlefieldLocation{bf.id}});
                std::string bf_name = "BF#" + std::to_string(bf.id);
                if (ctx.state.objectExists(bf.card_object_id)) {
                    bf_name = ctx.state.getObject(bf.card_object_id).name;
                }
                labels.push_back(bf_name);
            }
        }
        uint32_t legal_mask = (locations.size() >= 32)
            ? 0xFFFFFFFFu
            : ((1u << locations.size()) - 1);
        int chosen = pickMode(ctx, "Aurora: where to play unit",
                               static_cast<int>(locations.size()),
                               labels, legal_mask);
        if (chosen == -1) return;  // yielded for agent input
        if (chosen < 0 || static_cast<size_t>(chosen) >= locations.size()) {
            chosen = 0;
        }
        ctx.executor.playIgnoringCost(ctx.controller, unit_id,
                                       locations[chosen]);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 160;
        d.def_id = R"RB(ogn-160-298)RB";
        d.name = R"RB(Dazzling Aurora)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-160/298)RB";
        d.collector_number = 160;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Body};
        d.energy_cost = 9;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(At the end of your turn, reveal cards from the top of your Main Deck until you reveal a unit and banish it. Play it, ignoring its cost, and recycle the rest.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c023856a50c75058465f283d181277ce265ba108-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_160(CardRegistry& r) {
    r.registerCard(160, std::make_unique<DazzlingAurora>());
}

} // namespace riftbound
