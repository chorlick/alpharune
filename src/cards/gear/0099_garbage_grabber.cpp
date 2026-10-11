#include "cards/card.h"
#include "cards/card_helpers.h"
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

class GarbageGrabber : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    // "Recycle 3 from your trash, [1], [E]: Draw 1." Energy + exhaust are the
    // engine-paid ActivationCost. "Recycle 3 from your trash" is a cost too:
    // it is paid as the ability is finalized (kFinalizeActivate + costDueNow),
    // with the three cards its controller picks, before anyone can respond.
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return {.exhaust = true, .energy = 1};
    }
    bool canActivateAbility(const GameState& state,
                            PlayerId controller) const override {
        const auto& ps = state.player(controller);
        int n = 0;
        for (auto cid : ps.trash) if (state.objectExists(cid)) ++n;
        return n >= 3;
    }
    void onActivate(CardContext& ctx,
                    const std::vector<GameObjectId>& /*targets*/) override {
        if (costDueNow(ctx)) {
            auto& ps = ctx.state.player(ctx.controller);
            std::vector<GameObjectId> in_trash;
            for (auto cid : ps.trash)
                if (ctx.state.objectExists(cid)) in_trash.push_back(cid);
            if (in_trash.size() < 3) return;  // cost cannot be paid
            std::vector<GameObjectId> to_recycle;
            if (in_trash.size() == 3) {
                to_recycle = in_trash;
            } else {
                for (size_t i = 0; i < 3; ++i) {
                    std::vector<GameObjectId> rest;
                    for (auto cid : in_trash)
                        if (std::find(to_recycle.begin(), to_recycle.end(), cid) == to_recycle.end())
                            rest.push_back(cid);
                    const auto pick = selectObject(
                        ctx, "Garbage Grabber: recycle from your trash (" +
                             std::to_string(i + 1) + " of 3)", rest, 7 + 2 * i);
                    if (!pick.has_value() || *pick == kInvalidId) return;   // waiting on the agent
                    if (std::find(rest.begin(), rest.end(), *pick) == rest.end()) return;
                    to_recycle.push_back(*pick);
                }
            }
            for (auto cid : to_recycle)
                ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), cid), ps.trash.end());
            ctx.executor.recycleCards(ctx.controller, to_recycle);
            ctx.events.logTrace("GARBAGE GRABBER: recycled 3 from trash");
        }
        if (finalizingChoices(ctx)) return;   // costs end here (CR 355)
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("GARBAGE GRABBER: draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 99;
        d.def_id = R"RB(ogn-099-298)RB";
        d.name = R"RB(Garbage Grabber)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-099/298)RB";
        d.collector_number = 99;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Mind};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(Recycle 3 from your trash, [1], [E]: Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/199f21237aeee2582904463a15ec62ce29452c10-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_99(CardRegistry& r) {
    r.registerCard(99, std::make_unique<GarbageGrabber>());
}

} // namespace riftbound
