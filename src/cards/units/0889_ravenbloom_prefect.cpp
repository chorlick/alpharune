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

class RavenbloomPrefect : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    // ── behaviour (Vendetta batch C) ──
    // "When an opponent plays a gear, you may banish me to banish it."
    // The played gear rides on the chain item as triggering_subject.
    TriggerType triggerType() const override { return TriggerType::WhenOpponentPlaysAGear; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        const GameObjectId gear = ctx.state.chain.resuming
            ? ctx.state.chain.resuming->triggering_subject : kInvalidId;
        auto gear_in_play = [&]() {
            if (gear == kInvalidId || !ctx.state.objectExists(gear)) return false;
            const auto& g = ctx.state.getObject(gear);
            // Still being played (on the chain) or already on the board.
            return g.zone == ZoneType::Chain || g.location.has_value();
        };
        auto still_legal = [&]() {
            if (!gear_in_play()) return false;
            if (costAlreadyPaid(ctx)) return true;
            return ctx.state.objectExists(ctx.source) &&
                   ctx.state.getObject(ctx.source).location.has_value();   // I'm the cost
        };
        int conf = confirmOptional(ctx, "Ravenbloom Prefect: banish me to banish the gear?",
                                   still_legal);
        if (conf == -1) return;                       // waiting for the agent
        // "banish me" is the cost: paid as the ability goes on the chain (CR 383.3).
        if (conf == 1 && costDueNow(ctx)) ctx.executor.banishObject(ctx.source);
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        if (conf < 1 || !gear_in_play()) return;
        // If the gear has not finished being played, its pending permanent
        // chain item must go with it so it never enters the board.
        auto& items = ctx.state.chain.items;
        items.erase(std::remove_if(items.begin(), items.end(),
                        [gear](const ChainItem& it) {
                            return it.source == gear && !it.is_ability;
                        }),
                    items.end());
        ctx.executor.banishObject(gear);
        ctx.events.logTrace("RAVENBLOOM PREFECT: banished self -> banished the played gear");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 889;
        d.def_id = R"RB(ven-102-166)RB";
        d.name = R"RB(Ravenbloom Prefect)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-102/166)RB";
        d.collector_number = 102;
        d.artist = R"RB(Polar Engine Studio)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Common;
        d.ability_text = R"RB(When an opponent plays a gear, you may banish me to banish it.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/57ecb26880ea084ea16b785c9a563ff298ee6a42-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_889(CardRegistry& r) {
    r.registerCard(889, std::make_unique<RavenbloomPrefect>());
}

}  // namespace riftbound
