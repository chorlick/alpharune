#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include "cards/card_helpers.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace riftbound {
namespace {

class MightOfDemaciaStarter : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you conquer, if you have 4+ units at that battlefield, draw 2."
    // "That battlefield" is the one just conquered: the trigger's subject is
    // its card object (TriggerManager::onScore). The "if" is an intervening
    // condition — checked when the conquer happens and again as the ability
    // resolves.
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    bool triggerCondition(const GameState& state, GameObjectId /*source*/, PlayerId controller,
                          TriggerType, GameObjectId subject) const override {
        return unitsAtConquered(state, controller, subject) >= 4;
    }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId bf_card = ctx.state.chain.resuming.has_value()
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        if (unitsAtConquered(ctx.state, ctx.controller, bf_card) < 4) return;
        ctx.executor.drawCards(ctx.controller, 2);
        RIFT_TRACE(ctx.events, "MIGHT OF DEMACIA: conquer with 4+ units here -> draw 2");
    }
private:
    static int unitsAtConquered(const GameState& state, PlayerId controller,
                                GameObjectId bf_card) {
        const auto bf = battlefieldOfCard(state, bf_card);
        if (!bf.has_value()) return 0;
        int count = 0;
        for (const auto& [uid, u] : state.objects)
            if (u.isUnit() && u.controller == controller && u.battlefieldId() == bf) ++count;
        return count;
    }
    const CardDef def_ = [] {
        CardDef d;
        d.id = 321;
        d.def_id = R"RB(ogs-023-024)RB";
        d.name = R"RB(Might of Demacia - Starter)RB";
        d.set_code = R"RB(OGS)RB";
        d.set_name = R"RB(Proving Grounds)RB";
        d.public_code = R"RB(OGS-023/024)RB";
        d.collector_number = 23;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Legend;
        d.domains = {Domain::Body, Domain::Order};
        d.tags = {R"RB(Garen)RB"};
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you conquer, if you have 4+ units at that battlefield, draw 2.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/e7185deb46f17770802d06aeddfe3b929afff880-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_321(CardRegistry& r) {
    r.registerCard(321, std::make_unique<MightOfDemaciaStarter>());
}

} // namespace riftbound
