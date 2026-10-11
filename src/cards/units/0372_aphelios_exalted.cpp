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

// "When you attach an Equipment to me, choose one that hasn't been chosen this
//  turn — Ready 2 runes. / Channel 1 rune exhausted. / Buff a friendly unit."
// Wired via WhenEquipmentAttachedToMe: EffectExecutor::attachGear emits
// ObjectStateChangedEvent{"equipped"}; TriggerManager now dispatches that as
// WhenEquipmentAttachedToMe on the equipped unit. The modal body tracks which
// modes were chosen this turn (card_counters["__aph_mask"], reset when the turn
// rolls over) so a mode can be picked only once per turn.
class ApheliosExalted : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override {
        return TriggerType::WhenEquipmentAttachedToMe;
    }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& me = ctx.state.getObject(ctx.source);
        // Which modes were chosen this turn: card_counters["__aph_mask"], valid
        // for the turn in ["__aph_turn"]. A mode is taken when it is CHOSEN
        // (as the trigger is finalized), so a second trigger finalized before
        // this one resolves can't choose it again. The chain item that took a
        // mode is remembered (["__aph_item"] / ["__aph_item_mode"]) so that
        // the same item, asked again (re-entry, or new choices after
        // makeNewChoicesFor), is not blocked by its own earlier answer.
        const int turn = ctx.state.turn.turn_number;
        const int item = ctx.state.chain.resuming.has_value()
            ? static_cast<int>(ctx.state.chain.resuming->id) : -1;
        auto counter = [&me](const char* key) {
            auto it = me.card_counters.find(key);
            return it == me.card_counters.end() ? 0 : it->second;
        };
        uint32_t used = counter("__aph_turn") == turn
            ? static_cast<uint32_t>(counter("__aph_mask")) : 0u;
        const bool mine = counter("__aph_turn") == turn && counter("__aph_item") == item + 1;
        if (mine) used &= ~(1u << counter("__aph_item_mode"));
        uint32_t legal = (~used) & 0x7u;  // modes 0,1,2 not yet chosen this turn
        int mode = pickMode(ctx, "Aphelios, Exalted", 3,
                            {"Ready 2 runes", "Channel 1 rune (exhausted)",
                             "Buff a friendly unit"}, legal);
        if (mode < 0) return;  // -1 waiting / -2 no legal mode left this turn
        // Record the choice (the only write before the end-of-choices marker;
        // writing the same answer again changes nothing).
        me.card_counters["__aph_turn"] = turn;
        me.card_counters["__aph_mask"] = static_cast<int>(used | (1u << mode));
        me.card_counters["__aph_item"] = item + 1;
        me.card_counters["__aph_item_mode"] = mode;
        // Mode 2's unit is chosen with the mode.
        GameObjectId pick = kInvalidId;
        std::vector<GameObjectId> friendly;
        if (mode == 2) {
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value())
                    friendly.push_back(id);
            if (!friendly.empty()) {
                pick = pickTarget(ctx, "Aphelios (buff)", friendly);
                if (pick == kInvalidId && ctx.state.chain.resuming.has_value() &&
                    ctx.state.chain.resuming->resume_point == 7)
                    return;  // suspended on target choice
            }
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        switch (mode) {
        case 0: {  // "Ready 2 runes": which two is the player's choice
            const LocationId base{BaseLocation{ctx.controller}};
            std::vector<GameObjectId> exhausted;
            for (auto& [id, obj] : ctx.state.objects)
                if (obj.isRune() && obj.controller == ctx.controller && obj.is_exhausted &&
                    obj.location.has_value() && *obj.location == base)
                    exhausted.push_back(id);
            std::sort(exhausted.begin(), exhausted.end());
            std::vector<GameObjectId> to_ready = exhausted;
            if (exhausted.size() > 2) {
                auto picked = selectSeveral(ctx, "Aphelios: ready a rune (2)", exhausted);
                if (!picked.has_value()) return;   // waiting for the agent
                to_ready.clear();
                for (auto id : *picked)
                    if (to_ready.size() < 2 &&
                        std::find(to_ready.begin(), to_ready.end(), id) == to_ready.end())
                        to_ready.push_back(id);
                // Two runes are readied, not fewer: fill from the lowest ids.
                for (auto id : exhausted)
                    if (to_ready.size() < 2 &&
                        std::find(to_ready.begin(), to_ready.end(), id) == to_ready.end())
                        to_ready.push_back(id);
            }
            for (auto id : to_ready) ctx.executor.readyObject(id);
            ctx.events.logTrace("APHELIOS: readied " + std::to_string(to_ready.size()) + " runes");
            break;
        }
        case 1:
            ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
            ctx.events.logTrace("APHELIOS: channeled 1 rune (exhausted)");
            break;
        case 2:
            // buffUnit places a buff only on a unit without one (CR 427.1.b).
            if (pick != kInvalidId) ctx.executor.buffUnit(pick);
            break;
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
