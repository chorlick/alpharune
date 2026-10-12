#include "cards/card.h"
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

// "You may pay [1] as an additional cost to play me.
//  When you play me, if you paid the additional cost, buff me. (Give me a
//  +1 [M] buff if I don't already have one.)"

class SeaMonkey : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    OptionalAdditionalCost optionalAdditionalCost() const override {
        return {/*valid=*/true, /*energy=*/1, /*power=*/0, Domain::Fury,
                /*any_domain=*/false, /*paid_flag=*/"__sea_monkey_paid"};
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "if you paid the additional cost" is part of the trigger condition (CR
    // 383.2.a.1): asked as I am played; unpaid, nothing is put on the chain.
    // The engine forgets the flag as each play begins
    // (Card::forgetPlayPayments).
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType /*trigger*/, GameObjectId /*subject*/) const override {
        if (!state.objectExists(source)) return false;
        const auto& c = state.getObject(source).card_counters;
        const auto it = c.find("__sea_monkey_paid");
        return it != c.end() && it->second == 1;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        // The payment covers this play only.
        ctx.state.getObject(ctx.source).card_counters.erase("__sea_monkey_paid");
        ctx.executor.buffUnit(ctx.source);
        RIFT_TRACE(ctx.events, "SEA MONKEY: paid [1] -> buff me (+1 [M] buff)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 420;
        d.def_id = R"RB(sfd-098-221)RB";
        d.name = R"RB(Sea Monkey)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-098/221)RB";
        d.collector_number = 98;
        d.artist = R"RB(Bubble Cat Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.ability_text = R"RB(You may pay [1] as an additional cost to play me.
When you play me, if you paid the additional cost, buff me. (Give me a +1 [M] buff if I don't already have one.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0ef7f1de2bc7845f5e3dace1634c6c58a8765452-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_420(CardRegistry& r) {
    r.registerCard(420, std::make_unique<SeaMonkey>());
}

} // namespace riftbound
