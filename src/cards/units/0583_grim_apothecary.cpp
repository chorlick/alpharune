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

class GrimApothecary : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "a friendly unit at a battlefield" — the controller picks which
        // (this unit included, if it was played to one).
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects)
            if (obj.isUnit() && obj.controller == ctx.controller && obj.isAtBattlefield())
                legal.push_back(id);
        int conf = confirmOptional(ctx,
            "Grim Apothecary: return a friendly unit at a battlefield to hand?",
            [&legal]() { return !legal.empty(); });
        GameObjectId target = kInvalidId;
        if (conf == 1)
            target = pickTarget(ctx, "Grim Apothecary: choose a friendly unit", legal);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (conf != 1 || target == kInvalidId) return;
        const std::string name = ctx.state.getObject(target).name;
        ctx.executor.bounceToHand(target);
        RIFT_TRACE(ctx.events, "GRIM APOTHECARY: bounced " + name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 583;
        d.def_id = R"RB(unl-021-219)RB";
        d.name = R"RB(Grim Apothecary)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-021/219)RB";
        d.collector_number = 21;
        d.artist = R"RB(Wild Blue Studios)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Ambush);
        d.ability_text = R"RB([Ambush] (You may play me as a [Reaction] to a battlefield where you have units.)
When you play me, you may return a friendly unit at a battlefield to its owner's hand.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/af548bbfc856e306c5feefb34eb6bd9a4e442904-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_583(CardRegistry& r) {
    r.registerCard(583, std::make_unique<GrimApothecary>());
}

} // namespace riftbound
