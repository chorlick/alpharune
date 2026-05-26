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

class AvaAchiever : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    // "When I attack, you may pay [C] to play a card with [Hidden] from your
    // hand, ignoring its cost. If it's a unit, play it here." [C] = one Power of
    // my domain (Mind). Cost paid via payOnePower; Power-only approximation
    // for the played card (playIgnoringCost ignores all costs).
    std::vector<GameObjectId> hiddenInHand(CardContext& ctx) const {
        std::vector<GameObjectId> out;
        const auto& ps = ctx.state.player(ctx.controller);
        for (auto cid : ps.hand) {
            if (!ctx.state.objectExists(cid)) continue;
            if (ctx.state.getObject(cid).hasKeyword(Keyword::Hidden)) out.push_back(cid);
        }
        return out;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto still_legal = [&]() { return !hiddenInHand(ctx).empty(); };
        int conf = confirmOptional(ctx, "Ava Achiever: pay [C] to play a Hidden card?",
                                   still_legal);
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / no Hidden cards
        // Pay [C] = one Mind power. If we can't pay, abort.
        if (!payOnePower(ctx, ctx.controller, Domain::Mind)) return;
        auto eligible = hiddenInHand(ctx);
        if (eligible.empty()) return;
        GameObjectId picked = pickTarget(ctx, "Ava Achiever (Hidden card)", eligible);
        if (picked == kInvalidId && ctx.state.chain.resuming.has_value() &&
            ctx.state.chain.resuming->resume_point == 7) {
            return;  // suspended
        }
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        auto& ps = ctx.state.player(ctx.controller);
        auto it = std::find(ps.hand.begin(), ps.hand.end(), picked);
        if (it != ps.hand.end()) ps.hand.erase(it);
        // "If it's a unit, play it here." (my battlefield); otherwise default.
        std::optional<LocationId> loc;
        if (ctx.state.getObject(picked).isUnit()) {
            auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
            if (my_bf) loc = LocationId{BattlefieldLocation{*my_bf}};
        }
        ctx.executor.playIgnoringCost(ctx.controller, picked, loc);
        ctx.events.logTrace("AVA ACHIEVER: paid [C] -> played a Hidden card from hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 107;
        d.def_id = R"RB(ogn-107-298)RB";
        d.name = R"RB(Ava Achiever)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-107/298)RB";
        d.collector_number = 107;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Yordle)RB"};
        d.energy_cost = 5;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB(When I attack, you may pay [C] to play a card with [Hidden] from your hand, ignoring its cost. If it's a unit, play it here.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/93e91aa99eb09baa68dd95b0013a89d9ffde5240-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_107(CardRegistry& r) {
    r.registerCard(107, std::make_unique<AvaAchiever>());
}

} // namespace riftbound
