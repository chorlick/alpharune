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

class Pickpocket : public UnitCard {
public:
    const CardDef& def() const override { return def_; }

    // "When you play me, you may kill a gear with Energy cost no more than [1].
    //  If you do, play a Gold gear token exhausted."
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayMe; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        auto candidates = [&]() {
            std::vector<GameObjectId> out;
            for (auto& [id, obj] : ctx.state.objects) {
                if (!obj.isGear() || !obj.location.has_value()) continue;
                if (obj.controller != ctx.controller && obj.untargetable_by_enemy)
                    continue;
                const auto& def = ctx.executor.cardDB().get(obj.card_def_id);
                if (def.energy_cost > 1) continue;
                out.push_back(id);
            }
            return out;
        };
        int conf = confirmOptional(ctx, "Pickpocket: kill a gear (<= [1] cost)?",
                                   [&]() { return !candidates().empty(); });
        if (conf == -1) return;  // waiting for agent
        if (conf == 0) return;   // declined / none

        GameObjectId gear = pickTarget(ctx, "Pickpocket: choose a gear to kill",
                                       candidates());
        if (gear == kInvalidId || !ctx.state.objectExists(gear)) return;
        ctx.executor.killObject(gear);
        // "If you do, play a Gold gear token exhausted."
        createGoldExhausted(ctx);
        ctx.events.logTrace("PICKPOCKET: killed a <=[1] gear -> Gold gear token (exhausted)");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 397;
        d.def_id = R"RB(sfd-074-221)RB";
        d.name = R"RB(Pickpocket)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-074/221)RB";
        d.collector_number = 74;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Mind};
        d.tags = {R"RB(Zaun)RB"};
        d.energy_cost = 3;
        d.might = 3;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you play me, you may kill a gear with Energy cost no more than [1]. If you do, play a Gold gear token exhausted.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/ebf54af997e53079e1a476feb0411d1791b20f7e-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_397(CardRegistry& r) {
    r.registerCard(397, std::make_unique<Pickpocket>());
}

} // namespace riftbound
