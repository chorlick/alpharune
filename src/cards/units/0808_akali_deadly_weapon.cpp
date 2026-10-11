#include "cards/card.h"
#include "cards/card_helpers.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <stdexcept>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class AkaliDeadlyWeapon : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // "[Empower] [2][R]"
    EmpowerSpec empowerSpec() const override { return empowerCost(2, 1, Domain::Fury); }
    // "[Empowered][>] I have +1 [M]."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        forEachEmpoweredSelf(*this, state, controller,
            [](GameObjectId sid, GameObject& self) { auraMight(self, sid, 1); });
    }

    // "When I move, you may deal 1 to a unit at a battlefield I moved to or
    //  from. If I'm [Empowered], deal 2 instead."
    // Both ends are those of the move that triggered this (triggeringMove,
    // carried on the chain item) — not where I am now, nor my latest move.
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    std::vector<GameObjectId> unitsWhereIMoved(CardContext& ctx) const {
        if (!ctx.state.objectExists(ctx.source)) return {};
        const auto* move = triggeringMove(ctx.state);
        if (!move) throw std::logic_error("Akali, Deadly Weapon: \"When I move\" resolved without a move");
        const std::optional<BattlefieldId> to_bf = battlefieldOfLocation(move->to);
        const std::optional<BattlefieldId> from_bf = battlefieldOfLocation(move->from);
        if (!to_bf && !from_bf) return {};
        return chooseableUnits(ctx.state, ctx.controller, [&](const GameObject& o) {
            auto bf = o.battlefieldId();
            return bf && ((to_bf && *bf == *to_bf) || (from_bf && *bf == *from_bf));
        });
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        int conf = confirmOptional(ctx, "Akali, Deadly Weapon: deal damage to a unit?",
                                   [&]() { return !unitsWhereIMoved(ctx).empty(); });
        if (conf == -1) return;  // waiting for agent
        if (conf < 1) return;    // declined / nothing to hit
        GameObjectId tgt = pickTarget(ctx, "Akali, Deadly Weapon: choose a unit",
                                      unitsWhereIMoved(ctx));
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (tgt == kInvalidId || !ctx.state.objectExists(tgt)) return;
        if (!ctx.state.objectExists(ctx.source)) return;
        const int amount = ctx.state.getObject(ctx.source).isEmpowered() ? 2 : 1;
        ctx.executor.dealDamage(tgt, amount, ctx.source);
        RIFT_TRACE(ctx.events, "AKALI, DEADLY WEAPON: moved -> dealt " + std::to_string(amount));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 808;
        d.def_id = R"RB(ven-021-166)RB";
        d.name = R"RB(Akali, Deadly Weapon)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-021/166)RB";
        d.collector_number = 21;
        d.artist = R"RB(莺之歌)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Ionia)RB", R"RB(Akali)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Empower);
        d.ability_text = R"RB([Empower] [2][R] ([2][R]: Empower me. Use only if not Empowered.)
When I move, you may deal 1 to a unit at a battlefield I moved to or from. If I'm [Empowered], deal 2 instead.
[Empowered][>] I have +1 [M].)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/564e5f35ae39b6604290e83f39ffb61c2b2b9175-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_808(CardRegistry& r) {
    r.registerCard(808, std::make_unique<AkaliDeadlyWeapon>());
}

}  // namespace riftbound
