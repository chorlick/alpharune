#include "cards/card.h"
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

// "When you attach an Equipment to me, choose one that hasn't been chosen this
//  turn — Ready 2 runes. / Channel 1 rune exhausted. / Buff a friendly unit."
// Wired via WhenEquipmentAttachedToMe: attachGearToUnit/attachFree already emit
// ObjectStateChangedEvent{"equipped"}; TriggerManager now dispatches that as
// WhenEquipmentAttachedToMe on the equipped unit. The modal body tracks which
// modes were chosen this turn (card_counters["__aph_mask"], reset when the turn
// rolls over) so a mode can be picked only once per turn.
class ApheliosExalted : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override {
        return TriggerType::WhenEquipmentAttachedToMe;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        // Reset the per-turn "modes chosen" mask on a new turn.
        if (me.card_counters["__aph_turn"] != ctx.state.turn.turn_number) {
            me.card_counters["__aph_turn"] = ctx.state.turn.turn_number;
            me.card_counters["__aph_mask"] = 0;
        }
        uint32_t used = static_cast<uint32_t>(me.card_counters["__aph_mask"]);
        uint32_t legal = (~used) & 0x7u;  // modes 0,1,2 not yet chosen this turn
        int mode = pickMode(ctx, "Aphelios, Exalted", 3,
                            {"Ready 2 runes", "Channel 1 rune (exhausted)",
                             "Buff a friendly unit"}, legal);
        if (mode < 0) return;  // -1 waiting / -2 no legal mode left this turn
        // Record the chosen mode as used this turn (idempotent across re-entry).
        ctx.state.getObject(ctx.source).card_counters["__aph_mask"] =
            static_cast<int>(used | (1u << mode));
        switch (mode) {
        case 0: {  // Ready up to 2 of my exhausted runes
            int readied = 0;
            LocationId base{BaseLocation{ctx.controller}};
            for (auto& [id, obj] : ctx.state.objects) {
                if (readied >= 2) break;
                if (!obj.isRune() || obj.controller != ctx.controller) continue;
                if (!obj.is_exhausted || !obj.location.has_value()) continue;
                if (*obj.location != base) continue;
                obj.is_exhausted = false;
                ++readied;
            }
            ctx.events.logTrace("APHELIOS: readied " + std::to_string(readied) + " runes");
            break;
        }
        case 1:
            ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
            ctx.events.logTrace("APHELIOS: channeled 1 rune (exhausted)");
            break;
        case 2: {
            std::vector<GameObjectId> friendly;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value())
                    friendly.push_back(id);
            if (friendly.empty()) break;
            GameObjectId pick = pickTarget(ctx, "Aphelios (buff)", friendly);
            if (pick == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7)
                return;  // suspended on target choice
            if (pick != kInvalidId && ctx.state.objectExists(pick))
                ctx.executor.buffUnit(pick);
            break;
        }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 372;
        d.def_id = R"RB(sfd-049-221)RB";
        d.name = R"RB(Aphelios, Exalted)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-049/221)RB";
        d.collector_number = 49;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Aphelios)RB", R"RB(Mount Targon)RB"};
        d.energy_cost = 4;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you attach an Equipment to me, choose one that hasn't been chosen this turn —
Ready 2 runes.Channel 1 rune exhausted.Buff a friendly unit.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/df9ccbba2501059e5781a44434745a1f0e33ecae-744x1039.png?accountingTag=RB)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_372(CardRegistry& r) {
    r.registerCard(372, std::make_unique<ApheliosExalted>());
}

} // namespace riftbound
