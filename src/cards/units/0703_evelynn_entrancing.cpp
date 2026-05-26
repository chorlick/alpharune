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
        if (!self.isAtBattlefield()) return;      // need a destination battlefield
        auto my_bf = self.battlefieldId();
        if (!my_bf) return;

        PlayerId opp = opponent(ctx.controller);
        std::vector<GameObjectId> legal;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit() || obj.controller != opp) continue;
            if (!obj.location.has_value()) continue;
            // "at a different location" — exclude enemies already at my BF.
            auto obf = obj.battlefieldId();
            if (obf && *obf == *my_bf) continue;
            legal.push_back(id);
        }
        auto still_legal = [&legal]() { return !legal.empty(); };
        if (!still_legal()) return;

        int conf = confirmOptional(ctx, "Evelynn: move an enemy unit to my battlefield?",
                                    still_legal);
        if (conf < 1) return;
        GameObjectId picked = pickTarget(ctx, "Evelynn: pick enemy unit", legal);
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.moveToBattlefield(picked, *my_bf);
        ctx.events.logTrace("EVELYNN: pulled " + ctx.state.getObject(picked).name +
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
