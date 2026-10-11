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
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    // "When I attack, you may pay [C] to play a card with [Hidden] from your
    // hand, ignoring its cost. If it's a unit, play it here." [C] = one Power of
    // my domain (Mind): floating Mind (or universal) power, else a Mind rune
    // recycled through payOnePower.
    bool canPayC(CardContext& ctx) const {
        return payableOnePower(ctx.state, ctx.controller, Domain::Mind);
    }
    bool payC(CardContext& ctx) const { return payOnePower(ctx, ctx.controller, Domain::Mind); }
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
        // CR 445.2: only offer a payment that can be made.
        auto still_legal = [&]() { return !hiddenInHand(ctx).empty() && canPayC(ctx); };
        int conf = confirmOptional(ctx, "Ava Achiever: pay [C] to play a Hidden card?",
                                   still_legal);
        if (conf < 1) return;    // waiting / declined / no Hidden cards
        GameObjectId picked = pickTarget(ctx, "Ava Achiever (Hidden card)", hiddenInHand(ctx));
        // [C] is paid with the choices, as the ability goes on the chain
        // (CR 383.3) — once, after the card is known: pickTarget suspends and
        // this trigger is re-entered.
        if (picked != kInvalidId && costDueNow(ctx) && !payC(ctx)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (picked == kInvalidId) return;     // none / no longer in hand / waiting
        // "If it's a unit, play it here." (my battlefield); otherwise default.
        std::optional<LocationId> loc;
        if (ctx.state.getObject(picked).isUnit()) {
            auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
            if (my_bf) loc = LocationId{BattlefieldLocation{*my_bf}};
        }
        ctx.executor.playIgnoringCost(ctx.controller, picked, loc);
        RIFT_TRACE(ctx.events, "AVA ACHIEVER: paid [C] -> played a Hidden card from hand");
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
        // No printed [Hidden]: the text only refers to the keyword on other cards.
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
