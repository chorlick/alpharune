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

class YasuoWindrider : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    // "The third time I move in a turn, you score 1 point."
    TriggerType triggerType() const override { return TriggerType::WhenIMove; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        int turn_id = ctx.state.turn.turn_number + 1;
        // Reset per-turn move count when the turn rolls over.
        if (self.card_counters["yasuo_move_turn"] != turn_id) {
            self.card_counters["yasuo_move_turn"] = turn_id;
            self.card_counters["yasuo_moves"] = 0;
        }
        int moves = ++self.card_counters["yasuo_moves"];
        if (moves == 3) {
            ctx.state.player(ctx.controller).score++;
            ctx.events.logTrace("YASUO WINDRIDER: 3rd move this turn -> score 1");
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 205;
        d.def_id = R"RB(ogn-205-298)RB";
        d.name = R"RB(Yasuo, Windrider)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-205/298)RB";
        d.collector_number = 205;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Chaos};
        d.tags = {R"RB(Yasuo)RB", R"RB(Ionia)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 4;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Ganking);
        d.ability_text = R"RB([Ganking] (I can move from battlefield to battlefield.)
The third time I move in a turn, you score 1 point.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/f5ba378ce4dad16d17e001814e091d5f484f2681-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_205(CardRegistry& r) {
    r.registerCard(205, std::make_unique<YasuoWindrider>());
}

} // namespace riftbound
