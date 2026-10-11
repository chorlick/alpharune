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

class AkaliSilent : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "I can't be chosen by enemy spells and abilities unless I'm in combat."
    // Card::canBeChosenByEnemy() is not state-aware, so the flag the engine
    // derives from it (GameObject::untargetable_by_enemy, reset per object
    // right before that object's applyPassiveAura) is asserted here instead.
    // Deliberately idempotent with no once-per-pass marker: every instance's
    // call re-asserts the flag on all my copies, so a copy reset later in the
    // engine's pass is set again by its own call.
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId() || obj.controller != controller) continue;
            if (!obj.isUnit() || !obj.location.has_value()) continue;
            if (obj.combat_designation == CombatDesignation::None)
                obj.untargetable_by_enemy = true;
        }
    }
    // "When I move to a battlefield, give me +2 [M] this turn."
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        ctx.executor.giveTemporaryMight(ctx.source, 2);
        RIFT_TRACE(ctx.events, "AKALI, SILENT: moved to a battlefield -> +2 [M] this turn");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 825;
        d.def_id = R"RB(ven-038-166)RB";
        d.name = R"RB(Akali, Silent)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-038/166)RB";
        d.collector_number = 38;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Ionia)RB", R"RB(Akali)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(I can't be chosen by enemy spells and abilities unless I'm in combat.
When I move to a battlefield, give me +2 [M] this turn.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f1a4d73853194a3dde02c51b083d197af89d2bda-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_825(CardRegistry& r) {
    r.registerCard(825, std::make_unique<AkaliSilent>());
}

}  // namespace riftbound
