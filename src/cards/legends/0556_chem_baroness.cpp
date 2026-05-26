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

class ChemBaroness : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Optional: exhaust me to play a Gold gear token exhausted.
        if (!ctx.state.objectExists(ctx.source)) return;
        int yes = confirmOptional(ctx, "Chem-Baroness: exhaust to play a Gold token?",
                                  [&]() { return true; });
        if (yes != 1) return;
        auto& self = ctx.state.getObject(ctx.source);
        self.is_exhausted = true;
        LocationId loc{BaseLocation{ctx.controller}};
        auto tok = ctx.executor.createToken(ctx.controller, CardType::Gear, "Gold",
                                            0, {}, {}, loc, /*enter_ready=*/false);
        if (ctx.state.objectExists(tok)) ctx.state.getObject(tok).is_exhausted = true;
        ctx.events.logTrace("CHEM-BARONESS: played a Gold token (Gold-add bonus "
                            "passive not modeled)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 556;
        d.def_id = R"RB(sfd-249-221)RB";
        d.name = R"RB(Chem-Baroness)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-249/221)RB";
        d.collector_number = 249;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Mind, Domain::Order};
        d.tags = {R"RB(Renata Glasc)RB"};
        d.rarity = Rarity::Showcase;
        d.ability_text = R"RB(When you or an ally hold, you may exhaust me to play a Gold gear token exhausted.
While your score is within 3 points of the Victory Score, your Gold [ADD] an additional [1].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/5df25d5a1351d0a97e103ef8e155991297b86ca9-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_556(CardRegistry& r) {
    r.registerCard(556, std::make_unique<ChemBaroness>());
}

} // namespace riftbound
