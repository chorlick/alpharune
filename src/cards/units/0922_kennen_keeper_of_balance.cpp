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

class KennenKeeperOfBalance : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // ── behaviour (Vendetta batch C) ──
    // [Hidden] is engine-handled.
    // "When you play me or I attack, you may pay [2] to [Stun] a unit."
    std::vector<TriggerType> triggerTypes() const override {
        return {TriggerType::WhenYouPlayMe, TriggerType::WhenIAttack};
    }
    // The [2] is paid with card_helpers.h payEnergy: floating pool first, then
    // ready runes (the rune order is not offered to the agent).
    // His PLAY effect, when he was played from facedown, chooses among the
    // units at the battlefield he was played to (CR 811.1.d.3); "or I attack"
    // is not a play effect and chooses freely.
    std::vector<GameObjectId> stunnable(CardContext& ctx) const {
        auto units = chooseableUnits(ctx.state, ctx.controller,
                                     [](const GameObject&) { return true; });
        if (ctx.firing_trigger == TriggerType::WhenYouPlayMe)
            units = atHiddenBattlefield(ctx.state, ctx.source, std::move(units));
        return units;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto can_do = [&]() {
            return payableEnergy(ctx.state, ctx.controller) >= 2 && !stunnable(ctx).empty();
        };
        int conf = confirmOptional(ctx, "Kennen, Keeper of Balance: pay [2] to stun a unit?",
                                   can_do);
        if (conf < 1) return;
        GameObjectId picked = pickTarget(ctx, "Kennen, Keeper of Balance: stun", stunnable(ctx));
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        // Pay only once the unit is chosen (this branch runs exactly once).
        if (costDueNow(ctx) && !payEnergy(ctx, ctx.controller, 2)) return;
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        ctx.executor.stunUnitBy(picked, ctx.source);
    }
    // "While there's a stunned enemy unit here, I have +2 [M]."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachSelfOnce(*this, state, controller, [&](GameObjectId sid, GameObject& self) {
            if (!self.location.has_value()) return;
            for (const auto& [id, obj] : state.objects.onBoard()) {
                if (!obj.isUnit() || obj.controller == controller || !obj.is_stunned) continue;
                if (!obj.location.has_value() || *obj.location != *self.location) continue;
                auraMight(self, sid, 2);
                return;
            }
        });
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 922;
        d.def_id = R"RB(ven-135-166)RB";
        d.name = R"RB(Kennen, Keeper of Balance)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-135/166)RB";
        d.collector_number = 135;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Order};
        d.tags = {R"RB(Yordle)RB", R"RB(Ionia)RB", R"RB(Kennen)RB"};
        d.energy_cost = 3;
        d.might = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for [0].)
When you play me or I attack, you may pay [2] to [Stun] a unit. (It doesn't deal combat damage this turn.)
While there's a stunned enemy unit here, I have +2 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/446ff54c091b07627f826952f45ee40e9976d040-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_922(CardRegistry& r) {
    r.registerCard(922, std::make_unique<KennenKeeperOfBalance>());
}

}  // namespace riftbound
