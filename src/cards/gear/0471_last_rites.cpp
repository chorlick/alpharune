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

class LastRites : public GearCard {
public:
    const CardDef& def() const override { return def_; }

    bool hasEquipAbility() const override { return true; }
    bool onEquip(CardContext& ctx, GameObjectId unit) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.trash.size() < 2) return false;

        // Pre-check a Chaos power rune is available before committing.
        bool has_chaos = false;
        auto base_loc = BaseLocation{ctx.controller};
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isRune() || obj.controller != ctx.controller) continue;
            if (!obj.location.has_value() || *obj.location != LocationId{base_loc}) continue;
            for (auto d : obj.domains) {
                if (d == Domain::Chaos) { has_chaos = true; break; }
            }
            if (has_chaos) break;
        }
        if (!has_chaos) return false;

        // Pay the additional cost: recycle 2 cards from trash (back-of-trash;
        // which-2 is auto, an agent-choice refinement for later).
        for (int i = 0; i < 2; ++i) {
            auto card_id = ps.trash.back();
            ps.trash.pop_back();
            ctx.events.logTrace("  EQUIP_COST: recycled " +
                                 ctx.state.getObject(card_id).name + " from trash");
            ctx.state.getObject(card_id).zone = ZoneType::MainDeck;
            ps.main_deck.insert(ps.main_deck.begin(), card_id);
        }
        return standardEquip(ctx, ctx.source, unit, /*energy=*/0, Domain::Chaos);
    }

    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId /*unit*/,
                           const std::vector<GameObjectId>& /*targets*/) override {
        auto units_in_trash = [&]() {
            std::vector<GameObjectId> out;
            auto& ps = ctx.state.player(ctx.controller);
            for (auto card_id : ps.trash) {
                if (!ctx.state.objectExists(card_id)) continue;
                if (ctx.state.getObject(card_id).isUnit()) out.push_back(card_id);
            }
            return out;
        };

        // "you may play a unit from your trash"
        int answer = confirmOptional(ctx, "Last Rites: play a unit from trash",
            [&]() { return !units_in_trash().empty(); });
        if (answer == -1) return;     // yielded for agent input
        if (answer == 0) return;      // declined

        GameObjectId picked = pickTarget(ctx, "Last Rites: which unit from trash",
                                         units_in_trash());
        if (picked == kInvalidId) return;  // yielded / no legal target

        auto& ps = ctx.state.player(ctx.controller);
        ctx.events.logTrace("LAST RITES: plays " +
                             ctx.state.getObject(picked).name +
                             " from trash (cost-payment not yet wired — engine limitation)");
        // NOTE: text says "You still pay its costs", but EffectExecutor cannot
        // drive the engine's payCardCost cursor from inside a chain-resolving
        // trigger, so we play it for free for now (documented engine gap).
        ctx.executor.playIgnoringCost(ctx.controller, picked);
        ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), picked),
                       ps.trash.end());
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 471;
        d.def_id = R"RB(sfd-150-221)RB";
        d.name = R"RB(Last Rites)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-150/221)RB";
        d.collector_number = 150;
        d.artist = R"RB(黯荧岛Dark Glow)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Equipment)RB"};
        d.energy_cost = 3;
        d.might_bonus = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.ability_text = R"RB([Equip] — [P], Recycle 2 cards from your trash (Pay the cost: Attach this to a unit you control.))RB";
        d.effect_text = R"RB(When I conquer or hold, you may play a unit from your trash. (You still pay its costs.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6cfab85030bff454262794e4fa12f11b5dc45a74-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_471(CardRegistry& r) {
    r.registerCard(471, std::make_unique<LastRites>());
}

} // namespace riftbound
