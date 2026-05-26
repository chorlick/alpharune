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

class SunkenTemple : public BattlefieldCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenYouConquerHere; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        // For a battlefield card the trigger fires from the BF card object;
        // map ctx.source -> its BattlefieldId via the battlefields list.
        std::optional<BattlefieldId> my_bf;
        for (const auto& bf : ctx.state.battlefields) {
            if (bf.card_object_id == ctx.source) { my_bf = bf.id; break; }
        }
        if (!my_bf) return;
        // Require one or more friendly Mighty (5+ M) units at this battlefield.
        bool has_mighty = false;
        for (auto& [id, obj] : ctx.state.objects) {
            if (!obj.isUnit()) continue;
            if (obj.controller != ctx.controller) continue;
            if (obj.battlefieldId() != my_bf) continue;
            if (obj.current_might >= 5) { has_mighty = true; break; }
        }
        if (!has_mighty) return;
        auto& ps = ctx.state.player(ctx.controller);
        auto still_legal = [&]() { return ps.rune_pool.energy >= 1; };
        if (!still_legal()) return;
        int conf = confirmOptional(ctx, "Sunken Temple: pay [1] to draw 1?",
                                   still_legal);
        if (conf < 1) return;
        ps.rune_pool.energy -= 1;
        ctx.executor.drawCards(ctx.controller, 1);
        ctx.events.logTrace("SUNKEN TEMPLE: paid [1] -> draw 1");
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 532;
        d.def_id = R"RB(sfd-218-221)RB";
        d.name = R"RB(Sunken Temple)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-218/221)RB";
        d.collector_number = 218;
        d.artist = R"RB(Envar Studio)RB";
        d.card_type = CardType::Battlefield;
        d.rarity = Rarity::Uncommon;
        d.ability_text = R"RB(When you conquer here with one or more [Mighty] units, you may pay [1] to draw 1. (A unit is Mighty while it has 5+ [M].))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/06f6d17929d19000006cf281d013ecbe1543af0e-1039x744.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_532(CardRegistry& r) {
    r.registerCard(532, std::make_unique<SunkenTemple>());
}

} // namespace riftbound
