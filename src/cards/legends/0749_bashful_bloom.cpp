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

class BashfulBloom : public LegendCard {
public:
    const CardDef& def() const override { return def_; }
    bool hasActivatedAbility() const override { return true; }
    ActivationCost getActivationCost() const override {
        return {.exhaust = true, .energy = 4};
    }
    // "This ability costs [1] less for each friendly unit with [Temporary]."
    int activationCostReduction(const GameState& state, PlayerId controller,
                                int /*ability_index*/) const override {
        int n = 0;
        for (const auto& [id, obj] : state.objects)
            if (obj.isUnit() && obj.controller == controller &&
                obj.location.has_value() && obj.hasKeyword(Keyword::Temporary)) ++n;
        return n;
    }
    void onActivate(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        KeywordSet kw; kw.set(Keyword::Temporary);
        auto loc = BaseLocation{ctx.controller};
        ctx.executor.createToken(ctx.controller, CardType::Unit, "Sprite", 3,
                                  {"Fae"}, kw, loc, /*enter_ready=*/true);
        ctx.events.logTrace("BASHFUL BLOOM: plays ready 3M Sprite (Temporary)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 749;
        d.def_id = R"RB(unl-189-219)RB";
        d.name = R"RB(Bashful Bloom)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-189/219)RB";
        d.collector_number = 189;
        d.artist = R"RB(Pandart Studio)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Lillia)RB"};
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Temporary);
        d.ability_text = R"RB([4], [E]: Play a ready 3 [M] Sprite unit token with [Temporary]. This ability costs [1] less for each friendly unit with [Temporary].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7e1554365120c5042947aef8bcac48a07445e9f3-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_749(CardRegistry& r) {
    r.registerCard(749, std::make_unique<BashfulBloom>());
}

} // namespace riftbound
