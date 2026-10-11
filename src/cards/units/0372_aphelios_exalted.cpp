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
            for (auto& [id, obj] : ctx.state.objects.onBoard())
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value())
                    friendly.push_back(id);
            // Called with an empty list too: pickTarget then stores "no
            // target" on the chain item, so the resolution asks nothing if a
            // friendly unit has arrived since.
            pick = pickTarget(ctx, "Aphelios (buff)", friendly);
            if (pick == kInvalidId && ctx.state.chain.resuming.has_value() &&
                ctx.state.chain.resuming->resume_point == 7)
                return;  // suspended on target choice
        }
        // Mode 0's runes are chosen with the mode (CR 355.5, 383.3), so the
        // opponent responds knowing them.
        // "Ready 2 runes": bare "runes", with no "friendly" (Sona 73) or
        // "your" (Ekko 110), is any rune on the board (CR 133.3.a, 355.9.a.1,
        // 355.9.b), so an opponent's exhausted rune may be named too. It is
        // not "up to 2": with two or more exhausted runes exactly two are
        // readied (no "None" on the prompts), with fewer every one there is
        // (CR 055) — and with two or fewer there is nothing to ask.
        auto readiable = [&ctx](GameObjectId id) {
            if (id == kInvalidId || !ctx.state.objectExists(id)) return false;
            const auto& obj = ctx.state.getObject(id);
            return obj.isRune() && obj.is_exhausted && obj.location.has_value();
        };
        GameObjectId rune_a = kInvalidId, rune_b = kInvalidId;
        if (mode == 0) {
            std::vector<GameObjectId> exhausted;
            for (const auto& [id, obj] : ctx.state.objects.onBoard())
                if (readiable(id)) exhausted.push_back(id);
            std::sort(exhausted.begin(), exhausted.end());
            // selectObject keeps each answer on the chain item (slots 7 and
            // 9) and takes a single option without asking; at resolution it
            // returns the stored rune whatever the list is by then.
            std::vector<GameObjectId> first = exhausted;
            if (first.size() == 2) first.resize(1);   // both are readied: no choice
            const auto a = selectObject(ctx, "Aphelios: ready a rune (1 of 2)", first, 7);
            if (!a.has_value()) return;   // waiting for the agent
            rune_a = *a;
            std::vector<GameObjectId> second;
            for (auto id : exhausted)
                if (id != rune_a) second.push_back(id);
            const auto b = selectObject(ctx, "Aphelios: ready a rune (2 of 2)", second, 9);
            if (!b.has_value()) return;   // waiting for the agent
            rune_b = *b;
        }
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        switch (mode) {
        case 0: {  // "Ready 2 runes": the two named as the ability was finalized
            int readied = 0;
            for (auto id : {rune_a, rune_b}) {
                // Not dead: selectObject hands back the stored answer as
                // it was given, without looking at the list again. So this
                // is what skips (a) kInvalidId — fewer than two exhausted
                // runes when the ability was finalized — and (b) a rune
                // named then that has been readied or has left the board
                // since; no other rune is readied in its place.
                if (!readiable(id)) continue;
                ctx.executor.readyObject(id);
                ++readied;
            }
            RIFT_TRACE(ctx.events, "APHELIOS: readied " + std::to_string(readied) + " runes");
            break;
        }
        case 1:
            ctx.executor.channelRunes(ctx.controller, 1, /*enter_exhausted=*/true);
            RIFT_TRACE(ctx.events, "APHELIOS: channeled 1 rune (exhausted)");
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
