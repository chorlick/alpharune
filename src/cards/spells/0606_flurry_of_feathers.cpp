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

class FlurryOfFeathers : public SpellCard {
public:
    const CardDef& def() const override { return def_; }

    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& chain = ctx.state.chain;
        bool counter_legal = !chain.items.empty() &&
                             chain.items.back().is_spell &&
                             chain.items.back().source != ctx.source;

        uint32_t legal_mask = 0b10;  // mode 1 (spawn birds) always legal
        if (counter_legal) legal_mask |= 0b01;

        int mode = pickMode(ctx, "Flurry of Feathers", /*num_modes=*/2,
                            {"Counter a spell", "Play four Bird tokens"},
                            legal_mask);
        if (mode == -1) return;   // yielded for agent input
        if (mode == -2) mode = 1; // no legal mode (shouldn't happen — birds always legal)

        if (mode == 0 && counter_legal) {
            auto victim = chain.items.back();
            chain.items.pop_back();
            ctx.events.logTrace("FLURRY OF FEATHERS: countered spell (id=" +
                                 std::to_string(victim.source) + ")");
            if (ctx.state.objectExists(victim.source)) {
                auto& sp = ctx.state.getObject(victim.source);
                auto owner = sp.owner;
                sp.zone = ZoneType::Trash;
                if (owner != PlayerId::None) {
                    ctx.state.player(owner).trash.push_back(victim.source);
                }
            }
            return;
        }

        // Mode 1: spawn four 1 [M] Bird tokens with [Deflect] at controller's base.
        KeywordSet kw; kw.set(Keyword::Deflect);
        for (int i = 0; i < 4; ++i) {
            ctx.executor.createToken(ctx.controller, CardType::Unit, "Bird",
                                      /*might=*/1, /*tags=*/{"Bird"}, kw,
                                      BaseLocation{ctx.controller},
                                      /*enter_ready=*/false);
        }
        ctx.events.logTrace("FLURRY OF FEATHERS: spawned 4 Bird tokens");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 606;
        d.def_id = R"RB(unl-044-219)RB";
        d.name = R"RB(Flurry of Feathers)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-044/219)RB";
        d.collector_number = 44;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Calm};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction]
Choose one —
Counter a spell.Play four 1 [M] Bird unit tokens with [Deflect]. (Opponents must pay [A] to choose them with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/77bd864b29f4975868e557e31a39b94d06a4ecb2-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_606(CardRegistry& r) {
    r.registerCard(606, std::make_unique<FlurryOfFeathers>());
}

} // namespace riftbound
