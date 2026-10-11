#include "cards/units/weaponmaster_base.h"
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

class JaxUnrelenting : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // Two triggered abilities:
    //  - [Weaponmaster] "When you play me, you may [Equip] one of your
    //    Equipment to me for [A] less, even if it's already attached."
    //  - "When you attach an Equipment to me, you may pay [1] to draw 1." —
    //    its own ability: it triggers on EVERY attach (Weaponmaster, a later
    //    Equip activation, an effect), not only on the play.
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenEquipmentAttachedToMe};
    }

    // Resume points of the Weaponmaster choice (clear of the helper ranges).
    static constexpr int kAwaitGear = 20;
    static constexpr int kDone = 21;

    // [Weaponmaster] (CR 821): the Equipment's declared Equip cost
    // (Card::equipCost), one Power less, through the shared Weaponmaster
    // terms — any Equipment, whatever its cost is made of.
    std::vector<GameObjectId> equippable(CardContext& ctx) const {
        return WeaponmasterUnit::weaponmasterChoices(ctx, ctx.source);
    }
    void equip(CardContext& ctx, GameObjectId gear_id) {
        if (WeaponmasterUnit::weaponmasterEquip(ctx, gear_id, ctx.source))
            ctx.events.logTrace("JAX, UNRELENTING: [Weaponmaster] equipped " +
                                ctx.state.getObject(gear_id).name);
    }

    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        if (ctx.firing_trigger == TriggerType::WhenEquipmentAttachedToMe) {
            // "you may pay [1] to draw 1". CR 445.2 / 164.2.a: the [1] may come
            // from floating Energy or from exhausting a ready rune.
            auto still_legal = [&ctx]() {
                return payableEnergy(ctx.state, ctx.controller) >= 1;
            };
            int conf = confirmOptional(ctx, "Jax, Unrelenting: pay [1] to draw 1?",
                                       still_legal);
            if (conf != 1) return;       // waiting, declined, or can't pay
            if (!payEnergy(ctx, ctx.controller, 1)) return;
            ctx.executor.drawCards(ctx.controller, 1);
            ctx.events.logTrace("JAX, UNRELENTING: paid [1] -> draw 1");
            return;
        }

        // ── [Weaponmaster] (when you play me) ──
        if (!ctx.state.getObject(ctx.source).location.has_value()) return;
        auto& ri_opt = ctx.state.chain.resuming;
        if (!ri_opt.has_value()) {
            // Direct invocation (no chain item to resume): no agent can be
            // asked, so take the first Equipment, as the choice helpers do.
            auto cands = equippable(ctx);
            if (!cands.empty()) equip(ctx, cands.front());
            return;
        }
        auto& ri = *ri_opt;
        if (ri.resume_point == kDone) return;
        if (ri.resume_point == kAwaitGear) {
            ri.resume_point = kDone;
            auto choice = ctx.executor.takeChoice();
            auto cands = equippable(ctx);
            GameObjectId picked = kInvalidId;
            if (choice && !choice->chosen_objects.empty()) picked = choice->chosen_objects[0];
            if (std::find(cands.begin(), cands.end(), picked) == cands.end()) {
                ctx.events.logTrace("JAX, UNRELENTING: [Weaponmaster] declined");
                return;
            }
            equip(ctx, picked);
            return;
        }
        // Which Equipment — or none ("you may") — is the controller's choice.
        auto cands = equippable(ctx);
        if (cands.empty()) { ri.resume_point = kDone; return; }
        std::vector<Intent> opts;
        for (auto id : cands) {
            Intent c;
            c.type = IntentType::MakeChoice;
            c.player = ctx.controller;
            c.chosen_objects = {id};
            opts.push_back(std::move(c));
        }
        Intent none;
        none.type = IntentType::MakeChoice;
        none.player = ctx.controller;
        none.chosen_value = 0;
        none.choice_label = "No — Jax, Unrelenting: [Weaponmaster]";
        opts.push_back(std::move(none));
        ctx.executor.requestChoice(ctx.controller, std::move(opts),
                                   "Jax, Unrelenting: [Weaponmaster] equip which Equipment?");
        ri.resume_point = kAwaitGear;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 440;
        d.def_id = R"RB(sfd-119-221)RB";
        d.name = R"RB(Jax, Unrelenting)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-119/221)RB";
        d.collector_number = 119;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Jax)RB", R"RB(Icathia)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Weaponmaster);
        d.ability_text = R"RB([Weaponmaster] (When you play me, you may [Equip] one of your Equipment to me for [A] less, even if it's already attached.)
When you attach an Equipment to me, you may pay [1] to draw 1.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e63ab3370a6e4ce4baddfba12ab92468f6cc9541-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_440(CardRegistry& r) {
    r.registerCard(440, std::make_unique<JaxUnrelenting>());
}

} // namespace riftbound
