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

class EdgeOfNight : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        // [C] = Chaos-domain power, no energy (mirrors original
        // SimpleEquipGear(460, Domain::Chaos) with energy_cost 0).
        return standardEquip(ctx, ctx.source, unit,
                                         /*energy_cost=*/0, Domain::Chaos);
    }

    // "When you play this from face down, attach it to a unit you control
    // (here)." Played face down means played at the battlefield it was hidden
    // at; from hand it lands in base and nothing happens. The controller
    // chooses which of their units here (CR 383.3: at finalization).
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override {
        return TriggerType::WhenYouPlayThis;
    }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        std::vector<GameObjectId> here;
        if (ctx.state.objectExists(ctx.source)) {
            const auto& self = ctx.state.getObject(ctx.source);
            if (!self.attached_to.has_value() && self.isAtBattlefield()) {
                for (const auto& [id, obj] : ctx.state.objects) {
                    if (!obj.isUnit() || obj.controller != ctx.controller) continue;
                    if (!obj.location.has_value() || *obj.location != *self.location) continue;
                    here.push_back(id);
                }
            }
        }
        std::sort(here.begin(), here.end());
        const GameObjectId unit = pickTarget(ctx, "Edge of Night: attach to which unit here?", here);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (unit == kInvalidId) return;       // none / mistarget / waiting
        // attachFree has no board check of its own: both ends must be there.
        if (!equipEndsOnBoard(ctx.state, ctx.source, unit)) return;
        if (ctx.state.getObject(ctx.source).attached_to.has_value()) return;
        attachFree(ctx, ctx.source, unit);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 460;
        d.def_id = R"RB(sfd-139-221)RB";
        d.name = R"RB(Edge of Night)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-139/221)RB";
        d.collector_number = 139;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play this from face down, attach it to a unit you control (here).
[Equip] [C] ([C]: Attach this to a unit you control.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/b3e385162bf3566618bb58b7a866eef846beefba-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_460(CardRegistry& r) {
    r.registerCard(460, std::make_unique<EdgeOfNight>());
}

} // namespace riftbound
