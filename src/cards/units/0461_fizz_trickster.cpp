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

class FizzTrickster : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    unsigned choicesAtFinalization() const override { return kFinalizeTrigger; }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        auto& ps = ctx.state.player(ctx.controller);
        const auto& db = ctx.executor.cardDB();
        // Energy is waived; "(You must still pay its Power cost.)" (CR 356.1.b.2).
        const EffectExecutor::EffectPlayCost power_only{.ignore_energy = true};
        // Spells in my trash with Energy cost [3] or less whose Power I can
        // pay — the controller chooses among them.
        auto playable_spells = [&]() {
            std::vector<GameObjectId> out;
            for (auto cid : ps.trash) {
                if (!ctx.state.objectExists(cid)) continue;
                auto& obj = ctx.state.getObject(cid);
                if (obj.card_type != CardType::Spell) continue;
                if (obj.card_def_id == kInvalidId) continue;
                if (db.get(obj.card_def_id).energy_cost > 3) continue;
                if (!ctx.executor.canPlayPayingCost(ctx.controller, cid, power_only)) continue;
                out.push_back(cid);
            }
            return out;
        };
        int conf = confirmOptional(ctx,
            "Fizz: play a spell (cost <= 3) from trash for free?",
            [&]() { return !playable_spells().empty(); });
        GameObjectId chosen = kInvalidId;
        if (conf == 1) chosen = pickTarget(ctx, "Fizz: choose a spell from your trash",
                                           playable_spells());
        if (finalizingChoices(ctx)) return;   // choices end here (CR 383.3)
        if (conf != 1 || chosen == kInvalidId) return;
        std::string sname = ctx.state.getObject(chosen).name;
        // The spell goes on the chain and resolves; "Then recycle it" replaces
        // where it goes when it leaves the chain. playPayingCost takes it out
        // of the trash itself — and only if it is actually played.
        ctx.state.getObject(chosen).card_counters["__recycle_on_leave_chain"] = 1;
        if (!ctx.executor.playPayingCost(ctx.controller, chosen, std::nullopt, power_only)) {
            ctx.state.getObject(chosen).card_counters.erase("__recycle_on_leave_chain");
            return;
        }
        // With no chain to resolve on (an executor driven directly, outside a
        // game), the spell can't leave the chain later: recycle it now.
        if (ctx.state.objectExists(chosen) &&
            ctx.state.getObject(chosen).zone != ZoneType::Chain) {
            ctx.state.getObject(chosen).card_counters["__recycle_on_leave_chain"] = 0;
            ctx.executor.recycleCards(ctx.controller, {chosen});
        }
        RIFT_TRACE(ctx.events, "FIZZ: played " + sname +
                             " from trash (energy<=3, ignoring energy), then recycled it");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 461;
        d.def_id = R"RB(sfd-140-221)RB";
        d.name = R"RB(Fizz, Trickster)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-140/221)RB";
        d.collector_number = 140;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yordle)RB", R"RB(Fizz)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 3;
        d.power_cost = 1;
        d.might = 3;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When you play me, you may play a spell from your trash with Energy cost no more than [3], ignoring its Energy cost. Then recycle it. (You must still pay its Power cost.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6d1c6615fb6ef5fb520a35b2ce76f8feee9167dc-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_461(CardRegistry& r) {
    r.registerCard(461, std::make_unique<FizzTrickster>());
}

} // namespace riftbound
