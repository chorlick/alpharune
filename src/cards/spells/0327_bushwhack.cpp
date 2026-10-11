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

class Bushwhack : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    // "Friendly units enter ready this turn. Play a Gold gear token exhausted."
    // The first sentence is about units that enter from now until end of
    // turn; units already on the board are not readied.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        ctx.state.player(ctx.controller).units_enter_ready_this_turn = true;
        createGoldExhausted(ctx);
        ctx.events.logTrace("BUSHWHACK: friendly units enter ready this turn + "
                            "Gold gear token (exhausted)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 327;
        d.def_id = R"RB(sfd-004-221)RB";
        d.name = R"RB(Bushwhack)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-004/221)RB";
        d.collector_number = 4;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
Friendly units enter ready this turn. Play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f728959ad6d4ee6c507310000c3c9e02c0772a16-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_327(CardRegistry& r) {
    r.registerCard(327, std::make_unique<Bushwhack>());
}

} // namespace riftbound
