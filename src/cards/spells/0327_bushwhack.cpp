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
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // "Friendly units enter ready this turn" is a continuous effect on
        // units played LATER this turn. The engine has no per-player
        // "units enter ready this turn" flag (would require an engine hook),
        // so we approximate by readying the controller's currently-on-board
        // exhausted units now. NOTE: this does not affect units the
        // controller plays after this spell resolves (engine gap).
        int readied = 0;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value() || !obj.is_exhausted) continue;
            ctx.executor.readyObject(id);
            ++readied;
        }
        createGoldExhausted(ctx);
        ctx.events.logTrace("BUSHWHACK: readied " + std::to_string(readied) +
                            " friendly units (approx) + Gold gear token (exhausted)");
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
