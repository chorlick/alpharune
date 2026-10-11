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

class EvelynnEntrancing : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        // "When you play me FROM FACE DOWN ON YOUR TURN": WhenYouPlayMe fires
        // for every play, so gate on the marker GameEngine::executePlayFromHidden
        // stamps. Latch and consume it on the first entry (as Tornado Warrior
        // does) so a later play from hand does not see a stale marker.
        const auto& ri = ctx.state.chain.resuming;
        if (!ri.has_value() || ri->resume_point == 0) {
            auto& from_facedown = self.card_counters["__played_from_facedown"];
            self.card_counters["__evelynn_from_facedown"] =
                (from_facedown == 1 && ctx.state.turn.turn_player == ctx.controller) ? 1 : 0;
            from_facedown = 0;
        }
        if (self.card_counters["__evelynn_from_facedown"] != 1) return;
        if (!self.isAtBattlefield()) return;      // need a destination battlefield
        auto my_bf = self.battlefieldId();
        if (!my_bf) return;

        // Enemy units at a different location; chooseableUnits drops the ones
        // that can't be chosen by enemy spells and abilities.
        auto legal = chooseableUnits(ctx.state, ctx.controller,
            [&](const GameObject& obj) {
                if (obj.controller == ctx.controller) return false;
                auto obf = obj.battlefieldId();
                return !(obf && *obf == *my_bf);
            });
        auto still_legal = [&legal]() { return !legal.empty(); };
        if (!still_legal()) return;

        int conf = confirmOptional(ctx, "Evelynn: move an enemy unit to my battlefield?",
                                    still_legal);
        if (conf < 1) return;
        GameObjectId picked = pickTarget(ctx, "Evelynn: pick enemy unit", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.moveToBattlefield(picked, *my_bf);
        RIFT_TRACE(ctx.events, "EVELYNN: pulled " + ctx.state.getObject(picked).name +
                            " to my battlefield BF" + std::to_string(*my_bf));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 703;
        d.def_id = R"RB(unl-141-219)RB";
        d.name = R"RB(Evelynn, Entrancing)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-141/219)RB";
        d.collector_number = 141;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Evelynn)RB", R"RB(Demon)RB"};
        d.energy_cost = 2;
        d.might = 2;
        d.rarity = Rarity::Rare;
        d.keywords.set(Keyword::Backline);
        d.keywords.set(Keyword::Hidden);
        d.ability_text = R"RB([Hidden] (Hide now for [A] to react with later for .)
[Backline] (I must be assigned combat damage last.)
When you play me from face down on your turn, you may move an enemy unit at a different location to my battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/1cea41a2b9c3de59a1c95ceacc59950be1d01907-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_703(CardRegistry& r) {
    r.registerCard(703, std::make_unique<EvelynnEntrancing>());
}

} // namespace riftbound
