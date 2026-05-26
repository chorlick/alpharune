#include "cards/card.h"
#include "cards/card_registry.h"
#include "core/game_state.h"
#include "core/events.h"
#include "engine/effect_executor.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "cards/gear/equip_base.h"

namespace riftbound {
namespace {

class ShurelyaSRequiem : public UniversalEquipGear {
public:
    const CardDef& def() const override { return def_; }

    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>&) override {
        for (auto& [id, obj] : ctx.state.objects) {
            if (obj.isUnit() && obj.controller == ctx.controller &&
                obj.location.has_value() && obj.is_exhausted) {
                ctx.executor.readyObject(id);
            }
        }
        ctx.events.logTrace("SHURELYA'S REQUIEM: ready your units");
    }

    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Locate this gear instance (attached, controlled by `controller`)
        // and grant [Ganking] to the controller's units at its battlefield.
        for (auto& [gid, gear] : state.objects) {
            if (gear.card_def_id != cardDefId()) continue;
            if (gear.controller != controller || !gear.attached_to.has_value()) continue;
            auto bf = gear.battlefieldId();
            if (!bf) continue;
            for (auto& [tid, tgt] : state.objects) {
                if (!tgt.isUnit() || tgt.controller != controller) continue;
                auto tbf = tgt.battlefieldId();
                if (!tbf || *tbf != *bf) continue;
                GameObject::AuraEffect ae;
                ae.source = gid;
                ae.keyword = Keyword::Ganking;
                tgt.aura_effects.push_back(ae);
            }
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 509;
        d.def_id = R"RB(sfd-192-221)RB";
        d.name = R"RB(Shurelya's Requiem)RB";
        d.set_code = R"RB(SFD)RB";
        d.set_name = R"RB(Spiritforged)RB";
        d.public_code = R"RB(SFD-192/221)RB";
        d.collector_number = 192;
        d.artist = R"RB(Grafit Studio)RB";
        d.card_type = CardType::Gear;
        d.super_type = SuperType::Signature;
        d.domains = {Domain::Calm, Domain::Mind};
        d.tags = {R"RB(Ornn)RB", R"RB(Equipment)RB"};
        d.energy_cost = 4;
        d.power_cost = 2;
        d.might_bonus = 2;
        d.rarity = Rarity::Epic;
        d.keywords.set(Keyword::Equip);
        d.keywords.set(Keyword::Unique);
        d.ability_text = R"RB([Unique] (Your deck can have only 1 card with this name.)
[Equip] [A] ([A]: Attach this to a unit you control.)
When you play this, ready your units.)RB";
        d.effect_text = R"RB(Your units here have [Ganking]. (We can move from battlefield to battlefield.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/6f2f7175e61486859d7f5da804e41733d66b2254-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_509(CardRegistry& r) {
    r.registerCard(509, std::make_unique<ShurelyaSRequiem>());
}

} // namespace riftbound
