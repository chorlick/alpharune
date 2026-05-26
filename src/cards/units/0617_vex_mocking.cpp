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

class VexMocking : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouStun; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto it = self.card_counters.find("__stunned_unit_id");
        if (it == self.card_counters.end()) return;
        auto stunned = static_cast<GameObjectId>(it->second);
        if (!ctx.state.objectExists(stunned)) return;

        // Determine the battlefield of the stunned enemy unit ("at a
        // battlefield" / "that battlefield").
        auto target_bf_of = [&ctx, stunned]() -> std::optional<BattlefieldId> {
            if (!ctx.state.objectExists(stunned)) return std::nullopt;
            return ctx.state.getObject(stunned).battlefieldId();
        };
        auto still_legal = [&]() {
            auto bf = target_bf_of();
            if (!bf) return false;                       // stunned unit must be at a BF
            if (!ctx.state.objectExists(ctx.source)) return false;
            auto& s = ctx.state.getObject(ctx.source);
            // Already there? then there's nothing to move.
            auto cur = s.battlefieldId();
            return !(cur && *cur == *bf);
        };
        if (!still_legal()) return;

        int conf = confirmOptional(ctx, "Vex Mocking: move me to that battlefield?",
                                    still_legal);
        if (conf < 1) return;
        auto bf = target_bf_of();
        if (!bf) return;
        ctx.executor.moveToBattlefield(ctx.source, *bf);
        ctx.events.logTrace("VEX MOCKING: moved to stunned unit's battlefield BF" +
                            std::to_string(*bf));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 617;
        d.def_id = R"RB(unl-055-219)RB";
        d.name = R"RB(Vex, Mocking)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-055/219)RB";
        d.collector_number = 55;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Unit;
        d.super_type = SuperType::Champion;
        d.domains = {Domain::Calm};
        d.tags = {R"RB(Yordle)RB", R"RB(Vex)RB", R"RB(Shadow Isles)RB"};
        d.energy_cost = 5;
        d.power_cost = 1;
        d.might = 5;
        d.rarity = Rarity::Rare;
        d.shield_value = 1;
        d.keywords.set(Keyword::Shield);
        d.keywords.set(Keyword::Tank);
        d.ability_text = R"RB([Shield] (+1 [M] while I'm a defender.)
[Tank] (I must be assigned combat damage first.)
When you [Stun] an enemy unit at a battlefield, you may move me to that battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ed2c20de6ceb7ff23f62ccb28a49fd305c5b15b7-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_617(CardRegistry& r) {
    r.registerCard(617, std::make_unique<VexMocking>());
}

} // namespace riftbound
