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

class AltarOfBlood : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    // "If a unit here would die during combat, its controller may pay [A][A][A]
    //  to heal it, exhaust it, and recall it instead." A replacement effect on
    //  the battlefield card: GameEngine::killUnit asks the card of the
    //  battlefield a unit is dying at, on behalf of that unit's controller.
    bool hasReplacementEffect() const override { return true; }
    bool applyReplacement(CardContext& ctx, GameObjectId dying_unit) override {
        if (!ctx.state.objectExists(dying_unit)) return false;
        auto& unit = ctx.state.getObject(dying_unit);
        if (!unit.isUnit()) return false;
        // "here ... during combat": at my battlefield, with a combat on there.
        auto my_bf = battlefieldOfCard(ctx.state, ctx.source);
        if (!my_bf || unit.battlefieldId() != my_bf) return false;
        bool in_combat = false;
        for (const auto& bf : ctx.state.battlefields)
            if (bf.id == *my_bf) in_combat = bf.combat_in_progress;
        if (!in_combat) return false;
        // "its controller may pay [A][A][A]" — offered only when payable
        // (floating Power or any rune in base, ready or exhausted).
        const PlayerId payer = unit.controller;
        if (ctx.executor.payableAnyPower(payer) < 3) return false;
        if (!ctx.executor.confirmNow(payer, "Altar of Blood: pay [A][A][A] to heal, exhaust "
                                            "and recall " + unit.name + "?"))
            return false;
        if (!ctx.executor.payAnyPower(payer, 3)) return false;
        ctx.events.logTrace("ALTAR OF BLOOD: " + unit.name +
                            " pays [A][A][A] -> heal/exhaust/recall");
        ctx.executor.healObject(dying_unit);
        ctx.executor.exhaustObject(dying_unit);
        unit.combat_designation = CombatDesignation::None;
        // A recall is not a move (CR 458): it must not count toward "moved N
        // times this turn", which EffectExecutor::moveToBase bumps.
        const int moves = unit.moves_this_turn;
        ctx.executor.moveToBase(dying_unit);
        unit.moves_this_turn = moves;
        return true;
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 762;
        d.def_id = R"RB(unl-206-219)RB";
        d.name = R"RB(Altar of Blood)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-206/219)RB";
        d.collector_number = 206;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(If a unit here would die during combat, its controller may pay [A][A][A] to heal it, exhaust it, and recall it instead.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a17e5bb639866671772dc514a6096a5863712f71-1039x744.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_762(CardRegistry& r) {
    r.registerCard(762, std::make_unique<AltarOfBlood>());
}

} // namespace riftbound
