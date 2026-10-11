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

class Tideturner : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "When you play me, you may choose a unit you control at another
    // location. Move me to its location and it to my original location."
    // The "may" and the unit are decided as the ability goes on the chain
    // (CR 383.3).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // Units I control somewhere other than where I am. Empty once I've
        // left the board.
        auto elsewhere = [&]() {
            std::vector<GameObjectId> out;
            const auto& self = ctx.state.getObject(ctx.source);
            if (!self.location.has_value()) return out;
            for (const auto& [id, obj] : ctx.state.objects.onBoard()) {
                if (id == ctx.source || !obj.isUnit() || obj.controller != ctx.controller) continue;
                if (!obj.location.has_value() || *obj.location == *self.location) continue;
                out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        const int go = confirmOptional(ctx, "Tideturner: swap locations with a unit you control?",
                                       [&]() { return !elsewhere().empty(); });
        if (go < 1) return;                   // declined / nothing to choose / waiting
        const GameObjectId t = pickTarget(ctx, "Tideturner: swap with which unit", elsewhere());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (t == kInvalidId) return;          // none / mistarget / waiting
        const auto my_loc = ctx.state.getObject(ctx.source).location;
        const auto its_loc = ctx.state.getObject(t).location;
        moveToLocation(ctx.executor, ctx.source, its_loc);
        moveToLocation(ctx.executor, t, my_loc);
        RIFT_TRACE(ctx.events, "TIDETURNER: swapped locations");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 199;
        d.def_id = R"RB(ogn-199-298)RB";
        d.name = R"RB(Tideturner)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-199/298)RB";
        d.collector_number = 199;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Bilgewater)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play me, you may choose a unit you control at another location. Move me to its location and it to my original location.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/bad355dba8b32d1fba33dc3924cad6a34b61b5af-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_199(CardRegistry& r) {
    r.registerCard(199, std::make_unique<Tideturner>());
}

} // namespace riftbound
