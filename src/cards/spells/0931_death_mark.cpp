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

class DeathMark : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    // [vendetta-E]
    // "[Burn 3]. Play a 0 [M] Shadow Clone unit token."
    // (The token carries the registered Shadow Clone card, id 955.) It is
    // played to the controller's base or a battlefield they control.
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // The Burn happens once: not again when this is re-entered with the
        // token's location (the helper leaves resume_point non-zero while it
        // waits, and its slot records a token already played).
        auto& ri = ctx.state.chain.resuming;
        const bool first_pass = !ri.has_value() ||
            (ri->resume_point == 0 && ri->resume_data.size() <= kTokenPlaySlot);
        if (first_pass) {
            ctx.executor.burn(ctx.controller, 3);
            if (ctx.state.game_over) return;
        }
        std::vector<GameObjectId> made;
        auto make = [&](LocationId loc) {
            auto clone_id = ctx.executor.createToken(ctx.controller, CardType::Unit,
                                                     "Shadow Clone", 0, {}, KeywordSet{}, loc,
                                                     /*enter_ready=*/false, {},
                                                     // the Shadow Clone token card ("When I attack")
                                                     /*token_card=*/955);
            return clone_id;
        };
        // At resolution: a play location is a permission, not a target (CR 355.10.b; M5; batch M1).
        playTokensChoosingLocations(ctx, "Death Mark: play the Shadow Clone", 1, make, made);
    }
    // "[Flow] [1][A][A]" — each [A] is Power of any domain: the two need not
    // share one (GameEngine::trashGrantCost / resource::payIn).
    FlowCost flowCost() const override { return flow(1, 2, Domain::Fury, /*any_domain=*/true); }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 931;
        d.def_id = R"RB(ven-144-166)RB";
        d.name = R"RB(Death Mark)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-144/166)RB";
        d.collector_number = 144;
        d.artist = R"RB(JunHuan)RB";
        d.card_type = CardType::Spell;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Fury, Domain::Chaos};
        d.tags = {R"RB(Zed)RB"};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Flow);
        d.ability_text = R"RB([Burn 3]. (Put the top 3 cards of your Main Deck into your trash.)
Play a 0 [M] Shadow Clone unit token. (It has "When I attack, you may banish a unit from your trash. If you do, give me [Assault 4] this turn.")
[Flow] [1][A][A] (You may play this from your trash for its Flow cost. Then banish it.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/87ccfb8d1c19f402bb37ffa392011ee2f0880aa9-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_931(CardRegistry& r) {
    r.registerCard(931, std::make_unique<DeathMark>());
}

}  // namespace riftbound
