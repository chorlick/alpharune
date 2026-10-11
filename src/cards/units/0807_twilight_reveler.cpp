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

class TwilightReveler : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "When I attack, ready another friendly unit."
    TriggerType triggerType() const override { return TriggerType::WhenIAttack; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> others;
        for (const auto& [id, obj] : ctx.state.objects) {
            if (id == ctx.source || !obj.isUnit()) continue;
            if (obj.controller != ctx.controller || !obj.location.has_value()) continue;
            others.push_back(id);
        }
        GameObjectId tgt = pickTarget(ctx, "Twilight Reveler: ready another friendly unit", others);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        ctx.executor.readyObject(tgt);
        RIFT_TRACE(ctx.events, "TWILIGHT REVELER: readied " + ctx.state.getObject(tgt).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 807;
        d.def_id = R"RB(ven-020-166)RB";
        d.name = R"RB(Twilight Reveler)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-020/166)RB";
        d.collector_number = 20;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Ionia)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I attack, ready another friendly unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c9b3acfc3f996d784d1b5f98ab87ba4d1c6136ce-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_807(CardRegistry& r) {
    r.registerCard(807, std::make_unique<TwilightReveler>());
}

}  // namespace riftbound
