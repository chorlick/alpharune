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

    // "[Equip] — [P], Recycle 2 cards from your trash". All-or-nothing: the
    // whole cost is checked before any of it is paid. Equip resolves outside
    // the chain, so which two cards are recycled is a direct agent decision
    // (EffectExecutor::chooseObjectNow).
    bool hasEquipAbility() const override { return true; }
    // "[Equip] — [P], Recycle 2 cards from your trash".
    EquipCost equipCost(const GameState&, PlayerId, GameObjectId, GameObjectId) const override {
        return {.valid = true, .power = 1, .power_domain = Domain::Chaos};
    }
    bool canPayEquipExtraCost(const GameState& state, PlayerId controller, GameObjectId,
                              GameObjectId) const override {
        return state.player(controller).trash.size() >= 2;
    }
    bool payEquipExtraCost(CardContext& ctx, GameObjectId) override {
        auto& ps = ctx.state.player(ctx.controller);
        if (ps.trash.size() < 2) return false;
        GameObjectId last_recycled = kInvalidId;
        for (int i = 0; i < 2; ++i) {
            std::vector<GameObjectId> in_trash(ps.trash.begin(), ps.trash.end());
            const GameObjectId card_id = ctx.executor.chooseObjectNow(ctx.controller, in_trash);
            if (card_id == kInvalidId) break;
            ps.trash.erase(std::remove(ps.trash.begin(), ps.trash.end(), card_id), ps.trash.end());
            ctx.events.logTrace("  EQUIP_COST: recycled " +
                                 ctx.state.getObject(card_id).name + " from trash");
            deck::putOnBottom(ctx.state, ctx.controller, card_id, deck::Place::Public);
            last_recycled = card_id;
        }
        // "When you recycle one or more cards to your Main Deck" (once).
        if (last_recycled != kInvalidId)
            ctx.events.emit(ObjectStateChangedEvent{last_recycled, "recycled_main"});
        return true;
    }

    // Equipped: "When I conquer or hold, you may play a unit from your trash.
    // (You still pay its costs.)" The "may" and the unit are chosen as the
    // ability is finalized (CR 383.3); the unit's cost is paid as it is played.
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType equippedTriggerType() const override { return TriggerType::WhenIConquerOrHold; }
    void onEquippedTrigger(CardContext& ctx, GameObjectId /*unit*/,
                           const std::vector<GameObjectId>& /*targets*/) override {
        // Units in the trash whose cost could be paid right now.
        auto playable_units = [&]() {
            std::vector<GameObjectId> out;
            for (auto card_id : ctx.state.player(ctx.controller).trash) {
                if (!ctx.state.objectExists(card_id)) continue;
                if (!ctx.state.getObject(card_id).isUnit()) continue;
                if (ctx.executor.canPlayPayingCost(ctx.controller, card_id)) out.push_back(card_id);
            }
            return out;
        };
        const int answer = confirmOptional(ctx, "Last Rites: play a unit from trash",
            [&]() { return !playable_units().empty(); });
        GameObjectId picked = kInvalidId;
        if (answer == 1)
            picked = pickTarget(ctx, "Last Rites: which unit from trash", playable_units());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (answer != 1 || picked == kInvalidId) return;
        // CR 356.1.b.2: the cost is not waived. playPayingCost takes the card
        // out of the trash itself, and leaves it there if the cost isn't paid.
        const std::string name = ctx.state.getObject(picked).name;
        if (ctx.executor.playPayingCost(ctx.controller, picked))
            ctx.events.logTrace("LAST RITES: played " + name + " from trash");
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
