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

class ForgeOfTheFluft : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeActivate; }
    // "While you control this battlefield, friendly legends have
    //  '[E]: Attach an Equipment you control to a unit you control.'"
    // Wired via the aura-granted-ability pipeline: applyPassiveAura grants this
    // card's ability (index 0) to the controller's legend; activation routes
    // back here to onActivate, which attaches a chosen Equipment to a chosen
    // unit (both controller-owned). Exhausts the bearer (the legend).
    // The Equipment and the unit are picked as the ability is finalized
    // (kFinalizeActivate): the ability must NOT set
    // needs_activation_time_target, which hands target choice to the engine
    // and skips the finalize pass.
    std::vector<ActivatedAbility> activatedAbilities() const override {
        ActivatedAbility a;
        a.cost.exhaust = true;
        return {a};
    }
    // Equipment `player` controls, attached or not: attaching an attached
    // Equipment moves it (CR 435.1.f, attachFree detaches it first).
    static std::vector<GameObjectId> equipmentOf(const GameState& state, PlayerId player) {
        std::vector<GameObjectId> out;
        for (const auto& [id, obj] : state.objects.onBoard())
            if (obj.controller == player && obj.location.has_value() && isEquipment(obj))
                out.push_back(id);
        return out;
    }
    // Units `player` controls that `equipment` is not already attached to.
    static std::vector<GameObjectId> bearersFor(const GameState& state, PlayerId player,
                                                GameObjectId equipment) {
        std::vector<GameObjectId> out;
        if (!state.objectExists(equipment)) return out;
        const auto& eq = state.getObject(equipment);
        for (const auto& [id, obj] : state.objects.onBoard()) {
            if (!obj.isUnit() || obj.controller != player || !obj.location.has_value()) continue;
            if (eq.attached_to.has_value() && *eq.attached_to == id) continue;
            out.push_back(id);
        }
        return out;
    }
    // Offered only when something can be attached somewhere.
    bool hasLegalTargets(const GameState& state, PlayerId controller) const override {
        for (auto eq : equipmentOf(state, controller))
            if (!bearersFor(state, controller, eq).empty()) return true;
        return false;
    }
    void onActivate(CardContext& ctx, int /*ability_index*/,
                    const std::vector<GameObjectId>&) override {
        std::vector<GameObjectId> equips;
        for (auto eq : equipmentOf(ctx.state, ctx.controller))
            if (!bearersFor(ctx.state, ctx.controller, eq).empty()) equips.push_back(eq);
        auto units_fn = [&](GameObjectId eq) {
            return bearersFor(ctx.state, ctx.controller, eq);
        };
        auto [eq, unit] = pickTargetPair(ctx, "Forge of the Fluft: attach Equipment to a unit",
                                         equips, units_fn);
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (eq == kInvalidId || unit == kInvalidId) return;  // suspended or fizzled
        if (!equipEndsOnBoard(ctx.state, eq, unit)) return;
        attachFree(ctx, eq, unit);
        RIFT_TRACE(ctx.events, "FORGE OF THE FLUFT: attached an Equipment to a unit");
    }
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        if (controller == PlayerId::None) return;  // uncontrolled BF
        auto legend = state.player(controller).legend_zone;
        if (legend != kInvalidId && state.objectExists(legend))
            state.getObject(legend).granted_abilities.push_back({cardDefId(), 0});
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 522;
        d.def_id = R"RB(sfd-208-221)RB";
        d.name = R"RB(Forge of the Fluft)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-208/221)RB";
        d.collector_number = 208;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(While you control this battlefield, friendly legends have "[E]: Attach an Equipment you control to a unit you control.")RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6ad10353e7fc1337d8cc79086f8eaac1c75f5598-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_522(CardRegistry& r) {
    r.registerCard(522, std::make_unique<ForgeOfTheFluft>());
}

} // namespace riftbound
