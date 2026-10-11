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

class WildclawShaman : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    // "When you play me, you may spend a buff to buff me and ready me. (If I
    // don't have a buff, I get a +1 [M] buff.)" Optional: on yes, ensure I have
    // a buff (gain +1 if unbuffed per the parenthetical), then ready me.
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // "Spend a buff" is the cost of the "may": a buff comes off a unit my
        // controller chooses among their buffed units, as the ability goes on
        // the chain (CR 383.3). No buffed unit, no offer.
        auto buffed = [&]() {
            std::vector<GameObjectId> out;
            for (const auto& [id, obj] : ctx.state.objects.onBoard())
                if (obj.isUnit() && obj.controller == ctx.controller &&
                    obj.location.has_value() && obj.buff_count > 0)
                    out.push_back(id);
            std::sort(out.begin(), out.end());
            return out;
        };
        int conf = confirmOptional(ctx, "Wildclaw Shaman: spend a buff to buff & ready me?",
                                   [&]() { return !buffed().empty(); });
        if (conf < 1) return;    // waiting / declined / no buff to spend
        if (!costAlreadyPaid(ctx)) {
            // Paying a cost, not choosing a unit: no Deflect, no "chosen"
            // bookkeeping, no "when you choose a friendly unit" trigger.
            const auto from = selectObject(ctx, "Wildclaw Shaman: spend which unit's buff",
                                           buffed());
            if (!from.has_value() || *from == kInvalidId) return;   // waiting / no buff to spend
            if (costDueNow(ctx)) {
                auto& unit = ctx.state.getObject(*from);
                ctx.executor.spendBuff(*from, ctx.controller);   // CR 427.2; "When you spend a buff"
                RIFT_TRACE(ctx.events, "WILDCLAW SHAMAN: spent " + unit.name + "'s buff");
            }
        }
        if (finalizingChoices(ctx)) return;   // choices and costs end here (CR 383.3)
        // "buff me and ready me": only a Shaman still on the board (the
        // executor does nothing for one that left).
        ctx.executor.buffUnit(ctx.source);
        ctx.executor.readyObject(ctx.source);
        RIFT_TRACE(ctx.events, "WILDCLAW SHAMAN: buffed & readied me");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 147;
        d.def_id = R"RB(ogn-147-298)RB";
        d.name = R"RB(Wildclaw Shaman)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-147/298)RB";
        d.collector_number = 147;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Freljord)RB"};
        d.energy_cost = 4;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, you may spend a buff to buff me and ready me. (If I don't have a buff, I get a +1 [M] buff.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/7abc938fee4ba397f52c8ea60d350857a7517b0c-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_147(CardRegistry& r) {
    r.registerCard(147, std::make_unique<WildclawShaman>());
}

} // namespace riftbound
