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

class Windsinger : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "another unit at a battlefield with 3 [M] or less" — an enemy one
        // only if it can be chosen by enemy spells and abilities.
        auto legal = [&]() {
            return chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
                return o.id != ctx.source && o.isAtBattlefield() && o.current_might <= 3;
            });
        };
        auto still_legal = [&]() { return !legal().empty(); };
        int conf = confirmOptional(ctx, "Windsinger: return a small unit to hand?",
                                   still_legal);
        if (conf == -1) return;
        if (conf == 0) return;
        GameObjectId picked = pickTarget(ctx, "Windsinger", legal());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        std::string n = ctx.state.getObject(picked).name;
        ctx.executor.bounceToHand(picked);
        RIFT_TRACE(ctx.events, "WINDSINGER: returned " + n + " to hand");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 459;
        d.def_id = R"RB(sfd-138-221)RB";
        d.name = R"RB(Windsinger)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-138/221)RB";
        d.collector_number = 138;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.rarity = Rarity::Uncommon;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
When you play me, you may return another unit at a battlefield with 3 [M] or less to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e47c9a3956f8fbb77daa5fdb2ee433dd1772f247-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_459(CardRegistry& r) {
    r.registerCard(459, std::make_unique<Windsinger>());
}

} // namespace riftbound
