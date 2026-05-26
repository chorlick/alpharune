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

class CardSharp : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "you ... may play a Gold gear token exhausted" — controller's token.
        createGoldExhausted(ctx);
        // "each opponent may" — 1v1: opponent always opts in (strictly good).
        PlayerId opp = opponent(ctx.controller);
        {
            LocationId loc{BaseLocation{opp}};
            auto tok = ctx.executor.createToken(opp, CardType::Gear, "Gold",
                                                0, {}, {}, loc, /*enter_ready=*/false);
            if (ctx.state.objectExists(tok)) ctx.state.getObject(tok).is_exhausted = true;
        }
        // "For each opponent who did, you play a Gold gear token exhausted."
        createGoldExhausted(ctx);
        ctx.events.logTrace("CARD SHARP: Gold tokens (you + opp + payoff)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 403;
        d.def_id = R"RB(sfd-081-221)RB";
        d.name = R"RB(Card Sharp)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-081/221)RB";
        d.collector_number = 81;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, you and each opponent may play a Gold gear token exhausted. For each opponent who did, you play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8bfd5dc22473d439cd78d5a3cb23ed03de66953d-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_403(CardRegistry& r) {
    r.registerCard(403, std::make_unique<CardSharp>());
}

} // namespace riftbound
