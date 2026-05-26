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

class IrresistibleFaefolk : public UnitCard {
public:
    const CardDef& def() const override { return def_; }
    TriggerType triggerType() const override { return TriggerType::WhenIMoveToFB; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& /*targets*/) override {
        if (!ctx.state.objectExists(ctx.source)) return;
        auto& self = ctx.state.getObject(ctx.source);
        auto my_bf = self.battlefieldId();
        if (!my_bf) return;  // not at a BF — nothing to move to

        // Find first enemy unit that's NOT already at my BF. Includes
        // enemies at base and enemies at other BFs.
        auto find_enemy_elsewhere = [&]() -> GameObjectId {
            for (auto& [id, obj] : ctx.state.objects) {
                if (id == ctx.source) continue;
                if (!obj.isUnit()) continue;
                if (obj.controller == ctx.controller) continue;
                if (obj.battlefieldId() == my_bf) continue;  // already here
                if (!obj.location.has_value()) continue;
                return id;
            }
            return kInvalidId;
        };
        auto still_legal = [&]() { return find_enemy_elsewhere() != kInvalidId; };

        int conf = confirmOptional(ctx,
            "Irresistible Faefolk: pull an enemy unit to my battlefield?",
            still_legal);
        if (conf == -1) return;
        if (conf == 0) return;

        auto target = find_enemy_elsewhere();
        if (target == kInvalidId) return;
        auto& obj = ctx.state.getObject(target);
        std::string name = obj.name;
        ctx.executor.moveToBattlefield(target, *my_bf);
        ctx.events.logTrace("IRRESISTIBLE FAEFOLK: pulled " + name +
                             " to BF#" + std::to_string(static_cast<int>(*my_bf)));
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 674;
        d.def_id = R"RB(unl-112-219)RB";
        d.name = R"RB(Irresistible Faefolk)RB";
        d.set_code = R"RB(UNL)RB";
        d.set_name = R"RB(Unleashed)RB";
        d.public_code = R"RB(UNL-112/219)RB";
        d.collector_number = 112;
        d.artist = R"RB(Six More Vodka)RB";
        d.card_type = CardType::Unit;
        d.domains = {Domain::Body};
        d.tags = {R"RB(Fae)RB", R"RB(Ionia)RB"};
        d.energy_cost = 2;
        d.might = 1;
        d.rarity = Rarity::Rare;
        d.ability_text = R"RB(When I move to a battlefield, you may move an enemy unit to that battlefield.)RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6834e4b00a22dd6d7476d36884b43cf59b60c611-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_674(CardRegistry& r) {
    r.registerCard(674, std::make_unique<IrresistibleFaefolk>());
}

} // namespace riftbound
