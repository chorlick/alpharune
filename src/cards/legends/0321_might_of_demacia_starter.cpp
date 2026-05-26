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

class MightOfDemaciaStarter : public LegendCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you conquer, if you have 4+ units at that battlefield, draw 2."
    // Legends fire WhenIConquer via the legend-zone sweep (no per-event
    // battlefield is threaded into the chain item). "That battlefield" is the
    // one just conquered; the available signal is battlefields_scored_this_turn.
    // We satisfy the condition if ANY battlefield the controller scored this
    // turn (and controls) holds 4+ friendly units — correct in the common
    // single-conquer case, and conservative (only draws on a real 4+ board).
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx,
                   const std::vector<GameObjectId>& /*targets*/) override {
        const auto& ps = ctx.state.player(ctx.controller);
        for (auto bf_id : ps.battlefields_scored_this_turn) {
            int count = 0;
            for (const auto& [uid, u] : ctx.state.objects) {
                if (!u.isUnit() || u.controller != ctx.controller) continue;
                if (u.battlefieldId() == bf_id) ++count;
            }
            if (count >= 4) {
                ctx.executor.drawCards(ctx.controller, 2);
                ctx.events.logTrace("MIGHT OF DEMACIA: conquer with 4+ units here "
                                    "-> draw 2");
                return;
            }
        }
    }
private:
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
