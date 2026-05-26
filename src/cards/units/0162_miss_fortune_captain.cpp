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

class MissFortuneCaptain : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& captain = ctx.state.getObject(ctx.source);

        // "First time I move each turn" — store turn_id (+1 so 0 = never).
        // We only set the sentinel on FIRST entry; subsequent re-entries
        // during the same chain resolution carry the resume_point state
        // forward, so the "already fired this turn" guard mustn't reject
        // them.
        int turn_id = ctx.state.turn.turn_number + 1;
        bool first_entry = !ctx.state.chain.resuming.has_value()
                        || ctx.state.chain.resuming->resume_point == 0;
        int& last_fired = captain.card_counters["mf_captain_move_turn"];
        if (first_entry && last_fired == turn_id) return;  // not the first move
        if (first_entry) last_fired = turn_id;

        // Targets: friendly things other than Captain that are exhausted and
        // on board (skip in-deck/banished/etc.).
        auto find_targets = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (obj.controller != ctx.controller) continue;
                if (!obj.is_exhausted) continue;
                if (!obj.location.has_value()) continue;
                out.push_back(id);
            }
            return out;
        };

        int answer = confirmOptional(ctx, "Miss Fortune ready another",
                                     [&]() { return !find_targets().empty(); });
        if (answer == -1) return;     // yielded for agent input
        if (answer == 0) {
            ctx.events.logTrace("MISS FORTUNE: declined to ready another");
            return;
        }

        auto target = pickTarget(ctx, "Miss Fortune ready an exhausted friendly",
                                  find_targets());
        if (target == kInvalidId) return;  // yielded for agent input

        ctx.executor.readyObject(target);
        ctx.events.logTrace("MISS FORTUNE: readied " +
                             ctx.state.getObject(target).name);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 162;
        d.def_id = R"RB(ogn-162-298)RB";
        d.name = R"RB(Miss Fortune, Captain)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-162/298)RB";
        d.collector_number = 162;
        d.artist = R"RB(League Splash Team)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Pirate)RB", R"RB(Miss Fortune)RB", R"RB(Bilgewater)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Accelerate);
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Accelerate] (You may pay [1][O] as an additional cost to have me enter ready.)
[Ganking] (I can move from battlefield to battlefield.)
The first time I move each turn, you may ready something else that's exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/3c7b219245cd6c6ee835974dd74771bc605289de-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_162(CardRegistry& r) {
    r.registerCard(162, std::make_unique<MissFortuneCaptain>());
}

} // namespace riftbound
