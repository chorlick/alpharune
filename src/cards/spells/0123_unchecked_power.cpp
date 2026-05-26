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

class UncheckedPower : public SpellCard {
public:
    const CardDef& def() const override { return def_; }
    void onResolve(CardContext& ctx, const std::vector<GameObjectId>&) override {
        // 1. Exhaust all friendly units.
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == ctx.controller &&
                obj.location.has_value()) {
                obj.is_exhausted = true;
            }
        }
        // 2. Deal 12 to every unit currently at any battlefield. Snapshot
        //    first so dealDamage / kill chain doesn't invalidate iterator.
        std::vector<GameObjectId> victims;
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.isAtBattlefield()) victims.push_back(id);
        }
        for (auto v : victims) ctx.executor.dealDamage(v, 12, ctx.source);
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 123;
        d.def_id = R"RB(ogn-123-298)RB";
        d.name = R"RB(Unchecked Power)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-123/298)RB";
        d.collector_number = 123;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Spell;
        d.domains = {Domain::Mind};
        d.energy_cost = 7;
        d.power_cost = 2;
        d.rarity = Rarity::Epic;
        d.ability_text = R"RB(Exhaust all friendly units, then deal 12 to ALL units at battlefields.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/c44cd1f6f41144c770ac79f10e5ae75f0c6d6435-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_123(CardRegistry& r) {
    r.registerCard(123, std::make_unique<UncheckedPower>());
}

} // namespace riftbound
