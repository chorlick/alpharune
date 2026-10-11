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

class NoxianDemolitionist : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }

    // "When I conquer, you may kill a gear with Energy cost no more than my Might."
    TriggerType triggerType() const override { return TriggerType::WhenIConquer; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto gears = [&]() {
            const int my_might = ctx.state.getObject(ctx.source).current_might;
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects.onBoard()) {
                if (!obj.isGear() || !obj.location.has_value()) continue;
                if (obj.controller != ctx.controller && obj.untargetable_by_enemy) continue;
                // Tokens have no CardDef-backed cost: Energy cost 0. A gear
                // token may have no card at all (card_def_id == kInvalidId), so
                // the card table is only consulted for a real card.
                int cost = 0;
                if (!obj.isToken() && obj.card_def_id != kInvalidId)
                    cost = ctx.executor.cardDB().get(obj.card_def_id).energy_cost;
                if (cost <= my_might) out.push_back(id);
            }
            std::sort(out.begin(), out.end());
            return out;
        };
        int conf = confirmOptional(ctx, "Noxian Demolitionist: kill a gear?",
                                   [&]() { return !gears().empty(); });
        if (conf != 1) return;                        // waiting, declined or no legal gear
        auto picked = pickTarget(ctx, "Noxian Demolitionist: choose a gear", gears());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (picked == kInvalidId || !ctx.state.objectExists(picked)) return;
        ctx.executor.killObject(picked);
        RIFT_TRACE(ctx.events, "NOXIAN DEMOLITIONIST: killed a gear");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 867;
        d.def_id = R"RB(ven-080-166)RB";
        d.name = R"RB(Noxian Demolitionist)RB";
        d.set_code = R"RB(VEN)RB";
        d.set_name = R"RB(Vendetta)RB";
        d.public_code = R"RB(VEN-080/166)RB";
        d.collector_number = 80;
        d.artist = R"RB(Dao Trong Le)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Noxus)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When I conquer, you may kill a gear with Energy cost no more than my Might.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/a8c9a1b0eefc89106aa154ce3958c82d86e70940-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_867(CardRegistry& r) {
    r.registerCard(867, std::make_unique<NoxianDemolitionist>());
}

}  // namespace riftbound
