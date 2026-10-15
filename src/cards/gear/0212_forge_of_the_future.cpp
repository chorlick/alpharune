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
        RIFT_TRACE(ctx.events, "FORGE OF THE FUTURE: played -> 1[M] Recruit to base");
    }

    // "Kill this: Recycle up to 4 cards from trashes."
    // "Kill this" is the whole cost, paid as the ability is activated, after
    // its choices (ActivationCost::kill_self; CR 403.2 -> 405.1). The cards —
    // targets: trashes are public (CR 355.10.a.1), up to 4, zero allowed (CR
    // 355.13) — are chosen as it is activated, before anyone can respond
    // (testing phase D41); each goes to its owner's Main Deck.
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility ab;
        ab.cost.kill_self = true;
        return {ab};
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> in_trashes;
        for (auto pid : {PlayerId::Player1, PlayerId::Player2})
            for (auto cid : ctx.state.player(pid).trash)
                if (ctx.state.objectExists(cid)) in_trashes.push_back(cid);
        const auto picks = selectSeveralTargets(*this, ctx,
            "Forge of the Future: recycle a card from a trash (up to 4)", in_trashes,
            /*max_picks=*/4);
        if (!picks.has_value()) return;          // waiting on the agent
        if (finalizingChoices(ctx)) return;      // choices end here (CR 403)
        std::vector<GameObjectId> to_recycle;
        for (auto cid : *picks) {
            auto& trash = ctx.state.player(ctx.state.getObject(cid).owner).trash;
            if (std::find(trash.begin(), trash.end(), cid) == trash.end()) continue;
            trash.erase(std::remove(trash.begin(), trash.end(), cid), trash.end());
            to_recycle.push_back(cid);
        }
        if (!to_recycle.empty()) ctx.executor.recycleCards(ctx.controller, to_recycle);
        RIFT_TRACE(ctx.events, "FORGE OF THE FUTURE: recycled " +
                            std::to_string(to_recycle.size()) + " card(s) from trashes");
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
