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

class SpiritSRefuge : public GearCard {
public:
    const CardDef& def() const override { return def_; }
    void onTrigger(CardContext& ctx, const std::vector<GameObjectId>& targets) override {
        if (!targets.empty())
            ctx.executor.buffUnit(targets[0]);
    }
    TriggerType triggerType() const override { return TriggerType::WhenYouPlayThis; }
    TargetRequirements getTargetRequirements() const override {
        return TargetRequirements{.count = 1, .must_be_unit = true, .must_be_friendly = true};
    }
    // "Friendly buffed units have [Deflect] if they didn't already."
    void applyPassiveAura(GameState& state, PlayerId controller) const override {
        // Active only while an on-board instance exists.
        GameObjectId self_id = kInvalidId;
        for (auto& [id, obj] : state.objects) {
            if (obj.card_def_id != cardDefId()) continue;
            if (obj.controller != controller || !obj.location.has_value()) continue;
            self_id = id;
            break;
        }
        if (self_id == kInvalidId) return;
        for (auto& [uid, u] : state.objects) {
            if (!u.isUnit() || u.controller != controller || !u.location.has_value())
                continue;
            if (u.buff_count <= 0) continue;  // "buffed" = has a +1 [M] buff
            GameObject::AuraEffect ae;
            ae.source = self_id;
            ae.keyword = Keyword::Deflect;
            ae.keyword_value = 1;
            u.aura_effects.push_back(ae);
        }
    }
private:
    const CardDef def_ = [] {
        CardDef d;
        d.id = 63;
        d.def_id = R"RB(ogn-063-298)RB";
        d.name = R"RB(Spirit's Refuge)RB";
        d.set_code = R"RB(OGN)RB";
        d.set_name = R"RB(Origins)RB";
        d.public_code = R"RB(OGN-063/298)RB";
        d.collector_number = 63;
        d.artist = R"RB(Kudos Productions)RB";
        d.card_type = CardType::Gear;
        d.domains = {Domain::Calm};
        d.energy_cost = 2;
        d.power_cost = 1;
        d.rarity = Rarity::Uncommon;
        d.deflect_value = 1;
        d.keywords.set(Keyword::Deflect);
        d.ability_text = R"RB(When you play this, buff a friendly unit. (If it doesn't have a buff, it gets a +1 [M] buff.)
Friendly buffed units have [Deflect] if they didn't already. (Opponents must pay [A] to choose those units with a spell or ability.))RB";
        d.image_url = R"RB(https://cmsassets.rgpub.io/sanity/images/dsfx7636/game_data_live/26d5e54580d1f17ed6c35a66ac6a90ce56c99ec8-744x1039.png)RB";
        return d;
    }();
};

}  // anonymous namespace

void register_card_63(CardRegistry& r) {
    r.registerCard(63, std::make_unique<SpiritSRefuge>());
}

} // namespace riftbound
