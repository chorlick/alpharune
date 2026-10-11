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
#include "cards/units/weaponmaster_base.h"

namespace riftbound {
namespace {

// "[Weaponmaster] ... When I conquer a battlefield that was uncontrolled, deal
//  damage equal to my Might to an enemy unit in a base."
class YoneBlademaster : public WeaponmasterUnit {
public:
    const CardDef& def() const override { return def_; }
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIConquer};
    }

    // "a battlefield that was uncontrolled": nothing tells a conquer trigger
    // who held the battlefield before, so while Yone stands at a battlefield
    // her controller does not control she notes whether anyone does. The note
    // is what the battlefield was when she conquered it.
    static constexpr const char* kSeenBf = "__yone_seen_bf";
    static constexpr const char* kSeenOpen = "__yone_seen_uncontrolled";
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        for (auto& [id, self] : state.objects) {
            if (self.card_def_id != cardDefId() || self.controller != controller) continue;
            const auto here = self.battlefieldId();
            if (!here.has_value()) continue;
            for (const auto& bf : state.battlefields) {
                if (bf.id != *here) continue;
                if (bf.controller.has_value() && *bf.controller == controller) continue;
                self.card_counters[kSeenBf] = static_cast<int>(bf.id);
                self.card_counters[kSeenOpen] = bf.controller.has_value() ? 0 : 1;
            }
        }
    }
    bool triggerCondition(const GameState& state, GameObjectId source, PlayerId /*controller*/,
                          TriggerType trigger, GameObjectId /*subject*/) const override {
        if (trigger != TriggerType::WhenIConquer) return true;
        if (!state.objectExists(source)) return false;
        const auto& self = state.getObject(source);
        const auto here = self.battlefieldId();
        if (!here.has_value()) return false;
        const auto bf = self.card_counters.find(kSeenBf);
        const auto open = self.card_counters.find(kSeenOpen);
        return bf != self.card_counters.end() && bf->second == static_cast<int>(*here) &&
               open != self.card_counters.end() && open->second == 1;
    }

    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& targets) override {
        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe) {
            // [Weaponmaster]: the shared base asks the "may" and the Equipment.
            WeaponmasterUnit::onTrigger(ctx, targets);
            return;
        }
        if (ctx.firing_trigger != TriggerType::WhenIConquer) return;
        // "an enemy unit in a base" — chosen as the ability resolves (the
        // Weaponmaster base decides at resolution, so this card is not opted
        // into choices at finalization).
        const auto legal = chooseableUnits(ctx.state, ctx.controller,
            [&](const GameObject& u) { return u.controller != ctx.controller && u.isAtBase(); });
        const GameObjectId t = pickTarget(ctx, "Yone: deal my Might to an enemy unit in a base",
                                          legal);
        if (t == kInvalidId) return;   // none / waiting for the agent
        if (!ctx.state.objectExists(ctx.source)) return;
        const auto& self = ctx.state.getObject(ctx.source);
        if (!self.location.has_value()) return;   // "my Might": I must still be here
        const int my_might = self.current_might;
        if (my_might <= 0) return;
        ctx.executor.dealDamage(t, my_might, ctx.source);
        if (ctx.state.objectExists(t) && ctx.state.getObject(t).hasLethalDamage())
            ctx.executor.killObject(t);
        ctx.events.logTrace("YONE: conquered an uncontrolled battlefield -> deal " +
                            std::to_string(my_might) + " to an enemy unit in a base");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 544;
        d.def_id = R"RB(sfd-233-221)RB";
        d.name = R"RB(Yone, Blademaster)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-233/221)RB";
        d.collector_number = 233;
        d.artist = R"RB(Anson Tan)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Yone)RB", R"RB(Ionia)RB", R"RB(Demon)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Showcase;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.)
When I conquer a battlefield that was uncontrolled, deal damage equal to my Might to an enemy unit in a base.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/0bb6fd6e7d1e6fefeeccc82059bdae2eaa000730-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_544(CardRegistry& r) {
    r.registerCard(544, std::make_unique<YoneBlademaster>());
}

} // namespace riftbound
