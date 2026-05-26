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

class Shakedown : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true,
                                  .must_be_enemy = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.empty() || !ctx.state.objectExists(targets[0])) return;
        // Default branch: opponent declines to feed cards, so deal 6.
        // (Opponent "have you draw 2 instead" opt-out is an unmodeled partial —
        // see header comment: no opponent-side resumable choice hook here.)
        ctx.executor.dealDamage(targets[0], 6, ctx.source);
        ctx.events.logTrace("SHAKEDOWN: deal 6 to chosen enemy unit "
                            "(opponent draw-2 opt-out unmodeled)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 33;
        d.def_id = R"RB(ogn-033-298)RB";
        d.name = R"RB(Shakedown)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-033/298)RB";
        d.collector_number = 33;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Fury};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Choose an enemy unit. Deal 6 to it unless its controller has you draw 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ab71d92c94bf609e2fa6efc8fec06fe1e8b10108-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_33(CardRegistry& r) {
    r.registerCard(33, std::make_unique<Shakedown>());
}

} // namespace riftbound
