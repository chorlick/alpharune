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

class HarpoonSquad : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When I move from a battlefield, give me +2 [M] this turn."
    // The WhenIMove trigger doesn't surface the source location to onTrigger,
    // so the "from a battlefield" qualifier can't be checked here; granted on
    // any move (close approximation — combat units predominantly move off BFs).
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        ctx.events.logTrace("HARPOON SQUAD: moved -> +2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 458;
        d.def_id = R"RB(sfd-137-221)RB";
        d.name = R"RB(Harpoon Squad)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-137/221)RB";
        d.collector_number = 137;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Pirate)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 4;
        d.might = 4;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I move from a battlefield, give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f50d32a202375bf09781ae578db107abe84937e0-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_458(CardRegistry& r) {
    r.registerCard(458, std::make_unique<HarpoonSquad>());
}

} // namespace riftbound
