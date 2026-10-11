#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/units/weaponmaster_base.h"
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

class RivenShattered : public WeaponmasterUnit {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // [Weaponmaster] (WhenYouPlayMe) comes from WeaponmasterUnit; this card
    // adds a second trigger and branches on the firing event.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIAttack};
    }
    static int equipmentAttached(const GameState& state, const GameObject& self) {
        int n = 0;
        for (auto gid : self.attachments)
            if (state.objectExists(gid) && isEquipment(state.getObject(gid))) ++n;
        return n;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (ctx.firing_trigger != TriggerType::WhenIAttack) {
            WeaponmasterUnit::onTrigger(ctx, targets);   // marks its own end of choices
            return;
        }
        // "When I attack, choose an enemy unit here. Deal 2 to it for each
        //  Equipment attached to me."
        if (!ctx.state.objectExists(ctx.source)) return;
        auto my_bf = ctx.state.getObject(ctx.source).battlefieldId();
        if (!my_bf) return;
        const BattlefieldId here = *my_bf;
        const PlayerId me = ctx.controller;
        GameObjectId tgt = pickTarget(ctx, "Riven, Shattered: choose an enemy unit here",
            chooseableUnits(ctx.state, me, [here, me](const GameObject& o) {
                auto bf = o.battlefieldId();
                return o.controller != me && bf && *bf == here;
            }));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        // Counted at resolution, after the choice.
        const int n = equipmentAttached(ctx.state, ctx.state.getObject(ctx.source));
        if (n <= 0) return;
        ctx.executor.dealDamage(tgt, 2 * n, ctx.source);
        ctx.events.logTrace("RIVEN, SHATTERED: " + std::to_string(n) +
                            " Equipment -> dealt " + std::to_string(2 * n));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 828;
        d.def_id = R"RB(ven-041-166)RB";
        d.name = R"RB(Riven, Shattered)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-041/166)RB";
        d.collector_number = 41;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Noxus)RB", R"RB(Riven)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.)
When I attack, choose an enemy unit here. Deal 2 to it for each Equipment attached to me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/8c16a53ee243e468f3c337de4354668665f3b386-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_828(CardRegistry& r) {
    r.registerCard(828, std::make_unique<RivenShattered>());
}

}  // namespace riftbound
