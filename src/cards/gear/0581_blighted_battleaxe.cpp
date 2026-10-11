#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/gear/equip_base.h"

namespace riftbound {
namespace {

class BlightedBattleaxe : public SimpleEquipGear {
public:
    BlightedBattleaxe() : SimpleEquipGear(Domain::Fury, 1) {}
    const CardDef& def() const override { return def_; }
    // Effect text, so it speaks for the unit this is attached to and applies
    // exactly while attached (the gear's printed text is Inactive then, CR
    // 718.2): an equipped trigger, fired by TriggerManager at the end of the
    // bearer's controller's turn.
    TriggerType equippedTriggerType() const override { return TriggerType::AtEndOfTurn; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId unit_id,
                           const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source) || !ctx.state.objectExists(unit_id)) return;
        const auto& gear = ctx.state.getObject(ctx.source);
        // Still attached to that unit, which is still on the board.
        if (!gear.attached_to.has_value() || *gear.attached_to != unit_id) return;
        const auto& unit = ctx.state.getObject(unit_id);
        if (!unit.location.has_value()) return;
        auto it = unit.card_counters.find("__conquered_turn");
        bool conquered_this_turn = (it != unit.card_counters.end() &&
                                    it->second == ctx.state.turn.turn_number);
        if (conquered_this_turn) return;
        RIFT_TRACE(ctx.events, "BLIGHTED BATTLEAXE: no conquer this turn — "
                            "unattach + deal 4 to bearer");
        ctx.executor.unattachGear(ctx.source);
        ctx.executor.dealDamage(unit_id, 4, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 581;
        d.def_id = R"RB(unl-019-219)RB";
        d.name = R"RB(Blighted Battleaxe)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-019/219)RB";
        d.collector_number = 19;
        d.artist = R"RB(Caravan Studio)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Fury};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 4;
        d.might_bonus = 4;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] [1][R] ([1][R]: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(At the end of your turn, if I didn't conquer this turn, unattach this and deal 4 to me.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a6425462180dc6bc3396397cd491c0abae58f616-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_581(CardRegistry& r) {
    r.registerCard(581, std::make_unique<BlightedBattleaxe>());
}

} // namespace riftbound
