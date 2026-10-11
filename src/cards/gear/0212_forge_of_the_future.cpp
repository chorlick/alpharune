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

class ForgeOfTheFuture : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play this, play a 1 [M] Recruit unit token at your base."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        LocationId loc{BaseLocation{ctx.controller}};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Recruit",
                                 1, {"Recruit"}, KeywordSet{}, loc, false);
        ctx.events.logTrace("FORGE OF THE FUTURE: played -> 1[M] Recruit to base");
    }

    // "Kill this: Recycle up to 4 cards from trashes."
    // "Kill this" is the whole cost (no [E]): it is paid as the ability is
    // finalized (kFinalizeActivate + costDueNow). The cards — from either
    // player's trash, each to its owner's Main Deck — are picked one at a time
    // as the ability resolves, stopping at "None".
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        return {ActivatedAbility{}};
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        if (costDueNow(ctx)) {
            if (!ctx.state.objectExists(ctx.source) ||
                !ctx.state.getObject(ctx.source).location.has_value()) return;   // nothing to kill
            ctx.executor.killObject(ctx.source);
        }
        if (finalizingChoices(ctx)) return;   // costs end here (CR 355)
        auto in_trashes = [&]() {
            std::vector<GameObjectId> out;
            for (auto pid : {PlayerId::Player1, PlayerId::Player2})
                for (auto cid : ctx.state.player(pid).trash)
                    if (ctx.state.objectExists(cid)) out.push_back(cid);
            return out;
        };
        std::vector<GameObjectId> picks;
        for (size_t i = 0; i < 4; ++i) {
            std::vector<GameObjectId> rest;
            for (auto cid : in_trashes())
                if (std::find(picks.begin(), picks.end(), cid) == picks.end()) rest.push_back(cid);
            if (rest.empty()) break;
            const auto pick = selectObject(ctx, "Forge of the Future: recycle a card from a trash (" +
                                                std::to_string(i + 1) + " of up to 4)",
                                           rest, 7 + 2 * i, /*may_decline=*/true);
            if (!pick.has_value()) return;          // waiting on the agent
            if (*pick == kInvalidId) break;         // "up to": done
            if (std::find(rest.begin(), rest.end(), *pick) == rest.end()) break;
            picks.push_back(*pick);
        }
        for (auto cid : picks) {
            auto& trash = ctx.state.player(ctx.state.getObject(cid).owner).trash;
            trash.erase(std::remove(trash.begin(), trash.end(), cid), trash.end());
        }
        if (!picks.empty()) ctx.executor.recycleCards(ctx.controller, picks);
        ctx.events.logTrace("FORGE OF THE FUTURE: killed -> recycled " +
                            std::to_string(picks.size()) + " card(s) from trashes");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 212;
        d.def_id = R"RB(ogn-212-298)RB";
        d.name = R"RB(Forge of the Future)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-212/298)RB";
        d.collector_number = 212;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.ability_text = R"RB(When you play this, play a 1 [M] Recruit unit token at your base.
Kill this: Recycle up to 4 cards from trashes.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8e6a76d28590bcd835b0a9a1e806fe0fa6883141-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_212(CardRegistry& r) {
    r.registerCard(212, std::make_unique<ForgeOfTheFuture>());
}

} // namespace riftbound
