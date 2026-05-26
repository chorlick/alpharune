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

class Deathgrip : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 2, .must_be_unit = true,
                                   .must_be_friendly = true};
    }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (targets.size() >= 2 && ctx.state.objectExists(targets[0])
                                && ctx.state.objectExists(targets[1])) {
            auto sacrifice_id = targets[0];
            auto recipient_id = targets[1];
            int might = ctx.state.getObject(sacrifice_id).current_might;
            ctx.executor.killObject(sacrifice_id);
            ctx.executor.giveTemporaryMight(recipient_id, might);
        }
        ctx.executor.drawCards(ctx.controller, 1);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 484;
        d.def_id = R"RB(sfd-163-221)RB";
        d.name = R"RB(Deathgrip)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-163/221)RB";
        d.collector_number = 163;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Order};
        d.energy_cost = 2;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Reaction);
        d.ability_text = R"RB([Reaction] (Play any time, even before spells and abilities resolve.)
Kill a friendly unit. If you do, give +[M] equal to its Might to another friendly unit this turn.
Draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/695304200c6fd3b287bd319ac564d31936b23d66-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_484(CardRegistry& r) {
    r.registerCard(484, std::make_unique<Deathgrip>());
}

} // namespace riftbound
